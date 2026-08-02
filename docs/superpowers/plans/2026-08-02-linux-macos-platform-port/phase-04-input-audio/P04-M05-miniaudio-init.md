# P04-M05 — Make miniaudio Initialization Platform-Neutral

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Initialize the existing open audio backend without a Win32 window, DirectSound selection, or Windows diagnostics.

**Dependencies:** P03-M05, P04-M02.

**Allowed files:** `Sources/src/SFX/SFX.h`, `Sources/src/SFX/AudioBackend.h`, `Sources/src/SFX/AudioBackend.cpp`, `Sources/src/SFX/AudioBackendOpen.cpp`, `Sources/src/SFX/AudioBackendImpl.h`, `Sources/src/SFX/SoundEngine.h`, `Sources/src/SFX/SoundEngine.cpp`, `Sources/src/Main/iMain.h`, `Sources/src/Main/Initialization.cpp`, `Sources/src/Game/GameMain.cpp`, `tools/zig/platform_audio_test.cpp`, `build.zig`.

- [ ] Add null-device and real-device tests for context init, device enumeration, selected device, 44.1/48 kHz conversion, stereo playback, stop, uninit, and repeated no-device recovery.
- [ ] Remove unconditional `contextConfig.dsound.hWnd`; let miniaudio select the native backend unless an existing user option selects a named device.
- [ ] Remove `HWND` from `ISFX::Init`, `CSoundEngine::Init`, and all audio-backend `InitDevice` declarations/definitions; pass no application window to miniaudio.
- [ ] Collapse `NMain::Initialize(HWND, HWND, HWND, bool)` to one borrowed `GFXNativeWindow` plus the game-mode flag, update `GameMain`, and request the portable automatic audio backend instead of `SFX_OUTPUT_DSOUND`.
- [ ] Route high-resolution audio timing through `Clock` and all backend/engine diagnostics through `DebugWrite`.
- [ ] Route diagnostics through `NPlatform::DebugWrite` and record backend/device once at startup.
- [ ] Keep Xiph/Vorbis decode and mixer ownership unchanged; introduce no SDL audio path.
- [ ] Run the null backend in CI and a native playback smoke on Windows/Linux.
- [ ] Commit: `audio: initialize miniaudio portably`

**Evidence:** backend/device startup line and clean repeated shutdown.
