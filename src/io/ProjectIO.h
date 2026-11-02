#pragma once
// =====================================================================
// PCB 工程文件读写：保存/载入、版本与单位处理、异常文件处理。
// 文件格式：JSON（扩展名建议 .pcbproj），顶层含 format/version/unit。
// =====================================================================
#include <string>
#include <vector>
#include "core/Project.h"

namespace pcb {

// 载入结果
struct LoadResult {
    bool ok = true;                      // 是否成功
    std::vector<std::string> warnings;   // 非致命告警
    std::string error;                   // 致命错误（ok==false 时有效）
};

class ProjectIO {
public:
    static constexpr int kCurrentVersion = 1;   // 当前文件格式版本
    static constexpr const char* kFormatTag = "PCB-Project";

    // 保存为 JSON 字符串
    static std::string saveToString(const PcbProject& p);
    // 保存到文件；失败时返回 false 并写入 err
    static bool saveToFile(const PcbProject& p, const std::string& path, std::string& err);
    // 从文件载入
    static LoadResult loadFromFile(const std::string& path, PcbProject& out);
    // 从字符串载入
    static LoadResult loadFromString(const std::string& text, PcbProject& out);
};

} // namespace pcb
