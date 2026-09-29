---
phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
plan: 03
subsystem: map-editor
tags: [map-editor, m2, app, commands, markers, tool-registry, input, automation, camera-anchors, zig]

requires:
  - phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
    plan: 01
    provides: the record_edit command, Editor.setCameraAnchor / clearCameraAnchor, record_generations
provides:
  - commands.zig, one table of named commands and predicates shared by menus, panels and BK_EDITOR_AUTO do=/expect=
  - the M2 marker layer (marker_logic.zig pure, markers.zig drawing) with View -> Markers and per-kind caps
  - the Camera anchors panel, Map -> Player camera menu (D-22)
  - tool_registry.zig replacing the closed Tool enum, with per-tool input flags and bare-digit shortcuts
  - tools.Event right_press/right_drag/right_release/double_click and tools.Key enter/insert/escape/space, routed by view.zig
  - automation verbs do, expect, tool, rclick, rpress, rdrag, rrelease, dblclick, text and key INSERT
  - build step map-editor-auto-m2
affects: [04-04, 04-05, 04-06, 04-07, 04-08, 04-09, 04-10, 04-11, 04-12, 04-13]

commits: 3
plan_head_before: 8c5f7fa7f25c5bf567145e435cd0dd72c3bd179b
actuals:
  tokens: 24300
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "A later plan adds a tool as one ToolId, one registry Entry (flags, shortcut 4-9, marker kinds) and one arm in View.dispatch; routing reads the flags, never the tool's name"
    - "A later plan adds a command or predicate as one row in commands.zig's tables; the menu, the panel button and do=/expect= all call the same function"
    - "A later plan adds a marker kind as one function in markers.zig and one call in drawM2Markers; visibility is marker_logic.visible(set, kind, active tool's kinds)"
    - "The M2 scenario is a Zig array of entry strings in build.zig, frames ascending; each plan appends a block of lines"

key-files:
  created:
    - Sources/editor/app/commands.zig
    - Sources/editor/app/marker_logic.zig
    - Sources/editor/app/markers.zig
    - Sources/editor/app/panels_m2.zig
    - Sources/editor/app/tool_registry.zig
  modified:
    - Sources/editor/core/tools.zig
    - Sources/editor/app/view.zig
    - Sources/editor/app/view_math.zig
    - Sources/editor/app/panels.zig
    - Sources/editor/app/panels_logic.zig
    - Sources/editor/app/auto.zig
    - Sources/editor/app/smoke.zig
    - build.zig

key-decisions:
  - "Command and predicate handlers return Outcome (ok, refused, unknown_name, bad_arg) directly instead of a bool, so a bad argument is told apart from a refusal without a second channel"
  - "A view routes the right button, Ctrl+left and double click only to a tool whose registry entry asks for them: the other tools are not handed the events at all, and open no right gesture"
  - "The second click of a double click is a double_click event and its release ends nothing (kindOf returns null for a left up with clicks 2); the single click's own press and release already arrived"
  - "tool= takes the ToolId's own lower-case name (select, brush, place, later roads_rivers ...), so no second label table exists"
  - "The current player for Map -> Player camera is the placer's player (the Objects palette has no player picker of its own)"

patterns-established:
  - "View.centreOn(real, x, y): a clamped camera move for every Go to (anchors now, areas later)"
  - "Local reference shots are refreshed when the panels change on purpose: delete zig-out/local-test/map-editor-auto/reference/edited.tga and let the next run seed it"

requirements-completed: [D-06, D-22, D-02, D-24]

