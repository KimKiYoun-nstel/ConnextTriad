/**
 * @file dkmrtp_ipc.cpp
 * ### 파일 설명(한글)
 * DkmRtpIpc 구현 파일.
 * * Winsock 초기화, 송신(send/WSASend) 및 수신 스레드(recv/select) 루프를 포함.

 */
#include "dkmrtp_ipc.hpp"
#include <chrono>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
namespace dkmrtp {
    namespace ipc {
        static uint64_t now_ns() {
            using namespace std::chrono;
            return duration_cast<nanoseconds>(
                       steady_clock::now().time_since_epoch())
                .count();
        }
        DkmRtpIpc::DkmRtpIpc() {
        }
        DkmRtpIpc::~DkmRtpIpc() {
            stop();
        }
        bool DkmRtpIpc::open_socket(Role role, const Endpoint &ep) {
            WSADATA wsa{};
            if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
                return false;
            SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s == INVALID_SOCKET)
                return false;
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(ep.port);
            inet_pton(AF_INET, ep.address.c_str(), &addr.sin_addr);
            if (role == Role::Server) {
                if (::bind(s, (sockaddr *)&addr, sizeof(addr)) ==
                    SOCKET_ERROR) {
                    ::closesocket(s);
                    WSACleanup();
                    return false;
                }
            } else {
                if (::connect(s, (sockaddr *)&addr, sizeof(addr)) ==
                    SOCKET_ERROR) {
                    ::closesocket(s);
                    WSACleanup();
                    return false;
                }
            }
            sock_ = new SOCKET(s);
            return true;
        }
        void DkmRtpIpc::close_socket() {
            if (!sock_)
                return;
            SOCKET s = *reinterpret_cast<SOCKET *>(sock_);
            ::closesocket(s);
            delete reinterpret_cast<SOCKET *>(sock_);
            sock_ = nullptr;
            WSACleanup();
        }
        bool DkmRtpIpc::start(Role role, const Endpoint &ep) {
            role_ = role;
            ep_ = ep;
            if (!open_socket(role, ep))
                return false;
            running_ = true;
            th_ = std::thread(&DkmRtpIpc::recv_loop, this);
            return true;
        }
        void DkmRtpIpc::stop() {
            running_ = false;
            if (th_.joinable())
                th_.join();
            close_socket();
        }

        bool DkmRtpIpc::send_frame(uint16_t frame_type, uint32_t corr_id, const uint8_t *payload, uint32_t len) {
            return send_raw(frame_type, corr_id, payload, len);
        }

        bool DkmRtpIpc::send_raw(uint16_t type, uint32_t corr_id,
                                 const uint8_t *payload, uint32_t len) {
            if (!sock_)
                return false;
            Header h;
            h.type = type;
            h.corr_id = corr_id;
            h.length = len;
            h.ts_ns = now_ns();

            std::lock_guard<std::mutex> lk(send_mtx_);
            SOCKET s = *reinterpret_cast<SOCKET *>(sock_);

            if (role_ == Role::Client) {
                WSABUF bufs[2];
                bufs[0].buf = reinterpret_cast<char *>(&h);
                bufs[0].len = sizeof(h);
                bufs[1].buf = (char *)payload;
                bufs[1].len = len;
                DWORD sent = 0;
                int rc = WSASend(s, bufs, 2, &sent, 0, nullptr, nullptr);
                return rc == 0;
            } else {
                if (!last_peer_.valid)
                    return false;
                sockaddr_in peer{};
                peer.sin_family = AF_INET;
                peer.sin_addr.S_un.S_addr = last_peer_.addr_be;
                peer.sin_port = last_peer_.port_be;

                int rc = sendto(s, (const char *)&h, sizeof(h), 0,
                                (sockaddr *)&peer, sizeof(peer));
                if (rc == SOCKET_ERROR)
                    return false;
                if (payload && len) {
                    rc = sendto(s, (const char *)payload, len, 0,
                                (sockaddr *)&peer, sizeof(peer));
                    if (rc == SOCKET_ERROR)
                        return false;
                }
                return true;
            }
        }


        bool DkmRtpIpc::send_ack(uint32_t corr_id) {
            return send_raw(MSG_RSP_ACK, corr_id, nullptr, 0);
        }

        bool DkmRtpIpc::send_error(uint32_t corr_id, uint32_t code,
                                   const char *msg) {
            RspError e{code};
            size_t msglen = msg ? (strlen(msg) + 1) : 0;
            std::vector<uint8_t> buf(sizeof(e) + msglen);
            memcpy(buf.data(), &e, sizeof(e));
            if (msglen)
                memcpy(buf.data() + sizeof(e), msg, msglen);
            return send_raw(MSG_RSP_ERROR, corr_id, buf.data(),
                            (uint32_t)buf.size());
        }

