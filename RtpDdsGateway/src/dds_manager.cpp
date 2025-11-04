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
#include <type_traits>

#include "dds_manager.hpp"
#include "dds_type_registry.hpp"
#include "triad_log.hpp"
#include "sample_factory.hpp"
#include "sample_guard.hpp"

namespace rtpdds {
// Helper utilities for consistent entry logging across DdsManager public methods.
static std::string truncate_for_log(const std::string& s, size_t max_len = 200)
{
    if (s.size() <= max_len) return s;
    return s.substr(0, max_len) + "...(len=" + std::to_string(s.size()) + ")";
}

static void log_entry(const char* fn, const std::string& params)
{
    // Use INFO level for important entry tracing so it is visible in normal runs.
    LOG_INF("DDS", "[ENTRY] %s %s", fn, params.c_str());
}

DdsManager::DdsManager(const std::string& qos_dir)
{
    init_dds_type_registry();
    qos_store_ = std::make_unique<rtpdds::QosStore>(qos_dir);
    qos_store_->initialize();

    LOG_DBG("DDS", "DdsManager initialized. Registered factories:");

    LOG_DBG("DDS", "Registered Topic Factories:");
    for (const auto& factory : topic_factories) {
        LOG_DBG("DDS", "  Topic Type: %s", factory.first.c_str());
    }

    LOG_DBG("DDS", "Registered Writer Factories:");
    for (const auto& factory : writer_factories) {
        LOG_DBG("DDS", "  Writer Type: %s", factory.first.c_str());
    }

    LOG_DBG("DDS", "Registered Reader Factories:");
    for (const auto& factory : reader_factories) {
        LOG_DBG("DDS", "  Reader Type: %s", factory.first.c_str());
    }
}

DdsManager::~DdsManager()
{
    clear_entities();
}

/**
 * @brief 실행 중 모든 DDS 엔티티(Participant, Publisher, Subscriber, Writer, Reader 등)를 초기화(해제)한다.
 * @details 컨테이너를 clear()하여 리소스를 모두 해제하며, 이후 재생성 가능하다.
 * @note DDS 표준 계층 순서에 따라 하위 엔티티부터 상위 순으로 해제:
 *       Reader/Writer → Topic → Subscriber/Publisher → Participant
 */
void DdsManager::clear_entities()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 1단계: Reader/Writer 먼저 해제 (최하위)
    readers_.clear();
    writers_.clear();
    
    // 2단계: Topic 해제
    topics_.clear();
    topic_to_type_.clear();
    
    // 3단계: Subscriber/Publisher 해제
    subscribers_.clear();
    publishers_.clear();
    
    // 4단계: Participant 마지막 해제 (최상위)
    participants_.clear();
    
    LOG_INF("DDS", "clear_entities completed in correct hierarchical order");
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
    log_entry("create_participant", std::string("domain_id=") + std::to_string(domain_id) + ", qos_lib=" + truncate_for_log(qos_lib) + ", qos_profile=" + truncate_for_log(qos_profile));
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (participants_.count(domain_id)) {
        LOG_WRN("DDS", "create_participant: participant already exists for domain=%d", domain_id);
        return DdsResult(false, DdsErrorCategory::Logic,
                         "Participant already exists for domain=" + std::to_string(domain_id));
    }
    std::shared_ptr<dds::domain::DomainParticipant> participant;
    if (qos_store_) {
        if (auto pack = qos_store_->find_or_reload(qos_lib, qos_profile)) {
            try {
                participant = std::make_shared<dds::domain::DomainParticipant>(domain_id, pack->participant);
                LOG_INF("DDS", "[apply-qos] participant domain=%d lib=%s prof=%s %s", domain_id, qos_lib.c_str(), qos_profile.c_str(), summarize_qos(*pack).c_str());
            } catch (const std::exception& ex) {
                LOG_ERR("DDS", "[qos-apply-failed] participant domain=%d lib=%s prof=%s error=%s", domain_id, qos_lib.c_str(), qos_profile.c_str(), ex.what());
                LOG_WRN("DDS", "[apply-qos:default] participant domain=%d (fallback to Default due to qos apply failure)", domain_id);
            }
        }
    }
    if (!participant) {
        participant = std::make_shared<dds::domain::DomainParticipant>(domain_id);
        LOG_WRN("DDS", "[apply-qos:default] participant domain=%d (lib=%s prof=%s not found)", domain_id, qos_lib.c_str(), qos_profile.c_str());
    }
    participants_[domain_id] = participant;
    LOG_INF("DDS", "participant created domain=%d", domain_id);
    return DdsResult(true, DdsErrorCategory::None,
                     "Participant created successfully: domain=" + std::to_string(domain_id));
}

/**
 * @brief 내부 헬퍼: mutex 이미 획득한 상태에서 publisher 생성 (재귀 호출 방지)
 * @note mutex_가 이미 lock된 상태에서만 호출해야 함
 */
