add_executable(
  pikaid_tests
  main.cpp
  conformance_test.cpp
  conversion_test.cpp
  generation_test.cpp
  validation_test.cpp)

target_link_libraries(pikaid_tests PRIVATE pikaid::pikaid)
target_include_directories(pikaid_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(pikaid_tests PRIVATE cxx_std_17)
set_target_properties(
  pikaid_tests
  PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

target_compile_definitions(
  pikaid_tests
  PRIVATE
    PIKAID_TEST_VECTOR_FILE="${CMAKE_CURRENT_SOURCE_DIR}/vectors/pikaid-v1.json")

# The library enables strict warnings; apply the same policy to the tests so
# that the suite itself cannot accumulate warnings.
if(NOT MSVC)
  target_compile_options(
    pikaid_tests
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion)
else()
  target_compile_options(pikaid_tests PRIVATE /W4 /permissive-)
endif()
if(PIKAID_WARNINGS_AS_ERRORS)
  if(MSVC)
    target_compile_options(pikaid_tests PRIVATE /WX)
  else()
    target_compile_options(pikaid_tests PRIVATE -Werror)
  endif()
endif()

add_test(NAME pikaid_tests COMMAND pikaid_tests)
