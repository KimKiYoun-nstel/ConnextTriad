/**
 * @file qos_xml_helpers.cpp
 * @brief QoS XML 변환/파싱/병합 유틸 구현
 */

#include "qos_xml_helpers.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <rti/core/policy/CorePolicy.hpp>

#include "../../DkmRtpIpc/include/triad_log.hpp"

namespace rtpdds
{
namespace qos_xml
{

// ============================================================================
// 내부 헬퍼 함수들 (익명 namespace)
// ============================================================================

namespace
{

// enum → string 변환 헬퍼들
inline const char* to_str(dds::core::policy::ReliabilityKind k)
{
    return (k == dds::core::policy::ReliabilityKind::RELIABLE) ? "RELIABLE" : "BEST_EFFORT";
}

inline const char* to_str(dds::core::policy::DurabilityKind k)
{
    using K = dds::core::policy::DurabilityKind;
    if (k == K::PERSISTENT) return "PERSISTENT";
    if (k == K::TRANSIENT) return "TRANSIENT";
    if (k == K::TRANSIENT_LOCAL) return "TRANSIENT_LOCAL";
    return "VOLATILE";
}

inline const char* to_str(dds::core::policy::OwnershipKind k)
{
    return (k == dds::core::policy::OwnershipKind::EXCLUSIVE) ? "EXCLUSIVE" : "SHARED";
}

inline const char* to_str(dds::core::policy::HistoryKind k)
{
    return (k == dds::core::policy::HistoryKind::KEEP_LAST) ? "KEEP_LAST" : "KEEP_ALL";
}

// Duration을 XML 형식으로 변환 (압축)
std::string duration_to_xml(const dds::core::Duration& d)
{
    if (d == dds::core::Duration::infinite()) {
        return "<sec>DURATION_INFINITE_SEC</sec><nanosec>DURATION_INFINITE_NSEC</nanosec>";
    }
    return "<sec>" + std::to_string(d.sec()) + "</sec><nanosec>" + std::to_string(d.nanosec()) + "</nanosec>";
}

// Reliability 정책을 XML로 변환 (압축)
std::string reliability_to_xml(const dds::core::policy::Reliability& rel)
{
    std::string xml = "<reliability><kind>" + std::string(to_str(rel.kind())) + "_RELIABILITY_QOS</kind>";

    // max_blocking_time은 Writer QoS에만 해당
    try {
        const auto& mbt = rel.max_blocking_time();
        if (mbt != dds::core::Duration::from_millisecs(100)) {  // 기본값이 아니면 포함
            xml += "<max_blocking_time>" + duration_to_xml(mbt) + "</max_blocking_time>";
        }
    } catch (...) {
    }

    xml += "</reliability>";
    return xml;
}

// Durability 정책을 XML로 변환 (압축)
std::string durability_to_xml(const dds::core::policy::Durability& dur)
{
    return "<durability><kind>" + std::string(to_str(dur.kind())) + "_DURABILITY_QOS</kind></durability>";
}

// History 정책을 XML로 변환 (압축)
std::string history_to_xml(const dds::core::policy::History& hist)
{
    std::string xml = "<history><kind>" + std::string(to_str(hist.kind())) + "_HISTORY_QOS</kind>";
    if (hist.kind() == dds::core::policy::HistoryKind::KEEP_LAST) {
        xml += "<depth>" + std::to_string(hist.depth()) + "</depth>";
    }
    xml += "</history>";
    return xml;
}

// ResourceLimits 정책을 XML로 변환 (압축)
std::string resource_limits_to_xml(const dds::core::policy::ResourceLimits& rl)
{
    auto fmt_limit = [](int v) -> std::string {
        return (v == dds::core::LENGTH_UNLIMITED) ? "LENGTH_UNLIMITED" : std::to_string(v);
    };

    return "<resource_limits>"
           "<max_samples>" +
           fmt_limit(rl.max_samples()) +
           "</max_samples>"
           "<max_instances>" +
           fmt_limit(rl.max_instances()) +
           "</max_instances>"
           "<max_samples_per_instance>" +
           fmt_limit(rl.max_samples_per_instance()) + "</max_samples_per_instance>"
                                                       "</resource_limits>";
}

// Deadline 정책을 XML로 변환 (압축)
std::string deadline_to_xml(const dds::core::policy::Deadline& dl)
{
    return "<deadline><period>" + duration_to_xml(dl.period()) + "</period></deadline>";
}

// LatencyBudget 정책을 XML로 변환 (압축)
std::string latency_budget_to_xml(const dds::core::policy::LatencyBudget& lb)
{
    return "<latency_budget><duration>" + duration_to_xml(lb.duration()) + "</duration></latency_budget>";
}

// Liveliness 정책을 XML로 변환 (압축)
std::string liveliness_to_xml(const dds::core::policy::Liveliness& lv)
{
    auto kind_to_str = [](dds::core::policy::LivelinessKind k) -> std::string {
        using K = dds::core::policy::LivelinessKind;
        if (k == K::MANUAL_BY_PARTICIPANT) return "MANUAL_BY_PARTICIPANT";
        if (k == K::MANUAL_BY_TOPIC) return "MANUAL_BY_TOPIC";
        return "AUTOMATIC";
    };

    return "<liveliness><kind>" + kind_to_str(lv.kind()) +
           "_LIVELINESS_QOS</kind>"
           "<lease_duration>" +
           duration_to_xml(lv.lease_duration()) + "</lease_duration>"
                                                   "</liveliness>";
}

// Ownership 정책을 XML로 변환 (압축)
std::string ownership_to_xml(const dds::core::policy::Ownership& own)
{
    return "<ownership><kind>" + std::string(to_str(own.kind())) + "_OWNERSHIP_QOS</kind></ownership>";
}

// OwnershipStrength 정책을 XML로 변환 (압축, DataWriter만 해당)
std::string ownership_strength_to_xml(const dds::core::policy::OwnershipStrength& os)
{
    return "<ownership_strength><value>" + std::to_string(os.value()) + "</value></ownership_strength>";
}

// TransportPriority 정책을 XML로 변환 (압축, DataWriter만 해당)
std::string transport_priority_to_xml(int32_t value)
{
    if (value == 0) return "";  // 기본값이면 생략
    return "<transport_priority><value>" + std::to_string(value) + "</value></transport_priority>";
}

// DataWriter QoS를 XML로 변환 (압축)
std::string datawriter_qos_to_xml(const dds::pub::qos::DataWriterQos& w)
{
    std::string xml = "<datawriter_qos>";

    try {
        xml += reliability_to_xml(w.policy<dds::core::policy::Reliability>());
        xml += history_to_xml(w.policy<dds::core::policy::History>());
        xml += resource_limits_to_xml(w.policy<dds::core::policy::ResourceLimits>());

        const auto& dl = w.policy<dds::core::policy::Deadline>();
        if (dl.period() != dds::core::Duration::infinite()) {
            xml += deadline_to_xml(dl);
        }

        const auto& lb = w.policy<dds::core::policy::LatencyBudget>();
        if (lb.duration() != dds::core::Duration::zero()) {
            xml += latency_budget_to_xml(lb);
        }

        xml += liveliness_to_xml(w.policy<dds::core::policy::Liveliness>());
        xml += ownership_to_xml(w.policy<dds::core::policy::Ownership>());

        const auto& os = w.policy<dds::core::policy::OwnershipStrength>();
        if (os.value() != 0) {
            xml += ownership_strength_to_xml(os);
        }

        // TransportPriority
        try {
            const auto& tp = w.policy<dds::core::policy::TransportPriority>();
            xml += transport_priority_to_xml(tp.value());
        } catch (...) {
        }

    } catch (const std::exception& e) {
        LOG_WRN("DDS", "datawriter_qos_to_xml failed: %s", e.what());
    }

    xml += "</datawriter_qos>";
    return xml;
}

// DataReader QoS를 XML로 변환 (압축)
std::string datareader_qos_to_xml(const dds::sub::qos::DataReaderQos& r)
{
    std::string xml = "<datareader_qos>";

    try {
        xml += reliability_to_xml(r.policy<dds::core::policy::Reliability>());
        xml += history_to_xml(r.policy<dds::core::policy::History>());
        xml += resource_limits_to_xml(r.policy<dds::core::policy::ResourceLimits>());

        const auto& dl = r.policy<dds::core::policy::Deadline>();
        if (dl.period() != dds::core::Duration::infinite()) {
            xml += deadline_to_xml(dl);
        }

        const auto& lb = r.policy<dds::core::policy::LatencyBudget>();
        if (lb.duration() != dds::core::Duration::zero()) {
            xml += latency_budget_to_xml(lb);
        }

        xml += liveliness_to_xml(r.policy<dds::core::policy::Liveliness>());
        xml += ownership_to_xml(r.policy<dds::core::policy::Ownership>());

    } catch (const std::exception& e) {
        LOG_WRN("DDS", "datareader_qos_to_xml failed: %s", e.what());
    }

    xml += "</datareader_qos>";
    return xml;
}

// Topic QoS를 XML로 변환 (압축)
std::string topic_qos_to_xml(const dds::topic::qos::TopicQos& t)
{
    std::string xml = "<topic_qos>";

    try {
        xml += durability_to_xml(t.policy<dds::core::policy::Durability>());
        xml += history_to_xml(t.policy<dds::core::policy::History>());
        xml += resource_limits_to_xml(t.policy<dds::core::policy::ResourceLimits>());

        const auto& dl = t.policy<dds::core::policy::Deadline>();
        if (dl.period() != dds::core::Duration::infinite()) {
            xml += deadline_to_xml(dl);
        }

        const auto& lb = t.policy<dds::core::policy::LatencyBudget>();
        if (lb.duration() != dds::core::Duration::zero()) {
            xml += latency_budget_to_xml(lb);
        }

        xml += liveliness_to_xml(t.policy<dds::core::policy::Liveliness>());
        xml += ownership_to_xml(t.policy<dds::core::policy::Ownership>());

    } catch (const std::exception& e) {
        LOG_WRN("DDS", "topic_qos_to_xml failed: %s", e.what());
    }

    xml += "</topic_qos>";
    return xml;
}

}  // anonymous namespace

// ============================================================================
// 공개 API 구현
// ============================================================================

std::string qos_pack_to_profile_xml(const std::string& profile_name, const dds::pub::qos::DataWriterQos& w,
                                     const dds::sub::qos::DataReaderQos& r, const dds::topic::qos::TopicQos& t,
                                     const std::string& base_name)
{
    std::string xml = "<qos_profile name=\"" + profile_name + "\"";
    if (!base_name.empty()) {
        xml += " base_name=\"" + base_name + "\"";
    }
    xml += ">";

    xml += datawriter_qos_to_xml(w);
    xml += datareader_qos_to_xml(r);
    xml += topic_qos_to_xml(t);

    xml += "</qos_profile>";
    return xml;
}

std::vector<std::pair<std::string, std::string>> parse_profiles_from_file(const std::string& file_path)
{
    std::vector<std::pair<std::string, std::string>> out;
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
            std::string lib_name;
            auto npos = opening.find("name=");
            if (npos != std::string::npos) {
                auto quote = opening.find_first_of("\"'", npos + 5);
                if (quote != std::string::npos) {
                    char q = opening[quote];
                    auto qend = opening.find(q, quote + 1);
                    if (qend != std::string::npos) {
                        lib_name = opening.substr(quote + 1, qend - (quote + 1));
                    }
                }
            }
            if (lib_name.empty()) {
                search_pos = lib_tag_end + 1;
                continue;
            }
            auto lib_close = content.find("</qos_library>", lib_tag_end);
            if (lib_close == std::string::npos) break;
            std::string lib_block = content.substr(lib_tag_end + 1, lib_close - (lib_tag_end + 1));
            size_t prof_pos = lib_block.find("<qos_profile");
            while (prof_pos != std::string::npos) {
                auto prof_tag_end = lib_block.find('>', prof_pos);
                if (prof_tag_end == std::string::npos) break;
                std::string prof_open = lib_block.substr(prof_pos, prof_tag_end - prof_pos + 1);
                std::string prof_name;
                auto pnpos = prof_open.find("name=");
                if (pnpos != std::string::npos) {
                    auto pquote = prof_open.find_first_of("\"'", pnpos + 5);
                    if (pquote != std::string::npos) {
                        char pq = prof_open[pquote];
                        auto pqend = prof_open.find(pq, pquote + 1);
                        if (pqend != std::string::npos) {
                            prof_name = prof_open.substr(pquote + 1, pqend - (pquote + 1));
                        }
                    }
                }
                if (!prof_name.empty()) {
                    out.emplace_back(lib_name, prof_name);
                }
                prof_pos = lib_block.find("<qos_profile", prof_tag_end);
            }
            search_pos = lib_tag_end + 1;
        }
    } catch (const std::exception& e) {
        LOG_WRN("DDS", "parse_profiles_from_file failed: %s (%s)", file_path.c_str(), e.what());
    }
    return out;
}

