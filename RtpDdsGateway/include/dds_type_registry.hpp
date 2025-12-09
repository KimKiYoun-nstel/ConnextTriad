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
 * @brief DDS 이벤트 처리를 위한 공통 인터페이스
 * 
 * Listener 모드와 WaitSet 모드에서 공통으로 사용할 로직을 정의합니다.
 * - process_status: 상태 변경 이벤트 처리 (로그 출력 등)
 * - process_data: 데이터 수신 이벤트 처리 (Reader 전용)
 */
struct IDdsEventHandler {
    virtual ~IDdsEventHandler() = default;

    /**
     * @brief 상태 변화 처리 (Writer/Reader 공용)
     * @param mask 변경된 상태 비트마스크
     */
    virtual void process_status(const dds::core::status::StatusMask& mask) = 0;

    /**
     * @brief 데이터 처리 (Reader 전용)
     * Writer는 구현할 필요 없음 (빈 구현)
     */
    virtual void process_data() {}

    /**
     * @brief WaitSet용 StatusCondition 객체 반환
     * @return dds::core::cond::StatusCondition
     */
    virtual dds::core::cond::StatusCondition get_status_condition() = 0;

    /**
     * @brief WaitSet용 ReadCondition 객체 반환 (Reader 전용)
     * @return dds::sub::cond::ReadCondition (Writer는 null condition 반환)
     */
    virtual dds::sub::cond::ReadCondition get_read_condition() { 
        return dds::sub::cond::ReadCondition(dds::core::null); 
    }
    
    /**
     * @brief Listener 모드 활성/비활성 제어
     * @param enable true: Listener 활성화, false: Listener 비활성화 (WaitSet용)
     */
    /**
     * @brief Listener 모드 활성화
     * @note 런타임에서 동적으로 listener를 끄는 동작은 현재 지원하지 않습니다.
     *       `enable == true`일 때만 리스너를 활성화하도록 구현되어야 하며,
     *       `enable == false`는 no-op(무시)로 처리됩니다.
     */
    virtual void enable_listener_mode(bool enable) = 0;
};

/**
 * @brief 타입별 팩토리/레지스트리 초기화 함수. 신규 타입 추가 시 이 함수에 등록.
 */
void init_dds_type_registry();

// Topic Holder: 토픽 객체를 타입 안전하게 보관/관리하기 위한 추상 인터페이스
struct ITopicHolder {
    virtual ~ITopicHolder() = default;
    virtual void set_qos(const dds::topic::qos::TopicQos& /*q*/) {}
};

/**
 * @brief 실제 토픽 객체를 타입별로 보관하는 Holder 템플릿
 * @tparam T 토픽 데이터 타입
 */
template <typename T>
struct TopicHolder : public ITopicHolder {
    std::shared_ptr<dds::topic::Topic<T> > topic;
    explicit TopicHolder(std::shared_ptr<dds::topic::Topic<T> > t) : topic(std::move(t)) {}
    void set_qos(const dds::topic::qos::TopicQos& q) override { if (topic) topic->qos(q); }
};

// Writer Holder: 데이터 라이터 객체를 타입 안전하게 보관/관리하기 위한 추상 인터페이스
struct IWriterHolder : public IDdsEventHandler {
    virtual ~IWriterHolder() = default;
    /**
     * @brief AnyData 타입의 데이터를 실제 타입으로 변환하여 DDS로 publish
     * @param data AnyData(실제 샘플)
     * @throws std::bad_cast 타입 불일치 시 예외 (std::bad_any_cast 대신 std::bad_cast로 통일)
     */
    virtual void write_any(const AnyData& data) = 0;
    virtual void set_qos(const dds::pub::qos::DataWriterQos& /*q*/) {}
};

/**
 * @brief 실제 데이터 라이터 객체를 타입별로 보관하는 Holder 템플릿
 * @tparam T 데이터 타입
 */
template <typename T>
struct WriterHolder : public IWriterHolder {
    std::shared_ptr<dds::pub::DataWriter<T> > writer;
    std::shared_ptr<void> writer_holder_guard;
    std::string topic_name; // 토픽명 저장

