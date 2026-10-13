# Safe allocator tier results (M003 / S02 and later)

Durable record of every tier run on the BkMemory allocator, so a restarted session reads it and skips
what already has a row (override 2026-10-10T17:19, D081). Append one row at once after each tier; commit
the file after every two tiers with `docs(planning): safe allocator tier results, <tiers>`.

- Machine: win-home (Windows 11 x64), Zig 0.17.0, debug build
- Branch: fix/memory-leaks, code state ead1721dd (build.zig and Sources/src/BkMemory last changed 20:16, committed 20:55 +0700; nothing in the tree changed after)
- Each tier runs one at a time, bounded, under tier-logs, and is stopped by its own PID only (D080)

## How to read a row

- Exit: `0`, `1`, ... or `TIMEOUT`. `0 (inferred)` means the log was read after the fact and holds no `error:` and no `Build Summary:` line (zig prints those only on failure), so the build command exited 0.
- Duration: wall time. `<= N min` is an upper bound taken from the gap between the previous log's last write and this log's last write (the start time was not recorded).
- BK_MEM_REPORT: the mode on the run step. `setLeakReportLog` in build.zig sets `count` (D079 as refined); the leak summary line in each log confirms a report ran.

## Rows

| Tier | Command | Exit | Duration | BK_MEM_REPORT | Notes |
|------|---------|------|----------|---------------|-------|
| map-editor-host-check | `zig build map-editor-host-check` | 0 (inferred) | not recorded (log finished 22:46) | count | log zig-out/local-test/map-editor-host-check.log; host check, unknown-object, panel smoke and mod switch all PASS; `bk_mem: 45395 leaked block(s)` |
| map-editor-smoke | `zig build map-editor-smoke` | 0 (inferred) | <= 2 min (22:46 to 22:48) | count | smoke PASS (52 steps, 260 objects, saved and reopened); `bk_mem: 52655 leaked block(s)` |
| map-editor-auto | `zig build map-editor-auto` | 0 (inferred) | <= 2 min (22:48 to 22:50) | count | BK_EDITOR_AUTO done (13 actions); `bk_mem: 55913 leaked block(s)` |
| map-editor-auto-m2 | `zig build map-editor-auto-m2` | 0 (inferred) | <= 3 min (22:50 to 22:53) | count | BK_EDITOR_AUTO done (298 actions); `bk_mem: 59799 leaked block(s)` |
| map-editor-m3-auto | `zig build map-editor-m3-auto` | 0 (inferred) | <= 12 min (22:53 to 23:05) | count | BK_EDITOR_AUTO done (668 actions); editor-bridge PASS; `bk_mem: 171090 leaked block(s)`; the interval includes any idle time, real duration likely shorter |
| map-editor-game-reads-it | `zig build map-editor-game-reads-it` | 0 (inferred) | <= 3 min (23:05 to 23:08) | count | game log shows the Game ran the placed unit; `bk_mem: 136137 leaked block(s)`; baseline/edited game logs present |
| map-editor-game-reads-it-m2 | `zig build map-editor-game-reads-it-m2` | 0 (inferred) | <= 1 min (23:08 to 23:09) | count | log holds only game trace lines (BK_MAP_TRACE); the M2 PASS line is in the m3 log below |
| map-editor-game-reads-it-m3 | `zig build map-editor-game-reads-it-m3` | 1 | <= 4 min (23:09 to 23:13) | count | FAIL, harness artifact: `game reads it M3 FAIL: the report zig-out/local-test/map-editor-game-reads-it-m3.log would not write: FileBusy`. The run's stdout was redirected into the very file the tier writes its report to. M2 PASS and editor-bridge PASS (twice) are in that log. NOT a code failure: rerun with stdout/stderr redirected under tier-logs |
| test-editor-core | `zig build test-editor-core -Dtest-mode=run` | 0 | 1 s (cache hit, no source change since the last pass) | count | log zig-out/local-test/tier-logs/test-editor-core.log is empty: no inputs changed, so the cached pass stands |
| test-map-editor-view | `zig build test-map-editor-view -Dtest-mode=run` | 0 | 1 s (cache hit) | count | cached pass, empty log |
| test-map-editor-panels | `zig build test-map-editor-panels -Dtest-mode=run` | 0 | 1 s (cache hit) | count | cached pass, empty log |
| test-map-editor-testlaunch | `zig build test-map-editor-testlaunch -Dtest-mode=run` | 0 | 1 s (cache hit) | count | cached pass, empty log |
| test-map-editor-auto | `zig build test-map-editor-auto -Dtest-mode=run` | 0 | 1 s (cache hit) | count | cached pass, empty log |
| test-map-editor-engine | `zig build test-map-editor-engine -Dtest-mode=run` | 0 | 173 s | count | PASS (260 objects), `bk_mem: 73051 leaked block(s)`. First attempt (18 s) failed `stage: could not replace locked PlatformRuntime.dll: FileBusy`: a `map-editor-m3-auto` build (maker.exe PID 41156, started 00:31, not started by this session) was still running its MapEditor.exe. Not killed (D080); waited 210 s until it exited, then reran clean |
| test-editor-bridge | `zig build test-editor-bridge` (debug) | 0 | about 49 min (01:00:13 to 01:49:10), against the 1022 s baseline | count | editor-bridge PASS; `bk_mem: 151745 leaked block(s)`. Run started by the T04 attempt; T05 only waited for its maker.exe (PID 22372) to exit, it blocked `install-game` with RuntimeReplacementDenied meanwhile. Debug time is far over 1022 s: recorded for S04, not investigated here |
| install-game install-map-editor | `zig build install-game install-map-editor` | 0 | not timed | count | first attempt failed `RuntimeReplacementDenied` (bridge tier above held editor-bridge-test.exe); passes once it exited |
| resource-editor-host-check | `zig build resource-editor-host-check` | 0 | 87 s | count | host check PASS, docks PASS; `bk_mem: 43750 leaked block(s)` |
| resource-editor-smoke | `zig build resource-editor-smoke` | 0 | 83 s | count | smoke PASS, smoke-edit PASS; `bk_mem: 43703 leaked block(s)` (last run in log) |
| resource-editor-batch | `zig build resource-editor-batch` | 0 | 172 s | count | batch check PASS (21 projects); `bk_mem: 43588 leaked block(s)` |
| resource-editor-game-reads-it | `zig build resource-editor-game-reads-it` | 0 | 288 s | count | auto PASS (154 actions); `bk_mem: 43709 leaked block(s)` |
| resource-editor-auto-core | `zig build resource-editor-auto-core` | 0 | 140 s | count | auto PASS; `bk_mem: 43713 leaked block(s)` (last report in log) |
| resource-editor-auto-wpn | `zig build resource-editor-auto-wpn` | 0 | 94 s | count | auto PASS; `bk_mem: 43713 leaked block(s)` (last report in log) |
| resource-editor-auto-unt | `zig build resource-editor-auto-unt` | 0 | 107 s | count | auto PASS; `bk_mem: 43773 leaked block(s)` (last report in log) |
| resource-editor-auto-spt | `zig build resource-editor-auto-spt` | 0 | 95 s | count | auto PASS; `bk_mem: 43734 leaked block(s)` (last report in log) |
| resource-editor-auto-msh | `zig build resource-editor-auto-msh` | 0 | 149 s | count | auto PASS; `bk_mem: 44151 leaked block(s)` (last report in log) |
| resource-editor-auto-obt | `zig build resource-editor-auto-obt` | 0 | 93 s | count | auto PASS; `bk_mem: 43717 leaked block(s)` (last report in log) |
| resource-editor-auto-fnc | `zig build resource-editor-auto-fnc` | 0 | 94 s | count | auto PASS; `bk_mem: 43711 leaked block(s)` (last report in log) |
| resource-editor-auto-bld | `zig build resource-editor-auto-bld` | 0 | 100 s | count | auto PASS; `bk_mem: 43717 leaked block(s)` (last report in log) |
| resource-editor-auto-bdg | `zig build resource-editor-auto-bdg` | 0 | 107 s | count | auto PASS; `bk_mem: 43711 leaked block(s)` (last report in log) |
| resource-editor-auto-pcp | `zig build resource-editor-auto-pcp` | 0 | 122 s | count | auto PASS; `bk_mem: 43769 leaked block(s)` (last report in log) |
| resource-editor-auto-eff | `zig build resource-editor-auto-eff` | 0 | 105 s | count | auto PASS; `bk_mem: 43705 leaked block(s)` (last report in log) |
| resource-editor-auto-til | `zig build resource-editor-auto-til` | 0 | 89 s | count | auto PASS; `bk_mem: 43704 leaked block(s)` (last report in log) |
| resource-editor-auto-3rd | `zig build resource-editor-auto-3rd` | 0 | 88 s | count | auto PASS; `bk_mem: 43226 leaked block(s)` (last report in log) |
| resource-editor-auto-3rv | `zig build resource-editor-auto-3rv` | 0 | 90 s | count | auto PASS; `bk_mem: 43472 leaked block(s)` (last report in log) |
| resource-editor-auto-mip | `zig build resource-editor-auto-mip` | 0 | 88 s | count | auto PASS; `bk_mem: 43714 leaked block(s)` (last report in log) |
| resource-editor-auto-chc | `zig build resource-editor-auto-chc` | 0 | 88 s | count | auto PASS; `bk_mem: 43721 leaked block(s)` (last report in log) |
| resource-editor-auto-cgc | `zig build resource-editor-auto-cgc` | 0 | 87 s | count | auto PASS; `bk_mem: 43721 leaked block(s)` (last report in log) |
| resource-editor-auto-mdc | `zig build resource-editor-auto-mdc` | 0 | 85 s | count | auto PASS; `bk_mem: 43703 leaked block(s)` (last report in log) |
| resource-editor-auto-gui | `zig build resource-editor-auto-gui` | 0 | 339 s | count | auto PASS; `bk_mem: 43420 leaked block(s)` (last report in log) |

