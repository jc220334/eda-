// =====================================================================
// PCB 工程文件读写实现。
//   - 保存：内部毫米坐标 -> 按工程单位缩放后写入，顶层写 format/version/unit。
//   - 载入：读取 unit 并换算回毫米；校验格式/版本；逐实体解析。
//   - 异常处理：语法错误、格式错误、版本过高、字段缺失等均转为可读信息。
// =====================================================================
#include "io/ProjectIO.h"
#include "io/Json.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace pcb {

namespace {

// 读取时的解析错误（内部抛出，由 loadFromString 捕获）
[[noreturn]] void fail(const std::string& where, const std::string& msg) {
    throw std::runtime_error("[" + where + "] " + msg);
}

// ---------- 序列化辅助 ----------
JsonValue pointToJson(const Point& p, double scale) {
    JsonValue o = JsonValue::makeObject();
    o.set("x", JsonValue::makeNumber(p.x * scale));
    o.set("y", JsonValue::makeNumber(p.y * scale));
    return o;
}
Point pointFromJson(const JsonValue& o, double scale, const std::string& where) {
    if (!o.has("x") || !o.has("y")) fail(where, "坐标点缺少 x/y");
    return Point(o.get("x").asNumber() * scale, o.get("y").asNumber() * scale);
}
JsonValue pointsToJson(const std::vector<Point>& pts, double scale) {
    JsonValue arr = JsonValue::makeArray();
    for (const auto& p : pts) arr.push(pointToJson(p, scale));
    return arr;
}
std::vector<Point> pointsFromJson(const JsonValue& arr, double scale, const std::string& where) {
    std::vector<Point> out;
    for (std::size_t i = 0; i < arr.size(); ++i)
        out.push_back(pointFromJson(arr.at(i), scale, where));
    return out;
}
std::string strField(const JsonValue& o, const char* key, const std::string& dflt = "") {
    return o.has(key) ? o.get(key).asString() : dflt;
}
double numField(const JsonValue& o, const char* key, double dflt = 0.0) {
    return o.has(key) ? o.get(key).asNumber(dflt) : dflt;
}
bool boolField(const JsonValue& o, const char* key, bool dflt) {
    return o.has(key) ? o.get(key).asBool(dflt) : dflt;
}

// ==================== 各实体 <-> JSON ====================

// Layer
JsonValue layerToJson(const Layer& l) {
    JsonValue o = JsonValue::makeObject();
    o.set("id",      JsonValue::makeString(l.id));
    o.set("name",    JsonValue::makeString(l.name));
    o.set("type",    JsonValue::makeString(layerTypeName(l.type)));
    o.set("visible", JsonValue::makeBool(l.visible));
    o.set("order",   JsonValue::makeNumber(l.order));
    o.set("color",   JsonValue::makeString(l.color.hex));
    return o;
}
Layer layerFromJson(const JsonValue& o, const std::string& where) {
    Layer l;
    l.id = strField(o, "id");
    l.name = strField(o, "name", l.id);
    l.visible = boolField(o, "visible", true);
    l.order = static_cast<int>(numField(o, "order", 0));
    l.color.hex = strField(o, "color", "#000000");
    std::string t = strField(o, "type", "other");
    if (!parseLayerType(t, l.type)) l.type = LayerType::Other;
    if (l.id.empty()) fail(where, "图层缺少 id");
    return l;
}

// LayerGroup
JsonValue groupToJson(const LayerGroup& g) {
    JsonValue o = JsonValue::makeObject();
    o.set("name",    JsonValue::makeString(g.name));
    o.set("visible", JsonValue::makeBool(g.visible));
    JsonValue arr = JsonValue::makeArray();
    for (const auto& id : g.layerIds) arr.push(JsonValue::makeString(id));
    o.set("layers", arr);
    return o;
}
LayerGroup groupFromJson(const JsonValue& o, const std::string& where) {
    LayerGroup g;
    g.name = strField(o, "name");
    g.visible = boolField(o, "visible", true);
    if (o.has("layers"))
        for (std::size_t i = 0; i < o.get("layers").size(); ++i)
            g.layerIds.push_back(o.get("layers").at(i).asString());
    if (g.name.empty()) fail(where, "层组缺少 name");
    return g;
}

// Net
JsonValue netToJson(const Net& n) {
    JsonValue o = JsonValue::makeObject();
    o.set("id",   JsonValue::makeString(n.id));
    o.set("name", JsonValue::makeString(n.name));
    return o;
}
Net netFromJson(const JsonValue& o, const std::string& where) {
    Net n;
    n.id = strField(o, "id");
    n.name = strField(o, "name");
    if (n.id.empty()) fail(where, "网络缺少 id");
    return n;
}

// Pad
JsonValue padToJson(const Pad& p, double scale) {
    JsonValue o = JsonValue::makeObject();
    o.set("number", JsonValue::makeString(p.number));
    o.set("name",   JsonValue::makeString(p.name));
    o.set("pos",    pointToJson(p.pos, scale));
    o.set("width",  JsonValue::makeNumber(p.width * scale));
    o.set("height", JsonValue::makeNumber(p.height * scale));
    o.set("shape",  JsonValue::makeString(padShapeName(p.shape)));
    o.set("drill",  JsonValue::makeNumber(p.drill * scale));
    return o;
}
Pad padFromJson(const JsonValue& o, double scale, const std::string& where) {
    Pad p;
    p.number = strField(o, "number");
    p.name = strField(o, "name");
    p.pos = pointFromJson(o.get("pos"), scale, where);
    p.width = numField(o, "width") * scale;
    p.height = numField(o, "height") * scale;
    p.drill = numField(o, "drill") * scale;
    std::string s = strField(o, "shape", "rect");
    if (!parsePadShape(s, p.shape)) p.shape = PadShape::Rect;
    if (p.number.empty()) fail(where, "焊盘缺少 number");
    return p;
}

// Footprint
JsonValue footprintToJson(const Footprint& f, double scale) {
    JsonValue o = JsonValue::makeObject();
    o.set("id",          JsonValue::makeString(f.id));
    o.set("name",        JsonValue::makeString(f.name));
    o.set("description", JsonValue::makeString(f.description));
    JsonValue arr = JsonValue::makeArray();
    for (const auto& p : f.pads) arr.push(padToJson(p, scale));
    o.set("pads", arr);
    return o;
}
Footprint footprintFromJson(const JsonValue& o, double scale, const std::string& where) {
    Footprint f;
    f.id = strField(o, "id");
    f.name = strField(o, "name");
    f.description = strField(o, "description");
    if (o.has("pads"))
        for (std::size_t i = 0; i < o.get("pads").size(); ++i)
            f.pads.push_back(padFromJson(o.get("pads").at(i), scale, where));
    if (f.id.empty()) fail(where, "封装缺少 id");
    return f;
}

// PadNet
JsonValue padNetToJson(const PadNet& p) {
    JsonValue o = JsonValue::makeObject();
    o.set("pad", JsonValue::makeString(p.padNumber));
    o.set("net", JsonValue::makeString(p.netId));
    return o;
}
PadNet padNetFromJson(const JsonValue& o, const std::string& where) {
    PadNet p;
    p.padNumber = strField(o, "pad");
    p.netId = strField(o, "net");
    if (p.padNumber.empty()) fail(where, "焊盘网络映射缺少 pad");
    return p;
}

// Component
JsonValue componentToJson(const Component& c, double scale) {
    JsonValue o = JsonValue::makeObject();
    o.set("id",          JsonValue::makeString(c.id));
    o.set("footprintId", JsonValue::makeString(c.footprintId));
    o.set("reference",   JsonValue::makeString(c.reference));
    o.set("position",    pointToJson(c.position, scale));
    o.set("rotation",    JsonValue::makeNumber(c.rotation));
    o.set("onTop",       JsonValue::makeBool(c.onTop));
    JsonValue arr = JsonValue::makeArray();
    for (const auto& pn : c.padNets) arr.push(padNetToJson(pn));
    o.set("padNets", arr);
    return o;
}
Component componentFromJson(const JsonValue& o, double scale, const std::string& where) {
    Component c;
    c.id = strField(o, "id");
    c.footprintId = strField(o, "footprintId");
    c.reference = strField(o, "reference");
    c.position = pointFromJson(o.get("position"), scale, where);
    c.rotation = numField(o, "rotation");
    c.onTop = boolField(o, "onTop", true);
    if (o.has("padNets"))
        for (std::size_t i = 0; i < o.get("padNets").size(); ++i)
            c.padNets.push_back(padNetFromJson(o.get("padNets").at(i), where));
    if (c.id.empty()) fail(where, "元件缺少 id");
    return c;
}

// Trace
JsonValue traceToJson(const Trace& t, double scale) {
    JsonValue o = JsonValue::makeObject();
    o.set("id",      JsonValue::makeString(t.id));
    o.set("net",     JsonValue::makeString(t.netId));
    o.set("layer",   JsonValue::makeString(t.layerId));
    o.set("width",   JsonValue::makeNumber(t.width * scale));
    o.set("points",  pointsToJson(t.points, scale));
    return o;
}
Trace traceFromJson(const JsonValue& o, double scale, const std::string& where) {
    Trace t;
    t.id = strField(o, "id");
    t.netId = strField(o, "net");
    t.layerId = strField(o, "layer");
    t.width = numField(o, "width", 0.2) * scale;
    t.points = pointsFromJson(o.get("points"), scale, where);
    if (t.id.empty()) fail(where, "走线缺少 id");
    return t;
}

// Via
JsonValue viaToJson(const Via& v, double scale) {
    JsonValue o = JsonValue::makeObject();
    o.set("id",        JsonValue::makeString(v.id));
    o.set("net",       JsonValue::makeString(v.netId));
    o.set("position",  pointToJson(v.position, scale));
    o.set("diameter",  JsonValue::makeNumber(v.diameter * scale));
    o.set("drill",     JsonValue::makeNumber(v.drill * scale));
    o.set("fromLayer", JsonValue::makeString(v.fromLayer));
    o.set("toLayer",   JsonValue::makeString(v.toLayer));
    return o;
}
Via viaFromJson(const JsonValue& o, double scale, const std::string& where) {
    Via v;
    v.id = strField(o, "id");
    v.netId = strField(o, "net");
    v.position = pointFromJson(o.get("position"), scale, where);
    v.diameter = numField(o, "diameter", 0.6) * scale;
    v.drill = numField(o, "drill", 0.3) * scale;
    v.fromLayer = strField(o, "fromLayer");
    v.toLayer = strField(o, "toLayer");
    if (v.id.empty()) fail(where, "过孔缺少 id");
    return v;
}

// CopperPour
JsonValue pourToJson(const CopperPour& p, double scale) {
    JsonValue o = JsonValue::makeObject();
    o.set("id",      JsonValue::makeString(p.id));
    o.set("net",     JsonValue::makeString(p.netId));
    o.set("layer",   JsonValue::makeString(p.layerId));
    o.set("outline", pointsToJson(p.outline, scale));
    return o;
}
CopperPour pourFromJson(const JsonValue& o, double scale, const std::string& where) {
    CopperPour p;
    p.id = strField(o, "id");
    p.netId = strField(o, "net");
    p.layerId = strField(o, "layer");
    p.outline = pointsFromJson(o.get("outline"), scale, where);
    if (p.id.empty()) fail(where, "敷铜缺少 id");
    return p;
}

// Board
JsonValue boardToJson(const Board& b, double scale) {
    JsonValue o = JsonValue::makeObject();

    o.set("outline", pointsToJson(b.outline.points, scale));

    JsonValue lyr = JsonValue::makeArray();
    for (const auto& l : b.layers.layers()) lyr.push(layerToJson(l));
    o.set("layers", lyr);

    JsonValue grp = JsonValue::makeArray();
    for (const auto& g : b.layers.groups()) grp.push(groupToJson(g));
    o.set("layerGroups", grp);

    JsonValue nets = JsonValue::makeArray();
    for (const auto& n : b.nets.nets()) nets.push(netToJson(n));
    o.set("nets", nets);

    JsonValue fps = JsonValue::makeArray();
    for (const auto& f : b.footprints) fps.push(footprintToJson(f, scale));
    o.set("footprints", fps);

    JsonValue cmps = JsonValue::makeArray();
    for (const auto& c : b.components) cmps.push(componentToJson(c, scale));
    o.set("components", cmps);

    JsonValue trs = JsonValue::makeArray();
    for (const auto& t : b.traces) trs.push(traceToJson(t, scale));
    o.set("traces", trs);

    JsonValue vias = JsonValue::makeArray();
    for (const auto& v : b.vias) vias.push(viaToJson(v, scale));
    o.set("vias", vias);

    JsonValue pours = JsonValue::makeArray();
    for (const auto& p : b.copperPours) pours.push(pourToJson(p, scale));
    o.set("copperPours", pours);

    return o;
}
void boardFromJson(const JsonValue& o, double scale, Board& b) {
    b.outline.points = pointsFromJson(o.get("outline"), scale, "board.outline");

    if (o.has("layers"))
        for (std::size_t i = 0; i < o.get("layers").size(); ++i)
            b.layers.addLayer(layerFromJson(o.get("layers").at(i), "board.layers"));

    if (o.has("layerGroups"))
        for (std::size_t i = 0; i < o.get("layerGroups").size(); ++i)
            b.layers.addGroup(groupFromJson(o.get("layerGroups").at(i), "board.layerGroups"));

    if (o.has("nets"))
        for (std::size_t i = 0; i < o.get("nets").size(); ++i) {
            Net n = netFromJson(o.get("nets").at(i), "board.nets");
            if (!b.nets.addNet(n.name, n.id)) fail("board.nets", "网络 id 重复: " + n.id);
        }

    if (o.has("footprints"))
        for (std::size_t i = 0; i < o.get("footprints").size(); ++i)
            b.addFootprint(footprintFromJson(o.get("footprints").at(i), scale, "board.footprints"));

    if (o.has("components"))
        for (std::size_t i = 0; i < o.get("components").size(); ++i)
            b.addComponent(componentFromJson(o.get("components").at(i), scale, "board.components"));

    if (o.has("traces"))
        for (std::size_t i = 0; i < o.get("traces").size(); ++i)
            b.addTrace(traceFromJson(o.get("traces").at(i), scale, "board.traces"));

    if (o.has("vias"))
        for (std::size_t i = 0; i < o.get("vias").size(); ++i)
            b.addVia(viaFromJson(o.get("vias").at(i), scale, "board.vias"));

    if (o.has("copperPours"))
        for (std::size_t i = 0; i < o.get("copperPours").size(); ++i)
            b.addCopperPour(pourFromJson(o.get("copperPours").at(i), scale, "board.copperPours"));
}

} // namespace

