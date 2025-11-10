#include "rtpdds/dds_manager_adapter.hpp"
#include "dds_manager.hpp"
#include <nlohmann/json.hpp>

namespace rtpdds {

DdsManagerAdapter::DdsManagerAdapter(DdsManager& mgr) noexcept : mgr_(mgr) {}

DdsManagerAdapter::~DdsManagerAdapter() = default;

DdsResult DdsManagerAdapter::create_participant(int domain_id, const std::string& qos_lib, const std::string& qos_profile)
{
    return mgr_.create_participant(domain_id, qos_lib, qos_profile);
}

DdsResult DdsManagerAdapter::create_publisher(int domain_id, const std::string& pub_name, const std::string& qos_lib,
                                              const std::string& qos_profile)
{
    return mgr_.create_publisher(domain_id, pub_name, qos_lib, qos_profile);
}

DdsResult DdsManagerAdapter::create_subscriber(int domain_id, const std::string& sub_name, const std::string& qos_lib,
                                               const std::string& qos_profile)
{
    return mgr_.create_subscriber(domain_id, sub_name, qos_lib, qos_profile);
}

DdsResult DdsManagerAdapter::create_writer(int domain_id, const std::string& pub_name, const std::string& topic,
                                           const std::string& type_name, const std::string& qos_lib,
                                           const std::string& qos_profile, uint64_t* out_id)
{
    return mgr_.create_writer(domain_id, pub_name, topic, type_name, qos_lib, qos_profile, out_id);
}

DdsResult DdsManagerAdapter::create_reader(int domain_id, const std::string& sub_name, const std::string& topic,
                                           const std::string& type_name, const std::string& qos_lib,
                                           const std::string& qos_profile, uint64_t* out_id)
{
    return mgr_.create_reader(domain_id, sub_name, topic, type_name, qos_lib, qos_profile, out_id);
}

nlohmann::json DdsManagerAdapter::list_qos_profiles(bool include_builtin, bool include_detail) const
{
    return mgr_.list_qos_profiles(include_builtin, include_detail);
}

std::string DdsManagerAdapter::add_or_update_qos_profile(const std::string& library, const std::string& profile,
                                                        const std::string& profile_xml)
{
    return mgr_.add_or_update_qos_profile(library, profile, profile_xml);
}

DdsResult DdsManagerAdapter::publish_json(const std::string& topic, const nlohmann::json& j)
{
    return mgr_.publish_json(topic, j);
}

DdsResult DdsManagerAdapter::publish_json(int domain_id, const std::string& pub_name, const std::string& topic,
                                          const nlohmann::json& j)
{
    return mgr_.publish_json(domain_id, pub_name, topic, j);
}

void DdsManagerAdapter::set_on_sample(SampleCallback cb)
{
    // DdsManager expects SampleHandler (from dds_type_registry). Adapter needs to adapt types.
    mgr_.set_on_sample([cb](const std::string& topic, const std::string& type_name, const AnyData& data){
        // direct pass-through if AnyData is stored as shared_ptr<void>
        try {
            auto sp = std::any_cast<std::shared_ptr<void>>(data);
            cb(topic, type_name, data);
        } catch (...) {
            // fallback: create AnyData wrapper if cb expects AnyData type signature
            // cb signature (from dds_type_registry) expects AnyData (std::any), so pass as-is
            cb(topic, type_name, data);
        }
    });
}

std::string DdsManagerAdapter::get_type_for_topic(const std::string& topic) const
{
    return mgr_.get_type_for_topic(topic);
}

void DdsManagerAdapter::clear_entities()
{
    mgr_.clear_entities();
}

} // namespace rtpdds
