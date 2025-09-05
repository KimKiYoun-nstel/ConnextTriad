# TOPICS\_AND\_COMMANDS.md — 초안 (카탈로그)

> 목적: Copilot과 개발자가 동일한 **데이터 계약(Topics/Types)** 및 \*\*명령 규격(UI↔DDS)\*\*을 공유하도록 하는 단일 진실원천(Single Source of Truth).
> 언어 정책: 서술은 **한국어**, 코드/식별자는 **영문** 유지.

---

## 1. Topic / Type 카탈로그 (Authoritative)

| Topic(실명)      | IDL Type  | Header(ref)           | Direction | Domain | Partition | QoS(Lib::Profile)            | Key Fields | Converter                | Registry Key |
| -------------- | --------- | --------------------- | --------- | ------ | --------- | ---------------------------- | ---------- | ------------------------ | ------------ |
| `<TBD_Alarm>`  | AlarmMsg  | generated/AlarmMsg.h  | Pub/Sub   | 0      | default   | TriadQosLib::DefaultReliable | `id?`      | DataConverter<AlarmMsg>  | `alarm`      |
| `<TBD_String>` | StringMsg | generated/StringMsg.h | Pub/Sub   | 0      | default   | TriadQosLib::DefaultReliable | —          | DataConverter<StringMsg> | `text`       |
| P_Alarms_PSM::C_Actual_Alarm | P_Alarms_PSM::C_Actual_Alarm | generated/Alarms_PSM.h | Pub/Sub | 0 | default | TriadQosLib::DefaultReliable | 복합키(see IDL) | DataConverter<P_Alarms_PSM::C_Actual_Alarm> | actual_alarm |

> 규칙
>
> * 생성물은 **헤더(`generated/*.h`)만 참조**, **소스(`*.cxx`, `*Plugin.*`, `*Support.*`)는 참조/수정 금지**.
> * Topic 이름/Domain/Partition/QoS/Key는 실제 값으로 채워 넣는다.

### 1.1 QoS 프로파일 표

* `TriadQosLib::DefaultReliable` (기본)
* `TriadQosLib::LowLatency`
* `TriadQosLib::BestEffort`

> 필요 시 프로젝트 표준에 맞게 추가/수정.

---

## 2. UI ↔ Gateway 명령/메시지 규격 (Single Source of Truth)

### 2.1 공통 Envelope 구조 (REQ/RESP/EVT)

* `op`: string (예: "create", "write" 등, 수행할 동작)
* `target`: object (대상 리소스 정보)
  * `kind`: string (예: "participant", "publisher", "subscriber", "writer", "reader" 등)
  * `topic`: string (writer/reader 생성/쓰기 시 필수)
  * `type`: string (writer/reader 생성 시 필수)
* `args`: object (동작별 파라미터)
  * `domain`: int (도메인 ID)
  * `publisher`: string (publisher 이름)
  * `subscriber`: string (subscriber 이름)
  * `qos`: string (예: "TriadQosLib::DefaultReliable")
* `data`: object (write 시 payload, 예: { "text": "..." })
* `corr_id`: int (요청-응답 매칭용, 옵션)

#### Response/ACK 예시

```json
{
  "ok": true,
  "result": { "action": "writer created", ... },
  "corr_id": 1001
}
```

#### Error 예시

```json
{
  "ok": false,
  "err": 4,
  "msg": "Missing topic or type tag",
  "corr_id": 1001
}
```

#### Error codes

| code | 의미                |
|-----:|---------------------|
| 0    | OK                  |
| 4    | Logic/Validation    |
| 6    | MissingTag          |
| 400  | BadArgs(인자 오류)  |
| 404  | NotFound            |
| 409  | Exists              |
| 501  | NotImplemented      |
| 503  | DdsError            |

### 2.2 지원 op / target.kind

* `create`: `participant` | `publisher` | `subscriber` | `writer` | `reader`
* `destroy`: `participant` | `publisher` | `subscriber` | `writer` | `reader`
* `write`:   `writer`
* `subscribe` / `unsubscribe`: `reader`

### 2.3 주요 메시지 예시

#### (1) Participant 생성

```json
{
  "op": "create",
  "target": { "kind": "participant" },
  "args": { "domain": 0, "qos": "TriadQosLib::DefaultReliable" },
  "corr_id": 1001
}
```

#### (2) Publisher 생성

```json
{
  "op": "create",
  "target": { "kind": "publisher" },
  "args": { "domain": 0, "publisher": "pub1", "qos": "TriadQosLib::DefaultReliable" },
  "corr_id": 1002
}
```

#### (3) Writer 생성

