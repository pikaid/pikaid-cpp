// SPDX-License-Identifier: MIT
//
// Lossless conversion, exact layout, known answers computed independently of
// the C++ implementation, and ordering properties.

#include <pikaid/pikaid.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

#include "support.hpp"
#include "test_harness.hpp"

using testing_support::binary_from_hex;
using testing_support::bytes_to_hex;
using testing_support::randomness_from_hex;

namespace {

struct known_answer {
    std::uint64_t timestamp;
    const char* entropy_hex;
    const char* id;
    const char* binary_hex;
};

// Oracles computed with a separate Python Base36 implementation from the
// definition in pikaid specification revision 1.0.4. They are not derived from
// the C++ code under test.
constexpr known_answer known_answers[] = {
    {0ULL, "000000000000000000000000", "00000000000000000000000000",
     "0000000000000000000000000000000000"},
    {0ULL, "000000000000000000000001", "00000000000000000000000001",
     "0000000000000000000000000000000001"},
    {0ULL, "ffffffffffffffffffffffff", "00000007oiylpimjg5u2ca1ypr",
     "0000000000ffffffffffffffffffffffff"},
    {1ULL, "000000000000000000000001", "00000010000000000000000001",
     "0000000001000000000000000000000001"},
    {35ULL, "000000000000000000000000", "000000z0000000000000000000",
     "0000000023000000000000000000000000"},
    {36ULL, "ffffffffffffffffffffffff", "00000107oiylpimjg5u2ca1ypr",
     "0000000024ffffffffffffffffffffffff"},
    {1295ULL, "800000000000000000000000", "00000zz3u9haurb9q2x1650zcw",
     "000000050f800000000000000000000000"},
    {1296ULL, "7fffffffffffffffffffffff", "00001003u9haurb9q2x1650zcv",
     "00000005107fffffffffffffffffffffff"},
    {1700000000ULL, "00112233445566778899aabb", "0s44we8002loz3po6mcf6vsn23",
     "006553f10000112233445566778899aabb"},
    {2176782335ULL, "000000000000000000000000", "0zzzzzz0000000000000000000",
     "0081bf0fff000000000000000000000000"},
    {2176782336ULL, "000000000000000000000000", "10000000000000000000000000",
     "0081bf1000000000000000000000000000"},
    {78364164094ULL, "ffffffffffffffffffffffff", "zzzzzzy7oiylpimjg5u2ca1ypr",
     "123ede3ffeffffffffffffffffffffffff"},
    {78364164095ULL, "000000000000000000000000", "zzzzzzz0000000000000000000",
     "123ede3fff000000000000000000000000"},
    {78364164095ULL, "ffffffffffffffffffffffff", "zzzzzzz7oiylpimjg5u2ca1ypr",
     "123ede3fffffffffffffffffffffffffff"},
};

/// Deterministic, self-contained pseudo-random generator for property tests.
class lcg {
public:
    explicit lcg(std::uint64_t seed) : state_(seed) {}

    std::uint64_t next()
    {
        state_ = state_ * 6364136223846793005ULL + 1442695040888963407ULL;
        return state_ >> 11U;
    }

private:
    std::uint64_t state_;
};

pikaid::randomness_bytes next_randomness(lcg& generator)
{
    pikaid::randomness_bytes bytes{};
    for (std::size_t index = 0; index < bytes.size(); index += 8) {
        std::uint64_t word = generator.next();
        for (std::size_t offset = 0; offset < 8 && index + offset < bytes.size();
             ++offset) {
            bytes[index + offset] = static_cast<std::uint8_t>(word & 0xffU);
            word >>= 8U;
        }
    }
    return bytes;
}

} // namespace

PIKAID_TEST(conversion_known_answers_from_independent_oracle)
{
    for (const known_answer& answer : known_answers) {
        const pikaid::binary expected_binary =
            binary_from_hex(answer.binary_hex);
        const pikaid::randomness_bytes expected_randomness =
            randomness_from_hex(answer.entropy_hex);

        CHECK_EQ(pikaid::from_binary(expected_binary), std::string(answer.id));
        CHECK_EQ(pikaid::to_binary(answer.id), expected_binary);
        CHECK_EQ(pikaid::timestamp(answer.id), answer.timestamp);
        CHECK_EQ(pikaid::randomness_hex(answer.id),
                 std::string(answer.entropy_hex));
        CHECK_EQ(pikaid::randomness(answer.id), expected_randomness);
        CHECK(pikaid::is_valid(answer.id));

        // The deterministic generation path must agree with the same
        // implementation-independent oracle, not only the conversion path.
        const pikaid::randomness_bytes entropy = expected_randomness;
        CHECK_EQ(pikaid::generate(static_cast<std::int64_t>(answer.timestamp),
                                  [entropy] { return entropy; }),
                 std::string(answer.id));
        CHECK_EQ(pikaid::generate_binary(
                     static_cast<std::int64_t>(answer.timestamp),
                     [entropy] { return entropy; }),
                 expected_binary);
    }
}

