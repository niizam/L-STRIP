# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.15)
project(L-STRIP VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(LSTRIP_BUILD_PLUGINS "Build the VST2/VST3/LV2/CLAP/JACK plugin targets (needs vendor/dpf)" ON)
option(LSTRIP_BUILD_TESTS   "Build the headless DSP tests" ON)

# ---------------------------------------------------------------- DSP (header-only)
add_library(lstrip_dsp INTERFACE)
target_include_directories(lstrip_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_features(lstrip_dsp INTERFACE cxx_std_17)

if(LSTRIP_BUILD_TESTS)
  enable_testing()
  add_executable(lstrip-dsp-tests tests/dsp_tests.cpp)
  target_link_libraries(lstrip-dsp-tests PRIVATE lstrip_dsp)
  target_compile_options(lstrip-dsp-tests PRIVATE -Wall -Wextra -O2)
  add_test(NAME dsp COMMAND lstrip-dsp-tests)
endif()

# ---------------------------------------------------------------- Plugin (DPF)
if(LSTRIP_BUILD_PLUGINS)
  if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dpf/CMakeLists.txt)
    message(FATAL_ERROR "vendor/dpf is missing. Run: git submodule update --init --recursive")
  endif()
  add_subdirectory(vendor/dpf)

  dpf_add_plugin(L-STRIP
    TARGETS vst2 vst3 lv2 clap jack
    UI_TYPE opengl
    USE_FILE_BROWSER
    FILES_DSP
      src/plugin/LStripPlugin.cpp
    FILES_UI
      src/ui/LStripUI.cpp
      src/ui/UiBase.cpp
      src/ui/FontData.cpp)

  target_include_directories(L-STRIP PUBLIC src/plugin src)
  target_compile_options(L-STRIP PUBLIC -O2 -fno-math-errno)
  target_link_libraries(L-STRIP PUBLIC lstrip_dsp)
  target_compile_definitions(L-STRIP PUBLIC LSTRIP_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts")
  find_package(Threads REQUIRED)
  target_link_libraries(L-STRIP PUBLIC Threads::Threads)
endif()

# VST2 glue test: loads the built plugin like a host would.
if(LSTRIP_BUILD_PLUGINS AND LSTRIP_BUILD_TESTS)
  add_executable(lstrip-vst2-host-test tests/vst2_host_test.cpp)
  target_link_libraries(lstrip-vst2-host-test PRIVATE lstrip_dsp ${CMAKE_DL_LIBS})
  add_dependencies(lstrip-vst2-host-test L-STRIP-vst2)
  add_test(NAME vst2-host COMMAND lstrip-vst2-host-test $<TARGET_FILE:L-STRIP-vst2>)
endif()
