// SPDX-License-Identifier: MIT
//
// Test runner for the pikaid test suite.

#include <exception>
#include <iostream>

#include "test_harness.hpp"

int main()
{
    int failures = 0;
    for (const testing::test_case& entry : testing::registry()) {
        testing::current_case = entry.name;
        const int before = testing::failures;
        try {
            entry.function();
        } catch (const std::exception& exception) {
            std::cerr << entry.name << ": uncaught exception: "
                      << exception.what() << '\n';
            ++testing::failures;
        } catch (...) {
            std::cerr << entry.name << ": uncaught non-standard exception\n";
            ++testing::failures;
        }
        const int case_failures = testing::failures - before;
        if (case_failures != 0) {
            ++failures;
            std::cout << "FAIL " << entry.name << " (" << case_failures
                      << " failing checks)\n";
        }
    }

    std::cout << "\n" << testing::registry().size() << " test cases, "
              << testing::checks << " checks, " << testing::failures
              << " failing checks\n";
    return failures == 0 ? 0 : 1;
}