std::string extract_profile_xml_from_content(const std::string& content, const std::string& lib,
                                              const std::string& profile)
{
    try {
        size_t search_pos = 0;
        const std::string lib_tag = "<qos_library";
        while (true) {
            auto lib_pos = content.find(lib_tag, search_pos);
            if (lib_pos == std::string::npos) break;
            auto lib_tag_end = content.find('>', lib_pos);
            if (lib_tag_end == std::string::npos) break;
            std::string opening = content.substr(lib_pos, lib_tag_end - lib_pos + 1);
            const std::string name1 = "name=\"" + lib + "\"";
            const std::string name2 = "name='" + lib + "'";
            if (opening.find(name1) != std::string::npos || opening.find(name2) != std::string::npos) {
                auto lib_close = content.find("</qos_library>", lib_tag_end);
                if (lib_close == std::string::npos) break;
                std::string lib_block = content.substr(lib_tag_end + 1, lib_close - (lib_tag_end + 1));
                size_t prof_pos = lib_block.find("<qos_profile");
                while (prof_pos != std::string::npos) {
                    auto prof_tag_end = lib_block.find('>', prof_pos);
                    if (prof_tag_end == std::string::npos) break;
                    std::string prof_open = lib_block.substr(prof_pos, prof_tag_end - prof_pos + 1);
                    const std::string pname1 = "name=\"" + profile + "\"";
                    const std::string pname2 = "name='" + profile + "'";
                    if (prof_open.find(pname1) != std::string::npos || prof_open.find(pname2) != std::string::npos) {
                        auto prof_close = lib_block.find("</qos_profile>", prof_tag_end);
                        if (prof_close == std::string::npos) break;
                        return lib_block.substr(prof_pos, prof_close + std::strlen("</qos_profile>") - prof_pos);
                    }
                    prof_pos = lib_block.find("<qos_profile", prof_tag_end);
                }
            }
            search_pos = lib_tag_end + 1;
        }
    } catch (const std::exception& e) {
        LOG_WRN("DDS", "extract_profile_xml_from_content failed: %s", e.what());
    }
    return {};
}

