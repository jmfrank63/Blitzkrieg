[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Modding tools

Seven command-line tools for making Blitzkrieg mods. They replace the Windows-only tools of 2003 and run on Windows, macOS and Linux. Every tool here writes files the game reads through its own loaders; none of them needs a game install unless it takes `--data`.

Where they are: in the game folder's `Tools/` directory. On Windows that folder has `bk-pak`, `bk-table`, `bk-image`, `bk-video` and `bk-font`; Linux and macOS also have `bk-map` and `bk-sprite`. One missing from your `Tools/` is built from source with `zig build bk-<name>` and lands in `zig-out/bin`.

## Rules every tool follows

- `bk-<tool> --help` prints the usage and exits 0. A wrong command line exits 2 with the usage. A failure exits 1 with one line on stderr that starts `bk-<tool>: ` and names the file.
- On success a tool prints one summary line and nothing else.
- Paths take `/` or `\`; output paths are printed with your system's separator.
- A tool writes only the outputs you name (or beside its input when it says so). It never replaces an existing output unless you pass `--force`.
- Output is deterministic: the same input gives the same bytes on every system.
- Tools that read game data take `--data <dir>`, default `./Data`. Paths in a map or a mod are the game's Data paths, not your disk's.

A mod lives in `mods/<Name>/data/` beside the game's own `Data/`, with a `mod.xml` naming it (`MODName`, `MODVersion`), and the game runs it with `-mod=<Name>`. The game reads the files in that folder and the files inside every `.pak` there alike.

## Language

Help and messages come in ten languages. Choose one with `--lang <code>` (for example `--lang=ja`), else the `BK_LANG` environment variable, else your system's language, else English. (Tests pin the system's language with `BK_OS_LOCALES`, a comma-separated list that replaces what the system reports; empty means none. It is a test seam, not a setting.) The `usage: ` and `bk-<tool>: ` prefixes, exit codes and summary lines do not change. On Windows, the legacy console window may lack CJK glyphs; Windows Terminal shows them.

## Old tool, new command

| Old tool | New command |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | none; ship the `.pak` or a plain zip |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | the Blender exporter, see `docs/blender-replacement.md` |

## bk-pak: make and unpack `.pak` archives

A `.pak` is a zip archive. `bk-pak pack` writes one the way the game's own release packager does: every file stored uncompressed, names sorted, `/` separators. `unpack` checks the whole archive first and writes nothing if any entry is damaged or would land outside the target folder.

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

Names inside the archive are the paths below `<dir>`, so lay `<dir>` out like the mod's `data` folder. Zip64 archives (more than 65,535 entries or 4 GiB) are not supported.

Example: pack a mod's changed unit into the mod.

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font: make a game font from a TrueType font

Renders characters 32..255 of a code page into a font folder like `Data/Fonts/small`: `1.tfd` (metrics, texture rectangles, A/B/C spacing, kerning), `1.tga`, and `1_h.dds`, the texture the game reads.

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` is the cell height in pixels, ascent plus descent (default 20).
- `--charset` is `ansi` (code page 1252, the default), `easteurope` (1250), `russian` (1251), or a code page number.
- `--chars <file>` is a UTF-8 file: the font gets every distinct character of the Basic Multilingual Plane in it, plus ASCII 32..126; characters the face lacks are left out. `--set latin1`, `cyrillic` or `devanagari` adds a built-in range and can be repeated.
- `--clusters <table>` is the cluster table the language-mods export wrote (`U+E000<TAB>cluster` lines): each cluster becomes one glyph under its Private Use Area character, shaped with the `--ttf` face. `--script` (default `Deva`) and `--language` (default `hi`) choose the shaping.
- `--ttf` takes a `.ttf`, `.otf` or `.ttc` file. A face name is looked up in the system font folders, best effort; a file is the reliable way.
- `--weight 600` or more picks the bold face or emboldens one; `--italic` picks or slants. `--no-aa` turns anti-aliasing off.
- Check the font's licence before you ship it in a mod.

Example: a 16-pixel font for a mod, from an open-licence font file.

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table: edit stat files in a spreadsheet

`export` flattens every matching XML stat file in a folder into one tab-separated sheet: one row per file, one column per field. Excel, LibreOffice and Numbers open it. `import` writes the cells you changed back into the files, changing only those values' bytes; comments, order and formatting of everything else stay as they were.

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- Column names are field paths: `MaxHP` is an attribute, `KeyName;#text` an element's text, `Guns;item(01);Ammo` the second entry of a list.
- The stats are read from `<desc>` for `*.bld` and `*.obt`, `<effect>` for `*.eff`, and `<RPG>` otherwise. Shipped buildings keep theirs in `1.xml` under `<desc>`, so export them with `--root desc`. A new stat type of your own takes `--root <name>`.
- `--only` keeps and `--ignore` drops the columns starting with the first word of each line of `<fields.txt>`.
- On import an empty cell or `_` leaves the value alone. A missing file or field is reported and skipped; import never adds anything.
- The `FileName` column is relative to where you ran `export`; run `import` from the same place. Sheets saved by a spreadsheet (quoted cells, CRLF) and old `.txt` sheets are accepted.

Example: raise one unit's hit points.

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

Change `MaxHP` of the `Stats/tank.xml` row from 120 to 150 in the spreadsheet, save it as tab-separated text, then:

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

`Stats/tank.xml` now has `MaxHP="150"`; no other byte changed.

## bk-map: maps, minimaps and Blender models

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` and `to-bzm` convert a map between the game's binary `.bzm` and editable `.xml`, with the same reader and writer as the Map Editor.
- `minimap` draws `<map>_c.dds`, `<map>_l.dds` and `<map>_h.dds`, `<px>` square, the images the game shows for a map.
- A directory is searched recursively and mirrored under `--out`; without `--out` each output goes beside its input. For a map that has both a `.bzm` and an `.xml`, `minimap` uses the newer one, as the game does.
- The map commands need the game's `Data` (roads, rivers, objects and tilesets): run them from the game folder or pass `--data`.
- `obj2mod` turns the Blender exporter's OBJ and its skeleton and animation files into a `.mod`; `validate-obj2mod` checks the same files and writes nothing. The file formats and the Blender side are in `docs/blender-replacement.md`.

Example: edit a map as XML, convert it back and draw its minimap.

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

Example: a model from Blender.

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite: compose sprite animations

Packs the rendered frames of a 2D object (bridges, fences, buildings, effects) into one texture `<out-name>.tga` and describes its animations in `<out-name>.san`.

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` holds `anims.txt`, one `[section]` per animation, and the frames: frame `i` of animation `<name>` is `<desc-dir>/<name>/<iii>.tga` (`000.tga`, `001.tga`, ...). An animation named `default` has the one frame `<desc-dir>/1.tga`.

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

The section names are the game's animation slots (`idle`, `run`, `death`, ...); `default` and `effect` are both `idle`, so use only one of the three. `<out-name>` is a path from where you run the tool; its folder is created.

Example: an object with a two-direction idle loop and a one-shot death.

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

## bk-image: convert terrain tile art

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` swaps the two diamond-shaped halves of a terrain tile, as `OffsetRomb.exe` did, to bring tile art drawn in the old offset layout into the game's layout. It reads true-colour and greyscale TGA files and writes a 32-bit TGA. Without `--out` it overwrites the input. Running it twice does not give back the original; keep your source file.

Example:

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video: convert Bink movies to Ogg Theora

The game plays Ogg Theora `.ogv` movies. `bk-video convert` turns every `.bik` under a folder into `<name>.ogv` beside it, and the movie's first audio track into a `<name>.audio.ogg` sidecar. It runs `ffmpeg` and `ffprobe`, which you install yourself (any stock build has the Bink decoder, Theora and Vorbis); nothing is converted without them.

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` defaults to `Data/movies`. `--no-recurse` reads only that folder.
- `--what-if` lists the jobs and runs nothing. Existing outputs are kept unless `--force`.
- `--ffmpeg` and `--ffprobe` point at the programs when they are not on `PATH`.
- `--key-frame-interval` is ffmpeg's `-g` (default 1, every frame a key frame).
- A movie ffmpeg cannot read is reported on stderr; the rest still convert and the tool exits 1.

Example:

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
