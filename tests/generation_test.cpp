// SPDX-License-Identifier: MIT
//
// Generation: exact deterministic output, secure default source, and explicit
// failure behaviour.

#include <pikaid/pikaid.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>

#include "support.hpp"
#include "test_harness.hpp"

namespace {

pikaid::randomness_bytes sequential_entropy()
{
    return {0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
            0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb};
}

bool is_base36_text(const std::string& text)
{
    for (char character : text) {
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'z'))) {
            return false;
        }
    }
    return true;
}

} // namespace

PIKAID_TEST(generation_with_timestamp_is_deterministic)
{
    int invocations = 0;
    const std::string id = pikaid::generate(1700000000, [&invocations] {
        ++invocations;
        return sequential_entropy();
    });

    CHECK_EQ(invocations, 1);
    CHECK_EQ(id, std::string("0s44we8002loz3po6mcf6vsn23"));
    CHECK_EQ(pikaid::timestamp(id), 1700000000ULL);
    CHECK_EQ(pikaid::randomness_hex(id),
             std::string("00112233445566778899aabb"));
}

PIKAID_TEST(generation_binary_with_timestamp_is_deterministic)
{
    int invocations = 0;
    const pikaid::binary bytes = pikaid::generate_binary(
        1700000000, [&invocations] {
            ++invocations;
            return sequential_entropy();
        });

    CHECK_EQ(invocations, 1);
    CHECK_EQ(testing_support::bytes_to_hex(bytes),
             std::string("006553f10000112233445566778899aabb"));
    CHECK_EQ(pikaid::from_binary(bytes),
             std::string("0s44we8002loz3po6mcf6vsn23"));
}

PIKAID_TEST(generation_uses_the_supplied_entropy_exactly)
{
    pikaid::randomness_bytes entropy{};
    for (std::size_t index = 0; index < entropy.size(); ++index) {
        entropy[index] = static_cast<std::uint8_t>(0xf0U + index);
    }

    const std::string id = pikaid::generate(
        78364164095, [entropy] { return entropy; });
    CHECK_EQ(pikaid::timestamp(id), 78364164095ULL);
    CHECK_EQ(pikaid::randomness(id), entropy);
    CHECK_EQ(pikaid::randomness_hex(id),
             std::string("f0f1f2f3f4f5f6f7f8f9fafb"));
    CHECK_EQ(pikaid::to_binary(id)[0], static_cast<std::uint8_t>(0x12));
    CHECK_EQ(pikaid::to_binary(id)[4], static_cast<std::uint8_t>(0xff));
}

PIKAID_TEST(generation_zero_and_maximum_timestamps)
{
    const pikaid::randomness_bytes zero{};
    CHECK_EQ(pikaid::generate(0, [zero] { return zero; }),
             std::string("00000000000000000000000000"));
    CHECK_EQ(pikaid::generate(pikaid::max_timestamp,
                              [zero] { return zero; }),
             std::string("zzzzzzz0000000000000000000"));

    pikaid::randomness_bytes ones{};
    ones.fill(0xffU);
    CHECK_EQ(pikaid::generate(0, [ones] { return ones; }),
             std::string("00000007oiylpimjg5u2ca1ypr"));
    CHECK_EQ(pikaid::generate(pikaid::max_timestamp,
                              [ones] { return ones; }),
             std::string("zzzzzzz7oiylpimjg5u2ca1ypr"));
}

PIKAID_TEST(generation_default_uses_current_time)
{
    const auto before = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    const std::string id = pikaid::generate();
    const auto after = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();

    CHECK_EQ(id.size(), pikaid::string_length);
    CHECK(pikaid::is_valid(id));
    CHECK(is_base36_text(id));
    const std::uint64_t generated = pikaid::timestamp(id);
    CHECK(generated >= static_cast<std::uint64_t>(before));
    CHECK(generated <= static_cast<std::uint64_t>(after));
    CHECK_EQ(pikaid::from_binary(pikaid::to_binary(id)), id);
}

PIKAID_TEST(generation_default_binary_uses_current_time)
{
    const pikaid::binary bytes = pikaid::generate_binary();
    CHECK_EQ(bytes.size(), pikaid::binary_length);
    const std::string id = pikaid::from_binary(bytes);
    CHECK(pikaid::is_valid(id));
    CHECK_EQ(pikaid::to_binary(id), bytes);

    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    const std::uint64_t generated = pikaid::timestamp(id);
    CHECK(generated + 5ULL >= static_cast<std::uint64_t>(now));
    CHECK(generated <= static_cast<std::uint64_t>(now) + 5ULL);
}