DdsResult DdsManager::create_publisher_locked(int domain_id, const std::string& pub_name,
                                              const std::string& qos_lib, const std::string& qos_profile)
{
    // Participant 존재 여부 확인 (자동생성 없음)
    if (!participants_.count(domain_id)) {
        LOG_ERR("DDS", "create_publisher_locked: participant not found for domain=%d", domain_id);
        return DdsResult(false, DdsErrorCategory::Logic,
                        "Participant must be created before publisher: domain=" + std::to_string(domain_id));
    }
    
    auto& participant = participants_[domain_id];
    
    // 중복 확인
    if (publishers_[domain_id].count(pub_name)) {
        LOG_WRN("DDS", "create_publisher_locked: publisher already exists for domain=%d pub=%s", domain_id, pub_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic,
                        "Publisher already exists for domain=" + std::to_string(domain_id) + " pub=" + pub_name);
    }
    
    // Publisher 생성 (create_publisher()와 동일한 로직)
    std::shared_ptr<dds::pub::Publisher> publisher;
    if (qos_store_) {
        if (auto pack = qos_store_->find_or_reload(qos_lib, qos_profile)) {
            try {
                publisher = std::make_shared<dds::pub::Publisher>(*participant, pack->publisher);
                LOG_INF("DDS", "[apply-qos] publisher domain=%d pub=%s lib=%s prof=%s %s", 
                       domain_id, pub_name.c_str(), qos_lib.c_str(), qos_profile.c_str(), summarize_qos(*pack).c_str());
            } catch (const std::exception& ex) {
                LOG_ERR("DDS", "[qos-apply-failed] publisher domain=%d pub=%s lib=%s prof=%s error=%s", 
                       domain_id, pub_name.c_str(), qos_lib.c_str(), qos_profile.c_str(), ex.what());
                LOG_WRN("DDS", "[apply-qos:default] publisher domain=%d pub=%s (fallback to Default due to qos apply failure)", 
                       domain_id, pub_name.c_str());
            }
        }
    }
    if (!publisher) {
        publisher = std::make_shared<dds::pub::Publisher>(*participant);
        LOG_WRN("DDS", "[apply-qos:default] publisher domain=%d pub=%s (lib=%s prof=%s not found)", 
               domain_id, pub_name.c_str(), qos_lib.c_str(), qos_profile.c_str());
    }
    publishers_[domain_id][pub_name] = publisher;
    LOG_INF("DDS", "publisher auto-created domain=%d pub=%s", domain_id, pub_name.c_str());
    return DdsResult(true, DdsErrorCategory::None,
                    "Publisher created successfully: domain=" + std::to_string(domain_id) + " pub=" + pub_name);
}

/**
 * @brief 내부 헬퍼: mutex 이미 획득한 상태에서 subscriber 생성 (재귀 호출 방지)
 * @note mutex_가 이미 lock된 상태에서만 호출해야 함
 */
DdsResult DdsManager::create_subscriber_locked(int domain_id, const std::string& sub_name,
                                               const std::string& qos_lib, const std::string& qos_profile)
{
    // Participant 존재 여부 확인 (자동생성 없음)
    if (!participants_.count(domain_id)) {
        LOG_ERR("DDS", "create_subscriber_locked: participant not found for domain=%d", domain_id);
        return DdsResult(false, DdsErrorCategory::Logic,
                        "Participant must be created before subscriber: domain=" + std::to_string(domain_id));
    }
    
    auto& participant = participants_[domain_id];
    
    // 중복 확인
    if (subscribers_[domain_id].count(sub_name)) {
        LOG_WRN("DDS", "create_subscriber_locked: subscriber already exists for domain=%d sub=%s", domain_id, sub_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic,
                        "Subscriber already exists for domain=" + std::to_string(domain_id) + " sub=" + sub_name);
    }
    
    // Subscriber 생성 (create_subscriber()와 동일한 로직)
    std::shared_ptr<dds::sub::Subscriber> subscriber;
    if (qos_store_) {
        if (auto pack = qos_store_->find_or_reload(qos_lib, qos_profile)) {
            try {
                subscriber = std::make_shared<dds::sub::Subscriber>(*participant, pack->subscriber);
                LOG_INF("DDS", "[apply-qos] subscriber domain=%d sub=%s lib=%s prof=%s %s", 
                       domain_id, sub_name.c_str(), qos_lib.c_str(), qos_profile.c_str(), summarize_qos(*pack).c_str());
            } catch (const std::exception& ex) {
                LOG_ERR("DDS", "[qos-apply-failed] subscriber domain=%d sub=%s lib=%s prof=%s error=%s", 
                       domain_id, sub_name.c_str(), qos_lib.c_str(), qos_profile.c_str(), ex.what());
                LOG_WRN("DDS", "[apply-qos:default] subscriber domain=%d sub=%s (fallback to Default due to qos apply failure)", 
                       domain_id, sub_name.c_str());
            }
        }
    }
    if (!subscriber) {
        subscriber = std::make_shared<dds::sub::Subscriber>(*participant);
        LOG_WRN("DDS", "[apply-qos:default] subscriber domain=%d sub=%s (lib=%s prof=%s not found)", 
               domain_id, sub_name.c_str(), qos_lib.c_str(), qos_profile.c_str());
    }
    subscribers_[domain_id][sub_name] = subscriber;
    LOG_INF("DDS", "subscriber auto-created domain=%d sub=%s", domain_id, sub_name.c_str());
    return DdsResult(true, DdsErrorCategory::None,
                    "Subscriber created successfully: domain=" + std::to_string(domain_id) + " sub=" + sub_name);
}

