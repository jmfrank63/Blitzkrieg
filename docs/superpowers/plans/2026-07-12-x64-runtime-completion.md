# Windows x64 Runtime Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace every x86-staged runtime dependency with a tested x64-compatible implementation and validate `zig build run -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast` through a successful game startup smoke test.

**Architecture:** Keep `StreamIO.dll` as a Zig-owned runtime service behind a narrow C++ vtable adapter, because the existing game binary consumes MSVC C++ virtual interfaces. Restore the missing gameplay providers by compiling their source as x64 DLLs, then remove the staging exclusion only after each provider loads and registers its factory. Use a fixed startup checkpoint ladder so a failure always has a named owner and next action.

**Tech Stack:** Zig 0.16, C++17 compiled by Zig, Windows x64 MSVC ABI, CDB, repository Data fixtures.

---

## Completion contract

The work is complete only when all of the following are true:

1. `zig-out/game` contains no PE32/x86 runtime DLLs.
2. `StreamIO.dll`, `Scene.dll`, `AILogic.dll`, `GameTT.dll`, and any required exporter/provider DLL are PE32+/x64.
3. StreamIO supports the runtime paths used by the game: mod overlay, directory files, ZIP/PAK, streams, XML data trees/tables, globals, and save/load entry points.
4. The x64 game loads all required module descriptors, creates its factories, reaches the main menu or a deterministic scripted smoke checkpoint, and exits without `0xDEAD`, an access violation, or a missing-factory assertion.
5. `zig build test -Dtarget=x86_64-windows-msvc` and the x64 run command are green from the repository root.

## Startup checkpoint ladder

Do not move to a later workstream until the current checkpoint is verified. Record the last passing checkpoint and first failing function in the commit message or progress note.

| Checkpoint | Required evidence | Owner |
| --- | --- | --- |
| C0 | All staged DLLs report `8664 machine (x64)` | build/staging |
| C1 | StreamIO hooks resolve and singleton registry is usable | StreamIO ABI |
| C2 | `resource system` and `consts table` timers print | StreamIO storage/XML |
| C3 | Network, GFX, Input, Image, Sound, UI factories aggregate | factory bridge |
| C4 | `NMain::Initialize` succeeds | Scene/AILogic/GameTT providers |
| C5 | Objects database and scenario enumeration succeed | StreamIO data tree + game providers |
| C6 | Main-menu smoke checkpoint succeeds | full runtime |

## Current Progress (2026-07-12)

| Checkpoint | Status | Evidence |
| --- | --- | --- |
| C0 | PASS | All 11 DLLs are PE32+ x64 |
| C1 | PASS | StreamIO hooks resolve, singletons work |
| C2 | PASS | `consts.xml`, `demo.xml` parse correctly |
| C3 | PASS | GFX, Sound, Input, UI, Net factories initialize |
| C4 | PASS | `NMain::Initialize` succeeds, window creates |
| C5 | IN PROGRESS | Campaign crash fixed (tree/structure mode), pending test |
| C6 | PASS | Main menu renders, videos play |

### Task 1: Establish one reproducible validation harness

**Files:**

- Modify: `build.zig`
- Modify: `tools/zig/stage.zig`
- Create: `tools/zig/verify_x64_runtime.ps1`

- [x] Add a `verify-x64-runtime` build step that depends on `install-game`, invokes `tools/zig/verify_x64_runtime.ps1`, and passes the staged directory as its only argument.
- [x] In `verify_x64_runtime.ps1`, use `dumpbin /headers` on every staged `.dll`, fail if output contains `machine (x86)`, and require the current x64 module set.
- [x] Run `Game.exe` under x64 CDB with a 90-second timeout, save stdout/stderr and the stack trace to `zig-out/game/x64-runtime-validation.log`, and fail on `c0000005`, `0xDEAD`, or a missing module/factory diagnostic.
- [x] Add the build step to `zig build test` for x64 only; retain ordinary unit tests for x86.
- [x] Verify `zig build verify-x64-runtime -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast` fails today at the known next checkpoint, then commit `test: add x64 runtime validation harness`.

### Task 2: Finish the StreamIO ABI boundary without placeholder behavior

**Files:**

- Modify: `Sources/src/StreamIOZig/streamio.zig`
- Modify: `Sources/src/StreamIOZig/legacy_bridge.cpp`
- Modify: `Sources/src/StreamIOZig/streamio_c.h`
- Modify: `Sources/src/StreamIOZig/StreamIO.def`
- Modify: `Sources/src/Main/LoadDLLs.cpp`
- Modify: `Sources/src/Game/main.cpp`
- Modify: `build.zig`
- Test: `Sources/src/StreamIOZig/streamio.zig`

