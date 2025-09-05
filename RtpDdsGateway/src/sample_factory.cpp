#include "sample_factory.hpp"
#include "dds_util.hpp"

#include <nlohmann/json.hpp>



namespace rtpdds
{

// AnyData → JSON 변환 테이블
std::unordered_map<std::string, SampleToJson> sample_to_json;

void init_sample_to_json()
{
    sample_to_json["StringMsg"] = [](const AnyData& data) {
        const auto& v = std::any_cast<StringMsg>(data);
        return nlohmann::json{{"text", v.text()}};
    };
    sample_to_json["AlarmMsg"] = [](const AnyData& data) {
        const auto& v = std::any_cast<AlarmMsg>(data);
        return nlohmann::json{{"level", v.level()}, {"text", v.text()}};
    };
    sample_to_json["P_Alarms_PSM::C_Actual_Alarm"] = [](const AnyData& data) {
        const auto& v = std::any_cast<P_Alarms_PSM::C_Actual_Alarm>(data);
        nlohmann::json j;
        j["resourceId"] = v.A_sourceID().A_resourceId();
        j["instanceId"] = v.A_sourceID().A_instanceId();
        j["componentName"] = v.A_componentName();
        j["nature"] = v.A_nature();
        j["subsystemName"] = v.A_subsystemName();
        j["measure"] = v.A_measure();
        j["alarmState"] = v.A_alarmState();
        j["dateTimeRaised_sec"] = v.A_dateTimeRaised().A_second();
        j["dateTimeRaised_nsec"] = v.A_dateTimeRaised().A_nanoseconds();
        j["timeOfDataGeneration_sec"] = v.A_timeOfDataGeneration().A_second();
        j["timeOfDataGeneration_nsec"] = v.A_timeOfDataGeneration().A_nanoseconds();
        return j;
    };
}

// payload(텍스트/JSON 등) → AnyData 생성 테이블
std::unordered_map<std::string, SampleFactory> sample_factories;

void init_sample_factories()
{
    using nlohmann::json;

    // StringMsg { string text; }
    sample_factories["StringMsg"] = [](const std::string& s) -> AnyData {
        StringMsg v;
        v.text(s);
        return v;
    };

    // AlarmMsg { long level; string text; }
    sample_factories["AlarmMsg"] = [](const std::string& js) -> AnyData {
        AlarmMsg v;
        try {
            auto j = json::parse(js);
            if (j.contains("level")) {
                v.level(j["level"].get<int>());
            }
            if (j.contains("text")) {
                v.text(j["text"].get<std::string>());
            }
        } catch (...) {
            v.level(0);
            v.text(js);
        }
        return v;
    };

    // P_Alarms_PSM::C_Actual_Alarm
    sample_factories["P_Alarms_PSM::C_Actual_Alarm"] = [](const std::string& js) -> AnyData {
        P_Alarms_PSM::C_Actual_Alarm v;
        try {
            nlohmann::json j = nlohmann::json::parse(js);

            // key
            if (j.contains("sourceId")) {
                auto jsid = j["sourceId"];
                v.A_sourceID().A_resourceId(jsid.value("resourceId", 0));
                v.A_sourceID().A_instanceId(jsid.value("instanceId", 0));
            }

            // times
            if (j.contains("timeOfDataGeneration")) {
                auto jt = j["timeOfDataGeneration"];
                v.A_timeOfDataGeneration().A_second(jt.value("sec", 0LL));
                v.A_timeOfDataGeneration().A_nanoseconds(jt.value("nsec", 0));
            }
            if (j.contains("dateTimeRaised")) {
                auto jt = j["dateTimeRaised"];
                v.A_dateTimeRaised().A_second(jt.value("sec", 0LL));
                v.A_dateTimeRaised().A_nanoseconds(jt.value("nsec", 0));
            }

            // state
            if (j.contains("alarmState")) {
                v.A_alarmState(static_cast<P_Alarms_PSM::T_Actual_Alarm_StateType>(j.value("alarmState", 0)));
            }

            // 문자열 필드들: 길이에 맞춰 공용 처리
            if (j.contains("componentName")) {
                rtpdds::set_bounded_string<20>([&](auto&& s) { v.A_componentName(std::move(s)); },
                                               j.value("componentName", std::string{}));
            }
            if (j.contains("nature")) {
                rtpdds::set_bounded_string<20>([&](auto&& s) { v.A_nature(std::move(s)); },
                                               j.value("nature", std::string{}));
            }
            if (j.contains("subsystemName")) {
                rtpdds::set_bounded_string<20>([&](auto&& s) { v.A_subsystemName(std::move(s)); },
                                               j.value("subsystemName", std::string{}));
            }
            if (j.contains("measure")) {
                rtpdds::set_bounded_string<20>([&](auto&& s) { v.A_measure(std::move(s)); },
                                               j.value("measure", std::string{}));
            }

            // 만약 Medium/Long 이 필요한 필드가 나오면:
            // rtpdds::set_bounded_string<100>([&](auto&& s){ v.A_someMediumField(std::move(s)); }, j.value("...", ""));
            // rtpdds::set_bounded_string<500>([&](auto&& s){ v.A_someLongField  (std::move(s)); }, j.value("...", ""));

        } catch (...) {
            // JSON 파싱 실패 → 기본값 유지
        }
        return v;
    };
}
}  // namespace rtpdds
