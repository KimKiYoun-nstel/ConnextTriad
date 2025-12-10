# cmake/toolchain-vxworks-ppce6500.cmake

# 1) 타겟 시스템 정보
set(CMAKE_SYSTEM_NAME VxWorks)
set(CMAKE_SYSTEM_PROCESSOR powerpc)

# 2) 기본 경로들 (환경변수에서 가져오기)
if(NOT DEFINED ENV{WIND_BASE})
  message(FATAL_ERROR "WIND_BASE 환경 변수가 설정되어야 합니다 (예: D:/WindRiver/vxworks/23.03)")
endif()
string(STRIP "$ENV{WIND_BASE}" WIND_BASE_RAW)
# \ → / 변환
file(TO_CMAKE_PATH "${WIND_BASE_RAW}" WIND_BASE)

if(NOT DEFINED ENV{NDDSHOME_CTL})
  message(FATAL_ERROR "NDDSHOME_CTL 환경 변수가 설정되어야 합니다 (예: D:/WindRiver/workspace/rti_connext_dds-7.3.0)")
endif()
string(STRIP "$ENV{NDDSHOME_CTL}" NDDSHOME_CTL_RAW)
file(TO_CMAKE_PATH "${NDDSHOME_CTL_RAW}" NDDSHOME_CTL)

# 하위 프로젝트에서 사용할 수 있도록 캐시 변수로 설정
set(RTI_ROOT "${NDDSHOME_CTL}" CACHE PATH "RTI Connext Root Directory for VxWorks" FORCE)

if(NOT DEFINED ENV{WIND_CC_SYSROOT})
  message(FATAL_ERROR "WIND_CC_SYSROOT 환경 변수가 설정되어야 합니다 (예: D:/WindRiver/vxworks/23.03/ppce6500/vsb)")  
else()
  string(STRIP "$ENV{WIND_CC_SYSROOT}" VSB_SYSROOT_RAW)
  file(TO_CMAKE_PATH "${VSB_SYSROOT_RAW}" VSB_SYSROOT)
endif()

message(STATUS "WIND_BASE       = '${WIND_BASE}'")
message(STATUS "NDDSHOME_CTL    = '${NDDSHOME_CTL}'")
message(STATUS "WIND_CC_SYSROOT = '${VSB_SYSROOT}'")


# 3) 크로스 컴파일러 설정
#   컴파일은 clang++을 직접 사용하고, 링크는 wr-c++ 래퍼를 사용합니다.
#   - CLANGXX 환경 변수가 있으면 우선 사용
#   - 없으면 기본 경로 탐색
if(DEFINED ENV{CLANGXX})
  set(_clangxx_raw "$ENV{CLANGXX}")
  file(TO_CMAKE_PATH "${_clangxx_raw}" _clangxx)
  if(EXISTS "${_clangxx}")
    set(CMAKE_CXX_COMPILER "${_clangxx}")
    message(STATUS "ENV{CLANGXX}에서 clang++ 사용: '${CMAKE_CXX_COMPILER}'")
  endif()
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER)
  # WindRiver 컴파일러 경로 시도
  set(_maybe_clang "${WIND_BASE}/host/x86-win64/bin/clang++.exe")
  if(EXISTS "${_maybe_clang}")
    set(CMAKE_CXX_COMPILER "${_maybe_clang}")
    message(STATUS "clang++ 발견: '${CMAKE_CXX_COMPILER}'")
  endif()
endif()

if(NOT DEFINED CMAKE_CXX_COMPILER)
  # LLVM_ROOT 환경 변수 시도
  if(DEFINED ENV{LLVM_ROOT})
    file(TO_CMAKE_PATH "$ENV{LLVM_ROOT}/bin/clang++.exe" _llvm_clang)
    if(EXISTS "${_llvm_clang}")
      set(CMAKE_CXX_COMPILER "${_llvm_clang}")
      message(STATUS "ENV{LLVM_ROOT}에서 clang++ 발견: '${CMAKE_CXX_COMPILER}'")
    endif()
  endif()
endif()

# clang++을 찾지 못한 경우 wr-c++로 폴백
if(NOT DEFINED CMAKE_CXX_COMPILER)
  set(CMAKE_CXX_COMPILER "${WIND_BASE}/host/x86-win64/bin/wr-c++.exe")
  message(STATUS "C++ 컴파일러로 wr-c++ 사용 (폴백): '${CMAKE_CXX_COMPILER}'")
endif()

# C 컴파일러 설정: 본 프로젝트는 Modern C++ 전용이므로 C 컴파일러(clang/wr-cc)는 설정하지 않음.
# CMakeLists.txt에서 project(... LANGUAGES CXX)로 설정되어 있어 C 컴파일러를 찾지 않음.

# 4) CMake가 try_compile 할 때 실제 실행 안 해도 되게 설정 (크로스 빌드 필수)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 5) VxWorks & RTI용 컴파일 플래그 설정
#    기존 Makefile 및 RTI Platform Notes 기반 플래그 통합

# CPU 및 타겟 아키텍처 플래그
set(VX_CPU_FLAGS "--target=powerpc-wrs-vxworks -mhard-float -mno-altivec -msecure-plt -mcpu=e6500")

