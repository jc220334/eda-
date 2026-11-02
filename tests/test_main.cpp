// =====================================================================
// 测试程序：验证数据模型、文件读写前后一致性、关联关系维护与异常处理。
// 全部通过返回 0，任一失败返回 1。
// =====================================================================
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/Project.h"
#include "core/Sample.h"
#include "io/ProjectIO.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace pcb;

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const std::string& msg) {
    if (cond) { ++g_pass; std::printf("[通过] %s\n", msg.c_str()); }
    else      { ++g_fail; std::printf("[失败] %s\n", msg.c_str()); }
}

// 测试 1：单位换算
static void testUnits() {
    check(near(unit::toMm(1.0, Unit::Mil), 0.0254), "1 mil == 0.0254 mm");
    check(near(unit::toMm(1.0, Unit::Inch), 25.4), "1 inch == 25.4 mm");
    check(near(unit::fromMm(25.4, Unit::Inch), 1.0), "25.4 mm == 1 inch");
    Unit u;
    check(unit::parse("mil", u) && u == Unit::Mil, "解析单位 mil");
    check(!unit::parse("foo", u), "解析未知单位应失败");
}

// 测试 2：图层管理（创建/命名/排序/显示隐藏/层组）
static void testLayers() {
    LayerManager lm;
    check(lm.addLayer(Layer{"F.Cu", "Top", LayerType::Copper, true, 10, Color{"#ff0000"}}),
          "创建图层 F.Cu");
    check(!lm.addLayer(Layer{"F.Cu", "dup", LayerType::Copper, true, 0, Color{}}),
          "重复 id 创建应失败");
    lm.addLayer(Layer{"B.Cu", "Bottom", LayerType::Copper, true, 20, Color{"#0000ff"}});
    lm.addLayer(Layer{"Edge", "Edge", LayerType::Edge, true, 5, Color{}});
    check(lm.layers().size() == 3, "图层数量为 3");

    lm.sortByOrder();
    check(lm.layers()[0].id == "Edge", "按 order 排序后首位为 Edge");

    check(lm.setVisible("F.Cu", false) && !lm.findLayer("F.Cu")->visible, "隐藏图层 F.Cu");
    check(lm.addGroup(LayerGroup{"Copper", {"F.Cu", "B.Cu"}, true}), "创建层组 Copper");
    check(lm.findGroup("Copper") != nullptr, "查找层组 Copper");

    check(lm.removeLayer("F.Cu"), "删除图层 F.Cu");
    bool stillInGroup = false;
    for (const auto& id : lm.findGroup("Copper")->layerIds)
        if (id == "F.Cu") stillInGroup = true;
    check(!stillInGroup, "删除图层后层组内引用同步移除");
}

// 测试 3：完整工程 字符串保存/载入往返
static void testRoundTripString() {
    PcbProject p = makeSampleProject();
    std::string s = ProjectIO::saveToString(p);
    PcbProject q;
    LoadResult r = ProjectIO::loadFromString(s, q);
    check(r.ok, "字符串载入成功");
    check(q == p, "字符串往返后数据完全一致");
    check(q.board.checkIntegrity().empty(), "示例工程无悬空引用");
}

// 测试 4：完整工程 文件保存/载入往返
static void testRoundTripFile() {
    PcbProject p = makeSampleProject();
    const std::string path = "test_roundtrip.pcbproj";
    std::string err;
    check(ProjectIO::saveToFile(p, path, err), "保存到文件成功");
    PcbProject q;
    LoadResult r = ProjectIO::loadFromFile(path, q);
    check(r.ok, "从文件载入成功");
    check(q == p, "文件往返后数据完全一致");
    std::remove(path.c_str());
}

// 测试 5：单位信息（mil 往返，坐标换算）
static void testUnitRoundTrip() {
    PcbProject p = makeSampleProject();
    p.unit = Unit::Mil;
    std::string s = ProjectIO::saveToString(p);
    PcbProject q;
    LoadResult r = ProjectIO::loadFromString(s, q);
    check(r.ok, "mil 单位文件载入成功");
    check(q.unit == Unit::Mil, "载入后单位字段为 mil");
    check(q == p, "mil 往返后坐标已换算回 mm 且一致");
}

