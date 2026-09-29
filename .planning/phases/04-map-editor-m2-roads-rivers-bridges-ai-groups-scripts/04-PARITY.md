# Phase 4: M2 parity rows

The checklist of record for every MFC map-editor feature is `../05-map-editor-m3-random-map-templates-minimap-tools-parity/05-PARITY.md`. It has one row per feature, with owner M1, M2, M3 or NF (not a feature). This file expands its **M2 rows** into:
- the plan that delivers each row;
- the decision that shapes it;
- the evidence that closes it.

It also records the MFC behaviours M2 deliberately does not copy.

Evidence is filled in by 04-08 and copied into `05-PARITY.md`. File references are relative to `Sources/src/MapEditor/` unless stated. `TEF` = `TemplateEditorFrame1.cpp`.

## Summary of the full inventory by owner (from 05-PARITY.md)

| Area (05-PARITY section) | M1 (done) | M2 (this phase) | M3 (phase 5) | NF |
|---|---|---|---|---|
| 1 File | F2, F3, F6, F7, F9, F11 | – | F1, F4 removal, F5, F8, F10, F12–F15 | – |
| 2 Map menu | M3 | M2 camera anchors, M6 script file | M1, M4, M5, M7–M11 | – |
| 3 Unit menu | – | U1, U2, U3 | – | – |
| 4 Layers | – | – | L1–L15 | L16 |
| 5 View / tools / help | V5, V7, T1, T8 | – | V1–V4, V6, H1, H2, T2–T6 | T7 |
| 6 Edit toolbar | (real undo exceeds MFC) | – | – | E1–E4 |
| 7 Terrain pane | TR1, TR3 | – | TR2, TR4–TR12, TR14–TR18 | TR13, TR19 |
| 8 Objects tab | O1, O5, O7, O8, O9 single, O12 single, O16 single | O16 references, O18 trench drawing, Script ID field (from O17/O19) | O2–O4, O6, O9 squad, O10, O11, O13–O15, O17, O19–O21 | O22 |
| 9 Fences / Bridges / Vector | – | VO1–VO7 | – | – |
| 10 Tools / Groups / AI | – | MT2, G1, AI1 | MT1 | – |
| 11 Minimap | – | – | MM1–MM4 | MM5 |
| 12 RMG composers | – | – | R1–R12, R14, R15 | R13 (built anyway), R16, R17 |
| 13 Dead tabs | – | – | – | D1–D5 |
| 14 Save / load | S3, S5 | S4 bridge spans | S1, S2, S6 scenario tint, S7 | – |

Changes this phase makes to 05-PARITY rows, applied when 04-08 records the evidence:
- **VO3** "Enter toggles destroyed/intact" is really "Enter toggles *built during play*" (`fHP` −1), and only for `WoodenBig_Heavy_*` (`RoadDrawState.cpp:1244-1272`). See D-12.
- **O17/O19** "Script ID" field: M2 delivers the single-selection Script ID field and its command (D-15). M3 keeps the rest of the properties panel.
- **O16** "Delete removes the selection (also from start commands/reserve positions)": M2 delivers the reference cascade (D-04).

## M2 rows, per plan

