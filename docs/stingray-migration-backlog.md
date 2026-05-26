# Stingray Migration Implementation Backlog

## Goal
Replace the editor-side Stingray Objective Toolkit dependency in:

- `Sources/src/MapEditor`
- `Sources/src/editor`
- `Sources/src/ELK`

Primary blocker to remove:
- `OTP801a.lib`
- `OTP801as.lib`

This backlog is derived from `docs/stingray-migration-matrix.md` and is intended to be implementation-ready.

---

## Delivery strategy

### Guiding rules
1. Migrate **Objective Toolkit** first.
2. Treat **Objective Grid** as a separate follow-up track.
3. Prefer **modern MFC replacements** before considering larger UI rewrites.
4. Migrate one vertical slice at a time and keep each tool buildable.
5. Remove `secres.*` only after each project no longer depends on Toolkit UI classes.

### Sequencing
1. Shared prep and replacement conventions
2. `MapEditor`
3. `editor`
4. `ELK`
5. Remove Toolkit linkage from projects
6. Reassess Grid and modernization

---

## Workstreams

| ID | Workstream | Purpose |
|---|---|---|
| WS-01 | Shared migration foundation | Establish rules, wrappers, and validation for all three tools |
| WS-02 | MapEditor Objective Toolkit migration | First full replacement track |
| WS-03 | editor Objective Toolkit migration | Reuse `MapEditor` patterns for the main content tool |
| WS-04 | ELK Objective Toolkit migration | Replace specialized tree and frame usage |
| WS-05 | Toolkit removal and project cleanup | Remove `OTP801*`, `secres.*`, and Stingray Toolkit coupling |
| WS-06 | Objective Grid audit | Separate post-Toolkit audit for `GX*` / `og902*` |

---

## WS-01 Shared migration foundation

### ST-01.1 Create replacement mapping table
**Purpose**
Create a short engineering note that maps each `SEC*` class family to its intended replacement.

**Deliverable**
- `docs/stingray-replacement-map.md`

**Initial mappings**
- `SECWorkbook` -> `CMDIFrameWndEx` or `CFrameWndEx`
- `SECFrameWnd` -> `CFrameWndEx`
- `SECStatusBar` -> `CMFCStatusBar`
- `SECControlBar` -> `CDockablePane`
- `SECToolBarManager` -> app-owned `CMFCToolBar`/toolbar registry layer
- `SECCustomToolBar` -> `CMFCToolBar`
- `SECDirSelectDlg` -> `CFolderPickerDialog`
- `SECSplashWnd` -> custom splash dialog/window
- `SECTreeCtrl` -> custom wrapper over `CTreeCtrl` + list/header support
- `SECShortcutBar` -> `CMFCOutlookBar` or custom pane/tab host

**Acceptance criteria**
- Every Toolkit class found in the matrix has a proposed target.
- Unknown or non-trivial replacements are explicitly flagged.

### ST-01.2 Create migration validation checklist
**Purpose**
Make regressions measurable during UI replacement.

**Deliverable**
- `docs/stingray-validation-checklist.md`

**Checklist sections**
- Frame creation
- Toolbar visibility and customization
- Pane docking/undocking
- Status bar updates
- Tree selection behavior
- Dialog layout and resizing
- Recent files / persisted state
- High-DPI smoke test

**Acceptance criteria**
- Checklist can be reused for all three tools.

### ST-01.3 Define shared pane abstraction
**Purpose**
Reduce churn when replacing `SECControlBar` in multiple projects.

**Implementation idea**
Create a local pane base type or conventions note for converting:
- creation
- sizing
- focus forwarding
- visibility toggling
- docking IDs

**Acceptance criteria**
- The pane abstraction is simple enough to apply in `MapEditor`, `editor`, and `ELK`.

---

## WS-02 MapEditor Objective Toolkit migration

### Epic summary
`MapEditor` is the best first migration target because it exposes the broadest frame-level Toolkit dependency surface.

### ST-02.1 Replace splash screen
**Files**
- `Sources/src/MapEditor/editor.cpp`

**Current dependency**
- `SECSplashWnd`

**Target**
- custom splash dialog or bitmap popup window

**Acceptance criteria**
- startup still shows a splash or equivalent lightweight startup feedback
- no `secsplsh.h` dependency remains

### ST-02.2 Replace simple pane: MiniMap
**Files**
- `Sources/src/MapEditor/MiniMapBar.h`
- `Sources/src/MapEditor/MiniMapBar.cpp`

**Current dependency**
- `SECControlBar`

**Target**
- `CDockablePane`

**Acceptance criteria**
- pane docks left/right
- pane resizes correctly
- minimap dialog remains embedded and functional

### ST-02.3 Replace `CInputControlBar`
**Files**
- `Sources/src/MapEditor/MapEditorBarWnd.h`
- `Sources/src/MapEditor/MapEditorBarWnd.cpp`

**Current dependency**
- `SECControlBar`
- relies on `SECBar`/shortcut bar plumbing

**Target**
- `CDockablePane` host with explicit child window management

**Acceptance criteria**
- workspace/input pane opens, docks, resizes, and forwards focus correctly

