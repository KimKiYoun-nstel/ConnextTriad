#pragma once

#include <string>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>

#include "triad_log.hpp"
#include "triad_thread.hpp"

class AppConfig {
public:
    struct NetworkConfig {
        std::string role = "server"; // "server" or "client"
        std::string ip = "0.0.0.0";
        uint16_t port = 25000;
    };

    struct DdsConfig {
        std::string qos_dir = "qos";
        std::string mode = "waitset"; // "waitset" or "listener"
    };

    struct LogConfig {
        bool file_output = true; // 파일 출력 선택
        std::string log_dir = "logs";
        std::string file_name = "agent.log";
        std::string level = "info"; // debug, info, warn, error
        bool console_output = true;
        int max_file_size_mb = 10;
        int max_backup_files = 5;
    };

    static AppConfig& instance();

    // Load configuration from a JSON file.
    // Returns true if successful, false otherwise.
    bool load(const std::string& path);

    // Start watching config file for changes (runs in background thread)
    void start_watching(const std::string& path);

    // Stop watching config file
    void stop_watching();

    const NetworkConfig& network() const { return network_; }
    const DdsConfig& dds() const { return dds_; }
    const LogConfig& logging() const { return logging_; }

    NetworkConfig& network() { return network_; }
    DdsConfig& dds() { return dds_; }
    LogConfig& logging() { return logging_; }

private:
    AppConfig() = default;
    ~AppConfig() = default;
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    NetworkConfig network_;
    DdsConfig dds_;
    LogConfig logging_;

    std::string config_path_;
    triad::TriadThread watch_thread_;
    std::atomic<bool> watching_ = false;
    mutable std::mutex config_mutex_;
};
