/**
 * @file gateway.hpp
 * @brief RtpDdsGateway 애플리케이션 엔트리: IPC 서버/클라이언트 시작과 DdsManager 초기화 담당
 *
 * 콘솔 인자에 따라 서버/클라이언트 모드를 선택하며, 전체 시스템의 수명주기를 관리합니다.
 *
 * 연관 파일:
 *   - dds_manager.hpp (엔티티 관리)
 *   - ipc_adapter.hpp (명령 변환/콜백)
 */
#pragma once
#include <memory>

#include "dds_manager.hpp"
#include "dds_type_registry.hpp"
#include "ipc_adapter.hpp"
// async pipeline
#include "async/async_event_processor.hpp"
#include "async/sample_event.hpp"
#include "async/receiver_factory.hpp"

namespace rtpdds
{
/** @brief 게이트웨이 애플리케이션 수명주기 관리자 */
/**
 * @class GatewayApp
 * @brief 게이트웨이 애플리케이션 수명주기 관리자
 *
 * 서버/클라이언트 모드 전환, DdsManager 및 IPC 어댑터 초기화, run/stop 등 전체 실행 흐름을 담당합니다.
 */
class GatewayApp {
public:
    /** 생성자: 내부 매니저/어댑터 초기화 */
    GatewayApp();
    /** 서버 모드 시작 (bind/port 지정) */
    bool start_server(const std::string& bind, uint16_t port);
    /** 클라이언트 모드 시작 (peer/port 지정) */
    bool start_client(const std::string& peer, uint16_t port);
    /** 메인 루프 실행 */
    void run();
    /** 종료 및 자원 해제 */
    void stop();

private:
    DdsManager mgr_{};              ///< DDS 엔티티/샘플 관리
    std::unique_ptr<IpcAdapter> ipc_{}; ///< IPC 명령 변환/콜백 어댑터
    
    // 추가: DDS/IPC 이벤트를 처리할 소비자 스레드
    async::AsyncEventProcessor async_;
    async::DdsReceiveMode rx_mode_{async::DdsReceiveMode::Listener};
    std::unique_ptr<async::IDdsReceiver> rx_{};
};
}  // namespace rtpdds
