// =====================================================================
// JSON 解析器与序列化器实现。
// =====================================================================
#include "io/Json.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <sstream>

namespace pcb {

// ==================== 构造 ====================
JsonValue JsonValue::makeNull()              { JsonValue v; v.type_ = Type::Null;   return v; }
JsonValue JsonValue::makeBool(bool b)        { JsonValue v; v.type_ = Type::Bool;   v.bool_ = b; return v; }
JsonValue JsonValue::makeNumber(double n)    { JsonValue v; v.type_ = Type::Number; v.number_ = n; return v; }
JsonValue JsonValue::makeString(const std::string& s) { JsonValue v; v.type_ = Type::String; v.string_ = s; return v; }
JsonValue JsonValue::makeArray()             { JsonValue v; v.type_ = Type::Array;  return v; }
JsonValue JsonValue::makeObject()            { JsonValue v; v.type_ = Type::Object; return v; }

// ==================== 数组 / 对象 ====================
void JsonValue::push(JsonValue v) { array_.push_back(std::move(v)); }
std::size_t JsonValue::size() const {
    return type_ == Type::Array ? array_.size() : object_.size();
}
const JsonValue& JsonValue::at(std::size_t i) const {
    if (type_ != Type::Array || i >= array_.size())
        throw JsonError("数组下标越界", 0, 0);
    return array_[i];
}
void JsonValue::set(const std::string& key, JsonValue v) {
    for (auto& kv : object_)
        if (kv.first == key) { kv.second = std::move(v); return; }
    object_.emplace_back(key, std::move(v));
}
bool JsonValue::has(const std::string& key) const {
    for (const auto& kv : object_)
        if (kv.first == key) return true;
    return false;
}
const JsonValue& JsonValue::get(const std::string& key) const {
    for (const auto& kv : object_)
        if (kv.first == key) return kv.second;
    throw JsonError("缺少字段 \"" + key + "\"", 0, 0);
}

// ==================== 取值 ====================
double JsonValue::asNumber(double dflt) const {
    return type_ == Type::Number ? number_ : dflt;
}
bool JsonValue::asBool(bool dflt) const {
    return type_ == Type::Bool ? bool_ : dflt;
}
const std::string& JsonValue::asString() const {
    static const std::string empty;
    return type_ == Type::String ? string_ : empty;
}

// ==================== 序列化 ====================
static std::string fmtNumber(double d) {
    if (std::isnan(d) || std::isinf(d)) return "null";
    long long i = static_cast<long long>(d);
    if (d == static_cast<double>(i)) return std::to_string(i);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.6f", d);
    std::string t(buf);
    while (!t.empty() && t.back() == '0') t.pop_back();
    if (!t.empty() && t.back() == '.') t.pop_back();
    return t;
}

static std::string escapeString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string JsonValue::dump(bool pretty) const { return dump(0, 0, pretty); }

std::string JsonValue::dump(int indent, int level, bool pretty) const {
    std::string pad(pretty ? static_cast<size_t>(level + 1) * 2 : 0, ' ');
    std::string nl = pretty ? "\n" : "";
    switch (type_) {
        case Type::Null:   return "null";
        case Type::Bool:   return bool_ ? "true" : "false";
        case Type::Number: return fmtNumber(number_);
        case Type::String: return "\"" + escapeString(string_) + "\"";
        case Type::Array: {
            std::string out = "[";
            for (std::size_t i = 0; i < array_.size(); ++i) {
                if (i) out += ",";
                if (pretty) out += nl + pad;
                out += array_[i].dump(indent, level + 1, pretty);
            }
            if (!array_.empty() && pretty) out += nl + std::string(static_cast<size_t>(level) * 2, ' ');
            out += "]";
            return out;
        }
        case Type::Object: {
            std::string out = "{";
            for (std::size_t i = 0; i < object_.size(); ++i) {
                if (i) out += ",";
                if (pretty) out += nl + pad;
                out += "\"" + escapeString(object_[i].first) + "\":";
                if (pretty) out += " ";
                out += object_[i].second.dump(indent, level + 1, pretty);
            }
            if (!object_.empty() && pretty) out += nl + std::string(static_cast<size_t>(level) * 2, ' ');
            out += "}";
            return out;
        }
    }
    return "null";
}

// ==================== 解析 ====================
namespace {

struct Parser {
    const std::string& s;
    std::size_t pos = 0;
    int line = 1, col = 1;