## Still to run (T04 leftovers)

map-editor-game-reads-it-m3 (rerun, redirect elsewhere), Game headless start. Full sweep (tools/zig/run-resource-sweep.sh) left to the maintainer.
| test-random-missions (safe, first try) | `zig build test-random-missions --release=fast -Drandom-missions-repeat=20 -Dtest-mode=run` | 1 (exit 3 panic) | 311 s | count | `bk_mem_free ... bad header magic` from SDL_GetDisplayForWindow: the tool called SDL_Init before BkMemoryInstallSdlFunctions. Fixed in four tools |
| test-random-missions (safe, sweep all) | same, after the SDL fix | cut at 590 s | 591 s | count | 440 cases of the `all` sweep done, heap flat at 101535308 B; the full sweep x 20 is hours, so the gate sweep below is used |
| test-random-missions backend=safe | `zig build test-random-missions --release=fast -Drandom-missions-repeat=20 -Dtest-mode=run -Dbk-mem-allocator=safe -Drandom-missions-sweep=only=summer_ukraine\securearea00` | 0 | 188 s (120 s run) | count | heap 93200887 after round 10 and 20, gate pass |
| test-random-missions backend=smp | same with `-Dbk-mem-allocator=smp` | 0 | 221 s (127 s run) | count | heap 93190137 / 93189856, gate pass |
| test-random-missions backend=crt | same with `-Dbk-mem-allocator=crt` | 1 | 154 s | count | link fails: duplicate `_cexit`, `_invalid_parameter_noinfo`, `_wctype`, `__pctype_func`, `exit` |
| Game frame time, smp then safe x2 | `powershell zig-out/local-test/t06-frametime.ps1 -Label <x>` (Game.exe -scenarios\scenariomissions\german\kharkov42\1.xml, 75 s) | n/a | 75 s each | count | smp 10.02 / 10.58 ms mean / p95; safe 9.48 / 9.96 and 9.52 / 10.14 |
| install-game install-map-editor, install-resource-editor, bk-memory-import-audit | `zig build ... --release=fast` | 0 | 114 s / 78 s / 63 s | count | audit: 25 modules, 0 failures. Editors in the layout were stale until reinstalled; the audit step only depends on install-game, so run it after the editor installs |

