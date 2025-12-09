cmake_minimum_required(VERSION 3.24)
project(ConnextTriad LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_UI "Build desktop UI" OFF)

# --- [NEW] VxWorks 여부 감지 ------------------------------------------
if(CMAKE_SYSTEM_NAME STREQUAL "VxWorks")
  set(IS_VXWORKS TRUE)
  message(STATUS "Configuring for VxWorks RTP (ppce6500)")
else()
  set(IS_VXWORKS FALSE)
endif()
# ----------------------------------------------------------------------


# USE_CONNEXT auto-on if NDDSHOME present

# --- [CHANGED] NDDSHOME / NDDSHOME_CTL 출력 ---------------------------
# VxWorks 빌드일 때는 NDDSHOME_CTL을, 나머지는 기존 NDDSHOME을 사용
if(IS_VXWORKS)
  if(DEFINED ENV{NDDSHOME_CTL})
    message(STATUS "NDDSHOME_CTL (VxWorks CTL) detected: $ENV{NDDSHOME_CTL}")
  else()
    message(WARNING "VxWorks build: NDDSHOME_CTL is not set. "
                    "RTI VxWorks CTL may not be found.")
  endif()
else()
  if(DEFINED ENV{NDDSHOME})
    message(STATUS "NDDSHOME detected: $ENV{NDDSHOME}")
  endif()
endif()
# ----------------------------------------------------------------------

# If the toolchain exposed Makefile-like flags, inherit them for the build.
# Note: per-target compile flags from the toolchain are applied inside
# the IdlKit / other subprojects where targets are created. Avoid globally
# appending to CMAKE_CXX_FLAGS here to keep behavior scoped and predictable.


if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /EHsc /bigobj)
  # Prevent legacy winsock.h from being included via windows.h; we use winsock2.h explicitly when needed.
  add_compile_definitions(_WINSOCKAPI_=1)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  
  # VxWorks System Header Warnings Suppression
  if(IS_VXWORKS)
    add_compile_options(
      -Wno-unused-local-typedef
      -Wno-unused-parameter
      -Wno-missing-field-initializers
      -Wno-reorder
      -Wno-deprecated-copy
      -Wno-deprecated-builtins
      -Wno-c11-extensions
      -Wno-user-defined-literals
      -Wno-gnu-null-pointer-arithmetic
      -Wno-misleading-indentation
      -Wno-atomic-alignment
      -Wno-delete-non-abstract-non-virtual-dtor
    )
  endif()
endif()

# Silence Clang-specific warning about the GNU extension of zero-argument
# variadic macros (uses the '##__VA_ARGS__' token-pasting trick in
# include/triad/triad_log.hpp). This is a targeted, compiler-only
# suppression so other warnings remain enabled.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-Wno-gnu-zero-variadic-macro-arguments)
  # VxWorks 빌드 시 Toolchain 파일에서 이미 필요한 플래그를 CMAKE_CXX_FLAGS_INIT에
  # 설정했으므로, 여기서 별도로 TOOLCHAIN_CXXFLAGS_COMMON을 파싱할 필요가 없습니다.
endif()

# --- [CHANGED] RTI_UNIX / RTI_VXWORKS 정의 분리 -----------------------
# UNIX 계열이면서 VxWorks가 아닌 경우만 RTI_UNIX 사용
if(UNIX AND NOT IS_VXWORKS)
  add_compile_definitions(RTI_UNIX=1)
  message(STATUS "UNIX detected (non-VxWorks): enabling RTI_UNIX compile definition")
endif()
# VxWorks의 경우 Toolchain 파일에서 이미 RTI_VXWORKS 등을 정의했으므로 추가 작업 불필요
# ----------------------------------------------------------------------

# --- nlohmann_json 의존성 처리 ---
if(IS_VXWORKS)
  # VxWorks/CTL 빌드: 헤더온리 버전을 third_party에서 직접 사용
  message(STATUS "VxWorks build: using bundled nlohmann_json from third_party")
  add_library(nlohmann_json INTERFACE)
  # 예시: third_party/nlohmann/json.hpp 구조라고 가정
  target_include_directories(nlohmann_json INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party
  )
  # 기존 코드에서 쓰는 이름을 맞추기 위해 ALIAS 생성
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
else()
  # 호스트(Windows/Linux) 빌드: 기존처럼 패키지 찾기
  find_package(nlohmann_json CONFIG REQUIRED)
endif()

# Ensure generated VSB config macro is visible to all targets early
# Some VxWorks public headers rely on the macro `_VSB_CONFIG_FILE` to
# include the generated config header. Add a global compile definition
# before any subdirectories/targets are created so every TU receives it.
# (Toolchain 파일에서 이미 정의되었으므로 여기서는 중복 정의하지 않음)
if(IS_VXWORKS AND DEFINED VSB_SYSROOT)
  # Toolchain에서 처리됨
endif()

# 순서 중요: IdlKit 먼저
add_subdirectory(IdlKit)
## DkmRtpIpc는 원래 Windows(winsock2)를 사용하지만, 현재는 POSIX 빌드도
## 지원하도록 소스에 플랫폼 분기(_WIN32 / else)를 추가했습니다. 따라서 모든
## 플랫폼에서 서브디렉터리를 추가합니다. CMake 내부에서 Windows 전용 링크는
## DkmRtpIpc/CMakeLists.txt에서 WIN32로 분기합니다.
add_subdirectory(DkmRtpIpc)
add_subdirectory(RtpDdsGateway)
if(BUILD_UI)
  add_subdirectory(ConnextControlUI)
endif()

include_directories(${CMAKE_SOURCE_DIR}/third_party)

# 실제 존재하는 타겟에만 안전하게 링크
foreach(t IN ITEMS DkmRtpIpc RtpDdsGateway ConnextControlUI)
  if (TARGET ${t})
    # If the target is an INTERFACE library, link using INTERFACE keyword.
    get_target_property(_t_type ${t} TYPE)
    if(_t_type STREQUAL "INTERFACE_LIBRARY")
      target_link_libraries(${t} INTERFACE nlohmann_json::nlohmann_json)
    else()
      target_link_libraries(${t} PRIVATE nlohmann_json::nlohmann_json)
    endif()
  endif()
endforeach()