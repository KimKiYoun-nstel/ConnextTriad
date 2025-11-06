
#pragma once
/**
 * @file sample_factory.hpp
 * @brief 다양한 토픽 타입(AlarmMsg, StringMsg 등)에 대해 AnyData ↔ JSON 변환, 샘플 동적 생성/변환 담당
 *
 * 신규 토픽 타입 추가 시 sample_factories/sample_to_json에 변환 함수를 등록하여 확장할 수 있습니다.
 * UI/IPC/테스트 등에서 텍스트/JSON 기반 샘플 생성 및 변환이 필요할 때 활용합니다.
 *
 * 연관 파일:
 *   - dds_manager.hpp (샘플 publish/take)
 *   - dds_util.hpp (타입 변환 유틸리티)
 *   - Alarms_PSM_enum_utils.hpp (enum 변환)
 */
#include <any>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace rtpdds {
	void* create_sample(const std::string& type_name);
	void  destroy_sample(const std::string& type_name, void* p);
	bool  json_to_dds(const nlohmann::json& j,
							const std::string& type_name,
							void* sample);
	bool  dds_to_json(const std::string& type_name,
							const void* sample,
							nlohmann::json& out);
}
