// SPDX-License-Identifier: MIT
//
// Minimal dependency-free JSON reader used only by the test suite.

#ifndef PIKAID_TEST_JSON_HPP
#define PIKAID_TEST_JSON_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test_json {

struct value {
    enum class kind_t {
        null_value,
        boolean_value,
        number_value,
        string_value,
        array_value,
        object_value,
    };

    kind_t kind = kind_t::null_value;
    bool boolean = false;
    std::int64_t number = 0;
    std::string text;
    std::vector<value> items;
    std::map<std::string, value, std::less<>> members;

    [[nodiscard]] const value& at(std::string_view key) const
    {
        const auto found = members.find(key);
        if (found == members.end()) {
            throw std::runtime_error("missing JSON key: " + std::string(key));
        }
        return found->second;
    }

    [[nodiscard]] const value& at(std::size_t index) const
    {
        if (index >= items.size()) {
            throw std::runtime_error("JSON array index out of range");
        }
        return items[index];
    }
};

namespace detail {

class parser {
public:
    explicit parser(std::string_view input) : input_(input) {}

    [[nodiscard]] value parse_document()
    {
        skip_whitespace();
        value document = parse_value();
        skip_whitespace();
        if (position_ != input_.size()) {
            fail("trailing data");
        }
        return document;
    }

private:
    std::string_view input_;
    std::size_t position_ = 0;

    [[noreturn]] void fail(const std::string& message) const
    {
        throw std::runtime_error("JSON parse error at byte " +
                                 std::to_string(position_) + ": " + message);
    }

    [[nodiscard]] bool at_end() const noexcept
    {
        return position_ >= input_.size();
    }

    [[nodiscard]] char peek() const
    {
        if (at_end()) {
            fail("unexpected end of input");
        }
        return input_[position_];
    }

    void skip_whitespace() noexcept
    {
        while (!at_end()) {
            const char character = input_[position_];
            if (character != ' ' && character != '\t' && character != '\n' &&
                character != '\r') {
                break;
            }
            ++position_;
        }
    }

    void expect(char expected)
    {
        if (at_end() || input_[position_] != expected) {
            fail(std::string("expected '") + expected + "'");
        }
        ++position_;
    }

    [[nodiscard]] static int hex_value(char character) noexcept
    {
        if (character >= '0' && character <= '9') {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f') {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F') {
            return character - 'A' + 10;
        }
        return -1;
    }

    [[nodiscard]] value parse_value()
    {
        skip_whitespace();
        const char character = peek();
        switch (character) {
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        case '"': {
            value result;
            result.kind = value::kind_t::string_value;
            result.text = parse_string();
            return result;
        }
        case 't':
            expect_literal("true");
            {
                value result;
                result.kind = value::kind_t::boolean_value;
                result.boolean = true;
                return result;
            }
        case 'f':
            expect_literal("false");
            {
                value result;
                result.kind = value::kind_t::boolean_value;
                result.boolean = false;
                return result;
            }
        case 'n':
            expect_literal("null");
            return value{};
        default:
            if (character == '-' || (character >= '0' && character <= '9')) {
                return parse_number();
            }
            fail("unexpected token");
        }
    }

    void expect_literal(std::string_view literal)
    {
        if (input_.substr(position_, literal.size()) != literal) {
            fail("expected " + std::string(literal));
        }
        position_ += literal.size();
    }

    [[nodiscard]] value parse_number()
    {
        const std::size_t start = position_;
        if (peek() == '-') {
            ++position_;
        }
        if (at_end() || input_[position_] < '0' || input_[position_] > '9') {
            fail("expected a digit");
        }
        while (!at_end() && input_[position_] >= '0' && input_[position_] <= '9') {
            ++position_;
        }
        if (!at_end() && (input_[position_] == '.' || input_[position_] == 'e' ||
                          input_[position_] == 'E')) {
            fail("only integer JSON numbers are supported");
        }

        value result;
        result.kind = value::kind_t::number_value;
        try {
            result.number =
                std::stoll(std::string(input_.substr(start, position_ - start)));
        } catch (const std::exception&) {
            fail("integer out of range");
        }
        return result;
    }

    [[nodiscard]] value parse_object()
    {
        value result;
        result.kind = value::kind_t::object_value;
        expect('{');
        skip_whitespace();
        if (!at_end() && peek() == '}') {
            ++position_;
            return result;
        }
        while (true) {
            skip_whitespace();
            if (peek() != '"') {
                fail("expected an object key");
            }
            std::string key = parse_string();
            skip_whitespace();
            expect(':');
            value item = parse_value();
            result.members.emplace(std::move(key), std::move(item));
            skip_whitespace();
            const char separator = peek();
            if (separator == ',') {
                ++position_;
                continue;
            }
            if (separator == '}') {
                ++position_;
                return result;
            }
            fail("expected ',' or '}'");
        }
    }

    [[nodiscard]] value parse_array()
    {
        value result;
        result.kind = value::kind_t::array_value;
        expect('[');
        skip_whitespace();
        if (!at_end() && peek() == ']') {
            ++position_;
            return result;
        }
        while (true) {
            result.items.push_back(parse_value());
            skip_whitespace();
            const char separator = peek();
            if (separator == ',') {
                ++position_;
                continue;
            }
            if (separator == ']') {
                ++position_;
                return result;
            }
            fail("expected ',' or ']'");
        }
    }

    void append_utf8(std::string& output, std::uint32_t code_point)
    {
        if (code_point <= 0x7fU) {
            output.push_back(static_cast<char>(code_point));
        } else if (code_point <= 0x7ffU) {
            output.push_back(static_cast<char>(0xc0U | (code_point >> 6U)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
        } else if (code_point <= 0xffffU) {
            if (code_point >= 0xd800U && code_point <= 0xdfffU) {
                fail("surrogate code points are not supported");
            }
            output.push_back(static_cast<char>(0xe0U | (code_point >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
        } else {
            fail("code point out of range");
        }
    }

    [[nodiscard]] std::string parse_string()
    {
        expect('"');
        std::string output;
        while (true) {
            if (at_end()) {
                fail("unterminated string");
            }
            const char character = input_[position_++];
            if (character == '"') {
                return output;
            }
            if (static_cast<unsigned char>(character) < 0x20U) {
                fail("unescaped control character");
            }
            if (character != '\\') {
                output.push_back(character);
                continue;
            }
            if (at_end()) {
                fail("truncated escape sequence");
            }
            const char escape = input_[position_++];
            switch (escape) {
            case '"':
                output.push_back('"');
                break;
            case '\\':
                output.push_back('\\');
                break;
            case '/':
                output.push_back('/');
                break;
            case 'b':
                output.push_back('\b');
                break;
            case 'f':
                output.push_back('\f');
                break;
            case 'n':
                output.push_back('\n');
                break;
            case 'r':
                output.push_back('\r');
                break;
            case 't':
                output.push_back('\t');
                break;
            case 'u': {
                std::uint32_t code_point = 0;
                for (int index = 0; index < 4; ++index) {
                    if (at_end()) {
                        fail("truncated \\u escape");
                    }
                    const int digit = hex_value(input_[position_++]);
                    if (digit < 0) {
                        fail("invalid \\u escape");
                    }
                    code_point = (code_point << 4U) |
                                 static_cast<std::uint32_t>(digit);
                }
                append_utf8(output, code_point);
                break;
            }
            default:
                fail("invalid escape sequence");
            }
        }
    }
};

} // namespace detail

[[nodiscard]] inline value parse(std::string_view input)
{
    return detail::parser(input).parse_document();
}

} // namespace test_json

#endif // PIKAID_TEST_JSON_HPP