PIKAID_TEST(conversion_binary_layout_is_exact)
{
    lcg generator(0x70696b616964ULL);
    for (int iteration = 0; iteration < 500; ++iteration) {
        const std::uint64_t timestamp =
            generator.next() % 78364164096ULL;
        pikaid::randomness_bytes entropy = next_randomness(generator);

        const std::string id =
            pikaid::generate(static_cast<std::int64_t>(timestamp),
                             [entropy] { return entropy; });
        const pikaid::binary bytes = pikaid::to_binary(id);

        CHECK_EQ(bytes.size(), pikaid::binary_length);
        for (std::size_t index = 0; index < 5; ++index) {
            CHECK_EQ(bytes[index], static_cast<std::uint8_t>(
                                       (timestamp >>
                                        ((4U - static_cast<unsigned>(index)) * 8U)) &
                                       0xffU));
        }
        for (std::size_t index = 0; index < entropy.size(); ++index) {
            CHECK_EQ(bytes[index + 5], entropy[index]);
        }
        CHECK_EQ(pikaid::from_binary(bytes), id);
        CHECK_EQ(pikaid::parse(id).randomness, entropy);
    }
}

PIKAID_TEST(conversion_preserves_leading_zeros_and_boundaries)
{
    const std::string zeros(pikaid::string_length, '0');
    CHECK_EQ(pikaid::from_binary(pikaid::binary{}), zeros);
    CHECK_EQ(pikaid::to_binary(zeros), pikaid::binary{});

    const std::string one = std::string(25, '0') + "1";
    CHECK(pikaid::is_valid(one));
    const pikaid::binary one_binary = pikaid::to_binary(one);
    CHECK_EQ(one_binary[16], static_cast<std::uint8_t>(1));
    CHECK_EQ(pikaid::from_binary(one_binary), one);

    const std::string maximum = "zzzzzzz7oiylpimjg5u2ca1ypr";
    const pikaid::binary maximum_binary = pikaid::to_binary(maximum);
    CHECK_EQ(bytes_to_hex(maximum_binary),
             std::string("123ede3fffffffffffffffffffffffffff"));
    CHECK_EQ(pikaid::from_binary(maximum_binary), maximum);

    // Every single-bit entropy value survives the trip unchanged.
    for (std::size_t byte_index = 0; byte_index < 12; ++byte_index) {
        for (int bit = 0; bit < 8; ++bit) {
            pikaid::randomness_bytes entropy{};
            entropy[byte_index] =
                static_cast<std::uint8_t>(1U << static_cast<unsigned>(bit));
            const std::string id = pikaid::generate(
                1700000000, [entropy] { return entropy; });
            CHECK_EQ(pikaid::randomness(id), entropy);
            CHECK_EQ(pikaid::from_binary(pikaid::to_binary(id)), id);
            CHECK_EQ(pikaid::randomness_hex(id),
                     bytes_to_hex(entropy));
        }
    }
}

PIKAID_TEST(conversion_randomness_decoder_handles_96_bit_boundary)
{
    // The 19-character suffix is decoded with 32-bit long division: a byte is
    // at most 255 and the carry at most 35, so 255 * 36 + 35 = 9215 never
    // wraps. Exercise the exact upper boundary independently derived from the
    // specification: 2^96 - 1 is the largest accepted suffix and 2^96 is the
    // smallest rejected one.
    const std::string maximum_suffix = "7oiylpimjg5u2ca1ypr"; // 2^96 - 1
    const std::string overflow_suffix = "7oiylpimjg5u2ca1yps"; // 2^96

    const std::string prefix(pikaid::timestamp_length, '0');
    const std::string maximum = prefix + maximum_suffix;
    const std::string overflow = prefix + overflow_suffix;

    CHECK(pikaid::is_valid(maximum));
    CHECK_EQ(pikaid::randomness_hex(maximum), std::string(24, 'f'));
    CHECK_EQ(pikaid::parse(maximum).randomness,
             randomness_from_hex("ffffffffffffffffffffffff"));

    CHECK(pikaid::is_well_formed(overflow));
    CHECK(!pikaid::is_valid(overflow));
    CHECK_THROWS_CODE(pikaid::parse(overflow),
                      pikaid::error_code::randomness_overflow);
}

