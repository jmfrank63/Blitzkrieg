# Audio Backend Scaffold Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split the current FMOD backend into a selectable implementation file while keeping the existing runtime behavior active.

**Architecture:** `AudioBackend.cpp` remains the stable public `NAudioBackend` entry point used by the SFX upper layer. It delegates to internal backend functions declared in `AudioBackendImpl.h`; the first implementation is `AudioBackendFmod.cpp`, guarded by `SFX_USE_FMOD_BACKEND` as the default. A static guard ensures FMOD symbols stay out of the public wrapper and upper layer.

**Tech Stack:** Visual C++ project files, C++03-era SFX code, PowerShell guard scripts, existing FMOD compatibility implementation.

---

### Task 1: Add Backend Implementation Boundary

**Files:**
- Create: `Sources/src/SFX/AudioBackendImpl.h`
- Create: `Sources/src/SFX/AudioBackendFmod.cpp`
- Modify: `Sources/src/SFX/AudioBackend.cpp`
- Modify: `Sources/src/SFX/SFX.vcxproj`
- Create: `tools/audio/check_sfx_backend_scaffold.ps1`

- [ ] **Step 1: Write the failing guard**

Create `tools/audio/check_sfx_backend_scaffold.ps1` so it fails while `AudioBackend.cpp` still contains direct `FSOUND`, `FMOD`, or `AudioFmodCompat` references.

- [ ] **Step 2: Run the guard to verify it fails**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/audio/check_sfx_backend_scaffold.ps1`
Expected: FAIL listing `Sources/src/SFX/AudioBackend.cpp`.

- [ ] **Step 3: Move FMOD implementation**

Move the existing FMOD function bodies from `AudioBackend.cpp` to `AudioBackendFmod.cpp` under internal namespace `NAudioBackendImpl`. Add `AudioBackendImpl.h` with default backend selection and matching declarations.

- [ ] **Step 4: Add delegating public wrapper**

Replace `AudioBackend.cpp` with `NAudioBackend` functions that forward to `NAudioBackendImpl`.

- [ ] **Step 5: Update project**

Add `AudioBackendFmod.cpp` and `AudioBackendImpl.h` to `Sources/src/SFX/SFX.vcxproj`.

- [ ] **Step 6: Verify**

Run all audio guard scripts, `git diff --check`, and Debug Win32 builds for `SFX.vcxproj` and `Game.vcxproj`.

- [ ] **Step 7: Commit**

Commit message: `refactor: scaffold selectable audio backend`
