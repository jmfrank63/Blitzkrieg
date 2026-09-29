# Phase 4: Map editor M2 - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> The decisions are in 04-CONTEXT.md. This log keeps the alternatives that were considered.

**Date:** 2026-09-30
**Phase:** 04-map-editor-m2-roads-rivers-bridges-ai-groups-scripts
**Mode:** Autonomous smart discuss. Johannes's standing order: "always choose the recommended answer, never come back with questions". The recommended answer was taken in every case. Johannes also asked that M2 be implemented fully (MFC parity for these areas), with deferrals only to M3 or later phases.
**Inputs:**
- 03-CONTEXT.md and 03-06-SUMMARY.md;
- the design spec;
- 05-CONTEXT.md and 05-PARITY.md (phase 5 had already claimed its rows);
- two read-only scouting passes over `Sources/src/MapEditor`, `Sources/src/EditorBridge`, `Sources/src/MapFile`, `Sources/editor`, the engine interfaces and the game readers.

**Areas:** Saving/undo/references; Roads and rivers; Bridges, entrenchments, fences; AI and unit logic; Scripts, areas, anchors; Camera rotation; Plan split.

---

## Area 1: Saving, undo and references

| # | Question | Chosen (recommended) | Rejected alternatives |
|---|---|---|---|
| 1 | How are M2 collections saved? | Record-level overlay over the snapshot: untouched records byte-identical (D-01) | Rebuild the collections from the engine at save, as the MFC editor does. Rejected: it breaks the preservation invariant M1 proved over 1,755 maps. Or write the whole collection from the bridge copy whenever it changed. Rejected: it reorders or reformats untouched records. |
| 2 | Undo record shape | Before/after whole records; undo writes the before-record back (D-02) | One inverse operation per command kind. Rejected: much more bridge surface, and the inverse of "resample a road" isn't exact. |
| 3 | Derived data (sampled road points, spans, trench pieces) | Computed once per edit and stored in the command; our own deterministic `nID` and frame indices (D-03) | Recompute at save. Rejected: order-dependent and untestable. Or take the engine's `rand()` `nID` and MFC's random frame indices. Rejected: the saved output could not be predicted. |
| 4 | Deleting an object others refer to | Cascade like the MFC editor, in one undo step; script-ID references are only noted (D-04) | Keep M1's refusal. Rejected: that was a stopgap "until M2 can edit those references", and the MFC editor cascades. Or also strip script IDs from groups and `mobileScriptIDs`. Rejected: other objects and Lua may share the script ID. |
| 5 | Loose spans, pieces and fences in the palette | Remove them from the palette; only their tools place them (D-05) | Keep them and add grouping afterwards. Rejected: the game asserts the grouping records, so a loose span is a trap. |
| 6 | How markers are drawn | App overlay through ImGui with WorldToScreen, toggled from View → Markers (D-06) | Engine-side debug geometry. Rejected: new renderer work, and it would be hidden behind sprites. |

## Area 2: Roads and rivers

| # | Question | Chosen | Rejected |
|---|---|---|---|
| 1 | Editing model | MFC's: control polyline plus key-point width and opacity, sampled with the same VSO code (D-07) | A freehand stripe brush. Rejected: not what the format or the MFC editor does. Or edit sampled points directly. Rejected: they would drift from the control points. |
| 2 | Tool layout | One tool with a Road/River switch and the MFC gestures (D-08) | Two separate tools. Rejected: the state and gestures are the same. |
| 3 | Engine and AI | Terrain editor Add/Update/Remove; river passability through `IAIEditor`; roads none, as in the MFC editor (D-09) | Also update road passability live. Rejected: no engine API exists, and the game computes it when it loads. |
| 4 | Heights | Never changed by M2; `UpdateZ` only fits to existing heights | Carve river beds or smooth heights along roads. Rejected: the MFC editor doesn't, and heights are M3's D-19. |

## Area 3: Bridges, entrenchments, fences

