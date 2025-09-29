#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 사용: emit_autoreg.py <GEN_DIR> <XML_DIR>
# 산출:
#   GEN_DIR/idl_type_registry.hpp
#   GEN_DIR/idl_type_registry.cpp          # (IdlKit에서 컴파일)
#   GEN_DIR/idl_install_factories.cpp      # (Gateway에서 컴파일)

import sys, pathlib, xml.etree.ElementTree as ET

if len(sys.argv) != 3:
    print("usage: emit_autoreg.py <GEN_DIR> <XML_DIR>", file=sys.stderr); sys.exit(2)
GEN = pathlib.Path(sys.argv[1]).resolve()
XML = pathlib.Path(sys.argv[2]).resolve()

out_h = GEN/"idl_type_registry.hpp"
out_c = GEN/"idl_type_registry.cpp"
out_install = GEN/"idl_install_factories.cpp"

def collect_structs(xml_path):
    try:
        root = ET.parse(xml_path).getroot()
    except Exception:
        return []
    out, stack = [], []
    def dfs(n):
        tag = n.tag.split('}')[-1]
        if tag == "module":
            name = n.attrib.get("name")
            if name: stack.append(name); [dfs(ch) for ch in n]; stack.pop(); return
        if tag == "struct":
            name = n.attrib.get("name")
            if name:
                fqn = "::".join(stack+[name]) if stack else name
                out.append((name, fqn))
            [dfs(ch) for ch in n]; return
        [dfs(ch) for ch in n]
    dfs(root); return out

# 모든 XML에서 struct 수집
all_structs = []
for x in sorted(XML.glob("*.xml")):
    all_structs.extend(collect_structs(x))

# 토픽 = 이름이 C_ 로 시작
topic_fqns = []
seen=set()
for name, fqn in all_structs:
    if not name.startswith("C_"):
        continue
    if fqn in seen: continue
    seen.add(fqn); topic_fqns.append(fqn)

# GEN의 모든 헤더 포함(심볼 가시화)
hpp_list = sorted([p.name for p in GEN.glob("*.hpp")])

# ---------- idl_generated_includes.hpp (umbrella) ----------
# Create a single header that includes all rtiddsgen-generated headers so
# other generated sources (and external consumers) can include one file.
includes_hdr = GEN / "idl_generated_includes.hpp"
incs = ['#pragma once', '/* auto-generated: umbrella includes for IDL-generated headers */']
for h in hpp_list:
    # avoid self-include if script is re-run
    if h == includes_hdr.name:
        continue
    incs.append(f'#include "{h}"')
includes_hdr.write_text("\n".join(incs), encoding="utf-8")

# ---------- idl_type_registry.hpp ----------
hdr = """#pragma once
#include <string>
#include <unordered_map>

namespace idlmeta {

struct TypeOps {
  const char* name;
  void* (*create)() noexcept;
  void  (*destroy)(void*) noexcept;
};

// 토픽 타입(C_*)만 등록됨
const std::unordered_map<std::string, TypeOps>& type_registry() noexcept;

} // namespace idlmeta
"""
out_h.write_text(hdr, encoding="utf-8")

# ---------- idl_type_registry.cpp ----------
src = []
src.append('#include "idl_type_registry.hpp"')
# include the umbrella header instead of listing every generated hpp
src.append('#include "idl_generated_includes.hpp"')
src.append('#include <unordered_map>')

for i, T in enumerate(topic_fqns):
    src.append(f"static void* create_{i}() noexcept {{ return new {T}(); }}")
    src.append(f"static void  destroy_{i}(void* p) noexcept {{ delete static_cast<{T}*>(p); }}")

src.append("static const std::unordered_map<std::string, idlmeta::TypeOps> kReg = {")
for i, T in enumerate(topic_fqns):
    src.append(f'  {{ "{T}", {{ "{T}", &create_{i}, &destroy_{i} }} }},')
src.append("};")

src.append("""
namespace idlmeta {
const std::unordered_map<std::string, TypeOps>& type_registry() noexcept { return kReg; }
} // namespace idlmeta
""".strip())
out_c.write_text("\n".join(src), encoding="utf-8")

# ---------- idl_install_factories.cpp ----------
install = []
install.append('#include "dds_type_registry.hpp"')  # rtpdds::register_dds_type<T>()
# include umbrella header instead of per-header includes
install.append('#include "idl_generated_includes.hpp"')
install.append("namespace idlmeta {\nvoid install_factories() noexcept {")
for T in topic_fqns:
    install.append(f'  rtpdds::register_dds_type<{T}>("{T}");')
install.append("}\n} // namespace idlmeta\n")
out_install.write_text("\n".join(install), encoding="utf-8")

print(f"generated: {out_h.name}, {out_c.name}, {out_install.name} | topics={len(topic_fqns)}")
