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

#include "qos_xml_helpers.hpp"
#include "../../DkmRtpIpc/include/triad_log.hpp"

namespace fs = std::filesystem;

namespace rtpdds
{

// qos_xml 네임스페이스의 헬퍼 함수들을 현재 스코프로 가져오기
using qos_xml::compress_xml;
using qos_xml::extract_profile_xml_from_content;
using qos_xml::merge_profile_into_library;
using qos_xml::parse_profiles_from_file;
using qos_xml::qos_pack_to_profile_xml;

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
            // cache full XML content
            try {
                std::ifstream ifs(f);
                if (ifs) {
                    pe.xml_content.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                }
            } catch (const std::exception& e) {
                LOG_WRN("DDS", "[qos-file] XML read failed: %s (%s)", f.c_str(), e.what());
            }
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
    
    // 1. 캐시 조회
    {
        std::shared_lock lk(mtx_);
        auto it = cache_.find(k);
        if (it != cache_.end()) return it->second;
    }

    // 2. 동적 프로파일 조회 (우선순위 높음)
    {
        std::unique_lock lk(mtx_);
        
        // 캐시 재확인 (경쟁 조건 방지)
        if (auto itc = cache_.find(k); itc != cache_.end()) return itc->second;
        
        // 동적 Provider에서 검색
        auto dyn_prov_it = dynamic_providers_.find(lib);
        if (dyn_prov_it != dynamic_providers_.end()) {
            const std::string combined = lib + "::" + profile;
            try {
                QosPack p;
                p.participant = dyn_prov_it->second->participant_qos(combined);
                p.publisher = dyn_prov_it->second->publisher_qos(combined);
                p.subscriber = dyn_prov_it->second->subscriber_qos(combined);
                p.topic = dyn_prov_it->second->topic_qos(combined);
                p.writer = dyn_prov_it->second->datawriter_qos(combined);
                p.reader = dyn_prov_it->second->datareader_qos(combined);
                p.origin_file = "(dynamic)";
                
                cache_[k] = p;
                LOG_INF("DDS", "[qos-load] %s from dynamic provider", combined.c_str());
                return p;
            } catch (const std::exception& ex) {
                // Profile이 해당 Library에 없음, 파일 기반으로 폴백
                LOG_DBG("DDS", "[qos-dynamic] %s not found in dynamic library, trying file-based", combined.c_str());
            }
        }
    }
    
    // 3. 파일 기반 Provider 조회
    {
        std::unique_lock lk(mtx_);
        if (auto itc = cache_.find(k); itc != cache_.end()) return itc->second;
        if (auto pack = resolve_from_providers(lib, profile)) {
            cache_[k] = *pack;
            // log source xml (압축 형식)
            auto xml = extract_profile_xml(pack->origin_file, lib, profile);
            if (!xml.empty()) {
                LOG_INF("DDS", "[qos-load] %s::%s from=%s\n%s", lib.c_str(), profile.c_str(), pack->origin_file.c_str(),
                        compress_xml(xml).c_str());
            } else {
                LOG_WRN("DDS", "[qos-load] profile xml not found for %s::%s (file=%s)", lib.c_str(), profile.c_str(),
                        pack->origin_file.c_str());
            }
            return pack;
        }
    }

    // 4. 캐시 미스: 모든 Provider 재로드 후 재시도
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
                        pack->origin_file.c_str(), compress_xml(xml).c_str());
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
        return extract_profile_xml_from_content(content, lib, profile);
    } catch (const std::exception& e) {
        LOG_WRN("DDS", "extract_profile_xml failed: %s", e.what());
    }
    return {};
}