    explicit WriterHolder(std::shared_ptr<dds::pub::DataWriter<T> > w, std::string t_name = "") 
        : writer(std::move(w)), topic_name(std::move(t_name)) {
        if (topic_name.empty() && writer) {
            topic_name = writer->topic().name();
        }
    }
    
    ~WriterHolder() {
        // DDS Writer 파괴 전 리스너를 명시적으로 해제하여 Use-After-Free 방지
        if (writer) {
            try {
                writer->set_listener(nullptr, dds::core::status::StatusMask::none());
            } catch (...) {
                // 예외 무시 (소멸자에서 안전)
            }
        }
        writer_holder_guard.reset();
    }
    
    void write_any(const AnyData& data) override
    {
        try {
            // std::any에서 void*를 추출
            if (!data.has_value()) {
                LOG_ERR("WriterHolder", "write_any: data is empty");
                throw std::invalid_argument("WriterHolder: data is empty");
            }

            // std::any에서 void*를 추출하고, 기대하는 타입으로 변환
            void* raw_data = std::any_cast<void*>(data);
            if (!raw_data) {
                LOG_ERR("WriterHolder", "write_any: extracted data is null");
                throw std::invalid_argument("WriterHolder: extracted data is null");
            }

            // void*를 기대하는 타입으로 변환
            const T* typed_data = static_cast<const T*>(raw_data);
            if (!typed_data) {
                LOG_ERR("WriterHolder", "write_any: failed to cast data to expected type");
                throw std::bad_cast();
            }

            LOG_FLOW("write_any: data cast successful. Writing data to writer.");

            // RTI writer 호출
            writer->write(*typed_data);
            LOG_FLOW("write_any: write successful.");
        } catch (const std::bad_cast& e) {
            LOG_ERR("WriterHolder", "write_any: bad_cast exception: %s", e.what());
            throw std::runtime_error("WriterHolder: bad_cast for type " + dds::topic::topic_type_name<T>::value() +
                                    ": " + e.what());
        } catch (const std::exception& e) {
            LOG_ERR("WriterHolder", "write_any: exception: %s", e.what());
            throw;
        }
    }

    void set_qos(const dds::pub::qos::DataWriterQos& q) override { if (writer) writer->qos(q); }

    // [IDdsEventHandler 구현] 상태 처리
    void process_status(const dds::core::status::StatusMask& mask) override {
        if ((mask & dds::core::status::StatusMask::publication_matched()).any()) {
            auto s = writer->publication_matched_status();
            LOG_INF("DDS", "pub_matched topic=%s cur=%d total=%d", topic_name.c_str(), s.current_count(), s.total_count());
        }
        if ((mask & dds::core::status::StatusMask::offered_incompatible_qos()).any()) {
            auto s = writer->offered_incompatible_qos_status();
            LOG_WRN("DDS", "offered_incompat_qos topic=%s id=%d", topic_name.c_str(), s.last_policy_id());
        }
        if ((mask & dds::core::status::StatusMask::liveliness_lost()).any()) {
            auto s = writer->liveliness_lost_status();
            LOG_WRN("DDS", "liveliness_lost topic=%s total=%d", topic_name.c_str(), s.total_count());
        }
    }

    // [IDdsEventHandler 구현] WaitSet용 Condition
    dds::core::cond::StatusCondition get_status_condition() override {
        return dds::core::cond::StatusCondition(*writer);
    }

    // [IDdsEventHandler 구현] Listener 모드 제어
    void enable_listener_mode(bool enable) override {
        if (!enable) {
            // 런타임에서 listener를 끄는 동작은 지원하지 않음(무시).
            // 초기화/설정 시점에만 리스너를 활성화하도록 설계됨.
            return;
        }

        // Listener 모드: 내부 리스너 생성 및 등록
        if (!writer_holder_guard) {
            auto lis = std::make_shared<WriterHolderListener>(this);
            writer_holder_guard = lis;
        }
        auto lis = std::static_pointer_cast<WriterHolderListener>(writer_holder_guard);
        writer->set_listener(lis, dds::core::status::StatusMask::publication_matched() |
                                        dds::core::status::StatusMask::offered_incompatible_qos() |
                                        dds::core::status::StatusMask::liveliness_lost());
    }

