find_package(Python3 COMPONENTS Interpreter)

set(GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/generated)
file(MAKE_DIRECTORY ${GEN_DIR})

# IDL 경로(지금 트리 기준)
set(IDL_FILES
  ${CMAKE_SOURCE_DIR}/RtpDdsGateway/types/Alarms_PSM.idl
  ${CMAKE_SOURCE_DIR}/RtpDdsGateway/types/LDM_Common.idl
  ${CMAKE_SOURCE_DIR}/RtpDdsGateway/types/AlarmMsg.idl
  ${CMAKE_SOURCE_DIR}/RtpDdsGateway/types/StringMsg.idl
)

if(Python3_Interpreter_FOUND)
  add_custom_command(
    OUTPUT  ${GEN_DIR}/parser_core_gen.cpp
            ${GEN_DIR}/parser_core_gen.hpp
            ${GEN_DIR}/enum_tables_gen.hpp
    COMMAND ${CMAKE_COMMAND} -E env PYTHONUTF8=1
            ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/idl2json/gen.py
            --idl ${IDL_FILES}
            --discover
            --out ${GEN_DIR}
    DEPENDS ${IDL_FILES}
            ${CMAKE_SOURCE_DIR}/tools/idl2json/gen.py
            ${CMAKE_SOURCE_DIR}/tools/idl2json/templates/struct_from.tmpl
            ${CMAKE_SOURCE_DIR}/tools/idl2json/templates/struct_to.tmpl
            ${CMAKE_SOURCE_DIR}/tools/idl2json/templates/registry.tmpl
    COMMENT "Generating JSON parser sources (stub for now)"
  )
else()
  message(WARNING "Python3 not found. Using existing generated sources in ${GEN_DIR}")
endif()

add_library(parser_core_obj OBJECT
  src/parser_core.cpp
  ${GEN_DIR}/parser_core_gen.cpp
)
target_include_directories(parser_core_obj PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${GEN_DIR}
  ${CMAKE_SOURCE_DIR}/third_party
)
target_link_libraries(parser_core_obj PUBLIC parser_config)



# 공유/정적 아티팩트
add_library(ngva_parser_shared SHARED $<TARGET_OBJECTS:parser_core_obj> src/exports.cpp)
target_include_directories(ngva_parser_shared PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${GEN_DIR}
  ${CMAKE_SOURCE_DIR}/common
)
target_compile_definitions(ngva_parser_shared PRIVATE PARSER_BUILD_SHARED=1)
set_target_properties(ngva_parser_shared PROPERTIES
  OUTPUT_NAME         "ngva_parser"        # DLL
  ARCHIVE_OUTPUT_NAME "ngva_parser_dyn"    # DLL import .lib 이름(충돌 방지)
)


add_library(ngva_parser_static STATIC
  $<TARGET_OBJECTS:parser_core_obj>
  src/exports.cpp
)
target_include_directories(ngva_parser_static PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${GEN_DIR}
  ${CMAKE_SOURCE_DIR}/common
)
set_target_properties(ngva_parser_static PROPERTIES OUTPUT_NAME "ngva_parser_static")
