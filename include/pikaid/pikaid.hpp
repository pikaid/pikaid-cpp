// SPDX-License-Identifier: MIT
//
// Official C++ implementation of pikaid.
//
// A pikaid is an immutable, unversioned identifier encoded as exactly 26
// lowercase Base36 characters: a 7-character Unix timestamp in whole seconds
// followed by 19 characters carrying exactly 96 bits of cryptographic
// randomness. The binary form is exactly 17 bytes: a 5-byte unsigned
// big-endian timestamp followed by the original 12 randomness bytes.

#ifndef PIKAID_PIKAID_HPP
#define PIKAID_PIKAID_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(_WIN32) && defined(PIKAID_SHARED)
#if defined(PIKAID_BUILDING_LIBRARY)
#define PIKAID_API __declspec(dllexport)
#else
#define PIKAID_API __declspec(dllimport)
#endif
#else
#define PIKAID_API
#endif

namespace pikaid {

/// Length of the canonical text representation, in characters.
inline constexpr std::size_t string_length = 26;

/// Length of the canonical binary representation, in bytes.
inline constexpr std::size_t binary_length = 17;

/// Width of the timestamp prefix in the text representation.
inline constexpr std::size_t timestamp_length = 7;

/// Width of the randomness suffix in the text representation.
inline constexpr std::size_t randomness_length = 19;

/// Width of the randomness field in the binary representation, in bytes.
inline constexpr std::size_t randomness_byte_length = 12;

/// Length of the lowercase hexadecimal rendering of the randomness field.
inline constexpr std::size_t randomness_hex_length = 24;

/// Largest representable timestamp: 36^7 - 1 (4453-04-05T15:21:35Z).
inline constexpr std::int64_t max_timestamp = 78'364'164'095;

/// Smallest representable timestamp: the Unix epoch.
inline constexpr std::int64_t min_timestamp = 0;

/// Exactly 96 bits of randomness, in big-endian byte order.
using randomness_bytes = std::array<std::uint8_t, randomness_byte_length>;

/// Exactly 17 bytes: [5-byte big-endian timestamp][12 randomness bytes].
using binary = std::array<std::uint8_t, binary_length>;

/// Machine-readable reason for a pikaid error.
enum class error_code : int {
    /// A text representation is not exactly 26 characters long.
    invalid_string_length = 1,
    /// A text representation contains a byte outside [0-9a-z].
    invalid_character = 2,
    /// A timestamp exceeds max_timestamp or a binary timestamp field does.
    timestamp_overflow = 3,
    /// A timestamp is before the Unix epoch.
    timestamp_negative = 4,
    /// A 19-character suffix decodes to a value above 2^96 - 1.
    randomness_overflow = 5,
    /// A binary representation is not exactly 17 bytes long.
    invalid_binary_length = 6,
    /// The platform cryptographic randomness source failed.
    randomness_failure = 7,
};

/// Exception thrown by every strict pikaid operation on invalid input.
///
/// The type derives from std::invalid_argument, so existing code that catches
/// std::invalid_argument (or std::exception) keeps working, while new code can
/// inspect code() for a stable, programmatic reason.
class PIKAID_API error : public std::invalid_argument {
public:
    error(error_code code, const std::string& message);