| 05-PARITY row | MFC feature | MFC source | Plan | Decision | Closes with |
|---|---|---|---|---|---|
| O16 (refs) | Delete also removes the object from start commands and reserve positions | ObjectPlacerState.cpp:627–743 | 04-01 | D-04 | core cascade round trip; map-file cascade vs builder; engine delete-with-references |
| – (bug) | `FindReferences` matches reinforcement groups by link ID; misses entrenchments, reserve positions and `mobileScriptIDs` | `Sources/src/MapFile/MapOverlay.cpp:81-126` | 04-01 | D-04 | map-file tests per reference kind |
| – (M1 gap) | The palette places loose bridge spans, entrenchment pieces and fences | `Sources/editor/app/panels_logic.zig:81-86`, `Sources/src/EditorBridge/catalogue.cpp:113` | 04-01 | D-05 | panels_logic test: none of the three kinds is in the palette |
| S4 (bridges) | Save writes spans into `objects` and their link IDs into `bridges[i]`; ±0.1 nudge; `fHP` ±1 | TEF:3138–3185 | 04-01 / 04-03 | D-01, D-10 | map-file bridge overlay vs builder; idempotent save |
| VO5 | Roads/Rivers: type list, width 1..16, opacity, width modes | TabVOVSODialog.cpp | 04-02 | D-08 | app scenario; core tool test |
| VO6 | Roads/Rivers: add/select/edit states, Insert/Delete points, width handles, opacity right-drag | VectorStripeObjectsState.cpp:116–741 | 04-02 | D-07, D-08 | core tool tests per gesture; engine add/edit/delete road and river vs expected map |
| VO7 | Rivers update AI passability | VectorStripeObjectsState.cpp:1042–1094 | 04-02 | D-09 | engine: passability present after add, gone after delete |
| VO2 | Bridges: list, ghost, axis-locked drag into begin/middle/end spans, all-or-nothing | BridgeSetupDialog.cpp, RoadDrawState.cpp:71–104, 626–667, 963–1031 | 04-03 | D-10 | engine draw bridge vs expected; game-reads-it shot |
| VO3 | Bridges: Enter toggles built during play; Delete removes the span group | RoadDrawState.cpp:1241–1384 | 04-03 | D-11, D-12 | core round trip; engine toggle + delete; game-reads-it loads a built-during-play bridge |
| (M1 deferral) | Rotate a bridge (variant `_01` ↔ `_02`) | RPGStats.h:1313–1314; Data bridge descriptors | 04-03 | D-11 | engine rotate vs expected; refusal test with no partner |
| VO1 | Fences: list, ghost (Ctrl flips), axis-locked drag, direction in frame index | FenceSetupWindow.cpp, RoadDrawState.cpp:519–622, 841–898, 1034–1096 | 04-03 | D-14 | map-file frame index vs builder; engine draw fences |
| VO4, O18 (drawing) | Entrenchments: path drawing, segment/arc/terminator/fireplace build, hover highlight, Delete whole | RoadDrawState.cpp:247–465, 673–1585 | 04-04 | D-13 | map-file trench vs builder (same geometry code); engine draw + delete; game-reads-it `LoadEntrenchments` passes |
| (script ID) | Script ID of the selected object | SEditorMApObject.cpp:54, 227, 303 | 04-05 | D-15 | core round trip; engine `GetObjectScriptID` |
| G1 | Reinforcement groups: list, new (auto ID), delete, hide checked, script IDs per group | GroupManagerDialog.cpp, GetGroupID.cpp, EnterScriptIDDialog.cpp | 04-05 | D-16 | core round trip; map-file groups vs builder; game-reads-it: grouped units are held back |
| M6 | Map script file (`szScriptFile`) | TEF:5102–5118, MapOptionsDialog.cpp | 04-05 | D-20 | map-file; test launch copies the script; game-reads-it: the script runs |
| MT2 | Script areas: rectangle/circle, name, list centres the camera, delete | MapToolState.cpp:188–274, TabToolsDialog.cpp:109–149, AreaNameDialog.cpp | 04-05 | D-21 | map-file AI-unit round trip; duplicate-name refusal; game-reads-it: the script finds the area |
| M2 | Player camera anchors (per player / neutral) | TEF:5035–5051, 3419–3453 | 04-05 | D-22 | map-file; game-reads-it: the camera starts at the anchor |
| U1 | Add start command (properties, click sets the target, red lines) | TEF:3870–3927, AIStartCommand.cpp, ObjectPlacerState.cpp:753–771 | 04-06 | D-17 | core round trip; map-file vs builder; game-reads-it `InitStartCommands` |
| U2 | Start commands list (Delete/Space) | TEF:4051, AIStartCommandsDialog.cpp | 04-06 | D-17 | app scenario |
| U3 | Artillery reserve positions mode | TEF:4122–4175, 2958–2990, ObjectPlacerState.cpp:525–622 | 04-06 | D-18 | core round trip; map-file; game-reads-it `InitReservePositions` |
| AI1 | AI general: sides, mobile script IDs, parcels, reinforce points, type switch | TabAIGeneralDialog.cpp, StateAIGeneral.cpp:153–390 | 04-07 | D-19 | core round trip; map-file vs builder; game-reads-it: SupremeBeing init with the parcel |
| (phase 3 D-12) | Free camera rotation revisit | 03-06-SUMMARY.md | 04-01 (spec text) | D-23 | spec updated; no code |

## MFC save/load behaviours M2 does not copy (by design)

| MFC behaviour | MFC source | Why not | Decision |
|---|---|---|---|
| `HandOutLinks` renumbers every link ID on save | TEF:2956 | Would rewrite untouched records and break the preservation invariant | D-01 |
| Random begin/line/end frame index per bridge span | RoadDrawState.cpp:650–660, 978–990 | Non-deterministic; the saved result could not be tested | D-03 |
| Engine-assigned random `nID` for new roads/rivers | Scene/TerrainEditor.cpp:242–318 | Non-deterministic `nID` in the saved record | D-03 |
| `playersCameraAnchors[0] = vCameraAnchor` after the file is written | TEF:3263–3266 | Side effect on the in-memory map only; the saved file is not affected | D-01 |
| Full-map shade recompute and `CheckMap(false)` fixes on save | TEF:2934–3291 | M3 owns shades (D-19/D-20) and Check Map as a warning (D-33) | D-01 |
| Dropping empty start commands and invalid reserve positions at save | TEF:2958–3038 | Done when the edit happens (the cascade), not silently at save; untouched records stay as read | D-04 |
