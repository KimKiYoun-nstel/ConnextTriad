# set.qos 명령 사용 예시

## 0. get.qos와 set.qos 연계 워크플로우

UI에서 기존 QoS 프로파일을 수정하거나 복제하려면 다음 순서로 작업합니다:

### 단계 1: 기존 프로파일 조회 (get.qos)

```json
{
  "op": "get",
  "target": { "kind": "qos" },
  "args": { "detail": true }
}
```

응답:

```json
{
  "ok": true,
  "result": ["NGVA_QoS_Library::control_low_latency_reliable", "..."],
  "detail": [
    {
      "NGVA_QoS_Library::control_low_latency_reliable": {
        "source_kind": "external",
        "xml": "<qos_profile name=\"control_low_latency_reliable\" base_name=\"NetLib::BaseUdpOnly\">\n  <datawriter_qos>\n    <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>\n    <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>1</depth></history>\n    <resource_limits>\n      <max_samples>32</max_samples>\n      <max_instances>1</max_instances>\n      <max_samples_per_instance>32</max_samples_per_instance>\n    </resource_limits>\n    <deadline><period><sec>0</sec><nanosec>100000000</nanosec></period></deadline>\n    <latency_budget><duration><sec>0</sec><nanosec>3000000</nanosec></duration></latency_budget>\n    <liveliness>\n      <kind>AUTOMATIC_LIVELINESS_QOS</kind>\n      <lease_duration>\n        <sec>DURATION_INFINITE_SEC</sec>\n        <nanosec>DURATION_INFINITE_NSEC</nanosec>\n      </lease_duration>\n    </liveliness>\n    <ownership><kind>EXCLUSIVE_OWNERSHIP_QOS</kind></ownership>\n    <ownership_strength><value>0</value></ownership_strength>\n    <transport_priority><value>100</value></transport_priority>\n  </datawriter_qos>\n  <datareader_qos>...</datareader_qos>\n  <topic_qos>...</topic_qos>\n</qos_profile>"
      }
    }
  ]
}
```

### 단계 2: XML 수정

- `detail[].xml` 필드에서 원하는 프로파일의 XML을 추출
- XML을 파싱하여 원하는 값 수정 (예: `<depth>1</depth>` → `<depth>5</depth>`)
- 또는 profile name을 변경하여 새 프로파일로 저장

### 단계 3: 수정된 XML로 set.qos 호출

```json
{
  "op": "set",
  "target": { "kind": "qos" },
  "data": {
    "library": "NGVA_QoS_Library",
    "profile": "control_low_latency_reliable",  // 기존 이름 → 업데이트 / 새 이름 → 복제
    "xml": "<qos_profile name=\"control_low_latency_reliable\" base_name=\"NetLib::BaseUdpOnly\">...(수정된 XML)...</qos_profile>"
  }
}
```

응답:

```json
{
  "ok": true,
  "result": {
    "action": "qos profile updated",
    "profile": "NGVA_QoS_Library::control_low_latency_reliable"
  }
}
```

### 장점

- **UI는 복잡한 QoS 구조를 직접 생성할 필요 없음**
- **기존 프로파일을 템플릿으로 활용**
- **XML 편집만으로 세밀한 조정 가능**
- **XML에 주요 10개 정책 포함**: reliability, durability, ownership, history, resource_limits, deadline, latency_budget, liveliness, ownership_strength, transport_priority
- **선택적**: discovery/runtime JSON 요약 필드는 코드 주석 해제로 활성화 가능 (현재 비활성화)

---

## 1. 새로운 Custom Profile 추가

### 요청 (간결한 XML)

```json
{
  "op": "set",
  "target": { "kind": "qos" },
  "data": {
    "library": "NGVA_QoS_Library",
    "profile": "custom_high_throughput",
    "xml": "<qos_profile name=\"custom_high_throughput\" base_name=\"NetLib::BaseUdpOnly\"><datawriter_qos><reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability><durability><kind>VOLATILE_DURABILITY_QOS</kind></durability><history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>100</depth></history><resource_limits><max_samples>1000</max_samples></resource_limits></datawriter_qos><datareader_qos><reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability><history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>100</depth></history></datareader_qos></qos_profile>"
  }
}
```

