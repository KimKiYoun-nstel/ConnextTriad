# cmake/toolchain-vxworks-ppce6500.cmake

# 1) 타겟 시스템 정보
set(CMAKE_SYSTEM_NAME VxWorks)
set(CMAKE_SYSTEM_PROCESSOR powerpc)

# 2) 기본 경로들 (환경변수에서 가져오기)
if(NOT DEFINED ENV{WIND_BASE})
  message(FATAL_ERROR "WIND_BASE environment variable must be set to your VxWorks install (e.g. D:/WindRiver/vxworks/23.03)")
endif()
string(STRIP "$ENV{WIND_BASE}" WIND_BASE_RAW)
# \ → / 변환
file(TO_CMAKE_PATH "${WIND_BASE_RAW}" WIND_BASE)

if(NOT DEFINED ENV{NDDSHOME_CTL})
  message(FATAL_ERROR "NDDSHOME_CTL environment variable must be set to your RTI Connext install (e.g. D:/WindRiver/workspace/rti_connext_dds-7.3.0)")
endif()
string(STRIP "$ENV{NDDSHOME_CTL}" NDDSHOME_CTL_RAW)
file(TO_CMAKE_PATH "${NDDSHOME_CTL_RAW}" NDDSHOME_CTL)

if(NOT DEFINED ENV{WIND_CC_SYSROOT})
  message(FATAL_ERROR "WIND_CC_SYSROOT environment variable must be set to your VxWorks sysroot (e.g. D:/WindRiver/vxworks/23.03/ppce6500/vsb)")  
else()
  string(STRIP "$ENV{WIND_CC_SYSROOT}" VSB_SYSROOT_RAW)
  file(TO_CMAKE_PATH "${VSB_SYSROOT_RAW}" VSB_SYSROOT)
endif()

message(STATUS "WIND_BASE       = '${WIND_BASE}'")
message(STATUS "NDDSHOME_CTL    = '${NDDSHOME_CTL}'")
message(STATUS "WIND_CC_SYSROOT = '${VSB_SYSROOT}'")


# 3) 크로스 컴파일러 설정
#   가장 깔끔한 건 wr-cc / wr-c++ 래퍼를 쓰는 것 (SDK/Workbench가 제공)
#   정확한 경로는 네 설치에 맞게 바꿔야 함.
#   예시는 host x86-win32 기준
set(CMAKE_C_COMPILER   "${WIND_BASE}/host/x86-win64/bin/wr-cc.exe")
set(CMAKE_CXX_COMPILER "${WIND_BASE}/host/x86-win64/bin/wr-c++.exe")

# wr-cc가 알아서 타겟 + sysroot를 잡으므로 별도 --target은 보통 필요 없음.
# 만약 wr-cc 대신 clang.exe를 직접 쓸 거라면, 이런 식으로 바꿔야 함:
# set(CMAKE_C_COMPILER   "D:/WindRiver/compilers/llvm-15.0.0.1/WIN64/bin/clang.exe")
# set(CMAKE_CXX_COMPILER "D:/WindRiver/compilers/llvm-15.0.0.1/WIN64/bin/clang++.exe")
# set(CMAKE_C_FLAGS_INIT   "--target=powerpc-wrs-vxworks -fno-exceptions ...")
# set(CMAKE_CXX_FLAGS_INIT "--target=powerpc-wrs-vxworks -fno-exceptions ...")
# -> 이 루트는 나중에 필요하면 같이 튜닝하자. 처음엔 wr-cc 쓰는 걸 추천.

# 4) CMake가 try_compile 할 때 실제 실행 안 해도 되게 (크로스 빌드 필수 패턴)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# 5) VxWorks / RTI용 공통 컴파일 플래그
#    RTI Platform Notes 에서 권장하는 매크로들:
#    RTI_VXWORKS, RTI_CLANG, RTI_64BIT 
set(COMMON_VX_C_FLAGS   "-D_VXWORKS_SOURCE -DRTI_VXWORKS -DRTI_CLANG")
set(COMMON_VX_CXX_FLAGS "${COMMON_VX_C_FLAGS} -std=c++17")

set(CMAKE_C_FLAGS_INIT   "${COMMON_VX_C_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${COMMON_VX_CXX_FLAGS}")

# RTP + 정적 링크 (wr-cc 전용 옵션)
#  -rtp  : RTP 실행파일
#  -static : 정적 링크 (PPC+RTP에서는 동적 lib 불가, RTI Platform Notes 8.1.1) 
set(CMAKE_EXE_LINKER_FLAGS_INIT "-rtp -static")

# 6) VxWorks & RTI 헤더/라이브러리 경로
# VxWorks 기본 헤더
set(VXWORKS_INCLUDE_DIR "${WIND_BASE}/target/h")
# 필요하면 coreip 등 세부 include도 나중에 추가 가능

# RTI Connext VxWorks CTL 경로
set(RTI_VX_LIB_DIR "${NDDSHOME_CTL}/lib/ppce6500Vx23.03llvm15.0_rtp")

# CMake 전역 include/link path (간단하게 가자)
include_directories(
  "${VXWORKS_INCLUDE_DIR}"
  "${NDDSHOME_CTL}/include"                      # RTI 헤더
)

link_directories(
  "${RTI_VX_LIB_DIR}"
)

# 7) 나중에 프로젝트 쪽에서 이 변수들을 활용할 수 있게 export 느낌으로 남겨두기
set(VXWORKS TRUE)
set(VX_TARGET_ARCH "ppce6500Vx23.03llvm15.0_rtp")