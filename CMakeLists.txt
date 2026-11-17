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
  find_package(Threads REQUIRED)
  target_link_libraries(lstrip-dsp-tests PRIVATE Threads::Threads)
  target_link_libraries(lstrip-dsp-tests PRIVATE lstrip_dsp)
  target_compile_options(lstrip-dsp-tests PRIVATE -Wall -Wextra -O2)
  add_test(NAME dsp COMMAND lstrip-dsp-tests)
endif()

# Parameter reference generated from the parameter table (and checked for drift).
add_executable(dump_params tools/dump_params.cpp)
target_compile_features(dump_params PRIVATE cxx_std_17)
add_custom_target(params-doc
  COMMAND dump_params > ${CMAKE_CURRENT_SOURCE_DIR}/Documentation/parameters.rst
  DEPENDS dump_params VERBATIM)
if(LSTRIP_BUILD_TESTS)
  add_test(NAME params-doc
    COMMAND ${CMAKE_COMMAND} -DDUMP=$<TARGET_FILE:dump_params>
            -DDOC=${CMAKE_CURRENT_SOURCE_DIR}/Documentation/parameters.rst
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_params_doc.cmake)
endif()

# ---------------------------------------------------------------- Plugin (DPF)
if(LSTRIP_BUILD_PLUGINS)
  if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/dpf/CMakeLists.txt)
    message(FATAL_ERROR "vendor/dpf is missing. Run: git submodule update --init --recursive")
  endif()
  add_subdirectory(vendor/dpf)

  dpf_add_plugin(L-STRIP
    TARGETS vst2 vst3 lv2 clap jack
    UI_TYPE cairo
    USE_FILE_BROWSER
    MONOLITHIC   # LV2: UI inside the DSP object, required by DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
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
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)   # editor text: Inter via FreeType + Cairo
  target_link_libraries(L-STRIP PUBLIC Threads::Threads PkgConfig::FREETYPE)
  # Mono variant (1 in / 1 out) for hosts that feed single-channel sources, e.g. OBS microphones.
  dpf_add_plugin(L-STRIP-Mono
    TARGETS vst2 vst3 lv2 clap
    UI_TYPE cairo
    USE_FILE_BROWSER
    MONOLITHIC   # LV2: UI inside the DSP object, required by DISTRHO_PLUGIN_WANT_DIRECT_ACCESS
    FILES_DSP
      src/plugin/LStripPlugin.cpp
    FILES_UI
      src/ui/LStripUI.cpp
      src/ui/UiBase.cpp
      src/ui/FontData.cpp)
  target_include_directories(L-STRIP-Mono PUBLIC src/plugin-mono src)
  target_compile_options(L-STRIP-Mono PUBLIC -O2 -fno-math-errno)
  target_link_libraries(L-STRIP-Mono PUBLIC lstrip_dsp Threads::Threads PkgConfig::FREETYPE)
  target_compile_definitions(L-STRIP-Mono PUBLIC LSTRIP_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts")
endif()

# VST2 glue test: loads the built plugin like a host would.
if(LSTRIP_BUILD_PLUGINS AND LSTRIP_BUILD_TESTS)
  add_executable(lstrip-vst2-host-test tests/vst2_host_test.cpp)
  target_link_libraries(lstrip-vst2-host-test PRIVATE lstrip_dsp ${CMAKE_DL_LIBS})
  add_dependencies(lstrip-vst2-host-test L-STRIP-vst2)
  add_test(NAME vst2-host COMMAND lstrip-vst2-host-test $<TARGET_FILE:L-STRIP-vst2>)
  add_dependencies(lstrip-vst2-host-test L-STRIP-Mono-vst2)
  add_test(NAME vst2-host-mono COMMAND lstrip-vst2-host-test $<TARGET_FILE:L-STRIP-Mono-vst2> mono)
endif()

# ---------------------------------------------------------------- Install (packagers)
if(LSTRIP_BUILD_PLUGINS)
  include(GNUInstallDirs)
  set(CMAKE_INSTALL_DOCDIR ${CMAKE_INSTALL_DATADIR}/doc/l-strip)
  set(_bin ${CMAKE_BINARY_DIR}/bin)
  # OBS lists VST2 plugins by file name, so install them as L-STRIP.so / L-STRIP-Mono.so.
  install(FILES $<TARGET_FILE:L-STRIP-vst2> DESTINATION ${CMAKE_INSTALL_LIBDIR}/vst RENAME L-STRIP.so)
  install(FILES $<TARGET_FILE:L-STRIP-Mono-vst2> DESTINATION ${CMAKE_INSTALL_LIBDIR}/vst RENAME L-STRIP-Mono.so)
  install(DIRECTORY ${_bin}/L-STRIP.vst3 ${_bin}/L-STRIP-Mono.vst3 DESTINATION ${CMAKE_INSTALL_LIBDIR}/vst3)
  install(DIRECTORY ${_bin}/L-STRIP.lv2 ${_bin}/L-STRIP-Mono.lv2 DESTINATION ${CMAKE_INSTALL_LIBDIR}/lv2)
  install(FILES ${_bin}/L-STRIP.clap ${_bin}/L-STRIP-Mono.clap DESTINATION ${CMAKE_INSTALL_LIBDIR}/clap)
  install(PROGRAMS ${_bin}/L-STRIP DESTINATION ${CMAKE_INSTALL_BINDIR})
  # The Inter font is embedded in the binaries: its licence must accompany them.
  install(FILES COPYING LICENSE THIRD-PARTY.rst README.rst assets/fonts/OFL-Inter.txt
          DESTINATION ${CMAKE_INSTALL_DOCDIR})
  install(DIRECTORY Documentation/ DESTINATION ${CMAKE_INSTALL_DOCDIR}/Documentation FILES_MATCHING PATTERN "*.rst")
endif()
