# Agent Simulator & Test Tools

이 디렉토리는 Agent(`RtpDdsGateway`)의 성능 부하 테스트 및 기능 검증을 위한 시뮬레이터와 테스트 도구들을 포함하고 있습니다.

## 환경 설정 (Prerequisites)

Python 3.8 이상이 필요하며, 통신 프로토콜 처리를 위해 `cbor2` 패키지가 필요합니다.

```bash
pip install -r requirements.txt
# 또는
pip install cbor2
```

## 주요 스크립트 설명

### 1. 성능 부하 시뮬레이터 (`perf_simulator.py`)
Agent에 대량의 트래픽을 발생시켜 성능을 측정하기 위한 도구입니다.
- **기능**: 설정된 주기(Hz)로 다수의 Topic에 대해 데이터를 발행(Write)하고, Agent로부터의 응답 및 이벤트를 수신하여 통계를 출력합니다.
- **설정 파일**: `perf_config.json` (대상 호스트, 포트, Writer/Reader 설정, 전송 주기 등)
- **실행 방법**:
  ```bash
  # Agent가 먼저 실행되어 있어야 합니다.
  python3 perf_simulator.py perf_config.json
  ```

### 2. 통합 부하 테스트 (`integration_test.py`)
Agent와 `perf_simulator.py`를 자동으로 실행하여 연동 테스트를 수행하는 스크립트입니다.
- **기능**: Agent를 백그라운드에서 실행하고, 시뮬레이터를 구동한 뒤, 전송된 패킷 수와 Agent가 처리한 패킷 수를 비교하여 테스트 성공 여부를 판단합니다.
- **실행 방법**:
  ```bash
  # 프로젝트 루트 디렉토리에서 실행
  python3 Simulator/integration_test.py
  ```

### 3. 시나리오 기반 테스트 러너 (`test_runner.py`)
정해진 순서와 절차에 따라 Agent와 통신하며 기능을 검증하는 도구입니다.
- **기능**: JSON 형식의 시나리오 파일을 읽어 순차적으로 요청(Request)을 보내고, 응답(Response) 및 이벤트(Event)를 검증합니다. 변수 저장 및 치환 기능을 지원하여 동적인 테스트가 가능합니다.
- **시나리오 파일**: `scenarios/*.json`
- **실행 방법**:
  ```bash
  python3 test_runner.py scenarios/scenario1.json --host 127.0.0.1 --port 25000
  ```

### 4. 시나리오 통합 테스트 (`run_scenario_test.py`)
Agent와 `test_runner.py`를 자동으로 실행하여 시나리오 테스트를 수행하는 스크립트입니다.
- **기능**: Agent 실행 -> 시나리오 러너 실행 -> 결과 검증 -> Agent 종료 과정을 자동화합니다.
- **실행 방법**:
  ```bash
  # 프로젝트 루트 디렉토리에서 실행
  python3 Simulator/run_scenario_test.py
  ```

## 폴더 구조

- `samples/`: 부하 테스트 시 전송할 Topic별 JSON 데이터 샘플
- `scenarios/`: `test_runner.py`에서 사용하는 테스트 시나리오 파일들
- `perf_config.json`: 부하 테스트 설정 파일
- `ipc_protocol.py`: IPC 통신 프로토콜 정의 (헤더 포맷 등)

## 시뮬레이터 동작 방식

시뮬레이터는 Agent와 UDP 통신을 수행하며, 자체 정의된 IPC 프로토콜(Header + CBOR Body)을 사용합니다.

1. **초기화**: Agent 접속 후 `create_participant` 명령을 통해 DDS DomainParticipant를 생성합니다.
2. **Entity 생성**: 설정에 따라 Publisher, Subscriber, DataWriter, DataReader를 생성 요청합니다.
3. **데이터 통신**:
   - **Writer**: 설정된 주기대로 `write` 명령을 전송하여 데이터를 발행합니다.
   - **Reader**: `create` (reader) 요청을 통해 구독을 설정하고, Agent로부터 수신되는 `EVT`(이벤트) 메시지를 카운트합니다.
4. **통계**: 전송량(Tx), 수신량(Rx), 에러율 등을 실시간으로 집계하여 출력합니다.
