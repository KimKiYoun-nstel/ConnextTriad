#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
emit_jsonbind.py — RTI XML(rtiddsgen -convertToXml) → JSON<->DDS 바인딩 코드 생성

호출:
  python emit_jsonbind.py <GEN_DIR> <XML_DIR>
  (옵션형도 지원) --out-dir <GEN_DIR> --xml-dir <XML_DIR>

산출:
  <GEN_DIR>/idl_json_bind.hpp
  <GEN_DIR>/idl_json_bind.cpp

규칙:
  - Modern C++ 전용. 필드 접근은 전부 접근자: s.field(), s.field(value)
  - C_ 접두 struct만 레지스트리에 등록
  - enum은 문자열 직렬화. 파싱은 문자열/정수 허용
  - sequence<char> ↔ JSON string
  - bounded string/sequence 길이 검증
"""
import argparse, sys, xml.etree.ElementTree as ET
from pathlib import Path
from collections import namedtuple

# ---------- Primitive map ----------
PRIMS = {
    'boolean':'bool','char':'char','char8':'char','octet':'uint8_t','uint8':'uint8_t',
    'int16':'int16_t','uint16':'uint16_t','int32':'int32_t','uint32':'uint32_t',
    'int64':'int64_t','uint64':'uint64_t','float32':'float','float64':'double',
    'longDouble':'long double'
}

def tn(e): return e.tag.split('}',1)[1] if '}' in e.tag else e.tag
def fqn(ns): return '::'.join([p for p in ns if p])

Type = dict
Member = namedtuple('Member','name type')

class EnumType:   # fqn -> enumerators[(name,int)]
    def __init__(self,fqn,enumerators,xml): self.fqn,self.enumerators,self.xml=fqn,enumerators,xml
class StructType: # fqn -> members[List[Member]]
    def __init__(self,fqn,members,xml): self.fqn,self.members,self.xml=fqn,members,xml
class TypedefType: # fqn -> aliased Type
    def __init__(self,fqn,aliased,xml): self.fqn,self.aliased_type,self.xml=fqn,aliased,xml

class Model:
    def __init__(self):
        self.enums={} 
        self.structs={} 
        self.typedefs={}
        self.files=set()
    def resolve(self, t:Type)->Type:
        seen=set()
        while t:
            k=t.get('kind')
            if k=='type': t=t['inner']; continue
            if k=='typedef':
                f=t['fqn']
                if f in seen: break
                seen.add(f)
                td=self.typedefs.get(f); 
                if not td: break
                t=td.aliased_type; continue
            if k=='nonbasic':
                td=self.typedefs.get(t.get('fqn'))
                if td: t=td.aliased_type; continue
            break
        return t

# ---------- Parse ----------
def parse_enum(e,ns,base):
    name=e.attrib.get('name',''); fq=fqn(ns+[name]); out=[]; auto=0
    for en in e.findall('./{*}enumerator'):
        nm=en.attrib.get('name',''); v=en.attrib.get('value')
        vi=auto if v is None else (int(v,0) if v else auto)
        out.append((nm,vi)); auto=vi+1
    return EnumType(fq,out,base)

def parse_member_type_attrs(mem):
    t=mem.attrib.get('type'); 
    if not t: return None
    t=t.strip()
    if t=='string':
        ml=None
        for k in ('stringMaxLength','maxLength','length'):
            if k in mem.attrib:
                try: ml=int(mem.attrib[k]); break
                except: pass
        return {'kind':'string','wide':False,'max_len':ml}
    if t=='nonBasic':
        ref=mem.attrib.get('nonBasicTypeName') or mem.attrib.get('typeName') or mem.attrib.get('nonBasic')
        if not ref: return None
        ref=ref.replace(':::', '::')
        if 'sequenceMaxLength' in mem.attrib:
            try: m=int(mem.attrib['sequenceMaxLength'])
            except: m=None
            return {'kind':'sequence','elem':{'kind':'nonbasic','fqn':ref},'max_len':m}
        return {'kind':'nonbasic','fqn':ref}
    if t in PRIMS:
        return {'kind':'prim','name':PRIMS[t]}
    return None

def parse_type(node, ns):
    if node is None: return None
    t=tn(node)
    if t in PRIMS: return {'kind':'prim','name':PRIMS[t]}
    if t in ('string','wstring'):
        ml=None
        for k in ('length','maxLength','stringMaxLength'):
            if k in node.attrib:
                try: ml=int(node.attrib[k]); break
                except: pass
        return {'kind':'string','wide':(t=='wstring'),'max_len':ml}
    if t=='nonBasic':
        ref=node.attrib.get('name') or node.attrib.get('typeName') or node.attrib.get('nonBasicTypeName') or ''
        return {'kind':'nonbasic','fqn':ref.replace(':::','::')}
    if t=='sequence':
        ml=None
        for k in ('maxLength','length','sequenceMaxLength'):
            if k in node.attrib:
                try: ml=int(node.attrib[k]); break
                except: pass
        el=node.find('./{*}elementType'); child=None
        if el is not None: child=next(iter(el),None)
        if child is None: child=next(iter(node),None)
        return {'kind':'sequence','elem':parse_type(child,ns) if child is not None else None,'max_len':ml}
    if t=='array':
        dims=[]; dims_attr=node.attrib.get('dimensions') or node.attrib.get('dim') or node.attrib.get('cardinality')
        if dims_attr: dims=[int(x) for x in dims_attr.replace(',', ' ').split() if x.strip().isdigit()]
        else:
            for d in node.findall('./{*}dimension'):
                try: dims.append(int(d.attrib.get('size','')))
                except: pass
        el=node.find('./{*}elementType'); child=None
        if el is not None: child=next(iter(el),None)
        if child is None: child=next(iter(node),None)
        return {'kind':'array','elem':parse_type(child,ns) if child is not None else None,'dims':dims}
    if t in ('member','field'):
        mt=parse_member_type_attrs(node); 
        if mt: return mt
    return {'kind':'unknown','raw':ET.tostring(node,encoding='unicode')}

def parse_struct(e,ns,base):
    name=e.attrib.get('name',''); fq=fqn(ns+[name]); members=[]
    mems=e.findall('./{*}member') or e.findall('.//{*}member')
    for m in mems:
        mname=m.attrib.get('name',''); 
        if not mname: continue
        t=parse_member_type_attrs(m)
        if t is None:
            tnode=m.find('./{*}type'); child=None
            if tnode is not None: child=next(iter(tnode),None)
            if child is None:
                for c in m:
                    if tn(c) in ('nonBasic','string','wstring','sequence','array') or tn(c) in PRIMS:
                        child=c; break
            t=parse_type(child,ns) if child is not None else None
        if t: members.append(Member(mname,t))
    return StructType(fq,members,base)

def parse_typedef(e,ns,base):
    name=e.attrib.get('name',''); fq=fqn(ns+[name])
    tattr=e.attrib.get('type')
    if tattr:
        tattr=tattr.strip()
        if tattr=='string':
            ml=None
            for k in ('stringMaxLength','maxLength','length'):
                if k in e.attrib:
                    try: ml=int(e.attrib[k]); break
                    except: pass
            return TypedefType(fq,{'kind':'string','wide':False,'max_len':ml},base)
        if tattr=='nonBasic':
            ref=e.attrib.get('nonBasicTypeName') or e.attrib.get('typeName') or e.attrib.get('nonBasic')
            if ref:
                ref=ref.replace(':::','::')
                if 'sequenceMaxLength' in e.attrib:
                    try: m=int(e.attrib['sequenceMaxLength'])
                    except: m=None
                    return TypedefType(fq,{'kind':'sequence','elem':{'kind':'nonbasic','fqn':ref},'max_len':m},base)
                return TypedefType(fq,{'kind':'nonbasic','fqn':ref},base)
        if tattr in PRIMS:
            return TypedefType(fq,{'kind':'prim','name':PRIMS[tattr]},base)
    tnode=e.find('./{*}type'); child=None
    if tnode is not None: child=next(iter(tnode),None)
    if child is None: child=next(iter(e),None)
    inner=parse_type(child,ns) if child is not None else None
    return TypedefType(fq,inner,base)

def walk(node,ns,base,model):
    t=tn(node)
    if t=='module': ns=ns+[node.attrib.get('name','')]
    if t == 'enum':
        en = parse_enum(node, ns, base)
        model.enums[en.fqn] = en
        return

    if t == 'typedef':
        td = parse_typedef(node, ns, base)
        model.typedefs[td.fqn] = td
        return

    if t == 'struct':
        st = parse_struct(node, ns, base)
        prev = model.structs.get(st.fqn)
        if prev is None or len(st.members) > len(prev.members):
            model.structs[st.fqn] = st
        return

    for c in node: walk(c,ns,base,model)

def parse_xml_dir(xml_dir):
    m=Model(); xml_dir=Path(xml_dir)
    if not xml_dir.exists(): raise SystemExit(f"XML dir not found: {xml_dir}")
    for p in sorted(xml_dir.glob('*.xml')):
        try: root=ET.parse(p).getroot()
        except ET.ParseError as e: print(f"[warn] skip {p.name}: {e}", file=sys.stderr); continue
        m.files.add(p.stem); walk(root,[],p.stem,m)
    return m

# ---------- Helpers for emission ----------
def cpp_id(s): return s.replace('::','__').replace('.','_')
def q(s): return '"' + s.replace('\\','\\\\').replace('"','\\"') + '"'
def is_topic(fqn): return fqn.split('::')[-1].startswith('C_')

def is_char_seq(model,t):
    t=model.resolve(t)
    if not t or t.get('kind')!='sequence': return False
    e=model.resolve(t['elem'])
    return e and e.get('kind')=='prim' and e.get('name')=='char'

def is_string(model,t):
    tt=model.resolve(t); return tt.get('kind')=='string' and not tt.get('wide',False)
def is_wstring(model,t):
    tt=model.resolve(t); return tt.get('kind')=='string' and tt.get('wide',False)
def is_enum(model,t):
    tt=model.resolve(t); return tt.get('kind')=='nonbasic' and tt.get('fqn') in model.enums
def is_struct(model,t):
    tt=model.resolve(t); return tt.get('kind')=='nonbasic' and tt.get('fqn') in model.structs

# ---------- Emit ----------
def emit_header(out_dir:Path):
    h = """#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace idlmeta {
  using ToJsonFn   = bool (*)(const void* sample, nlohmann::json& out);
  using FromJsonFn = bool (*)(const nlohmann::json& in, void* sample);
  struct JsonOps { ToJsonFn to_json; FromJsonFn from_json; };
  const std::unordered_map<std::string, JsonOps>& json_registry() noexcept;
} // namespace idlmeta
"""
    (out_dir/'idl_json_bind.hpp').write_text(h,encoding='utf-8')

def emit_cpp(model,out_dir:Path,xml_basenames):
    incs=['#include "idl_json_bind.hpp"']+[f'#include "{b}.hpp"' for b in sorted(xml_basenames)]
    incs+=['#include <vector>','#include <array>','#include <type_traits>','#include <utility>']
    incs+=['using nlohmann::json;','']

    helpers = """
