---
schema_version: 1
open_count: 1
waived_count: 0
fixed_count: 4
total_count: 5
last_updated: 2026-10-02T15:34:14.928Z
---

# Broken Windows Ledger

> Cross-phase defect register. With `workflow.windows_enforce` enabled, `/gsd-ship` blocks while `open_count > 0`.
> Waive with `gsd-tools windows waive <id> "<reason>"` (reason required).
> Mark fixed with `gsd-tools windows fixed <id>`.

| id | phase | kind | file | line | description | status | reason | recorded_at | resolved_at |
|----|-------|------|------|------|-------------|--------|--------|-------------|-------------|
| 1 | 03 | deviation | Sources/editor/app/panels.zig |  | plan-5 carried: the status line is never cleared after a later success (Task 4) | fixed | fixed in 03-16 d421402a8: status messages carry their source; TestLaunchPrompt.noteLaunch clears a Test-in-game failure on a later start or modal failure (tests in test-map-editor-panels); lost-frame and dialog-busy messages also cleared by their own success | 2026-09-28T17:53:06.985Z | 2026-09-29T10:08:34.494Z |
| 2 | 03 | unrun-verify | Sources/editor/app/view.zig |  | plan-5 carried: no test of view.zig's event-to-tool wiring beyond the routing function (Task 4) | fixed | fixed in 03-16 294bcdf74: View = ViewWith(SdlInput), bridge as anytype; 19 view.zig tests feed real SDL events through handleEvent/update against fake bridges, incl. D-15 per-map camera restore; run by test-map-editor-view and CI's unit tier | 2026-09-28T17:53:07.093Z | 2026-09-29T10:08:34.609Z |
| 3 | 03 | deviation | Sources/editor/app/view_math.zig |  | plan-5 carried: the scroll-direction unit test restates its own constants; an engine-tier ScreenToWorld direction check would catch a sign error (Task 6) | fixed | fixed in 03-16 82a08a462: the direction tests assert literal camera positions (a notch up: 2000,2000 -> 1971.716,2028.284); view.zig's wheel tests do the same through SDL events; the engine tier's TestWorldToScreenRoundTrip already checks screen up is world (-x,+y) | 2026-09-28T17:53:07.199Z | 2026-09-29T10:08:34.712Z |
| 4 | 05 | unmet-truth | Sources/src/EditorBridge/session_fields.cpp |  | The engine's field fill leaves its final scanline's tile picks unreproducible across seeded replays (recorded in 05-03-SUMMARY deviations; the touched-cell proof and the byte-exact undo proofs carry the contract) | fixed | fixed on fix/m3-fields-flake 9f19fb411: not a scanline effect - FillTileSet picks each cell's tile variant with STileTypeDesc::GetMapsIndex (Formats/fmtTerrain.h:87), which draws from the C runtime's rand(); SeedFieldFills never seeded it and the engine time-seeds it at start, so every fill rolled its own variants (57 of 64 cells, every row; the two copies differed too). SeedFieldFills now calls srand() as CreateRandomMap does; TestM3Fields compares the tile values over the whole map exactly and asserts two identical applies save the same bytes | 2026-10-01T20:30:49.716Z | 2026-10-02T13:33:46.360Z |
| 5 | 5 | deviation | Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp | 970 | 05-08: a generated map names its script by the absolute output path (engine's own szScriptFile = output path); Test in game warns 'not a plain name' for a generated map - see deferred-items.md From 05-08 | open |  | 2026-10-02T15:34:14.928Z |  |

````json
[
  {
    "id": 1,
    "kind": "deviation",
    "phase": "03",
    "file": "Sources/editor/app/panels.zig",
    "line": null,
    "description": "plan-5 carried: the status line is never cleared after a later success (Task 4)",
    "status": "fixed",
    "reason": "fixed in 03-16 d421402a8: status messages carry their source; TestLaunchPrompt.noteLaunch clears a Test-in-game failure on a later start or modal failure (tests in test-map-editor-panels); lost-frame and dialog-busy messages also cleared by their own success",
    "recorded_at": "2026-09-28T17:53:06.985Z",
    "resolved_at": "2026-09-29T10:08:34.494Z"
  },
  {
    "id": 2,
    "kind": "unrun-verify",
    "phase": "03",
    "file": "Sources/editor/app/view.zig",
    "line": null,
    "description": "plan-5 carried: no test of view.zig's event-to-tool wiring beyond the routing function (Task 4)",
    "status": "fixed",
    "reason": "fixed in 03-16 294bcdf74: View = ViewWith(SdlInput), bridge as anytype; 19 view.zig tests feed real SDL events through handleEvent/update against fake bridges, incl. D-15 per-map camera restore; run by test-map-editor-view and CI's unit tier",
    "recorded_at": "2026-09-28T17:53:07.093Z",
    "resolved_at": "2026-09-29T10:08:34.609Z"
  },
  {
    "id": 3,
    "kind": "deviation",
    "phase": "03",
    "file": "Sources/editor/app/view_math.zig",
    "line": null,
    "description": "plan-5 carried: the scroll-direction unit test restates its own constants; an engine-tier ScreenToWorld direction check would catch a sign error (Task 6)",
    "status": "fixed",
    "reason": "fixed in 03-16 82a08a462: the direction tests assert literal camera positions (a notch up: 2000,2000 -> 1971.716,2028.284); view.zig's wheel tests do the same through SDL events; the engine tier's TestWorldToScreenRoundTrip already checks screen up is world (-x,+y)",
    "recorded_at": "2026-09-28T17:53:07.199Z",
    "resolved_at": "2026-09-29T10:08:34.712Z"
  },
  {
    "id": 4,
    "kind": "unmet-truth",
    "phase": "05",
    "file": "Sources/src/EditorBridge/session_fields.cpp",
    "line": null,
    "description": "The engine's field fill leaves its final scanline's tile picks unreproducible across seeded replays (recorded in 05-03-SUMMARY deviations; the touched-cell proof and the byte-exact undo proofs carry the contract)",
    "status": "fixed",
    "reason": "fixed on fix/m3-fields-flake 9f19fb411: not a scanline effect - FillTileSet picks each cell's tile variant with STileTypeDesc::GetMapsIndex (Formats/fmtTerrain.h:87), which draws from the C runtime's rand(); SeedFieldFills never seeded it and the engine time-seeds it at start, so every fill rolled its own variants (57 of 64 cells, every row; the two copies differed too). SeedFieldFills now calls srand() as CreateRandomMap does; TestM3Fields compares the tile values over the whole map exactly and asserts two identical applies save the same bytes",
    "recorded_at": "2026-10-01T20:30:49.716Z",
    "resolved_at": "2026-10-02T13:33:46.360Z"
  },
  {
    "id": 5,
    "kind": "deviation",
    "phase": "5",
    "file": "Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp",
    "line": 970,
    "description": "05-08: a generated map names its script by the absolute output path (engine's own szScriptFile = output path); Test in game warns 'not a plain name' for a generated map - see deferred-items.md From 05-08",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-10-02T15:34:14.928Z",
    "resolved_at": null,
    "milestone": null
  }
]
````