std::string compress_xml(const std::string& xml)
{
    if (xml.empty()) return xml;

    std::string result;
    result.reserve(xml.size());

    bool in_tag = false;
    bool in_content = false;
    bool prev_was_space = false;

    for (size_t i = 0; i < xml.size(); ++i) {
        char c = xml[i];

        // 개행, 탭 무시
        if (c == '\n' || c == '\r' || c == '\t') {
            continue;
        }

        // 태그 시작/종료 추적
        if (c == '<') {
            in_tag = true;
            in_content = false;
            prev_was_space = false;
            result += c;
            continue;
        }

        if (c == '>') {
            in_tag = false;
            in_content = true;
            prev_was_space = false;
            result += c;
            continue;
        }

        // 태그 내부는 공백 제거 (단, 속성 사이 공백 하나는 유지)
        if (in_tag) {
            if (c == ' ') {
                if (!prev_was_space) {
                    result += c;
                    prev_was_space = true;
                }
            } else {
                result += c;
                prev_was_space = false;
            }
            continue;
        }

        // 컨텐츠 영역 (태그 사이 공백은 모두 제거)
        if (in_content) {
            if (c == ' ') {
                continue;  // 태그 사이 공백 무시
            }
            result += c;
            continue;
        }

        // 기본: 그대로 추가
        result += c;
    }

    return result;
}

