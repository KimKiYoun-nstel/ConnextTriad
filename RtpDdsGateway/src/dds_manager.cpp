/**
 * @file dds_manager.cpp
 * ### 파일 설명(한글)
 * DdsManager 구현 파일.
 * * Participant/Writer/Reader 생성과 샘플 publish, Reader Listener의 take/return_loan 흐름을 포함.
 */
#include "dds_manager.hpp"

#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

#include "triad_log.hpp"

namespace rtpdds
{

DdsManager::DdsManager()
{
    // 초기 타입 등록: 기존 타입들 한 줄씩
    REGISTER_MESSAGE_TYPE(registry_, StringMsg);
    REGISTER_MESSAGE_TYPE(registry_, AlarmMsg);
    REGISTER_MESSAGE_TYPE(registry_, P_Alarms_PSM_C_Actual_Alarm);
}

DdsManager::~DdsManager()
{
    // 모든 도메인/엔티티 순회하여 자원 해제
    for (auto& p : participants_) {
        auto domain_id = p.first;
        auto* participant = p.second;
        // Writers
        if (writers_.count(domain_id)) {
            for (auto& pubMap : writers_[domain_id]) {
                for (auto& wkv : pubMap.second) {
                    participant->delete_datawriter(wkv.second);
                }
            }
        }
        // Readers
        if (readers_.count(domain_id)) {
            for (auto& subMap : readers_[domain_id]) {
                for (auto& rkv : subMap.second) {
                    participant->delete_datareader(rkv.second);
                }
            }
        }
        // Publishers
        if (publishers_.count(domain_id)) {
            for (auto& pkv : publishers_[domain_id]) {
                participant->delete_publisher(pkv.second);
            }
        }
        // Subscribers
        if (subscribers_.count(domain_id)) {
            for (auto& skv : subscribers_[domain_id]) {
                participant->delete_subscriber(skv.second);
            }
        }
        DDSTheParticipantFactory->delete_participant(participant);
    }
    participants_.clear();
    publishers_.clear();
    subscribers_.clear();
    writers_.clear();
    readers_.clear();
}

DdsResult DdsManager::create_participant(int domain_id, const std::string&, const std::string&)
{
    DDS_DomainParticipantQos pqos;
    DDSDomainParticipantFactory* factory = DDSDomainParticipantFactory::get_instance();
    factory->get_default_participant_qos(pqos);
    DDSDomainParticipant* participant = factory->create_participant(domain_id, pqos, NULL, DDS_STATUS_MASK_NONE);
    if (!participant) {
        LOG_ERR("DDS", "create_participant failed");
        return DdsResult(false, DdsErrorCategory::Resource, "RTI DDS participant creation failed");
    }
    if (participants_.count(domain_id)) {
        LOG_WRN("DDS", "Duplicate participant creation attempt: domain=%d", domain_id);
        return DdsResult(false, DdsErrorCategory::Logic,
                         "Duplicate participant creation not allowed: domain=" + std::to_string(domain_id));
    }
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
    DDS_PublisherQos pub_qos;
    participant->get_default_publisher_qos(pub_qos);
    DDSPublisher* publisher = participant->create_publisher(pub_qos, NULL, DDS_STATUS_MASK_NONE);
    if (!publisher) {
        LOG_ERR("DDS", "create_publisher failed domain=%d pub=%s", domain_id, pub_name.c_str());
        return DdsResult(false, DdsErrorCategory::Resource, "RTI DDS publisher creation failed");
    }
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
    DDS_SubscriberQos sub_qos;
    participant->get_default_subscriber_qos(sub_qos);
    DDSSubscriber* subscriber = participant->create_subscriber(sub_qos, NULL, DDS_STATUS_MASK_NONE);
    if (!subscriber) {
        LOG_ERR("DDS", "create_subscriber failed domain=%d sub=%s", domain_id, sub_name.c_str());
        return DdsResult(false, DdsErrorCategory::Resource, "RTI DDS subscriber creation failed");
    }
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
    auto& typeReg = registry_.by_name;
    auto it = typeReg.find(type_name);
    if (it == typeReg.end()) {
        LOG_ERR("DDS", "create_writer: type_name not found: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "type_name not found: " + type_name);
    }
    const auto& e = it->second;
    if (!e.register_type(participant)) {
        LOG_ERR("DDS", "create_writer: register_type failed: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Resource, "register_type failed: " + type_name);
    }
    DDS_TopicQos tqos;
    participant->get_default_topic_qos(tqos);
    DDSTopic* t = participant->create_topic(topic.c_str(), e.get_type_name(), tqos, NULL, DDS_STATUS_MASK_NONE);
    if (!t) {
        LOG_ERR("DDS", "create_topic failed topic=%s", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Resource, "RTI DDS topic creation failed: " + topic);
    }
    if (writers_[domain_id][pub_name].count(topic)) {
        LOG_WRN("DDS", "Duplicate writer creation attempt: domain=%d pub=%s topic=%s", domain_id, pub_name.c_str(),
                topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic,
                         "Duplicate writer creation not allowed: domain=" + std::to_string(domain_id) +
                             " pub=" + pub_name + " topic=" + topic);
    }
    DDSDataWriter* w = (DDSDataWriter*)e.create_writer(publisher, t);
    if (!w) {
        LOG_ERR("DDS", "create_datawriter failed topic=%s", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Resource, "RTI DDS datawriter creation failed: " + topic);
    }
    writers_[domain_id][pub_name][topic] = w;
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
    auto& typeReg = registry_.by_name;
    auto it = typeReg.find(type_name);
    if (it == typeReg.end()) {
        LOG_ERR("DDS", "create_reader: type_name not found: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "type_name not found: " + type_name);
    }
    const auto& e = it->second;
    if (!e.register_type(participant)) {
        LOG_ERR("DDS", "create_reader: register_type failed: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Resource, "register_type failed: " + type_name);
    }
    DDS_TopicQos tqos;
    participant->get_default_topic_qos(tqos);
    DDSTopic* t = participant->create_topic(topic.c_str(), e.get_type_name(), tqos, NULL, DDS_STATUS_MASK_NONE);
    if (!t) {
        LOG_ERR("DDS", "create_topic failed topic=%s", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Resource, "RTI DDS topic creation failed: " + topic);
    }
    if (readers_[domain_id][sub_name].count(topic)) {
        LOG_WRN("DDS", "Duplicate reader creation attempt: domain=%d sub=%s topic=%s", domain_id, sub_name.c_str(),
                topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic,
                         "Duplicate reader creation not allowed: domain=" + std::to_string(domain_id) +
                             " sub=" + sub_name + " topic=" + topic);
    }
    DDSDataReader* r = (DDSDataReader*)e.create_reader(subscriber, t);
    if (!r) {
        LOG_ERR("DDS", "create_datareader failed topic=%s", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Resource, "RTI DDS datareader creation failed: " + topic);
    }
    readers_[domain_id][sub_name][topic] = r;
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
                // 타입 추출
                auto type_it = registry_.by_name.find(topic_to_type_[topic]);
                if (type_it == registry_.by_name.end()) continue;
                bool ok = type_it->second.publish_from_text(it->second, text);
                if (ok) {
                    LOG_INF("DDS", "write ok topic=%s domain=%d pub=%s size=%zu", topic.c_str(), dom.first,
                            pub.first.c_str(), text.size());
                    count++;
                }
            }
        }
    }
    if (count == 0) {
        LOG_ERR("DDS", "publish_text: topic=%s writer not found", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "Writer not found for topic: " + topic);
    }
    if (count > 1) {
        LOG_WRN("DDS", "publish_text: topic=%s published to %d writers (duplicate transmission warning)", topic.c_str(),
                count);
    }
    return DdsResult(true, DdsErrorCategory::None,
                     "Publish succeeded: topic=" + topic + " count=" + std::to_string(count));
}

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
        LOG_ERR("DDS", "publish_text: topic=%s not found in publisher=%s domain=%d", topic.c_str(), pub_name.c_str(),
                domain_id);
        return DdsResult(false, DdsErrorCategory::Logic, "Topic not found: " + topic);
    }
    auto type_it = registry_.by_name.find(topic_to_type_[topic]);
    if (type_it == registry_.by_name.end()) {
        LOG_ERR("DDS", "publish_text: type for topic=%s not found", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "Type for topic not found: " + topic);
    }
    bool ok = type_it->second.publish_from_text(topicIt->second, text);
    if (!ok) {
        LOG_ERR("DDS", "publish_text: publish failed for topic=%s", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Resource, "publish failed for topic: " + topic);
    }
    LOG_INF("DDS", "write ok domain=%d pub=%s topic=%s size=%zu", domain_id, pub_name.c_str(), topic.c_str(),
            text.size());
    return DdsResult(true, DdsErrorCategory::None,
                     "Publish succeeded: domain=" + std::to_string(domain_id) + " pub=" + pub_name + " topic=" + topic);
}

void DdsManager::set_on_sample(SampleHandler cb)
{
    on_sample_ = std::move(cb);
}

void DdsManager::ReaderListener::on_data_available(DDSDataReader* reader)
{
    const std::string& type_name = owner.topic_to_type_[topic];
    auto it = owner.registry_.by_name.find(type_name);
    if (it == owner.registry_.by_name.end()) return;
    std::string disp = it->second.take_one_to_display(reader);
    if (!disp.empty() && owner.on_sample_) owner.on_sample_(topic, type_name, disp);
}

}  // namespace rtpdds
