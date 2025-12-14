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
#include "rtpdds/api/idds_manager.hpp"
#include "async/waitset_dispatcher.hpp"

namespace rtpdds
{
/** @brief DDS 구성의 수명과 소유권을 관리하는 핵심 매니저 클래스 */
/**
 * @class DdsManager
 * @brief DDS 구성의 수명과 소유권을 관리하는 파사드(Facade)
 *
 * 기능 요약:
 * - Participant/Publisher/Subscriber/Topic/Writer/Reader 생성 및 소유권 관리
 * - 토픽별 타입 등록/검증 및 샘플 변환(JSON↔DDS)
 * - QoS 프로파일 조회/요약/동적 업데이트 위임(QosStore)
 * - 수신 샘플을 상위로 전달하는 콜백 관리
 *
 * 스레드 안전성:
 * - 모든 공개 API는 내부 mutex로 동기화되어 다중 스레드 호출에 안전합니다.
 *
 * 오류 분류(DdsErrorCategory):
 * - None: 성공
 * - Logic: 호출 전제 위반(미등록 타입/중복 생성/부모 부재 등)
 * - Resource: DDS 엔티티 생성 실패 등 런타임 자원 문제
 */
class DdsManager {
public:
    /**
     * @brief 이벤트 처리 모드
     */
    enum class EventMode {
        Listener,   ///< RTI 내부 스레드(Listener) 사용 (기본값)
        WaitSet     ///< 별도 스레드(WaitSet) 사용
    };

    /**
     * @brief 생성자
     * @param qos_dir QoS XML 탐색 디렉토리 경로
     * @details 타입/JSON 레지스트리 초기화 후 QosStore를 준비합니다.
     */
    explicit DdsManager(const std::string& qos_dir = "qos");
    /** 소멸자: 스마트 포인터 기반 자원 해제 */
    ~DdsManager();

    /**
     * @brief 이벤트 처리 모드 설정
     * @param mode 설정할 모드
     * @note 엔티티 생성 전에 설정하는 것을 권장합니다.
     */
    void set_event_mode(EventMode mode);

    /**
     * @brief 모든 DDS 엔티티 해제
     * @details
     * - 해제 순서: Reader/Writer → Topic → Subscriber/Publisher → Participant
     * - 이후 동일 인스턴스로 재생성 가능
     */
    void clear_entities();

    /**
     * @brief 도메인ID별 participant 생성
     * @param domain_id 도메인 ID
     * @param qos_lib   QoS 라이브러리명
     * @param qos_profile QoS 프로파일명
     * @return 결과(성공/실패, 분류, 사유)
     * @pre 동일 domain_id의 participant가 존재하지 않아야 합니다.
     * @note QoS 라이브러리/프로파일이 유효하지 않으면 기본 QoS로 폴백합니다.
     */
    DdsResult create_participant(int domain_id, const std::string& qos_lib, const std::string& qos_profile);

    /**
     * @brief 도메인/이름별 publisher 생성
     * @param domain_id 도메인 ID(사전 생성된 participant 필요)
     * @param pub_name 퍼블리셔 이름(도메인 내 유일)
     * @param qos_lib QoS 라이브러리명
     * @param qos_profile QoS 프로파일명
     * @return 결과
     * @pre 지정 도메인에 participant가 이미 생성되어 있어야 합니다.
     * @note 동일 (domain,pub_name) 중복 생성은 Logic 에러를 반환합니다.
     */
    DdsResult create_publisher(int domain_id, const std::string& pub_name, const std::string& qos_lib,
                               const std::string& qos_profile);

    /**
     * @brief 도메인/이름별 subscriber 생성
     * @param domain_id 도메인 ID(사전 생성된 participant 필요)
     * @param sub_name 서브스크라이버 이름(도메인 내 유일)
     * @param qos_lib QoS 라이브러리명
     * @param qos_profile QoS 프로파일명
     * @return 결과
     * @pre 지정 도메인에 participant가 이미 생성되어 있어야 합니다.
     * @note 동일 (domain,sub_name) 중복 생성은 Logic 에러를 반환합니다.
     */
    DdsResult create_subscriber(int domain_id, const std::string& sub_name, const std::string& qos_lib,
                                const std::string& qos_profile);

