# editor_kit — the Zig editor kit

Reusable editor plumbing with no MapEditor-specific types. Only `host` touches
the engine: it starts it through the shared `BkEditor*` C ABI (`bridge.h`).
Any Zig editor on the kit (MapEditor, ResourceEditor, Mission, …) gets the
same `files`, `shipped`, `script_file`, `autosave`, `history`, `settings`,
`host`, `window`, `capture`, `dialogs`, `fonts`, `l10n`, `crt`, `imgui`, `auto_schedule`, `pictures_cache`
and `testlaunch`
submodules and composes its own domain-specific state over them.

Dependency direction: `editor_core -> editor_kit`. The kit must not import
`editor_core`; map-specific state (layers, filters, terrain toggles, the
MapEditor Command union) lives in `Sources/editor/core/*.zig` and composes
the kit's generic primitives from above.

The `editor_imgui` module is built from `kit/imgui/` but reached through its
own module name (one file belongs to one Zig module), not through this root.

## files

The editor's own `Files` interface (`StdFiles` backed by `std.Io.Dir`,
`FakeFiles` backed by an in-memory map for tests). Covers what `Editor.save`'s
safe-save contract needs — existence, overwriting copy, replacing rename,
best-effort delete, directory listing, `.bak`/`.~save` suffix handling — plus
`osPathFromEngine` for engine → OS path conversion and `freeNames` for owning
the result of a directory listing. `max_path` is the shared buffer size.
Map-neutral: the interface works for any file an editor saves.

## shipped

Read-only test for a shipped-map path (D-18): a map under `<base_root>Data/`,
under `<base_root>mods/<any>/data/`, or anywhere whose ancestor named `data`
holds an `isDataRootMarker`. Save becomes Save As, autosave writes only a
recovery copy, and `Editor.save` refuses to write there at all. Exposes
`isShipped`, `isDataRootMarker`, `normalizeForCompare`, `isAbsolutePath`.
Map-neutral: a shipped file is shipped whether it is a map, a resource or a
mission.

## script_file

The map's Lua script file on disk (04-10, D-20): a bare `isBareName` check,
`gameScriptName`, `scriptPathBeside`/`scriptPathIn` builders, `copyAlong`,
`copyInto`, `copyForTest`, `listBeside`, `folderUrl` and `sameFile`. Every
destination is a fixed directory plus a bare name — a user-typed path never
becomes a path. Map-ish by heritage, but the module is parameterised on
`Files` only, so another editor could reuse it for any `<file>.<ext>`
sidecar.

## autosave

The autosave schedule and target (D-20..D-22): `Autosave` holds `enabled`,
`interval_ms`, `dirty_since_ms`, `last_write_ms`; `due` returns whether a
write is now due; `target(needs_save_as)` picks between `map_file` and
`recovery_copy`. `recoveryName(buffer, doc_path, extension)` builds the
recovery file name `<stem><extension>` from the document's stem; MapEditor
passes `".bzm"`. The extension is a run-time argument, so a resource or
mission editor threads its own.

## history

A generic undo/redo stack primitive: `History(comptime Command)` holds two
stacks of `Entry(Command)` (command + gesture), a save mark (`clean_depth`),
a revision counter the UI watches to rebuild costly views, and push/pop/merge
hooks. The kit has no knowledge of what a command is or how to replay it;
MapEditor's own Command union, its merge policy and its `editor.replay`
driver live in `Sources/editor/core/history.zig`. Map-neutral: takes a
`comptime Command`.

## settings

A generic `key=value` settings primitive: `Settings` with scroll speed,
autosave on/off and interval, default folder, recent-files list, default
format, game-parameters string, hidden-panels bits, and the `FixedPath`
buffer the free-text fields share. `parse` is lenient; `format` round-trips
every field. Duck-typed helpers (`applyGenericKey`, `writeGenericKeys`,
`writeGenericTailKeys`, `writeRecentLines`, `pushRecentField`,
`setGameParametersField`, `hasControl`, `formatExtension`) let a composed
caller like MapEditor's `core/settings.zig` share one parser/writer with its
own sidecar fields and keep `mapeditor.cfg`'s on-disk byte order unchanged.
Not yet fully map-neutral: `Format` is `bzm`/`xml` and the folder key is
`maps_folder`; a second editor generalises these when it composes the kit.

## host

The editor's process: one SDL window, the engine started on it through the
bridge, and ImGui drawing into the engine's own frame. Exposes `Host` with
`start`/`deinit`/`beginFrame`/`endFrame`/`frameSize`/`failureReason`, the
`HostError` set, `Options` (title, size, hidden), and `c` — a direct
`@cImport("bridge.h")` the host owns so any editor on the kit reaches the
engine ABI through the same translated types. Map-neutral: it hosts an
engine window; what the editor draws into it is its own.

## window

The engine-less counterpart of `host` (07-06, D-03): one SDL window, an
SDL_GPU device of its own and ImGui on the `editor_imgui` backend, with the
same `start`/`handleEvent`/`beginFrame`/`endFrame`/`stop` shape plus
`setTitle` and `failureReason`. A hidden window draws into an offscreen
texture rather than a swapchain. ELK runs on it; any editor that loads no
engine gets it for free.

