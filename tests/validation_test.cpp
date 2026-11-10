// SPDX-License-Identifier: MIT
//
// Strict validation: every non-canonical representation is rejected, and no
// input is silently normalized or truncated.

#include <pikaid/pikaid.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "test_harness.hpp"

namespace {

constexpr std::size_t length = pikaid::string_length;

std::string all_zero_id()
{
    return std::string(length, '0');
}

std::string id_with(std::size_t position, char character)
{
    std::string id = all_zero_id();
    id[position] = character;
    return id;
}

/// Identifier whose randomness suffix is exactly the given 19 characters.
std::string id_with_randomness(std::string_view suffix)
{
    std::string id(pikaid::timestamp_length, '0');
    id.append(suffix);
    return id;
}

bool is_base36(char character)
{
    return (character >= '0' && character <= '9') ||
           (character >= 'a' && character <= 'z');
}

pikaid::binary binary_with_timestamp(std::uint64_t timestamp)
{
    pikaid::binary bytes{};
    for (std::size_t index = 5; index-- > 0;) {
        bytes[index] = static_cast<std::uint8_t>(timestamp & 0xffU);
        timestamp >>= 8U;
    }
    return bytes;
}

} // namespace

PIKAID_TEST(validation_rejects_every_wrong_length)
{
    const std::vector<std::size_t> lengths = {0, 1, 25, 27, 28, 100};
    for (std::size_t size : lengths) {
        const std::string id(size, '0');
        CHECK_CTX(!pikaid::is_well_formed(id),
                  "length " + std::to_string(size));
        CHECK_CTX(!pikaid::is_valid(id), "length " + std::to_string(size));
        CHECK_THROWS_CODE_CTX(pikaid::parse(id),
                              pikaid::error_code::invalid_string_length,
                              "length " + std::to_string(size));
        CHECK_THROWS_CODE_CTX(pikaid::to_binary(id),
                              pikaid::error_code::invalid_string_length,
                              "length " + std::to_string(size));
        CHECK_THROWS_CODE_CTX(pikaid::from_binary(id),
                              pikaid::error_code::invalid_binary_length,
                              "length " + std::to_string(size));
    }
}

PIKAID_TEST(validation_rejects_non_base36_ascii_at_every_position)
{
    const std::vector<std::size_t> positions = {0, 6, 7, 12, 25};
    for (int code = 0; code < 128; ++code) {
        const char character = static_cast<char>(code);
        if (is_base36(character)) {
            continue;
        }
        for (std::size_t position : positions) {
            const std::string id = id_with(position, character);
            const std::string context = "code " + std::to_string(code) +
                                        " at " + std::to_string(position);
            CHECK_CTX(!pikaid::is_well_formed(id), context);
            CHECK_CTX(!pikaid::is_valid(id), context);
            CHECK_THROWS_CODE_CTX(pikaid::parse(id),
                                  pikaid::error_code::invalid_character,
                                  context);
        }
    }
}

PIKAID_TEST(validation_rejects_unicode_and_high_bytes)
{
    for (int code = 0x80; code <= 0xff; ++code) {
        const std::string id =
            id_with(4, static_cast<char>(static_cast<unsigned char>(code)));
        const std::string context = "code " + std::to_string(code);
        CHECK_CTX(!pikaid::is_well_formed(id), context);
        CHECK_CTX(!pikaid::is_valid(id), context);
        CHECK_THROWS_CODE_CTX(pikaid::parse(id),
                              pikaid::error_code::invalid_character, context);
    }

    // A valid two-byte UTF-8 sequence still fails: pikaid is ASCII-only.
    const std::string utf8 = id_with(0, static_cast<char>(0xc3));
    CHECK(!pikaid::is_valid(utf8));
    CHECK_THROWS_CODE(pikaid::parse(utf8), pikaid::error_code::invalid_character);
}

PIKAID_TEST(validation_rejects_uppercase_everywhere)
{
    for (std::size_t position = 0; position < length; ++position) {
        for (char letter = 'A'; letter <= 'Z'; ++letter) {
            const std::string id = id_with(position, letter);
            const std::string context =
                std::string(1, letter) + " at " + std::to_string(position);
            CHECK_CTX(!pikaid::is_well_formed(id), context);
            CHECK_CTX(!pikaid::is_valid(id), context);
            CHECK_THROWS_CODE_CTX(pikaid::parse(id),
                                  pikaid::error_code::invalid_character,
                                  context);
        }
    }
}

