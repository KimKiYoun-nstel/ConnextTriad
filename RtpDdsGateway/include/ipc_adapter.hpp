#pragma once
/**
 * @file ipc_adapter.hpp
 * @brief IPC 명령 ↔ DDS 동작 간의 변환 레이어 인터페이스
 */
#include "dkmrtp_ipc.hpp"
#include "dkmrtp_ipc_types.hpp"
#include <string>
#include "async/sample_event.hpp"
namespace rtpdds
{
class IDdsManager;
/** @brief IPC 명령 ↔ DDS 동작 간의 변환 레이어 */
/**
 * @class IpcAdapter
 * @brief IPC 명령 ↔ DDS 동작 간의 변환 및 EVT 전송 레이어
 *
 * 기능 요약:
 * - IPC 수신 프레임(CBOR/JSON)을 파싱하여 DdsManager API로 위임
 * - 처리 결과를 RSP 프레임으로 응답
 * - DDS에서 수신한 샘플을 EVT 프레임으로 변환/전송
 *
 * 스레드 모델:
 * - on_request는 IO 스레드에서 호출되어 CommandEvent로 변환 후 post 콜백으로 비동기 큐에 적재
 * - process_request는 소비자 스레드에서 실제 명령을 처리
 * - emit_evt_from_sample은 소비자 스레드에서 EVT 전송 수행
 */
class IpcAdapter {
public:
    /**
     * @brief DdsManager 참조로 어댑터 생성
     * @param mgr DDS 엔티티/샘플 관리자 참조
     */
    explicit IpcAdapter(IDdsManager& mgr);
    
    /**
     * @brief DDS 샘플 수신 이벤트를 IPC EVT로 변환/전송
     * @param ev 샘플 이벤트(토픽/타입/AnyData)
     * @details AnyData를 DDS→JSON 변환 후 CBOR로 직렬화하여 EVT 프레임으로 송신한다.
     */
    void emit_evt_from_sample(const async::SampleEvent& ev);

    /**
     * @brief CommandEvent를 비동기 큐에 투입할 post 함수 지정
     * @param f post 함수(소비자 스레드에서 처리)
     */
    void set_command_post(std::function<void(const async::CommandEvent&)> f);

    /**
     * @brief IPC 요청 처리(소비자 스레드)
     * @param ev 수신된 CommandEvent(수신 시각/코릴레이션/바디 포함)
     * @details op/target/data를 해석하여 DdsManager 동작을 수행하고, 결과를 RSP 프레임으로 응답한다.
     */
    void process_request(const async::CommandEvent& ev);
    /** 자원 해제 및 종료 */
    ~IpcAdapter();
    /**
     * @brief 서버 모드 시작
     * @param bind_addr 바인드 주소
     * @param port 포트
     * @return 시작 성공 여부
     */
    bool start_server(const std::string& bind_addr, uint16_t port);
    /**
     * @brief 클라이언트 모드 시작
     * @param peer_addr 서버 주소
     * @param port 포트
     * @return 시작 성공 여부
     */
    bool start_client(const std::string& peer_addr, uint16_t port);
    /**
     * @brief 종료 및 콜백 해제
     * @details IPC 연결을 종료하고 내부 콜백을 해제한다.
     */
    void stop();

private:
    /**
     * @brief 내부 콜백 설치
     * @details IPC의 on_request 핸들러를 설정하여 수신 프레임을 CommandEvent로 변환한다.
     */
    void install_callbacks();
    IDdsManager& mgr_;              ///< DDS 엔티티/샘플 관리 참조 (interface)
    dkmrtp::ipc::DkmRtpIpc ipc_;   ///< IPC 통신 객체
    std::function<void(const async::CommandEvent&)> post_cmd_; // command post sink
};
}  // namespace rtpdds