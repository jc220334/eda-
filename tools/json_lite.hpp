#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace eda::json {

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Value {
public:
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value, std::less<>>;

    enum class Type {
        Null,
        Boolean,
        Integer,
        String,
        Array,
        Object,
    };

    Value() = default;

    explicit Value(bool value) : m_type(Type::Boolean), m_boolean(value) {}
    explicit Value(std::int64_t value) : m_type(Type::Integer), m_integer(value) {}
    explicit Value(std::string value) : m_type(Type::String), m_string(std::move(value)) {}
    explicit Value(Array value) : m_type(Type::Array), m_array(std::move(value)) {}
    explicit Value(Object value) : m_type(Type::Object), m_object(std::move(value)) {}

    [[nodiscard]] Type GetType() const { return m_type; }
    [[nodiscard]] bool IsNull() const { return m_type == Type::Null; }
    [[nodiscard]] bool IsBoolean() const { return m_type == Type::Boolean; }
    [[nodiscard]] bool IsInteger() const { return m_type == Type::Integer; }
    [[nodiscard]] bool IsString() const { return m_type == Type::String; }
    [[nodiscard]] bool IsArray() const { return m_type == Type::Array; }
    [[nodiscard]] bool IsObject() const { return m_type == Type::Object; }

    [[nodiscard]] bool AsBoolean() const
    {
        RequireType(Type::Boolean, "boolean");
        return m_boolean;
    }

    [[nodiscard]] std::int64_t AsInteger() const
    {
        RequireType(Type::Integer, "integer");
        return m_integer;
    }

    [[nodiscard]] const std::string& AsString() const
    {
        RequireType(Type::String, "string");
        return m_string;
    }

    [[nodiscard]] const Array& AsArray() const
    {
        RequireType(Type::Array, "array");
        return m_array;
    }

    [[nodiscard]] const Object& AsObject() const
    {
        RequireType(Type::Object, "object");
        return m_object;
    }

    [[nodiscard]] Array& AsArray()
    {
        RequireType(Type::Array, "array");
        return m_array;
    }

    [[nodiscard]] Object& AsObject()
    {
        RequireType(Type::Object, "object");
        return m_object;
    }

private:
    void RequireType(Type expected, const char* expectedName) const
    {
        if (m_type != expected) {
            throw ParseError(std::string("JSON value is not ") + expectedName);
        }
    }

    Type m_type = Type::Null;
    bool m_boolean = false;
    std::int64_t m_integer = 0;
    std::string m_string;
    Array m_array;
    Object m_object;
};

class Parser {
public:
    explicit Parser(std::string_view input) : m_input(input) {}

    [[nodiscard]] Value Parse()
    {
        SkipWhitespace();
        Value result = ParseValue(0);
        SkipWhitespace();
        if (m_position != m_input.size()) {
            Fail("unexpected data after the root value");
        }
        return result;
    }

private:
    static constexpr int kMaxDepth = 128;

    [[nodiscard]] Value ParseValue(int depth)
    {
        if (depth > kMaxDepth) {
            Fail("maximum JSON nesting depth exceeded");
        }
        if (m_position >= m_input.size()) {
            Fail("unexpected end of input");
        }

        const char current = m_input[m_position];
        if (current == '{') {
            return ParseObject(depth);
        }
        if (current == '[') {
            return ParseArray(depth);
        }
        if (current == '"') {
            return Value(ParseString());
        }
        if (current == '-' || (current >= '0' && current <= '9')) {
            return Value(ParseInteger());
        }
        if (ConsumeLiteral("true")) {
            return Value(true);
        }
        if (ConsumeLiteral("false")) {
            return Value(false);
        }
        if (ConsumeLiteral("null")) {
            return Value();
        }
        Fail("expected a JSON value");
    }

    [[nodiscard]] Value ParseObject(int depth)
    {
        Expect('{');
        SkipWhitespace();
        Value::Object object;
        if (TryConsume('}')) {
            return Value(std::move(object));
        }

        while (true) {
            SkipWhitespace();
            if (m_position >= m_input.size() || m_input[m_position] != '"') {
                Fail("expected an object key");
            }
            std::string key = ParseString();
            SkipWhitespace();
            Expect(':');
            SkipWhitespace();
            Value value = ParseValue(depth + 1);
            if (!object.emplace(std::move(key), std::move(value)).second) {
                Fail("duplicate object key");
            }
            SkipWhitespace();
            if (TryConsume('}')) {
                return Value(std::move(object));
            }
            Expect(',');
        }
    }