    explicit Parser(const std::string& text) : s(text) {}

    [[noreturn]] void fail(const std::string& msg) {
        throw JsonError(msg, line, col);
    }
    char peek() const { return pos < s.size() ? s[pos] : '\0'; }
    char get() {
        if (pos >= s.size()) return '\0';
        char c = s[pos++];
        if (c == '\n') { ++line; col = 1; } else { ++col; }
        return c;
    }
    void skipWs() {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' ||
                                  s[pos] == '\r' || s[pos] == '\n'))
            get();
    }
    void expect(char c) {
        if (peek() != c) fail(std::string("期望 '") + c + "'");
        get();
    }
    bool consume(char c) {
        if (peek() == c) { get(); return true; }
        return false;
    }

    JsonValue parseValue() {
        skipWs();
        char c = peek();
        switch (c) {
            case '{': return parseObject();
            case '[': return parseArray();
            case '"': return JsonValue::makeString(parseString());
            case 't': return parseLiteral("true",  true);
            case 'f': return parseLiteral("false", false);
            case 'n': return parseLiteral("null",  false, true);
            case '\0': fail("意外的文件结尾");
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return parseNumber();
                fail(std::string("意外的字符 '") + c + "'");
        }
        return JsonValue::makeNull(); // 不可达
    }

    JsonValue parseObject() {
        JsonValue obj = JsonValue::makeObject();
        expect('{');
        skipWs();
        if (consume('}')) return obj;
        while (true) {
            skipWs();
            if (peek() != '"') fail("对象键必须是字符串");
            std::string key = parseString();
            skipWs();
            expect(':');
            obj.set(key, parseValue());
            skipWs();
            if (consume('}')) break;
            expect(',');
        }
        return obj;
    }

    JsonValue parseArray() {
        JsonValue arr = JsonValue::makeArray();
        expect('[');
        skipWs();
        if (consume(']')) return arr;
        while (true) {
            arr.push(parseValue());
            skipWs();
            if (consume(']')) break;
            expect(',');
        }
        return arr;
    }

    JsonValue parseLiteral(const char* lit, bool bval, bool isNull = false) {
        std::size_t n = std::strlen(lit);
        for (std::size_t i = 0; i < n; ++i)
            if (peek() != lit[i]) fail(std::string("无效字面量 ") + lit);
            else get();
        if (isNull) return JsonValue::makeNull();
        return JsonValue::makeBool(bval);
    }

    JsonValue parseNumber() {
        std::size_t start = pos;
        if (peek() == '-') get();
        while (peek() >= '0' && peek() <= '9') get();
        if (peek() == '.') {
            get();
            while (peek() >= '0' && peek() <= '9') get();
        }
        if (peek() == 'e' || peek() == 'E') {
            get();
            if (peek() == '+' || peek() == '-') get();
            while (peek() >= '0' && peek() <= '9') get();
        }
        std::string token = s.substr(start, pos - start);
        char* end = nullptr;
        double v = std::strtod(token.c_str(), &end);
        if (end == token.c_str()) fail("无效数字");
        return JsonValue::makeNumber(v);
    }

    static void utf8Encode(unsigned cp, std::string& out) {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    unsigned parseHex4() {
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = get();
            v <<= 4;
            if (c >= '0' && c <= '9')       v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f')  v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')  v |= static_cast<unsigned>(c - 'A' + 10);
            else fail("无效的 \\u 转义");
        }
        return v;
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (true) {
            char c = get();
            if (c == '"') break;
            if (c == '\0') fail("字符串未闭合");
            if (c == '\\') {
                char e = get();
                switch (e) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'n':  out += '\n'; break;
                    case 't':  out += '\t'; break;
                    case 'r':  out += '\r'; break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'u': {
                        unsigned cp = parseHex4();
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos + 1 < s.size() &&
                            s[pos] == '\\' && s[pos + 1] == 'u') {
                            get(); get(); // 消费 \u
                            unsigned lo = parseHex4();
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        utf8Encode(cp, out);
                        break;
                    }
                    default: fail("无效转义字符");
                }
            } else {
                out += c;
            }
        }
        return out;
    }
};

} // namespace

JsonValue JsonValue::parse(const std::string& text) {
    Parser p(text);
    JsonValue v = p.parseValue();
    p.skipWs();
    if (p.pos != text.size()) p.fail("末尾存在多余内容");
    return v;
}

} // namespace pcb
