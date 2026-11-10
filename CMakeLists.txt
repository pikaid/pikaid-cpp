cmake_minimum_required(VERSION 3.16)

project(
  pikaid
  VERSION 1.0.0
  DESCRIPTION "Official C++ implementation of pikaid"
  HOMEPAGE_URL "https://github.com/pikaid/pikaid-cpp"
  LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(PIKAID_IS_TOP_LEVEL ON)
else()
  set(PIKAID_IS_TOP_LEVEL OFF)
endif()

option(PIKAID_BUILD_TESTS "Build the pikaid test suite" ${PIKAID_IS_TOP_LEVEL})
option(PIKAID_BUILD_EXAMPLES "Build the pikaid examples" ${PIKAID_IS_TOP_LEVEL})
option(PIKAID_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(PIKAID_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(PIKAID_COVERAGE "Build with coverage instrumentation" OFF)

if(PIKAID_SANITIZE)
  if(MSVC)
    message(FATAL_ERROR "PIKAID_SANITIZE is not supported with MSVC")
  endif()
  add_compile_options(
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

if(PIKAID_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping -O0 -g)
    add_link_options(-fprofile-instr-generate -fcoverage-mapping)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
  else()
    message(FATAL_ERROR "PIKAID_COVERAGE requires Clang or GCC")
  endif()
endif()

function(pikaid_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(PIKAID_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(
      ${target}
      PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion)
    if(PIKAID_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

add_library(pikaid src/pikaid.cpp)
add_library(pikaid::pikaid ALIAS pikaid)

target_compile_features(pikaid PUBLIC cxx_std_17)
set_target_properties(
  pikaid
  PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME pikaid
    WINDOWS_EXPORT_ALL_SYMBOLS ON)

target_include_directories(
  pikaid
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

get_target_property(PIKAID_LIBRARY_TYPE pikaid TYPE)
if(PIKAID_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
  target_compile_definitions(
    pikaid
    PRIVATE PIKAID_BUILDING_LIBRARY
    PUBLIC PIKAID_SHARED)
endif()

if(WIN32)
  target_link_libraries(pikaid PRIVATE bcrypt)
endif()

pikaid_enable_warnings(pikaid)

if(PIKAID_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(PIKAID_BUILD_EXAMPLES)
  add_executable(pikaid_example examples/basic.cpp)
  target_link_libraries(pikaid_example PRIVATE pikaid::pikaid)
  set_target_properties(
    pikaid_example
    PROPERTIES
      CXX_STANDARD 17
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF)
  pikaid_enable_warnings(pikaid_example)
  if(PIKAID_BUILD_TESTS)
    add_test(NAME pikaid_example COMMAND pikaid_example)
  endif()
endif()

install(
  TARGETS pikaid
  EXPORT pikaidTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
  FILES LICENSE README.md CHANGELOG.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR})

install(
  EXPORT pikaidTargets
  FILE pikaidTargets.cmake
  NAMESPACE pikaid::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pikaid)

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/pikaidConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/pikaidConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pikaid)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/pikaidConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(
  FILES
    ${CMAKE_CURRENT_BINARY_DIR}/pikaidConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/pikaidConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pikaid)

# ---------------------------------------------------------------------------
# Packaging
#
# `cpack` produces a source archive and binary archives. The ignore patterns
# below are the authoritative exclusion rule for distributable artifacts:
# `.gitignore` alone does not keep a file out of a package, and `/.ai/` holds
# local-only agent artifacts that must never be published even though the
# directory now lives outside version control.
# ---------------------------------------------------------------------------
if(PIKAID_IS_TOP_LEVEL)
  set(CPACK_PACKAGE_NAME "pikaid")
  set(CPACK_PACKAGE_VENDOR "pikaid")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
      "Official C++ implementation of pikaid")
  set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/pikaid/pikaid-cpp")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  set(CPACK_VERBATIM_VARIABLES YES)

  set(CPACK_GENERATOR "TGZ;ZIP")
  set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
  set(CPACK_PACKAGING_INSTALL_PREFIX "/")
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "pikaid-${PROJECT_VERSION}")

  # Patterns are regular expressions matched against absolute paths. Note that
  # `[.]ai` cannot match a file merely named e.g. `pikaid.ai.txt`.
  set(CPACK_SOURCE_IGNORE_FILES
      "/[.]git/"
      "/[.]git$"
      "/[.]ai/"
      "/[.]ai$"
      "/build[^/]*/"
      "/cmake-build-[^/]*/"
      "/[.]cache/"
      "/CMakeFiles/"
      "/CMakeCache[.]txt$"
      "/compile_commands[.]json$"
      "/Testing/"
      "/coverage/"
      "/[.]DS_Store$"
      "/opencode[.]jsonc$"
      "[.]o$"
      "[.]a$"
      "[.]so([.][0-9]+)*$"
      "[.]dylib$"
      "[.]dll$"
      "[.]lib$"
      "[.]exe$"
      "[.]profraw$"
      "[.]profdata$"
      "[.]gcda$"
      "[.]gcno$"
      "[.]gcov$")

  include(CPack)
endif()
