// SPDX-License-Identifier: MIT
//
// Test-only JSON-Lines bridge for the official PikaID conformance suite.
//
// This executable is built from a separate CMake project against an installed
// `pikaid::pikaid` package. It is never installed or shipped. Every PikaID
// operation below delegates to the public C++ API; the bridge only translates
// the wire format (JSON, hexadecimal) and never encodes, validates, parses or
// generates identifiers itself.

#include <pikaid/pikaid.hpp>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#endif

#include "json.hpp"

namespace {

using json_value = test_json::value;

/// A request argument was malformed or missing. This maps to
/// `invalid_input`, not to an unexpected internal failure.
struct invalid_input : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/// The requested operation is not part of protocol v1.
struct unsupported_operation : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ---------------------------------------------------------------------------
// Wire-format helpers (hexadecimal and JSON only)
// ---------------------------------------------------------------------------

[[nodiscard]] std::string hex_encode(const std::uint8_t* data,
                                     std::size_t size)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string text(size * 2, '0');
    for (std::size_t index = 0; index < size; ++index) {
        text[index * 2] = digits[(data[index] >> 4U) & 0x0fU];
        text[index * 2 + 1] = digits[data[index] & 0x0fU];
    }
    return text;
}

[[nodiscard]] std::string hex_encode(const pikaid::binary& bytes)
{
    return hex_encode(bytes.data(), bytes.size());
}

[[nodiscard]] std::string hex_encode(const pikaid::randomness_bytes& bytes)
{
    return hex_encode(bytes.data(), bytes.size());
}

[[nodiscard]] int hex_digit(char character) noexcept
{
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    return -1;
}

/// Decode the protocol's canonical `binary_hex` form. Uppercase, odd-length
/// and non-hexadecimal inputs are rejected as invalid input: the wire contract
/// is exactly 34 lowercase hexadecimal digits.
[[nodiscard]] pikaid::binary decode_binary(std::string_view hex)
{
    if (hex.size() != pikaid::binary_length * 2) {
        throw invalid_input("binary_hex must be exactly 34 lowercase hex digits");
    }
    pikaid::binary bytes{};
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        const int high = hex_digit(hex[index * 2]);
        const int low = hex_digit(hex[index * 2 + 1]);
        if (high < 0 || low < 0) {
            throw invalid_input(
                "binary_hex must contain only lowercase hexadecimal digits");
        }
        bytes[index] =
            static_cast<std::uint8_t>((high << 4) | low);
    }
    return bytes;
}

[[nodiscard]] std::string json_escape(std::string_view text)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(text.size() + 2);
    for (char character : text) {
        const auto byte = static_cast<unsigned char>(character);
        switch (character) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (byte < 0x20U) {
                output += "\\u00";
                output.push_back(digits[(byte >> 4U) & 0x0fU]);
                output.push_back(digits[byte & 0x0fU]);
            } else {
                output.push_back(character);
            }
            break;
        }
    }
    return output;
}

[[nodiscard]] std::string json_string(std::string_view text)
{
    return "\"" + json_escape(text) + "\"";
}

[[nodiscard]] std::string json_bool(bool value)
{
    return value ? "true" : "false";
}

// ---------------------------------------------------------------------------
// Request argument access
// ---------------------------------------------------------------------------

[[nodiscard]] const json_value& object_arguments(const json_value& request)
{
    const auto found = request.members.find("arguments");
    if (found == request.members.end()) {
        static const json_value empty = [] {
            json_value value;
            value.kind = json_value::kind_t::object_value;
            return value;
        }();
        return empty;
    }
    if (found->second.kind != json_value::kind_t::object_value) {
        throw invalid_input("arguments must be a JSON object");
    }
    return found->second;
}

[[nodiscard]] const json_value& argument(const json_value& arguments,
                                         std::string_view key)
{
    const auto found = arguments.members.find(key);
    if (found == arguments.members.end()) {
        throw invalid_input("missing required argument: " + std::string(key));
    }
    return found->second;
}

