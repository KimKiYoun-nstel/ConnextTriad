/**
 * @file ipc_adapter.cpp
 * @brief IpcAdapter 구현 파일
 *
 * UI에서 들어오는 명령을 받아 DdsManager API 호출 → 결과를 ACK/ERR로 응답한다.
 * DdsManager에서 수신한 샘플은 EVT(DATA)로 UI에 전송한다.
 * 각 함수별 파라미터/리턴/예외/로그 정책을 Doxygen 주석으로 기술한다.
 */
#include "triad_log.hpp"
#include "sample_factory.hpp"
#include "ipc_adapter.hpp"
#include "dds_manager.hpp"
#include "type_registry.hpp"
#include "dds_manager_internal.hpp"
#include <nlohmann/json.hpp>
#include <any>
#include <vector>

namespace rtpdds
{
using rtpdds::internal::truncate_for_log;

/**
 * @brief DdsManager 참조로 어댑터 생성, 콜백 설치
 * @param mgr DDS 엔티티/샘플 관리 참조
 */
IpcAdapter::IpcAdapter(IDdsManager& mgr) : mgr_(mgr)
{
    install_callbacks();
}

/**
 * @brief 자원 해제 및 종료
 */
IpcAdapter::~IpcAdapter()
{
    stop();
}

/**
 * @brief 서버 모드 시작
 * @param bind_addr 바인드 주소
 * @param port 바인드 포트
 * @return 서버 시작 성공 여부
 */
bool IpcAdapter::start_server(const std::string& bind_addr, uint16_t port)
{
    return ipc_.start(dkmrtp::ipc::Role::Server, {bind_addr, port});
}

/**
 * @brief 클라이언트 모드 시작
 * @param peer_addr 접속할 서버 주소
 * @param port 접속할 서버 포트
 * @return 클라이언트 시작 성공 여부
 */
bool IpcAdapter::start_client(const std::string& peer_addr, uint16_t port)
{
    return ipc_.start(dkmrtp::ipc::Role::Client, {peer_addr, port});
}

/**
 * @brief 종료 및 콜백 해제
 */
void IpcAdapter::stop()
{
    ipc_.stop();
}

/**
 * @brief 내부 콜백 설치 (IPC 요청/응답/이벤트 처리)
 *
 * - on_request: UI에서 수신한 CBOR(JSON) 기반 RPC를 파싱하여 DdsManager API로 위임하고, 결과를 CBOR로 응답한다.
 * - 이벤트 전송: DDS 샘플 수신은 Gateway의 AsyncEventProcessor를 통해 비동기 EVT로 전송한다.
 */
void IpcAdapter::install_callbacks()
{
    dkmrtp::ipc::DkmRtpIpc::Callbacks cb{};
    // on_cmd_XX 관련 콜백 제거됨. REQ/RSP/EVT 구조만 남김.

    // === 통합 RPC Envelope (IPC 위 CBOR) ===
    cb.on_request = [this](const dkmrtp::ipc::Header& h, const uint8_t* body, uint32_t len) {
        // 수신 프레임을 비동기 CommandEvent로 변환하여 소비자 스레드로 전달
    LOG_DBG("IPC", "on_request corr_id=%u size=%u", h.corr_id, len);

        // FLOW 로깅: CBOR를 JSON으로 시도 변환(비치명적)
        try {
            nlohmann::json j = nlohmann::json::from_cbor(body, body + len);
            auto msg = j.dump();
            LOG_FLOW("IN corr_id=%u msg=%s", h.corr_id, truncate_for_log(msg, 1024).c_str());
        } catch (...) {
            LOG_FLOW("IN corr_id=%u msg=<non-json/cbor payload size=%u>", h.corr_id, len);
        }

        async::CommandEvent ev;
        ev.corr_id = h.corr_id;
        ev.route = "ipc";
        ev.body.assign(body, body + len);
        ev.is_cbor = true;

        if (!post_cmd_) {
            LOG_WRN("IPC", "command post is null, replying error corr_id=%u", h.corr_id);
            nlohmann::json rsp = {{"ok", false}, {"err", 7}, {"msg", "no command sink"}};
            auto out = nlohmann::json::to_cbor(rsp);
            // OUT flow log for error response (debug-level with truncation)
            auto preview = rsp.dump();
            LOG_FLOW("OUT corr_id=%u rsp=%s", h.corr_id, truncate_for_log(preview, 1024).c_str());
            ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_RSP, h.corr_id, out.data(), (uint32_t)out.size());
            return;
        }
        post_cmd_(ev);
    };

