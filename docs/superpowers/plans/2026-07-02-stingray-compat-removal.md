# Stingray Compatibility Removal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove direct project-file coupling to the legacy Stingray Studio 2003 SDK while keeping the existing MFC-based compatibility layer as the open replacement path.

**Architecture:** Treat `Sources/src/Common/StingrayCompat.h` as the local open compatibility boundary for the subset of Stingray `SEC*` APIs used by `ELK`, `editor`, and `MapEditor`. Visual Studio projects must compile through repo-local code and platform/MFC headers, without `Sources/sdk/STINGRAY` include or library directories.

**Tech Stack:** Visual C++ Debug Win32, MFC, existing `StingrayCompat.h`, PowerShell guard scripts.

---

### Task 1: Capture Dependency Guard

**Files:**
- Create: `tools/stingray/check_stingray_project_coupling.ps1`

- [x] Add a guard that fails when Visual Studio project, property, or resource files reference `sdk\\STINGRAY`.
- [x] Cover `Sources/src/**/*.vcxproj`, `Sources/src/**/*.props`, and `Sources/src/**/*.rc`.
- [x] Keep the guard focused on build/resource coupling, not the historical SDK submodule itself.

### Task 2: Remove Project SDK Paths

**Files:**
- Modify: `Sources/src/ELK/ELK.vcxproj`
- Modify: `Sources/src/ELK/ELK.rc`
- Modify: `Sources/src/editor/editor.vcxproj`
- Modify: `Sources/src/editor/editor.rc`
- Modify: `Sources/src/MapEditor/MapEditor.vcxproj`
- Modify: `Sources/src/MapEditor/editor.rc`

- [x] Remove Stingray include directories from C/C++ and resource compiler settings.
- [x] Remove Stingray library directories from linker settings.
- [x] Remove dead `secres.h` and `secres.rc` resource includes.
- [x] Preserve all non-Stingray include/library directories and inherited macros.

### Task 3: Verify Compatibility Boundary

**Files:**
- Existing: `Sources/src/Common/StingrayCompat.h`

- [x] Build `ELK.vcxproj` in `Debug | Win32`.
- [x] Build `editor.vcxproj` in `Debug | Win32`.
- [x] Build `MapEditor.vcxproj` in `Debug | Win32`.
- [x] Patch `StingrayCompat.h` only for missing `SEC*` compatibility symbols reported by the compiler.

### Task 4: Final Validation

**Files:**
- All files changed by Tasks 1-3.

- [x] Run `tools/stingray/check_stingray_project_coupling.ps1`.
- [x] Run `git diff --check`.
- [x] Confirm no project file still requires `Sources/sdk/STINGRAY`.
