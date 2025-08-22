# cmake - UF: UTF-8
function(ngva_configure_codegen)
  set(oneValueArgs TARGET IDL_DIR OUT_DIR PLATFORM)
  set(multiValueArgs MANIFEST_IDL_LIST)
  cmake_parse_arguments(NGVA "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if (NOT NGVA_TARGET OR NOT NGVA_IDL_DIR OR NOT NGVA_OUT_DIR)
    message(FATAL_ERROR "ngva_configure_codegen: TARGET/IDL_DIR/OUT_DIR 필요")
  endif()

  file(GLOB NGVA_IDLS "${NGVA_IDL_DIR}/*.idl")
  if (NGVA_IDLS STREQUAL "")
    message(WARNING "[NGVA] IDL이 없습니다: ${NGVA_IDL_DIR}")
  endif()

  file(MAKE_DIRECTORY "${NGVA_OUT_DIR}")

  # rtiddsgen.bat 경로 탐색 (PATH에 있거나 RTI 설치 위치에서)
  find_program(NDDSGEN_EXE
    NAMES rtiddsgen.bat rtiddsgen
    HINTS "$ENV{RTI_HOME}/bin" "C:/Program Files/rti_connext_dds-7.5.0/bin"
  )
  if (NOT NDDSGEN_EXE)
    message(FATAL_ERROR "[NGVA] rtiddsgen(.bat) 찾을 수 없음. RTI_HOME/bin PATH 확인")
  endif()

  set(NGVA_STAMPS "")
  foreach(idl ${NGVA_IDLS})
    get_filename_component(base "${idl}" NAME_WE)
    set(stamp "${NGVA_OUT_DIR}/${base}.stamp")
    add_custom_command(
      OUTPUT "${stamp}"
      COMMAND "${NDDSGEN_EXE}" -language C++98 -platform ${NGVA_PLATFORM}
              -d "${NGVA_OUT_DIR}" "${idl}"
      COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
      DEPENDS "${idl}"
      VERBATIM
      COMMENT "[NGVA] rtiddsgen ${base}.idl"
    )
    list(APPEND NGVA_STAMPS "${stamp}")
  endforeach()

  if (NGVA_STAMPS)
    add_custom_target(ngva_types ALL DEPENDS ${NGVA_STAMPS})
    add_dependencies(${NGVA_TARGET} ngva_types)
  endif()

  # (선택) 매니페스트 기반으로 생성 소스 편입
  if (NGVA_MANIFEST_IDL_LIST)
    set(gen_srcs "")
    foreach(mid ${NGVA_MANIFEST_IDL_LIST})
      get_filename_component(base "${mid}" NAME_WE)
      list(APPEND gen_srcs
        "${NGVA_OUT_DIR}/${base}.cxx"
        "${NGVA_OUT_DIR}/${base}Plugin.cxx"
        "${NGVA_OUT_DIR}/${base}Support.cxx"
      )
    endforeach()
    # 존재하는 파일만 추가 (처음 구성 시엔 없을 수 있음)
    foreach(s ${gen_srcs})
      if (EXISTS "${s}")
        target_sources(${NGVA_TARGET} PRIVATE "${s}")
      endif()
    endforeach()
  endif()
endfunction()
