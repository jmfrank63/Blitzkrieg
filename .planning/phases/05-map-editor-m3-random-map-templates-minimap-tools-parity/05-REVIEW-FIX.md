---
phase: 05
review: 05-REVIEW.md
fix_scope: critical_warning
findings_in_scope: 35
fixed: 35
skipped: 0
status: all_fixed
fixed_at: 2026-10-04
tip: 0a5076cf5
---

# Phase 5 review fixes

All 6 critical and 29 warning findings in `05-REVIEW.md` are fixed, one commit each. The 19 info findings are out of scope and remain open in `05-REVIEW.md`.

## Area A: Zig editor app

| Finding | Commit | Fix |
|---|---|---|
| CR-A01 | `6f993989c` | File > New wires the view, brush tiles and placer |
| CR-A02 | `a206b39df` | Fields Composer Remove no longer indexes the lists it just shrank |
| CR-A03 | `928daa6e2` | A long `filter_active` no longer overflows the Delete Filter popup buffer |
| WR-A01 | `66bb59561` | An unanswered single-instance hand-off is not treated as a dead owner |
| WR-A02 | `cc5f86e91` | `Instance.deinit` neither blocks on a replaced socket nor unlinks another editor's |
| WR-A03 | `97c14f6a7` | The hand-off keeps `-mod=` (0x1f separator) |
| WR-A04 | `14970e643`, `a0648c070` | The shared `/tmp` fallback endpoint is per-user 0700. The follow-up keeps every path inside macOS's 104-byte `sun_path`, which the first commit's test broke under a long `$TMPDIR` |
| WR-A05 | `d05d091aa` | Comment says what hung-peer handling does not cover |
| WR-A06 | `be791c7ec` | A never-saved (File > New) map autosaves and test-launches |
| WR-A07 | `08c1520f6` | A Select double click is delivered only on the selection |
| WR-A08 | `2138452a0` | Fix all re-runs Check Map first |
| WR-A09 | `d96b97440` | Template Diplomacy popup does not underflow on an empty table |
| WR-A10 | `e6d52b615` | Open and File > New delete the replaced document's recovery copy |

## Area B: editor core

| Finding | Commit | Fix |
|---|---|---|
| CR-B01 | `6d69f1030` | `deleteMany` is all-or-nothing and deletes passengers before hosts |
| WR-B01 | `6089b3f95` | A failed undo/redo of a paint stroke or multi-delete unwinds |
| WR-B02 | `01a712f95` | Rotation widened to i64 |
| WR-B03 | `070d5e472` | Control characters kept out of `mapeditor.cfg` values |
| WR-B04 | `43c99bfdc` | The once-per-session `.bak` is keyed by a normalised Windows path |
| WR-B05 | `e89e80965` | `Document.cancel` gives back what `begin` set aside |
| WR-B06 | `ddcb24c1c` | A map in a filesystem root keeps the root |

## Area C: engine bridge

| Finding | Commit | Fix |
|---|---|---|
| CR-C01 | `b2c318d41` | `BkEditorApplyField` answers OK with the token when the report buffer is too small |
| CR-C02 | `b0c3c04c5` | A delete around a link cycle is refused, and so is a link that would close one (`NMapRecords::WouldLinkCycle`) |
| WR-C01 | `583e173c5` | Unlinked objects are not passengers of link ID 0 |
| WR-C02 | `a877270f8` | NaN, infinite and out-of-range positions and directions are refused |
| WR-C03 | `242b5600b` | `SVertexAltitude` padding is named and zeroed |
| WR-C04 | `133becf62` | A part-way Update Map failure restores the map |
| WR-C05 | `3a72028dc` | `BkEditorMoveObjects` checks its array and count first |
| WR-C06 | `13cf4bdb5` | The user filter file is written aside and renamed |
| WR-C07 | `191b8bba2` | NewMap and ApplyField check their char arrays for a terminator |
| WR-C08 | `58936f2d6` | The snapshot's unit creation is put back when the working copy refuses |

## Area D: build and CI

| Finding | Commit | Fix |
|---|---|---|
| WR-D01 | `fe655c59e` | The M3 scenario's save paths land in the scratch folder |
| WR-D02 | `319993691` | `BK_USER_ROOT` keeps the M3 scenarios' user data out of `%APPDATA%` on Windows |
| WR-D03 | `9c9854568` | `BK_REQUIRE_ENGINE=1` on the Windows and macos-14 jobs turns an engine tool's skip into a failure |
| WR-D04 | `7fe456989` | Shipped RMG record counts are pinned as lower bounds |
| WR-D05 | `0a5076cf5` | Johannes chose to fix it. The Windows and macos-14 jobs now run `map-editor-m3-auto` (M1, M2, M3 scripts) and `map-editor-game-reads-it-m3` (the real Game on the M1, M2 and M3 maps, including D-33's short railroad) |

## Found while fixing (not in the review)

- `b15faedb3`: when `DeleteObjectInChain` refused the host itself, the passengers it had already deleted stayed deleted. The refusal now restores them, the fake bridge models the same behaviour, and `editor_bridge_test` covers a passenger of a bridge span.
- `3b8c231a0`: `-Drandom-missions-sweep=cover-from=<n>` resumes a cover sweep that was cut short.

## Gates on the merged tree (macOS arm64, logs `zig-out/local-test/05-fix-*.log`)

| Gate | Result |
|---|---|
| `zig build test` | 791/791. ECONNREFUSED stack traces from the std's unmapped errno 61 are printed but not failures |
| `build_hermeticity_test.zig` | 3/3 |
| `test-editor-bridge`, `test-map-editor-engine` | pass |
| `map-editor-smoke`, `map-editor-auto` | pass |
| `map-editor-auto-m2` | pass on rerun. The first run's smoke failed once with "ImGui does not want the mouse after 60 frames" while the machine was contended |
| `map-editor-m3-auto`, `map-editor-game-reads-it-m3` | pass |
| `test-random-missions` cover | 155 + 53 (`cover-from=155`) = 208 cases, 0 failed |

CI run 37151247396 at `0a5076cf5` is the cross-platform check, including the new WR-D05 steps.
