#pragma once
// =====================================================================
// PCB 工程(PcbProject)：顶层文档对象。
// 包含工程元数据（名称、描述、文件版本、单位）与电路板数据。
// =====================================================================
#include "Board.h"
#include "Types.h"

namespace pcb {

class PcbProject {
public:
    std::string name;              // 工程名
    std::string description;
    int fileVersion = 1;           // 文件格式版本
    Unit unit = Unit::Millimeter;  // 单位信息（文件内坐标的统一单位）
    Board board;                   // 电路板数据

    bool operator==(const PcbProject& o) const {
        return name == o.name && description == o.description &&
               fileVersion == o.fileVersion && unit == o.unit &&
               board == o.board;
    }
    bool operator!=(const PcbProject& o) const { return !(*this == o); }
};

} // namespace pcb