coverage:
  - id: D1
    description: "Set camera for player N and the neutral camera store the ground point under the screen centre as one undo step each; the Camera anchors panel lists every anchor with Go to and Clear; anchors are marked on the map"
    requirement: "D-22"
    verification:
      - kind: integration
        ref: "zig build map-editor-auto-m2: do=camera_player:4, expect=anchor_set:4, expect=undo_depth:1, Ctrl+Z, expect=anchor_unset:4, Ctrl+Y, expect=anchor_set:4, camera_neutral, camera_goto:0, camera_clear:neutral, BK_EDITOR_AUTO: done"
        status: pass
      - kind: other
        ref: "zig-out/local-test/04-03-m2_anchor.png (capture inspected: the P0 marker at the screen centre after goto, the panel rows with positions and unset)"
        status: pass
    human_judgment: false
  - id: D2
    description: "M2 markers draw on the background draw list through BkEditorWorldToScreen; View -> Markers switches each kind; the active tool's kinds stay on; per-kind caps"
    requirement: "D-06"
    verification:
      - kind: unit
        ref: "zig build test-map-editor-panels: marker_logic.zig tests (default all on, active tool forces on, AI/world round trip, labels and caps)"
        status: pass
      - kind: integration
        ref: "map-editor-auto-m2 capture: the anchor triangle and its label are drawn (visual inspection of m2_anchor)"
        status: pass
    human_judgment: false
  - id: D3
    description: "A tool can receive right press/drag/release, double click and the Enter, Insert, Escape and Space keys; Ctrl+left is the right button only where the registry entry asks"
    requirement: "D-24"
    verification:
      - kind: unit
        ref: "zig build test-map-editor-view: view_math kindOf/staleGesture tests (right button, clicks 2, end_right) and view.zig rig tests (no gesture in tools that do not ask, double click opens no gesture, Ctrl+left stays a press in select, shortcuts)"
        status: pass
      - kind: unit
        ref: "zig build test-editor-core: 'the M1 tools ignore the right button, double click and the new keys'"
        status: pass
    human_judgment: false
  - id: D4
    description: "The registry's shortcuts are unique bare digits, M1 keeps 1-3, none is a taken letter or a Ctrl chord"
    requirement: "D-24"
    verification:
      - kind: unit
        ref: "zig build test-map-editor-panels: tool_registry.zig tests"
        status: pass
    human_judgment: false
  - id: D5
    description: "BK_EDITOR_AUTO can switch tools, right-click, double-click, right-drag, type text, press Insert, run commands and assert predicates, with malformed entries rejected naming the entry"
    requirement: "D-24"
    verification:
      - kind: unit
        ref: "zig build test-map-editor-auto: parser tests for every new verb, good and bad (name/argument/label/text classes and lengths)"
        status: pass
      - kind: integration
        ref: "zig build map-editor-auto map-editor-auto-m2: two 'BK_EDITOR_AUTO: done' lines, no FAIL"
        status: pass
    human_judgment: false

duration: 20min
completed: 2026-09-30
status: complete
---

# Phase 4 Plan 03: App foundations for M2 Summary

**A shared named-command registry, the M2 marker layer, a tool registry with right-button, double-click and MFC-key routing, and the BK_EDITOR_AUTO verbs to script all of it, traced end to end by the camera-anchor panel and a local `map-editor-auto-m2` scenario.**

## Performance

- **Duration:** about 20 min of work (started 2026-09-29T22:57Z; the run paused once for freed disk space)
- **Completed:** 2026-09-29T23:20Z
- **Tasks:** 3 (one tracer, two auto)
- **Files:** 13 changed (5 created), 1313 insertions, 76 deletions

## Accomplishments