[[nodiscard]] std::string string_argument(const json_value& arguments,
                                          std::string_view key)
{
    const json_value& value = argument(arguments, key);
    if (value.kind != json_value::kind_t::string_value) {
        throw invalid_input("argument " + std::string(key) +
                            " must be a string");
    }
    return value.text;
}

[[nodiscard]] std::int64_t integer_argument(const json_value& arguments,
                                            std::string_view key)
{
    const json_value& value = argument(arguments, key);
    if (value.kind != json_value::kind_t::number_value) {
        throw invalid_input("argument " + std::string(key) +
                            " must be an integer");
    }
    return value.number;
}

[[nodiscard]] bool bool_argument(const json_value& arguments,
                                 std::string_view key)
{
    const json_value& value = argument(arguments, key);
    if (value.kind != json_value::kind_t::boolean_value) {
        throw invalid_input("argument " + std::string(key) +
                            " must be a boolean");
    }
    return value.boolean;
}

// ---------------------------------------------------------------------------
// Adapter state and operations
// ---------------------------------------------------------------------------

struct adapter_state {
    bool csprng_failure = false;
};

[[nodiscard]] pikaid::randomness_bytes injected_csprng_failure()
{
    throw pikaid::error(pikaid::error_code::randomness_failure,
                        "injected CSPRNG failure");
}

/// A current-time generation path has no public randomness-source overload.
/// When fault injection is active the bridge fails closed before calling the
/// library, exactly as a consumer would have to observe an OS CSPRNG failure.
[[noreturn]] void fail_current_time_generation_closed(const char* operation)
{
    throw pikaid::error(pikaid::error_code::randomness_failure,
                        std::string("injected CSPRNG failure (") + operation +
                            " current-time path)");
}

[[nodiscard]] std::string operation_hello()
{
    // parse_datetime is deliberately not advertised: the public C++ API
    // exposes lossless integer timestamps only, so no correct UTC datetime can
    // be returned for every legal timestamp. Every other optional operation is
    // backed by a public function.
    return R"({"protocol_version":1,"capabilities":[)"
           R"("generate_string","text_to_binary","binary_to_text",)"
           R"("boolean_validation","format_only_validation","parse_components",)"
           R"("generate_with_timestamp","generate_binary",)"
           R"("generate_binary_with_timestamp","public_compare",)"
           R"("csprng_failure_injection"]})";
}

[[nodiscard]] std::string operation_value(adapter_state& state,
                                          std::string_view operation,
                                          const json_value& arguments)
{
    if (operation == "hello") {
        return operation_hello();
    }
    if (operation == "generate_string") {
        if (state.csprng_failure) {
            fail_current_time_generation_closed("generate_string");
        }
        return json_string(pikaid::generate());
    }
    if (operation == "text_to_binary") {
        return json_string(hex_encode(
            pikaid::to_binary(string_argument(arguments, "text"))));
    }
    if (operation == "binary_to_text") {
        const pikaid::binary bytes =
            decode_binary(string_argument(arguments, "binary_hex"));
        return json_string(pikaid::from_binary(
            std::string_view(reinterpret_cast<const char*>(bytes.data()),
                             bytes.size())));
    }
    if (operation == "boolean_validation") {
        return json_bool(
            pikaid::is_valid(string_argument(arguments, "text")));
    }
    if (operation == "format_only_validation") {
        return json_bool(
            pikaid::is_well_formed(string_argument(arguments, "text")));
    }
    if (operation == "parse_components") {
        const pikaid::parts parsed =
            pikaid::parse(string_argument(arguments, "text"));
        return "{\"timestamp_seconds\":" +
               std::to_string(parsed.timestamp) +
               ",\"randomness_hex\":" +
               json_string(hex_encode(parsed.randomness)) + "}";
    }
    if (operation == "generate_with_timestamp") {
        const auto timestamp = integer_argument(arguments, "timestamp_seconds");
        if (state.csprng_failure) {
            static_cast<void>(pikaid::generate(timestamp,
                                               injected_csprng_failure));
        }
        return json_string(pikaid::generate(timestamp));
    }
    if (operation == "generate_binary") {
        if (state.csprng_failure) {
            fail_current_time_generation_closed("generate_binary");
        }
        return json_string(hex_encode(pikaid::generate_binary()));
    }
    if (operation == "generate_binary_with_timestamp") {
        const auto timestamp = integer_argument(arguments, "timestamp_seconds");
        if (state.csprng_failure) {
            static_cast<void>(pikaid::generate_binary(
                timestamp, injected_csprng_failure));
        }
        return json_string(
            hex_encode(pikaid::generate_binary(timestamp)));
    }
    if (operation == "public_compare") {
        return std::to_string(pikaid::compare(
            string_argument(arguments, "a"),
            string_argument(arguments, "b")));
    }
    if (operation == "inject_fault") {
        const std::string fault = string_argument(arguments, "fault");
        const bool active = bool_argument(arguments, "active");
        if (fault != "csprng_failure") {
            throw invalid_input("unsupported fault: " + fault);
        }
        state.csprng_failure = active;
        return "{\"fault\":" + json_string(fault) +
               ",\"active\":" + json_bool(active) + "}";
    }
    throw unsupported_operation("unsupported operation: " +
                                std::string(operation));
}

