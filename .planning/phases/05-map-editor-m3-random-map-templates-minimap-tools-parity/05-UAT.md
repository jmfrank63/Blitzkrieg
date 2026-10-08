---
status: complete
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
source: [05-VERIFICATION.md, 05-11-SUMMARY.md]
started: 2026-10-04T02:05:40Z
updated: 2026-10-05T16:00:00Z
---

## Current Test

[testing complete]

## Tests

Build first (release). macOS: `zig build install-map-editor --release=fast -Dtarget=aarch64-macos -Dcopy-data=false`, then run `zig-out/game/macos/arm64/release/MapEditor`. Windows: the same command with `-Dtarget=x86_64-windows-msvc` on a machine with a desktop, or `zig build package-game-editors --release=fast` and unzip the editors package. Tests 1-12 are on macOS; test 13 repeats them on Windows.

Tests 1-12 were run on an Intel Mac (`zig-out/game/macos/x86_64/release/MapEditor`, built with `zig build install-map-editor --release=fast -Dcopy-data=false`) after build.zig let MapEditor build for macOS x86_64.

### 1. Start with no map - View, Help, About
expected: Every panel and window has a View check. Hidden Tools, Sounds and status bar come back with Reset layout. Help > Keys and tools (F1) lists tools and keys. About names the product and the licence.
result: pass

### 2. New map (Cmd/Ctrl+N, 16x16 summer)
expected: The title shows `*`, the size and the mod. The status bar shows VIS and SCRIPT coordinates.
result: pass

### 3. Heights
expected: Raise, lower and Alt+drag level all work. Generate hills works. After a Ctrl+drag cliff, the Minimap's Heights picture turns those vertices red.
result: pass

### 4. Fields and Objects - place, wheel, band-select, garrison, Properties
expected: With the Place tool, a half-transparent picture of the chosen object follows the pointer. A squad places. Dragging over the whole wheel, lower half too, turns the selection by the drag amount. Band-select works. A unit dropped on a bunker garrisons it. A double-click opens Properties.
result: pass

### 5. Players, Unit Creation Info, Check Map
expected: Players and Unit Creation Info edit and keep their values. Check Map lists the findings, and Fix all is one undo step.
result: pass

### 6. Layers
expected: Each layer toggle changes the view. Depth Complexity is greyed (accepted gap). Unit Fire Ranges shows.
result: pass

### 7. Minimap
expected: A click on the minimap moves the camera. After a Save As, Create Minimap Images writes the images.
result: pass

### 8. Save as XML / BZM, Test in game, Options game parameters
expected: Both formats save and reopen. Test in game starts the game. With `-windowed` (or another harmless parameter) in Tools > Options > Game parameters, Test in game starts the game with it.
result: pass
note: "Re-test after 5c85692b4. The pasted crash report (09:39:29) matches an agent\'s deliberate repro run with progress.xml hidden; the crash itself is fixed either way."

### 9. Create Random Map
expected: With a template and a seed, the map opens. After moving the map and its `.lua` to another folder, Test in game runs the script from there.
result: pass

### 10. Composers (Containers, Graphs, Fields, Templates)
expected: Each composer opens a shipped file, edits, passes Check!, saves with Save As, and a map regenerates from the saved file.
result: pass

### 11. Tools > Export lists
expected: The four lists land under the user folder.
result: pass

### 12. OS drag and drop and single instance
expected: A `.bzm` dragged from Finder or Explorer onto the window opens; a dirty map asks first. A `.txt` is ignored with a note. With one editor running, `MapEditor <another map>` brings the running window forward and opens the map, and the second process exits.
result: pass

### 13. Windows release build - repeat 1-12 and the GPU look
expected: Tests 1-12 pass on Windows. Roads, rivers, wire frame and the minimap look right on D3D12/Vulkan.
result: pass
note: "Approved by Johannes 2026-10-05 on main 1d7264fd6, Windows x86_64-windows-msvc release build (zig build install-game install-map-editor --release=fast)."

### 14. Minimap shots look right
expected: `m3-minimap-before`, `-after`, `-game` and `-heights` from map-editor-m3-auto look like plausible minimaps. The automated compare only shows that the pixels change, not that they look right.
result: pass
note: "Approved by Johannes 2026-10-05."

## Summary

total: 14
passed: 14
issues: 0
pending: 0
skipped: 0
blocked: 0

## Gaps

- gap_id: G-05-8
  truth: "Test in game starts the game from the editor and the mission loads"
  status: resolved
  resolved_by: 5c85692b4
  resolved_at: 2026-10-04
  reason: "User reported: Game SIGABRT in CProgressScreen::Init (CTreeAccessor::Add<vector<SProgressMovieInfo>>) on Test in game; a missing progress.xml must be handled cleanly, not crash"
  severity: blocker
  test: 8
  root_cause: "progress.xml (Data/movies/progress) not found by the launched Game; CProgressScreen::Init reads the missing stream and the tree reader panics. CProgressScreen::Init passed a null tree from the missing file to CTreeAccessor::Add. Now the mission loads without the progress screen and logs one line; BK_PROGRESS_XML leg in map-editor-game-reads-it-m3 guards it. Staging copies all of Data, so installs are not missing the file; the crash report matched an agent's deliberate repro"
  artifacts: []
  missing: []
