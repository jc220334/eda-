#pragma once
// =====================================================================
// 基础类型：单位、坐标点、焊盘形状、图层类型、颜色
// 内部统一使用「毫米(mm)」作为规范坐标单位；文件中的单位信息另存。
// =====================================================================
#include <string>
#include <cmath>

namespace pcb {

// ---------- 单位 ----------
enum class Unit { Millimeter, Mil, Inch };

namespace unit {
inline const char* name(Unit u) {
    switch (u) {
        case Unit::Millimeter: return "mm";
        case Unit::Mil:        return "mil";
        case Unit::Inch:       return "inch";
    }
    return "mm";
}

// 把 value（以 u 为单位）换算成毫米
inline double toMm(double value, Unit u) {
    switch (u) {
        case Unit::Millimeter: return value;
        case Unit::Mil:        return value * 0.0254;
        case Unit::Inch:       return value * 25.4;
    }
    return value;
}

// 把毫米换算成 u
inline double fromMm(double mm, Unit u) {
    switch (u) {
        case Unit::Millimeter: return mm;
        case Unit::Mil:        return mm / 0.0254;
        case Unit::Inch:       return mm / 25.4;
    }
    return mm;
}

// 从字符串解析单位；未知返回 false
inline bool parse(const std::string& s, Unit& out) {
    if (s == "mm")   { out = Unit::Millimeter; return true; }
    if (s == "mil")  { out = Unit::Mil;        return true; }
    if (s == "inch") { out = Unit::Inch;       return true; }
    return false;
}
} // namespace unit

// ---------- 浮点比较（相等判断 / 测试用） ----------
inline constexpr double kEps = 1e-6;
inline bool near(double a, double b) { return std::fabs(a - b) <= kEps; }

// ---------- 坐标点（规范单位：毫米） ----------
struct Point {
    double x = 0.0;
    double y = 0.0;
    Point() = default;
    Point(double x_, double y_) : x(x_), y(y_) {}

    bool operator==(const Point& o) const { return near(x, o.x) && near(y, o.y); }
    bool operator!=(const Point& o) const { return !(*this == o); }
    double distanceTo(const Point& o) const { return std::hypot(x - o.x, y - o.y); }
};

// ---------- 焊盘形状 ----------
enum class PadShape { Circle, Rect, Oval, RoundedRect };

inline const char* padShapeName(PadShape s) {
    switch (s) {
        case PadShape::Circle:      return "circle";
        case PadShape::Rect:        return "rect";
        case PadShape::Oval:        return "oval";
        case PadShape::RoundedRect: return "rounded-rect";
    }
    return "rect";
}
inline bool parsePadShape(const std::string& s, PadShape& out) {
    if (s == "circle")       { out = PadShape::Circle;      return true; }
    if (s == "rect")         { out = PadShape::Rect;        return true; }
    if (s == "oval")         { out = PadShape::Oval;        return true; }
    if (s == "rounded-rect") { out = PadShape::RoundedRect; return true; }
    return false;
}

// ---------- 图层类型 ----------
enum class LayerType { Copper, SilkScreen, Mask, Paste, Edge, Other };

inline const char* layerTypeName(LayerType t) {
    switch (t) {
        case LayerType::Copper:     return "copper";
        case LayerType::SilkScreen: return "silkscreen";
        case LayerType::Mask:       return "mask";
        case LayerType::Paste:      return "paste";
        case LayerType::Edge:       return "edge";
        case LayerType::Other:      return "other";
    }
    return "other";
}
inline bool parseLayerType(const std::string& s, LayerType& out) {
    if (s == "copper")     { out = LayerType::Copper;     return true; }
    if (s == "silkscreen") { out = LayerType::SilkScreen; return true; }
    if (s == "mask")       { out = LayerType::Mask;       return true; }
    if (s == "paste")      { out = LayerType::Paste;      return true; }
    if (s == "edge")       { out = LayerType::Edge;       return true; }
    if (s == "other")      { out = LayerType::Other;      return true; }
    return false;
}

// ---------- 颜色（以 "#RRGGBB" 字符串表示） ----------
struct Color {
    std::string hex = "#000000";
    bool operator==(const Color& o) const { return hex == o.hex; }
};

} // namespace pcb
