[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



这个仓库是 Johannes Maria Frank 的个人宠物项目。我用于娱乐和学习，尤其是练习棕地应用中的代理式编码。原始 README 已保存在 `readme_original`。

警告：此项目仍在进行中。



# 本仓库当前能做什么

- 包含完整的 Blitzkrieg 单机源代码和游戏数据。

- 可以在现代 MSVC 工具链下，从干净的 `A7.sln` 解决方案进行干净构建。

- 默认以真正的窗口模式运行，同时仍可通过 `-fullscreen` 命令行选项使用全屏模式。

- 在 `Debug | Win32` 构建中，教程现在已经可以运行；可以选择并移动单位，同时运行时调试仍在继续。

- 包含本机异常报告和现代调试支持。

- 对缺失的库使用 Git 子模块管理。

- 支持在 VS Code Insiders 中进行本机 C++ 和 WinDbg 调试。

- 已删除旧的 BugSlay 崩溃处理，并替换为标准 C++ 断言。



# 使用 Visual Studio 2026 Insiders 运行游戏

1. 使用子模块克隆仓库，或在现有检出中运行 `git submodule update --init --recursive`。

2. 安装 Visual Studio 2026 Insiders / Visual Studio 18，并包含“使用 C++ 的桌面开发”工作负载、VS 2026 MSVC 工具链，以及 Windows 10 或 Windows 11 SDK。

3. 打开 `Sources/src/A7.sln`。

4. 选择 `Debug | Win32`。

5. 构建 `Game` 项目，或构建整个解决方案。

6. 使用 F5 启动 `Game` 项目，或直接运行 `Sources/src/Game/Debug/Game.exe`。

7. 游戏默认以窗口模式启动。如果想使用旧的全屏行为，请在 Game 项目的命令参数中添加 `-fullscreen`。

如果构建无法将 DLL 复制到 `Sources/src/Game/Debug`，请关闭正在运行的 `Game.exe` 进程，然后重新构建。



# 到目前为止的历史

- 找到缺失库并将它们作为 Git 子模块添加进来。

- 创建了一个 Windows XP SP3 虚拟机并安装 Visual Studio 6，以还原旧环境。

- VS6 非常不稳定，经常崩溃，因此改为使用现代工具。

- 安装了 Visual Studio 2010，并将旧的 `.dwr` 项目转换为 `.sln`。

- 将解决方案导入 VS 2026 Insiders，并更新了所有依赖项。

- 经过两周的代理式编码，终于使解决方案从干净状态编译通过，且没有错误和警告。

- 运行时调试现在已经越过初始菜单：教程可以加载并游玩，包括选择和移动坦克。

- 将开发工作转移到 VS Code Insiders，因为它的工具更强大。

- 配置了两条调试路径：本机 C++ 和 WinDbg。

- 删除了 BugSlay，因为它让调试更困难，且在一次崩溃后它自己也崩溃了。

- 用简单的标准 C++ 断言替换了 BugSlay。

- BugSlay 删除后，项目现在可以继续调试，朝着完全运行迈进。



# 路线图

1. 在现在已经可用的教程路径之外，继续清除剩余的运行时异常。

2. 将编译迁移到 Zig。

3. 用开源替代品替换 FMOD、Stingray 和 Bink。

4. 开始逐个用 Zig 代码替换 C++ 项目。