PIKAID_TEST(validation_rejects_embedded_nul)
{
    std::string id = all_zero_id();
    id[5] = '\0';
    CHECK_EQ(id.size(), length);
    CHECK(!pikaid::is_well_formed(id));
    CHECK(!pikaid::is_valid(id));
    CHECK_THROWS_CODE(pikaid::parse(id), pikaid::error_code::invalid_character);
}

PIKAID_TEST(validation_enforces_96_bit_randomness_boundary)
{
    const std::string max_suffix = "7oiylpimjg5u2ca1ypr"; // 2^96 - 1
    const std::string over_suffix = "7oiylpimjg5u2ca1yps"; // 2^96
    const std::string all_z = std::string(pikaid::randomness_length, 'z');
    const std::string high_bit = "3u9haurb9q2x1650zcw"; // 2^95

    const std::string max_id = id_with_randomness(max_suffix);
    CHECK(pikaid::is_well_formed(max_id));
    CHECK(pikaid::is_valid(max_id));
    CHECK_EQ(pikaid::randomness_hex(max_id),
             std::string("ffffffffffffffffffffffff"));

    const std::string over_id = id_with_randomness(over_suffix);
    CHECK(pikaid::is_well_formed(over_id));
    CHECK(!pikaid::is_valid(over_id));
    CHECK_THROWS_CODE(pikaid::parse(over_id),
                      pikaid::error_code::randomness_overflow);
    CHECK_THROWS_CODE(pikaid::randomness(over_id),
                      pikaid::error_code::randomness_overflow);
    CHECK_THROWS_CODE(pikaid::timestamp(over_id),
                      pikaid::error_code::randomness_overflow);

    const std::string all_z_id = id_with_randomness(all_z);
    CHECK(pikaid::is_well_formed(all_z_id));
    CHECK(!pikaid::is_valid(all_z_id));
    CHECK_THROWS_CODE(pikaid::parse(all_z_id),
                      pikaid::error_code::randomness_overflow);

    // 36^19 - 1 is the largest 19-digit Base36 value, but it exceeds 2^96 - 1.
    CHECK(all_z > max_suffix);

    const std::string high_id = id_with_randomness(high_bit);
    CHECK(pikaid::is_valid(high_id));
    CHECK_EQ(pikaid::randomness_hex(high_id),
             std::string("800000000000000000000000"));
}

PIKAID_TEST(validation_distinguishes_shape_from_semantics)
{
    const std::string overflowing = id_with_randomness("zzzzzzzzzzzzzzzzzzz");
    CHECK(pikaid::is_well_formed(overflowing));
    CHECK(!pikaid::is_valid(overflowing));

    const std::string uppercase = id_with(0, 'A');
    CHECK(!pikaid::is_well_formed(uppercase));
    CHECK(!pikaid::is_valid(uppercase));

    CHECK(pikaid::is_well_formed(all_zero_id()));
    CHECK(pikaid::is_valid(all_zero_id()));
}

PIKAID_TEST(validation_timestamp_range_for_generation)
{
    CHECK_NOTHROW(pikaid::validate_timestamp(0));
    CHECK_NOTHROW(pikaid::validate_timestamp(1));
    CHECK_NOTHROW(pikaid::validate_timestamp(1700000000));
    CHECK_NOTHROW(pikaid::validate_timestamp(pikaid::max_timestamp));
    CHECK_THROWS_CODE(pikaid::validate_timestamp(-1),
                      pikaid::error_code::timestamp_negative);
    CHECK_THROWS_CODE(pikaid::validate_timestamp(-1700000000),
                      pikaid::error_code::timestamp_negative);
    CHECK_THROWS_CODE(pikaid::validate_timestamp(
                          std::int64_t{0} - 9223372036854775807LL - 1),
                      pikaid::error_code::timestamp_negative);
    CHECK_THROWS_CODE(pikaid::validate_timestamp(pikaid::max_timestamp + 1),
                      pikaid::error_code::timestamp_overflow);
    CHECK_THROWS_CODE(pikaid::validate_timestamp(9223372036854775807LL),
                      pikaid::error_code::timestamp_overflow);

    CHECK_THROWS_CODE(pikaid::generate(-1),
                      pikaid::error_code::timestamp_negative);
    CHECK_THROWS_CODE(pikaid::generate(pikaid::max_timestamp + 1),
                      pikaid::error_code::timestamp_overflow);
    CHECK_THROWS_CODE(pikaid::generate_binary(-1),
                      pikaid::error_code::timestamp_negative);
    CHECK_THROWS_CODE(pikaid::generate_binary(pikaid::max_timestamp + 1),
                      pikaid::error_code::timestamp_overflow);
}

