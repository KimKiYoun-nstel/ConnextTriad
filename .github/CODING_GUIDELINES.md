# 코딩 가이드라인 (C/C++)

## 스타일

- 기본: Google C++ Style을 따르며, `.clang-format`(4칸 들여쓰기, 120자 제한, 포인터는 타입에 붙임)으로 강제 적용
- `.editorconfig`로 EOL/공백 일관성 유지
- 모든 클래스, 함수, 멤버 변수에는 반드시 Doxygen 주석 작성

## 언어 레벨

- Modern C++ (C++17 이상) 사용을 권장합니다.
- 스마트 포인터, 범위 기반 for, 구조적 바인딩, std::optional 등 최신 C++ 표준 라이브러리와 언어 기능을 적극 활용합니다.
- RAII 원칙을 따르며, raw `new/delete` 사용을 최소화하고 스마트 포인터 사용을 기본으로 합니다.
- 플랫폼/컴파일러 호환성에 특별한 제약이 없는 한, 최신 C++ 기능 사용에 제한이 없습니다.

## include 및 헤더 관리

- 공개 헤더는 `RtpDdsGateway/include/`(또는 각 모듈별 `include/`)에 위치
- 헤더는 항상 self-contained, 최소화하며, 가능하면 forward-declare 사용

## 에러 처리

- 반환값은 명시적 status 또는 boolean 사용, DDS 경계에서는 예외 사용 금지
- DDS 반환 코드는 `ipc_adapter`를 통해 명확한 ACK/ERR 응답으로 변환

## 동시성

- `DkmRtpIpc`가 수신 스레드를 소유하며, 공유 상태는 mutex로 보호
- Listener 콜백은 반드시 경량이어야 하며, 처리는 converter/IPC로 위임

## 네이밍 및 파일 구성

- 타입은 `PascalCase`, 함수는 `camelCase`, 상수는 `kCamelCase`(Google 스타일)
- 파일당 하나의 책임, 함수는 가급적 50줄 미만으로 작성

## 문서화 및 예시

- Doxygen 블록을 사용합니다:

  ```cpp
  /**
   * @brief DDS subscriber를 초기화합니다.
   * @param domain_id DDS 도메인 ID
   * @return 성공 시 true
   */
  bool init(int domain_id);
  ```

## 문서화 언어 정책

- 모든 주석(Doxygen 포함), 커밋 메시지, 프로젝트 문서는 **한국어**로 작성해야 합니다.
- 코드 식별자(타입, 함수, enum, 상수) 및 외부 API 이름은 반드시 **영문**을 유지합니다.
- Doxygen 예시는 한글 설명 + 영문 코드 심볼을 혼용합니다.

## 로그 메시지 언어

소스 코드 내 모든 로그 메시지는 반드시 **영어**로 작성해야 합니다.

## 중복 금지

- API 추가 전 반드시 `include/`, `src/`에 기존 구현이 있는지 검색하고, 확장/재사용을 우선합니다.
