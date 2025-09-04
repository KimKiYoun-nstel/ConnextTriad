#pragma once
#include <string>
#include <string_view>
#include <algorithm> // std::min, std::copy_n
#include <ndds/hpp/dds/dds.hpp>
#include <rti/core/BoundedSequence.hpp>   // bounded_sequence
#include "LDM_Common.hpp"                 // T_ShortString / T_MediumString / T_LongString

namespace rtpdds {

// 1) 범용: 문자열 -> bounded_sequence<char, N>
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

// 2) 범용: bounded_sequence<char, N> -> std::string (표시/로그용)
template <std::size_t N>
inline std::string to_std_string(const ::rti::core::bounded_sequence<char, N>& b)
{
    return std::string(b.begin(), b.end()); // 널 종료 필요 없음
}

// 3) 네 프로젝트의 IDL alias에 맞춘 얇은 래퍼
inline P_LDM_Common::T_ShortString  make_short_string (std::string_view s)  { return make_bounded_string<20>(s);  }
inline P_LDM_Common::T_MediumString make_medium_string(std::string_view s)  { return make_bounded_string<100>(s); }
inline P_LDM_Common::T_LongString   make_long_string  (std::string_view s)  { return make_bounded_string<500>(s); }

inline std::string to_string(const P_LDM_Common::T_ShortString&  b) { return to_std_string<20>(b);  }
inline std::string to_string(const P_LDM_Common::T_MediumString& b) { return to_std_string<100>(b); }
inline std::string to_string(const P_LDM_Common::T_LongString&   b) { return to_std_string<500>(b); }

// 4) JSON 세터 보조: setter 가 rvalue(…&&)를 받는 경우
template <std::size_t N, typename Setter>
inline void set_bounded_string(Setter&& setter, std::string_view s)
{
    auto tmp = make_bounded_string<N>(s);
    setter(std::move(tmp)); // 세터 시그니처가 T_XXXString&& 를 요구
}

} // namespace rtpdds