// ---------------------------------------------------------------------------
// Envelope handling
// ---------------------------------------------------------------------------

[[nodiscard]] std::string error_response(std::int64_t request_id,
                                         std::string_view category,
                                         std::string_view message)
{
    return "{\"request_id\":" + std::to_string(request_id) +
           ",\"ok\":false,\"error\":{\"category\":" + json_string(category) +
           ",\"message\":" + json_string(message) + "}}";
}

[[nodiscard]] std::string handle_request(adapter_state& state,
                                         const json_value& request)
{
    if (request.kind != json_value::kind_t::object_value) {
        throw invalid_input("request must be a JSON object");
    }

    std::int64_t request_id = 0;
    const auto id = request.members.find("request_id");
    if (id == request.members.end() ||
        id->second.kind != json_value::kind_t::number_value) {
        throw invalid_input("request_id must be an integer");
    }
    request_id = id->second.number;

    const auto operation = request.members.find("operation");
    if (operation == request.members.end() ||
        operation->second.kind != json_value::kind_t::string_value) {
        throw invalid_input("operation must be a string");
    }

    const json_value& arguments = object_arguments(request);
    try {
        const std::string value =
            operation_value(state, operation->second.text, arguments);
        return "{\"request_id\":" + std::to_string(request_id) +
               ",\"ok\":true,\"value\":" + value + "}";
    } catch (const unsupported_operation& exc) {
        return error_response(request_id, "unsupported_operation",
                              exc.what());
    } catch (const invalid_input& exc) {
        return error_response(request_id, "invalid_input", exc.what());
    } catch (const pikaid::error& exc) {
        if (exc.code() == pikaid::error_code::randomness_failure) {
            return error_response(request_id, "entropy_unavailable",
                                  exc.what());
        }
        return error_response(request_id, "invalid_input", exc.what());
    } catch (const std::exception& exc) {
        return error_response(request_id, "internal_error", exc.what());
    } catch (...) {
        return error_response(request_id, "internal_error",
                              "unknown internal failure");
    }
}

} // namespace

int main()
{
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    adapter_state state;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        std::string response;
        try {
            const test_json::value request = test_json::parse(line);
            response = handle_request(state, request);
        } catch (const std::exception& exc) {
            // The runner always sends valid JSON. This branch is only a
            // defensive transport guard, and it cannot carry a request id.
            std::cerr << "pikaid conformance adapter: cannot handle request: "
                      << exc.what() << '\n';
            response = error_response(0, "invalid_input",
                                      "malformed request JSON");
        }

        std::cout << response << '\n';
        std::cout.flush();
    }
    return 0;
}
