#pragma once
// =====================================================================
// 电路板(Board)：所有 PCB 数据的统一容器与访问接口（公共接口）。
// 统一管理板框、图层、网络、封装、元件实例、走线、过孔、敷铜，
// 并维护它们之间的关联关系（元件->封装、焊盘/走线/过孔/敷铜->网络）。
// =====================================================================
#include "Types.h"
#include "Layer.h"
#include "Net.h"
#include "Footprint.h"
#include <string>
#include <vector>
#include <algorithm>
#include <utility>

namespace pcb {

// ---------- 走线 ----------
struct Trace {
    std::string id;
    std::string netId;        // 走线与网络关系
    std::string layerId;      // 所在图层
    double width = 0.2;       // 线宽（mm）
    std::vector<Point> points; // 折线顶点

    bool operator==(const Trace& o) const {
        return id == o.id && netId == o.netId && layerId == o.layerId &&
               near(width, o.width) && points == o.points;
    }
};

// ---------- 过孔 ----------
struct Via {
    std::string id;
    std::string netId;
    Point position;
    double diameter = 0.6;    // 焊盘直径（mm）
    double drill = 0.3;       // 钻孔直径（mm）
    std::string fromLayer;    // 起始层，如 "F.Cu"
    std::string toLayer;      // 终止层，如 "B.Cu"

    bool operator==(const Via& o) const {
        return id == o.id && netId == o.netId && position == o.position &&
               near(diameter, o.diameter) && near(drill, o.drill) &&
               fromLayer == o.fromLayer && toLayer == o.toLayer;
    }
};

// ---------- 敷铜 ----------
struct CopperPour {
    std::string id;
    std::string netId;
    std::string layerId;
    std::vector<Point> outline; // 多边形区域
    std::vector<std::vector<Point>> holes; // 不同网络对象的避让孔
    double clearance = 0.25;    // 基础铜间距（mm）

    bool operator==(const CopperPour& o) const {
        return id == o.id && netId == o.netId && layerId == o.layerId &&
               outline == o.outline && holes == o.holes &&
               near(clearance, o.clearance);
    }
};

// ---------- 板框 ----------
struct BoardOutline {
    std::vector<Point> points; // 闭合多边形（mm）
    bool operator==(const BoardOutline& o) const { return points == o.points; }
};

// 焊盘与网络的查询结果（用于「焊盘与网络」关联查询）
struct NetPadRef {
    std::string componentId;
    std::string reference;
    std::string padNumber;
};

// ---------- 电路板 ----------
class Board {
public:
    LayerManager layers;
    NetTable nets;
    std::vector<Footprint> footprints;
    std::vector<Component> components;
    std::vector<Trace> traces;
    std::vector<Via> vias;
    std::vector<CopperPour> copperPours;
    BoardOutline outline;

    // ============ 封装 ============
    Footprint* addFootprint(Footprint f) {
        if (findFootprint(f.id)) return nullptr;
        footprints.push_back(std::move(f));
        return &footprints.back();
    }
    Footprint* findFootprint(const std::string& id) {
        auto it = std::find_if(footprints.begin(), footprints.end(),
            [&](const Footprint& f) { return f.id == id; });
        return it == footprints.end() ? nullptr : &*it;
    }
    const Footprint* findFootprint(const std::string& id) const {
        return const_cast<Board*>(this)->findFootprint(id);
    }
    // 若封装被元件引用则不允许删除（维护「元件与封装」关系一致性）
    bool removeFootprint(const std::string& id) {
        for (const auto& c : components)
            if (c.footprintId == id) return false;
        auto it = std::find_if(footprints.begin(), footprints.end(),
            [&](const Footprint& f) { return f.id == id; });
        if (it == footprints.end()) return false;
        footprints.erase(it);
        return true;
    }

