Build_Ref.md

이 문서는 ConnextTriad 리포지토리에서 사용할 수 있는 다양한 CMake/빌드 명령행 레시피를
정리한 참조입니다. 각 케이스별로 복사해서 사용할 수 있는 명령어와 짧은 설명을 제공합니다.
모든 예시는 리눅스(또는 WSL) 환경을 기준으로 하며, Windows 프리셋 사용 예시는 별도로 표기합니다.

환경 변수 (자주 사용)

- NDDSHOME: RTI Connext DDS 설치 디렉터리
  예: export NDDSHOME=/home/nstel/rti_connext_dds-7.5.0
- RTIDDSGEN_EXECUTABLE: rtiddsgen 실행 파일 경로 (CMake 캐시 덮어쓰기 용)
  예: -DRTIDDSGEN_EXECUTABLE=$NDDSHOME/bin/rtiddsgen
- RTI_LIB_DIR: RTI 라이브러리 디렉터리(링커용)
  예: -DRTI_LIB_DIR=$NDDSHOME/lib/x64Linux4gcc7.3.0
- RTI_WORKSPACE: rtiddsgen이 사용하는 작업공간(충돌을 피하려면 임시 디렉터리 사용 권장)
  예: RTI_WORKSPACE=/tmp/rti_ws_empty

참고: NDDSHOME 및 RTI 라이브러리 하위 경로는 설치된 Connext 버전/플랫폼에 따라 다릅니다.

기본 가정

- 소스 루트: (repo root) `/home/nstel/Agent`와 같은 위치
- 빌드 디렉터리: `build` (권장)
- 빌드 시스템: Ninja 권장 (또는 Unix Makefiles)

---

1) 최초 구성(Configure) + 전체 빌드 (권장, 신규 clone)

```bash
# 환경변수 설정
export NDDSHOME=/home/nstel/rti_connext_dds-7.5.0
export PATH=$NDDSHOME/bin:$PATH

# 클린 빌드 디렉터리 생성 및 Configure
rm -rf build
cmake -S . -B build -G Ninja \
  -DRTIDDSGEN_EXECUTABLE=$NDDSHOME/bin/rtiddsgen \
  -DRTI_LIB_DIR=$NDDSHOME/lib/x64Linux4gcc7.3.0

# 빌드 (병렬)
RTI_WORKSPACE=/tmp/rti_ws_empty cmake --build build -- -j$(nproc)
```

- 설명: CMake를 새로 실행해 캐시를 만들고, 모든 타깃을 빌드합니다. `RTI_WORKSPACE`는
  rtiddsgen 호출 시 작업공간 충돌을 피하는 데 유용합니다.

---

2) CMake Preset을 사용하는 방법 (프리셋이 설정되어 있을 때)

- Configure (preset에 RTIDDSGEN/RTI_LIB_DIR가 없다면 --extra-arg 형식으로 전달 가능)

```bash
cmake --preset linux-ninja
# 또는 변수를 한 번만 오버라이드 하려면
cmake --preset linux-ninja -- -DRTIDDSGEN_EXECUTABLE=$NDDSHOME/bin/rtiddsgen -DRTI_LIB_DIR=$NDDSHOME/lib/x64Linux4gcc7.3.0
```

- Build (preset을 쓸 수도 있고, 단순히 빌드 디렉터리 지정)

```bash
RTI_WORKSPACE=/tmp/rti_ws_empty cmake --build build -- -j$(nproc)
# 또는 build preset이 있는 경우
cmake --preset build-ninja-linux
```

---

3) CMakeLists.txt(또는 CMake 설정) 수정 후: 재구성(Reconfigure) 절차

- 변경사항이 CMake 레벨(예: CMakeLists.txt, CMakePresets.json, toolchain 등)에 발생했으면
  반드시 configure 단계를 다시 실행해야 합니다.

```bash
# 빌드 디렉터리에서 다시 configure
cmake -S . -B build -G Ninja
# 또는 preset 사용
cmake --preset linux-ninja

# 이후 빌드
RTI_WORKSPACE=/tmp/rti_ws_empty cmake --build build -- -j$(nproc)
```

- 팁: configure 단계에서 캐시가 꼬였다고 판단되면 `build` 폴더 전체를 삭제하고 다시 configure하세요:

```bash
rm -rf build
cmake -S . -B build -G Ninja -DRTIDDSGEN_EXECUTABLE=... -DRTI_LIB_DIR=...
RTI_WORKSPACE=/tmp/rti_ws_empty cmake --build build -- -j$(nproc)
```

---

4) 캐시만 삭제하고 재 configure (더 안전한 클린)

```bash
# CMake cache(및 내부 파일)만 제거
rm -rf build/CMakeCache.txt build/CMakeFiles
# 필요시 캐시 변수 재설정
cmake -S . -B build -G Ninja -DRTIDDSGEN_EXECUTABLE=... -DRTI_LIB_DIR=...
cmake --build build -- -j$(nproc)
```

