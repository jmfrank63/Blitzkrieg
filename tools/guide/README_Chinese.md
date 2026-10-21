[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Mod 制作工具

七个用于制作 Blitzkrieg mod 的命令行工具。它们取代了 2003 年仅限 Windows 的工具，可在 Windows、macOS 和 Linux 上运行。这里的每个工具写出的文件都由游戏自己的加载器读取；除非工具带有 `--data` 参数，否则都不需要安装游戏。

位置：游戏文件夹的 `Tools/` 目录。在 Windows 上该文件夹包含 `bk-pak`、`bk-table`、`bk-image`、`bk-video` 和 `bk-font`；Linux 和 macOS 还有 `bk-map` 和 `bk-sprite`。`Tools/` 中缺少的工具可用 `zig build bk-<name>` 从源码构建，生成在 `zig-out/bin`。

## 所有工具遵循的规则

- `bk-<tool> --help` 打印用法并以 0 退出。错误的命令行以 2 退出并打印用法。失败时以 1 退出，并在 stderr 输出一行以 `bk-<tool>: ` 开头、指明文件的信息。
- 成功时，工具只打印一行摘要，不输出其他内容。
- 路径可以使用 `/` 或 `\`；输出路径按您系统的分隔符打印。
- 工具只写入您指定的输出（或在说明如此时写在输入文件旁边）。除非传入 `--force`，否则绝不会替换已有的输出。
- 输出是确定性的：相同的输入在任何系统上都得到相同的字节。
- 读取游戏数据的工具接受 `--data <dir>`，默认为 `./Data`。地图或 mod 中的路径是游戏的 Data 路径，而不是您磁盘上的路径。

mod 位于游戏自带 `Data/` 旁边的 `mods/<Name>/data/` 中，并带有一个命名它的 `mod.xml`（`MODName`、`MODVersion`），游戏用 `-mod=<Name>` 运行它。游戏对该文件夹中的文件和其中每个 `.pak` 内的文件一视同仁地读取。

## 语言

帮助和消息提供十种语言。用 `--lang <code>` 选择（例如 `--lang=ja`），否则使用环境变量 `BK_LANG`，否则使用系统语言，否则使用英语。`usage: ` 和 `bk-<tool>: ` 前缀、退出码和摘要行不会改变。在 Windows 上，旧式控制台窗口可能缺少 CJK 字形；Windows Terminal 可以显示。

## 旧工具，新命令

| 旧工具 | 新命令 |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | 无；发布 `.pak` 或普通 zip |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | Blender 导出器，参见 `docs/blender-replacement.md` |

## bk-pak：制作和解包 `.pak` 归档

`.pak` 是 zip 归档。`bk-pak pack` 按游戏自带发布打包器的方式写出：每个文件都不压缩存储，名称已排序，使用 `/` 分隔符。`unpack` 先检查整个归档，如果任何条目损坏或会落到目标文件夹之外，则不写入任何内容。

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

归档内的名称是 `<dir>` 以下的路径，因此请像 mod 的 `data` 文件夹那样布置 `<dir>`。不支持 Zip64 归档（超过 65,535 个条目或 4 GiB）。

示例：把 mod 中改动过的单位打包进 mod。

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font：用 TrueType 字体制作游戏字体

把某个代码页的字符 32..255 渲染到类似 `Data/Fonts/small` 的字体文件夹中：`1.tfd`（度量、纹理矩形、A/B/C 间距、字距调整）、`1.tga`，以及游戏读取的纹理 `1_h.dds`。

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` 是以像素为单位的单元格高度，即上升部加下降部（默认 20）。
- `--charset` 为 `ansi`（代码页 1252，默认）、`easteurope`（1250）、`russian`（1251），或某个代码页编号。
- `--chars <file>` 是 UTF-8 文件：字体包含其中出现的每个不同的基本多文种平面字符，外加 ASCII 32..126；字体缺少的字符会被略过。`--set latin1`、`cyrillic` 或 `devanagari` 会添加内置范围，可重复使用。
- `--clusters <table>` 是 language-mods 导出写出的簇表（`U+E000<TAB>cluster` 行）：每个簇成为其私用区字符下的一个字形，并用 `--ttf` 字体进行排版整形。`--script`（默认 `Deva`）和 `--language`（默认 `hi`）选择整形方式。
- `--ttf` 接受 `.ttf`、`.otf` 或 `.ttc` 文件。字体名称会尽力在系统字体文件夹中查找；使用文件最可靠。
- `--weight 600` 或更大会选用粗体或加粗字体；`--italic` 选用斜体或倾斜字体。`--no-aa` 关闭抗锯齿。
- 在 mod 中发布字体之前，请检查该字体的许可证。

示例：用开放许可的字体文件为 mod 制作 16 像素字体。

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table：在电子表格中编辑数值文件

`export` 把文件夹中每个匹配的 XML 数值文件展平为一张以制表符分隔的表：每个文件一行，每个字段一列。Excel、LibreOffice 和 Numbers 都能打开。`import` 把您修改过的单元格写回文件，只改动这些值的字节；其余所有内容的注释、顺序和格式保持原样。

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- 列名是字段路径：`MaxHP` 是属性，`KeyName;#text` 是元素的文本，`Guns;item(01);Ammo` 是列表的第二项。
- 数值从以下位置读取：`*.bld` 和 `*.obt` 读 `<desc>`，`*.eff` 读 `<effect>`，其他读 `<RPG>`。随游戏发布的建筑把数值放在 `1.xml` 的 `<desc>` 下，因此导出它们时用 `--root desc`。您自己的新数值类型用 `--root <name>`。
- `--only` 保留、`--ignore` 丢弃以 `<fields.txt>` 每行第一个词开头的列。
- 导入时，空单元格或 `_` 保持原值。缺失的文件或字段会被报告并跳过；导入绝不会添加任何内容。
- `FileName` 列相对于您运行 `export` 的位置；请从同一位置运行 `import`。电子表格保存的表（带引号的单元格、CRLF）和旧的 `.txt` 表均可接受。

示例：提高某个单位的生命值。

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

在电子表格中把 `Stats/tank.xml` 行的 `MaxHP` 从 120 改为 150，另存为以制表符分隔的文本，然后：

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

`Stats/tank.xml` 现在是 `MaxHP="150"`；没有其他字节改变。

## bk-map：地图、小地图和 Blender 模型

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` 和 `to-bzm` 在游戏的二进制 `.bzm` 与可编辑的 `.xml` 之间转换地图，使用与地图编辑器相同的读取器和写入器。
- `minimap` 绘制 `<map>_c.dds`、`<map>_l.dds` 和 `<map>_h.dds`，边长 `<px>` 的正方形，即游戏为地图显示的图像。
- 目录会被递归搜索，并在 `--out` 下镜像；没有 `--out` 时，每个输出都放在其输入旁边。对于同时有 `.bzm` 和 `.xml` 的地图，`minimap` 与游戏一样使用较新的那个。
- 地图命令需要游戏的 `Data`（道路、河流、物体和地形集）：请在游戏文件夹中运行，或传入 `--data`。
- `obj2mod` 把 Blender 导出器的 OBJ 及其骨骼和动画文件转成 `.mod`；`validate-obj2mod` 检查同样的文件，不写入任何内容。文件格式和 Blender 一侧的说明见 `docs/blender-replacement.md`。

示例：以 XML 编辑地图，转换回去并绘制其小地图。

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

示例：来自 Blender 的模型。

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite：合成精灵动画

把 2D 物体（桥梁、围栏、建筑、特效）已渲染的帧打包进一张纹理 `<out-name>.tga`，并在 `<out-name>.san` 中描述其动画。

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` 包含 `anims.txt`（每个动画一个 `[section]`）和各帧：动画 `<name>` 的第 `i` 帧是 `<desc-dir>/<name>/<iii>.tga`（`000.tga`、`001.tga`、...）。名为 `default` 的动画只有一帧 `<desc-dir>/1.tga`。

```text
dirs=<n>                 number of directions (required)
dir<k> seq=<i>,<j>,...   frame indices of direction k (required)
frametime=<ms>           one frame's show time (default 125)
speed=<f>                translation speed (default 0)
cycled=<0|1>             cycled or one-shot (default 1)
shift=<x>,<y>            every frame's shift (default 0,0)
dir<k> shift=<x>,<y>     direction k's frames' shift (default: shift)
frame<i> shift=<x>,<y>   frame i's shift (default: its direction's)
```

节名是游戏的动画槽位（`idle`、`run`、`death`、...）；`default` 和 `effect` 都等同于 `idle`，所以三者只用其一。`<out-name>` 是从您运行工具的位置出发的路径；其文件夹会被创建。

示例：一个带有双方向待机循环和一次性死亡动画的物体。

```text
; smoke/anims.txt, frames in smoke/idle/000..002.tga and smoke/death/000..001.tga
[idle]
dirs=2
dir0 seq=0,1,2
dir1 seq=2,1,0
frametime=100
shift=4,8

[death]
dirs=1
dir0 seq=0,1
frametime=150
cycled=0
```

```text
$ bk-sprite compose smoke out/smoke
wrote out/smoke.tga and out/smoke.san: 2 animations, 5 frames, 32x16 atlas
```

## bk-image：转换地形瓦片图

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` 交换地形瓦片两个菱形的半块，如同 `OffsetRomb.exe` 所做的那样，把按旧偏移布局绘制的瓦片图转为游戏的布局。它读取真彩色和灰度 TGA 文件，写出 32 位 TGA。不带 `--out` 时会覆盖输入文件。运行两次不会还原；请保留您的源文件。

示例：

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video：把 Bink 影片转换为 Ogg Theora

游戏播放 Ogg Theora `.ogv` 影片。`bk-video convert` 把文件夹下的每个 `.bik` 转成其旁边的 `<name>.ogv`，并把影片的第一条音轨转成 `<name>.audio.ogg` 附属文件。它运行 `ffmpeg` 和 `ffprobe`，需要您自行安装（任何标准构建都带有 Bink 解码器、Theora 和 Vorbis）；没有它们就无法转换。

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` 默认为 `Data/movies`。`--no-recurse` 只读取该文件夹。
- `--what-if` 列出任务但不运行。除非 `--force`，否则保留已有的输出。
- 当 `ffmpeg` 和 `ffprobe` 不在 `PATH` 中时，用 `--ffmpeg` 和 `--ffprobe` 指向这些程序。
- `--key-frame-interval` 即 ffmpeg 的 `-g`（默认 1，每一帧都是关键帧）。
- ffmpeg 无法读取的影片会在 stderr 报告；其余仍会转换，工具以 1 退出。

示例：

```text
$ bk-video convert --input-root movies --what-if
Would convert: movies/intro.bik -> movies/intro.ogv
Would extract audio: movies/intro.bik -> movies/intro.audio.ogg
BIK to OGV conversion finished. Converted=0 Skipped=0 AudioConverted=0 AudioSkipped=0 Failed=0
$ bk-video convert --input-root movies
Converting: movies/intro.bik -> movies/intro.ogv
Extracting audio: movies/intro.bik -> movies/intro.audio.ogg
BIK to OGV conversion finished. Converted=1 Skipped=0 AudioConverted=1 AudioSkipped=0 Failed=0
```
