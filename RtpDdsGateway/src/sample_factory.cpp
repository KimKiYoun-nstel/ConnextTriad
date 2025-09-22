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
    const auto& reg = idlmeta::type_registry();
    auto it = reg.find(type_name);
    if (it == reg.end()) return nullptr;
    return it->second.create();
}

void destroy_sample(const std::string& type_name, void* p) {
    const auto& reg = idlmeta::type_registry();
    auto it = reg.find(type_name);
    if (it == reg.end() || !p) return;
    it->second.destroy(p);
}

bool json_to_dds(const nlohmann::json& j, const std::string& type_name, void* sample) {
    const auto& jr = idlmeta::json_registry();
    auto it = jr.find(type_name);
    if (it == jr.end() || !sample) return false;
    return it->second.from_json(j, sample);
}

bool dds_to_json(const std::string& type_name, const void* sample, nlohmann::json& out) {
    const auto& jr = idlmeta::json_registry();
    auto it = jr.find(type_name);
    if (it == jr.end() || !sample) return false;
    return it->second.to_json(sample, out);
}
}  // namespace rtpdds
