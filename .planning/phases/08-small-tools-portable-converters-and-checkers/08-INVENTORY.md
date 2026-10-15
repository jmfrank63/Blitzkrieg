# Phase 8: Small tools - Inventory

**Taken:** 2026-09-30, from the tree at `32b9233ce` (worktree `map-editor-6`).
**Method:** read each tool's `main`, its usage text and its inputs/outputs; `strings` on the shipped binaries that have no source here; grep of `build.zig`, `tools/zig/stage.zig`, `tools/zig/build_support.zig`, `tools/zig/game_install.ps1`, `docs/` for who still references them; count of the data each format feeds (`find Data`).
**Legend:** PORT = portable replacement built by `zig build`; REPLACE = an existing tool or OS facility covers it; DROP = removed with the written reason; NOT A TOOL = listed only so nobody re-checks it. Decision ids (D-nn) are in `08-CONTEXT.md`. Plans are 08-01..08-06 (split in `08-CONTEXT.md`, "Plan split").

## A. Shipped Windows binaries in `Sources/Tools` (all tracked in git, 8.8 MB)

| Binary | What it does | Inputs / outputs | Referenced by | Decision | Plan |
|---|---|---|---|---|---|
| `ExcelExporter.exe` (also a second copy `Sources/src/bin/ExcelExporter.exe`, 1.5 MB) | Flattens the `*.xml` stat files of a folder (`*.bld`, `*.obt`, `*.eff`, `*.msh`... root node `RPG`/`desc`/`effect`) into one sheet, one row per file, one column per XML field path (`FileName<TAB>path<TAB>path...`, tab separated text despite the name); import mode writes edited cells back into the XML files. Used through ODBC Excel driver + MSXML: Windows only. | export: `<mask> <folder> <out.txt> [-i] [crap file] [-nr]`; import: `<sheet.txt>` | `stage.zig` `copyEditors`, `game_install.ps1` | **DONE**, replaced by `bk-table` (D-08..D-11), staged into `Tools/`. The format is plain TSV, so the port needs no spreadsheet library; Excel, Numbers and LibreOffice open it. Both copies deleted in 08-03 with their `stage.zig` `copyEditors` and `game_install.ps1` entries. | 08-03 |
| `FontGen.exe` | Renders a font face (GDI `CreateFont`, 8 bit code page, chars 32..255) into a glyph atlas `1.tga` + font description `1.tfd` (`SFontFormat`: metrics, per char UVs and A/B/C spacing, kerning). Shipped `Data/Fonts/{tiny,small,medium,large}/1.tfd` were made this way. Windows only (GDI). | `-h# -w# -it -a0 -pitch -<charset> "<Face>"` -> `1.tfd`, `1.tga` in cwd | build.zig `addFontGen` (Windows only), ELK `CFontGen` (same GDI code, duplicated) | **PORT** as `bk-font` on a shared glyph-atlas library, TrueType rasteriser = the `imstb_truetype.h` already vendored under `vendor/dcimgui` (D-12..D-15). **Link to Phase 7 (ELK):** ELK's `CFontGen` (`ELK_Types.h`, `ChooseFontsDialog.cpp`, `ELK_StaticMethods.cpp`) is the same generator with a dialog; the library in 08-02 is what ELK's font page must call, so 08-02 is scheduled first and may run before Phase 7's font plan. | 08-02 |
| `image.dll`, `streamio.dll` | 2002 builds of the engine DLLs that `FontGen.exe` / `ExcelExporter.exe` load from their own directory. | - | those two exes only | **DROP** with their owners (dead weight once the exes go; the engine modules are built by `zig build` now). `ExcelExporter.exe` is gone; they stay until `FontGen.exe` goes. | 08-02 / 08-03 |
| `zip.exe`, `unzip.exe` | Info-ZIP 2.x/5.x. A `.pak` is a plain zip; these were the modder's way to make and unpack one. | files -> `.pak`; `.pak` -> files | nothing in the tree calls them | **REPLACE** by `bk-pak` (D-05..D-07): reuses `tools/zig/package.zig`'s deterministic stored-zip writer and Zig `std.zip` for extraction; macOS and Windows have no bundled `zip -X` with stable order, so a deterministic tool is worth having. | 08-01 |
| `zip2exe.exe` | Wraps a zip in a self-extracting Windows exe stub. | zip -> exe | nothing | **DROP**: self-extracting stubs are Windows-only, unsigned exes that antivirus flags; distribution is the package zip / installer (see 999.2). | 08-06 |
| `WhereIS.exe` | Find files by mask(s) and run a command per match (`WhereIS *.bak [ del @$1@ ]`). Generic dev utility, no game knowledge. | masks + command | nothing | **DROP**: `find`/`xargs`, PowerShell `Get-ChildItem`, Zig's `delete_matching_files.zig` already cover it. | 08-06 |
| `A7ExportModel.mll` | Maya 4.x plug-in exporting models to Blitzkrieg `.mod`. Needs Maya 4 (2002). | Maya scene -> `.mod` | `stage.zig` lists the stale `A7ExportModel.dll` as a leftover to delete | **DROP**: already replaced by Blender (`tools/blender/blitzkrieg_export.py`, `docs/blender-replacement.md`), whose `-obj2mod` leg lives in `bzmconvertor` and is ported (row B). | 08-06 |
| `dds.8bi` | Photoshop 6/7 DDS plug-in (Nvidia-era). | psd <-> dds | nothing | **DROP**: 32-bit Photoshop plug-in; modern DDS export is in GIMP/Krita/Photoshop 2022 (native) and the game reads the DDS variants the season/texture tools already write (`tools/zig/season_textures.zig`, `tools/image/DxtCodecTest.cpp`). | 08-06 |
| `error.txt` | A crash log from FontGen, committed by accident. | - | - | **DROP** (delete). | 08-06 |