### ST-02.4 Replace `SECShortcutBar`
**Files**
- `Sources/src/MapEditor/InputNotifyShortcutBar.h`
- `Sources/src/MapEditor/InputNotifyShortcutBar.cpp`

**Current dependency**
- `SECShortcutBar`
- `SECBar`

**Target options**
- `CMFCOutlookBar`
- custom tab/pane host with equivalent notifications

**Acceptance criteria**
- shortcut/page switching works
- dependent child windows still receive page-change notifications

### ST-02.5 Replace frame shell and toolbar infrastructure
**Files**
- `Sources/src/MapEditor/MainFrm.h`
- `Sources/src/MapEditor/MainFrm.cpp`

**Current dependency**
- `SECWorkbook`
- `SECStatusBar`
- `SECToolBarManager`
- `SECMDIMenuBar`
- `SECToolBarsPage`
- `SECToolBarCmdPage`
- `SECToolBarSheet`
- `SECControlBar::GetUniqueBarID`

**Target**
- `CMDIFrameWndEx`
- `CMFCMenuBar`
- `CMFCStatusBar`
- `CMFCToolBar`
- `CMFCToolBarsCustomizeDialog`
- explicit pane/toolbar IDs

**Acceptance criteria**
- main frame starts and hosts the editor child window
- toolbars load and dock
- status bar indicators update
- customize command still works or is deliberately simplified with documented tradeoffs

### ST-02.6 Remove Toolkit resources from MapEditor
**Files**
- `Sources/src/MapEditor/editor.rc`

**Current dependency**
- `secres.h`
- `secres.rc`

**Target**
- local resources only

**Acceptance criteria**
- resource build succeeds without Toolkit resources
- no missing command IDs, dialogs, or bitmaps from Toolkit remain

### Exit criteria for WS-02
- `MapEditor` builds without `OTP801*`
- no `SEC*` Toolkit usage remains in `MapEditor`
- any residual `GX*` usage is separately documented

---

## WS-03 editor Objective Toolkit migration

### ST-03.1 Replace directory picker dialogs
**Files**
- `Sources/src/editor/BatchModeDialog.cpp`
- `Sources/src/editor/SetDirDialog.cpp`

**Current dependency**
- `SECDirSelectDlg`

**Target**
- `CFolderPickerDialog`

**Acceptance criteria**
- both browse flows behave correctly
- no `SECDirSelectDlg` remains in `editor`

### ST-03.2 Replace reference pane: `CPropView`
**Files**
- `Sources/src/editor/COI/PropView.h`
- `Sources/src/editor/COI/PropView.cpp`

**Current dependency**
- `SECControlBar`

**Target**
- `CDockablePane`

**Acceptance criteria**
- property view docks, resizes, and receives focus/messages correctly
- embedded object inspector still resizes with the pane

### ST-03.3 Replace remaining pane family
**Files**
- `Sources/src/editor/TreeDockWnd.*`
- `Sources/src/editor/ThumbListDockBar.*`
- `Sources/src/editor/TemplateTreeDock.*`
- `Sources/src/editor/PropertyDockBar.*`

**Current dependency**
- `SECControlBar`

**Target**
- `CDockablePane`

**Acceptance criteria**
- tree, thumbnail, template, and property panes retain basic docking and sizing behavior

### ST-03.4 Replace main frame and toolbar infrastructure
**Files**
- `Sources/src/editor/MainFrm.h`
- `Sources/src/editor/MainFrm.cpp`
- frame-specific consumers such as:
  - `BuildFrm.cpp`
  - `BridgeFrm.cpp`
  - `ObjectFrm.cpp`
  - `ParticleFrm.cpp`
  - `SquadFrm.cpp`
  - `ChapterFrm.cpp`
  - `ParentFrame.cpp`

**Current dependency**
- `SECWorkbook`
- `SECStatusBar`
- `SECToolBarManager`
- `SECCustomToolBar`

**Target**
- same replacement stack as `MapEditor`

**Acceptance criteria**
- main frame builds and runs
- representative frame/tool windows can show and hide associated panes/toolbars
- toolbar retrieval logic no longer depends on `SECToolBarManager`

### ST-03.5 Remove Toolkit resources from editor
**Files**
- `Sources/src/editor/editor.rc`

**Acceptance criteria**
- `editor.rc` builds without `secres.h` / `secres.rc`

### Exit criteria for WS-03
- `editor` builds without `OTP801*`
- no remaining `SEC*` Toolkit references in `editor`

---

## WS-04 ELK Objective Toolkit migration

### ST-04.1 Replace directory picker dialog
**Files**
- `Sources/src/ELK/ImportFromGameDialog.cpp`

**Current dependency**
- `SECDirSelectDlg`

**Target**
- `CFolderPickerDialog`

### ST-04.2 Replace `TreeDockWindow`
**Files**
- `Sources/src/ELK/TreeDockWindow.h`
- `Sources/src/ELK/TreeDockWindow.cpp`

**Current dependency**
- `SECControlBar`
- `SECTreeCtrl`

**Target**
- `CDockablePane`
- tree wrapper or standard tree control composition

