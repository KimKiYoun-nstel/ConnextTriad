// Topic 컨테이너 추가 (헤더에도 정의 필요)
// std::unordered_map<int, std::unordered_map<std::string, std::unordered_map<std::string, std::shared_ptr<ITopicHolder>>>> topics_;
/**
 * @file dds_manager.cpp
 * ### 파일 설명(한글)
 * DdsManager 구현 파일.
 * * Participant/Writer/Reader 생성과 샘플 publish, Reader Listener의 take/return_loan 흐름을 포함.
 */

 #include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

#include "dds_manager.hpp"
#include "triad_log.hpp"
#include "sample_factory.hpp"

namespace rtpdds
{

DdsManager::DdsManager()
{
    // 타입 레지스트리 및 샘플/JSON 변환 테이블 초기화
    init_dds_type_registry();
    init_sample_factories();
    init_sample_to_json();
}


DdsManager::~DdsManager()
{
    // Modern C++11 API: 스마트 포인터 사용, 별도 자원 해제 불필요
    participants_.clear();
    publishers_.clear();
    subscribers_.clear();
    writers_.clear();
    readers_.clear();
}


DdsResult DdsManager::create_participant(int domain_id, const std::string&, const std::string&)
{
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

// Publisher 생성

DdsResult DdsManager::create_publisher(int domain_id, const std::string& pub_name, const std::string& qos_lib,
                                       const std::string& qos_profile)
{
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

// Subscriber 생성

DdsResult DdsManager::create_subscriber(int domain_id, const std::string& sub_name, const std::string& qos_lib,
                                        const std::string& qos_profile)
{
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

// Writer 생성 (상위 엔티티 자동 생성)
DdsResult DdsManager::create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
                                    const std::string& type_name, const std::string& qos_lib,
                                    const std::string& qos_profile)
{
    if (!participants_.count(domain_id)) {
        DdsResult res = create_participant(domain_id, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, DdsErrorCategory::Resource, "Participant creation failed: " + res.reason);
    }
    if (!publishers_[domain_id].count(pub_name)) {
        DdsResult res = create_publisher(domain_id, pub_name, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, res.category, "Publisher creation failed: " + res.reason);
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
    LOG_INF("DDS", "writer created domain=%d pub=%s topic=%s", domain_id, pub_name.c_str(), topic.c_str());
    return DdsResult(
        true, DdsErrorCategory::None,
        "Writer created successfully: domain=" + std::to_string(domain_id) + " pub=" + pub_name + " topic=" + topic);
}

// Reader 생성 (상위 엔티티 자동 생성)
DdsResult DdsManager::create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
                                    const std::string& type_name, const std::string& qos_lib,
                                    const std::string& qos_profile)
{
    if (!participants_.count(domain_id)) {
        DdsResult res = create_participant(domain_id, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, DdsErrorCategory::Resource, "Participant creation failed: " + res.reason);
    }
    if (!subscribers_[domain_id].count(sub_name)) {
        DdsResult res = create_subscriber(domain_id, sub_name, qos_lib, qos_profile);
        if (!res.ok) return DdsResult(false, res.category, "Subscriber creation failed: " + res.reason);
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
        auto guard = reader_holder->attach_forwarding_listener(topic, on_sample_);
        listeners_[topic] = guard;
    }

    LOG_INF("DDS", "reader created domain=%d sub=%s topic=%s", domain_id, sub_name.c_str(), topic.c_str());
    return DdsResult(
        true, DdsErrorCategory::None,
        "Reader created successfully: domain=" + std::to_string(domain_id) + " sub=" + sub_name + " topic=" + topic);
}


DdsResult DdsManager::publish_text(const std::string& topic, const std::string& text)
{
    int count = 0;
    for (const auto& dom : writers_) {
        for (const auto& pub : dom.second) {
            auto it = pub.second.find(topic);
            if (it != pub.second.end()) {
                auto& holder = it->second;
                // 문자열 기반 타입 결정: topic_to_type_에서 타입명 조회
                std::string type_name;
                auto type_it = topic_to_type_.find(topic);
                if (type_it != topic_to_type_.end()) {
                    type_name = type_it->second;
                } else {
                    LOG_ERR("DDS", "publish_text: type_name not found for topic=%s", topic.c_str());
                    continue;
                }
                // 샘플 생성: type string 기반 type-erased 함수 사용
                auto sample_factory_it = sample_factories.find(type_name);
                if (sample_factory_it == sample_factories.end()) {
                    LOG_ERR("DDS", "publish_text: sample_factory not found for type=%s", type_name.c_str());
                    continue;
                }
                
                auto sample = sample_factory_it->second(text);
                holder->write_any(sample.has_value());
                LOG_INF("DDS", "write ok topic=%s domain=%d pub=%s size=%zu", topic.c_str(), dom.first, pub.first.c_str(), text.size());
                count++;
            }
        }
    }
    if (count == 0) {
        LOG_ERR("DDS", "publish_text: topic=%s writer not found", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "Writer not found for topic: " + topic);
    }
    if (count > 1) {
        LOG_WRN("DDS", "publish_text: topic=%s published to %d writers (duplicate transmission warning)", topic.c_str(), count);
    }
    return DdsResult(true, DdsErrorCategory::None,
                     "Publish succeeded: topic=" + topic + " count=" + std::to_string(count));
}

// Modern C++11 API: writer->write() 직접 호출
DdsResult DdsManager::publish_text(int domain_id, const std::string& pub_name, const std::string& topic,
                                   const std::string& text)
{
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

void DdsManager::set_on_sample(SampleHandler cb)
{
    on_sample_ = std::move(cb);
}

}  // namespace rtpdds
