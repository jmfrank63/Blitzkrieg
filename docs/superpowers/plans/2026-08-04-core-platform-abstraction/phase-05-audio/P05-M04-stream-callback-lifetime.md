# P05-M04 — Stream and Callback Lifetime

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove decoded-stream ownership and callback safety across stop, seek, EOF, and shutdown.

**Dependencies:** P05-M03.

**Allowed files:** `Sources/src/SFX/AudioBackendOpen.cpp`, `Sources/src/SFX/AudioBackendXiphVorbis.h`, `Sources/src/SFX/AudioBackendXiphVorbis.c`, `Sources/src/SFX/StreamingSound.cpp`, `tools/zig/audio_stream_test.cpp`, `build.zig`.

- [ ] Add deterministic PCM/Vorbis fixtures for read, seek, loop, EOF, short read, stop during callback, and source destruction.
- [ ] Separate callback-owned state from gameplay object lifetime with explicit stop-and-drain.
- [ ] Fix standards-conforming C/C++ warnings without changing decoded samples.
- [ ] Compare PCM hashes and frame counts on Windows/Linux.
- [ ] Run repeated stream create/play/stop/destroy cycles with allocation counters.
- [ ] Commit: `audio: harden portable stream callback lifetime`

**Evidence:** PCM hashes, frame counts, and zero-live-stream report.
