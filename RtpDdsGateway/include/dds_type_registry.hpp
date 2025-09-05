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
#include <any>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <dds/dds.hpp>


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
    explicit WriterHolder(std::shared_ptr<dds::pub::DataWriter<T> > w) : writer(std::move(w)) {}
    void write_any(const AnyData& data) override {
        try {
            writer->write(std::any_cast<const T&>(data));
        } catch (const std::bad_any_cast& e) {
            throw std::runtime_error("WriterHolder: bad_any_cast for type " + dds::topic::topic_type_name<T>::value() + ": " + e.what());
        }
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
    virtual std::shared_ptr<void> attach_forwarding_listener(const std::string& topic, SampleCallback cb) = 0;
};

/**
 * @brief 실제 데이터 리더 객체를 타입별로 보관하는 Holder 템플릿
 * @tparam T 데이터 타입
 */
template <typename T>
struct ReaderHolder : public IReaderHolder {
    std::shared_ptr<dds::sub::DataReader<T> > reader;
    explicit ReaderHolder(std::shared_ptr<dds::sub::DataReader<T> > r) : reader(std::move(r)) {}

    struct ForwardingReaderListener : public dds::sub::NoOpDataReaderListener<T> {
        SampleCallback cb;
        std::string topic;
        explicit ForwardingReaderListener(SampleCallback cb_, std::string t) : cb(std::move(cb_)), topic(std::move(t)) {}
        void on_data_available(dds::sub::DataReader<T>& r) override {
            for (auto sample : r.take()) {
                if (sample.info().valid()) {
                    // 샘플 수신 시 콜백 호출 (타입명, 데이터 전달)
                    cb(topic, dds::topic::topic_type_name<T>::value(), AnyData(sample.data()));
                }
            }
        }
    };

    std::shared_ptr<void> attach_forwarding_listener(const std::string& topic, SampleCallback cb) override
    {
        auto lis = std::make_shared<ForwardingReaderListener>(std::move(cb), topic);
        reader->listener(lis.get(), dds::core::status::StatusMask::data_available());
        return lis;
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
        if (!typed_topic) {
            throw std::runtime_error("Topic type mismatch for Writer");
        }
        auto writer = std::make_shared<dds::pub::DataWriter<T> >(publisher, *(typed_topic->topic));
        return std::make_shared<WriterHolder<T> >(writer);
    };
    reader_factories[type_name] = [](dds::sub::Subscriber& subscriber, ITopicHolder& th) {
        auto typed_topic = dynamic_cast<TopicHolder<T>*>(&th);
        if (!typed_topic) {
            throw std::runtime_error("Topic type mismatch for Reader");
        }
        auto reader = std::make_shared<dds::sub::DataReader<T> >(subscriber, *(typed_topic->topic));
        return std::make_shared<ReaderHolder<T> >(reader);
    };
}

}  // namespace rtpdds
