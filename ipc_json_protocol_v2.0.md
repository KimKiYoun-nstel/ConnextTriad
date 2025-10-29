# IPC JSON/CBOR 프로토콜 (v2.0)

  이 문서는 `RtpDdsGateway`의 `IpcAdapter` 구현(`RtpDdsGateway/src/ipc_adapter.cpp`)을 기준으로 정의한
  JSON/CBOR 기반 IPC 프로토콜 규격(v2.0)입니다. 문서 언어는 한국어(설명)이며 코드 식별자는 영문으로 표기합니다.

  수정일: 2025-10-20

  요약: 이 규격은 UI ↔ Gateway 간의 명령(Request)/응답(Response) 및 DDS 샘플 알림(Event)을 JSON 객체로 표현하고,
  전송 바디는 CBOR로 직렬화하여 IPC 프레임에 실어 전달하는 방식을 정의합니다. `corr_id`(상관 ID)를 통해 REQ↔RSP를 매칭합니다.

  ----------------------------

## 1. 개요

- 전송 레이어: 내부 IPC 프레임 (dkmrtp::ipc). 메시지 바디는 JSON 객체를 CBOR(binary)로 직렬화한 것을 권장합니다.
- 프레임 유형(IPC 전달 계층):
  - REQ: `dkmrtp::ipc::MSG_FRAME_REQ` (UI -> Gateway 요청)
  - RSP: `dkmrtp::ipc::MSG_FRAME_RSP` (Gateway -> UI 응답)
  - EVT: `dkmrtp::ipc::MSG_FRAME_EVT` (Gateway -> UI 이벤트/알림)
- 흐름: UI가 REQ(CBOR(JSON))를 송신 → Gateway(install_callbacks의 on_request)에서 수신 → `post_cmd_`로 비동기 큐에 전달 → async consumer가 처리 후 `process_request` 호출 → 처리 결과를 RSP로 CBOR 응답
- DDS 샘플 수신은 비동기 이벤트(`async::SampleEvent`)로 처리되어 `emit_evt_from_sample`에서 EVT 프레임으로 전송됩니다.
- 주요 제약/행동:
  - 요청/응답은 항상 JSON 객체여야 하며, 전송은 CBOR 권장
  - 응답은 항상 `ok`(boolean)를 포함
  - 오류 발생 시 `err`(int), `msg`(string) 및 필요 시 `category`(int)를 포함
  - 이벤트 데이터는 `display`(미리보기, 길이 제한)와 `data`(전체 JSON 데이터)를 포함
  - `corr_id`는 요청/응답 매칭에 사용됩니다.

  ----------------------------

## 2. 공통 규칙

- 바디 인코딩: CBOR 권장 (lib: nlohmann::json::to_cbor / from_cbor 사용). 전송 바디는 CBOR 바이트 배열입니다.
- 파싱/예외: 수신 시 CBOR -> JSON 디코딩 도중 예외가 발생하면 `err=7` 또는 적절한 에러 응답을 반환합니다.
- 필수 필드 및 타입 검증: 핸들러는 필수 필드 누락 시 `err=6` (Bad request)로 응답합니다.
- QoS 문자열: `Lib::Profile` 형식(예: `TriadQosLib::DefaultReliable`) — 코드에서는 `::`로 분리하여 `lib`와 `prof`로 나눕니다.
- corr_id 사용법:
  - 요청 수신: IPC Header의 `corr_id`를 유지하여 처리 후 동일한 `corr_id`로 응답 전송
  - EVT 전송 시 corr_id는 보통 0(또는 무관)로 전송
- 로깅/플로우 모니터링: IpcAdapter는 IN/OUT flow 로그를 남깁니다 (비정상 payload의 경우 표시 제한).
- display(미리보기) 제약: `emit_evt_from_sample`에서 display는 `dump()`의 출력으로 생성되며 최대 2048바이트로 잘립니다.

  ----------------------------

## 3. 요청(Request) — 공통 구조

  모든 요청은 CBOR로 인코딩된 JSON 객체입니다. 기본 스키마:

  ```
  {
    "op": "<operation>",         // string, 필수
    "target": { ... },            // object, 선택 (생성/조작 대상)
    "args": { ... },              // object, 선택 (추가 인자)
    "data": { ... }               // object, 선택 (전송 데이터)
  }
  ```

  설명:

- `op` (string): 수행할 동작 식별자. IpcAdapter에 구현된 op: `create`, `write`, `clear`, `hello`, `get`
- `target` (object): 작업 대상 메타
  - 공통 하위 필드:
    - `kind`: "participant" | "publisher" | "subscriber" | "writer" | "reader" | "qos" (string)
    - `topic`: (writer/reader 대상) topic name (string)
    - `type`: (writer/reader 대상) type name (string)