PIKAID_TEST(validation_rejects_bad_binary_lengths)
{
    for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{16},
                             std::size_t{18}, std::size_t{34}}) {
        const std::string bytes(size, '\x7f');
        CHECK_THROWS_CODE_CTX(pikaid::from_binary(bytes),
                              pikaid::error_code::invalid_binary_length,
                              "size " + std::to_string(size));
    }
    CHECK_THROWS_CODE(pikaid::from_binary(std::string_view()),
                      pikaid::error_code::invalid_binary_length);
}

PIKAID_TEST(validation_rejects_binary_timestamp_overflow)
{
    const pikaid::binary just_over = binary_with_timestamp(78364164096ULL);
    CHECK_THROWS_CODE(pikaid::from_binary(just_over),
                      pikaid::error_code::timestamp_overflow);

    pikaid::binary forty_bit_max{};
    for (std::size_t index = 0; index < 5; ++index) {
        forty_bit_max[index] = 0xffU;
    }
    CHECK_THROWS_CODE(pikaid::from_binary(forty_bit_max),
                      pikaid::error_code::timestamp_overflow);
    CHECK_THROWS_CODE(pikaid::compare_binary(forty_bit_max, pikaid::binary{}),
                      pikaid::error_code::timestamp_overflow);

    // Maximum representable timestamp is accepted, with any 12-byte suffix.
    const pikaid::binary maximum = binary_with_timestamp(78364164095ULL);
    CHECK_NOTHROW(pikaid::from_binary(maximum));
    CHECK_EQ(pikaid::from_binary(maximum), std::string("zzzzzzz") +
                                               std::string(19, '0'));

    const pikaid::binary zero = binary_with_timestamp(0ULL);
    CHECK_EQ(pikaid::from_binary(zero), all_zero_id());
}

PIKAID_TEST(validation_rejects_empty_randomness_sources)
{
    const pikaid::randomness_source empty;
    CHECK_THROWS_CODE(pikaid::generate(1700000000, empty),
                      pikaid::error_code::randomness_failure);
    CHECK_THROWS_CODE(pikaid::generate_binary(1700000000, empty),
                      pikaid::error_code::randomness_failure);
}

PIKAID_TEST(validation_error_type_and_codes_are_stable)
{
    CHECK_EQ(static_cast<int>(pikaid::error_code::invalid_string_length), 1);
    CHECK_EQ(static_cast<int>(pikaid::error_code::invalid_character), 2);
    CHECK_EQ(static_cast<int>(pikaid::error_code::timestamp_overflow), 3);
    CHECK_EQ(static_cast<int>(pikaid::error_code::timestamp_negative), 4);
    CHECK_EQ(static_cast<int>(pikaid::error_code::randomness_overflow), 5);
    CHECK_EQ(static_cast<int>(pikaid::error_code::invalid_binary_length), 6);
    CHECK_EQ(static_cast<int>(pikaid::error_code::randomness_failure), 7);

    try {
        static_cast<void>(pikaid::parse("short"));
        CHECK(false);
    } catch (const pikaid::error& exception) {
        CHECK_EQ(static_cast<int>(exception.code()),
                 static_cast<int>(pikaid::error_code::invalid_string_length));
        CHECK(std::string(exception.what()).find("26") != std::string::npos);
        CHECK(!std::string(exception.what()).empty());
        const std::invalid_argument& base = exception;
        CHECK(!std::string(base.what()).empty());
    } catch (...) {
        CHECK(false);
    }

    // The exception is catchable as the standard invalid-argument type.
    bool caught_as_invalid_argument = false;
    try {
        static_cast<void>(pikaid::parse("UPPERCASE0000000000000000000"));
    } catch (const std::invalid_argument&) {
        caught_as_invalid_argument = true;
    }
    CHECK(caught_as_invalid_argument);

    bool caught_as_exception = false;
    try {
        static_cast<void>(pikaid::to_binary(id_with_randomness("zzzzzzzzzzzzzzzzzzz")));
    } catch (const std::exception& exception) {
        caught_as_exception = true;
        CHECK(std::string(exception.what()).find("96") != std::string::npos);
    }
    CHECK(caught_as_exception);
}