**Acceptance criteria**
- docked tree pane shows, resizes, and updates selection correctly

### ST-04.3 Replace `StatisticDialog` tree/list control
**Files**
- `Sources/src/ELK/StatisticDialog.h`
- `Sources/src/ELK/StatisticDialog.cpp`

**Current dependency**
- `SECTreeCtrl`
- column/list hybrid tree features

**Target**
- custom wrapper around tree/list controls
- or a standard list/tree combination with equivalent column rendering

**Acceptance criteria**
- columns render
- tree hierarchy is navigable
- images/tooltips/selection behavior remain acceptable

### ST-04.4 Replace main frame and toolbar infrastructure
**Files**
- `Sources/src/ELK/MainFrm.h`
- `Sources/src/ELK/MainFrm.cpp`

**Current dependency**
- `SECFrameWnd`
- `SECStatusBar`
- `SECToolBarManager`
- `SECControlBar::GetUniqueBarID`

**Target**
- `CFrameWndEx`
- `CMFCStatusBar`
- `CMFCToolBar`
- explicit pane/toolbar IDs

**Acceptance criteria**
- ELK frame starts cleanly
- toolbar/menu interactions still function
- tree and input panes are hosted without Toolkit

### ST-04.5 Remove Toolkit resources from ELK
**Files**
- `Sources/src/ELK/ELK.rc`

**Acceptance criteria**
- ELK resources build without `secres.h` / `secres.rc`

### Exit criteria for WS-04
- `ELK` builds without `OTP801*`
- no remaining `SEC*` Toolkit references in `ELK`

---

## WS-05 Toolkit removal and cleanup

### ST-05.1 Remove Toolkit linkage from project files
**Targets**
- `Sources/src/editor/editor.vcxproj`
- `Sources/src/MapEditor/MapEditor.vcxproj`
- `Sources/src/ELK/ELK.vcxproj`

**Acceptance criteria**
- no build path attempts to load `OTP801a.lib` / `OTP801as.lib`
- no remaining `secver.h`-driven Toolkit dependency is required by those projects

### ST-05.2 Remove Toolkit runtime artifacts from output assumptions
**Acceptance criteria**
- no post-build or packaging logic expects `otp801a.dll` / `otp801as.dll`

### ST-05.3 Repository-wide verification
**Acceptance criteria**
- repository searches show no remaining Toolkit dependencies in the three migrated projects:
  - `SEC*`
  - `secres.h`
  - `secres.rc`
  - `SECDirSelectDlg`
  - `SECSplashWnd`

---

## WS-06 Objective Grid audit

### ST-06.1 Run repository-wide `GX*` audit
**Purpose**
Validate whether Objective Grid is truly absent from the first migration scope.

**Acceptance criteria**
- produce a list of any `GX*` / `gx*.h` / `og902*` consumers
- decide whether Grid migration is in scope for a later milestone

---

## Milestones

| Milestone | Description | Exit condition |
|---|---|---|
| M1 | Shared migration foundation ready | replacement map, validation checklist, pane conventions done |
| M2 | `MapEditor` off Objective Toolkit | `MapEditor` builds without `OTP801*` |
| M3 | `editor` off Objective Toolkit | `editor` builds without `OTP801*` |
| M4 | `ELK` off Objective Toolkit | `ELK` builds without `OTP801*` |
| M5 | Toolkit removed from all three tools | no Toolkit binaries/resources required by those tools |
| M6 | Grid audit complete | next-scope decision made for `GX*` / `og902*` |

---

## Recommended first sprint

### Sprint 1 candidate scope
1. ST-01.1 Create replacement mapping table
2. ST-01.2 Create validation checklist
3. ST-02.1 Replace `SECSplashWnd` in `MapEditor`
4. ST-03.1 Replace `SECDirSelectDlg` in `editor`
5. ST-04.1 Replace `SECDirSelectDlg` in `ELK`
6. ST-02.2 Prototype `CMiniMapBar` as `CDockablePane`

### Why this sprint
- low-to-medium risk
- removes visible Toolkit surface area early
- establishes patterns needed for larger frame migration
- keeps scope small enough to validate incrementally

---

## Risks to track

| Risk | Impact | Mitigation |
|---|---|---|
| `SECWorkbook` behavior does not map cleanly to MFC frame classes | high | prototype in `MapEditor` first |
| `SECTreeCtrl` feature parity is poor with stock MFC | high | isolate in wrapper before broad conversion |
| toolbar customization behavior regresses | medium | keep a before/after checklist and allow temporary simplification |
| `secres.*` provided hidden commands/resources | medium | remove only after class replacement is complete |
| indirect Objective Grid usage appears later | medium | keep Grid as a separate audit milestone |

---

## Definition of done

The Stingray migration is complete for the targeted tools when:

1. `MapEditor`, `editor`, and `ELK` build without `OTP801*`
2. no `SEC*` Toolkit classes remain in those projects
3. no `secres.h` / `secres.rc` remains in those projects
4. docking, toolbars, tree controls, and dialogs pass the validation checklist
5. remaining `GX*` / Objective Grid scope is either absent or separately planned
