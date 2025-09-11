#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
IDL→JSON 파서 코드 제너레이터(초판)
- 외부 패키지 불필요. Python3 표준 라이브러리만 사용.
- 출력: parsers/ngva/generated/
    - parser_core_gen.hpp
    - parser_core_gen.cpp
    - enum_tables_gen.hpp
- 규칙: 필드명=IDL 그대로, 중첩=IDL 구조, enum=토큰 문자열, 여분 키 금지, @optional 허용, 길이/범위 검증.
"""
import argparse, pathlib, re, sys, textwrap, json
from fnmatch import fnmatch

# ---------- 유틸 ----------
DEF_INT = {
    'short':      ('int16',  -2**15, 2**15-1),
    'unsigned short': ('uint16', 0, 2**16-1),
    'long':       ('int32',  -2**31, 2**31-1),
    'unsigned long': ('uint32', 0, 2**32-1),
    'long long':  ('int64',  -2**63, 2**63-1),
    'unsigned long long': ('uint64', 0, 2**64-1),
}
PRIMS = {
    **{k:v[0] for k,v in DEF_INT.items()},
    'float':  'float32',
    'double': 'float64',
    'boolean':'bool',
    'char':   'char',
    'octet':  'uint8',
    'string': 'string',
}

WS = re.compile(r"\s+")
C_COMMENT = re.compile(r"/\*.*?\*/", re.S)
CPP_COMMENT = re.compile(r"//.*?$", re.M)

STRUCT_RE = re.compile(r"struct\s+(\w+)\s*\{(.*?)\};", re.S)
ENUM_RE   = re.compile(r"enum\s+(\w+)\s*\{(.*?)\};", re.S)
TYPEDEF_RE= re.compile(r"typedef\s+([^;]+?)\s+(\w+)\s*;", re.S)
MODULE_RE = re.compile(r"module\s+(\w+)\s*\{")

SEQUENCE_RE = re.compile(r"sequence\s*<\s*([^,>]+)\s*(?:,\s*(\d+)\s*)?>")

class Field:
    def __init__(self, name, type_str, optional=False):
        self.name=name; self.type_str=type_str; self.optional=optional
        # resolved
        self.kind=None; self.max_len=None; self.enum_name=None; self.elem=None; self.ref=None

class Struct:
    def __init__(self, fqn):
        self.fqn=fqn; self.fields=[]
        self.func=fqn.replace('::','__')

class Enum:
    def __init__(self, fqn, tokens):
        self.fqn=fqn; self.tokens=[t.strip() for t in tokens if t.strip()]

class Typedef:
    def __init__(self, alias, base):
        self.alias=alias; self.base=base

class Model:
    def __init__(self):
        self.structs={}   # fqn -> Struct
        self.enums={}     # name or fqn -> Enum
        self.typedefs={}  # alias -> base str

    def find_struct(self, name):
        if name in self.structs: return self.structs[name]
        # try simple name
        for k in self.structs:
            if k.split('::')[-1]==name: return self.structs[k]
        return None
    def has_enum(self, name):
        if name in self.enums: return True
        for k in self.enums:
            if k.split('::')[-1]==name: return True
        return False

# ---------- 파서 ----------

def decomment(txt:str)->str:
    txt=C_COMMENT.sub('',txt)
    txt=CPP_COMMENT.sub('',txt)
    return txt

def parse_modules(txt):
    # 반환: [(mod_chain, inner_text)] 재귀적으로 풀기보단 선형 스택으로 구조만 추출
    blocks=[]
    i=0; n=len(txt)
    stack=[]
    while i<n:
        m=MODULE_RE.search(txt, i)
        if not m: break
        name=m.group(1)
        # find matching '}'
        j=m.end()
        depth=1
        while j<n and depth>0:
            if txt[j]=='{': depth+=1; j+=1
            elif txt[j]=='}': depth-=1; j+=1
            else: j+=1
        inner=txt[m.end():j-1]
        blocks.append( (name, inner) )
        i=j
    return blocks

def parse_structs_enums_typedefs(txt, ns_chain, model:Model):
    # typedefs
    for m in TYPEDEF_RE.finditer(txt):
        base=m.group(1).strip(); alias=m.group(2).strip()
        model.typedefs[alias]=WS.sub(' ', base)
    # enums
    for m in ENUM_RE.finditer(txt):
        name=m.group(1)
        body=m.group(2)
        toks=[t.strip() for t in body.split(',') if t.strip()]
        fqn='::'.join(ns_chain+[name]) if ns_chain else name
        model.enums[fqn]=Enum(fqn,toks)
    # structs
    for m in STRUCT_RE.finditer(txt):
        name=m.group(1); body=m.group(2)
        fqn='::'.join(ns_chain+[name]) if ns_chain else name
        st=Struct(fqn)
        for line in body.split(';'):
            line=line.strip()
            if not line: continue
            opt='@optional' in line
            line=line.replace('@optional','').strip()
            # e.g. "P_LDM_Common::T_IdentifierType A_sourceID" or "long level"
            parts=line.split()
            fname=parts[-1]
            tstr=line[:line.rfind(fname)].strip()
            st.fields.append(Field(fname, WS.sub(' ', tstr), opt))
        model.structs[fqn]=st


def parse_idl_files(files)->Model:
    model=Model()
    for fp in files:
        txt=pathlib.Path(fp).read_text(encoding='utf-8', errors='ignore')
        txt=decomment(txt)
        # top-level typedef/enum/struct
        parse_structs_enums_typedefs(txt, [], model)
        # modules
        for name, inner in parse_modules(txt):
            parse_structs_enums_typedefs(inner, [name], model)
    return model

# ---------- 타입 해석 ----------

def resolve_type(model: Model, tstr: str, current_ns=None):
    tstr = tstr.strip()

    # typedef 치환
    seen = set()
    while tstr in model.typedefs and tstr not in seen:
        seen.add(tstr)
        tstr = model.typedefs[tstr]

    # sequence<...>
    m = SEQUENCE_RE.search(tstr)
    if m:
        elem = m.group(1).strip()
        cap  = m.group(2)
        max_len = int(cap) if cap else None
        if elem in ('char', 'T_Char', 'P_LDM_Common::T_Char'):
            return {'kind': 'string', 'max_len': max_len}
        elem_res = resolve_type(model, elem, current_ns)
        return {'kind': 'sequence', 'elem': elem_res, 'max_len': max_len}

    # 기본형
    if tstr in PRIMS:
        return {'kind': PRIMS[tstr]}
    
    def resolve_fqn_simple(name: str, is_enum: bool):
        if '::' in name:
            return name
        ns_chain = current_ns or []
        for i in range(len(ns_chain), -1, -1):
            cand = '::'.join(ns_chain[:i] + [name]) if i > 0 else name
            if is_enum:
                if model.has_enum(cand):
                    # 짧은 이름 대신 항상 FQN 반환
                    for k in model.enums:
                        if k.split("::")[-1] == name:
                            return k
                    return cand


    # enum
    efqn = resolve_fqn_simple(tstr, is_enum=True)
    if model.has_enum(efqn):
        return {'kind': 'enum', 'enum_name': efqn}

    # struct
    sfqn = resolve_fqn_simple(tstr, is_enum=False)
    st = model.find_struct(sfqn)
    if st:
        return {'kind': 'struct', 'ref': st.fqn}

    return {'kind': 'unknown', 'raw': tstr}

def resolve_all(model:Model):
    for st in model.structs.values():
        ns = st.fqn.split('::')[:-1]  # 현재 struct의 네임스페이스
        for f in st.fields:
            info = resolve_type(model, f.type_str, ns)
            f.kind      = info.get('kind')
            f.max_len   = info.get('max_len')
            f.enum_name = info.get('enum_name')
            f.elem      = info.get('elem')
            f.ref       = info.get('ref')

# ---------- 토픽 선택 ----------

def pick_topics(model:Model, includes, excludes):
    topics=[]
    for fqn, st in model.structs.items():
        # 기본 제외: LDM_Common 계열
        if fqn.startswith('P_LDM_Common::') or fqn.startswith('LDM_Common::'): continue
        # include/exclude 패턴 적용
        inc_ok = (not includes) or any(fnmatch(fqn, pat) or fnmatch('::'+fqn.split('::')[-1], pat) for pat in includes)
        exc_bad = any(fnmatch(fqn, pat) for pat in excludes)
        if inc_ok and not exc_bad:
            topics.append(st)
    return topics

# ---------- 코드 생성 ----------

HDR_PROLOGUE = """
#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>
#include <string>
using json = nlohmann::json;
using Handler = bool(*)(const json&, std::string&);
// 생성됨: 자동 생성 코드. 수정 금지.
void register_handlers(std::unordered_map<std::string, std::pair<Handler,Handler>>& reg);
""".strip()

CPP_PROLOGUE = """
#include "parser_core_gen.hpp"
#include "enum_tables_gen.hpp"
#include <limits>

