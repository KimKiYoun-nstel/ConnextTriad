/**
 * @file dds_util.cpp
 * @brief DDS 샘플과 JSON 간 변환, 시간/ID 변환 등 공통 유틸리티 함수 구현
 *
 * 각 함수별로 파라미터/리턴/비즈니스 처리에 대한 상세 주석을 추가.
 */
#include "dds_util.hpp"
#include <iomanip>
#include <sstream>
#include <chrono>

namespace rtpdds {

/**
 * @brief ISO8601 문자열을 std::chrono::system_clock::time_point로 변환
 * @param iso8601 ISO8601 형식의 문자열
 * @return 변환된 time_point. 파싱 실패 시 epoch 반환
 */
std::chrono::system_clock::time_point parse_iso8601(const std::string& iso8601) {
    std::istringstream ss(iso8601);
    std::tm dt = {};
    ss >> std::get_time(&dt, "%Y-%m-%dT%H:%M:%S");
    if (ss.fail()) {
        // 파싱 실패 시 epoch 반환
        return std::chrono::system_clock::time_point{};
    }
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&dt));
    return tp;
}

/**
 * @brief std::chrono::system_clock::time_point를 ISO8601 문자열로 변환
 * @param tp 변환할 time_point
 * @return ISO8601 형식의 문자열
 */
std::string to_iso8601(const std::chrono::system_clock::time_point& tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return std::string(buf);
}

/**
 * @brief UUID-like 문자열 생성 (임시 ID 생성용)
 * @return 랜덤한 16진수 문자열
 */
std::string generate_id() {
    static int counter = 0;
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << std::rand();
    oss << std::setw(8) << std::setfill('0') << counter++;
    return oss.str();
}

/**
 * @brief DDS 시간 형식을 JSON에 기록
 * @param j JSON 객체
 * @param key JSON 키
 * @param t DDS 시간 객체
 */
void write_time(nlohmann::json& j, const char* key, const P_LDM_Common::T_DateTimeType& t) {
    j[key] = { {"sec", t.A_second()}, {"nsec", t.A_nanoseconds()} };
}

/**
 * @brief JSON에서 DDS 시간 형식으로 읽기
 * @param j JSON 객체
 * @param key JSON 키
 * @param t DDS 시간 객체
 */
void read_time(const nlohmann::json& j, const char* key, P_LDM_Common::T_DateTimeType& t) {
    if (j.contains(key)) {
        auto jt = j.at(key);
        t.A_second(jt.value("sec", 0LL));
        t.A_nanoseconds(jt.value("nsec", 0));
    } else {
        std::string s = std::string(key);
        t.A_second(j.value(s + "_sec", 0LL));
        t.A_nanoseconds(j.value(s + "_nsec", 0));
    }
}

/**
 * @brief DDS 소스 ID 형식을 JSON에 기록
 * @param j JSON 객체
 * @param sid DDS 식별자 객체
 */
void write_source_id(nlohmann::json& j, const P_LDM_Common::T_IdentifierType& sid)
{
    j["sourceId"] = { {"resourceId", sid.A_resourceId()}, {"instanceId", sid.A_instanceId()} };
}

/**
 * @brief JSON에서 DDS 소스 ID 형식으로 읽기
 * @param j JSON 객체
 * @param sid DDS 식별자 객체
 */
void read_source_id(const nlohmann::json& j, P_LDM_Common::T_IdentifierType& sid)
{
    if (j.contains("sourceId")) {
        auto jsid = j["sourceId"];
        sid.A_resourceId(jsid.value("resourceId", 0));
        sid.A_instanceId(jsid.value("instanceId", 0));
    } else {
        sid.A_resourceId(j.value("resourceId", 0));
        sid.A_instanceId(j.value("instanceId", 0));
    }
}

} // namespace rtpdds
