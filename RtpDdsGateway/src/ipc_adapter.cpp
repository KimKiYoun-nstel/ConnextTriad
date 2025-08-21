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
        cb.on_cmd_participant_create =
            [this](const dkmrtp::ipc::Header &h,
                   const dkmrtp::ipc::CmdParticipantCreate &cmd) {
                LOG_INF(
                    "IPC", "CMD_PARTICIPANT_CREATE id=%u domain=%d qos=%s::%s",
                    h.corr_id, cmd.domain_id, cmd.qos_library, cmd.qos_profile);
                const bool ok = mgr_.create_participant(
                    cmd.domain_id, cmd.qos_library, cmd.qos_profile);
                if (ok) {
                    ipc_.send_ack(h.corr_id);
        // 한국어: 명령 성공 → UI에 ACK 응답
                    LOG_INF("IPC", "ACK id=%u", h.corr_id);
                } else {
                    ipc_.send_error(h.corr_id, 1, "participant create failed");
                    LOG_ERR("IPC", "ERR id=%u code=1", h.corr_id);
                }
            };

        cb.on_cmd_publisher_create =
            [this](const dkmrtp::ipc::Header &h,
                   const dkmrtp::ipc::CmdPublisherCreate &cmd) {
                LOG_INF(
                    "IPC",
                    "CMD_PUBLISHER_CREATE id=%u topic=%s type=%s qos=%s::%s",
                    h.corr_id, cmd.topic, cmd.type_name, cmd.qos_library,
                    cmd.qos_profile);
                const bool ok = mgr_.create_writer(
                    cmd.topic, cmd.type_name, cmd.qos_library, cmd.qos_profile);
                if (ok) {
                    ipc_.send_ack(h.corr_id);
                    LOG_INF("IPC", "ACK id=%u", h.corr_id);
                } else {
                    ipc_.send_error(h.corr_id, 2, "writer create failed");
                    LOG_ERR("IPC", "ERR id=%u code=2", h.corr_id);
                }
            };

        cb.on_cmd_subscriber_create =
            [this](const dkmrtp::ipc::Header &h,
                   const dkmrtp::ipc::CmdSubscriberCreate &cmd) {
                LOG_INF(
                    "IPC",
                    "CMD_SUBSCRIBER_CREATE id=%u topic=%s type=%s qos=%s::%s",
                    h.corr_id, cmd.topic, cmd.type_name, cmd.qos_library,
                    cmd.qos_profile);
                const bool ok = mgr_.create_reader(
                    cmd.topic, cmd.type_name, cmd.qos_library, cmd.qos_profile);
                if (ok) {
                    ipc_.send_ack(h.corr_id);
                    LOG_INF("IPC", "ACK id=%u", h.corr_id);
                } else {
                    ipc_.send_error(h.corr_id, 3, "reader create failed");
                    LOG_ERR("IPC", "ERR id=%u code=3", h.corr_id);
                }
            };

        cb.on_cmd_publish_sample =
            [this](const dkmrtp::ipc::Header &h,
                   const dkmrtp::ipc::CmdPublishSample &cmd,
                   const uint8_t *body) {
                std::string text;
                if (cmd.content_len)
                    text.assign(reinterpret_cast<const char *>(body),
                                cmd.content_len);
                LOG_INF("IPC", "CMD_PUBLISH_SAMPLE id=%u topic=%s bytes=%u",
                        h.corr_id, cmd.topic, cmd.content_len);
                const bool ok = mgr_.publish_text(cmd.topic, text);
                if (ok) {
                    ipc_.send_ack(h.corr_id);
                    LOG_INF("IPC", "ACK id=%u", h.corr_id);
                } else {
                    ipc_.send_error(h.corr_id, 4, "publish failed");
                    LOG_ERR("IPC", "ERR id=%u code=4", h.corr_id);
                }
            };

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
                    std::string qos = req["args"].value(
                        "qos", "TriadQosLib::DefaultReliable");
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
                    std::string topic = target.value("topic", "");
                    std::string type = target.value("type", "StringMsg");
                    std::string qos = req["args"].value(
                        "qos", "TriadQosLib::DefaultReliable");
                    std::string lib = "", prof = "";
                    auto p = qos.find("::");
                    if (p != std::string::npos) {
                        lib = qos.substr(0, p);
                        prof = qos.substr(p + 2);
                    }
                    ok = mgr_.create_writer(topic, type, lib, prof);
                    if (ok)
                        rsp = {{"ok", true}};
                } else if (op == "create" && kind == "reader") {
                    std::string topic = target.value("topic", "");
                    std::string type = target.value("type", "StringMsg");
                    std::string qos = req["args"].value(
                        "qos", "TriadQosLib::DefaultReliable");
                    std::string lib = "", prof = "";
                    auto p = qos.find("::");
                    if (p != std::string::npos) {
                        lib = qos.substr(0, p);
                        prof = qos.substr(p + 2);
                    }
                    ok = mgr_.create_reader(topic, type, lib, prof);
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
            [this](const std::string &topic, const std::string &text) {
                nlohmann::json evt = {{"evt", "data"},
                                      {"topic", topic},
                                      {"type", "StringMsg"},
                                      {"sample", {{"text", text}}}};
                auto out = nlohmann::json::to_cbor(evt);
                ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_EVT, 0, out.data(),
                                (uint32_t)out.size());
            });



        ipc_.set_callbacks(cb);
    }
} // namespace rtpdds