#pragma once
/**
 * @file qos_store.hpp
 * @brief QoS 프로파일 로딩/캐시/업데이트/조회 저장소 인터페이스
 */
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

/**
 * @brief 단일 QoS 프로파일에서 추출된 QoS 세트
 * @details 동일한 library::profile로부터 participant/publisher/subscriber/topic/writer/reader QoS를 모은 묶음
 */
struct QosPack {
    dds::domain::qos::DomainParticipantQos participant;
    dds::pub::qos::PublisherQos            publisher;
    dds::sub::qos::SubscriberQos           subscriber;
    dds::topic::qos::TopicQos              topic;
    dds::pub::qos::DataWriterQos           writer;
    dds::sub::qos::DataReaderQos           reader;
    std::string                            origin_file;
};

/**
 * @class QosStore
 * @brief QoS 프로파일 로딩/캐시/동적 업데이트/상세 조회를 담당하는 저장소
 *
 * 기능 요약:
 * - 외부 XML 디렉토리에서 QosProvider를 로드하여 library::profile를 인덱싱
 * - 요청된 library::profile을 캐시하여 재사용(find_or_reload)
 * - 런타임 XML 조각을 병합하여 동적 라이브러리를 구성(add_or_update_profile)
 * - 목록(list_profiles)과 상세(detail_profiles) 조회 제공
 *
 * 스레드 안전성:
 * - 내부 shared_mutex로 보호되어 다중 스레드 접근에 안전합니다.
 */
class QosStore {
public:
    /**
     * @brief 생성자
     * @param dir QoS XML 디렉토리 경로
     */
    explicit QosStore(std::string dir);
    ~QosStore();

    /**
     * @brief 초기화 수행(파일 스캔/Provider 생성/캐시 클리어)
     */
    void initialize();

    /**
     * @brief library::profile 검색 및 캐시 반환(필요 시 재로드)
     * @param lib 라이브러리 이름
     * @param profile 프로파일 이름
     * @return QosPack (없으면 nullopt)
     * @details 동적 provider → 외부 파일 provider → 전체 재로드 순으로 탐색합니다.
     */
    std::optional<QosPack> find_or_reload(const std::string& lib, const std::string& profile);

    /**
     * @brief 모든 Provider를 재로드하고 캐시를 무효화
     */
    void reload_all();

    /**
     * @brief 사용 가능한 프로파일 목록
     * @param include_builtin 내장 후보 포함 여부
     * @return ["lib::profile", ...] 정렬/중복 제거된 목록
     */
    std::vector<std::string> list_profiles(bool include_builtin = true) const;
    /**
     * @brief 프로파일 상세 정보(배열)
     * @param include_builtin 내장 후보 포함 여부
     * @return [{"lib::profile": { "source_kind": "external|dynamic|builtin", "xml": "..." }} ...]
     */
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

    /**
     * @brief 파일로부터 특정 library::profile의 XML 조각 추출
     * @param file_path XML 파일 경로
     * @param lib 라이브러리 이름
     * @param profile 프로파일 이름
     * @return 해당 qos_profile 블록(미존재 시 빈 문자열)
     */
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
    // 내장 후보 풀네임 캐시 (예: "lib::profile")
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
