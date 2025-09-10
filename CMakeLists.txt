cmake_minimum_required(VERSION 3.24)
project(ConnextTriad LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# USE_CONNEXT auto-on if NDDSHOME present
set(USE_CONNEXT OFF)
if(DEFINED ENV{NDDSHOME})
  message(STATUS "NDDSHOME detected: $ENV{NDDSHOME}")
  set(USE_CONNEXT ON)
endif()

# USE_CONNEXT 옵션 기본값 명시
option(USE_CONNEXT "Build with RTI Connext DDS" ${USE_CONNEXT})

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /EHsc)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(nlohmann_json CONFIG REQUIRED)


# 단일 스위치: dynamic | static
set(PARSER_LINK_MODE "dynamic" CACHE STRING "dynamic or static")
string(TOLOWER "${PARSER_LINK_MODE}" _PARSER_LINK_MODE)
if(NOT _PARSER_LINK_MODE STREQUAL "dynamic" AND NOT _PARSER_LINK_MODE STREQUAL "static")
  message(FATAL_ERROR "PARSER_LINK_MODE must be dynamic or static")
endif()
message(STATUS "PARSER_LINK_MODE=${_PARSER_LINK_MODE}")

# 코드 매크로 전달용 인터페이스 타깃
add_library(parser_config INTERFACE)
if(_PARSER_LINK_MODE STREQUAL "static")
  target_compile_definitions(parser_config INTERFACE PARSER_LINK_STATIC=1)
endif()

# 파서 플러그인 옵션 및 서브디렉토리 추가
add_subdirectory(common)
add_subdirectory(parsers/ngva)

add_subdirectory(DkmRtpIpc)
add_subdirectory(RtpDdsGateway)
add_subdirectory(ConnextControlUI)

include_directories(${CMAKE_SOURCE_DIR}/third_party)

# 실제 존재하는 타겟에만 안전하게 링크
foreach(t IN ITEMS DkmRtpIpc RtpDdsGateway ConnextControlUI)
  if (TARGET ${t})
    target_link_libraries(${t} PRIVATE nlohmann_json::nlohmann_json)
  endif()
endforeach()