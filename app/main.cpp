// =====================================================================
// 集成演示：创建 PCB 工程 -> 保存 -> 重新打开 -> 恢复数据并校验一致性。
// =====================================================================
#include <cstdio>
#include <string>

#include "core/Project.h"
#include "core/Sample.h"
#include "io/ProjectIO.h"

#ifdef _WIN32
#include <windows.h>
#endif

using namespace pcb;

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::printf("===== PCB 工程核心数据管理与持久化 演示 =====\n\n");

    // 1. 创建工程
    PcbProject proj = makeSampleProject();
    std::printf("[1] 创建工程 \"%s\"（单位 %s，文件版本 v%d）\n",
                proj.name.c_str(), unit::name(proj.unit), proj.fileVersion);

    // 2. 数据规模与关联关系
    Board& b = proj.board;
    std::printf("[2] 数据规模：图层 %zu、层组 %zu、网络 %zu、封装 %zu、"
                "元件 %zu、走线 %zu、过孔 %zu、敷铜 %zu\n",
                b.layers.layers().size(), b.layers.groups().size(),
                b.nets.nets().size(), b.footprints.size(), b.components.size(),
                b.traces.size(), b.vias.size(), b.copperPours.size());

    if (const Component* r1 = b.findByReference("R1")) {
        const Footprint* fp = b.footprintOfComponent(*r1);
        std::printf("    关联：元件 %s -> 封装 %s\n",
                    r1->reference.c_str(), fp ? fp->name.c_str() : "(缺失)");
        for (const auto& pn : r1->padNets)
            std::printf("    关联：焊盘 %s -> 网络 %s\n",
                        pn.padNumber.c_str(), pn.netId.c_str());
    }

    // 3. 保存
    const std::string path = "demo.pcbproj";
    std::string err;
    if (!ProjectIO::saveToFile(proj, path, err)) {
        std::printf("[3] 保存失败：%s\n", err.c_str());
        return 1;
    }
    std::printf("[3] 已保存到 %s\n", path.c_str());

    // 4. 重新打开
    PcbProject reloaded;
    LoadResult r = ProjectIO::loadFromFile(path, reloaded);
    if (!r.ok) {
        std::printf("[4] 载入失败：%s\n", r.error.c_str());
        return 1;
    }
    std::printf("[4] 已重新打开（%zu 条告警）\n", r.warnings.size());
    for (const auto& w : r.warnings)
        std::printf("    - %s\n", w.c_str());

    // 5. 一致性校验
    std::printf("[5] 数据一致性检查：%s\n",
                reloaded.board.checkIntegrity().empty() ? "通过（无悬空引用）" : "存在问题");

    // 6. 完整性确认
    std::printf("[6] 重新打开后数据与保存前：%s\n",
                reloaded == proj ? "完全一致" : "不一致（错误）");

    std::printf("\n演示完成。\n");
    return reloaded == proj ? 0 : 1;
}
