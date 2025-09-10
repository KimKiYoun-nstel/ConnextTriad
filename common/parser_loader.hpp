#pragma once
#include <string>
struct Parser {
  void* h{}; // HMODULE
  int  (*ver)() = nullptr;
  bool (*from_json)(const char*,const char*,char*,size_t*) = nullptr;
  bool (*to_json)(const char*,const char*,char*,size_t*)   = nullptr;
  bool ok() const { return h && ver && from_json && to_json; }
};
Parser load_parser(const char* dll_path);
bool call_from(Parser&, const std::string& type, const std::string& ui,    std::string& out);
bool call_to  (Parser&, const std::string& type, const std::string& canon, std::string& out);
