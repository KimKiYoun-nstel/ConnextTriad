#include "parser_loader.hpp"
#include "parser_plugin.h"
#include <vector>
#ifdef _WIN32
  #include <windows.h>
#endif

static bool call_with_buf(bool (*fn)(const char*,const char*,char*,size_t*),
                          const std::string& type, const std::string& in, std::string& out) {
  size_t need = 0;
  if (!fn(type.c_str(), in.c_str(), nullptr, &need) || need==0) return false;
  std::vector<char> buf(need);
  if (!fn(type.c_str(), in.c_str(), buf.data(), &need)) return false;
  out.assign(buf.data(), need);
  return true;
}

Parser load_parser(const char* dll_path) {
  Parser p{};
#ifdef _WIN32
  HMODULE h = LoadLibraryA(dll_path);
  if (!h) return p;
  p.h = h;
  p.ver       = (int(*)())           GetProcAddress(h, "parser_api_version");
  p.from_json = (bool(*)(const char*,const char*,char*,size_t*))GetProcAddress(h, "parser_from_json");
  p.to_json   = (bool(*)(const char*,const char*,char*,size_t*))GetProcAddress(h, "parser_to_json");
#endif
  return p;
}

bool call_from(Parser& p, const std::string& t, const std::string& ui,    std::string& out){ return p.ok() && call_with_buf(p.from_json, t, ui, out); }
bool call_to  (Parser& p, const std::string& t, const std::string& canon, std::string& out){ return p.ok() && call_with_buf(p.to_json,   t, canon, out); }
