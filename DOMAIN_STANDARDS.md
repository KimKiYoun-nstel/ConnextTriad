# 도메인 표준 (NGVA/DDS/Connext)

## DDS/Connext

- RTI Connext DDS 7.5.x (Classic C++ API) 버전 고정 사용
- Participant/Publisher/Subscriber/Topic/Writer/Reader 등 모든 DDS 엔티티는 반드시 `dds_manager`를 통해 생성해야 함
- QoS는 `qos/qos_profiles.xml`로 중앙 관리되며, gateway 실행 파일 옆에 복사됨

## IDL 및 코드 생성 정책

- `RtpDdsGateway/types/` 하위의 `.idl` 파일이 **최상위 소스**이며, 반드시 버전 관리에 커밋
- 생성된 **헤더**는 데이터 타입 참고용으로만 읽기 허용
- 생성된 **소스**는 빌드 산출물로, 직접 수정/참조 금지
- 스키마 변경은 `.idl`을 수정 후 재생성

## IPC 계약

- 모든 UI 상호작용은 반드시 `DkmRtpIpc`와 `ipc_adapter`를 거쳐야 하며(ACK/ERR/EVT 코드)
- 메시지 포맷은 최대한 안정적으로 유지, 필요 시 버전 관리된 envelope로 점진적 진화

## NGVA 정합성

- 적용 가능한 경우 NGVA 데이터 모델에 따라 topic/type을 설계
- 벤더 종속적 가정이 외부 인터페이스에 노출되지 않도록 주의

## 버전 관리 및 빌드

- `NDDSHOME` 환경변수 필수, CMake가 `RTI_LIB_DIR`(VS2017/2019/2022 버전)을 자동 감지 후 `nddscpp`, `nddsc`, `nddscore`를 링크
