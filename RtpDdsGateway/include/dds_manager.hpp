#pragma once
/**
 * @file dds_manager.hpp
 * @brief DDS 엔티티(Participant/Publisher/Subscriber/Topic/Writer/Reader) 생성 및 수명/소유권 관리 핵심 매니저.
 *
 * UI(IPC)로부터 명령을 받아 DDS 동작을 수행하고, 필요시 수신 샘플을 콜백으로 올려줍니다.
 *
 * 연관 파일:
 *   - dds_type_registry.hpp (엔티티 동적 생성/타입 안전성)
 *   - sample_factory.hpp (샘플 변환/생성)
 *   - ipc_adapter.hpp (명령 변환/콜백)
 *   - gateway.hpp (애플리케이션 엔트리)
 */
#pragma once
#include <algorithm>
#include <atomic>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>

#include <dds/dds.hpp>
#include <nlohmann/json.hpp>

#include "dds_type_registry.hpp"
#include "idl_type_registry.hpp"
#include "type_registry.hpp"
#include "qos_store.hpp"


namespace rtpdds
{
enum class DdsErrorCategory { None, Resource, Logic };
struct DdsResult {
    bool ok;
    DdsErrorCategory category;
    std::string reason;
    DdsResult(bool success = true, DdsErrorCategory cat = DdsErrorCategory::None, std::string msg = "")
        : ok(success), category(cat), reason(std::move(msg))
    {
    }
};
/** @brief DDS 구성의 수명과 소유권을 관리하는 핵심 매니저 클래스 */
/**
 * @class DdsManager
 * @brief DDS 구성의 수명과 소유권을 관리하는 핵심 매니저 클래스
 *
 * 도메인/토픽별로 participant, publisher, subscriber, writer, reader를 안전하게 관리하며,
 * UI/IPC 명령을 받아 실제 DDS 동작을 수행하고, 샘플 수신 시 콜백을 통해 상위로 전달합니다.
 */
class DdsManager {
public:
    /** 생성자: 내부 레지스트리/유틸리티 초기화 */
    explicit DdsManager(const std::string& qos_dir = "qos");
    /** 소멸자: 스마트 포인터 기반 자원 해제 */
    ~DdsManager();

    /**
     * @brief 실행 중 모든 DDS 엔티티(Participant, Publisher, Subscriber, Writer, Reader 등)를 초기화(해제)한다.
     * @details 컨테이너를 clear()하여 리소스를 모두 해제하며, 이후 재생성 가능하다.
     */
    void clear_entities();

    /**
     * @brief 도메인ID별 participant 생성
     * @param domain_id 도메인 ID
     * @param qos_lib   QoS 라이브러리명
     * @param qos_profile QoS 프로파일명
     * @return 성공/실패 및 사유
     */
    DdsResult create_participant(int domain_id, const std::string& qos_lib, const std::string& qos_profile);

    /**
     * @brief 도메인/이름별 publisher 생성
     */
    DdsResult create_publisher(int domain_id, const std::string& pub_name, const std::string& qos_lib,
                               const std::string& qos_profile);

    /**
     * @brief 도메인/이름별 subscriber 생성
     */
    DdsResult create_subscriber(int domain_id, const std::string& sub_name, const std::string& qos_lib,
                                const std::string& qos_profile);

    /**
     * @brief 도메인/이름/토픽/타입별 writer 생성
     */
    // out_id: optional output parameter to receive the created writer id
    DdsResult create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
                            const std::string& type_name, const std::string& qos_lib, const std::string& qos_profile,
                            uint64_t* out_id = nullptr);

