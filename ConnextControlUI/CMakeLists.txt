project(ConnextControlUI LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(ConnextControlUI 
	src/main.cpp 
	src/mainwindow.cpp
	include/mainwindow.hpp
)

target_include_directories(ConnextControlUI PRIVATE include)
target_include_directories(ConnextControlUI PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(ConnextControlUI PRIVATE Qt6::Widgets DkmRtpIpc ws2_32)