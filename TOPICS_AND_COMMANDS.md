# TOPICS\_AND\_COMMANDS.md — 초안 (카탈로그)

> 목적: Copilot과 개발자가 동일한 **데이터 계약(Topics/Types)** 및 \*\*명령 규격(UI↔DDS)\*\*을 공유하도록 하는 단일 진실원천(Single Source of Truth).
> 언어 정책: 서술은 **한국어**, 코드/식별자는 **영문** 유지.

---

## 1. Topic / Type 카탈로그 (Authoritative)

| Topic(실명)      | IDL Type  | Header(ref)           | Direction | Domain | Partition | QoS(Lib::Profile)            | Key Fields | Converter                | Registry Key |
| -------------- | --------- | --------------------- | --------- | ------ | --------- | ---------------------------- | ---------- | ------------------------ | ------------ |
| `<TBD_Alarm>`  | AlarmMsg  | generated/AlarmMsg.h  | Pub/Sub   | 0      | default   | TriadQosLib::DefaultReliable | `id?`      | DataConverter<AlarmMsg>  | `alarm`      |
| `<TBD_String>` | StringMsg | generated/StringMsg.h | Pub/Sub   | 0      | default   | TriadQosLib::DefaultReliable | —          | DataConverter<StringMsg> | `text`       |

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

## 2. UI ↔ DDS 명령 카탈로그 (Spec)

### 2.1 공통 스키마

* **Request**

```json
{
  "op": "string",
  "target": { "kind": "string" },
  "args": { },
  "corr_id": 0
}
```

* **Response**

```json
{
  "ack": true,
  "code": 0,
  "msg": "",
  "data": {},
  "corr_id": 0
}
```

* **Error codes**
  \| code | 의미                |
  \|-----:|---------------------|
  \| 0    | OK                  |
  \| 400  | BadArgs(인자 오류)  |
  \| 404  | NotFound            |
  \| 409  | Exists              |
  \| 501  | NotImplemented      |
  \| 503  | DdsError            |

### 2.2 지원 op / target.kind

* `create`: `participant|publisher|subscriber|topic|writer|reader`
* `destroy`: `participant|publisher|subscriber|topic|writer|reader`
* `write`:   `writer`
* `subscribe` / `unsubscribe`: `reader`

### 2.3 예시

**1) Participant 생성**

```json
{
  "op": "create",
  "target": { "kind": "participant" },
  "args": { "domain": 0, "qos": "TriadQosLib::DefaultReliable" },
  "corr_id": 1001
}
```

**2) Topic/Writer/Reader 생성**

```json
{
  "op": "create",
  "target": { "kind": "topic", "name": "<TBD_Alarm>", "type": "AlarmMsg" },
  "args": { "domain": 0, "qos": "TriadQosLib::DefaultReliable" },
  "corr_id": 1004
}
```

```json
{
  "op": "create",
  "target": { "kind": "writer", "topic": "<TBD_Alarm>", "type": "AlarmMsg" },
  "args": { "domain": 0 },
  "corr_id": 1005
}
```

```json
{
  "op": "create",
  "target": { "kind": "reader", "topic": "<TBD_Alarm>", "type": "AlarmMsg" },
  "args": { "domain": 0 },
  "corr_id": 1006
}
```

**3) Publish(write)**

```json
{
  "op": "write",
  "target": { "kind": "writer", "topic": "<TBD_String>", "type": "StringMsg" },
  "args": { "domain": 0, "payload": { "text": "hello" } },
  "corr_id": 2001
}
```

**4) Subscribe/Unsubscribe**

```json
{
  "op": "subscribe",
  "target": { "kind": "reader", "topic": "<TBD_Alarm>", "type": "AlarmMsg" },
  "args": { "domain": 0 },
  "corr_id": 3001
}
```

```json
{
  "op": "unsubscribe",
  "target": { "kind": "reader", "topic": "<TBD_Alarm>", "type": "AlarmMsg" },
  "args": { "domain": 0 },
  "corr_id": 3002
}
```

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
