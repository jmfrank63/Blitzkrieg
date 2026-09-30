---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
reviewed: 2026-09-30T00:00:00Z
depth: standard
files_reviewed: 54
diff_base: a1a9600dd
findings:
  critical: 3
  warning: 27
  info: 20
  total: 50
status: issues_found
---

# Phase 4 code review (merged from three parallel slices)

Slice A: C++ engine side (Sources/src). Slice B: Zig editor core. Slice C: Zig app, build, CI, tests.
Finding IDs are prefixed with their slice (e.g. CR-A01). Note CR-B01 and CR-C01 are the same defect (script copy-along overwrites an existing .lua).


# Slice A


# Phase 04 (review lane A): Code Review Report

**Reviewed:** 2026-09-30
**Depth:** standard (diff `a1a9600dd..HEAD`)
**Files Reviewed:** 20
**Status:** issues_found

## Summary

I reviewed the EditorBridge M2 surface: the C ABI in `bridge.cpp`/`bridge.h`, the session files (records, roads and rivers, bridge/fence/trench groups), the map-file overlay (`MapRecords`, `MapOverlay` cascade, `MapGeometry`), the `RemoveRoad` return fix and the game-side `BK_MAP_TRACE` seam.

Most of it holds up:
- **ABI buffers.** Every two-pass fill I checked writes only up to `min(capacity, total)`. Null arrays are only accepted with a count of 0. The parcel point ranges are checked without overflow.
- **Record puts.** They go into both copies with a rollback on the second copy.
- **Cascade restore.** `RestoreObject` undoes the cascade in the reverse of the order it was applied. `RemoveGroup` and `AddGroup` mirror each other in that order.
- **Engine links.** `ReleaseLink` runs before `DeleteObject` in every removal path.
- **Trace seam.** `BK_MAP_TRACE` reads the environment once through a function-local static and prints nothing when the variable is unset.
- **Windows.** No `std::min`/`std::max`, `near`/`far`/`small` hazards were found in the changed files.

**Main problems:**
1. Road and river edits that resample can read out of bounds on a record with fewer than two control points (CR-A01).
2. The group edit log has no rollback on `Revert`, and `RemoveGroup` touches the engine before the map has decided. On the failure paths this loses a bridge or leaves the map and the engine out of step (WR-A01, WR-A02).
3. The AI-general put can silently drop sides nobody asked to change (WR-A03).
4. Script areas lack the "file's own data" undo exemption that the other record kinds have (WR-A04).
5. The script-ID edit bypasses the unknown-type preservation invariant (WR-A05).
6. The trace seam reports `init=1` even when the Lua `Init` failed (WR-A06).

## Critical Issues

### CR-A01: Resampling a road or river whose record has fewer than 2 control points reads out of bounds (asserts compiled out)

**File:** `Sources/src/EditorBridge/session_vso.cpp:483-487, 530-561, 568-617`; `Sources/src/EditorBridge/bridge.cpp:2798-2822`
**Issue:**
- `MoveVsoPointsInSession` and `SetVsoWidthInSession` call `ResampleKeepingKeys`, which calls `CVSOBuilder::Update` and then `SampleCurve`.
- `SampleCurve` reads `rControlPoints[1]` and `rControlPoints[size - 2]` behind only an `NI_ASSERT_T` (`VSO_StaticMethods.cpp:89-102`). With 0 control points it also loops to `plots.size() - 3 == SIZE_MAX`.
- `ReplaceVsoInSession` checks `LongEnough` only after `modify()` has already resampled. So a map-file record with 0 or 1 control points crashes the editor on the first move or width edit.
- The ABI lets this through: `BkEditorMoveVsoPoints` accepts `nCount == 0` whenever the record's `controlpoints.size()` is 0.
- `SetVsoWidth` only needs `KeyCount > nKey`, which counts sampled `points`, not control points.
- The game uses only `points`, so such a record loads fine in the game and in the editor. It only kills the editor when someone edits it.
- `InsertVsoPointInSession` and `DeleteVsoPointInSession` already guard `nCount < 2` / `nCount <= 2`. Move and width do not.

**Fix:** Refuse the edit before `modify()` runs, and keep such a record as read:
```cpp
// ReplaceVsoInSession, after the SessionVso lookup:
if ( pVso->controlpoints.size() < 2 || pVso->points.size() < 2 )
{
	pSession->szMessage = NStr::Format( "%s %d has fewer than 2 control points; it is kept as read", KindName( nKind ), nIndex );
	*pbRefused = true;
	return false;
}
```

## Warnings

### WR-A01: `SGroupEdit::Revert` has no rollback: a failed undo of a rotate loses the bridge, and the log gets stuck

**File:** `Sources/src/EditorBridge/session_groups.cpp:311-319` (with `UndoEditInSession`, `session_vso.cpp:43-59`)
**Issue:**
- `Apply` puts the old group back when the new one will not go in. `Revert` does not do the reverse.
- For a rotate, `Revert` runs `RemoveGroup(newGroup)` and then `AddGroup(oldGroup)`. If the second call fails (the engine will not place a span, a link ID is in use, or the duplicate-link case in WR-A02), the rotated bridge is gone and the original is not back.
- `UndoEditInSession` leaves the token in `appliedEdits`, so a retry fails in `RemoveGroup` on the `SameEntry` check ("bridge N is not the one the edit log holds").
- Redo is unreachable. The bridge is lost from the map with no way to recover it short of reopening without saving.
- An undo of `DeleteBridge` fails the same way, but the map is at least self-consistent there.

**Fix:** Mirror `Apply`: when `AddGroup(oldGroup)` fails after `RemoveGroup(newGroup)` succeeded, call `AddGroup(newGroup, ...)` to put the new group back. Only report "reopen the map" if that also fails.
```cpp
virtual bool Revert( SEditorSession *pSession )
{
	bool bRefused = false;
	if ( bNew && !RemoveGroup( pSession, &newGroup ) )
		return false;
	if ( bOld && !AddGroup( pSession, oldGroup, &bRefused ) )
	{
		const std::string szWhy = pSession->szMessage;
		bool bIgnored = false;
		if ( bNew && !AddGroup( pSession, newGroup, &bIgnored ) )
			pSession->szMessage = szWhy + "; and the edit could not be put back: reopen the map";
		UpdateSessionWorld( pSession );
		return false;
	}
	UpdateSessionWorld( pSession );
	return true;
}
```

### WR-A02: Group removal touches the engine before the map decides; `CanTakeOutWhole` misses shared or duplicated link IDs

