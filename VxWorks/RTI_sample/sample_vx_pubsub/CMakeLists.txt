cmake_minimum_required(VERSION 3.10)
project(rti_vx_pubsub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_RTI "Link against RTI Connext" ON)

if(NOT DEFINED NDDSHOME_CTL)
  set(NDDSHOME_CTL "$ENV{NDDSHOME_CTL}")
endif()

if(NDDSHOME_CTL)
  message(STATUS "NDDSHOME_CTL=${NDDSHOME_CTL}")
else()
  message(FATAL_ERROR "NDDSHOME_CTL not set. Set the environment variable or pass -DNDDSHOME_CTL=... to CMake")
endif()

if(USE_RTI)
  include_directories(
    ${NDDSHOME_CTL}/include
    ${NDDSHOME_CTL}/include/ndds
    ${NDDSHOME_CTL}/include/ndds/hpp
  )
  find_program(RTIDDSGEN_EXECUTABLE rtiddsgen.bat HINTS ${NDDSHOME_CTL}/bin PATHS ENV PATH)
  if(NOT RTIDDSGEN_EXECUTABLE)
    message(FATAL_ERROR "rtiddsgen not found. Ensure ${NDDSHOME_CTL}/bin is on PATH or rtiddsgen is installed.")
  endif()
else()
  message(FATAL_ERROR "This sample requires RTI Connext (USE_RTI=ON). Set USE_RTI=ON when configuring.")
endif()

# Paths
set(IDL_FILE ${CMAKE_CURRENT_SOURCE_DIR}/HelloWorld.idl)
set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Run rtiddsgen at configure time to generate C++11 type support
message(STATUS "Generating type support from ${IDL_FILE} into ${GEN_DIR}")
file(MAKE_DIRECTORY ${GEN_DIR})
execute_process(
  COMMAND "${RTIDDSGEN_EXECUTABLE}" -language C++11 -d "${GEN_DIR}" "${IDL_FILE}"
  RESULT_VARIABLE RTIDDSGEN_RES
  OUTPUT_VARIABLE RTIDDSGEN_OUT
  ERROR_VARIABLE RTIDDSGEN_ERR
)
if(NOT RTIDDSGEN_RES EQUAL 0)
  message(FATAL_ERROR "rtiddsgen failed (rc=${RTIDDSGEN_RES}):\n${RTIDDSGEN_OUT}\n${RTIDDSGEN_ERR}")
endif()

# Collect generated sources & headers
file(GLOB GENERATED_SRCS "${GEN_DIR}/*.cxx" "${GEN_DIR}/*.cpp")
file(GLOB GENERATED_HDRS "${GEN_DIR}/*.h" "${GEN_DIR}/*.hpp")
include_directories(${GEN_DIR})

add_executable(rti_pub src/publisher.cpp ${GENERATED_SRCS})
add_executable(rti_sub src/subscriber.cpp ${GENERATED_SRCS})

# Link against RTI libraries (platform-specific names may vary)
if(DEFINED RTI_VX_LIB_DIR)
  message(STATUS "Using RTI_VX_LIB_DIR=${RTI_VX_LIB_DIR}")
  link_directories(${RTI_VX_LIB_DIR})
else()
  link_directories(${NDDSHOME_CTL}/lib)
endif()

# Prefer static RTI libraries from the VxWorks RTI lib dir. Try to find the
# platform-specific static archive matching libnddscpp*.a and libnddscore*.a
set(NDDSCPP_LIB_PATH "")
set(NDDSCORE_LIB_PATH "")
if(DEFINED RTI_VX_LIB_DIR)
  file(GLOB NDDSCPP_CANDIDATES "${RTI_VX_LIB_DIR}/libnddscpp*.a")
  file(GLOB NDDSCORE_CANDIDATES "${RTI_VX_LIB_DIR}/libnddscore*.a")
else()
  file(GLOB NDDSCPP_CANDIDATES "${NDDSHOME_CTL}/lib/libnddscpp*.a")
  file(GLOB NDDSCORE_CANDIDATES "${NDDSHOME_CTL}/lib/libnddscore*.a")
endif()

function(pick_static_lib out_var candidates)
  set(selected "")
  foreach(f ${${candidates}})
    # prefer non-debug variant (no trailing 'd' before .a)
    if(NOT f MATCHES "d\.a$")
      set(selected ${f})
      break()
    endif()
  endforeach()
  if(NOT selected AND ${candidates})
    list(GET ${candidates} 0 selected)
  endif()
  set(${out_var} ${selected} PARENT_SCOPE)
endfunction()

pick_static_lib(NDDSCPP_LIB_PATH NDDSCPP_CANDIDATES)
pick_static_lib(NDDSCORE_LIB_PATH NDDSCORE_CANDIDATES)

if(NDDSCPP_LIB_PATH)
  message(STATUS "Found static nddscpp: ${NDDSCPP_LIB_PATH}")
endif()
if(NDDSCORE_LIB_PATH)
  message(STATUS "Found static nddscore: ${NDDSCORE_LIB_PATH}")
endif()

# Collect all RTI static archives in RTI_VX_LIB_DIR so dependent C libraries are linked too
set(RTI_LIB_LIST)
if(DEFINED RTI_VX_LIB_DIR)
  file(GLOB RTI_STATIC_LIBS "${RTI_VX_LIB_DIR}/*.a")
  if(RTI_STATIC_LIBS)
    list(SORT RTI_STATIC_LIBS)
    foreach(l ${RTI_STATIC_LIBS})
      list(APPEND RTI_LIB_LIST ${l})
    endforeach()
    message(STATUS "Collected RTI static libs: ${RTI_LIB_LIST}")
  endif()
endif()

target_compile_definitions(rti_pub PRIVATE RTI_CONNEXT)
target_compile_definitions(rti_sub PRIVATE RTI_CONNEXT)

# Link: prefer full-path static libs found earlier; otherwise fall back to plain names
if(RTI_LIB_LIST)
  message(STATUS "Linking with full RTI static list (RTI_LIB_LIST) using linker group")
  # Wrap the static archives in a linker group to allow the linker to resolve
  # circular dependencies between the RTI libraries when linking static .a files.
  # This uses the GNU/ld style --start-group/--end-group flags which are
  # supported by the VxWorks toolchain linker (ldppc). If your linker does
  # not accept -Wl,--start-group, remove these strings and link the list
  # manually in the correct order.
  target_link_libraries(rti_pub PRIVATE "-Wl,--start-group" ${RTI_LIB_LIST} "-Wl,--end-group")
  target_link_libraries(rti_sub PRIVATE "-Wl,--start-group" ${RTI_LIB_LIST} "-Wl,--end-group")
elseif(NDDSCPP_LIB_PATH AND NDDSCORE_LIB_PATH)
  target_link_libraries(rti_pub PRIVATE ${NDDSCPP_LIB_PATH} ${NDDSCORE_LIB_PATH})
  target_link_libraries(rti_sub PRIVATE ${NDDSCPP_LIB_PATH} ${NDDSCORE_LIB_PATH})
else()
  message(WARNING "Could not find platform-specific static RTI libs; falling back to library names (-lnddscpp -lnddscore)")
  target_link_libraries(rti_pub PRIVATE nddscpp nddscore)
  target_link_libraries(rti_sub PRIVATE nddscpp nddscore)
endif()

install(TARGETS rti_pub rti_sub RUNTIME DESTINATION bin)