    /**
     * @brief 도메인/이름/토픽/타입별 writer 생성
     * @param domain_id 도메인 ID(사전 생성된 participant 필요)
     * @param pub_name 퍼블리셔 이름(없으면 자동 생성)
     * @param topic 토픽명(도메인 내 동일명 토픽은 단일 타입만 허용)
     * @param type_name DDS 타입명(사전 등록 필수)
     * @param qos_lib QoS 라이브러리명
     * @param qos_profile QoS 프로파일명
     * @param out_id 생성된 Writer 식별자(선택)
     * @return 결과
     * @pre participant 존재 필요, type_registry에 type_name이 등록되어 있어야 합니다.
     * @note 동일 (domain,pub_name,topic)에 기존 writer가 있으면 Logic 에러를 반환합니다.
     */
    // out_id: optional output parameter to receive the created writer id
    DdsResult create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
                            const std::string& type_name, const std::string& qos_lib, const std::string& qos_profile,
                            uint64_t* out_id = nullptr);

    /**
     * @brief 도메인/이름/토픽/타입별 reader 생성
     * @param domain_id 도메인 ID(사전 생성된 participant 필요)
     * @param sub_name 서브스크라이버 이름(없으면 자동 생성)
     * @param topic 토픽명(도메인 내 동일명 토픽은 단일 타입만 허용)
     * @param type_name DDS 타입명(사전 등록 필수)
     * @param qos_lib QoS 라이브러리명
     * @param qos_profile QoS 프로파일명
     * @param out_id 생성된 Reader 식별자(선택)
     * @return 결과
     * @pre participant 존재 필요, type_registry에 type_name이 등록되어 있어야 합니다.
     * @note 동일 (domain,sub_name,topic)에 기존 reader가 있으면 Logic 에러를 반환합니다.
     */
    // out_id: optional output parameter to receive the created reader id
    DdsResult create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
                            const std::string& type_name, const std::string& qos_lib, const std::string& qos_profile,
                            uint64_t* out_id = nullptr);

    /**
     * @brief 토픽에 JSON 기반 샘플 publish (모든 writer 후보에 전송)
     * @param topic 토픽명
     * @param j JSON 객체(타입별 필드)
     * @return 결과
     * @details 동일 토픽에 다수 writer가 있으면 모두에게 전송되며, 그 경우 WARN 로그가 출력됩니다.
     * @pre topic→type 매핑이 존재해야 하며, 해당 타입의 JSON 바인딩이 등록되어 있어야 합니다.
     */
    DdsResult publish_json(const std::string& topic, const nlohmann::json& j);

    /**
     * @brief 도메인/퍼블리셔/토픽 지정 후 JSON 기반 샘플 publish
     * @return 결과
     * @pre 지정 (domain,pub,topic)에 writer가 존재해야 하며, topic→type 매핑이 존재해야 합니다.
     */
    DdsResult publish_json(int domain_id, const std::string& pub_name, const std::string& topic,
                           const nlohmann::json& j);

    /** @brief 샘플 수신 콜백 핸들러 타입 */
    using SampleHandler = SampleCallback; // preserved alias for backward compatibility

    /**
     * @brief 샘플 수신 콜백 등록
     * @param cb 콜백 함수(토픽명, 타입명, AnyData)
     * @details 내부에서 스레드 안전하게 저장되며, reader 생성 시 리스너에 연결됩니다.
     */
    void set_on_sample(SampleHandler cb);

    /**
     * @brief QoS 프로파일 목록/상세 조회(IPC get.qos 용)
     * @param include_builtin 내장 후보 포함 여부
     * @param include_detail 상세 정보(detail 배열) 포함 여부
     * @return { "result": ["lib::prof",...], (optional) "detail": [ {"lib::prof": {...}}, ... ] }
     */
    nlohmann::json list_qos_profiles(bool include_builtin = false, bool include_detail = false) const;

    /**
     * @brief QoS 프로파일 동적 추가/업데이트 (IPC set.qos)
     * @param library Library 이름
     * @param profile Profile 이름
     * @param profile_xml qos_profile 태그를 포함한 XML 조각
     * @return 성공 시 "library::profile" 형식의 전체 이름, 실패 시 빈 문자열
     * @details 동적 라이브러리에 병합 후 provider를 재구성하며, 캐시를 무효화합니다.
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
    
    /**
     * @brief 스레드 동기화용 뮤텍스
     * @details DDS 엔티티 관련 컨테이너 접근을 보호합니다.
     */
    mutable std::mutex mutex_;
    // 내부 헬퍼: mutex 이미 획득한 상태에서 pub/sub 생성 (재귀 호출 방지)
    DdsResult create_publisher_locked(int domain_id, const std::string& pub_name, 
                                      const std::string& qos_lib, const std::string& qos_profile);
    DdsResult create_subscriber_locked(int domain_id, const std::string& sub_name,
                                       const std::string& qos_lib, const std::string& qos_profile);

    // 적용 로깅 보조: 요약 tag/value 문자열
    static std::string summarize_qos(const rtpdds::QosPack& pack);

    /**
     * @name 내부 멤버 (목적 설명)
     * 멤버 변수들의 역할과 데이터 구조를 이해하기 쉽도록 간단한 설명을 덧붙입니다.
     */
    ///@{
    /**
     * @brief 도메인ID -> DomainParticipant 매핑
     * @details 각 도메인을 대표하는 Participant 객체를 소유합니다. Participant는
     * 도메인 범위의 엔티티(Topic/Publisher/Subscriber 등) 생성을 위한 루트 컨텍스트입니다.
     */
    std::unordered_map<int, std::shared_ptr<dds::domain::DomainParticipant> > participants_;

    /**
     * @brief 도메인 -> 논리명 -> Publisher 매핑
     * @details 애플리케이션에서 지정한 퍼블리셔 논리명으로 Publisher 객체를 관리합니다.
     */
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<dds::pub::Publisher> > > publishers_;

    /**
     * @brief 도메인 -> 논리명 -> Subscriber 매핑
     */
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<dds::sub::Subscriber> > > subscribers_;
    ///@}

    // 도메인/이름/토픽별 writer (여러 writer 허용)
    using HolderId = uint64_t;
    struct WriterEntry { HolderId id; std::shared_ptr<IWriterHolder> holder; };
    struct ReaderEntry { HolderId id; std::shared_ptr<IReaderHolder> holder; };

    /**
     * @brief writers: domain -> publisher name -> topic -> vector of WriterEntry
     * @details
     * - 구조 예시: writers_[domain_id][publisher_name][topic] -> vector<WriterEntry>
     * - 동일 topic에 대해 여러 Writer가 존재할 수 있으므로 vector로 관리합니다.
     * - WriterEntry.holder는 IWriterHolder 인터페이스를 통해 실제 write_any 동작을 캡슐화합니다.
     */
    std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, std::vector<WriterEntry> > > > writers_;
    // readers: domain -> subscriber name -> topic -> vector of entries
    std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, std::vector<ReaderEntry> > > > readers_;

    // next id generator for writer/reader entries
    std::atomic<HolderId> next_holder_id_{1};

    /** @brief 샘플 수신 콜백 (Reader에서 수신한 샘플을 상위로 전달) */
    SampleHandler on_sample_;

    /**
     * @brief 토픽명 → 타입명 매핑 (도메인별 분리)
     * @details topic_to_type_[domain_id][topic] -> type_name
     * 같은 topic 이름이라도 도메인별 서로 다른 타입을 가질 수 있으므로 도메인 분리 맵을 사용합니다.
     */
    std::unordered_map<int, std::unordered_map<std::string, std::string>> topic_to_type_{};
    // 도메인/토픽별 topic holder (participant 스코프)
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<ITopicHolder>>> topics_;

    // 개별 writer/reader 제거 (내부용)
    DdsResult remove_writer(HolderId id);
    DdsResult remove_reader(HolderId id);

    // 이벤트 처리 모드 및 WaitSet Dispatcher
    EventMode event_mode_ = EventMode::Listener;
    std::unique_ptr<async::WaitSetDispatcher> waitset_dispatcher_;

    // 내부 헬퍼: 이벤트 등록/해제
    void register_reader_event(std::shared_ptr<IReaderHolder> holder);
    void register_writer_event(std::shared_ptr<IWriterHolder> holder);
    void unregister_reader_event(std::shared_ptr<IReaderHolder> holder);
    void unregister_writer_event(std::shared_ptr<IWriterHolder> holder);
};
}  // namespace rtpdds
