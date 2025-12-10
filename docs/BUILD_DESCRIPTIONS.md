# Agent_v3.0 빌드 시스템 설명서

이 문서는 `Agent_v3.0` 프로젝트의 빌드 시스템 구조, 플랫폼별 빌드 전략, 그리고 주요 설정 파일들에 대해 설명합니다.

## 1. 빌드 시스템 개요

이 프로젝트는 **CMake**를 메인 빌드 시스템으로 사용하며, **Ninja**를 빌드 제너레이터로 사용합니다.
플랫폼별로 빌드 구성 방식에 차이가 있습니다.

* **Windows / Linux**: `CMakePresets.json`을 통해 표준화된 빌드 프리셋을 제공합니다.
* **VxWorks (Target)**: 현재 프리셋을 사용하지 않고, 전용 툴체인 파일(`toolchain-vxworks-ppce6500.cmake`)을 지정하여 직접 CMake 명령어로 빌드를 구성합니다.

### 주요 특징

- **멀티 플랫폼 지원**: Windows (Host), VxWorks (Target), Linux (Host/Target)
* **Modern C++**: C++17 표준 준수
* **DDS 미들웨어**: RTI Connext DDS 7.5.x 사용
* **IDL 코드 생성**: `IdlKit` 모듈을 통해 IDL 파일로부터 DDS 타입 코드를 자동 생성

---

## 2. 플랫폼별 빌드 전략

### 2.1. VxWorks (Target: ppce6500)

VxWorks 빌드는 **Hybrid Toolchain Strategy**를 사용합니다.

* **컴파일러 (Compile)**: `clang` / `clang++` (LLVM 15.0.0.1)
  * **이유**: RTI Connext DDS의 Modern C++ API는 최신 C++ 표준 및 ABI 호환성을 요구합니다. Wind River의 기본 `wr-c++` 컴파일러보다 `clang`이 Modern C++ 지원 및 템플릿 처리에 더 적합하며, RTI 라이브러리와의 ABI 호환성 문제를 해결해 줍니다.
* **링커 (Link)**: `wr-c++` (Wind River C++ Wrapper)
  * **이유**: VxWorks RTP(Real Time Process) 실행 파일 생성 시, 초기화 코드(`_HAVE_TOOL_XTORS`), 시스템 라이브러리 경로, 링커 스크립트 처리를 위해 Wind River 전용 래퍼인 `wr-c++`를 사용해야 합니다.
* **주의사항**: 링킹 단계에서 컴파일러 플래그(`<FLAGS>`)가 링커로 전달되지 않도록 툴체인 파일이 구성되어 있습니다.

### 2.2. Windows & Linux (Host)

* **Windows**: MSVC (`cl.exe`) 사용. `vs2022-ninja` 프리셋 기반.
* **Linux**: GCC 또는 Clang 사용. `linux-ninja` 프리셋 기반.

---

## 3. 프로젝트 디렉토리 구조

```text
Agent_v3.0/
├── CMakeLists.txt                  # 루트 CMake 설정 파일
├── CMakePresets.json               # Windows/Linux용 빌드 프리셋 정의
├── build-vx/                       # VxWorks 빌드 산출물 (Out-of-source build)
├── cmake/                          # CMake 모듈 및 툴체인
│   ├── toolchain-vxworks-ppce6500.cmake  # [핵심] VxWorks 크로스 컴파일 설정
│   ├── Build_cmd                   # VxWorks 빌드 명령어 모음 (참조용)
│   └── RTI_sample/                 # RTI 예제 코드
├── script/                         # Linux 빌드 자동화 스크립트
│   ├── preset_build.sh             # 프리셋 기반 빌드 스크립트
│   ├── clean_project.sh            # 클린 스크립트
│   └── ...
├── IdlKit/                         # IDL 및 생성 코드 관리
│   ├── CMakeLists.txt
│   ├── idl/                        # .idl 소스 파일 (AlarmMsg.idl 등)
│   └── gen/                        # (생성됨) rtiddsgen 생성 코드
├── RtpDdsGateway/                  # 메인 애플리케이션 (Gateway)
│   ├── CMakeLists.txt
│   ├── include/                    # 헤더 파일
│   └── src/                        # 소스 파일
├── DkmRtpIpc/                      # IPC 통신 라이브러리
│   ├── CMakeLists.txt
│   └── include/
└── qos/                            # DDS QoS XML 설정 파일
```

---

## 4. 주요 설정 파일 상세

### 4.1. `cmake/toolchain-vxworks-ppce6500.cmake`

VxWorks 빌드를 위한 툴체인 파일입니다. 컴파일러는 Clang으로, 링커는 `wr-c++`로 강제 설정하며, 링킹 시 불필요한 플래그가 전달되지 않도록 `CMAKE_CXX_LINK_EXECUTABLE` 규칙을 재정의하고 있습니다.

### 4.2. `cmake/Build_cmd`

VxWorks 빌드를 위해 실행해야 하는 CMake 명령어들이 기록된 파일입니다. 프리셋 대신 이 파일의 명령어를 참조하여 빌드합니다.

---

## 5. 빌드 방법

### 5.1. VxWorks 빌드 (Windows Host)

VxWorks 빌드는 `CMakePresets.json`을 사용하지 않고, 직접 명령어를 입력합니다.

1. **환경 설정**: `vx_env.bat` 실행 (Wind River 환경 변수 로드)
2. **빌드 구성 (Configure)**:

    ```cmd
    cmake -S . -B build-vx -G Ninja -DCMAKE_TOOLCHAIN_FILE=./cmake/toolchain-vxworks-ppce6500.cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS_DEBUG="-g" -DCMAKE_CXX_FLAGS_DEBUG="-g" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    ```

3. **빌드 실행 (Build)**:

    ```cmd
    cmake --build build-vx -v
    ```

    또는 특정 타겟만 빌드:

    ```cmd
    cmake --build build-vx --target RtpDdsGateway
    ```

### 5.2. Linux 빌드

Linux는 `script/` 디렉토리의 쉘 스크립트 또는 CMake Preset을 사용합니다.

1. **스크립트 사용**:

    ```bash
    ./script/preset_build.sh linux-ninja-debug
    ```

2. **직접 Preset 사용**:

    ```bash
    cmake --preset linux-ninja-debug
    cmake --build --preset linux-ninja-debug
    ```

### 5.3. Windows 빌드

Windows는 VS Code의 CMake Tools 확장을 사용하거나 터미널에서 Preset을 사용합니다.

```cmd
cmake --preset vs2022-ninja-debug
cmake --build --preset vs2022-ninja-debug
```

---

## 6. 문제 해결 (Troubleshooting)

* **VxWorks 링킹 오류 ("cannot find config file")**:
  * `wr-c++` 링커에 컴파일러용 플래그(`-D`, `-I` 등)가 전달되면 발생합니다. 툴체인 파일 수정이 필요합니다.
* **실행 오류 ("program not executable")**:
  * `clang++`로 링킹했을 때 발생할 수 있습니다. 반드시 `wr-c++`로 링킹되었는지 확인하세요.
