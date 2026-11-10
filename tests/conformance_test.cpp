// SPDX-License-Identifier: MIT
//
// Conformance against the official pikaid-php v1.2.0 vector fixture and its
// JSON schema. These cases are deliberately driven by the external fixture so
// that the oracles do not come from the implementation under test.

#include <pikaid/pikaid.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "support.hpp"
#include "sha256.hpp"
#include "test_harness.hpp"

using testing_support::binary_from_hex;
using testing_support::bytes_to_hex;
using testing_support::randomness_from_hex;
using testing_support::vectors;

namespace {

/// SHA-256 of pikaid-php v1.2.0 tests/vectors/pikaid-v1.json.
constexpr std::string_view fixture_sha256 =
    "b0c9dfdd063613424414fef5c9065fbd2f81d645d0ab42b34e27cde715613500";

std::string timestamp_hex(std::uint64_t value)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string text(10, '0');
    for (std::size_t index = 5; index-- > 0;) {
        text[index * 2] = digits[(value >> 4U) & 0x0fU];
        text[index * 2 + 1] = digits[value & 0x0fU];
        value >>= 8U;
    }
    return text;
}

int expected_string_error(std::string_view name)
{
    if (name == "entropy-overflow") {
        return static_cast<int>(pikaid::error_code::randomness_overflow);
    }
    if (name == "too-short" || name == "too-long") {
        return static_cast<int>(pikaid::error_code::invalid_string_length);
    }
    return static_cast<int>(pikaid::error_code::invalid_character);
}

} // namespace

PIKAID_TEST(official_meta_describes_revision_1_0_4)
{
    const test_json::value& meta = vectors().at("meta");
    CHECK_EQ(meta.at("project").text, std::string("pikaid"));
    CHECK_EQ(meta.at("string_length").number, 26);
    CHECK_EQ(meta.at("spec_revision").text, std::string("1.0.4"));
    CHECK_EQ(meta.at("identifier_format").text,
             std::string("immutable and unversioned"));

    const test_json::value& timestamp = meta.at("timestamp");
    CHECK_EQ(timestamp.at("encoding").text, std::string("base36-lower"));
    CHECK_EQ(timestamp.at("chars").number, 7);
    CHECK_EQ(timestamp.at("min").number, 0);
    CHECK_EQ(timestamp.at("max").number, 78364164095);

    const test_json::value& entropy = meta.at("entropy");
    CHECK_EQ(entropy.at("bytes").number, 12);
    CHECK_EQ(entropy.at("chars").number, 19);

    const test_json::value& binary = meta.at("binary");
    CHECK_EQ(binary.at("bytes").number, 17);

    CHECK_EQ(pikaid::string_length, static_cast<std::size_t>(26));
    CHECK_EQ(pikaid::binary_length, static_cast<std::size_t>(17));
    CHECK_EQ(pikaid::timestamp_length, static_cast<std::size_t>(7));
    CHECK_EQ(pikaid::randomness_length, static_cast<std::size_t>(19));
    CHECK_EQ(pikaid::randomness_byte_length, static_cast<std::size_t>(12));
    CHECK_EQ(pikaid::max_timestamp, static_cast<std::int64_t>(78364164095));
}

PIKAID_TEST(official_valid_vectors_round_trip)
{
    const test_json::value& valid = vectors().at("valid");
    CHECK(valid.kind == test_json::value::kind_t::array_value);
    CHECK_EQ(valid.items.size(), static_cast<std::size_t>(4));

    for (const test_json::value& vector : valid.items) {
        const std::string name = vector.at("name").text;
        const std::string id = vector.at("id").text;
        const std::string bin_hex = vector.at("bin_hex").text;
        const std::string entropy_hex = vector.at("entropy_hex").text;
        const std::uint64_t expected_timestamp =
            static_cast<std::uint64_t>(vector.at("timestamp").number);

        CHECK_EQ_CTX(id.size(), static_cast<std::size_t>(26), name);
        CHECK_CTX(pikaid::is_well_formed(id), name);
        CHECK_CTX(pikaid::is_valid(id), name);

        const pikaid::binary expected_binary = binary_from_hex(bin_hex);
        CHECK_EQ_CTX(pikaid::to_binary(id), expected_binary, name);
        CHECK_EQ_CTX(pikaid::from_binary(expected_binary), id, name);
        CHECK_EQ_CTX(pikaid::from_binary(std::string_view(
                         reinterpret_cast<const char*>(expected_binary.data()),
                         expected_binary.size())),
                     id, name);

        CHECK_EQ_CTX(pikaid::timestamp(id), expected_timestamp, name);
        CHECK_EQ_CTX(pikaid::randomness_hex(id), entropy_hex, name);

        const pikaid::parts parsed = pikaid::parse(id);
        CHECK_EQ_CTX(parsed.timestamp, expected_timestamp, name);
        CHECK_EQ_CTX(bytes_to_hex(parsed.randomness), entropy_hex, name);
        CHECK_EQ_CTX(parsed.randomness,
                     randomness_from_hex(entropy_hex), name);

        // Same-value conformance: the deterministic generation path must
        // reproduce the official id and bytes from the fixture timestamp and
        // entropy, so encoding is not only tested through decoding.
        const pikaid::randomness_bytes entropy =
            randomness_from_hex(entropy_hex);
        CHECK_EQ_CTX(pikaid::generate(
                         static_cast<std::int64_t>(expected_timestamp),
                         [entropy] { return entropy; }),
                     id, name);
        CHECK_EQ_CTX(pikaid::generate_binary(
                         static_cast<std::int64_t>(expected_timestamp),
                         [entropy] { return entropy; }),
                     expected_binary, name);

        CHECK_EQ_CTX(pikaid::compare(id, id), 0, name);
        CHECK_EQ_CTX(pikaid::compare_binary(expected_binary, expected_binary),
                     0, name);

        // The binary layout starts with the big-endian timestamp.
        CHECK_EQ_CTX(bytes_to_hex(expected_binary).substr(0, 10),
                     timestamp_hex(expected_timestamp), name);
    }
}