## M003/S03/T05: Map Editor host on BkMemory (debug, win-home)

| Tier | Command | Exit | Time | Mode | Result |
|---|---|---|---|---|---|
| map-editor-planted-leak | `zig build map-editor-planted-leak` | 0 | about 110 s | count | clean 56059 leaked blocks, planted 56060 (+1, BK_ZIG_PLANT_LEAK=1), both exit 0 |
| test-editor-core, -view, -panels, -testlaunch, -auto | `zig build test-editor-core test-map-editor-view test-map-editor-panels test-map-editor-testlaunch test-map-editor-auto -Dtest-mode=run` | 0 | 15 s | zig test | all pass; the WR-A02 single-instance test takes its detached accept thread's memory from static storage, not the leak-checked allocator |
| test-map-editor-engine | `zig build test-map-editor-engine` | 0 | 180 s | count | pass; `bk_mem: 73568 leaked block(s)` |
| map-editor-host-check | `zig build map-editor-host-check` | 0 | 111 s | count | pass; `bk_mem: 56059`, `56059`, `45924` leaked block(s) |
| map-editor-smoke | `zig build map-editor-smoke` | 0 | 80 s | count | smoke PASS (52 steps); `bk_mem: 53174` |
| map-editor-auto | `zig build map-editor-auto` | 0 | 118 s | count | pass; `bk_mem: 53196`, `56435` |
| map-editor-auto-m2, map-editor-m3-auto, map-editor-game-reads-it-m3 | not run | n/a | n/a | n/a | left to the maintainer |

