
#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
using json = nlohmann::json;
using Handler = bool(*)(const json&, std::string&);

// 비어있는 핸들러(스텁). 실제 제너레이터가 채울 예정.
inline bool __stub_handle(const json& in, std::string& out) {
    if (!in.is_object()) return false;
    out = in.dump(); return true;
}

// 생성물이 제공해야 하는 등록 함수
inline void register_handlers(std::unordered_map<std::string, std::pair<Handler,Handler>>& reg) {
    // TODO: gen.py가 자동으로 채움
}