PIKAID_TEST(conversion_text_binary_round_trip_property)
{
    lcg generator(0x626173653336ULL);
    for (int iteration = 0; iteration < 2000; ++iteration) {
        const std::uint64_t timestamp = generator.next() % 78364164096ULL;
        const pikaid::randomness_bytes entropy = next_randomness(generator);
        const std::string id = pikaid::generate(
            static_cast<std::int64_t>(timestamp),
            [entropy] { return entropy; });

        CHECK_EQ(id.size(), pikaid::string_length);
        CHECK(pikaid::is_valid(id));
        CHECK_EQ(pikaid::from_binary(pikaid::to_binary(id)), id);
        CHECK_EQ(pikaid::timestamp(id), timestamp);
        CHECK_EQ(pikaid::randomness(id), entropy);
    }
}

PIKAID_TEST(conversion_binary_text_round_trip_property)
{
    lcg generator(0x7a7a7aULL);
    for (int iteration = 0; iteration < 2000; ++iteration) {
        pikaid::binary bytes{};
        const std::uint64_t timestamp = generator.next() % 78364164096ULL;
        for (std::size_t index = 5; index-- > 0;) {
            bytes[index] = static_cast<std::uint8_t>(timestamp >>
                                                     ((4U - static_cast<unsigned>(
                                                               index)) *
                                                      8U));
        }
        const pikaid::randomness_bytes entropy = next_randomness(generator);
        std::copy(entropy.begin(), entropy.end(), bytes.begin() + 5);

        const std::string id = pikaid::from_binary(bytes);
        CHECK_EQ(pikaid::to_binary(id), bytes);
        CHECK_EQ(pikaid::timestamp(id), timestamp);
        CHECK_EQ(pikaid::randomness(id), entropy);
        CHECK_EQ(id.substr(0, 7).size(), pikaid::timestamp_length);
    }
}

PIKAID_TEST(conversion_randomness_hex_matches_binary)
{
    lcg generator(0x686578ULL);
    for (int iteration = 0; iteration < 300; ++iteration) {
        const pikaid::randomness_bytes entropy = next_randomness(generator);
        const std::string id = pikaid::generate(
            1700000000, [entropy] { return entropy; });
        const std::string hex = pikaid::randomness_hex(id);
        CHECK_EQ(hex.size(), pikaid::randomness_hex_length);
        CHECK_EQ(hex, bytes_to_hex(entropy));

        // Lowercase hexadecimal only.
        for (char character : hex) {
            CHECK((character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'f'));
        }
    }
}

PIKAID_TEST(conversion_compare_is_lexicographic_across_seconds)
{
    const std::string zero = std::string(26, '0');
    const std::string second = pikaid::from_binary(
        binary_from_hex("0000000001000000000000000000000001"));
    CHECK_EQ(pikaid::compare(zero, second), -1);
    CHECK_EQ(pikaid::compare(second, zero), 1);
    CHECK_EQ(pikaid::compare(zero, zero), 0);

    // A later timestamp always sorts after an earlier one, regardless of the
    // randomness field.
    const std::string earlier = pikaid::generate(
        1700000000, [] { return pikaid::randomness_bytes{}; });
    const std::string later = pikaid::generate(
        1700000001, [] { return pikaid::randomness_bytes{}; });
    CHECK_EQ(pikaid::compare(earlier, later), -1);
    CHECK(pikaid::compare(later, earlier) > 0);

    // Maximum timestamp sorts last.
    const std::string maximum = "zzzzzzz7oiylpimjg5u2ca1ypr";
    CHECK_EQ(pikaid::compare(later, maximum), -1);
    CHECK_EQ(pikaid::compare(maximum, maximum), 0);
}

PIKAID_TEST(conversion_compare_within_one_second_is_not_monotonic)
{
    // Two identifiers in the same second are ordered by randomness, not by
    // generation order. Both orders are legal and no monotonicity is promised.
    const pikaid::randomness_bytes low{};
    pikaid::randomness_bytes high{};
    high.fill(0xffU);

    const std::string generated_first = pikaid::generate(
        1700000000, [high] { return high; });
    const std::string generated_second = pikaid::generate(
        1700000000, [low] { return low; });

    CHECK_EQ(pikaid::timestamp(generated_first), 1700000000ULL);
    CHECK_EQ(pikaid::timestamp(generated_second), 1700000000ULL);
    CHECK_EQ(pikaid::compare(generated_first, generated_second), 1);
    CHECK_EQ(pikaid::compare(generated_second, generated_first), -1);
}