namespace {
  template <typename E>
  constexpr auto to_underlying(E e) noexcept -> typename std::underlying_type<E>::type {
    return static_cast<typename std::underlying_type<E>::type>(e);
  }
} // anon
""".strip('\n')

    fwd=[]
    for f in sorted(model.structs):
        tag=cpp_id(f)
        fwd+= [f'static bool to_json_{tag}(const void* vp, json& j) noexcept;',
               f'static bool from_json_{tag}(const json& j, void* vp) noexcept;']

    enum_defs=[]
    for f,en in sorted(model.enums.items()):
        tag=cpp_id(f); L=[]
        L+= [f'static inline const char* to_string_{tag}({f} v) noexcept {{',
             '  switch (static_cast<int>(v)) {']
        for name,val in en.enumerators: L.append(f'    case {val}: return {q(name)};')
        L+= ['    default: return "UNKNOWN_ENUM_VALUE";','  }','}','']
        L+= [f'static inline bool parse_enum_{tag}(const json& j, {f}& out) noexcept {{',
             '  try {',
             '    if (j.is_string()) {',
             '      const auto& s = j.get_ref<const std::string&>();']
        for name,val in en.enumerators: L.append(f'      if (s == {q(name)}) {{ out = static_cast<{f}>({val}); return true; }}')
        L+= ['      return false;',
             '    } else if (j.is_number_integer()) {',
             f'      out = static_cast<{f}>(j.get<int>());',
             '      return true;',
             '    } else { return false; }',
             '  } catch (...) { return false; }',
             '}']
        enum_defs.append('\n'.join(L))

    struct_defs=[]
    for f,st in sorted(model.structs.items()):
        tag=cpp_id(f); B=[]
        # to_json: 접근자 기반 읽기
        B += [f'static bool to_json_{tag}(const void* vp, json& j) noexcept {{',
              f'  try {{ auto const& s = *static_cast<const {f}*>(vp);',
              '    j = json::object();']
        for mem in st.members:
            m=mem.name; mt=model.resolve(mem.type); mk=mt.get('kind')
            # sequence<char> -> string
            if mk=='sequence' and is_char_seq(model,mt):
                B.append(f'    {{ const auto& v = s.{m}(); j[{q(m)}] = std::string(v.begin(), v.end()); }}'); continue
            # primitive or std::string
            if mk=='prim' or is_string(model,mt):
                B.append(f'    j[{q(m)}] = s.{m}();'); continue
            # wstring -> array of code units
            if is_wstring(model,mt):
                B.append(f'    {{ const auto& ws = s.{m}(); j[{q(m)}] = json::array(); for (auto ch : ws) j[{q(m)}].push_back(static_cast<uint32_t>(ch)); }}'); continue
            # nonbasic: enum/struct
            if mk=='nonbasic':
                ref=mt['fqn']
                if ref in model.enums:
                    et=cpp_id(ref); B.append(f'    j[{q(m)}] = to_string_{et}(s.{m}());')
                elif ref in model.structs:
                    ct=cpp_id(ref); B+= [f'    {{ json _tmp; if (!to_json_{ct}(&s.{m}(), _tmp)) return false; j[{q(m)}] = std::move(_tmp); }}']
                else:
                    B.append(f'    /* unknown nonbasic {m} */')
                continue
            # sequence<T>
            if mk=='sequence':
                elem=model.resolve(mt['elem'])
                if is_enum(model,elem):
                    et=cpp_id(elem['fqn'])
                    B+= [f'    j[{q(m)}] = json::array();',
                         f'    for (const auto& v : s.{m}()) j[{q(m)}].push_back(to_string_{et}(v));']
                elif is_struct(model,elem):
                    ct=cpp_id(elem['fqn'])
                    B+= [f'    j[{q(m)}] = json::array();',
                         f'    for (const auto& v : s.{m}()) {{ json _e; if(!to_json_{ct}(&v, _e)) return false; j[{q(m)}].push_back(std::move(_e)); }}']
                else:
                    B.append(f'    j[{q(m)}] = s.{m}();')
                continue
            # array[T;N] => std::array
            if mk=='array':
                dims=mt.get('dims',[])
                if len(dims)==1:
                    n=dims[0]; elem=model.resolve(mt['elem'])
                    B.append(f'    j[{q(m)}] = json::array();')
                    if is_enum(model,elem):
                        et=cpp_id(elem['fqn'])
                        B.append(f'    {{ const auto& a = s.{m}(); for (size_t i=0;i<{n};++i) j[{q(m)}].push_back(to_string_{et}(a[i])); }}')
                    elif is_struct(model,elem):
                        ct=cpp_id(elem['fqn'])
                        B.append(f'    {{ const auto& a = s.{m}(); for (size_t i=0;i<{n};++i) {{ json _e; if(!to_json_{ct}(&a[i], _e)) return false; j[{q(m)}].push_back(std::move(_e)); }} }}')
                    else:
                        B.append(f'    {{ const auto& a = s.{m}(); for (size_t i=0;i<{n};++i) j[{q(m)}].push_back(a[i]); }}')
                else:
                    B.append(f'    /* unsupported multi-dim array {m} */')
                continue
            B.append(f'    /* unhandled {m} */')
        B.append('    return true; } catch (...) { return false; } }')

        # from_json: 접근자 기반 쓰기
        B += [f'static bool from_json_{tag}(const json& j, void* vp) noexcept {{',
              f'  try {{ auto& s = *static_cast<{f}*>(vp);']
        for mem in st.members:
            m=mem.name; mt=model.resolve(mem.type); mk=mt.get('kind')
            B.append(f'    if (!j.contains({q(m)})) return false;')
            if mk=='sequence' and is_char_seq(model,mt):
                ml=mt.get('max_len')
                B.append(f'    {{ auto _str = j.at({q(m)}).get<std::string>();')
                if ml is not None:
                    B.append(f'      if (_str.size() > {ml}) return false;')
                B.append(f'      auto& v = s.{m}(); v.resize(_str.size());')
                B.append(f'      for (size_t i=0; i<_str.size(); ++i) v[i] = _str[i]; }}')
                continue
            if mk=='prim':
                B.append(f'    {{ typename std::remove_reference<decltype(s.{m}())>::type _tmp{{}}; j.at({q(m)}).get_to(_tmp); s.{m}(_tmp); }}'); continue
            if is_string(model,mt):
                ml=mt.get('max_len')
                if ml is not None:
                    B.append(f'    {{ std::string _str = j.at({q(m)}).get<std::string>(); if (_str.size() > {ml}) return false; s.{m}(_str); }}')
                else:
                    B.append(f'    {{ std::string _str = j.at({q(m)}).get<std::string>(); s.{m}(_str); }}')
                continue
            if is_wstring(model,mt):
                B+= [f'    {{ const auto& _arr = j.at({q(m)}); if (!_arr.is_array()) return false; auto& ws = s.{m}(); ws.clear(); ws.reserve(_arr.size());',
                     f'      for (const auto& _e : _arr) ws.push_back(static_cast<wchar_t>(_e.get<uint32_t>())); }}']
                continue
            if mk=='nonbasic':
                ref=mt['fqn']
                if ref in model.enums:
                    et=cpp_id(ref)
                    B.append(f'    {{ {ref} _ev{{}}; if (!parse_enum_{et}(j.at({q(m)}), _ev)) return false; s.{m}(_ev); }}')
                elif ref in model.structs:
                    ct=cpp_id(ref)
                    B.append(f'    if (!from_json_{ct}(j.at({q(m)}), &s.{m}())) return false;')
                else:
                    B.append('    return false;')
                continue
            if mk=='sequence':
                elem=model.resolve(mt['elem']); ml=mt.get('max_len')
                B.append(f'    {{ const auto& _arr = j.at({q(m)}); if (!_arr.is_array()) return false; auto& v = s.{m}(); v.clear();')
                if ml is not None: B.append(f'      if (_arr.size() > {ml}) return false;')
                if is_enum(model,elem):
                    et=cpp_id(elem['fqn'])
                    B.append(f'      v.reserve(_arr.size()); for (const auto& _e : _arr) {{ {elem["fqn"]} _tmp{{}}; if(!parse_enum_{et}(_e, _tmp)) return false; v.push_back(_tmp); }}')
                elif is_struct(model,elem):
                    ct=cpp_id(elem['fqn'])
                    B.append(f'      v.reserve(_arr.size()); for (const auto& _e : _arr) {{ {elem["fqn"]} _tmp{{}}; if(!from_json_{ct}(_e, &_tmp)) return false; v.push_back(std::move(_tmp)); }}')
                else:
                    B.append(f'      v = _arr.get<decltype(v)>();')
                B.append('    }'); continue
            if mk=='array':
                dims=mt.get('dims',[])
                if len(dims)==1:
                    n=dims[0]; elem=model.resolve(mt['elem'])
                    B.append(f'    {{ const auto& _arr = j.at({q(m)}); if (!_arr.is_array()) return false; if (_arr.size() != {n}) return false; auto& a = s.{m}();')
                    if is_enum(model,elem):
                        et=cpp_id(elem['fqn'])
                        B.append(f'      for (size_t i=0;i<{n};++i) if(!parse_enum_{et}(_arr[i], a[i])) return false; }}')
                    elif is_struct(model,elem):
                        ct=cpp_id(elem['fqn'])
                        B.append(f'      for (size_t i=0;i<{n};++i) if(!from_json_{ct}(_arr[i], &a[i])) return false; }}')
                    else:
                        B.append(f'      for (size_t i=0;i<{n};++i) a[i] = _arr[i].get<decltype(a[i])>(); }}')
                else:
                    B.append('    return false;')
                continue
            B.append('    return false;')
        B.append('    return true; } catch (...) { return false; } }')
        struct_defs.append('\n'.join(B))

    reg=['namespace idlmeta {',
         '  static const std::unordered_map<std::string, JsonOps>& make_registry() {',
         '    static const std::unordered_map<std::string, JsonOps> reg = {']
    for f in sorted(model.structs):
        if not is_topic(f): continue
        tag=cpp_id(f); reg.append(f'      {{ {q(f)}, JsonOps{{ &to_json_{tag}, &from_json_{tag} }} }},')
    reg += ['    };','    return reg;','  }',
            '  const std::unordered_map<std::string, JsonOps>& json_registry() noexcept {',
            '    return make_registry();','  }','} // namespace idlmeta']

    cpp = '\n'.join(incs)+'\n'+helpers+'\n'+'\n'.join(fwd)+'\n\n'+'\n\n'.join(enum_defs)+'\n\n'+'\n\n'.join(struct_defs)+'\n\n'+'\n'.join(reg)+'\n'
    (out_dir/'idl_json_bind.cpp').write_text(cpp,encoding='utf-8')

# ---------- CLI ----------
def parse_cli():
    argv=sys.argv[1:]
    if len(argv)>=2 and not argv[0].startswith('-') and not argv[1].startswith('-'):
        return Path(argv[0]), Path(argv[1])
    ap=argparse.ArgumentParser()
    ap.add_argument('--out-dir',required=True); ap.add_argument('--xml-dir',required=True)
    a=ap.parse_args(argv); return Path(a.out_dir), Path(a.xml_dir)

def main():
    out_dir, xml_dir = parse_cli()
    out_dir.mkdir(parents=True, exist_ok=True)
    model = parse_xml_dir(xml_dir)
    emit_header(out_dir)
    emit_cpp(model,out_dir,model.files)
    print(f"[ok] Generated: {out_dir/'idl_json_bind.hpp'}")
    print(f"[ok] Generated: {out_dir/'idl_json_bind.cpp'}")

if __name__=='__main__': main()
