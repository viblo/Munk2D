cmake_minimum_required(VERSION 3.10)

project(chipmunk)

# Munk2D 2.2.0
set(MUNK2D_VERSION_MAJOR 2)
set(MUNK2D_VERSION_MINOR 3)
set(MUNK2D_VERSION_PATCH 0)
set(MUNK2D_VERSION "${MUNK2D_VERSION_MAJOR}.${MUNK2D_VERSION_MINOR}.${MUNK2D_VERSION_PATCH}")

# to change the prefix, run cmake with the parameter:
#   -D CMAKE_INSTALL_PREFIX=/my/prefix

# to change the build type, run cmake with the parameter:
#   -D CMAKE_BUILD_TYPE=<build-type>
# run "cmake --help-variable CMAKE_BUILD_TYPE" for details
if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

# to manually select install locations of libraries and executables
#   -D LIB_INSTALL_DIR mylib
#   -D BIN_INSTALL_DIR newbin
set(LIB_INSTALL_DIR lib CACHE STRING "Install location of libraries")
set(BIN_INSTALL_DIR bin CACHE STRING "Install location of executables")

# other options for the build, you can i.e. activate the shared library by passing
#   -D BUILD_SHARED=ON
# to cmake. Other options analog
if(ANDROID)
  option(BUILD_DEMOS "Build the demo applications" OFF)
  option(INSTALL_DEMOS "Install the demo applications" OFF)
  option(BUILD_SHARED "Build and install the shared library" ON)
  option(BUILD_STATIC "Build as static library" ON)
  option(INSTALL_STATIC "Install the static library" OFF)
else()
  option(BUILD_DEMOS "Build the demo applications" ON)
  option(INSTALL_DEMOS "Install the demo applications" OFF)
  option(BUILD_SHARED "Build and install the shared library" ON)
  option(BUILD_STATIC "Build as static library" ON)
  option(INSTALL_STATIC "Install the static library" ON)
endif()

include(CTest)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  option(FORCE_CLANG_BLOCKS "Force enable Clang blocks" YES)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-nan-infinity-disabled")
endif()

# sanity checks...
if(INSTALL_DEMOS)
  set(BUILD_DEMOS ON FORCE)
endif()

# these need the static lib too
if(BUILD_DEMOS OR INSTALL_STATIC)
  set(BUILD_STATIC ON FORCE)
endif()

if(BUILD_TESTING)
  set(BUILD_STATIC ON FORCE)
endif()

if(NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99") # always use gnu99
  if(FORCE_CLANG_BLOCKS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fblocks")
  endif()
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall") # extend debug-profile with -Wall
endif()

option(MUNK2D_ENABLE_LTO "Enable supported interprocedural/link-time optimization in non-Debug builds" ON)
if(MUNK2D_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT MUNK2D_IPO_SUPPORTED OUTPUT MUNK2D_IPO_ERROR LANGUAGES C CXX)
  if(MUNK2D_IPO_SUPPORTED)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
  else()
    message(WARNING "MUNK2D_ENABLE_LTO requested, but IPO/LTO is unavailable: ${MUNK2D_IPO_ERROR}")
  endif()
endif()

option(MUNK2D_ENABLE_AVX2_CONTACT_SOLVER "Build the optional runtime-guarded x86-64 AVX2 contact solver" ON)
# SIMD changes the Wasm module loading requirement, so it must be explicit.
option(MUNK2D_ENABLE_WASM_SIMD_CONTACT_SOLVER "Build a SIMD-requiring Emscripten module with the optional contact solver" OFF)

add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

option(BUILD_BENCHMARKS "Build standalone benchmark executables" OFF)

if(BUILD_BENCHMARKS)
  add_subdirectory(benchmarks)
endif()

if(BUILD_DEMOS)
  add_subdirectory(demo)
endif()

option(BUILD_DOCS "Build documentation" OFF)

if(BUILD_DOCS)
  add_subdirectory(docs)
endif()
