/**
 * @file dds_manager_cleanup.cpp
 * @brief DdsManager 엔티티 정리/제거 구현 (정상화된 단일 버전)
 *
 * 이 파일은 다음 기능을 포함합니다:
 * - clear_entities(): 모든 DDS 엔티티를 계층적으로 정리
 * - remove_writer(id): 특정 writer holder(id)를 제거하고 관련 맵을 정리
 * - remove_reader(id): 특정 reader holder(id)를 제거하고 관련 맵을 정리
 */

#include <algorithm>
#include <mutex>

#include "dds_manager.hpp"
#include "triad_log.hpp"

namespace rtpdds {

void DdsManager::clear_entities()
{
	std::lock_guard<std::mutex> lock(mutex_);
	// 하위 엔티티부터 상위 엔티티 순으로 컨테이너를 비워 리소스를 해제
	readers_.clear();
	writers_.clear();
	topics_.clear();
	topic_to_type_.clear();
	subscribers_.clear();
	publishers_.clear();
	participants_.clear();

	LOG_FLOW("clear_entities completed in correct hierarchical order");
}

/**
 * @brief Writer 식별자(id)에 해당하는 엔트리를 제거합니다.
 *
 * - 비워진 topic/publisher 맵을 정리합니다.
 * - 동일 topic을 사용하는 Reader/Writer가 더 이상 없으면 topic_to_type_ 매핑도 제거합니다.
 * - 내부 상태 보호를 위해 mutex_로 동기화됩니다.
 *
 * @param id Writer 고유 식별자
 * @return DdsResult 성공/실패 및 메시지
 */
DdsResult DdsManager::remove_writer(uint64_t id)
{
	using std::string;
	std::lock_guard<std::mutex> lock(mutex_);

	for (auto domIt = writers_.begin(); domIt != writers_.end(); ++domIt) {
		int domain_id = domIt->first;
		auto &pubmap = domIt->second;
		for (auto pubIt = pubmap.begin(); pubIt != pubmap.end(); ++pubIt) {
			auto &topic_map = pubIt->second;
			for (auto topicIt = topic_map.begin(); topicIt != topic_map.end(); ++topicIt) {
				const string topic_name = topicIt->first; // erase 안전성을 위해 이름을 복사
				auto &vec = topicIt->second;
				auto it = std::find_if(vec.begin(), vec.end(), [id](const WriterEntry &e) { return e.id == id; });
				if (it != vec.end()) {
					// writer 제거
					vec.erase(it);
					LOG_FLOW("removed writer id=%llu domain=%d pub=%s topic=%s",
						static_cast<unsigned long long>(id), domain_id, pubIt->first.c_str(), topic_name.c_str());

					// 비어있는 topic 벡터/맵 제거
					if (vec.empty()) {
						topic_map.erase(topicIt);
					}
					if (topic_map.empty()) {
						pubmap.erase(pubIt);
					}

					// 동일 도메인 내 다른 writers/readers에 해당 topic이 남아있는지 검사
					bool has_any = false;
					auto wdomIt = writers_.find(domain_id);
					if (wdomIt != writers_.end()) {
						for (const auto &p : wdomIt->second) {
							if (p.second.count(topic_name)) { has_any = true; break; }
						}
					}
					if (!has_any) {
						auto rdomIt = readers_.find(domain_id);
						if (rdomIt != readers_.end()) {
							for (const auto &p : rdomIt->second) {
								if (p.second.count(topic_name)) { has_any = true; break; }
							}
						}
					}

					if (!has_any) {
						auto dom_type_it = topic_to_type_.find(domain_id);
						if (dom_type_it != topic_to_type_.end()) {
							dom_type_it->second.erase(topic_name);
						}
					}

					return DdsResult(true, DdsErrorCategory::None, "Writer removed: id=" + std::to_string(id));
				}
			}
		}
	}
	return DdsResult(false, DdsErrorCategory::Logic, "Writer id not found: " + std::to_string(id));
}

/**
 * @brief Reader 식별자(id)에 해당하는 엔트리를 제거합니다.
 *
 * - 비워진 topic/subscriber 맵을 정리합니다.
 * - topic_to_type_ 정리는 Writer 제거 경로에서 수행됩니다(의도된 비대칭).
 * - 내부 상태 보호를 위해 mutex_로 동기화됩니다.
 *
 * @param id Reader 고유 식별자
 * @return DdsResult 성공/실패 및 메시지
 */
DdsResult DdsManager::remove_reader(uint64_t id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	for (auto domIt = readers_.begin(); domIt != readers_.end(); ++domIt) {
		int domain_id = domIt->first;
		auto &submap = domIt->second;
		for (auto subIt = submap.begin(); subIt != submap.end(); ++subIt) {
			auto &topic_map = subIt->second;
			for (auto topicIt = topic_map.begin(); topicIt != topic_map.end(); ++topicIt) {
				auto &vec = topicIt->second;
				auto it = std::find_if(vec.begin(), vec.end(), [id](const ReaderEntry &e) { return e.id == id; });
				if (it != vec.end()) {
					vec.erase(it);
					LOG_FLOW("removed reader id=%llu domain=%d sub=%s topic=%s",
						static_cast<unsigned long long>(id), domain_id, subIt->first.c_str(), topicIt->first.c_str());

					if (vec.empty()) {
						topic_map.erase(topicIt);
					}
					if (topic_map.empty()) {
						submap.erase(subIt);
					}
					return DdsResult(true, DdsErrorCategory::None, "Reader removed: id=" + std::to_string(id));
				}
			}
		}
	}
	return DdsResult(false, DdsErrorCategory::Logic, "Reader id not found: " + std::to_string(id));
}

} // namespace rtpdds