#!/usr/bin/env python3
"""
간단하지만 확장 가능한 시나리오 기반 테스트 러너

- 프로젝트의 IPC 규격(RIPC 헤더 + CBOR body)을 사용해 Agent와 통신
- 시나리오 파일(JSON)을 읽어 순차적으로 요청/대기/슬립을 처리
- 향후 변수 바인딩, 응답값 캡처, 스키마 검증으로 확장 가능

의존: cbor2
사용 예:
    python examples/test_runner.py examples/scenarios/scenario1.json --host 127.0.0.1 --port 25000
"""
from __future__ import annotations
import argparse
import json
import socket
import struct
import time
from pathlib import Path
import cbor2

# RIPC-like 헤더 포맷 (프로젝트 문서와 호환되도록 설계)
MAGIC = 0x52495043  # 'RIPC'
VERSION = 0x0001
MSG_FRAME_REQ = 0x1000
MSG_FRAME_RSP = 0x1001
MSG_FRAME_EVT = 0x1002

# struct format: magic(4) ver(2) type(2) corr_id(4) length(4) ts_ns(8)
HEADER_FMT = "!I H H I I Q"
HEADER_LEN = struct.calcsize(HEADER_FMT)


def now_ns() -> int:
    return time.time_ns()


def pack_frame(payload_bytes: bytes, frame_type: int, corr_id: int) -> bytes:
    length = len(payload_bytes)
    header = struct.pack(HEADER_FMT, MAGIC, VERSION, frame_type, corr_id, length, now_ns())
    return header + payload_bytes


def unpack_header(buf: bytes) -> dict:
    if len(buf) < HEADER_LEN:
        raise ValueError("buffer too small for header")
    vals = struct.unpack(HEADER_FMT, buf[:HEADER_LEN])
    return {
        "magic": vals[0],
        "version": vals[1],
        "type": vals[2],
        "corr_id": vals[3],
        "length": vals[4],
        "ts_ns": vals[5],
    }


def validate_scenario(scenario: dict) -> list:
    """시나리오 JSON의 기본 구조를 검사하고 문제 목록을 반환합니다.

    반환값: 문제 문자열 목록(문제가 없으면 빈 리스트)
    """
    errors = []
    if not isinstance(scenario, dict):
        errors.append("scenario must be a JSON object")
        return errors
    meta = scenario.get("meta")
    if not isinstance(meta, dict):
        errors.append("meta missing or not an object")
    steps = scenario.get("steps")
    if not isinstance(steps, list) or len(steps) == 0:
        errors.append("steps must be a non-empty array")
        return errors
    for idx, step in enumerate(steps):
        base = f"steps[{idx}]"
        if not isinstance(step, dict):
            errors.append(f"{base} must be an object")
            continue
        name = step.get("name")
        if not name:
            errors.append(f"{base} missing 'name'")
        stype = step.get("type")
        if stype not in ("request", "wait_evt", "sleep"):
            errors.append(f"{base} unknown or missing 'type' (expected request|wait_evt|sleep): {stype}")
            continue
        if stype == "request":
            body = step.get("body")
            if not isinstance(body, dict):
                errors.append(f"{base} request step requires 'body' object")
        elif stype == "wait_evt":
            topic = step.get("topic")
            mtype = step.get("msg_type")
            if not topic or not isinstance(topic, str):
                errors.append(f"{base} wait_evt requires 'topic' string")
            if not mtype or not isinstance(mtype, str):
                errors.append(f"{base} wait_evt requires 'msg_type' string")
        elif stype == "sleep":
            dur = step.get("duration")
            if dur is None:
                errors.append(f"{base} sleep requires 'duration' number")
            else:
                try:
                    fv = float(dur)
                    if fv < 0:
                        errors.append(f"{base} sleep duration must be >= 0")
                except Exception:
                    errors.append(f"{base} sleep duration must be a number")
    return errors


