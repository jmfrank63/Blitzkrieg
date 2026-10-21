[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Blitzkrieg 编辑器测试项目

此文件夹为每个资源编辑器提供一个冒烟测试用的夹具文件夹。

## 使用方法

1. 打开 `manifest.json`，选择对应的编辑器文件夹。
2. 如果文件夹中有复制来的项目文件，直接用 `editor.exe` 打开它。
3. 如果文件夹标记为 `external-project-reference`，请使用该文件夹 `README.md` 中列出的 GOG 外部路径。
4. 如果文件夹标记为 `runtime-reference`，说明没有找到 composer 项目文件；在测试编辑器启动、导入路径和渲染行为时，请把列出的运行时 XML 当作最接近的可用数据参考。

## 复制策略

这里只复制了仓库中已有的文件。GOG 商业安装中的文件只记录路径，不会复制。
