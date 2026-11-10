// SPDX-License-Identifier: MIT

#include "pikaid/pikaid.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <bcrypt.h>
#else
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#endif

#if defined(__has_include)
#if __has_include(<sys/random.h>)
#include <sys/random.h>
#define PIKAID_HAS_SYS_RANDOM 1
#endif
#endif

/// /dev/urandom is compiled only where the dedicated system call is not
/// available, so an unused fallback never becomes untested dead code.
#if !defined(_WIN32) &&                                                       \
    (!defined(PIKAID_HAS_SYS_RANDOM) || (!defined(__linux__) &&               \
                                         !defined(__APPLE__) &&               \
                                         !defined(__FreeBSD__) &&             \
                                         !defined(__OpenBSD__) &&             \
                                         !defined(__NetBSD__) &&              \
                                         !defined(__DragonFly__)))
#define PIKAID_USE_URANDOM 1
#endif

namespace pikaid {
namespace {

constexpr char alphabet[] = "0123456789abcdefghijklmnopqrstuvwxyz";

/// Base of the pikaid alphabet. Kept as std::uint32_t so the long-division
/// inner loops below stay in 32-bit unsigned arithmetic on every platform:
/// a byte is at most 255, so 255 * 36 + 35 = 9215 fits far below 2^32 - 1.
constexpr std::uint32_t alphabet_size = 36;

[[nodiscard]] int base36_digit(unsigned char character) noexcept
{
    if (character >= static_cast<unsigned char>('0') &&
        character <= static_cast<unsigned char>('9')) {
        return static_cast<int>(character - static_cast<unsigned char>('0'));
    }
    if (character >= static_cast<unsigned char>('a') &&
        character <= static_cast<unsigned char>('z')) {
        return static_cast<int>(character - static_cast<unsigned char>('a')) + 10;
    }
    return -1;
}

[[nodiscard]] std::string describe_byte(unsigned char character)
{
    if (character >= 0x20U && character <= 0x7eU) {
        return "'" + std::string(1, static_cast<char>(character)) + "'";
    }
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "0x";
    result += hex[(character >> 4U) & 0x0fU];
    result += hex[character & 0x0fU];
    return result;
}

[[nodiscard]] std::string encode_timestamp(std::uint64_t value)
{
    std::string encoded(timestamp_length, '0');
    for (std::size_t index = timestamp_length; index-- > 0;) {
        encoded[index] = alphabet[static_cast<std::size_t>(value % alphabet_size)];
        value /= alphabet_size;
    }
    return encoded;
}

[[nodiscard]] std::string encode_randomness(const randomness_bytes& bytes)
{
    randomness_bytes work = bytes;
    std::string encoded(randomness_length, '0');
    for (std::size_t position = randomness_length; position-- > 0;) {
        std::uint32_t remainder = 0;
        for (std::size_t index = 0; index < work.size(); ++index) {
            const std::uint32_t value =
                (remainder << 8U) | static_cast<std::uint32_t>(work[index]);
            work[index] = static_cast<std::uint8_t>(value / alphabet_size);
            remainder = value % alphabet_size;
        }
        encoded[position] = alphabet[static_cast<std::size_t>(remainder)];
    }
    return encoded;
}

/// Decode exactly 19 Base36 characters into 12 bytes.
///
/// Returns false instead of wrapping when the value needs more than 96 bits.
[[nodiscard]] bool decode_randomness(std::string_view text,
                                     randomness_bytes& result) noexcept
{
    randomness_bytes bytes{};
    for (char character : text) {
        const int digit = base36_digit(static_cast<unsigned char>(character));
        if (digit < 0) {
            return false;
        }
        std::uint32_t carry = static_cast<std::uint32_t>(digit);
        for (std::size_t index = bytes.size(); index-- > 0;) {
            const std::uint32_t value =
                static_cast<std::uint32_t>(bytes[index]) * alphabet_size + carry;
            bytes[index] = static_cast<std::uint8_t>(value & 0xffU);
            carry = value >> 8U;
        }
        if (carry != 0U) {
            return false;
        }
    }
    result = bytes;
    return true;
}

[[nodiscard]] std::uint64_t decode_timestamp(std::string_view text) noexcept
{
    std::uint64_t value = 0;
    for (char character : text) {
        value = value * alphabet_size +
                static_cast<std::uint64_t>(
                    base36_digit(static_cast<unsigned char>(character)));
    }
    return value;
}

[[nodiscard]] std::uint64_t read_binary_timestamp(const binary& bytes) noexcept
{
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 5; ++index) {
        value = (value << 8U) | static_cast<std::uint64_t>(bytes[index]);
    }
    return value;
}

void validate_binary_timestamp(const binary& bytes)
{
    const std::uint64_t value = read_binary_timestamp(bytes);
    if (value > static_cast<std::uint64_t>(max_timestamp)) {
        throw error(error_code::timestamp_overflow,
                    "binary timestamp " + std::to_string(value) +
                        " exceeds maximum " + std::to_string(max_timestamp));
    }
}

[[nodiscard]] std::int64_t current_unix_seconds()
{
    const auto now = std::chrono::system_clock::now();
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch());
    return static_cast<std::int64_t>(seconds.count());
}

