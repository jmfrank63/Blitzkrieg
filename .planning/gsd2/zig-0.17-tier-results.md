# Zig 0.17 core tier results (S04 / T02)

- Date: 2026-10-10
- Machine: win-home (Windows 11 x64)
- Zig: 0.17.0
- Commit at start: 0a474bea4 (branch chore/zig-0.17)
- Bound: 480 s per tier from the 2026-10-09T22:19 steer (D068; the first two rows ran under the earlier 900 s bound), whole process tree killed on expiry

| Tier | Command | Exit | Duration | Limit | 0.16 comparison |
|------|---------|------|----------|-------|-----------------|
| test-editor-core | `zig build test-editor-core -Dtest-mode=run` | 0 | 1 s (cache hit: "run test cached"; identical inputs already passed under 0.17.0) | 900 s | CI green at 4d7645fc5 (windows-map-editor / windows-game) |
| test-platform-foundation | `zig build test-platform-foundation -Dtest-mode=run` | 0 | 1 s (cache hit: every "run test" step cached; identical inputs already passed under 0.17.0) | 900 s | CI green at 4d7645fc5 (windows-game) |
| test-editor-bridge | `zig build test-editor-bridge -Dtest-mode=run` (debug) | TIMEOUT (no result; wrapper printed "exit 1" at 899 s under the earlier 900 s bound) | 899 s | 900 s (earlier bound) | Fails locally on 0.16.0 at 4d7645fc5 too, in ..\Blitzkrieg-zig016: my run TIMEOUT at 901 s; maintainer's runs (D069) exited 255 after 1.7 min and after 14.8 min. All stop at `adding game type 100 (4385 in the catalogue) as 20mm_aviacannon`. CI windows-game passes this tier at 4d7645fc5. Pre-existing local failure on win-home, not a 0.17 regression; CI is the proof, re-checked in S05. |
| test-editor-kit | `zig build test-editor-kit -Dtest-mode=run` | 0 | 1 s (cache hit, "run test cached"; identical inputs already passed under 0.17.0) | 480 s | CI green at 4d7645fc5 (windows-map-editor / windows-resource-editor) |
| test-map-files | `zig build test-map-files -Dtest-mode=run` | 0 | 45 s | 480 s | CI green at 4d7645fc5 (windows-game) |
| test-random-missions only=kharkov42 | `zig build test-random-missions -Dtest-mode=run -Drandom-missions-sweep=only=kharkov42` | TIMEOUT | 481 s (22 missions done) | 480 s | 0.16.0 at 4d7645fc5 in ..\Blitzkrieg-zig016, same command: TIMEOUT at 481 s (21 missions done, includes cold compile). Same pace, so the debug sweep is simply longer than 480 s locally; pre-existing, not a 0.17 regression. CI windows-game runs it as a full sweep at 4d7645fc5 and is green. |
| test-random-missions leak gate | `zig build test-random-missions --release=fast -Dtest-mode=run -Drandom-missions-sweep=only=summer_ukraine\securearea00 -Drandom-missions-repeat=20` | 0 | 171 s (6 cases, 0 failed, 116 s of run time) | 480 s | Heap flat: 112469994 bytes after round 10, 112470154 after round 20 (+160 bytes). CI green at 4d7645fc5 (windows-game runs this gate). |

## rootPath fix

`rootPath(b, sub)` returns the build root joined with `sub` as an absolute string for run-step arguments and environment variables.

- Before (0.17 port, `b.root.joinString`): the sub path is kept as written, so `rootPath(b, "zig-out/local-test/x")` was `C:\...\Blitzkrieg\zig-out/local-test/x`: a "/" inside a Windows path.
- After (`b.pathResolve(&.{joined})`): native separators only, `C:\...\Blitzkrieg\zig-out\local-test\x`.

Which tier failed without it: **map-editor-smoke** (and map-editor-auto, which runs the same smoke step), measured on 0.17.0 with the fix reverted and build.zig restored afterwards (`git status` clean):

- `map-editor: smoke FAIL: another installation's map opens read-only: the document is C:\...\zig-out\local-test\map-editor-smoke-foreign\Data\Maps\Multiplayer\coldwinter.bzm, want C:\...\zig-out/local-test\map-editor-smoke-foreign\Data\Maps\Multiplayer\coldwinter.bzm` (exit 1, 62 s). The editor compares the path it built (backslashes) with the string build.zig passed (mixed separators), so the two equal paths differ.
- With the fix: `zig build map-editor-smoke -Dtest-mode=run` exit 0, 81 s.
- map-editor-host-check passes both with and without the fix (78 s and 91 s), it only writes the mixed path into a log line.