**File:** `Sources/src/EditorBridge/session_groups.cpp:162-187, 593-643`; `Sources/src/EditorBridge/session.cpp:175-218` (`BuildOneBridge`)
**Issue:**
- `RemoveGroup` erases the entry and then, for each span, calls `RemoveFromEngine` (ReleaseLink plus DeleteObject in the AI) before `NMapOverlay::DeleteObject` gets a say.
- `DeleteObject` still refuses a link ID that another `bridges` or `entrenchments` entry names (`WhyRefused`). `CanTakeOutWhole` never checks that the group's link IDs are exclusive to the entry.
- So on a map where two entries share a span, deleting either one erases its entry, destroys the engine object and then refuses in the map. The session is left half-removed: the engine object is gone but the record is still there, and `Apply` returns "reopen the map" without restoring anything.
- A link ID listed twice in one entry (or in two sections of one trench) passes `CanTakeOutWhole` (`nHolders == 1`) and deletes fine. On undo, `AddGroup` then gets `spans.size() == 1` while `BuildOneBridge` places the span twice. It overwrites `byLinkID` and orphans the first engine object (drawn, never deleted), and `nPlaced != spans.size()` fails the undo. Open-time `BuildBridges` has the same double placement.

**Fix:**
- In `CanTakeOutWhole`, refuse a group whose link IDs repeat, or that any other `bridges` or `entrenchments` entry names.
- In `RemoveGroup`, run `NMapOverlay::DeleteObject` (or a dry-run `WhyRefused`) for every span before calling `RemoveFromEngine`.
- In `BuildOneBridge`, skip a link ID already placed in this call.

### WR-A03: `SetSessionAIGeneralSide` silently drops non-empty sides when a put shrinks the side count

**File:** `Sources/src/EditorBridge/session_records.cpp:1433-1464`; `Sources/src/MapFile/MapRecords.cpp:239-250`; `Sources/src/EditorBridge/bridge.cpp:2450-2482`
**Issue:**
- `PutAIGeneralSide` does `rSides.resize( nSideCount )` for any count from 0 to 1024. The only check is that side `nSide` is empty when it lies at or above the count.
- A call such as `BkEditorSetAIGeneralSide(s, 0, 0, …)`, or any count below the current size, deletes every side above the count, parcels and mobile IDs included. The validation looks only at side `nSide`.
- The failure rollback (`before`) restores only side `nSide`.
- Undo needs to shrink, but only over sides that the put being undone created, and those are empty.
- The bridge is the last line of defence for an ABI that the app, `BK_EDITOR_AUTO` and tests all drive. A shrink over non-empty sides should be refused, not trusted to the caller.

**Fix:** Refuse (`BK_EDITOR_REFUSED`) a put where `nSideCount < current size` and any side in `[nSideCount, size)` other than `nSide` is non-empty. Undo of a side creation still works, because those sides are empty.

### WR-A04: Script areas have no "file's own data" exemption, so undoing an edit or delete of an odd file area fails

**File:** `Sources/src/EditorBridge/session_records.cpp:253-286, 319-359`
**Issue:**
- `AreaPutAllowed` exempts only duplicate names held at open.
- A file area whose centre is off the map (or on the far edge, since `x == width` is refused), or which has a negative size, can be deleted or moved. Its undo is then refused:
  - The re-add passes `pCurrent == 0`, so `bMoved` is true and the centre check runs.
  - The set-back compares against the moved value, so `bMoved` is true.
- The core's history keeps the delete as applied. The area is lost unless the user reopens the map.
- Start commands, reserve positions, AI sides and groups all carry an `opened*` exemption for exactly this case. Areas were missed.

**Fix:** Record `openedAreas` (the full records) at open. In `AreaPutAllowed`, return true when `rWanted` equals, bit for bit, an area the file held, as `IsOpenedStartCommand` does.

### WR-A05: Setting a script ID edits objects of unknown type (breaks the preservation invariant)

**File:** `Sources/src/EditorBridge/session.cpp:566-605`
**Issue:**
- `PlaceObjectInSession` and `DeleteObjectFromSession` refuse any link in `unknownLinkIDs`: "an object the database does not know is written back exactly as it was read, so no edit may reach it."
- `SetSessionObjectScriptID` has no such check. It rewrites `nScriptID` of an unknown-type record in both copies, and the map is then saved with the change.

**Fix:**
```cpp
if ( std::find( pSession->unknownLinkIDs.begin(), pSession->unknownLinkIDs.end(), nLinkID ) != pSession->unknownLinkIDs.end() )
{
	pSession->szMessage = "the object database does not know this object's type; it is kept as it is";
	if ( pbRefused != 0 ) *pbRefused = true;
	return false;
}
```

### WR-A06: `BK_MAP_TRACE` script line reports `init=1` whenever the file loaded, even if `Init` failed or does not exist

**File:** `Sources/src/AILogic/Scripts/Scripts.cpp:142-157`
**Issue:**
- `init=%d` is fed from `bLoaded` a second time. The return value of `script.Call( 0, 0 )` (`lua_call`, which returns an error code, `LuaLib/Script.h:273`) is thrown away.
- Pitfall 12 says Lua failures are otherwise silent, and this seam exists so the game-reads-it tier can prove the script ran. A map whose `Init` errors, or has no `Init`, still prints `init=1`, so the scenario passes when it should fail.

**Fix:**
```cpp
const bool bLoaded = ReadScriptFile();
int nInitError = -1;
if ( bLoaded )
{
	script.GetGlobal( "Init" );
	nInitError = script.Call( 0, 0 );
}
...
fprintf( stderr, "BK_MAP_TRACE: script name=\"%s\" loaded=%d init=%d\n", szBaseName.c_str(), bLoaded ? 1 : 0, nInitError == 0 ? 1 : 0 );
```

### WR-A07: The bridge build toggle applies partially with no rollback and ignores shared link IDs

**File:** `Sources/src/EditorBridge/session_groups.cpp:707-724, 779-811`
**Issue:**
- `SBridgeBuildEdit::Put` sets HP span by span. If `SetObjectHP` fails part-way (for example link ID 0 in the entry, where `SetObjectHP` refuses 0), the earlier spans stay flipped in the snapshot, `Put` returns false and nothing is logged. The saved map then differs from what the editor shows and there is no undo.
- `ToggleBridgeBuildInSession` does not call `CanTakeOutWhole` or `RefuseSharedLinkID`. `SetObjectHP` edits the first record holding the link, which may not be the span `EntrySpans` found.

