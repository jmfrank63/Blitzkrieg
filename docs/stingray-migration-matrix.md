# Stingray Migration Dependency Matrix

## Scope
This matrix covers the editor-side MFC tools that currently depend on Stingray in this workspace:

- `Sources/src/editor`
- `Sources/src/MapEditor`
- `Sources/src/ELK`

The runtime/game modules are intentionally excluded.

## Stingray component split

| Component | Typical symbols | Legacy library family | Notes |
|---|---|---|---|
| Objective Toolkit | `SEC*`, `sec*.h`, `secres.*` | `OTP801*.lib` | Current hard blocker. Missing from local SDK. |
| Objective Grid | `GX*`, `gx*.h` | `og902*.lib` | Separate migration track. No direct usage surfaced in the inventoried tool projects. |
| Foundation | `sfl*` headers/libs | `sfl202*.lib` | Still present in SDK, but not a replacement for Objective Toolkit. |

## Executive summary

- All three tool projects depend heavily on **Objective Toolkit**.
- The main dependency surface is not only classes, but also:
  - resource includes: `secres.h`, `secres.rc`
  - auto-link headers: `secwb.h`, `secsplsh.h`, related Toolkit headers
  - toolbar customization pages and docking infrastructure
- In the inventoried tool projects, **Objective Grid usage was not found** with the current search set.
- Recommended migration order:
  1. `MapEditor`
  2. `editor`
  3. `ELK`

---

## Project matrix

### 1. `editor`

#### Dependency summary

| Area | Stingray usage | Example files | Migration target |
|---|---|---|---|
| Main frame/workbook shell | `SECWorkbook`, `SECStatusBar`, `SECToolBarManager`, `SECCustomToolBar`, `SECControlBar` | `Sources/src/editor/MainFrm.h` | `CMDIFrameWndEx` or `CFrameWndEx`, `CMFCStatusBar`, `CMFCToolBar`, `CDockablePane` |
| Docked panes/control bars | `SECControlBar` | `COI/PropView.h`, `TreeDockWnd.h`, `ThumbListDockBar.h`, `TemplateTreeDock.h`, `PropertyDockBar.h` | `CDockablePane` or pane-host wrapper |
| Toolbar manager/customization | `SECToolBarManager`, `SECCustomToolBar` | `BuildFrm.cpp`, `BridgeFrm.cpp`, `ObjectFrm.cpp`, `ParticleFrm.cpp`, `SquadFrm.cpp`, `ChapterFrm.cpp` | `CMFCToolBar`, `CMFCToolBarsCustomizeDialog`, app-owned toolbar registry |
| Directory picker dialogs | `SECDirSelectDlg` | `BatchModeDialog.cpp`, `SetDirDialog.cpp` | `CFolderPickerDialog` or shell-based wrapper |
| Toolkit resources | `secres.h`, `secres.rc` | `editor.rc` | Local resources or modern MFC resources |

#### Key hotspots

| File | Current dependency | Why it matters |
|---|---|---|
| `Sources/src/editor/MainFrm.h` | `SECWorkbook`, `SECStatusBar`, `SECToolBarManager`, `SECCustomToolBar`, `SECControlBar` | Central frame and toolbar architecture for the whole tool |
| `Sources/src/editor/COI/PropView.h` | `SECControlBar` | Representative docked-pane migration template |
| `Sources/src/editor/COI/PropView.cpp` | `SECControlBar` lifecycle and sizing | Good first pane conversion candidate |
| `Sources/src/editor/TreeDockWnd.*` | `SECControlBar` | Tree dock infrastructure likely reused across modes |
| `Sources/src/editor/ThumbListDockBar.*` | `SECControlBar` | Thumbnail pane migration pattern |
| `Sources/src/editor/TemplateTreeDock.*` | `SECControlBar` | Additional pane pattern |
| `Sources/src/editor/PropertyDockBar.*` | `SECControlBar` | Secondary pane pattern |
| `Sources/src/editor/editor.rc` | `secres.h`, `secres.rc` | Resource-level Stingray dependency that must be removed |

#### Suggested migration wave for `editor`

1. Replace `SECWorkbook`-based main frame shell
2. Introduce a pane abstraction to replace `SECControlBar`
3. Convert `PropView` as the reference pane
4. Convert tree/property/thumb/template dock panes
5. Replace toolbar manager/customization code
6. Remove `SECDirSelectDlg`
7. Remove `secres.*` from resources

---