    struct WriterHolderListener : dds::pub::NoOpDataWriterListener<T> {
        WriterHolder<T>* parent;
        explicit WriterHolderListener(WriterHolder<T>* p) : parent(p) {}
        
        void on_publication_matched(dds::pub::DataWriter<T>&,
                                    const dds::core::status::PublicationMatchedStatus&) override
        {
            parent->process_status(dds::core::status::StatusMask::publication_matched());
        }
        void on_offered_incompatible_qos(dds::pub::DataWriter<T>&,
                                         const dds::core::status::OfferedIncompatibleQosStatus&) override
        {
            parent->process_status(dds::core::status::StatusMask::offered_incompatible_qos());
        }
        void on_liveliness_lost(dds::pub::DataWriter<T>&, const dds::core::status::LivelinessLostStatus&) override
        {
            parent->process_status(dds::core::status::StatusMask::liveliness_lost());
        }
    };

    void writer_holder_listener(const std::string& topic)
    {
        // 하위 호환성 및 초기화용 (이제 enable_listener_mode(true)와 유사)
        // topic 인자는 생성자에서 이미 처리했으므로 무시하거나 갱신
        if (topic_name.empty()) topic_name = topic;
        enable_listener_mode(true);
    }
};

// Reader Holder: 데이터 리더 객체를 타입 안전하게 보관/관리하기 위한 추상 인터페이스
using SampleCallback = std::function<void(const std::string& topic, const std::string& type_name, const AnyData& data)>;

struct IReaderHolder : public IDdsEventHandler {
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
    virtual void set_qos(const dds::sub::qos::DataReaderQos& /*q*/) {}
};

/**
 * @brief 실제 데이터 리더 객체를 타입별로 보관하는 Holder 템플릿
 * @tparam T 데이터 타입
 */
template <typename T>
struct ReaderHolder : IReaderHolder {
    std::shared_ptr<dds::sub::DataReader<T> > reader;
    std::string topic_name;
    SampleCallback sample_callback_; // 콜백을 Holder가 직접 관리

    explicit ReaderHolder(std::shared_ptr<dds::sub::DataReader<T> > r) : reader(std::move(r)) {
        if (reader) {
            // DataReader에는 일부 RTI 버전에서 topic() 멤버가 없을 수 있으므로
            // topic_description()을 통해 토픽명을 안전하게 조회합니다.
            try {
                topic_name = reader->topic_description().name();
            } catch (...) {
                topic_name.clear();
            }
        }
    }
    
    ~ReaderHolder() {
        // DDS Reader 파괴 전 리스너를 명시적으로 해제하여 Use-After-Free 방지
        if (reader) {
            try {
                reader->listener(nullptr, dds::core::status::StatusMask::none());
            } catch (...) {
                // 예외 무시 (소멸자에서 안전)
            }
        }
        listener_guard.reset();
    }

    std::shared_ptr<void> listener_guard;          // 수명 보관
    dds::core::status::StatusMask current_mask{};  // 현재 마스크

    // [IDdsEventHandler 구현] 데이터 처리 (공통 로직)
    void process_data() override {
        if (!sample_callback_) return;
        
        // thread_local 풀: 단일 워커 환경에서 재사용
        thread_local std::vector<std::shared_ptr<T>> sample_pool;
        thread_local size_t pool_index = 0;

        // take()로 데이터 가져오기
        dds::sub::LoanedSamples<T> samples = reader->take();
        for (const auto& sample : samples) {
            if (sample.info().valid()) {
                // 풀에서 재사용 가능한 샘플 확보
                std::shared_ptr<T> sp;
                if (pool_index < sample_pool.size()) {
                    sp = sample_pool[pool_index++];
                    *sp = sample.data(); // 기존 객체 재사용
                } else {
                    sp = std::make_shared<T>(sample.data());
                    sample_pool.push_back(sp);
                    ++pool_index;
                }
                
                std::shared_ptr<void> pv = sp;
                // 콜백 호출 (Context Switching은 콜백 내부에서 처리됨)
                sample_callback_(topic_name, dds::topic::topic_type_name<T>::value(), AnyData(pv));
            }
        }
        
        // 처리 완료 후 풀 인덱스 리셋 (다음 호출에서 재사용)
        pool_index = 0;
    }