### 요청 (포맷팅된 XML - 가독성 우선)

```json
{
  "op": "set",
  "target": { "kind": "qos" },
  "data": {
    "library": "NGVA_QoS_Library",
    "profile": "custom_high_throughput",
    "xml": "<qos_profile name=\"custom_high_throughput\" base_name=\"NetLib::BaseUdpOnly\">\n  <datawriter_qos>\n    <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>\n    <durability><kind>VOLATILE_DURABILITY_QOS</kind></durability>\n    <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>100</depth></history>\n    <resource_limits>\n      <max_samples>1000</max_samples>\n    </resource_limits>\n  </datawriter_qos>\n  <datareader_qos>\n    <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>\n    <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>100</depth></history>\n  </datareader_qos>\n</qos_profile>"
  }
}
```

### 응답

```json
{
  "ok": true,
  "result": {
    "action": "qos profile updated",
    "profile": "NGVA_QoS_Library::custom_high_throughput"
  }
}
```

---

## 2. 기존 Profile 업데이트 (control_low_latency_reliable 수정)

### 원본 XML (NGVA_QoS_Library.xml)

```xml
<qos_profile name="control_low_latency_reliable" is_default_qos="true" base_name="NetLib::BaseUdpOnly">
  <datawriter_qos>
    <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>
    <durability><kind>VOLATILE_DURABILITY_QOS</kind></durability>
    <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>1</depth></history>
    <deadline><period><sec>0</sec><nanosec>100000000</nanosec></period></deadline>
    <latency_budget><duration><sec>0</sec><nanosec>3000000</nanosec></duration></latency_budget>
    <ownership><kind>EXCLUSIVE_OWNERSHIP_QOS</kind></ownership>
    <transport_priority><value>100</value></transport_priority>
  </datawriter_qos>
  <datareader_qos>
    <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>
    <durability><kind>VOLATILE_DURABILITY_QOS</kind></durability>
    <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>1</depth></history>
  </datareader_qos>
</qos_profile>
```

### 요청 (history depth를 1→5로, deadline을 100ms→50ms로 변경)

```json
{
  "op": "set",
  "target": { "kind": "qos" },
  "data": {
    "library": "NGVA_QoS_Library",
    "profile": "control_low_latency_reliable",
    "xml": "<qos_profile name=\"control_low_latency_reliable\" is_default_qos=\"true\" base_name=\"NetLib::BaseUdpOnly\">\n  <datawriter_qos>\n    <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>\n    <durability><kind>VOLATILE_DURABILITY_QOS</kind></durability>\n    <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>5</depth></history>\n    <deadline><period><sec>0</sec><nanosec>50000000</nanosec></period></deadline>\n    <latency_budget><duration><sec>0</sec><nanosec>3000000</nanosec></duration></latency_budget>\n    <ownership><kind>EXCLUSIVE_OWNERSHIP_QOS</kind></ownership>\n    <transport_priority><value>100</value></transport_priority>\n  </datawriter_qos>\n  <datareader_qos>\n    <reliability><kind>RELIABLE_RELIABILITY_QOS</kind></reliability>\n    <durability><kind>VOLATILE_DURABILITY_QOS</kind></durability>\n    <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>5</depth></history>\n  </datareader_qos>\n</qos_profile>"
  }
}
```

### 응답

```json
{
  "ok": true,
  "result": {
    "action": "qos profile updated",
    "profile": "NGVA_QoS_Library::control_low_latency_reliable"
  }
}
```

---

## 3. 새 Library 생성 및 Profile 추가

### 요청

