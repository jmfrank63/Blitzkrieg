# editor_kit — the Zig editor kit

Reusable editor plumbing with no engine-bridge or MapEditor-specific types.
Any Zig editor on the kit (MapEditor, ResourceEditor, Mission, …) gets the
same `files`, `shipped`, `script_file`, `autosave`, `history`, `settings`,
`host`, `crt`, `imgui`, `auto_schedule`, `pictures_cache` and `testlaunch`
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
`<name>.recovery.<ext>` filename beside the map; MapEditor passes `".bzm"`.
Map-neutral: takes a `comptime extension`-shaped argument so a resource or
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
Map-neutral: the generic struct carries no map-only fields.

## host

The editor's process: one SDL window, the engine started on it through the
bridge, and ImGui drawing into the engine's own frame. Exposes `Host` with
`start`/`deinit`/`beginFrame`/`endFrame`/`frameSize`/`failureReason`, the
`HostError` set, `Options` (title, size, hidden), and `c` — a direct
`@cImport("bridge.h")` the host owns so any editor on the kit reaches the
engine ABI through the same translated types. Map-neutral: it hosts an
engine window; what the editor draws into it is its own.

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
event step driver and TGA-compare primitives: generic over an `Action` enum
the caller provides and a `dispatch(action) !void` callback the caller plugs
in. MapEditor's `app/auto.zig` resolves `tool=label` / `do=<named_cmd>` into
its own action enum and plugs in MapEditor's commands; a different editor can
plug in its own vocabulary and reuse the whole frame-scheduling, keypress,
mouse-gesture, TGA-screenshot and exit-code machinery unchanged. Map-neutral:
takes a `comptime Action` and a dispatch callback.

## pictures_cache

Per-name picture cache (D-29): an ordered request queue with a per-frame
decode budget, one SDL_GPU texture per decoded name, a `missing` set the
queue refuses to re-queue until `clear`. Parameterised on a decoder callback
`fn (name, out) !void` — MapEditor plugs in `BkEditorObjectPicture` /
`BkEditorTilePicture`, another editor plugs in its own decoder. Map-neutral:
takes a decoder callback and knows nothing of the engine bridge.

## testlaunch

Spawns `Game` beside `MapEditor` for Test in game (D-01..D-09): builds argv
as an array (never a shell string), redirects child stdout/stderr to a log
file, and polls the child's lifecycle without ever blocking the caller's
frame loop (`Running.poll` reimplements the OS non-blocking probe —
`waitpid(..., WNOHANG)` on POSIX, `WaitForSingleObject(handle, 0)` on
Windows). std-only: no window toolkit, no engine bridge, no C — the module's
own tests run on the host without a staged installation. Map-neutral: it
spawns a sibling executable; the launcher knows nothing of maps.
