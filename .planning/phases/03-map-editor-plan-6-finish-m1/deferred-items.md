# Deferred items - phase 03

- 2026-09-29 (03-15, mod-switch gap fix): `test-editor-bridge`'s `TestMissingSeasonTextureFallsBack` fails in a worktree whose `Data` holds the untracked, generated season textures (`season-textures`, c5cff9022): `105mm_M2A1_USA` now has a `1w`, so it is drawn with `\1w`, not the fallback `\1` the test expects. The test needs a unit that has no winter texture even after generation, or has to accept the generated one. Not fixed: out of scope for the mod switch.