    // 주의: mgr_.set_on_sample은 여기서 설치하지 않는다.
    // Gateway가 AsyncEventProcessor를 소유하며, DDS 수신 샘플은 비동기 큐로 전달된다.
    // 소비자 스레드가 IpcAdapter::emit_evt_from_sample을 호출하여 JSON→CBOR 변환 및 IPC 전송을 수행한다.
    // 이를 통해 DDS 리스너 콜백은 경량/논블로킹으로 유지된다.

    ipc_.set_callbacks(cb);
}

// 이동된: emit_evt_from_sample 구현
void IpcAdapter::emit_evt_from_sample(const async::SampleEvent& ev)
{
    const std::string& topic = ev.topic;
    const std::string& type_name = ev.type_name;
    const AnyData& data = ev.data;

    nlohmann::json data_json;

    const void* sample_ptr = nullptr;
    try {
        auto sp = std::any_cast<std::shared_ptr<void> >(data);
        sample_ptr = sp.get();
        if (!sample_ptr) {
            LOG_ERR("IPC", "AnyData contains null pointer for type=%s", type_name.c_str());
        }
    } catch (const std::bad_cast&) {
        LOG_ERR("IPC", "AnyData is not shared_ptr<void> for type=%s", type_name.c_str());
    }

    bool ok = sample_ptr ? rtpdds::dds_to_json(type_name, sample_ptr, data_json) : false;

    if (!ok) {
        data_json = nlohmann::json();
        LOG_WRN("IPC", "dds_to_json failed type=%s", type_name.c_str());
    } else {
        // Keep a truncated preview only for server-side logs; do not include a separate
        // "display" field in the EVT payload. The EVT carries the full JSON object
        // in the "data" field per protocol (object-first).
        auto preview = data_json.dump();
    if (preview.size() > 2048) preview.resize(2048);
    LOG_DBG("IPC", "data json preview=%s", preview.c_str());
    }

    nlohmann::json evt = {{"evt", "data"}, {"topic", topic}, {"type", type_name}, {"data", data_json}};
    LOG_INF("IPC", "send EVT topic=%s type=%s", topic.c_str(), type_name.c_str());

    auto out = nlohmann::json::to_cbor(evt);
    // OUT flow log for event (debug-level, truncated)
    auto evt_preview = evt.dump();
    LOG_FLOW("OUT evt topic=%s type=%s evt=%s", topic.c_str(), type_name.c_str(), truncate_for_log(evt_preview, 1024).c_str());
    ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_EVT, 0, out.data(), (uint32_t)out.size());
}

