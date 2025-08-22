project(RtpDdsGateway LANGUAGES CXX)

# --- IDL -> generated code (Classic C++) ---
set(IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/types/StringMsg.idl)
set(GEN_DIR  ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

# rtiddsgen path & language (classic)
set(RTIDDSGEN "$ENV{NDDSHOME}/bin/rtiddsgen.bat")
set(RTIDDSGEN_LANG "C++98" CACHE STRING "rtiddsgen language for Classic C++ (C++ or C++03)")

add_custom_command(
  OUTPUT
    ${GEN_DIR}/StringMsg.h
    ${GEN_DIR}/StringMsg.cxx
    ${GEN_DIR}/StringMsgPlugin.h
    ${GEN_DIR}/StringMsgPlugin.cxx
    ${GEN_DIR}/StringMsgSupport.h
    ${GEN_DIR}/StringMsgSupport.cxx
    COMMAND "${RTIDDSGEN}"
          -language ${RTIDDSGEN_LANG}
          -platform x64Win64VS2017
          -d "${GEN_DIR}" "${IDL_FILE}"
  DEPENDS ${IDL_FILE}
  VERBATIM
)

add_library(RtpDdsCore STATIC
  src/gateway.cpp
  src/dds_manager.cpp
  src/ipc_adapter.cpp
  ${GEN_DIR}/StringMsg.cxx
  ${GEN_DIR}/StringMsgPlugin.cxx
  ${GEN_DIR}/StringMsgSupport.cxx
)

target_include_directories(RtpDdsCore PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

# our headers + generated headers
target_include_directories(RtpDdsCore PUBLIC include ${GEN_DIR})

# uses DkmRtpIpc headers in ipc_adapter.hpp
target_link_libraries(RtpDdsCore PUBLIC DkmRtpIpc)

# switch by USE_CONNEXT
target_compile_definitions(RtpDdsCore PUBLIC $<$<BOOL:${USE_CONNEXT}>:USE_CONNEXT>)

if(USE_CONNEXT)
  # RTI headers
  target_include_directories(RtpDdsCore PUBLIC "$ENV{NDDSHOME}/include" "$ENV{NDDSHOME}/include/ndds")


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

  # --- Debug/Release에 맞는 라이브러리 선택 ---
  # 디버그 lib가 없을 수도 있으니 존재 여부 먼저 확인
  if(EXISTS "${RTI_LIB_DIR}/nddscppd.lib")
    set(RTI_LIBS_DEBUG nddscppd nddscd nddscored)
  else()
    # 설치에 디버그 lib가 없다면 릴리스로 대체 (경고)
    message(WARNING "RTI debug libraries not found in ${RTI_LIB_DIR}. Using release libs for Debug.")
    set(RTI_LIBS_DEBUG nddscpp nddsc nddscore)
  endif()


  # Classic C++ API libs
  target_link_libraries(RtpDdsCore PUBLIC
    $<$<CONFIG:Debug>:${RTI_LIBS_DEBUG}>
    $<$<NOT:$<CONFIG:Debug>>:nddscpp;nddsc;nddscore>
  )
endif()

# ==== NGVA 옵션 ====
option(NGVA_MODE "Enable NGVA integration (RMDP IDL, QoS XML, handlers)" ON)

set(RMDP_ROOT        "${CMAKE_SOURCE_DIR}/ngva" CACHE PATH "NGVA RMDP root")
set(RMDP_IDL_DIR     "${RMDP_ROOT}/idl"         CACHE PATH "NGVA IDL dir")
set(RMDP_QOS_FILE    "${RMDP_ROOT}/qos/ngva_qos_profiles.xml" CACHE FILEPATH "NGVA QoS XML")
set(NDDSGEN_PLATFORM "x64Win64VS2017"           CACHE STRING "rtiddsgen -platform")
set(NGVA_GEN_DIR     "${CMAKE_BINARY_DIR}/RtpDdsGateway/generated_ngva" CACHE PATH "NGVA generated out")

if (NGVA_MODE)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NgvaRtiddsgen.cmake)

  # (선택) 매니페스트 파일: 특정 IDL의 타입 이름을 적어두면,
  # 생성된 .cxx를 RtpDdsCore 타깃에 자동 편입
  if (EXISTS "${RMDP_ROOT}/manifest.cmake")
    include("${RMDP_ROOT}/manifest.cmake")
  endif()

  # rtiddsgen 수행 + (가능하면) 생성 소스 타깃에 편입
  ngva_configure_codegen(
    TARGET RtpDdsCore
    IDL_DIR "${RMDP_IDL_DIR}"
    OUT_DIR "${NGVA_GEN_DIR}"
    PLATFORM "${NDDSGEN_PLATFORM}"
    # 매니페스트에서 채워진 변수들 사용 (없으면 DynamicData fallback만)
    MANIFEST_IDL_LIST "${NGVA_IDL_LIST}"
  )

  # 생성 헤더 포함
  target_include_directories(RtpDdsCore PRIVATE "${NGVA_GEN_DIR}")
  # NGVA 소스 추가 (QoS 로더/핸들러/프로브)
  target_sources(RtpDdsCore PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngva_qos_loader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngva_handlers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ngva_dynamic_probe.cpp
  )

  target_compile_definitions(RtpDdsCore PRIVATE NGVA_ENABLED=1)
endif()

add_executable(RtpDdsGateway src/main.cpp)
target_link_libraries(RtpDdsGateway PRIVATE RtpDdsCore ws2_32)

# copy QoS xml next to the exe
add_custom_command(TARGET RtpDdsGateway POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/qos/qos_profiles.xml
          $<TARGET_FILE_DIR:RtpDdsGateway>/qos_profiles.xml)