// 测试 6：关联关系查询
static void testRelations() {
    PcbProject p = makeSampleProject();
    Board& b = p.board;

    const Component* r1 = b.findByReference("R1");
    check(r1 != nullptr, "按位号找到 R1");
    const Footprint* fp = r1 ? b.footprintOfComponent(*r1) : nullptr;
    check(fp && fp->name == "R0603", "元件 R1 关联封装 R0603");
    check(b.componentsUsingFootprint("fp:R0603").size() == 1, "使用 R0603 的元件数量为 1");

    auto pads = b.padsOfNet("net:GND");
    check(pads.size() == 1 && pads[0].padNumber == "1", "GND 网络包含 R1 的 1 号焊盘");
    check(b.tracesOfNet("net:SIG").size() == 1, "SIG 网络有 1 条走线");
    check(b.viasOfNet("net:SIG").size() == 1, "SIG 网络有 1 个过孔");
    check(b.poursOfNet("net:GND").size() == 1, "GND 网络有 1 块敷铜");
}

// 测试 7：删除网络后清理引用（电气数据一致性）
static void testRemoveNet() {
    PcbProject p = makeSampleProject();
    Board& b = p.board;
    check(b.removeNet("net:SIG"), "删除网络 SIG");
    check(b.findNetById("net:SIG") == nullptr, "网络 SIG 已不存在");
    check(b.tracesOfNet("net:SIG").empty() && b.viasOfNet("net:SIG").empty(),
          "SIG 的走线/过孔引用已清理");
    bool padStillRefs = false;
    for (const auto& c : b.components)
        for (const auto& pn : c.padNets)
            if (pn.netId == "net:SIG") padStillRefs = true;
    check(!padStillRefs, "元件焊盘对 SIG 的引用已清理");
}

// 测试 8：删除被引用封装应被拒绝（元件与封装关系）
static void testRemoveFootprint() {
    PcbProject p = makeSampleProject();
    Board& b = p.board;
    check(!b.removeFootprint("fp:R0603"), "被元件引用的封装不可删除");
    b.removeComponent("cmp:R1");
    check(b.removeFootprint("fp:R0603"), "删除元件后可删除封装");
}

// 测试 9：异常文件处理
static void testErrorHandling() {
    PcbProject out;
    LoadResult r;

    r = ProjectIO::loadFromString("{ 这不是合法 JSON", out);
    check(!r.ok, "语法错误被识别并报错");

    r = ProjectIO::loadFromString("{\"format\":\"WRONG\",\"version\":1,\"board\":{}}", out);
    check(!r.ok, "format 标记不匹配被识别");

    r = ProjectIO::loadFromString("{\"format\":\"PCB-Project\",\"version\":99,\"board\":{}}", out);
    check(!r.ok, "过高版本被拒绝");

    r = ProjectIO::loadFromString("{\"format\":\"PCB-Project\",\"version\":1,\"unit\":\"mm\"}", out);
    check(!r.ok, "缺少 board 字段被识别");

    std::string old = "{\"format\":\"PCB-Project\",\"version\":0,\"unit\":\"mm\","
                      "\"board\":{\"outline\":[]}}";
    r = ProjectIO::loadFromString(old, out);
    check(r.ok, "旧版本文件仍可载入");
    bool hasWarn = false;
    for (const auto& w : r.warnings)
        if (w.find("旧版本") != std::string::npos) hasWarn = true;
    check(hasWarn, "旧版本文件产生告警");
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::printf("===== PCB 核心数据模型与持久化 测试 =====\n\n");

    testUnits();
    testLayers();
    testRoundTripString();
    testRoundTripFile();
    testUnitRoundTrip();
    testRelations();
    testRemoveNet();
    testRemoveFootprint();
    testErrorHandling();

    std::printf("\n===== 结果：%d 通过，%d 失败 =====\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
