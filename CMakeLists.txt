cmake_minimum_required(VERSION 3.24)
project(ConnextTriad LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_UI "Build desktop UI" OFF)

# USE_CONNEXT auto-on if NDDSHOME present

# NDDSHOME 경로 출력 (항상 출력)
if(DEFINED ENV{NDDSHOME})
  message(STATUS "NDDSHOME detected: $ENV{NDDSHOME}")
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /EHsc /bigobj)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# When building on Unix-like systems, enable RTI's UNIX platform macro so
# RTI headers select the correct OS-specific typedefs and structures.
if(UNIX)
  add_compile_definitions(RTI_UNIX=1)
  message(STATUS "UNIX detected: enabling RTI_UNIX compile definition")
endif()

find_package(nlohmann_json CONFIG REQUIRED)


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