    [[nodiscard]] Value ParseArray(int depth)
    {
        Expect('[');
        SkipWhitespace();
        Value::Array array;
        if (TryConsume(']')) {
            return Value(std::move(array));
        }

        while (true) {
            array.push_back(ParseValue(depth + 1));
            SkipWhitespace();
            if (TryConsume(']')) {
                return Value(std::move(array));
            }
            Expect(',');
            SkipWhitespace();
        }
    }

    [[nodiscard]] std::string ParseString()
    {
        Expect('"');
        std::string result;
        while (m_position < m_input.size()) {
            const char current = m_input[m_position++];
            if (current == '"') {
                return result;
            }
            if (static_cast<unsigned char>(current) < 0x20) {
                Fail("unescaped control character in string");
            }
            if (current != '\\') {
                result.push_back(current);
                continue;
            }
            if (m_position >= m_input.size()) {
                Fail("unfinished string escape");
            }

            const char escaped = m_input[m_position++];
            switch (escaped) {
            case '"':
            case '\\':
            case '/':
                result.push_back(escaped);
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            case 'u':
                AppendUtf8(result, ParseUnicodeEscape());
                break;
            default:
                Fail("invalid string escape");
            }
        }
        Fail("unterminated string");
    }

    [[nodiscard]] std::int64_t ParseInteger()
    {
        const std::size_t start = m_position;
        if (m_input[m_position] == '-') {
            ++m_position;
            if (m_position >= m_input.size()) {
                Fail("unfinished number");
            }
        }
        if (m_input[m_position] == '0') {
            ++m_position;
        } else if (m_input[m_position] >= '1' && m_input[m_position] <= '9') {
            while (m_position < m_input.size() &&
                   m_input[m_position] >= '0' &&
                   m_input[m_position] <= '9') {
                ++m_position;
            }
        } else {
            Fail("invalid number");
        }

        std::int64_t value = 0;
        const std::string_view text = m_input.substr(start, m_position - start);
        try {
            std::size_t consumed = 0;
            value = std::stoll(std::string(text), &consumed);
            if (consumed != text.size()) {
                Fail("invalid integer");
            }
        } catch (const std::exception&) {
            Fail("integer is out of range");
        }
        return value;
    }

    [[nodiscard]] std::uint32_t ParseUnicodeEscape()
    {
        std::uint32_t first = ParseHex4();
        if (first >= 0xD800 && first <= 0xDBFF) {
            if (m_position + 2 > m_input.size() || m_input[m_position] != '\\' ||
                m_input[m_position + 1] != 'u') {
                Fail("missing low surrogate");
            }
            m_position += 2;
            const std::uint32_t second = ParseHex4();
            if (second < 0xDC00 || second > 0xDFFF) {
                Fail("invalid low surrogate");
            }
            return 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
        }
        if (first >= 0xDC00 && first <= 0xDFFF) {
            Fail("unexpected low surrogate");
        }
        return first;
    }

    [[nodiscard]] std::uint32_t ParseHex4()
    {
        if (m_position + 4 > m_input.size()) {
            Fail("unfinished unicode escape");
        }
        std::uint32_t value = 0;
        for (int index = 0; index < 4; ++index) {
            const char digit = m_input[m_position++];
            value <<= 4;
            if (digit >= '0' && digit <= '9') {
                value += static_cast<std::uint32_t>(digit - '0');
            } else if (digit >= 'a' && digit <= 'f') {
                value += static_cast<std::uint32_t>(digit - 'a' + 10);
            } else if (digit >= 'A' && digit <= 'F') {
                value += static_cast<std::uint32_t>(digit - 'A' + 10);
            } else {
                Fail("invalid unicode escape");
            }
        }
        return value;
    }

    static void AppendUtf8(std::string& output, std::uint32_t codePoint)
    {
        if (codePoint <= 0x7F) {
            output.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint <= 0xFFFF) {
            output.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    [[nodiscard]] bool ConsumeLiteral(std::string_view literal)
    {
        if (m_input.substr(m_position, literal.size()) == literal) {
            m_position += literal.size();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool TryConsume(char expected)
    {
        if (m_position < m_input.size() && m_input[m_position] == expected) {
            ++m_position;
            return true;
        }
        return false;
    }

    void Expect(char expected)
    {
        if (!TryConsume(expected)) {
            Fail(std::string("expected '") + expected + "'");
        }
    }

    void SkipWhitespace()
    {
        while (m_position < m_input.size()) {
            const char current = m_input[m_position];
            if (current != ' ' && current != '\t' && current != '\r' && current != '\n') {
                break;
            }
            ++m_position;
        }
    }

    [[noreturn]] void Fail(const std::string& message) const
    {
        throw ParseError(message + " at byte " + std::to_string(m_position));
    }

    std::string_view m_input;
    std::size_t m_position = 0;
};

inline Value Parse(std::string_view input)
{
    return Parser(input).Parse();
}

} // namespace eda::json
