# SPDX-License-Identifier: MIT
#
# Standalone, test-only JSON-Lines adapter for the official PikaID
# conformance suite. It is configured against an installed pikaid package:
#
#   cmake -S tools/conformance -B build-conformance \
#     -DCMAKE_PREFIX_PATH=/path/to/pikaid/prefix
#
# The adapter is deliberately not part of the main build, not installed and
# not packaged. The source package still contains it so the conformance
# evidence can be reproduced from a release archive.

cmake_minimum_required(VERSION 3.16)

project(pikaid_conformance_adapter LANGUAGES CXX)

find_package(pikaid 1.0 CONFIG REQUIRED)

# Reuse the repository's dependency-free, test-only JSON reader instead of
# adding a second JSON implementation. A source-package consumer has the same
# relative layout.
set(
  PIKAID_TEST_JSON_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/../../tests"
  CACHE PATH "Directory containing the test-only json.hpp reader")
if(NOT EXISTS "${PIKAID_TEST_JSON_DIR}/json.hpp")
  message(
    FATAL_ERROR
    "json.hpp was not found in PIKAID_TEST_JSON_DIR=${PIKAID_TEST_JSON_DIR}")
endif()

option(
  PIKAID_CONFORMANCE_WARNINGS_AS_ERRORS
  "Treat compiler warnings in the conformance adapter as errors"
  OFF)

add_executable(pikaid_conformance_adapter adapter.cpp)
target_link_libraries(pikaid_conformance_adapter PRIVATE pikaid::pikaid)
target_include_directories(
  pikaid_conformance_adapter
  PRIVATE "${PIKAID_TEST_JSON_DIR}")
target_compile_features(pikaid_conformance_adapter PRIVATE cxx_std_17)
set_target_properties(
  pikaid_conformance_adapter
  PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

if(MSVC)
  target_compile_options(pikaid_conformance_adapter PRIVATE /W4 /permissive-)
  if(PIKAID_CONFORMANCE_WARNINGS_AS_ERRORS)
    target_compile_options(pikaid_conformance_adapter PRIVATE /WX)
  endif()
else()
  target_compile_options(
    pikaid_conformance_adapter
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion)
  if(PIKAID_CONFORMANCE_WARNINGS_AS_ERRORS)
    target_compile_options(pikaid_conformance_adapter PRIVATE -Werror)
  endif()
endif()
