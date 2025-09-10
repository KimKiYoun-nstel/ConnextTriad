#include "parser_plugin.h"
#include "parser_core.hpp"
#include <cstring>

#ifdef _WIN32
#undef PARSER_API
#if defined(PARSER_BUILD_SHARED)
#define PARSER_API __declspec(dllexport)
#else
#define PARSER_API
#endif
#endif


extern "C" {
PARSER_API int  parser_api_version() { return ngva::api_version(); }

PARSER_API bool parser_from_json(const char* type_name, const char* ui_json,
                                 char* out_json, size_t* inout_len) {
  if (!inout_len || !type_name || !ui_json) return false;
  std::string out;
  if (!ngva::from_json(type_name, ui_json, out)) return false;
  if (!out_json) { *inout_len = out.size()+1; return true; }
  if (*inout_len < out.size()+1) return false;
  std::memcpy(out_json, out.c_str(), out.size()+1);
  *inout_len = out.size()+1;
  return true;
}

PARSER_API bool parser_to_json(const char* type_name, const char* canonical_json,
                               char* out_json, size_t* inout_len) {
  if (!inout_len || !type_name || !canonical_json) return false;
  std::string out;
  if (!ngva::to_json(type_name, canonical_json, out)) return false;
  if (!out_json) { *inout_len = out.size()+1; return true; }
  if (*inout_len < out.size()+1) return false;
  std::memcpy(out_json, out.c_str(), out.size()+1);
  *inout_len = out.size()+1;
  return true;
}
}
