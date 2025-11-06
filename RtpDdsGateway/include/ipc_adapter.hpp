#pragma once
/**
 * @file ipc_adapter.hpp
 * @brief UI에서 들어오는 IPC 명령을 DdsManager 동작으로 변환하고, 결과(ACK/ERR/EVT)를 UI로 반환하는 어댑터
 *
 * 명령 처리 성공/실패에 따라 적절한 응답 코드를 전송하며, IPC ↔ DDS 변환의 핵심 레이어입니다.
 *
 * 연관 파일:
 *   - dds_manager.hpp (엔티티 관리/콜백)
 *   - gateway.hpp (애플리케이션 엔트리)
 */
#pragma once
#include "dkmrtp_ipc.hpp"
#include "dkmrtp_ipc_messages.hpp"
#include "dkmrtp_ipc_types.hpp"
#include <string>
#include "async/sample_event.hpp"
namespace rtpdds
{
class DdsManager;
/** @brief IPC 명령 ↔ DDS 동작 간의 변환 레이어 */
/**
 * @class IpcAdapter
 * @brief IPC 명령 ↔ DDS 동작 간의 변환 레이어
 *
 * UI에서 들어오는 명령을 받아 DdsManager의 API를 호출하고, 결과를 UI로 반환합니다.
 * 콜백 설치, 서버/클라이언트 모드 전환, 종료 등 IPC 통신의 핵심 역할을 담당합니다.
 */
class IpcAdapter {
public:
    /** DdsManager 참조로 어댑터 생성 */
    explicit IpcAdapter(DdsManager& mgr);
    
    // 추가: Gateway의 소비자 스레드가 호출하여 IPC 이벤트 전송을 수행
    void emit_evt_from_sample(const async::SampleEvent& ev);

    // 2단계: 소비자 스레드로 보낼 post 함수 주입
    void set_command_post(std::function<void(const async::CommandEvent&)> f);

    // 2단계: 소비자 스레드에서 호출되는 실제 처리
    void process_request(const async::CommandEvent& ev);
    /** 자원 해제 및 종료 */
    ~IpcAdapter();
    /** 서버 모드 시작 (bind/port 지정) */
    bool start_server(const std::string& bind_addr, uint16_t port);
    /** 클라이언트 모드 시작 (peer/port 지정) */
    bool start_client(const std::string& peer_addr, uint16_t port);
    /** 종료 및 콜백 해제 */
    void stop();

private:
    /** 내부 콜백 설치 */
    void install_callbacks();
    DdsManager& mgr_;              ///< DDS 엔티티/샘플 관리 참조
    dkmrtp::ipc::DkmRtpIpc ipc_;   ///< IPC 통신 객체
    std::function<void(const async::CommandEvent&)> post_cmd_; // command post sink
};
}  // namespace rtpdds