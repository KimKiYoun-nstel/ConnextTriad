/**
 * @file gateway.cpp
 * ### 파일 설명(한글)
 * GatewayApp 구현 파일.
 * * 서버/클라이언트 모드 진입과 종료 루틴을 담당.

 */
#include "gateway.hpp"
#include <iostream>
#include <thread>
namespace rtpdds {
    GatewayApp::GatewayApp() {
    }
    bool GatewayApp::start_server(const std::string &bind, uint16_t port) {
        ipc_.reset(new IpcAdapter(mgr_));
        return ipc_->start_server(bind, port);
    }
    bool GatewayApp::start_client(const std::string &peer, uint16_t port) {
        ipc_.reset(new IpcAdapter(mgr_));
        return ipc_->start_client(peer, port);
    }
    void GatewayApp::run() {
        std::cout << "[Gateway] running. Press Ctrl+C to exit.\n";
        while (true)
            std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    void GatewayApp::stop() {
        ipc_.reset();
    }
} // namespace rtpdds