#pragma once
/**
 * @file dds_type_registry.hpp
 * @brief DDS 엔티티(토픽/라이터/리더) 동적 생성 및 타입 안전성 제공, 타입별 팩토리/홀더/리스너 추상화.
 *
 * 다양한 타입의 토픽/엔티티를 런타임에 안전하게 생성/관리할 수 있도록 추상화된 구조를 제공합니다.
 *
 * 연관 파일:
 *   - dds_manager.hpp (엔티티 관리/수명)
 *   - sample_factory.hpp (샘플 변환/생성)
 *   - type_registry.hpp (타입별 등록/헬퍼)
 */
#include "../../DkmRtpIpc/include/triad_log.hpp"
// #include <dds/core/status/StatusMask.hpp>
#include <any>
#include <dds/dds.hpp>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unordered_map>

namespace rtpdds
{
using AnyData = std::any;

/**
 * @brief 타입별 팩토리/레지스트리 초기화 함수. 신규 타입 추가 시 이 함수에 등록.
 */
void init_dds_type_registry();

// Topic Holder: 토픽 객체를 타입 안전하게 보관/관리하기 위한 추상 인터페이스
struct ITopicHolder {
    virtual ~ITopicHolder() = default;
};

/**
 * @brief 실제 토픽 객체를 타입별로 보관하는 Holder 템플릿
 * @tparam T 토픽 데이터 타입
 */
template <typename T>
struct TopicHolder : public ITopicHolder {
    std::shared_ptr<dds::topic::Topic<T> > topic;
    explicit TopicHolder(std::shared_ptr<dds::topic::Topic<T> > t) : topic(std::move(t)) {}
};

// Writer Holder: 데이터 라이터 객체를 타입 안전하게 보관/관리하기 위한 추상 인터페이스
struct IWriterHolder {
    virtual ~IWriterHolder() = default;
    /**
     * @brief AnyData 타입의 데이터를 실제 타입으로 변환하여 DDS로 publish
     * @param data AnyData(실제 샘플)
     * @throws std::bad_any_cast 타입 불일치 시 예외
     */
    virtual void write_any(const AnyData& data) = 0;
};

/**
 * @brief 실제 데이터 라이터 객체를 타입별로 보관하는 Holder 템플릿
 * @tparam T 데이터 타입
 */
template <typename T>
struct WriterHolder : public IWriterHolder {
    std::shared_ptr<dds::pub::DataWriter<T> > writer;
    std::shared_ptr<void> writer_holder_guard;
    explicit WriterHolder(std::shared_ptr<dds::pub::DataWriter<T> > w) : writer(std::move(w)) {}
    void write_any(const AnyData& data) override
    {
        try {
            writer->write(std::any_cast<const T&>(data));
        } catch (const std::bad_any_cast& e) {
            throw std::runtime_error("WriterHolder: bad_any_cast for type " + dds::topic::topic_type_name<T>::value() +
                                     ": " + e.what());
        }
    }

    struct WriterHolderListener : dds::pub::NoOpDataWriterListener<T> {
        std::string topic_;
        explicit WriterHolderListener(std::string t) : topic_(std::move(t)) {}
        void on_publication_matched(dds::pub::DataWriter<T>&,
                                    const dds::core::status::PublicationMatchedStatus& s) override
        {
            LOG_INF("DDS", "pub_matched topic=%s cur=%d total=%d", topic_.c_str(), s.current_count(), s.total_count());
        }
        void on_offered_incompatible_qos(dds::pub::DataWriter<T>&,
                                         const dds::core::status::OfferedIncompatibleQosStatus& s) override
        {
            LOG_WRN("DDS", "offered_incompat_qos topic=%s id=%d", topic_.c_str(), s.last_policy_id());
        }
        void on_liveliness_lost(dds::pub::DataWriter<T>&, const dds::core::status::LivelinessLostStatus& s) override
        {
            LOG_WRN("DDS", "liveliness_lost topic=%s total=%d", topic_.c_str(), s.total_count());
        }
    };

    void writer_holder_listener(const std::string& topic)
    {
        auto lis = std::make_shared<WriterHolderListener>(topic);
        writer->set_listener(lis, dds::core::status::StatusMask::publication_matched() |
                                      dds::core::status::StatusMask::offered_incompatible_qos() |
                                      dds::core::status::StatusMask::liveliness_lost());
        writer_holder_guard = lis;
    }
};

// Reader Holder: 데이터 리더 객체를 타입 안전하게 보관/관리하기 위한 추상 인터페이스
using SampleCallback = std::function<void(const std::string& topic, const std::string& type_name, const AnyData& data)>;

struct IReaderHolder {
    virtual ~IReaderHolder() = default;
    /**
     * @brief 샘플 수신 시 콜백을 연결하는 리스너 설치
     * @param topic 토픽명
     * @param cb 샘플 수신 콜백
     * @return 리스너 객체
     */
    // 리스너 설치: 상태 이벤트만 활성화. 필요 시 data_available은 나중에 추가
    virtual std::shared_ptr<void> reader_holder_listener(const std::string& topic, bool enable_data = false) = 0;

    // 나중에 샘플 콜백 세팅
    virtual void set_sample_callback(SampleCallback cb) = 0;
};

/**
 * @brief 실제 데이터 리더 객체를 타입별로 보관하는 Holder 템플릿
 * @tparam T 데이터 타입
 */
template <typename T>
struct ReaderHolder : IReaderHolder {
    std::shared_ptr<dds::sub::DataReader<T> > reader;
    explicit ReaderHolder(std::shared_ptr<dds::sub::DataReader<T> > r) : reader(std::move(r)) {}