- `args` (object): 동작별 인자 (아래에 상세화)
- `data` (object): 주로 `write` 같은 동작에서 사용되는 실제 데이터

  ----------------------------

## 4. 응답(Response) — 공통 구조

  응답 또한 CBOR로 인코딩된 JSON 객체입니다. 공통 필드:

  ```
  {
    "ok": <bool>,                 // 항상 포함
    // 성공일 때
    "result": { ... },            // object|array optional
    // 실패일 때
    "err": <int>,                 // error code
    "msg": "...",               // human readable message
    "category": <int>             // DdsResult::category (optional, from DdsManager)
  }
  ```

  에러 코드(현재 구현에서 사용되는 값):

- 4: 내부 처리/리소스 생성 실패 (DdsResult 실패에 대응)
- 6: 잘못된 요청(필수 필드 누락 등)
- 7: JSON/CBOR 파싱 또는 내부 예외

  참고: 구현은 `DdsResult`를 반환할 때 `res.ok`, `res.category`, `res.reason`을 RSP에 포함시킵니다.

  ----------------------------

## 5. 이벤트(Event — MSG_FRAME_EVT)

  Gateway가 DDS 샘플을 수신했을 때 UI로 전송하는 구조:

  ```
  {
    "evt": "data",
    "topic": "<topic name>",
    "type": "<type name>",
    "display": <json|null>,    // preview: string 또는 object, 없으면 null
    "data": <json object>      // 전체 샘플을 JSON으로 변환한 값
  }
  ```

- `evt`는 고정값 "data"
- `display`는 `emit_evt_from_sample`에서 `dds_to_json` 결과를 `display.dump()`로 만들고 최대 2048바이트로 잘라 사용
- `data`는 타입별 변환 결과(예: {"text":"Hello"})

  EVT 전송 시 IPC corr_id는 일반적으로 0으로 전송되므로 요청/응답 매칭 대상이 아닙니다.

  ----------------------------

## 6. 구현별 동작 및 필드 상세

  아래는 `ipc_adapter.cpp`의 구현을 기반으로 각 `op`에 대해 명확히 규정한 요청/응답 스펙과 예시입니다.

  참고: 모든 예시는 JSON 형태로 기술되며 전송 시 CBOR로 직렬화됩니다.

### 6.1 op = "create"

- 목적: DDS 엔티티 생성
- target.kind 별 동작:
  - participant: `DdsManager::create_participant(domain, lib, prof)`
  - publisher: `DdsManager::create_publisher(domain, pub, lib, prof)`
  - subscriber: `DdsManager::create_subscriber(domain, sub, lib, prof)`
  - writer: `DdsManager::create_writer(domain, pub, topic, type, lib, prof)`
  - reader: `DdsManager::create_reader(domain, sub, topic, type, lib, prof)`

  요청 예 (participant 생성):

  REQ (JSON)

  ```json
  {
    "op": "create",
    "target": { "kind": "participant" },
    "args": { "domain": 0, "qos": "TriadQosLib::DefaultReliable" }
  }
  ```

  성공 응답 (RSP)

  ```json
  {
    "ok": true,
    "result": { "action":"participant created", "domain": 0 }
  }
  ```

  실패 응답 (RSP) — DdsResult 실패

  ```json
  {
    "ok": false,
    "err": 4,
    "category": 2,
    "msg": "creation failed reason"
  }
  ```

  주의사항 및 유효성 검사:

- writer/reader 생성은 `target.topic` 및 `target.type`가 반드시 필요합니다. 없으면 `err=6`과 함께 메시지 반환.
- `qos`는 선택적이며 기본값은 `TriadQosLib::DefaultReliable`
- 코드에서 `qos`는 `"lib::profile"` 형태로 분해되어 `lib`/`prof`로 전달됩니다。

### 6.2 op = "write"

- 목적: 간단 텍스트 publish 예시(또는 확장 가능한 데이터 publish)
- 동작: `DdsManager::publish_text(topic, text)` (현재 구현)

  요청 예 (텍스트 publish):
  REQ

  ```json
  {
    "op": "write",
    "target": { "kind": "writer", "topic": "chat" },
    "data": { "text": "Hello world" }
  }
  ```

  성공 응답

  ```json
  {
    "ok": true,
    "result": { "action": "publish ok", "topic": "chat" }
  }
  ```

  실패 응답

  ```json
  {
    "ok": false,
    "err": 4,
    "category": 3,
    "msg": "publish failed"
  }
  ```

  유효성 검사:

