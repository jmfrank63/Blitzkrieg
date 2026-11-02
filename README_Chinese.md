[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

这个仓库是 Johannes Maria Frank 的个人业余项目。我把它用于娱乐和学习，尤其是在既有应用（棕地项目）上练习智能体编程。原始 README 保存在 `readme_original` 中。

警告：本项目仍在开发中。


# 项目现状（2026 年 11 月）

移植工作本身已基本完成：游戏可在 Windows、macOS 和 Linux 上原生运行，支持十种语言，所有编辑器也都已移植。目前项目卡在美术资源上，也就是建筑和道具转向后的视图。见下文“瓶颈：美术资源”。


# 已经完成的工作

**游戏**

- 完整的单人游戏及其数据都在这里，用 `zig build`（Zig + clang）构建为原生 64 位程序。战役、教程和随机任务都可以游玩，包括保存/载入、视频和声音。

- 引擎已脱离 Win32 和 DirectX，改用可移植的层：用 SDL3 处理窗口、输入和音频，并使用 SDL GPU 渲染器。

- 闭源库已经去掉：FMOD、Bink 和 Stingray 已被移除。声音和影片现在使用 Ogg Vorbis 和 Ogg Theora。

- 引擎的持久化层 StreamIO.dll 已由 Zig 实现取代（`Sources/src/StreamIOZig`）。

- 遗留的 C++ 代码现在用 clang 和 UBSan 编译，由此发现并修复了数十个 2003 年的构建默默容忍的错误。

- 原版的随机任务已经恢复，每个战役章节会按其脚本的意图解锁对应的历史任务。

- 玩家档案可以通过云同步在不同机器之间跟随玩家。

- 局域网多人游戏可以使用。GameSpy 代码已被移除，互联网对战尚未接通。

**十种语言**

- 游戏、所有编辑器、命令行工具及其指南，以及本 README，都提供英语、俄语、中文（简体）、印地语、西班牙语、法语、德语、葡萄牙语（巴西）、日语和印度尼西亚语版本。

- 其中九种译文以语言模组的形式安装，并配有新的 TrueType 文字引擎，可以绘制这些语言的全部文字。

- 译文由机器生成，仍需母语者审校。欢迎提出修改意见。

**所有编辑器**

- 地图编辑器、带有全部子编辑器的资源编辑器，以及本地化工具包 ELK，都已用 Zig 和 Dear ImGui 重写（`Sources/editor`）。它们通过 C ABI 与 C++ 引擎通信（`Sources/src/EditorBridge`）。

- 旧的、仅限 Windows 的 MFC 编辑器已被删除。游戏可以读取新编辑器写出的内容，测试用游戏自己的读取器来验证这一点。

- 旧的命令行工具已被七个可移植的工具取代，用于制作模组：`bk-pak`、`bk-table`、`bk-image`、`bk-video`、`bk-font`、`bk-map` 和 `bk-sprite`。见 `tools/guide/README.md`。

**重制版选项**

- 选项 > 设置 > 游戏玩法 > 重制版 默认关闭，关闭时使用原版规则。开启后会加入 2003 年的游戏所没有的功能，例如分四步旋转镜头、最快路线寻路，以及让步兵直接从一个容器转移到另一个容器。

- 任务回放正在开发中；其基础部分已经合并。


# 瓶颈：美术资源

旋转镜头意味着每座建筑和每件道具都必须能从四个方向看到，但 2003 年的美术资源只是每个物体从一侧拍下的图片。其他几侧从来没有画过。

到目前为止，我对全部 412 个物体尝试了两种方法：

- 用 2003 年的像素拼出每一张转向后的图片（2026-10-29 放弃：解决一个问题，就会为每个物体带来一个新问题）。

- 渲染简单的 3D 模型，并用同样的那些图片给它们上色。这是项目目前的做法。2003 年美术资源所展示的那一侧看起来是对的，但它从未见过的每一部分都得靠猜。细节也会出问题：画在平屋顶上的老虎窗，换一个方向看就不可能正确。

两种方法都不够好。正确的做法是为每个物体制作真正的 3D 模型，把老虎窗这样的细节做成真实的几何体，并给每个平面贴上纹理，这样每一侧的样子都由形状决定。

问题在于这些模型和纹理从哪里来：

- 手工建模，或借助智能体建模，沿用 2003 年的设计。

- Blitzkrieg 2 的模型，它们同样由 Nival 发布。一次测试渲染表明，它们正是这样构建的，四个方向看起来都很干净，但其中许多建筑与 2003 年同名的建筑并不是同一座，而且许可证并没有明确涵盖它们的美术资源。

- 图像转 3D 的生成器，我还没有在这些美术资源上试过。

目前还没有做出决定。如果你是 3D 美术师，或了解这类工作，我很想听听你的意见。


# Blender 资源转换与验证

Maya40 的替代流水线记录在 `docs/blender-replacement.md` 中。

快速命令：

- 转换：`bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- 仅验证：`bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# 支持的平台

用 `-Dtarget=` 指定目标。生成的目录树按 `zig-out/game/<os>/<arch>/<config>` 排布，因此多个目标和两种优化级别可以并存。

| 平台 | 目标三元组 | 状态 |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | 游戏和编辑器可运行；已在 CI 中测试 |
| macOS (Apple Silicon) | `aarch64-macos` | 游戏和编辑器可运行；已在 CI 中测试 |
| Linux (x86_64) | `x86_64-linux-gnu` | 游戏和编辑器可运行；已在 CI 中用软件渲染器测试 |
| macOS (Intel) | `x86_64-macos` | 可构建；CI 运行器没有可供游戏使用的 GPU，因此游戏在真实的 Intel Mac 上检查 |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | 仅平台测试；游戏和编辑器尚未测试 |
| Windows (MinGW) | `x86_64-windows-gnu` | 仅平台测试；引擎目前还不能在这里编译 |

CI 在与各目标架构相同的运行器上运行每个目标，因此测试套件是真正执行，而不只是链接。Linux 和 macOS 上的部分任务目前失败。

在 Linux 上，窗口图标由程序自己设置，X11 和 KDE 会采用它。Wayland 上的 GNOME 会忽略它，而是从以程序的应用 ID 命名的桌面条目中获取程序坞和 Alt-Tab 的图标（游戏是 `org.blitzkrieg.game`，编辑器是 `org.blitzkrieg.mapeditor`、`org.blitzkrieg.resourceeditor` 和 `org.blitzkrieg.elk`）。安装包附带一个可选的脚本，会把这些条目和图标写入 `~/.local/share`；不会有任何东西替你运行它。在安装文件夹中运行 `Tools/install_desktop_entry.sh`（在检出的仓库中则是 `tools/install_desktop_entry.sh [install-dir]`），用 `Tools/install_desktop_entry.sh --remove` 可以撤销。

只有 `x86_64-windows-msvc` 需要安装 Visual Studio。对于其他所有目标（包括 MinGW），Zig 自带工具链、头文件和导入库。


# 构建与运行

1. 克隆仓库并带上子模块，或在已有的检出中运行 `git submodule update --init --recursive`。

2. 安装 Zig 0.17。对于 `x86_64-windows-msvc`，还需要 MSVC 工具链和 Windows SDK（如果路径与 `build.zig` 中的默认值不同，可通过 `-Dmsvc-include`/`-Dwindows-sdk-include` 及其 `lib` 对应选项配置）。

3. 运行 `zig build install-all -Dtarget=<triple> --release=fast` 来构建游戏、九个语言模组和所有编辑器，或运行 `zig build install-game` 只构建游戏。省略 `-Dtarget` 则为主机构建，去掉 `--release=fast` 则为调试构建。

4. 启动生成的二进制文件，例如 Windows 上的 `zig-out/game/windows/x86_64/release/Game.exe`，或 Apple Silicon 上的 `zig-out/game/macos/arm64/release/Game`（不带 `--release=fast` 时为 `.../debug/...`）。游戏在窗口中运行；`-fullscreen` 可作为命令行选项使用。

5. `zig build package` 会创建可分发的 zip 包，`zig build --help` 会列出所有步骤和测试。


# 迄今为止的历程

- 找到缺失的库，让 2003 年的解决方案在现代 Visual Studio 下从干净状态编译通过，然后玩了第一个教程。

- 把构建迁移到 Zig：`build.zig` 用 clang 编译每个项目，并生成可运行的游戏目录。

- 用 Zig 取代 StreamIO.dll，并让游戏在 Zig 构建下可以从头玩到尾。

- 把游戏移植到 64 位，并用 UBSan 通过教程逐个崩溃地试玩。

- 从 Win32 迁移到 SDL3 和 SDL GPU 渲染器，并让游戏在 macOS 和 Linux 上运行起来。

- 移植了地图编辑器、资源编辑器和 ELK，并删除了 MFC 版本。取代了旧的命令行工具。

- 移除了 FMOD、Bink 和 Stingray。

- 在游戏、编辑器和工具中加入了文字引擎和十种语言。

- 加入了重制版选项和镜头旋转，这导致了上文所述的瓶颈。


# 路线图

1. 越过美术资源这道瓶颈：决定 3D 模型和纹理从哪里来，并为任何需要许可的内容取得许可。

2. 完成任务回放。

3. 让所有 CI 任务重新通过。

4. 在 Linux arm64 和 MinGW 上运行游戏和编辑器，并加入 Windows arm64。

5. 通过取代 GameSpy 的 OpenSpy 替代方案接通互联网多人游戏。

6. 构建一个把 32 位存档转换为 64 位格式的工具。

7. 以 StreamIO 的移植为模板，继续用 Zig 代码逐个替换 C++ 项目。
