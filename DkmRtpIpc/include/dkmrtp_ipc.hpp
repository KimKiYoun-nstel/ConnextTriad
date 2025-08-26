// =========================
// File: DkmRtpIpc/include/dkmrtp_ipc.hpp
// =========================
#pragma once
#include "dkmrtp_ipc_messages.hpp"
#include "dkmrtp_ipc_types.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4201) // winsock 내부 nameless struct/union 경고 억제
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#pragma warning(pop)
#endif

namespace dkmrtp {
    namespace ipc {

        using PeerId = std::string; // "ip:port"

        class DkmRtpIpc {
          public:
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

                // 프레임 수신 (프레이밍만 구분되고, 상위 해석은 게이트웨이에서)
                std::function<void(const Header &, const uint8_t *, uint32_t)>
                    on_request; // MSG_FRAME_REQ
                std::function<void(const Header &, const uint8_t *, uint32_t)>
                    on_response; // MSG_FRAME_RSP
                std::function<void(const Header &, const uint8_t *, uint32_t)>
                    on_event; // MSG_FRAME_EVT

                std::function<void(const Header &)> on_unhandled;
            };

            // PeerId까지 함께 넘겨주는 확장 콜백 (멀티 클라이언트 대응)
            struct CallbacksEx {
                std::function<void(const PeerId &, const Header &,
                                   const uint8_t *, uint32_t)>
                    on_request;
                std::function<void(const PeerId &, const Header &,
                                   const uint8_t *, uint32_t)>
                    on_response;
                std::function<void(const PeerId &, const Header &,
                                   const uint8_t *, uint32_t)>
                    on_event;
            };

            DkmRtpIpc();
            ~DkmRtpIpc();

            bool start(Role role, const Endpoint &ep);
            void stop();

            // 전송 헬퍼
            bool send_ack(uint32_t corr_id);
            bool send_error(uint32_t corr_id, uint32_t code, const char *msg);
            bool send_evt_data(const char *topic, const uint8_t *data,
                               uint32_t len, uint32_t corr_id = 0);

            // 프레임/로우 전송
            bool send_raw(uint16_t type, uint32_t corr_id,
                          const uint8_t *payload, uint32_t len);
            bool send_frame(uint16_t frame_type, uint32_t corr_id,
                            const uint8_t *payload, uint32_t len);

            // 콜백 등록
            void set_callbacks(const Callbacks &cb);
            void set_callbacks_ex(const CallbacksEx &cb);

            // Peer 관리/전송
            std::vector<PeerId> list_peers() const;
            bool is_peer_valid(const PeerId &id) const;
            bool remove_peer(const PeerId &id);

            bool send_raw_to(const PeerId &id, uint16_t type, uint32_t corr_id,
                             const uint8_t *payload, uint32_t len);
            bool broadcast_raw(uint16_t type, uint32_t corr_id,
                               const uint8_t *payload, uint32_t len);

          private:
            struct IpcPeer {
                sockaddr_in addr; // <-- 중괄호 초기화 쓰지 말고 선언만
                bool valid;
                uint64_t last_active_ns;
            };

            // 상태
            Role role_{Role::Server};
            Endpoint ep_{};       // 바인드/커넥트 대상
            void *sock_{nullptr}; // SOCKET 저장용(포인터로 캐스팅)
            std::atomic<bool> running_{false};
            std::thread rx_thread_{};

            using PeerMap = std::unordered_map<std::string, IpcPeer>;

            // 동기화/라우팅
            mutable std::mutex peers_mtx_;
            std::mutex send_mtx_;
            PeerMap peers_;

            // 콜백
            Callbacks cb_{};
            CallbacksEx cb_ex_{};

            // 내부 유틸
            static PeerId make_peer_id(const sockaddr_in &a);
            static uint64_t now_ns();

            bool open_socket(Role role, const Endpoint &ep);
            void close_socket();
            void recv_loop();
        };

    } // namespace ipc
} // namespace dkmrtp
