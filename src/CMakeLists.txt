file(GLOB chipmunk_source_files "*.c")
file(GLOB chipmunk_public_header "${chipmunk_SOURCE_DIR}/include/chipmunk/*.h")

include_directories(${chipmunk_SOURCE_DIR}/include)

# Munk2D 2.0.0
set(MUNK2D_VERSION_MAJOR 2)
set(MUNK2D_VERSION_MINOR 0)
set(MUNK2D_VERSION_PATCH 1)
set(MUNK2D_VERSION "${MUNK2D_VERSION_MAJOR}.${MUNK2D_VERSION_MINOR}.${MUNK2D_VERSION_PATCH}")
message("Configuring Munk2D version ${MUNK2D_VERSION}")


if(BUILD_SHARED)
  add_library(chipmunk SHARED
    ${chipmunk_source_files}
  )
  # Tell MSVC to compile the code as C++.
  if(MSVC)
    set_source_files_properties(${chipmunk_source_files} PROPERTIES LANGUAGE CXX)
    set_target_properties(chipmunk PROPERTIES LINKER_LANGUAGE CXX)
  endif(MSVC)
  # set the lib's version number
  # But avoid on Android because symlinks to version numbered .so's don't work with Android's Java-side loadLibrary.
  if(NOT ANDROID)
    set_target_properties(chipmunk PROPERTIES
      SOVERSION ${MUNK2D_VERSION_MAJOR}
      VERSION ${MUNK2D_VERSION})
  endif(NOT ANDROID)
  if(ANDROID OR UNIX)
	  # need to explicitly link to the math library because the CMake/Android toolchains may not do it automatically
	  target_link_libraries(chipmunk m)
  endif(ANDROID OR UNIX)
  install(TARGETS chipmunk RUNTIME DESTINATION ${BIN_INSTALL_DIR}
                           LIBRARY DESTINATION ${LIB_INSTALL_DIR}
                           ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
endif(BUILD_SHARED)

if(BUILD_STATIC)
  add_library(chipmunk_static STATIC
    ${chipmunk_source_files}
  )
  # Tell MSVC to compile the code as C++.
  if(MSVC)
    set_source_files_properties(${chipmunk_source_files} PROPERTIES LANGUAGE CXX)
    set_target_properties(chipmunk_static PROPERTIES LINKER_LANGUAGE CXX)
  endif(MSVC)
  # Sets chipmunk_static to output "libchipmunk.a" not "libchipmunk_static.a"
  set_target_properties(chipmunk_static PROPERTIES OUTPUT_NAME chipmunk)
  if(INSTALL_STATIC)
    install(TARGETS chipmunk_static ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
  endif(INSTALL_STATIC)
endif(BUILD_STATIC)

if(BUILD_SHARED OR INSTALL_STATIC)
  # FIXME: change to PUBLIC_HEADER to allow building frameworks
  install(FILES ${chipmunk_public_header} DESTINATION include/chipmunk)
  install(FILES ${chipmunk_constraint_header} DESTINATION include/chipmunk/constraints)
endif(BUILD_SHARED OR INSTALL_STATIC)
