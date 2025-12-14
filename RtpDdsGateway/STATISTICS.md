# 통계 기능 (간단 텍스트 출력)

설계 개요

- 출력 주기: 1분 단위(매분 00초). 콘솔 및 선택적 파일 출력.
- 수집 항목:
  - IPC: IN/OUT 카운트(순수 입/출력 프레임 수)
  - DDS 엔티티(현재 상태): 참여자(participants), publisher, subscriber, writers, readers, topics
  - DDS 메시지(분단위 집계): writer(topic 단위)의 write 수, reader(topic 단위)의 take 수

구현 위치

- 설정: `agent_config.json`에 `statistics` 섹션 추가
- 통계 매니저: `RtpDdsGateway/include/stats_manager.hpp`, `RtpDdsGateway/src/stats_manager.cpp`
- IPC 계측: `RtpDdsGateway/src/ipc_adapter.cpp` (수신/송신 프레임 카운트)
- DDS 계측:
  - `RtpDdsGateway/include/dds_type_registry.hpp`의 `WriterHolder::write_any` 및 `ReaderHolder::process_data`에서 메시지 카운트
  - `RtpDdsGateway/src/dds_manager_entities.cpp`에서 엔티티 생성 시 현재 상태 스냅샷 업데이트
- 통계 스케줄러: `StatsManager` 내부 스레드가 매분 스냅샷을 출력

동작 원칙

- 통계는 비차단/경량: 메시지 경로에 영향이 없도록 간단한 원자/뮤텍스 보호 카운터 사용
- Writer/Reader 카운트는 Agent 관점의 write/take 수만 카운팅 (상대 연결 수 미반영)
- DDS 엔티티 카운트는 현재 상태(스냅샷)로 제공

확장 포인트

- 출력 형식(JSON 옵션 추가)
- 통계 집계 라벨(도메인별, 퍼블리셔/서브스크라이버별 상세)
- 히스토리 저장(rolling 파일 또는 DB)

사용 예

- `agent_config.json`의 statistics 섹션 예:

```
"statistics": {
  "enabled": true,
  "file_output": true,
  "file_dir": "logs",
  "file_name": "stats.log"
}
```

- 앱 시작 시 `StatsManager`가 활성화되며 매분 통계가 출력됩니다.

테스트 및 검증

- 단위: `StatsManager::snapshot_and_reset_counts()` 호출로 반환값 검증
- 통합: 실제 IPC 요청/응답 및 DDS write/take 경로에서 카운트 증가 확인

출력 샘플 (DB 형태: KEY + 컬럼)

아래 예시는 사람이 읽기 쉬운 간단 텍스트이지만, 각 행을 DB 레코드처럼 KEY와 컬럼으로 해석할 수 있도록 표기한 것입니다.

1) 엔티티 스냅샷 테이블 (현재 상태)

METRIC | SCOPE       | KEY                        | VALUE | NOTE
------ | ----------- | -------------------------- | -----:| -------------------------------------------------------------
PARTICIPANT | domain=0  | participant_count         |      2 | 이 Agent가 알고 있는 participant 엔티티 수 (domain 스코프)
PUBLISHER   | domain=0  | publisher_count           |      1 | 생성된 Publisher 엔티티 총수
SUBSCRIBER  | domain=0  | subscriber_count          |      1 | 생성된 Subscriber 엔티티 총수
WRITER      | domain=0  | writers_total             |      3 | 이 Agent가 생성한 Writer 엔티티의 총수
READER      | domain=0  | readers_total             |      4 | 이 Agent가 생성한 Reader 엔티티의 총수
TOPIC       | domain=0  | topics_total              |      3 | 도메인 내에 등록된 Topic 수 (TopicHolder 기반)

설명(명확화):

- `PARTICIPANT/PUBLISHER/SUBSCRIBER/WRITER/READER/TOPIC` 값은 **현재 상태의 엔티티 수**를 의미합니다.
- 예: `WRITER=3` 은 "내(Agent)가 생성/보유한 Writer 엔티티가 3개 있다"는 뜻입니다. 해당 Writer에 현재 연결된 remote Reader(매칭 수)는 이 스냅샷 행에 직접 포함되어 있지 않습니다.

2) 메시지 카운트 테이블 (분 단위 집계)

TOPIC         | WRITER_WRITES | READER_TAKES | NOTE
------------  | ------------: | ----------:  | ----------------------------------------------------------
ExampleTopic  |           120 |         110  | `WRITER_WRITES`: 이 Agent가 해당 topic에 대해 호출한 `write()` 수
chat          |            15 |          12  | `READER_TAKES`: 이 Agent가 해당 topic에서 `take()`로 읽어 처리한 샘플 수

설명(명확화):

- `TOPIC` 칼럼은 실제 `topic_name`을 의미합니다(예: `ExampleTopic`, `chat`).
- `WRITER_WRITES`은 Agent 내부의 Writer들이 해당 토픽에 대해 수행한 write 호출의 합계입니다(Agent 관점의 송신 수).
- `READER_TAKES`은 Agent 내부의 Reader들이 해당 토픽에서 take()로 유효 샘플을 가져간(처리한) 수입니다(Agent 관점의 수신 수).

