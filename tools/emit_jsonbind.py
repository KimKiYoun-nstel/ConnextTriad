#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
emit_jsonbind.py  GEN_DIR  XML_DIR

출력:
  GEN_DIR/idl_json_bind.hpp
  GEN_DIR/idl_json_bind.cpp

규칙:
- 모든 struct에 대해 JSON 변환 함수(to_json_X / from_json_X) 생성
- 레지스트리(json_registry)는 C_* 로 시작하는 struct만 등록 (토픽)
- enum은 이름 기반 JSON 변환(문자열). 입력은 문자열 또는 정수 모두 허용
- sequence<...> 포함. 원소가 enum이면 문자열 배열로 직렬화
"""

import sys, pathlib, xml.etree.ElementTree as ET, re

# -------------------- 인자 --------------------
if len(sys.argv) != 3:
    print("usage: emit_jsonbind.py <GEN_DIR> <XML_DIR>", file=sys.stderr)
    sys.exit(2)

GEN = pathlib.Path(sys.argv[1]).resolve()
XML = pathlib.Path(sys.argv[2]).resolve()
out_h = GEN / "idl_json_bind.hpp"
out_c = GEN / "idl_json_bind.cpp"

# -------------------- 기본형 집합 --------------------
PRIMS = {
    "boolean", "bool", "char", "octet",
    "int8", "uint8", "short", "unsigned short",
    "long", "unsigned long", "long long", "unsigned long long",
    "int16", "uint16", "int32", "uint32", "int64", "uint64",
    "float", "double", "string"
}

# -------------------- XML 파서 --------------------
def local(n): return n.tag.split('}')[-1]

def parse_idl(xml_path):
    try:
        root = ET.parse(xml_path).getroot()
    except Exception:
        return [], []  # structs, enums
    structs, enums, stack = [], [], []

    def dfs(n):
        t = local(n)
        if t == "module":
            name = n.attrib.get("name")
            if name:
                stack.append(name)
                for c in n: dfs(c)
                stack.pop()
            else:
                for c in n: dfs(c)
            return

        if t == "enum":
            ename = n.attrib.get("name")
            if ename:
                fqn = "::".join(stack+[ename]) if stack else ename
                items = []
                for c in n:
                    if local(c) == "enumerator":
                        nm = c.attrib.get("name")
                        if nm:
                            items.append(nm)
                enums.append((ename, fqn, items))
            return

        if t == "struct":
            sname = n.attrib.get("name")
            if sname:
                fqn = "::".join(stack+[sname]) if stack else sname
                members = []
                for c in n:
                    if local(c) == "member":
                        mname = c.attrib.get("name")
                        mtype = c.attrib.get("type")
                        if mname and mtype:
                            members.append((mname, mtype))
                structs.append((sname, fqn, members))
            for c in n: dfs(c)
            return

        for c in n: dfs(c)

    dfs(root)
    return structs, enums

# 모든 XML에서 수집
structs_all, enums_all = [], []
for x in sorted(XML.glob("*.xml")):
    s, e = parse_idl(x)
    structs_all.extend(s)
    enums_all.extend(e)

# 중복 제거(순서 유지)
def dedup(seq, key):
    seen=set(); out=[]
    for it in seq:
        k = key(it)
        if k in seen: continue
        seen.add(k); out.append(it)
    return out

structs = dedup(structs_all, key=lambda x: x[1])  # by FQN
enums   = dedup(enums_all,   key=lambda x: x[1])

# 토픽 = C_* struct
topic_set = {fqn for name, fqn, _ in structs if name.startswith("C_")}

# enum 판정용 이름 집합(FQN + 말단명)
enum_fqn_set  = {fqn for _, fqn, _ in enums}
enum_leaf_set = {fqn.split("::")[-1] for fqn in enum_fqn_set}

# -------------------- 타입 유틸 --------------------
seq_re = re.compile(r'^\s*sequence\s*<\s*(.+?)\s*>\s*$')

def is_seq(t: str): return bool(seq_re.match(t))
def elem_of_seq(t: str):
    m = seq_re.match(t); return m.group(1) if m else None
def is_prim(t: str): return t in PRIMS

def is_enum_type(t: str):
    # 완전일치(FQN) 또는 말단명 일치 지원
    if t in enum_fqn_set: return True
    leaf = t.split("::")[-1]
    return leaf in enum_leaf_set

def enum_tag(t: str):
    # 코드 심볼 생성에 사용
    return t.replace("::","_")

def resolve_enum_fqn(t: str):
    if t in enum_fqn_set:
        return t
    leaf = t.split("::")[-1]
    cands = [fqn for fqn in enum_fqn_set if fqn.endswith("::"+leaf) or fqn == leaf]
    return cands[0] if cands else t  # 없으면 원문 반환

# -------------------- C++ 코드 조각 생성 --------------------
def to_cpp_expr_write(member_expr, mtype, j_lhs):
    """C++ -> JSON"""
    if is_seq(mtype):
        et = elem_of_seq(mtype)
        if is_enum_type(et):
            et_fqn = resolve_enum_fqn(et); tag = enum_tag(et_fqn)
            return f"""{j_lhs} = nlohmann::json::array();
