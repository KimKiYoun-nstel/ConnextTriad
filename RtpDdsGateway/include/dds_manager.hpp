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
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_map>

#include <dds/dds.hpp>

#include "dds_type_registry.hpp"


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
    DdsManager();
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
    DdsResult create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
                            const std::string& type_name, const std::string& qos_lib, const std::string& qos_profile);

    /**
     * @brief 도메인/이름/토픽/타입별 reader 생성
     */
    DdsResult create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
                            const std::string& type_name, const std::string& qos_lib, const std::string& qos_profile);

    /**
     * @brief 토픽에 텍스트 샘플 publish (기본 도메인/퍼블리셔)
     */
    DdsResult publish_text(const std::string& topic, const std::string& text);

    /**
     * @brief 도메인/퍼블리셔/토픽별로 텍스트 샘플 publish
     */
    DdsResult publish_text(int domain_id, const std::string& pub_name, const std::string& topic,
                           const std::string& text);

    /**
     * @brief 샘플 수신 콜백 핸들러 타입
     */
    using SampleHandler = SampleCallback;

    /**
     * @brief 샘플 수신 시 호출될 콜백 등록
     * @param cb 콜백 함수
     */
    void set_on_sample(SampleHandler cb);

private:
    // 도메인ID별 participant
    std::unordered_map<int, std::shared_ptr<dds::domain::DomainParticipant> > participants_;
    // 도메인/이름별 publisher
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<dds::pub::Publisher> > > publishers_;
    // 도메인/이름별 subscriber
    std::unordered_map<int, std::unordered_map<std::string, std::shared_ptr<dds::sub::Subscriber> > > subscribers_;
    // 도메인/이름/토픽별 writer
    std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<IWriterHolder> > > > writers_;
    // 도메인/이름/토픽별 reader
    std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<IReaderHolder> > > > readers_;

    // 샘플 수신 콜백
    SampleHandler on_sample_;

    // 토픽명 → 타입명 매핑
    std::unordered_map<std::string, std::string> topic_to_type_{};
    // 도메인/이름/토픽별 topic holder
    std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<ITopicHolder>>>> topics_;
};
}  // namespace rtpdds
