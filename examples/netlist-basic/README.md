# 基础网表样例

本目录提供成员 C 在第一周使用的一组完整设计样例，用于评审网表格式、封装匹配和 PCB 模型。

| 文件 | 作用 |
| --- | --- |
| `sample.net.json` | 输入的逻辑网表，包含 4 个元件和 4 个网络 |
| `sample.lib.json` | 网表引用封装的最小元件库 |
| `sample.board.json` | 成功导入并完成初始摆放后的 PCB 工程参考 |

对应 Schema：

- [`netlist.schema.json`](../../schemas/netlist.schema.json)
- [`footprint-library.schema.json`](../../schemas/footprint-library.schema.json)
- [`board.schema.json`](../../schemas/board.schema.json)

## 样例说明

`sample.net.json` 包含：

- `U1`：SOIC-8，器件值 `74HC00`。
- `R1`：0805，电阻值 `10k`。
- `R2`：0805，电阻值 `1k`。
- `C1`：0603，电容值 `100nF`。
- 网络：`VCC`、`GND`、`SIG_IN`、`SIG_OUT`。

`U1` 的 6 号引脚不参与任何网络，用来验证“未列出的引脚保持未连接”的规则。`GND` 同时连接 `U1` 的 4、7 号引脚，用来验证同一元件存在多个网络连接。

`sample.board.json` 使用 60 x 40 mm 板框，内部坐标单位为微米。四个元件均有稳定实例 ID，连接表共包含 13 个引脚绑定。工程中暂不放置走线、过孔和敷铜，便于后续单独验证手动布线和网络检查。

## 预期导入结果

如果实现正确，导入 `sample.net.json` 后应得到：

| 检查项 | 预期值 |
| --- | --- |
| 元件数量 | 4 |
| 网络数量 | 4 |
| 成功匹配的元件引脚连接 | 13 |
| 未参与网络的 `U1` 引脚 | 6 号 |
| 无法匹配的封装 | 0 |
| 无法匹配的引脚 | 0 |

## 需要准备的错误样例

正式实现解析器时，从有效样例派生以下测试文件，不要手工维护与主样例无关的网表：

| 派生方式 | 预期错误 |
| --- | --- |
| 将 `R1` 的封装改为不存在的 ID | `NET-008` |
| 将 `U1` 的某个连接改为 9 号引脚 | `NET-009` |
| 增加第二个位号为 `U1` 的元件 | `NET-004` |
| 把 `U1.1` 同时放入两个网络 | `NET-010` |
| 将 `U1.6` 单独放入只有一个连接的网络 | `NET-006` |

## Visual Studio 2022 使用

当前阶段是格式和模型设计。源码开始后，用 Visual Studio 2022 打开仓库根目录，并选择 x64 配置。工具链和路径约定见 [`docs/vs2022-setup.md`](../../docs/vs2022-setup.md)。

不得用本机 MinGW `g++` 链接 `D:\wxwidgets\lib\vc_x64_lib` 中的 MSVC 库。

在仓库根目录运行以下命令，可检查三个样例的结构及交叉引用：

```powershell
python tools/validate_design.py
```

成功时应输出：

```text
Library footprints: 3
Netlist components: 4
Netlist nets: 4
Matched pin connections: 13
Design samples: OK
```