/**
 * @brief 도메인/이름별 publisher 생성
 * @param domain_id 도메인 ID
 * @param pub_name 퍼블리셔 이름
 * @param qos_lib   QoS 라이브러리명
 * @param qos_profile QoS 프로파일명
 * @return 성공/실패 및 사유
 *
 * participant가 없으면 실패. 이미 있으면 Logic 에러 반환
 */
DdsResult DdsManager::create_publisher(int domain_id, const std::string& pub_name, const std::string& qos_lib,
                                       const std::string& qos_profile)
{
    log_entry("create_publisher", std::string("domain_id=") + std::to_string(domain_id) + ", pub_name=" + truncate_for_log(pub_name) + ", qos_lib=" + truncate_for_log(qos_lib) + ", qos_profile=" + truncate_for_log(qos_profile));
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 내부 헬퍼 함수로 위임 (로직 중복 제거)
    return create_publisher_locked(domain_id, pub_name, qos_lib, qos_profile);
}

/**
 * @brief 도메인/이름별 subscriber 생성
 * @param domain_id 도메인 ID
 * @param sub_name 서브스크라이버 이름
 * @param qos_lib   QoS 라이브러리명
 * @param qos_profile QoS 프로파일명
 * @return 성공/실패 및 사유
 *
 * participant가 없으면 실패. 이미 있으면 Logic 에러 반환
 */