// hello 응답에 사용되는 기능(capability) 목록을 구조적으로 생성한다.
static nlohmann::json build_hello_capabilities()
{
    nlohmann::json caps = nlohmann::json::array();

    // create.participant
    {
        nlohmann::json cap;
        cap["name"] = "create.participant";
        nlohmann::json example;
        example["op"] = "create";
        example["target"] = nlohmann::json::object();
        example["target"]["kind"] = "participant";
        example["args"] = nlohmann::json::object();
        example["args"]["domain"] = 0;
        example["args"]["qos"] = "TriadQosLib::DefaultReliable";
        cap["example"] = example;
        caps.push_back(cap);
    }

    // create.publisher
    {
        nlohmann::json cap;
        cap["name"] = "create.publisher";
        nlohmann::json example;
        example["op"] = "create";
        example["target"] = nlohmann::json::object();
        example["target"]["kind"] = "publisher";
        example["args"] = nlohmann::json::object();
        example["args"]["domain"] = 0;
        example["args"]["publisher"] = "pub1";
        example["args"]["qos"] = "TriadQosLib::DefaultReliable";
        cap["example"] = example;
        caps.push_back(cap);
    }

    // create.subscriber
    {
        nlohmann::json cap;
        cap["name"] = "create.subscriber";
        nlohmann::json example;
        example["op"] = "create";
        example["target"] = nlohmann::json::object();
        example["target"]["kind"] = "subscriber";
        example["args"] = nlohmann::json::object();
        example["args"]["domain"] = 0;
        example["args"]["subscriber"] = "sub1";
        example["args"]["qos"] = "TriadQosLib::DefaultReliable";
        cap["example"] = example;
        caps.push_back(cap);
    }

    // create.writer
    {
        nlohmann::json cap;
        cap["name"] = "create.writer";
        nlohmann::json example;
        example["op"] = "create";
        example["target"] = nlohmann::json::object();
        example["target"]["kind"] = "writer";
        example["target"]["topic"] = "ExampleTopic";
        example["target"]["type"] = "ExampleType";
        example["args"] = nlohmann::json::object();
        example["args"]["domain"] = 0;
        example["args"]["publisher"] = "pub1";
        example["args"]["qos"] = "TriadQosLib::DefaultReliable";
        cap["example"] = example;
        caps.push_back(cap);
    }

    // create.reader
    {
        nlohmann::json cap;
        cap["name"] = "create.reader";
        nlohmann::json example;
        example["op"] = "create";
        example["target"] = nlohmann::json::object();
        example["target"]["kind"] = "reader";
        example["target"]["topic"] = "ExampleTopic";
        example["target"]["type"] = "ExampleType";
        example["args"] = nlohmann::json::object();
        example["args"]["domain"] = 0;
        example["args"]["subscriber"] = "sub1";
        example["args"]["qos"] = "TriadQosLib::DefaultReliable";
        cap["example"] = example;
        caps.push_back(cap);
    }

    // write (publish text)
    {
        nlohmann::json cap;
        cap["name"] = "write";
        nlohmann::json example;
        example["op"] = "write";
        example["target"] = nlohmann::json::object();
        example["target"]["kind"] = "writer";
        example["target"]["topic"] = "chat";
        example["data"] = nlohmann::json::object();
        example["data"]["text"] = "Hello world";
        cap["example"] = example;
        caps.push_back(cap);
    }

    // get.qos
    {
        nlohmann::json cap;
        cap["name"] = "get.qos";
        nlohmann::json example;
        example["op"] = "get";
        example["target"] = nlohmann::json::object();
        example["target"]["kind"] = "qos";
        cap["example"] = example;
        caps.push_back(cap);
    }

    // set.qos
    {
        nlohmann::json cap;
        cap["name"] = "set.qos";
        nlohmann::json example;
        example["op"] = "set";
        example["target"] = nlohmann::json::object();
        example["target"]["kind"] = "qos";
        example["data"] = nlohmann::json::object();
        example["data"]["library"] = "NGVA_QoS_Library";
        example["data"]["profile"] = "custom_profile";
        example["data"]["xml"] = "<qos_profile name=\"custom_profile\">...</qos_profile>";
        cap["example"] = example;
        caps.push_back(cap);
    }

    // evt.data description
    {
        nlohmann::json cap;
        cap["name"] = "evt.data";
        cap["description"] = "Gateway sends EVT messages when DDS samples are received. See protocol doc for evt.data format.";
        caps.push_back(cap);
    }

    return caps;
}

// CommandEvent를 게이트웨이 비동기 큐로 전달하기 위한 post 함수 설정
void IpcAdapter::set_command_post(std::function<void(const async::CommandEvent&)> f) {
    post_cmd_ = std::move(f);
    LOG_INF("IPC", "command post installed");
}


