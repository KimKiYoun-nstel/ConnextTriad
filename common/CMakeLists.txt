add_library(parser_loader STATIC parser_loader.cpp)
target_include_directories(parser_loader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_library(parser_client STATIC parser_client.cpp)
target_include_directories(parser_client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(parser_client PUBLIC parser_config)
if(PARSER_LINK_MODE STREQUAL "dynamic")
  target_link_libraries(parser_client PRIVATE parser_loader)     # 동적 경로
else()
  target_link_libraries(parser_client PRIVATE ngva_parser_static) # 정적 경로
endif()
