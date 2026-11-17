# SPDX-License-Identifier: MIT
# Fails if Documentation/parameters.rst differs from what dump_params generates.
execute_process(COMMAND ${DUMP} OUTPUT_VARIABLE generated)
file(READ ${DOC} committed)
if(NOT generated STREQUAL committed)
  message(FATAL_ERROR "Documentation/parameters.rst is stale. Run: cmake --build build --target params-doc")
endif()
