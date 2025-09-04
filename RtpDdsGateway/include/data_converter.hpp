#pragma once
#if 0
#include <string>

#include "dds_util.hpp"
#include "type_traits.hpp"

namespace rtpdds
{

template <typename T>
struct DataConverter;

template <>
struct DataConverter<StringMsg> {
    static StringMsg from_text(const std::string& text)
    {
        StringMsg m{};
        m.text = (char*)DDS_String_dup(text.c_str());
        return m;
    }
    static std::string to_display(const StringMsg& m)
    {
        return std::string(m.text ? m.text : "");
    }
    static void cleanup(StringMsg& m)
    {
        if (m.text) {
            DDS_String_free(m.text);
            m.text = nullptr;
        }
    }
};

template <>
struct DataConverter<AlarmMsg> {
    static AlarmMsg from_text(const std::string& text)
    {
        AlarmMsg m{};
        m.level = 1;
        m.text = (char*)DDS_String_dup(text.c_str());
        return m;
    }
    static std::string to_display(const AlarmMsg& m)
    {
        return std::string("Level: ") + std::to_string(m.level) + ", Text: " + (m.text ? m.text : "");
    }
    static void cleanup(AlarmMsg& m)
    {
        if (m.text) {
            DDS_String_free(m.text);
            m.text = nullptr;
        }
    }
};

template <>
struct DataConverter<P_Alarms_PSM_C_Actual_Alarm> {
    static P_Alarms_PSM_C_Actual_Alarm from_text(const std::string& text)
    {
        // 안전하게 생성
        P_Alarms_PSM_C_Actual_Alarm* tmp = P_Alarms_PSM_C_Actual_AlarmTypeSupport::create_data();

        // 기본은 빈 객체
        P_Alarms_PSM_C_Actual_Alarm out(*tmp);
        P_Alarms_PSM_C_Actual_AlarmTypeSupport::delete_data(tmp);

        try {
            nlohmann::json j = nlohmann::json::parse(text);

            // key
            if (j.contains("sourceId")) {
                auto js = j["sourceId"];
                out.A_sourceID.A_resourceId = js.value("resourceId", 0);
                out.A_sourceID.A_instanceId = js.value("instanceId", 0);
            }

            // times
            if (j.contains("timeOfDataGeneration")) {
                auto jt = j["timeOfDataGeneration"];
                out.A_timeOfDataGeneration.A_second = jt.value("sec", 0LL);
                out.A_timeOfDataGeneration.A_nanoseconds = jt.value("nsec", 0);
            }
            if (j.contains("dateTimeRaised")) {
                auto jt = j["dateTimeRaised"];
                out.A_dateTimeRaised.A_second = jt.value("sec", 0LL);
                out.A_dateTimeRaised.A_nanoseconds = jt.value("nsec", 0);
            }

            // state
            if (j.contains("alarmState")) {
                out.A_alarmState = static_cast<P_Alarms_PSM_T_Actual_Alarm_StateType>(j.value("alarmState", 0));
            }

            // 문자열 필드들은 필요시 복사 (bounded sequence<char>)
            // P_LDM_Common_T_ShortString은 DDS_CharSeq로, DDS_String_replace가 아닌 직접 복사 필요
            assign_string(out.A_componentName, j, "componentName");
            assign_string(out.A_nature, j, "nature");
            assign_string(out.A_subsystemName, j, "subsystemName");
            assign_string(out.A_measure, j, "measure");
        } catch (...) {
            // JSON 파싱 실패 → 기본값 유지
        }

        return out;  // 값 반환
    }

    static std::string to_display(const P_Alarms_PSM_C_Actual_Alarm& m)
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "ActualAlarm{src=(%d,%d), state=%d}", (int)m.A_sourceID.A_resourceId,
                      (int)m.A_sourceID.A_instanceId, (int)m.A_alarmState);
        return std::string(buf);
    }

    static void cleanup(P_Alarms_PSM_C_Actual_Alarm& /*m*/)
    {
        // 값으로 쓰이므로 여기서는 특별히 해줄 필요 없음.
        // (내부 문자열은 DDS runtime이 관리)
    }
};

}  // namespace rtpdds
#endif
