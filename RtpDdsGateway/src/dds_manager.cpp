/**
 * @file dds_manager.cpp
 * @brief DdsManager 구현 파일
 *
 * Participant/Writer/Reader 생성과 샘플 publish, Reader Listener의 take/return_loan 흐름을 포함.
 * 각 함수별로 파라미터/리턴/비즈니스 처리에 대한 상세 주석을 추가.
 */

#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

#include "dds_manager.hpp"
#include "dds_type_registry.hpp"
#include "triad_log.hpp"
#include "sample_factory.hpp"

namespace rtpdds
{
DdsManager::DdsManager()
{
    // 타입 레지스트리 및 샘플/JSON 변환 테이블 초기화
    // (신규 타입/토픽 추가 시 이 함수들에 등록 필요)
    init_dds_type_registry();
    init_sample_factories();
    init_sample_to_json();
}

DdsManager::~DdsManager()
{
    clear_entities();
}

/**
 * @brief 실행 중 모든 DDS 엔티티(Participant, Publisher, Subscriber, Writer, Reader 등)를 초기화(해제)한다.
 * @details 컨테이너를 clear()하여 리소스를 모두 해제하며, 이후 재생성 가능하다.
 */
void DdsManager::clear_entities()
{
    participants_.clear();
    publishers_.clear();
    subscribers_.clear();
    writers_.clear();
    readers_.clear();
    topic_to_type_.clear();
    topics_.clear();
    // 필요시 추가 리소스도 clear
}

/**
 * @brief 도메인ID별 participant 생성
 * @param domain_id 도메인 ID
 * @param qos_lib   QoS 라이브러리명
 * @param qos_profile QoS 프로파일명
 * @return 성공/실패 및 사유
 *
 * 이미 존재하는 도메인ID에 대해 중복 생성 시 Logic 에러 반환
 */
DdsResult DdsManager::create_participant(int domain_id, const std::string& qos_lib, const std::string& qos_profile)
{
    LOG_DBG("DDS", "create_participant(domain_id=%d, qos_lib=%s, qos_profile=%s)", domain_id, qos_lib.c_str(), qos_profile.c_str());
    if (participants_.count(domain_id)) {
        LOG_WRN("DDS", "Duplicate participant creation attempt: domain=%d", domain_id);
        return DdsResult(false, DdsErrorCategory::Logic,
                         "Duplicate participant creation not allowed: domain=" + std::to_string(domain_id));
    }
    auto participant = std::make_shared<dds::domain::DomainParticipant>(domain_id);
    participants_[domain_id] = participant;
    LOG_INF("DDS", "participant created domain=%d", domain_id);
    return DdsResult(true, DdsErrorCategory::None,
                     "Participant created successfully: domain=" + std::to_string(domain_id));
}

/**
 * @brief 도메인/이름별 publisher 생성
 * @param domain_id 도메인 ID
 * @param pub_name 퍼블리셔 이름
 * @param qos_lib   QoS 라이브러리명
 * @param qos_profile QoS 프로파일명
 * @return 성공/실패 및 사유
 *
 * participant가 없으면 자동 생성. 이미 있으면 Logic 에러 반환
 */
DdsResult DdsManager::create_publisher(int domain_id, const std::string& pub_name, const std::string& qos_lib,
                                       const std::string& qos_profile)
{
    LOG_DBG("DDS", "create_publisher(domain_id=%d, pub_name=%s, qos_lib=%s, qos_profile=%s)", domain_id, pub_name.c_str(), qos_lib.c_str(), qos_profile.c_str());
    if (!participants_.count(domain_id)) {
        DdsResult res = create_participant(domain_id, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, DdsErrorCategory::Resource, "Participant creation failed: " + res.reason);
    }
    auto& participant = participants_[domain_id];
    if (publishers_[domain_id].count(pub_name)) {
        LOG_WRN("DDS", "Duplicate publisher creation attempt: domain=%d pub=%s", domain_id, pub_name.c_str());
        return DdsResult(
            false, DdsErrorCategory::Logic,
            "Duplicate publisher creation not allowed: domain=" + std::to_string(domain_id) + " pub=" + pub_name);
    }
    // Modern C++11 API: QoS를 명시하지 않으면 default QoS가 자동 적용됨
    auto publisher = std::make_shared<dds::pub::Publisher>(*participant);
    publishers_[domain_id][pub_name] = publisher;
    LOG_INF("DDS", "publisher created domain=%d pub=%s", domain_id, pub_name.c_str());
    return DdsResult(true, DdsErrorCategory::None,
                     "Publisher created successfully: domain=" + std::to_string(domain_id) + " pub=" + pub_name);
}

/**
 * @brief 도메인/이름별 subscriber 생성
 * @param domain_id 도메인 ID
 * @param sub_name 서브스크라이버 이름
 * @param qos_lib   QoS 라이브러리명
 * @param qos_profile QoS 프로파일명
 * @return 성공/실패 및 사유
 *
 * participant가 없으면 자동 생성. 이미 있으면 Logic 에러 반환
 */
DdsResult DdsManager::create_subscriber(int domain_id, const std::string& sub_name, const std::string& qos_lib,
                                        const std::string& qos_profile)
{
    LOG_DBG("DDS", "create_subscriber(domain_id=%d, sub_name=%s, qos_lib=%s, qos_profile=%s)", domain_id, sub_name.c_str(), qos_lib.c_str(), qos_profile.c_str());
    if (!participants_.count(domain_id)) {
        DdsResult res = create_participant(domain_id, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, DdsErrorCategory::Resource, "Participant creation failed: " + res.reason);
    }
    auto& participant = participants_[domain_id];
    if (subscribers_[domain_id].count(sub_name)) {
        LOG_WRN("DDS", "Duplicate subscriber creation attempt: domain=%d sub=%s", domain_id, sub_name.c_str());
        return DdsResult(
            false, DdsErrorCategory::Logic,
            "Duplicate subscriber creation not allowed: domain=" + std::to_string(domain_id) + " sub=" + sub_name);
    }
    // Modern C++11 API: QoS를 명시하지 않으면 default QoS가 자동 적용됨
    auto subscriber = std::make_shared<dds::sub::Subscriber>(*participant);
    subscribers_[domain_id][sub_name] = subscriber;
    LOG_INF("DDS", "subscriber created domain=%d sub=%s", domain_id, sub_name.c_str());
    return DdsResult(true, DdsErrorCategory::None,
                     "Subscriber created successfully: domain=" + std::to_string(domain_id) + " sub=" + sub_name);
}

/**
 * @brief 도메인/이름/토픽/타입별 writer 생성
 * @param domain_id 도메인 ID
 * @param pub_name 퍼블리셔 이름
 * @param topic 토픽명
 * @param type_name 타입명
 * @param qos_lib   QoS 라이브러리명
 * @param qos_profile QoS 프로파일명
 * @return 성공/실패 및 사유
 *
 * 상위 participant/publisher가 없으면 자동 생성. 타입/팩토리 등록 여부 체크. 중복 생성 시 Logic 에러 반환
 * topic_factories, writer_factories에서 타입별 팩토리 함수 사용
 */
DdsResult DdsManager::create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
                                    const std::string& type_name, const std::string& qos_lib,
                                    const std::string& qos_profile)
{
    LOG_DBG("DDS", "create_writer(domain_id=%d, pub_name=%s, topic=%s, type_name=%s, qos_lib=%s, qos_profile=%s)", domain_id, pub_name.c_str(), topic.c_str(), type_name.c_str(), qos_lib.c_str(), qos_profile.c_str());
    if (!participants_.count(domain_id)) {
        DdsResult res = create_participant(domain_id, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, DdsErrorCategory::Resource, "Participant creation failed: " + res.reason);
    }
    if (!publishers_[domain_id].count(pub_name)) {
        DdsResult res = create_publisher(domain_id, pub_name, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, res.category, "Publisher creation failed: " + res.reason);
    }

    // type_name 유효성 체크 (등록 여부)
    if (topic_factories.find(type_name) == topic_factories.end()) {
        LOG_ERR("DDS", "create_writer: unknown DDS type: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "Unknown DDS type: " + type_name);
    }
    if (writer_factories.find(type_name) == writer_factories.end()) {
        LOG_ERR("DDS", "create_writer: no writer factory for DDS type: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "No writer factory for DDS type: " + type_name);
    }

    auto& participant = participants_[domain_id];
    auto& publisher = publishers_[domain_id][pub_name];
    if (writers_[domain_id][pub_name].count(topic)) {
        LOG_WRN("DDS", "Duplicate writer creation attempt: domain=%d pub=%s topic=%s", domain_id, pub_name.c_str(), topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic,
                         "Duplicate writer creation not allowed: domain=" + std::to_string(domain_id) + " pub=" + pub_name + " topic=" + topic);
    }

    // TopicHolder 생성 및 저장 (이미 있으면 재사용)
    std::shared_ptr<ITopicHolder> topic_holder;
    auto topic_it = topics_[domain_id][pub_name].find(topic);
    if (topic_it != topics_[domain_id][pub_name].end()) {
        topic_holder = topic_it->second;
    } else {
        topic_holder = topic_factories[type_name](*participant, topic);
        topics_[domain_id][pub_name][topic] = topic_holder;
    }

    // WriterHolder 생성 (Topic 객체 활용)
    auto writer_holder = writer_factories[type_name](*publisher, *topic_holder);
    writers_[domain_id][pub_name][topic] = writer_holder;
    // topic과 type_name 매핑 저장 (publish_text에서 사용)
    topic_to_type_[topic] = type_name;
    LOG_INF("DDS", "writer created domain=%d pub=%s topic=%s", domain_id, pub_name.c_str(), topic.c_str());
    return DdsResult(
        true, DdsErrorCategory::None,
        "Writer created successfully: domain=" + std::to_string(domain_id) + " pub=" + pub_name + " topic=" + topic);
}

/**
 * @brief 도메인/이름/토픽/타입별 reader 생성
 * @param domain_id 도메인 ID
 * @param sub_name 서브스크라이버 이름
 * @param topic 토픽명
 * @param type_name 타입명
 * @param qos_lib   QoS 라이브러리명
 * @param qos_profile QoS 프로파일명
 * @return 성공/실패 및 사유
 *
 * 상위 participant/subscriber가 없으면 자동 생성. 타입/팩토리 등록 여부 체크. 중복 생성 시 Logic 에러 반환
 * topic_factories, reader_factories에서 타입별 팩토리 함수 사용
 * 샘플 수신 콜백(on_sample_)이 등록되어 있으면 리스너 부착
 */
DdsResult DdsManager::create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
                                    const std::string& type_name, const std::string& qos_lib,
                                    const std::string& qos_profile)
{
    LOG_DBG("DDS", "create_reader(domain_id=%d, sub_name=%s, topic=%s, type_name=%s, qos_lib=%s, qos_profile=%s)", domain_id, sub_name.c_str(), topic.c_str(), type_name.c_str(), qos_lib.c_str(), qos_profile.c_str());
    LOG_INF("DDS", "reader ready domain=%d sub=%s topic=%s type=%s", domain_id, sub_name.c_str(), topic.c_str(), type_name.c_str());
    if (!participants_.count(domain_id)) {
        DdsResult res = create_participant(domain_id, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, DdsErrorCategory::Resource, "Participant creation failed: " + res.reason);
    }
    if (!subscribers_[domain_id].count(sub_name)) {
        DdsResult res = create_subscriber(domain_id, sub_name, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, res.category, "Subscriber creation failed: " + res.reason);
    }

    // type_name 유효성 체크 (등록 여부)
    if (topic_factories.find(type_name) == topic_factories.end()) {
        LOG_ERR("DDS", "create_reader: unknown DDS type: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "Unknown DDS type: " + type_name);
    }
    if (reader_factories.find(type_name) == reader_factories.end()) {
        LOG_ERR("DDS", "create_reader: no reader factory for DDS type: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "No reader factory for DDS type: " + type_name);
    }

    auto& participant = participants_[domain_id];
    auto& subscriber = subscribers_[domain_id][sub_name];
    if (readers_[domain_id][sub_name].count(topic)) {
        LOG_WRN("DDS", "Duplicate reader creation attempt: domain=%d sub=%s topic=%s", domain_id, sub_name.c_str(), topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic,
                         "Duplicate reader creation not allowed: domain=" + std::to_string(domain_id) + " sub=" + sub_name + " topic=" + topic);
    }

    // TopicHolder 재사용 (Writer 생성 시 저장된 객체)
    std::shared_ptr<ITopicHolder> topic_holder;
    auto topic_it = topics_[domain_id][sub_name].find(topic);
    if (topic_it != topics_[domain_id][sub_name].end()) {
        topic_holder = topic_it->second;
    } else {
        // 없으면 새로 생성
        topic_holder = topic_factories[type_name](*participant, topic);
        topics_[domain_id][sub_name][topic] = topic_holder;
    }

    auto reader_holder = reader_factories[type_name](*subscriber, *topic_holder);
    readers_[domain_id][sub_name][topic] = reader_holder;

    // 타입 소거 리스너 부착 및 수명 유지
    if (on_sample_) {
        reader_holder->set_sample_callback(on_sample_);
    LOG_DBG("DDS", "listener attached topic=%s", topic.c_str());
    }

    LOG_INF("DDS", "reader created domain=%d sub=%s topic=%s", domain_id, sub_name.c_str(), topic.c_str());
    return DdsResult(
        true, DdsErrorCategory::None,
        "Reader created successfully: domain=" + std::to_string(domain_id) + " sub=" + sub_name + " topic=" + topic);
}

/**
 * @brief 토픽에 텍스트 샘플 publish (기본 도메인/퍼블리셔)
 * @param topic 토픽명
 * @param text  샘플 데이터(텍스트/JSON 등)
 * @return 성공/실패 및 사유
 *
 * topic_to_type_에서 타입명 조회 → sample_factory에서 샘플 생성 → writer에 publish
 * 여러 writer에 중복 전송될 수 있음(경고)
 */
DdsResult DdsManager::publish_text(const std::string& topic, const std::string& text)
{
    LOG_DBG("DDS", "publish_text(topic=%s, text=%s)", topic.c_str(), text.c_str());
    int count = 0;
    for (const auto& dom : writers_) {
        for (const auto& pub : dom.second) {
            auto it = pub.second.find(topic);
            if (it != pub.second.end()) {
                auto& holder = it->second;
                // 문자열 기반 타입 결정: topic_to_type_에서 타입명 조회
                std::string type_name;
                auto type_it = topic_to_type_.find(topic);
                if (type_it == topic_to_type_.end()) {
                    LOG_ERR("DDS", "publish_text: type_name not found for topic=%s", topic.c_str());
                    continue;
                }
                type_name = type_it->second;
                // 샘플 생성: type string 기반 type-erased 함수 사용
                auto sample_factory_it = sample_factories.find(type_name);
                if (sample_factory_it == sample_factories.end()) {
                    LOG_ERR("DDS", "publish_text: sample_factory not found for type=%s", type_name.c_str());
                    continue;
                }
                auto sample = sample_factory_it->second(text);
                if (!sample.has_value()) {
                    LOG_ERR("DDS", "publish_text: failed to create sample for type=%s", type_name.c_str());
                    continue;
                }
                holder->write_any(sample);
                LOG_INF("DDS", "write ok topic=%s domain=%d pub=%s size=%zu", topic.c_str(), dom.first, pub.first.c_str(), text.size());
                count++;
            }
        }
    }
    if (count == 0) {
        LOG_ERR("DDS", "publish_text: topic=%s writer not found or invalid type/sample", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "Writer not found or invalid type/sample for topic: " + topic);
    }
    if (count > 1) {
        LOG_WRN("DDS", "publish_text: topic=%s published to %d writers (duplicate transmission warning)", topic.c_str(), count);
    }
    return DdsResult(true, DdsErrorCategory::None,
                     "Publish succeeded: topic=" + topic + " count=" + std::to_string(count));
}

/**
 * @brief 도메인/퍼블리셔/토픽별로 텍스트 샘플 publish
 * @param domain_id 도메인 ID
 * @param pub_name 퍼블리셔 이름
 * @param topic 토픽명
 * @param text  샘플 데이터(텍스트/JSON 등)
 * @return 성공/실패 및 사유
 *
 * sample_factory에서 샘플 생성 → writer에 publish
 */
DdsResult DdsManager::publish_text(int domain_id, const std::string& pub_name, const std::string& topic,
                                   const std::string& text)
{
    LOG_DBG("DDS", "publish_text(domain_id=%d, pub_name=%s, topic=%s, text=%s)", domain_id, pub_name.c_str(), topic.c_str(), text.c_str());
    auto domIt = writers_.find(domain_id);
    if (domIt == writers_.end()) {
        LOG_ERR("DDS", "publish_text: domain=%d not found", domain_id);
        return DdsResult(false, DdsErrorCategory::Logic, "Domain not found: " + std::to_string(domain_id));
    }
    auto pubIt = domIt->second.find(pub_name);
    if (pubIt == domIt->second.end()) {
        LOG_ERR("DDS", "publish_text: publisher=%s not found in domain=%d", pub_name.c_str(), domain_id);
        return DdsResult(false, DdsErrorCategory::Logic, "Publisher not found: " + pub_name);
    }
    auto topicIt = pubIt->second.find(topic);
    if (topicIt == pubIt->second.end()) {
        LOG_ERR("DDS", "publish_text: topic=%s not found in publisher=%s domain=%d", topic.c_str(), pub_name.c_str(), domain_id);
        return DdsResult(false, DdsErrorCategory::Logic, "Topic not found: " + topic);
    }
    auto& holder = topicIt->second;
    std::string type_name;
    auto type_it = topic_to_type_.find(topic);
    if (type_it != topic_to_type_.end()) {
        type_name = type_it->second;
    } else {
        LOG_ERR("DDS", "publish_text: type_name not found for topic=%s", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "type_name not found for topic: " + topic);
    }
    auto fit = sample_factories.find(type_name);
    if (fit == sample_factories.end()) {
        LOG_ERR("DDS", "publish_text: sample_factory not found for type=%s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "No sample factory for type: " + type_name);
    }
    auto sample = fit->second(text);
    if (!sample.has_value()) {
        LOG_ERR("DDS", "publish_text: failed to create sample for type=%s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "failed to create sample for type: " + type_name);
    }
    holder->write_any(sample.has_value());
    LOG_INF("DDS", "write ok domain=%d pub=%s topic=%s size=%zu", domain_id, pub_name.c_str(), topic.c_str(), text.size());
    return DdsResult(true, DdsErrorCategory::None,
                     "Publish succeeded: domain=" + std::to_string(domain_id) + " pub=" + pub_name + " topic=" + topic);
}

/**
 * @brief 샘플 수신 콜백 핸들러 등록
 * @param cb 콜백 함수
 */
void DdsManager::set_on_sample(SampleHandler cb)
{
    on_sample_ = std::move(cb);
    LOG_DBG("DDS", "on_sample handler installed");
}

}  // namespace rtpdds
