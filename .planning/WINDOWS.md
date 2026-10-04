---
schema_version: 1
open_count: 0
waived_count: 1
fixed_count: 8
total_count: 9
last_updated: 2026-10-03T05:36:16.353Z
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
| 5 | 5 | deviation | Sources/src/RandomMapGen/MapInfo_StaticMethods_RMGeneration.cpp | 970 | 05-08: a generated map names its script by the absolute output path (engine's own szScriptFile = output path); Test in game warns 'not a plain name' for a generated map - see deferred-items.md From 05-08 | fixed | fixed in the quick task 2026-10-03-script-path-wheel-link-pad (the user's ruling of 2026-10-03: saves sync through the cloud to other computers): a map stores its script path RELATIVE to its own folder - in practice the bare name, '/' the only separator it could hold - and a loader expands it beside the map (NMapScriptPath, Formats/fmtMapScriptPath.h). CreateRandomMap stores the name of the map it wrote; the bridge's save writes an ABSOLUTE value (an older generated map, or the shipped intro maps' C:\\a7\\data\\maps\\...) as its last component and leaves every other value exactly as the map had it, so the 66/66 round trip and an unedited save are unchanged; the game's loaders (iMissionInternal, GameCreation, CommandsHistory) expand through the same rule. Proof: rmg-determinism generates one seed into two folders byte identical; map-editor-game-reads-it-m3 moves a generated map and its .lua to another folder (originals deleted), Test in game copies the script and the real Game loads and runs it (script name=m3_authored loaded=1 init=1); test-editor-bridge saves a Windows drive path and a macOS root path to the same bytes; map-file TestScriptPathForms | 2026-10-02T15:34:14.928Z | 2026-10-02T23:54:36.097Z |
| 6 | 5 | unrun-verify | .github/workflows/cross-platform.yml |  | 05-09: test-rmg-composer-roundtrip (and 05-08's test-rmg-determinism) are not in the CI workflow yet, and the composer m3-auto frames have no Windows run - see deferred-items.md From 05-09 | fixed | fixed in 05-10 00c02ae11: test-rmg-composer-roundtrip and test-rmg-determinism run in the Windows and macOS engine jobs of cross-platform.yml (macOS green on run 37072698456); the Windows frames and the Windows run of the two steps are entry 7 | 2026-10-02T19:55:21.443Z | 2026-10-02T23:05:38.337Z |
| 7 | 5 | unrun-verify | .planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-10-SUMMARY.md |  | 05-10: the Fields and Templates composer frames of map-editor-m3-auto (1214-1400) and the Windows run of test-rmg-composer-roundtrip / test-rmg-determinism have no green Windows run yet: the Windows engine job already fails earlier on the RMG nondeterminism fixed on fix/rmg-windows-determinism (run 37072698456); the macOS job ran both steps green | waived | user-waived 2026-10-03: the Windows GUI legs (the composer frames of map-editor-m3-auto and a hand double launch) run only in CI or by hand, not over ssh win-home; the Windows data-only steps (test-rmg-composer-roundtrip, test-rmg-determinism) run in CI's Windows engine job | 2026-10-02T23:05:43.881Z | 2026-10-03T05:36:16.353Z |
| 8 | 05 | unmet-truth | Sources/src/Formats/fmtVSO.h | 13 | 05-08 D-04 on windows-msvc: the same seed did not regenerate a byte-identical map (test-editor-bridge, CI run 37029035801) - the road/river points' 3 pad bytes after bKeyPoint are saved raw and were never written | fixed | fixed on fix/rmg-windows-determinism cabb77da8: SVectorStripeObjectPoint is saved as raw bytes (CSaverAccessor::DoDataVector) and CVSOBuilder::SliceSpline built each point on its stack, so its 3 pad bytes carried stack leftovers into the BZM - run-varying on windows-msvc, repeatable (but still garbage) on macOS. Full diff of two same-seed maps on a Windows host: 1487 bytes, all at offset 33..35 of a 40-byte point in the roads3 points chunk, nothing else (not the stored script path - see #5 - not objects, not altitudes). The pad is now a zero-initialised member cReserved[3]; layout and format unchanged (static_assert 40), 66/66 maps still round-trip. TestVsoPointBytes (test-map-files) fails 4/4 without the fix on windows-msvc and passes with it; test-editor-bridge passes on the Windows host and macOS | 2026-10-02T19:16:41.583Z | 2026-10-02T19:16:41.715Z |
| 9 | 05 | unmet-truth | Sources/src/Formats/fmtMap.h | 20 | SMapObjectInfo::SLinkInfo has 3 uninitialised pad bytes after bIntention and a map saves the link as raw bytes (SMapObjectInfo::operator&( IStructureSaver & ) adds link; SLinkInfo has no IStructureSaver operator&) - the same bug class as window 8 (SVectorStripeObjectPoint): the bytes took whatever memory held where an object was built into the BZM | fixed | fixed in the quick task 2026-10-03-script-path-wheel-link-pad, the pattern of window 8 (fmtVSO.h cReserved[3]): the three bytes after bIntention are now a named member BYTE cReserved[3], zero-initialised in the constructor, with a static_assert that SLinkInfo stays 12 bytes (layout and file format unchanged; a link read from a map keeps the bytes the map had, 66 of 66 maps still round-trip byte for byte). TestLinkInfoBytes (test-map-files) builds a link over 0x00 and 0xff memory, copies one into 0xa5 memory, and builds the same object after two different stack fills and saves it into two maps: all five checks fail on macOS without the fix and pass with it | 2026-10-03T00:21:46.136Z | 2026-10-03T00:21:50.033Z |

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
    "status": "fixed",
    "reason": "fixed in the quick task 2026-10-03-script-path-wheel-link-pad (the user's ruling of 2026-10-03: saves sync through the cloud to other computers): a map stores its script path RELATIVE to its own folder - in practice the bare name, '/' the only separator it could hold - and a loader expands it beside the map (NMapScriptPath, Formats/fmtMapScriptPath.h). CreateRandomMap stores the name of the map it wrote; the bridge's save writes an ABSOLUTE value (an older generated map, or the shipped intro maps' C:\\a7\\data\\maps\\...) as its last component and leaves every other value exactly as the map had it, so the 66/66 round trip and an unedited save are unchanged; the game's loaders (iMissionInternal, GameCreation, CommandsHistory) expand through the same rule. Proof: rmg-determinism generates one seed into two folders byte identical; map-editor-game-reads-it-m3 moves a generated map and its .lua to another folder (originals deleted), Test in game copies the script and the real Game loads and runs it (script name=m3_authored loaded=1 init=1); test-editor-bridge saves a Windows drive path and a macOS root path to the same bytes; map-file TestScriptPathForms",
    "recorded_at": "2026-10-02T15:34:14.928Z",
    "resolved_at": "2026-10-02T23:54:36.097Z",
    "milestone": null
  },
  {
    "id": 6,
    "kind": "unrun-verify",
    "phase": "5",
    "file": ".github/workflows/cross-platform.yml",
    "line": null,
    "description": "05-09: test-rmg-composer-roundtrip (and 05-08's test-rmg-determinism) are not in the CI workflow yet, and the composer m3-auto frames have no Windows run - see deferred-items.md From 05-09",
    "status": "fixed",
    "reason": "fixed in 05-10 00c02ae11: test-rmg-composer-roundtrip and test-rmg-determinism run in the Windows and macOS engine jobs of cross-platform.yml (macOS green on run 37072698456); the Windows frames and the Windows run of the two steps are entry 7",
    "recorded_at": "2026-10-02T19:55:21.443Z",
    "resolved_at": "2026-10-02T23:05:38.337Z",
    "milestone": null
  },
  {
    "id": 7,
    "kind": "unrun-verify",
    "phase": "5",
    "file": ".planning/phases/05-map-editor-m3-random-map-templates-minimap-tools-parity/05-10-SUMMARY.md",
    "line": null,
    "description": "05-10: the Fields and Templates composer frames of map-editor-m3-auto (1214-1400) and the Windows run of test-rmg-composer-roundtrip / test-rmg-determinism have no green Windows run yet: the Windows engine job already fails earlier on the RMG nondeterminism fixed on fix/rmg-windows-determinism (run 37072698456); the macOS job ran both steps green",
    "status": "waived",
    "reason": "user-waived 2026-10-03: the Windows GUI legs (the composer frames of map-editor-m3-auto and a hand double launch) run only in CI or by hand, not over ssh win-home; the Windows data-only steps (test-rmg-composer-roundtrip, test-rmg-determinism) run in CI's Windows engine job",
    "recorded_at": "2026-10-02T23:05:43.881Z",
    "resolved_at": "2026-10-03T05:36:16.353Z",
    "milestone": null
  },
  {
    "id": 8,
    "kind": "unmet-truth",
    "phase": "05",
    "file": "Sources/src/Formats/fmtVSO.h",
    "line": 13,
    "description": "05-08 D-04 on windows-msvc: the same seed did not regenerate a byte-identical map (test-editor-bridge, CI run 37029035801) - the road/river points' 3 pad bytes after bKeyPoint are saved raw and were never written",
    "status": "fixed",
    "reason": "fixed on fix/rmg-windows-determinism cabb77da8: SVectorStripeObjectPoint is saved as raw bytes (CSaverAccessor::DoDataVector) and CVSOBuilder::SliceSpline built each point on its stack, so its 3 pad bytes carried stack leftovers into the BZM - run-varying on windows-msvc, repeatable (but still garbage) on macOS. Full diff of two same-seed maps on a Windows host: 1487 bytes, all at offset 33..35 of a 40-byte point in the roads3 points chunk, nothing else (not the stored script path - see #5 - not objects, not altitudes). The pad is now a zero-initialised member cReserved[3]; layout and format unchanged (static_assert 40), 66/66 maps still round-trip. TestVsoPointBytes (test-map-files) fails 4/4 without the fix on windows-msvc and passes with it; test-editor-bridge passes on the Windows host and macOS",
    "recorded_at": "2026-10-02T19:16:41.583Z",
    "resolved_at": "2026-10-02T19:16:41.715Z",
    "milestone": null
  },
  {
    "id": 9,
    "kind": "unmet-truth",
    "phase": "05",
    "file": "Sources/src/Formats/fmtMap.h",
    "line": 20,
    "description": "SMapObjectInfo::SLinkInfo has 3 uninitialised pad bytes after bIntention and a map saves the link as raw bytes (SMapObjectInfo::operator&( IStructureSaver & ) adds link; SLinkInfo has no IStructureSaver operator&) - the same bug class as window 8 (SVectorStripeObjectPoint): the bytes took whatever memory held where an object was built into the BZM",
    "status": "fixed",
    "reason": "fixed in the quick task 2026-10-03-script-path-wheel-link-pad, the pattern of window 8 (fmtVSO.h cReserved[3]): the three bytes after bIntention are now a named member BYTE cReserved[3], zero-initialised in the constructor, with a static_assert that SLinkInfo stays 12 bytes (layout and file format unchanged; a link read from a map keeps the bytes the map had, 66 of 66 maps still round-trip byte for byte). TestLinkInfoBytes (test-map-files) builds a link over 0x00 and 0xff memory, copies one into 0xa5 memory, and builds the same object after two different stack fills and saves it into two maps: all five checks fail on macOS without the fix and pass with it",
    "recorded_at": "2026-10-03T00:21:46.136Z",
    "resolved_at": "2026-10-03T00:21:50.033Z",
    "milestone": null
  }
]
````