- `commands.zig`: `run` / `check` over two tables. Commands `camera_player`, `camera_neutral`, `camera_clear`, `camera_goto`; predicates `anchor_set`, `anchor_unset`, `undo_depth`. The Map menu, the Camera anchors panel and `do=` / `expect=` call the same functions. Predicates read the bridge fresh, so the frame after a command already sees it.
- Camera anchors (D-22): Map -> Player camera -> "Set camera for player N" / "Set neutral camera" (screen-centre ground point, one undo step), the panel with Go to and Clear per row and the game's fallback note, anchor triangles with N / P<n> labels on the background draw list. `State.refreshAnchors` follows `record_generations`, so undo and redo update panel and markers.
- `marker_logic.zig` (pure, tested from the panels tier): `MarkerKind`, `MarkerSet`, `visible`, `cap`, `aiToWorld` / `worldToAi` with `fAITileXCoeff` unrounded. View -> Markers has one checkbox per kind.
- `tool_registry.zig` replaces the closed enum (`Tool` is now an alias of `ToolId`); the Tools menu and palette list the registry entries.
- Input: `kindOf` knows the right button and clicks == 2; `staleGesture` gains `end_right`; `View` keeps a right gesture (its own button, or Ctrl+left where the entry asks) and `switchTool` ends both buttons' gestures first; Return/KP_Enter, Insert, Escape and Space reach tools (no key repeat).
- Automation: `do`, `expect`, `tool`, `rclick`, `rpress`, `rdrag`, `rrelease`, `dblclick`, `text`, key `INSERT`. A double click is two down/up pairs (clicks 1 then 2).
- `map-editor-auto-m2`: a joined-array schedule; it exercises set, undo, redo, neutral, goto, clear, then the gesture and key block, then saves and shoots.

## Task Commits

1. **Task 1: Camera anchors in the app (tracer)** - `2ca1311b4` (feat)
2. **Task 2: Tool registry, right button, double click, new keys** - `92fec8046` (feat)
3. **Task 3: Automation verbs for tools and gestures** - `0c9a64eeb` (feat)

**Plan metadata:** the docs commit that carries this file.

## Verification (plan level, macOS arm64, `-Dcopy-data=false -Dtest-mode=run`)

| Check | Result |
|---|---|
| `zig test tools/zig/build_hermeticity_test.zig` (after every build.zig edit) | 3/3 pass |
| `test-editor-core` | 101 pass (5 new) |
| `test-map-editor-view` | 27 view_math + 62 view.zig rig tests pass |
| `test-map-editor-panels` | 115 pass (marker_logic and tool_registry now run here) |
| `test-map-editor-auto` | 9 pass, including a good and a bad case per new verb |
| `map-editor-smoke`, `map-editor-host-check` | pass |
| `map-editor-auto` and `map-editor-auto-m2` | two `BK_EDITOR_AUTO: done`, no FAIL |

Tracer gate: the tracer's `<verify>` is automated, so it was re-run end to end under the end-of-phase mode; it passed and the plan expanded.

## Decisions Made

See `key-decisions`. Two more, both taken because the plan left them open:

- Ctrl+left as the right button: the view checks `SDL_KMOD_CTRL` only on a left press, only when the entry has both `needs_right_button` and `ctrl_click_is_right`, and holds the gesture until that left release. No shipped tool needs it yet; a unit test proves Select keeps an ordinary press.
- The Camera anchors panel sits under Players and takes 190 px from the Sounds panel's share of the right column (first appearance only; users can drag).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug in the plan's scenario] coldwinter already holds anchors for players 0-3**
- **Found during:** Task 1 (first `map-editor-auto-m2` run: `expect=anchor_unset:0 was false`)
- **Issue:** the plan's segment sets player 0, undoes it and expects "unset", but undo restores the file's own anchor for player 0.
- **Fix:** the segment works on player 4 and the neutral anchor, both unset in that map, and adds neutral, goto and clear coverage.
- **Files modified:** `build.zig`
- **Committed in:** `2ca1311b4`

**2. [Rule 3 - Blocking] `Build.Step` has no `mustRunAfter`**
- **Found during:** Task 1
- **Issue:** the plan wants the M2 step after `map-editor-auto` so two engines never start at once; this Zig has no ordering-only edge.
- **Fix:** `map-editor-auto-m2` depends on the M1 step's cleanup, so `zig build map-editor-auto-m2` runs the M1 scenario first (about a minute longer). Noted in a build.zig comment.
- **Committed in:** `2ca1311b4`

