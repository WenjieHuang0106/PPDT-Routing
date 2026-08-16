# PPDT-Routing(PCB 自动布线算法可视化平台)

分支：dev

一个面向 PCB 自动布线算法研究的前端 + 算法一体化项目。项目实现了**EDA 内核（Qt-free 解析 + 布线算法）**与**前端可视化框架**的彻底解耦：研究者可以直接复用本项目的界面与数据解析框架，接入自己的布线算法进行可视化调试与实验；同时 `src_db` + `src_algorithms` 作为纯 C++ 内核，可独立移植到 CLI / Tcl 脚本 / 其他 GUI 框架中调用，不依赖 Qt。

> **分支说明**：`dev` 为项目主干分支，核心改进是新增了独立的 `src_db` 纯 C++ 解析内核，并完成了 `src_data`（前端适配层）、`src_panels`（面板管理）、`src_services`（服务层控制器）的结构重组，为后续接入 CLI/Tcl 命令接口打好基础。轻量实验分支见 `exp`，二次开发可基于该分支，在 `src_algorithms` 目录下实现自己的算法模块，修改 `src_services/RoutingController` 实现算法衔接。

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
  - [如何独立使用 EDA 内核（不含 Qt）](#如何独立使用-eda-内核不含-qt)
- [配置文件说明](#配置文件说明)
- [测试数据](#测试数据)

---

## 项目简介

本项目是一个 PCB 布线算法的可视化实验平台，主要特点：

1. **EDA 内核独立（Qt-free）**：`src_db`（文件解析 + 几何/IO 工具） + `src_algorithms`（布线算法）均为纯 C++，可直接脱离 Qt 编译为独立的库或 CLI 工具，通过 Tcl 或其他命令行调用。
2. **前后端解耦**：界面框架（`src_ui`/`src_paint`/`src_panels`）与内核/算法层分离，算法开发者无需关心绘图细节。
3. **基于 DSN 文件格式**：内核解析器支持读取 Specctra DSN 格式的 PCB 设计文件（包含元件、焊盘、网络、布线等信息）。
4. **内置 PPDT 布线算法**：包含斯坦纳树构建、基于多边形障碍物的网格无关（meshless）路径搜索、推挤避让、后处理（尖角裁剪、方向约束）等完整流程。
5. **交互式调试**：支持鼠标拖拽推线（push line）、缩放平移、按层显示/隐藏、飞线/搜索树/规划点可视化等。
6. **实验数据导出**：可自动将布线结果与统计数据写入 `data/result/` 目录。

### 核心模块总览

| 层次 | 模块 | 文件 | 作用 |
|------|------|------|------|
| **EDA 内核层** (`src_db`) | 几何基元 | [src_db/dbPoint.h](src_db/dbPoint.h) | `db::Point2D` / `db::Box2D`，纯 C++ 几何运算 |
| | 字符串工具 | [src_db/dbString.h](src_db/dbString.h) | 纯 C++ `trim`/`split`/`startsWith` 等 |
| | 文件 IO | [src_db/dbIO.h](src_db/dbIO.h) / [.cpp](src_db/dbIO.cpp) | `std::ifstream` + `std::regex`，纯 C++ 文本读取 |
| | 内核抽象接口 | [src_db/dbDataBase.h](src_db/dbDataBase.h) / [dbParserBase.h](src_db/dbParserBase.h) | `db::DataBase` / `db::ParserBase`，对外统一输入接口 |
| | DSN 内核数据 | [src_db/dsn/dbData_dsn.h](src_db/dsn/dbData_dsn.h) | `std::unordered_map` / `std::vector` 存储 pads/pins/nets/wirings |
| | DSN 内核解析器 | [src_db/dsn/dbParser_dsn.h](src_db/dsn/dbParser_dsn.h) / [.cpp](src_db/dsn/dbParser_dsn.cpp) | 逐段解析 DSN，完全不依赖 Qt |
| **算法层** (`src_algorithms`) | 算法基础 | [src_algorithms/src_basics/dataStructAlg.h](src_algorithms/src_basics/dataStructAlg.h) / [utils.h](src_algorithms/src_basics/utils.h) | `Point` / `Line` 几何类与工具函数 |
| | 算法抽象 | [src_algorithms/src_basics/RouterBase.h](src_algorithms/src_basics/RouterBase.h) / [SteinerSolverBase.h](src_algorithms/src_basics/SteinerSolverBase.h) / [RouterFactory.h](src_algorithms/src_basics/RouterFactory.h) | 布线器、斯坦纳求解器基类 + 工厂模式 |
| | 斯坦纳树求解器 | [src_algorithms/src_dsn/MST.h](src_algorithms/src_dsn/MST.h) / [.cpp](src_algorithms/src_dsn/MST.cpp) | 基于 MST 的层感知斯坦纳树构建，支持跨层共享过孔 |
| | PPDT 布线器 | [src_algorithms/src_dsn/RouterMeshless.h](src_algorithms/src_dsn/RouterMeshless.h) / [.cpp](src_algorithms/src_dsn/RouterMeshless.cpp) | 多边形障碍物感知的网格无关路径搜索，含推挤避让与后处理 |
| | 空间网格索引 | [src_algorithms/src_dsn/Grid.h](src_algorithms/src_dsn/Grid.h) | 均匀网格空间索引，加速碰撞查询 |
| | 路径节点结构 | [src_algorithms/src_dsn/RoutingNode.h](src_algorithms/src_dsn/RoutingNode.h) | 定义 `PinPad`、`PolyShape`、`PathNode`、`PathTree` 等核心数据结构 |
| **服务层** | 布线控制器 | [src_services/RoutingController.h](src_services/RoutingController.h) / [.cpp](src_services/RoutingController.cpp) | **算法 ↔ UI 桥接核心**：从内核 `db::dsn::Data_dsn` 直接取数，组织斯坦纳树求解 + 布线运行 + 结果回填前端绘图容器 |

---

## 环境要求

| 项目 | 要求 |
|------|------|
| C++ 标准 | **C++20** 及以上 |
| Qt 版本 | **Qt 6.8** 及以上（仅 GUI 部分需要；EDA 内核 `src_db` + `src_algorithms` 不需要 Qt） |
| 编译器 | MSVC（Visual Studio 2022，推荐）或 MinGW |
| 构建工具 | Visual Studio 2022（.sln 方案）或 **CMake 3.20+**（项目已提供 `CMakeLists.txt`） |
| 操作系统 | Windows（已验证），理论上可移植到 Linux/macOS |

### 依赖说明

- GUI 可执行程序仅依赖 Qt Widgets 模块，无任何第三方库依赖。
- **独立 EDA 内核**（`src_db` + `src_algorithms`）零外部依赖，纯 C++ 标准库即可编译。
- 使用了 C++20 的结构化绑定、`contains()` 等特性，需确保编译器支持。

---

## 编译与运行

本项目提供**5 种构建方式**，按**从最简单 → 最专业**排列。**强烈建议按顺序尝试**：先从方式一开始，在你电脑上跑不通时再往下试下一种。

> **小提示**：方式一到方式四编译出来的都是带界面的 GUI 版；方式五是纯内核命令行版，没有窗口。

---

### 方式一：直接打开 `.sln`（最常用，适合 99% 的人）

这是最简单的方式：和传统 C++ 项目一样，Visual Studio 直接打开项目自带的解决方案。**不需要安装 CMake，也不需要设置任何环境变量**（只要 VS + Qt + Qt VS Tools 装对就行）。

**前置软件**（一次性安装，顺序无关）：

1. 安装 **Visual Studio 2022**（社区版即可）：安装时勾选「使用 C++ 的桌面开发」工作负载。
2. 安装 **Qt 6.8+ (MSVC 2022 x64)**：从官网 Qt Online Installer 安装，组件里勾选 `MSVC 2022 64-bit`。
3. 在 VS 里装 **Qt VS Tools** 扩展（这一步是**让 VS 知道 Qt 装在哪里**，因此不需要手动配环境变量）：
   - 打开 VS → 顶部菜单「扩展 → 管理扩展」→ 搜 "Qt Visual Studio Tools" 下载并安装。
   - 安装后按提示重启 VS。
   - 再次打开 VS，顶部菜单会出现「Qt VS Tools」→ 点它 → 点「Qt Versions…」。
   - 在弹出的窗口里点左上「+」号新增一个版本：
     - `Version name` 随便写（比如 `Qt68_msvc2022_64`）。
     - `Path` 点右边「…」按钮，选中你 Qt 安装目录下的 `bin\qmake.exe`，比如 `D:\ProgramFiles\QT6\6.8.0\msvc2022_64\bin\qmake.exe` → 确定。
   - 一路确定回到 VS 主界面。
4. （可选：如果你就是不想装 Qt VS Tools）那么按「[方式二前置里的 Windows 设置环境变量步骤](#附windows-设置环境变量的具体步骤方式二方式三方式四通用)」，加一个**用户变量** `QTDIR` = `D:\ProgramFiles\QT6\6.8.0\msvc2022_64`。

**操作步骤**：

1. 在资源管理器里双击打开 [PTreeRouting.sln](PTreeRouting.sln)。
2. VS 顶部工具栏里，平台选 `x64`，配置选 `Release`（✅ 推荐默认，算法跑得最快；需要单步调试时再切到 `Debug`）。
3. 按 `F7`（或菜单「生成 → 生成解决方案」）。
4. 按 `F5` 启动运行。
5. 生成的 exe 路径：`x64\Release\PTreeRouting.exe`（Debug 版在 `x64\Debug\PTreeRouting.exe`）。

**用这个方式，可以删除哪些文件（即不再需要 CMake 构建体系）**：

| 可删除文件 | 说明 |
|------|------|
| `CMakeLists.txt` | CMake 根工程，方式一用不到 |
| `configure.bat` | CMake 生成脚本 |
| `build.bat` | CMake 构建脚本 |
| `cmake/` | CMake 辅助脚本目录（含 windeployqt 模板），方式一用不到 |

---

### 方式二：`build.bat` 一键生成 exe（操作最简单，双击两次就行）

> 适合**不想折腾 VS 扩展配置、只想快点跑出 exe** 的用户。全程不需要打开 Visual Studio IDE（但 VS 本身仍要装）。

**前置软件**（一次性安装）：

1. 安装 **Visual Studio 2022**（社区版即可）：安装时勾选「使用 C++ 的桌面开发」。
2. 安装 **CMake 3.20+**：去 cmake.org 下载 Windows x64 Installer，**安装时必须勾选「Add CMake to the system PATH for all users」**（不勾的话下一步找不到 cmake 命令）。
   - **装完一定要验证**（防止你误以为自己勾了，其实没勾）：
     1. 按 `Win + R`，输入 `cmd` 回车打开命令提示符（黑窗口）。
     2. 输入 `cmake --version` 回车。
     3. 对的情况：会输出 `cmake version 3.xx.x` 一串文字（比如 `cmake version 3.30.5`）。
     4. 错的情况：提示 `'cmake' 不是内部或外部命令，也不是可运行的程序`。这说明 PATH 里没有 cmake，**请重新运行 CMake 安装包 → 选 Change/Modify → 一定要勾选那个 "Add CMake to the system PATH for all users" → 下一步直到完成，然后再执行上面验证步骤 1~3**。
3. 安装 **Qt 6.8+ (MSVC 2022 x64)**：组件勾选 `MSVC 2022 64-bit`。
4. **设置 `CMAKE_PREFIX_PATH` 环境变量**：让 CMake 找到 Qt。怎么做看下面的「附：Windows 设置环境变量的具体步骤」，配置的具体值是：
   - 变量名：`CMAKE_PREFIX_PATH`
   - 变量值：`D:\ProgramFiles\QT6\6.8.0\msvc2022_64`（替换成你自己的 Qt 根目录，注意末尾不要多一个反斜杠）

> ##### 附：Windows 设置环境变量的具体步骤（方式二 / 方式三 / 方式四通用）
>
> 设置环境变量是一次性工作，以后再建其他 CMake + Qt 项目也用得到。
>
> 1. 按键盘 `Win + R`（Win 键就是左下角 Ctrl 旁边那个四个小方块的键），会弹出「运行」小窗口。
> 2. 里面输入 `sysdm.cpl`（可以直接复制粘贴），按回车 → 会弹出「系统属性」大窗口。
> 3. 点大窗口上方的「高级」选项卡 → 再点右下角的「环境变量」按钮。
> 4. 现在你看到上下两个列表：上面写着「<用户名>的用户变量(U)」是只对你这个 Windows 账号生效的，下面「系统变量」是电脑上所有账号都生效。
> 5. 在**变量列表**右边点「新建(W)…」：
>    - 第一行「变量名(N):」填：`CMAKE_PREFIX_PATH`
>    - 第二行「变量值(V):」填你自己的 Qt 根目录，比如 `D:\ProgramFiles\QT6\6.8.0\msvc2022_64`
>    - 点「确定」保存。
> 6. **非常关键的一步**：现在所有你已经打开过的「命令提示符 / PowerShell / Visual Studio / 资源管理器」窗口，它们里面的环境变量还是旧的！必须**全部关掉再重新打开**。最简单省事：直接**重启电脑**（绝对稳）。
>
> **验证 Qt 路径对不对**：
> 把你刚才填的"变量值"整段复制 → 打开一个文件夹窗口 → 粘贴到顶部地址栏里按回车。
> - ✅ 对的情况 1（目录结构）：里面**必须直接能看到** 3 个文件夹：`bin`、`lib`、`plugins`。如果看到的是一个 `6.8.0` 子文件夹，说明你多选了一层，少选一层。
> - ✅ 对的情况 2（ABI 匹配，很重要！）：你填的路径末尾文件夹名里**必须带 `msvc2022_64`**（例如 `...\6.8.0\msvc2022_64`）。**一定不要填带 `mingw_64` 的路径**，因为 configure.bat 用的是 Visual Studio 编译器（MSVC），必须配**同一套编译器生成的 Qt 库**，否则后续会出现「Qt6Gui.dll 明明有却提示找不到」「大量 Qt 符号链接错误」这类奇怪问题。
> - ✅ 再双击进去 `lib\cmake\Qt6`，里面要能看到 `Qt6Config.cmake` 这个文件。
> - ❌ 错的情况：只看到一个叫 `6.8.0` 或者叫 `msvc2022_64` 的文件夹，说明你多选了一层，少选一层就行。

**操作步骤**：

1. 确保你重启过电脑（或至少关掉了所有 CMD / PowerShell 窗口）。
2. 进入项目根目录，**双击运行 `build.bat`**。
   - 它会自动先调 `configure.bat` 生成 `build/` 目录，再调 `cmake --build` 编译。
   - ✅ **默认就是 Release 模式**（算法跑得最快，且文件最小），不用你加任何参数。
3. 等 3 分钟左右（第一次慢一点），黑框里最后一行看到 `PTreeRouting.vcxproj -> ...\Release\PTreeRouting.exe` 字样就算成功。
4. 打开 `build\bin\Release\`，**直接双击里面的 `PTreeRouting.exe`** 就能运行。
   - 不需要自己拷任何 DLL 和配置：构建脚本已经把 Qt 运行时、`data/`、`config/` 都自动复制到 exe 同级目录了。

> 如果想要 **Debug 版本**（单步调试、能看到更详细调试信息）：在项目根目录的空白处，按住 Shift 同时点鼠标右键 → 选择「在此处打开命令窗口」或「在终端中打开」→ 输入 `build.bat Debug` 再回车即可。编译产物位于 `build\bin\Debug\`。

**用这个方式，可以删除哪些文件（即不再需要原 VS 工程体系）**：

| 可删除文件 | 说明 |
|------|------|
| `PTreeRouting.sln` | 原 VS 解决方案文件，`build.bat` 走 CMake 流程完全用不到 |
| `GeoDisplay.vcxproj` | 原 VS 工程文件 |
| `GeoDisplay.vcxproj.filters` | 原 VS 工程的过滤器文件 |

---

### 方式三：`configure.bat` 生成 `.sln` 再用 VS 打开（想继续用 VS 调试，但不想维护原 `.vcxproj`）

> 适合**想要 CMake 作为工程真相源（single source of truth），但日常开发还是用 VS IDE 调试**的用户。相比方式二，这种方式可以在 VS 里设置断点、单步调试、鼠标看变量。

**前置软件**（4 项，和方式二完全一样，装过就不用再装）：

1. 安装 **Visual Studio 2022**（含 C++ 桌面开发）。
2. 安装 **CMake 3.20+**（安装时勾选加入 PATH）。
3. 安装 **Qt 6.8+ (MSVC 2022 x64)**。
4. **设置 `CMAKE_PREFIX_PATH` 环境变量** = 你的 Qt 根目录（怎么设置？直接照着方式二里的「附：Windows 设置环境变量的具体步骤」做一遍就行）。

**操作步骤**：

1. 确保你重启过电脑（或关掉了所有 CMD / PowerShell / VS 窗口）。
2. 进入项目根目录，**双击运行 `configure.bat`**。
   - （临时兜底方案，只在你嫌配环境变量麻烦时用）不想设置环境变量？在项目根目录地址栏输入 `cmd` 回车，输入 `configure.bat "D:\ProgramFiles\QT6\6.8.0\msvc2022_64"` 再回车（引号里替换成你自己的 Qt 路径）。
3. 黑框里看到 `Build files have been written to: E:/.../ppdt-dev/build` 字样就算生成成功。
4. 进入项目根目录下的 `build` 文件夹，**双击打开 `PPDT_Routing.sln`**（这是 CMake 自动生成的新解决方案，和根目录里那个旧的 `PTreeRouting.sln` 没有关系）。
5. 在 VS 顶部工具栏里，**把配置选成 `Release`**（编译好后算法跑得最快；只有在你需要下断点单步调试时再切到 `Debug`）。
6. 在 VS 右侧解决方案资源管理器里，右键 `PTreeRouting` 项目 → 选择「设为启动项目」。
7. 按 `F5` 就能启动运行。
8. 以后你改完 `CMakeLists.txt`，或者往 `src_*` 目录里新增了源文件，只要**重新双击运行一次 `configure.bat`**，VS 里会自动弹出"此项目已被外部修改，是否重新加载？"，点「全部重新加载」即可。

**用这个方式，可以删除哪些文件（即不再需要原 VS 工程体系）**：

| 可删除文件 | 说明 |
|------|------|
| `PTreeRouting.sln` | 用 `build/PPDT_Routing.sln` 代替 |
| `GeoDisplay.vcxproj` | 用 `build/PTreeRouting.vcxproj` 代替 |
| `GeoDisplay.vcxproj.filters` | 同上 |

---

### 方式四：手动调 CMake 命令（需要跨平台 / 自动化 / 需要精细控制构建参数）

> 适合**有 CMake 经验的用户**：需要在 MinGW / Ninja / Linux 上构建，或要加自定义编译选项。

**前置软件**：

- 和方式二完全一样的 4 项（VS2022 + CMake3.20+ + Qt6.8 + `CMAKE_PREFIX_PATH` 环境变量）。环境变量怎么配，看方式二里的「附」小节。

**操作步骤**：

1. 在项目根目录的空白处，按住 Shift 同时点鼠标右键 → 选择「在终端中打开」（旧版 Windows 选「在此处打开命令窗口」）。
2. 粘贴下面这条命令回车（命令会自动读取你已经配好的 `CMAKE_PREFIX_PATH` 环境变量，不用再写一遍）：

   ```bat
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   ```

   （兜底：如果你**没配环境变量**，想临时传一次 Qt 路径，就改成：
   ```bat
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="D:\ProgramFiles\QT6\6.8.0\msvc2022_64"
   ```
   ）

3. 看到 `Build files have been written to: .../build` 之后，继续编译（**默认编 Release，算法跑得最快**）：
   ```bat
   cmake --build build --config Release
   :: 想编 Debug（符号调试用）就把 Release 改成 Debug
   cmake --build build --config Debug
   ```
4. 编译成功后，exe 在 `build\bin\Release\PTreeRouting.exe`（Debug 版在 `build\bin\Debug\` 子目录里）。

**用这个方式，可以删除哪些文件**：

| 可删除文件 | 说明 |
|------|------|
| `PTreeRouting.sln` | 由手动 cmake 命令在 build/ 下生成 |
| `GeoDisplay.vcxproj` | 同上 |
| `GeoDisplay.vcxproj.filters` | 同上 |
| `configure.bat` | 手动 cmake 即可，不再需要脚本封装 |
| `build.bat` | 同上 |
| `cmake/` | CMake 辅助脚本目录，手动 cmake 可直接删 |

---

### 方式五：独立编译 EDA 内核（不含 Qt，用于命令行 / Tcl 脚本调用）

> 适合**研究布线算法、只想要内核解析 + 算法结果，不需要 GUI** 的用户。零 Qt 依赖，零 GUI 文件。

**前置软件**：

- 一个支持 C++20 的编译器即可（MSVC / GCC / Clang 都可以）。
- **不需要 Qt**，也**不需要 CMake**（当然你想写自己的 CMake 也行）。

**操作步骤**：

1. 新建一个空工程（比如叫 `router_cli`）。
2. 把本项目里以下 2 个 include 目录加进你的工程头文件搜索路径：
   - `ppdt-dev/src_db/`
   - `ppdt-dev/src_algorithms/`
3. 把本项目里以下源文件加入编译列表：
   - `ppdt-dev/src_db/dbIO.cpp`
   - `ppdt-dev/src_db/dsn/dbParser_dsn.cpp`
   - `ppdt-dev/src_algorithms/src_basics/dataStructAlg.cpp`
   - `ppdt-dev/src_algorithms/src_basics/utils.cpp`
   - `ppdt-dev/src_algorithms/src_basics/RouterFactory.cpp`
   - `ppdt-dev/src_algorithms/src_dsn/MST.cpp`
   - `ppdt-dev/src_algorithms/src_dsn/RouterMeshless.cpp`
4. 在你自己的 `main.cpp` 里直接调用内核接口：

```cpp
#include <iostream>
#include <vector>
#include <string>

// 纯 C++ 头，不依赖 Qt
#include "src_db/dsn/dbData_dsn.h"
#include "src_db/dsn/dbParser_dsn.h"
#include "src_algorithms/src_dsn/RouterMeshless.h"
#include "src_algorithms/src_dsn/MST.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: router_cli <in.dsn>\n"; return 1; }

    // 1) 用内核解析器读取 DSN
    db::dsn::Data_dsn db;
    db::dsn::Parser_dsn parser;
    parser.bind(&db);
    if (!parser.readFile(argv[1])) { std::cerr << "parse failed\n"; return 2; }

    // 2) 准备算法数据：参考 src_services/RoutingController.cpp dataInit() 组织数据
    // 3) 运行斯坦纳树 + 布线器，取结果...
    //    SteinerTreeSolver sts(...);  RouterMeshless router(...);
    return 0;
}
```

**用这个方式，可以删除哪些文件（整个 GUI + CMake 体系都不需要）**：

| 可删除目录 / 文件 | 说明 |
|------|------|
| `main.cpp` | 项目自带的 GUI 入口 |
| `PTreeRouting.sln` / `GeoDisplay.vcxproj*` | VS GUI 工程 |
| `CMakeLists.txt` / `configure.bat` / `build.bat` | CMake GUI 构建文件 |
| `src_ui/` | 主窗口 UI |
| `src_panels/` | 面板管理 |
| `src_paint/` | 绘图层 |
| `src_data/` | 前端适配层 |
| `src_services/` | 布线控制器（GUI 桥接用） |
| `src_config/` | 配置系统（Qt 依赖） |
| `config/` | GUI 配置文件 |

---

### 运行 GUI

使用方式一 ~ 方式四编译出的 exe，启动后通过菜单栏 `文件 -> 打开` 选择 `data/` 目录下的 `.dsn` 文件即可载入 PCB 设计并进行布线。

---

## 项目结构

```
ppdt-dev/                          # ← dev 分支根目录
├── main.cpp                          # 程序入口，创建 GeoDisplay 主窗口
├── PTreeRouting.sln                  # VS 解决方案（方式一编译用）
├── GeoDisplay.vcxproj                # VS 工程（方式一编译用）
├── GeoDisplay.vcxproj.filters
├── CMakeLists.txt                    # CMake 根工程（方式二/三编译用）
├── configure.bat                     # Windows 一键生成 VS 解决方案脚本（方式二）
├── build.bat                         # Windows 一键构建脚本（方式三）
├── .gitignore
│
├── data/                             # 测试用例（DSN 格式 PCB 文件）
│   ├── case1.dsn ~ case6.dsn
│   └── result/                       # 布线结果输出（运行时自动生成）
│
├── config/                           # 配置文件
│   ├── default.config                # 默认配置（首次运行自动复制为 app.config）
│   └── app.config                    # 当前使用的配置文件
│
│ ───────────────────────────────────────────────────────────
│  EDA 内核层（纯 C++ / Qt-free，可独立编译为 CLI/Tcl）
│ ───────────────────────────────────────────────────────────
├── src_db/                           # ★ 解析与数据内核层（db 前缀，Qt-free）
│   ├── dbPoint.h                     # 几何基元：Point2D / Box2D
│   ├── dbString.h                    # 字符串工具：trim / split / startsWith
│   ├── dbIO.h / dbIO.cpp             # 纯 C++ 文件读取（std::ifstream + std::regex）
│   ├── dbDataBase.h                  # db::DataBase 抽象基类
│   ├── dbParserBase.h                # db::ParserBase 抽象基类（对外统一入口）
│   └── dsn/
│       ├── dbData_dsn.h              # DSN 内核数据结构（std::unordered_map/vector 存储）
│       └── dbParser_dsn.h / .cpp     # DSN 内核解析器实现（Qt-free）
│
├── src_algorithms/                   # ★ 算法层（Qt-free，可与 src_db 打包独立使用）
│   ├── src_basics/                   # 算法抽象 + 基础数据结构
│   │   ├── dataStructAlg.h / .cpp    # Point、Line 几何类
│   │   ├── utils.h / .cpp            # 几何工具函数（距离、投影、交点等）
│   │   ├── RouterBase.h              # 布线器抽象基类
│   │   ├── SteinerSolverBase.h       # 斯坦纳求解器抽象基类
│   │   └── RouterFactory.h / .cpp    # 算法工厂模式
│   └── src_dsn/                      # DSN 专用算法
│       ├── RoutingNode.h             # 核心数据结构：PinPad/PolyShape/PathNode/PathTree
│       ├── Grid.h                    # 均匀网格空间索引
│       ├── MST.h / .cpp              # 层感知斯坦纳树求解器
│       └── RouterMeshless.h / .cpp   # PPDT 布线器主算法
│
│ ───────────────────────────────────────────────────────────
│  前端适配层（Qt 依赖，将内核数据转换为 UI 绘图数据）
│ ───────────────────────────────────────────────────────────
├── src_data/                         # 前端数据基类 + 适配层（可依赖 Qt）
│   ├── DataBase.h                    # 前端数据基类（绘图相关：margin + 样式管理器）
│   ├── DataParserBase.h              # 前端解析器抽象接口（委托内核解析器）
│   ├── DiagramDataBase.h             # 绘图数据桥接接口（图元填充 + min/maxPt 缓存）
│   ├── dataStructUI.h / .cpp         # UI 数据结构：LineUI / CircleUI / SelectedTarget
│   └── dsn/
│       ├── Data_dsn.h / .cpp         # DSN 前端数据适配类：持有 shared_ptr<db::dsn::Data_dsn>，实现 DiagramDataBase
│       ├── DataParser_dsn.h / .cpp   # DSN 前端解析器：委托内核解析器，触发 UI 数据构建
│       └── Style_dsn.h               # DSN 绘图样式定义
│
│ ───────────────────────────────────────────────────────────
│  UI / 绘图层（Qt 依赖）
│ ───────────────────────────────────────────────────────────
├── src_ui/                           # 主窗口界面设计
│   ├── GeoDisplay.h / .cpp           # 主窗口（菜单栏/工具栏/标签页/面板管理）
│   ├── GeoDisplay.ui / .qrc
│   ├── setupMenuBar.cpp              # 菜单栏搭建
│   ├── setupToolBar.cpp              # 工具栏搭建
│   ├── setupTabWidget.cpp            # 标签页控件搭建
│   └── setupPanels.cpp               # 面板（选项面板/状态栏等）搭建
│
├── src_panels/                       # 面板管理（PanelManager 统一管理可停靠面板）
│   ├── PanelBase.h / .cpp            # 面板基类
│   ├── PanelManager.h / .cpp         # 面板管理器（注册 / 查找 / 信号转发）
│   └── panels/options/
│       └── OptionsPanel.h / .cpp / .ui  # 右侧选项面板（算法/显示/输入参数控制）
│
├── src_paint/                        # 绘图与标签页实现
│   ├── Diagram.h / .cpp              # 核心绘图控件：坐标变换、缓存绘制、鼠标交互、线段裁剪
│   ├── DiagramStyle.h / .cpp         # 绘图样式系统（PointStyle / LineStyle / PolygonStyle / Manager）
│   ├── TabPageBase.h / .cpp          # 标签页基类（封装路径、绘图窗口、交互槽、工厂模式）
│   ├── TabPageRegister.cpp           # 标签页工厂注册（按文件后缀关联 TabPage）
│   └── pages/dsn/
│       └── TabPage_dsn.h / .cpp      # DSN 文件专用标签页实现
│
└── src_services/                     # 服务层（用例编排/控制器）
    └── RoutingController.h / .cpp    # 布线控制器：从内核取数 → 斯坦纳树 → 布线算法 → 结果回填 UI
```

### 各源文件作用详解

#### `src_db/`（EDA 内核层，Qt-free）

| 文件 | 作用 |
|------|------|
| `dbPoint.h` | 内核几何基元，定义 `Point2D`（带 `dot`/`cross`/`length`/`scale` 等）与 `Box2D`（带 `expand`/`intersects`/`contains`）。 |
| `dbString.h` | `trim`/`splitByAny`/`startsWith`/`parseDoubles` 等字符串小工具，纯 C++。 |
| `dbIO.h/.cpp` | `readAllLines` / 正则包装 / 路径小工具，`std::ifstream` + `std::regex` 替代 `QFile`。 |
| `dbDataBase.h` | 内核数据基类 `db::DataBase`，维护 `m_bbox`（`Box2D`）与 `m_scale`（单位缩放），提供 `applyScale()` 基类虚函数。 |
| `dbParserBase.h` | 解析器基类 `db::ParserBase`，定义 `bind(DataBase*)` / `readFile(const std::string&)` 等对外统一接口，后续新格式只需继承它。 |
| `dsn/dbData_dsn.h` | DSN 格式的内核数据，`std::unordered_map<std::string, Pad>` / `std::unordered_map<std::string, Point2D>` / `Net` / `Wiring` 等全部使用标准容器；提供 `applyScale(scale)` 将 mil → mm 或其它单位统一换算。 |
| `dsn/dbParser_dsn.h/.cpp` | DSN 内核解析器，按 `parser/resolution/structure/placement/library/network/wiring` 分段解析，数据写回到 `db::dsn::Data_dsn`；全程零 Qt 头文件。 |

#### `src_algorithms/`（算法层，Qt-free）

| 文件 | 作用 |
|------|------|
| `src_basics/dataStructAlg.h/.cpp` | 几何基础类 `Point`（含哈希、距离、投影、叉积等）与 `Line`（线段/圆弧、层、线宽）。 |
| `src_basics/utils.h/.cpp` | 几何工具函数：向量运算、交点求解、角度判断、点集转换等。 |
| `src_basics/RouterBase.h` | 布线器抽象基类 `IRouter`，统一 `run()`、结果访问、调试输出接口。 |
| `src_basics/SteinerSolverBase.h` | 斯坦纳树求解器抽象基类 `ISteinerSolver`。 |
| `src_basics/RouterFactory.h/.cpp` | 工厂 + 单例，支持按名注册并实例化不同布线器 / 斯坦纳求解器，便于对比多算法。 |
| `src_dsn/RoutingNode.h` | **算法核心数据结构**：`PinPad`（焊盘/过孔）、`PolyShape`（多边形/路径，含环形链表节点）、`PathNode`（路径节点双向链表）、`PathTree`（A* 搜索树节点）、`ViaInfo`/`NetInfo`（网表约束）。 |
| `src_dsn/Grid.h` | 空间均匀网格索引 `GridManager`，支持按 box/线段快速查询障碍物与已布路径，加速碰撞检测。 |
| `src_dsn/MST.h/.cpp` | `SteinerTreeSolver` 斯坦纳树求解器：先构建 MST，再按层对插入共享斯坦纳点（共享过孔），输出飞线。 |
| `src_dsn/RouterMeshless.h/.cpp` | `RouterMeshless` PPDT 布线器主类：基于多边形障碍物的网格无关路径搜索，含节点选择/扩展、推挤避让、过孔插入、后处理（尖角裁剪、方向标准化、线间距修正）、几何重构（引力场推线）。 |

#### `src_data/`（前端适配层）

| 文件 | 作用 |
|------|------|
| `DataBase.h` | 前端数据基类：绘图相关的 `margin`（画布留白）与 `DiagramStyleManager m_styles`（样式管理器），不再存放数据边界（边界改由内核 `m_bbox` 推算）。 |
| `DataParserBase.h` | 前端解析器抽象基类，定义 `readFile`/`saveFile`，内部通过 `attachDbData()` 把内核数据关联到前端。 |
| `DiagramDataBase.h` | 绘图数据桥接接口：`set_canvas_data1/2/3`（三层画布填充）、`getMinPoint()/getMaxPoint()`（返回缓存的 QPointF* 给绘图层）。所有要被 `Diagram` 绘制的数据类都要实现它。 |
| `dataStructUI.h/.cpp` | UI 层使用的几何结构：`LineUI`（带线宽与层的线段）、`CircleUI`（带半径的圆）、`SelectedTarget`（鼠标选中状态）。 |
| `dsn/Data_dsn.h/.cpp` | DSN 前端数据适配类：持有 `shared_ptr<db::dsn::Data_dsn>` 内核数据；`setPaintData()` 把内核 pads/pins/nets/wirings 转换成 UI 图元；`syncBBoxCache()` 将内核 `Box2D` 同步为 `QPointF m_cachedMinPt / m_cachedMaxPt` 供绘图使用。 |
| `dsn/DataParser_dsn.h/.cpp` | DSN 前端解析器：封装内核 `db::dsn::Parser_dsn`，解析完成后 `attachDbData()` + `setPaintData()` 串联起"内核 → UI"的数据流向。 |
| `dsn/Style_dsn.h` | DSN 专用绘图样式定义（飞线/路径/焊盘/过孔/边界等样式 key 与样式值）。 |

#### `src_ui/` + `src_panels/`（主窗口 + 面板管理）

| 文件 | 作用 |
|------|------|
| `GeoDisplay.h/.cpp` | 主窗口类，集成菜单栏、工具栏、标签页控件、PanelManager 面板管理、状态栏。 |
| `PanelManager.h/.cpp` | 面板管理器：单例模式统一注册、查找、信号转发所有可停靠面板（OptionsPanel、属性面板等）。 |
| `PanelBase.h/.cpp` | 面板基类，所有自定义面板继承自此，通过 `PanelManager` 自动注册。 |
| `panels/options/OptionsPanel.h/.cpp/.ui` | 右侧停靠面板，提供算法选项勾选、显示开关、参数输入控件，通过信号通知 `PanelManager` 再广播给订阅者。 |
| `setupMenuBar.cpp` / `setupToolBar.cpp` / `setupTabWidget.cpp` / `setupPanels.cpp` | 按功能拆分的界面搭建函数，保持 GeoDisplay 主类文件清晰。 |

#### `src_paint/`（绘图与标签页实现）

| 文件 | 作用 |
|------|------|
| `Diagram.h/.cpp` | 核心绘图控件：坐标变换（数据坐标↔屏幕坐标：`dataToScreen`/`screenToData`）、双缓存绘制、鼠标交互（左键选线推线、中键平移、滚轮缩放、以鼠标位置为中心缩放）、Liang-Barsky 线段裁剪、点/线/多边形/圆分层绘制。 |
| `DiagramStyle.h/.cpp` | 绘图样式体系：`PointStyle`、`LineStyle`、`PolygonStyle` 以及 `DiagramStyleManager`，按 key 注册/查询样式并统一管理可见性、可选择性等。 |
| `TabPageBase.h/.cpp` | 标签页基类，封装文件路径、绘图窗口 `m_diagram`、鼠标交互槽函数。内含 `TabPageFactory` 单例工厂，按文件后缀注册并创建对应的 `TabPage` 子类。 |
| `TabPageRegister.cpp` | 标签页工厂注册入口。当前注册了 `.dsn` → `TabPage_dsn`，新增格式在此添加注册。 |
| `pages/dsn/TabPage_dsn.h/.cpp` | DSN 标签页实现，串联「内核解析→UI 适配→绘图窗口→布线控制器→回写结果→刷新绘图」的完整生命周期。 |

#### `src_services/`（服务层）

| 文件 | 作用 |
|------|------|
| `RoutingController.h/.cpp` | **布线业务控制器（算法-UI 桥接核心，二次开发主要入口）**。职责：<br>1. `dataInit()` 直接从内核 `db::dsn::Data_dsn` 取 pads/pins/nets/bound 等数据（零拷贝，避免 Qt↔std 容器转换）；<br>2. `routingRunBegin()` 构建斯坦纳树（飞线）并回填 UI 容器；<br>3. `routingRun()` 实例化并调用 `RouterMeshless::run()`；<br>4. 结果回填阶段把布线结果、过孔、规划点、搜索树写入 `Data_dsn` 对应 UI 绘图容器供 Diagram 重绘。 |

---

## 架构与数据流

整体采用**四层架构（EDA 内核 / 前端适配 / UI 绘图 / 服务编排）+ 工厂模式**设计，数据从文件流向算法再回流到绘图：

```
 ┌───────────────────────────────────────────────────────────────┐
 │  EDA 内核层 (Qt-free)  src_db + src_algorithms                 │
 │   db::ParserBase ──→ db::dsn::Data_dsn ──→ RouterMeshless      │
 │   （纯 C++，可独立用于 CLI / Tcl）                                │
 └─────────────────────┬─────────────────────────────────────────┘
                       │ shared_ptr 关联
                       ▼
 ┌───────────────────────────────────────────────────────────────┐
 │  前端适配层  src_data                                          │
 │   DataParser_dsn（委托内核）                                   │
 │        ↓ attachDbData()                                        │
 │   Data_dsn : DataBase + DiagramDataBase                       │
 │        ↓ setPaintData() 生成 LineUI/CircleUI/QPointF 容器      │
 └─────────────────────┬─────────────────────────────────────────┘
                       │
        ┌──────────────┼──────────────────────────────┐
        ▼              ▼                              ▼
 ┌─────────────┐ ┌────────────┐  ┌──────────────────────────────┐
 │  src_paint  │ │src_services│  │   src_panels / src_ui        │
 │  Diagram    │ │RoutingCtrl │  │  OptionsPanel / GeoDisplay    │
 │  (绘图层)   │ │(控制器)    │  │  (交互/参数/菜单/工具栏)      │
 └──────┬──────┘ └──────┬─────┘  └──────────────┬───────────────┘
        │               │                         │ 用户操作
        ▼               ▼                         ▼
 ┌───────────────────────────────────────────────────────────────┐
 │                    GeoDisplay（主窗口编排）                    │
 │   Menu / ToolBar / TabWidget / PanelManager / StatusBar       │
 └───────────────────────────────────────────────────────────────┘
```

### 数据流步骤（打开 case1.dsn 并布线）

1. **打开文件**：`GeoDisplay::openFile()` → `TabPageFactory::create()` → `TabPage_dsn`。
2. **解析数据**：`TabPage_dsn::fillData()` 调用前端 `DataParser_dsn::readFile()`，内部委托内核 `db::dsn::Parser_dsn` 解析到 `db::dsn::Data_dsn`；按默认 `scale=1`（显示 mil 原始坐标）做单位转换；再 `attachDbData()` 转移给前端 `Data_dsn`。
3. **构建 UI 绘图容器**：`Data_dsn::setPaintData()` 把内核 pads/pins/nets/wirings 生成 `m_boundaryLines/m_PadsPoly/m_circles/m_PinsNet/m_flyLines/...` 等 UI 图元容器，并将 `minPt/maxPt` 在内核 `Box2D` 与前端 `QPointF` 间同步缓存。
4. **创建绘图 + 服务层桥接**：实例化 `Diagram`（绑定 `Data_dsn` 指针），再实例化 `RoutingController`（直接访问 `Data_dsn::dbData()` 拿内核数据，零 Qt 容器转换）。
5. **预处理**：`RoutingController::routingRunBegin()` 从内核 `m_pins/m_nets` 构造 `PinPad`/`NetInfo`/`ViaInfo` 等算法数据，运行 `SteinerTreeSolver` 生成飞线并回填 `Data_dsn::m_flyLines`。
6. **运行算法**：用户点击运行按钮 → `TabPage_dsn::pressRunButton()` → `RoutingController::routingRun()` → `RouterMeshless::run()`。
7. **结果回填**：布线执行后，`RoutingController` 将 `m_paths/m_viaInfos/m_planningPts/m_treesLines` 等写入 `Data_dsn` 对应 UI 容器。
8. **刷新绘图**：`Diagram::setData1/2/3()` 通过 `DiagramDataBase::set_canvas_data1/2/3()` 拉取三层画布数据并重绘。

---

## 二次开发指南

### 如何复用现有界面

本项目的界面框架与算法/内核彻底解耦，复用界面只需关注以下几个接口：

#### 1. 实现 `DiagramDataBase` 接口（你的数据类 → 绘图层）

要让你的数据被 `Diagram` 绘制，你的数据类需要继承 `DataBase` 并实现 `DiagramDataBase`：

```cpp
class MyData : public DataBase, public DiagramDataBase {
public:
    MyData() { setupStyleManager(); }

    // 实现 DiagramDataBase 的三个方法，分别填充三层画布数据
    void set_canvas_data1(...) override { /* 背景层：边界、焊盘等 */ }
    void set_canvas_data2(...) override { /* 前景层：飞线、路径、搜索树等 */ }
    void set_canvas_data3(...) override { /* 动态层：下拉切换的树形结构等 */ }

    // 如果用内核数据源，可通过缓存同步 min/maxPt
    QPointF* getMinPoint() override { syncBBoxCache(); return &m_cachedMinPt; }
    QPointF* getMaxPoint() override { syncBBoxCache(); return &m_cachedMaxPt; }

protected:
    void setupStyleManager() override { /* 注册样式 key 到 m_styles */ }
};
```

`Diagram` 支持的图元类型：点（`QPointF`）、线（`LineUI`）、多边形（`LineUI` 集合）、圆（`CircleUI`）。每种图元通过 `Style` 设置颜色、线宽、填充、是否可选中。

#### 2. 实现前端文件解析器（可选）

若你的输入文件格式与 DSN 不同：
