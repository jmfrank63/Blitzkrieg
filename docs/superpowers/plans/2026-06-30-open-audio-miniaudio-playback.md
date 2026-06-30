# Open Audio Miniaudio Playback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the opt-in open audio backend play basic WAV samples while keeping FMOD as the default fallback backend.

**Architecture:** Vendor single-header miniaudio under `Sources/sdk/miniaudio` and use it only from `AudioBackendOpen.cpp`. Keep the current `NAudioBackend` API unchanged, route normal builds to FMOD, and compile/test the open backend with `/p:AudioBackend=Open`.

**Tech Stack:** Visual C++ Debug Win32, miniaudio single-header backend, existing SFX backend switch, PowerShell guard scripts.

---

### Task 1: Vendor miniaudio

**Files:**
- Create: `Sources/sdk/miniaudio/miniaudio.h`
- Modify: `Sources/src/SFX/SFX.vcxproj`
- Modify: `Sources/src/SFX/SFX.vcxproj.filters`
- Create: `tools/audio/check_sfx_open_backend_miniaudio.ps1`

- [ ] Add a guard that requires `Sources/sdk/miniaudio/miniaudio.h` and miniaudio usage in `AudioBackendOpen.cpp`.
- [ ] Download the official miniaudio single header.
- [ ] Add the include path only for the SFX project.
- [ ] Verify the guard fails before the integration and passes afterward.

### Task 2: Open backend device and channel playback

**Files:**
- Modify: `Sources/src/SFX/AudioBackendOpen.cpp`
- Modify: `docs/audio/fmod-replacement-inventory.md`
- Modify: `.planning/threads/audio-replacment.md`

- [ ] Initialize a `ma_engine` in `InitDevice` and uninitialize it in `CloseDevice`.
- [ ] Store loaded WAV PCM data in a miniaudio-compatible format.
- [ ] Add a bounded channel table for `PlaySample`, `PlaySamplePaused`, pause, stop, volume, pan, position, and `IsChannelPlaying`.
- [ ] Keep looping and 3D calls safe even if initially simple.
- [ ] Build and guard-test `/p:AudioBackend=Open`, then force-rebuild `/p:AudioBackend=FMOD`.

### Task 3: Commit safe checkpoint

**Files:**
- All files changed by Tasks 1-2.

- [ ] Run all audio guard scripts.
- [ ] Run `git diff --check`.
- [ ] Build `SFX.vcxproj` with `/p:AudioBackend=Open`.
- [ ] Force rebuild `SFX.vcxproj` with `/p:AudioBackend=FMOD`.
- [ ] Build `Game.vcxproj`.
- [ ] Commit with `feat: play wav samples in open audio backend`.