## M003/S03/T06: Resource Editor host, planted-leak delta and Zig allocator audit (debug, win-home)

| Tier | Command | Exit | Time | Mode | Result |
|---|---|---|---|---|---|
| zig-allocator-audit | `zig build zig-allocator-audit` | 0 | 2 s | n/a | 150 files scanned, no forbidden allocator |
| test-zig-allocator-audit | `zig build test-zig-allocator-audit -Dtest-mode=run` | 0 | 2 s | zig test | planted line reported with file:line; comment/string and allowlist cases pass |
| resource-editor-planted-leak, first try | `zig build resource-editor-planted-leak` (plant 1, exact +1) | 1 | 96 s | count | clean 44243, planted 44243, then clean 44242, planted 44244: the base count jitters, so exact +1 is unsound |
| resource-editor-planted-leak | `zig build resource-editor-planted-leak` (`--plant 64 --slack 8`) | 0 | 107 s | count | clean 44242, planted 44307 (+65 within 64 +/- 8) |
| map-editor-planted-leak, rerun | `zig build map-editor-planted-leak` (T05's exact +1 failed once, clean 56060 = planted 56060, jitter) | 0 | 87 s | count | now `--plant 64 --slack 8`: clean 56059, planted 56124 (+65) |
| test-resource-core | `zig build test-resource-core -Dtest-mode=run` | 0 | 23 s | zig test | pass |
| resource-editor-host-check | `zig build resource-editor-host-check -Dtest-mode=run` | 0 | 72 s | count | pass; `bk_mem: 44242` leaked block(s) |
| resource-editor-smoke | `zig build resource-editor-smoke -Dtest-mode=run` | 0 | 84 s | count | pass |
| resource-editor-auto-core | `zig build resource-editor-auto-core -Dtest-mode=run` | 0 | 135 s | count | pass |
| full resource sweep | `tools/zig/run-resource-sweep.sh` | n/a | n/a | n/a | left to the maintainer |

## M003/S03/T07: SDL hook in resource_bridge_test, BkMemory in the comparator (win-home)

| Tier | Command | Exit | Time | Mode | Result |
|---|---|---|---|---|---|
| test-resource-bridge, debug | `zig build test-resource-bridge -Dtest-mode=run` | n/a | over 30 min | count | no verdict: debug run still at 100% CPU after 33 min and was stopped; a stale resource-bridge-test from the interrupted session also held PlatformRuntime.dll (FileBusy) and was killed first |
| test-resource-bridge | `zig build test-resource-bridge -Dtest-mode=run -Dbuild-variant=release` | 0 | 327 s | count | pass: all BATCH lines, `resource-bridge: Project+Tree OK (21 fixtures)`, no bad-header-magic panic |
| test-resource-model-comparator, debug | `zig build test-resource-model-comparator -Dtest-mode=run` | n/a | over 10 min | count | linked (the undefined `_bk_mem_*` fix is for macOS); debug run stopped, too slow for one command |
| test-resource-model-comparator | `zig build test-resource-model-comparator -Dtest-mode=run -Dbuild-variant=release` | 0 | under 10 min | count | `VERDICT=PASS` |
| cross compile x86_64-linux-gnu | `zig build test-resource-bridge test-resource-model-comparator -Dtarget=x86_64-linux-gnu -Dtest-mode=compile` | 1 | n/a | compile | win-home has no Linux libc headers: `'stdio.h' file not found` from PortableCrt.h in MapFile and zlib (not code this task touched), plus a libSDL3.so symlink PermissionDenied. CI (Linux job) checks it |
| cross compile aarch64-macos | `zig build test-resource-bridge test-resource-model-comparator -Dtarget=aarch64-macos -Dtest-mode=compile` | 1 | n/a | compile | needs `-Dsystem_include_path`, `-Dsystem_framework_path`, `-Dlibrary_path` (macOS SDK sysroot) that win-home lacks; configure stops before compiling. CI macOS arm64 checks the link |
| cross compile x86_64-macos | `zig build test-resource-bridge test-resource-model-comparator -Dtarget=x86_64-macos -Dtest-mode=compile` | 1 | n/a | compile | same sysroot requirement; CI macOS x64 checks the `_bk_mem_*` link |

## Engine-linking executables (T08)

The audit asked for by D085/R053: every `addExecutable` in build.zig whose module links an engine
library, zlib, libpng, Lua or SDL, with its BkMemory link and SDL hook. The hook is one of:
`static` - SDLApplication.cpp's `g_sdlMemoryRouted` static initializer (Sources/src/Platform/SDLApplication.cpp:34),
before `main`; `main` - an explicit `BkMemoryInstallSdlFunctions()` first thing in the tool's `main`;
`zig` - `bk_memory.installSdlMemoryFunctions(sdl3.c)` before `SDL_Init`, or kit/host.zig's
`SDL_SetMemoryFunctions` for the editors. Audit made 2026-10-11 on win-home.

| Executable (step) | Engine link | BkMemory | SDL hook | CI jobs |
|---|---|---|---|---|
| Game.exe (install-game) | full engine | linkEngineCxxRuntimeEngine | static | windows-game, mingw-game, windows-arm-game, macos-game, macos-intel-game, linux-game, linux-arm-game |
| MapEditor.exe (install-map-editor and every map-editor-* tier) | full engine + editor bridge | linkBkMemoryAllocator + bridge | zig (kit/host.zig:45) | windows-map-editor, mingw-map-editor, macos-map-editor, macos-intel-map-editor, linux-map-editor, linux-arm-map-editor |
| ResourceEditor.exe (install-resource-editor and every resource-editor-* tier) | full engine + editor bridge | linkBkMemoryAllocator (resource_app/main.zig:133) | zig (kit/host.zig:45) | windows-resource-editor, mingw-resource-editor, macos-resource-editor, macos-intel-resource-editor, linux-resource-editor, linux-arm-resource-editor |
| editor-bridge-test (test-editor-bridge) | full engine | linkEngineCxxRuntimeEngine + audit | main (editor_bridge_test.cpp:11763) | windows-game, macos-game, linux-game |
| resource-bridge-test and the D-11 comparator test (test-resource-bridge, test-resource-model, test-resource-model-comparator, test-resource-mod-roundtrip) | full engine | linkEngineCxxRuntimeEngine + audit | main (resource_bridge_test.cpp:9284, resource_mod_roundtrip_test.cpp:807) | windows-game/-resource-editor, macos-*, linux-* |
| random-missions-test, rmg-determinism-test, composer-roundtrip-test, preview-scene-spike (test-random-missions, test-rmg-*) | full engine | linkEngineCxxRuntimeEngine + audit | main (random_missions_test.cpp:423, rmg_determinism_test.cpp:229, composer_roundtrip_test.cpp:435, preview_scene_spike.cpp:198) | windows-game (random missions all/only), macos-game, linux-game |
| platform-runtime test family (test-platform-foundation, test-platform-system, test-platform-modules, test-platform-debug, test-platform-socket-abi, test-platform-events, test-platform-controller, test-platform-clipboard, test-platform-network, test-network-*, test-game-frame, test-game-loop, test-game-bootstrap, test-game-command-line, game system keys/mouse capture, platform window test) | Platform/System, SDLApplication, sockets | linkEngineCxxRuntimeFlags (T08) + audit for the SDL ones | static (SDLApplication.cpp:34) or main (platform_system_test.cpp:27, network_system_gate.cpp:22) | windows-game (foundation matrix), mingw-platform, macos-intel-platform, linux-arm-platform; the rest build per game/map job |
| gfxgpu-factory-test (gfxgpu-factory-test) | engine + SDL GPU | linkEngineCxxRuntimeEngine + audit | main (gfxgpu_factory_test.cpp:242) | windows-game, linux-game, macos-game |
| gfxgpu-abi-test, gfxgpu core/compatibility tests | GfxGpuZig + engine | linkEngineCxxRuntimeEngine / linkBkMemoryAllocator | static or none (no SDL window) | windows-game, macos-game, linux-game |
| editor-overlay-spike, gfxgpu-smoke, gpu-device-probe (Zig SDL hosts) | GfxGpuZig + sdl3 | linkBkMemoryAllocator | zig (T08: editor_overlay_spike.zig:52, gfxgpu_smoke.zig:297, gpu_device_probe.zig:17) | gpu-device-probe all platform jobs; overlay spike build-only (editor-overlay-spike-build) on windows-map-editor, mingw-map-editor, linux-map-editor |
| bk-memory-zig-host-test, bk-memory cross-module test, platform clock/sync/dynamic/storage gates, cloudsync and streamio tests, input/sfx/ui/anim/font/gfx module tests | engine libraries per module | linkEngineCxxRuntime* (S02) | not SDL users (or static via SDLApplication) | platform/game jobs per tier |

Guard: `bk-memory-import-audit` now also audits the T08 test executables. Registrations made before
the audit run step exists are queued and drained after it (build.zig `bk_audit_pending`), which is
what let platform-system-test (created earlier in build()) join. Game-bootstrap-smoke is the one
C++ exe that cannot take `linkEngineCxxRuntime` whole: it links GfxGpuZig, and on MSVC that static
library already brings Zig's libc, so the helper's linkMsvcRuntime names a second CRT (duplicate
`_cexit`/`_wctype`, measured twice). It gets new_delete.cpp and the BkMemory import directly
(build.zig addGameBootstrapSmoke) without the MSVC CRT, which the audit accepts.

| Tier | Command | Exit | Time | Mode | Result |
|---|---|---|---|---|---|
| test-platform-system | `zig build test-platform-system -Dtest-mode=run` | 0 | minutes | count | pass; parent `bk_mem: 0 leaked block(s)`, child 1 |
| test-network-system-gate | `zig build test-network-system-gate -Dtest-mode=run` | 0 | minutes | count | `network/system gate passed: fixture=c70d495e tcp=4 child=23`; `bk_mem: 0` |
| test-platform-controller | `zig build test-platform-controller -Dtest-mode=run` | 0 | minutes | count | `platform controller passed`; `bk_mem: 0` |
| test-platform-socket-abi | `zig build test-platform-socket-abi -Dtest-mode=run` | 0 | minutes | count | `platform socket ABI passed`; `bk_mem: 0` |
| test-platform-events | `zig build test-platform-events -Dtest-mode=run` | 0 | minutes | count | pass (SDL event translation); `bk_mem: 0` |
| test-game-frame | `zig build test-game-frame -Dtest-mode=run` | 0 | minutes | count | `game frame contract compiled`; `bk_mem: 229 leaked block(s)` (S04's ledger, not this task's) |
| test-game-loop | `zig build test-game-loop -Dtest-mode=run` | 0 | minutes | count | `game loop policy passed`; `bk_mem: 0` |
| test-game-bootstrap | `zig build test-game-bootstrap -Dtest-mode=run` | 0 | minutes | count | after the new_delete/BkMemory change: `game bootstrap: SDL window, event/controller services, 3 GfxGpu restart cycles, clean shutdown`; `bk_mem: 0` |
| gpu-device-probe | `zig build gpu-device-probe` | 0 | seconds | n/a | `video=yes device=yes driver=direct3d12 window=yes claim=yes` with the T08 hook |
| gfxgpu-smoke | `zig build gfxgpu-smoke` | 0 | minutes | n/a | direct3d12, 25 shader pairs created and released with the T08 hook |
| editor-overlay-spike | `zig build editor-overlay-spike -- --hidden` | 0 | minutes | n/a | `overlay-spike: PASS driver=direct3d12 ... hidden=true` with the T08 hook |
| test-resource-bridge | `zig build test-resource-bridge -Dtest-mode=run -Doptimize=ReleaseFast` | 0 | minutes | count | `resource-bridge: Project+Tree OK (21 fixtures)`, no bad-header-magic panic |
| test-resource-model-comparator | `zig build test-resource-model-comparator -Dtest-mode=run -Doptimize=ReleaseFast` | 0 | minutes | count | `VERDICT=PASS` |
| test-resource-model | `zig build test-resource-model -Dtest-mode=run -Doptimize=ReleaseFast` | 0 | minutes | count | `VERDICT=PASS` (aggregate sweep over shipped data) |
| test-resource-bridge, debug | `zig build test-resource-bridge -Dtest-mode=run` | TIMEOUT | stopped at 10 min | count | same shape as T07's 33+ min debug record: SafeAllocator stack recording, S04's topic |
| bk-memory-import-audit | `zig build bk-memory-import-audit` | 0 | minutes | n/a | `33 module(s) audited, 0 failure(s)` including the T08 registrations |
| cross compile x86_64-linux-gnu | `zig build test-resource-bridge -Dtarget=x86_64-linux-gnu -Dtest-mode=compile` | 1 | n/a | compile | environment-blocked as in T07: no Linux libc headers on win-home (`'stdio.h' file not found` from PortableCrt.h) plus libSDL3.so symlink PermissionDenied; CI Linux checks it |
| cross compile aarch64-macos | `zig build test-resource-bridge -Dtarget=aarch64-macos -Dtest-mode=compile` | 1 | n/a | compile | environment-blocked: macOS SDK sysroot options required, win-home lacks them; CI macOS arm64 checks it |
| cross compile x86_64-macos | `zig build test-resource-bridge -Dtarget=x86_64-macos -Dtest-mode=compile` | 1 | n/a | compile | same sysroot requirement; CI macOS x64 checks it |

Debug runs of the touched tiers were each stopped by the one-command bound, not by a failure; the
release runs above and CI's debug jobs are the proof for them.

| resource-editor (T02 direct, BK_MEM_REPORT=log) | ResourceEditor.exe --check wpn <tga> <fixture> | 0 | ~13 min (600 s wait + capture) | log | logs s04-t02-resource-editor.out/err.log; 44,232 blocks / 74.6 MB; the small-alloc string family + the 70.2 MB backing; matches count baseline |
| map-editor (T02 direct) | MapEditor.exe --check coldwinter <tga> | 0 | ~30 min incl. capture | log | s04-t02-map-editor.out/err.log; 56,059 blocks / 77.9 MB; same shape |
| game (T02 direct, 60 s menu, graceful close) | Game.exe | 0 | ~75 s | log | s04-t02-game.out/err.log; 1,224 blocks / 127 KB; 1,221 the same family |
## M003/S04/T06: hidden test windows do not load libdecor/GTK (win-home build, WSL clone run)

The change: SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR, "0") before the resource-bridge
test's hidden window (Sources/src/EditorBridge/resource_bridge_test.cpp) and around the borderless
splash's creation, restored after (Sources/src/Platform/SDLApplication.cpp). The main window keeps
the caller's value: Wayland negotiates decorations at window creation, so a window that will be
shown on a desktop must still be allowed libdecor or it loses its title bar there.

