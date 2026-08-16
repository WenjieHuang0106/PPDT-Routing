# PPDT-Routing(PCB 自动布线算法可视化平台)

分支：exp

一个面向 PCB 自动布线算法研究的前端 + 算法一体化项目。项目实现了**前端可视化框架**与**布线算法**的解耦：研究者可以直接复用本项目的界面与数据解析框架，接入自己的布线算法进行可视化调试与实验。

> **分支说明**：`exp` 为轻量实验分支，包含项目示例算法迭代与功能更新，二次开发可基于该分支，在src_algorithms目录下实现自己的算法模块，修改src_paint/TabPage_dsn/AlgorithmLink下的AlgorithmLink文件实现算法衔接。

github地址：https://github.com/WenjieHuang0106/PPDT-Routing.git

如有疑问，欢迎加入QQ交流群：808099802，验证信息填github

<img width="1502" height="944" alt="image" src="https://github.com/user-attachments/assets/ac2e65d4-fb67-4de8-b0d2-2fa176d9eb8a" />

---

## 目录

- [项目简介](#项目简介)
- [环境要求](#环境要求)
- [编译与运行](#编译与运行)
- [项目结构](#项目结构)
- [架构与数据流](#架构与数据流)
- [二次开发指南](#二次开发指南)
  - [如何复用现有界面](#如何复用现有界面)
  - [如何接入自己的算法](#如何接入自己的算法)
- [配置文件说明](#配置文件说明)
- [测试数据](#测试数据)

---

## 项目简介

本项目是一个 PCB 布线算法的可视化实验平台，主要特点：

1. **前后端解耦**：界面框架与算法逻辑分离，算法开发者无需关心绘图细节。
2. **基于 DSN 文件格式**：支持读取 Specctra DSN 格式的 PCB 设计文件(包含元件、焊盘、网络、布线等信息)。
3. **内置 PPDT 布线算法**：包含斯坦纳树构建、基于多边形障碍物的网格无关(meshless)路径搜索、推挤避让、后处理(尖角裁剪、方向约束)等完整流程。
4. **交互式调试**：支持鼠标拖拽推线(push line)、缩放平移、按层显示/隐藏、飞线/搜索树/规划点可视化等。
5. **实验数据导出**：可自动将布线结果与统计数据写入 `data/result/` 目录。

### 核心算法模块(内置示例)

| 模块 | 文件 | 作用 |
|------|------|------|
| 斯坦纳树求解器 | [src_algorithms/src_dsn/MST.h](src_algorithms/src_dsn/MST.h) | 基于 MST 的层感知斯坦纳树构建，支持跨层共享过孔 |
| PPDT 布线器 | [src_algorithms/src_dsn/RouterMeshless.h](src_algorithms/src_dsn/RouterMeshless.h) | 多边形障碍物感知的网格无关路径搜索，含推挤避让与后处理 |
| 空间网格索引 | [src_algorithms/src_dsn/Grid.h](src_algorithms/src_dsn/Grid.h) | 均匀网格空间索引，加速碰撞查询 |
| 路径节点结构 | [src_algorithms/src_dsn/RoutingNode.h](src_algorithms/src_dsn/RoutingNode.h) | 定义 `PinPad`、`PolyShape`、`PathNode`、`PathTree` 等核心数据结构 |

---

## 环境要求

| 项目 | 要求 |
|------|------|
| C++ 标准 | **C++20** 及以上 |
| Qt 版本 | **Qt 6.8** 及以上 |
| 编译器 | MSVC(推荐，项目自带 `.sln`/`.vcxproj`)或 MinGW |
| 构建工具 | Visual Studio 2022(Windows)或 CMake(需自行配置) |
| 操作系统 | Windows(已验证)，理论上可移植到 Linux/macOS |

### 依赖说明

- 仅依赖 Qt Widgets 模块，无任何第三方库依赖。
- 使用了 C++20 的结构化绑定、`contains()`、`std::format` 风格等特性，需确保编译器支持。

---

## 编译与运行

### 方式一：使用 Visual Studio(推荐)

1. 安装 Qt 6.8+ 并配置 `QTDIR` 环境变量，或在 VS 中安装 Qt VS Tools 并指定 Qt 版本。
2. 打开 [PTreeRouting.sln](PTreeRouting.sln)。
3. 选择 `x64` 平台，选择 `Debug` 或 `Release` 配置。
4. 生成解决方案并运行。

### 方式二：使用命令行

```bash
# 使用 CMake(需自行编写 CMakeLists.txt，项目目前仅提供 VS 工程文件)
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### 运行

启动后通过菜单栏 `文件 -> 打开` 选择 `data/` 目录下的 `.dsn` 文件即可载入 PCB 设计并进行布线。

---

## 项目结构

```
ppdt-exp/
├── main.cpp                          # 程序入口，创建 GeoDisplay 主窗口
├── PTreeRouting.sln                  # VS 解决方案
├── GeoDisplay.vcxproj                # VS 工程
├── .gitignore
│
├── data/                             # 测试用例(DSN 格式 PCB 文件)
│   └── case1~6.dsn
│
├── src_BaseClasses/                  # 基类与通用接口(前后端共享)
│   ├── Data.h                        # 数据基类，存储边界与样式管理器
│   ├── DataParser.h                  # 文件解析器抽象接口(readFile/saveFile)
│   ├── DiagramData.h                 # 绘图数据接口，所有需绘制的 Data 类须实现
│   ├── TabPage.h                     # 标签页基类 + TabPageFactory 工厂模式
│   ├── dataStructUI.h                # UI 数据结构：LineUI、CircleUI、SelectedTarget
│   ├── TabPage.cpp                   # 基类实现
│   └── dataStructUI.cpp
│
├── src_config/                       # 配置与样式
│   ├── configUI.h                    # 全局 UI 配置(视图/算法/显示/输入选项)
│   ├── configUI.cpp
│   ├── DiagramStyle.h                # 绘图样式类：PointStyle/LineStyle/PolygonStyle
│   ├── DiagramStyle.cpp
│   └── config_default.config         # 默认配置文件(运行时自动生成 config.config)
│
├── src_uiDesign/                     # 主窗口界面设计
│   ├── GeoDisplay.h / .cpp           # 主窗口(菜单栏/工具栏/标签页/选项面板)
│   ├── GeoDisplay.ui                 # 主窗口界面文件
│   ├── GeoDisplay.qrc                # 资源文件
│   ├── OptionsPanel.h / .cpp         # 右侧选项面板(算法/显示/输入参数控制)
│   ├── OptionsPanel.ui
│   ├── setupMenuBar.cpp              # 菜单栏搭建
│   ├── setupToolBar.cpp              # 工具栏搭建
│   ├── setupTabWidget.cpp            # 标签页控件搭建
│   ├── setupOptionPanel.cpp          # 选项面板搭建
│   └── setupDiagramWidget.cpp        # 绘图区搭建
│
├── src_paint/                        # 绘图与标签页实现
│   ├── Diagram.h / .cpp              # 绘图控件：坐标变换、缓存绘制、鼠标交互
│   ├── TabPageRegister.cpp           # 标签页工厂注册(按文件后缀关联 TabPage)
│   └── TabPage_dsn/                  # DSN 文件专用标签页
│       ├── TabPage_dsn.h / .cpp      # DSN 标签页实现(继承 TabPage)
│       ├── Data_dsn.h                # DSN 数据类(继承 Data + DiagramData)
│       ├── DataParser_dsn.h / .cpp   # DSN 文件解析器
│       ├── Style_dsn.h               # DSN 绘图样式定义
│       └── AlgorithmLink/
│           ├── AlgorithmLink_dsn.h   # 算法-UI 桥接层(核心接入点)
│           └── AlgorithmLink_dsn.cpp
│
└── src_algorithms/                   # 算法实现
    ├── src_basics/                   # 算法基础数据结构与工具
    │   ├── dataStructAlg.h           # Point、Line 几何类
    │   ├── dataStructAlg.cpp
    │   ├── utils.h                   # 几何工具函数(距离、投影、交点等)
    │   └── utils.cpp
    └── src_dsn/                      # DSN 专用算法
        ├── RoutingNode.h             # 核心数据结构：PinPad/PolyShape/PathNode/PathTree
        ├── Grid.h                    # 均匀网格空间索引(GridCell/GridManager)
        ├── MST.h / .cpp              # 层感知斯坦纳树求解器
        └── RouterMeshless.h / .cpp   # PPDT 布线器(主算法)
```

### 各源文件作用详解

#### src_BaseClasses/(基类层)

| 文件 | 作用 |
|------|------|
| `Data.h` | 所有数据类的基类，存储图形边界 `minPt`/`maxPt`、边距 `margin` 以及样式管理器 `m_styles`。 |
| `DataParser.h` | 文件解析器抽象基类，定义 `readFile`/`saveFile`/`saveFileAs` 纯虚接口，子类按格式实现解析逻辑。 |
| `DiagramData.h` | 绘图数据接口，定义 `set_canvas_data1/2/3` 三个层次的绘图数据填充方法。任何要被 `Diagram` 绘制的类必须实现此接口。 |
| `TabPage.h` | 标签页基类，封装文件路径、绘图窗口 `m_diagram`、鼠标交互槽函数。内含 `TabPageFactory` 单例工厂，按文件后缀注册并创建对应的 `TabPage` 子类。 |
| `dataStructUI.h` | 定义 UI 层使用的几何结构：`LineUI`(带线宽与层的线段)、`CircleUI`(带半径的圆)、`SelectedTarget`(鼠标选中状态)。 |

#### src_config/(配置层)

| 文件 | 作用 |
|------|------|
| `configUI.h` | 全局配置类，集中管理视图选项(翻转/旋转)、算法选项(GND/VCC/差分/推挤)、显示选项(飞线/路径/规划点显隐)、用户输入参数(字符串/数值)。 |
| `DiagramStyle.h` | 绘图样式体系：`PointStyle`、`LineStyle`、`PolygonStyle`，以及 `DiagramStyleManager` 样式管理器，按 key 注册/查询样式。 |
| `config_default.config` | 默认配置，程序首次运行时复制为 `config.config` 并在每次算法执行后自动更新。 |

#### src_uiDesign/(主窗口界面)

| 文件 | 作用 |
|------|------|
| `GeoDisplay.h/.cpp` | 主窗口类，集成菜单栏、工具栏、标签页控件、选项面板、状态栏。 |
| `OptionsPanel.h/.cpp` | 右侧停靠面板，提供算法选项勾选、显示开关、参数输入控件，通过信号通知主窗口更新。 |
| `setup*.cpp` | 按功能拆分的搭建函数：菜单栏、工具栏、标签页、选项面板、绘图区。 |

#### src_paint/(绘图与标签页实现)

| 文件 | 作用 |
|------|------|
| `Diagram.h/.cpp` | 核心绘图控件，负责坐标变换(数据坐标↔屏幕坐标)、双缓存绘制、鼠标交互(左键选线推线、中键平移、滚轮缩放)、线段裁剪。 |
| `TabPageRegister.cpp` | 标签页工厂注册入口。当前注册了 `.dsn` 后缀 → `TabPage_dsn`。新增文件格式时在此添加注册。 |
| `TabPage_dsn/Data_dsn.h` | DSN 数据类，存储 PCB 解析结果(元件/焊盘/网络/布线)，并实现 `DiagramData` 接口将数据分类填充到三层画布。 |
| `TabPage_dsn/DataParser_dsn.h/.cpp` | DSN 文件解析器，逐段解析 parser/resolution/structure/placement/library/network/wiring。 |
| `TabPage_dsn/Style_dsn.h` | DSN 专用绘图样式定义(飞线/路径/焊盘/过孔/边界等样式 key 与样式值)。 |
| `TabPage_dsn/TabPage_dsn.h/.cpp` | DSN 标签页实现，串联数据解析→绘图窗口→算法桥接层，并响应运行按钮与鼠标交互。 |
| `TabPage_dsn/AlgorithmLink/AlgorithmLink_dsn.h/.cpp` | **算法与 UI 的桥接层(二次开发核心接入点)**。负责从 `Data_dsn` 提取算法输入数据、创建并调用布线器、将算法结果回填到 `Data_dsn` 供绘图。 |

#### src_algorithms/(算法层)

| 文件 | 作用 |
|------|------|
| `src_basics/dataStructAlg.h` | 几何基础类 `Point`(含哈希、距离、投影等)与 `Line`(线段/圆弧)。 |
| `src_basics/utils.h/.cpp` | 几何工具函数：向量运算、交点求解、角度判断、点集转换等。 |
| `src_dsn/RoutingNode.h` | **算法核心数据结构**：`PinPad`(焊盘/过孔)、`PolyShape`(多边形/路径，含环形链表节点)、`PathNode`(路径节点双向链表)、`PathTree`(A*搜索树节点)、`ViaInfo`/`NetInfo`(网表约束)。 |
| `src_dsn/Grid.h` | 空间均匀网格索引 `GridManager`，支持按 box/线段快速查询障碍物与已布路径，加速碰撞检测。 |
| `src_dsn/MST.h/.cpp` | `SteinerTreeSolver` 斯坦纳树求解器：先构建 MST，再按层对插入共享斯坦纳点(共享过孔)，输出飞线。 |
| `src_dsn/RouterMeshless.h/.cpp` | `RouterMeshless` PPDT 布线器主类：基于多边形障碍物的网格无关路径搜索，含节点选择/扩展、推挤避让、过孔插入、后处理(尖角裁剪、方向标准化、线间距修正)、几何重构(引力场推线)。 |

---

## 架构与数据流

整体采用**分层 + 工厂模式**设计，数据从文件流向算法再回流到绘图：

```
┌─────────────────────────────────────────────────────────────┐
│                      GeoDisplay(主窗口)                      │
│  菜单栏 / 工具栏 / 标签页 / 选项面板(OptionsPanel)              │
└───────────────┬─────────────────────────────────────────────┘
                │ 用户打开文件
                ▼
┌─────────────────────────────────────────────────────────────┐
│            TabPageFactory(按后缀创建标签页)                   │
│   .dsn  →  TabPage_dsn                                      │
└───────────────┬─────────────────────────────────────────────┘
                ▼
┌─────────────────────────────────────────────────────────────┐
│                    TabPage_dsn(标签页)                       │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │ DataParser  │→ │   Data_dsn   │← │  AlgorithmLink_dsn │  │
│  │ (解析DSN)   │  │  (数据+绘图)  │  │   (算法桥接层)       │  │
│  └─────────────┘  └──────┬───────┘  └─────────┬──────────┘  │
│                         │                     │             │
│                         │                     ▼             │
│                         │          ┌────────────────────┐   │
│                         │          │  SteinerTreeSolver │   │
│                         │          │  RouterMeshless    │   │
│                         │          └────────────────────┘   │
│                         │                     │             │
│                         │   算法结果回填        │             │
│                         │←────────────────────┘             │
│                         ▼                                   │
│                ┌────────────────┐                           │
│                │    Diagram     │  (绘图控件，读取 Data_dsn) │
│                └────────────────┘                           │
└─────────────────────────────────────────────────────────────┘
```

### 数据流步骤

1. **打开文件**：`GeoDisplay` → `TabPageFactory::create()` → `TabPage_dsn`。
2. **解析数据**：`TabPage_dsn::fillData()` 调用 `DataParser_dsn::readFile()` 解析 DSN 到 `Data_dsn`。
3. **创建绘图与算法桥接**：实例化 `Diagram`(绑定 `Data_dsn`)和 `AlgorithmLink_dsn`。
4. **预处理**：`AlgorithmLink_dsn::routingRunBegin()` 提取算法输入数据，构建斯坦纳树。
5. **运行算法**：用户点击运行按钮 → `TabPage_dsn::pressRunButton()` → `AlgorithmLink_dsn::routingRun()` → `RouterMeshless::run()`。
6. **结果回填**：算法执行后，`AlgorithmLink_dsn` 将路径、过孔、规划点、搜索树写入 `Data_dsn` 的绘图容器。
7. **刷新绘图**：`Diagram` 重新读取 `Data_dsn` 的三层画布数据并重绘。

---

## 二次开发指南

### 如何复用现有界面

本项目的界面框架与算法完全解耦，复用界面只需关注以下几个接口：

#### 1. 实现 `DiagramData` 接口

要让你的数据被 `Diagram` 绘制，你的数据类需要继承 `Data` 并实现 `DiagramData`：

```cpp
class MyData : public Data, public DiagramData {
public:
    MyData() { setupStyleManager(); }

    // 实现 DiagramData 的三个方法，分别填充三层画布数据
    void set_canvas_data1(...) override { /* 背景层：边界、焊盘等 */ }
    void set_canvas_data2(...) override { /* 前景层：飞线、路径等 */ }
    void set_canvas_data3(...) override { /* 动态层：搜索树等 */ }

    QPointF* getMinPoint() override { return &minPt; }
    QPointF* getMaxPoint() override { return &maxPt; }

protected:
    void setupStyleManager() override { /* 注册样式 key */ }
};
```

`Diagram` 支持的图元类型：点(`QPointF`)、线(`LineUI`)、多边形(`LineUI` 集合)、圆(`CircleUI`)。每种图元通过 `Style` 设置颜色、线宽、填充、是否可选中。

#### 2. 实现文件解析器(可选)

若你的输入文件格式与 DSN 不同，继承 `DataParser`：

```cpp
class MyParser : public DataParser {
public:
    MyParser(MyData* data) : m_data(data) {}
    bool readFile(const QString& qFullName) override { /* 解析逻辑 */ }
    bool saveFile(const QString& qFullName) override { /* 保存逻辑 */ }
    bool saveFileAs(const QString& qFullName) override { /* 另存为 */ }
protected:
    void setMinMax() override { /* 计算数据边界 */ }
private:
    MyData* m_data;
};
```

#### 3. 创建标签页并注册

继承 `TabPage`，在 `fillData()` 中完成数据解析、绘图窗口创建、算法桥接层创建：

```cpp
class MyTabPage : public TabPage {
    // 实现纯虚函数：fillData, refreshUI, pressRunButton, onConfigChanged,
    //              leftPressRun, leftPressMoveRun, leftReleaseRun 等
};
```

在 [src_paint/TabPageRegister.cpp](src_paint/TabPageRegister.cpp) 中注册后缀与构造函数：

```cpp
static auto fun_myFormat = [](const QString& qFullName, ...) {
    return new MyTabPage(qFullName, qFilePath, qFileName, parent);
};
// 在 TabPageRegister 构造函数中
factory.registerCreator("myext", fun_myFormat);
```

#### 4. 复用绘图样式系统

通过 [Style_dsn.h](src_paint/TabPage_dsn/Style_dsn.h) 的方式定义样式 key 与样式值，在 `setupStyleManager()` 中注册到 `DiagramStyleManager`，即可在 `set_canvas_data*` 中按 key 获取样式指针并绑定到图元。

#### 5. 复用配置面板

选项面板`OptionsPanel` 通过 `ConfigUI` 与标签页以及算法层通信，面板上设置的所有属性都会存储在 `m_config` 对象中，你只需要在算法中传递这个对象的指针就可以在任意地方获取UI中的选项值。

你可以复用现有的勾选项与输入框，在 `onConfigChanged()` 中读取 `m_config` 对象的相关字段，调用你自己定义的接口，来使得面板参数变化后需要立马执行的任务，比如显示/隐藏飞线被点击后，需要立刻设置飞线的属性为不显示，然后刷新界面。

---

### 如何接入自己的算法

接入算法的核心位置是 [src_paint/TabPage_dsn/AlgorithmLink/AlgorithmLink_dsn.h](src_paint/TabPage_dsn/AlgorithmLink/AlgorithmLink_dsn.h)。推荐两种方式：

#### 方式 A：替换 `RouterMeshless`(推荐，改动最小)

如果你只想替换路径搜索算法，但保留斯坦纳树与数据结构，按以下步骤操作：

1. **实现你自己的布线器类**，输入输出契约参照 `RouterMeshless`：

   ```cpp
   class MyRouter {
   public:
       // 构造函数接收算法输入数据(与 RouterMeshless 一致)
       MyRouter(
           unordered_map<string, STN>* netTrees,
           unordered_map<string, PinPad>* pads,
           unordered_map<string, PinPad>* preVias,
           vector<double>* bound,
           unordered_map<string, ViaInfo>* netViaInfos,
           unordered_map<string, NetInfo>* netInfos,
           unordered_map<string, vector<PinPad*>>* nets
       );

       void run(vector<string>& routingInfo);          // 执行布线

       // 提供结果访问接口(供 AlgorithmLink 回填绘图)
       vector<PathTree*>* getTreesHeadsOrdered();
       unordered_map<PathNode*, PolyShape>* getPaths();
       unordered_set<Point, Point::Hash>* getPlanningPts();
       unordered_map<Point, PinPad, Point::Hash>* getVias();
   };
   ```

2. **在 `AlgorithmLink_dsn` 中替换** `RouterMeshless* m_router` 为 `MyRouter* m_router`，并修改 [setNetMST()](src_paint/TabPage_dsn/AlgorithmLink/AlgorithmLink_dsn.cpp) 中的实例化代码。

3. **算法结果回填**：`routingRun()` 末尾已实现将 `m_router` 的结果填充到 `Data_dsn` 的绘图容器(`fillPaintPathLines`/`fillPlanningPt`/`fillPaintTrees`)，如果你的结果数据结构一致则无需修改。

#### 方式 B：完全自定义算法流程

如果你想完全控制算法流程(包括不用斯坦纳树)：

1. 在 `AlgorithmLink_dsn` 中新增你自己的方法，例如 `myRoutingRun()`。
2. 从 `Data_dsn` 提取你需要的输入数据(焊盘坐标 `m_dsnPins`、网络 `m_dsnNets`、焊盘形状 `m_dsnPads` 等)。
3. 执行你的算法。
4. 将结果转换为 `Data_dsn` 的绘图容器格式：
   - 路径线段：`std::vector<std::vector<LineUI>> m_paths`(外层按层，内层为线段)
   - 过孔：`std::vector<std::vector<CircleUI>> m_viaInfos`
   - 飞线：`std::vector<std::vector<LineUI>> m_flyLines`
   - 规划点：`std::vector<QPointF> m_planningPts`
   - 搜索树：`std::vector<std::vector<LineUI>> m_treesLines`
5. 在 `TabPage_dsn::pressRunButton()` 中改为调用你的方法。
6. 调用 `m_diagram->setData2()` / `setData3()` / `refresh()` 刷新绘图。

#### 算法数据结构说明

接入算法前建议先熟悉以下核心数据结构(定义于 [src_algorithms/src_dsn/RoutingNode.h](src_algorithms/src_dsn/RoutingNode.h))：

| 结构 | 说明 |
|------|------|
| `Point` | 二维点，含哈希、距离、投影、叉积等运算([dataStructAlg.h](src_algorithms/src_basics/dataStructAlg.h)) |
| `Line` | 线段或圆弧，含层、线宽属性 |
| `PinPad` | 焊盘/过孔，含位置、网络名、多层多边形形状 `shapes`、外包盒 `box` |
| `PolyShape` | 多边形或路径，由 `PathLine`(边)组成，`PathNode` 构成环形/双向链表 |
| `PathNode` | 路径节点，含位置、方向、层、前后驱指针 |
| `PathTree` | A* 搜索树节点，含 g/h/e/f 代价、父子关系 |
| `STN` (`shared_ptr<SteinerNode>`) | 斯坦纳树节点 |
| `ViaInfo` / `NetInfo` | 过孔规格(半径、层)/ 网络约束(线宽、间距) |

---

## 配置文件说明

程序运行时会在 `src_config/` 目录下生成 `config.config`(首次从 `config_default.config` 复制)，每次算法执行后自动更新。

| 配置项 | 含义 |
|--------|------|
| `defaltPath` | 默认打开路径 |
| `viewOpt` | 视图选项(翻转X/翻转Y/旋转90) |
| `algmOpt` | 算法选项(预via分配/GND布线/VCC布线/差分布线/4-8方向树/自动推线/自动写结果) |
| `dispOpt` | 显示选项(障碍物层1/层2/飞线/路径/规划点/过孔/Pin中心) |
| `boolOpt` | 其他选项(后处理/显示树/调试中断) |
| `m_directionOp` | 方向约束(0任意/1四方向/2八方向) |
| `m_gridType` | 网格类型(0无/1线形/2点形) |
| `m_pushRunMode` | 推线模式(0关/1阻塞) |
| `m_postMode` | 后处理模式 |
| `m_flexibleOpt` | 灵活选项字符串(6位，控制引脚对换/尖角裁剪/推挤/线间距/GND层/GND via数) |
| `m_doubleNum` | 用户输入的数值(坐标/参数) |
| `m_showTreeIndex` | 显示第几棵搜索树(-1为全部) |

---

## 测试数据

`data/` 目录提供 6 个 DSN 测试用例(`case1.dsn` ~ `case6.dsn`)，包含不同规模的 PCB 设计，可用于验证算法效果。

算法执行的结果文件(含统计数据)会输出到 `data/result/` 目录，文件名格式为 `yyyyMMddHHmmss_原文件名.txt`。

---

## 许可协议

本项目仅供学习与研究使用。如需用于商业用途，请联系作者。
