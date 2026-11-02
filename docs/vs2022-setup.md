# Visual Studio 2022 开发约定

项目采用 Visual Studio 2022、C++17 和 CMake。当前阶段只包含网表格式、样例和模型设计，源码实现开始后继续使用相同的 64 位工具链。

## 1. 本机已确认环境

| 项目 | 当前路径或版本 |
| --- | --- |
| Visual Studio 2022 | `D:\2022` |
| MSVC 64 位工具链 | `D:\2022\VC` |
| Visual Studio 自带 CMake | `D:\2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` |
| Visual Studio 自带 Ninja | `D:\2022\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe` |
| wxWidgets | `D:\wxwidgets` |
| wxWidgets 版本 | 3.2.2 |
| wxWidgets MSVC x64 库 | `D:\wxwidgets\lib\vc_x64_lib` |
| wxFormBuilder | `D:\wxFormBuilder` |

## 2. 固定工具链

- 操作系统：Windows 10/11 x64。
- C++ 标准：C++17。
- 编译器：MSVC v143 x64。
- 构建系统：CMake。
- 生成器：`Visual Studio 17 2022`，架构 `x64`。
- GUI：wxWidgets 3.2.2。
- 绘图：`wxGraphicsContext` 配合双缓冲绘制。

现有 wxWidgets 库使用 MSVC 编译。不得改用本机 `g++` 链接这些 `.lib` 文件。

## 3. Visual Studio 使用方式

源码阶段使用 Visual Studio 的“打开本地文件夹”功能打开仓库根目录，由 CMake 负责配置，不手工维护 `.sln` 和 `.vcxproj`。

配置目标：

| 配置 | 架构 | 用途 |
| --- | --- | --- |
| Debug | x64 | 日常开发、断言和调试 |
| Release | x64 | 最终演示和性能检查 |

推荐在仓库根目录加入 `CMakePresets.json`，包含：

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "vs2022-x64",
      "displayName": "Visual Studio 2022 x64",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build/vs2022-x64"
    }
  ]
}
```

该文件应在 `CMakeLists.txt` 和首个 C++ 目标建立时一并加入，避免引用不存在的目标。

## 4. 计划中的 CMake 目标

| 目标 | 负责人 | 内容 |
| --- | --- | --- |
| `eda_core` | 成员 A | 模型、坐标、文件读写 |
| `eda_ui` | 成员 B | wxWidgets 窗口、通用画布和编辑工具 |
| `eda_pcb` | 成员 C | 网表、布局、布线、过孔和敷铜 |
| `eda_app` | 全体 | 程序入口和模块装配 |
| `eda_tests` | 全体 | 模型、导入和 PCB 业务测试 |

依赖方向必须保持单向：

```text
eda_ui  ─┐
         ├──> eda_core
eda_pcb ─┘

eda_app ───> eda_ui, eda_pcb, eda_core
```

核心模型和 PCB 算法不得依赖 wxWidgets 控件。界面通过服务接口调用模型和业务模块。

## 5. 忽略文件

以下内容不得提交到 Git：

- `.vs/`
- `build/`
- `out/`
- `CMakeUserPresets.json`
- `*.user`
- `*.suo`
- 程序运行生成的临时工程和日志

## 6. 编码和格式

- 源文件使用 UTF-8。
- C++ 源文件推荐 LF 换行，由 Git 在 Windows 工作区自动转换。
- 使用 `.clang-format` 统一格式，建议基于 Microsoft 风格、4 空格缩进、行宽 100。
- 头文件使用 `#pragma once`。
- 公共类型使用 PascalCase，成员变量使用 `m_` 前缀，公共方法使用 PascalCase。
- 头文件不写 `using namespace`。

## 7. 验证顺序

每次提交前至少执行：

1. 使用 VS2022 x64 配置重新构建。
2. 运行全部 CTest 测试。
3. 打开并通过样例网表验证元件和引脚匹配。
4. 保存并重新加载 PCB 工程，比较元件、网络和连接数量。
5. 确认没有提交 `build/`、`.vs/` 和本地路径配置。
