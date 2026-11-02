#pragma once
// =====================================================================
// 示例工程：覆盖板框/图层/层组/网络/封装/元件/走线/过孔/敷铜，
// 供测试程序与集成演示共用。
// =====================================================================
#include "Project.h"

namespace pcb {

inline PcbProject makeSampleProject() {
    PcbProject p;
    p.name = "Demo Board";
    p.description = "示例 PCB 工程";
    p.fileVersion = 1;
    p.unit = Unit::Millimeter;

    Board& b = p.board;

    // 板框 50 x 40 mm
    b.outline.points = { {0, 0}, {50, 0}, {50, 40}, {0, 40} };

    // 图层
    b.layers.addLayer(Layer{"F.Cu",   "Top Copper",     LayerType::Copper,     true, 10, Color{"#ff0000"}});
    b.layers.addLayer(Layer{"B.Cu",   "Bottom Copper",  LayerType::Copper,     true, 20, Color{"#0000ff"}});
    b.layers.addLayer(Layer{"F.Silk", "Top Silkscreen", LayerType::SilkScreen, true, 30, Color{"#ffffff"}});
    b.layers.addLayer(Layer{"Edge",   "Board Edge",     LayerType::Edge,       true, 40, Color{"#ffff00"}});

    // 层组
    b.layers.addGroup(LayerGroup{"Copper Layers", {"F.Cu", "B.Cu"}, true});
    b.layers.addGroup(LayerGroup{"Silk Layers", {"F.Silk"}, true});

    // 网络
    b.addNet("GND");
    b.addNet("VCC");
    b.addNet("SIG");

    // 封装 R0603
    Footprint fp;
    fp.id = "fp:R0603";
    fp.name = "R0603";
    fp.description = "0603 电阻封装";
    fp.pads = {
        Pad{"1", "", Point{-1.0, 0.0}, 0.9, 1.0, PadShape::Rect, 0.0},
        Pad{"2", "", Point{ 1.0, 0.0}, 0.9, 1.0, PadShape::Rect, 0.0},
    };
    b.addFootprint(fp);

    // 元件 R1（引用 R0603），1 脚接 GND，2 脚接 SIG
    Component c;
    c.id = "cmp:R1";
    c.footprintId = "fp:R0603";
    c.reference = "R1";
    c.position = Point{10.0, 10.0};
    c.padNets = { PadNet{"1", "net:GND"}, PadNet{"2", "net:SIG"} };
    b.addComponent(c);

    // 走线：SIG 网络，从 R1 的 2 脚附近到过孔
    Trace t;
    t.id = "trc:1";
    t.netId = "net:SIG";
    t.layerId = "F.Cu";
    t.width = 0.2;
    t.points = { Point{11.0, 10.0}, Point{20.0, 10.0} };
    b.addTrace(t);

    // 过孔
    Via v;
    v.id = "via:1";
    v.netId = "net:SIG";
    v.position = Point{20.0, 10.0};
    v.diameter = 0.6;
    v.drill = 0.3;
    v.fromLayer = "F.Cu";
    v.toLayer = "B.Cu";
    b.addVia(v);

    // 敷铜 GND（底层整板）
    CopperPour cp;
    cp.id = "pour:1";
    cp.netId = "net:GND";
    cp.layerId = "B.Cu";
    cp.outline = { {0, 0}, {50, 0}, {50, 40}, {0, 40} };
    b.addCopperPour(cp);

    return p;
}

} // namespace pcb
