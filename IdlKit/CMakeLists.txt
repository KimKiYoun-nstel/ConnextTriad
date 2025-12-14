cmake_minimum_required(VERSION 3.16)
project(IdlKit LANGUAGES CXX)

find_package(Python3 REQUIRED)

# rtiddsgen 경로
# VxWorks(CTL) 빌드일 때는 NDDSHOME_CTL/bin에서, 그 외에는 NDDSHOME/bin에서 찾는다.
if(NOT DEFINED RTIDDSGEN_EXECUTABLE OR RTIDDSGEN_EXECUTABLE STREQUAL "")
  if(IS_VXWORKS)
    # Toolchain에서 설정한 RTI_ROOT 사용
    if(DEFINED RTI_ROOT)
      set(RTI_SEARCH_PATH "${RTI_ROOT}/bin")
    elseif(DEFINED ENV{NDDSHOME_CTL})
      set(RTI_SEARCH_PATH "$ENV{NDDSHOME_CTL}/bin")
    else()
      set(RTI_SEARCH_PATH "")
    endif()
  else()
    if(DEFINED ENV{NDDSHOME})
      set(RTI_SEARCH_PATH "$ENV{NDDSHOME}/bin")
    else()
      set(RTI_SEARCH_PATH "")
    endif()
  endif()

  # Prefer the requested RTI install explicitly via HINTS so that CTL/NDDSHOME
  # bin directories are searched before the system PATH. If RTI_SEARCH_PATH
  # is empty the HINTS are ignored and normal PATH search still applies.
  if(IS_VXWORKS AND DEFINED RTI_ROOT)
    find_program(RTIDDSGEN_EXECUTABLE
      NAMES rtiddsgen rtiddsgen.bat
      HINTS "${RTI_ROOT}/bin"
      PATHS ${RTI_SEARCH_PATH}
    )
  elseif(IS_VXWORKS AND DEFINED ENV{NDDSHOME_CTL})
    find_program(RTIDDSGEN_EXECUTABLE
      NAMES rtiddsgen rtiddsgen.bat
      HINTS "$ENV{NDDSHOME_CTL}/bin"
      PATHS ${RTI_SEARCH_PATH}
    )
  elseif(NOT IS_VXWORKS AND DEFINED ENV{NDDSHOME})
    find_program(RTIDDSGEN_EXECUTABLE
      NAMES rtiddsgen rtiddsgen.bat
      HINTS "$ENV{NDDSHOME}/bin"
      PATHS ${RTI_SEARCH_PATH}
    )
  else()
    find_program(RTIDDSGEN_EXECUTABLE
      NAMES rtiddsgen rtiddsgen.bat
      PATHS ${RTI_SEARCH_PATH}
    )
  endif()

  if(RTIDDSGEN_EXECUTABLE)
    set(RTIDDSGEN_EXECUTABLE "${RTIDDSGEN_EXECUTABLE}" CACHE FILEPATH "rtiddsgen path")
  endif()
endif()

if(NOT RTIDDSGEN_EXECUTABLE)
  if(IS_VXWORKS)
    message(FATAL_ERROR "VxWorks/CTL build: set NDDSHOME_CTL or RTIDDSGEN_EXECUTABLE (CTL용 rtiddsgen 필요)")
  else()
    message(FATAL_ERROR "Set NDDSHOME or RTIDDSGEN_EXECUTABLE")
  endif()
else()
  message(STATUS "RTIDDSGEN executable: ${RTIDDSGEN_EXECUTABLE}")
endif()



# 경로
set(IDL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/idl)
file(GLOB IDL_FILES "${IDL_DIR}/*.idl")
set(GEN_DIR ${CMAKE_BINARY_DIR}/idlkit/gen)
set(XML_DIR ${CMAKE_BINARY_DIR}/idlkit/xml)

# per-IDL 산출물 선언
set(GEN_CPP_SOURCES "")
set(GEN_HEADERS "")
set(XML_OUTPUTS "")

foreach(IDL ${IDL_FILES})
  get_filename_component(IDL_NAME "${IDL}" NAME_WE)

  set(OUT_HPP       ${GEN_DIR}/${IDL_NAME}.hpp)
  set(OUT_CXX       ${GEN_DIR}/${IDL_NAME}.cxx)
  set(OUT_PLUG_HPP  ${GEN_DIR}/${IDL_NAME}Plugin.hpp)
  set(OUT_PLUG_CXX  ${GEN_DIR}/${IDL_NAME}Plugin.cxx)

  add_custom_command(
    OUTPUT ${OUT_HPP} ${OUT_CXX} ${OUT_PLUG_HPP} ${OUT_PLUG_CXX}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_DIR}
    COMMAND "${RTIDDSGEN_EXECUTABLE}" -language C++11
            -d "${GEN_DIR}" "${IDL}"
    DEPENDS ${IDL}
    COMMENT "rtiddsgen C++: ${IDL_NAME}"
  )

  list(APPEND GEN_CPP_SOURCES ${OUT_CXX} ${OUT_PLUG_CXX})
  list(APPEND GEN_HEADERS     ${OUT_HPP} ${OUT_PLUG_HPP})

  set(OUT_XML ${XML_DIR}/${IDL_NAME}.xml)
  add_custom_command(
    OUTPUT ${OUT_XML}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${XML_DIR}
    COMMAND "${RTIDDSGEN_EXECUTABLE}" -convertToXml -d "${XML_DIR}" "${IDL}"
    DEPENDS ${IDL}
    COMMENT "rtiddsgen XML: ${IDL_NAME}"
  )
  list(APPEND XML_OUTPUTS ${OUT_XML})
