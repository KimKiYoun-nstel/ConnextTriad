/**
 * @file dds_manager.cpp
 * ### 파일 설명(한글)
 * DdsManager 구현 파일.
 * * Participant/Writer/Reader 생성과 샘플 publish, Reader Listener의 take/return_loan 흐름을 포함.

 */
#include "dds_manager.hpp"
#include "triad_log.hpp"
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

namespace rtpdds {

    DdsManager::DdsManager() {
#ifdef USE_CONNEXT
        // 초기 타입 등록: 기존 타입들 한 줄씩
        REGISTER_MESSAGE_TYPE(registry_, StringMsg);
        REGISTER_MESSAGE_TYPE(registry_, AlarmMsg);
#endif
    }

    DdsManager::~DdsManager() {
#ifdef USE_CONNEXT
        if (participant_) {
            for (auto &kv : readers_) {
                if (kv.second)
                    kv.second->set_listener(NULL, DDS_STATUS_MASK_NONE);
            }
            for (auto &kv : listeners_)
                delete kv.second;
            listeners_.clear();

            if (publisher_)
                participant_->delete_publisher(publisher_);
            if (subscriber_)
                participant_->delete_subscriber(subscriber_);
            for (auto &kv : writers_)
                participant_->delete_datawriter(kv.second);
            for (auto &kv : readers_)
                participant_->delete_datareader(kv.second);
            DDSTheParticipantFactory->delete_participant(participant_);
        }
#endif
    }

    bool DdsManager::create_participant(int domain_id, const std::string &,
                                        const std::string &) {
#ifndef USE_CONNEXT
        LOG_INF("DDS", "(stub) create_participant domain=%d", domain_id);
        return true;
#else
        DDS_DomainParticipantQos pqos;
        DDSDomainParticipantFactory *factory =
            DDSDomainParticipantFactory::get_instance();
        factory->get_default_participant_qos(pqos);

        participant_ = factory->create_participant(domain_id, pqos, NULL,
                                                   DDS_STATUS_MASK_NONE);
        if (!participant_) {
            LOG_ERR("DDS", "create_participant failed");
            return false;
        }

        DDS_PublisherQos pub_qos;
        participant_->get_default_publisher_qos(pub_qos);
        DDS_SubscriberQos sub_qos;
        participant_->get_default_subscriber_qos(sub_qos);

        publisher_ =
            participant_->create_publisher(pub_qos, NULL, DDS_STATUS_MASK_NONE);
    // 한국어: Participant 생성 후 Publisher/Subscriber를 기본 QoS로 생성한다.
        subscriber_ = participant_->create_subscriber(sub_qos, NULL,
                                                      DDS_STATUS_MASK_NONE);

        if (!publisher_ || !subscriber_) {
            LOG_ERR("DDS", "create_pub/sub failed");
            return false;
        }
        LOG_INF("DDS", "participant created domain=%d", domain_id);
        return true;
#endif
    }

    bool DdsManager::create_writer(const std::string &topic,
                                   const std::string & type_name,
                                   const std::string &, const std::string &) {
#ifndef USE_CONNEXT
        LOG_INF("DDS", "(stub) create_writer topic=%s", topic.c_str());
        return true;
#else
        if (!participant_ || !publisher_)
            return false;

        auto it = registry_.by_name.find(type_name);
        if (it == registry_.by_name.end()) return false;
        const auto &e = it->second;
        if (!e.register_type(participant_)) return false;

        DDS_TopicQos tqos;
        participant_->get_default_topic_qos(tqos);
        DDSTopic *t = participant_->create_topic(topic.c_str(), e.get_type_name(), tqos,
                                                 NULL, DDS_STATUS_MASK_NONE);
        if (!t) {
            LOG_ERR("DDS", "create_topic failed topic=%s", topic.c_str());
            return false;
        }

        DDSDataWriter *w = (DDSDataWriter*) e.create_writer(publisher_, t);
        if (!w) {
            LOG_ERR("DDS", "create_datawriter failed topic=%s", topic.c_str());
            return false;
        }
        LOG_INF("DDS", "writer created topic=%s", topic.c_str());

        writers_[topic] = w;
        topic_to_type_[topic] = type_name;
        return true;
#endif
    }

    bool DdsManager::create_reader(const std::string &topic,
                                   const std::string & type_name,
                                   const std::string &, const std::string &) {
#ifndef USE_CONNEXT
        LOG_INF("DDS", "(stub) create_reader topic=%s", topic.c_str());
        return true;
#else
        if (!participant_ || !subscriber_)
            return false;

        auto it = registry_.by_name.find(type_name);
        if (it == registry_.by_name.end()) return false;
        const auto &e = it->second;
        if (!e.register_type(participant_)) return false;

        DDS_TopicQos tqos;
        participant_->get_default_topic_qos(tqos);
        DDSTopic *t = participant_->create_topic(topic.c_str(), e.get_type_name(), tqos,
                                                 NULL, DDS_STATUS_MASK_NONE);
        if (!t) {
            LOG_ERR("DDS", "create_topic failed topic=%s", topic.c_str());
            return false;
        }

        DDSDataReader *r = (DDSDataReader*) e.create_reader(subscriber_, t);
        if (!r) {
            LOG_ERR("DDS", "create_datareader failed topic=%s", topic.c_str());
            return false;
        }

        // ⬇️ Listener 장착 (데이터 수신 시 UI로 올릴 준비)
        auto *lst = new ReaderListener(*this, topic);
        r->set_listener(lst, DDS_DATA_AVAILABLE_STATUS);
        listeners_[topic] = lst;

        readers_[topic] = r;
        topic_to_type_[topic] = type_name;
        LOG_INF("DDS", "reader created topic=%s", topic.c_str());
        return true;
#endif
    }

    bool DdsManager::publish_text(const std::string &topic,
                                  const std::string &text) {
#ifndef USE_CONNEXT
        LOG_INF("DDS", "(stub) publish_text topic=%s text=%s", topic.c_str(),
                text.c_str());
        return true;
#else
        auto it = writers_.find(topic);
        if (it == writers_.end())
            return false;
        const std::string &type_name = topic_to_type_[topic];
        auto it2 = registry_.by_name.find(type_name);
        if (it2 == registry_.by_name.end()) return false;
        const bool ok = it2->second.publish_from_text(it->second, text);
        if (!ok) return false;
        LOG_INF("DDS", "write ok topic=%s size=%zu", topic.c_str(), text.size());
        return true;
#endif
    }

    void DdsManager::set_on_sample(SampleHandler cb) {
        on_sample_ = std::move(cb);
    }

    void DdsManager::ReaderListener::on_data_available(DDSDataReader *reader) {
        const std::string &type_name = owner.topic_to_type_[topic];
        auto it = owner.registry_.by_name.find(type_name);
        if (it == owner.registry_.by_name.end()) return;
        std::string disp = it->second.take_one_to_display(reader);
        if (!disp.empty() && owner.on_sample_) owner.on_sample_(topic, type_name, disp);
    }

} // namespace rtpdds