[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

这个仓库是 Johannes Maria Frank 的个人业余项目。我把它用于娱乐和学习，尤其是在既有应用（棕地项目）上练习智能体编程。原始 README 保存在 `readme_original` 中。

警告：本项目仍在开发中。


# 这个仓库目前能做什么

- 包含完整的 Blitzkrieg 单人模式源代码和游戏数据。

- 使用 `zig build`（Zig + clang）为六个目标构建整个游戏——Windows（MSVC 和 MinGW）、macOS（Intel 和 Apple Silicon）以及 Linux（x86_64 和 arm64）——并在 `zig-out/game/<os>/<arch>/<config>` 下生成可运行的安装目录。只有 MSVC 目标需要 Visual Studio，其余目标由 Zig 自带工具链。

- 可构建并运行原生 **64 位** Game.exe（`zig build install-game -Dtarget=x86_64-windows-msvc`）；32 位目标已停用。战役任务和教程可在 x64 下游玩，包括存档/读档、视频和声音。

- 随附的 StreamIO.dll 已完全由 Zig 实现取代（`Sources/src/StreamIOZig`）——即引擎的整个持久化层：用于存档/读档的二进制对象图、XML 数据树、对象工厂、全局变量、控制台和选项。

- 用 clang 并开启 UBSan 编译遗留的 C++ 代码，发现并修复了数十个 2003 年 MSVC 构建默默容忍的潜在错误，其中包括坦克/炮塔销毁时存在 22 年之久的纯虚函数调用崩溃。

- 在 CI 中验证每个受支持的目标，在各自的运行器上原生运行平台测试套件——Linux x86_64 和 arm64、Windows MSVC 和 MinGW、macOS Intel 和 Apple Silicon——而不只是交叉编译。

- 仍可在现代 MSVC 工具下，从原始的 `A7.sln` 解决方案干净地构建（`Debug | Win32`），作为行为参考。

- 默认在真实窗口中运行，`-fullscreen` 仍可作为命令行选项使用。

- 使用 Git 子模块提供缺失的库。

- 支持在 VS Code Insiders 中调试原生 C++ 和 WinDbg，并在 `tools/zig/` 下提供无界面的崩溃捕获脚本。

- 在 `VideoAssets/` 下提供压缩影片资源打包的辅助流程。

- 已移除旧的 BugSlay 崩溃处理，改用标准 C++ 断言。


# Blender 资源转换与验证

Maya40 的替代流水线记录在 `docs/blender-replacement.md` 中。

快速命令：

- 转换： `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- 仅验证： `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# 支持的平台

`zig build` 接受六个目标。用 `-Dtarget=` 指定其中一个；生成的目录树按 `zig-out/game/<os>/<arch>/<config>` 排布，因此多个目标和两种优化级别可以并存，不会互相覆盖。

| 平台 | 目标三元组 | 状态 |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | 可游玩 |
| macOS (Apple Silicon) | `aarch64-macos` | 可游玩 |
| macOS (Intel) | `x86_64-macos` | 可构建；未经试玩 |
| Linux (x86_64) | `x86_64-linux-gnu` | 开发中 |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | 仅平台层 |
| Windows (MinGW) | `x86_64-windows-gnu` | 仅平台层 |

“仅平台层”表示该目标可以构建并通过平台测试套件，但尚未在其上完整启动游戏构建。

只有 `x86_64-windows-msvc` 需要安装 Visual Studio。对于其他所有目标（包括 MinGW），Zig 自带工具链、头文件和导入库。

每个目标都由 `Cross-platform validation` 工作流在对应架构的运行器上执行，因此测试套件是真正运行，而不只是链接。


# 用 Zig 运行游戏（首选）

1. 克隆仓库并带上子模块，或在已有的检出中运行 `git submodule update --init --recursive`。

2. 安装 Zig（0.17 或更高版本）。对于 `x86_64-windows-msvc`，还需要 MSVC 工具链和 Windows SDK（如果路径与 `build.zig` 中的默认值不同，可通过 `-Dmsvc-include`/`-Dwindows-sdk-include` 及其 `lib` 对应选项配置）。其他目标都不需要它们。

3. 运行 `zig build install-game -Dtarget=<triple> --release=fast` 来游玩，或去掉 `--release=fast` 进行调试构建。省略 `-Dtarget` 则为主机构建。

4. 启动生成的二进制文件——Windows 上为 `zig-out/game/windows/x86_64/release/Game.exe`，Apple Silicon 上为 `zig-out/game/macos/arm64/release/Game`（不带 `--release=fast` 时为 `.../debug/...`）。目录树以其所含的优化级别命名，因此两者不会互相覆盖。

5. `zig build package` 会创建可分发的 zip 包；`zig build test` 会运行 Zig 单元测试和 C++ ABI 冒烟测试。


# 用 Visual Studio 2026 Insiders 运行游戏

1. 克隆仓库并带上子模块，或在已有的检出中运行 `git submodule update --init --recursive`。

2. 安装 Visual Studio 2026 Insiders / Visual Studio 18，并包含“使用 C++ 的桌面开发”工作负载、VS 2026 的 MSVC 工具链以及 Windows 10 或 Windows 11 SDK。

3. 打开 `Sources/src/A7.sln`。

4. 选择 `Debug | Win32`。

5. 构建 `Game` 项目，或构建整个解决方案。

6. 按 F5 启动 `Game` 项目，或直接运行 `Sources/src/Game/Debug/Game.exe`。

7. 游戏默认以窗口模式启动。如果想要旧的全屏行为，请在 `Game` 项目的命令行参数中添加 `-fullscreen`。

如果构建无法把 DLL 复制到 `Sources/src/Game/Debug`，请关闭所有正在运行的 `Game.exe` 进程后重新构建。


# 迄今为止的历程

- 找到缺失的库，并将它们添加为 Git 子模块。

- 搭建了装有 Visual Studio 6 的 Windows XP SP3 虚拟机，以研究原始环境。

- VS 6 不稳定且经常崩溃，因此工作转向现代工具。

- 安装了 Visual Studio 2010，并将旧的 `.dwr` 项目转换为 `.sln`。

- 把解决方案载入 VS 2026 Insiders 并更新了所有依赖项。

- 完成了两周的智能体编程，使解决方案能从干净状态编译，零错误、零警告。

- 运行时调试已越过初始菜单：教程可以加载并游玩，包括选择和移动坦克。

- 为了更好的工具，把开发迁移到 VS Code Insiders。

- 配置了两条调试路径：原生 C++ 和 WinDbg。

- 移除了 BugSlay，因为它让调试更困难，并且曾在一次崩溃之后再次崩溃。

- 用简单的标准 C++ 断言取代了 BugSlay。

- 移除 BugSlay 后，项目已准备好进行有针对性的运行时调试，朝着完整运行迈进。

- 把构建迁移到 Zig：`build.zig` 用 clang 编译每个项目，链接 MSVC 工具链，并生成可运行的游戏目录。

- 用 Zig 实现加 C++ ABI 桥接取代了 StreamIO.dll（引擎的持久化和服务层），并顺带修复了存档/读档对象图格式、XML 读写、选项以及逐帧性能退化。

- 在 Zig 构建下让游戏可以从头玩到尾：战役、教程、登舰、航空、存档/读档往返、视频同步，以及在现代分辨率下正确的小地图。

- 把游戏移植到 64 位：修复了渲染器中的指针截断、x87 时代的 `_control87` 调用、STL 类型上的 `#pragma pack(1)`、x86 内联汇编（改用可移植的 SSE 内部函数）、Lua 单位指针通道以及存档格式的对象 ID。

- 通过教程逐个崩溃地试玩 x64 构建，UBSan 把每个存在 20 年的未定义行为变成精确的文件和行号：小地图坐标、编队哈希、轰炸角度计算等等。

- 从 Win32 迁移到可移植的平台层：用 SDL3 处理窗口、输入和音频，用 SDL GPU 渲染器取代 DirectX 路径，并为每个操作系统提供一个小型原生适配器。

- 让游戏在 macOS 上运行，它在 Apple Silicon 上可以游玩，随后是 Linux。

- 新增 Linux arm64（树莓派级别）和 Windows GNU ABI（MinGW）作为目标，这意味着要在整个构建中把“是不是 Windows？”与“是不是 Visual Studio？”分开。这一过程发现 Intel macOS 一直是在没有任何 Cocoa 适配器的情况下编译的，Linux 任务因缺少 C 运行时而一直是红色，Linux 的 C++ 路径被固定在 x86_64 多架构三元组上。

- 把六个目标全部纳入 CI，每个目标都在对应架构的运行器上运行。


# 路线图

1. 完成对剩余教程和战役的 64 位构建稳定化。

2. 在 Linux arm64 和 MinGW 上启动完整的游戏构建（目前只验证了它们的平台层），并试玩 Intel macOS。

3. 构建一个把 32 位存档转换为 64 位格式的工具。

4. 用开源替代品取代 FMOD、Stingray 和 Bink。

5. 以 StreamIO 的移植为模板，继续用 Zig 代码逐个替换 C++ 项目。
