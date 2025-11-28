#include "app_config.hpp"
#include <fstream>
#include <iostream>

AppConfig& AppConfig::instance() {
    static AppConfig inst;
    return inst;
}

bool AppConfig::load(const std::string& path) {
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
            logging_.use_file_logging = true; // Enable file logging if config exists
            auto& log = j["logging"];
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