PIKAID_TEST(validation_compare_rejects_invalid_operands)
{
    const std::string valid = all_zero_id();
    CHECK_THROWS_CODE(pikaid::compare("short", valid),
                      pikaid::error_code::invalid_string_length);
    CHECK_THROWS_CODE(pikaid::compare(valid, "short"),
                      pikaid::error_code::invalid_string_length);
    CHECK_THROWS_CODE(pikaid::compare(id_with(0, 'A'), valid),
                      pikaid::error_code::invalid_character);
    CHECK_THROWS_CODE(
        pikaid::compare(valid, id_with_randomness("zzzzzzzzzzzzzzzzzzz")),
        pikaid::error_code::randomness_overflow);
    CHECK_THROWS_CODE(pikaid::randomness_hex(id_with(0, '!')),
                      pikaid::error_code::invalid_character);
}

PIKAID_TEST(validation_never_normalizes_or_truncates)
{
    const std::string original = "0000000000000000000000000A";
    const std::string copy = original;
    CHECK(!pikaid::is_valid(copy));
    CHECK_EQ(copy, original);
    CHECK_EQ(original.size(), static_cast<std::size_t>(26));

    // A 26-character prefix of a longer valid identifier is rejected; no
    // truncation happens anywhere in the API.
    const std::string padded = all_zero_id() + "trailing";
    CHECK(!pikaid::is_valid(padded));
    CHECK_THROWS_CODE(pikaid::parse(padded),
                      pikaid::error_code::invalid_string_length);

    // Leading whitespace is not trimmed.
    const std::string lead = " " + std::string(25, '0');
    CHECK(!pikaid::is_valid(lead));
    CHECK_THROWS_CODE(pikaid::parse(lead), pikaid::error_code::invalid_character);

    // Trailing whitespace is not trimmed either.
    const std::string trail = std::string(25, '0') + " ";
    CHECK(!pikaid::is_valid(trail));
    CHECK_THROWS_CODE(pikaid::parse(trail),
                      pikaid::error_code::invalid_character);
}

PIKAID_TEST(validation_rejects_crlf_and_zero_width_unicode)
{
    // CR/LF inside a 26-byte value is an invalid character, never a separator
    // or line ending that gets stripped.
    const std::string embedded_crlf = std::string(24, '0') + "\r\n";
    CHECK_EQ(embedded_crlf.size(), length);
    CHECK(!pikaid::is_well_formed(embedded_crlf));
    CHECK(!pikaid::is_valid(embedded_crlf));
    CHECK_THROWS_CODE(pikaid::parse(embedded_crlf),
                      pikaid::error_code::invalid_character);

    // A trailing CRLF makes the value 27 bytes; the length check must reject
    // it before any character is examined.
    const std::string trailing_crlf = std::string(25, '0') + "\r\n";
    CHECK_EQ(trailing_crlf.size(), length + 1);
    CHECK(!pikaid::is_well_formed(trailing_crlf));
    CHECK(!pikaid::is_valid(trailing_crlf));
    CHECK_THROWS_CODE(pikaid::parse(trailing_crlf),
                      pikaid::error_code::invalid_string_length);

    // Zero-width space (U+200B, three UTF-8 bytes) is not Base36.
    const std::string zero_width_space = std::string(23, '0') + "\xe2\x80\x8b";
    CHECK_EQ(zero_width_space.size(), length);
    CHECK(!pikaid::is_well_formed(zero_width_space));
    CHECK(!pikaid::is_valid(zero_width_space));
    CHECK_THROWS_CODE(pikaid::parse(zero_width_space),
                      pikaid::error_code::invalid_character);

    // A no-breaking space (U+00A0) and a fullwidth digit zero (U+FF10) must
    // not be trimmed or case/width-folded into ASCII.
    const std::string no_break_space = std::string(24, '0') + "\xc2\xa0";
    CHECK_EQ(no_break_space.size(), length);
    CHECK(!pikaid::is_valid(no_break_space));
    CHECK_THROWS_CODE(pikaid::parse(no_break_space),
                      pikaid::error_code::invalid_character);

    const std::string fullwidth_zero = std::string(23, '0') + "\xef\xbc\x90";
    CHECK_EQ(fullwidth_zero.size(), length);
    CHECK(!pikaid::is_valid(fullwidth_zero));
    CHECK_THROWS_CODE(pikaid::parse(fullwidth_zero),
                      pikaid::error_code::invalid_character);

    // A multibyte sequence cut by the 26-byte boundary is still an invalid
    // character, never a partial match or truncation.
    const std::string truncated_utf8 = std::string(24, '0') + "\xe2\x80";
    CHECK_EQ(truncated_utf8.size(), length);
    CHECK(!pikaid::is_well_formed(truncated_utf8));
    CHECK(!pikaid::is_valid(truncated_utf8));
    CHECK_THROWS_CODE(pikaid::parse(truncated_utf8),
                      pikaid::error_code::invalid_character);
}