- `target.topic` 필수. 누락 시 `err=6` 리턴
- 향후 `data` 필드 내 구조(예: complex types)는 `DdsManager`의 publish API 확장에 따릅니다。

### 6.3 op = "clear"

- 목적: 리소스 정리(e.g., clear DDS entities)
- 구현: `mgr_.clear_entities()`

  REQ (예)

  ```json
  {
    "op": "clear",
    "target": "dds_entities"
  }
  ```

  성공 응답

  ```json
  { "ok": true, "result": { "action": "dds entities cleared" } }
  ```

### 6.4 op = "hello"

- 목적: Gateway 기능/프로토콜/Capabilities 확인
- 구현: `build_hello_capabilities()` 호출 후 응답

  REQ

  ```json
  { "op": "hello" }
  ```

  RSP (요약)

  ```json
  {
    "ok": true,
    "result": {
      "proto": 1,
      "cap": [
        { "name": "create.participant", "example": { } },
        { "name": "create.writer", "example": { } },
        { "name": "write", "example": { } },
        { "name": "get.qos", "example": { } },
        { "name": "evt.data", "description": "DDS sample events" }
      ]
    }
  }
  ```

  `cap` 항목은 구조화된 예제(`example`)를 포함하여 UI가 어떤 요청을 구성해야 하는지 정확히 알 수 있도록 합니다。

### 6.5 op = "get"

- 목적: 리소스 조회 (현재 구현은 QoS 목록 조회)
- 구현: `mgr_.list_qos_profiles(include_builtin, include_detail)`

  요청 예

  ```json
  {
    "op": "get",
    "target": { "kind": "qos" },
    "args": { "include_builtin": true, "detail": false }
  }
  ```

  성공 응답(간단)

  ```json
  {
    "ok": true,
    "result": [ "TriadQosLib::DefaultReliable", "TriadQosLib::BestEffort" ]
  }
  ```

  성공 응답(디테일 포함)

  ```json
  {
    "ok": true,
    "result": [ "TriadQosLib::DefaultReliable" ],
    "detail": [ { "TriadQosLib::DefaultReliable": { } } ]
  }
  ```

  예외 처리:

- Manager 호출 중 예외 발생 시 `ok=false`, `err=4`, `msg="failed to build qos list"`

  ----------------------------

## 7. EVT (DDS sample) 예시

  간단한 텍스트 메시지 이벤트:

  ```json
  {
    "evt": "data",
    "topic": "chat",
    "type": "StringMsg",
    "display": "Hello world",
    "data": { "text": "Hello world" }
  }
  ```

  복잡 타입 예시(구조체):

  ```json
  {
    "evt": "data",
    "topic": "Telemetry",
    "type": "Telemetry_v1",
    "display": { "alt": 123.4, "spd": 42 },
    "data": { "alt": 123.4, "spd": 42, "meta": { } }
  }
  ```

  전송: 위 JSON을 CBOR로 직렬화하여 `ipc_.send_frame(dkmrtp::ipc::MSG_FRAME_EVT, 0, out.data(), out.size())` 호출

  ----------------------------

## 8. 오류 처리/매핑 상세

- 4: 내부 처리 실패 / DdsResult 실패 — `category` 필드 포함 가능
- 6: 잘못된 요청(필드 누락/invalid arguments)
- 7: 파싱/디코딩 에러 또는 내부 예외

  구현 주의사항:

- `process_request` 내부에서 예외 발생 시 `err=7`(json/cbor error)로 응답
- `post_cmd_`가 null인 경우 `install_callbacks`의 on_request는 즉시 에러 응답을 보내고 리턴(예시: `{"ok":false,"err":7,"msg":"no command sink"}`)

  ----------------------------

## 9. 형식/버전 관리 및 확장성

- 버전 표기: hello 응답의 `result.proto`는 프로토콜 메이저 버전. 현재 값은 1(하위 호환 유지). 앞으로 major 변경 시 proto를 2로 올리고 v2.0 문서와 매핑할 것.
- 확장: 새로운 `op` 추가 후 `build_hello_capabilities()`에 capability 항목을 추가해 UI가 자동으로 인지하도록 권장

  ----------------------------

## 10. 구현자 노트 (리마인더)

- `ipc_adapter`는 들어오는 바디를 먼저 `nlohmann::json::from_cbor`로 디코딩한 뒤 내부 로직에 따라 `mgr_` 호출을 함。
- `emit_evt_from_sample` 내부는 `std::any`에서 `shared_ptr<void>`를 꺼내 `dds_to_json(type_name, sample_ptr, json)`을 호출해 `data`를 구성함。 `display`는 `data`의 dump()를 축약한 것。
- 로그: IN/OUT flow에 대해 `LOG_WRN("FLOW", ...)` 또는 `LOG_DBG/LOG_INF` 등을 남깁니다。 UI/운영팀은 로그 레벨을 기준으로 트래픽을 모니터링 가능。

  ----------------------------

