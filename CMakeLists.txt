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
project(fsvr VERSION 0.5.2 LANGUAGES C CXX)
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
file(GLOB VOP3HOST CONFIGURE_DEPENDS src/fs1r/firmware/vop3host/*.cpp)
add_library(vop3host STATIC ${VOP3HOST} src/fs1r/effects.cpp src/fs1r/filter.cpp src/fs1r/chips/vop3.cpp)
target_include_directories(vop3host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
# The chip's programs compiled to native code, with AsmJit (third_party/asmjit). AsmJit needs C++20, so it and
# yss236_jit.cpp, the one file that includes it, are built as C++20; the rest stays C++17.
file(GLOB ASMJIT CONFIGURE_DEPENDS third_party/asmjit/asmjit/core/*.cpp third_party/asmjit/asmjit/x86/*.cpp
     third_party/asmjit/asmjit/arm/*.cpp third_party/asmjit/asmjit/axl/*.cpp)
add_library(asmjit STATIC ${ASMJIT})
target_include_directories(asmjit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asmjit)
target_compile_definitions(asmjit PUBLIC ASMJIT_STATIC ASMJIT_NO_BUILDER ASMJIT_NO_COMPILER ASMJIT_NO_UJIT ASMJIT_NO_FOREIGN ASMJIT_NO_SHM_OPEN)
find_package(Threads REQUIRED)
target_link_libraries(asmjit PUBLIC Threads::Threads)
add_library(vop3jit STATIC src/fs1r/chips/yss236_jit.cpp)
target_include_directories(vop3jit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(vop3jit PRIVATE asmjit)
set_target_properties(asmjit vop3jit PROPERTIES CXX_STANDARD 20)
target_link_libraries(vop3host PUBLIC vop3jit)
foreach(t vop3host vop3jit)
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

option(FSVR_BUILD_PLUGIN "Build the plug-in (downloads its SDKs on the first configure)" OFF)
if(FSVR_BUILD_PLUGIN)
  add_subdirectory(hollow)
  add_subdirectory(plugin)
endif()