std::string merge_profile_into_library(const std::string& lib_xml,
                                        const std::string& library_name,
                                        const std::string& profile_name,
                                        const std::string& profile_xml)
{
    try {
        // Profile XML 정규화 (qos_profile 태그가 없으면 에러)
        std::string normalized_profile = profile_xml;
        if (profile_xml.find("<qos_profile") == std::string::npos) {
            // profile_xml이 qos_profile 태그를 포함하지 않으면 실패
            return "";
        }

        // name 속성을 요청된 profile_name으로 강제 변경 (없으면 추가)
        {
            // opening tag 범위 추출
            auto prof_tag_pos = normalized_profile.find("<qos_profile");
            auto prof_tag_end = normalized_profile.find('>', prof_tag_pos);
            if (prof_tag_pos == std::string::npos || prof_tag_end == std::string::npos) return "";
            std::string opening = normalized_profile.substr(prof_tag_pos, prof_tag_end - prof_tag_pos + 1);

            // name= 존재 여부 확인
            auto npos = opening.find("name=");
            if (npos != std::string::npos) {
                // 값 영역 교체
                size_t quote = opening.find_first_of("\"'", npos + 5);
                if (quote != std::string::npos) {
                    char q = opening[quote];
                    size_t qend = opening.find(q, quote + 1);
                    if (qend != std::string::npos) {
                        std::string before = normalized_profile.substr(0, prof_tag_pos);
                        std::string after = normalized_profile.substr(prof_tag_end + 1);
                        std::string new_opening = opening.substr(0, quote + 1) + profile_name + opening.substr(qend);
                        normalized_profile = before + new_opening + after;
                    }
                }
            } else {
                // name 속성 추가: <qos_profile 뒤에 name="..." 삽입
                std::string before = normalized_profile.substr(0, prof_tag_pos + std::strlen("<qos_profile"));
                std::string after = normalized_profile.substr(prof_tag_pos + std::strlen("<qos_profile"));
                normalized_profile = before + " name=\"" + profile_name + "\"" + after;
            }
        }

        // Library XML 파싱: 정확한 <qos_library name="..."> 블록을 찾아 그 내부에서 교체/삽입 수행
        const std::string lib_tag = "<qos_library";
        size_t search_lib_pos = 0;
        size_t lib_block_start = std::string::npos;
        size_t lib_block_end = std::string::npos;

        while (true) {
            auto lib_pos = lib_xml.find(lib_tag, search_lib_pos);
            if (lib_pos == std::string::npos) break;
            auto lib_tag_end = lib_xml.find('>', lib_pos);
            if (lib_tag_end == std::string::npos) break;
            std::string opening = lib_xml.substr(lib_pos, lib_tag_end - lib_pos + 1);
            // extract library name
            std::string lib_name;
            auto npos = opening.find("name=");
            if (npos != std::string::npos) {
                size_t quote = opening.find_first_of("\"'", npos + 5);
                if (quote != std::string::npos) {
                    char q = opening[quote];
                    size_t qend = opening.find(q, quote + 1);
                    if (qend != std::string::npos) {
                        lib_name = opening.substr(quote + 1, qend - (quote + 1));
                    }
                }
            }
            if (lib_name.empty() || lib_name != library_name) {
                search_lib_pos = lib_tag_end + 1;
                continue; // 원하는 라이브러리 아니면 다음으로
            }

            // 해당 라이브러리의 닫는 태그 찾기
            auto lib_close = lib_xml.find("</qos_library>", lib_tag_end);
            if (lib_close == std::string::npos) {
                search_lib_pos = lib_tag_end + 1;
                continue;
            }

            // 정확히 일치하는 라이브러리 블록 범위 결정
            lib_block_start = lib_tag_end + 1;
            lib_block_end = lib_close;
            break;
        }

        if (lib_block_start == std::string::npos || lib_block_end == std::string::npos) {
            return ""; // invalid structure
        }

        std::string lib_block = lib_xml.substr(lib_block_start, lib_block_end - lib_block_start);

        // Profile 검색: lib_block 범위내에서 동일한 profile_name을 가진 qos_profile 찾기
        std::string search_pattern = "<qos_profile";
        size_t search_pos = 0;
        size_t found_prof_start = std::string::npos;
        size_t found_prof_end = std::string::npos;

        while (true) {
            size_t prof_pos = lib_block.find(search_pattern, search_pos);
            if (prof_pos == std::string::npos) break;

            size_t tag_end = lib_block.find('>', prof_pos);
            if (tag_end == std::string::npos) break;

            std::string opening_tag = lib_block.substr(prof_pos, tag_end - prof_pos + 1);
            // name 속성 추출
            std::string extracted_name;
            auto name_pos = opening_tag.find("name=");
            if (name_pos != std::string::npos) {
                size_t quote = opening_tag.find_first_of("\"'", name_pos + 5);
                if (quote != std::string::npos) {
                    char q = opening_tag[quote];
                    size_t quote_end = opening_tag.find(q, quote + 1);
                    if (quote_end != std::string::npos) {
                        extracted_name = opening_tag.substr(quote + 1, quote_end - (quote + 1));
                    }
                }
            }

            if (extracted_name == profile_name) {
                found_prof_start = prof_pos;
                size_t prof_close = lib_block.find("</qos_profile>", tag_end);
                if (prof_close != std::string::npos) {
                    found_prof_end = prof_close + std::strlen("</qos_profile>");
                }
                break;
            }

            search_pos = tag_end + 1;
        }

        std::string result;
        if (found_prof_start != std::string::npos && found_prof_end != std::string::npos) {
            // 기존 Profile 교체: lib_xml 전체를 결합해서 교체 위치 계산
            size_t abs_start = lib_block_start + found_prof_start;
            size_t abs_end = lib_block_start + found_prof_end;
            result = lib_xml.substr(0, abs_start) + "  " + normalized_profile + "\n" + lib_xml.substr(abs_end);
        } else {
            // 새 Profile 추가 (해당 라이브러리 블록 끝에 삽입)
            size_t abs_insert = lib_block_end; // 위치는 </qos_library> 바로 앞
            result = lib_xml.substr(0, abs_insert) + "  " + normalized_profile + "\n" + lib_xml.substr(abs_insert);
        }

        return result;

    } catch (const std::exception& ex) {
        LOG_ERR("DDS", "merge_profile_into_library exception: %s", ex.what());
        return "";
    }
}

}  // namespace qos_xml
}  // namespace rtpdds
