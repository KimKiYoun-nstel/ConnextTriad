project(RtpDdsGateway LANGUAGES CXX)

# --- IDL -> generated code (Classic C++) ---
# 여러 IDL 파일 자동 검색
file(GLOB IDL_FILES "types/*.idl")
set(GEN_DIR  ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

# rtiddsgen path & language (classic)
set(RTIDDSGEN "$ENV{NDDSHOME}/bin/rtiddsgen.bat")
set(RTIDDSGEN_LANG "C++11" CACHE STRING "rtiddsgen language for Modern C++ (C++11)")

# 생성될 파일들을 저장할 리스트
set(GENERATED_SOURCES "")

# 각 IDL 파일에 대해 코드 생성
foreach(IDL_FILE ${IDL_FILES})
    get_filename_component(IDL_NAME ${IDL_FILE} NAME_WE)
    
    # 현재 IDL 파일에서 생성될 파일들
  set(CURRENT_GENERATED_FILES
    ${GEN_DIR}/${IDL_NAME}.hpp
    ${GEN_DIR}/${IDL_NAME}.cxx
    ${GEN_DIR}/${IDL_NAME}Plugin.hpp
    ${GEN_DIR}/${IDL_NAME}Plugin.cxx
    ${GEN_DIR}/${IDL_NAME}Support.hpp
    ${GEN_DIR}/${IDL_NAME}Support.cxx
  )
    
    # rtiddsgen 명령어 실행
    add_custom_command(
        OUTPUT ${CURRENT_GENERATED_FILES}
        COMMAND "${RTIDDSGEN}"
              -language ${RTIDDSGEN_LANG}
              -platform x64Win64VS2017
              -d "${GEN_DIR}" "${IDL_FILE}"
        DEPENDS ${IDL_FILE}
        VERBATIM
        COMMENT "Generating DDS code from ${IDL_NAME}.idl"
    )
    
    # 생성된 소스 파일들을 전체 리스트에 추가
    list(APPEND GENERATED_SOURCES 
        ${GEN_DIR}/${IDL_NAME}.cxx
        ${GEN_DIR}/${IDL_NAME}Plugin.cxx
    )
endforeach()

# 생성된 소스 파일들 출력 (디버깅용)
message(STATUS "Generated DDS sources: ${GENERATED_SOURCES}")

file(GLOB SRC_FILES "src/*.cpp")
add_library(RtpDdsCore STATIC
  ${SRC_FILES}
  ${GENERATED_SOURCES}
)

target_include_directories(RtpDdsCore PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

# our headers + generated headers
target_include_directories(RtpDdsCore PUBLIC include ${GEN_DIR})

# uses DkmRtpIpc headers in ipc_adapter.hpp
target_link_libraries(RtpDdsCore PUBLIC DkmRtpIpc)

# switch by USE_CONNEXT
target_compile_definitions(RtpDdsCore PUBLIC $<$<BOOL:${USE_CONNEXT}>:USE_CONNEXT>)

if(USE_CONNEXT)
  # RTI headers (Modern C++11)
  target_include_directories(RtpDdsCore PUBLIC "$ENV{NDDSHOME}/include" "$ENV{NDDSHOME}/include/ndds" "$ENV{NDDSHOME}/include/ndds/hpp")


  # --- Auto-detect RTI lib dir (prefer x64Win64VS2017) ---
  if(NOT DEFINED RTI_LIB_DIR)
    set(_rti_candidates
      "$ENV{NDDSHOME}/lib/x64Win64VS2017"
      "$ENV{NDDSHOME}/lib/x64Win64VS2019"
      "$ENV{NDDSHOME}/lib/x64Win64VS2022"
      "$ENV{NDDSHOME}/lib/x64Win64"
    )
    foreach(d ${_rti_candidates})
      if(EXISTS "${d}")
        set(RTI_LIB_DIR "${d}")
        break()
      endif()
    endforeach()
  endif()

  # if empty or not found, fail with a clear message
  if(NOT RTI_LIB_DIR)
    message(FATAL_ERROR "RTI_LIB_DIR not found. Set -DRTI_LIB_DIR=<path> or verify NDDSHOME.")
  endif()

  message(STATUS "Using RTI_LIB_DIR=${RTI_LIB_DIR}")
  target_link_directories(RtpDdsCore PUBLIC "${RTI_LIB_DIR}")

  target_compile_definitions(RtpDdsCore PUBLIC NDDS_DLL_VARIABLE)

  # --- Debug/Release에 맞는 Modern C++11 라이브러리 선택 ---
  if(EXISTS "${RTI_LIB_DIR}/nddscpp2d.lib")
    set(RTI_LIBS_DEBUG nddscpp2d nddscd nddscored)
  else()
    message(WARNING "RTI debug C++11 libraries not found in ${RTI_LIB_DIR}. Using release libs for Debug.")
    set(RTI_LIBS_DEBUG nddscpp2 nddsc nddscore)
  endif()

  # Modern C++11 API libs
  target_link_libraries(RtpDdsCore PUBLIC
    $<$<CONFIG:Debug>:${RTI_LIBS_DEBUG}>
    $<$<NOT:$<CONFIG:Debug>>:nddscpp2;nddsc;nddscore>
  )
endif()

add_executable(RtpDdsGateway src/main.cpp)
target_link_libraries(RtpDdsGateway PRIVATE RtpDdsCore ws2_32)

# copy QoS xml next to the exe
add_custom_command(TARGET RtpDdsGateway POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/qos/qos_profiles.xml
          $<TARGET_FILE_DIR:RtpDdsGateway>/qos_profiles.xml)
