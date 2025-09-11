#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>
#include <string>
using json = nlohmann::json;
using Handler = bool(*)(const json&, std::string&);
// 생성됨: 자동 생성 코드. 수정 금지.
void register_handlers(std::unordered_map<std::string, std::pair<Handler,Handler>>& reg);