`capture(alloc, io, path)` reads the last drawn frame of a hidden window back
(a download transfer buffer and a copy pass on the offscreen texture) and
writes it as an uncompressed 32-bit top-left TGA, printing
`capture: <path> <w>x<h>`; a failure names the path and SDL's error in
`failureReason()`. The file format is the pure `capture.encodeTga`, unit-tested
without a device. A visible window cannot be captured. Linux is the proved
platform; Windows and macOS are left to CI. ELK's `shot=<name>` writes
`<name>.tga` this way, and `tools/zig/l10n_measure.zig`'s `decodeTga` reads it
back for measuring.

## dialogs

`PathSlot(Kind, capacity)`: where an SDL file dialog's answer lands. SDL may
call back on another thread, so `callback` only writes the slot and the
frame loop `take`s a `path` (with the `Kind` it was requested for),
`failed` (SDL's reason, or a path longer than the slot) or `cancelled`.
One dialog at a time per slot. Scripted runs skip the OS dialog and call
`deliver` themselves. The map editor, the resource editor and ELK each
show their own dialogs (filters, start folder) over it.

## crt

Windows CRT hygiene shared by every editor that links the MSVC CRT without
libc and enters through `mainCRTStartup`: `routeCrtReportsToStderr`,
`attachParentConsole`, `assertionsAbort`. No-ops on non-Windows. Map-neutral.

## imgui

Dear ImGui for the Zig editors: `c` (the dcimgui C API plus the SDL3 / SDL
GPU backend shim) and the `overlayCallback` the GFXGPU overlay fires every
frame. Reached through the separate `editor_imgui` Zig module, not through
this root. Map-neutral.

## auto_schedule

`BK_EDITOR_AUTO="frame:action,frame:action,…"` grammar, parser, synthetic-
event step driver and TGA-compare primitives. The `Action` union is fixed
(keys, mouse gestures, `tool=` / `do=` labels, screenshots, exit); the caller
resolves the labels against its own vocabulary. MapEditor's `app/auto.zig`
re-exports this module unchanged.

## pictures_cache

Per-name picture cache (D-29): an ordered request queue with a per-frame
decode budget, one SDL_GPU texture per decoded name, a `missing` set the
queue refuses to re-queue until `clear`. Parameterised on a decoder callback
`fn (ctx, name, pixel_buffer, max_side) ?Decoded` — MapEditor plugs in
`BkEditorObjectPicture` / `BkEditorTilePicture`, another editor plugs in its
own decoder. Map-neutral: takes a decoder callback and knows nothing of the
engine bridge.

## testlaunch

Spawns `Game` beside `MapEditor` for Test in game (D-01..D-09): builds argv
as an array (never a shell string), redirects child stdout/stderr to a log
file, and polls the child's lifecycle without ever blocking the caller's
frame loop (`Running.poll` reimplements the OS non-blocking probe —
`waitpid(..., WNOHANG)` on POSIX, `WaitForSingleObject(handle, 0)` on
Windows). std-only: no window toolkit, no engine bridge, no C — the module's
own tests run on the host without a staged installation. Map-neutral: it
spawns a sibling executable; the launcher knows nothing of maps.

## fonts

The UI font stack (D059, D078): the bundled Noto faces from `vendor/fonts`, never
the system's, so a translation looks the same on every machine. Noto Sans is the
base; Noto Sans Devanagari, Noto Sans SC and Noto Sans JP are merged into it at
16 px, and ImGui rasterises glyphs on demand. The merge order depends on the
language: `ja` puts JP before SC (the Han characters differ between the faces),
every other language puts SC before JP (`mergeOrder`).

The faces are read from `<exe dir>/Fonts/`, which `install-elk` fills with
`vendor/fonts/*.ttf`, `*.otf` and `OFL.txt`, or from the directory in
`BK_FONT_DIR` (`resolveDir`). A face that is not there is `error.FontMissing`
and stderr names its full path. `load(gpa, io, override, code)` builds the atlas
before the first frame; `reload(code)` rebuilds it between frames for a language
switch. `covers(cp)` asks the stack for a glyph.

ImGui draws one glyph per code point and cannot shape Devanagari, so Hindi goes
through the cluster loader in `imgui/imgui_backend.cpp`, an `ImFontLoader`
attached to the Devanagari face (ImGui's font loader API is internal and new in
1.92). `encodeHindi(buffer, text)` shapes the text with HarfBuzz
(`Sources/src/FontAtlas/Shaper.h`) and replaces each cluster of more than one code
point by one Private Use Area character from a process-wide table (U+E000..U+F8FF,
6400 slots); the loader draws such a character as the whole cluster, shaped again
at the baked size and rasterised glyph by glyph with stb_truetype into one atlas
glyph. A full table logs `l10n: cluster table full` once and the text stays
unshaped. Half forms take the advance of the cluster shaped alone (the S01 limit).
Shaper.cpp and HarfBuzz are linked into `editor-imgui`. ELK's smoke draws a fixed
Hindi text (`do=probe_hindi`), captures it and measures the ink.

## l10n

String tables (D061), one `lang/<code>.lang` per language: `en` and the nine of
`tools/zig/l10n_languages.zig`, all embedded with `@embedFile`. The format is UTF-8 without a
byte order mark, CRLF lines, `#` comment lines, `key=value` (the first `=` splits) with the
escapes `\n`, `\t`, `\\` and the placeholders `{0}`..`{9}`. A table stores the text only; `id`
appends `###<key>`: every ImGui label that is a translation (widget, menu, menu item, button, tab, header, popup, window) uses `id`, because ImGui hashes the label as the widget ID and `t` would change the ID per language and collide equal translations; `t` stays for plain text. `l10n-check` fails on an ID-bearing `ig*` call whose label is `l10n.t(`. `tools/zig/l10n_check.zig` (`zig build l10n-check`) requires every language to
hold exactly the keys of `en.lang`, the same placeholders, and only glyphs its Noto face covers,
which is why the picker's native names come from `nativeName` (the language list), not from the
tables.

- `t("key")` and `id("key")` return a `[*:0]const u8` for the current language; the key is checked
  against `en.lang` at compile time. Pointers stay valid for the life of the process.
- `format(buf, "key", .{ "files", 3 })` and `formatZ` fill `{0}`..`{9}` from strings and integers.
- `setLanguage(code)` switches the language; `resolveLanguage(setting, os_locales)` maps the
  `ui_language` setting (`system` or a table code) and the operating system's locales
  (`zh_CN`, `pt_PT`, `ja_JP.UTF-8`, ...) to a code, and `applySetting` also logs and selects it.
- A key a language lacks shows the English text and logs `l10n: missing <code> <key>` once. An
  unknown setting logs `l10n: unknown ui_language <value>, using <code>`.
- `BK_L10N_DIR` (`setOverrideDir`, which apps call with the variable's value) loads
  `<dir>/<code>.lang` instead of the embedded table, for tests that drop a key.
- Hindi results are encoded with `fonts.encodeHindi`; load the fonts before the first lookup.

- `str("key")` and `say(buf, "key", .{...})` give the text or the filled sentence as a slice, for the
  calls that take one (status lines, notes); `say` falls back to the key's unfilled text when the buffer
  is too small.
- `rect_log` (off unless an app sets it for `--l10n-auto`) makes `noteRect(key)` (after a widget with its
  label inside, such as a button or menu title), `noteLabelRect(key)` (after a widget whose label is drawn
  at its right, such as a slider, input, checkbox or combo) and `noteWindowRect(key)` (inside a window)
  print `l10n-auto: rect <key> x y w h`, so a tier measures where each label was drawn. Normal runs print
  nothing.
- `kit.common.*` holds the verbs every editor shows (OK, Cancel, Yes, No, Apply, Close, Open, Save, Delete,
  Add); the Resource Editor reuses them and adds its own `resource_app.*` keys (`resource_app.gui.*` for the GUI Editor dock).
- The key lookup is a comptime `StaticStringMap`, sized for the Map Editor's 1,200 keys (`keyIndex` raises
  the eval branch quota locally).

Its tests run in `zig build test-editor-kit`. ELK's and the Map Editor's use of it is measured by
`zig build elk-l10n-auto`, `zig build map-editor-l10n-auto` and `zig build resource-editor-l10n-auto` (ten languages, frames captured with
`capture`, the missing-key case through `BK_L10N_DIR`); all share `tools/zig/l10n_auto_common.zig` (the log
parser, thresholds, the reference correlation, the Hindi and Han checks). See `Localization/README.md`,
"Editor tables".

## language_picker

The shared language combo (D061). `draw(current)` shows a combo labelled `kit.language.label` with
System first, then the ten languages under their own names (Hindi cluster-encoded), and returns
`?Choice` with the new `ui_language` value on the frame a row other than the current one is picked.
The caller applies it between frames: store it in the settings, call `l10n.setLanguage` and
`fonts.reload`. `setup(gpa, io, environ, &settings, scripted)` is what each editor calls once after its
window and ImGui exist: it resolves the language (a scripted run that names none draws English), loads the
fonts (`BK_FONT_DIR` or `<exe dir>/Fonts`), applies `BK_L10N_DIR` and selects the language; ELK and the
Map Editor and Resource Editor use it. `valueAt(row)` and `rowOf(value)` are the pure row/value mapping (an unknown value
selects System).

The `ui_language` key is shared: `settings.applySharedKey` reads it into a caller's
`ui_language_storage` (a `FixedPath`, duck-typed with `@hasField`), `uiLanguage(&settings)` returns
it or `system`, and `writeSharedTailKeys` writes `ui_language=<v>` only when it is not `system`, so
files that never chose a language stay byte-identical. A value is kept as written, even when
unknown. ELK, the Map Editor and the Resource Editor all carry the key; ELK's `spell_language`
stays a separate setting.
