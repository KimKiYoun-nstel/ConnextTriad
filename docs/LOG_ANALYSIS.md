# Agent 로그 시스템 개선 완료 보고서

## 📊 Before & After

### 로그 레벨별 분포 변화

```
              개선 전         개선 후        변화
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
LOG_DBG  :    35 (14.8%)  →  36 (16.5%)   +1
LOG_INF  :    37 (15.7%)  →  36 (16.5%)   -1
LOG_FLOW :    45 (19.1%)  →  27 (12.4%)  -18 ✨
LOG_WRN  :    46 (19.5%)  →  89 (40.8%)  +43 ✨
LOG_ERR  :    73 (30.9%)  →  30 (13.8%)  -43 ✨
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
Total    :   236 (100%)  → 218 (100%)   -18
```

**핵심 개선**:

- ⚠️ ERROR 30.9% → 13.8%: 복구 가능한 오류를 WARN으로 재분류
- 🔍 TRACE 19.1% → 12.4%: 중간 단계 로그 제거, 외부 경계만 유지
- ✅ WARN 19.5% → 40.8%: 설계 의도대로 주요 경고 레벨로 활용

---

## 🎯 로그 레벨별 사용 기준 (확정)

### DEBUG (36회, 16.5%)

**용도**: 내부 로직 디버깅, 상세 변환 추적

**출력 예시**:

```log
[DBG][SampleFactory] json_to_dds: converting JSON to DDS for type=AlarmData
[DBG][DDS] [qos-cache] builtin candidates=12
[DBG][IPC] data json preview={"topic":"Alarms","severity":2,"message":"Engine"}
[DBG][IPC] process_request done corr_id=1234 q_delay(us)=250 exec(us)=1200 rsp_size=128
```

### INFO (36회, 16.5%)

**용도**: 엔티티 생성 완료, QoS 적용 성공, 시스템 초기화

**출력 예시**:

```log
[INF][IPC] participant created: domain=0 qos=TriadQosLib::DefaultReliable
[INF][DDS] create_writer: success domain=0 pub=pub1 topic=Alarms type=AlarmData id=123
[INF][DDS] [apply-qos] writer created with QoS topic=Alarms lib=TriadQosLib prof=Reliable
[INF][IPC] publish_json ok: topic=Alarms
[INF][Gateway] starting mode=server addr=0.0.0.0 port=25000 rx_mode=waitset
```

### TRACE (27회, 12.4%)

**용도**: Agent 외부 경계 추적 - 메시지당 1쌍만 출력

**출력 예시**:

```log
# IPC 메시지 송수신
[TRC][IPC] IN corr_id=1001 msg={"op":"create","target":{"kind":"participant"},"args":{...}}
[TRC][IPC] OUT corr_id=1001 rsp={"ok":true,"result":{"action":"participant created"}}

# DDS 샘플 처리
[TRC][Gateway] sample enq topic=Alarms type=AlarmData seq=5001
[TRC][Gateway] sample exec topic=Alarms type=AlarmData seq=5001 queue_delay_us=180

# IPC 이벤트 전송
[TRC][IPC] OUT evt topic=Alarms type=AlarmData evt={"evt":"data","topic":"Alarms",...}
```

### WARN (89회, 40.8%)

**용도**: 복구 가능한 오류, 사용자 입력 오류, 설계 위반

**출력 예시**:

```log
# 사용자 입력 오류
[WRN][IPC] request parse failed corr_id=1002 error=unexpected token at line 5
[WRN][DDS] create_writer: unknown DDS type: InvalidTypeName
[WRN][IPC] writer creation failed: missing topic or type tag

# 엔티티 미존재 (순서 위반)
[WRN][DDS] create_writer: participant not found for domain=0 (must be created first)
[WRN][DDS] publish_json: topic=Alarms writer not found or invalid type/sample

# 타입 불일치
[WRN][DDS] create_writer: topic='Alarms' already exists with type='AlarmData', cannot create with type='StringMsg'

# 변환 실패
[WRN][SampleFactory] json_to_dds: failed to convert JSON to DDS for type=AlarmData; reason=missing required field 'severity'
[WRN][IPC] dds_to_json failed type=AlarmData

# QoS 적용 실패 (fallback 성공)
[WRN][DDS] [qos-apply-failed] topic=Alarms lib=CustomLib prof=Strict error=incompatible QoS
[WRN][DDS] [apply-qos:default] topic=Alarms (fallback to Default due to qos apply failure)

# 성능 경고
[WRN][ASYNC] high_queue_delay sample topic=Alarms delay_us=8500
```

### ERROR (30회, 13.8%)

**용도**: 진짜 프로그램 오류, 복구 불가능한 심각한 상황

**출력 예시**:

```log
# 내부 일관성 오류 (버그)
[ERR][IPC] AnyData contains null pointer for type=AlarmData
[ERR][IPC] AnyData is not shared_ptr<void> for type=AlarmData

# 초기화 실패
[ERR][DDS] add_or_update_qos_profile: qos_store not initialized

# 최종 fallback 실패
[ERR][DDS] create_writer: failed to create writer (fallback also failed): resource limit exceeded
[ERR][DDS] create_reader: failed to create reader (fallback also failed): incompatible QoS

# RTI DDS 내부 오류
[ERR][RTI] DDS_DataWriter_write failed: OUT_OF_RESOURCES

# WaitSet 예외
[ERR][WaitSetDispatcher] Monitor thread exception: wait failed
[ERR][ASYNC] exec exception=bad_alloc
```

---

## 🔧 RTI Logger 빌드 제어

### 개선 전

```cpp
// main.cpp - 항상 활성화
#define _RTPDDS_DEBUG

// Release 빌드에서도 RTI 로그 출력됨
```

### 개선 후