    // ============ 元件实例 ============
    Component* addComponent(Component c) {
        if (findComponent(c.id)) return nullptr;
        components.push_back(std::move(c));
        return &components.back();
    }
    Component* findComponent(const std::string& id) {
        auto it = std::find_if(components.begin(), components.end(),
            [&](const Component& c) { return c.id == id; });
        return it == components.end() ? nullptr : &*it;
    }
    const Component* findComponent(const std::string& id) const {
        return const_cast<Board*>(this)->findComponent(id);
    }
    Component* findByReference(const std::string& ref) {
        auto it = std::find_if(components.begin(), components.end(),
            [&](const Component& c) { return c.reference == ref; });
        return it == components.end() ? nullptr : &*it;
    }
    bool removeComponent(const std::string& id) {
        auto it = std::find_if(components.begin(), components.end(),
            [&](const Component& c) { return c.id == id; });
        if (it == components.end()) return false;
        components.erase(it);
        return true;
    }
    // 元件与封装关联查询
    const Footprint* footprintOfComponent(const Component& c) const {
        return findFootprint(c.footprintId);
    }
    std::vector<const Component*> componentsUsingFootprint(const std::string& fpId) const {
        std::vector<const Component*> out;
        for (const auto& c : components)
            if (c.footprintId == fpId) out.push_back(&c);
        return out;
    }

    // ============ 走线 ============
    Trace* addTrace(Trace t) {
        if (findTrace(t.id)) return nullptr;
        traces.push_back(std::move(t));
        return &traces.back();
    }
    Trace* findTrace(const std::string& id) {
        auto it = std::find_if(traces.begin(), traces.end(),
            [&](const Trace& t) { return t.id == id; });
        return it == traces.end() ? nullptr : &*it;
    }
    const Trace* findTrace(const std::string& id) const {
        return const_cast<Board*>(this)->findTrace(id);
    }
    bool removeTrace(const std::string& id) {
        auto it = std::find_if(traces.begin(), traces.end(),
            [&](const Trace& t) { return t.id == id; });
        if (it == traces.end()) return false;
        traces.erase(it);
        return true;
    }
    std::vector<const Trace*> tracesOfNet(const std::string& netId) const {
        std::vector<const Trace*> out;
        for (const auto& t : traces)
            if (t.netId == netId) out.push_back(&t);
        return out;
    }

    // ============ 过孔 ============
    Via* addVia(Via v) {
        if (findVia(v.id)) return nullptr;
        vias.push_back(std::move(v));
        return &vias.back();
    }
    Via* findVia(const std::string& id) {
        auto it = std::find_if(vias.begin(), vias.end(),
            [&](const Via& v) { return v.id == id; });
        return it == vias.end() ? nullptr : &*it;
    }
    const Via* findVia(const std::string& id) const {
        return const_cast<Board*>(this)->findVia(id);
    }
    bool removeVia(const std::string& id) {
        auto it = std::find_if(vias.begin(), vias.end(),
            [&](const Via& v) { return v.id == id; });
        if (it == vias.end()) return false;
        vias.erase(it);
        return true;
    }
    std::vector<const Via*> viasOfNet(const std::string& netId) const {
        std::vector<const Via*> out;
        for (const auto& v : vias)
            if (v.netId == netId) out.push_back(&v);
        return out;
    }

    // ============ 敷铜 ============
    CopperPour* addCopperPour(CopperPour p) {
        if (findCopperPour(p.id)) return nullptr;
        copperPours.push_back(std::move(p));
        return &copperPours.back();
    }
    CopperPour* findCopperPour(const std::string& id) {
        auto it = std::find_if(copperPours.begin(), copperPours.end(),
            [&](const CopperPour& p) { return p.id == id; });
        return it == copperPours.end() ? nullptr : &*it;
    }
    const CopperPour* findCopperPour(const std::string& id) const {
        return const_cast<Board*>(this)->findCopperPour(id);
    }
    bool removeCopperPour(const std::string& id) {
        auto it = std::find_if(copperPours.begin(), copperPours.end(),
            [&](const CopperPour& p) { return p.id == id; });
        if (it == copperPours.end()) return false;
        copperPours.erase(it);
        return true;
    }
    std::vector<const CopperPour*> poursOfNet(const std::string& netId) const {
        std::vector<const CopperPour*> out;
        for (const auto& p : copperPours)
            if (p.netId == netId) out.push_back(&p);
        return out;
    }

