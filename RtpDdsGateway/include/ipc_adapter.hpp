/**
 * @file ipc_adapter.hpp
 * ### 파일 설명(한글)
 * UI에서 들어오는 IPC 명령을 DdsManager 동작으로 변환하고, 결과(ACK/ERR/EVT)를 UI로 돌려보내는 어댑터.
 * * 명령 처리 성공/실패에 따라 적절한 응답 코드를 전송한다.

 */
#pragma once
#include "dkmrtp_ipc.hpp"
#include "dkmrtp_ipc_messages.hpp"
#include "dkmrtp_ipc_types.hpp"
#include <string>
namespace rtpdds {
    class DdsManager;
    /** @brief IPC 명령 ↔ DDS 동작 간의 변환 레이어 */
class IpcAdapter {
      public:
        explicit IpcAdapter(DdsManager &mgr);
        ~IpcAdapter();
        bool start_server(const std::string &bind_addr, uint16_t port);
        bool start_client(const std::string &peer_addr, uint16_t port);
        void stop();

      private:
        void install_callbacks();
        DdsManager &mgr_;
        dkmrtp::ipc::DkmRtpIpc ipc_;
    };
} // namespace rtpdds