# 프로젝트 아키텍처 (2025.09 기준)

## 모듈 구성

- **RtpDdsGateway**
  - 정적 라이브러리: `RtpDdsCore` + 실행 파일: `RtpDdsGateway`
  - 주요 구성요소:
    - `dds_manager`: DDS 엔티티(Participant/Publisher/Subscriber/Topic/Writer/Reader) 생성 및 콜백 기반 샘플 라우팅 담당
    - `ipc_adapter`: UI IPC 명령을 DDS 동작으로 변환(ACK/ERR/EVT를 UI로 반환)
    - `sample_factory`: 다양한 토픽 타입(AlarmMsg, StringMsg 등)에 대해 AnyData ↔ JSON 변환, 샘플 동적 생성/변환 담당. 신규 타입 추가 시 변환 함수를 등록하여 확장.
    - `dds_util`: DDS 타입과 JSON/문자열 변환, bounded string, 시간/ID 변환 등 공통 유틸리티 제공. IDL 타입/필드 추가 시 변환 유틸리티 확장.
    - `type_registry`, `dds_type_registry`: 토픽/타입별 등록, 엔티티 생성, publish/take 헬퍼 등을 중앙에서 관리. 신규 타입/토픽 추가 시 레지스트리에 등록.
    - `Alarms_PSM_enum_utils`: enum ↔ 문자열 변환 유틸리티. 신규 enum 타입 추가 시 변환 함수 확장.
  - `RtpDdsGateway/types/*.idl`에 정의된 IDL 타입 사용(예: `AlarmMsg.idl`, `StringMsg.idl`)

- **ConnextControlUI**
  - Qt 기반 GUI 앱. IPC를 통해 RPC/명령 전송 및 로그/이벤트 표시
  - `rpc_envelope` 빌더로 요청 구조화(`to_cbor()`, `to_json()` 등)

- **DkmRtpIpc**
  - 경량 UDP IPC(클라이언트/서버), 백그라운드 수신 루프 및 콜백 제공
  - 프레임 메시지 및 raw/evt/error 전송 헬퍼 제공

- **QoS**
  - `qos/qos_profiles.xml`은 빌드/포스트빌드 시 gateway 실행 파일 옆에 복사됨

## 빌드 및 코드 생성

- **IDL → 생성 코드**: `rtiddsgen`이 Classic C++ 코드를 `${binary_dir}/generated`에 생성
- 생성된 헤더(`*.h`)는 실제 데이터 타입을 제공. 생성 소스(`*.cxx`, `*Plugin.*`, `*Support.*`)는 빌드 산출물로, 직접 수정하지 않음
- CMake는 RTI 라이브러리 디렉토리를 자동 감지(우선순위: `x64Win64VS2017/2019/2022`) 후 `nddscpp`, `nddsc`, `nddscore`를 링크. `NDDSHOME` 환경변수 필요

## 데이터 흐름 (런타임)

```
UI (ConnextControlUI)
  ⇅  DkmRtpIpc (UDP, 요청/응답, 이벤트)
  ⇅  ipc_adapter (명령 변환 + ack/err/evt)
  ⇅  dds_manager (리더/라이터 관리 + 콜백)
  ⇅  RTI Connext DDS 네트워크 (IDL 기반 토픽)
```

- DDS 샘플 수신 시 `ReaderListener::on_data_available`가 호출되고, sample_factory/유틸리티를 거쳐 IPC로 UI에 전달됨
- UI에서 보낸 명령은 엔티티 생성, 샘플 전송, 구독 등으로 변환됨

## 동시성 및 소유권

- `DkmRtpIpc`는 별도 수신 스레드를 운영하며, 콜백은 앱 레이어로 전달됨
- `dds_manager`는 도메인/토픽별 DDS 엔티티 맵(참가자, 퍼블리셔, 서브스크라이버, 라이터, 리더)을 소유
- `ReaderListener`는 토픽별로 존재하며, 리스너 내부에서 무거운 작업을 피하고 변환/IPC 파이프라인을 활용

## 확장 작업 흐름

### 새로운 DDS 토픽 타입 추가

1. `RtpDdsGateway/types/NewType.idl` 파일을 추가하고 VCS에 커밋
2. CMake/rtiddsgen으로 재설정/생성 → 헤더가 `generated/`에 생성됨
3. `sample_factory` 및 `type_registry`/`dds_type_registry`에 변환/등록 함수 추가
4. `dds_manager` 동작 및 UI RPC 바인딩에서 사용
5. 생성 소스는 직접 수정하지 않음

### 새로운 UI 명령 추가

1. UI에서 RPC envelope 정의(`rpc_envelope`)
2. `ipc_adapter`에서 처리 → 적절한 `dds_manager` 동작으로 변환
3. 기존 함수 재사용 우선, 중복 구현 지양

## 경계(Do/Don't)

- **Do**: DDS 엔티티는 반드시 `dds_manager`에서만 생성; UI는 직접 접근 금지
- **Do**: 타입 변환/매핑은 sample_factory, dds_util, type_registry 등 중앙 유틸리티/레지스트리에서 관리; 변환 로직 분산 금지
- **Don't**: `generated/` 파일은 헤더만 참조(읽기 전용), 스키마 변경은 `.idl`에서만
- **Don't**: 유사 로직 중복 구현 금지; `type_registry`, `ipc_adapter`, `dds_manager`에 기존 구현 우선 확인

## 관측성 및 로깅(가이드)

- IPC/로그 훅(`DkmRtpIpc`, UI 로그 패널 등) 활용 권장
- DDS 이벤트는 UI로 전달 시 구조화된 메시지(JSON) 사용 권장
