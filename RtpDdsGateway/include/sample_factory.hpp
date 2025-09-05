
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
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

// 코드젠 헤더 (토픽/타입 선언 용)
#include "StringMsg.hpp"
#include "AlarmMsg.hpp"
#include "Alarms_PSM.hpp"

namespace rtpdds {
	/**
	* @brief 타입 소거된 ‘샘플(데이터 1건)’
	* AnyData로 관리하여 런타임에 다양한 타입을 동적으로 처리
	*/
	using AnyData = std::any;

	/**
	* @brief payload(텍스트/JSON 등) → AnyData 생성 함수 타입
	* @param payload 텍스트/JSON 등 입력
	* @return AnyData(실제 샘플)
	*/
	using SampleFactory = std::function<AnyData(const std::string& payload)>;
	/**
	* @brief 토픽명 → 샘플 생성 함수 매핑 테이블
	* 신규 타입 추가 시 여기에 등록
	*/
	extern std::unordered_map<std::string, SampleFactory> sample_factories;
	/**
	* @brief 샘플 생성 함수 테이블 초기화
	*/
	void init_sample_factories();

	/**
	* @brief AnyData → JSON 변환 함수 타입
	* @param AnyData(실제 샘플)
	* @return nlohmann::json 변환 결과
	*/
	using SampleToJson = std::function<nlohmann::json(const AnyData&)>;
	/**
	* @brief 토픽명 → 샘플→JSON 변환 함수 매핑 테이블
	* 신규 타입 추가 시 여기에 등록
	*/
	extern std::unordered_map<std::string, SampleToJson> sample_to_json;
	/**
	* @brief 샘플→JSON 변환 함수 테이블 초기화
	*/
	void init_sample_to_json();
} // namespace rtpdds
