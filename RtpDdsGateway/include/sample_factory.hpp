
#pragma once
/**
 * @file sample_factory.hpp
 * @brief 다양한 토픽 타입(AlarmMsg, StringMsg 등)에 대해 AnyData ↔ JSON 변환, 샘플 동적 생성/변환 담당
 *
 * 신규 토픽 타입 추가 시 factory/convert 함수를 등록하여 확장할 수 있습니다.
 * UI/IPC/테스트 등에서 JSON 기반 샘플 생성 및 변환이 필요할 때 활용합니다.
 *
 * 연관 파일:
 *   - dds_manager.hpp (샘플 publish/take)
 *   - dds_type_registry.hpp (엔티티/타입 레지스트리)
 */
#include <any>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace rtpdds {

/**
 * @brief 타입명으로 샘플 객체를 동적으로 생성합니다.
 * @param type_name DDS 타입명
 * @return 생성된 샘플 포인터(소유권 호출자), 실패 시 nullptr
 * @note 반환 포인터는 반드시 destroy_sample로 해제해야 합니다.
 */
void* create_sample(const std::string& type_name);

/**
 * @brief create_sample로 생성한 샘플을 해제합니다.
 * @param type_name DDS 타입명(해제 시 검증/추가 로직에 사용될 수 있음)
 * @param p 해제할 샘플 포인터
 */
void  destroy_sample(const std::string& type_name, void* p);

/**
 * @brief JSON 객체를 DDS 샘플로 변환합니다.
 * @param j 입력 JSON(객체형)
 * @param type_name DDS 타입명
 * @param sample 변환 대상 샘플 포인터(필수 필드 채움)
 * @return 성공 여부(false면 필드 매핑 실패 등)
 * @pre sample은 해당 타입의 유효한 포인터여야 합니다.
 */
bool  json_to_dds(const nlohmann::json& j,
                  const std::string& type_name,
                  void* sample);

/**
 * @brief DDS 샘플을 JSON으로 변환합니다.
 * @param type_name DDS 타입명
 * @param sample 입력 샘플 포인터
 * @param out 출력 JSON 객체
 * @return 성공 여부(false면 필드 매핑 실패 등)
 */
bool  dds_to_json(const std::string& type_name,
                  const void* sample,
                  nlohmann::json& out);
}
