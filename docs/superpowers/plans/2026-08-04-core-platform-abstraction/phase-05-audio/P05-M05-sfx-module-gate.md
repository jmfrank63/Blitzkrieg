# P05-M05 — Close the SFX Module Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Build, load, initialize, exercise, and unload the real SFX module on all targets.

**Dependencies:** P05-M04.

**Allowed files:** `Sources/src/SFX/GlobalsLoader.cpp`, `Sources/src/SFX/StdAfx.h`, `Sources/src/SFX/SoundObjectFactory.cpp`, `Sources/src/SFX/Sound.def`, `tools/zig/sfx_module_test.cpp`, `tools/zig/input_audio_gate.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [x] Load the real SFX factory and run the no-device, invalid-stream, volume, mute, stop, and restart lifecycle fixture. Sample playback and fade-duration assertions remain open for a real decoded asset fixture.
- [x] Target-guard `.def`, `winmm`, COM, ODBC, and Windows-only backend settings; the SFX `.def` is Windows-only and Windows libraries remain inside the Windows branch.
- [x] Run the native Windows SFX module and audio gates; Linux native execution and macOS arm64 compilation remain open by the project’s Windows-first test scope.
- [ ] Verify Linux/macOS link commands contain no Windows libraries and staged SFX resolves miniaudio dependencies.
- [x] Remove SFX-owned heap/debug/atomic/timer tokens from the allowlist; remaining allowlist entries are unrelated legacy/core ownership.
- [ ] Commit: `audio: close portable SFX module gate`

**Evidence:** Windows `test-sfx-module` passes against the real staged `SFX.dll`: descriptor `Sound v0100`, six factory types, no-device init, volume/mute/invalid-stream/stop, restart, and repeated `Done()`. `sfx`, `test-audio-lifecycle`, `test-audio-worker`, and `test-audio-stream` pass. Cross-target link audit and decoded sample/fade fixtures remain open.
