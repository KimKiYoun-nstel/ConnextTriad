// 경량 DDS 매니저 인터페이스
// 퍼블릭 헤더에는 heavy dependency를 두지 않도록 설계합니다.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <cstdint>
#include "dds_type_registry.hpp"
#include <nlohmann/json.hpp>

namespace rtpdds {

// 단순한 결과 타입: 헤더에 복잡한 enum 포함을 피하기 위해 정수 카테고리 사용
/// 오류 분류: None/Resource/Logic
enum class DdsErrorCategory : int { None = 0, Resource = 1, Logic = 2 };

struct DdsResult {
    bool ok{true};
    DdsErrorCategory category{DdsErrorCategory::None};
    std::string reason;
    DdsResult() = default;
    DdsResult(bool o, DdsErrorCategory c, std::string r) : ok(o), category(c), reason(std::move(r)) {}
};

// 샘플 콜백: topic, type_name, opaque data pointer (shared ownership)
// 이름 충돌 방지를 위해 로컬 인터페이스용 타입 별칭을 사용합니다.
// Reuse the project's AnyData/SampleCallback definitions to remain compatible with existing code.
using SampleCallback = rtpdds::SampleCallback; // from dds_type_registry.hpp

/**
 * @brief IDdsManager: DdsManager의 최소한의 (경량) 퍼블릭 인터페이스
 *
 * 목적: heavy header(nlohmann::json, dds/dds.hpp 등)를 퍼블릭 API에서 제거하도록
 *       하는 것. 기존 DdsManager는 향후 이 인터페이스를 구현하도록 변경할 수 있음.
 */
class IDdsManager {
public:
    virtual ~IDdsManager() = default;

    virtual DdsResult create_participant(int domain_id, const std::string& qos_lib, const std::string& qos_profile) = 0;
    virtual DdsResult create_publisher(int domain_id, const std::string& pub_name, const std::string& qos_lib,
                                       const std::string& qos_profile) = 0;
    virtual DdsResult create_subscriber(int domain_id, const std::string& sub_name, const std::string& qos_lib,
                                        const std::string& qos_profile) = 0;

    virtual DdsResult create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
                                    const std::string& type_name, const std::string& qos_lib,
                                    const std::string& qos_profile, uint64_t* out_id = nullptr) = 0;

    virtual DdsResult create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
                                    const std::string& type_name, const std::string& qos_lib,
                                    const std::string& qos_profile, uint64_t* out_id = nullptr) = 0;

    virtual nlohmann::json list_qos_profiles(bool include_builtin = false, bool include_detail = false) const = 0;
    virtual std::string add_or_update_qos_profile(const std::string& library, const std::string& profile,
                                                  const std::string& profile_xml) = 0;

    virtual DdsResult publish_json(const std::string& topic, const nlohmann::json& j) = 0;
    virtual DdsResult publish_json(int domain_id, const std::string& pub_name, const std::string& topic,
                                   const nlohmann::json& j) = 0;

    virtual void set_on_sample(SampleCallback cb) = 0;

    virtual std::string get_type_for_topic(const std::string& topic) const = 0;
    virtual void clear_entities() = 0;
};

} // namespace rtpdds

