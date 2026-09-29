# Phase 8: Small tools: portable converters and checkers - Context

**Gathered:** 2026-09-30
**Status:** Ready for planning

<domain>
## Phase Boundary

Replace the remaining Windows-only authoring tools with portable command-line tools built by `zig build` on macOS arm64 and Windows x64 (MSVC), each with tests, and remove the shipped Windows binaries in `Sources/Tools` that are replaced. The per-tool inventory and decisions are in `08-INVENTORY.md` (the source of truth for what each tool does); this file records the decisions and the plan split.

Delivered: `bk-pak`, `bk-font` (with a shared glyph-atlas library), `bk-table`, `bk-map`, `bk-sprite`, `bk-image`, `bk-video`, a modder guide for them, and the removal of everything replaced or dropped (binaries, sources, `A7.sln` entries, `build.zig` and `stage.zig` references).

Not in this phase: the map editor (Phases 3-5), the Resource Editor (Phase 6, `editor.exe`), ELK (Phase 7; only its font generator is shared, see D-13), the 40+ CI guard scripts under `tools/*/check_*.ps1`, installer size work (999.2), a new module or engine format. Consistent with `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md` ("The editor set": Small tools = "converters and validators", own spec - this phase is that spec's home; the design lives in this CONTEXT rather than a separate document because each tool is a thin wrapper of an existing format).

</domain>

<decisions>
## Implementation Decisions

### Tool set and architecture
- **D-01:** Every tool is decided per tool with a written reason (`08-INVENTORY.md`): PORT `ExcelExporter`, `FontGen`, `bzmconvertor` (three of four modes), the minimap creator inside `imagedefrag`, `spcomp`, `OffsetRomb`, and `convert_bik_to_ogv.ps1`; REPLACE `zip`/`unzip`; DROP `zip2exe`, `WhereIS`, `A7ExportModel.mll`, `dds.8bi`, `betakeygen`, `buildversion`, `AutoRun`/`Sources/autorun`, `PlanePathTest`, `InterfaceSystemTest`, the rest of `imagedefrag`, and bzmconvertor's `-fences`. `GameTT` and `Sources/sdk` are not tools.
- **D-02:** Named `bk-<noun>` executables with subcommands, one per workflow, not one megatool and not a port of each legacy name: `bk-pak`, `bk-font`, `bk-table`, `bk-map`, `bk-sprite`, `bk-image`, `bk-video`. Names describe the job (`bk-map to-bzm`) rather than the 2002 binary (`bzmconvertor`), and there are no `.exe`-only assumptions.
- **D-03:** Language by dependency, not by taste. Pure file transforms (`bk-pak`, `bk-table`, `bk-image`, `bk-video`) are std-only Zig (no libc, no engine), following `tools/zig/season_textures.zig`. Tools that must produce bytes the engine's own savers define (`bk-font` -> `SFontFormat`, `bk-map` -> `CMapInfo`, `.mod`, `bk-sprite` -> `SSpriteAnimationFormat`) are small C++ console programs linked to the engine libraries on the data-only startup (`tools/zig/data_only_startup.*`, the recipe in memory "C++ test exe recipe"), so the output cannot drift from what the game reads. No tool re-implements a binary structure-saver format in Zig.
- **D-04:** Conventions shared by all seven, written once in 08-01 and checked by a common test: `--help` prints usage and exits 0; bad usage exits 2; a failed operation exits 1 with the reason on stderr; success prints nothing but a one-line summary; `/` and `\` are both accepted on input and written as the host's on output; paths are UTF-8; no tool writes outside its named output or an explicit `--out`; no tool needs a game install except where the Data root is an argument (`--data <dir>`, default `./Data`); deterministic output (sorted iteration, no timestamps) so tests compare bytes.

### Per-tool behaviour (what a modder sees)
- **D-05:** `bk-pak` has `pack <dir> <out.pak>`, `unpack <in.pak> <dir>`, `list <in.pak>` and `verify <in.pak>`. `pack` writes stored (uncompressed) entries in sorted order with `/` names, exactly what `tools/zig/package.zig` writes, because the game mounts `*.pak` (mods, GeneratedData) and 999.2 decided to keep `.pak` and store entries uncompressed. `package.zig`'s writer becomes a shared library used by both `bk-pak pack` and the release packager; the release zip's bytes and hash do not change (existing `package_test.zig` stays green untouched).
- **D-06:** `unpack`/`list`/`verify` read stored and deflate entries (Zig `std.zip`), reject path traversal (`..`, absolute paths, drive letters) and entries whose CRC or size disagree, and refuse to overwrite existing files without `--force`. Zip64 stays out (the existing 65,535 entry limit, loud failure, is inherited).
- **D-07:** `zip2exe` (self-extracting exe) is dropped and no equivalent is written; `bk-pak` needs no `.exe` output.
- **D-08:** `bk-table export <mask> <folder> <out.tsv> [--ignore|--only <fields.txt>] [--no-recurse]` writes exactly the legacy sheet: header `FileName<TAB>field-path...`, one row per file, columns the sorted union of field paths (attributes and element text flattened with the legacy `item(NN)` numbering rules). `bk-table import <sheet.tsv>` writes changed cells back. The file extension is `.tsv` (the legacy `.txt` is still accepted on import so old sheets work). Excel, Numbers and LibreOffice open it directly; no `.xls`/`.xlsx` writer is built (the legacy exe never wrote real xls either: its "Excel file" was tab text).
- **D-09:** Import edits the XML in place: only the value text of cells that changed is patched into the original file bytes, so comments, ordering, attributes and whitespace of untouched content survive (the legacy tool re-serialised through MSXML and normalised every file it touched). A cell equal to `_` means "leave alone" as before. A row whose file is missing, or a cell whose path is not in the file, is reported and skipped; nothing is created.
- **D-10:** The legacy per-extension root node table (`*.bld`/`*.obt` -> `desc`, `*.eff` -> `effect`, default `RPG`) becomes a small built-in table with `--root <name>` to override, so a mod's new stat types work without a code change.
- **D-11:** A minimal, strict XML reader/patcher lives in the `bk-table` module (Zig std has none); it accepts what the game's own data files use (UTF-8/ASCII, comments, CDATA, entities) and fails with line and column on anything else rather than guessing.
- **D-12:** `bk-font` keeps the legacy output contract (`1.tfd` and `1.tga` in the output directory, chars 32..255 of the chosen code page as before, A/B/C spacing, kerning pairs, `SFontFormat` through the engine's saver) and renders with `imstb_truetype.h`, already vendored at `vendor/dcimgui/src-docking/` (no new dependency, no GDI, no SDL_ttf/FreeType). Input is `--ttf <file>` (or a face name searched in the OS font folders, best effort); options `--height --weight --italic --no-aa --charset --out` map the old flags.
- **D-13:** Link to ELK (Phase 7): the atlas, metrics and `SFontFormat` building are a library (`FontAtlas`, no HDC, no dialogs, no globals) that `bk-font` calls and that ELK's font page must call instead of its GDI copy in `CFontGen` (`ELK_Types.h`). 08-02 is therefore the first plan and may execute before Phase 7's font plan; Phase 7's plan depends on 08-02's API (`FontAtlas::Build(SFontRequest) -> {SFontFormat, image}`), which 08-02 documents in its summary. ELK's font list, code-page picker and sizes (`FONTS_SIZE {8,16,24,48}`) stay Phase 7's decisions.
- **D-14:** Byte parity with the GDI-made `Data/Fonts/*/1.tfd` is not a goal (different rasteriser, different hinting); the goal is a font the game loads and draws legibly. Acceptance is structural (every requested char present, metrics self-consistent, UVs inside the texture, no overlap of glyph cells, kerning symmetric with the source font) plus one measured look: a `-mod` test font drawn in game and captured with F9, measured from the TGA (memory "Measure graphics artefacts"), not eyeballed from code.
- **D-15:** No proprietary font is bundled or committed. Tests use a font already inside the tree under an open licence (Dear ImGui's embedded ProggyClean TrueType data, MIT, or an OFL font vendored with its licence file under `tools/zig/fixtures/`, planner's choice); nothing from the GOG install is committed.
- **D-16:** `bk-map` subcommands: `to-bzm <in.xml|dir> [--out]`, `to-xml <in.bzm|dir> [--out]`, `minimap <map|dir> --size N [--out]`, `obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt]`, `validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]`. Directory mode keeps the legacy behaviour (mirror the tree under `--out`). The `obj2mod` and `validate-obj2mod` code and the sidecar formats move over unchanged, and `docs/blender-replacement.md`, `tools/blender/README.md` and the exporter's help text switch to the new command.
- **D-17:** `bk-map to-bzm` and `to-xml` share the map loader/saver the game and editors use; the round trip is judged by `MapFile`'s `MapEquivalence` (the same oracle the M1 open/save sweep uses) plus byte identity of a `.bzm -> .xml -> .bzm` cycle where the format allows it, never by eyeballing.
- **D-18:** `bk-map minimap` calls the existing `CMapInfo::CreateMiniMapImage` (used by the map editors and `GameTT/MinimapCreation.cpp`); it is not a fork. The dead image packer, terrain generator, Perlin noise and beta-spline code in `imagedefrag` is not ported.
- **D-19:** `bk-map`'s legacy `-fences` migration is dropped (D-01): it rewrites 2002-era frame indices, every shipped map is already in type-index form, and the M1 sweep opens all of them. If a mod author needs it the source stays in git history.
- **D-20:** `bk-sprite compose <desc-dir> <out-name> [--data <dir>]` reads the same sprite-sequence description database as `spcomp` (same XML, same directions/frames/shift/speed/cycle fields), packs frames with the engine's `ComposeImages`, and writes `<out>.tga` + `<out>.san` through the same saver, so a re-composed shipped animation is byte-identical to what `spcomp` wrote. Windows paths and `IDataStorage` writes are replaced by plain filesystem access. Resource Editor (Phase 6) animation editors consume the `.san` code as a library, not this exe.
- **D-21:** `bk-image offset-romb <in.tga> [--out <out.tga>]` ports the legacy pixel permutation exactly (`FlipRomb1` then `FlipRomb2`, Bresenham lines and all), writing an uncompressed 32-bit TGA; the TGA codec is the one in `tools/zig/season_textures.zig`, lifted into a shared module rather than copied. Later image conversions land as further `bk-image` subcommands, not new exes.
- **D-22:** `bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]` keeps `convert_bik_to_ogv.ps1`'s flags, output naming (`.ogv` plus the `.audio` sidecar) and key-frame setting; it shells out to `ffmpeg`/`ffprobe` found on `PATH` (or `--ffmpeg`/`--ffprobe`) and says clearly when they are missing. No codec is built in. The existing `tools/video/check_*.ps1` guards keep their own way of running.

### Drops and removals
- **D-23:** Dropped tools go with a written reason in `08-INVENTORY.md` and in the commit message that deletes them. The deletion happens in the plan that ships the replacement (ported or replaced tools) or in 08-06 (dropped ones); git history is the archive (nothing is moved to an `attic` folder).
- **D-24:** Binaries removed from the tree: `Sources/Tools/{ExcelExporter.exe, FontGen.exe, image.dll, streamio.dll, zip.exe, unzip.exe, zip2exe.exe, WhereIS.exe, A7ExportModel.mll, dds.8bi, error.txt}` (the directory then disappears), `Sources/src/bin/ExcelExporter.exe`, and `Sources/autorun/*` (6 MB incl. the VC6 runtime DLLs). `Sources/src/bin/{editor.exe, MapEditor.exe}` stay: they belong to Phases 5 and 6.
- **D-25:** Sources removed with them: `Sources/src/{excelexporter, FontGen, bzmconvertor, imagedefrag, spcomp, betakeygen, buildversion, AutoRun, PlanePathTest, InterfaceSystemTest}` and `Sources/src/Tools/OffsetRomb`, once the replacement passes its tests; their `A7.sln` entries and configuration lines go in the same commit. `Main/BetaKey.cpp` and its call in `Game/GameMain.cpp` are deleted only if the call is compiled out (`_DO_BETA_CHECK` is defined only by the keygen project), otherwise left.
- **D-26:** The `Windows`-only `build.zig` steps `addFontGen`, `addBuildVersion`, `addBetaKeyGen`, the `fontgen`/`buildversion`/`betakeygen` steps and sources lists, and the `excluded_utilities` list in `tools/zig/build_support.zig` are removed or replaced by the new `bk-*` steps. `build.zig` is never run through `zig fmt`; every edit is followed by `zig test tools/zig/build_hermeticity_test.zig` and the platform build matrix tests.
- **D-27:** `stage.zig` (`copyEditors`) and `game_install.ps1` stop copying `ExcelExporter.exe`. The staged/packaged game gains a `Tools/` directory holding the `bk-*` executables on both platforms (not gated on `editors_supported`, which is Windows-only today), so a modder with only the release zip has them; `stale_root_files` keeps naming the old exes so upgraded installs are cleaned.

### Testing, CI, packaging
- **D-28:** Every tool has its own build step `bk-<x>` and test step `test-bk-<x>` wired into the platform build matrix, hermetic (no network, no GOG data, no writes outside the build cache and `zig-out/local-test`), the Zig ones running under `zig build test-...` on both platforms and the C++ ones as engine-tier executables like the other `*_test.cpp` tiers.
- **D-29:** Fixtures are small synthetic files committed under `tools/zig/fixtures/small-tools/` (a three-file stat folder, a two-sprite description, a 16x16 tile TGA, a hand-made map XML, a tiny OBJ with skeleton and animation sidecars, a tiny pak). GOG files are never committed. A second, opt-in tier runs the same tools over the real `Data` when it is present (skipped, not failed, when absent): every shipped `.xml` map -> `.bzm` -> `.xml` equivalence, and re-composition of a sample of shipped sprite animations; artefacts go to `zig-out/local-test`.
- **D-30:** Each Windows-relevant tool is also run on `win-home` (the repo checkout at `C:\Users\jmfrank\source\repos\jmfrank63\Blitzkrieg`; the GOG install there is read-only, so real-data runs write elsewhere) as part of the exit gate; CI (`.github/workflows/cross-platform.yml`) builds and tests the Zig tools on every target it already covers and the C++ ones where the engine tier already builds.
- **D-31:** A modder-facing guide `docs/modding-tools.md` documents the seven tools with one worked example each and the "old exe -> new command" table; `docs/blender-replacement.md` and `docs/zig-build-transition.md` are updated, `docs/PLANNED_FEATURES.md` section 1 gets the tools ticked off. No new README files are scattered per tool.
- **D-32:** C++ rules apply to every new line: no `std::min`/`std::max` (the codebase's `Min`/`Max`), no engine module edited without need, no ABI break; Zig code targets 0.16 and follows the layout of `tools/zig`.

### Claude's Discretion
- Internal module layout of each tool under `tools/zig/` or `Sources/src/tools/`, the XML reader's internals, the glyph-packing heuristic (any deterministic packer that meets D-14), the exact help text, and the choice of fixture font (D-15).
- Whether `bk-map` and `bk-sprite` share one engine-backed startup helper (recommended: one `tools/zig/tool_host.cpp/.h` beside `data_only_startup`), and whether the sheet reader accepts CRLF and BOM (recommended yes, on input).
- Whether the deferred CI-guard scripts get a `bk-check` later (not this phase).

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `tools/zig/package.zig` (deterministic stored zip writer with POSIX modes, `createPackage`) and `package_test.zig`: the core of `bk-pak pack`.
- `tools/zig/season_textures.zig`: the model modder-facing Zig tool (its own `build.zig` steps `season-textures` and `test-season-textures`, DDS/TGA codecs, plan/apply split) and the TGA codec for `bk-image`.
- `tools/zig/data_only_startup.{h,cpp}`: storage + constants + object database with no window or GPU, for `bk-map`, `bk-sprite`, `bk-font`; `editor_bridge_test.cpp` and `map_file_test.cpp` show the engine-tier test pattern; `Sources/src/MapFile` (`MapEquivalence`) is the round-trip oracle.
- `Sources/src/RandomMapGen/MapInfo_StaticMethods_MiniMapCreation.cpp` (`CreateMiniMapImage`), `Sources/src/Formats/fmtFont.h` (`SFontFormat`), `fmtMesh.h`, `fmtAnimation.h`/`Common/fmtAnimation.h`, `Image` (`ComposeImages`, `SaveImageAsTGA`).
- `vendor/dcimgui/src-docking/imstb_truetype.h` (TrueType rasteriser) and Dear ImGui's embedded ProggyClean font for a test fixture.
- `tools/blender/blitzkrieg_export.py` and its tests produce the OBJ + sidecars `bk-map obj2mod` consumes.
- `tools/zig/delete_matching_files.zig` covers what `WhereIS` did for the build.

### Established Patterns
- `build.zig` (never `zig fmt`, `test-...` steps, `test_mode == .run`), `tools/zig/build_support.zig` policy tables, `tools/zig/stage.zig` staging with `copyEditors`/`stale_root_files`, `build_hermeticity_test.zig` guarding the build graph.
- Engine-tier C++ test executables linked to the Zig-built engine libraries (memory: C++ test exe recipe; copy `gfxgpu-factory-test`; install `StreamIOOptionsAbi` too or Windows fails alone); test artefacts under `zig-out/local-test`.
- Modder-facing data formats stay engine-defined: the tools call the engine's savers/loaders.

### Integration Points
- `build.zig`: new `bk-*` and `test-bk-*` steps next to `season-textures`; removal of `addFontGen`, `addBuildVersion`, `addBetaKeyGen` and their source lists; `build_support.zig` `excluded_utilities`.
- `tools/zig/stage.zig` and `game_install.ps1`: `Tools/` directory in the staged and packaged game; drop the `ExcelExporter.exe` line.
- `Sources/src/A7.sln`: remove the project entries of every deleted tool.
- ELK (Phase 7): `CFontGen` in `Sources/src/ELK` switches to the `FontAtlas` library from 08-02.
- Phase 5 (map editor M3 minimap tools) and Phase 6 (Resource Editor animation and font editors) reuse the `bk-map minimap` function, the `.san` composer and the `FontAtlas` library as libraries.
- `.github/workflows/cross-platform.yml`, `win-home` for the Windows run.

</code_context>

<specifics>
## Specific Ideas

- Johannes's rule for the whole project: portable on macOS and Windows, each with tests, nothing Windows-only left behind; and "always choose the recommended answer, never come back with questions" for this autonomous run (recorded, alternatives in `08-DISCUSSION-LOG.md`).
- Modders are the customer: the test of a port is that a modder can do what the old exe did with a one-line command and a documented example, including Blender -> `.mod` and Excel-editable stat sheets.
- The legacy sheet format is preserved on purpose (same header and columns) so existing sheets and habits keep working.
- Exit gate is measured: byte or equivalence checks against outputs made by the shipped/legacy tools where they exist (release zip bytes, shipped `.san`, shipped maps), and one in-game capture for the generated font.

## Testable exit criteria

1. `zig build test-bk-pak test-bk-table test-bk-font test-bk-map test-bk-sprite test-bk-image test-bk-video` passes on macOS arm64 and on `win-home` (Windows x64 MSVC), and the Zig-only ones on every target the platform matrix already covers.
2. `bk-pak pack` of a fixture tree yields the same bytes as `tools/zig/package.zig` for the same tree (a test compares them), `package_test.zig` passes unchanged, `bk-pak unpack` of the result reproduces the tree, and a hostile zip (`../x`, absolute path, bad CRC) is rejected without writing.
3. `bk-table export` on the fixture stat folder produces the expected TSV byte for byte; `import` of an edited sheet changes only the edited values (diff of the XML shows only those lines) and skips `_` cells; a missing file or path is reported, exit 1, nothing else touched.
4. `bk-font --ttf <fixture> -h16` produces `1.tfd` + `1.tga` that the engine loads back (`SFontFormat` round trip in the engine-tier test), passes the D-14 structural checks, and one `-mod` font drawn in game and captured with F9 is measured from the TGA and recorded in the summary.
5. `bk-map to-xml` then `to-bzm` over the fixture maps and (opt-in) over every shipped map is equivalent under `MapEquivalence`; `obj2mod` and `validate-obj2mod` give the same `.mod` bytes and error messages as the current `BZMConvertor` code on the Blender fixture (golden captured from the code before it is deleted); `minimap` writes an image of the requested size that matches the map editor's for the same map.
6. `bk-sprite compose` on the fixture description writes a `.tga` + `.san` that the engine loads, and (opt-in) re-composing a sample of shipped animations from their frames is byte-identical to the shipped `.san`.
7. `bk-image offset-romb` matches an explicit golden on the fixture tile; `bk-video convert --what-if` lists the same jobs as the script on a fixture folder and fails clearly without `ffmpeg`.
8. `bk-*` executables are built by `zig build` on both platforms, appear in the staged and packaged game under `Tools/`, and each answers `--help` with exit 0 and bad usage with exit 2 (one shared test).
9. Nothing in `Sources/Tools`, `Sources/autorun`, `Sources/src/bin/ExcelExporter.exe` or the deleted source directories remains; `git grep` finds no reference to a removed tool in `build.zig`, `stage.zig`, `game_install.ps1`, `A7.sln` or `docs/` other than the "old -> new" table; `zig test tools/zig/build_hermeticity_test.zig` passes after the `build.zig` edits; `zig build` of the game and `MapEditor` is unaffected on both platforms.
10. `docs/modding-tools.md` exists with a worked example per tool, and each example was run.

## Plan split

| Plan | Content | Depends on | Exit criteria |
|---|---|---|---|
| **08-01** | Conventions + shared test (D-04); `package.zig` split into library + `bk-pak` (pack/unpack/list/verify); build/test steps; remove `zip.exe`, `unzip.exe`, `zip2exe.exe`; the `Tools/` staging directory mechanism in `stage.zig` (D-27) | - | 2, 8 (pak part) |
| **08-02** | `FontAtlas` library + `bk-font`; documented API for ELK; remove `FontGen.exe`, `image.dll`, `streamio.dll`, `FontGen/` sources, `addFontGen` | 08-01 conventions | 4; may run before Phase 7's font plan |
| **08-03** | `bk-table` (reader/patcher, export, import); remove both `ExcelExporter.exe`, `excelexporter/` sources, `stage.zig`/`game_install.ps1` lines | 08-01 | 3 |
| **08-04** | Shared `tool_host` startup helper; `bk-map` (`to-bzm`, `to-xml`, `minimap`, `obj2mod`, `validate-obj2mod`); goldens captured from the legacy code first; Blender docs switch; remove `bzmconvertor/`, `imagedefrag/` | 08-01 | 5 |
| **08-05** | `bk-sprite compose`, `bk-image offset-romb` (TGA codec lifted from `season_textures.zig`); remove `spcomp/`, `Tools/OffsetRomb/` | 08-04 host helper | 6, 7 (image part) |
| **08-06** | `bk-video`; drop the rest with reasons (`WhereIS`, `zip2exe` if left, `A7ExportModel.mll`, `dds.8bi`, `error.txt`, `betakeygen/`, `buildversion/`, `AutoRun/`, `Sources/autorun/`, `PlanePathTest/`, `InterfaceSystemTest/`, dead `BetaKey`); `A7.sln`, `build.zig`, `build_support.zig`, `stage.zig` cleanup; `docs/modding-tools.md`; win-home + CI run; final reference sweep | 08-01..08-05 | 1, 7 (video part), 8, 9, 10 |

Waves: 08-01 first; 08-02, 08-03, 08-04 in parallel after it (08-02 pulled earlier if Phase 7 needs the font library); 08-05 after 08-04; 08-06 last.

</specifics>

<deferred>
## Deferred Ideas

- `bk-check`: porting the 40+ `tools/*/check_*.ps1` CI guards (proprietary references, video/audio scaffolds) to Zig so they run on macOS - a CI-hygiene phase of its own; they are not modder tools and run on `win-home` today.
- Real `.xlsx` read/write in `bk-table` (would need a zip + sheet XML writer; the tab-text sheet already opens in every spreadsheet).
- A validator set from `docs/PLANNED_FEATURES.md` ("headless validation for maps, campaigns, sprites, effects, meshes, localisation data, mod manifests"): `bk-map` and `bk-sprite` are the first two verbs it would grow from, but the validators themselves are a separate phase.
- A bundled open-licence default font for `bk-font` and ELK (a Phase 7 decision).
- `bk-pak` compression (deflate/zstd writer) and Zip64: with 999.2 (smaller installer).
- The `-fences` frame-index migration, image packer and terrain generator of `imagedefrag`: dropped, recoverable from git history if a mod ever needs them.
- Removing `Sources/src/bin/{editor.exe, MapEditor.exe}` and the empty `Sources/sdk/openspy-core`: belong to Phases 5/6 and the GameSpy replacement work.

</deferred>
