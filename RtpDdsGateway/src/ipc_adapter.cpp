/**
 * @file ipc_adapter.cpp
 * ### 파일 설명(한글)
 * IpcAdapter 구현 파일.
 * * UI에서 들어오는 명령을 받아 DdsManager API 호출 → 결과를 ACK/ERR로 응답.
 * * DdsManager에서 수신한 샘플은 EVT_DATA로 UI에 전송.

 */
#include "ipc_adapter.hpp"
#include "dds_manager.hpp"
#include "triad_log.hpp"
#include "nlohmann/json.hpp"


namespace rtpdds {
    IpcAdapter::IpcAdapter(DdsManager &mgr) : mgr_(mgr) {
        install_callbacks();
    }
    IpcAdapter::~IpcAdapter() {
        stop();
    }
    bool IpcAdapter::start_server(const std::string &bind_addr, uint16_t port) {
        return ipc_.start(dkmrtp::ipc::Role::Server, {bind_addr, port});
    }
    bool IpcAdapter::start_client(const std::string &peer_addr, uint16_t port) {
        return ipc_.start(dkmrtp::ipc::Role::Client, {peer_addr, port});
    }
    void IpcAdapter::stop() {
        ipc_.stop();
    }
    void IpcAdapter::install_callbacks() {
        dkmrtp::ipc::DkmRtpIpc::Callbacks cb{};
    // on_cmd_XX 관련 콜백 제거됨. REQ/RSP/EVT 구조만 남김.

// === Unified RPC Envelope (CBOR over IPC) ===
        cb.on_request = [this](const dkmrtp::ipc::Header &h,
                               const uint8_t *body, uint32_t len) {
            try {
                nlohmann::json req = nlohmann::json::from_cbor(
                    std::vector<uint8_t>(body, body + len));
                auto op = req.value("op", std::string());
                auto target = req.value("target", nlohmann::json::object());
                auto kind = target.value("kind", std::string());
                nlohmann::json rsp;
                bool ok = false;

                if (op == "create" && kind == "participant") {
                    int domain = req["args"].value("domain", 0);
                    std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
                    std::string lib = "", prof = "";
                    auto p = qos.find("::");
                    if (p != std::string::npos) {
                        lib = qos.substr(0, p);
                        prof = qos.substr(p + 2);
                    }
                    ok = mgr_.create_participant(domain, lib, prof);
                    if (ok)
                        rsp = {{"ok", true}};
                } else if (op == "create" && kind == "writer") {
                    int domain = req["args"].value("domain", 0);
                    std::string pub = req["args"].value("publisher", "pub1");
                    std::string topic = target.value("topic", "");
                    std::string type = target.value("type", "StringMsg");
                    std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
                    std::string lib = "", prof = "";
                    auto p = qos.find("::");
                    if (p != std::string::npos) {
                        lib = qos.substr(0, p);
                        prof = qos.substr(p + 2);
                    }
                    ok = mgr_.create_writer(domain, pub, topic, type, lib, prof);
                    if (ok)
                        rsp = {{"ok", true}};
                } else if (op == "create" && kind == "reader") {
                    int domain = req["args"].value("domain", 0);
                    std::string sub = req["args"].value("subscriber", "sub1");
                    std::string topic = target.value("topic", "");
                    std::string type = target.value("type", "StringMsg");
                    std::string qos = req["args"].value("qos", "TriadQosLib::DefaultReliable");
                    std::string lib = "", prof = "";
                    auto p = qos.find("::");
                    if (p != std::string::npos) {
                        lib = qos.substr(0, p);
                        prof = qos.substr(p + 2);
                    }
                    ok = mgr_.create_reader(domain, sub, topic, type, lib, prof);
                    if (ok)
                        rsp = {{"ok", true}};
                } else if (op == "write" && kind == "writer") {
                    std::string topic = target.value("topic", "");
                    std::string text = req["data"].value("text", "");
                    ok = mgr_.publish_text(topic, text);
                    if (ok)
                        rsp = {{"ok", true}};
                } else if (op == "hello") {
                    ok = true;
                    rsp = {{"ok", true},
                           {"result",
                            {{"proto", 1},
                             {"cap",
                              {"create.participant", "create.writer",
                               "create.reader", "write", "evt.data"}}}}};
                }

                if (!ok && rsp.empty())
                    rsp = {{"ok", false},
                           {"err", 4},
                           {"msg", "unsupported or failed"}};

                auto out = nlohmann::json::to_cbor(rsp);
                ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_RSP, h.corr_id,
                                out.data(), (uint32_t)out.size());
            } catch (...) {
                nlohmann::json rsp = {
                    {"ok", false}, {"err", 7}, {"msg", "json/cbor error"}};
                auto out = nlohmann::json::to_cbor(rsp);
                ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_RSP, h.corr_id,
                                out.data(), (uint32_t)out.size());
            }
        };

        // DDS에서 샘플 수신 시 EVT 전송(JSON→CBOR)
        mgr_.set_on_sample(
            [this](const std::string &topic, const std::string &type_name, const std::string &display) {
                nlohmann::json evt = {{"evt", "data"},
                                      {"topic", topic},
                                      {"type", type_name},
                                      {"display", display}};
                auto out = nlohmann::json::to_cbor(evt);
                ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_EVT, 0, out.data(),
                                (uint32_t)out.size());
            });



        ipc_.set_callbacks(cb);
    }
} // namespace rtpdds