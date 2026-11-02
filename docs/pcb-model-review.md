# PCB 模型评审与边界约定

本文用于评价网表导入所需的 PCB 公共模型是否足够，并为成员 A、B、C 之间的接口提供统一约定。

## 1. 评审结论

模型采用“元件库模板 + PCB 实例 + 网络绑定”的三层结构：

```text
Footprint Library
    └── Footprint
          ├── FootprintPad[]
          └── FootprintGraphic[]

Board
    ├── ComponentInstance[] -> 引用 Footprint
    ├── Connection[] -> 实例引脚绑定到网络
    ├── Net[]
    ├── Track[]
    ├── Via[]
    └── CopperZone[]
```

PCB 不复制一份封装焊盘几何。板上的焊盘位置由“封装局部几何 + 元件实例变换”计算得到。

## 2. 单位和标识

### 2.1 坐标

- 当前 C++ 核心模型统一使用 `double` 毫米。
- 外部封装库 JSON 使用整数微米，导入时除以 1000 转换为毫米。
- PCB 工程文件默认保存毫米，也可按工程单位保存为 mil 或 inch。
- 显示层可转换为屏幕像素，但模型本身不保存像素坐标。
- 导出其他格式前再执行单位换算。

### 2.2 角度

- 元件朝向第一版只允许 0、90、180、270 度。
- 文件字段使用 `rotation_mdeg`，即 0.001 度。
- 圆弧角度也使用 `start_mdeg`、`end_mdeg`。
- 角度以逆时针为正方向。

### 2.3 实体 ID

- 元件实例、网络、走线、过孔、敷铜和封装引用均有稳定字符串 ID。
- ID 在单个工程或元件库内唯一。
- 不使用内存地址或数组下标作为持久化标识。
- 位号和引脚编号用于人机匹配，实体 ID 用于模型引用。

## 3. 元件库模型

### 3.1 Footprint

建议字段：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 稳定封装标识 |
| `name` | string | 用户可读名称 |
| `category` | string | 类别，例如 `IC`、`Resistor` |
| `description` | string | 说明 |
| `origin` | PointMm | 封装局部原点 |
| `pads` | Pad[] | 焊盘或安装孔 |
| `graphics` | Graphic[] | 丝印、文本等非焊接图形 |

### 3.2 FootprintPad

建议字段：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 封装内唯一焊盘 ID |
| `pin` | string/null | 对应电气引脚；安装孔可以为空 |
| `name` | string | 显示名称 |
| `shape` | enum | `rect`、`circle`、`oval`、`polygon` |
| `center` | PointMm | 相对封装原点的位置 |
| `size` | SizeUm | 外接尺寸 |
| `rotation_mdeg` | int | 局部旋转 |
| `layer_set` | string[] | 单层铜或 `*.Cu` |
| `mount` | enum | `smd`、`plated_through`、`non_plated` |
| `drill_um` | integer | 钻孔直径，无孔时为 0 |
| `solder_mask_expand_um` | integer | 阻焊开窗扩展量 |
| `copper_clearance_um` | integer | 默认铜间距 |
| `outline` | PointMm[] | `polygon` 焊盘轮廓，可省略 |

多个焊盘可以共享同一个 `pin`，因此不能假设“引脚编号等于焊盘 ID”。

### 3.3 FootprintGraphic

第一版支持：

- `line`：起点、终点、线宽。
- `polyline`：至少两个点、线宽。
- `circle`：圆心、半径、线宽、是否填充。
- `arc`：圆心、半径、起止角度、线宽。
- `text`：内容、位置、字高、旋转角。

每个图形必须记录所属层。元件库中的丝印不包含具体元件位号，位号由 PCB 实例在显示时生成。

## 4. PCB 工程模型

### 4.1 Board

建议字段：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 工程 ID |
| `name` | string | 工程名 |
| `outline` | PointMm[] | 闭合板框，首尾点相同，不允许自交 |
| `layers` | LayerDefinition[] | 工程层定义 |
| `layerGroups` | LayerGroup[] | 层组关系 |
| `components` | ComponentInstance[] | 已放置元件 |
| `nets` | Net[] | 电气网络 |
| `connections` | Connection[] | 实例引脚到网络的绑定 |
| `tracks` | TrackSegment[] | 走线 |
| `vias` | Via[] | 过孔 |
| `copperZones` | CopperZone[] | 敷铜区域 |

### 4.2 ComponentInstance

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 板上唯一元件实例 ID |
| `ref` | string | 唯一位号 |
| `footprint` | string | 封装库 ID |
| `position` | PointMm | 封装原点在板上的位置 |
| `rotation_mdeg` | int | 元件旋转 |
| `side` | enum | `top` 或 `bottom` |
| `locked` | boolean | 是否禁止移动 |
| `value` | string | 可选的器件值 |

元件实例只引用封装，不保存焊盘几何副本。移动和旋转元件后，板上焊盘通过统一变换函数重新计算。

### 4.3 Connection

连接表把“板上的具体引脚”绑定到网络：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `component` | string | ComponentInstance ID |
| `pin` | string | 规范化后的引脚编号 |
| `net` | string | Net ID |

