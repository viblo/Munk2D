add_executable(munkbench
  munkbench.c
)

target_include_directories(munkbench PRIVATE ${chipmunk_SOURCE_DIR}/include)
target_link_libraries(munkbench PRIVATE chipmunk_static)
target_compile_definitions(munkbench PRIVATE MUNK2D_VERSION="${MUNK2D_VERSION}")

if(ANDROID OR UNIX)
  target_link_libraries(munkbench PRIVATE m)
endif()

if(MSVC)
  set_source_files_properties(munkbench.c PROPERTIES LANGUAGE CXX)
  set_target_properties(munkbench PROPERTIES LINKER_LANGUAGE CXX)
endif()

# Keep these mixed workloads separate from the published browser suite.
add_executable(munkbench_mixed mixedbench.c)
target_include_directories(munkbench_mixed PRIVATE ${chipmunk_SOURCE_DIR}/include)
target_link_libraries(munkbench_mixed PRIVATE chipmunk_static)
if(ANDROID OR UNIX)
  target_link_libraries(munkbench_mixed PRIVATE m)
endif()
if(MSVC)
  set_source_files_properties(mixedbench.c PROPERTIES LANGUAGE CXX)
  set_target_properties(munkbench_mixed PROPERTIES LINKER_LANGUAGE CXX)
endif()
if(BUILD_TESTING)
  foreach(scene light dense mechanisms)
    add_test(NAME munkbench_mixed_${scene}_smoke COMMAND munkbench_mixed --scene ${scene} --count 64 --steps 120)
  endforeach()
endif()

if(BUILD_TESTING)
  # Exercise both the new selector and rejection of the removed spelling.
  # Use the configured emulator for cross-compiled (e.g. Wasm) executables.
  add_test(NAME baseline_solver_cli
    COMMAND ${CMAKE_COMMAND}
      "-DMUNKBENCH=$<TARGET_FILE:munkbench>"
      "-DMIXEDBENCH=$<TARGET_FILE:munkbench_mixed>"
      "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/test_baseline_cli.cmake")
  set_tests_properties(baseline_solver_cli PROPERTIES TIMEOUT 60)
  add_executable(munkbench_cleanup_tests test_cleanup.c)
  target_include_directories(munkbench_cleanup_tests PRIVATE ${chipmunk_SOURCE_DIR}/include)
  target_link_libraries(munkbench_cleanup_tests PRIVATE chipmunk_static)

  if(ANDROID OR UNIX)
    target_link_libraries(munkbench_cleanup_tests PRIVATE m)
  endif()

  if(MSVC)
    set_source_files_properties(test_cleanup.c PROPERTIES LANGUAGE CXX)
    set_target_properties(munkbench_cleanup_tests PROPERTIES LINKER_LANGUAGE CXX)
  endif()

  add_test(NAME munkbench_cleanup_tests COMMAND munkbench_cleanup_tests)
endif()
