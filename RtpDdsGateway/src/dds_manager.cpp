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
                                   const std::string & /*type_name*/,
                                   const std::string &, const std::string &) {
#ifndef USE_CONNEXT
        LOG_INF("DDS", "(stub) create_writer topic=%s", topic.c_str());
        return true;
#else
        if (!participant_ || !publisher_)
            return false;

        const char *reg_type = StringMsgTypeSupport::get_type_name();
        if (StringMsgTypeSupport::register_type(participant_, reg_type) !=
            DDS_RETCODE_OK) {
            std::cerr << "[DDS Classic] register_type failed\n";
            return false;
        }

        DDS_TopicQos tqos;
        participant_->get_default_topic_qos(tqos);
        DDSTopic *t = participant_->create_topic(topic.c_str(), reg_type, tqos,
                                                 NULL, DDS_STATUS_MASK_NONE);
        if (!t) {
            LOG_ERR("DDS", "create_topic failed topic=%s", topic.c_str());
            return false;
        }

        DDS_DataWriterQos wqos;
        publisher_->get_default_datawriter_qos(wqos);
        DDSDataWriter *w =
            publisher_->create_datawriter(t, wqos, NULL, DDS_STATUS_MASK_NONE);
        if (!w) {
            LOG_ERR("DDS", "create_datawriter failed topic=%s", topic.c_str());
            return false;
        }
        LOG_INF("DDS", "writer created topic=%s", topic.c_str());

        writers_[topic] = w;
        return true;
#endif
    }

    bool DdsManager::create_reader(const std::string &topic,
                                   const std::string & /*type_name*/,
                                   const std::string &, const std::string &) {
#ifndef USE_CONNEXT
        LOG_INF("DDS", "(stub) create_reader topic=%s", topic.c_str());
        return true;
#else
        if (!participant_ || !subscriber_)
            return false;

        const char *reg_type = StringMsgTypeSupport::get_type_name();
        if (StringMsgTypeSupport::register_type(participant_, reg_type) !=
            DDS_RETCODE_OK) {
            std::cerr << "[DDS Classic] register_type failed\n";
            return false;
        }

        DDS_TopicQos tqos;
        participant_->get_default_topic_qos(tqos);
        DDSTopic *t = participant_->create_topic(topic.c_str(), reg_type, tqos,
                                                 NULL, DDS_STATUS_MASK_NONE);
        if (!t) {
            LOG_ERR("DDS", "create_topic failed topic=%s", topic.c_str());
            return false;
        }

        DDS_DataReaderQos rqos;
        subscriber_->get_default_datareader_qos(rqos);
        DDSDataReader *r =
            subscriber_->create_datareader(t, rqos, NULL, DDS_STATUS_MASK_NONE);
        if (!r) {
            LOG_ERR("DDS", "create_datareader failed topic=%s", topic.c_str());
            return false;
        }

        // ⬇️ Listener 장착 (데이터 수신 시 UI로 올릴 준비)
        auto *lst = new ReaderListener(*this, topic);
        r->set_listener(lst, DDS_DATA_AVAILABLE_STATUS);
        listeners_[topic] = lst;

        readers_[topic] = r;
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
        StringMsgDataWriter *w = StringMsgDataWriter::narrow(it->second);
        if (!w) {
            std::cerr << "narrow writer failed\n";
            return false;
        }
        StringMsg sample;
        sample.text = (char *)DDS_String_dup(text.c_str());
        DDS_InstanceHandle_t nil_handle;
        std::memset(&nil_handle, 0, sizeof(nil_handle));
        const DDS_ReturnCode_t rc = w->write(sample, nil_handle);
        DDS_String_free(sample.text);
        if (rc != DDS_RETCODE_OK) {
            LOG_ERR("DDS", "write failed topic=%s rc=%d", topic.c_str(),
                    (int)rc);
            return false;
        }
        LOG_INF("DDS", "write ok topic=%s size=%zu", topic.c_str(),
                text.size());
        return true;
#endif
    }

    void DdsManager::set_on_sample(SampleHandler cb) {
        on_sample_ = std::move(cb);
    }

    void DdsManager::ReaderListener::on_data_available(DDSDataReader *reader) {
        StringMsgDataReader *tr = StringMsgDataReader::narrow(reader);
        if (!tr) {
            LOG_ERR("DDS", "narrow reader failed");
            return;
        }

        StringMsgSeq data_seq;
        DDS_SampleInfoSeq info_seq;
        DDS_ReturnCode_t rc = tr->take(data_seq, info_seq, DDS_LENGTH_UNLIMITED,
                                       DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE,
                                       DDS_ANY_INSTANCE_STATE);

        if (rc == DDS_RETCODE_NO_DATA)
            return;
        if (rc != DDS_RETCODE_OK) {
            LOG_ERR("DDS", "take failed rc=%d", (int)rc);
            return;
        }

        for (int i = 0; i < data_seq.length(); ++i) {
            if (info_seq[i].valid_data) {
                const char *txt = data_seq[i].text ? data_seq[i].text : "";
                LOG_INF("DDS", "recv topic=%s text=%s", topic.c_str(), txt);
                if (owner.on_sample_)
                    owner.on_sample_(topic, std::string(txt));
            }
        }
        tr->return_loan(data_seq, info_seq);
    }

} // namespace rtpdds