#include "app_config.hpp"
#include "../../DkmRtpIpc/include/triad_thread.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>

AppConfig& AppConfig::instance() {
    static AppConfig inst;
    return inst;
}

bool AppConfig::load(const std::string& path) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << path << std::endl;
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;

        // Network
        if (j.contains("network")) {
            auto& net = j["network"];
            network_.role = net.value("role", network_.role);
            network_.ip = net.value("ip", network_.ip);
            network_.port = net.value("port", network_.port);
        }

        // DDS
        if (j.contains("dds")) {
            auto& dds = j["dds"];
            dds_.qos_dir = dds.value("qos_dir", dds_.qos_dir);
            dds_.mode = dds.value("mode", dds_.mode);
        }

        // Logging
        if (j.contains("logging")) {
            auto& log = j["logging"];
            logging_.file_output = log.value("file_output", logging_.file_output);
            logging_.log_dir = log.value("log_dir", logging_.log_dir);
            logging_.file_name = log.value("file_name", logging_.file_name);
            logging_.level = log.value("level", logging_.level);
            logging_.console_output = log.value("console_output", logging_.console_output);
            logging_.max_file_size_mb = log.value("max_file_size_mb", logging_.max_file_size_mb);
            logging_.max_backup_files = log.value("max_backup_files", logging_.max_backup_files);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
        return false;
    }
}

void AppConfig::start_watching(const std::string& path) {
    if (watching_) return;
    config_path_ = path;
    watching_ = true;
    
    auto watch_func = [this]() {
#ifndef RTI_VXWORKS
        triad::set_thread_name("DA_CfgWatch");
#endif
        std::string last_level;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            last_level = logging_.level;
        }
        while (watching_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            if (!watching_) break;
            
            if (load(config_path_)) {
                std::lock_guard<std::mutex> lock(config_mutex_);
                if (logging_.level != last_level) {
                    // Update log level
                    triad::Lvl lvl = triad::Lvl::Info;
                    if (logging_.level == "debug") lvl = triad::Lvl::Debug;
                    else if (logging_.level == "trace") lvl = triad::Lvl::Trace;
                    else if (logging_.level == "warn") lvl = triad::Lvl::Warn;
                    else if (logging_.level == "error") lvl = triad::Lvl::Error;
                    triad::set_level(lvl);
                    last_level = logging_.level;
                }
            }
        }
    };

#ifdef RTI_VXWORKS
    watch_thread_.start(watch_func, "DA_CfgWatch");
#else
    watch_thread_ = std::thread(watch_func);
#endif
}

void AppConfig::stop_watching() {
    watching_ = false;
    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }
}
