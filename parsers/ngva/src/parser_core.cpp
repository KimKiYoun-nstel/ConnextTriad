#include "parser_core.hpp"
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>

using json = nlohmann::json;
using Handler = bool(*)(const json&, std::string&);

// 생성물이 제공하는 선언(빌드시 generated/에 생성)
#include "../generated/parser_core_gen.hpp"

namespace {
std::unordered_map<std::string, std::pair<Handler,Handler>> g_handlers;
inline bool ensure_handlers() {
    if (g_handlers.empty()) register_handlers(g_handlers);
    return !g_handlers.empty();
}
} // namespace

namespace ngva {

bool from_json(const std::string& type_name, const std::string& ui_json, std::string& out_json) {
    if (!ensure_handlers()) return false;
    auto it = g_handlers.find(type_name);
    if (it == g_handlers.end()) return false;
    json j = json::parse(ui_json, nullptr, /*allow_exceptions*/false);
    if (j.is_discarded()) return false;
    return it->second.first(j, out_json);
}

bool to_json(const std::string& type_name, const std::string& canonical_json, std::string& out_json) {
    if (!ensure_handlers()) return false;
    auto it = g_handlers.find(type_name);
    if (it == g_handlers.end()) return false;
    json j = json::parse(canonical_json, nullptr, /*allow_exceptions*/false);
    if (j.is_discarded()) return false;
    return it->second.second(j, out_json);
}

int api_version() { return 1; }

} // namespace ngva
