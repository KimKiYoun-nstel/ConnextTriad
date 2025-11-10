# 리눅스 환경에서 프로젝트 빌드 준비 및 단계별 튜토리얼

이 문서는 리눅스(WSL 포함)에서 RTI Connext DDS 기반 프로젝트(ConnextTriad)를 빌드하는
실무 절차를 튜토리얼 형식으로 정리한 것입니다. 환경 준비 → CMake Preset 구성 → 플랫폼
분기(CMake / C++ 전처리기) → 빌드 → 실행 검증 순으로 단계별로 따라오세요.

목표
- 리눅스에서 프로젝트를 완전히 빌드하고 `RtpDdsGateway` 실행파일을 생성한다.
- 윈도우 빌드와 충돌하지 않도록 모든 플랫폼 분기를 명확히 한다.

요약: 핵심 규칙
- CMake 분기: `if(WIN32)` / `if(UNIX)` 사용
- C/C++ 분기: `#ifdef _WIN32` / `#else` / `#endif` 사용
- 프로젝트 레벨 매크로: Unix에서는 `RTI_UNIX=1`를 설정해 RTI 헤더가 올바른 플랫폼 분기를 선택하도록 함

---

## 0) 권장 사전 준비
아래 사양으로 테스트했습니다: Ubuntu (WSL), gcc/g++ 13.x, CMake 3.24, Ninja.

필수 패키지 설치 (Ubuntu 예시):

```bash
sudo apt update
sudo apt install -y build-essential cmake g++ ninja-build python3 python3-pip \
    libssl-dev libxml2-dev libncurses5-dev
sudo apt install -y nlohmann-json3-dev # CMake 패키지용
pip3 install -r examples/requirements.txt || true
```

설치 시 문제 발생하면 `apt` 캐시를 정리하고 재시도하세요.

---

## 1) RTI Connext DDS 환경변수 설정
RTI가 `/home/nstel/rti_connext_dds-7.5.0`에 설치되어 있다고 가정합니다. 아래를 `~/.bashrc`에 추가하세요:

```bash
export NDDSHOME=/home/nstel/rti_connext_dds-7.5.0
export PATH=$NDDSHOME/bin:$PATH
export LD_LIBRARY_PATH=$NDDSHOME/lib/x64Linux4gcc7.3.0:$LD_LIBRARY_PATH
export CMAKE_PREFIX_PATH=$NDDSHOME/resource/cmake:$CMAKE_PREFIX_PATH
source ~/.bashrc
```

※ `LD_LIBRARY_PATH`의 서브디렉토리는 설치된 Connext 버전/플랫폼에 따라 달라질 수 있습니다
(예: x64Linux4gcc7.3.0). 본인의 NDDSHOME 하위에 존재하는 정확한 경로를 사용하세요.

---

## 2) CMake Preset 구성(권장)
프로젝트에는 Windows용 프리셋이 포함되어 있었습니다. 리눅스용 프리셋을 사용하거나
새로 만드세요. 예시: `linux-ninja` configure preset을 만들고 사용자용 `linux-ninja-user`
프리셋에서 빌드 타입 등을 지정합니다.

- configure (예시)

```bash
cmake -S . -B build -G Ninja \
  -DRTIDDSGEN_EXECUTABLE=$NDDSHOME/bin/rtiddsgen \
  -DRTI_LIB_DIR=$NDDSHOME/lib/x64Linux4gcc7.3.0
```

- 또는 presets 사용 예시:

```bash
# (프리셋이 있으면)
cmake --preset linux-ninja
# 또는 사용자 수준 override
cmake --preset linux-ninja -- -DRTIDDSGEN_EXECUTABLE=$NDDSHOME/bin/rtiddsgen -DRTI_LIB_DIR=$NDDSHOME/lib/x64Linux4gcc7.3.0
```

팁: `rtiddsgen`이 기본 RTI 작업공간으로 파일을 복사하려 시도하여 충돌이 발생하면
임시 작업공간을 지정하여 빌드하세요:

```bash
mkdir -p /tmp/rti_ws_empty
RTI_WORKSPACE=/tmp/rti_ws_empty cmake --build build -- -j$(nproc)
```

---

## 3) 프로젝트 수준 플랫폼 매크로(권장 설정)
루트 `CMakeLists.txt`에서 Unix 환경이면 `RTI_UNIX`를 자동으로 추가하도록 했습니다:

```cmake
if(UNIX)
  add_compile_definitions(RTI_UNIX=1)
  message(STATUS "UNIX detected: enabling RTI_UNIX compile definition")
endif()
```