### 2. `MapEditor`

#### Dependency summary

| Area | Stingray usage | Example files | Migration target |
|---|---|---|---|
| Main frame/workbook shell | `SECWorkbook`, `SECStatusBar`, `SECToolBarManager`, `SECMDIMenuBar`, `SECControlBar` | `Sources/src/MapEditor/MainFrm.h`, `MainFrm.cpp` | `CMDIFrameWndEx`, `CMFCMenuBar`, `CMFCStatusBar`, `CMFCToolBar`, `CDockablePane` |
| Toolbar customization pages | `SECToolBarsPage`, `SECToolBarCmdPage`, `SECToolBarSheet` | `MainFrm.cpp` | `CMFCToolBarsCustomizeDialog` or custom dialog |
| Splash screen | `SECSplashWnd` | `editor.cpp` | Simple startup dialog, bitmap window, or custom splash wrapper |
| Docked panes/control bars | `SECControlBar` | `MapEditorBarWnd.h`, `MiniMapBar.h` | `CDockablePane` |
| Shortcut bar / Outlook-style navigation | `SECShortcutBar`, `SECBar` | `InputNotifyShortcutBar.h`, `MapEditorBarWnd.h` | custom tab/pane host or `CMFCOutlookBar`-style replacement |
| Toolkit resources | `secres.h`, `secres.rc` | `editor.rc` | Local resources or modern MFC resources |

#### Key hotspots

| File | Current dependency | Why it matters |
|---|---|---|
| `Sources/src/MapEditor/MainFrm.h` | `SECWorkbook`, `SECStatusBar`, `SECToolBarManager`, `SECControlBar` | Primary frame dependency hub |
| `Sources/src/MapEditor/MainFrm.cpp` | toolbar definitions, menu bar, docking, customization pages | Best file to establish the replacement architecture |
| `Sources/src/MapEditor/editor.cpp` | `SECSplashWnd`, `ShowSECControlBar` helper | App startup and common control-bar toggling |
| `Sources/src/MapEditor/editor.rc` | `secres.h`, `secres.rc` | Resource dependency removal |
| `Sources/src/MapEditor/InputNotifyShortcutBar.h` | `SECShortcutBar` | Specialized navigation control that likely needs a custom wrapper |
| `Sources/src/MapEditor/MapEditorBarWnd.h` | `SECControlBar`, `SECBar` | Compound pane host with shortcut bar integration |
| `Sources/src/MapEditor/MiniMapBar.h` | `SECControlBar` | Simple pane candidate for early migration |

#### Suggested migration wave for `MapEditor`

1. Replace the main frame (`SECWorkbook`, menu/status/toolbars)
2. Replace toolbar customization dialog path
3. Convert `MiniMapBar` and one simple `SECControlBar` pane
4. Convert `MapEditorBarWnd` to a pane host
5. Replace `SECShortcutBar` behavior in `InputNotifyShortcutBar`
6. Replace splash screen
7. Remove `secres.*` from resources

#### Why `MapEditor` should go first

- It has the clearest top-level frame architecture.
- It gives a full sample of workbook, menu bar, status bar, docking, toolbar customization, splash, and pane usage in one project.
- The migration patterns established here can be reused by `editor` and `ELK`.

---

### 3. `ELK`

#### Dependency summary

| Area | Stingray usage | Example files | Migration target |
|---|---|---|---|
| Main frame shell | `SECFrameWnd`, `SECStatusBar`, `SECToolBarManager`, `SECControlBar` | `Sources/src/ELK/MainFrm.h`, `MainFrm.cpp` | `CFrameWndEx`, `CMFCStatusBar`, `CMFCToolBar`, `CDockablePane` |
| Docked tree pane | `SECControlBar`, `SECTreeCtrl` | `TreeDockWindow.h`, `TreeDockWindow.cpp` | `CDockablePane` + `CTreeCtrl`/custom tree-list |
| Tree/list dialog | `SECTreeCtrl` | `StatisticDialog.h`, `StatisticDialog.cpp` | custom tree-list wrapper or modern tree/list composition |
| Directory picker dialog | `SECDirSelectDlg` | `ImportFromGameDialog.cpp` | `CFolderPickerDialog` |
| Toolbar manager/customization | `SECToolBarManager` | `MainFrm.cpp` | MFC toolbar infrastructure |
| Toolkit resources | `secres.h`, `secres.rc` | `ELK.rc` | Local resources or modern MFC resources |

