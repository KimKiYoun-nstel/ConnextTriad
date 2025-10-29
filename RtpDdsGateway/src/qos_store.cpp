#include "qos_store.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <regex>
#include <rti/core/BuiltinProfiles.hpp>
#include <rti/core/rticore.hpp>
#include <unordered_set>

#include "../../DkmRtpIpc/include/triad_log.hpp"

namespace fs = std::filesystem;

namespace rtpdds
{

// Previously cached missing builtin libs (removed - we now use QosProvider::Default())

// Minimal summarizers for discovery/runtime fields.
// IMPORTANT: avoid accessing implementation-specific QoS accessor members
// (e.g. p.writer.reliability()) because RTI wrapper types vary by version
// and may not expose those methods. Return empty objects as placeholders.
// --- enum → string helpers ---
static inline const char* to_str(dds::core::policy::ReliabilityKind k)
{
    return (k == dds::core::policy::ReliabilityKind::RELIABLE) ? "RELIABLE" : "BEST_EFFORT";
}

static inline const char* to_str(dds::core::policy::DurabilityKind k)
{
    using K = dds::core::policy::DurabilityKind;
    if (k == K::PERSISTENT) return "PERSISTENT";
    if (k == K::TRANSIENT) return "TRANSIENT";
    if (k == K::TRANSIENT_LOCAL) return "TRANSIENT_LOCAL";
    return "VOLATILE";
}

static inline const char* to_str(dds::core::policy::OwnershipKind k)
{
    return (k == dds::core::policy::OwnershipKind::EXCLUSIVE) ? "EXCLUSIVE" : "SHARED";
}

static inline const char* to_str(dds::core::policy::HistoryKind k)
{
    return (k == dds::core::policy::HistoryKind::KEEP_LAST) ? "KEEP_LAST" : "KEEP_ALL";
}

static inline nlohmann::json to_json(const dds::core::policy::Liveliness& lv)
{
    // convert kind -> string via an internal lambda to avoid a separate top-level to_str overload
    auto kind_to_str = [](dds::core::policy::LivelinessKind k) -> const char* {
        using K = dds::core::policy::LivelinessKind;
        if (k == K::MANUAL_BY_PARTICIPANT) return "MANUAL_BY_PARTICIPANT";
        if (k == K::MANUAL_BY_TOPIC) return "MANUAL_BY_TOPIC";
        return "AUTOMATIC";
    };

    const auto& lease = lv.lease_duration();
    nlohmann::json lease_json;
    if (lease == dds::core::Duration::infinite()) {
        lease_json = "INFINITE";
    } else if (lease == dds::core::Duration::automatic()) {
        lease_json = "AUTOMATIC";
    } else {
        lease_json = {{"sec", lease.sec()}, {"nanosec", lease.nanosec()}};
    }

    return {{"kind", kind_to_str(lv.kind())}, {"lease_duration", lease_json}};
}

static inline const char* to_str(dds::core::policy::PresentationAccessScopeKind k)
{
    using K = dds::core::policy::PresentationAccessScopeKind;
    if (k == K::INSTANCE) return "INSTANCE";
    if (k == K::TOPIC) return "TOPIC";
    if (k == K::GROUP) return "GROUP";
    if (k == K::HIGHEST_OFFERED) return "HIGHEST_OFFERED";
    return "UNKNOWN";
}

static inline nlohmann::json to_json(const dds::core::Duration& d)
{
    if (d == dds::core::Duration::infinite()) return {{"special", "INFINITE"}};    // 무한
    if (d == dds::core::Duration::automatic()) return {{"special", "AUTOMATIC"}};  // 자동
    return {{"sec", d.sec()}, {"nanosec", d.nanosec()}};
}

static inline nlohmann::json to_json(const dds::core::policy::ResourceLimits& rl)
{
    auto make_limit = [](int v) -> nlohmann::json {
        return (v == dds::core::LENGTH_UNLIMITED) ? nlohmann::json("UNLIMITED")
                                                  : nlohmann::json(static_cast<int64_t>(v));
    };

    return {{"max_samples", make_limit(rl.max_samples())},
            {"max_instances", make_limit(rl.max_instances())},
            {"max_samples_per_instance", make_limit(rl.max_samples_per_instance())}};
}

// discovery (writer/reader/topic)
static nlohmann::json summarize_discovery(const dds::pub::qos::DataWriterQos& w,
                                          const dds::sub::qos::DataReaderQos& /*r*/, const dds::topic::qos::TopicQos& t)
{
    nlohmann::json j;
    try {
        j["reliability"] = to_str(w.policy<dds::core::policy::Reliability>().kind());
        j["durability"] = to_str(t.policy<dds::core::policy::Durability>().kind());
        j["ownership"] = to_str(w.policy<dds::core::policy::Ownership>().kind());
        j["presentation"] = "N/A";
        j["partition"] = nlohmann::json::array();
    } catch (...) {
        j = nlohmann::json::object();
    }
    return j;
}

// runtime
static nlohmann::json summarize_runtime(const dds::pub::qos::DataWriterQos& w,
                                        const dds::sub::qos::DataReaderQos& /*r*/,
                                        const dds::topic::qos::TopicQos& /*t*/)
{
    nlohmann::json j;
    try {
        const auto& hist = w.policy<dds::core::policy::History>();
        const auto& rl = w.policy<dds::core::policy::ResourceLimits>();
        const auto& dl = w.policy<dds::core::policy::Deadline>();
        const auto& lb = w.policy<dds::core::policy::LatencyBudget>();
        const auto& lv = w.policy<dds::core::policy::Liveliness>();

        j["history"] = {{"kind", to_str(hist.kind())}, {"depth", (int)hist.depth()}};
        j["resource_limits"] = to_json(rl);
        j["deadline"] = to_json(dl.period());
        j["latency_budget"] = to_json(lb.duration());
        j["liveliness"] = to_json(lv);

        // optional RTI extension: PublishMode
        try {
            const auto& pm = w.policy<rti::core::policy::PublishMode>();
            j["publish_mode"] = (pm.kind() == rti::core::policy::PublishModeKind::ASYNCHRONOUS) ? "ASYNC" : "SYNC";
        } catch (...) {
            // ignore
        }
    } catch (...) {
        j = nlohmann::json::object();
    }
    return j;
}

// Helper: parse all qos_library::qos_profile name pairs from a QoS XML file
static std::vector<std::pair<std::string, std::string> > parse_profiles_from_file(const std::string& file_path)
{
    std::vector<std::pair<std::string, std::string> > out;
    try {
        std::ifstream ifs(file_path);
        if (!ifs) return out;
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        size_t search_pos = 0;
        const std::string lib_tag = "<qos_library";
        while (true) {
            auto lib_pos = content.find(lib_tag, search_pos);
            if (lib_pos == std::string::npos) break;
            auto lib_tag_end = content.find('>', lib_pos);
            if (lib_tag_end == std::string::npos) break;
            std::string opening = content.substr(lib_pos, lib_tag_end - lib_pos + 1);
            // extract name attribute if present
            auto name_pos = opening.find("name=");
            std::string lib_name;
            if (name_pos != std::string::npos) {
                size_t quote = opening.find_first_of("\"'", name_pos + 5);
                if (quote != std::string::npos) {
                    char q = opening[quote];
                    size_t quote_end = opening.find(q, quote + 1);
                    if (quote_end != std::string::npos) {
                        lib_name = opening.substr(quote + 1, quote_end - (quote + 1));
                    }
                }
            }
            // find end of this qos_library block
            auto lib_close = content.find("</qos_library>", lib_tag_end);
            if (lib_close == std::string::npos) break;
            std::string lib_block = content.substr(lib_tag_end + 1, lib_close - (lib_tag_end + 1));
            // find all qos_profile entries
            size_t prof_search = 0;
            const std::string prof_tag = "<qos_profile";
            while (true) {
                auto prof_pos = lib_block.find(prof_tag, prof_search);
                if (prof_pos == std::string::npos) break;
                auto prof_tag_end = lib_block.find('>', prof_pos);
                if (prof_tag_end == std::string::npos) break;
                std::string prof_open = lib_block.substr(prof_pos, prof_tag_end - prof_pos + 1);
                std::string prof_name;
                auto pname_pos = prof_open.find("name=");
                if (pname_pos != std::string::npos) {
                    size_t pquote = prof_open.find_first_of("\"'", pname_pos + 5);
                    if (pquote != std::string::npos) {
                        char pq = prof_open[pquote];
                        size_t pquote_end = prof_open.find(pq, pquote + 1);
                        if (pquote_end != std::string::npos) {
                            prof_name = prof_open.substr(pquote + 1, pquote_end - (pquote + 1));
                        }
                    }
                }
                if (!lib_name.empty() && !prof_name.empty()) out.emplace_back(lib_name, prof_name);
                prof_search = prof_tag_end + 1;
            }
            search_pos = lib_close + std::strlen("</qos_library>");
        }
    } catch (const std::exception& e) {
        LOG_WRN("DDS", "parse_profiles_from_file failed: %s (%s)", file_path.c_str(), e.what());
    }
    return out;
}

// Collect all qos_lib helper names available via rti::core::builtin_profiles helpers
static std::vector<std::string> collect_builtin_qos_lib_names()
{
    std::vector<std::string> names;
    auto try_push = [&](std::function<std::string()> fn) {
        try {
            std::string v = fn();
            if (!v.empty()) names.push_back(std::move(v));
        } catch (...) {
        }
    };

    // Fully-qualified calls wrapped in lambdas to handle potential missing symbols across RTI versions
    // try_push([](){ return rti::core::builtin_profiles::qos_lib::library_name(); });
    try_push([]() { return rti::core::builtin_profiles::qos_lib::baseline(); });
    try_push([]() { return rti::core::builtin_profiles::qos_lib::generic_common(); });
    /* 추후에 재사용할 것임.
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_monitoring_common(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_strict_reliable(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_keep_last_reliable(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_best_effort(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_connext_micro_compatibility(); });
    */
    /* 추후에 재사용할 것임.
    try_push([](){ return rti::core::builtin_profiles::qos_lib::baseline_5_0_0(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::baseline_5_1_0(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::baseline_5_2_0(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::baseline_5_3_0(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::baseline_6_0_0(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::baseline_6_1_0(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::baseline_7_0_0(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::baseline_7_1_0(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_connext_micro_compatibility_2_4_9(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_connext_micro_compatibility_2_4_3(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_other_dds_vendor_compatibility(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_510_transport_compatibility(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_security(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_strict_reliable_high_throughput(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_strict_reliable_low_latency(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_participant_large_data(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_participant_large_data_monitoring(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_strict_reliable_large_data(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_keep_last_reliable_large_data(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_strict_reliable_large_data_fast_flow(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_strict_reliable_large_data_medium_flow(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_strict_reliable_large_data_slow_flow(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_keep_last_reliable_large_data_fast_flow(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_keep_last_reliable_large_data_medium_flow(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_keep_last_reliable_large_data_slow_flow(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_keep_last_reliable_transient_local(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_keep_last_reliable_transient(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_keep_last_reliable_persistent(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_auto_tuning(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_minimal_memory_footprint(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::generic_monitoring2(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::pattern_rpc(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::pattern_periodic_data(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::pattern_streaming(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::pattern_reliable_streaming(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::pattern_event(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::pattern_alarm_event(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::pattern_status(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::pattern_alarm_status(); });
    try_push([](){ return rti::core::builtin_profiles::qos_lib::pattern_last_value_cache(); });
    */

    // ensure unique
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

QosStore::QosStore(std::string dir) : dir_(std::move(dir)) {}

QosStore::~QosStore() = default;

void QosStore::initialize()
{
    try {
        std::unique_lock lk(mtx_);
        providers_ = load_providers_from_dir_nothrow(dir_);
        cache_.clear();
        cache_version_ = 0;
        LOG_INF("DDS", "QosStore initialized dir=%s providers=%zu", dir_.c_str(), providers_.size());

        // enumerate and log available library::profile pairs for user visibility
        for (const auto& pe : providers_) {
            auto pairs = parse_profiles_from_file(pe.path);
            for (const auto& lp : pairs) {
                LOG_INF("DDS", "[qos-profile] %s::%s (file=%s)", lp.first.c_str(), lp.second.c_str(), pe.path.c_str());
            }
            if (pairs.empty()) {
                LOG_DBG("DDS", "[qos-profile] none found in %s", pe.path.c_str());
            }
        }
        // cache builtin candidate names once for consistency and to avoid repeated helper calls
        try {
            builtin_candidates_ = collect_builtin_qos_lib_names();
            LOG_DBG("DDS", "[qos-cache] builtin candidates=%zu", builtin_candidates_.size());
        } catch (...) {
            LOG_DBG("DDS", "[qos-cache] collect_builtin_qos_lib_names failed");
        }
    } catch (const std::exception& e) {
        LOG_WRN("DDS", "QosStore initialize exception: %s", e.what());
    }
}

std::vector<QosStore::ProviderEntry> QosStore::load_providers_from_dir_nothrow(const std::string& dir)
{
    std::vector<std::string> files;
    try {
        if (!fs::exists(dir)) {
            LOG_WRN("DDS", "Qos dir not found: %s", dir.c_str());
            return {};
        }
        for (auto& p : fs::directory_iterator(dir)) {
            if (!p.is_regular_file()) continue;
            auto path = p.path().string();
            auto ext = p.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
            if (ext == ".xml") files.push_back(path);
        }
        // sort by name (stable) so that last files are considered "last loaded"
        std::sort(files.begin(), files.end());
    } catch (const std::exception& e) {
        LOG_WRN("DDS", "Qos dir scan failed: %s", e.what());
    }

    std::vector<ProviderEntry> out;
    for (const auto& f : files) {
        try {
            auto prov = std::make_shared<dds::core::QosProvider>(f);
            ProviderEntry pe;
            pe.path = f;
            pe.provider = prov;
            // parse profiles once and cache
            try {
                auto pairs = parse_profiles_from_file(f);
                for (const auto &p : pairs) pe.profiles.insert(p.first + "::" + p.second);
            } catch (...) {
                // ignore parse failures here
            }
            // record last write time if possible
            try {
                pe.mtime = fs::last_write_time(f);
            } catch (...) {
            }
            out.push_back(std::move(pe));
            LOG_INF("DDS", "[qos-file] loaded: %s", f.c_str());
        } catch (const std::exception& e) {
            LOG_WRN("DDS", "[qos-file] load failed: %s (%s)", f.c_str(), e.what());
        }
    }
    return out;
}

std::string QosStore::key(const std::string& lib, const std::string& profile)
{
    return lib + "::" + profile;
}

std::optional<QosPack> QosStore::resolve_from_providers(const std::string& lib, const std::string& profile)
{
    const std::string combined = lib + "::" + profile;
    for (auto it = providers_.rbegin(); it != providers_.rend(); ++it) {
        // check cached profile list first; if absent, consider reparsing file if it changed
        bool maybe_present = false;
        try {
            if (it->profiles.find(combined) != it->profiles.end()) {
                maybe_present = true;
            } else {
                // check mtime to avoid repeated reparsing
                try {
                    auto now = fs::last_write_time(it->path);
                    if (now != it->mtime) {
                        // file changed -> reparse and update cache
                        auto pairs = parse_profiles_from_file(it->path);
                        it->profiles.clear();
                        for (const auto &p : pairs) it->profiles.insert(p.first + "::" + p.second);
                        it->mtime = now;
                        if (it->profiles.find(combined) != it->profiles.end()) maybe_present = true;
                        LOG_DBG("DDS", "[qos-cache] reparse %s profiles=%zu", it->path.c_str(), it->profiles.size());
                    }
                } catch (...) {
                    // ignore file time errors
                }
            }
        } catch (...) {
            // ignore any cache inspection errors and attempt direct resolve below
        }

        if (!maybe_present) {
            // not present according to cache; skip RTI lookup to avoid noisy negative logs
            continue;
        }

        try {
            QosPack p;
            // RTI QosProvider APIs accept a single profile identifier in the form 'library::profile'
            p.participant = it->provider->participant_qos(combined);
            p.publisher = it->provider->publisher_qos(combined);
            p.subscriber = it->provider->subscriber_qos(combined);
            p.topic = it->provider->topic_qos(combined);
            p.writer = it->provider->datawriter_qos(combined);
            p.reader = it->provider->datareader_qos(combined);
            p.origin_file = it->path;
            return p;
        } catch (const std::exception&) {
            // not found in this provider, continue
        }
    }
    return std::nullopt;
}

std::optional<QosPack> QosStore::find_or_reload(const std::string& lib, const std::string& profile)
{
    const auto k = key(lib, profile);
    {  // read lock for cache
        std::shared_lock lk(mtx_);
        auto it = cache_.find(k);
        if (it != cache_.end()) return it->second;
    }

    {  // attempt resolve from current providers with unique lock
        std::unique_lock lk(mtx_);
        if (auto itc = cache_.find(k); itc != cache_.end()) return itc->second;
        if (auto pack = resolve_from_providers(lib, profile)) {
            cache_[k] = *pack;
            // log source xml
            auto xml = extract_profile_xml(pack->origin_file, lib, profile);
            if (!xml.empty()) {
                LOG_INF("DDS", "[qos-load] %s::%s from=%s\n%s", lib.c_str(), profile.c_str(), pack->origin_file.c_str(),
                        xml.c_str());
            } else {
                LOG_WRN("DDS", "[qos-load] profile xml not found for %s::%s (file=%s)", lib.c_str(), profile.c_str(),
                        pack->origin_file.c_str());
            }
            return pack;
        }
    }

    // cache miss: reload all providers and retry
    reload_all();

    {
        std::unique_lock lk(mtx_);
        if (auto pack = resolve_from_providers(lib, profile)) {
            cache_.clear();
            cache_[k] = *pack;
            cache_version_++;
            auto xml = extract_profile_xml(pack->origin_file, lib, profile);
            if (!xml.empty()) {
                LOG_INF("DDS", "[qos-reload] %s::%s from=%s\n%s", lib.c_str(), profile.c_str(),
                        pack->origin_file.c_str(), xml.c_str());
            }
            return pack;
        }
        LOG_WRN("DDS", "[qos-miss] %s::%s not found after reload", lib.c_str(), profile.c_str());
        return std::nullopt;
    }
}

void QosStore::reload_all()
{
    std::unique_lock lk(mtx_);
    providers_ = load_providers_from_dir_nothrow(dir_);
    cache_.clear();
    cache_version_++;
    LOG_INF("DDS", "[qos-reload-all] dir=%s providers=%zu version=%llu", dir_.c_str(), providers_.size(),
            (unsigned long long)cache_version_);
}

std::string QosStore::extract_profile_xml(const std::string& file_path, const std::string& lib,
                                          const std::string& profile)
{
    try {
        std::ifstream ifs(file_path);
        if (!ifs) return {};
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        // Simple and robust parser: find qos_library opening tag that has matching name attribute,
        // then find qos_profile inside it with matching name attribute. Avoid complex regex flags.
        size_t search_pos = 0;
        const std::string lib_tag = "<qos_library";
        while (true) {
            auto lib_pos = content.find(lib_tag, search_pos);
            if (lib_pos == std::string::npos) break;
            // find end of opening tag
            auto lib_tag_end = content.find('>', lib_pos);
            if (lib_tag_end == std::string::npos) break;
            std::string opening = content.substr(lib_pos, lib_tag_end - lib_pos + 1);
            // check for name attribute in opening tag
            const std::string name1 = "name=\"" + lib + "\"";
            const std::string name2 = "name='" + lib + "'";
            if (opening.find(name1) != std::string::npos || opening.find(name2) != std::string::npos) {
                // find closing </qos_library>
                auto lib_close = content.find("</qos_library>", lib_tag_end);
                if (lib_close == std::string::npos) break;
                std::string lib_block = content.substr(lib_tag_end + 1, lib_close - (lib_tag_end + 1));
                // search for qos_profile inside lib_block
                size_t prof_pos = lib_block.find("<qos_profile");
                while (prof_pos != std::string::npos) {
                    // find end of opening profile tag
                    auto prof_tag_end = lib_block.find('>', prof_pos);
                    if (prof_tag_end == std::string::npos) break;
                    std::string prof_open = lib_block.substr(prof_pos, prof_tag_end - prof_pos + 1);
                    const std::string pname1 = "name=\"" + profile + "\"";
                    const std::string pname2 = "name='" + profile + "'";
                    if (prof_open.find(pname1) != std::string::npos || prof_open.find(pname2) != std::string::npos) {
                        // find closing </qos_profile>
                        auto prof_close = lib_block.find("</qos_profile>", prof_tag_end);
                        if (prof_close == std::string::npos) break;
                        // return full profile element including tags
                        size_t abs_start = lib_pos + (lib_tag_end + 1 - lib_pos) + prof_pos;
                        size_t abs_end =
                            lib_pos + (lib_tag_end + 1 - lib_pos) + prof_close + std::strlen("</qos_profile>");
                        return content.substr(abs_start - (prof_pos),
                                              (prof_close + std::strlen("</qos_profile>")) - prof_pos);
                    }
                    // move to next profile inside lib_block
                    prof_pos = lib_block.find("<qos_profile", prof_tag_end);
                }
            }
            search_pos = lib_tag_end + 1;
        }
    } catch (const std::exception& e) {
        LOG_WRN("DDS", "extract_profile_xml failed: %s", e.what());
    }
    return {};
}

// Return a sorted unique list of available profiles (external first from providers, thenbuiltin candidates)
std::vector<std::string> QosStore::list_profiles(bool include_builtin) const
{
    std::vector<std::string> out;
    std::shared_lock lk(mtx_);
    // external: iterate providers in load order and collect pairs
    for (const auto& pe : providers_) {
        auto pairs = parse_profiles_from_file(pe.path);
        for (const auto& lp : pairs) {
            out.push_back(lp.first + "::" + lp.second);
        }
    }
    // builtin candidates appended using cached helper names where available
    if (include_builtin) {
        out.insert(out.end(), builtin_candidates_.begin(), builtin_candidates_.end());
    }
    // unique & stable sort
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// Build detail JSON as an ARRAY of entries aligned with list_profiles() order.
// Each entry has the form: { "name": "lib::profile", "detail": { source_kind:..., discovery:{}, runtime:{} } }
nlohmann::json QosStore::detail_profiles(bool include_builtin) const
{
    // Build the detail map first (same as previous behavior), then convert to an array
    nlohmann::json detail_map = nlohmann::json::object();
    std::shared_lock lk(mtx_);

    auto fill_effective = [&](auto& prov, const std::string& full, nlohmann::json& obj) -> bool {
        try {
            auto w = prov.datawriter_qos(full);
            auto r = prov.datareader_qos(full);
            auto t = prov.topic_qos(full);
            obj["discovery"] = summarize_discovery(w, r, t);
            obj["runtime"] = summarize_runtime(w, r, t);
            return true;
        } catch (const std::exception& ex) {
            (void)ex;
            return false;
        } catch (...) {
            return false;
        }
    };

    // 1) 외부: "통합된 값"만 (effective) + source_kind
    for (auto it = providers_.rbegin(); it != providers_.rend(); ++it) {  // 마지막 로드 우선
        auto pairs = parse_profiles_from_file(it->path);
        for (const auto& lp : pairs) {
            const std::string full = lp.first + "::" + lp.second;

            nlohmann::json obj;
            if (!fill_effective(*it->provider, full, obj)) {
                LOG_DBG("DDS", "[qos-detail] provider=%s profile=%s not found or summarize failed", it->path.c_str(),
                        full.c_str());
                continue;
            }
            obj["source_kind"] = "external";

            // 마지막 로드 우선 정책: 기존에 있으면 덮어씀
            detail_map[full] = std::move(obj);
            LOG_DBG("DDS", "[qos-detail] loaded external profile=%s from=%s", full.c_str(), it->path.c_str());
        }
    }

    // 2) 내장: use QosProvider::Default() + RTI helper profile name strings
    if (include_builtin) {
        try {
            auto prov = dds::core::QosProvider::Default();
            const std::vector<std::string> candidates = builtin_candidates_;

            for (const auto& full : candidates) {
                nlohmann::json obj;
                if (fill_effective(prov, full, obj)) {
                    obj["source_kind"] = "builtin";
                    detail_map[full] = std::move(obj);
                    LOG_DBG("DDS", "[qos-detail] loaded builtin profile=%s via Default()", full.c_str());
                } else {
                    LOG_DBG("DDS", "[qos-detail] builtin profile not present: %s", full.c_str());
                }
            }
        } catch (const std::exception& ex) {
            LOG_WRN("DDS", "[qos-detail] builtin profiles query failed: %s", ex.what());
        } catch (...) {
            LOG_WRN("DDS", "[qos-detail] builtin profiles query failed: unknown error");
        }
    }

    // Build an array of single-key objects: [{ "lib::profile": { ... } }, ...]
    // The order of entries does not need to match result; sort keys for deterministic output.
    nlohmann::json detail_arr = nlohmann::json::array();
    std::vector<std::string> keys;
    for (auto it = detail_map.begin(); it != detail_map.end(); ++it) keys.push_back(it.key());
    std::sort(keys.begin(), keys.end());
    for (const auto& full : keys) {
        nlohmann::json item = nlohmann::json::object();
        item[full] = detail_map.value(full, nlohmann::json::object());
        detail_arr.push_back(std::move(item));
    }

    return detail_arr;
}

}  // namespace rtpdds