# 최적화 및 경고 제어 플래그
# -nostdlibinc: VxWorks 시스템 헤더만 사용하기 위해 표준 헤더 경로 제외
set(VX_OPT_FLAGS "-fno-builtin -fno-strict-aliasing -nostdlibinc -Wno-return-type-c-linkage")

# 필수 매크로 정의
# _VSB_CONFIG_FILE: VxWorks 헤더가 VSB 설정을 찾기 위해 필요
# 주의: 경로에 공백이 없다고 가정하며, 따옴표 이스케이프를 위해 텍스트 그대로 전달
# CMake/Ninja/Shell을 거치면서 따옴표가 사라지지 않도록 이스케이프 강화 (\")
# 꺾쇠 괄호 < > 대신 큰따옴표 " 사용 (쉘 리다이렉션 오해 방지)
set(VX_MACRO_FLAGS "-DRTI_VXWORKS -DRTI_CLANG -DRTI_STATIC -DRTI_RTP -D_HAVE_TOOL_XTORS -D_USE_INIT_ARRAY -D_VSB_CONFIG_FILE=\\\"${VSB_SYSROOT}/h/config/vsbConfig.h\\\" -D_VX_CPU=_VX_PPCE6500 -D_VX_TOOL=llvm -D_VX_TOOL_FAMILY=llvm -D__ppc -D__ppc__ -D__vxworks -D__ELF__ -D__RTP__ -D__VXWORKS__")

# 초기 C/CXX 플래그 설정
# 주의: 이미 캐시된 CMAKE_CXX_FLAGS가 있으면 _INIT 변수는 무시됩니다.
# 변경된 플래그를 강제로 적용하기 위해 CACHE FORCE를 사용합니다.
set(CMAKE_C_FLAGS "${VX_CPU_FLAGS} ${VX_OPT_FLAGS} ${VX_MACRO_FLAGS}" CACHE STRING "VxWorks C Flags" FORCE)
set(CMAKE_CXX_FLAGS "${VX_CPU_FLAGS} ${VX_OPT_FLAGS} ${VX_MACRO_FLAGS} -std=c++17" CACHE STRING "VxWorks C++ Flags" FORCE)

set(CMAKE_C_FLAGS_INIT   "${VX_CPU_FLAGS} ${VX_OPT_FLAGS} ${VX_MACRO_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${VX_CPU_FLAGS} ${VX_OPT_FLAGS} ${VX_MACRO_FLAGS} -std=c++17")

# 링크 플래그 (wr-c++ 전용 옵션)
# -rtp: RTP 실행파일 생성
# -static: 정적 링크 (PPC+RTP 환경 제약)
set(CMAKE_EXE_LINKER_FLAGS "-rtp -static" CACHE STRING "VxWorks Linker Flags" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-rtp -static")

# 링커 드라이버 설정: wr-c++ 래퍼 사용
# RTOS 특화 링크 동작(스크립트 처리 등)을 위해 최종 링크는 wr-c++가 담당해야 함
set(_wr_cxx_driver "${WIND_BASE}/host/x86-win64/bin/wr-c++.exe")
if(EXISTS "${_wr_cxx_driver}")
  set(CMAKE_LINKER "${_wr_cxx_driver}")
  # CMake C++ 링크 규칙 재정의: 링커 드라이버로 wr-c++ 사용
  # 사용자 요청 반영: 링크 단계에서는 컴파일러 플래그(<FLAGS>)를 전달하지 않음
  # wr-c++가 환경 변수와 VSB 설정을 기반으로 링크를 수행하도록 함
  set(CMAKE_CXX_LINK_EXECUTABLE "\"${CMAKE_LINKER}\" <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
  message(STATUS "링커 드라이버로 wr-c++ 사용: '${CMAKE_LINKER}' (컴파일러: '${CMAKE_CXX_COMPILER}')")
else()
  message(WARNING "wr-c++를 찾을 수 없습니다 ('${_wr_cxx_driver}'). CMake가 컴파일러 드라이버를 링커로 사용합니다.")
endif()

# 6) VxWorks & RTI 헤더/라이브러리 경로
# VxWorks 기본 헤더
set(VXWORKS_INCLUDE_DIR "${VSB_SYSROOT}/share/h/public" "${VSB_SYSROOT}/usr/h/public" "${VSB_SYSROOT}/usr/h")

# RTI Connext VxWorks CTL 라이브러리 경로
set(RTI_VX_LIB_DIR "${NDDSHOME_CTL}/lib/ppce6500Vx23.03llvm15.0_rtp" CACHE PATH "RTI VxWorks Lib Dir" FORCE)

# 전역 include 경로 설정
# -nostdlibinc를 사용하므로 VxWorks 시스템 헤더를 명시적으로 포함해야 함
include_directories(
  ${VXWORKS_INCLUDE_DIR}
  "${NDDSHOME_CTL}/include"
  "${NDDSHOME_CTL}/include/ndds"
  "${NDDSHOME_CTL}/include/ndds/hpp"
)

link_directories(
  "${RTI_VX_LIB_DIR}"
)

# 7) 프로젝트에서 활용할 변수 설정
set(VXWORKS TRUE)
set(VX_TARGET_ARCH "ppce6500Vx23.03llvm15.0_rtp")
