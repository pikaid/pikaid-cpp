// SPDX-License-Identifier: MIT
//
// Shared helpers for the pikaid test suite.

#ifndef PIKAID_TEST_SUPPORT_HPP
#define PIKAID_TEST_SUPPORT_HPP

#include <pikaid/pikaid.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "json.hpp"

namespace testing_support {

inline std::uint8_t hex_value(char character)
{
    if (character >= '0' && character <= '9') {
        return static_cast<std::uint8_t>(character - '0');
    }
    if (character >= 'a' && character <= 'f') {
        return static_cast<std::uint8_t>(character - 'a' + 10);
    }
    if (character >= 'A' && character <= 'F') {
        return static_cast<std::uint8_t>(character - 'A' + 10);
    }
    throw std::runtime_error(std::string("invalid hex character: ") + character);
}

inline std::vector<std::uint8_t> hex_to_bytes(std::string_view text)
{
    if (text.size() % 2 != 0) {
        throw std::runtime_error("hex string has an odd number of characters");
    }
    std::vector<std::uint8_t> bytes(text.size() / 2);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const std::uint8_t high = hex_value(text[index * 2]);
        const std::uint8_t low = hex_value(text[index * 2 + 1]);
        bytes[index] = static_cast<std::uint8_t>((high << 4U) | low);
    }
    return bytes;
}

inline std::string bytes_to_hex(const std::uint8_t* data, std::size_t size)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string text(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        text[index * 2] = digits[(data[index] >> 4U) & 0x0fU];
        text[index * 2 + 1] = digits[data[index] & 0x0fU];
    }
    return text;
}

inline std::string bytes_to_hex(const pikaid::binary& bytes)
{
    return bytes_to_hex(bytes.data(), bytes.size());
}

inline std::string bytes_to_hex(const pikaid::randomness_bytes& bytes)
{
    return bytes_to_hex(bytes.data(), bytes.size());
}

inline pikaid::binary binary_from_hex(std::string_view text)
{
    const std::vector<std::uint8_t> bytes = hex_to_bytes(text);
    if (bytes.size() != pikaid::binary_length) {
        throw std::runtime_error("binary fixture is not 17 bytes");
    }
    pikaid::binary result{};
    for (std::size_t index = 0; index < pikaid::binary_length; ++index) {
        result[index] = bytes[index];
    }
    return result;
}

inline pikaid::randomness_bytes randomness_from_hex(std::string_view text)
{
    const std::vector<std::uint8_t> bytes = hex_to_bytes(text);
    if (bytes.size() != pikaid::randomness_byte_length) {
        throw std::runtime_error("randomness fixture is not 12 bytes");
    }
    pikaid::randomness_bytes result{};
    for (std::size_t index = 0; index < pikaid::randomness_byte_length; ++index) {
        result[index] = bytes[index];
    }
    return result;
}

/// Exact bytes of the official pikaid-php v1.2.0 vector fixture.
///
/// The fixture is read at run time and its bytes are checked against a SHA-256
/// digest in the test suite, so an accidental edit cannot silently weaken
/// conformance.
inline const std::string& raw_vectors()
{
    static const std::string text = [] {
        std::ifstream stream(PIKAID_TEST_VECTOR_FILE, std::ios::binary);
        if (!stream) {
            throw std::runtime_error(std::string("cannot open vector fixture: ") +
                                     PIKAID_TEST_VECTOR_FILE);
        }
        return std::string((std::istreambuf_iterator<char>(stream)),
                           std::istreambuf_iterator<char>());
    }();
    return text;
}

inline const test_json::value& vectors()
{
    static const test_json::value document = test_json::parse(raw_vectors());
    return document;
}

} // namespace testing_support

#endif // PIKAID_TEST_SUPPORT_HPP