#### Key hotspots

| File | Current dependency | Why it matters |
|---|---|---|
| `Sources/src/ELK/MainFrm.h` | `SECFrameWnd`, `SECStatusBar` | Main frame starting point |
| `Sources/src/ELK/MainFrm.cpp` | `SECToolBarManager`, unique bar IDs, toolbar customization | Core frame/tooling behavior |
| `Sources/src/ELK/TreeDockWindow.*` | `SECControlBar`, `SECTreeCtrl` | Primary docked navigation surface |
| `Sources/src/ELK/StatisticDialog.h` | `SECTreeCtrl` | Specialized tree/list UI hotspot |
| `Sources/src/ELK/StatisticDialog.cpp` | `SECTreeCtrl` APIs for columns/styles | Will need a custom replacement, not a trivial rename |
| `Sources/src/ELK/ImportFromGameDialog.cpp` | `SECDirSelectDlg` | Easy standalone migration task |
| `Sources/src/ELK/ELK.rc` | `secres.h`, `secres.rc` | Resource dependency removal |

#### Suggested migration wave for `ELK`

1. Replace main frame shell and toolbar manager
2. Convert `TreeDockWindow` to a standard pane
3. Replace `SECTreeCtrl` in `StatisticDialog`
4. Replace `SECDirSelectDlg`
5. Remove `secres.*` from resources

---

## Cross-project dependency patterns

| Pattern | Seen in | Replacement approach |
|---|---|---|
| `SECWorkbook` / `SECFrameWnd` main frames | `editor`, `MapEditor`, `ELK` | Replace with `CMDIFrameWndEx` / `CFrameWndEx` first |
| `SECControlBar`-derived panes | all three | Standardize on `CDockablePane` or a local pane base class |
| `SECToolBarManager` and `SECCustomToolBar` | `editor`, `MapEditor`, `ELK` | Central toolbar migration layer needed |
| `SECStatusBar` | `editor`, `MapEditor`, `ELK` | Swap to `CMFCStatusBar` or `CStatusBar` |
| `SECDirSelectDlg` | `editor`, `ELK` | Low-risk early win with `CFolderPickerDialog` |
| `SECSplashWnd` | `MapEditor` | Low-risk early replacement |
| `SECTreeCtrl` | `ELK` | Medium/high complexity; likely custom wrapper required |
| `SECShortcutBar` | `MapEditor` | Medium/high complexity; decide replacement architecture early |
| `secres.h` / `secres.rc` | all three | Must be removed before final Stingray detachment |

---

## Objective Grid status

Searches in the inventoried tool projects did **not** surface direct `GX*` or `gx*.h` usage. That suggests:

- Objective Grid is not part of the first migration wave for these three tools, or
- usage is indirect and would need a broader second-pass inventory outside the current file set.

Recommendation:
- finish Objective Toolkit inventory and migration first
- then run a wider repository-level `GX*` / `og902*` audit before touching Grid-related libs

---

## Proposed migration order

### Phase 0: preparation
- Capture screenshots and workflows for current docking, toolbar customization, pane visibility, and tree interactions.
- Create a small compatibility note for each Stingray class family found.

### Phase 1: MapEditor frame platform
- Replace frame shell, menu bar, status bar, toolbar manager, docking model.
- Remove splash and resource dependencies.

### Phase 2: editor pane platform
- Reuse pane abstractions from `MapEditor`.
- Convert `SECControlBar`-derived panes and toolbar manager usage.

### Phase 3: ELK specialized tree UX
- Convert frame shell.
- Replace `SECTreeCtrl`-dependent views/dialogs.

### Phase 4: Stingray removal
- Remove `secver.h`-driven `OTP801*` linkage.
- Remove `secres.*` dependencies from all three tools.
- Validate the three tools build without Objective Toolkit binaries.

### Phase 5: follow-up modernization
- Reassess toolset, language standard, DPI manifests, dark mode, and warning levels after the UI migration stabilizes.

---

## Suggested first implementation tasks

1. `MapEditor`: replace `SECSplashWnd` in `editor.cpp`
2. `editor` and `ELK`: replace `SECDirSelectDlg` call sites
3. `MapEditor`: prototype `CDockablePane` replacement for `CMiniMapBar`
4. `editor`: prototype `CDockablePane` replacement for `CPropView`
5. `ELK`: design replacement wrapper for `SECTreeCtrl`

These tasks reduce Stingray surface area before the heavier frame-shell conversions.
