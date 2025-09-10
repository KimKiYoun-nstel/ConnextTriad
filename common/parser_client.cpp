#include "parser_client.hpp"
#ifdef PARSER_LINK_STATIC
#include "parser_plugin.h"
bool parser_from(const std::string& t, const std::string& in, std::string& out)
{
    size_t need = 0;
    if (!parser_from_json(t.c_str(), in.c_str(), nullptr, &need) || need == 0) return false;
    out.resize(need);
    return parser_from_json(t.c_str(), in.c_str(), out.data(), &need);
}
bool parser_to(const std::string& t, const std::string& in, std::string& out)
{
    size_t need = 0;
    if (!parser_to_json(t.c_str(), in.c_str(), nullptr, &need) || need == 0) return false;
    out.resize(need);
    return parser_to_json(t.c_str(), in.c_str(), out.data(), &need);
}
#else
#include "parser_loader.hpp"
static Parser& H()
{
    static Parser p = load_parser("./parsers/ngva_parser.dll");
    return p;
}
bool parser_from(const std::string& t, const std::string& in, std::string& out)
{
    auto& p = H();
    return p.ok() && call_from(p, t, in, out);
}
bool parser_to(const std::string& t, const std::string& in, std::string& out)
{
    auto& p = H();
    return p.ok() && call_to(p, t, in, out);
}
#endif
