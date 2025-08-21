﻿/**
 * @file dkmrtp_ipc.hpp
 * ### 파일 설명(한글)
 * UDP 기반의 경량 IPC 라이브러리(정적). 서버/클라이언트 역할을 모두 지원.
 * * `send_raw()`로 메시지를 전송하고, 내부 수신 스레드가 수신 데이터를 콜백으로 전달한다.

 */
#pragma once
#include "dkmrtp_ipc_messages.hpp"
#include "dkmrtp_ipc_types.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif
namespace dkmrtp {
    namespace ipc {
        /** @brief UDP IPC 엔진(윈도우 Winsock 기반). 스레드 세이프한 전송/콜백을 제공. */
class DkmRtpIpc {
          public:
            using ByteVec = std::vector<uint8_t>;
            struct Callbacks {
                std::function<void(const Header &,
                                   const CmdParticipantCreate &)>
                    on_cmd_participant_create;
                std::function<void(const Header &, const CmdPublisherCreate &)>
                    on_cmd_publisher_create;
                std::function<void(const Header &, const CmdSubscriberCreate &)>
                    on_cmd_subscriber_create;
                std::function<void(const Header &, const CmdPublishSample &,
                                   const uint8_t *body)>
                    on_cmd_publish_sample;
                std::function<void(const Header &, const uint8_t *body)>
                    on_evt_data;
                std::function<void(const Header &)> on_ack;
                std::function<void(const Header &, const RspError &,
                                   const char *msg)>
                    on_error;

                /// 새로운 통합 프레임 수신 콜백(REQ/RSP/EVT) - 페이로드는
                /// 상위에서 CBOR/JSON 해석
                std::function<void(const Header &, const uint8_t *payload, uint32_t len)> on_request;
                std::function<void(const Header &, const uint8_t *payload, uint32_t len)> on_response;
                std::function<void(const Header &, const uint8_t *payload, uint32_t len)> on_event;
                /// 미처리 타입 수신 시 호출
                std::function<void(const Header &)> on_unhandled;

            };
            DkmRtpIpc();
            ~DkmRtpIpc();
            bool start(Role role, const Endpoint &ep);
            void stop();
            bool send_ack(uint32_t corr_id);
            bool send_error(uint32_t corr_id, uint32_t code, const char *msg);
            bool send_evt_data(const char *topic, const uint8_t *data,
                               uint32_t len, uint32_t corr_id = 0);
            bool send_raw(uint16_t type, uint32_t corr_id,
                          const uint8_t *payload, uint32_t len);
            void set_callbacks(const Callbacks &cb);
            bool send_frame(uint16_t frame_type, uint32_t corr_id, const uint8_t *payload, uint32_t len);

          private:
            void recv_loop();
            bool open_socket(Role role, const Endpoint &ep);
            void close_socket();

          private:
            Role role_{Role::Server};
            Endpoint ep_{};
            std::atomic<bool> running_{false};
            std::thread th_;
            void *sock_{nullptr};
            Callbacks cb_{};
            std::mutex send_mtx_;

          private:
            struct Peer {
                uint32_t addr_be{0};
                uint16_t port_be{0};
                bool valid{false};
            } last_peer_;
        };
    } // namespace ipc
} // namespace dkmrtp