```json
{
  "op": "create",
  "target": { "kind": "writer", "topic": "MyTopic", "type": "StringMsg" },
  "args": { "domain": 0, "publisher": "pub1", "qos": "TriadQosLib::DefaultReliable" },
  "corr_id": 1003
}
```

#### (4) 데이터 쓰기 (write)

```json
{
  "op": "write",
  "target": { "kind": "writer", "topic": "MyTopic", "type": "StringMsg" },
  "data": { "text": "Hello, world!" },
  "corr_id": 2001
}
```

#### (5) Subscribe/Unsubscribe

```json
{
  "op": "subscribe",
  "target": { "kind": "reader", "topic": "MyTopic", "type": "StringMsg" },
  "args": { "domain": 0 },
  "corr_id": 3001
}
```

```json
{
  "op": "unsubscribe",
  "target": { "kind": "reader", "topic": "MyTopic", "type": "StringMsg" },
  "args": { "domain": 0 },
  "corr_id": 3002
}
```

### 2.4 필수/옵션 Tag 정리

* `op`: 항상 필수
* `target.kind`: 항상 필수
* `target.topic/type`: writer/reader 생성/쓰기 시 필수
* `args.domain`: 대부분의 생성 요청에 필수
* `args.publisher/subscriber`: publisher/subscriber 생성 시 필수
* `args.qos`: 옵션(기본값 있음)
* `data`: write 시 필수
* `corr_id`: 권장(요청-응답 매칭)

### 2.5 기타 참고사항

- 모든 메시지는 CBOR로 직렬화되어 IPC로 송수신, gateway 내부에서는 nlohmann::json으로 파싱
* 오류 응답 시: `{ "ok": false, "err": <code>, "msg": "<reason>" }` 형태로 반환
* 실제 payload 구조(예: AlarmMsg 등)는 각 topic/type에 따라 별도 정의됨

---

## 3. EVT(이벤트) 페이로드 규격 (DDS→UI)

* 공통 래퍼

```json
{
  "evt": "data",
  "topic": "<TopicName>",
  "type": "<IDLType>",
  "domain": 0,
  "data": { /* 타입별 필드 */ },
  "ts": "2025-08-29T12:00:00Z"
}
```

* `AlarmMsg` 예시

```json
{
  "evt": "data",
  "topic": "<TBD_Alarm>",
  "type": "AlarmMsg",
  "domain": 0,
  "data": { "id": 1, "level": 2, "text": "..." },
  "ts": "2025-08-29T12:00:00Z"
}
```

* `StringMsg` 예시

```json
{
  "evt": "data",
  "topic": "<TBD_String>",
  "type": "StringMsg",
  "domain": 0,
  "data": { "text": "hello" },
  "ts": "2025-08-29T12:00:00Z"
}
```

* `P_Alarms_PSM::C_Actual_Alarm` 예시 (신규 topic type)

```json
{
  "evt": "data",
  "topic": "P_Alarms_PSM::C_Actual_Alarm",
  "type": "P_Alarms_PSM::C_Actual_Alarm",
  "domain": 0,
  "data": {
    "A_sourceID": { "id": "SRC001" },
    "A_timeOfDataGeneration": { "sec": 1693900000, "nanosec": 123456789 },
    "A_dateTimeRaised": { "sec": 1693900000, "nanosec": 123456789 },
    "A_raisingCondition_sourceID": { "id": "COND01" },
    "A_alarmCategory_sourceID": { "id": "CAT01" },
    "A_alarmText": "Alarm text..."
  },
  "ts": "2025-08-29T12:00:00Z"
}
```

---

## 4. 버전 관리 / 변경 원칙

* 이 문서의 스키마가 **단일 진실원천**. 변경 시 먼저 여기 갱신 → 코드 반영.
* 파괴적 변경은 메이저 버전 업, 기존 필드는 가급적 `deprecated`로 표시 후 단계적 제거.

---

## 5. 컴플라이언스 체크

* [ ] NGVA/DDS 표준 위반 없음
* [ ] `dds_manager` 외 직접 DDS 엔티티 조작 금지
* [ ] `generated/*.h`만 참조, `*.cxx`/`*Plugin.*`/`*Support.*` 참조 금지
* [ ] 신규 기능은 중복 금지, 기존 로직 재사용/확장 우선

---

## 6. TODO (채우기)

* [ ] 실제 Topic 이름 확정 (`<TBD_Alarm>`, `<TBD_String>` 대체)
* [ ] 각 Topic의 Domain/Partition/QoS 채움
* [ ] `AlarmMsg` 키필드 정의
* [ ] UI 명령 전체 목록 확정(op/args/에러코드 구체화)
