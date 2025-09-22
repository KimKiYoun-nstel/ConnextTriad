project(ConnextControlUI LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Xml)


set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    include/mainwindow.hpp
    src/xml_type_catalog.cpp
    include/xml_type_catalog.hpp
    src/generic_form_dialog.cpp
    include/generic_form_dialog.hpp
)

add_executable(ConnextControlUI WIN32 ${SOURCES})

target_include_directories(ConnextControlUI PRIVATE include)
target_include_directories(ConnextControlUI PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

target_link_libraries(ConnextControlUI PRIVATE Qt6::Widgets Qt6::Xml DkmRtpIpc)



# IdlKit이 먼저 빌드된다고 가정 (최상위 CMake에 add_subdirectory(IdlKit) 존재)
add_dependencies(ConnextControlUI idl_xml)

# 실행 폴더에 XML 복사
add_custom_command(TARGET ConnextControlUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
                    $<TARGET_PROPERTY:DdsTypeXml,IDL_XML_DIR>
                    $<TARGET_FILE_DIR:ConnextControlUI>/types/xml)

target_compile_definitions(ConnextControlUI PRIVATE IDL_XML_DIR_RELATIVE="types/xml")