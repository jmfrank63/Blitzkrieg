# Agent instructions for Blitzkrieg Reloaded

A port of the 2003 Blitzkrieg game and its authoring tools to a portable stack: the game and engine stay C++,
built by `zig build` (Zig 0.17) for Windows x64 (MSVC), macOS (arm64 and x64) and Linux x64; the new editors are Zig
apps with Dear ImGui (`Sources/editor`) talking to the engine through the C ABI `Sources/src/EditorBridge`.

## Machine and commands

- This machine is Linux x64 (Ubuntu 24.04, GNOME Wayland). Build and test natively here; macOS and Windows are
  verified by CI (`.github/workflows/cross-platform.yml`) when a branch is pushed. Never claim a macOS or Windows
  result you did not see.
- Build: `zig build install-game install-map-editor`. The Linux install is `zig-out/game/linux/x86_64/release/`.
- Map Editor tiers: `test-editor-core`, `test-map-editor-view`, `-panels`, `-testlaunch`, `-auto`,
  `test-map-editor-engine`, `test-editor-bridge`, `map-editor-host-check`, `map-editor-smoke`, `map-editor-auto`,
  `map-editor-auto-m2`, `map-editor-m3-auto`, `map-editor-game-reads-it-m3`. Resource Editor: `resource-editor-host-check`, `resource-editor-smoke`, `resource-editor-batch`, `resource-editor-game-reads-it` and `resource-editor-auto-<editor>` (core, wpn, unt, spt, msh, obt, fnc, bld, bdg, pcp, eff, til, 3rd, 3rv, mip, chc, cgc, mdc, gui; each under about 3 minutes, the aggregate `resource-editor-auto` chains them and is too long for one foreground command). Any change to shared editor code must
  keep these green.
- `zig build --help` lists every step. Long steps: run them with a generous timeout, not in a loop.
- Never start a long build or test (more than about 10 minutes, e.g. `tools/zig/run-resource-sweep.sh`, about 27 minutes)
  in the background and then end the session: the session ends, the task stays incomplete, and the next attempt
  starts a second copy that races the first on `zig-out`. A foreground command is capped at 10 minutes, so an agent
  cannot run the full sweep at all. Run the individual tiers your change touches in the foreground instead (each is
  well under 10 minutes). The maintainer runs `tools/zig/run-resource-sweep.sh` at the end of each slice; a task whose
  plan asks for the full sweep records "full sweep left to the maintainer" plus the tiers it did run, and completes.

## Rules

- Plans and decisions already exist: `.planning/phases/06-*`, `07-*`, `08-*` (CONTEXT, PARITY, INVENTORY,
  DISCUSSION-LOG) and the specs in `docs/superpowers/specs/`. Follow them; where this file or a milestone context
  changes one (for example adding Linux), the newer instruction wins, and record the change in the spec.
- Line endings are CRLF for all text files (`.gitattributes`). Keep new files CRLF.
- Match the surrounding code: naming, comment density, idiom. Zig doc comments explain why, in plain sentences.
- Tests first where practical. Tests use only tracked data (`Data/`, `tools/zig/fixtures/`), copied to
  `zig-out/local-test/...`; never write into shipped `Data/`, never touch the user's own profile, saves, settings or
  cloud sync (redirect with `XDG_DATA_HOME` / the `BK_*_SETTINGS` seams).
- "The game reads it unchanged" is proved by the engine's own readers or by the real Game, never by eyeballing.
- Graphics claims are proved by captured TGA/PNG measured by code, not judged from source.
- Linux pitfalls already solved for the Map Editor, reuse them: executables that load engine modules need
  `rdynamic` and a `$ORIGIN` rpath; worker threads need the default 16 MiB stack on Linux (glibc carves static TLS
  out of the stack); hidden test windows use `SDL_WINDOW_HIDDEN | SDL_WINDOW_NOT_FOCUSABLE`; stop `ISFX` before
  modules unload; resolve Data paths case-insensitively (`DataFile` helper in `tools/zig/editor_bridge_test.cpp`).
- A task plan's Verify line holds only runnable commands joined by `&&`: no notes in parentheses, no pipes, `;`,
  redirects or quoted one-liners. GSD's pre-run check rejects anything else and pauses auto mode. Put notes in the
  task description.
- `BK_DEBUG_LOG=1` is needed for `DebugTrace` output in release builds.
- Do not edit `.gitignore`'s GSD lines (`.gsd`, `.gsd-id`, `.bg-shell/`, `.mcp.json`) and do not ignore `vendor/`,
  `build/` or `.vscode/`: they hold tracked files.
- `CArray2D::SetZero` used to trap in debug builds (fixed in `3e8afc8a7`); the Map Editor tiers that open a map now pass
  on a Linux debug build too. A tier failure there is a real regression, not a known pre-existing failure.
- Before a task completes, `git status` must show nothing the task touched as modified or untracked: headers,
  fixtures and docs included. GSD's task commit can leave files out; a clean checkout (CI, the maintainer's sweep
  worktree) then fails to compile, as at S13 (`affe58ffd`).
- Commit messages: conventional prefix with a scope, e.g. `feat(resource-editor): ...`, `fix(editor): ...`,
  `test(...)`, `docs(...)`. Do not push; the maintainer merges and pushes.
- Deleting legacy MFC code or shipped binaries is part of a phase's end only after its parity checklist is fully
  verified, and only as the phase context describes.