    /// Stable reason for the failure.
    [[nodiscard]] error_code code() const noexcept { return code_; }

private:
    error_code code_;
};

/// Decoded components of a valid pikaid.
struct parts {
    /// Whole Unix seconds, 0..max_timestamp.
    std::uint64_t timestamp{};
    /// The original 12 randomness bytes, leading zeros preserved.
    randomness_bytes randomness{};
};

/// Callable returning exactly 12 fresh randomness bytes.
///
/// The default used by generate()/generate_binary() is secure_randomness(),
/// which reads from the operating system cryptographic generator. The
/// injection point exists for deterministic tests, deterministic data
/// migrations and integration with an alternative audited CSPRNG. It must
/// never be used to supply predictable randomness in production.
using randomness_source = std::function<randomness_bytes()>;

// ---------------------------------------------------------------------------
// Randomness
// ---------------------------------------------------------------------------

/// Read exactly 12 bytes from the operating system CSPRNG.
///
/// Backends: BCryptGenRandom(BCRYPT_USE_SYSTEM_PREFERRED_RNG) on Windows,
/// getrandom(2) on Linux when available, getentropy(2) on Apple/BSD platforms
/// when available, and /dev/urandom on other POSIX systems. The function fails
/// closed with error_code::randomness_failure if the source reports an error
/// or a short read. std::random_device is never used.
[[nodiscard]] PIKAID_API randomness_bytes secure_randomness();

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

/// Generate a text identifier using the current Unix time.
///
/// Throws error_code::timestamp_negative or timestamp_overflow when the system
/// clock is outside the representable range, and
/// error_code::randomness_failure when the OS CSPRNG fails.
[[nodiscard]] PIKAID_API std::string generate();

/// Generate a text identifier with an explicit timestamp.
///
/// The timestamp must be a whole number of Unix seconds in
/// [min_timestamp, max_timestamp]; negative values are rejected, never
/// coerced. Randomness still comes from the operating system CSPRNG.
[[nodiscard]] PIKAID_API std::string generate(std::int64_t timestamp);

/// Deterministic-generation hook: use the supplied randomness source.
///
/// The source is invoked exactly once. This overload is intended for tests and
/// migrations. Production code should use generate(timestamp).
[[nodiscard]] PIKAID_API std::string generate(std::int64_t timestamp,
                                              const randomness_source& source);

/// Generate a 17-byte binary identifier using the current Unix time.
[[nodiscard]] PIKAID_API binary generate_binary();

/// Generate a 17-byte binary identifier with an explicit timestamp.
[[nodiscard]] PIKAID_API binary generate_binary(std::int64_t timestamp);

/// Deterministic binary-generation hook; the source is invoked exactly once.
[[nodiscard]] PIKAID_API binary generate_binary(std::int64_t timestamp,
                                                const randomness_source& source);

// ---------------------------------------------------------------------------
// Validation
// ---------------------------------------------------------------------------

/// Shape check only: exactly 26 characters from [0-9a-z].
///
/// This deliberately does not detect a suffix above 2^96 - 1. A true result
/// does not mean the value is a valid pikaid; use is_valid() for that.
[[nodiscard]] PIKAID_API bool is_well_formed(std::string_view id) noexcept;

/// Full semantic validation: shape plus a randomness value within 96 bits.
///
/// Never throws and never normalizes input.
[[nodiscard]] PIKAID_API bool is_valid(std::string_view id) noexcept;

/// Validate a timestamp for generation.
///
/// Throws error_code::timestamp_negative for values below 0 and
/// error_code::timestamp_overflow for values above max_timestamp.
PIKAID_API void validate_timestamp(std::int64_t timestamp);

// ---------------------------------------------------------------------------
// Parsing and accessors
// ---------------------------------------------------------------------------

/// Strictly parse a text identifier into its timestamp and randomness bytes.
///
/// Throws error on any violation of the format; the input is never truncated
/// or case-folded.
[[nodiscard]] PIKAID_API parts parse(std::string_view id);

/// Extract the timestamp; the whole identifier is validated first.
[[nodiscard]] PIKAID_API std::uint64_t timestamp(std::string_view id);

/// Extract the 12 randomness bytes; the whole identifier is validated first.
[[nodiscard]] PIKAID_API randomness_bytes randomness(std::string_view id);

/// Extract the randomness as 24 lowercase hexadecimal characters.
[[nodiscard]] PIKAID_API std::string randomness_hex(std::string_view id);

// ---------------------------------------------------------------------------
// Binary conversion
// ---------------------------------------------------------------------------

/// Convert a valid text identifier to its canonical 17-byte form.
[[nodiscard]] PIKAID_API binary to_binary(std::string_view id);

/// Convert a canonical 17-byte value to its text identifier.
///
/// Throws error_code::timestamp_overflow when the big-endian timestamp exceeds
/// max_timestamp. Every 12-byte randomness field is valid.
[[nodiscard]] PIKAID_API std::string from_binary(const binary& bytes);

/// Byte-buffer overload; throws error_code::invalid_binary_length unless
/// bytes.size() == binary_length.
[[nodiscard]] PIKAID_API std::string from_binary(std::string_view bytes);

// ---------------------------------------------------------------------------
// Ordering
// ---------------------------------------------------------------------------

/// Compare two valid text identifiers bytewise (ASCII-compatible).
///
/// Returns -1, 0 or 1. Different timestamps sort chronologically. Identifiers
/// with the same timestamp sort by randomness, not by generation order:
/// pikaid is not monotonic within a second. Both inputs are fully validated.
[[nodiscard]] PIKAID_API int compare(std::string_view a, std::string_view b);

/// Compare two canonical binary identifiers by unsigned bytewise order.
///
/// Both timestamps are validated; randomness needs no further validation.
[[nodiscard]] PIKAID_API int compare_binary(const binary& a, const binary& b);

} // namespace pikaid

#endif // PIKAID_PIKAID_HPP