同一元件引脚最多绑定一个网络。没有 Connection 的引脚表示未连接。

### 4.4 Net

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 网络唯一 ID |
| `name` | string | 用户可读网络名 |
| `trackWidthUm` | integer | 默认线宽 |
| `clearanceUm` | integer | 默认最小间距 |
| `color` | string | 可选网络颜色 |

飞线属于由网表和连接关系生成的显示数据，不作为必须保存的工程对象。

### 4.5 TrackSegment

第一版采用折线走线：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 走线 ID |
| `net` | string | 所属网络 |
| `layer` | string | 所在层 |
| `widthUm` | integer | 线宽 |
| `points` | PointMm[] | 至少两个连续点 |

一轮连续布线的多个拐点放在同一个 TrackSegment 中，便于整体选择、删除和撤销。

### 4.6 Via

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 过孔 ID |
| `net` | string | 所属网络 |
| `position` | PointMm | 中心位置 |
| `outerDiameterUm` | integer | 外径 |
| `drillDiameterUm` | integer | 钻孔直径 |
| `fromLayer` | string | 起始层 |
| `toLayer` | string | 目标层 |

第一版只要求通孔连接两层。多层盲孔、埋孔放入后续扩展范围。

### 4.7 CopperZone

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `id` | string | 敷铜 ID |
| `net` | string | 连接网络 |
| `layer` | string | 所属层 |
| `outer` | PointMm[] | 闭合外边界 |
| `holes` | PointMm[][] | 可选的封闭孔洞或避让边界 |
| `clearanceUm` | integer | 与不同网络对象的最小间距 |
| `fillMode` | enum | `solid` 或 `hatch` |

第一版可先实现实心填充；热焊盘、孤岛检测和复杂铜皮优化不属于当前验收范围。

## 5. 必须保持的不变量

导入、编辑和保存前后均应满足：

- ComponentInstance ID、连通网络 ID 和其他实体 ID 在各自作用域内唯一。
- 元件位号在板内唯一。
- 每个元件引用的封装必须存在。
- 每个 Connection 的元件、引脚和网络都必须存在。
- 同一元件引脚不能绑定到两个网络。
- 走线、过孔和敷铜的网络必须存在。
- 所有长度和坐标使用相同内部单位。
- 板框至少包含三个不重复角点；实际验证时要求首尾点相同、不自交且面积大于零。
- 坐标变换只能改变实例几何，不能修改共享元件库。
- 飞线、缓存包围盒、屏幕坐标等派生数据不得覆盖模型中的源数据。
- 文件中的层 ID 在保存和重新载入后保持稳定。

## 6. 变换规则

封装局部坐标到板坐标的计算顺序固定为：

1. 以封装原点为基准应用局部旋转。
2. 镜像到元件所在面。
3. 平移到 ComponentInstance.position。
4. 按网格吸附只修改实例位置，不修改库中几何。

旋转 90 度时，焊盘中心和焊盘方向都必须转换。不能只旋转丝印后保持焊盘位置不变。

## 7. 与各模块的接口

### 7.1 成员 A 提供

- 本文中的核心数据类型。
- 坐标、尺寸和角度类型。
- 封装查询接口。
- PCB 工程 JSON 保存与载入。
- 模型完整性检查和文件往返测试。

### 7.2 成员 B 提供

- 网表文件选择对话框。
- 导入结果和错误列表显示。
- 画布选择、拖动、旋转工具。
- 元件、飞线、走线和敷铜的图形显示。

### 7.3 成员 C 提供

- 网表解析、规范性检查和错误码。
- 元件实例与封装引脚匹配。
- 网络绑定和飞线端点生成。
- 移动、旋转后的 PCB 业务更新。
- 基础自动布局、布线和避让计算。
- PCB 样例及导入、连接验证记录。

## 8. 评审中确定的方案

| 问题 | 结论 |
| --- | --- |
| 焊盘是否保存在 PCB 元件副本中 | 不保存，统一从封装库和实例变换计算 |
| 网络是否写入共享封装库 | 不写入，只写入板级 Connection |
| 引脚编号是否强制为整数 | 不强制，统一使用字符串 |
| 一个引脚能否有多个焊盘 | 可以，通过相同 `pin` 关联 |
| 坐标是否使用 double 毫米 | 使用；外部微米库在导入时转换为毫米 |
| 是否保存飞线 | 不保存，根据 Connection 和实际走线派生 |
| 导入是否允许部分成功 | 不允许，阻塞错误导致整个导入失败 |
| 是否直接支持商业网表和封装格式 | v1 不支持，先定义并实现本课程格式 |

## 9. 后续扩展

- 增量导入和位号匹配冲突处理。
- 盲孔、埋孔及多孔层栈。
- 差分对、等长组和网络类规则。
- 敷铜热焊盘、孤岛移除和优先级。
- KiCad 或其他格式的独立适配器。

实现上述扩展时，不应直接改变 PCB 核心对象来适配某个第三方格式；应通过导入或导出适配层转换。