for (const auto& _e : {member_expr}) {{
  nlohmann::json _tmp;
  if (!enum_to_json_{tag}(&_e, _tmp)) return false;
  {j_lhs}.push_back(std::move(_tmp));
}}"""
        if is_prim(et):
            return f"""{j_lhs} = nlohmann::json::array();
for (const auto& _e : {member_expr}) {{ {j_lhs}.push_back(_e); }}"""
        # nested struct
        return f"""{j_lhs} = nlohmann::json::array();
for (const auto& _e : {member_expr}) {{
  nlohmann::json _tmp;
  if (!to_json_{et.replace('::','_')}(&_e, _tmp)) return false;
  {j_lhs}.push_back(std::move(_tmp));
}}"""

    if is_enum_type(mtype):
        efqn = resolve_enum_fqn(mtype); tag = enum_tag(efqn)
        return f"""{{ nlohmann::json _tmp; if (!enum_to_json_{tag}(&{member_expr}, _tmp)) return false; {j_lhs} = std::move(_tmp); }}"""

    if is_prim(mtype):
        return f"{j_lhs} = {member_expr};"

    # nested struct
    return f"if (!to_json_{mtype.replace('::','_')}(&{member_expr}, {j_lhs})) return false;"

def to_cpp_expr_read(member_expr, mtype, j_expr):
    """JSON -> C++"""
    if is_seq(mtype):
        et = elem_of_seq(mtype)
        if is_enum_type(et):
            et_fqn = resolve_enum_fqn(et); tag = enum_tag(et_fqn)
            return f"""{member_expr}.clear();
for (auto& _e : {j_expr}) {{
  typename decltype({member_expr})::value_type _tmp{{}};
  if (!enum_from_json_{tag}(&_tmp, _e)) return false;
  {member_expr}.push_back(_tmp);
}}"""
        if is_prim(et):
            return f"""{member_expr}.clear();
for (auto& _e : {j_expr}) {{ {member_expr}.push_back(_e.get<decltype({member_expr})::value_type>()); }}"""
        # nested struct
        return f"""{member_expr}.clear();