    // [IDdsEventHandler 구현] 상태 처리 (공통 로직)
    void process_status(const dds::core::status::StatusMask& mask) override {
        // RTI Modern C++ API의 StatusMask는 std::bitset을 상속받거나 유사하게 동작하지만,
        // bool 컨텍스트로 자동 변환되지 않을 수 있음. any() 메서드를 사용하여 검사.
        if ((mask & dds::core::status::StatusMask::subscription_matched()).any()) {
            auto st = reader->subscription_matched_status();
            LOG_INF("DDS", "sub_matched topic=%s cur=%d total=%d", topic_name.c_str(), st.current_count(), st.total_count());
        }
        if ((mask & dds::core::status::StatusMask::requested_incompatible_qos()).any()) {
            auto st = reader->requested_incompatible_qos_status();
            LOG_WRN("DDS", "req_incompat_qos topic=%s id=%d", topic_name.c_str(), static_cast<int>(st.last_policy_id()));
        }
        if ((mask & dds::core::status::StatusMask::sample_lost()).any()) {
            auto st = reader->sample_lost_status();
            LOG_WRN("DDS", "sample_lost topic=%s total=%d", topic_name.c_str(), st.total_count());
        }
    }

    // [IDdsEventHandler 구현] WaitSet용 Condition
    dds::core::cond::StatusCondition get_status_condition() override {
        return dds::core::cond::StatusCondition(*reader);
    }

    dds::sub::cond::ReadCondition get_read_condition() override {
        // 읽지 않은 새로운 데이터가 있을 때만 트리거
        return dds::sub::cond::ReadCondition(*reader, dds::sub::status::DataState::new_data());
    }

    // [IDdsEventHandler 구현] Listener 모드 제어
    void enable_listener_mode(bool enable) override {
        if (!enable) {
            // 런타임에서 listener를 끄는 동작은 지원하지 않음(무시).
            // 초기화/설정 시점에만 리스너를 활성화하도록 설계됨.
            return;
        }

        if (!listener_guard) {
            auto lis = std::make_shared<ReaderHolderListener>(this);
            listener_guard = lis;
        }
        auto lis = std::static_pointer_cast<ReaderHolderListener>(listener_guard);
        reader->listener(lis.get(), current_mask);
    }

    // 통합 리스너: 샘플 전달 + 상태 로그
    struct ReaderHolderListener : dds::sub::NoOpDataReaderListener<T> {
        ReaderHolder<T>* parent;
        explicit ReaderHolderListener(ReaderHolder<T>* p) : parent(p) {}

        void on_data_available(dds::sub::DataReader<T>&) override
        {
            parent->process_data();
        }

        void on_subscription_matched(dds::sub::DataReader<T>&,
                                     const dds::core::status::SubscriptionMatchedStatus&) override
        {
            parent->process_status(dds::core::status::StatusMask::subscription_matched());
        }
        void on_requested_incompatible_qos(dds::sub::DataReader<T>&,
                                           const dds::core::status::RequestedIncompatibleQosStatus&) override
        {
            parent->process_status(dds::core::status::StatusMask::requested_incompatible_qos());
        }
        void on_sample_lost(dds::sub::DataReader<T>&, const dds::core::status::SampleLostStatus&) override
        {
            parent->process_status(dds::core::status::StatusMask::sample_lost());
        }
    };

