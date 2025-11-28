#pragma once

#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

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
        bool use_file_logging = false; // Default: console only
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
};
