# P05-M04 — Stream and Callback Lifetime

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove decoded-stream ownership and callback safety across stop, seek, EOF, and shutdown.

**Dependencies:** P05-M03.

**Allowed files:** `Sources/src/SFX/AudioBackendOpen.cpp`, `Sources/src/SFX/AudioBackendXiphVorbis.h`, `Sources/src/SFX/AudioBackendXiphVorbis.c`, `Sources/src/SFX/StreamingSound.cpp`, `tools/zig/audio_stream_test.cpp`, `build.zig`.

- [x] Add a deterministic PCM fixture for read, seek, loop, EOF, short read, callback stop, and source destruction; a valid Vorbis fixture remains open because the repository has no checked-in Ogg sample.
- [x] Separate callback-owned state from gameplay object lifetime with atomic callback publication, a closing flag, callback reader counting, callback detachment before sound stop, and explicit drain before deletion.
- [x] Fix standards-conforming C/C++ warnings in the lifetime path without changing decoded samples.
- [ ] Compare PCM hashes and frame counts on Windows/Linux; Windows frame/count coverage passes, Linux execution remains open.
- [x] Run repeated stream callback create/stop/destroy cycles with a zero-live-reader assertion (`cycles=100`).
- [ ] Commit: `audio: harden portable stream callback lifetime`

**Evidence:** Windows `test-audio-stream` passes PCM read/seek/loop/EOF and 100 callback-drain cycles. The production SFX module builds after callback detachment and reader-drain hardening. Valid Vorbis decode parity and cross-platform hashes remain open.
