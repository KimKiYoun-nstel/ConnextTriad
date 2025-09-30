# Copilot 프로젝트 지침서

## 교차 참조 (Copilot 컨텍스트용)

- 상위 모듈 경계 및 데이터 흐름: `PROJECT_ARCHITECTURE.md` 참고
- 상세 코딩 규칙 및 예시: `CODING_GUIDELINES.md` 참고
- NGVA/DDS 컴플라이언스 및 Connext 제약: `DOMAIN_STANDARDS.md` 참고
- 공식 Topic/Type 및 Command 명세: `TOPICS_AND_COMMANDS.md` 참고

## 언어 정책 (필수)

- Copilot Chat 응답 및 생성되는 모든 자연어 텍스트의 기본 언어는 **한국어**입니다.
- 모든 주석, docstring, 사용자 문서는 **한국어**로 작성해야 합니다.
- 코드 식별자 및 외부 API 이름(타입/함수/클래스/enum, DDS/NGVA 용어 등)은 **영문**을 유지합니다.
- UI 라벨, 로그/에러 메시지 등 사용자 노출 문자열은 별도 명세가 없는 한 **한국어**로 작성합니다.
- 애매할 경우 **한글 서술 + 영문 코드 토큰**을 우선합니다.

## 개발 환경

- OS: Windows
- IDE: Visual Studio 2022
- 빌드 시스템: CMake
- 언어: Modern C++ (C++17 이상)

## 스타일

- Google C++ Style Guide를 따르며, 프로젝트별 `.clang-format`을 적용합니다.
- 4칸 들여쓰기, 최대 120자, 포인터는 타입에 붙임(`int* p`)
- 탭 사용 금지, 공백만 허용
- 모든 함수, 클래스, 멤버 구조체에는 의미 있는 주석 필수

## 프레임워크 및 표준

- 미들웨어: RTI Connext DDS 7.5.x
- 모든 설계는 **NGVA** 및 **DDS** 표준을 엄격히 준수해야 합니다.
- DDS/NGVA 설계 원칙을 위반하는 코드는 도입 금지

## 개발 규칙

- 명시적으로 요청하지 않는 한 빌드/테스트를 실행하지 않습니다.
- 신규 기능 추가 전 반드시 기존에 유사 기능이 있는지 확인하고, 있으면 재사용/수정 우선
- 로직/기능 중복 구현 금지
- 모든 공개/비공개 API는 문서화 필수
- 모든 클래스, 함수, 멤버 변수에는 **의미/목적**을 설명하는 주석 필수

## 문서화 가이드라인

- 클래스, 함수, 멤버에는 반드시 **Doxygen 스타일 주석** 사용

## DDS 코드 생성 정책

- `IdlKit/Idl` 하위의 모든 `.idl` 파일은 **소스 파일**이며, 반드시 버전 관리에 커밋해야 합니다.
- `rtiddsgen`이 생성한 코드는(`*.cxx`, `*Plugin.*`, `*Support.*`) 빌드 산출물로, 커밋하지 않음
- 생성 파일은 직접 수정 금지, 변경 필요 시 `.idl`을 수정 후 재생성
- Copilot은 생성 코드 직접 수정 제안 금지, 반드시 IDL 정의를 통한 변경만 안내
- 생성 코드: `<build>/idlkit/gen` / XML: `<build>/idlkit/xml` 반영.
- DDS 생성 헤더(`gen/*.hpp`)는 데이터 타입/구조 추론에만 사용
- DDS 생성 소스(`*.cxx`, `*Plugin.*`, `*Support.*`)는 무시, 직접 수정/참조 금지
- 스키마 변경은 항상 `.idl` 파일에서만, 이후 코드 재생성

## Copilot 컨텍스트 힌트

- 데이터 타입 추론 시 `<build>/idlkit/gen/*.hpp`만 참고, `<build>/idlkit/gen/*.cxx`, `*Plugin.*`, `*Support.*`는 참조 금지
- 신규 기능 제안 전 `RtpDdsGateway/include/`, `DkmRtpIpc/include/`에 유사 기능이 있는지 먼저 검색
- 명시적 요청 없으면 빌드/테스트 실행 금지
- `TOPICS_AND_COMMANDS.md`를 topics/types 및 UI↔DDS 명령의 단일 진실원천으로 간주, 반드시 준수