    // ① 리스너 설치. 기본은 상태 이벤트만. enable_data=true면 data_available까지 즉시 활성
    std::shared_ptr<void> reader_holder_listener(const std::string& topic, bool enable_data = false) override
    {
        // 토픽명이 인자로 주어지면 갱신, 아니면 내부 저장값이나 reader에서 조회 시도
        if (!topic.empty()) {
            topic_name = topic;
        } else {
            if (topic_name.empty()) {
                try {
                    topic_name = reader->topic_description().name();
                } catch (...) {
                    // 토픽명을 회복할 수 없으면 등록을 건너뜁니다.
                    LOG_ERR("DDS", "Invalid empty topic name (listener registration skipped)");
                    return nullptr;
                }
            }
        }

        LOG_DBG("DDS", "Before registering reader listener: checking enable state topic=%s", topic_name.c_str());
        try {
            reader->enable();
            LOG_INF("DDS", "Reader enable completed topic=%s", topic.c_str());
        } catch (const dds::core::AlreadyClosedError&) {
            LOG_WRN("DDS", "Reader already closed (enable ignored) topic=%s", topic.c_str());
        } catch (const dds::core::Exception& ex) {
            LOG_ERR("DDS", "Reader enable failed topic=%s reason=%s", topic.c_str(), ex.what());
            throw;
        }
        
        current_mask = dds::core::status::StatusMask::subscription_matched() |
                       dds::core::status::StatusMask::requested_incompatible_qos() |
                       dds::core::status::StatusMask::sample_lost();

        if (enable_data) current_mask |= dds::core::status::StatusMask::data_available();

        // Listener 모드 활성화 (기본값)
        enable_listener_mode(true);
        
        LOG_INF("DDS", "Reader listener registered topic=%s mask=0x%X", topic_name.c_str(), current_mask.to_ulong());
        return listener_guard;
    }

    // ② 나중에 콜백 세팅. 필요하면 data_available 마스크 추가 적용
    void set_sample_callback(SampleCallback cb) override
    {
        sample_callback_ = std::move(cb);

        if ((current_mask & dds::core::status::StatusMask::data_available()).none()) {
            current_mask |= dds::core::status::StatusMask::data_available();
            // Listener 모드일 때만 즉시 적용
            if (listener_guard) {
                reader->listener(std::static_pointer_cast<ReaderHolderListener>(listener_guard).get(), current_mask);
            }
        }
    }

    void set_qos(const dds::sub::qos::DataReaderQos& q) override { if (reader) reader->qos(q); }
};

using TopicFactory = std::function<std::shared_ptr<ITopicHolder>(dds::domain::DomainParticipant&, const std::string&)>;
using WriterFactory = std::function<std::shared_ptr<IWriterHolder>(dds::pub::Publisher&, ITopicHolder&, const dds::pub::qos::DataWriterQos*)>;
using ReaderFactory = std::function<std::shared_ptr<IReaderHolder>(dds::sub::Subscriber&, ITopicHolder&, const dds::sub::qos::DataReaderQos*)>;

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
    writer_factories[type_name] = [](dds::pub::Publisher& publisher, ITopicHolder& th, const dds::pub::qos::DataWriterQos* q) {
        auto typed_topic = dynamic_cast<TopicHolder<T>*>(&th);
        if (!typed_topic) throw std::runtime_error("Topic type mismatch for Writer");
        std::shared_ptr<dds::pub::DataWriter<T> > writer;
        if (q) {
            writer = std::make_shared<dds::pub::DataWriter<T> >(publisher, *(typed_topic->topic), *q);
        } else {
            writer = std::make_shared<dds::pub::DataWriter<T> >(publisher, *(typed_topic->topic));
        }
        auto holder = std::make_shared<WriterHolder<T> >(writer);
            // Do not register listener here; DdsManager will enable listeners
            // only when running in Listener mode. This avoids creating listeners
            // unconditionally during factory creation which can conflict with
            // WaitSet mode or cause use-after-free in certain RTI versions.
        return holder;
    };
    reader_factories[type_name] = [](dds::sub::Subscriber& subscriber, ITopicHolder& th, const dds::sub::qos::DataReaderQos* q) {
        auto typed_topic = dynamic_cast<TopicHolder<T>*>(&th);
        if (!typed_topic) throw std::runtime_error("Topic type mismatch for Reader");
        std::shared_ptr<dds::sub::DataReader<T> > reader;
        if (q) {
            reader = std::make_shared<dds::sub::DataReader<T> >(subscriber, *(typed_topic->topic), *q);
        } else {
            reader = std::make_shared<dds::sub::DataReader<T> >(subscriber, *(typed_topic->topic));
        }
        auto holder = std::make_shared<ReaderHolder<T> >(reader);
            // Do not register listener here; DdsManager will enable listeners
            // only when running in Listener mode. This avoids creating listeners
            // unconditionally during factory creation which can conflict with
            // WaitSet mode or cause use-after-free in certain RTI versions.
        return holder;
    };
}

}  // namespace rtpdds
