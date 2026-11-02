#pragma once
// =====================================================================
// 封装与元件：焊盘(Pad)、封装(Footprint)、元件实例(Component)
// 关联关系：
//   - 元件 -> 封装    （Component.footprintId -> Footprint.id）
//   - 焊盘 -> 网络    （Component.padNets[i].netId -> Net.id）
// =====================================================================
#include "Types.h"
#include <string>
#include <vector>

namespace pcb {

// ---------- 焊盘 ----------
struct Pad {
    std::string number;    // 焊盘编号 "1" / "2" ...
    std::string name;      // 可选名称
    Point pos;             // 在封装内的坐标（mm）
    double width = 0.0;    // mm
    double height = 0.0;   // mm
    PadShape shape = PadShape::Rect;
    double drill = 0.0;    // 钻孔直径（mm），0 表示 SMD 无钻孔

    bool operator==(const Pad& o) const {
        return number == o.number && name == o.name && pos == o.pos &&
               near(width, o.width) && near(height, o.height) &&
               shape == o.shape && near(drill, o.drill);
    }
};

// ---------- 封装（元件库中的标准封装） ----------
struct Footprint {
    std::string id;           // 唯一标识
    std::string name;         // "R0603" / "SOP-8"
    std::string description;
    std::vector<Pad> pads;

    bool operator==(const Footprint& o) const {
        return id == o.id && name == o.name &&
               description == o.description && pads == o.pads;
    }
};

// 焊盘编号 -> 网络 id 的映射项
struct PadNet {
    std::string padNumber;
    std::string netId;
    bool operator==(const PadNet& o) const {
        return padNumber == o.padNumber && netId == o.netId;
    }
};

// ---------- 元件实例（放置到板上的元件） ----------
struct Component {
    std::string id;             // 唯一标识
    std::string footprintId;    // 关联的封装（元件与封装关系）
    std::string reference;      // 位号 "R1" / "U2"
    Point position;             // 放置位置（mm）
    double rotation = 0.0;      // 旋转角（度）
    bool onTop = true;          // true=顶层，false=底层
    bool locked = false;        // 是否禁止交互式移动
    std::vector<PadNet> padNets; // 焊盘与网络的关系

    bool operator==(const Component& o) const {
        return id == o.id && footprintId == o.footprintId &&
               reference == o.reference && position == o.position &&
               near(rotation, o.rotation) && onTop == o.onTop && locked == o.locked &&
               padNets == o.padNets;
    }
};

} // namespace pcb