class ScenarioRunner:
    """시나리오를 읽어 실행하는 클래스

    간단한 책임 분리로 향후 확장(변수 바인딩, 동시성 등)이 쉽도록 설계.
    """

    def __init__(self, host: str = "127.0.0.1", port: int = 25000, timeout: float = 2.0):
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(timeout)
        self.next_corr = 1
        # 상태 저장소: 변수 바인딩 저장 (예: 생성된 id 등)
        self.state: dict = {}

    # --- 템플릿 치환 및 경로 추출 유틸리티 ---
    def _substitute_in_obj(self, obj):
        """객체를 재귀적으로 순회하며 ${var} 템플릿을 상태로 치환합니다.

        - 전체 문자열이 단일 플레이스홀더일 경우 실제 타입을 반환합니다.
        - 문자열 내 일부 치환은 문자열로 결합됩니다.
        """
        import re

        pattern = re.compile(r"\$\{([^}]+)\}")

        def sub_str(s: str):
            matches = list(pattern.finditer(s))
            if not matches:
                return s
            # 전체 문자열이 단일 플레이스홀더인지 확인
            if len(matches) == 1 and matches[0].span() == (0, len(s)):
                var = matches[0].group(1)
                return self.state.get(var, "")
            # 부분 치환: 모든 플레이스홀더를 문자열로 치환
            def repl(m):
                var = m.group(1)
                return str(self.state.get(var, ""))
            return pattern.sub(repl, s)

        if isinstance(obj, dict):
            return {k: self._substitute_in_obj(v) for k, v in obj.items()}
        if isinstance(obj, list):
            return [self._substitute_in_obj(v) for v in obj]
        if isinstance(obj, str):
            return sub_str(obj)
        return obj

    def _get_by_path(self, obj, path: str):
        """단일 문자열 경로('result.id' 등)를 받아 객체에서 값을 추출합니다.

        리스트 인덱스 접근도 지원합니다(예: items.0.id)
        """
        if obj is None:
            return None
        cur = obj
        for seg in path.split('.'):
            if cur is None:
                return None
            # 리스트 인덱스인지 시도
            if isinstance(cur, list):
                try:
                    idx = int(seg)
                    cur = cur[idx]
                except Exception:
                    return None
            elif isinstance(cur, dict):
                cur = cur.get(seg)
            else:
                return None
        return cur

    def send_request(self, body: dict, timeout: float | None = None) -> tuple[dict, object]:
        """요청을 전송하고 대응되는 RSP를 기다립니다.

        - corr_id를 자동 발급
        - EVT가 중간에 오면 EVT를 호출자에게 보고
        """
        corr = self.next_corr
        self.next_corr += 1
        # 전송 전에 body 내 템플릿을 상태로 치환
        body_to_send = self._substitute_in_obj(body)
        payload = cbor2.dumps(body_to_send)
        frame = pack_frame(payload, MSG_FRAME_REQ, corr)
        self.sock.sendto(frame, (self.host, self.port))

        t0 = time.time()
        to = timeout if timeout is not None else self.sock.gettimeout()
        while True:
            remaining = max(0.0, to - (time.time() - t0))
            self.sock.settimeout(remaining)
            try:
                data, _ = self.sock.recvfrom(65536)
            except socket.timeout:
                raise TimeoutError(f"rsp timeout corr_id={corr}")
            hdr = unpack_header(data)
            payload = data[HEADER_LEN:HEADER_LEN + hdr["length"]]
            try:
                body = cbor2.loads(payload) if payload else None
            except Exception:
                body = None
            if hdr["type"] == MSG_FRAME_RSP and hdr["corr_id"] == corr:
                return hdr, body
            if hdr["type"] == MSG_FRAME_EVT:
                evt = cbor2.loads(payload) if payload else None
                return hdr, {"__evt__": evt}

    def wait_evt(self, match_topic: str | None = None, match_type: str | None = None, timeout: float = 5.0):
        t0 = time.time()
        self.sock.settimeout(timeout)
        while True:
            remaining = max(0.0, timeout - (time.time() - t0))
            if remaining <= 0:
                raise TimeoutError("wait_evt timeout")
            self.sock.settimeout(remaining)
            try:
                data, _ = self.sock.recvfrom(65536)
            except socket.timeout:
                raise TimeoutError("wait_evt timeout")
            hdr = unpack_header(data)
            if hdr["type"] == MSG_FRAME_EVT:
                payload = data[HEADER_LEN:HEADER_LEN + hdr["length"]]
                evt = cbor2.loads(payload) if payload else None
                if not isinstance(evt, dict):
                    continue
                if match_topic and evt.get("topic") != match_topic:
                    continue
                if match_type and evt.get("type") != match_type:
                    continue
                return hdr, evt

    def run_scenario(self, scenario: dict) -> bool:
        steps = scenario.get("steps", [])
        for i, step in enumerate(steps, start=1):
            stype = step.get("type", "request")
            print(f"[{i}] step={step.get('name','')} type={stype}")
            if stype == "request":
                body = step["body"]
                expect = step.get("expect")
                try:
                    hdr, rsp = self.send_request(body, timeout=step.get("timeout"))
                except Exception as ex:
                    print(f"  ERROR sending request: {ex}")
                    return False
                print("  RSP hdr:", hdr)
                print("  RSP body:", json.dumps(rsp, ensure_ascii=False, default=str))
                # 응답에서 변수 저장(save) 처리: step에 'save': { "varname": "result.id" }
                save_map = step.get("save")
                if isinstance(save_map, dict):
                    for varname, path in save_map.items():
                        if not isinstance(varname, str) or not isinstance(path, str):
                            continue
                        val = self._get_by_path(rsp, path)
                        self.state[varname] = val
                        print(f"  SAVED: {varname} = {val}")
                if expect:
                    if "ok" in expect and (not isinstance(rsp, dict) or rsp.get("ok") != expect["ok"]):
                        print("  EXPECTATION FAILED: ok mismatch")
                        return False
                    if "result_contains" in expect:
                        rc = expect["result_contains"]
                        result = rsp.get("result", {})
                        for k, v in rc.items():
                            if isinstance(result, dict):
                                if result.get(k) != v:
                                    print(f"  EXPECTATION FAILED: result.{k} != {v}")
                                    return False
                time.sleep(step.get("post_delay", 0.05))
            elif stype == "wait_evt":
                try:
                    # step 내 메시지 타입 필드는 충돌을 피하기 위해 `msg_type`으로 명명합니다.
                    hdr, evt = self.wait_evt(match_topic=step.get("topic"), match_type=step.get("msg_type"), timeout=step.get("timeout", 5.0))
                except Exception as ex:
                    print(f"  ERROR waiting evt: {ex}")
                    return False
                print("  EVT hdr:", hdr)
                print("  EVT body:", json.dumps(evt, ensure_ascii=False, default=str))
            elif stype == "sleep":
                s = float(step.get("duration", 1.0))
                print(f"  sleeping {s}s")
                time.sleep(s)
            else:
                print(f"  Unknown step type: {stype}")
        return True


def main():
    p = argparse.ArgumentParser()
    p.add_argument("scenario", type=Path)
    p.add_argument("--validate-only", action="store_true", help="시나리오 파일의 형식만 검사 후 종료")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=25000)
    p.add_argument("--timeout", type=float, default=2.0)
    args = p.parse_args()

    scf = args.scenario
    if not scf.exists():
        print("scenario not found:", scf)
        raise SystemExit(2)
    scenario = json.loads(scf.read_text(encoding="utf-8"))

    # 시나리오 포맷 검사
    errors = validate_scenario(scenario)
    if errors:
        print("SCENARIO VALIDATION ERRORS:")
        for e in errors:
            print(" -", e)
    else:
        print("Scenario validation: OK")
    if args.validate_only:
        raise SystemExit(0 if not errors else 4)

    runner = ScenarioRunner(host=args.host, port=args.port, timeout=args.timeout)
    ok = runner.run_scenario(scenario)
    print("SCENARIO RESULT:", "OK" if ok else "FAILED")
    raise SystemExit(0 if ok else 3)


if __name__ == "__main__":
    main()
