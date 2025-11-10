/**
 * @file dds_manager_qos.cpp
 * @brief DdsManager의 QoS 요약/목록/업데이트 구현
 */

#include <nlohmann/json.hpp>
#include <mutex>

#include "dds_manager.hpp"
#include "dds_manager_internal.hpp"

using rtpdds::internal::log_entry;

namespace rtpdds {

/**
 * @brief QosPack 정보를 간단히 요약한 문자열로 반환합니다.
 *
 * - 원본 파일 경로가 존재하면 그 정보를 포함합니다.
 * - 비어있으면 (qos:default)로 표시합니다.
 *
 * @param p QosStore에서 관리하는 QoS 묶음 정보
 * @return std::string 요약 문자열
 */
std::string DdsManager::summarize_qos(const rtpdds::QosPack& p)
{
	if (p.origin_file.empty()) return std::string("(qos:default)");
	return std::string("(qos from=") + p.origin_file + ")";
}

/**
 * @brief 사용 가능한 QoS 프로파일 목록을 JSON으로 반환합니다.
 *
 * - result 배열에 프로파일 식별 정보를 담습니다.
 * - include_detail이 true면 detail 배열에 상세 내용을 포함합니다.
 * - qos_store_가 초기화되지 않은 경우 비어있는 배열을 반환합니다.
 * - 내부 상태 보호를 위해 mutex_로 동기화됩니다.
 *
 * @param include_builtin 내장(builtin) 프로파일 포함 여부
 * @param include_detail  상세 정보 포함 여부
 * @return nlohmann::json { result: [...], detail?: [...] }
 */
nlohmann::json DdsManager::list_qos_profiles(bool include_builtin, bool include_detail) const
{
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

/**
 * @brief QoS 라이브러리/프로파일을 추가하거나 기존 프로파일 XML을 업데이트합니다.
 *
 * - qos_store_가 초기화되어 있어야 합니다.
 * - 내부 상태 보호를 위해 mutex_로 동기화됩니다.
 *
 * @param library      QoS 라이브러리 이름
 * @param profile      QoS 프로파일 이름
 * @param profile_xml  QoS XML 문자열(단일 프로파일 조각)
 * @return std::string 저장/업데이트 결과 식별자(내부 정책에 따름), 실패 시 빈 문자열
 */
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

} // namespace rtpdds
