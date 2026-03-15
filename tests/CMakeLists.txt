add_executable(chipmunk_basic_tests
  test_basic.c
)

target_include_directories(chipmunk_basic_tests PRIVATE ${chipmunk_SOURCE_DIR}/include)
target_link_libraries(chipmunk_basic_tests PRIVATE chipmunk_static)

if(ANDROID OR UNIX)
  target_link_libraries(chipmunk_basic_tests PRIVATE m)
endif()

if(MSVC)
  set_source_files_properties(test_basic.c PROPERTIES LANGUAGE CXX)
  set_target_properties(chipmunk_basic_tests PROPERTIES LINKER_LANGUAGE CXX)
endif()

add_test(NAME chipmunk_basic_tests COMMAND chipmunk_basic_tests)