    /**
     * @brief 도메인/이름/토픽/타입별 reader 생성
     */
    // out_id: optional output parameter to receive the created reader id
    DdsResult create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
                            const std::string& type_name, const std::string& qos_lib, const std::string& qos_profile,
                            uint64_t* out_id = nullptr);

    /**
     * @brief 토픽에 JSON 객체 기반 샘플 publish (기본 도메인/퍼블리셔)
     * @param topic 토픽명
     * @param j JSON 객체(타입별 필드)
     */
    DdsResult publish_json(const std::string& topic, const nlohmann::json& j);

    /**
     * @brief 도메인/퍼블리셔/토픽별로 JSON 객체 기반 샘플 publish
     */
    DdsResult publish_json(int domain_id, const std::string& pub_name, const std::string& topic,
                           const nlohmann::json& j);

    /**
     * @brief 샘플 수신 콜백 핸들러 타입
     */
    using SampleHandler = SampleCallback;

    /**
     * @brief 샘플 수신 시 호출될 콜백 등록
     * @param cb 콜백 함수
     */
    void set_on_sample(SampleHandler cb);

    // Return available QoS profiles and detailed summary (for IPC get.qos)
    // Parameters:
    //  - include_builtin: when true include builtin QoS candidates, otherwise only external providers
    //  - include_detail: when true include detailed profile info under the "detail" key
    nlohmann::json list_qos_profiles(bool include_builtin = false, bool include_detail = false) const;

    /**
     * @brief QoS 프로파일을 동적으로 추가 또는 업데이트합니다 (IPC set.qos용)
     * @param library Library 이름
     * @param profile Profile 이름
     * @param profile_xml qos_profile 태그를 포함한 XML 조각
     * @return 성공 시 "library::profile" 형식의 전체 이름, 실패 시 빈 문자열
     */
    std::string add_or_update_qos_profile(const std::string& library, 
                                          const std::string& profile, 
                                          const std::string& profile_xml);

    /**
     * @brief 토픽명으로 타입명을 조회합니다.
     * @param topic 토픽명
     * @return 타입명(없으면 빈 문자열)
     */
    std::string get_type_for_topic(const std::string& topic) const {
        // domain-aware lookup: return first matching type across domains (if any)
        for (const auto &dom : topic_to_type_) {
            auto it = dom.second.find(topic);
            if (it != dom.second.end()) return it->second;
        }
        return std::string();
    }

    
private:
    TypeRegistry type_registry_;
    std::unique_ptr<rtpdds::QosStore> qos_store_;
    
    // 스레드 동기화: DDS 엔티티 컨테이너 보호용 뮤텍스
    mutable std::mutex mutex_;

    // 내부 헬퍼: mutex 이미 획득한 상태에서 pub/sub 생성 (재귀 호출 방지)
    DdsResult create_publisher_locked(int domain_id, const std::string& pub_name, 
                                      const std::string& qos_lib, const std::string& qos_profile);
    DdsResult create_subscriber_locked(int domain_id, const std::string& sub_name,
                                       const std::string& qos_lib, const std::string& qos_profile);

    // 적용 로깅 보조: 요약 tag/value 문자열
    static std::string summarize_qos(const rtpdds::QosPack& pack);
    // 도메인ID별 participant
    std::unordered_map<int, std::shared_ptr<dds::domain::DomainParticipant> > participants_;
    // 도메인/이름별 publisher
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<dds::pub::Publisher> > > publishers_;
    // 도메인/이름별 subscriber
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<dds::sub::Subscriber> > > subscribers_;
    // 도메인/이름/토픽별 writer (여러 writer 허용)
    using HolderId = uint64_t;
    struct WriterEntry { HolderId id; std::shared_ptr<IWriterHolder> holder; };
    struct ReaderEntry { HolderId id; std::shared_ptr<IReaderHolder> holder; };

    // writers: domain -> publisher name -> topic -> vector of entries
    std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, std::vector<WriterEntry> > > > writers_;
    // readers: domain -> subscriber name -> topic -> vector of entries
    std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, std::vector<ReaderEntry> > > > readers_;

    // next id generator for writer/reader entries
    std::atomic<HolderId> next_holder_id_{1};

    // 샘플 수신 콜백
    SampleHandler on_sample_;

    // 토픽명 → 타입명 매핑 (도메인별 분리)
    std::unordered_map<int, std::unordered_map<std::string, std::string>> topic_to_type_{};
    // 도메인/토픽별 topic holder (participant 스코프)
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<ITopicHolder>>> topics_;

    // Remove individual writer/reader by id
    DdsResult remove_writer(HolderId id);
    DdsResult remove_reader(HolderId id);
};
}  // namespace rtpdds
