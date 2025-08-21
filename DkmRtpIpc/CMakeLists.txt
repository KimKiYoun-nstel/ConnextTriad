project(DkmRtpIpc LANGUAGES CXX)
add_library(DkmRtpIpc STATIC src/dkmrtp_ipc.cpp)
target_include_directories(DkmRtpIpc PUBLIC include)
target_link_libraries(DkmRtpIpc PUBLIC ws2_32)