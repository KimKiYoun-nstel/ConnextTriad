#pragma once
#include <cstring>
#include <string>

#include <ndds/ndds_cpp.h>
#include <nlohmann/json.hpp>

namespace rtpdds
{

// === 공용 유틸: DDS sequence<char> ↔ std::string / JSON ===

inline void assign_string(DDS_CharSeq& seq, const std::string& s)
{
    DDS_Long n = static_cast<DDS_Long>(s.size());
    seq.length(n);
    if (n > 0) {
        std::memcpy(seq.get_contiguous_buffer(), s.data(), n);
    }
}

inline std::string to_string(const DDS_CharSeq& seq)
{
    if (seq.length() == 0) return {};
    return std::string(seq.get_contiguous_buffer(), seq.get_contiguous_buffer() + seq.length());
}

// JSON → DDS_CharSeq (필드 없을 때 안전 기본값)
inline void assign_string(DDS_CharSeq& seq, const nlohmann::json& j, const char* key)
{
    if (j.contains(key)) {
        auto s = j.value(key, std::string{});
        assign_string(seq, s);
    }
}

// DDS_CharSeq → JSON (필드에 문자열로 삽입)
inline void to_json(nlohmann::json& j, const char* key, const DDS_CharSeq& seq)
{
    j[key] = to_string(seq);
}

}  // namespace rtpdds
