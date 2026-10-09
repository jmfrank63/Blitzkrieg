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

## rootPath fix

(to be filled in)