static inline bool only_keys(const json& j, const std::unordered_set<std::string>& allowed){
    if(!j.is_object()) return false; for(auto it=j.begin(); it!=j.end(); ++it) if(!allowed.count(it.key())) return false; return true;
}
static inline bool chk_i64(const json& v){ return v.is_number_integer(); }
static inline bool chk_num(const json& v){ return v.is_number(); }
static inline bool chk_bool(const json& v){ return v.is_boolean(); }
static inline bool chk_str(const json& v){ return v.is_string(); }

""".strip()+"\n"

ENUM_HPP_PROLOGUE = """
#pragma once
#include <string>
#include <unordered_set>

// 생성됨: enum 허용 토큰 집합
""".strip()+"\n"


def gen_enum_tables(model:Model):
    lines=[ENUM_HPP_PROLOGUE]
    for fqn, en in sorted(model.enums.items()):
        # 짧은 이름 건너뛰기 조건 제거
        var=fqn.replace('::','__')
        arr=', '.join([f'"{t}"' for t in en.tokens])
        lines.append(f'static const std::unordered_set<std::string> kEnum_{var} = '+'{'+arr+'};')
    return '\n'.join(lines)+"\n"


def keyset(fields):
    keys=', '.join([f'"{f.name}"' for f in fields])
    return f'static const std::unordered_set<std::string> __keys = '+'{'+keys+'};\n'


def emit_check_assign(f:Field, model:Model, src_expr:str, dst_obj:str, indent:int=4)->str:
    sp=' '*indent
    name=f.name
    opt=f.optional
    lines=[]
    def need_present():
        return f"if(!{src_expr}.contains(\"{name}\")) return false;\n"
    ref=f"{src_expr}[\"{name}\"]"
    if f.kind in ('int16','uint16','int32','uint32','int64','uint64','uint8','char'):
        if not opt: lines.append(sp+need_present())
        lines.append(sp+f"if({src_expr}.contains(\"{name}\")) {{ const auto& v={ref}; if(!v.is_number_integer()) return false; {dst_obj}[\"{name}\"]=v; }}\n")
    elif f.kind in ('float32','float64'):
        if not opt: lines.append(sp+need_present())
        lines.append(sp+f"if({src_expr}.contains(\"{name}\")) {{ const auto& v={ref}; if(!v.is_number()) return false; {dst_obj}[\"{name}\"]=v; }}\n")
    elif f.kind=='bool':
        if not opt: lines.append(sp+need_present())
        lines.append(sp+f"if({src_expr}.contains(\"{name}\")) {{ const auto& v={ref}; if(!v.is_boolean()) return false; {dst_obj}[\"{name}\"]=v; }}\n")
    elif f.kind=='string':
        if not opt: lines.append(sp+need_present())
        chk_len = f" if(s.size()>{f.max_len}) return false;" if f.max_len else ""
        lines.append(sp+f"if({src_expr}.contains(\"{name}\")) {{ const auto& v={ref}; if(!v.is_string()) return false; std::string s=v.get<std::string>();{chk_len} {dst_obj}[\"{name}\"]=std::move(s); }}\n")
    elif f.kind=='enum':
        if not opt: lines.append(sp+need_present())
        var = (f.enum_name or '').replace('::','__')
        lines.append(sp+f"""if({src_expr}.contains("{name}")) {{
        const auto& v={ref}; if(!v.is_string()) return false;
        std::string tok=v.get<std::string>();
        if(!kEnum_{var}.count(tok)) return false;
        {dst_obj}["{name}"]=std::move(tok);
    }}