## B. Tool sources under `Sources/src`

| Source | What it does | Inputs / outputs | State | Decision | Plan |
|---|---|---|---|---|---|
| `excelexporter/` (15 files, MFC + ODBC + MSXML) | Source of row A `ExcelExporter.exe`. | see above | deleted | **DONE**: sources and their `A7.sln` project deleted in 08-03 after `bk-table` passed | 08-03 |
| `FontGen/` (10 files) | Source of `FontGen.exe`. | see above | built by `addFontGen` on Windows only | **PORT**; the `addFontGen` step and `fontgen_sources` are replaced by `bk-font`; `docs/zig-build-transition.md` line 91 updated | 08-02 |
| `bzmconvertor/` (`main.cpp`, 799 lines) | Four modes: (1) `<dir>` / `<map.xml>` : map XML -> binary `.bzm` (default) or `-nobzm`; (2) `-fences`: rewrite legacy frame indices of fences, entrenchments and bridges to type indices (a 2002 data migration); (3) `-obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt]` (added for the Blender pipeline); (4) `-validateobj2mod`. Reads `.\data\*.pak` and the object database. Backslash paths, Windows only. | XML/BZM maps; OBJ + sidecars -> `.mod` | in `A7.sln`; not in `build.zig`. `docs/blender-replacement.md` and `tools/blender/*` name it as the modder path | **DONE** (three of four modes), replaced by `bk-map`: XML -> BZM is `bk-map to-bzm`, `-nobzm`'s XML output is `bk-map to-xml` (new: BZM -> XML), both through `NMapFile::Read`/`Write` (`tools/zig/bk_map.cpp`); `-obj2mod` and `-validateobj2mod` moved to `tools/zig/bk_map_obj2mod.cpp` as `bk-map obj2mod` / `validate-obj2mod`, sidecar formats unchanged. `-fences` is **DROP** (one-time migration, every shipped map converted; the M1 open/save sweep proves the shipped set loads). Built and tested on the data-only startup with the shared `tools/zig/tool_host.{h,cpp}`; staged into `Tools/` on Linux and macOS. See section G. The source and its `A7.sln` entry stay until the phase's deletion step. | 08-04 |
| `imagedefrag/` (20 files, `main.cpp` 705 lines) | Named for an image packer, but the live code (lines 476-542) is the "MiniMap Creator": for every `.bzm`/`.xml` map in a folder call `CMapInfo::CreateMiniMapImage` and write the minimap image (`<size> <folder> <pattern>`). The image packer (`ImagePack`, `ImageEdge`), `TerrainGenerator`, `PNoise`, `BetaSpline` and the `l/m/w/p/u` modes are either commented out (`/* */` at 543-646) or unreachable experiments; nothing else in the tree includes them. Hard-coded `C:\a7\data\*.pak`. | maps -> minimap images | in `A7.sln`; not in `build.zig` | **DONE** for the minimap creator, replaced by `bk-map minimap <map\|dir> --size N [--out] [--data]`: it reads the map with `NMapFile::Read`, calls `UnpackFrameIndices` and then the same `CMapInfo::CreateMiniMapImage` the Map Editor's bridge (`BkEditorCreateMiniMapImage`) and `GameTT/MinimapCreation.cpp` call, with imagedefrag's parameter (one square size, DDS: `_c`, `_l`, `_h`). No fork. **DROP the rest** (image packer, terrain generator, Perlin noise, beta spline: dead code, not ported; history keeps it). The source and its `A7.sln` entry stay until the phase's deletion step. | 08-04 |
| `spcomp/` (`main.cpp`, 205 lines) | Sprite composer: reads a sprite-sequence description database (directions, frame lists, per-frame shifts, speed, cycle) plus the rendered frame images from a folder, packs every frame into one atlas (`IImageProcessor::ComposeImages`) and writes `<name>.tga` + `<name>.san` (`SSpriteAnimationFormat`). The game ships **3799** `.san` files (bridges, fences, buildings, effects, editor markers), so this is how any new 2D-sprite object gets made. Windows only (backslashes, MSVC-only headers). | desc dir -> `.tga` + `.san` | in neither `A7.sln` nor `build.zig` | **PORT** as `bk-sprite compose` (engine-backed C++, reusing `ComposeImages` and the `.san` saver so bytes match). Phase 6 (Resource Editor animation editors) reads the same `.san`; it consumes the library, not a copy. | 08-05 |
| `Tools/OffsetRomb/` (`main.cpp`, 212 lines) | "Romboid tile offsetter": swaps two diamond-shaped halves of a terrain tile TGA in place with Bresenham lines (`FlipRomb1`, `FlipRomb2`). Pure pixel permutation. Still needed by a modder converting tile art authored in the old offset layout. | `<in.tga> [<out.tga>]` | in neither `A7.sln` nor `build.zig` | **PORT** (cheap, std-only Zig) as `bk-image offset-romb`; TGA read/write reused from `tools/zig/season_textures.zig`. Test is a permutation golden (round trip twice is not the identity, so the golden is explicit). | 08-05 |
| `betakeygen/` (9 files) | Generates `beta.key` (user name + expiry + CRC32 with a 20 byte secret). The game's check is under `_DO_BETA_CHECK`, which only this project defines. | name -> `beta.key` | Windows-only steps in `build.zig`; listed in `build_support.zig` `excluded_utilities`; `stage.zig` deletes a stale `BetaKeyGen.exe` | **DROP**: copy-protection keygen for a 2003 beta with an embedded secret; nothing in the game reads a beta key. Also delete the dead `Main/BetaKey.cpp` / `CheckBetaKey` call site only if it is a compiled-out no-op (checked by 08-06). | 08-06 |
| `buildversion/` (12 files) | Bumps a build number in a Visual C++ 6 `.dsw` workspace and checks files in/out of Visual SourceSafe. | `.dsw` + project names | Windows-only steps in `build.zig`; `excluded_utilities` | **DROP**: VC6 workspaces and SourceSafe do not exist any more; the version comes from `build.zig.zon`/the release step. | 08-06 |
| `AutoRun/` (36 files, MFC) and `Sources/autorun/` (`autorun.exe`, `autorun.inf`, `autorun.pak`, `mfc42.dll`, `msvcp60.dll`, `msvcrt.dll`, icon; 6 MB) | CD-ROM autorun menu with an MFC dialog, its own zip reader and a bundled VC6 runtime. | - | in `A7.sln` (line 22); not in `build.zig` | **DROP**: optical media are not a distribution channel; the redistributed `mfc42`/`msvcp60`/`msvcrt` DLLs are 1998-era Microsoft binaries that `stage.zig` already strips from installs. | 08-06 |
| `PlanePathTest/` (46 files, MFC dialog) | Interactive sandbox drawing plane manoeuvre paths (`CManuver*`, `PlanePathMath`, `PlaneFlightExecutor`). Private copy of code that lives in `AILogic` (`PlanePath.cpp` ...). | - | in `A7.sln`; not in `build.zig`; nothing else includes it | **DROP** (dev sandbox on a fork of AILogic files; the maths is covered by the game's own plane behaviour tests where they exist). Written reason recorded so a later flight-model work can recover it from history. | 08-06 |
| `InterfaceSystemTest/` (71 files) | Old UI test bed: a fork of `UI/` with a Win32 host. | - | in `A7.sln`; not in `build.zig` | **DROP**: the live `UI` module is built and exercised by the game and `scene_screen_scale_test`/`ui_*_test`; the fork has drifted (diff against `UI/` differs in every file). | 08-06 |
| `GameTT/` (154 files) | **Not a tool.** It is the game logic module, shipped as `GameTT.dll/.dylib` and listed in the runtime file sets in `build.zig`. | - | built | NOT A TOOL | - |
| `Sources/sdk/{miniaudio,openspy-core,stb,xiph}` | **Not tools.** Vendored runtime libraries (audio, ogg/vorbis/theora, `stb_vorbis`); `openspy-core` is an empty directory. | - | built into the runtime | NOT A TOOL (the empty `openspy-core` dir is left to the GameSpy replacement work) | - |
| `Sources/src/bin/{editor.exe, MapEditor.exe}` | Committed Windows binaries of the two MFC editors; staged into `Editors/` on Windows. | - | `stage.zig` `copyEditors` | **Out of scope**: they go with Phase 5 (MFC map editor) and Phase 6 (resource editor). Phase 8 removes only `ExcelExporter.exe` from that list (done in 08-03). | - |
| `ELK` (`Sources/src/ELK`) | Not a small tool; own phase. Its font generator is the FontGen link above. | - | - | Phase 7 | - |

## C. `tools/` (repo scripts)

| Item | What it does | Decision |
|---|---|---|
| `tools/zig/*.zig` (`package.zig`, `stage.zig`, `season_textures.zig`, `compare_trees.zig`, `delete_matching_files.zig`, `compile_gfxgpu_shaders.zig`, `verify_*.zig`, ...) | Portable already (macOS + Windows). `package.zig` is the deterministic stored zip writer; `season_textures.zig` is the first modder-facing Zig tool and the pattern for the new ones. | Keep. `package.zig` is refactored into a library shared with `bk-pak` (08-01). |
| `tools/blender/` (`blitzkrieg_export.py`, tests, README) | Blender exporter, already portable. Emits OBJ + skeleton/animation sidecars for `bzmconvertor -obj2mod`. | Keep. **Done** (08-04): the exporter runs `bk-map obj2mod` (default `bk-map` on `PATH`), and its tests, `check_blender_export_practical.ps1` (now `zig-out\bin\bk-map.exe`), its README, `docs/blender-replacement.md` and the top-level `README.md` name the new command. |
| `tools/data/gog_scenarios.py` | GOG scenario hashing; Python. | Keep (not Windows-only). |
| `tools/video/convert_bik_to_ogv.ps1` | Converts the original Bink `.bik` movies to Ogg Theora with `ffmpeg`/`ffprobe` and an audio sidecar. Modders converting their own videos need it, and PowerShell is not installed on stock macOS. | **PORT** as `bk-video convert` (thin, std-only Zig wrapper over `ffmpeg`/`ffprobe` on `PATH`, same flags and sidecar naming) in 08-06. |
| `tools/video/check_*.ps1`, `tools/audio/check_*.ps1`, `tools/stingray`, `tools/openspy`, `tools/ui`, `tools/image/check_dxt_codec.ps1`, `tools/CheckRuntimeProprietaryRefs.ps1` | 40+ scripts asserting that a scaffold, a header or a proprietary reference is gone. CI and developer guards, not modder tools; run on `win-home` and Windows CI. | **Keep, out of scope.** A guard is ported only when it must run on the macOS path (none does today); recorded as a deferred idea. |
| `tools/zig/*.ps1` (`capture_*`, `run_game`, `game_install`, `test_game_startup`, `verify_x64_runtime`, `fix_globals_loaders`, `run_x64_diag`) | Windows dev/CI drivers around the Windows build. | Keep, out of scope. `game_install.ps1` lost its `ExcelExporter.exe` copy (08-03, done). |
| `tools/install_desktop_entry.sh`, `tools/profile/flamegraph.sh`, `tools/missions/run_random_mission.sh`, `tools/shadercross` | POSIX or cross-platform already. | Keep. |

## D. What the tool set looks like after Phase 8

| New tool | Language / kind | Replaces | Tests |
|---|---|---|---|
| `bk-pak` | Zig, std only | `zip.exe`, `unzip.exe`, `zip2exe.exe` | `zig build test-bk-pak` |
| `bk-font` + glyph-atlas library | C++ library + console exe on the engine's `SFontFormat` saver; rasteriser `imstb_truetype.h` | `FontGen.exe` (and the GDI code in ELK) | `test-bk-font` |
| `bk-table` | Zig, std only | `ExcelExporter.exe` | `test-bk-table` |
| `bk-map` | C++ on the data-only startup | `bzmconvertor` (3 modes), `imagedefrag` minimap creator | `test-bk-map` (opt-in: `test-bk-map-all`) |
| `bk-sprite` | C++ on the data-only startup | `spcomp` | `test-bk-sprite` |
| `bk-image` | Zig, std only | `OffsetRomb` | `test-bk-image` |
| `bk-video` | Zig, std only wrapper | `convert_bik_to_ogv.ps1` | `test-bk-video` |

Everything else in sections A-C is dropped, left alone, or not a tool, with the reasons above.

## E. The FontAtlas API (08-02), for Phase 7's font plan

D-13 makes ELK's font page call this library instead of its GDI `CFontGen`. Shipped in 08-02 as `Sources/src/FontAtlas/FontAtlas.{h,cpp}`: plain C++ on the engine headers (include after the module's `StdAfx.h`), no HDC, no dialog, no globals, no file IO, rasteriser `imstb_truetype.h` compiled `static` inside `FontAtlas.cpp` (so it can sit beside Dear ImGui's copy). A consumer compiles `FontAtlas.cpp` and links `Formats` (for `SFontFormat::operator&`) and `Misc` (`NStr`).

```cpp
namespace FontAtlas
{
const int FIRST_CHAR = 32, LAST_CHAR = 255;   // FontGen's range, bytes of the code page
const BYTE DEFAULT_CHAR = 0x1F;               // GDI's tmDefaultChar for TrueType: the .notdef box

struct SFontRequest
{
	const BYTE *pFontData; int nFontDataSize; // a .ttf/.otf/.ttc in memory, read only during Build
	int nFaceIndex;                           // face in a collection (0)
	std::string szFaceName;                   // stored lower-cased; empty = the face's family name
	int nHeight;                              // cell height in px, ascent + descent (FontGen -h; 20)
	int nWeight;                              // 0..1000 (400); >= 600 emboldens a face that is not bold
	bool bItalic;                             // slants a face that is not italic
	bool bAntialias;                          // false = every pixel 0 or 255 (true)
	int nCodePage;                            // 1250, 1251 or 1252 (1252)
};
struct SFontImage { int nSizeX, nSizeY; std::vector<DWORD> pixels; };  // top row first, 0xAARRGGBB, white, alpha = coverage
struct SFontAtlas { SFontFormat format; SFontImage image; };

bool GetCharSet( int nCodePage, BYTE *pCharSet );       // the cCharSet byte (0, 204, 238)
WORD ToUnicode( int nCodePage, BYTE c );                // the code page byte's character
int FindFace( const BYTE *pFontData, int nFontDataSize, const std::string &szName, bool bBold, bool bItalic );  // face index or -1
bool Build( const SFontRequest &request, SFontAtlas *pAtlas, std::string *pszError );
}
```

`Build` is D-13's `FontAtlas::Build(SFontRequest) -> {SFontFormat, image}`: false with the reason for an unreadable font, an unknown code page, a height outside 4..256 or an atlas above 4096x4096. The layout is FontGen's (`FontGen.cpp` `MeasureFont`/`DrawFont`/`CreateFontFormat`): characters 32..255 plus 0x1F, sorted, translated to Unicode through the code page; texture size from the same 2:1/1:1 power-of-two estimate, growing until everything fits; cells in rows of `nHeight` pixels, 2 pixels apart, `B + |C|` wide; UVs with FontGen's half-texel offset; kerning pairs rounded to whole pixels, zero pairs left out. Kerning comes from the face's `kern` table or the GPOS pair lookups `imstb_truetype.h` reads (GDI's `GetKerningPairs` read only `kern`). The image becomes files through the engine's image processor (`IImageProcessor::CreateImage`, `SaveImageAsTGA`, `Compress(GFXPF_ARGB8888)` + `SaveImageAsDDS`) and the format through the structure saver, chunk 1, which is what `bk-font` (`tools/zig/bk_font.cpp`) does and what ELK's font page needs to do.

Output contract, changed in 08-02: `1.tfd` and `1.tga` as FontGen wrote them, plus `1_h.dds`. The game's texture manager reads only DDS (`GfxGpuObjectFactory.cpp`, `ResolveStreamName`: the quality suffix, then `_h`, `_c`, `_l`); FontGen's `1.tga` reached it through the old resource pipeline, which nothing in the tree now has a font path for. `1_h.dds` is the lossless 32-bit level every quality setting falls back to, the same layout as the shipped `Data/Fonts/*/1_h.dds`.

Acceptance as built (`zig build test-bk-font`): the D-14 structural checks on ProggyClean (`tools/zig/fixtures/fonts/`, MIT, D-15) at 8, 13, 16, 20, 24 and 48 pixels in all three code pages, bold, italic and `--no-aa`; the same checker passes the four GDI-made shipped fonts; the files `bk-font` wrote are read back by the engine's structure saver, TGA reader and DDS reader and equal an in-process build. The in-game `-mod` capture with F9 measured from the TGA (D-14's one look) is the maintainer's step and has not been taken.

## F. `bk-table` as built (08-03)

`bk-table` (`tools/zig/bk_table.zig`, reader/patcher `tools/zig/bk_table_xml.zig`) keeps ExcelExporter's sheet: the root is the first child of the document element with the root's name; attributes are `<prefix><name>`; non-empty text, CDATA and comment children are `<prefix>#text`, `#cdata-section` and `#comment` (MSXML's node names); a child element adds `<name>;`, and the n-th `item` child is `item(NN)` (`%.2d`). Text is trimmed and whitespace-only text is no field, as MSXML's default did. Columns are the byte-ordered union; when two fields share a path, the first one wins. Shipped data uses comments (4,811 files, almost all in Sounds and Scenarios) and no CDATA, entities, DOCTYPE or declared encodings; the reader accepts all of these except DOCTYPE and non-UTF-8 encodings, which it refuses.

Deliberate differences from the legacy tool, decided in 08-03:

- Rows are sorted and name files as `<folder>/<relative path>` with `/` on every host, so a sheet made on one OS imports on another (the legacy order and separators were whatever Windows enumeration gave).
- The root is looked up per file by extension on export as well as on import (the legacy export compared the mask string with `*.bld`). Shipped buildings' `1.xml` hold `<desc>`, so they need `--root desc`.
- `--ignore` drops only the paths a field names. The legacy subtree test matched any path that shared a prefix with an ignored field, so ignoring `Armor;Front` also dropped `Armor;Side`. `--only` behaves exactly as before.
- Export fails as a whole (exit 1, no sheet written) when a file is malformed, has no root, or holds a tab or line break that a sheet cannot carry. Each such file is named with its line and column. The legacy tool wrote an empty or broken row instead.
- A value that starts with `"` is written quoted, the way Excel writes it, and quoted cells are unquoted on import, so a sheet saved by a spreadsheet (CRLF line ends, quoted cells) reads back the same.
- Import never creates anything. A cell for a path the file lacks (including text in an empty element such as `<KeyName></KeyName>`) is reported and skipped, per D-09. The legacy tool created the node.

Acceptance as built (`zig build test-bk-table`): the fixture folder `tools/zig/fixtures/small-tools/stats` exports to the expected sheet byte for byte, with and without `--no-recurse` and with `--only`. A one-cell import into a file with a BOM, comments, CDATA, entities, both quote styles and mixed line ends changes only that value's bytes. Export, import, export is stable, and an edited sheet reads back as edited. A missing file or path is reported, skipped and never created. The root table and `--root` hold on export and on import. Twenty-four malformed inputs fail with their exact line and column. By hand, on a copy of the shipped `Data/Units` (302 files): the unchanged round trip is byte-identical, and editing one `MaxHP` cell changes those bytes and nothing else.

## G. `bk-map` as built (08-04)

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

What moved where: the map conversions are `NMapFile::Read` and `Write` (`Sources/src/MapFile`, the Map Editor's reader and writer, which also write the `SQuickLoadMapInfo` chunk); the minimap is `CMapInfo::CreateMiniMapImage` with Main's object database loaded, after `UnpackFrameIndices` as the editor bridge and the game do; obj2mod is bzmconvertor's code in `tools/zig/bk_map_obj2mod.cpp`; the path and module-finding helpers bk-font had are `tools/zig/tool_host.{h,cpp}`, shared by both tools.

Differences from the legacy tools, decided in 08-04:

- A directory is searched recursively and mirrored under `--out` as before; without `--out` the output goes beside each input, as before. Existing outputs are refused unless `--force`, checked for every file before anything is written, and the run stops at the first map that does not read, naming it (bzmconvertor carried on; imagedefrag printed `Done 0`).
- `minimap` on a directory takes `.bzm` and `.xml` maps and draws a map that has both once, from the `.xml` only when it is strictly newer (the game's rule); imagedefrag took one pattern and drew both.
- Reading a map needs the game's Data (road and river descriptions resolve through the storage while the map is read), so the map commands take `--data` (default `./Data`, D-04). `minimap` loads the object database from that Data's `objects.xml` and writes nothing there (the engine's `objects.gdb` cache is not created on this path; checked after the test runs).
- obj2mod's messages start `bk-map: `, the legacy text after it; a face naming a missing vertex now says so (the legacy exe exited 0xDEAD silently); `Validation passed` is printed by `validate-obj2mod` only, so `obj2mod` prints one summary line; the `.mod`'s folder is created when missing.
- Windows: `bk-map.exe` is built and tested in `zig-out/bin`, beside its DLLs, but not staged into `Tools/`, because a Windows executable there has no loader-relative path to the game folder's DLLs (Linux and macOS use `$ORIGIN/..` and `@loader_path/..`).

Bytes, where the format allows (D-17): the shipped files' own bytes do not come back, for two reasons the formats leave open, and the map is equivalent either way. A `.bzm` saves road and river points raw (`SVectorStripeObjectPoint`, three padding bytes after `bKeyPoint`); the tool that wrote the shipped maps left memory in that padding, and an `.xml` carries none, so it comes back as zeros (588 bytes of `Allies/Norway/Narwick.bzm`). And both savers write a `std::unordered_map` (the reinforcement groups) in iteration order, which follows the order it was filled in, so the first `.xml` of a shipped map can list the groups in another order than the next one (16 of the 57 `Data/Maps` maps; the order differs between C++ libraries too, so the same map's `.xml` from Linux and from Windows may differ in it). Once a map is written by today's savers it is a fixed point: `.bzm -> .xml -> .bzm` and `.xml -> .bzm -> .xml` are byte-identical (checked on `Allies` and `Tutorial` in the test, and by hand on all 16 maps above). Sorting those containers on save would change the game's own savers and is not done here.

Acceptance as built (`zig build test-bk-map`, about a minute and a half on Linux x64 debug): the installed `bk-map` converts `Data/Maps/Allies` (eight maps in nested folders) and `Data/Maps/Tutorial` (six, four with several reinforcement groups) to `.xml` and back, every map that comes back is `MapEquivalence`-equal to the shipped one, and both cycles from there are byte-identical as above; the two shipped `.xml` maps convert to `.bzm` in directory mode and `river3d` back to `.xml` as one file, equivalent and byte-identical with `NMapFile`'s own `.xml`; `minimap` on `coldwinter.bzm` at 128 writes `_c`, `_l` and `_h` DDS files that decode at 128x128, the `_h` level with 6,225 colours and no black pixel; the obj2mod fixture (`tools/zig/fixtures/small-tools/obj2mod`, a quad with a two-node skeleton and a two-key animation, CRLF) converts to a `.mod` the engine's savers read back as the fixture says, and `validate-obj2mod` prints its summary and the exact messages for two roots, an action key out of range and a missing vertex. `test-bk-tools` runs bk-map's row of the convention test. `zig build test-bk-map-all` converts all 57 `Data/Maps` maps to `.xml` and back and finds every one equivalent (under four minutes in a debug build; it leaves the byte cycles, another `.xml` parse of every map, to the default tier). By hand: `bk-map` staged in the Linux install's `Tools/` converts a map and draws its minimap from an unrelated working directory.

Not done in 08-04: the `.mod` and error-message goldens captured from the legacy exe (`BZMConvertor.vcxproj` is Windows/MSVC only and built nowhere here; the code moved unchanged and the fixture is judged by the engine's readers instead); comparing `minimap` with the map editor's picture of the same map pixel for pixel (the bridge's minimap runs only with a GPU session); the hand-made fixture map XML of D-29 (the tracked `Data/Maps` are the fixtures); deleting `Sources/src/bzmconvertor`, `Sources/src/imagedefrag` and their `A7.sln` entries (D-25, the phase's deletion step).