    std::shared_ptr<void> listener_guard;          // 수명 보관
    dds::core::status::StatusMask current_mask{};  // 현재 마스크

    // 통합 리스너: 샘플 전달 + 상태 로그
    struct ReaderHolderListener : dds::sub::NoOpDataReaderListener<T> {
        SampleCallback cb;
        std::string topic;
        explicit ReaderHolderListener(std::string t) : topic(std::move(t)) {}

        void set_callback(SampleCallback c)
        {
            cb = std::move(c);
        }

        void on_data_available(dds::sub::DataReader<T>& r) override
        {
            if (!cb) return;
            for (auto s : r.take()) {
                if (!s.info().valid()) continue;
                cb(topic, dds::topic::topic_type_name<T>::value(), AnyData(s.data()));
            }
        }

        void on_subscription_matched(dds::sub::DataReader<T>&,
                                     const dds::core::status::SubscriptionMatchedStatus& st) override
        {
            LOG_INF("DDS", "sub_matched topic=%s cur=%d total=%d", topic.c_str(), st.current_count(), st.total_count());
        }
        void on_requested_incompatible_qos(dds::sub::DataReader<T>&,
                                           const dds::core::status::RequestedIncompatibleQosStatus& st) override
        {
            LOG_WRN("DDS", "req_incompat_qos topic=%s id=%d", topic.c_str(), static_cast<int>(st.last_policy_id()));
        }
        void on_sample_lost(dds::sub::DataReader<T>&, const dds::core::status::SampleLostStatus& st) override
        {
            LOG_WRN("DDS", "sample_lost topic=%s total=%d", topic.c_str(), st.total_count());
        }
        // 필요 시 추가 콜백:
        // void on_liveliness_changed(...);
        // void on_sample_rejected(...);
        // void on_requested_deadline_missed(...);
    };

    std::shared_ptr<ReaderHolderListener> lis;  // 리스너 보관

    // ① 리스너 설치. 기본은 상태 이벤트만. enable_data=true면 data_available까지 즉시 활성
    std::shared_ptr<void> reader_holder_listener(const std::string& topic, bool enable_data = false) override
    {
        lis = std::make_shared<ReaderHolderListener>(topic);

        current_mask = dds::core::status::StatusMask::subscription_matched() |
                       dds::core::status::StatusMask::requested_incompatible_qos() |
                       dds::core::status::StatusMask::sample_lost();

        if (enable_data) current_mask |= dds::core::status::StatusMask::data_available();

        reader->listener(lis.get(), current_mask);  // 단일 리스너 등록
        listener_guard = lis;                       // 수명 유지
        return lis;
    }

    // ② 나중에 콜백 세팅. 필요하면 data_available 마스크 추가 적용
    void set_sample_callback(SampleCallback cb) override
    {
        if (!lis) return;  // 아직 리스너 설치 전
        lis->set_callback(std::move(cb));

        if ((current_mask & dds::core::status::StatusMask::data_available()) == 0) {
            current_mask |= dds::core::status::StatusMask::data_available();
            reader->listener(lis.get(), current_mask);  // 동일 리스너로 마스크 갱신
        }
        listener_guard = lis;
    }
};

using TopicFactory = std::function<std::shared_ptr<ITopicHolder>(dds::domain::DomainParticipant&, const std::string&)>;
using WriterFactory = std::function<std::shared_ptr<IWriterHolder>(dds::pub::Publisher&, ITopicHolder&)>;
using ReaderFactory = std::function<std::shared_ptr<IReaderHolder>(dds::sub::Subscriber&, ITopicHolder&)>;

extern std::unordered_map<std::string, TopicFactory> topic_factories;
extern std::unordered_map<std::string, WriterFactory> writer_factories;
extern std::unordered_map<std::string, ReaderFactory> reader_factories;

template <typename T>
void register_dds_type(const std::string& type_name)
{
    topic_factories[type_name] = [](dds::domain::DomainParticipant& participant, const std::string& topic) {
        auto dds_topic = std::make_shared<dds::topic::Topic<T> >(participant, topic);
        return std::make_shared<TopicHolder<T> >(dds_topic);
    };
    writer_factories[type_name] = [](dds::pub::Publisher& publisher, ITopicHolder& th) {
        auto typed_topic = dynamic_cast<TopicHolder<T>*>(&th);
        if (!typed_topic) throw std::runtime_error("Topic type mismatch for Writer");
        auto writer = std::make_shared<dds::pub::DataWriter<T> >(publisher, *(typed_topic->topic));
        auto holder = std::make_shared<WriterHolder<T> >(writer);
        holder->writer_holder_listener(typed_topic->topic->name());
        return holder;
    };
    reader_factories[type_name] = [](dds::sub::Subscriber& subscriber, ITopicHolder& th) {
        auto typed_topic = dynamic_cast<TopicHolder<T>*>(&th);
        if (!typed_topic) throw std::runtime_error("Topic type mismatch for Reader");
        auto reader = std::make_shared<dds::sub::DataReader<T> >(subscriber, *(typed_topic->topic));
        auto holder = std::make_shared<ReaderHolder<T> >(reader);
        holder->reader_holder_listener(typed_topic->topic->name());
        return holder;
    };
}

}  // namespace rtpdds