**Fix:**
- Validate every link first: non-zero, exactly one holder, present in the snapshot. Only then write.
- Alternatively, on failure write `before` back for the spans already changed.
- Reuse `CanTakeOutWhole` (or at least the holder-count part) in the toggle.

### WR-A08: Bridge stats checked only for the first begin and first line; other indices reach the engine unchecked

**File:** `Sources/src/EditorBridge/session_groups.cpp:418-436, 356-361`
**Issue:**
- `BridgePlanInputFor` range-checks `nBegin` (seed 0) and `lines[0]` against `spans`, and `spans[nBegin].nSlab` against `segments`.
- `NewGroupFromPlan` then gives middle spans a seed of `i` and the end span a seed of 0. `GetIndexFromType` returns `lines[i % n]` and `ends[0]`, which are never range-checked.
- A mod descriptor with a bad `lines[k]` or `ends[0]` puts an out-of-range sprite index into the working copy, and the engine indexes `spans` or `segments` with it (asserts compiled out). Trenches (`SegmentIndexOk` on every list) and fences (`FenceCentreIndex`) are already checked this way; bridges are not.

**Fix:** Check every entry of `begins`, `lines` and `ends` is `< spans.size()`, and each span's `nSlab` is `< segments.size()`, the way `SegmentIndexOk` does for trenches.

### WR-A09: Float-to-int conversions of unbounded ABI coordinates are undefined behaviour

**File:** `Sources/src/MapFile/MapGeometry.cpp:64-70, 609-612`; `Sources/src/EditorBridge/bridge.cpp:2136-2146`
**Issue:**
- `PlanBridge` computes `int( fRun / fL )` and only then range-checks `nParts`. `VisLengthToAI` computes `int( fVis * fAITileXCoeff1 + 0.3f )`.
- The ABI checks only `std::isfinite` on drag and area coordinates, so a value like 1e30 converts out of int range, which is undefined.
- In practice the result differs by platform: x86 gives `INT_MIN`, so the check refuses. ARM64 saturates to `INT_MAX`, so a huge half-size or radius from `BkEditorScriptAreaFromVis` or `…Resized` comes back as 2147483647 and passes `AreaPutAllowed`, which checks only the centre.

**Fix:** Clamp or refuse before converting, for example `if ( !( fRun / fL <= 4096.0f ) ) refuse;` and `if ( std::fabs( fVis ) > 1.0e6f ) refuse`. Bound drag coordinates to the map at the ABI, as `TrenchPath` already does with `fMaxTrenchCoordinate`.

### WR-A10: Assigning a group script ID does not warn when the object is named by a start command or reserve position

