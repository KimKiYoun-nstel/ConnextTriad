project(DkmRtpIpc LANGUAGES CXX)
add_library(DkmRtpIpc STATIC 
    src/dkmrtp_ipc.cpp
    src/triad_log.cpp
)
target_include_directories(DkmRtpIpc PUBLIC include)

# Windows 전용 라이브러리(ws2_32)는 WIN32일 때만 링크
if(WIN32)
	target_link_libraries(DkmRtpIpc PUBLIC ws2_32)
else()
	# On Unix, no special system library required for BSD sockets.
	# Keep Windows behavior intact; POSIX path uses native socket APIs.
endif()