```json
{
  "op": "set",
  "target": { "kind": "qos" },
  "data": {
    "library": "CustomApplicationLib",
    "profile": "video_streaming",
    "xml": "<qos_profile name=\"video_streaming\">\n  <datawriter_qos>\n    <reliability><kind>BEST_EFFORT_RELIABILITY_QOS</kind></reliability>\n    <durability><kind>VOLATILE_DURABILITY_QOS</kind></durability>\n    <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>1</depth></history>\n    <transport_priority><value>150</value></transport_priority>\n  </datawriter_qos>\n  <datareader_qos>\n    <reliability><kind>BEST_EFFORT_RELIABILITY_QOS</kind></reliability>\n    <durability><kind>VOLATILE_DURABILITY_QOS</kind></durability>\n    <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>1</depth></history>\n  </datareader_qos>\n</qos_profile>"
  }
}
```

### 응답

```json
{
  "ok": true,
  "result": {
    "action": "qos profile updated",
    "profile": "CustomApplicationLib::video_streaming"
  }
}
```

---

## 4. 동적 Profile 확인 (get.qos)

### 요청

```json
{
  "op": "get",
  "target": { "kind": "qos" },
  "args": { "detail": true }
}
```

### 응답 (동적 프로파일 포함)

```json
{
  "ok": true,
  "result": [
    "NGVA_QoS_Library::custom_high_throughput",
    "NGVA_QoS_Library::control_low_latency_reliable",
    "CustomApplicationLib::video_streaming",
    "TriadQosLib::DefaultReliable"
  ],
  "detail": [
    {
      "NGVA_QoS_Library::custom_high_throughput": {
        "source_kind": "dynamic",
        "discovery": {},
        "runtime": {}
      }
    },
    {
      "NGVA_QoS_Library::control_low_latency_reliable": {
        "source_kind": "dynamic",
        "discovery": {},
        "runtime": {}
      }
    },
    {
      "CustomApplicationLib::video_streaming": {
        "source_kind": "dynamic",
        "discovery": {},
        "runtime": {}
      }
    },
    {
      "TriadQosLib::DefaultReliable": {
        "source_kind": "external",
        "discovery": {},
        "runtime": {}
      }
    }
  ]
}
```

---

## 5. 동적 Profile로 DDS 엔티티 생성

### Writer 생성

```json
{
  "op": "create",
  "target": { "kind": "writer", "topic": "HighThroughputData", "type": "DataMsg" },
  "args": { "domain": 0, "publisher": "pub1", "qos": "NGVA_QoS_Library::custom_high_throughput" }
}
```

### 응답

```json
{
  "ok": true,
  "result": {
    "action": "writer created",
    "domain": 0,
    "publisher": "pub1",
    "topic": "HighThroughputData",
    "type": "DataMsg",
    "id": 1
  }
}
```

---

## 6. 오류 처리 예시

### 필수 필드 누락

요청:

```json
{
  "op": "set",
  "target": { "kind": "qos" },
  "data": {
    "library": "NGVA_QoS_Library"
  }
}
```

응답:

```json
{
  "ok": false,
  "err": 6,
  "msg": "Missing required fields: library, profile, xml"
}
```

### 잘못된 XML

요청:

```json
{
  "op": "set",
  "target": { "kind": "qos" },
  "data": {
    "library": "NGVA_QoS_Library",
    "profile": "invalid",
    "xml": "<invalid>xml</invalid>"
  }
}
```

응답:

```json
{
  "ok": false,
  "err": 4,
  "msg": "Failed to add/update QoS profile"
}
```

---

## XML 전문 범위 규칙

**반드시 포함해야 하는 XML 범위:**

```xml
<qos_profile name="프로파일명" [속성들...]>
  <datawriter_qos>...</datawriter_qos>
  <datareader_qos>...</datareader_qos>
  [<participant_qos>...</participant_qos>]
  [<publisher_qos>...</publisher_qos>]
  [<subscriber_qos>...</subscriber_qos>]
  [<topic_qos>...</topic_qos>]
</qos_profile>
```

**포함하지 않아야 하는 부분:**

- `<?xml version="1.0" encoding="UTF-8"?>` (XML 선언)
- `<dds>` (최상위 태그)
- `<qos_library>` (라이브러리 태그)

**이유:**

- Gateway는 내부적으로 Library XML을 자동 생성하여 래핑합니다
- `data.library` 파라미터로 Library 이름을 별도 지정합니다