| Tier | Command | Exit | Time | Mode | Result |
| --- | --- | --- | --- | --- | --- |
| resource-editor-host-check (WSL clone, patch applied) | zig build resource-editor-host-check (~/zig-0.17.0/zig) | 0 | about 9 min incl. rebuild | log | host check PASS, docks PASS; k_mem: 3108 leaked block(s); zero libdecor|gtk lines in the whole tier log |
| the same, patch stashed (A/B) | same | 0 | about 9 min | log | identical output, also zero libdecor|gtk lines - see the blocker below |
| libdecor presence | ldconfig -p \| grep -c libdecor (WSL distro) | 0 matches | - | - | libdecor is not installed in the WSL distro at all |

Environment-blocked, as the task anticipated: this WSL distro ships no libdecor, its non-GUI shells
export neither WAYLAND_DISPLAY nor DISPLAY (WSLg's Weston socket exists at
/mnt/wslg/runtime-dir/wayland-0 but the distro has nothing to load against it), and the Linux CI jobs
are X11 (xvfb) only. Weston answers xdg-decoration server-side, so even a Wayland run here would not
dlopen libdecor's GTK fallback - the GTK traces in the original dzn/lvp leak reports came from the
maintainer's interactive desktop. The negative cannot be exercised outside that desktop; the hint is
SDL's documented switch (SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR=0 never loads the library), the never-shown
windows are the ones covered, and the tier's exit-0 half of the verify is met on the WSL clone.


