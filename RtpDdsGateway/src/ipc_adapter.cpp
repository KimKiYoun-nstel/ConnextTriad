/**
 * @file ipc_adapter.cpp
 * @brief IpcAdapter 구현 파일
 *
 * UI에서 들어오는 명령을 받아 DdsManager API 호출 → 결과를 ACK/ERR로 응답.
 * DdsManager에서 수신한 샘플은 EVT_DATA로 UI에 전송.
 * 각 함수별로 파라미터/리턴/비즈니스 처리에 대한 상세 주석을 추가.
 */
#include "ipc_adapter.hpp"

#include "dds_manager.hpp"
#include "triad_log.hpp"
#include "sample_factory.hpp"

#include <nlohmann/json.hpp>

namespace rtpdds
{

/**
 * @brief DdsManager 참조로 어댑터 생성, 콜백 설치
 * @param mgr DDS 엔티티/샘플 관리 참조
 */
IpcAdapter::IpcAdapter(DdsManager& mgr) : mgr_(mgr)
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
 * - on_request: UI에서 들어오는 JSON 기반 RPC 명령을 파싱하여 DdsManager API 호출, 결과를 CBOR로 응답
 * - set_on_sample: DDS 샘플 수신 시 EVT 프레임으로 UI에 전송
 */
void IpcAdapter::install_callbacks()
{
    dkmrtp::ipc::DkmRtpIpc::Callbacks cb{};
    // on_cmd_XX 관련 콜백 제거됨. REQ/RSP/EVT 구조만 남김.

    // === Unified RPC Envelope (CBOR over IPC) ===
    cb.on_request = [this](const dkmrtp::ipc::Header& h, const uint8_t* body, uint32_t len) {
        try {
            nlohmann::json req = nlohmann::json::from_cbor(std::vector<uint8_t>(body, body + len));
            auto op = req.value("op", std::string());
            auto target = req.value("target", nlohmann::json::object());
            auto kind = target.value("kind", std::string());
            nlohmann::json rsp;
            bool ok = false;

            // 상세 로깅: 수신 JSON, 주요 파라미터
            LOG_DBG("IPC", "Received IPC request: %s", req.dump().c_str());
            LOG_DBG("IPC", "op=%s, kind=%s, target=%s, args=%s", op.c_str(), kind.c_str(), target.dump().c_str(), req["args"].dump().c_str());

            if (op == "create" && kind == "participant") {
                int domain = req["args"].value("domain", 0);
                std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
                std::string lib = "", prof = "";
                auto p = qos.find("::");
                if (p != std::string::npos) {
                    lib = qos.substr(0, p);
                    prof = qos.substr(p + 2);
                }
                LOG_DBG("IPC", "Calling DdsManager::create_participant(domain=%d, lib=%s, prof=%s)", domain, lib.c_str(), prof.c_str());
                DdsResult res = mgr_.create_participant(domain, lib, prof);
                if (res.ok) {
                    LOG_INF("IPC", "participant created: domain=%d qos=%s", domain, qos.c_str());
                    rsp = {{"ok", true}, {"result", {{"action", "participant created"}, {"domain", domain}}}};
                } else {
                    LOG_ERR("IPC", "participant creation failed: domain=%d category=%d reason=%s", domain,
                            (int)res.category, res.reason.c_str());
                    rsp = {{"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason}};
                }
            } else if (op == "create" && kind == "publisher") {
                int domain = req["args"].value("domain", 0);
                std::string pub = req["args"].value("publisher", "pub1");
                std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
                std::string lib = "", prof = "";
                auto p = qos.find("::");
                if (p != std::string::npos) {
                    lib = qos.substr(0, p);
                    prof = qos.substr(p + 2);
                }
                LOG_DBG("IPC", "Calling DdsManager::create_publisher(domain=%d, pub=%s, lib=%s, prof=%s)", domain, pub.c_str(), lib.c_str(), prof.c_str());
                DdsResult res = mgr_.create_publisher(domain, pub, lib, prof);
                if (res.ok) {
                    LOG_INF("IPC", "publisher created: domain=%d pub=%s qos=%s", domain, pub.c_str(), qos.c_str());
                    rsp = {{"ok", true},
                           {"result", {{"action", "publisher created"}, {"domain", domain}, {"publisher", pub}}}};
                } else {
                    LOG_ERR("IPC", "publisher creation failed: domain=%d pub=%s category=%d reason=%s", domain,
                            pub.c_str(), (int)res.category, res.reason.c_str());
                    rsp = {{"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason}};
                }
            } else if (op == "create" && kind == "subscriber") {
                int domain = req["args"].value("domain", 0);
                std::string sub = req["args"].value("subscriber", "sub1");
                std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
                std::string lib = "", prof = "";
                auto p = qos.find("::");
                if (p != std::string::npos) {
                    lib = qos.substr(0, p);
                    prof = qos.substr(p + 2);
                }
                LOG_DBG("IPC", "Calling DdsManager::create_subscriber(domain=%d, sub=%s, lib=%s, prof=%s)", domain, sub.c_str(), lib.c_str(), prof.c_str());
                DdsResult res = mgr_.create_subscriber(domain, sub, lib, prof);
                if (res.ok) {
                    LOG_INF("IPC", "subscriber created: domain=%d sub=%s qos=%s", domain, sub.c_str(), qos.c_str());
                    rsp = {{"ok", true},
                           {"result", {{"action", "subscriber created"}, {"domain", domain}, {"subscriber", sub}}}};
                } else {
                    LOG_ERR("IPC", "subscriber creation failed: domain=%d sub=%s category=%d reason=%s", domain,
                            sub.c_str(), (int)res.category, res.reason.c_str());
                    rsp = {{"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason}};
                }
            } else if (op == "create" && kind == "writer") {
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
                    LOG_ERR("IPC", "writer creation failed: missing topic or type tag");
                    rsp = {{"ok", false}, {"err", 6}, {"msg", "Missing topic or type tag"}};
                } else {
                    LOG_DBG("IPC", "Calling DdsManager::create_writer(domain=%d, pub=%s, topic=%s, type=%s, lib=%s, prof=%s)", domain, pub.c_str(), topic.c_str(), type.c_str(), lib.c_str(), prof.c_str());
                    DdsResult res = mgr_.create_writer(domain, pub, topic, type, lib, prof);
                    if (res.ok) {
                        LOG_INF("IPC", "writer created: domain=%d pub=%s topic=%s type=%s", domain, pub.c_str(), topic.c_str(), type.c_str());
                        rsp = {{"ok", true},
                               {"result", {{"action", "writer created"}, {"domain", domain}, {"publisher", pub}, {"topic", topic}, {"type", type}}}};
                    } else {
                        LOG_ERR("IPC", "writer creation failed: domain=%d pub=%s topic=%s type=%s category=%d reason=%s", domain, pub.c_str(), topic.c_str(), type.c_str(), (int)res.category, res.reason.c_str());
                        rsp = {{"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason}};
                    }
                }
            } else if (op == "create" && kind == "reader") {
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
                    LOG_ERR("IPC", "reader creation failed: missing topic or type tag");
                    rsp = {{"ok", false}, {"err", 6}, {"msg", "Missing topic or type tag"}};
                } else {
                    LOG_DBG("IPC", "Calling DdsManager::create_reader(domain=%d, sub=%s, topic=%s, type=%s, lib=%s, prof=%s)", domain, sub.c_str(), topic.c_str(), type.c_str(), lib.c_str(), prof.c_str());
                    DdsResult res = mgr_.create_reader(domain, sub, topic, type, lib, prof);
                    if (res.ok) {
                        LOG_INF("IPC", "reader created: domain=%d sub=%s topic=%s type=%s", domain, sub.c_str(), topic.c_str(), type.c_str());
                        rsp = {{"ok", true},
                               {"result", {{"action", "reader created"}, {"domain", domain}, {"subscriber", sub}, {"topic", topic}, {"type", type}}}};
                    } else {
                        LOG_ERR("IPC", "reader creation failed: domain=%d sub=%s topic=%s type=%s category=%d reason=%s", domain, sub.c_str(), topic.c_str(), type.c_str(), (int)res.category, res.reason.c_str());
                        rsp = {{"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason}};
                    }
                }
            } else if (op == "write" && kind == "writer") {
                std::string topic = target.value("topic", "");
                std::string text = req["data"].value("text", "");
                if (topic.empty()) {
                    LOG_ERR("IPC", "publish_text failed: missing topic tag");
                    rsp = {{"ok", false}, {"err", 6}, {"msg", "Missing topic tag"}};
                } else {
                    LOG_DBG("IPC", "Calling DdsManager::publish_text(topic=%s, text=%s)", topic.c_str(), text.c_str());
                    DdsResult res = mgr_.publish_text(topic, text);
                    if (res.ok) {
                        LOG_INF("IPC", "publish_text ok: topic=%s", topic.c_str());
                        rsp = {{"ok", true}, {"result", {{"action", "publish ok"}, {"topic", topic}}}};
                    } else {
                        LOG_ERR("IPC", "publish_text failed: topic=%s category=%d reason=%s", topic.c_str(), (int)res.category, res.reason.c_str());
                        rsp = {{"ok", false}, {"err", 4}, {"category", (int)res.category}, {"msg", res.reason}};
                    }
                }
            } else if (op == "hello") {
                LOG_DBG("IPC", "Received hello op");
                ok = true;
                rsp = {{"ok", true},
                       {"result",
                        {{"proto", 1},
                         {"cap", {"create.participant", "create.writer", "create.reader", "write", "evt.data"}}}}};
            }

            if (!ok && rsp.empty()) rsp = {{"ok", false}, {"err", 4}, {"msg", "unsupported or failed"}};

            auto out = nlohmann::json::to_cbor(rsp);
            ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_RSP, h.corr_id, out.data(), (uint32_t)out.size());
        } catch (const std::exception& ex) {
            LOG_ERR("IPC", "Exception in on_request: %s", ex.what());
            nlohmann::json rsp = {{"ok", false}, {"err", 7}, {"msg", "json/cbor error"}};
            auto out = nlohmann::json::to_cbor(rsp);
            ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_RSP, h.corr_id, out.data(), (uint32_t)out.size());
        } catch (...) {
            LOG_ERR("IPC", "Unknown exception in on_request. req may be malformed.");
            nlohmann::json rsp = {{"ok", false}, {"err", 7}, {"msg", "json/cbor error"}};
            auto out = nlohmann::json::to_cbor(rsp);
            ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_RSP, h.corr_id, out.data(), (uint32_t)out.size());
        }
    };



    // DDS에서 샘플 수신 시 EVT 전송(JSON→CBOR)
    mgr_.set_on_sample([this](const std::string& topic, const std::string& type_name, const AnyData& data) {
        nlohmann::json display;
        auto it = sample_to_json.find(type_name);
        if (it != sample_to_json.end()) {
            display = it->second(data);
        } else {
            // fallback: any를 string으로 변환 시도, 실패 시 null
            try {
                display = nlohmann::json{{"raw"}};
            } catch (...) {
                display = nullptr;
            }
        }
        nlohmann::json evt = { {"evt", "data"}, {"topic", topic}, {"type", type_name}, {"display", display} };
        auto out = nlohmann::json::to_cbor(evt);
        ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_EVT, 0, out.data(), (uint32_t)out.size());
    });

    ipc_.set_callbacks(cb);
}

}  // namespace rtpdds
