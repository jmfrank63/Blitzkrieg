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