for (auto& _e : {j_expr}) {{
  typename decltype({member_expr})::value_type _tmp;
  if (!from_json_{et.replace('::','_')}(&_tmp, _e)) return false;
  {member_expr}.push_back(std::move(_tmp));
}}"""

    if is_enum_type(mtype):
        efqn = resolve_enum_fqn(mtype); tag = enum_tag(efqn)
        return f"if (!enum_from_json_{tag}(&{member_expr}, {j_expr})) return false;"

    if is_prim(mtype):
        return f"{member_expr} = {j_expr}.get<decltype({member_expr})>();"

    # nested struct
    return f"if (!from_json_{mtype.replace('::','_')}(&{member_expr}, {j_expr})) return false;"

# -------------------- 헤더 생성 --------------------
hdr = """#pragma once
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace idlmeta {

struct JsonOps {
  bool (*to_json)(const void* sample, nlohmann::json& out) noexcept;
  bool (*from_json)(void* sample, const nlohmann::json& in) noexcept;
};

// 레지스트리는 C_* 토픽만
const std::unordered_map<std::string, JsonOps>& json_registry() noexcept;

} // namespace idlmeta
"""
out_h.write_text(hdr, encoding="utf-8")

# -------------------- 구현 생성 --------------------
src = []
src.append('#include "idl_json_bind.hpp"')
# GEN 디렉토리 HPP 전부 포함(타입 가시화)
for h in sorted([p.name for p in GEN.glob("*.hpp")]):
    src.append(f'#include "{h}"')
src += [
    "#include <unordered_map>",
    "#include <type_traits>",
    "#include <string>",
]

# 1) enum helpers
for ename, efqn, items in enums:
    tag = enum_tag(efqn)
    # to_json: enum -> string (미정의 값은 숫자)
    src.append(f"static bool enum_to_json_{tag}(const void* vp, nlohmann::json& j) noexcept {{")
    src.append(f"  try {{")
    src.append(f"    auto v = *static_cast<const {efqn}*>(vp);")
    if items:
        src.append("    switch (v) {")
        for nm in items:
            src.append(f'      case {efqn}::{nm}: j = std::string("{nm}"); return true;')
        src.append("      default: j = static_cast<int>(v); return true;")
        src.append("    }")
    else:
        src.append("    j = static_cast<int>(v);")
        src.append("    return true;")
    src.append(f"  }} catch (...) {{ return false; }}")
    src.append("}")

    # from_json: string|int -> enum
    src.append(f"static bool enum_from_json_{tag}(void* vp, const nlohmann::json& j) noexcept {{")
    src.append("  try {")
    src.append(f"    auto& v = *static_cast<{efqn}*>(vp);")
    src.append("    if (j.is_string()) {")
    src.append("      const auto s = j.get<std::string>();")
    for nm in items:
        src.append(f'      if (s == "{nm}") {{ v = {efqn}::{nm}; return true; }}')
    src.append("      return false;")
    src.append("    }")
    src.append("    if (j.is_number_integer()) { v = static_cast<"+efqn+">(j.get<int>()); return true; }")
    src.append("    return false;")
    src.append("  } catch (...) { return false; }")
    src.append("}")

# 2) struct converters
for sname, fqn, members in structs:
    T = fqn
    tag = fqn.replace("::","_")

    # to_json
    src.append(f"static bool to_json_{tag}(const void* vp, nlohmann::json& j) noexcept {{")
    src.append("  try {")
    src.append(f"    const {T}& v = *static_cast<const {T}*>(vp);")
    src.append("    j = nlohmann::json::object();")
    for mname, mtype in members:
        jlhs = f'j["{mname}"]'
        src.append("    {")
        src.append("      " + to_cpp_expr_write(f"v.{mname}", mtype, jlhs))
        src.append("    }")
    src.append("    return true;")
    src.append("  } catch (...) { return false; }")
    src.append("}")

    # from_json
    src.append(f"static bool from_json_{tag}(void* vp, const nlohmann::json& j) noexcept {{")
    src.append("  try {")
    src.append(f"    {T}& v = *static_cast<{T}*>(vp);")
    for mname, mtype in members:
        src.append(f'    if (j.contains("{mname}")) {{')
        src.append("      " + to_cpp_expr_read(f"v.{mname}", mtype, f'j.at("{mname}")'))
        src.append("    }")
    src.append("    return true;")
    src.append("  } catch (...) { return false; }")
    src.append("}")

# 3) 레지스트리(C_*만)
src.append("static const std::unordered_map<std::string, idlmeta::JsonOps> kJsonReg = {")
for sname, fqn, _ in structs:
    if fqn in topic_set:
        tag = fqn.replace("::","_")
        src.append(f'  {{ "{fqn}", {{ &to_json_{tag}, &from_json_{tag} }} }},')
src.append("};")

src.append("""
namespace idlmeta {
const std::unordered_map<std::string, JsonOps>& json_registry() noexcept { return kJsonReg; }
} // namespace idlmeta
""".strip())

# -------------------- 파일 출력 --------------------
out_h.write_text(hdr, encoding="utf-8")
out_c.write_text("\n".join(src), encoding="utf-8")
print(f"generated: {out_h.name}, {out_c.name} | structs={len(structs)} enums={len(enums)} topics={len(topic_set)}")