## M003/S04/T07: the policy verdict's verification runs (win-home, debug)

| Tier | Command | Exit | Time | Mode | Result |
| --- | --- | --- | --- | --- | --- |
| test-platform-system | zig build test-platform-system | 0 | about 40 s | count | the tier passes in its current mode; its 1-block residue (SDL_GetBasePath, 114 B) and the fix's 3-block report anomaly are recorded in safe-allocator-design.md's policy table |
| test-bk-memory | zig build test-bk-memory | 0 | about 20 s | strict + planted | clean run passes; the planted-leak run asserts exit 3 with leaked on stderr - the strict proof |
| test-bk-memory-zig-host | zig build test-bk-memory-zig-host | 0 | about 15 s | strict + planted | clean run passes; the planted Zig leak fails as designed |
| map-editor-smoke | zig build map-editor-smoke | 0 | about 100 s | count | smoke PASS (52 steps, 260 objects); 1,897 blocks - the engine's accepted floor for that tier |

The A/B that decided platform-system-test's verdict: the fixed ExecutablePath (copy, SDL_free, return)
reported 3 blocks (the return object at System.cpp:69 via main:42, capturedUrl at platform_system_
	est.cpp:20, childName at :36) against the unfixed build's 1 (the SDL base path) - the fixed build's
extras are blocks its destructors demonstrably free (the unfixed run shows them gone), so the report, not
the engine, is in question. Exe hashes: new 8552073d, old 8c34b33a in .zig-cache/o/.

