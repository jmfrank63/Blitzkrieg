# Phase 4 Research: Runtime Dialog Enlargement (Stability + Compatibility)

Date: 2026-06-07
Scope: Research only, implementation deferred to planning/execution.

## 1) Problem Framing For Dialog-Size Mismatch

The current runtime already includes selective legacy layout scaling at the screen level, but only for a narrow allowlist of UI resources. In practice, text and some UI elements have been enlarged in prior work, while many popup/dialog layouts still rely on original geometry (legacy authoring dimensions), causing clipping, cramped controls, and misaligned hit areas in some flows.

Why this matters for Phase 4:
- Stability risk: Any broad scaling change can alter child bounds, clipping rectangles, and text wrapping behavior across many UI trees.
- Compatibility risk: Runtime data assets (UI XML and textures) are loaded from data paths not fully tracked in this repo, so code-only changes must tolerate existing legacy content.
- User-facing impact: Popups and mission/intermission dialogs are heavily used in tutorial and progression flows; mismatch here directly affects core gameplay UX.

Concrete existing behavior proving the mismatch surface:
- Selective scaling is enabled only when `CUIScreen::Load` marks `bScaleLayoutToScreen` via `ShouldScaleLegacyLayout(...)` in `Sources/src/UI/UIScreen.cpp`.
- The current allowlist includes only:
  - `ui\mainmenu`
  - `ui\OptionsSettings`
  - `ui\MissionOptionsSettings`
  - `ui\Popup\IMTutorial`
  - `ui\Lists\IMTutorialList`
- Dialog-heavy resources outside that list load at legacy dimensions (for example popup/message flows in `Sources/src/GameTT/*.cpp`).

## 2) Current Architecture Map (Where Dialog Dimensions Are Defined/Applied)

### Runtime ownership layers

1. Screen-level orchestration
- `CUIScreen::Load` and `CUIScreen::Reposition` in `Sources/src/UI/UIScreen.cpp`.
- `Load` sets whether legacy layout scaling is active for the resource (`bScaleLayoutToScreen`).
- `Reposition` computes `vNewScale = screen / (1024x768)` and applies delta scaling once via `ScaleLayout(vDeltaScale)`.

2. Window geometry + recursive scaling
- `CSimpleWindow::ScaleLayout` in `Sources/src/UI/UIBasic.cpp` scales:
  - `vPos`, `vSize`
  - text offsets (`vShiftText`, `vTextPos`, `vShadowShift`)
  - optional bounds (`rcBound`)
  - `subRects[*].rc` for each state
  - `IGFXText` width and scale
- `CMultipleWindow::ScaleLayout` recursively scales children (`childList`).
- `CSimpleWindow::Reposition` recalculates absolute screen rect from `vPos`/`vSize` and placement flags.

3. Text layout/render coupling
- `CGFXText` in `Sources/src/GFX/Text.cpp` uses width and scale for wrapping (`SetWidth`, `SetScale`, `PreFormat`).
- `CGraphicsEngine::DrawText` in `Sources/src/GFX/GraphicsEngine.cpp` re-applies text width each draw (`pTxt->SetWidth(rect.right - rect.left)`), so dialog rect changes directly affect wrapping.

4. Dialog resource loading entry points
- Generic UI message box path:
  - `CUIScreen::MessageBox` loads `UI\OkMessageBox.xml` in `Sources/src/UI/UIScreen.cpp`.
  - `CUIMessageBox` behavior in `Sources/src/UI/UIMessageBox.cpp`.
- Game/intermission popup loaders (sample set):
  - `CInterfaceMessageBox::Create` -> `ui\Popup\MessageBox` / `ui\Popup\MessageBoxMission` (`Sources/src/GameTT/InterfaceMessageBox.cpp`)
  - `CInterfaceIMTutorial::StartInterface` -> `ui\Popup\IMTutorial` (`Sources/src/GameTT/IMTutorial.cpp`)
  - `CInterfaceNewDepotUpgrades::StartInterface` -> `ui\Popup\NewDepotUpgrades` (`Sources/src/GameTT/InterfaceNewDepotUpgrades.cpp`)
  - `CInterfaceSaveReplay::StartInterface` -> `ui\Popup\SaveReplay` (`Sources/src/GameTT/SaveReplay.cpp`)
  - `CInterfaceNextChapter` flows -> `ui\Popup\NextChapter*` (`Sources/src/GameTT/SwitchToNextChapter.cpp`)
  - `CInterfaceSwitchModeTo` flows -> `ui\Popup\DontHaveMod`, `ui\Popup\SwitchModTo` (`Sources/src/GameTT/InterfaceSwitchModeTo.cpp`)

