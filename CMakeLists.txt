# fs1rLib          the engine, no Windows, no host, no GUI. This is what the plug-in links.
# fsvr_console     the test console (WinMM, Windows only), built into bin/.
# render_capture   the renderer behind the calibration loop, into bin/.
# With FSVR_BUILD_PLUGIN, the plug-in: Hollow (hollow/) with the FSVR skin (plugin/), every format the
# platform has into bin/<format>. The first configure downloads the CLAP, VST3 and AU SDKs, RtAudio and
# RtMidi (through clap-wrapper) and the audio decoders.
#
# build.bat is the quick MSVC path; this file is for clang, CI, and the plug-in.
#   cmake -B build/cmake -S . && cmake --build build/cmake --config Release && ctest --test-dir build/cmake
#   cmake -B build/plugin -S . -DFSVR_BUILD_PLUGIN=ON && cmake --build build/plugin --config Release
cmake_minimum_required(VERSION 3.24)
# Before project(): C++17, the static C++ runtime, and on macOS universal binaries (arm64 and x86_64, so
# the same build runs on Apple silicon and Intel Macs; issue #3 was an arm64-only build) for 10.15 and up.
include(hollow/cmake/HollowDefaults.cmake)
project(fsvr VERSION 0.8.0 LANGUAGES C CXX)
if(APPLE)
  enable_language(OBJC OBJCXX)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()
# Everything meant to be run lands in bin/: the console here, the plug-in formats under bin/<Format>/.
set(FS1R_BIN ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# The chip, its two hosts and the sections over them. They are bit-exact only without FMA contraction, so they
# are a library of their own with its own flags; excess precision effect_host.h refuses at compile time.
# The effects and the voice filters (src/effects/, src/filters/) are bit-exact for the same reason, so they live here too.
file(GLOB VOP3HOST CONFIGURE_DEPENDS src/fs1r/firmware/vop3host/*.cpp src/effects/*.cpp src/filters/*.cpp)
add_library(vop3host STATIC ${VOP3HOST} src/fs1r/chips/vop3.cpp)
target_include_directories(vop3host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# C++20 for the natives' designated initializers ({.st = st, .sr = 5, .add = ADD_ACC}), which name a
# step's fields instead of counting commas.
set_target_properties(vop3host PROPERTIES CXX_STANDARD 20)
# Every program runs as C++, from src/effects/, so nothing else is compiled or linked here.
foreach(t vop3host)
  if(MSVC)
    target_compile_options(${t} PRIVATE /W3 /fp:precise /permissive-)
  else()
    target_compile_options(${t} PRIVATE -Wall -ffp-contract=off)
  endif()
endforeach()

add_library(fs1rLib STATIC
    src/fs1r/chips/ymp706.cpp
    src/fs1r/firmware/controllers.cpp
    src/fs1r/firmware/fseq.cpp
    src/fs1r/firmware/midi.cpp
    src/fs1r/firmware/notes.cpp
    src/fs1r/firmware/patch.cpp
    src/fs1r/firmware/rom.cpp
    src/fsvr/device.cpp
    src/fsvr/fastmath.cpp
    src/fsvr/selftest.cpp)
target_include_directories(fs1rLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(fs1rLib PUBLIC vop3host)
# The sample loop's maths backend, src/fsvr/fastmath.h: LUT (the default, and the fastest measured), RAW
# (libm, kept for A/B renders), CORDIC, or HYBRID, where the table seeds a short CORDIC rotation.
set(FSVR_MATH "LUT" CACHE STRING "fastmath backend: RAW, LUT, CORDIC or HYBRID")
set_property(CACHE FSVR_MATH PROPERTY STRINGS RAW LUT CORDIC HYBRID)
set(_fsvr_math_names RAW LUT CORDIC HYBRID)
list(FIND _fsvr_math_names "${FSVR_MATH}" _fsvr_math_id)
if(_fsvr_math_id LESS 0)
  message(FATAL_ERROR "FSVR_MATH must be RAW, LUT, CORDIC or HYBRID, not ${FSVR_MATH}")
endif()
target_compile_definitions(fs1rLib PUBLIC FSVR_MATH=${_fsvr_math_id})
if(MSVC)
  target_compile_options(fs1rLib PRIVATE /W3)
else()
  target_compile_options(fs1rLib PRIVATE -Wall -Wno-unused-function)
endif()

# The renderer behind the calibration loop. The console does this too, but only on Windows, and
# tools/analyze_capture.py needs a render of every capture request to compare a recording against.
add_executable(render_capture tools/render_capture.cpp)
target_link_libraries(render_capture PRIVATE fs1rLib)
set_target_properties(render_capture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${FS1R_BIN}>")

enable_testing()

if(WIN32)
  add_executable(fsvr_console src/console/main.cpp)
  target_link_libraries(fsvr_console PRIVATE fs1rLib winmm)
  set_target_properties(fsvr_console PROPERTIES RUNTIME_OUTPUT_DIRECTORY "$<1:${FS1R_BIN}>")
  add_test(NAME engine COMMAND fsvr_console -selftest)
else()
  message(STATUS "fsvr_console needs WinMM; building fs1rLib only")
  add_executable(engine_selftest tools/engine_selftest.cpp)
  target_link_libraries(engine_selftest PRIVATE fs1rLib)
  add_test(NAME engine COMMAND engine_selftest)
endif()

# The programs written in C++ against the interpreter, each on every type that runs it (docs/vop3_native/).
add_executable(vop3_native_check tools/vop3_native_check.cpp)
target_link_libraries(vop3_native_check PRIVATE vop3host)
if(NOT MSVC)
  target_compile_options(vop3_native_check PRIVATE -ffp-contract=off)
endif()
add_test(NAME vop3_native COMMAND vop3_native_check)
# The two chips' cost per sample, and their output's hash (not a test: a number to compare builds by).
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  add_executable(vop3_bench EXCLUDE_FROM_ALL tools/vop3_bench.cpp)
  target_link_libraries(vop3_bench PRIVATE vop3host)
endif()
# The same programs as the engine installs them, all at once, which is the path the plugin runs.
add_test(NAME vop3_installed COMMAND vop3_native_check --installed)

option(FSVR_BUILD_PLUGIN "Build the plug-in (downloads its SDKs on the first configure)" OFF)
if(FSVR_BUILD_PLUGIN)
  add_subdirectory(hollow)
  add_subdirectory(plugin)
endif()
