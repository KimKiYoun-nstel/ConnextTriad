/**
 * @file gateway.cpp
 * @brief GatewayApp 구현 파일
 *
 * 서버/클라이언트 모드 진입과 종료 루틴을 담당.
 * 각 함수별로 파라미터/리턴/비즈니스 처리에 대한 상세 주석을 추가.
 */
#include "gateway.hpp"
#include <iostream>
#include <thread>

namespace rtpdds {

/**
 * @brief GatewayApp 생성자
 * 내부적으로 DdsManager, IpcAdapter를 초기화할 준비만 함
 */
GatewayApp::GatewayApp() {
    // 별도 초기화 필요 없음 (멤버 기본 생성)
}

/**
 * @brief 서버 모드 시작
 * @param bind 바인드 주소
 * @param port 바인드 포트
 * @return 서버 시작 성공 여부
 *
 * 내부적으로 IpcAdapter를 생성하고 서버 모드로 시작
 */
bool GatewayApp::start_server(const std::string &bind, uint16_t port) {
    ipc_.reset(new IpcAdapter(mgr_)); // 기존 어댑터 해제 후 새로 할당
    return ipc_->start_server(bind, port);
}

/**
 * @brief 클라이언트 모드 시작
 * @param peer 접속할 서버 주소
 * @param port 접속할 서버 포트
 * @return 클라이언트 시작 성공 여부
 *
 * 내부적으로 IpcAdapter를 생성하고 클라이언트 모드로 시작
 */
bool GatewayApp::start_client(const std::string &peer, uint16_t port) {
    ipc_.reset(new IpcAdapter(mgr_)); // 기존 어댑터 해제 후 새로 할당
    return ipc_->start_client(peer, port);
}

/**
 * @brief 메인 루프 실행
 *
 * 실제 서비스에서는 이벤트 루프/종료 신호 처리 등이 추가될 수 있음
 */
void GatewayApp::run() {
    std::cout << "[Gateway] running. Press Ctrl+C to exit.\n";
    while (true)
        std::this_thread::sleep_for(std::chrono::seconds(1)); // 단순 대기 루프
}

/**
 * @brief 종료 및 자원 해제
 *
 * IpcAdapter를 해제하여 IPC 서버/클라이언트 종료
 */
void GatewayApp::stop() {
    ipc_.reset();
}

} // namespace rtpdds
