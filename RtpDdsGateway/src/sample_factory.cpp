/**
 * @file sample_factory.cpp
 * @brief 샘플 변환/생성(AnyData <-> JSON) 및 토픽별 팩토리/변환 테이블 초기화 구현
 *
 * 신규 토픽 타입 추가 시 sample_factories/sample_to_json에 변환 함수를 등록하여 확장할 수 있음.
 * 각 함수별로 파라미터/리턴/비즈니스 처리에 대한 상세 주석을 추가.
 */
#include "sample_factory.hpp"

#include <nlohmann/json.hpp>

#include "idl_type_registry.hpp"
#include "idl_json_bind.hpp"
#include "triad_log.hpp"


namespace rtpdds
{
/**
 * @brief 타입명에 해당하는 샘플을 동적으로 생성한다.
 * @details
 * - idlmeta::type_registry()에서 등록된 팩토리를 사용해 샘플을 생성합니다.
 * - 생성 실패 시 nullptr을 반환하며 호출자는 반환값을 검사해야 합니다.
 * - 주로 publish를 위한 빈 샘플 생성이나 테스트용 인스턴스 생성에 사용됩니다.
 */
void* create_sample(const std::string& type_name) {
    const auto& reg = idlmeta::type_registry();
    auto it = reg.find(type_name);
    if (it == reg.end()) {
        LOG_WRN("SampleFactory", "create_sample: no factory registered for type=%s", type_name.c_str());
        return nullptr;
    }
    void* sample = it->second.create();
    if (!sample) {
        LOG_ERR("SampleFactory", "create_sample: failed to create sample for type=%s", type_name.c_str());
    }
    return sample;
}

/**
 * @brief create_sample로 생성한 샘플을 안전하게 해제한다.
 * @details
 * - 등록된 팩토리의 destroy 함수를 호출하여 메모리를 반환합니다.
 * - null 포인터나 등록되지 않은 타입에 대해서는 적절한 로그를 남기고 조용히 반환합니다.
 */
void destroy_sample(const std::string& type_name, void* p) {
    const auto& reg = idlmeta::type_registry();
    auto it = reg.find(type_name);
    if (it == reg.end()) {
        LOG_WRN("SampleFactory", "destroy_sample: no factory registered for type=%s", type_name.c_str());
        return;
    }
    if (!p) {
        LOG_WRN("SampleFactory", "destroy_sample: null pointer provided for type=%s", type_name.c_str());
        return;
    }
    it->second.destroy(p);
}

/**
 * @brief JSON 객체를 해당 DDS 샘플로 변환한다.
 * @details
 * - idlmeta::json_registry()에 등록된 변환 함수를 사용합니다.
 * - 변환 중 에러가 발생하면 idlmeta::last_json_error()에서 상세 사유를 얻을 수 있습니다.
 * - 호출자는 반환값(false)을 수신하면 변환 실패에 대해 응답/재시도를 수행해야 합니다.
 */
bool json_to_dds(const nlohmann::json& j, const std::string& type_name, void* sample) {
    LOG_DBG("SampleFactory", "json_to_dds: converting JSON to DDS for type=%s", type_name.c_str());
    const auto& jr = idlmeta::json_registry();
    auto it = jr.find(type_name);
    if (it == jr.end()) {
        LOG_WRN("SampleFactory", "json_to_dds: no JSON registry entry for type=%s", type_name.c_str());
        return false;
    }
    if (!sample) {
        LOG_WRN("SampleFactory", "json_to_dds: null sample pointer provided for type=%s", type_name.c_str());
        return false;
    }
    // clear any previous error, then attempt conversion
    idlmeta::clear_json_error();
    bool result = it->second.from_json(j, sample);
    if (result) {
        LOG_DBG("SampleFactory", "json_to_dds: JSON converted successfully to DDS for type=%s", type_name.c_str());
    } else {
        const std::string& err = idlmeta::last_json_error();
        if (!err.empty()) {
            LOG_WRN("SampleFactory", "json_to_dds: failed to convert JSON to DDS for type=%s; reason=%s", type_name.c_str(), err.c_str());
        } else {
            LOG_WRN("SampleFactory", "json_to_dds: failed to convert JSON to DDS for type=%s; reason=unknown (type/format mismatch)", type_name.c_str());
        }
    }
    return result;
}

/**
 * @brief DDS 샘플을 JSON 객체로 직렬화합니다.
 * @details
 * - idlmeta::json_registry()에 등록된 to_json 함수를 사용합니다.
 * - 성공 시 out에 변환 결과가 채워지며 true를 반환합니다.
 * - 실패 시 false를 반환하고 호출자는 실패 원인을 로그/에러 처리해야 합니다.
 */
bool dds_to_json(const std::string& type_name, const void* sample, nlohmann::json& out) {
    LOG_DBG("SampleFactory", "dds_to_json: converting DDS to JSON for type=%s", type_name.c_str());
    const auto& jr = idlmeta::json_registry();
    auto it = jr.find(type_name);
    if (it == jr.end()) {
        LOG_WRN("SampleFactory", "dds_to_json: no JSON registry entry for type=%s", type_name.c_str());
        return false;
    }
    if (!sample) {
        LOG_WRN("SampleFactory", "dds_to_json: null sample pointer provided for type=%s", type_name.c_str());
        return false;
    }
    bool result = it->second.to_json(sample, out);
    if (result) {
        LOG_DBG("SampleFactory", "dds_to_json: DDS converted successfully to JSON for type=%s", type_name.c_str());
    } else {
        LOG_WRN("SampleFactory", "dds_to_json: failed to convert DDS to JSON for type=%s", type_name.c_str());
    }
    return result;
}
}  // namespace rtpdds
