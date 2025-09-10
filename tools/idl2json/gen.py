# tools/idl2json/gen.py
import argparse, os, pathlib, textwrap

STUB_HPP = r'''
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
'''

STUB_CPP = r'''
#include "parser_core_gen.hpp"
// 구현은 헤더 인라인로 충분(스텁)
'''

ENUM_HPP = r'''
#pragma once
// enum tables placeholder. gen.py가 실제 테이블을 채움
'''

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--idl', nargs='*', default=[])
    ap.add_argument('--discover', action='store_true')
    ap.add_argument('--include', nargs='*', default=[])
    ap.add_argument('--exclude', nargs='*', default=[])
    ap.add_argument('--out', required=True)
    args = ap.parse_args()

    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    (out / 'parser_core_gen.hpp').write_text(STUB_HPP, encoding='utf-8')
    (out / 'parser_core_gen.cpp').write_text(STUB_CPP, encoding='utf-8')
    (out / 'enum_tables_gen.hpp').write_text(ENUM_HPP, encoding='utf-8')

if __name__ == '__main__':
    main()
