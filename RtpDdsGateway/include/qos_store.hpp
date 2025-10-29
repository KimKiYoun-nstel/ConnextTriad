#pragma once
#include <dds/dds.hpp>
#include <dds/core/QosProvider.hpp>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <filesystem>
#include <unordered_set>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rtpdds {

struct QosPack {
    dds::domain::qos::DomainParticipantQos participant;
    dds::pub::qos::PublisherQos            publisher;
    dds::sub::qos::SubscriberQos           subscriber;
    dds::topic::qos::TopicQos              topic;
    dds::pub::qos::DataWriterQos           writer;
    dds::sub::qos::DataReaderQos           reader;
    std::string                            origin_file;
};

class QosStore {
public:
    explicit QosStore(std::string dir);
    ~QosStore();

    void initialize();

    std::optional<QosPack> find_or_reload(const std::string& lib, const std::string& profile);

    void reload_all();

    // 외부/내장 프로파일 목록 및 상세 (간단화된 effective only)
    std::vector<std::string> list_profiles(bool include_builtin = true) const;
    nlohmann::json           detail_profiles(bool include_builtin = true) const;

    static std::string extract_profile_xml(const std::string& file_path,
                                           const std::string& lib,
                                           const std::string& profile);

private:
    struct ProviderEntry {
        std::string path;
        std::shared_ptr<dds::core::QosProvider> provider;
        // cached set of "lib::profile" strings parsed from the XML file
        std::unordered_set<std::string> profiles;
        // last observed file modification time to avoid unnecessary reparsing
        std::filesystem::file_time_type mtime{};
    };

    std::vector<ProviderEntry> load_providers_from_dir_nothrow(const std::string& dir);
    static std::string key(const std::string& lib, const std::string& profile);
    std::optional<QosPack> resolve_from_providers(const std::string& lib, const std::string& profile);

private:
    std::string dir_;
    std::vector<ProviderEntry> providers_;
    // cached list of builtin candidate full profile names (e.g. "lib::profile")
    std::vector<std::string> builtin_candidates_;
    std::unordered_map<std::string, QosPack> cache_;
    mutable std::shared_mutex mtx_;
    uint64_t cache_version_ = 0;
};

} // namespace rtpdds
