#pragma once
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

void init_dds_type_registry();

// Topic Holder
struct ITopicHolder {
    virtual ~ITopicHolder() = default;
};

template <typename T>
struct TopicHolder : public ITopicHolder {
    std::shared_ptr<dds::topic::Topic<T> > topic;
    explicit TopicHolder(std::shared_ptr<dds::topic::Topic<T> > t) : topic(std::move(t)) {}
};

// Writer Holder
struct IWriterHolder {
    virtual ~IWriterHolder() = default;
    virtual void write_any(const AnyData& data) = 0;
};

template <typename T>
struct WriterHolder : public IWriterHolder {
    std::shared_ptr<dds::pub::DataWriter<T> > writer;
    explicit WriterHolder(std::shared_ptr<dds::pub::DataWriter<T> > w) : writer(std::move(w)) {}
    void write_any(const AnyData& data) override
    {
#if 1
        try {
            writer->write(std::any_cast<const T&>(data));
        } catch (const std::bad_any_cast& e) {
            throw std::runtime_error("WriterHolder: bad_any_cast for type " + dds::topic::topic_type_name<T>::value() + ": " +
                                     e.what());
        }
#else
        writer->write(std::any_cast<const T&>(data));
#endif
    }
};

// Reader Holder
using SampleCallback = std::function<void(const std::string& topic, const std::string& type_name, const AnyData& data)>;

struct IReaderHolder {
    virtual ~IReaderHolder() = default;
    virtual std::shared_ptr<void> attach_forwarding_listener(const std::string& topic, SampleCallback cb) = 0;
};

template <typename T>
struct ReaderHolder : public IReaderHolder {
    std::shared_ptr<dds::sub::DataReader<T> > reader;
    explicit ReaderHolder(std::shared_ptr<dds::sub::DataReader<T> > r) : reader(std::move(r)) {}

    struct ForwardingReaderListener : public dds::sub::NoOpDataReaderListener<T> {
        SampleCallback cb;
        std::string topic;
        explicit ForwardingReaderListener(SampleCallback cb_, std::string t) : cb(std::move(cb_)), topic(std::move(t))
        {
        }
        void on_data_available(dds::sub::DataReader<T>& r) override
        {
            for (auto sample : r.take()) {
                if (sample.info().valid()) {
                    //cb(topic, rti::topic::type_name<T>::get(), AnyData(sample.data()));
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
