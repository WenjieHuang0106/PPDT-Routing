# ==============================================================================
# PPDT-Routing (dev 分支) — CMakeLists.txt
# ==============================================================================

cmake_minimum_required(VERSION 3.20)

# --------- 1. 项目声明 ---------
project(
    PPDT_Routing
    VERSION 1.0.0
    DESCRIPTION "PCB 自动布线算法可视化平台（EDA 内核 + Qt 前端）"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows 下默认用 /MDd (Debug) /MD (Release)，与 Qt 官方二进制保持一致。
#
# IMPORTANT: do NOT hardcode "MultiThreadedDLL" here.  Doing so would force
# MSVC Debug builds to link the *release* runtime (MSVCRT.lib) while our
# Debug compile options (/RTC1 /ZI /JMC /Od) still mark object files as
# requiring the *debug* runtime (MSVCRTD.lib).  The linker would then emit:
#     LNK4098: defaultlib 'msvcrtd.lib' conflicts with other library usage
# and the final executable would run against TWO different CRT heaps,
# causing silent heap corruptions whenever the same pointer is allocated
# via one runtime and freed via the other -> very common symptom:
# "Debug build crashes when opening a file, Release build is fine".
set(CMAKE_MSVC_RUNTIME_LIBRARY
    "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# 统一输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# --------- 2. Qt 查找 ---------
# 开启 Qt 自动处理：moc、uic、rcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 兼容 Qt 环境变量：若用户设置了 QTDIR/QT_DIR 但没设 CMAKE_PREFIX_PATH，这里兜底
if(DEFINED ENV{QTDIR} AND NOT "$ENV{QTDIR}" STREQUAL "")
    list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
endif()
if(DEFINED ENV{QT_DIR} AND NOT "$ENV{QT_DIR}" STREQUAL "")
    list(APPEND CMAKE_PREFIX_PATH "$ENV{QT_DIR}")
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

# -------- 2.5. 先推导 Qt kit 根目录（后续 ABI 检查 + PATH 注入 + windeployqt 查找都要用） --------
# find_package(Qt6) 之后，Qt6_DIR 指向  <Qt kit 根> / lib / cmake / Qt6
# 向上三级回到 kit 根目录，bin/ lib/ plugins/ 是它的子目录。
get_filename_component(_QT_KIT_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
set(_QT_BIN_DIR "${_QT_KIT_ROOT}/bin")

# -------- 2.4. ABI 一致性检查：MSVC 生成器必须对应 MSVC 版本的 Qt kit，MinGW 对应 MinGW kit --------
# 否则会出现「链接阶段找不到 Qt6Widgets.lib 的符号」或者「运行时提示 Qt6Gui.dll 找不到」
# 这两种假象，真正原因是 ABI 不匹配。
set(_QT_KIT_MINGW 0)
set(_QT_KIT_MSVC  0)
if("${_QT_KIT_ROOT}" MATCHES "[Mm][Ii][Nn][Gg][Ww]")
    set(_QT_KIT_MINGW 1)
endif()
if("${_QT_KIT_ROOT}" MATCHES "[Mm][Ss][Vv][Cc]")
    set(_QT_KIT_MSVC 1)
endif()
if(MSVC AND _QT_KIT_MINGW AND NOT _QT_KIT_MSVC)
    message(WARNING
        "\n"
        " ##################################################################\n"
        "  ABI MISMATCH DETECTED ! ! !\n"
        "  You are building with a MSVC / Visual Studio generator,\n"
        "  but the Qt kit found is MinGW:\n"
        "    Qt root = ${_QT_KIT_ROOT}\n"
        "\n"
        "  Linking will fail with mysterious missing Qt symbol errors,\n"
        "  or the runtime will report Qt6Gui.dll not found even though\n"
        "  the file exists.  Fix your CMAKE_PREFIX_PATH / QTDIR to point\n"
        "  at the *MSVC* Qt kit, which should have \"msvc\" in its folder\n"
        "  name, for example:\n"
        "    CMAKE_PREFIX_PATH=D:/Qt/6.8.0/msvc2022_64\n"
        "  Then DELETE the build/ folder and re-run configure.bat.\n"
        " ##################################################################\n"
    )
endif()
if(MINGW AND _QT_KIT_MSVC AND NOT _QT_KIT_MINGW)
    message(WARNING
        "\n"
        " ##################################################################\n"
        "  ABI MISMATCH DETECTED ! ! !\n"
        "  You are building with MinGW generator, but the Qt kit found is\n"
        "  an MSVC build:\n"
        "    Qt root = ${_QT_KIT_ROOT}\n"
        "  Fix CMAKE_PREFIX_PATH / QTDIR to point at the mingw_64 kit.\n"
        "  Then DELETE build/ and re-configure.\n"
        " ##################################################################\n"
    )
endif()

# -------- 2.6. 将 Qt\bin 加入 PATH：解决编译期 moc.exe / uic.exe / rcc.exe 找不到 Qt6Gui.dll / Qt6Core.dll 的问题 --------
if(EXISTS "${_QT_BIN_DIR}")
    # VS IDE 中构建时使用的运行路径（影响 F5 调试运行、以及自定义工具的 DLL 搜索）
    set(CMAKE_MSVCIDE_RUN_PATH "${_QT_BIN_DIR}" CACHE STRING "Qt bin directory" FORCE)
    # 更关键：把 Qt\bin 预挂到 PATH，MSBuild 子进程调 moc/uic 时就能找到 Qt6Gui.dll
    set(ENV{PATH} "${_QT_BIN_DIR};$ENV{PATH}")
    message(STATUS "Qt bin directory added to PATH: ${_QT_BIN_DIR}")
else()
    message(WARNING "Expected Qt bin directory not found: ${_QT_BIN_DIR}  -  if the build fails with Qt6Gui.dll not found, check your Qt installation.")
endif()

# 另外也定位一下 windeployqt 的绝对路径（后续 POST_BUILD 步骤用）
if(WIN32 AND NOT TARGET Qt6::windeployqt)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt
        HINTS "${_QT_BIN_DIR}"
        NO_DEFAULT_PATH
    )
endif()

# --------- 3. 源文件分组（头/源分开列，IDE 里分组好看一点） ---------
# 3.1 入口
set(SRC_MAIN
    main.cpp
)

# 3.2 EDA 内核层（Qt-free，纯 C++）
set(SRC_DB
    src_db/dbIO.cpp
    src_db/dbIO.h
    src_db/dbPoint.h
    src_db/dbString.h
    src_db/dbDataBase.h
    src_db/dbParserBase.h
    src_db/dsn/dbData_dsn.h
    src_db/dsn/dbParser_dsn.cpp
    src_db/dsn/dbParser_dsn.h
)

# 3.3 算法层（Qt-free，纯 C++）
set(SRC_ALG
    src_algorithms/src_basics/dataStructAlg.cpp
    src_algorithms/src_basics/dataStructAlg.h
    src_algorithms/src_basics/utils.cpp
    src_algorithms/src_basics/utils.h
    src_algorithms/src_basics/RouterBase.h
    src_algorithms/src_basics/SteinerSolverBase.h
    src_algorithms/src_basics/RouterFactory.cpp
    src_algorithms/src_basics/RouterFactory.h
    src_algorithms/src_dsn/RoutingNode.h
    src_algorithms/src_dsn/Grid.h
    src_algorithms/src_dsn/MST.cpp
    src_algorithms/src_dsn/MST.h
    src_algorithms/src_dsn/RouterMeshless.cpp
    src_algorithms/src_dsn/RouterMeshless.h
)

# 3.4 配置层
set(SRC_CONFIG
    src_config/ConfigBase.h
    src_config/AppConfig.cpp
    src_config/AppConfig.h
    src_config/ConfigManager.cpp
    src_config/ConfigManager.h
)

# 3.5 前端数据适配层
set(SRC_DATA
    src_data/dataStructUI.cpp
    src_data/dataStructUI.h
    src_data/DataBase.h
    src_data/DataParserBase.h
    src_data/DiagramDataBase.h
    src_data/dsn/Data_dsn.cpp
    src_data/dsn/Data_dsn.h
    src_data/dsn/DataParser_dsn.cpp
    src_data/dsn/DataParser_dsn.h
    src_data/dsn/Style_dsn.h
)

# 3.6 服务层（用例编排）
set(SRC_SERVICES
    src_services/RoutingController.cpp
    src_services/RoutingController.h
)

# 3.7 绘图层
set(SRC_PAINT
    src_paint/Diagram.cpp
    src_paint/Diagram.h
    src_paint/DiagramStyle.cpp
    src_paint/DiagramStyle.h
    src_paint/TabPageBase.cpp
    src_paint/TabPageBase.h
    src_paint/TabPageRegister.cpp
    src_paint/pages/dsn/TabPage_dsn.cpp
    src_paint/pages/dsn/TabPage_dsn.h
)

# 3.8 面板层
set(SRC_PANELS
    src_panels/PanelBase.cpp
    src_panels/PanelBase.h
    src_panels/PanelManager.cpp
    src_panels/PanelManager.h
    src_panels/panels/options/OptionsPanel.cpp
    src_panels/panels/options/OptionsPanel.h
    src_panels/panels/options/OptionsPanel.ui
)

# 3.9 UI 主窗口
set(SRC_UI
    src_ui/GeoDisplay.cpp
    src_ui/GeoDisplay.h
    src_ui/GeoDisplay.ui
    src_ui/GeoDisplay.qrc
    src_ui/setupMenuBar.cpp
    src_ui/setupToolBar.cpp
    src_ui/setupTabWidget.cpp
    src_ui/setupPanels.cpp
)

# --------- 4. 可执行目标 ---------
add_executable(PTreeRouting WIN32
    ${SRC_MAIN}
    ${SRC_DB}
    ${SRC_ALG}
    ${SRC_CONFIG}
    ${SRC_DATA}
    ${SRC_SERVICES}
    ${SRC_PAINT}
    ${SRC_PANELS}
    ${SRC_UI}
)

# --------- 5. include 目录 ---------
# 把所有顶层目录加入 include 搜索路径，保持与 VS 工程的 "项目根 /I" 行为一致
target_include_directories(PTreeRouting PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    src_db
    src_algorithms
    src_config
    src_data
    src_services
    src_paint
    src_panels
    src_ui
)

# --------- 6. 链接库（只链接 Qt6::Widgets） + VS 调试器工作目录 ---------
target_link_libraries(PTreeRouting PRIVATE
    Qt6::Widgets
)

# For the CMake-generated .vcxproj (ways 2/3/4) Visual Studio defaults the
# debugger working directory to $(ProjectDir), which is the build/ folder.
# POST_BUILD copies data/ / config/ / Qt DLLs only next to the .exe, so the
# program can't find them when started via F5 -> crashes the moment the user
# tries to open a test file in Debug mode.
#
# $<TARGET_FILE_DIR:PTreeRouting> always expands to bin/<cfg>/ (the folder
# that actually holds the produced .exe), which matches what the user gets
# by double-clicking the .exe directly.
set_target_properties(PTreeRouting PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:PTreeRouting>"
)

# --------- 7. 编译选项（MSVC / MinGW 通用处理） ---------
target_compile_definitions(PTreeRouting PRIVATE
    QT_WIDGETS_LIB
    QT_GUI_LIB
    QT_CORE_LIB
    UNICODE
    _UNICODE
)

if(MSVC)
    target_compile_options(PTreeRouting PRIVATE
        /W3
        /utf-8            # 源码按 UTF-8 解析（与 VS 工程一致）
        /permissive-
        /Zc:__cplusplus
        /Zc:inline
        /Zc:wchar_t
        /Zc:forScope
        /MP               # 多进程编译
    )
    # Debug：保留 Edit & Continue + 运行时检查
    target_compile_options(PTreeRouting PRIVATE $<$<CONFIG:Debug>:/JMC;/ZI;/RTC1;/Od>)
    # Release：优化
    target_compile_options(PTreeRouting PRIVATE $<$<CONFIG:Release>:/O2;/GL>)
    target_link_options(PTreeRouting PRIVATE $<$<CONFIG:Release>:/LTCG>)
else()
    target_compile_options(PTreeRouting PRIVATE -Wall -Wextra)
endif()

# --------- 8. VS 解决方案中的目录分组（source_group，可选，不影响编译） ---------
source_group("01_Main"          FILES ${SRC_MAIN})
source_group("02_db"            FILES ${SRC_DB})
source_group("03_algorithms"    FILES ${SRC_ALG})
source_group("04_config"        FILES ${SRC_CONFIG})
source_group("05_data"          FILES ${SRC_DATA})
source_group("06_services"      FILES ${SRC_SERVICES})
source_group("07_paint"         FILES ${SRC_PAINT})
source_group("08_panels"        FILES ${SRC_PANELS})
source_group("09_ui"            FILES ${SRC_UI})

# --------- 9. 后处理：把 Qt 运行时 DLL 复制到产物目录（Windows 特有） ---------
#   修复说明：
#   ---------------------------------------------------------------------------
#   以往把 windeployqt 命令（含 generator expression $<$<CONFIG:Debug>:--debug>）
#   塞进 set(_DEPLOY_CMD ...) 再给 add_custom_command 展开时，分号会把
#   generator expression 拆成多个"看起来像列表项"的片段，导致 MSBuild 在
#   Debug 配置里看到的是字面字符串 "$<$<CONFIG:Debug>:--debug>" 而非
#   "--debug"，结果 windeployqt 按默认 Release 模式拷 DLL，Debug 版 exe
#   启动就会报 "Qt6Cored.dll / Qt6Widgetsd.dll 找不到"（即使旁边已有
#   Qt6Core.dll 也没用，因为 Debug exe 只认末尾带 d 的版本）。
#
#   解决办法：把 "--debug / --release 的选择"从 generator expression 改写成
#   一个独立的 cmake -P 脚本 RunWindeployQt.cmake，其中用普通的
#   if(CONFIG STREQUAL "Debug") 分支判断。CONFIG 通过命令行
#   -D CONFIG=$<CONFIG> 传进脚本，传递路径不会被 set() 拆分，100% 正确。
#
#   脚本同时做：
#     1) 部署前删除 exe 同级目录下所有陈旧 Qt DLL + plugins（防止
#        Debug/Release 切换时残留旧 DLL 继续迷惑用户）；
#     2) 部署后核查 Qt6Core[d].dll / Qt6Gui[d].dll / Qt6Widgets[d].dll
#        + platforms/qwindows[d].dll 是否真的出现，缺哪个就直接报 WARNING，
#        并明确指出用户 Qt 安装可能没勾 Debug 组件这个根因。
if(WIN32)
    # Resolve windeployqt absolute path (single source of truth for the script)
    set(_WD "")
    if(TARGET Qt6::windeployqt)
        # TARGET_PROPERTY LOCATION_<cfg> may not exist for imported tool targets;
        # prefer looking next to qmake.exe which is always in Qt\bin.
        get_target_property(_QMAKE_LOC Qt6::qmake IMPORTED_LOCATION)
        if(_QMAKE_LOC)
            get_filename_component(_QT_BIN_FROM_QMAKE "${_QMAKE_LOC}" DIRECTORY)
            if(EXISTS "${_QT_BIN_FROM_QMAKE}/windeployqt.exe")
                set(_WD "${_QT_BIN_FROM_QMAKE}/windeployqt.exe")
            endif()
        endif()
    endif()
    if(NOT _WD)
        # Same fallback set as the earlier sections:
        get_filename_component(_QT_KIT_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
        set(_QT_BIN_DIR "${_QT_KIT_ROOT}/bin")
        find_program(_WD_FALLBACK
            NAMES windeployqt
            HINTS "${_QT_BIN_DIR}"
            NO_DEFAULT_PATH
        )
        if(_WD_FALLBACK)
            set(_WD "${_WD_FALLBACK}")
        endif()
    endif()

    if(NOT _WD)
        message(WARNING
            "\n"
            " !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
            "  windeployqt.exe could not be located anywhere under:\n"
            "    ${_QT_BIN_DIR}\n"
            "  The build will still succeed, but when you double-click PTreeRouting.exe\n"
            "  Windows will complain that Qt6Gui.dll / Qt6Core.dll is missing.\n"
            "\n"
            "  How to fix (EITHER):\n"
            "    1. Re-run configure.bat / CMake with a correct CMAKE_PREFIX_PATH\n"
            "       pointing at your Qt kit root (the folder with bin/lib/plugins).\n"
            "    2. Or, after building, copy Qt DLLs manually (see README).\n"
            " !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n"
        )
    else()
        # Configure the helper script once per project (shared for all configs)
        set(RUN_WINDEPLOYQT_EXECUTABLE "${_WD}")
        configure_file(
            "${CMAKE_SOURCE_DIR}/cmake/RunWindeployQt.cmake.in"
            "${CMAKE_BINARY_DIR}/RunWindeployQt_PTreeRouting.cmake"
            @ONLY
        )

        add_custom_command(TARGET PTreeRouting POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    -D "CONFIG=$<CONFIG>"
                    -D "TARGET_FILE=$<TARGET_FILE:PTreeRouting>"
                    -P "${CMAKE_BINARY_DIR}/RunWindeployQt_PTreeRouting.cmake"
            COMMENT "POST_BUILD: deploying Qt runtime DLLs via windeployqt ..."
            VERBATIM
        )
    endif()
endif()

# --------- 10. 打包 data/ 与 config/ 到产物目录，保证双击 exe 就能跑 ---------
add_custom_command(TARGET PTreeRouting POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:PTreeRouting>/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:PTreeRouting>/config"
    COMMENT "Copying data/ and config/ next to executable ..."
)

# 让 CMake 生成的 VS 解决方案里可执行项目名字与 .sln 一致（可选美化）
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT PTreeRouting)
