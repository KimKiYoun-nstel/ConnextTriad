#pragma once
#include <string>

namespace ngva {
bool from_json(const std::string& type_name,
               const std::string& ui_json,
               std::string& out_canonical_json);

bool to_json(const std::string& type_name,
             const std::string& canonical_json,
             std::string& out_ui_json);

int api_version();
} // namespace ngva