[[noreturn]] void throw_randomness_failure(const std::string& detail)
{
    throw error(error_code::randomness_failure,
                "cryptographic randomness source failed: " + detail);
}

#if !defined(_WIN32)
[[noreturn]] void throw_errno_failure(const char* operation)
{
    const int saved_errno = errno;
    throw_randomness_failure(std::string(operation) + ": " +
                             std::string(std::strerror(saved_errno)));
}

#if defined(PIKAID_USE_URANDOM)
void fill_from_urandom(std::uint8_t* output, std::size_t size)
{
    int flags = O_RDONLY;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif

    const int descriptor = ::open("/dev/urandom", flags);
    if (descriptor < 0) {
        throw_errno_failure("open(\"/dev/urandom\")");
    }

    std::size_t filled = 0;
    while (filled < size) {
        const ssize_t count = ::read(descriptor, output + filled, size - filled);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int saved_errno = errno;
            ::close(descriptor);
            errno = saved_errno;
            throw_errno_failure("read(\"/dev/urandom\")");
        }
        if (count == 0) {
            ::close(descriptor);
            throw_randomness_failure(
                "read(\"/dev/urandom\") returned end-of-file");
        }
        filled += static_cast<std::size_t>(count);
    }

    if (::close(descriptor) != 0) {
        throw_errno_failure("close(\"/dev/urandom\")");
    }
}
#endif // PIKAID_USE_URANDOM
#endif

} // namespace

error::error(error_code code, const std::string& message)
    : std::invalid_argument(message), code_(code)
{
}

randomness_bytes secure_randomness()
{
    randomness_bytes output{};

#if defined(_WIN32)
    const NTSTATUS status =
        ::BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(output.data()),
                          static_cast<ULONG>(output.size()),
                          BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (status != 0) {
        throw_randomness_failure("BCryptGenRandom returned status " +
                                 std::to_string(static_cast<long>(status)));
    }
#elif defined(PIKAID_HAS_SYS_RANDOM) && defined(__linux__)
    std::size_t filled = 0;
    while (filled < output.size()) {
        const ssize_t count =
            ::getrandom(output.data() + filled, output.size() - filled, 0);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw_errno_failure("getrandom");
        }
        if (count == 0) {
            throw_randomness_failure("getrandom returned zero bytes");
        }
        filled += static_cast<std::size_t>(count);
    }
#elif defined(PIKAID_HAS_SYS_RANDOM) &&                                   \
    (defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
     defined(__NetBSD__) || defined(__DragonFly__))
    if (::getentropy(output.data(), output.size()) != 0) {
        throw_errno_failure("getentropy");
    }
#elif defined(PIKAID_HAS_SYS_RANDOM) && defined(__unix__)
    fill_from_urandom(output.data(), output.size());
#elif defined(__unix__) || defined(__unix)
    fill_from_urandom(output.data(), output.size());
#else
#error "pikaid: no cryptographic randomness backend for this platform"
#endif

    return output;
}

void validate_timestamp(std::int64_t timestamp)
{
    if (timestamp < min_timestamp) {
        throw error(error_code::timestamp_negative,
                    "timestamp " + std::to_string(timestamp) +
                        " is negative; pikaid timestamps start at the Unix epoch");
    }
    if (timestamp > max_timestamp) {
        throw error(error_code::timestamp_overflow,
                    "timestamp " + std::to_string(timestamp) +
                        " exceeds maximum " + std::to_string(max_timestamp) +
                        " (36^7 - 1)");
    }
}

std::string generate()
{
    return generate(current_unix_seconds());
}

std::string generate(std::int64_t timestamp)
{
    validate_timestamp(timestamp);
    return encode_timestamp(static_cast<std::uint64_t>(timestamp)) +
           encode_randomness(secure_randomness());
}

std::string generate(std::int64_t timestamp, const randomness_source& source)
{
    validate_timestamp(timestamp);
    if (!source) {
        throw error(error_code::randomness_failure,
                    "randomness source is empty");
    }
    return encode_timestamp(static_cast<std::uint64_t>(timestamp)) +
           encode_randomness(source());
}

binary generate_binary()
{
    return generate_binary(current_unix_seconds());
}

binary generate_binary(std::int64_t timestamp)
{
    validate_timestamp(timestamp);
    binary output{};
    const std::uint64_t value = static_cast<std::uint64_t>(timestamp);
    output[0] = static_cast<std::uint8_t>((value >> 32U) & 0xffU);
    output[1] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
    output[2] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    output[3] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    output[4] = static_cast<std::uint8_t>(value & 0xffU);
    const randomness_bytes random = secure_randomness();
    std::copy(random.begin(), random.end(), output.begin() + 5);
    return output;
}