---

5) 전체 클린 빌드 (산출물 모두 제거 후 재빌드)

```bash
# 완전 삭제
rm -rf build
# 다시 configure + 빌드
cmake -S . -B build -G Ninja -DRTIDDSGEN_EXECUTABLE=... -DRTI_LIB_DIR=...
RTI_WORKSPACE=/tmp/rti_ws_empty cmake --build build -- -j$(nproc)
```

- 용도: CMake 레벨 구조가 바뀌었거나, 생성물(예: IDL 생성 코드)이 꼬였을 때 권장합니다.

---

6) 소스 코드 변경 후(인크리멘탈 빌드, 빠른 방법)

- 소스(.cpp, .hpp)의 일반적인 변경은 단순 빌드 명령으로 충분합니다:

```bash
# 전체 (변경된 타깃만 재빌드됨)
cmake --build build -- -j$(nproc)

# 또는 특정 타깃만:
cmake --build build --target RtpDdsGateway -- -j$(nproc)
```

- 설명: CMake가 변경된 파일만 다시 컴파일하여 빠른 빌드를 제공합니다.

---

7) 모듈(타깃) 별 빌드

- 특정 라이브러리/실행파일만 빌드하려면 `--target`을 사용하세요.

```bash
# DkmRtpIpc 라이브러리만 빌드
cmake --build build --target DkmRtpIpc -- -j$(nproc)

# RtpDdsCore만 빌드
cmake --build build --target RtpDdsCore -- -j$(nproc)

# 전체 프로젝트의 실행파일만 빌드
cmake --build build --target RtpDdsGateway -- -j$(nproc)
```

- 팁: 의존성 때문에 일부 타깃을 빌드하면 자동으로 필요한 하위 타깃도 함께 빌드됩니다.

---

8) Clean target (빌드 시스템의 clean 사용)

```bash
cmake --build build --target clean
# 또는 직접
ninja -C build -t clean
# 또는 make 사용 시
make -C build clean
```

---

9) rtiddsgen/IDL 관련 재생성 팁

- 프로젝트의 CMake가 IDL -> C++/XML 생성을 자동으로 수행합니다. IDL 생성물이 꼬였을 경우:

```bash
# 빌드 디렉터리의 IDL 생성 폴더를 삭제하면 다음 빌드에서 재생성됩니다.
rm -rf build/idlkit/gen
RTI_WORKSPACE=/tmp/rti_ws_empty cmake --build build -- -j$(nproc)
```

- 또는 IDL 관련 개별 타깃(프로젝트에서 제공할 경우)을 빌드하세요(예: idl_xml, idl_cpp 등).
  - 프로젝트에 어떤 타깃이 있는지 확인하려면 `cmake --build build --target help` 또는
    `ninja -C build -t targets`를 사용해 확인할 수 있습니다.

---

10) Windows에서의 기본 흐름 (프리셋 사용 예)

```powershell
# PowerShell 예시 (Windows)
# Configure
cmake --preset vs2022-ninja-user
# Build
cmake --build build -- -j %NUMBER_OF_PROCESSORS%
```

- 검증 포인트: Windows에서 configure 할 때 `RTI_UNIX` 정의가 포함되지 않아야 합니다.

---

11) 환경 변수로 임시 오버라이드 (한 줄로 실행)

```bash
# 임시로 rtiddsgen 경로와 작업공간을 지정하고 빌드
RTIDDSGEN_EXECUTABLE=/opt/rti/bin/rtiddsgen RTI_WORKSPACE=/tmp/rti_ws_empty \
  cmake --build build -- -j$(nproc)
```

---

12) 문제 해결 빠른 팁

- 링크 에러(라이브러리 없음): `-DRTI_LIB_DIR`가 정확한지 확인, `LD_LIBRARY_PATH` 설정 확인
- 컴파일러/플래그 변경 시: 캐시 삭제 후 reconfigure 권장
- rtiddsgen이 실패할 때: `RTI_WORKSPACE`를 임시 디렉터리로 지정하여 다른 rtiddsgen 인스턴스와 충돌 회피
- Windows 전용 헤더 포함 에러(winsock2.h 등): 소스에 `_WIN32` 분기가 제대로 되어 있는지 확인

---

권장 워크플로우 요약

- CMake/프로젝트 파일(.cmake, CMakeLists.txt) 변경: `rm -rf build` → configure → build
- 일반 코드 변경(.cpp/.hpp): `cmake --build build -- -j$(nproc)` 또는 타깃 지정 빌드
- IDL(또는 코드 생성) 변경: `rm -rf build/idlkit/gen` → RTI_WORKSPACE=... cmake --build build -- -j

---

추가로 원하시면

- Windows 스모크 빌드 체크리스트 파일을 생성해 드리겠습니다.
- `ninja -C build -t targets` 출력을 캡처해 어떤 타깃이 있는지 정리해 드리겠습니다.

더 필요한 항목을 알려주세요.