5. Authoring/tooling constraints
- ELK font generation defaults in `Sources/src/ELK/ELK_Consts.cpp` (`CFontGen::FONTS_SIZE = {8,16,24,48}`), and generation range guards in `Sources/src/ELK/ELK_StaticMethods.cpp`.
- This means runtime dialog enlargement should remain decoupled from font asset regeneration for this phase (to reduce compatibility risk).

### Key symbols to reference in planning tasks

- `ShouldScaleLegacyLayout` (`Sources/src/UI/UIScreen.cpp`)
- `CUIScreen::Load`, `CUIScreen::Reposition` (`Sources/src/UI/UIScreen.cpp`)
- `CSimpleWindow::ScaleLayout`, `CMultipleWindow::ScaleLayout`, `CSimpleWindow::Reposition` (`Sources/src/UI/UIBasic.cpp`)
- `CGFXText::SetWidth`, `CGFXText::SetScale`, `CGFXText::PreFormat` (`Sources/src/GFX/Text.cpp`)
- `CGraphicsEngine::DrawText` (`Sources/src/GFX/GraphicsEngine.cpp`)

## 3) Candidate Approaches (Centralized Scaling vs Targeted Per-Dialog Updates)

### Approach A: Centralized runtime scaling policy (recommended baseline)

What it is:
- Expand the policy gate in `ShouldScaleLegacyLayout(...)` so additional dialog-heavy resources use the existing `CUIScreen` scaling pipeline.
- Keep geometry scaling centralized in existing `ScaleLayout` recursion.

Pros:
- Reuses proven mechanism already active for `mainmenu/options/tutorial`.
- Lowest code-surface change for Phase 4.
- Easy rollback: policy-only edits.

Cons:
- Coarse-grained; some dialogs may need per-resource multipliers/opt-outs.
- If scaling is over-applied, some background sub-rect compositions may stretch in undesirable ways.

Best fit for this phase:
- High. It aligns with runtime stability goal and avoids immediate data migration.

### Approach B: Targeted per-dialog runtime overrides

What it is:
- Keep current global policy narrow.
- Add explicit per-dialog runtime adjustments after load (for specific child IDs or containers), e.g., `SetWindowPlacement`/`SetSize` for identified dialogs.

Pros:
- Fine control and safer for problematic dialogs.
- Can patch high-priority flows without touching broad behavior.

Cons:
- Higher maintenance: each dialog needs dedicated code path and IDs.
- Harder to ensure complete coverage as popup list evolves.

Best fit for this phase:
- Medium as a fallback mechanism when centralized scaling causes regressions.

### Approach C: Data-only per-dialog XML resize (asset updates)

What it is:
- Update UI XML dimensions directly per dialog resource.

Pros:
- No runtime branching complexity once assets are fixed.

Cons:
- UI XML assets are not discoverable in this repo snapshot (`**/ui/**/*.xml` not present), likely due data packaging/ignore.
- Requires external data workflow and stronger compatibility coordination.

Best fit for this phase:
- Low for initial Phase 4 stabilization; suitable as a follow-up once runtime policy is validated.

## 4) Risks And Compatibility Constraints

1. Asset availability gap
- Risk: dialog XML content is not visible in repository workspace, so complete static verification of all dialog dimensions is impossible from code alone.
- Constraint: plan must include runtime/manual validation against actual installed data assets.

2. Over-scaling side effects in composed windows
- Risk: `CSimpleWindow::ScaleLayout` scales sub-rects/maps-driven windows and text offsets; some UI skins with tile assumptions can distort.
- Constraint: staged rollout by resource allowlist, not global unconditional scaling.

3. Text layout and clipping coupling
- Risk: text wrap is tightly coupled to runtime rect width (`DrawText` -> `SetWidth`), so scaling changes alter line breaks and possibly button overlap.
- Constraint: validate caption/body/button text in each targeted popup/resource.