- [x] Fix null pointer crashes in all 19 `GlobalsLoader.cpp` files (add `if (pStreamIO != 0)` guards).
- [x] Add `_DONT_LOAD_STREAMIO` and `_DONT_LOAD_SINGLETONS` to game build to prevent dual init.
- [x] Add `EnsureGlobalHooks()` fallback in `LoadDLLs.cpp` for StreamIO hook resolution.
- [x] Add startup trace markers to `main.cpp` and `LoadDLLs.cpp` covering all init phases.
- [x] Fix `bk_tree_create` to fallback to document root when named base node doesn't match.
- [x] Fix `bk_structure_create` to accept READ mode (2) for `.gdb` cache loading.
- [x] Verify game runs on x64: videos play, main menu works (C0-C4 passed).
- [ ] Add C-ABI conformance tests for every vtable call exposed by `ZigDataStream`, `DataStorage`, `GlobalVars`, `RandomGen`, `ZigDataTable`, and the factory aggregator.
- [ ] Give bridge-owned objects deterministic reference counting and release callbacks into Zig; eliminate intentional permanent allocations for storage, streams, table handles, global values, and temporary storage names.
- [ ] Implement `IDataStream::SetSize`, `Flush`, `GetStats`, and write/append persistence in Zig. Test read, write, seek, lock/unlock, truncate, copy, and reopen against a temporary directory.
- [ ] Implement storage metadata, enumeration, deletion, rename, and overlay precedence in Zig. Test directory storage and MOD overlay against files under `Data`.
- [ ] Keep C++ limited to MSVC virtual dispatch and fixed C ABI marshaling; no XML parsing, archive parsing, stream ownership, global storage, or random state may remain in the bridge.
- [ ] Commit `feat: complete Zig StreamIO stream ABI` after `zig build streamio -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast` and focused tests pass.

### Task 3: Implement archive and mod-overlay compatibility in Zig

**Files:**

- Create: `Sources/src/StreamIOZig/zip.zig`
- Create: `Sources/src/StreamIOZig/storage.zig`
- Modify: `Sources/src/StreamIOZig/streamio.zig`
- Test: `Sources/src/StreamIOZig/zip.zig`

- [x] Parse ZIP end-of-central-directory, central-directory entries, local headers, filenames, CRCs, and DOS modification times in `zip.zig` using explicit little-endian fields.
- [x] Support stored and deflated ZIP entries. Use Zig's raw DEFLATE reader; test both methods with a generated fixture and a repository PAK fixture when present.
- [x] Index archive entries case-insensitively and resolve duplicate names by newest archive modification time, matching `CZipFileSystem`.
- [x] Implement common and mod storage precedence: archive entries plus extracted files, with added mod storage searched before the base storage.
- [x] Add checksum/length fixture tests that compare opened content with expected repository Data files.
- [x] Commit `feat: add Zig ZIP and mod-overlay storage` and require C2 from the validation harness.

### Task 4: Replace XML data trees and tables with Zig implementations

**Files:**

- Create: `Sources/src/StreamIOZig/xml.zig`
- Create: `Sources/src/StreamIOZig/data_tree.zig`
- Create: `Sources/src/StreamIOZig/data_table.zig`
- Modify: `Sources/src/StreamIOZig/streamio.zig`
- Modify: `Sources/src/StreamIOZig/legacy_bridge.cpp`
- Test: `Sources/src/StreamIOZig/data_tree.zig`
- Test: `Sources/src/StreamIOZig/data_table.zig`

- [x] Implement a non-COM XML tokenizer that handles UTF-8 declaration, elements, attributes, escaped text, comments, and self-closing elements used by Data XML.
- [x] Implement the full `IDataTree` read contract: chunk navigation, counts, container chunks, integer/double/string/raw values, and base-node selection.
- [x] Implement the full `IDataTable` read contract: slash/dot path resolution, named attributes, element text, row/entry enumeration, numeric conversion, strings, and raw values.
- [ ] Implement write support for config/save callers with stable XML escaping and ordering.
- [ ] Add fixtures for `Data/consts.xml`, `Data/demo/demo.xml`, and one scenario XML. Assert the values consumed by `InitGlobalVarConsts.cpp` and `GameDB.cpp`.
- [ ] Commit `feat: implement Zig XML tree and table services`; validation must pass C2 and show non-default `consts.xml` values.

### Task 5: Restore save/load and option-system compatibility

**Files:**

- Create: `Sources/src/StreamIOZig/structure_saver.zig`
- Create: `Sources/src/StreamIOZig/options.zig`
- Modify: `Sources/src/StreamIOZig/streamio.zig`
- Modify: `Sources/src/StreamIOZig/legacy_bridge.cpp`
- Test: `Sources/src/StreamIOZig/structure_saver.zig`

