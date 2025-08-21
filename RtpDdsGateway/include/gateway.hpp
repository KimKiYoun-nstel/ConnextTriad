/**
 * @file gateway.hpp
 * ### 파일 설명(한글)
 * RtpDdsGateway 애플리케이션 엔트리: IPC 서버/클라이언트 시작과 DdsManager 초기화를 담당.
 * * 콘솔 인자에 따라 서버/클라이언트 모드를 선택한다.

 */
#pragma once
#include "dds_manager.hpp"
#include "ipc_adapter.hpp"
#include <memory>
namespace rtpdds {
    /** @brief 게이트웨이 애플리케이션 수명주기 관리자 */
class GatewayApp {
      public:
        GatewayApp();
        bool start_server(const std::string &bind, uint16_t port);
        bool start_client(const std::string &peer, uint16_t port);
        void run();
        void stop();

      private:
        DdsManager mgr_{};
        std::unique_ptr<IpcAdapter> ipc_{};
    };
} // namespace rtpdds