PIKAID_TEST(generation_secure_randomness_returns_fresh_bytes)
{
    const pikaid::randomness_bytes first = pikaid::secure_randomness();
    const pikaid::randomness_bytes second = pikaid::secure_randomness();

    CHECK(first != second);
    CHECK(first != pikaid::randomness_bytes{});
    CHECK(second != pikaid::randomness_bytes{});

    std::set<std::string> seen;
    for (int iteration = 0; iteration < 64; ++iteration) {
        const pikaid::randomness_bytes value = pikaid::secure_randomness();
        seen.insert(testing_support::bytes_to_hex(value));
    }
    CHECK_EQ(seen.size(), static_cast<std::size_t>(64));
}

PIKAID_TEST(generation_ids_are_distinct_in_practice)
{
    std::set<std::string> seen;
    for (int iteration = 0; iteration < 1000; ++iteration) {
        const std::string id = pikaid::generate();
        CHECK(pikaid::is_valid(id));
        seen.insert(id);
    }
    CHECK_EQ(seen.size(), static_cast<std::size_t>(1000));
}

PIKAID_TEST(generation_same_timestamp_still_varies)
{
    std::set<std::string> seen;
    for (int iteration = 0; iteration < 256; ++iteration) {
        const std::string id = pikaid::generate(1700000000);
        CHECK_EQ(pikaid::timestamp(id), 1700000000ULL);
        CHECK(pikaid::is_valid(id));
        seen.insert(id);
    }
    CHECK_EQ(seen.size(), static_cast<std::size_t>(256));
}

PIKAID_TEST(generation_randomness_source_failure_propagates)
{
    bool caught = false;
    try {
        static_cast<void>(pikaid::generate(1700000000, []() -> pikaid::randomness_bytes {
            throw std::runtime_error("injected CSPRNG failure");
        }));
    } catch (const std::runtime_error& exception) {
        caught = true;
        CHECK_EQ(std::string(exception.what()),
                 std::string("injected CSPRNG failure"));
    }
    CHECK(caught);

    bool binary_caught = false;
    try {
        static_cast<void>(pikaid::generate_binary(
            1700000000, []() -> pikaid::randomness_bytes {
                throw std::runtime_error("injected binary CSPRNG failure");
            }));
    } catch (const std::runtime_error& exception) {
        binary_caught = true;
        CHECK_EQ(std::string(exception.what()),
                 std::string("injected binary CSPRNG failure"));
    }
    CHECK(binary_caught);

    // A throwing source must not produce an identifier.
    CHECK_THROWS_ANY(pikaid::generate(0, []() -> pikaid::randomness_bytes {
        throw 42;
    }));
}

PIKAID_TEST(generation_source_is_not_called_for_invalid_timestamps)
{
    int invocations = 0;
    auto source = [&invocations] {
        ++invocations;
        return pikaid::randomness_bytes{};
    };

    CHECK_THROWS_CODE(pikaid::generate(-1, source),
                      pikaid::error_code::timestamp_negative);
    CHECK_THROWS_CODE(pikaid::generate(pikaid::max_timestamp + 1, source),
                      pikaid::error_code::timestamp_overflow);
    CHECK_THROWS_CODE(pikaid::generate_binary(-1, source),
                      pikaid::error_code::timestamp_negative);
    CHECK_EQ(invocations, 0);
}

PIKAID_TEST(generation_each_entropy_byte_affects_the_identifier)
{
    const pikaid::randomness_bytes zero{};
    const std::string baseline =
        pikaid::generate(1700000000, [zero] { return zero; });

    for (std::size_t index = 0; index < pikaid::randomness_byte_length; ++index) {
        pikaid::randomness_bytes entropy{};
        entropy[index] = 0x01U;
        const std::string changed = pikaid::generate(
            1700000000, [entropy] { return entropy; });
        CHECK_CTX(changed != baseline, "byte " + std::to_string(index));
        CHECK_EQ_CTX(pikaid::randomness(changed), entropy,
                     "byte " + std::to_string(index));
        CHECK_EQ_CTX(pikaid::randomness_hex(changed).substr(index * 2, 2),
                     std::string("01"), "byte " + std::to_string(index));
    }
}

PIKAID_TEST(generation_binary_matches_text_generation)
{
    for (int iteration = 0; iteration < 100; ++iteration) {
        const std::int64_t timestamp = 1700000000 + iteration;
        const std::string text = pikaid::generate(timestamp);
        const pikaid::binary bytes = pikaid::generate_binary(timestamp);
        CHECK_EQ(pikaid::timestamp(text), static_cast<std::uint64_t>(timestamp));
        CHECK_EQ(pikaid::from_binary(bytes).size(), pikaid::string_length);
        CHECK_EQ(pikaid::from_binary(pikaid::to_binary(text)), text);
        CHECK_EQ(pikaid::timestamp(pikaid::from_binary(bytes)),
                 static_cast<std::uint64_t>(timestamp));
    }
}
