
#pragma once
#include <any>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

// 코드젠 헤더 (토픽/타입 선언 용)
#include "StringMsg.hpp"
#include "AlarmMsg.hpp"
#include "Alarms_PSM.hpp"

namespace rtpdds {
	// 타입 소거된 ‘샘플(데이터 1건)’
	using AnyData = std::any;

	// payload(텍스트/JSON 등) → AnyData 생성
	using SampleFactory = std::function<AnyData(const std::string& payload)>;
	extern std::unordered_map<std::string, SampleFactory> sample_factories;
	void init_sample_factories();

	// AnyData → JSON 변환
	using SampleToJson = std::function<nlohmann::json(const AnyData&)>;
	extern std::unordered_map<std::string, SampleToJson> sample_to_json;
	void init_sample_to_json();
} // namespace rtpdds
