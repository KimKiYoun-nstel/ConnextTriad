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

    /**
     * @brief XML 문자열로부터 QoS 프로파일을 동적으로 추가/업데이트
     * @param library Library 이름 (예: "NGVA_QoS_Library")
     * @param profile Profile 이름 (예: "custom_profile")
     * @param profile_xml qos_profile 태그를 포함한 XML 조각
     * @return 성공 시 "library::profile" 형식의 전체 이름, 실패 시 빈 문자열
     */
    std::string add_or_update_profile(const std::string& library, 
                                      const std::string& profile, 
                                      const std::string& profile_xml);

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
        // cached full XML content for dynamic profile merging
        std::string xml_content;
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

    // 동적 프로파일 관리 (메모리에만 존재, 파일 저장 안 함)
    // library name → 전체 <qos_library> XML (DDS 래퍼 태그 포함)
    std::unordered_map<std::string, std::string> dynamic_libraries_;
    // library name → QosProvider (str:// URI로 생성)
    std::unordered_map<std::string, std::shared_ptr<dds::core::QosProvider>> dynamic_providers_;
    // "lib::profile" → true (빠른 존재 확인용)
    std::unordered_set<std::string> dynamic_profiles_index_;
};

} // namespace rtpdds
