/**
 * @file dds_manager_io.cpp
 * @brief DdsManager의 publish 및 on_sample 핸들러 구현
 */

#include <any>
#include <nlohmann/json.hpp>

#include "dds_manager.hpp"
#include "dds_manager_internal.hpp"
#include "sample_factory.hpp"
#include "sample_guard.hpp"

using rtpdds::internal::log_entry;
using rtpdds::internal::truncate_for_log;

namespace rtpdds {

/**
 * @brief 주어진 topic의 모든 Writer(도메인/Publisher 전역)에 JSON 샘플을 게시합니다.
 *
 * - JSON 객체를 타입에 맞는 DDS 샘플로 변환하여 write 합니다.
 * - 동일 topic이 여러 Writer에 연결된 경우 모두에게 전송되며, 2개 이상이면 경고 로그를 남깁니다.
 * - 내부 상태 보호를 위해 mutex_로 동기화됩니다.
 *
 * @param topic 게시 대상 DDS Topic 이름
 * @param j     전송할 JSON 객체(반드시 object 타입)
 * @return DdsResult 성공/실패 및 메시지
 *
 * 오류 조건:
 * - j가 object가 아닐 때
 * - topic -> type_name 매핑을 찾을 수 없을 때
 * - 샘플 생성 실패 또는 json_to_dds 변환 실패
 * - WriterHolder가 타입을 지원하지 않을 때(std::bad_any_cast)
 *
 * 상세 동작 및 자료구조 설명:
 * - writers_는 domain_id를 키로 갖는 맵입니다. 각 도메인 아래에는 퍼블리셔 이름을
 *   키로 갖는 맵이 있고, 퍼블리셔 맵 아래에는 topic 이름을 키로 하여 Writer 엔트리
 *   리스트(vector<WriterEntry>)를 보관합니다.
 *   (형태: writers_[domain_id][publisher_name][topic] -> vector<WriterEntry>)
 * - publish_json(topic, j)은 모든 도메인과 퍼블리셔를 순회하며 topic을 찾고,
 *   해당 엔트리들을 대상으로 JSON을 DDS 샘플로 변환한 뒤 각 Writer에 write_any를
 *   호출하여 전송합니다. 같은 topic에 여러 Writer가 있으면 중복 전송이 발생할 수
 *   있으므로 이를 카운트하여 WARN 로그를 남깁니다.
 * - topic_to_type_는 도메인별 topic->type_name 매핑을 보관합니다. Writer에 대한
 *   샘플을 생성하려면 도메인별 타입을 조회해야 합니다.
 * - SampleGuard는 type_name 기반으로 빈 DDS 샘플을 생성하고 RAII로 관리하는
 *   유틸리티입니다. json_to_dds로 JSON을 샘플에 채운 뒤 std::any로 래핑하여
 *   WriterHolder::write_any에 전달합니다. write_any 내부에서 올바른 타입으로 any_cast
 *   하여 실제 DDS write를 수행합니다.
 */
DdsResult DdsManager::publish_json(const std::string& topic, const nlohmann::json& j)
{
	auto jstr = j.dump();
	log_entry("publish_json", std::string("topic=") + truncate_for_log(topic) + ", jsize=" + std::to_string(jstr.size()));
	if (!j.is_object()) {
		LOG_ERR("DDS", "publish_json: payload is not a JSON object for topic=%s", topic.c_str());
		return DdsResult(false, DdsErrorCategory::Logic, "payload must be a JSON object");
	}

	std::lock_guard<std::mutex> lock(mutex_);
	int count = 0;
	for (const auto& dom : writers_) {
		int domain_id = dom.first;
		for (const auto& pub : dom.second) {
			// 각 퍼블리셔 내에서 topic으로 매핑되는 Writer 엔트리를 찾는다.
			auto it = pub.second.find(topic);
			if (it != pub.second.end()) {
				// entries: 해당 topic에 바인딩된 Writer 엔트리 리스트
				const auto& entries = it->second;

				std::string type_name;
				auto dom_type_it = topic_to_type_.find(domain_id);
				if (dom_type_it == topic_to_type_.end()) {
					// 같은 domain에 대해 topic->type 매핑 자체가 없는 경우
					LOG_ERR("DDS", "publish_json: type_name not found for topic=%s in domain=%d", topic.c_str(), domain_id);
					continue;
				}
				auto type_it = dom_type_it->second.find(topic);
				if (type_it == dom_type_it->second.end()) {
					// 해당 topic에 대한 타입 정보 미존재
					LOG_ERR("DDS", "publish_json: type_name not found for topic=%s in domain=%d", topic.c_str(), domain_id);
					continue;
				}
				type_name = type_it->second;
				LOG_DBG("DDS", "publish_json: type_name=%s", type_name.c_str());

				// SampleGuard: type_name 기반 샘플 생성 및 RAII 관리
				SampleGuard sample_guard(type_name);
				if (!sample_guard) {
					LOG_ERR("DDS", "publish_json: failed to create sample for type=%s", type_name.c_str());
					continue; // 샘플 생성 실패하면 해당 Writer 집합은 건너뜀
				}
				LOG_DBG("DDS", "publish_json: sample created for type=%s", type_name.c_str());

				// JSON -> DDS 구조체 변환. 실패 시 해당 엔트리 집합은 건너뜀
				if (!rtpdds::json_to_dds(j, type_name, sample_guard.get())) {
					LOG_ERR("DDS", "publish_json: json_to_dds failed for type=%s", type_name.c_str());
					continue;
				}
				LOG_DBG("DDS", "publish_json: json_to_dds succeeded for type=%s", type_name.c_str());

				// 변환이 성공했다면 Sample을 std::any로 래핑하여 모든 WriterEntry에 전달
				try {
					std::any wrapped_sample = sample_guard.get();
					for (const auto& entry : entries) {
						entry.holder->write_any(wrapped_sample);
					}
				} catch (const std::bad_cast& e) {
					// WriterHolder가 타입을 지원하지 않아 발생하는 예외 처리
					LOG_ERR("DDS", "publish_json: bad_cast exception: %s", e.what());
					LOG_ERR("DDS", "publish_json: WriterHolder may not support type=%s", type_name.c_str());
					continue;
				}

				LOG_FLOW("write ok topic=%s domain=%d pub=%s size=%zu", topic.c_str(), dom.first, pub.first.c_str(), jstr.size());
				// 실제로 데이터를 쓴 Writer 엔트리 수를 누적(중복 전송을 카운트)
				count += static_cast<int>(entries.size());
			}
		}
	}
	if (count == 0) {
		LOG_ERR("DDS", "publish_json: topic=%s writer not found or invalid type/sample", topic.c_str());
		return DdsResult(false, DdsErrorCategory::Logic, "Writer not found or invalid type/sample for topic: " + topic);
	}
	if (count > 1) {
		// 동일 topic으로 다수 Writer에 전송된 경우: 중복 전송 가능성을 알리는 경고
		LOG_WRN("DDS", "publish_json: topic=%s published to %d writers (duplicate transmission warning)", topic.c_str(), count);
	}
	return DdsResult(true, DdsErrorCategory::None,
					 "Publish succeeded: topic=" + topic + " count=" + std::to_string(count));
}

/**
 * @brief 지정된 도메인/Publisher 내 특정 topic Writer들에 JSON 샘플을 게시합니다.
 *
 * - 도메인과 Publisher를 한정하여 대상 범위를 좁힙니다.
 * - 타입 이름 조회, 샘플 생성, json_to_dds 변환 후 write를 수행합니다.
 * - 내부 상태 보호를 위해 mutex_로 동기화됩니다.
 *
 * @param domain_id 대상 Domain ID
 * @param pub_name  대상 Publisher 이름
 * @param topic     게시 대상 DDS Topic 이름
 * @param j         전송할 JSON 객체(반드시 object 타입)
 * @return DdsResult 성공/실패 및 메시지
 *
 * 실패 조건:
 * - 도메인/Publisher/Topic이 존재하지 않는 경우
 * - type_name 매핑이 없거나 샘플 생성 또는 변환 실패
 * - WriterHolder 타입 불일치(bad_any_cast)
 *
 * 상세 동작(도메인/퍼블리셔 한정):
 * - writers_[domain_id][pub_name][topic]로 특정 퍼블리셔 영역의 Writer 엔트리를 찾습니다.
 * - topic_to_type_[domain_id]에서 타입 이름을 조회하여 Sample을 생성/변환한 뒤
 *   해당 퍼블리셔의 모든 Writer에 대해 write_any를 호출합니다.
 */
DdsResult DdsManager::publish_json(int domain_id, const std::string& pub_name, const std::string& topic,
								   const nlohmann::json& j)
{
	auto jstr = j.dump();
	log_entry("publish_json(domain)", std::string("domain_id=") + std::to_string(domain_id) + ", pub_name=" + truncate_for_log(pub_name) + ", topic=" + truncate_for_log(topic) + ", jsize=" + std::to_string(jstr.size()));
	if (!j.is_object()) {
		LOG_ERR("DDS", "publish_json: payload is not a JSON object for topic=%s domain=%d", topic.c_str(), domain_id);
		return DdsResult(false, DdsErrorCategory::Logic, "payload must be a JSON object");
	}

	std::lock_guard<std::mutex> lock(mutex_);
	// domain/publisher/topic을 특정하여 게시하는 경로 (mutex_ 보호 하에 동작)
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

	SampleGuard sample_guard(type_name);
	if (!sample_guard) {
		LOG_ERR("DDS", "publish_json: failed to create sample for type=%s", type_name.c_str());
		return DdsResult(false, DdsErrorCategory::Logic, "failed to create sample for type: " + type_name);
	}

	if (!rtpdds::json_to_dds(j, type_name, sample_guard.get())) {
		LOG_ERR("DDS", "publish_json: json_to_dds failed for type=%s", type_name.c_str());
		return DdsResult(false, DdsErrorCategory::Logic, "json_to_dds failed for type: " + type_name);
	}

	try {
		std::any wrapped_sample = sample_guard.get();
		for (const auto& entry : entries) {
			entry.holder->write_any(wrapped_sample);
		}
	} catch (const std::bad_cast& e) {
		LOG_ERR("DDS", "publish_json: bad_cast exception: %s", e.what());
		LOG_ERR("DDS", "publish_json: WriterHolder may not support type=%s", type_name.c_str());
		return DdsResult(false, DdsErrorCategory::Logic, "WriterHolder type mismatch");
	}

	LOG_FLOW("write ok domain=%d pub=%s topic=%s size=%zu", domain_id, pub_name.c_str(), topic.c_str(), jstr.size());
	return DdsResult(true, DdsErrorCategory::None,
					 "Publish succeeded: domain=" + std::to_string(domain_id) + " pub=" + pub_name + " topic=" + topic);
}

/**
 * @brief Reader 샘플 수신 콜백을 등록합니다.
 *
 * - 이후 Reader에서 샘플이 도착하면 등록된 콜백이 호출됩니다.
 * - 기존 콜백은 대체됩니다.
 * - 내부 상태 보호를 위해 mutex_로 동기화됩니다.
 *
 * @details
 * - on_sample_는 Reader에서 수신된 샘플을 상위로 전달하는 콜백입니다.
 * - 콜백을 교체할 때는 mutex_로 보호하여 동시성 문제를 방지합니다.
 * - 콜백은 move로 소유권을 이전하여 불필요한 복사를 피합니다.
 *
 * @param cb 샘플 처리 콜백 핸들러
 */
void DdsManager::set_on_sample(SampleHandler cb)
{
	// 내부 설명: on_sample_ 콜백 설정
	// - on_sample_는 Reader에서 샘플이 도착했을 때 호출되는 함수 객체입니다.
	// - 외부에서 콜백을 교체할 때 동시성 문제를 피하기 위해 mutex_로 보호합니다.
	// - 콜백은 move로 소유권을 이전하여 불필요한 복사를 방지합니다.
	std::lock_guard<std::mutex> lock(mutex_);
	on_sample_ = std::move(cb);
	LOG_DBG("DDS", "on_sample handler installed");
}

} // namespace rtpdds