endforeach()

add_custom_target(idl_cpp ALL DEPENDS ${GEN_CPP_SOURCES} ${GEN_HEADERS})
add_custom_target(idl_xml ALL DEPENDS ${XML_OUTPUTS})

add_custom_command(
  OUTPUT
    ${GEN_DIR}/idl_type_registry.hpp
    ${GEN_DIR}/idl_type_registry.cpp
    ${GEN_DIR}/idl_install_factories.cpp
    ${GEN_DIR}/idl_generated_includes.hpp
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/emit_autoreg.py ${GEN_DIR} ${XML_DIR}
  # Ensure both generated C++ headers/sources and the XML outputs are ready
  # before running the autoreg script. Previously this depended only on
  # idl_cpp which could race with XML generation and yield an empty registry.
  DEPENDS idl_cpp idl_xml
  COMMENT "emit type registry + install factories"
)

add_custom_target(idl_type_registry DEPENDS ${GEN_DIR}/idl_type_registry.hpp ${GEN_DIR}/idl_type_registry.cpp ${GEN_DIR}/idl_generated_includes.hpp)

# JSON 바인딩 생성
add_custom_command(
  OUTPUT ${GEN_DIR}/idl_json_bind.hpp ${GEN_DIR}/idl_json_bind.cpp
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/emit_jsonbind.py --out-dir ${GEN_DIR} --xml-dir ${XML_DIR}
  # Ensure per-IDL headers and the auto-registry/umbrella header are generated first.
  DEPENDS ${GEN_HEADERS} ${XML_OUTPUTS} ${GEN_DIR}/idl_type_registry.hpp ${GEN_DIR}/idl_install_factories.cpp ${GEN_DIR}/idl_generated_includes.hpp
  COMMENT "Emit JSON bindings from XML"
)
add_custom_target(idl_json_bind DEPENDS ${GEN_DIR}/idl_json_bind.hpp ${GEN_DIR}/idl_json_bind.cpp)

# Ensure idl_json_bind generation waits for idl_type_registry/autoreg generation
add_dependencies(idl_json_bind idl_type_registry)

# 타입 라이브러리: 실제 생성된 .cxx + type registry + json bind
add_library(DdsTypes STATIC ${GEN_CPP_SOURCES} ${GEN_DIR}/idl_type_registry.cpp ${GEN_DIR}/idl_json_bind.cpp)
add_dependencies(DdsTypes idl_cpp idl_xml idl_type_registry idl_json_bind)

# Suppress C++20 designated-initializer warnings when building with Clang
# (VxWorks toolchain uses clang via wr-c++). The generated IDL code and RTI
# headers use designated initializers which are a C++20 feature; we compile
# with -std=c++17 so clang warns. Disable that specific warning for this
# target only to avoid noisy logs while keeping other warnings enabled.
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  target_compile_options(DdsTypes PRIVATE -Wno-c++20-designator)
endif()
# --- RTI include path for generated code ---
if(IS_VXWORKS)
  if(DEFINED RTI_ROOT)
    set(_RTI_IDL_INCLUDE "${RTI_ROOT}/include")
  elseif(DEFINED ENV{NDDSHOME_CTL})
    set(_RTI_IDL_INCLUDE "$ENV{NDDSHOME_CTL}/include")
  else()
    message(FATAL_ERROR "VxWorks/CTL build: NDDSHOME_CTL must be set for IdlKit")
  endif()
else()
  if(DEFINED ENV{NDDSHOME})
    set(_RTI_IDL_INCLUDE "$ENV{NDDSHOME}/include")
  else()
    message(FATAL_ERROR "Host build: NDDSHOME must be set for IdlKit")
  endif()
endif()
target_include_directories(DdsTypes PUBLIC ${GEN_DIR}
                                     PRIVATE "${_RTI_IDL_INCLUDE}" 
                                     "${_RTI_IDL_INCLUDE}/ndds" 
                                     "${_RTI_IDL_INCLUDE}/ndds/hpp" 
                                     "${CMAKE_SOURCE_DIR}/third_party" 
                                     "${CMAKE_SOURCE_DIR}/RtpDdsGateway/include")
if(WIN32)
  target_compile_definitions(DdsTypes PRIVATE NDDS_DLL_VARIABLE)
endif()

# RTI 플랫폼 매크로
if(IS_VXWORKS)
  # VxWorks + clang(llvm) 환경
  # Toolchain에서 이미 정의되었으므로 중복 정의하지 않음
  # target_compile_definitions(DdsTypes PRIVATE RTI_VXWORKS RTI_CLANG)
elseif(NOT WIN32)
  # Linux/기타 Unix
  target_compile_definitions(DdsTypes PRIVATE RTI_UNIX=1)
endif()


# GEN_DIR과 install TU 경로를 export
set_property(TARGET DdsTypes PROPERTY IDL_GEN_DIR "${GEN_DIR}")
set_property(TARGET DdsTypes PROPERTY IDL_INSTALL_FACTORIES "${GEN_DIR}/idl_install_factories.cpp")

# XML 소비자(경로 노출)
add_library(DdsTypeXml INTERFACE)
add_dependencies(DdsTypeXml idl_xml)
set_property(TARGET DdsTypeXml PROPERTY IDL_XML_DIR "${XML_DIR}")

# 설치(선택)
install(TARGETS DdsTypes)
install(DIRECTORY ${XML_DIR}/ DESTINATION share/idl/xml)
install(DIRECTORY ${GEN_DIR}/ DESTINATION include/idl/gen FILES_MATCHING PATTERN "*.hpp")
