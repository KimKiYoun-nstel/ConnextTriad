"""
간단한 예제: REQ 를 JSON -> CBOR로 직렬화하여 가상의 IPC로 전송하고,
응답과 EVT를 역직렬화하여 JSON으로 복원하는 예시입니다.

사용법:
  python examples\ipc_cbor_example.py

이 예제는 로컬에서 CBOR 인코딩/디코딩과 JSON Schema 검증을 시연합니다.
"""
from __future__ import annotations
import json
import cbor2
from jsonschema import validate, ValidationError
from pathlib import Path


SCHEMA_PATH = Path(__file__).resolve().parents[1] / 'docs' / 'ipc_protocol_schema_v2.json'


def load_schema():
    with open(SCHEMA_PATH, 'r', encoding='utf-8') as f:
        return json.load(f)


def make_request_example():
    req = {
        "op": "write",
        "target": { "kind": "writer", "topic": "chat" },
        "data": { "text": "Hello world from example" }
    }
    return req


def to_cbor(obj):
    return cbor2.dumps(obj)


def from_cbor(blob: bytes):
    return cbor2.loads(blob)


def validate_against(schema, instance, def_name):
    # validate against one of schema.definitions
    defs = schema.get('definitions', {})
    if def_name not in defs:
        raise KeyError(def_name)
    # Construct a small wrapper schema
    wrapper = {"$schema": schema.get('$schema'), "$ref": f"#/definitions/{def_name}", "definitions": defs}
    validate(instance=instance, schema=wrapper)


def main():
    schema = load_schema()
    req = make_request_example()
    print('Request JSON:', json.dumps(req, ensure_ascii=False))

    try:
        validate_against(schema, req, 'request')
        print('Request validated against schema (request).')
    except ValidationError as e:
        print('Schema validation failed:', e)

    blob = to_cbor(req)
    print('CBOR bytes length:', len(blob))

    # Simulate receiving side
    recv = from_cbor(blob)
    print('Decoded JSON:', json.dumps(recv, ensure_ascii=False))

    # Simulate a response
    rsp = { "ok": True, "result": { "action": "publish ok", "topic": "chat" } }
    validate_against(schema, rsp, 'response')
    print('Response validated.')
    rsp_blob = to_cbor(rsp)
    print('Response CBOR len:', len(rsp_blob))


if __name__ == '__main__':
    main()
