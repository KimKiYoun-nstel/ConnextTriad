/**
 * @file sample_factory.cpp
 * @brief 샘플 변환/생성(AnyData <-> JSON) 및 토픽별 팩토리/변환 테이블 초기화 구현
 *
 * 신규 토픽 타입 추가 시 sample_factories/sample_to_json에 변환 함수를 등록하여 확장할 수 있음.
 * 각 함수별로 파라미터/리턴/비즈니스 처리에 대한 상세 주석을 추가.
 */
#include "sample_factory.hpp"
#include "dds_util.hpp"
#include "Alarms_PSM_enum_utils.hpp"
#include "triad_log.hpp"

#include <nlohmann/json.hpp>


#define SET_STR(SetterExpr, Key, BoundConst) \
    if (j.contains(Key)) rtpdds::set_bounded_string<BoundConst>([&](auto&& s){ SetterExpr(std::move(s)); }, j.value(Key, std::string{}))
#define READ_TIME(Key, Target)  rtpdds::read_time(j, Key, Target)
#define WRITE_TIME(Key, Source) rtpdds::write_time(j, Key, Source)


namespace rtpdds
{

template <class BoundedStr>
inline std::string to_std_string(const BoundedStr& s)
{
    return std::string(s.c_str());
}

// AnyData → JSON 변환 테이블
std::unordered_map<std::string, SampleToJson> sample_to_json;

/**
 * @brief 토픽별 AnyData → JSON 변환 함수 테이블 초기화
 * 신규 토픽 타입 추가 시 이 함수에 변환 람다를 등록
 */
void init_sample_to_json()
{
    sample_to_json["StringMsg"] = [](const AnyData& data) {
        LOG_DBG("SAMPLE", "to_json begin type=StringMsg");
        try {
            const auto& v = std::any_cast<StringMsg>(data);
            return nlohmann::json{{"text", v.text()}};
        } catch (const std::exception& e) {
            LOG_ERR("SAMPLE", "to_json error type=StringMsg msg=%s", e.what());
            return nlohmann::json();
        }
    };
    sample_to_json["AlarmMsg"] = [](const AnyData& data) {
        LOG_DBG("SAMPLE", "to_json begin type=AlarmMsg");
        try {
            const auto& v = std::any_cast<AlarmMsg>(data);
            return nlohmann::json{{"level", v.level()}, {"text", v.text()}};
        } catch (const std::exception& e) {
            LOG_ERR("SAMPLE", "to_json error type=AlarmMsg msg=%s", e.what());
            return nlohmann::json();
        }
    };
    sample_to_json["P_Alarms_PSM::C_Actual_Alarm"] = [](const AnyData& data) {
        LOG_DBG("SAMPLE", "to_json begin type=P_Alarms_PSM::C_Actual_Alarm");
        try {
            const auto& v = std::any_cast<P_Alarms_PSM::C_Actual_Alarm>(data);
            nlohmann::json j;
            rtpdds::write_source_id(j, "sourceId", v.A_sourceID());
            // 추가: rasingCondition, alarmCategory 관련 sourceID
            rtpdds::write_source_id(j, "rasingConditionSourceID", v.A_raisingCondition_sourceID());
            rtpdds::write_source_id(j, "alarmCategorySourceID", v.A_alarmCategory_sourceID());
            j["componentName"] = to_std_string(v.A_componentName());
            j["nature"] = to_std_string(v.A_nature());
            j["subsystemName"] = to_std_string(v.A_subsystemName());
            j["measure"] = to_std_string(v.A_measure());
            j["alarmState"] = rtpdds::P_Alarms_PSM_enum::to_string(v.A_alarmState());
            WRITE_TIME("dateTimeRaised", v.A_dateTimeRaised());
            WRITE_TIME("timeOfDataGeneration", v.A_timeOfDataGeneration());
            return j;
        } catch (const std::exception& e) {
            LOG_ERR("SAMPLE", "to_json error type=P_Alarms_PSM::C_Actual_Alarm msg=%s", e.what());
            return nlohmann::json();
        }
    };
}

// payload(텍스트/JSON 등) → AnyData 생성 테이블
std::unordered_map<std::string, SampleFactory> sample_factories;

/**
 * @brief 토픽별 payload(텍스트/JSON 등) → AnyData 생성 함수 테이블 초기화
 * 신규 토픽 타입 추가 시 이 함수에 변환 람다를 등록
 */
void init_sample_factories()
{
    using nlohmann::json;

    // StringMsg { string text; }
    sample_factories["StringMsg"] = [](const std::string& s) -> AnyData {
        StringMsg v;
        v.text(s); // 단순 텍스트 필드 할당
        return v;
    };

    // AlarmMsg { long level; string text; }
    sample_factories["AlarmMsg"] = [](const std::string& js) -> AnyData {
        AlarmMsg v;
        try {
            auto j = json::parse(js); // JSON 파싱
            if (j.contains("level")) {
                v.level(j["level"].get<int>());
            }
            if (j.contains("text")) {
                v.text(j["text"].get<std::string>());
            }
        } catch (...) {
            // 파싱 실패 시 기본값
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
            rtpdds::read_source_id(j, "sourceId", v.A_sourceID());
            // 추가: rasingCondition, alarmCategory 관련 sourceID
            rtpdds::read_source_id(j, "rasingConditionSourceID", v.A_raisingCondition_sourceID());
            rtpdds::read_source_id(j, "alarmCategorySourceID", v.A_alarmCategory_sourceID());

            // times
            READ_TIME("timeOfDataGeneration", v.A_timeOfDataGeneration());
            READ_TIME("dateTimeRaised", v.A_dateTimeRaised());

            // state
            if (j.contains("alarmState")) {
                const auto& val = j["alarmState"];
                if (val.is_string()) {
                    P_Alarms_PSM::T_Actual_Alarm_StateType st{};
                    if (rtpdds::P_Alarms_PSM_enum::from_string(val.get<std::string>(), st)) v.A_alarmState(st);
                } else if (val.is_number_integer()) {
                    v.A_alarmState(static_cast<P_Alarms_PSM::T_Actual_Alarm_StateType>(val.get<int>()));
                }
            }

            // 문자열 필드들: 길이에 맞춰 변환
            SET_STR(v.A_componentName, "componentName", 20);
            SET_STR(v.A_nature,        "nature",        20);
            SET_STR(v.A_subsystemName, "subsystemName", 20);
            SET_STR(v.A_measure,       "measure",       20);

            // 만약 Medium/Long 이 필요한 필드가 나오면 아래처럼 확장
            // SET_STR(v.A_someMediumField, "...", 100);
            // SET_STR(v.A_someLongField,   "...", 500);

        } catch (...) {
            // JSON 파싱 실패 → 기본값 유지
        }
        return v;
    };
}

}  // namespace rtpdds