이렇게 하면 RTI 헤더와 IDL 생성물이 동일한 플랫폼 분기를 사용합니다. Windows에서는
이 정의가 추가되지 않으므로 원래 Windows 동작을 유지합니다.

---

## 4) 플랫폼 분기 패턴(소스 코드)
소스 수정 시 반드시 아래 패턴을 사용하세요。

- 헤더/OS include 분기

```cpp
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
```

- 함수/구현 분기

```cpp
void close_socket(SOCKET s) {
#ifdef _WIN32
  closesocket(s);
  WSACleanup();
#else
  close(s);
#endif
}
```

이 규칙을 지키면 Windows 원본 코드 경로는 보존하면서 Linux에서도 동작하도록 할 수 있습니다.

---

## 5) 이번 세션에서 적용한 소스/빌드 변경(요약)
간단히 핵심 파일과 변경 내용을 나열합니다 (자세한 변경/차이는 repo의 `WIN_DIFF_UNIX.md` 파일을
참고하세요).

- `CMakeLists.txt` (루트)
  - `if(UNIX) add_compile_definitions(RTI_UNIX=1)` 추가
  - nlohmann_json 링크시 INTERFACE 대상 처리 보완

- `DkmRtpIpc/CMakeLists.txt`
  - Windows 전용 `ws2_32` 링크를 `if(WIN32)`로 감쌈

- `DkmRtpIpc/include/triad_log.hpp`
  - `localtime_s` 대신 POSIX `localtime_r` 분기 추가

- `DkmRtpIpc/src/dkmrtp_ipc.cpp`
  - Winsock 기반 코드를 `_WIN32` / POSIX 분기로 분리
  - POSIX에서 사용할 `SOCKET`/`INVALID_SOCKET` 등 매핑 추가
  - 64비트 바이트오더 변환 헬퍼(`htonll`/`ntohll`) 추가

- `RtpDdsGateway/src/rti_logger_bridge.cpp`
  - 네임스페이스 한정자 제거(컴파일러 호환성) 및 주석 추가

- `RtpDdsGateway/include/async/async_event_processor.hpp` 및 `.cpp`
  - 기본 생성자/위임 생성자 추가로 컴파일러 간 초기화 차이 해소

자세한 변경은 repository 루트의 `WIN_DIFF_UNIX.md`를 확인하세요。

---

## 6) 빌드 및 실행(튜토리얼 흐름)
아래 순서대로 따라하세요。

1) 클린 빌드 디렉터리 생성

```bash
rm -rf build
cmake -S . -B build -G Ninja \
  -DRTIDDSGEN_EXECUTABLE=$NDDSHOME/bin/rtiddsgen \
  -DRTI_LIB_DIR=$NDDSHOME/lib/x64Linux4gcc7.3.0
```

2) 빌드 (병렬)

```bash
RTI_WORKSPACE=/tmp/rti_ws_empty cmake --build build -- -j$(nproc)
```

3) 실행(샘플)

```bash
./build/RtpDdsGateway/RtpDdsGateway
```

실행 중 로그/에러가 있으면 `./build/RtpDdsGateway/RtpDdsGateway`가 실행되는 현재 디렉터리의
`qos/` 와 `types/xml` 위치를 확인하세요(빌드 후 커맨드로 복사되도록 설정되어 있음).

---

## 7) Windows 빌드 영향/검증
변경은 Windows 빌드가 깨지지 않도록 주의해서 적용했습니다. 하지만 안전을 위해 Windows에서
스모크 빌드를 꼭 실행해 검증하세요. 체크리스트(권장):

1. Windows 빌드 프리셋(`vs2022-ninja-user`)으로 configure → build
2. `DkmRtpIpc`가 Winsock 경로로 제대로 링크되는지 확인 (ws2_32 링크)
3. `RTI_UNIX`가 Windows 빌드에 정의되지 않았는지 확인

윈도우 검증이 필요하면 제가 Windows 체크리스트 문서를 만들어 드리겠습니다。

---

## 8) 추가 참고 및 복구 방법
- 변경을 되돌려야 한다면 Git에서 해당 파일을 리셋하면 됩니다. (예: `git checkout -- <file>`)
- 이번 변경은 플랫폼 분기를 추가하는 형태로 원본 로직을 보존했으므로, Windows 빌드가 문제를
  일으키는 경우 윈도우 빌드 로그를 공유해 주세요. 곧바로 수정해 드리겠습니다。

---

끝 — 필요하면 `WIN_DIFF_UNIX.md`에서 변경 세부사항(파일/라인 요약)을 확인하고, 윈도우용
검증 문서를 생성해 드리겠습니다。