DdsResult DdsManager::create_subscriber(int domain_id, const std::string& sub_name, const std::string& qos_lib,
                                        const std::string& qos_profile)
{
    log_entry("create_subscriber", std::string("domain_id=") + std::to_string(domain_id) + ", sub_name=" + truncate_for_log(sub_name) + ", qos_lib=" + truncate_for_log(qos_lib) + ", qos_profile=" + truncate_for_log(qos_profile));
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 내부 헬퍼 함수로 위임 (로직 중복 제거)
    return create_subscriber_locked(domain_id, sub_name, qos_lib, qos_profile);
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
 * 상위 participant/publisher가 없으면 자동 생성. 
 * 동일 (domain, pub, topic) 조합에 대해 중복 생성 시도 시 실패 반환.
 * topic_factories, writer_factories에서 타입별 팩토리 함수 사용
 */
DdsResult DdsManager::create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
                                    const std::string& type_name, const std::string& qos_lib,
                                    const std::string& qos_profile, uint64_t* out_id)
{
    log_entry("create_writer", std::string("domain_id=") + std::to_string(domain_id) + ", pub_name=" + truncate_for_log(pub_name) + ", topic=" + truncate_for_log(topic) + ", type_name=" + truncate_for_log(type_name) + ", qos_lib=" + truncate_for_log(qos_lib) + ", qos_profile=" + truncate_for_log(qos_profile));
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 타입 등록 여부 확인 (먼저 검증)
    const auto& reg = idlmeta::type_registry();
    if (reg.find(type_name) == reg.end()) {
        LOG_ERR("DDS", "create_writer: unknown DDS type: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "Unknown DDS type: " + type_name);
    }
    
    // Topic 타입 불일치 검증: 동일 토픽명에 다른 타입 생성 시도 차단
    auto dom_type_it = topic_to_type_.find(domain_id);
    if (dom_type_it != topic_to_type_.end()) {
        auto existing_type_it = dom_type_it->second.find(topic);
        if (existing_type_it != dom_type_it->second.end()) {
            const std::string& existing_type = existing_type_it->second;
            if (existing_type != type_name) {
                LOG_ERR("DDS", "create_writer: topic='%s' already exists with type='%s', cannot create with type='%s'",
                        topic.c_str(), existing_type.c_str(), type_name.c_str());
                return DdsResult(false, DdsErrorCategory::Logic,
                                "Topic '" + topic + "' already exists with type '" + existing_type + 
                                "', cannot create with different type '" + type_name + "'");
            }
        }
    }
    
    // Participant 존재 확인 (자동생성 없음)
    if (!participants_.count(domain_id)) {
        LOG_ERR("DDS", "create_writer: participant not found for domain=%d (must be created first)", domain_id);
        return DdsResult(false, DdsErrorCategory::Logic,
                        "Participant must be created before writer: domain=" + std::to_string(domain_id));
    }
    
    // Publisher 자동생성 (헬퍼 함수 사용)
    if (!publishers_[domain_id].count(pub_name)) {
        DdsResult res = create_publisher_locked(domain_id, pub_name, qos_lib, qos_profile);
        if (!res.ok) return res;
    }

    auto& participant = participants_[domain_id];
    auto& publisher = publishers_[domain_id][pub_name];
    
    // 중복 Writer 생성 차단: 동일 (domain, pub, topic)에 대해 이미 존재하면 실패
    auto& topic_writers = writers_[domain_id][pub_name][topic];
    if (!topic_writers.empty()) {
        LOG_WRN("DDS", "create_writer: writer already exists for domain=%d pub=%s topic=%s (id=%llu)", 
                domain_id, pub_name.c_str(), topic.c_str(), 
                static_cast<unsigned long long>(topic_writers[0].id));
        return DdsResult(false, DdsErrorCategory::Logic,
                        "Writer already exists for domain=" + std::to_string(domain_id) + 
                        " pub=" + pub_name + " topic=" + topic + 
                        " (id=" + std::to_string(topic_writers[0].id) + ")");
    }

    // TopicHolder 생성 및 저장 (participant(domain) 스코프에서 재사용)
    std::shared_ptr<ITopicHolder> topic_holder;
    auto& domain_topics = topics_[domain_id];
    auto topic_it = domain_topics.find(topic);
    if (topic_it != domain_topics.end()) {
        topic_holder = topic_it->second;
    } else {
        topic_holder = topic_factories[type_name](*participant, topic);
        domain_topics[topic] = topic_holder;
    }

    // QoS 조회 및 적용(Topic)
    if (qos_store_) {
        if (auto pack = qos_store_->find_or_reload(qos_lib, qos_profile)) {
            try {
                topic_holder->set_qos(pack->topic);
                LOG_INF("DDS", "[apply-qos] topic=%s lib=%s prof=%s %s", topic.c_str(), qos_lib.c_str(), qos_profile.c_str(), summarize_qos(*pack).c_str());
            } catch (const std::exception& ex) {
                LOG_ERR("DDS", "[qos-apply-failed] topic=%s lib=%s prof=%s error=%s", topic.c_str(), qos_lib.c_str(), qos_profile.c_str(), ex.what());
                LOG_WRN("DDS", "[apply-qos:default] topic=%s (fallback to Default due to qos apply failure)", topic.c_str());
            }
        } else {
            LOG_WRN("DDS", "[apply-qos:default] topic=%s (lib=%s prof=%s not found)", topic.c_str(), qos_lib.c_str(), qos_profile.c_str());
        }
    }
    // WriterHolder 생성 (Topic 객체 활용) — create with QoS when possible to avoid immutable-policy errors
    std::shared_ptr<IWriterHolder> writer_holder;
    const dds::pub::qos::DataWriterQos* writer_q = nullptr;
    std::optional<rtpdds::QosPack> writer_pack;
    if (qos_store_) {
        if (auto pack = qos_store_->find_or_reload(qos_lib, qos_profile)) {
            writer_pack = *pack;
            writer_q = &writer_pack->writer;
        }
    }

    try {
        writer_holder = writer_factories[type_name](*publisher, *topic_holder, writer_q);
        if (writer_q) {
            LOG_INF("DDS", "[apply-qos] writer created with QoS topic=%s lib=%s prof=%s %s", topic.c_str(), qos_lib.c_str(), qos_profile.c_str(), (writer_pack ? writer_pack->origin_file.c_str() : "(builtin)"));
        } else {
            LOG_WRN("DDS", "[apply-qos:default] writer topic=%s (lib=%s prof=%s not found)", topic.c_str(), qos_lib.c_str(), qos_profile.c_str());
        }
    } catch (const std::exception& ex) {
        LOG_ERR("DDS", "create_writer: failed to create writer with requested QoS: %s", ex.what());
        // Attempt fallback: create writer without QoS
        try {
            writer_holder = writer_factories[type_name](*publisher, *topic_holder, nullptr);
            LOG_WRN("DDS", "create_writer: fallback to default writer QoS for topic=%s", topic.c_str());
        } catch (const std::exception& ex2) {
            LOG_ERR("DDS", "create_writer: failed to create writer (fallback also failed): %s", ex2.what());
            return DdsResult(false, DdsErrorCategory::Resource, std::string("Writer creation failed: ") + ex2.what());
        }
    }
    // Generate holder id and append writer to list for this (domain,pub_name,topic)
    DdsManager::HolderId id = next_holder_id_.fetch_add(1);
    writers_[domain_id][pub_name][topic].push_back({id, writer_holder});
    if (out_id) *out_id = static_cast<uint64_t>(id);
    // topic과 type_name 매핑 저장 (도메인별로 저장)
    topic_to_type_[domain_id][topic] = type_name;
    LOG_INF("DDS", "writer created id=%llu domain=%d pub=%s topic=%s", static_cast<unsigned long long>(id), domain_id, pub_name.c_str(), topic.c_str());
    return DdsResult(
        true, DdsErrorCategory::None,
        "Writer created successfully: id=" + std::to_string(id) + " domain=" + std::to_string(domain_id) + " pub=" + pub_name + " topic=" + topic);
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
 * 상위 participant/subscriber가 없으면 자동 생성. 
 * 동일 (domain, sub, topic) 조합에 대해 중복 생성 시도 시 실패 반환.
 * topic_factories, reader_factories에서 타입별 팩토리 함수 사용
 * 샘플 수신 콜백(on_sample_)이 등록되어 있으면 리스너 부착
 */
DdsResult DdsManager::create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
                                    const std::string& type_name, const std::string& qos_lib,
                                    const std::string& qos_profile, uint64_t* out_id)
{
    log_entry("create_reader", std::string("domain_id=") + std::to_string(domain_id) + ", sub_name=" + truncate_for_log(sub_name) + ", topic=" + truncate_for_log(topic) + ", type_name=" + truncate_for_log(type_name) + ", qos_lib=" + truncate_for_log(qos_lib) + ", qos_profile=" + truncate_for_log(qos_profile));
    LOG_INF("DDS", "reader ready domain=%d sub=%s topic=%s type=%s", domain_id, sub_name.c_str(), topic.c_str(), type_name.c_str());
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 타입 등록 여부 확인 (먼저 검증)
    const auto& reg = idlmeta::type_registry();
    if (reg.find(type_name) == reg.end()) {
        LOG_ERR("DDS", "create_reader: unknown DDS type: %s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "Unknown DDS type: " + type_name);
    }
    
    // Topic 타입 불일치 검증: 동일 토픽명에 다른 타입 생성 시도 차단
    auto dom_type_it = topic_to_type_.find(domain_id);
    if (dom_type_it != topic_to_type_.end()) {
        auto existing_type_it = dom_type_it->second.find(topic);
        if (existing_type_it != dom_type_it->second.end()) {
            const std::string& existing_type = existing_type_it->second;
            if (existing_type != type_name) {
                LOG_ERR("DDS", "create_reader: topic='%s' already exists with type='%s', cannot create with type='%s'",
                        topic.c_str(), existing_type.c_str(), type_name.c_str());
                return DdsResult(false, DdsErrorCategory::Logic,
                                "Topic '" + topic + "' already exists with type '" + existing_type + 
                                "', cannot create with different type '" + type_name + "'");
            }
        }
    }
    
    // Participant 존재 확인 (자동생성 없음)
    if (!participants_.count(domain_id)) {
        LOG_ERR("DDS", "create_reader: participant not found for domain=%d (must be created first)", domain_id);
        return DdsResult(false, DdsErrorCategory::Logic,
                        "Participant must be created before reader: domain=" + std::to_string(domain_id));
    }
    
    // Subscriber 자동생성 (헬퍼 함수 사용)
    if (!subscribers_[domain_id].count(sub_name)) {
        DdsResult res = create_subscriber_locked(domain_id, sub_name, qos_lib, qos_profile);
        if (!res.ok) return res;
    }

    auto& participant = participants_[domain_id];
    auto& subscriber = subscribers_[domain_id][sub_name];
    
    // 중복 Reader 생성 차단: 동일 (domain, sub, topic)에 대해 이미 존재하면 실패
    auto& topic_readers = readers_[domain_id][sub_name][topic];
    if (!topic_readers.empty()) {
        LOG_WRN("DDS", "create_reader: reader already exists for domain=%d sub=%s topic=%s (id=%llu)", 
                domain_id, sub_name.c_str(), topic.c_str(), 
                static_cast<unsigned long long>(topic_readers[0].id));
        return DdsResult(false, DdsErrorCategory::Logic,
                        "Reader already exists for domain=" + std::to_string(domain_id) + 
                        " sub=" + sub_name + " topic=" + topic + 
                        " (id=" + std::to_string(topic_readers[0].id) + ")");
    }

    // TopicHolder 재사용 (Writer 생성 시 저장된 객체)
    // TopicHolder 재사용 (participant(domain) 스코프)
    std::shared_ptr<ITopicHolder> topic_holder;
    auto& domain_topics_r = topics_[domain_id];
    auto topic_it_r = domain_topics_r.find(topic);
    if (topic_it_r != domain_topics_r.end()) {
        topic_holder = topic_it_r->second;
    } else {
        // 없으면 새로 생성
        topic_holder = topic_factories[type_name](*participant, topic);
        domain_topics_r[topic] = topic_holder;
    }

    // QoS 조회 및 적용(Topic)
    if (qos_store_) {
        if (auto pack = qos_store_->find_or_reload(qos_lib, qos_profile)) {
            topic_holder->set_qos(pack->topic);
            LOG_INF("DDS", "[apply-qos] topic=%s lib=%s prof=%s %s", topic.c_str(), qos_lib.c_str(), qos_profile.c_str(), summarize_qos(*pack).c_str());
        } else {
            LOG_WRN("DDS", "[apply-qos:default] topic=%s (lib=%s prof=%s not found)", topic.c_str(), qos_lib.c_str(), qos_profile.c_str());
        }
    }

    // ReaderHolder 생성 with QoS when possible
    std::shared_ptr<IReaderHolder> reader_holder;
    const dds::sub::qos::DataReaderQos* reader_q = nullptr;
    std::optional<rtpdds::QosPack> reader_pack;
    if (qos_store_) {
        if (auto pack = qos_store_->find_or_reload(qos_lib, qos_profile)) {
            reader_pack = *pack;
            reader_q = &reader_pack->reader;
        }
    }

    try {
        reader_holder = reader_factories[type_name](*subscriber, *topic_holder, reader_q);
        if (reader_q) {
            LOG_INF("DDS", "[apply-qos] reader created with QoS topic=%s lib=%s prof=%s %s", topic.c_str(), qos_lib.c_str(), qos_profile.c_str(), (reader_pack ? reader_pack->origin_file.c_str() : "(builtin)"));
        } else {
            LOG_WRN("DDS", "[apply-qos:default] reader topic=%s (lib=%s prof=%s not found)", topic.c_str(), qos_lib.c_str(), qos_profile.c_str());
        }
    } catch (const std::exception& ex) {
        LOG_ERR("DDS", "create_reader: failed to create reader with requested QoS: %s", ex.what());
        try {
            reader_holder = reader_factories[type_name](*subscriber, *topic_holder, nullptr);
            LOG_WRN("DDS", "create_reader: fallback to default reader QoS for topic=%s", topic.c_str());
        } catch (const std::exception& ex2) {
            LOG_ERR("DDS", "create_reader: failed to create reader (fallback also failed): %s", ex2.what());
            return DdsResult(false, DdsErrorCategory::Resource, std::string("Reader creation failed: ") + ex2.what());
        }
    }
    // Generate id and append reader to list for this (domain,sub_name,topic)
    DdsManager::HolderId id = next_holder_id_.fetch_add(1);
    readers_[domain_id][sub_name][topic].push_back({id, reader_holder});
    if (out_id) *out_id = static_cast<uint64_t>(id);

    // 타입 소거 리스너 부착 및 수명 유지
    if (on_sample_) {
        reader_holder->set_sample_callback(on_sample_);
    LOG_DBG("DDS", "listener attached topic=%s", topic.c_str());
    }

    LOG_INF("DDS", "reader created id=%llu domain=%d sub=%s topic=%s", static_cast<unsigned long long>(id), domain_id, sub_name.c_str(), topic.c_str());
    return DdsResult(
        true, DdsErrorCategory::None,
        "Reader created successfully: id=" + std::to_string(id) + " domain=" + std::to_string(domain_id) + " sub=" + sub_name + " topic=" + topic);
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
DdsResult DdsManager::publish_json(const std::string& topic, const nlohmann::json& j)
{
    log_entry("publish_json", std::string("topic=") + truncate_for_log(topic) + ", jsize=" + std::to_string(j.dump().size()));
    if (!j.is_object()) {
        LOG_ERR("DDS", "publish_json: payload is not a JSON object for topic=%s", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "payload must be a JSON object");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    int count = 0;
    for (const auto& dom : writers_) {
        int domain_id = dom.first;
        for (const auto& pub : dom.second) {
            auto it = pub.second.find(topic);
            if (it != pub.second.end()) {
                const auto& entries = it->second; // vector of WriterEntry
                std::string type_name;
                auto dom_type_it = topic_to_type_.find(domain_id);
                if (dom_type_it == topic_to_type_.end()) {
                    LOG_ERR("DDS", "publish_json: type_name not found for topic=%s in domain=%d", topic.c_str(), domain_id);
                    continue;
                }
                auto type_it = dom_type_it->second.find(topic);
                if (type_it == dom_type_it->second.end()) {
                    LOG_ERR("DDS", "publish_json: type_name not found for topic=%s in domain=%d", topic.c_str(), domain_id);
                    continue;
                }
                type_name = type_it->second;
                LOG_DBG("DDS", "publish_json: type_name=%s", type_name.c_str());

                // RAII: 샘플 자동 관리
                SampleGuard sample_guard(type_name);
                if (!sample_guard) {
                    LOG_ERR("DDS", "publish_json: failed to create sample for type=%s", type_name.c_str());
                    continue;
                }
                LOG_DBG("DDS", "publish_json: sample created for type=%s", type_name.c_str());

                if (!rtpdds::json_to_dds(j, type_name, sample_guard.get())) {
                    LOG_ERR("DDS", "publish_json: json_to_dds failed for type=%s", type_name.c_str());
                    continue;  // 자동 해제
                }
                LOG_DBG("DDS", "publish_json: json_to_dds succeeded for type=%s", type_name.c_str());

                try {
                    std::any wrapped_sample = sample_guard.get();
                    for (const auto& entry : entries) {
                        entry.holder->write_any(wrapped_sample);
                    }
                } catch (const std::bad_any_cast& e) {
                    LOG_ERR("DDS", "publish_json: bad_any_cast exception: %s", e.what());
                    LOG_ERR("DDS", "publish_json: WriterHolder may not support type=%s", type_name.c_str());
                    continue;  // 자동 해제
                }

                // 스코프 종료 시 자동 해제
                LOG_INF("DDS", "write ok topic=%s domain=%d pub=%s size=%zu", topic.c_str(), dom.first, pub.first.c_str(), j.dump().size());
                count += static_cast<int>(entries.size());
            }
        }
    }
    if (count == 0) {
        LOG_ERR("DDS", "publish_json: topic=%s writer not found or invalid type/sample", topic.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "Writer not found or invalid type/sample for topic: " + topic);
    }
    if (count > 1) {
        LOG_WRN("DDS", "publish_json: topic=%s published to %d writers (duplicate transmission warning)", topic.c_str(), count);
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
DdsResult DdsManager::publish_json(int domain_id, const std::string& pub_name, const std::string& topic,
                                   const nlohmann::json& j)
{
    log_entry("publish_json(domain)", std::string("domain_id=") + std::to_string(domain_id) + ", pub_name=" + truncate_for_log(pub_name) + ", topic=" + truncate_for_log(topic) + ", jsize=" + std::to_string(j.dump().size()));
    if (!j.is_object()) {
        LOG_ERR("DDS", "publish_json: payload is not a JSON object for topic=%s domain=%d", topic.c_str(), domain_id);
        return DdsResult(false, DdsErrorCategory::Logic, "payload must be a JSON object");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto domIt = writers_.find(domain_id);
    if (domIt == writers_.end()) {
        LOG_ERR("DDS", "publish_json: domain=%d not found", domain_id);
        return DdsResult(false, DdsErrorCategory::Logic, "Domain not found: " + std::to_string(domain_id));
    }
    auto pubIt = domIt->second.find(pub_name);
    if (pubIt == domIt->second.end()) {
        LOG_ERR("DDS", "publish_json: publisher=%s not found in domain=%d", pub_name.c_str(), domain_id);
        return DdsResult(false, DdsErrorCategory::Logic, "Publisher not found: " + pub_name);
    }
    auto topicIt = pubIt->second.find(topic);
    if (topicIt == pubIt->second.end()) {
        LOG_ERR("DDS", "publish_json: topic=%s not found in publisher=%s domain=%d", topic.c_str(), pub_name.c_str(), domain_id);
        return DdsResult(false, DdsErrorCategory::Logic, "Topic not found: " + topic);
    }
    auto& entries = topicIt->second;
    std::string type_name;
    auto dom_type_it = topic_to_type_.find(domain_id);
    if (dom_type_it != topic_to_type_.end()) {
        auto type_it = dom_type_it->second.find(topic);
        if (type_it != dom_type_it->second.end()) {
            type_name = type_it->second;
        }
    }
    if (type_name.empty()) {
        LOG_ERR("DDS", "publish_json: type_name not found for topic=%s in domain=%d", topic.c_str(), domain_id);
        return DdsResult(false, DdsErrorCategory::Logic, "type_name not found for topic: " + topic);
    }
    
    // RAII: 샘플 자동 관리
    SampleGuard sample_guard(type_name);
    if (!sample_guard) {
        LOG_ERR("DDS", "publish_json: failed to create sample for type=%s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "failed to create sample for type: " + type_name);
    }
    
    if (!rtpdds::json_to_dds(j, type_name, sample_guard.get())) {
        LOG_ERR("DDS", "publish_json: json_to_dds failed for type=%s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "json_to_dds failed for type: " + type_name);
        // 자동 해제
    }
    
    try {
        std::any wrapped_sample = sample_guard.get();
        for (const auto& entry : entries) {
            entry.holder->write_any(wrapped_sample);
        }
    } catch (const std::bad_any_cast& e) {
        LOG_ERR("DDS", "publish_json: bad_any_cast exception: %s", e.what());
        LOG_ERR("DDS", "publish_json: WriterHolder may not support type=%s", type_name.c_str());
        return DdsResult(false, DdsErrorCategory::Logic, "WriterHolder type mismatch");
        // 자동 해제
    }
    
    // 스코프 종료 시 자동 해제
    LOG_INF("DDS", "write ok domain=%d pub=%s topic=%s size=%zu", domain_id, pub_name.c_str(), topic.c_str(), j.dump().size());
    return DdsResult(true, DdsErrorCategory::None,
                     "Publish succeeded: domain=" + std::to_string(domain_id) + " pub=" + pub_name + " topic=" + topic);
}

/**
 * @brief 샘플 수신 콜백 핸들러 등록
 * @param cb 콜백 함수
 */
void DdsManager::set_on_sample(SampleHandler cb)
{
    std::lock_guard<std::mutex> lock(mutex_);
    on_sample_ = std::move(cb);
    LOG_DBG("DDS", "on_sample handler installed");
}

/**
 * @brief Remove a writer by its holder id
 */
DdsResult DdsManager::remove_writer(uint64_t id)
{
    log_entry("remove_writer", std::string("id=") + std::to_string(id));
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto domIt = writers_.begin(); domIt != writers_.end(); ++domIt) {
        int domain_id = domIt->first;
        auto &pubmap = domIt->second;
        for (auto pubIt = pubmap.begin(); pubIt != pubmap.end(); ) {
            auto &topic_map = pubIt->second;
            for (auto topicIt = topic_map.begin(); topicIt != topic_map.end(); ) {
                auto &vec = topicIt->second;
                auto it = std::find_if(vec.begin(), vec.end(), [id](const WriterEntry &e) { return e.id == id; });
                if (it != vec.end()) {
                    vec.erase(it);
                    LOG_INF("DDS", "removed writer id=%llu domain=%d pub=%s topic=%s", static_cast<unsigned long long>(id), domain_id, pubIt->first.c_str(), topicIt->first.c_str());
                    // cleanups
                    if (vec.empty()) {
                        topicIt = topic_map.erase(topicIt);
                    } else {
                        ++topicIt;
                    }
                    if (topic_map.empty()) {
                        pubIt = pubmap.erase(pubIt);
                    }
                    // If no more writers/readers exist for this topic in this domain, remove topic_to_type_
                    bool has_any = false;
                    // search writers_
                    auto wdomIt = writers_.find(domain_id);
                    if (wdomIt != writers_.end()) {
                        for (const auto &p : wdomIt->second) {
                            if (p.second.count(topicIt==topic_map.end() ? std::string() : topicIt->first)) { has_any = true; break; }
                        }
                    }
                    // search readers_ if still not found
                    if (!has_any) {
                        auto rdomIt = readers_.find(domain_id);
                        if (rdomIt != readers_.end()) {
                            for (const auto &p : rdomIt->second) {
                                if (p.second.count(topicIt==topic_map.end() ? std::string() : topicIt->first)) { has_any = true; break; }
                            }
                        }
                    }
                    if (!has_any) {
                        auto dom_type_it = topic_to_type_.find(domain_id);
                        if (dom_type_it != topic_to_type_.end()) {
                            dom_type_it->second.erase(topicIt==topic_map.end() ? std::string() : topicIt->first);
                        }
                    }
                    return DdsResult(true, DdsErrorCategory::None, "Writer removed: id=" + std::to_string(id));
                } else {
                    ++topicIt;
                }
            }
            ++pubIt;
        }
    }
    return DdsResult(false, DdsErrorCategory::Logic, "Writer id not found: " + std::to_string(id));
}

/**
 * @brief Remove a reader by its holder id
 */
DdsResult DdsManager::remove_reader(uint64_t id)
{
    log_entry("remove_reader", std::string("id=") + std::to_string(id));
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto domIt = readers_.begin(); domIt != readers_.end(); ++domIt) {
        int domain_id = domIt->first;
        auto &submap = domIt->second;
        for (auto subIt = submap.begin(); subIt != submap.end(); ) {
            auto &topic_map = subIt->second;
            for (auto topicIt = topic_map.begin(); topicIt != topic_map.end(); ) {
                auto &vec = topicIt->second;
                auto it = std::find_if(vec.begin(), vec.end(), [id](const ReaderEntry &e) { return e.id == id; });
                if (it != vec.end()) {
                    vec.erase(it);
                    LOG_INF("DDS", "removed reader id=%llu domain=%d sub=%s topic=%s", static_cast<unsigned long long>(id), domain_id, subIt->first.c_str(), topicIt->first.c_str());
                    if (vec.empty()) {
                        topicIt = topic_map.erase(topicIt);
                    } else {
                        ++topicIt;
                    }
                    if (topic_map.empty()) {
                        subIt = submap.erase(subIt);
                    }
                    return DdsResult(true, DdsErrorCategory::None, "Reader removed: id=" + std::to_string(id));
                } else {
                    ++topicIt;
                }
            }
            ++subIt;
        }
    }
    return DdsResult(false, DdsErrorCategory::Logic, "Reader id not found: " + std::to_string(id));
}


std::string DdsManager::summarize_qos(const rtpdds::QosPack& p)
{
    // 최소 요약: 원본 XML 파일 경로만 반환하여 RTI QoS wrapper 타입의 멤버 접근을 피함
    if (p.origin_file.empty()) return std::string("(qos:default)");
    return std::string("(qos from=") + p.origin_file + ")";
}

nlohmann::json DdsManager::list_qos_profiles(bool include_builtin, bool include_detail) const
{
    // Log entry with parameter values
    log_entry("list_qos_profiles", std::string("include_builtin=") + (include_builtin ? "true" : "false") + ", include_detail=" + (include_detail ? "true" : "false"));

    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!qos_store_) {
        nlohmann::json base = nlohmann::json::object();
        base["result"] = nlohmann::json::array();
        if (include_detail) base["detail"] = nlohmann::json::array();
        return base;
    }

    nlohmann::json out = nlohmann::json::object();
    out["result"] = qos_store_->list_profiles(include_builtin);
    if (include_detail) {
        out["detail"] = qos_store_->detail_profiles(include_builtin);
    }
    return out;
}

std::string DdsManager::add_or_update_qos_profile(const std::string& library, 
                                                   const std::string& profile, 
                                                   const std::string& profile_xml)
{
    log_entry("add_or_update_qos_profile", "library=" + library + " profile=" + profile);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!qos_store_) {
        LOG_ERR("DDS", "add_or_update_qos_profile: qos_store not initialized");
        return "";
    }
    
    return qos_store_->add_or_update_profile(library, profile, profile_xml);
}

}  // namespace rtpdds