    // ============ 网络（含引用清理） ============
    Net* addNet(const std::string& name, const std::string& id = "") {
        return nets.addNet(name, id);
    }
    Net* findNetByName(const std::string& name) { return nets.findByName(name); }
    const Net* findNetByName(const std::string& name) const { return nets.findByName(name); }
    Net* findNetById(const std::string& id) { return nets.findById(id); }
    const Net* findNetById(const std::string& id) const { return nets.findById(id); }
    // 删除网络，并清除所有指向它的引用（保证电气数据一致）
    bool removeNet(const std::string& id) {
        if (!nets.removeNet(id)) return false;
        for (auto& c : components)
            c.padNets.erase(std::remove_if(c.padNets.begin(), c.padNets.end(),
                [&](const PadNet& p) { return p.netId == id; }),
                c.padNets.end());
        traces.erase(std::remove_if(traces.begin(), traces.end(),
            [&](const Trace& t) { return t.netId == id; }), traces.end());
        vias.erase(std::remove_if(vias.begin(), vias.end(),
            [&](const Via& v) { return v.netId == id; }), vias.end());
        copperPours.erase(std::remove_if(copperPours.begin(), copperPours.end(),
            [&](const CopperPour& p) { return p.netId == id; }), copperPours.end());
        return true;
    }

    // ============ 焊盘与网络关联查询 ============
    std::vector<NetPadRef> padsOfNet(const std::string& netId) const {
        std::vector<NetPadRef> out;
        for (const auto& c : components)
            for (const auto& pn : c.padNets)
                if (pn.netId == netId)
                    out.push_back(NetPadRef{c.id, c.reference, pn.padNumber});
        return out;
    }

    // ============ 数据一致性检查 ============
    // 返回所有「悬空引用」问题的描述；空向量表示数据一致。
    std::vector<std::string> checkIntegrity() const {
        std::vector<std::string> problems;
        for (const auto& c : components) {
            if (!findFootprint(c.footprintId))
                problems.push_back("元件 " + c.reference + " 引用了不存在的封装 " + c.footprintId);
            for (const auto& pn : c.padNets)
                if (!nets.findById(pn.netId))
                    problems.push_back("元件 " + c.reference + " 的焊盘 " + pn.padNumber +
                                       " 引用了不存在的网络 " + pn.netId);
        }
        for (const auto& t : traces) {
            if (!nets.findById(t.netId))
                problems.push_back("走线 " + t.id + " 引用了不存在的网络 " + t.netId);
            if (!layers.findLayer(t.layerId))
                problems.push_back("走线 " + t.id + " 引用了不存在的图层 " + t.layerId);
        }
        for (const auto& v : vias)
            if (!nets.findById(v.netId))
                problems.push_back("过孔 " + v.id + " 引用了不存在的网络 " + v.netId);
        for (const auto& p : copperPours) {
            if (!nets.findById(p.netId))
                problems.push_back("敷铜 " + p.id + " 引用了不存在的网络 " + p.netId);
            if (!layers.findLayer(p.layerId))
                problems.push_back("敷铜 " + p.id + " 引用了不存在的图层 " + p.layerId);
        }
        return problems;
    }

    bool operator==(const Board& o) const {
        return layers == o.layers && nets == o.nets &&
               footprints == o.footprints && components == o.components &&
               traces == o.traces && vias == o.vias &&
               copperPours == o.copperPours && outline == o.outline;
    }
};

} // namespace pcb