**CMakeLists.txt**:

```cmake
# Debug 빌드에서만 RTI 로거 활성화
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(RtpDdsGateway PRIVATE _RTPDDS_DEBUG)
endif()
```

**rti_logger_bridge.cpp**:

```cpp
#ifdef _RTPDDS_DEBUG
    lg.verbosity(Verbosity::status_local);  // Debug: 상세 로그
#else
    lg.verbosity(Verbosity::warning);       // Release: 경고만
#endif
```

**실제 출력**:

```log
# Debug 빌드
[INF][RTI] DDS_DomainParticipant_create: domain=0
[INF][RTI] DDS_Publisher_create: name=pub1
[WRN][RTI] DDS_DataWriter_write: sample rejected by resource limits

# Release 빌드
[WRN][RTI] DDS_DataWriter_write: sample rejected by resource limits
# (INFO 레벨은 출력 안 됨)
```

---

## 📁 RTI 로그 파일 분리 (설정 준비 완료)

### agent_config.json

```json
{
    "logging": {
        "file_output": true,
        "log_dir": "logs",
        "file_name": "agent.log",
        "level": "info",
        "console_output": true,
        "max_file_size_mb": 10,
        "max_backup_files": 5,
        "rti_log_file": ""  // 빈 문자열: agent.log 통합 | "rti.log": 별도 파일
    }
}
```

**동작 방식**:

- `"rti_log_file": ""` → Agent + RTI 로그 모두 `agent.log`에 통합
- `"rti_log_file": "rti.log"` → Agent: `agent.log`, RTI: `rti.log` (향후 구현)
- 콘솔 출력은 항상 통합 (Agent + RTI)

---

## 📝 실제 로그 흐름 예시

### 시나리오: Writer 생성 및 데이터 발행

#### TRACE 레벨 (외부 경계만)

```log
[TRC][IPC] IN corr_id=100 msg={"op":"create","target":{"kind":"writer","topic":"Alarms","type":"AlarmData"},...}
[TRC][IPC] OUT corr_id=100 rsp={"ok":true,"result":{"action":"writer created","id":5}}
[TRC][IPC] IN corr_id=101 msg={"op":"write","target":{"kind":"writer","topic":"Alarms"},"data":{...}}
[TRC][IPC] OUT corr_id=101 rsp={"ok":true,"result":{"action":"publish ok"}}
```

#### INFO 레벨 (주요 이벤트)

```log
[INF][IPC] writer created: domain=0 pub=pub1 topic=Alarms type=AlarmData
[INF][DDS] [apply-qos] writer created with QoS topic=Alarms lib=TriadQosLib prof=Reliable
[INF][DDS] create_writer: success domain=0 pub=pub1 topic=Alarms type=AlarmData id=5
[INF][IPC] publish_json ok: topic=Alarms
```

#### DEBUG 레벨 (상세 정보)

```log
[DBG][SampleFactory] json_to_dds: converting JSON to DDS for type=AlarmData
[DBG][SampleFactory] json_to_dds: JSON converted successfully to DDS for type=AlarmData
[DBG][DDS] publish_json: type_name=AlarmData
[DBG][DDS] publish_json: sample created for type=AlarmData
[DBG][DDS] publish_json: json_to_dds succeeded for type=AlarmData
[DBG][IPC] process_request done corr_id=101 q_delay(us)=150 exec(us)=800 rsp_size=64
```

### 시나리오: 오류 발생 시

#### 사용자 오류 → WARN

```log
[TRC][IPC] IN corr_id=200 msg={"op":"create","target":{"kind":"writer"},"args":{...}}
[WRN][IPC] writer creation failed: missing topic or type tag
[TRC][IPC] OUT corr_id=200 rsp={"ok":false,"err":6,"msg":"Missing topic or type tag"}
```

#### 타입 불일치 → WARN

```log
[TRC][IPC] IN corr_id=201 msg={"op":"create","target":{"kind":"writer","topic":"Alarms","type":"WrongType"},...}
[WRN][DDS] create_writer: unknown DDS type: WrongType
[TRC][IPC] OUT corr_id=201 rsp={"ok":false,"err":4,"category":1,"msg":"Unknown DDS type: WrongType"}
```

#### 내부 버그 → ERROR

```log
[TRC][IPC] IN corr_id=300 msg={"op":"write","target":{"kind":"writer","topic":"Alarms"},"data":{...}}
[ERR][IPC] AnyData contains null pointer for type=AlarmData
[TRC][IPC] OUT corr_id=300 rsp={"ok":false,"err":7,"msg":"internal error"}
```

---

## ✅ 완료 항목

1. **LOG_ERR → LOG_WRN 강등 (43개)**
   - 복구 가능한 오류를 WARN으로 재분류
   - 사용자 입력 오류, 엔티티 미존재, 타입 불일치, 변환 실패, QoS 적용 실패

2. **LOG_FLOW 중간 로그 제거 (18개)**
   - IPC 요청 파싱, DdsManager 호출, 엔티티 생성 중간 단계 제거
   - 외부 경계만 유지: IPC IN/OUT, DDS enq/exec

3. **LOG_INF → LOG_DBG 이동 (1개)**
   - process_request done (성능 통계)

4. **RTI Logger CMake 빌드 제어**
   - Debug 빌드에서만 `_RTPDDS_DEBUG` 정의
   - Release: warning만, Debug: status_local까지

5. **RTI 로그 파일 분리 설정 준비**
   - `agent_config.json`에 `rti_log_file` 필드 추가
   - AppConfig 구조체 확장

---

**작성일**: 2025-12-11  
**개선 전 로그 호출**: 236개  
**개선 후 로그 호출**: 218개 (-18개)  
**상태**: ✅ 성능 테스트 준비 완료
