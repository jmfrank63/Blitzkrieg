---
status: testing
phase: 05-map-editor-m3-random-map-templates-minimap-tools-parity
source: [05-VERIFICATION.md, 05-11-SUMMARY.md]
started: 2026-10-04T02:05:40Z
updated: 2026-10-04T02:40:47Z
---

## Current Test

number: 1
name: Start with no map - View, Help, About
expected: |
  View: every panel and window has a check. Hide Tools, Sounds and the status bar, then Reset layout brings them back.
  Help > Keys and tools (also F1) lists the tools and keys. About names the product and the licence.
awaiting: user response

## Tests

Build first (release). macOS: `zig build install-map-editor --release=fast -Dtarget=aarch64-macos -Dcopy-data=false`, then run `zig-out/game/macos/arm64/release/MapEditor`. Windows: the same command with `-Dtarget=x86_64-windows-msvc` on a machine with a desktop, or `zig build package-game-editors --release=fast` and unzip the editors package. Tests 1-12 are on macOS; test 13 repeats them on Windows.

### 1. Start with no map - View, Help, About
expected: Every panel and window has a View check. Hidden Tools, Sounds and status bar come back with Reset layout. Help > Keys and tools (F1) lists tools and keys. About names the product and the licence.
result: [pending]

### 2. New map (Cmd/Ctrl+N, 16x16 summer)
expected: The title shows `*`, the size and the mod. The status bar shows VIS and SCRIPT coordinates.
result: [pending]

### 3. Heights
expected: Raise, lower and Alt+drag level all work. Generate hills works. After a Ctrl+drag cliff, the Minimap's Heights picture turns those vertices red.
result: [pending]

### 4. Fields and Objects - place, wheel, band-select, garrison, Properties
expected: With the Place tool, a half-transparent picture of the chosen object follows the pointer. A squad places. Dragging over the whole wheel, lower half too, turns the selection by the drag amount. Band-select works. A unit dropped on a bunker garrisons it. A double-click opens Properties.
result: [pending]

### 5. Players, Unit Creation Info, Check Map
expected: Players and Unit Creation Info edit and keep their values. Check Map lists the findings, and Fix all is one undo step.
result: [pending]

### 6. Layers
expected: Each layer toggle changes the view. Depth Complexity is greyed (accepted gap). Unit Fire Ranges shows.
result: [pending]

### 7. Minimap
expected: A click on the minimap moves the camera. After a Save As, Create Minimap Images writes the images.
result: [pending]

### 8. Save as XML / BZM, Test in game, Options game parameters
expected: Both formats save and reopen. Test in game starts the game. With `-windowed` (or another harmless parameter) in Tools > Options > Game parameters, Test in game starts the game with it.
result: issue
reported: "Crash report pasted: Game (child of MapEditor) SIGABRT at 09:39:29 - CTreeAccessor::Add<vector<SProgressMovieInfo>> in CProgressScreen::Init, from CInterfaceMission::NewMission. Also: a crash is not what should happen - handle a missing progress.xml cleanly instead of crashing."
severity: blocker

### 9. Create Random Map
expected: With a template and a seed, the map opens. After moving the map and its `.lua` to another folder, Test in game runs the script from there.
result: [pending]

### 10. Composers (Containers, Graphs, Fields, Templates)
expected: Each composer opens a shipped file, edits, passes Check!, saves with Save As, and a map regenerates from the saved file.
result: [pending]

### 11. Tools > Export lists
expected: The four lists land under the user folder.
result: [pending]

### 12. OS drag and drop and single instance
expected: A `.bzm` dragged from Finder or Explorer onto the window opens; a dirty map asks first. A `.txt` is ignored with a note. With one editor running, `MapEditor <another map>` brings the running window forward and opens the map, and the second process exits.
result: [pending]

### 13. Windows release build - repeat 1-12 and the GPU look
expected: Tests 1-12 pass on Windows. Roads, rivers, wire frame and the minimap look right on D3D12/Vulkan.
result: [pending]

### 14. Minimap shots look right
expected: `m3-minimap-before`, `-after`, `-game` and `-heights` from map-editor-m3-auto look like plausible minimaps. The automated compare only shows that the pixels change, not that they look right.
result: [pending]

## Summary

total: 14
passed: 0
issues: 1
pending: 13
skipped: 0
blocked: 0

## Gaps

- gap_id: G-05-8
  truth: "Test in game starts the game from the editor and the mission loads"
  status: failed
  reason: "User reported: Game SIGABRT in CProgressScreen::Init (CTreeAccessor::Add<vector<SProgressMovieInfo>>) on Test in game; a missing progress.xml must be handled cleanly, not crash"
  severity: blocker
  test: 8
  root_cause: "progress.xml (Data/movies/progress) not found by the launched Game; CProgressScreen::Init reads the missing stream and the tree reader panics. Fix in progress: handle the missing file, and check why the install lacks it"
  artifacts: []
  missing: []
