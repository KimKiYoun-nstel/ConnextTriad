#pragma once
/**
 * @file dds_manager_adapter.hpp
 * @brief DdsManager의 기존 구현체를 IDdsManager 인터페이스로 어댑트하는 래퍼
 *
 * 목적: 기존 DdsManager 구현을 변경하지 않고 IDdsManager 인터페이스를 제공하여
 *       의존성 역전을 가능하게 합니다.
 */

#include "rtpdds/api/idds_manager.hpp"

namespace rtpdds {

class DdsManager; // forward

class DdsManagerAdapter : public IDdsManager {
public:
    explicit DdsManagerAdapter(DdsManager& mgr) noexcept;
    ~DdsManagerAdapter() override;

    DdsResult create_participant(int domain_id, const std::string& qos_lib, const std::string& qos_profile) override;
    DdsResult create_publisher(int domain_id, const std::string& pub_name, const std::string& qos_lib,
                               const std::string& qos_profile) override;
    DdsResult create_subscriber(int domain_id, const std::string& sub_name, const std::string& qos_lib,
                                const std::string& qos_profile) override;

    DdsResult create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
                            const std::string& type_name, const std::string& qos_lib,
                            const std::string& qos_profile, uint64_t* out_id = nullptr) override;

    DdsResult create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
                            const std::string& type_name, const std::string& qos_lib,
                            const std::string& qos_profile, uint64_t* out_id = nullptr) override;

    nlohmann::json list_qos_profiles(bool include_builtin = false, bool include_detail = false) const override;
    std::string add_or_update_qos_profile(const std::string& library, const std::string& profile,
                                          const std::string& profile_xml) override;

    DdsResult publish_json(const std::string& topic, const nlohmann::json& j) override;
    DdsResult publish_json(int domain_id, const std::string& pub_name, const std::string& topic,
                           const nlohmann::json& j) override;

    void set_on_sample(SampleCallback cb) override;
    std::string get_type_for_topic(const std::string& topic) const override;
    void clear_entities() override;

private:
    DdsManager& mgr_;
};

} // namespace rtpdds
