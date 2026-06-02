[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



这个仓库是 Johannes Maria Frank 的个人宠物项目。我用于娱乐和学习，尤其是练习棕地应用中的代理式编码。原始 README 已保存在 `readme_original`。

警告：此项目仍在进行中。



# 本仓库当前能做什么

- 包含完整的 Blitzkrieg 单机源代码和游戏数据。

- 可以在现代 MSVC 工具链下，从干净的 `A7.sln` 解决方案进行干净构建。

- 包含本机异常报告和现代调试支持。

- 对缺失的库使用 Git 子模块管理。

- 支持在 VS Code Insiders 中进行本机 C++ 和 WinDbg 调试。

- 已删除旧的 BugSlay 崩溃处理，并替换为标准 C++ 断言。



# 到目前为止的历史

- 找到缺失库并将它们作为 Git 子模块添加进来。

- 创建了一个 Windows XP SP3 虚拟机并安装 Visual Studio 6，以还原旧环境。

- VS6 非常不稳定，经常崩溃，因此改为使用现代工具。

- 安装了 Visual Studio 2010，并将旧的 `.dwr` 项目转换为 `.sln`。

- 将解决方案导入 VS 2026 Insiders，并更新了所有依赖项。

- 经过两周的代理式编码，终于使解决方案从干净状态编译通过，且没有错误和警告。

- 这并不意味着游戏已经完全运行，但它至少有一次进入了教程加载阶段，视频和初始菜单已经能够加载。

- 将开发工作转移到 VS Code Insiders，因为它的工具更强大。

- 配置了两条调试路径：本机 C++ 和 WinDbg。

- 删除了 BugSlay，因为它让调试更困难，且在一次崩溃后它自己也崩溃了。

- 用简单的标准 C++ 断言替换了 BugSlay。

- BugSlay 删除后，项目现在可以继续调试，朝着完全运行迈进。



# 路线图

1. 让游戏在没有异常的情况下运行。

2. 将编译迁移到 Zig。

3. 用开源替代品替换 FMOD、Stingray 和 Bink。

4. 开始逐个用 Zig 代码替换 C++ 项目。
