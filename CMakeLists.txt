cmake_minimum_required(VERSION 3.24)
project(ConnextTriad LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# USE_CONNEXT auto-on if NDDSHOME present

# NDDSHOME 경로 출력 (항상 출력)
if(DEFINED ENV{NDDSHOME})
  message(STATUS "NDDSHOME detected: $ENV{NDDSHOME}")
endif()

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /EHsc)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(nlohmann_json CONFIG REQUIRED)


# 순서 중요: IdlKit 먼저
add_subdirectory(IdlKit)
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