PIKAID_TEST(conversion_cross_representation_ordering_agrees)
{
    lcg generator(0x6f72646572ULL);
    for (int iteration = 0; iteration < 1000; ++iteration) {
        const std::uint64_t left_timestamp = generator.next() % 78364164096ULL;
        const std::uint64_t right_timestamp = generator.next() % 78364164096ULL;
        const pikaid::randomness_bytes left_entropy = next_randomness(generator);
        const pikaid::randomness_bytes right_entropy = next_randomness(generator);

        const std::string left = pikaid::generate(
            static_cast<std::int64_t>(left_timestamp),
            [left_entropy] { return left_entropy; });
        const std::string right = pikaid::generate(
            static_cast<std::int64_t>(right_timestamp),
            [right_entropy] { return right_entropy; });

        const int text_order = pikaid::compare(left, right);
        const int binary_order =
            pikaid::compare_binary(pikaid::to_binary(left),
                                   pikaid::to_binary(right));

        CHECK_EQ(text_order, binary_order);
        CHECK_EQ(text_order, (left_timestamp != right_timestamp)
                                 ? (left_timestamp < right_timestamp ? -1 : 1)
                                 : (left_entropy < right_entropy
                                        ? -1
                                        : (left_entropy == right_entropy ? 0 : 1)));
    }
}

PIKAID_TEST(conversion_sorting_is_chronological_then_lexicographic)
{
    lcg generator(0x736f7274ULL);
    std::vector<std::string> ids;
    ids.reserve(500);
    for (int iteration = 0; iteration < 500; ++iteration) {
        // A deliberately small timestamp range forces many ties.
        const std::uint64_t timestamp = 1700000000ULL + generator.next() % 5U;
        const pikaid::randomness_bytes entropy = next_randomness(generator);
        ids.push_back(pikaid::generate(
            static_cast<std::int64_t>(timestamp),
            [entropy] { return entropy; }));
    }

    std::vector<std::string> shuffled = ids;
    std::sort(shuffled.begin(), shuffled.end(),
              [](const std::string& left, const std::string& right) {
                  return pikaid::compare(left, right) < 0;
              });

    for (std::size_t index = 1; index < shuffled.size(); ++index) {
        CHECK(pikaid::compare(shuffled[index - 1], shuffled[index]) <= 0);
        CHECK(shuffled[index - 1] < shuffled[index] ||
              shuffled[index - 1] == shuffled[index]);
        if (shuffled[index - 1] != shuffled[index]) {
            CHECK(shuffled[index - 1] < shuffled[index]);
        }
    }

    // Grouped by timestamp, each group must be sorted by its text suffix.
    std::size_t begin = 0;
    while (begin < shuffled.size()) {
        std::size_t end = begin + 1;
        while (end < shuffled.size() &&
               pikaid::timestamp(shuffled[end]) ==
                   pikaid::timestamp(shuffled[begin])) {
            ++end;
        }
        for (std::size_t index = begin + 1; index < end; ++index) {
            CHECK(shuffled[index - 1].substr(7) <= shuffled[index].substr(7));
        }
        begin = end;
    }
}

PIKAID_TEST(conversion_timestamp_encoding_at_base36_boundaries)
{
    const std::vector<std::uint64_t> timestamps = {
        0ULL,       1ULL,       35ULL,      36ULL,      37ULL,
        1295ULL,    1296ULL,    1297ULL,    46655ULL,   46656ULL,
        1679615ULL, 1679616ULL, 60466175ULL, 60466176ULL,
        2176782335ULL, 2176782336ULL, 78364164094ULL, 78364164095ULL};

    for (std::uint64_t timestamp : timestamps) {
        const pikaid::randomness_bytes zero{};
        const std::string id = pikaid::generate(
            static_cast<std::int64_t>(timestamp), [zero] { return zero; });
        CHECK_EQ(id.size(), pikaid::string_length);
        CHECK_EQ(pikaid::timestamp(id), timestamp);
        CHECK_EQ(id.substr(7), std::string(19, '0'));
        CHECK_EQ(pikaid::from_binary(pikaid::to_binary(id)), id);

        // The 7-character prefix must be zero-padded and lowercase Base36.
        const std::string prefix = id.substr(0, 7);
        CHECK_EQ(prefix.size(), pikaid::timestamp_length);
        for (char character : prefix) {
            CHECK((character >= '0' && character <= '9') ||
                  (character >= 'a' && character <= 'z'));
        }
        CHECK_EQ(pikaid::parse(id).timestamp, timestamp);
    }

    // Zero padding is significant: timestamps 35 and 36 differ only in the
    // last timestamp character.
    const pikaid::randomness_bytes zero{};
    const std::string thirty_five = pikaid::generate(
        35, [zero] { return zero; });
    const std::string thirty_six = pikaid::generate(36, [zero] { return zero; });
    CHECK_EQ(thirty_five.substr(0, 7), std::string("000000z"));
    CHECK_EQ(thirty_six.substr(0, 6), std::string("000001"));
    CHECK(thirty_five < thirty_six);
}
