/**
 * @file dds_manager_entities.cpp
 * @brief DdsManager 엔티티(Participant/Publisher/Subscriber/Topic/Writer/Reader) 생성 구현
 */

#include <optional>

#include "dds_manager.hpp"
#include "dds_manager_internal.hpp"

using rtpdds::internal::log_entry;
using rtpdds::internal::truncate_for_log;

namespace rtpdds {

/**
 * @brief 도메인 ID로 DomainParticipant를 생성
 * @param domain_id 도메인 ID
 * @param qos_lib QoS 라이브러리명(미존재/실패 시 기본 QoS 폴백)
 * @param qos_profile QoS 프로파일명
 * @return 성공/실패 및 사유
 * @details
 * - 이미 해당 도메인에 participant가 존재하면 Logic 에러를 반환합니다.
 * - QoS 적용 실패 시 예외를 잡아 기본 QoS로 폴백합니다.
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
	LOG_FLOW("participant created domain=%d", domain_id);
	return DdsResult(true, DdsErrorCategory::None,
					 "Participant created successfully: domain=" + std::to_string(domain_id));
}

/**
 * @brief 내부 헬퍼: publisher 생성(뮤텍스 보유 상태에서 호출)
 * @details
 * - participants_에 해당 도메인이 있어야 합니다. 없으면 Logic 에러를 반환합니다.
 * - 동일 pub_name이 이미 존재하면 중복 생성으로 간주합니다.
 * - QoS 적용을 시도하며 실패 시 기본 QoS로 생성합니다.
 * - 생성된 Publisher는 publishers_[domain_id][pub_name]에 저장됩니다.
 */
DdsResult DdsManager::create_publisher_locked(int domain_id, const std::string& pub_name,
											  const std::string& qos_lib, const std::string& qos_profile)
{
	if (!participants_.count(domain_id)) {
		LOG_ERR("DDS", "create_publisher_locked: participant not found for domain=%d", domain_id);
		return DdsResult(false, DdsErrorCategory::Logic,
						"Participant must be created before publisher: domain=" + std::to_string(domain_id));
	}
	auto& participant = participants_[domain_id];

	if (publishers_[domain_id].count(pub_name)) {
		LOG_WRN("DDS", "create_publisher_locked: publisher already exists for domain=%d pub=%s", domain_id, pub_name.c_str());
		return DdsResult(false, DdsErrorCategory::Logic,
						"Publisher already exists for domain=" + std::to_string(domain_id) + " pub=" + pub_name);
	}

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
	LOG_FLOW("publisher auto-created domain=%d pub=%s", domain_id, pub_name.c_str());
	return DdsResult(true, DdsErrorCategory::None,
					"Publisher created successfully: domain=" + std::to_string(domain_id) + " pub=" + pub_name);
}

/**
 * @brief 내부 헬퍼: subscriber 생성(뮤텍스 보유 상태에서 호출)
 * @details
 * - participants_에 해당 도메인이 있어야 합니다. 없으면 Logic 에러를 반환합니다.
 * - 동일 sub_name이 이미 존재하면 중복 생성으로 간주합니다.
 * - QoS 적용을 시도하며 실패 시 기본 QoS로 생성합니다.
 * - 생성된 Subscriber는 subscribers_[domain_id][sub_name]에 저장됩니다.
 */
DdsResult DdsManager::create_subscriber_locked(int domain_id, const std::string& sub_name,
											   const std::string& qos_lib, const std::string& qos_profile)
{
	if (!participants_.count(domain_id)) {
		LOG_ERR("DDS", "create_subscriber_locked: participant not found for domain=%d", domain_id);
		return DdsResult(false, DdsErrorCategory::Logic,
						"Participant must be created before subscriber: domain=" + std::to_string(domain_id));
	}
	auto& participant = participants_[domain_id];

	if (subscribers_[domain_id].count(sub_name)) {
		LOG_WRN("DDS", "create_subscriber_locked: subscriber already exists for domain=%d sub=%s", domain_id, sub_name.c_str());
		return DdsResult(false, DdsErrorCategory::Logic,
						"Subscriber already exists for domain=" + std::to_string(domain_id) + " sub=" + sub_name);
	}

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
	LOG_FLOW("subscriber auto-created domain=%d sub=%s", domain_id, sub_name.c_str());
	return DdsResult(true, DdsErrorCategory::None,
					"Subscriber created successfully: domain=" + std::to_string(domain_id) + " sub=" + sub_name);
}

/**
 * @brief 공개 API: publisher 생성(내부 잠금 포함)
 */
DdsResult DdsManager::create_publisher(int domain_id, const std::string& pub_name, const std::string& qos_lib,
									   const std::string& qos_profile)
{
	log_entry("create_publisher", std::string("domain_id=") + std::to_string(domain_id) + ", pub_name=" + truncate_for_log(pub_name) + ", qos_lib=" + truncate_for_log(qos_lib) + ", qos_profile=" + truncate_for_log(qos_profile));
	std::lock_guard<std::mutex> lock(mutex_);
	return create_publisher_locked(domain_id, pub_name, qos_lib, qos_profile);
}

/**
 * @brief 공개 API: subscriber 생성(내부 잠금 포함)
 */
DdsResult DdsManager::create_subscriber(int domain_id, const std::string& sub_name, const std::string& qos_lib,
										const std::string& qos_profile)
{
	log_entry("create_subscriber", std::string("domain_id=") + std::to_string(domain_id) + ", sub_name=" + truncate_for_log(sub_name) + ", qos_lib=" + truncate_for_log(qos_lib) + ", qos_profile=" + truncate_for_log(qos_profile));
	std::lock_guard<std::mutex> lock(mutex_);
	return create_subscriber_locked(domain_id, sub_name, qos_lib, qos_profile);
}

/**
 * @brief writer 생성: 타입 검증 → topic 재사용/생성 → QoS 적용 → WriterHolder 생성
 * @details
 * - mutex_를 획득하여 안전하게 상태를 검사/수정합니다.
 * - IDL 타입 레지스트리에서 type_name 존재를 확인합니다.
 * - 같은 topic이 이미 다른 타입으로 사용 중이면 에러를 반환합니다.
 * - participant/publisher가 없으면 자동으로 생성합니다.
 * - topics_에서 topic_holder를 재사용하거나 factory로 생성하여 도메인 스코프에서 캐시합니다.
 * - QoS를 적용 시도하고 실패 시 기본 QoS로 폴백합니다.
 * - writer_factories[type_name]를 사용해 IWriterHolder를 생성하고 writers_에 등록합니다.
 */
DdsResult DdsManager::create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
									const std::string& type_name, const std::string& qos_lib,
									const std::string& qos_profile, uint64_t* out_id)
{
	log_entry("create_writer", std::string("domain_id=") + std::to_string(domain_id) + ", pub_name=" + truncate_for_log(pub_name) + ", topic=" + truncate_for_log(topic) + ", type_name=" + truncate_for_log(type_name) + ", qos_lib=" + truncate_for_log(qos_lib) + ", qos_profile=" + truncate_for_log(qos_profile));
	std::lock_guard<std::mutex> lock(mutex_);

	const auto& reg = idlmeta::type_registry();
	if (reg.find(type_name) == reg.end()) {
		LOG_ERR("DDS", "create_writer: unknown DDS type: %s", type_name.c_str());
		return DdsResult(false, DdsErrorCategory::Logic, "Unknown DDS type: " + type_name);
	}

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

	if (!participants_.count(domain_id)) {
		LOG_ERR("DDS", "create_writer: participant not found for domain=%d (must be created first)", domain_id);
		return DdsResult(false, DdsErrorCategory::Logic,
						"Participant must be created before writer: domain=" + std::to_string(domain_id));
	}

	if (!publishers_[domain_id].count(pub_name)) {
		DdsResult res = create_publisher_locked(domain_id, pub_name, qos_lib, qos_profile);
		if (!res.ok) return res;
	}

	auto& participant = participants_[domain_id];
	auto& publisher = publishers_[domain_id][pub_name];

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

	// participant(domain) 스코프에서 Topic을 재사용
	std::shared_ptr<ITopicHolder> topic_holder;
	auto& domain_topics = topics_[domain_id];
	auto topic_it = domain_topics.find(topic);
	if (topic_it != domain_topics.end()) {
		topic_holder = topic_it->second;
	} else {
	// 타입 팩토리로 Topic 생성 후 캐시
	topic_holder = topic_factories[type_name](*participant, topic);
		domain_topics[topic] = topic_holder;
	}

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
		try {
			writer_holder = writer_factories[type_name](*publisher, *topic_holder, nullptr);
			LOG_WRN("DDS", "create_writer: fallback to default writer QoS for topic=%s", topic.c_str());
		} catch (const std::exception& ex2) {
			LOG_ERR("DDS", "create_writer: failed to create writer (fallback also failed): %s", ex2.what());
			return DdsResult(false, DdsErrorCategory::Resource, std::string("Writer creation failed: ") + ex2.what());
		}
	}
	DdsManager::HolderId id = next_holder_id_.fetch_add(1);
	writers_[domain_id][pub_name][topic].push_back({id, writer_holder});
	if (out_id) *out_id = static_cast<uint64_t>(id);
	topic_to_type_[domain_id][topic] = type_name;
	LOG_FLOW("writer created id=%llu domain=%d pub=%s topic=%s", static_cast<unsigned long long>(id), domain_id, pub_name.c_str(), topic.c_str());
	return DdsResult(
		true, DdsErrorCategory::None,
		"Writer created successfully: id=" + std::to_string(id) + " domain=" + std::to_string(domain_id) + " pub=" + pub_name + " topic=" + topic);
}