// ==================== 保存 ====================
std::string ProjectIO::saveToString(const PcbProject& p) {
    double scale = unit::fromMm(1.0, p.unit); // mm -> 文件单位

    JsonValue root = JsonValue::makeObject();
    root.set("format",  JsonValue::makeString(kFormatTag));
    root.set("version", JsonValue::makeNumber(p.fileVersion));
    root.set("unit",    JsonValue::makeString(unit::name(p.unit)));

    JsonValue proj = JsonValue::makeObject();
    proj.set("name",        JsonValue::makeString(p.name));
    proj.set("description", JsonValue::makeString(p.description));
    root.set("project", proj);

    root.set("board", boardToJson(p.board, scale));
    return root.dump(true);
}

bool ProjectIO::saveToFile(const PcbProject& p, const std::string& path, std::string& err) {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs.is_open()) {
        err = "无法打开文件进行写入: " + path;
        return false;
    }
    ofs << saveToString(p);
    if (!ofs.good()) {
        err = "写入文件失败: " + path;
        return false;
    }
    return true;
}

// ==================== 载入 ====================
LoadResult ProjectIO::loadFromString(const std::string& text, PcbProject& out) {
    LoadResult r;
    try {
        JsonValue root = JsonValue::parse(text);
        if (root.type() != JsonValue::Type::Object)
            fail("root", "顶层必须是对象");

        if (!root.has("format") || root.get("format").asString() != kFormatTag)
            fail("root", "不是有效的 PCB 工程文件（format 标记不匹配）");

        // 版本
        int ver = static_cast<int>(root.get("version").asNumber(1));
        if (ver > kCurrentVersion)
            fail("root", "文件版本 " + std::to_string(ver) +
                 " 高于当前支持的版本 " + std::to_string(kCurrentVersion));
        if (ver < kCurrentVersion)
            r.warnings.push_back("文件为旧版本 v" + std::to_string(ver) +
                                 "，已按 v" + std::to_string(kCurrentVersion) + " 载入");

        // 单位
        Unit fileUnit = Unit::Millimeter;
        std::string unitStr = "mm";
        if (root.has("unit")) {
            unitStr = root.get("unit").asString();
            if (!unit::parse(unitStr, fileUnit))
                r.warnings.push_back("未知单位 \"" + unitStr + "\"，按 mm 处理");
        }
        double scale = unit::toMm(1.0, fileUnit); // 文件单位 -> mm

        out = PcbProject();
        out.fileVersion = ver;
        out.unit = fileUnit;

        if (root.has("project")) {
            const JsonValue& proj = root.get("project");
            out.name = strField(proj, "name");
            out.description = strField(proj, "description");
        }

        if (root.has("board"))
            boardFromJson(root.get("board"), scale, out.board);
        else
            fail("root", "缺少 board 字段");

        // 一致性告警（不视为致命错误）
        for (const auto& w : out.board.checkIntegrity())
            r.warnings.push_back(w);
    } catch (const JsonError& e) {
        r.ok = false;
        r.error = "JSON 语法错误（第 " + std::to_string(e.line) + " 行，第 " +
                  std::to_string(e.col) + " 列）: " + e.what();
    } catch (const std::exception& e) {
        r.ok = false;
        r.error = e.what();
    }
    return r;
}

LoadResult ProjectIO::loadFromFile(const std::string& path, PcbProject& out) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        LoadResult r;
        r.ok = false;
        r.error = "无法打开文件: " + path;
        return r;
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    if (ifs.bad()) {
        LoadResult r;
        r.ok = false;
        r.error = "读取文件失败: " + path;
        return r;
    }
    return loadFromString(ss.str(), out);
}

} // namespace pcb
