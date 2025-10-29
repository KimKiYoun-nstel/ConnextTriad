cmake_minimum_required(VERSION 3.16)
project(IdlKit)

find_package(Python3 REQUIRED)

# rtiddsgen 경로
set(RTIDDSGEN_EXECUTABLE "$ENV{NDDSHOME}/bin/rtiddsgen.bat" CACHE FILEPATH "rtiddsgen path")
if(NOT EXISTS "${RTIDDSGEN_EXECUTABLE}")
  message(FATAL_ERROR "Set NDDSHOME or RTIDDSGEN_EXECUTABLE")
endif()
message(WARNING "${RTIDDSGEN_EXECUTABLE}")

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
target_include_directories(DdsTypes PUBLIC ${GEN_DIR}
                                     PRIVATE "$ENV{NDDSHOME}/include" "$ENV{NDDSHOME}/include/ndds" "$ENV{NDDSHOME}/include/ndds/hpp" "${CMAKE_SOURCE_DIR}/third_party" "${CMAKE_SOURCE_DIR}/RtpDdsGateway/include")
target_compile_definitions(DdsTypes PRIVATE NDDS_DLL_VARIABLE)

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