**3. [Rule 3 - Blocking] Stale local reference shot**
- **Found during:** Task 1 (`map-editor-auto`: `compare=edited: 4.6138% of pixels differ`)
- **Issue:** the local, never-committed reference `zig-out/local-test/map-editor-auto/reference/edited.tga` predated 04-01's palette filter (the object groups differ) and now this plan's Map menu and Camera anchors panel. The change is intended.
- **Fix:** deleted the reference and let the run seed it, as 03-12's rule says for a deliberate rendering change. Not a repository file.
- **Committed in:** none (local artefact)

**4. [Rule 3 - Blocking] Two edit targets landed in the wrong struct**
- **Found during:** Task 3 (compile errors)
- **Issue:** my scripted replacement changed `Script`'s `pushMotion` / `pushButton` bodies (smoke.zig has same-named helpers in `Script`) instead of `AutoRunner`'s.
- **Fix:** reverted `Script`'s two lines and edited `AutoRunner`'s; the diff touches `Script` in no line but the new import.
- **Committed in:** `0c9a64eeb`

### Signature refinements and reordering

- Handlers return `Outcome` rather than `bool` (see key-decisions); `run` / `check` are as planned.
- `View -> Markers` and the `MarkerSet` state landed in Task 1's commit (the panel and marker layer needed them), not Task 3's; Task 3 held only the verbs and the scenario block.
- `View.centreOn` (view.zig) is new: Go to needs a clamped camera move; tested in the view tier.
- `parseAnchorSlot` lives in `panels_logic.zig` so its tests run in the pure tier; `commands.zig` itself imports the panels and cannot be tested there.
- `staleGesture` takes a fourth argument; its three existing tests were updated, four new cases added.

### Process note

The commit trailer is `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>`, the model that ran this plan, as in 04-01 and 04-02, not the `(1M context)` wording in the executor rules.

---

**Total deviations:** 4 auto-fixed (1 Rule 1-class scenario error, 3 Rule 3) plus refinements
**Impact on plan:** no scope change; deviation 1 changes which slots the M2 scenario uses, and every later segment appends to the same array.

## Issues Encountered

- A first tool-registry-independent version of `markers.zig` used an empty active-tool set; Task 2 replaced it with the registry lookup.
- The `text=` verb has no focused ImGui field in the scenario, so the run proves only that the event is pushed and the loop survives; a scenario with a focused field arrives with the panels that have text fields (04-05 on).
- Tools with `needs_right_button` do not exist yet, so the view's right-button routing is covered by the "not handed to tools that do not ask" tests and the pure `kindOf` / `staleGesture` tests; 04-04 (roads) is the first real consumer.

## Known Stubs

None.

## User Setup Required

None.

## Next Phase Readiness

- 04-04 onward add a tool with: a `ToolId`, an `Entry` (shortcut 4-9, flags, marker kinds), one `dispatch` arm, commands and predicates in `commands.zig`, a marker function in `markers.zig`, and a block in `auto_m2_entries`.
- `zig-out/local-test/04-03-run-m2.sh "<schedule>"` runs the M2 scenario directly against the staged editor for quick iteration (a local script, not committed).

## Threat surface

No new surface beyond the plan's model: `do=` / `expect=` / `tool=` / `text=` arguments are checked for character class and length in `auto.zig` and unknown names fail the run (T-04-03-01); every marker kind has a cap and a failed conversion skips one item (T-04-03-02).

## Self-Check: PASSED

- Created files found: `commands.zig`, `marker_logic.zig`, `markers.zig`, `panels_m2.zig`, `tool_registry.zig`.
- Commits found: `2ca1311b4`, `92fec8046`, `0c9a64eeb`.
- Every task's acceptance greps re-run; the plan-level verification table above.

---
*Phase: 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts*
*Completed: 2026-09-30*
