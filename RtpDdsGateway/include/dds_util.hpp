#pragma once
/**
 * @file dds_util.hpp
 * @brief DDS 타입과 JSON/문자열 변환, bounded string, 시간/ID 변환 등 공통 유틸리티 제공
 *
 * IDL에서 정의된 bounded string 타입을 안전하게 변환하거나, 시간/식별자 필드의 직렬화/역직렬화, JSON 변환 등
 * 데이터 변환 일관성을 보장하는 핵심 유틸리티 함수들을 제공합니다.
 *
 * 연관 파일:
 *   - sample_factory.hpp (샘플 변환/생성)
 *   - dds_manager.hpp (엔티티 관리)
 *   - Alarms_PSM_enum_utils.hpp (enum 변환)
 */
#include <string>
#include <string_view>
#include <algorithm> // std::min, std::copy_n
#include <dds/dds.hpp>
#include <rti/core/BoundedSequence.hpp>   // bounded_sequence
#include "LDM_Common.hpp"                 // T_ShortString / T_MediumString / T_LongString
#include <nlohmann/json.hpp>

namespace rtpdds {

/**
 * @brief std::string_view → bounded_sequence<char, N> 변환
 * @tparam N 최대 길이
 * @param s 입력 문자열
 * @return bounded_sequence<char, N> (IDL에서 정의된 bounded string 타입)
 */
template <std::size_t N>
inline ::rti::core::bounded_sequence<char, N>
make_bounded_string(std::string_view s)
{
    ::rti::core::bounded_sequence<char, N> out;
    const std::size_t n = std::min<std::size_t>(s.size(), N);
    out.resize(n);
    // iterator 기반 복사 (data() 없어도 동작)
    std::copy_n(s.begin(), n, out.begin());
    return out; // RVO; rvalue 필요 시 호출부에서 std::move(...)
}

/**
 * @brief bounded_sequence<char, N> → std::string 변환 (표시/로그용)
 * @tparam N 최대 길이
 * @param b bounded_sequence 객체
 * @return std::string 변환 결과
 */
template <std::size_t N>
inline std::string to_std_string(const ::rti::core::bounded_sequence<char, N>& b)
{
    return std::string(b.begin(), b.end()); // 널 종료 필요 없음
}

// IDL alias에 맞춘 bounded string 변환 래퍼
inline P_LDM_Common::T_ShortString  make_short_string (std::string_view s)  { return make_bounded_string<20>(s);  }
inline P_LDM_Common::T_MediumString make_medium_string(std::string_view s)  { return make_bounded_string<100>(s); }
inline P_LDM_Common::T_LongString   make_long_string  (std::string_view s)  { return make_bounded_string<500>(s); }

inline std::string to_string(const P_LDM_Common::T_ShortString&  b) { return to_std_string<20>(b);  }
inline std::string to_string(const P_LDM_Common::T_MediumString& b) { return to_std_string<100>(b); }
inline std::string to_string(const P_LDM_Common::T_LongString&   b) { return to_std_string<500>(b); }

/**
 * @brief JSON 세터 보조: setter가 rvalue(…&&)를 받는 경우에 사용
 * @tparam N 최대 길이
 * @tparam Setter 세터 함수 타입
 * @param setter 세터 람다/함수
 * @param s 입력 문자열
 */
template <std::size_t N, typename Setter>
inline void set_bounded_string(Setter&& setter, std::string_view s)
{
    auto tmp = make_bounded_string<N>(s);
    setter(std::move(tmp)); // 세터 시그니처가 T_XXXString&& 를 요구
}

/**
 * @brief 시간 필드(DateTimeType)를 JSON에 기록
 * @param j JSON 객체
 * @param key 키 이름
 * @param t 시간 값
 */
void write_time(nlohmann::json& j, const char* key, const P_LDM_Common::T_DateTimeType& t);
/**
 * @brief JSON에서 시간 필드(DateTimeType) 읽기
 * @param j JSON 객체
 * @param key 키 이름
 * @param t 결과 저장 대상
 */
void read_time(const nlohmann::json& j, const char* key, P_LDM_Common::T_DateTimeType& t);
/**
 * @brief 식별자 필드(IdentifierType)를 JSON에 기록
 */
void write_source_id(nlohmann::json& j, const P_LDM_Common::T_IdentifierType& sid);
/**
 * @brief JSON에서 식별자 필드(IdentifierType) 읽기
 */
void read_source_id(const nlohmann::json& j, P_LDM_Common::T_IdentifierType& sid);

} // namespace rtpdds