**File:** `Sources/src/EditorBridge/session_records.cpp:502-522` (`SetSessionGroup`), `session.cpp:566-605` (`SetSessionObjectScriptID`)
**Issue:**
- Research Pitfall 8 asks for a status note when a group's script ID covers an object that a start command or reserve position names.
- `LoadUnits` holds such an object back, so the command or position silently does nothing in the mission.
- `HeldUnitWarning` exists but runs only when a start command is added or set. The two edits that actually create the situation (adding the ID to a group, or giving the object the group's script ID) say nothing. Reserve positions are never checked at all.

**Fix:** After a successful `SetSessionGroup` or `SetSessionObjectScriptID`, scan `startCommandsList` and `reservePositionsList` for objects in `snapshot.objects` whose script ID a group holds. Put the note in `szMessage`, as `AddStartCommandToSession` does.

## Info

### IN-A01: `IsMapTraceOn` is copied into four translation units
**File:** `AILogicInternal.cpp:465`, `GeneralInternal.cpp:451`, `Scripts/Scripts.cpp:108`, `GameTT/iMissionInternal.cpp:854`
**Issue:** The same getenv helper exists four times with the same comment. It is fine as is (zero cost, silent when unset), but a rename or a second switch has to be changed in four places.
**Fix:** Put one `bool IsMapTraceOn()` in a shared header or in Misc.

### IN-A02: Trace lines can break the one-line `key=value` format
**File:** `Scripts/Scripts.cpp:117-120, 1782-1783`
**Issue:** Area names are printed inside `"%s"` without escaping, and Lua `Trace` text is printed raw. A `"` in a name, or a newline in a traced message, splits or corrupts the line the game-reads-it parser reads.
**Fix:** Escape `"` and `\n` (or print a length prefix) before `fprintf`.

### IN-A03: The trace undercounts held-back units and overcounts launched start commands
**File:** `AILogic/AILogicInternal.cpp:480-510, 692-709`
**Issue:**
- `group … held=` counts only `mapInfo.objects`. Scenario objects held back at line ~872 are not counted.
- `startcmd launched=` counts every command with a non-empty unit list, even when every unit resolved to null because it was held back. `RegisterGroup` skips the nulls, so nothing is actually launched.
**Fix:** Count scenario holds too. Count a command only when at least one `unitsBuffer[i]` is non-null.

### IN-A04: ID "next" helpers overflow on `INT_MAX`
**File:** `MapFile/MapRecords.cpp:191-197, 270-278`
**Issue:** `NextVsoID` returns `nMax + 1`, and `FirstFreeGroupID` does `++nID` with no upper bound. A malformed file holding `INT_MAX` causes signed overflow (undefined behaviour).
**Fix:** Refuse, or cap, when `nMax == INT_MAX`.

### IN-A05: Group deletes run the object cascade silently
**File:** `EditorBridge/session_groups.cpp:174-187`
**Issue:** Deleting a bridge, trench or fence run goes through `NMapOverlay::DeleteObject`, so any start-command target naming a span or fence is set to 0 and any reserve position naming it is erased. Undo restores them correctly, but unlike `DeleteObjectFromSession` the status bar never says so (`DescribeCascade` is not called).
**Fix:** Merge the per-span cascades and put `DescribeCascade` output in `szMessage` after a successful `Apply`.

### IN-A06: An edit whose `LogEdit` throws stays applied with no token
**File:** `EditorBridge/session_vso.cpp:310-317`, `session_groups.cpp:370-377`
**Issue:** The edit is applied first and logged afterwards. A `bad_alloc` in `edits.push_back` or `appliedEdits.push_back` is caught by `Guarded` as `BK_EDITOR_FAILED`. The map has changed, but the core got no token and cannot undo it.
**Fix:** Reserve `edits` and `appliedEdits` capacity before applying, so `LogEdit` cannot throw after the change.

---

_Reviewed: 2026-09-30_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_


# Slice B


# Phase 4: Code Review Report (core, slice B)

**Reviewed:** 2026-09-30
**Depth:** standard (diff a1a9600dd..HEAD, `Sources/editor/core`; callers in `Sources/editor/app` and the C++ bridge were read only where a finding depends on them)
**Files Reviewed:** 12
**Status:** issues_found

## Summary

The history mechanics are mostly sound. The reserve-before-bridge-call pattern is applied consistently, and the owning `Value` and `Command` types free correctly. I traced `editRecord`, `addRecord`, `deleteRecord`, `prepareEdit` and `commitEdit` for leaks, double frees and use-after-free, and found none. `History.touchTop` and `dropTop` leave a dangling `clean_depth`, but the next `record*` call repairs it, so it is not a bug.

What is wrong falls into five groups:

- **Save As script copy.** One real data-loss path: `copyAlong` overwrites silently.
- **Undo after the bridge has already acted.** The replay of a bridge-logged edit can fail after the bridge has acted. That permanently wedges the undo stack, because the bridge refuses the retry.
- **Stale selections.** Every new tool keeps a bare list index as its selection and never revalidates it after undo or redo.
- **Script file helpers.** Shell-opening of a map-controlled `.lua`, and no defence-in-depth shipped-folder guard.
- **Fake bridge.** It diverges from the real bridge in ways the tests cannot see.

## Critical Issues

### CR-B01: `copyAlong` silently overwrites a different script already beside the new map

**File:** `Sources/editor/core/script_file.zig:225-235` (caller `Sources/editor/app/panels.zig:1402`)
**Issue:** `copyInto` protects an existing file: it returns `.exists` unless `overwrite` is set, and the caller asks first. `copyAlong`, used by the Save As copy-along, does no such check.

- After `sameFile` is false, it calls `files.copy(from, to)` straight away, and the copy overwrites.
- The question put to the user (`offerScriptCopyAlong`) only asks whether to copy the script beside the new map. It never says a file is already there.
- Save As into a folder that already holds a `<name>.lua` with different content destroys that file. For example, Save As of a shipped map such as `coldwinter` into a folder holding the user's own `coldwinter.lua`.
- The `exists` check the author wrote for `copyInto` shows the risk was known.

**Fix:** Give `copyAlong` the same contract as `copyInto`.

```zig
pub fn copyAlong(files: Files, from_map: []const u8, to_map: []const u8, value: []const u8, overwrite: bool) CopyIntoOutcome {
    // ... build `from` / `to` as now ...
    if (!files.exists(from)) return .missing;
    if (sameFile(files, from, to)) return .copied;
    if (files.exists(to) and !overwrite) return .exists;
    files.copy(from, to) catch return .failed;
    return .copied;
}
```

`offerScriptCopyAlong` should then raise a "Replace it?" question, as `pickScript` does.

## Warnings

### WR-B01: A failed replay of a bridge-logged edit wedges the undo stack, and `reloadObjects` can make that happen after a fully successful bridge replay

**File:** `Sources/editor/core/editor.zig:1606-1625` (replay `.edit`), `1652-1677` (`undo` / `redo`), `792-806` (`reloadObjects`)
**Issue:** `undo()` and `redo()` leave the entry in place when `replay` errors. For a multi-token `.edit` entry, the bridge's own log has already moved by the time the error surfaces. The bridge refuses any token out of order, so the retry can never succeed.

- **Partial replay:** if token k of n fails, tokens n-1..k+1 are already undone or redone. A retry starts at the newest token again and is refused ("edits are undone newest first"). Every later Ctrl+Z then fails on the same entry, and older entries are unreachable.
- **Reload failure:** for scope `.objects` the only remaining fallible step is `try self.reloadObjects()` (OOM, or a bridge `.failed`). It runs after every token has been replayed successfully. The error is returned, the entry stays on the wrong stack, and the next undo hits the same wedge.
- **Forward path:** `drawBridge`, `deleteBridge`, `rotateBridge`, `drawFences`, `drawEntrenchment` and `deleteEntrenchment` call `commitEdit` and then `try self.reloadObjects()`. A reload failure returns an error after the edit is committed and recorded. The document is short of the new spans, and the tool's `self.selected = try editor.drawBridge(...)` never runs.

**Fix:**
- In `undo` and `redo`, treat `reloadObjects` as a separate, post-commit step. Move the entry between the stacks once the bridge replay has finished, then reload. On a reload failure, set the status and mark the document as needing a reopen, but do not leave the history inconsistent.
- Pre-reserve what `reloadObjects` needs before replaying: size the list first, then do the bridge call.
- Consider a history state such as "poisoned" that disables further undo with a clear message, instead of letting every Ctrl+Z fail identically.

### WR-B02: Tools keep a bare list index as their selection and never revalidate it after undo or redo

**File:** `tools_groups.zig:50` (`BridgeTool.selected`), `:258` (`EntrenchmentTool.selected`), `tools_vso.zig:77` (`RoadsRivers.selected`), `tools_ai.zig:72` (`ScriptAreas.selected`), `:343` (`ReservePositions.selected`), `:568-570` (`AIGeneral.selected_parcel` / `selected_point`)
**Issue:** The selection is a position in a list the bridge owns. Undo and redo of a delete, add or insert shift those positions.

- **Example:** select bridge 1, then Ctrl+Z an earlier "delete bridge 0". Bridge 0 returns and the old bridge 1 is now index 2. `selected` still says 1, so Q (rotate), Enter (toggle build) or Delete now act on a different bridge.
- **Existing mitigation:** the app's `refresh*` functions and `ScriptAreas.selectedArea` only drop a selection that is past the end. Only the past-the-end case is covered, and `BridgeTool.selected` is not validated at all.
- **Consequence:** this is wrong-target mutation, not merely a stale highlight. It is undoable, but the user does not see it.
- **Related:** `RoadsRivers.selectedView` has the same flaw. After undoing a delete the index is valid but names a different road.

**Fix:** Store identity alongside the index. A road has `saved_id` (in `VsoView`), a bridge has its first span's link ID, and an area has its name. Re-resolve it whenever the matching generation counter (`vso_generation`, `bridges_generation`, `record_generations`) has moved. Or clear the tool's selection on every `undo` and `redo` of a command whose kind the tool edits.

### WR-B03: Undo of a camera-anchor edit is refused by the real bridge when the file's original anchor is off the map

**File:** `Sources/src/EditorBridge/session_records.cpp:126-141` (and the fake copy of the rule, `fake_bridge.zig:2471-2482`). Affects `editor.zig:160-197` (`editRecord`) and `:1593-1597` (replay `.record_edit`).
**Issue:** `SetSessionCameraAnchors` only exempts a slot equal to the current slot. The file's own off-map anchor is not exempt, unlike groups, script areas, start commands, reserve positions and script files, which all keep an at-open exemption.

- A legacy map whose anchor lies off the map can be edited: the user sets the slot to an on-map point.
- Undo then puts the file's off-map value back. The bridge answers `refused`.
- `replay` maps that to `error.Failed`, the entry stays, and WR-B01's wedge applies.
- The fake copies the same rule, so the core's "exact put" undo tests cannot catch it.

**Fix:** Exempt the at-open anchors in `SetSessionCameraAnchors`, as `GroupPutAllowed` does with `openedGroups`. Add a fake and test case that seeds an off-map anchor, edits it and undoes.

### WR-B04: "Open script" hands a map-controlled `.lua` file to the shell's default verb

**File:** `Sources/editor/core/script_file.zig:248-276` (`openUrl`); consumer `panels_m2.zig` `openUrlWithSystem` (`SDL_OpenURL`).
**Issue:** The URL is well built: validated name, percent-encoded, resolved folder. The risk is in the action, not the path.

- `SDL_OpenURL("file:///…/x.lua")` goes to the system's default "open" action, which is ShellExecute "open" on Windows.
- The `.lua` file sits beside a map that may have been downloaded. Its name comes from the map (`szScriptFile`) and its content is not the user's.
- Where `.lua` is associated with an interpreter (for example Lua for Windows), "Open script" runs the file. On other setups it may prompt or open an unexpected app.
- The code comment and the header both describe this as opening "with the default editor", which is an assumption rather than something the code enforces.
- I did not run this on Windows. The `ShellExecute` "open" behaviour is from SDL's implementation.

**Fix:** Do not use the "open" verb for a file that came with the map. Options:
- Reveal the containing folder instead.
- Use the `edit` verb on Windows, or `open -t` on macOS.
- Show a confirmation naming the file before opening.

### WR-B05: No shipped-folder guard in the script file writers (defence in depth)

**File:** `script_file.zig:209` (`copyInto`), `:225` (`copyAlong`), `:136` (`copyForTest`)
**Issue:** Every destination is "a fixed directory plus a validated name", but the directory itself is never checked against `shipped.isShipped`.

- `Editor.save` applies that guard "whichever caller asked", and its comment says so explicitly.
- `copyInto` writes beside the open map. Today its only caller (`panels.pickScript`) checks `documentIsShipped` first, and `commands.scriptChoose` goes through it.
- The core function would happily write into the game's read-only `Data` folder if the UI guard were bypassed or forgotten.
- This is the inconsistency with `Editor.save` that the brief asked about.

**Fix:** Pass `base_root` into the core function and refuse with a new outcome.

```zig
pub fn copyInto(files: Files, base_root: []const u8, map_path: []const u8, picked_path: []const u8, overwrite: bool) CopyIntoOutcome {
    if (shipped_mod.isShipped(map_path, base_root, files)) return .shipped;
    // ...
}
```

Do the same for the `to_map` of `copyAlong`.

### WR-B06: Fake bridge diverges from the real bridge in ways the tests cannot see

**File:** `fake_bridge.zig:920-933, 187/233/1437, 2399-2408`, `files.zig` `FakeFiles.listImpl`
**Issue:** Four differences that let real-bridge defects pass the fake-backed tests.

1. `readVso`: the real bridge returns the descriptor's full saved name, including the season folder (`bridge.h:1154-1157`, `c_bridge.zig:959`). The fake returns the bare name. `VsoView.desc` is documented as the full saved name, so code comparing it to `vsoDescriptors` names passes against the fake and fails in the game.
2. Capacity: the fake caps a road at 32 control points, a bridge or fence run at 32 spans, and a trench at 31 points (`max_vso_points`, `max_bridge_spans`). The tools allow 256 points and the real ABI 1024. The tools' "256 points" paths, and any long drag, cannot be exercised through the fake.
3. `insertRecord(.group)` checks `groupPutAllowed(&.{}, …)` with no at-open exemption, whereas the real `GroupPutAllowed` exempts `openedGroups`. So "delete a group holding a file-odd ID, then undo" is refused in the fake (correct per the fake) but accepted by the real bridge. The comment at `:2295` acknowledges it.
4. `FakeFiles.list` for directory `"."` matches nothing, because keys carry no separator. `StdFiles` lists the cwd. `listBeside` uses `"."` for a map path with no folder, so the no-folder branch is untested.

**Fix:** Make the fake return the `Roads3D\name` form in `readVso`, and keep bare names only for `vsoDescriptors`. Lift the span and point caps to the ABI's (heap-backed lists instead of fixed arrays), or at least assert the tool limits against them. Add the at-open exemption to the fake's group insert. Give `FakeFiles.listImpl` a `"."` branch.

### WR-B07: A double click with a road selected, and nothing being drawn, deselects it

**File:** `Sources/editor/core/tools_vso.zig:229-231`
**Issue:** `view_math.kindOf` delivers the second click of a double click as `.double_click` instead of a press. The first click of the pair already ran `press`, which selected the line or grabbed a handle.

- With `!adding()` the `double_click` handler then calls `deselect()`.
- Result: double-clicking a road to select it, or quick successive clicks on a control point, drops the selection, the hover and `last_grab` that Insert and Delete rely on.
- The MFC editor's double click is only "finish the line". Nothing in the spec says it should clear the selection, and no test covers the not-adding case. Enter and Space share this branch.

**Fix:** Make the not-adding branch a no-op for `.double_click`. Keep the deselect only for Enter and Space if that is intended.

### WR-B08: `AIGeneral.deleteSelected` escalates to deleting the whole parcel when the selected point index is stale

**File:** `Sources/editor/core/tools_ai.zig:752-761`
**Issue:** If `selected_point` is set but `point_index >= parcels[index].points.len` (for example after an undo that removed that point), the code clears `selected_point` and falls through to `removeParcel`. A Delete intended for one point removes the parcel and every reinforce point it holds. It is undoable, but unexpected.

**Fix:** When the point index is stale, reset the selection and return with a note, as the `index >= side.parcels.len` branch does.

```zig
if (point_index >= side.parcels[index].points.len) {
    self.selected_point = null;
    editor.note("that point is gone");
    return;
}
```

## Info

### IN-B01: Root directories resolve to the wrong place in `listBeside` and `openUrl`

**File:** `script_file.zig:158-159` and `:254-255`
**Issue:** `listBeside` trims the separator while `dir_len > 1`, so `C:\` becomes `C:`. On Windows that is "the current directory of drive C", not the root. The comment says "a root keeps its own" but only handles `/`. `openUrl` takes `script_path[0 .. lastIndexOfAny orelse 0]`, which gives `C:` for `C:\x.lua` and an empty string (mapped to `"."`) for `/x.lua`.
**Fix:** Keep the separator when the remainder is a bare drive or empty.

### IN-B02: `isBareName` accepts Windows reserved device names

**File:** `script_file.zig:27-37`
**Issue:** `CON`, `NUL`, `AUX`, `PRN`, `COM1` and `LPT1` pass. `<dir>\CON.lua` is still the console device on Windows, so a copy would target a device. It matches `NMapRecords::IsBareScriptName` by design.
**Fix:** Reject them in both implementations, or note the accepted risk.

### IN-B03: Smaller correctness and duplication points in `editor.zig`

**File:** `editor.zig:792-806` (`reloadObjects`), `821`, `944`, `1040`
**Issue:**
- `reloadObjects` duplicates `Document.reload`'s object read. Both ignore the second call's `total`, so if the bridge ever returns fewer objects than the sizing pass, the tail of the resized list is uninitialised.
- `addVso`, `drawBridge` and `drawEntrenchment` return `0` when the bridge reported a negative index with `.ok`. That would silently select a wrong line or bridge.
**Fix:** Share one helper, truncate the list to the second `total`, and return `error.Failed` for a negative index.

### IN-B04: `Editor.open` bumps some generations but not `record_generations` or `sounds_generation`

**File:** `editor.zig:170-172` and `:192-194`
**Issue:** `open` and `closeDocument` bump `vso`, `bridges` and `entrenchments`. The doc comment for `record_generations` says it moves on every record edit, undo and redo. Today the app compensates in `panels.State.mapOpened`, which resets every `*_generation_seen`. Any new consumer keyed on the counter alone would show the previous map's groups, areas and anchors.
**Fix:** Bump `record_generations` (all kinds) and `sounds_generation` in `open` and `closeDocument` too.

### IN-B05: A vso drag that returns to its start leaves a no-op undo step

**File:** `editor.zig:752-777` (`prepareEdit`, `commitEdit`); `tools_vso.zig:310-347`
**Issue:** `place`, `setScriptID` and `editRecord` drop a gesture that ends where it began (`dropTop`). The `.edit` path cannot compare states and always keeps the entry. The map is marked dirty and the history gains a no-op step. Each mouse-move event is also a separate bridge edit and token, all replayed on undo.
**Fix:** Low priority. Coalesce tokens per gesture, or have the bridge answer "unchanged" so the core can skip the token.

### IN-B06: Delete clears the selection before the bridge call, so a refused delete loses it

**File:** `tools_groups.zig:127-128, 329-331`; `tools_ai.zig:158-159, 379-380`
**Issue:** `self.selected = null; try editor.delete…(index)`. When the bridge refuses (an entrenchment holding units, for example) the selection is gone and the user must click again.
**Fix:** Clear the selection after the call succeeds.

### IN-B07: Esc cancels the gesture but not the edit it already made

**File:** `tools_ai.zig:161-167, 610`; `tools_vso.zig:236-238`; `tools_groups.zig:104-131`
**Issue:**
- For the handle-drag tools (Script Areas, AI General, Roads & Rivers), Esc during a drag stops the gesture but keeps the partially moved geometry as one undo step.
- `BridgeTool` ignores Esc entirely, unlike `FenceTool`, so a bridge drag can only be abandoned by releasing within 4 px of the start.
- Inconsistent, but no state is corrupted.
**Fix:** Decide on a policy. Either Esc reverts the gesture by undoing its entry, or it is documented as "stop dragging". Give `BridgeTool` the same Esc as `FenceTool`.

### IN-B08: Unchecked integer and float casts in helper paths

**File:** `records.zig:431-434`, `fake_bridge.zig:1194, 1376, 2545`
**Issue:**
- `AiSide.ensureSideExists` does `@intCast(self.side + 1)` to `u32`, which panics for a negative `side`. Current callers always pass 0..1023.
- The fake converts unbounded floats with `@intFromFloat` (`planFor` span count, `aiTile`, `visToAi`), so a huge finite coordinate panics the test process instead of being refused.
**Fix:** Validate `side >= 0`. Clamp or refuse out-of-range floats in the fake the way the real bridge does.

---

_Reviewed: 2026-09-30_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_


# Slice C


# Phase 4: Code Review Report (slice C: Zig app, build, CI, C++ tests)

**Reviewed:** 2026-09-30
**Depth:** standard (diff since a1a9600dd; `core/script_file.zig` and `core/files.zig` were also read, because the script copy and Save As path handling live there)
**Files Reviewed:** 22
**Status:** issues_found

## Summary

The app layer is mostly careful. Script destinations are always a fixed directory plus a name that passed `isBareName`. `pickScript` and `chooseOtherScript` refuse shipped maps. The CI sparse-checkout additions are complete: the Windows and macOS jobs already pull all of `/Data/Terrain/`. `tools/zig/build_hermeticity_test.zig` passes against the current `build.zig`. I ran `zig test` on it.

The main problems are these:

- **Save As** can silently overwrite a script the user already has.
- **Dead `errdefer`s.** The two-pass reads in `c_bridge.zig` use `errdefer` in functions that return a `Status` enum, so the cleanup never runs. I verified this with a probe.
- **No-map REFUSED.** `readStartCommand` and `readAiSide` treat the bridge's "no map is open" REFUSED as the sizing signal and then report a fake OK.
- **Fast clicks are dropped.** The new double-click routing drops the second of two fast clicks in every tool that does not want double clicks, including the M1 tools.
- **Sweep and predicate vacuity.** Some sweep steps and predicates can pass without checking anything.

## Critical Issues

### CR-C01: Save As "Copy script beside the new map?" silently overwrites an existing, different script

**File:** `Sources/editor/app/panels.zig:1374-1403` (and `Sources/editor/core/script_file.zig:225-235`, `core/files.zig:179`)
**Issue:** `offerScriptCopyAlong` only checks two things: that the old script exists, and that the old and new paths are not the same file. It never checks whether `<new folder>/<name>.lua` already exists. `answerScriptCopyAlong(yes)` then calls `copyAlong` → `files.copy`, which is `std.Io.Dir.copyFile(.., .{})` and replaces an existing file.

The only question the user sees is "Copy X.lua beside the new map?". Answering Yes destroys a different `X.lua` already in that folder. Shipped maps share generic script names, and users keep several maps per folder. `copyInto` (Choose other) does have a "Replace it?" question. The two paths are inconsistent, and only the less common one is protected.

**Fix:** In `offerScriptCopyAlong`, check `files.exists(new_path)` and compare bytes or mtime. If a different file is there, either ask "Replace it?" using the existing `script_pick` modal, or skip the offer and say so on the status line. Do not overwrite by default:
```zig
if (files.exists(new_path) and !core.script_file.sameFile(files, path, new_path)) {
    state.view.setStatus("script: ", "a different script of that name is already beside the new map; not copied");
    return;
}
```

## Warnings

### WR-C01: `errdefer` in functions that return `Status` never runs (leaks on every failure path)

**File:** `Sources/editor/app/c_bridge.zig:545, 574, 596-599, 676, 810`
**Issue:** `readStartCommand` (545), `readAiSide` (574 and 596), `readGroup` (676) and `recordKeys .group` (810) all return `Status`, a plain enum. `return .failed` and `return read` are not errors, so none of these `errdefer`s ever fires. I confirmed this with a probe (`errdefer freed = true; return .failed` leaves `freed == false`).

Every non-OK exit leaks the allocation: `units`, `mobile`, `ids`, `keys`, and the partly built `parcels` with their `points`. The hand-written `allocator.free(points)` at 609 shows the author expected the `errdefer`s to work everywhere else. Callers hand in `state.allocator`, so this is a real heap leak each time the bridge refuses or returns an inconsistent count.

**Fix:** Use explicit cleanup, for example a helper that returns `Status` plus `defer`/`errdefer`-free code:
```zig
var ok = false;
defer if (!ok) allocator.free(units);
...
ok = true; // just before `return .ok`
```
Or make the inner function return `error{...}!void` and convert to `Status` in the wrapper.

### WR-C02: Two-pass reads treat "no map is open" (REFUSED) as a successful sizing pass and report OK with a zeroed record

**File:** `Sources/editor/app/c_bridge.zig:541-560 (readStartCommand), 569-625 (readAiSide)`; also `vtableInsertRecord .group` at 845-849
**Issue:** The C side answers `BK_EDITOR_REFUSED` "no map is open" (bridge.cpp, `BkEditorStartCommand` and `BkEditorAIGeneralSide`) without touching the record. The adapter accepts `.refused` as the sizing signal. `record.unit_count` is still the zeroed value, so `units.len == 0`, the second call is skipped, and the function returns `.ok` with an all-zero start command (or an empty AI side). The REFUSED-on-sizing contract cannot tell "the content did not fit" apart from "nothing was read".

The same confusion in `vtableInsertRecord .group`: the `BkEditorGroup` probe is REFUSED with `count` still 0, so `count >= 0` triggers "there is already a reinforcement group with that ID" on a closed map.

**Fix:** Only treat REFUSED as sizing when the bridge actually wrote the counts. For example, pre-set `record.unit_count = -1` and `info.side_count = -1` and return `sizing` unchanged if the field is still negative. In the group probe, pre-set `count = -2` and check that it is exactly `-1` before calling the group absent.

### WR-C03: The new double-click routing drops the second of two fast clicks in every tool that does not want double clicks

**File:** `Sources/editor/app/view_math.zig:438` (`kindOf`), used at `Sources/editor/app/view.zig:377, 221-227`
**Issue:** `kindOf` maps every left press with `clicks == 2` to `.double_click` and every release with `clicks == 2` to `null`. `view.zig` ignores `.double_click` when `!spec.needs_double_click`. SDL3's default double-click window is 500 ms and 32 px. So in Select, Brush, Place, Bridge, Fence, Script Areas, Reserve Positions and AI General, a second click within 500 ms and 32 px of the first is swallowed completely.

That includes quick successive placements, quick brush dabs, and the reserve-position "gun, truck" picks and the AI "parcel, point" clicks. This is a regression for the M1 tools, and the unit tests only use an explicit `clicks = 2` event, so they never hit it.

**Fix:** Pass the tool's capability into the mapping. A tool that does not need double clicks gets its `clicks >= 2` events as ordinary press and release events:
```zig
const kind = view_math.kindOf(.{ ..., .clicks = if (spec.needs_double_click) button.clicks else 1 }) orelse return;
```

### WR-C04: `do=script_choose` reports OK when the copy was refused, if the map already names a script of that name

**File:** `Sources/editor/app/commands.zig:689-701`
**Issue:** After `panels.pickScript(...)`, the command decides success by reading the map's script name and comparing it to the picked name. `pickScript` can refuse without changing anything: a shipped map, a failed copy, or a non-bare name. If the map already names that script, `have == name` is true and the command returns `.ok`. The build comment says the scenario clears the script to `none` first, to avoid this.

**Fix:** Have `pickScript` return a status (`copied`, `asked`, `refused`) and return `.refused` for anything but `copied`, instead of inferring success from state. Alternatively, record the undo depth before the call and require that it grew.

### WR-C05: The M2 sweeps can pass without exercising the edits (refusals are "counted, never a failure")

**File:** `tools/zig/editor_bridge_test.cpp:8205-8326` (`TestM2Sweep`)
**Issue:** The engine sweep draws a bridge, a fence run and an entrenchment, and deletes a shipped bridge, a shipped entrenchment and a cascade unit. Every refusal only increments `refused[...]`. There is no assertion that any kind was ever applied. If a regression made the bridge refuse every draw, the sweep would end with `nEdits == 0` and print "all restored byte-exact".

The map-file sweep always applies at least two kinds (script file and camera anchor), so it cannot go fully vacuous. Its per-kind counts (`kinds`) are also only printed, never asserted.

**Fix:** After the loop, require a minimum per kind, for example `Check( done["bridge drawn"] >= 20, ... )`, and likewise for the other kinds. Do the same for `kinds[...]` in `SweepM2Edits`.

### WR-C06: `expect=test_game_script` can pass on a stale script copied by an earlier run

**File:** `Sources/editor/app/commands.zig:722-743`; `build.zig` (`stale_scripts` loop for `auto_m2_dir` / `auto_m2_along_dir`)
**Issue:** The build deletes stale `m2_script*.lua` beside `m2.bzm` and `m2_along.bzm`, because a stale file would let the copy-along check pass. Test in game copies the script into the generated-data test-map folder (`BkEditorTestMapPath`), and that folder is never cleaned. If `copyForTest` silently fails on a later run (a failed copy only shows a status note), the game loads the previous run's `m2_script.lua` and the predicate still passes.

**Fix:** Delete `<test folder>/m2_script.lua` before the scenario (for example a `delete_matching` step pointed at the test-map directory), or have `testGameScript` also require that the script file in the test folder is newer than the scenario's start.

### WR-C07: `game_reads_m2.run` asserts nothing about fences, and the shot is taken from the first `autoshot_*.rgba` found

**File:** `Sources/editor/app/game_reads_m2.zig:599-604, 504-520`
**Issue:**
- The fence step only checks that the editor placed fences (`fences` counts editor-side objects). The game's trace is not consulted, so the "game reads it" claim for fences cannot fail. The comment admits this ("clean exit is the proof"), but the PASS line lists fences among the checks "each asserted above on the game's own report".
- `keepEditedShot` keeps the first `autoshot_*.rgba` in the game directory without checking it is from the edited run. It is swept only at the end or by the build's cleanup. A leftover file from an aborted run is reported as "the edited game's shot".

**Fix:** Reword the PASS line so fences are not claimed as game-verified. Delete `autoshot_*.rgba` before each `play`, so the file found afterwards can only be from that run.

### WR-C08: `State.deinit` never frees the `ai_sides` list itself

**File:** `Sources/editor/app/panels.zig:597` (and `889-892`)
**Issue:** `deinit` calls `freeAiSides()`, which frees each side's contents and then does `clearRetainingCapacity()`. The `ArrayListUnmanaged` backing buffer is never released. Every other list in this block (`areas`, `groups`, `groups_checked`, `hidden_*`, `vso_line_*`, `ai_parcels_at_open`) gets a `.deinit`.

**Fix:** Add `self.ai_sides.deinit(self.allocator);` after `self.freeAiSides();`.

### WR-C09: `c_bridge_test` passes vacuously when it cannot run, and some assertions are weaker than their comments

**File:** `Sources/editor/app/c_bridge_test.zig:155-161, 322, 424-434, 455-470`
**Issue:**
- The engine tier returns success when `Host.start` fails with `error.NoDevice`. A CI runner that loses its GPU passes the whole tier silently, and the first test does the same with `SkipZigTest`. This is documented for the C++ tiers, but nothing in the Zig tier makes it visible.
- Line 322 asserts only `!= BK_EDITOR_BAD_ARGUMENT` for the `BkEditorGroupIDs` sizing call, so a REFUSED or FAILED answer passes.
- The start-command cascade checks (424-434) sit inside `if (editor.document.find(commanded) == null)`. If the delete is refused, none of them run.
- The AI read (455-470) accepts `OK or REFUSED` for the C call, and `again` is declared `undefined` and never read. A refusal that left `parcels` untouched would read uninitialised memory at `parcels[index]`.

**Fix:** Count and print skips, and require at least one executed engine test in the build step. Tighten line 322 to `== OK or == REFUSED`. Make the cascade branch an explicit skip with a message, or `expect` that the delete succeeded. In the AI read, require `OK`, or require `REFUSED` together with `info.parcel_count > 0`, before reading `parcels[index]`, and delete the unused `again`.

## Info

### IN-C01: Selection indices are not remapped after a delete or an undo

**File:** `Sources/editor/app/commands.zig:613-618, 1030-1034, 1243-1248`
**Issue:** `deleteArea`, `deleteStartCommandAt` and `deleteReserveAt` clear the selection only when it equals the deleted index. A selection above it keeps the same number and so points at the next record. `refreshStartCommands` and `refreshReserve` drop a selection that falls past the end, but nothing shifts one that is still in range. A following Delete acts on the wrong record.

**Fix:** Decrement the selected index when it is greater than the deleted one, and clear it when an undo changes the list.

### IN-C02: Hard-coded side limit of 12 in the AI panel against a cap of 1024 (and a cache of 64)

**File:** `Sources/editor/app/panels_m2.zig:978`; `Sources/editor/app/panels.zig` (`ai_sides_cap = 64`); `Sources/editor/core/records.zig:258`
**Issue:** The radio loop stops at `shown < 12`. `setAiSide` and the bridge accept 1024 sides, and the cache keeps 64. A map with more than 12 sides cannot have its later sides selected in the panel.

**Fix:** Use one named constant, or scroll the radios.

### IN-C03: Stray doc comment and unchecked truncation in the roads/rivers markers

**File:** `Sources/editor/app/panels.zig:(doc block above refreshVsoTypes)`, `Sources/editor/app/markers.zig:620-625`
**Issue:**
- The "Re-reads the camera anchors into `anchors`" doc block sits on `refreshVsoTypes`. It belongs to `refreshAnchors`, which is defined later without one.
- The selected road or river is drawn from at most 256 key points (`centre: [256]Vec3`). A longer line is silently truncated on screen, including its width handles.

**Fix:** Move the comment. Size the buffer from `view.key_points.len`, or draw straight from the slice.

### IN-C04: A scripted press can leave its hold set, and `showMap` never clears it

**File:** `Sources/editor/app/smoke.zig:1676-1680`
**Issue:** `runPress` sets `holdScripted` before pushing the events. If `pushMotion` fails (the run is failing anyway), the hold is never released. This is only visible in a failing scripted run, but `showMap` does not clear `scripted_buttons` either. A press left held across a map reopen keeps the stale-gesture guard off.

**Fix:** Clear `scripted_buttons` in `View.showMap`/`resetGestures`.

### IN-C05: `chooseOtherScript` leaves `script_slot` busy when OS dialogs are off

**File:** `Sources/editor/app/panels.zig:1454-1468` (`chooseOtherScript`)
**Issue:** `script_slot.request(.open)` marks the slot busy, then `if (!state.os_dialogs) return;` leaves it busy with no dialog to answer it. A scripted run that clicked Choose other would wedge the slot. The scenario avoids this through `script_choose`.

**Fix:** Check `state.os_dialogs` before `request`, or release the slot on that path.

### IN-C06: `startcmd_target` cannot distinguish a target at exactly (0,0) from no target

**File:** `Sources/editor/app/commands.zig:1172-1174` and `Sources/editor/app/markers.zig:186`
**Issue:** `is_pos = !is_link and (x != 0 or y != 0)`, and the marker code does the same. A target at map point (0,0) reads as "no target", so `expect=startcmd_target:N:pos` is false and no line is drawn. It is an edge case, since (0,0) is the map corner, but the record format has no separate "unset" flag either.

**Fix:** Document it in the predicate's comment, or have the bridge report a "has target" flag.

---

_Reviewed: 2026-09-30_
_Reviewer: Claude (gsd-code-reviewer)_
_Depth: standard_