/**
 * @brief Reader 생성: 타입 검증 → 토픽 재사용/생성 → QoS 적용 → ReaderHolder 생성 및 등록
 * @details
 * - mutex_를 획득하여 안전하게 상태를 검사/수정합니다.
 * - IDL 타입 레지스트리에서 type_name 존재를 확인합니다.
 * - topic_to_type_에서 같은 topic의 기존 타입과 충돌이 있는지 검사합니다.
 * - participant/subscriber가 없으면 자동으로 생성합니다.
 * - topics_에서 topic_holder를 재사용하거나 factory로 생성합니다.
 * - QoS를 적용 시도하고 실패 시 기본 QoS로 폴백합니다.
 * - reader_factories[type_name]를 사용해 IReaderHolder를 생성하고 readers_에 등록합니다.
 * - 생성된 Reader에 on_sample_ 콜백을 연결합니다(콜백이 등록된 경우).
 */
DdsResult DdsManager::create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
				const std::string& type_name, const std::string& qos_lib,
				const std::string& qos_profile, uint64_t* out_id)
{
	log_entry("create_reader", std::string("domain_id=") + std::to_string(domain_id) + ", sub_name=" + truncate_for_log(sub_name) + ", topic=" + truncate_for_log(topic) + ", type_name=" + truncate_for_log(type_name) + ", qos_lib=" + truncate_for_log(qos_lib) + ", qos_profile=" + truncate_for_log(qos_profile));
	LOG_INF("DDS", "reader ready domain=%d sub=%s topic=%s type=%s", domain_id, sub_name.c_str(), topic.c_str(), type_name.c_str());

	std::lock_guard<std::mutex> lock(mutex_);

	const auto& reg = idlmeta::type_registry();
	if (reg.find(type_name) == reg.end()) {
		LOG_ERR("DDS", "create_reader: unknown DDS type: %s", type_name.c_str());
		return DdsResult(false, DdsErrorCategory::Logic, "Unknown DDS type: " + type_name);
	}

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

	if (!participants_.count(domain_id)) {
		LOG_ERR("DDS", "create_reader: participant not found for domain=%d (must be created first)", domain_id);
		return DdsResult(false, DdsErrorCategory::Logic,
						"Participant must be created before reader: domain=" + std::to_string(domain_id));
	}

	if (!subscribers_[domain_id].count(sub_name)) {
		DdsResult res = create_subscriber_locked(domain_id, sub_name, qos_lib, qos_profile);
		if (!res.ok) return res;
	}

	auto& participant = participants_[domain_id];
	auto& subscriber = subscribers_[domain_id][sub_name];

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

	std::shared_ptr<ITopicHolder> topic_holder;
	auto& domain_topics_r = topics_[domain_id];
	auto topic_it_r = domain_topics_r.find(topic);
	if (topic_it_r != domain_topics_r.end()) {
		topic_holder = topic_it_r->second;
	} else {
		topic_holder = topic_factories[type_name](*participant, topic);
		domain_topics_r[topic] = topic_holder;
	}

	if (qos_store_) {
		if (auto pack = qos_store_->find_or_reload(qos_lib, qos_profile)) {
			topic_holder->set_qos(pack->topic);
			LOG_INF("DDS", "[apply-qos] topic=%s lib=%s prof=%s %s", topic.c_str(), qos_lib.c_str(), qos_profile.c_str(), summarize_qos(*pack).c_str());
		} else {
			LOG_WRN("DDS", "[apply-qos:default] topic=%s (lib=%s prof=%s not found)", topic.c_str(), qos_lib.c_str(), qos_profile.c_str());
		}
	}

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
	DdsManager::HolderId id = next_holder_id_.fetch_add(1);
	readers_[domain_id][sub_name][topic].push_back({id, reader_holder});
	if (out_id) *out_id = static_cast<uint64_t>(id);

	if (on_sample_) {
		reader_holder->set_sample_callback(on_sample_);
		LOG_DBG("DDS", "listener attached topic=%s", topic.c_str());
	}

	LOG_FLOW("reader created id=%llu domain=%d sub=%s topic=%s", static_cast<unsigned long long>(id), domain_id, sub_name.c_str(), topic.c_str());
	return DdsResult(
		true, DdsErrorCategory::None,
		"Reader created successfully: id=" + std::to_string(id) + " domain=" + std::to_string(domain_id) + " sub=" + sub_name + " topic=" + topic);
}

} // namespace rtpdds
