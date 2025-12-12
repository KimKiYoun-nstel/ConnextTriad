/**
 * @file main.cpp
 * ### 파일 설명(한글)
 * RtpDdsGateway 엔트리 포인트. 콘솔 인자를 파싱하여 GatewayApp을 구동.

 */

#include <iostream>

#include "gateway.hpp"
#include "rti_logger_bridge.hpp"
#include "triad_log.hpp"
#include "app_config.hpp"

int main(int argc, char** argv)
{
    using namespace rtpdds;

    // 1. Load Configuration
    auto& config = AppConfig::instance();
    if (config.load("agent_config.json")) {
        std::cout << "[Main] Loaded configuration from agent_config.json" << std::endl;
    } else {
        std::cout << "[Main] Using default configuration (no agent_config.json found)" << std::endl;
    }

    // 2. CLI Override (if provided)
    if (argc > 1) config.network().role = argv[1];
    if (argc > 2) config.network().ip = argv[2];
    if (argc > 3) config.network().port = static_cast<uint16_t>(std::stoi(argv[3]));
    if (argc > 4) config.dds().mode = argv[4];

    // 3. Initialize Logger
    const auto& log_cfg = config.logging();
    // Initialize logger according to config (file and console output)
    if (log_cfg.file_output || log_cfg.console_output) {
        triad::init_logger(log_cfg.log_dir, log_cfg.file_name,
                           log_cfg.max_file_size_mb, log_cfg.max_backup_files,
                           log_cfg.file_output, log_cfg.console_output);
    }

    InitRtiLoggerToTriad();

    // 4. Set Log Level
    triad::Lvl lvl = triad::Lvl::Info;
    if (log_cfg.level == "debug") lvl = triad::Lvl::Debug;
    else if (log_cfg.level == "trace") lvl = triad::Lvl::Trace;
    else if (log_cfg.level == "warn") lvl = triad::Lvl::Warn;
    else if (log_cfg.level == "error") lvl = triad::Lvl::Error;

#ifdef _RTPDDS_DEBUG
    // In debug build, keep RTI logger verbose but respect app log level from config
    SetRtiLoggerVerbosity(rti::config::Verbosity::warning);
#else
    // Release build
#endif
    triad::set_level(lvl);

    // 5. Start Application
    GatewayApp app;
    
    // map CLI receive mode to async::DdsReceiveMode
    using namespace rtpdds::async;
    DdsReceiveMode rxmode = DdsReceiveMode::WaitSet;
    std::string rx_mode_arg = config.dds().mode;

    if (!rx_mode_arg.empty()) {
        if (rx_mode_arg == "listener" || rx_mode_arg == "Listener") rxmode = DdsReceiveMode::Listener;
        else rxmode = DdsReceiveMode::WaitSet;
    }
    app.set_receive_mode(rxmode);

    std::string mode = config.network().role;
    std::string addr = config.network().ip;
    uint16_t port = config.network().port;

    bool ok = (mode == "server") ? app.start_server(addr, port) : app.start_client(addr, port);

    if (!ok) {
        std::cerr << "failed to start gateway\n";
        config.stop_watching();
        triad::shutdown_logger();
        return 1;
    }

    LOG_INF("Gateway", "starting mode=%s addr=%s port=%u rx_mode=%s", 
            mode.c_str(), addr.c_str(), (unsigned)port, rx_mode_arg.c_str());

    // Start config watching
    config.start_watching("agent_config.json");

    app.run();
    
    config.stop_watching();
    triad::shutdown_logger();
    return 0;
}