4. Numeric drift with repeated scale transitions
- Risk: scaling applies multiplicative deltas over stored values; repeated resolution changes can introduce floating-point drift.
- Constraint: keep scaling path deterministic and test repeated resolution toggles in-session.

5. Input/hitbox compatibility
- Risk: if geometry and visual alignment diverge, click targets can shift.
- Constraint: verify `OnLButtonDown`/`IsInside` behavior on scaled dialogs in manual checks.

6. Existing logic defects discovered (non-blocking but relevant)
- In `CUIMessageBox::SetMessageBoxType` (`Sources/src/UI/UIMessageBox.cpp`), `OKCANCEL` and `YESNO` currently set text on `pOK` twice instead of configuring `pCancel`.
- Not a size bug, but worth tracking as it affects popup correctness and can confuse validation outcomes.

## 5) Recommended Approach For This Phase

Primary recommendation:
- Use a hybrid strategy: centralized scaling policy expansion first, with targeted per-dialog overrides only for regressions.

Execution guidance for planner:

1. Build an explicit dialog resource audit list
- Source from `pUIScreen->Load("ui\\...")` callsites in `Sources/src/GameTT` and `Sources/src/UI/UIScreen.cpp`.
- Prioritize `ui\Popup\*` and dialog-like `ui\Lists\*` resources in mission/intermission paths.

2. Expand `ShouldScaleLegacyLayout(...)` safely
- Add high-priority popup resources to allowlist in small batches.
- Keep exact-string matching at first (avoid broad wildcard behavior in phase 4).

3. Add optional per-resource guard path
- For known problematic resources, permit opt-out or additional override multiplier.
- Keep this localized to `UIScreen` policy logic (avoid scattering adjustments across many interfaces).

4. Preserve runtime compatibility contracts
- Do not modify serialization shape of UI structures (`operator&(IDataTree)` forms in `UIBasic`/`UIScreen`).
- Do not require ELK/font regeneration in phase 4 baseline.

5. Capture unresolved data dependency explicitly
- Since popup XML files are not present in the workspace, include a planning task to validate against real runtime data pack and log per-resource outcomes.

## 6) Verification Checklist (Manual + Feasible Automated Checks)

### Automated (feasible in current repo)

- Build safety:
  - `Build Game (Debug)` task passes.
- Static loader audit:
  - Grep all `pUIScreen->Load("ui\\...")` callsites in `Sources/src/GameTT/**`.
  - Produce tracked list of popup/dialog resources targeted by scaling policy.
- Policy coverage check:
  - Compare resource audit list against `ShouldScaleLegacyLayout` allowlist and flag gaps intentionally deferred.

### Manual runtime checks (required)

Core dialog flows:
- Intermission tutorial popup (`ui\Popup\IMTutorial`) opens, text wraps without clipping, close/OK works.
- Message boxes:
  - Mission and non-mission (`ui\Popup\MessageBoxMission`, `ui\Popup\MessageBox`) show caption/body/buttons fully.
- Save replay popup (`ui\Popup\SaveReplay`) edit box and buttons remain aligned and clickable.
- Next chapter popup(s) (`ui\Popup\NextChapter*`) content and controls remain visible at target resolutions.
- Mod switch popups (`ui\Popup\DontHaveMod`, `ui\Popup\SwitchModTo`) remain stable.

Interaction stability:
- Verify click hitboxes align with visuals for all scaled dialogs.
- Verify keyboard input and focus paths for edit-box dialogs (enter/escape/tab where applicable).

Resolution regression sweep:
- At minimum: `1024x768` (legacy baseline), one modern 16:9 resolution, one high resolution.
- In one session, switch resolutions multiple times and revisit one popup to detect scaling drift.

Compatibility checks:
- Confirm no crash/assert in `UIBasic`/`UIScreen` scaling and reposition paths during popup open/close cycles.
- Confirm no obvious text rendering artifacts from increased scale (blurry threshold behavior in `DrawText` linear filtering path).

## Notes For Planner Task Breakdown

Suggested task grouping:
- Task group A: Resource inventory + policy mapping.
- Task group B: Incremental `ShouldScaleLegacyLayout` expansion with guarded rollout.
- Task group C: Targeted overrides for regressions.
- Task group D: Runtime validation checklist execution and evidence capture.

Known external dependency:
- UI XML assets are not present in this workspace snapshot; final verification depends on runtime data package used by the game executable.

## RESEARCH COMPLETE