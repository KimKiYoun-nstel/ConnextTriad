project(ConnextControlUI LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/actual_alarm_dialog.cpp
    include/mainwindow.hpp           # 헤더 파일 명시적으로 추가
    include/actual_alarm_dialog.hpp  # 헤더 파일 명시적으로
)

add_executable(ConnextControlUI WIN32 ${SOURCES})

target_include_directories(ConnextControlUI PRIVATE include)
target_include_directories(ConnextControlUI PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(ConnextControlUI PRIVATE Qt6::Widgets DkmRtpIpc ws2_32)