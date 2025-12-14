/**
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
#include "triad_thread.hpp"
#include <vector>
#ifdef _WIN32
// Windows: winsock2 must be included before any header that pulls in winsock.h (e.g. windows.h)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
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
                // REQ/RSP/EVT 메시지만 지원. 페이로드는 CBOR/JSON 해석
                std::function<void(const Header &, const uint8_t *payload, uint32_t len)> on_request;
                std::function<void(const Header &, const uint8_t *payload, uint32_t len)> on_response;
                std::function<void(const Header &, const uint8_t *payload, uint32_t len)> on_event;
                // 미처리 타입 수신 시 호출
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
            triad::TriadThread th_; // VxWorks에서 1MB 스택 적용
            void *sock_{nullptr};
            Callbacks cb_{};
            std::mutex send_mtx_;
            std::vector<uint8_t> send_buf_;  // 재사용 송신 버퍼 (Phase 1-1)

          private:
            struct Peer {
                uint32_t addr_be{0};
                uint16_t port_be{0};
                bool valid{false};
            } last_peer_;
        };
    } // namespace ipc
} // namespace dkmrtp