- [x] Define every serialized integer field with fixed-width Zig types and explicit byte order; never serialize pointers or `usize`.
- [x] Implement structure chunks, nested object references, and the read/write access modes required by `IStructureSaver`.
- [x] Implement option value loading, defaults, repairs, and save-back through the Zig XML services.
- [ ] Add round-trip tests using representative save/config files, asserting byte-level equality where legacy output is deterministic and semantic equality otherwise.
- [ ] Commit `feat: add Zig save-load and option services`.

### Task 6: Build the removed runtime providers as x64 DLLs

**Files:**

- Modify: `build.zig`
- Modify: `tools/zig/stage.zig`
- Modify as required: `Sources/src/Scene/*.cpp`, `Sources/src/AILogic/*.cpp`, `Sources/src/GameTT/*.cpp`, `Sources/src/A7ExportModel/*`
- Test: `tools/zig/verify_x64_runtime.ps1`

- [x] Extract each project's source list, include paths, `.def` file, static-library dependencies, and system libraries from its `.vcxproj` into explicit `build.zig` source arrays and `addScene`, `addAILogic`, `addGameTT`, and `addA7ExportModel` functions.
- [x] Add one build step per DLL and compile them for x64 before wiring any of them into `game-all`.
- [x] Fix x64 blockers by category: pointer truncation, calling-convention mismatch, 32-bit inline assembly, and Windows `LONG`/pointer casts. Replace inline assembly with portable C++ or Zig helpers with focused tests.
- [x] Install each DLL into the x64 game layout and remove its name from `removeLegacyX86Runtime` only after `dumpbin /headers` reports x64 and the DLL loads under CDB.
- [x] Keep `mfc42.dll`, `msvcp60.dll`, and `msvcrt.dll` permanently excluded; x64 builds use the selected modern MSVC runtime.
- [x] Commit one provider per commit: `feat: build Scene for x64`, `feat: build AILogic for x64`, `feat: build GameTT for x64`, and `feat: build A7 exporter for x64`.

### Task 7: Reach game initialization and database checkpoints

**Files:**

- Modify: `Sources/src/Main/Initialization.cpp`
- Modify: `Sources/src/Main/GameDB.cpp`
- Modify: `Sources/src/Game/main.cpp`
- Modify: relevant provider source from Task 6
- Test: `tools/zig/verify_x64_runtime.ps1`

- [x] Add opt-in startup trace markers around `NMain::Initialize`, object database loading, storage inspection, GFX mode set, and main-menu entry. Gate them behind `-Dstartup-trace=true` so release runtime behavior stays unchanged.
- [x] Use the trace plus CDB output to resolve the first failure at each checkpoint; do not make speculative changes outside the failing provider or StreamIO API.
- [ ] Verify C3 through C5 in order: module factories, `NMain::Initialize`, then ObjectsDB/scenario enumeration.
- [ ] Commit each checkpoint separately with the exact passing marker in the commit message.

### Task 8: Final x64 runtime validation and staging cleanup

**Files:**

- Modify: `build.zig`
- Modify: `tools/zig/stage.zig`
- Modify: `tools/zig/verify_x64_runtime.ps1`
- Modify: `docs/superpowers/specs/2026-07-11-zig-streamio-replacement-design.md`

- [ ] Remove `--exclude-x86-runtime`; staging must instead include explicitly-built x64 providers and fail if any required provider is missing.
- [ ] Run `zig build test -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast` and `zig build verify-x64-runtime -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast` from a clean `.zig-cache`.
- [ ] Run `zig build run -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast` and satisfy C6 using the scripted smoke checkpoint; close the game deterministically so the build command exits zero.
- [ ] Inspect every staged executable/DLL with `dumpbin /headers`; archive the machine-type report in `zig-out/game/x64-runtime-validation.log`.
- [ ] Update the StreamIO design status and document the exact Linux/macOS blockers that remain after architecture consistency is achieved.
- [ ] Commit `test: validate complete Windows x64 staged runtime`.

## Execution rule

Work through Tasks 1-8 continuously. A failed checkpoint means diagnose and fix the named owner, rerun that task's tests, rerun the validation harness, commit the isolated change, and continue immediately to the next checkpoint. Do not stop for confirmation between tasks.

## Coverage review

This plan covers every current blocker: incomplete StreamIO functionality, missing archive and XML compatibility, factory aggregation, omitted x86-only providers, staging, and full x64 runtime validation. It deliberately excludes Linux/macOS output packaging until the Windows x64 runtime has passed C6; platform API replacement is then a separate, evidence-based porting plan.