// process_request: 소비자 스레드에서 호출되어 명령을 실제 처리한다.
void IpcAdapter::process_request(const async::CommandEvent& ev) {
    const auto t0 = std::chrono::steady_clock::now();

    nlohmann::json rsp;
    // 1단계: CBOR → JSON 파싱 (파싱 실패 시 즉시 종료)
    nlohmann::json req;
    try {
        req = nlohmann::json::from_cbor(ev.body);
    } catch (const std::exception& ex) {
        LOG_WRN("IPC", "request parse failed corr_id=%u error=%s", ev.corr_id, ex.what());
        rsp = {
            {"ok", false},
            {"err", 7},                 // 기존 parse/error 코드 유지
            {"msg", "parse failed"},
            {"err_kind", "parse"},     // 에러 분류: 파싱 단계 실패
            {"fail_detail", ex.what()}, // 구체적 예외 메시지
            {"source", "agent"}        // UI 문제 아님을 명시
        };
        // 응답 로그 및 전송
        try {
            auto rsp_preview = rsp.dump();
            LOG_FLOW("OUT corr_id=%u rsp=%s", ev.corr_id, truncate_for_log(rsp_preview, 1024).c_str());
        } catch (...) {
            LOG_FLOW("OUT corr_id=%u rsp=<non-json>", ev.corr_id);
        }
        auto out = nlohmann::json::to_cbor(rsp);
        ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_RSP, ev.corr_id, out.data(), (uint32_t)out.size());
        const auto dt = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
        const auto qd = std::chrono::duration_cast<std::chrono::microseconds>(t0 - ev.received_time).count();
        LOG_DBG("IPC", "process_request done corr_id=%u q_delay(us)=%lld exec(us)=%lld rsp_size=%zu",
                ev.corr_id, (long long)qd, (long long)dt, out.size());
        return; // 파싱 실패 처리 종료
    }

    try {
        const std::string op = req.value("op", "");
        const auto target = req.value("target", nlohmann::json::object());
        const std::string kind = target.value("kind", std::string());

        bool ok = false;

    // 분기 가독성을 위한 소규모 헬퍼 람다들
        auto do_clear = [&]() {
            mgr_.clear_entities();
            rsp = { {"ok", true}, {"result", {{"action", "dds entities cleared"}}} };
            ok = true;
        };

        auto create_participant = [&]() {
            int domain = req["args"].value("domain", 0);
            std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
            std::string lib = "", prof = "";
            auto p = qos.find("::");
            if (p != std::string::npos) {
                lib = qos.substr(0, p);
                prof = qos.substr(p + 2);
            }
            DdsResult res = mgr_.create_participant(domain, lib, prof);
            if (res.ok) {
                LOG_INF("IPC", "participant created: domain=%d qos=%s", domain, qos.c_str());
                rsp = { {"ok", true}, {"result", {{"action", "participant created"}, {"domain", domain}}} };
                ok = true;
            } else {
                LOG_WRN("IPC", "participant creation failed: domain=%d category=%d reason=%s", domain,
                        (int)res.category, res.reason.c_str());
                rsp = { {"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason} };
            }
        };

        auto create_publisher = [&]() {
            int domain = req["args"].value("domain", 0);
            std::string pub = req["args"].value("publisher", "pub1");
            std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
            std::string lib = "", prof = "";
            auto p = qos.find("::");
            if (p != std::string::npos) {
                lib = qos.substr(0, p);
                prof = qos.substr(p + 2);
            }
            DdsResult res = mgr_.create_publisher(domain, pub, lib, prof);
            if (res.ok) {
                LOG_INF("IPC", "publisher created: domain=%d pub=%s qos=%s", domain, pub.c_str(), qos.c_str());
                rsp = { {"ok", true}, {"result", {{"action", "publisher created"}, {"domain", domain}, {"publisher", pub}}} };
                ok = true;
            } else {
                LOG_WRN("IPC", "publisher creation failed: domain=%d pub=%s category=%d reason=%s", domain,
                        pub.c_str(), (int)res.category, res.reason.c_str());
                rsp = { {"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason} };
            }
        };

        auto create_subscriber = [&]() {
            int domain = req["args"].value("domain", 0);
            std::string sub = req["args"].value("subscriber", "sub1");
            std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
            std::string lib = "", prof = "";
            auto p = qos.find("::");
            if (p != std::string::npos) {
                lib = qos.substr(0, p);
                prof = qos.substr(p + 2);
            }
            DdsResult res = mgr_.create_subscriber(domain, sub, lib, prof);
            if (res.ok) {
                LOG_INF("IPC", "subscriber created: domain=%d sub=%s qos=%s", domain, sub.c_str(), qos.c_str());
                rsp = { {"ok", true}, {"result", {{"action", "subscriber created"}, {"domain", domain}, {"subscriber", sub}}} };
                ok = true;
            } else {
                LOG_WRN("IPC", "subscriber creation failed: domain=%d sub=%s category=%d reason=%s", domain,
                        sub.c_str(), (int)res.category, res.reason.c_str());
                rsp = { {"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason} };
            }
        };

        auto create_writer = [&]() {
            int domain = req["args"].value("domain", 0);
            std::string pub = req["args"].value("publisher", "pub1");
            std::string topic = target.contains("topic") ? target.value("topic", "") : "";
            std::string type = target.contains("type") ? target.value("type", "") : "";
            std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
            std::string lib = "", prof = "";
            auto p = qos.find("::");
            if (p != std::string::npos) {
                lib = qos.substr(0, p);
                prof = qos.substr(p + 2);
            }
            if (topic.empty() || type.empty()) {
                LOG_WRN("IPC", "writer creation failed: missing topic or type tag");
                rsp = { {"ok", false}, {"err", 6}, {"msg", "Missing topic or type tag"} };
            } else {
                uint64_t holder_id = 0;
                DdsResult res = mgr_.create_writer(domain, pub, topic, type, lib, prof, &holder_id);
                if (res.ok) {
                    LOG_INF("IPC", "writer created: domain=%d pub=%s topic=%s type=%s", domain, pub.c_str(),
                            topic.c_str(), type.c_str());
                    rsp = { {"ok", true}, {"result", {{"action", "writer created"}, {"domain", domain}, {"publisher", pub}, {"topic", topic}, {"type", type}, {"id", holder_id}}} };
                    ok = true;
                } else {
                    LOG_WRN(
                        "IPC", "writer creation failed: domain=%d pub=%s topic=%s type=%s category=%d reason=%s",
                        domain, pub.c_str(), topic.c_str(), type.c_str(), (int)res.category, res.reason.c_str());
                    rsp = { {"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason} };
                }
            }
        };

        auto create_reader = [&]() {
            int domain = req["args"].value("domain", 0);
            std::string sub = req["args"].value("subscriber", "sub1");
            std::string topic = target.contains("topic") ? target.value("topic", "") : "";
            std::string type = target.contains("type") ? target.value("type", "") : "";
            std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
            std::string lib = "", prof = "";
            auto p = qos.find("::");
            if (p != std::string::npos) {
                lib = qos.substr(0, p);
                prof = qos.substr(p + 2);
            }
            if (topic.empty() || type.empty()) {
                LOG_WRN("IPC", "reader creation failed: missing topic or type tag");
                rsp = { {"ok", false}, {"err", 6}, {"msg", "Missing topic or type tag"} };
            } else {
                uint64_t holder_id = 0;
                DdsResult res = mgr_.create_reader(domain, sub, topic, type, lib, prof, &holder_id);
                if (res.ok) {
                    LOG_INF("IPC", "reader created: domain=%d sub=%s topic=%s type=%s", domain, sub.c_str(),
                            topic.c_str(), type.c_str());
                    rsp = { {"ok", true}, {"result", {{"action", "reader created"}, {"domain", domain}, {"subscriber", sub}, {"topic", topic}, {"type", type}, {"id", holder_id}}} };
                    ok = true;
                } else {
                    LOG_WRN(
                        "IPC", "reader creation failed: domain=%d sub=%s topic=%s type=%s category=%d reason=%s",
                        domain, sub.c_str(), topic.c_str(), type.c_str(), (int)res.category, res.reason.c_str());
                    rsp = { {"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason} };
                }
            }
        };

        auto do_write = [&]() {
            if (kind != "writer") return;
            std::string topic = target.value("topic", "");
            if (topic.empty()) {
                LOG_WRN("IPC", "publish_json failed: missing topic tag");
                rsp = { {"ok", false}, {"err", 6}, {"msg", "Missing topic tag"} };
                return;
            }
            // Expect data to be a JSON object
            if (!req.contains("data") || !req["data"].is_object()) {
                LOG_WRN("IPC", "publish_json failed: missing or invalid data object for topic=%s", topic.c_str());
                rsp = { {"ok", false}, {"err", 6}, {"msg", "Missing or invalid data object"} };
                return;
            }
            nlohmann::json data_json = req["data"];
            {
                auto data_preview = data_json.dump();
                LOG_DBG("IPC", "Calling DdsManager::publish_json(topic=%s, data=%s)", topic.c_str(), truncate_for_log(data_preview, 512).c_str());
            }
            DdsResult res = mgr_.publish_json(topic, data_json);
            if (res.ok) {
                LOG_INF("IPC", "publish_json ok: topic=%s", topic.c_str());
                rsp = { {"ok", true}, {"result", {{"action", "publish ok"}, {"topic", topic}}} };
                ok = true;
            } else {
                LOG_WRN("IPC", "publish_json failed: topic=%s category=%d reason=%s", topic.c_str(),
                        (int)res.category, res.reason.c_str());
                rsp = { {"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason} };
            }
        };

        auto do_hello = [&]() {
            ok = true;
            rsp = nlohmann::json::object();
            rsp["ok"] = true;
            rsp["result"] = nlohmann::json::object();
            rsp["result"]["proto"] = 1;
            rsp["result"]["cap"] = build_hello_capabilities();
        };

        auto do_get = [&]() {
            if (kind != "qos") return;
            LOG_FLOW("Received get qos request");
            try {
                // args are optional booleans
                bool include_builtin = false;
                bool include_detail = false;
                if (req.contains("args") && req["args"].is_object()) {
                    include_builtin = req["args"].value("include_builtin", false);
                    include_detail = req["args"].value("detail", false);
                }

                auto json_out = mgr_.list_qos_profiles(include_builtin, include_detail);
                // ensure result field present (must be returned)
                nlohmann::json result = json_out.value("result", nlohmann::json::array());

                rsp["ok"] = true;
                rsp["result"] = result;

                if (include_detail) {
                    // include detail only when requested; manager returns an array now
                    rsp["detail"] = json_out.value("detail", nlohmann::json::array());
                } else {
                }
                ok = true;
            } catch (const std::exception& ex) {
                LOG_WRN("IPC", "get qos handler exception: %s", ex.what());
                rsp = { {"ok", false}, {"err", 4}, {"msg", "failed to build qos list"} };
            }
        };

        auto do_set_qos = [&]() {
            // QoS Profile 동적 추가/업데이트
            // 요청 형식: { "op": "set", "target": { "kind": "qos" }, "data": { "library": "...", "profile": "...", "xml": "..." } }
            if (!req.contains("data") || !req["data"].is_object()) {
                rsp = { {"ok", false}, {"err", 6}, {"msg", "Missing or invalid data object for set.qos"} };
                return;
            }
            
            const auto& data = req["data"];
            std::string library = data.value("library", "");
            std::string profile = data.value("profile", "");
            std::string xml = data.value("xml", "");
            
            if (library.empty() || profile.empty() || xml.empty()) {
                rsp = { {"ok", false}, {"err", 6}, {"msg", "Missing required fields: library, profile, xml"} };
                return;
            }
            
            // DdsManager의 QosStore에 Profile 추가/업데이트
            std::string full_name = mgr_.add_or_update_qos_profile(library, profile, xml);
            if (full_name.empty()) {
                rsp = { {"ok", false}, {"err", 4}, {"msg", "Failed to add/update QoS profile"} };
                return;
            }
            
            ok = true;
            rsp = {
                {"ok", true},
                {"result", {
                    {"action", "qos profile updated"},
                    {"profile", full_name}
                }}
            };
        };

    // op 기준 디스패치
        if (op == "clear" && kind == "dds_entities") {
            do_clear();
        } else if (op == "create") {
            if (kind == "participant") create_participant();
            else if (kind == "publisher") create_publisher();
            else if (kind == "subscriber") create_subscriber();
            else if (kind == "writer") create_writer();
            else if (kind == "reader") create_reader();
        } else if (op == "write") {
            do_write();
        } else if (op == "hello") {
            do_hello();
        } else if (op == "get") {
            do_get();
        } else if (op == "set" && kind == "qos") {
            do_set_qos();
        }
    // 참고: 기타 분기(생성/쓰기/hello 등)는 위와 동일한 패턴으로 이곳에서 처리한다.

        if (!ok && rsp.empty()) rsp = {{"ok", false}, {"err", 4}, {"msg", "unsupported or failed"}};
    } catch (const std::exception& ex) {
        // 비즈니스 로직/RTI 호출 등 내부 처리 중 발생한 예외
        LOG_ERR("IPC", "process_request internal exception corr_id=%u error=%s", ev.corr_id, ex.what());
        rsp = {
            {"ok", false},
            {"err", 7},                  // 기존 에러 코드 재사용 (내부 오류 범주)
            {"msg", "internal error"},   // json/cbor error 오인 방지 문구 교체
            {"err_kind", "internal"},    // 내부 처리 예외 분류
            {"fail_detail", ex.what()},   // 예외 상세
            {"source", "agent"}          // UI 책임 아님을 명확히
        };
    }

    // OUT flow log for response (debug-level with truncation)
    try {
        auto rsp_preview = rsp.dump();
    LOG_FLOW("OUT corr_id=%u rsp=%s", ev.corr_id, truncate_for_log(rsp_preview, 1024).c_str());
    } catch (...) {
    LOG_FLOW("OUT corr_id=%u rsp=<non-json>", ev.corr_id);
    }
    auto out = nlohmann::json::to_cbor(rsp);
    ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_RSP, ev.corr_id, out.data(), (uint32_t)out.size());

    const auto dt = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
    const auto qd = std::chrono::duration_cast<std::chrono::microseconds>(t0 - ev.received_time).count();
    LOG_INF("IPC", "process_request done corr_id=%u q_delay(us)=%lld exec(us)=%lld rsp_size=%zu",
            ev.corr_id, (long long)qd, (long long)dt, out.size());
}

}  // namespace rtpdds