## Map Editor (S04 / T03)

- Date: 2026-10-10
- Zig: 0.17.0, debug, win-home real desktop, commit at start d44cfdf54 (includes the rootPath fix)
- Bound: 480 s per tier, whole process tree killed on expiry; none expired

| Tier | Command | Exit | Duration | Limit | 0.16 comparison |
|------|---------|------|----------|-------|-----------------|
| test-map-editor-view, -panels, -testlaunch, -auto | `zig build test-map-editor-view test-map-editor-panels test-map-editor-testlaunch test-map-editor-auto -Dtest-mode=run` | 0 | 1 s (cache hit, every "run test" step cached; identical inputs already passed under 0.17.0) | 480 s | CI windows-map-editor green at 4d7645fc5 |
| test-map-editor-engine | `zig build test-map-editor-engine -Dtest-mode=run` | 0 | 98 s | 480 s | CI windows-map-editor green at 4d7645fc5 |
| map-editor-host-check | `zig build map-editor-host-check -Dtest-mode=run` | 0 | 77 s | 480 s | CI windows-map-editor green at 4d7645fc5 |
| map-editor-smoke | `zig build map-editor-smoke -Dtest-mode=run` | 0 | 66 s | 480 s | CI windows-map-editor green at 4d7645fc5 (needs the rootPath fix on 0.17, see above) |
| map-editor-auto | `zig build map-editor-auto -Dtest-mode=run` | 0 | 89 s | 480 s | not in CI; passes on 0.17, no baseline needed |
| map-editor-auto-m2 | `zig build map-editor-auto-m2 -Dtest-mode=run` | 0 | 123 s | 480 s | not in CI; passes on 0.17, no baseline needed |
| map-editor-m3-auto | `zig build map-editor-m3-auto -Dtest-mode=run` | 0 | 384 s (668 actions) | 480 s | CI windows-map-editor green at 4d7645fc5 |
| map-editor-game-reads-it-m3 | `zig build map-editor-game-reads-it-m3 -Dtest-mode=run` | 0 | 274 s | 480 s | CI windows-map-editor green at 4d7645fc5 |

The two longest tiers sit at 80% and 57% of the bound; m3-auto is the one to watch if the machine is loaded.

## Resource Editor (part 1) (S04 / T04)

- Date: 2026-10-10
- Zig: 0.17.0, debug, win-home real desktop, commit at start 03e0824bf
- Bound: 480 s per tier (run-bounded.ps1), whole process tree killed on expiry
- 0.16 comparison: every step below is listed in the windows-resource-editor job of .github/workflows/cross-platform.yml, green at 4d7645fc5

| Tier | Command | Exit | Duration | Limit | 0.16 comparison |
|------|---------|------|----------|-------|-----------------|
| resource-editor-host-check | `zig build resource-editor-host-check -Dtest-mode=run` | 0 | 59 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-smoke | `zig build resource-editor-smoke -Dtest-mode=run` | 0 | 60 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-batch | `zig build resource-editor-batch -Dtest-mode=run` | 0 | 72 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-game-reads-it | `zig build resource-editor-game-reads-it -Dtest-mode=run` | 0 | 156 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-core | `zig build resource-editor-auto-core -Dtest-mode=run` | 0 | 129 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-wpn | `zig build resource-editor-auto-wpn -Dtest-mode=run` | 0 | 67 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-unt | `zig build resource-editor-auto-unt -Dtest-mode=run` | 0 | 65 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-spt | `zig build resource-editor-auto-spt -Dtest-mode=run` | 0 | 68 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-msh | `zig build resource-editor-auto-msh -Dtest-mode=run` | 0 | 88 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-obt | `zig build resource-editor-auto-obt -Dtest-mode=run` | 0 | 64 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-fnc | `zig build resource-editor-auto-fnc -Dtest-mode=run` | 0 | 64 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-bld | `zig build resource-editor-auto-bld -Dtest-mode=run` | 0 | 64 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-bdg | `zig build resource-editor-auto-bdg -Dtest-mode=run` | 0 | 66 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |
| resource-editor-auto-pcp | `zig build resource-editor-auto-pcp -Dtest-mode=run` | 0 | 71 s | 480 s | CI windows-resource-editor green at 4d7645fc5 |

All 14 tiers pass on Zig 0.17.0; none came near the 480 s bound (longest: game-reads-it at 156 s). The aggregate `resource-editor-auto` and the remaining auto tiers (eff, til, 3rd, 3rv, mip, chc, cgc, mdc, gui) are part 2; the full sweep is left to the maintainer.
