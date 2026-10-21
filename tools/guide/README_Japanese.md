[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Mod 制作ツール

Blitzkrieg の mod を作るための 7 つのコマンドラインツールです。2003 年の Windows 専用ツールに代わるもので、Windows、macOS、Linux で動作します。ここにあるツールはどれも、ゲーム自身のローダーが読み込むファイルを書き出します。`--data` を取るツール以外は、ゲームのインストールを必要としません。

場所: ゲームフォルダーの `Tools/` ディレクトリです。Windows ではそのフォルダーに `bk-pak`、`bk-table`、`bk-image`、`bk-video`、`bk-font` があり、Linux と macOS には `bk-map` と `bk-sprite` もあります。`Tools/` にないツールは `zig build bk-<name>` でソースからビルドでき、`zig-out/bin` に出力されます。

## すべてのツールに共通のルール

- `bk-<tool> --help` は使い方を表示して終了コード 0 で終了します。誤ったコマンドラインは使い方を表示して終了コード 2 で終了します。失敗時は終了コード 1 で、stderr に `bk-<tool>: ` で始まりファイル名を示す 1 行を出力します。
- 成功すると、ツールは要約を 1 行だけ表示し、ほかには何も出力しません。
- パスには `/` と `\` のどちらも使えます。出力パスはお使いのシステムの区切り文字で表示されます。
- ツールは指定した出力だけを書き込みます（説明がある場合は入力の隣に書きます）。`--force` を付けない限り、既存の出力を置き換えることはありません。
- 出力は決定的です。同じ入力からは、どのシステムでも同じバイト列が得られます。
- ゲームデータを読むツールは `--data <dir>` を取り、既定は `./Data` です。マップや mod 内のパスは、ディスク上のパスではなくゲームの Data パスです。

mod はゲーム本体の `Data/` の隣の `mods/<Name>/data/` に置き、名前を示す `mod.xml`（`MODName`、`MODVersion`）を添えます。ゲームは `-mod=<Name>` でそれを実行します。ゲームはそのフォルダー内のファイルと、そこにあるすべての `.pak` 内のファイルを同じように読み込みます。

## 言語

ヘルプとメッセージは 10 言語で表示されます。`--lang <code>`（例: `--lang=ja`）で選択でき、指定がなければ環境変数 `BK_LANG`、次にシステムの言語、最後に英語が使われます。`usage: ` と `bk-<tool>: ` の接頭辞、終了コード、要約行は変わりません。Windows の従来のコンソールウィンドウには CJK のグリフがない場合があります。Windows Terminal なら表示できます。

## 旧ツールと新コマンド

| 旧ツール | 新コマンド |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | なし。`.pak` か通常の zip を配布してください |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | Blender エクスポーター。`docs/blender-replacement.md` を参照 |

## bk-pak: `.pak` アーカイブの作成と展開

`.pak` は zip アーカイブです。`bk-pak pack` は、ゲーム自身のリリース用パッカーと同じ方法で書き出します。すべてのファイルは非圧縮で格納され、名前は並べ替えられ、区切りは `/` です。`unpack` はまずアーカイブ全体を検査し、壊れたエントリーや展開先フォルダーの外に出てしまうエントリーがあれば、何も書き込みません。

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

アーカイブ内の名前は `<dir>` 以下のパスなので、`<dir>` は mod の `data` フォルダーと同じ構成にしてください。Zip64 アーカイブ（65,535 エントリー超または 4 GiB 超）はサポートされません。

例: mod で変更したユニットを mod にパックします。

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font: TrueType フォントからゲーム用フォントを作る

コードページの文字 32..255 を、`Data/Fonts/small` のようなフォントフォルダーにレンダリングします。`1.tfd`（メトリクス、テクスチャ矩形、A/B/C スペーシング、カーニング）、`1.tga`、そしてゲームが読むテクスチャ `1_h.dds` ができます。

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` はピクセル単位のセルの高さで、アセントとディセントの合計です（既定は 20）。
- `--charset` は `ansi`（コードページ 1252、既定）、`easteurope`（1250）、`russian`（1251）、またはコードページ番号です。
- `--chars <file>` は UTF-8 ファイルです。その中にある基本多言語面のすべての異なる文字と ASCII 32..126 がフォントに入り、書体にない文字は除かれます。`--set latin1`、`cyrillic`、`devanagari` は組み込みの範囲を追加し、繰り返し指定できます。
- `--clusters <table>` は language-mods のエクスポートが書き出したクラスター表（`U+E000<TAB>cluster` の行）です。各クラスターは私用領域の文字に割り当てられた 1 つのグリフになり、`--ttf` の書体でシェーピングされます。`--script`（既定 `Deva`）と `--language`（既定 `hi`）でシェーピングを選びます。
- `--ttf` には `.ttf`、`.otf`、`.ttc` ファイルを指定します。書体名はシステムのフォントフォルダーから可能な範囲で探されます。確実なのはファイルを指定する方法です。
- `--weight 600` 以上は太字の書体を選ぶか、太くします。`--italic` は斜体を選ぶか、傾けます。`--no-aa` はアンチエイリアスをオフにします。
- mod に同梱する前に、フォントのライセンスを確認してください。

例: オープンライセンスのフォントファイルから、mod 用の 16 ピクセルのフォントを作ります。

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table: スプレッドシートでステータスファイルを編集する

`export` は、フォルダー内の条件に合うすべての XML ステータスファイルを、タブ区切りの 1 枚のシートに展開します。1 ファイルが 1 行、1 フィールドが 1 列です。Excel、LibreOffice、Numbers で開けます。`import` は、変更したセルをファイルに書き戻し、その値のバイトだけを変更します。それ以外のコメント、順序、書式はそのままです。

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- 列名はフィールドパスです。`MaxHP` は属性、`KeyName;#text` は要素のテキスト、`Guns;item(01);Ammo` はリストの 2 番目の項目です。
- ステータスは、`*.bld` と `*.obt` では `<desc>`、`*.eff` では `<effect>`、それ以外では `<RPG>` から読み込まれます。付属の建物は `1.xml` の `<desc>` の下に持っているため、`--root desc` でエクスポートしてください。独自の新しいステータス種別には `--root <name>` を使います。
- `--only` は `<fields.txt>` の各行の最初の単語で始まる列を残し、`--ignore` はそれらを除きます。
- インポート時、空のセルや `_` は値をそのままにします。存在しないファイルやフィールドは報告されてスキップされます。インポートが何かを追加することはありません。
- `FileName` 列は `export` を実行した場所からの相対パスです。`import` も同じ場所から実行してください。スプレッドシートで保存したシート（引用符付きセル、CRLF）や古い `.txt` シートも受け付けます。

例: あるユニットのヒットポイントを上げます。

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

スプレッドシートで `Stats/tank.xml` 行の `MaxHP` を 120 から 150 に変更し、タブ区切りテキストとして保存してから、次を実行します。

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

`Stats/tank.xml` は `MaxHP="150"` になり、ほかのバイトは変わりません。

## bk-map: マップ、ミニマップ、Blender モデル

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` と `to-bzm` は、マップエディターと同じリーダーとライターで、ゲームのバイナリ `.bzm` と編集可能な `.xml` の間でマップを変換します。
- `minimap` は `<map>_c.dds`、`<map>_l.dds`、`<map>_h.dds` を一辺 `<px>` の正方形で描画します。これらはゲームがマップに表示する画像です。
- ディレクトリは再帰的に検索され、`--out` の下に同じ構成で出力されます。`--out` がなければ、各出力は入力の隣に置かれます。`.bzm` と `.xml` の両方があるマップでは、`minimap` はゲームと同様に新しいほうを使います。
- マップのコマンドにはゲームの `Data`（道路、川、オブジェクト、タイルセット）が必要です。ゲームフォルダーから実行するか、`--data` を渡してください。
- `obj2mod` は Blender エクスポーターの OBJ とそのスケルトン・アニメーションファイルを `.mod` に変換します。`validate-obj2mod` は同じファイルを検査するだけで何も書き込みません。ファイル形式と Blender 側の説明は `docs/blender-replacement.md` にあります。

例: マップを XML として編集し、元に戻してミニマップを描画します。

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

例: Blender からのモデル。

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite: スプライトアニメーションを合成する

2D オブジェクト（橋、柵、建物、エフェクト）のレンダリング済みフレームを 1 枚のテクスチャ `<out-name>.tga` にまとめ、そのアニメーションを `<out-name>.san` に記述します。

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` には `anims.txt`（アニメーションごとに 1 つの `[section]`）とフレームを置きます。アニメーション `<name>` のフレーム `i` は `<desc-dir>/<name>/<iii>.tga`（`000.tga`、`001.tga`、...）です。`default` という名前のアニメーションは、1 フレーム `<desc-dir>/1.tga` だけを持ちます。

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

セクション名はゲームのアニメーションスロット（`idle`、`run`、`death`、...）です。`default` と `effect` はどちらも `idle` なので、3 つのうち 1 つだけを使ってください。`<out-name>` はツールを実行する場所からのパスで、そのフォルダーは作成されます。

例: 2 方向の待機ループと、1 回だけ再生される死亡アニメーションを持つオブジェクト。

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

## bk-image: 地形タイルの画像を変換する

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` は、`OffsetRomb.exe` と同様に、地形タイルの 2 つのひし形の半分を入れ替え、古いオフセット配置で描かれたタイル画像をゲームの配置にします。トゥルーカラーとグレースケールの TGA を読み込み、32 ビットの TGA を書き出します。`--out` がなければ入力を上書きします。2 回実行しても元には戻らないので、元のファイルを保管してください。

例:

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video: Bink ムービーを Ogg Theora に変換する

ゲームは Ogg Theora の `.ogv` ムービーを再生します。`bk-video convert` は、フォルダー内のすべての `.bik` を隣の `<name>.ogv` に変換し、ムービーの最初のオーディオトラックを `<name>.audio.ogg` というサイドカーに出力します。`ffmpeg` と `ffprobe` を実行するため、各自でインストールしてください（標準的なビルドなら Bink デコーダー、Theora、Vorbis を備えています）。これらがないと何も変換されません。

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` の既定は `Data/movies` です。`--no-recurse` はそのフォルダーだけを読みます。
- `--what-if` はジョブを一覧表示するだけで、何も実行しません。既存の出力は `--force` がない限り保持されます。
- `--ffmpeg` と `--ffprobe` は、プログラムが `PATH` にないときにその場所を指定します。
- `--key-frame-interval` は ffmpeg の `-g` です（既定は 1 で、すべてのフレームがキーフレーム）。
- ffmpeg が読めないムービーは stderr に報告されます。残りは変換され、ツールは終了コード 1 で終了します。

例:

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