| # | Question | Chosen | Rejected |
|---|---|---|---|
| 1 | What "rotate a bridge" means | Swap the `_01`/`_02` variant and rebuild along the other axis around the same centre (D-11) | Free rotation of spans. Rejected: the descriptor fixes the direction, and there is no art for other angles. Or leave rotation out. Rejected: it was explicitly deferred from M1 to here. |
| 2 | Bridge states | Intact / built during play (`fHP` −1), only for `WoodenBig_Heavy_*`, as in the MFC editor (D-12) | Add a "destroyed" state too. Rejected: the MFC editor has none; damage is M3's damage tool. Or allow built-during-play for every bridge. Rejected: the MFC editor restricts it, and the game data may expect it. |
| 3 | Selection | Whole groups for bridges and entrenchments; fences as single objects (D-11, D-13, D-14) | Allow single span or piece selection. Rejected: this orphans the grouping records. |
| 4 | Moving a whole bridge or trench | Not added: redraw, as in the MFC editor | Group drag. Rejected: beyond parity. |
| 5 | Where the trench geometry lives | A port of the MFC builder into GFX-free C++ that the bridge and the map-file tests share (Claude's discretion) | A reimplementation in Zig. Rejected: it would break the "same function" rule for expected values. |

## Area 4: AI and unit logic

| # | Question | Chosen | Rejected |
|---|---|---|---|
| 1 | Where script IDs are edited | A Script ID field in M1's Properties panel (single selection) in M2; M3 extends the panel (D-15) | Wait for M3's properties panel. Rejected: reinforcement groups cannot be used without it. Or a Script ID field only inside the Groups panel. Rejected: the MFC editor sets it on the object. |
| 2 | Groups panel | MFC Group Manager plus "Select objects" (D-16) | A bare list of IDs only. Rejected: below parity. |
| 3 | Start commands with many units | One unit at a time ("Add selected unit") until M3's multi-selection (D-17) | Pull multi-selection into M2. Rejected: M3 owns it (D-25 of phase 5), and doing it here would collide. |
| 4 | AI general | Full MFC parcel and point editing on the map, per side (D-19) | A numeric table only. Rejected: below parity. |
| 5 | Unit creation info | M3, per 05-PARITY M5 | M2. Rejected: phase 5 had already claimed it, and the coordinator confirmed that. |

## Area 5: Scripts, script areas, camera anchors

| # | Question | Chosen | Rejected |
|---|---|---|---|
| 1 | Where the script lives | Beside the map, since the game loads `<map dir>\<basename>.lua`. "Choose other…" copies the file beside the map; test launch copies it beside the test map (D-20) | Any storage path. Rejected: the game ignores the directory part. Or an embedded script editor. Rejected: beyond parity; Open in the system editor is enough. |
| 2 | Area coordinates | Keep AI units, as the file stores them; convert only edited areas with the MFC rule (D-21) | Keep Vis units and convert at save, as the MFC editor does. Rejected: it would change untouched areas. |
| 3 | Area names | Non-empty and unique, enforced | Free text. Rejected: Lua finds areas by name. |
| 4 | Area editing | Draw, rename, delete, plus move/resize handles (D-21) | Draw and delete only, as in the MFC editor. Rejected: the handles reuse D-06's machinery at little cost, and fixing an area by redrawing is error-prone. |
| 5 | Camera anchors | Set from the view centre per player/neutral, with Go to and Clear (D-22) | A numeric entry only. Rejected: the MFC editor uses the view. |

## Area 6: Camera rotation (phase 3's D-12)

| # | Question | Chosen | Rejected |
|---|---|---|---|
| 1 | Build it in M2? | No. Closed as not planned for the editor, with the reason in the spec (D-23) | Rewrite terrain drawing through the view matrix in M2. Rejected: even then, every building, tree and infantry sprite and the tile art are one-angle pre-rendered art with baked lighting, so rotation cannot show the back of a building, which was its whole purpose. Or keep it "deferred to M3". Rejected: M3 does not own it (05-CONTEXT), and the obstacle is art, not scheduling. Or build 90° steps only. Rejected: the same art problem (94–99 % black at +90/+180/+270 in 03-06). |
| 2 | How the need to see behind buildings is met | M2 markers drawn above sprites; M3's Units/Objects layer toggles | An object fade near the cursor in M2. Rejected: new renderer work for little gain once the layers exist. |

## Area 7: Plan split and exit

| # | Question | Chosen | Rejected |
|---|---|---|---|
| 1 | One phase or several? | One phase, 8 plans in 3 waves (D-24) | Split into 4a/4b phases. Rejected: the foundations are shared, and the roadmap has one M2 phase with M3 depending on it. |
| 2 | Running wave 2 | Plans one after another in the shared worktree (shared files) | Parallel executors. Rejected: `bridge.h`, `history.zig`, `panels.zig` and `auto.zig` would conflict. |
| 3 | Exit bar | Nine testable criteria covering all tiers, the game, CI, parity rows and a hand try (D-25) | Only automated tiers. Rejected: M1 set the precedent of a hand try on the release build. |

## Claude's Discretion
- The C ABI shape per collection, marker styling, key bindings the MFC editor does not fix, where the trench geometry code lives, and the deterministic frame-index rule.

## Deferred Ideas
- Moving a whole bridge or trench by drag.
- Object fade near the cursor.
- The rotation renderer and art work.
- Everything in 05-PARITY owned by M3.
