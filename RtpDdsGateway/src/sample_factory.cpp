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
void* create_sample(const std::string& type_name) {
    LOG_DBG("SampleFactory", "create_sample: creating sample for type=%s", type_name.c_str());
    const auto& reg = idlmeta::type_registry();
    auto it = reg.find(type_name);
    if (it == reg.end()) {
        LOG_ERR("SampleFactory", "create_sample: no factory registered for type=%s", type_name.c_str());
        return nullptr;
    }
    void* sample = it->second.create();
    if (!sample) {
        LOG_ERR("SampleFactory", "create_sample: failed to create sample for type=%s", type_name.c_str());
    } else {
        LOG_DBG("SampleFactory", "create_sample: sample created successfully for type=%s, sample_ptr=%p", type_name.c_str(), sample);
    }
    return sample;
}

void destroy_sample(const std::string& type_name, void* p) {
    LOG_DBG("SampleFactory", "destroy_sample: destroying sample for type=%s, sample_ptr=%p", type_name.c_str(), p);
    const auto& reg = idlmeta::type_registry();
    auto it = reg.find(type_name);
    if (it == reg.end()) {
        LOG_ERR("SampleFactory", "destroy_sample: no factory registered for type=%s", type_name.c_str());
        return;
    }
    if (!p) {
        LOG_WRN("SampleFactory", "destroy_sample: null pointer provided for type=%s", type_name.c_str());
        return;
    }
    it->second.destroy(p);
    LOG_DBG("SampleFactory", "destroy_sample: sample destroyed for type=%s", type_name.c_str());
}

bool json_to_dds(const nlohmann::json& j, const std::string& type_name, void* sample) {
    LOG_DBG("SampleFactory", "json_to_dds: converting JSON to DDS for type=%s", type_name.c_str());
    const auto& jr = idlmeta::json_registry();
    auto it = jr.find(type_name);
    if (it == jr.end()) {
        LOG_ERR("SampleFactory", "json_to_dds: no JSON registry entry for type=%s", type_name.c_str());
        return false;
    }
    if (!sample) {
        LOG_ERR("SampleFactory", "json_to_dds: null sample pointer provided for type=%s", type_name.c_str());
        return false;
    }
    bool result = it->second.from_json(j, sample);
    if (result) {
        LOG_DBG("SampleFactory", "json_to_dds: JSON converted successfully to DDS for type=%s", type_name.c_str());
    } else {
        LOG_ERR("SampleFactory", "json_to_dds: failed to convert JSON to DDS for type=%s", type_name.c_str());
    }
    return result;
}

bool dds_to_json(const std::string& type_name, const void* sample, nlohmann::json& out) {
    LOG_DBG("SampleFactory", "dds_to_json: converting DDS to JSON for type=%s", type_name.c_str());
    const auto& jr = idlmeta::json_registry();
    auto it = jr.find(type_name);
    if (it == jr.end()) {
        LOG_ERR("SampleFactory", "dds_to_json: no JSON registry entry for type=%s", type_name.c_str());
        return false;
    }
    if (!sample) {
        LOG_ERR("SampleFactory", "dds_to_json: null sample pointer provided for type=%s", type_name.c_str());
        return false;
    }
    bool result = it->second.to_json(sample, out);
    if (result) {
        LOG_DBG("SampleFactory", "dds_to_json: DDS converted successfully to JSON for type=%s", type_name.c_str());
    } else {
        LOG_ERR("SampleFactory", "dds_to_json: failed to convert DDS to JSON for type=%s", type_name.c_str());
    }
    return result;
}
}  // namespace rtpdds