// Return a sorted unique list of available profiles (dynamic, external, then builtin)
std::vector<std::string> QosStore::list_profiles(bool include_builtin) const
{
    std::vector<std::string> out;
    std::shared_lock lk(mtx_);
    
    // 1. 동적 프로파일 (우선순위 최상위)
    for (const auto& full_name : dynamic_profiles_index_) {
        out.push_back(full_name);
    }
    
    // 2. 외부 파일 기반 프로파일
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
// Each entry has the form: { "name": "lib::profile", "detail": { source_kind, xml } }
// Optional: discovery/runtime JSON fields (currently commented out - enable if needed)
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
            
            // Optional: discovery/runtime JSON 요약 (현재 비활성화 - 필요 시 주석 해제)
            // obj["discovery"] = summarize_discovery(w, r, t);
            // obj["runtime"] = summarize_runtime(w, r, t);
            
            // XML 전문 생성: profile 이름만 추출 (lib:: 제거)
            std::string profile_name = full;
            auto pos = full.find("::");
            if (pos != std::string::npos) {
                profile_name = full.substr(pos + 2);
            }
            obj["xml"] = qos_pack_to_profile_xml(profile_name, w, r, t);
            
            return true;
        } catch (const std::exception& ex) {
            (void)ex;
            return false;
        } catch (...) {
            return false;
        }
    };

    // 1) 동적 프로파일 (우선순위 최상위)
    for (const auto& [lib_name, provider] : dynamic_providers_) {
        // dynamic_profiles_index_에서 해당 Library에 속한 Profile 추출
        for (const auto& full_name : dynamic_profiles_index_) {
            // full_name 형식: "LibName::ProfileName"
            if (full_name.find(lib_name + "::") == 0) {
                nlohmann::json obj;
                if (!fill_effective(*provider, full_name, obj)) {
                    LOG_DBG("DDS", "[qos-detail] dynamic profile=%s not found or summarize failed", full_name.c_str());
                    continue;
                }
                obj["source_kind"] = "dynamic";

                // 동적 프로파일: 응답 xml은 항상 사용자가 제공/병합된 원본 스니펫 기반으로 반환
                auto lib_xml_it = dynamic_libraries_.find(lib_name);
                if (lib_xml_it != dynamic_libraries_.end()) {
                    std::string profile_name = full_name.substr(lib_name.size() + 2);  // "lib::" 제거
                    auto stored_xml = extract_profile_xml_from_content(lib_xml_it->second, lib_name, profile_name);
                    if (!stored_xml.empty()) {
                        obj["xml"] = compress_xml(stored_xml);
                    }
                }
                
                detail_map[full_name] = obj;
            }
        }
    }

    // 2) 외부 파일 기반: "통합된 값"만 (effective) + source_kind + 압축된 XML
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

            // 원본 파일에서 XML 추출 후 압축 (개행 제거)
            auto file_xml = extract_profile_xml(it->path, lp.first, lp.second);
            if (!file_xml.empty()) {
                obj["xml"] = compress_xml(file_xml);
            }

            // 마지막 로드 우선 정책: 동적 프로파일이 아닌 경우에만 덮어씀
            if (detail_map.find(full) == detail_map.end() || 
                detail_map[full].value("source_kind", "") != "dynamic") {
                detail_map[full] = std::move(obj);
                LOG_DBG("DDS", "[qos-detail] loaded external profile=%s from=%s", full.c_str(), it->path.c_str());
            }
        }
    }

    // 3) 내장: use QosProvider::Default() + RTI helper profile name strings
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

std::string QosStore::add_or_update_profile(const std::string& library, 
                                             const std::string& profile, 
                                             const std::string& profile_xml)
{
    std::unique_lock lk(mtx_);
    
    try {
        // 1. 기존 Library XML 조회 (없으면 빈 템플릿 생성)
        std::string lib_xml;
        auto lib_it = dynamic_libraries_.find(library);
        if (lib_it != dynamic_libraries_.end()) {
            lib_xml = lib_it->second;
        } else {
            // 파일 기반 Library XML에서 찾기 (캐시된 xml_content 사용)
            for (const auto& pe : providers_) {
                if (pe.xml_content.find("<qos_library name=\"" + library + "\"") != std::string::npos) {
                    lib_xml = pe.xml_content;
                    LOG_DBG("DDS", "add_or_update_profile: using cached XML from %s", pe.path.c_str());
                    break;
                }
            }
            if (lib_xml.empty()) {
                // 새 Library 템플릿 생성
                lib_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<dds>
  <qos_library name=")" + library + R"(">
  </qos_library>
</dds>)";
            }
        }
        
    // 2. Profile XML 병합 (교체 또는 추가)
    std::string updated_lib_xml = merge_profile_into_library(lib_xml, library, profile, profile_xml);
        if (updated_lib_xml.empty()) {
            LOG_ERR("DDS", "add_or_update_profile: failed to merge profile into library=%s profile=%s", 
                    library.c_str(), profile.c_str());
            return "";
        }
        
        // 3. QosProvider 생성 (str:// URI 사용)
        std::shared_ptr<dds::core::QosProvider> provider;
        try {
            // RTI Connext DDS는 str://"<XML>" 형식의 URI를 지원
            // 따옴표를 이스케이프하지 않고, XML 전체를 str:// 뒤에 붙임
            std::string qos_uri = "str://\"" + updated_lib_xml + "\"";
            
            provider = std::make_shared<dds::core::QosProvider>(qos_uri);
            
            LOG_DBG("DDS", "[qos-dynamic] created QosProvider from XML string for library=%s", library.c_str());
            
        } catch (const std::exception& ex) {
            LOG_ERR("DDS", "add_or_update_profile: QosProvider creation failed: %s", ex.what());
            return "";
        }
        
        // 4. 저장
        dynamic_libraries_[library] = updated_lib_xml;
        dynamic_providers_[library] = provider;
        const std::string full_name = library + "::" + profile;
        dynamic_profiles_index_.insert(full_name);
        
        // 5. 캐시 무효화 (해당 프로파일만)
        cache_.erase(key(library, profile));
        
        LOG_INF("DDS", "[qos-dynamic] added/updated %s", full_name.c_str());
        return full_name;
        
    } catch (const std::exception& ex) {
        LOG_ERR("DDS", "add_or_update_profile exception: %s", ex.what());
        return "";
    }
}

}  // namespace rtpdds