## 11. 전체 요청/응답 예시 모음

  1) participant 생성
  REQ

  ```json
  { "op":"create", "target":{ "kind":"participant" }, "args":{ "domain":1, "qos":"TriadQosLib::DefaultReliable" } }
  ```

  RSP

  ```json
  { "ok":true, "result":{ "action":"participant created", "domain":1 } }
  ```

  2) writer 생성 (필수: topic/type)
  REQ

  ```json
  { "op":"create", "target":{ "kind":"writer", "topic":"ExampleTopic", "type":"ExampleType" }, "args":{ "domain":0, "publisher":"pub1", "qos":"TriadQosLib::DefaultReliable" } }
  ```

  RSP (성공)

  ```json
  { "ok":true, "result":{ "action":"writer created", "domain":0, "publisher":"pub1", "topic":"ExampleTopic", "type":"ExampleType" } }
  ```

  RSP (실패, 누락)

  ```json
  { "ok":false, "err":6, "msg":"Missing topic or type tag" }
  ```

  3) write (publish text)
  REQ

  ```json
  { "op":"write", "target":{"kind":"writer","topic":"chat"}, "data":{"text":"Hello world"} }
  ```

  RSP

  ```json
  { "ok":true, "result":{"action":"publish ok","topic":"chat"} }
  ```

  4) get qos
  REQ

  ```json
  { "op":"get", "target":{"kind":"qos"}, "args":{ "include_builtin": true, "detail": true } }
  ```

  RSP

  ```json
  { "ok":true, "result":["TriadQosLib::DefaultReliable"], "detail":[ { "TriadQosLib::DefaultReliable": { } } ] }
  ```

  5) hello
  REQ

  ```json
  { "op":"hello" }
  ```

  RSP

  ```json
  { "ok":true, "result":{ "proto":1, "cap":[ {"name":"create.participant","example":{}}, {} ] } }
  ```

  6) clear dds entities
  REQ

  ```json
  { "op":"clear", "target":"dds_entities" }
  ```

  RSP

  ```json
  { "ok":true, "result":{"action":"dds entities cleared"} }
  ```

  7) EVT sample (StringMsg)
  EVT

  ```json
  { "evt":"data", "topic":"chat", "type":"StringMsg", "display":"Hello world", "data":{"text":"Hello world"} }
  ```

  ----------------------------

## 12. 체크리스트 (검증)

- [ ] 모든 REQ 예제는 CBOR로 직렬화 후 IPC 프레임(REQ)로 전송 가능해야 함
- [ ] 모든 RSP 예제는 Gateway에서 적절한 corr_id로 RSP 프레임을 통해 반환되어야 함
- [ ] EVT 예제는 DDS 샘플 수신 시 CBOR로 전송되어야 함
- [ ] `build_hello_capabilities()`의 내용은 실제 구현과 동기화되어야 함
- [ ] `dds_to_json` 변환 실패 시 `display=null` 및 `data`는 빈 object로 처리되는 현재 동작을 문서에 반영(이미 반영됨)

  ----------------------------

## 부록: 기계적 검증 스키마 및 예제

- JSON Schema: `docs/ipc_protocol_schema_v2.json`
  - 목적: REQ/RSP/EVT 본문을 자동 검증하기 위한 기계 판독 스키마입니다. UI가 요청을 생성하기 전에 검증하거나, 수신 측에서 페이로드를 검증하는 데 사용합니다.
- 예제 스크립트: `examples/ipc_cbor_example.py`
  - 목적: 로컬에서 JSON → CBOR 변환, CBOR → JSON 복원, 그리고 스키마 검증을 데모하기 위한 스크립트입니다. 개발자가 전송 바디를 만들고 검증하는 방법을 빠르게 확인할 수 있게 합니다.

간단 실행 (Windows PowerShell):

```powershell
# repo 루트에서
python -m pip install -r .\examples\requirements.txt
python .\examples\ipc_cbor_example.py
```

예상 출력(요약):

- Request JSON: { ... }
- Request validated against schema (request).
- CBOR bytes length: <n>
- Decoded JSON: { ... }
- Response validated.
- Response CBOR len: <m>

노트: 예제는 로컬 환경에서의 문서 검증 및 인코딩/디코딩 데모용입니다. 실제 IPC 전송은 프로젝트의 IPC 계층(`ipc_.send_frame`)을 통해 이진 CBOR 바이트를 전송해야 합니다.

  문서 끝.
