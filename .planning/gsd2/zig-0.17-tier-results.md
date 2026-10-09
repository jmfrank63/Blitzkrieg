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
| test-random-missions only=kharkov42 | `zig build test-random-missions -Dtest-mode=run -Drandom-missions-sweep=only=kharkov42` | TIMEOUT | 481 s | 480 s | 0.16 baseline pending (running next) |

## rootPath fix

(to be filled in)