binary generate_binary(std::int64_t timestamp, const randomness_source& source)
{
    validate_timestamp(timestamp);
    if (!source) {
        throw error(error_code::randomness_failure,
                    "randomness source is empty");
    }

    binary output{};
    const std::uint64_t value = static_cast<std::uint64_t>(timestamp);
    output[0] = static_cast<std::uint8_t>((value >> 32U) & 0xffU);
    output[1] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
    output[2] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    output[3] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    output[4] = static_cast<std::uint8_t>(value & 0xffU);
    const randomness_bytes random = source();
    std::copy(random.begin(), random.end(), output.begin() + 5);
    return output;
}

bool is_well_formed(std::string_view id) noexcept
{
    if (id.size() != string_length) {
        return false;
    }
    for (char character : id) {
        if (base36_digit(static_cast<unsigned char>(character)) < 0) {
            return false;
        }
    }
    return true;
}

bool is_valid(std::string_view id) noexcept
{
    if (!is_well_formed(id)) {
        return false;
    }
    randomness_bytes ignored{};
    return decode_randomness(id.substr(timestamp_length), ignored);
}

parts parse(std::string_view id)
{
    if (id.size() != string_length) {
        throw error(error_code::invalid_string_length,
                    "pikaid must be exactly " + std::to_string(string_length) +
                        " characters, got " + std::to_string(id.size()));
    }
    for (std::size_t index = 0; index < string_length; ++index) {
        if (base36_digit(static_cast<unsigned char>(id[index])) < 0) {
            throw error(error_code::invalid_character,
                        "invalid character " +
                            describe_byte(static_cast<unsigned char>(id[index])) +
                            " at position " + std::to_string(index) +
                            "; expected lowercase base36 [0-9a-z]");
        }
    }

    parts result;
    result.timestamp = decode_timestamp(id.substr(0, timestamp_length));
    if (!decode_randomness(id.substr(timestamp_length), result.randomness)) {
        throw error(error_code::randomness_overflow,
                    "randomness suffix exceeds 96 bits (2^96 - 1)");
    }
    return result;
}

std::uint64_t timestamp(std::string_view id)
{
    return parse(id).timestamp;
}

randomness_bytes randomness(std::string_view id)
{
    return parse(id).randomness;
}

std::string randomness_hex(std::string_view id)
{
    static constexpr char hex[] = "0123456789abcdef";
    const randomness_bytes bytes = randomness(id);
    std::string result(randomness_hex_length, '0');
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        result[index * 2] = hex[(bytes[index] >> 4U) & 0x0fU];
        result[index * 2 + 1] = hex[bytes[index] & 0x0fU];
    }
    return result;
}

binary to_binary(std::string_view id)
{
    const parts parsed = parse(id);
    binary output{};
    output[0] = static_cast<std::uint8_t>((parsed.timestamp >> 32U) & 0xffU);
    output[1] = static_cast<std::uint8_t>((parsed.timestamp >> 24U) & 0xffU);
    output[2] = static_cast<std::uint8_t>((parsed.timestamp >> 16U) & 0xffU);
    output[3] = static_cast<std::uint8_t>((parsed.timestamp >> 8U) & 0xffU);
    output[4] = static_cast<std::uint8_t>(parsed.timestamp & 0xffU);
    std::copy(parsed.randomness.begin(), parsed.randomness.end(),
              output.begin() + 5);
    return output;
}

std::string from_binary(const binary& bytes)
{
    validate_binary_timestamp(bytes);
    randomness_bytes random{};
    std::copy(bytes.begin() + 5, bytes.end(), random.begin());
    return encode_timestamp(read_binary_timestamp(bytes)) +
           encode_randomness(random);
}

std::string from_binary(std::string_view bytes)
{
    if (bytes.size() != binary_length) {
        throw error(error_code::invalid_binary_length,
                    "binary pikaid must be exactly " +
                        std::to_string(binary_length) + " bytes, got " +
                        std::to_string(bytes.size()));
    }
    binary buffer{};
    for (std::size_t index = 0; index < binary_length; ++index) {
        buffer[index] =
            static_cast<std::uint8_t>(static_cast<unsigned char>(bytes[index]));
    }
    return from_binary(buffer);
}

int compare(std::string_view a, std::string_view b)
{
    static_cast<void>(parse(a));
    static_cast<void>(parse(b));
    const int ordering = a.compare(b);
    return (ordering > 0) - (ordering < 0);
}

int compare_binary(const binary& a, const binary& b)
{
    validate_binary_timestamp(a);
    validate_binary_timestamp(b);
    for (std::size_t index = 0; index < binary_length; ++index) {
        if (a[index] != b[index]) {
            return a[index] < b[index] ? -1 : 1;
        }
    }
    return 0;
}

} // namespace pikaid
