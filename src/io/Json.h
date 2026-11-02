#pragma once
// =====================================================================
// 极简 JSON 值/解析/序列化（自包含，无第三方依赖）。
// 仅用于本工程的文件格式（PCB 工程文件），支持对象、数组、字符串、
// 数字、布尔、null，以及带行号/列号的解析错误。
// =====================================================================
#include <string>
#include <vector>
#include <utility>
#include <cstddef>
#include <stdexcept>

namespace pcb {

class JsonError : public std::runtime_error {
public:
    int line = 0;
    int col = 0;
    JsonError(const std::string& msg, int line_, int col_)
        : std::runtime_error(msg), line(line_), col(col_) {}
};

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() = default;
    Type type() const { return type_; }

    static JsonValue makeNull();
    static JsonValue makeBool(bool b);
    static JsonValue makeNumber(double n);
    static JsonValue makeString(const std::string& s);
    static JsonValue makeArray();
    static JsonValue makeObject();

    // 数组
    void push(JsonValue v);
    std::size_t size() const;                 // 数组/对象元素个数
    const JsonValue& at(std::size_t i) const;

    // 对象
    void set(const std::string& key, JsonValue v);
    bool has(const std::string& key) const;
    const JsonValue& get(const std::string& key) const; // 缺失则抛 JsonError

    // 取值
    double asNumber(double dflt = 0.0) const;
    bool asBool(bool dflt = false) const;
    const std::string& asString() const;      // 非字符串则返回空串

    // 序列化 / 反序列化
    std::string dump(bool pretty = true) const;
    static JsonValue parse(const std::string& text);   // 语法错误抛 JsonError

private:
    std::string dump(int indent, int level, bool pretty) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;
    std::vector<std::pair<std::string, JsonValue>> object_; // 保持插入顺序
};

} // namespace pcb