        bool DkmRtpIpc::send_evt_data(const char *, const uint8_t *data,
                                      uint32_t len, uint32_t corr_id) {
            return send_raw(MSG_EVT_DATA, corr_id, data, len);
        }

        void DkmRtpIpc::set_callbacks(const Callbacks &cb) {
            cb_ = cb;
        }

        void DkmRtpIpc::recv_loop() {
            SOCKET s = *reinterpret_cast<SOCKET *>(sock_);
            std::vector<uint8_t> buf(64 * 1024);
            while (running_) {
                fd_set rfds;
                FD_ZERO(&rfds);
                FD_SET(s, &rfds);
                timeval tv{1, 0};

                int r = select(0, &rfds, nullptr, nullptr, &tv);
        // 한국어: 수신 대기(최대 1초 타임아웃) 후 데이터가 있으면 읽는다.
                if (r <= 0)
                    continue;

                int recvd = 0;
                if (role_ == Role::Server) {
                    sockaddr_in peer{};
                    int plen = sizeof(peer);
                    recvd =
                        recvfrom(s, reinterpret_cast<char *>(buf.data()),
                                 (int)buf.size(), 0,
                                 reinterpret_cast<sockaddr *>(&peer), &plen);

                    if (recvd <= (int)sizeof(Header))
                        continue;

                    last_peer_.addr_be = peer.sin_addr.S_un.S_addr;      // network-order
                    last_peer_.port_be = peer.sin_port; // network-order
                    last_peer_.valid = true;

                } else {
                    recvd = recv(s, (char *)buf.data(), (int)buf.size(), 0);

                    if (recvd <= (int)sizeof(Header))
                        continue;
                }

                Header h;
                memcpy(&h, buf.data(), sizeof(h));
                const uint8_t *payload = buf.data() + sizeof(h);
                size_t plen = recvd - sizeof(h);

                switch (h.type) {
                case MSG_FRAME_REQ:
                    if (cb_.on_request)
                        cb_.on_request(h, payload, (uint32_t)plen);
                    else if (cb_.on_unhandled)
                        cb_.on_unhandled(h);
                    break;
                case MSG_FRAME_RSP:
                    if (cb_.on_response)
                        cb_.on_response(h, payload, (uint32_t)plen);
                    else if (cb_.on_unhandled)
                        cb_.on_unhandled(h);
                    break;
                case MSG_FRAME_EVT:
                    if (cb_.on_event)
                        cb_.on_event(h, payload, (uint32_t)plen);
                    else if (cb_.on_unhandled)
                        cb_.on_unhandled(h);
                    break;

                case MSG_CMD_PARTICIPANT_CREATE:
                    if (plen >= sizeof(CmdParticipantCreate) &&
                        cb_.on_cmd_participant_create) {
                        CmdParticipantCreate cmd{};
                        memcpy(&cmd, payload, sizeof(cmd));
                        cb_.on_cmd_participant_create(h, cmd);
                    }
                    break;
                case MSG_CMD_PUBLISHER_CREATE:
                    if (plen >= sizeof(CmdPublisherCreate) &&
                        cb_.on_cmd_publisher_create) {
                        CmdPublisherCreate cmd{};
                        memcpy(&cmd, payload, sizeof(cmd));
                        cb_.on_cmd_publisher_create(h, cmd);
                    }
                    break;
                case MSG_CMD_SUBSCRIBER_CREATE:
                    if (plen >= sizeof(CmdSubscriberCreate) &&
                        cb_.on_cmd_subscriber_create) {
                        CmdSubscriberCreate cmd{};
                        memcpy(&cmd, payload, sizeof(cmd));
                        cb_.on_cmd_subscriber_create(h, cmd);
                    }
                    break;
                case MSG_CMD_PUBLISH_SAMPLE:
                    if (plen >= sizeof(CmdPublishSample) &&
                        cb_.on_cmd_publish_sample) {
                        CmdPublishSample cmd{};
                        memcpy(&cmd, payload, sizeof(cmd));
                        const uint8_t *body =
                            payload + sizeof(CmdPublishSample);
                        cb_.on_cmd_publish_sample(h, cmd, body);
                    }
                    break;
                case MSG_RSP_ACK:
                    if (cb_.on_ack)
                        cb_.on_ack(h);
                    break;
                case MSG_RSP_ERROR:
                    if (plen >= sizeof(RspError) && cb_.on_error) {
                        RspError e{};
                        memcpy(&e, payload, sizeof(e));
                        const char *msg =
                            (plen > sizeof(RspError))
                                ? (const char *)(payload + sizeof(RspError))
                                : "";
                        cb_.on_error(h, e, msg);
                    }
                    break;
                case MSG_EVT_DATA:
                    if (cb_.on_evt_data)
                        cb_.on_evt_data(h, payload);
                    break;
                }
            }
        }
    } // namespace ipc
} // namespace dkmrtp