""")
    elif f.kind=='struct':
        if not opt: lines.append(sp+need_present())
        func = f.ref.replace('::','__')
        lines.append(sp+f"if({src_expr}.contains(\"{name}\")) {{ const auto& obj={ref}; json out_child; if(!validate_{func}(obj, out_child)) return false; {dst_obj}[\"{name}\"]=std::move(out_child); }}\n")
    elif f.kind=='sequence':
        if not opt: lines.append(sp+need_present())
        maxlen = f.max_len
        capchk = f" if(arr.size()>{maxlen}) return false;" if maxlen else ""
        lines.append(sp+f"if({src_expr}.contains(\"{name}\")) {{ const auto& arr={ref}; if(!arr.is_array()) return false;{capchk} json a=json::array();\n")
        # element check
        e=f.elem
        if e.get('kind')=='struct':
            func=e['ref'].replace('::','__')
            lines.append(sp+"  for(const auto& el: arr){ json ch; if(!validate_"+func+"(el, ch)) return false; a.push_back(std::move(ch)); }\n")
        elif e.get('kind')=='enum':
            en=e['enum_name']; setname = en if '::' in en else ('P_LDM_Common::'+en if model.has_enum('P_LDM_Common::'+en) else en)
            var=setname.replace('::','__')
            lines.append(sp+"  for(const auto& el: arr){ if(!el.is_string()) return false; std::string tok=el.get<std::string>(); if(!kEnum_"+var+".count(tok)) return false; a.push_back(std::move(tok)); }\n")
        elif e.get('kind') in ('int16','uint16','int32','uint32','int64','uint64','uint8','char'):
            lines.append(sp+"  for(const auto& el: arr){ if(!el.is_number_integer()) return false; a.push_back(el); }\n")
        elif e.get('kind') in ('float32','float64'):
            lines.append(sp+"  for(const auto& el: arr){ if(!el.is_number()) return false; a.push_back(el); }\n")
        elif e.get('kind')=='bool':
            lines.append(sp+"  for(const auto& el: arr){ if(!el.is_boolean()) return false; a.push_back(el); }\n")
        elif e.get('kind')=='string':
            mlen=e.get('max_len'); lenchk=(f" if(s.size()>{mlen}) return false;" if mlen else "")
            lines.append(sp+"  for(const auto& el: arr){ if(!el.is_string()) return false; std::string s=el.get<std::string>();"+lenchk+" a.push_back(std::move(s)); }\n")
        else:
            lines.append(sp+"  for(const auto& el: arr){ a.push_back(el); }\n")
        lines.append(sp+f"  {dst_obj}[\"{name}\"]=std::move(a); }}\n")
    else:
        if not opt: lines.append(sp+need_present())
        lines.append(sp+f"if({src_expr}.contains(\"{name}\")) {{ {dst_obj}[\"{name}\"]= {ref}; }}\n")
    return ''.join(lines)

def gen_forward_decls(model: Model) -> str:
    lines = []
    for fqn, st in sorted(model.structs.items()):
        func = st.fqn.replace('::','__')
        lines.append(f"static bool validate_{func}(const json& in, json& out);")
    return "\n".join(lines) + "\n\n"

def gen_validators(model:Model):
    code=[]
    # 1) 전방 선언
    code.append(gen_forward_decls(model))
    # 각 struct에 대해 validate_ 함수 생성
    for fqn, st in sorted(model.structs.items()):
        func=st.fqn.replace('::','__')
        code.append(f"// validate {st.fqn}\n")
        code.append(f"static bool validate_{func}(const json& in, json& out){{\n")
        code.append("    if(!in.is_object()) return false;\n")
        code.append('    '+keyset(st.fields))
        code.append("    if(!only_keys(in, __keys)) return false;\n")
        code.append("    json o=json::object();\n")
        for f in st.fields:
            code.append(emit_check_assign(f, model, 'in', 'o', indent=4))
        # 필수 필드가 누락된 경우는 위에서 체크됨(contains 추가). optional은 스킵 허용
        code.append("    out=std::move(o);\n    return true;\n}\n\n")
    return ''.join(code)


def gen_handlers(topics):
    code=[]
    for st in topics:
        func=st.fqn.replace('::','__')
        code.append(f"// handlers for {st.fqn}\n")
        code.append(f"static bool handle_{func}_from(const json& in, std::string& out){{ json o; if(!validate_{func}(in,o)) return false; out=o.dump(); return true; }}\n")
        code.append(f"static bool handle_{func}_to  (const json& in, std::string& out){{ json o; if(!validate_{func}(in,o)) return false; out=o.dump(); return true; }}\n\n")
    return ''.join(code)


def gen_registry(topics):
    entries=[]
    for st in topics:
        func=st.fqn.replace('::','__')
        entries.append(f'    reg["{st.fqn}"] = {{handle_{func}_from, handle_{func}_to}};')
    return '\n'.join(entries)


def render_cpp(model:Model, topics):
    parts=[CPP_PROLOGUE]
    parts.append(gen_validators(model))
    parts.append(gen_handlers(topics))
    parts.append("void register_handlers(std::unordered_map<std::string, std::pair<Handler,Handler>>& reg){\n")
    parts.append(gen_registry(topics))
    parts.append("\n}\n")
    return ''.join(parts)

# ---------- 메인 ----------

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--idl', nargs='*', required=True)
    ap.add_argument('--discover', action='store_true')
    ap.add_argument('--include', nargs='*', default=['::C_*','AlarmMsg','StringMsg'])
    ap.add_argument('--exclude', nargs='*', default=['P_LDM_Common::*','LDM_Common::*'])
    ap.add_argument('--out', required=True)
    args=ap.parse_args()

    outdir=pathlib.Path(args.out); outdir.mkdir(parents=True, exist_ok=True)

    model=parse_idl_files(args.idl)
    resolve_all(model)

    topics = pick_topics(model, args.include if args.discover else None, args.exclude)

    # 파일 생성
    (outdir/ 'parser_core_gen.hpp').write_text(HDR_PROLOGUE, encoding='utf-8')
    (outdir/ 'parser_core_gen.cpp').write_text(render_cpp(model, topics), encoding='utf-8')
    (outdir/ 'enum_tables_gen.hpp').write_text(gen_enum_tables(model), encoding='utf-8')

    # 디버그용 요약
    summary={
        'topics':[st.fqn for st in topics],
        'structs': list(sorted(model.structs.keys())),
        'enums': list(sorted({k for k in model.enums.keys() if '::' in k})),
        'typedefs': model.typedefs,
    }
    (outdir/ 'type_index.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding='utf-8')

if __name__=='__main__':
    main()