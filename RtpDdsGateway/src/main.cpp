/**
 * @file main.cpp
 * ### 파일 설명(한글)
 * RtpDdsGateway 엔트리 포인트. 콘솔 인자를 파싱하여 GatewayApp을 구동.

 */

#include <iostream>

#include "gateway.hpp"
#include "rti_logger_bridge.hpp"
#include "triad_log.hpp"

#define _RTPDDS_DEBUG

int main(int argc, char** argv)
{
    using namespace rtpdds;


    std::string mode = (argc > 1) ? argv[1] : "server";
    std::string addr = (argc > 2) ? argv[2] : "127.0.0.1";
    uint16_t port = (argc > 3) ? static_cast<uint16_t>(std::stoi(argv[3])) : 25000;

    InitRtiLoggerToTriad();
#ifdef _RTPDDS_DEBUG
    // SetRtiLoggerVerbosity(rti::config::Verbosity::status_all); // 개발시
    SetRtiLoggerVerbosity(rti::config::Verbosity::warning);
    triad::set_level(triad::Lvl::Debug);
#else
    triad::set_level(triad::Lvl::Info);
#endif

    GatewayApp app;
    
    bool ok = (mode == "server") ? app.start_server(addr, port) : app.start_client(addr, port);

    if (!ok) {
        std::cerr << "failed to start gateway\n";
        return 1;
    }

    LOG_INF("Gateway", "starting mode=%s addr=%s port=%u", mode.c_str(), addr.c_str(), (unsigned)port);

    app.run();
    return 0;
}
