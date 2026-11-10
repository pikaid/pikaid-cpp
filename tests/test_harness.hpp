// SPDX-License-Identifier: MIT
//
// Tiny self-contained test harness. No external test framework is required.

#ifndef PIKAID_TEST_HARNESS_HPP
#define PIKAID_TEST_HARNESS_HPP

#include <pikaid/pikaid.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace testing {

using case_function = void (*)();

template <typename Value>
std::string describe_value(const Value& value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

/// Render byte arrays as lowercase hexadecimal instead of raw characters.
template <std::size_t Size>
std::string describe_value(const std::array<std::uint8_t, Size>& value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string text = "0x";
    for (std::uint8_t byte : value) {
        text.push_back(digits[(byte >> 4U) & 0x0fU]);
        text.push_back(digits[byte & 0x0fU]);
    }
    return text;
}

struct test_case {
    const char* name;
    case_function function;
};

inline std::vector<test_case>& registry()
{
    static std::vector<test_case> cases;
    return cases;
}

inline bool register_case(const char* name, case_function function)
{
    registry().push_back({name, function});
    return true;
}

inline int checks = 0;
inline int failures = 0;
inline const char* current_case = "";

inline void report_failure(const char* file, int line, const std::string& message)
{
    ++failures;
    std::cerr << file << ":" << line << ": [" << current_case
              << "] " << message << '\n';
}

inline void check(bool condition, const char* expression, const char* file,
                  int line, const std::string& context = std::string())
{
    ++checks;
    if (!condition) {
        report_failure(file, line,
                       (context.empty() ? std::string() : context + ": ") +
                           "CHECK failed: " + expression);
    }
}

template <typename Actual, typename Expected>
void check_equal(const Actual& actual, const Expected& expected,
                 const char* actual_expression, const char* expected_expression,
                 const char* file, int line,
                 const std::string& context = std::string())
{
    ++checks;
    if (!(actual == expected)) {
        std::ostringstream message;
        if (!context.empty()) {
            message << context << ": ";
        }
        message << "CHECK_EQ failed: " << actual_expression << " == "
                << expected_expression << " (actual: "
                << describe_value(actual) << ", expected: "
                << describe_value(expected) << ")";
        report_failure(file, line, message.str());
    }
}

template <typename Function>
void check_throws_code(Function&& function, int expected_code,
                       const char* expression, const char* file, int line,
                       const std::string& context = std::string())
{
    ++checks;
    try {
        std::forward<Function>(function)();
    } catch (const pikaid::error& exception) {
        if (static_cast<int>(exception.code()) != expected_code) {
            std::ostringstream message;
            if (!context.empty()) {
                message << context << ": ";
            }
            message << "expected error code " << expected_code << " from "
                    << expression << ", got " << static_cast<int>(exception.code())
                    << " (" << exception.what() << ")";
            report_failure(file, line, message.str());
        }
        return;
    } catch (const std::exception& exception) {
        std::ostringstream message;
        if (!context.empty()) {
            message << context << ": ";
        }
        message << "wrong exception type from " << expression << ": "
                << exception.what();
        report_failure(file, line, message.str());
        return;
    } catch (...) {
        report_failure(file, line,
                       (context.empty() ? std::string() : context + ": ") +
                           "wrong non-standard exception from " + expression);
        return;
    }

    report_failure(file, line,
                   (context.empty() ? std::string() : context + ": ") +
                       "expected exception from " + expression);
}

template <typename Function>
void check_throws_any(Function&& function, const char* expression,
                      const char* file, int line,
                      const std::string& context = std::string())
{
    ++checks;
    try {
        std::forward<Function>(function)();
    } catch (...) {
        return;
    }
    report_failure(file, line,
                   (context.empty() ? std::string() : context + ": ") +
                       "expected an exception from " + expression);
}

template <typename Function>
void check_nothrow(Function&& function, const char* expression,
                   const char* file, int line,
                   const std::string& context = std::string())
{
    ++checks;
    try {
        std::forward<Function>(function)();
    } catch (const std::exception& exception) {
        report_failure(file, line,
                       (context.empty() ? std::string() : context + ": ") +
                           "unexpected exception from " + expression + ": " +
                           exception.what());
    } catch (...) {
        report_failure(file, line,
                       (context.empty() ? std::string() : context + ": ") +
                           "unexpected non-standard exception from " + expression);
    }
}

} // namespace testing

#define PIKAID_TEST(name)                                                     \
    static void name();                                                       \
    static const bool name##_registered =                                     \
        ::testing::register_case(#name, &name);                               \
    static void name()

#define CHECK(expression)                                                     \
    ::testing::check(static_cast<bool>(expression), #expression, __FILE__,    \
                     __LINE__)

#define CHECK_CTX(expression, context)                                        \
    ::testing::check(static_cast<bool>(expression), #expression, __FILE__,    \
                     __LINE__, (context))

#define CHECK_EQ(actual, expected)                                            \
    ::testing::check_equal((actual), (expected), #actual, #expected, __FILE__, \
                           __LINE__)

#define CHECK_EQ_CTX(actual, expected, context)                               \
    ::testing::check_equal((actual), (expected), #actual, #expected, __FILE__, \
                           __LINE__, (context))

#define CHECK_THROWS_CODE(expression, code)                                   \
    ::testing::check_throws_code([&]() { static_cast<void>(expression); },    \
                                 static_cast<int>(code), #expression,         \
                                 __FILE__, __LINE__)

#define CHECK_THROWS_CODE_CTX(expression, code, context)                      \
    ::testing::check_throws_code([&]() { static_cast<void>(expression); },    \
                                 static_cast<int>(code), #expression,         \
                                 __FILE__, __LINE__, (context))

#define CHECK_THROWS_ANY(expression)                                          \
    ::testing::check_throws_any([&]() { static_cast<void>(expression); },     \
                                #expression, __FILE__, __LINE__)

#define CHECK_NOTHROW(expression)                                             \
    ::testing::check_nothrow([&]() { static_cast<void>(expression); },        \
                             #expression, __FILE__, __LINE__)

#endif // PIKAID_TEST_HARNESS_HPP
