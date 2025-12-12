file(GLOB_RECURSE SRC_FILES
  CONFIGURE_DEPENDS
  "src/*.cpp"
)
add_library(RtpDdsCore STATIC ${SRC_FILES})


# Require at least C++17 for this target
target_compile_features(RtpDdsCore PUBLIC cxx_std_17)

target_include_directories(RtpDdsCore PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_include_directories(RtpDdsCore PUBLIC include)
target_link_libraries(RtpDdsCore PUBLIC DkmRtpIpc)
if(IS_VXWORKS)
  # Toolchain에서 설정한 RTI_ROOT 사용
  if(DEFINED RTI_ROOT)
    message(STATUS "Using RTI CTL headers from RTI_ROOT=${RTI_ROOT}")
    target_include_directories(RtpDdsCore PUBLIC "${RTI_ROOT}/include" "${RTI_ROOT}/include/ndds" "${RTI_ROOT}/include/ndds/hpp")
  else()
    message(STATUS "Using RTI CTL headers from NDDSHOME_CTL=$ENV{NDDSHOME_CTL}")
    target_include_directories(RtpDdsCore PUBLIC "$ENV{NDDSHOME_CTL}/include" "$ENV{NDDSHOME_CTL}/include/ndds" "$ENV{NDDSHOME_CTL}/include/ndds/hpp")
  endif()
else()
  message(STATUS "Using RTI headers from NDDSHOME=$ENV{NDDSHOME}")
  target_include_directories(RtpDdsCore PUBLIC "$ENV{NDDSHOME}/include" "$ENV{NDDSHOME}/include/ndds" "$ENV{NDDSHOME}/include/ndds/hpp")
endif()

# IdlKit 산출물 사용
if(TARGET DdsTypes)
  message(STATUS "DdsTypes target found and linked.")  

  # IdlKit이 설정한 경로 읽기
  get_target_property(_IDL_GEN_DIR DdsTypes IDL_GEN_DIR)
  get_target_property(_IDL_INSTALL DdsTypes IDL_INSTALL_FACTORIES)
  # 생성 파일 표시(존재 유무와 무관하게 GENERATED 지정)
  set_source_files_properties("${_IDL_INSTALL}" PROPERTIES GENERATED TRUE)

  # 게이트웨이 타깃에 소스 추가
  target_sources(RtpDdsCore PRIVATE "${_IDL_INSTALL}")

  target_link_libraries(RtpDdsCore PUBLIC DdsTypes)
  target_include_directories(RtpDdsCore PUBLIC $<TARGET_PROPERTY:DdsTypes,INTERFACE_INCLUDE_DIRECTORIES>)
  
  add_dependencies(RtpDdsCore idl_xml idl_type_registry idl_json_bind)
endif()

if(IS_VXWORKS)
  message(STATUS "Configuring RtpDdsCore for VxWorks target.")
  if(DEFINED RTI_VX_LIB_DIR)
    set(RTI_LIB_DIR "${RTI_VX_LIB_DIR}")
  else()
    set(RTI_LIB_DIR "$ENV{NDDSHOME_CTL}/lib/ppce6500Vx23.03llvm15.0_rtp")
  endif()
else()
  # --- Auto-detect RTI lib dir (prefer x64Win64VS2017) ---
  if(NOT DEFINED RTI_LIB_DIR)
    set(_rti_candidates
      "$ENV{NDDSHOME}/lib/x64Win64VS2017"
      "$ENV{NDDSHOME}/lib/x64Win64VS2019"
      "$ENV{NDDSHOME}/lib/x64Win64VS2022"
      "$ENV{NDDSHOME}/lib/x64Win64"
      "$ENV{NDDSHOME}/lib/x64Linux4gcc7.3.0"
    )
    foreach(d ${_rti_candidates})
      if(EXISTS "${d}")
        set(RTI_LIB_DIR "${d}")
        break()
      endif()
    endforeach()
  endif()
endif()

# if empty or not found, fail with a clear message
if(NOT RTI_LIB_DIR)
    message(FATAL_ERROR "RTI_LIB_DIR not found. Set -DRTI_LIB_DIR explicitly "
                      "(example host: -DRTI_LIB_DIR=$ENV{NDDSHOME}/lib/x64Linux4gcc7.3.0, "
                      "VxWorks: uses $ENV{NDDSHOME_CTL}/lib/ppce6500Vx23.03llvm15.0_rtp).")
endif()

message(STATUS "Using RTI_LIB_DIR=${RTI_LIB_DIR}")
target_link_directories(RtpDdsCore PUBLIC "${RTI_LIB_DIR}")

if(NOT IS_VXWORKS)
  target_compile_definitions(RtpDdsCore PUBLIC NDDS_DLL_VARIABLE)
endif()

if(IS_VXWORKS)
  # VxWorks CTL: 정적 라이브러리(z) 사용. Debug 구성 시 *_zd 가 존재하면 구성별로 선택하고
  # 없으면 릴리즈(z) 라이브러리를 모든 구성에 공통 사용.
  set(_rti_vx_debug_libs nddscpp2zd nddsczd nddscorezd rticonnextmsgcpp2zd)
  set(_rti_vx_release_libs nddscpp2z nddscz nddscorez rticonnextmsgcpp2z)
  set(_rti_vx_lib_debug_available TRUE)
  foreach(_lib ${_rti_vx_debug_libs})
    # VxWorks 정적 라이브러리 확장자는 .a 로 가정
    if(NOT EXISTS "${RTI_LIB_DIR}/${_lib}.a")
      set(_rti_vx_lib_debug_available FALSE)
    endif()
  endforeach()
  if(_rti_vx_lib_debug_available)
    message(STATUS "RTI VxWorks debug static libs found; linking per configuration.")
    target_link_libraries(RtpDdsCore PUBLIC
      $<$<CONFIG:Debug>:nddscpp2zd>$<$<NOT:$<CONFIG:Debug>>:nddscpp2z>
      $<$<CONFIG:Debug>:nddsczd>$<$<NOT:$<CONFIG:Debug>>:nddscz>
      $<$<CONFIG:Debug>:nddscorezd>$<$<NOT:$<CONFIG:Debug>>:nddscorez>
      $<$<CONFIG:Debug>:rticonnextmsgcpp2zd>$<$<NOT:$<CONFIG:Debug>>:rticonnextmsgcpp2z>
    )
  else()
    message(WARNING "RTI VxWorks debug static libs not found; using release static libs for all configurations.")
    target_link_libraries(RtpDdsCore PUBLIC
      nddscpp2z
      nddscz
      nddscorez
      rticonnextmsgcpp2z
    )
  endif()
else()
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
# On Windows we need ws2_32; on Unix systems skip the Windows-only lib.
if(WIN32)
  target_link_libraries(RtpDdsGateway PRIVATE RtpDdsCore ws2_32)
else()
  target_link_libraries(RtpDdsGateway PRIVATE RtpDdsCore)
endif()

if(IS_VXWORKS)
  # VxWorks RTP는 기본적으로 정적 링크(-static)를 해야 타겟에 없는 .so 의존성을 피할 수 있음
  target_link_options(RtpDdsGateway PRIVATE -static)
endif()

# 실행 폴더에 XML 복사 (게이트웨이가 런타임에 XML 읽을 경우)
add_dependencies(RtpDdsGateway idl_xml)
add_custom_command(TARGET RtpDdsGateway POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          $<TARGET_PROPERTY:DdsTypeXml,IDL_XML_DIR>
          $<TARGET_FILE_DIR:RtpDdsGateway>/types/xml)

# 코드에서 런타임 XML 경로 접근이 필요하면 컴파일 정의 제공(선택)
target_compile_definitions(RtpDdsGateway PRIVATE
  IDL_XML_DIR_RELATIVE="types/xml")

# Debug 빌드에서만 RTI 로거 활성화
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(RtpDdsGateway PRIVATE _RTPDDS_DEBUG)
endif()

# copy entire qos folder next to the exe so runtime can use ./qos
add_custom_command(TARGET RtpDdsGateway POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/qos
          $<TARGET_FILE_DIR:RtpDdsGateway>/qos)

# copy agent_config.json if exists
if(EXISTS "${CMAKE_SOURCE_DIR}/agent_config.json")
  add_custom_command(TARGET RtpDdsGateway POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/agent_config.json
            $<TARGET_FILE_DIR:RtpDdsGateway>/agent_config.json)
endif()

# For VxWorks builds, produce a .vxe artifact alongside the built target so
# users can find an RTP/loader-friendly filename. We use a POST_BUILD copy
# (instead of changing OUTPUT_NAME) to avoid platform-specific suffix issues
# and to keep the normal target file intact.
if(IS_VXWORKS)
  add_custom_command(TARGET RtpDdsGateway POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE:RtpDdsGateway>
            $<TARGET_FILE_DIR:RtpDdsGateway>/RtpDdsGateway.vxe
    COMMENT "Creating VxWorks .vxe wrapper for RtpDdsGateway"
  )
endif()