PIKAID_TEST(official_invalid_string_vectors_are_rejected)
{
    const test_json::value& invalid = vectors().at("invalid").at("string");
    CHECK_EQ(invalid.items.size(), static_cast<std::size_t>(6));

    for (const test_json::value& vector : invalid.items) {
        const std::string name = vector.at("name").text;
        const std::string id = vector.at("id").text;

        CHECK_CTX(!pikaid::is_valid(id), name);
        CHECK_THROWS_CODE_CTX(pikaid::parse(id), expected_string_error(name),
                              name);
        CHECK_THROWS_CODE_CTX(pikaid::to_binary(id), expected_string_error(name),
                              name);
        CHECK_THROWS_CODE_CTX(pikaid::timestamp(id), expected_string_error(name),
                              name);
        CHECK_THROWS_CODE_CTX(pikaid::randomness(id),
                              expected_string_error(name), name);
    }
}

PIKAID_TEST(official_invalid_binary_vectors_are_rejected)
{
    const test_json::value& invalid = vectors().at("invalid").at("binary");
    CHECK_EQ(invalid.items.size(), static_cast<std::size_t>(4));

    for (const test_json::value& vector : invalid.items) {
        const std::string name = vector.at("name").text;
        const std::vector<std::uint8_t> bytes =
            testing_support::hex_to_bytes(vector.at("bin_hex").text);
        const std::string_view view(
            reinterpret_cast<const char*>(bytes.data()), bytes.size());

        if (bytes.size() != pikaid::binary_length) {
            CHECK_THROWS_CODE_CTX(pikaid::from_binary(view),
                                  pikaid::error_code::invalid_binary_length,
                                  name);
        } else {
            const pikaid::binary binary = binary_from_hex(
                vector.at("bin_hex").text);
            CHECK_THROWS_CODE_CTX(pikaid::from_binary(binary),
                                  pikaid::error_code::timestamp_overflow,
                                  name);
            CHECK_THROWS_CODE_CTX(pikaid::from_binary(view),
                                  pikaid::error_code::timestamp_overflow,
                                  name);
            CHECK_THROWS_CODE_CTX(
                pikaid::compare_binary(binary, pikaid::binary{}),
                pikaid::error_code::timestamp_overflow, name);
        }
    }
}

PIKAID_TEST(official_fixture_schema_shape_is_satisfied)
{
    // Every fixture field required by pikaid-v1.schema.json is present with the
    // expected type. This is a targeted structural check, not a JSON Schema
    // implementation.
    const test_json::value& document = vectors();
    const test_json::value& valid = document.at("valid");
    for (const test_json::value& vector : valid.items) {
        CHECK(vector.at("name").kind ==
              test_json::value::kind_t::string_value);
        CHECK(vector.at("timestamp").kind ==
              test_json::value::kind_t::number_value);
        CHECK(vector.at("entropy_hex").kind ==
              test_json::value::kind_t::string_value);
        CHECK(vector.at("id").kind == test_json::value::kind_t::string_value);
        CHECK(vector.at("bin_hex").kind ==
              test_json::value::kind_t::string_value);
        CHECK_EQ(vector.at("entropy_hex").text.size(),
                 static_cast<std::size_t>(24));
        CHECK_EQ(vector.at("id").text.size(), static_cast<std::size_t>(26));
        CHECK_EQ(vector.at("bin_hex").text.size(),
                 static_cast<std::size_t>(34));
    }

    for (const test_json::value& vector :
         document.at("invalid").at("string").items) {
        CHECK(vector.at("name").kind ==
              test_json::value::kind_t::string_value);
        CHECK(vector.at("id").kind == test_json::value::kind_t::string_value);
    }

    for (const test_json::value& vector :
         document.at("invalid").at("binary").items) {
        CHECK(vector.at("name").kind ==
              test_json::value::kind_t::string_value);
        CHECK(vector.at("bin_hex").kind ==
              test_json::value::kind_t::string_value);
        CHECK_EQ(vector.at("bin_hex").text.size() % 2,
                 static_cast<std::size_t>(0));
    }

    // Guards the fixture provenance claim in tests/vectors/README.md.
    CHECK_EQ(test_sha256::digest(testing_support::raw_vectors()),
             std::string(fixture_sha256));
    CHECK_EQ(test_sha256::digest(std::string_view()),
             std::string("e3b0c44298fc1c149afbf4c8996fb924"
                         "27ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(test_sha256::digest(std::string_view("abc")),
             std::string("ba7816bf8f01cfea414140de5dae2223"
                         "b00361a396177a9cb410ff61f20015ad"));
}
