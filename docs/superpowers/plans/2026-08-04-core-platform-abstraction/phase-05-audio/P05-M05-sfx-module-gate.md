# P05-M05 — Close the SFX Module Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Build, load, initialize, exercise, and unload the real SFX module on all targets.

**Dependencies:** P05-M04.

**Allowed files:** `Sources/src/SFX/GlobalsLoader.cpp`, `Sources/src/SFX/StdAfx.h`, `Sources/src/SFX/SoundObjectFactory.cpp`, `Sources/src/SFX/Sound.def`, `tools/zig/sfx_module_test.cpp`, `tools/zig/input_audio_gate.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Load the real SFX factory and run sample, stream, fade, volume, mute, no-device, and restart fixtures.
- [ ] Target-guard `.def`, `winmm`, COM, ODBC, and Windows-only backend settings.
- [ ] Run native Windows/Linux audio gates and compile macOS arm64.
- [ ] Verify Linux/macOS link commands contain no Windows libraries and staged SFX resolves miniaudio dependencies.
- [ ] Remove SFX-owned heap/debug/atomic/timer tokens from the allowlist.
- [ ] Commit: `audio: close portable SFX module gate`

**Evidence:** module lifecycle trace, audio fixture hashes, and link audit.
