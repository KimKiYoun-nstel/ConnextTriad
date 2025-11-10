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
target_include_directories(RtpDdsCore PUBLIC "$ENV{NDDSHOME}/include" "$ENV{NDDSHOME}/include/ndds" "$ENV{NDDSHOME}/include/ndds/hpp")

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

# if empty or not found, fail with a clear message
if(NOT RTI_LIB_DIR)
  message(FATAL_ERROR "RTI_LIB_DIR not found. Set -DRTI_LIB_DIR=<path> or verify NDDSHOME (example: -DRTI_LIB_DIR=$ENV{NDDSHOME}/lib/x64Linux4gcc7.3.0).")
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


add_executable(RtpDdsGateway src/main.cpp)
# On Windows we need ws2_32; on Unix systems skip the Windows-only lib.
if(WIN32)
  target_link_libraries(RtpDdsGateway PRIVATE RtpDdsCore ws2_32)
else()
  target_link_libraries(RtpDdsGateway PRIVATE RtpDdsCore)
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

# copy entire qos folder next to the exe so runtime can use ./qos
add_custom_command(TARGET RtpDdsGateway POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/qos
          $<TARGET_FILE_DIR:RtpDdsGateway>/qos)