3) IPC 카운트 (분 단위)

METRIC   | VALUE | NOTE
-------- | ----: | ------------------------------------------------------------
IPC_IN   |    10 | 외부(예: UI)로부터 이 Agent가 수신한 IPC 프레임(요청 등) 건수
IPC_OUT  |     8 | Agent가 외부로 전송한 IPC 프레임(응답/이벤트) 건수

추가 유의사항

- 엔티티 간 포함/연관성: `Participant` > `Publisher/Subscriber` > (`Writer` / `Reader`) 형태로 포함관계가 존재합니다. 위 스냅샷은 각각의 엔티티 수를 독립적으로 보여줍니다.
- Writer와 Reader 간의 매칭(몇 개의 remote endpoint와 연결되었는지)은 RTI 상태(예: publication_matched / subscription_matched)를 통해 확인할 수 있으며, 필요하면 통계 항목으로 확장할 수 있습니다.
- 현재 메시지 카운트는 "Agent 관점"의 단순 집계입니다. 향후 1:N, N:1 연결성을 반영하려면 엔티티별 매칭 수와 각 매칭별 메시지 전달률을 추가해야 합니다.

간단 출력 예시 (사람 친화적 텍스트 / CSV / JSON 샘플)

1) TEXT (사람이 읽기 쉬운 기본 포맷)

```
[STATS] 2025-12-14T12:34:00Z
ENTITY_SNAPSHOT: domain=0 PARTICIPANT=2 PUBLISHER=1 SUBSCRIBER=1 WRITERS=3 READERS=4 TOPICS=3
IPC: IN=10 OUT=8
MSG_COUNTS:
  Topic=ExampleTopic WriterWrites=120 ReaderTakes=110 WriterMatched=2 ReaderMatched=2
  Topic=chat         WriterWrites=15  ReaderTakes=12  WriterMatched=1 ReaderMatched=1
```

설명: 기본 TEXT 포맷에 `WriterMatched`/`ReaderMatched` 칼럼을 추가하여 각 토픽의 현재 매칭(connected) 수를 함께 제공합니다.

2) CSV (파이프라인/DB 적재용, 헤더 포함)

```
timestamp,metric,scope,key,value
2025-12-14T12:34:00Z,ENTITY_SNAPSHOT,domain=0,participant_count,2
2025-12-14T12:34:00Z,ENTITY_SNAPSHOT,domain=0,publisher_count,1
2025-12-14T12:34:00Z,ENTITY_SNAPSHOT,domain=0,subscriber_count,1
2025-12-14T12:34:00Z,ENTITY_SNAPSHOT,domain=0,writers_total,3
2025-12-14T12:34:00Z,ENTITY_SNAPSHOT,domain=0,readers_total,4
2025-12-14T12:34:00Z,ENTITY_SNAPSHOT,domain=0,topics_total,3
2025-12-14T12:34:00Z,IPC,,in,10
2025-12-14T12:34:00Z,IPC,,out,8
2025-12-14T12:34:00Z,MSG_COUNT,ExampleTopic,writer_writes,120
2025-12-14T12:34:00Z,MSG_COUNT,ExampleTopic,reader_takes,110
2025-12-14T12:34:00Z,MSG_COUNT,ExampleTopic,writer_matched,2
2025-12-14T12:34:00Z,MSG_COUNT,ExampleTopic,reader_matched,2
2025-12-14T12:34:00Z,MSG_COUNT,chat,writer_writes,15
2025-12-14T12:34:00Z,MSG_COUNT,chat,reader_takes,12
2025-12-14T12:34:00Z,MSG_COUNT,chat,writer_matched,1
2025-12-14T12:34:00Z,MSG_COUNT,chat,reader_matched,1
```

설명: 각 행이 단일 레코드이며, CSV 로 파싱하여 DB나 로그 파이프라인으로 쉽게 적재할 수 있습니다.

3) JSON (머신-가독성 및 구조적 처리용)

```
{
  "timestamp": "2025-12-14T12:34:00Z",
  "ipc": { "in": 10, "out": 8 },
  "entities": {
    "participants": 2,
    "publishers": 1,
    "subscribers": 1,
    "writers": 3,
    "readers": 4,
    "topics": 3
  },
  "messages": {
    "ExampleTopic": {
      "writer": { "writer_writes": 120, "writer_matched": 2 },
      "reader": { "reader_takes": 110, "reader_matched": 2 }
    },
    "chat": {
      "writer": { "writer_writes": 15, "writer_matched": 1 },
      "reader": { "reader_takes": 12, "reader_matched": 1 }
    }
  }
}
```

설명: JSON 출력은 구조적 필드(entities, messages 등)로 파싱 및 자동화 처리에 적합합니다.

참고: `statistics.format` 설정을 `text`(기본), `csv`, `json`으로 변경하면 해당 포맷으로 출력됩니다.
