# P05-M05 — Run the Network and System Services Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove native service behavior and unchanged network serialization before full game linking.

**Dependencies:** P05-M03, P05-M04.

**Allowed files:** `tools/zig/network_system_gate.cpp`, `build.zig`, `docs/superpowers/evidence/platform-port/target-matrix.md`.

- [ ] Add one native gate that initializes platform runtime, starts TCP/UDP loopback, exchanges known protocol fixture bytes, exercises timeout/cancel, runs a no-op child process, records an injected dialog, and shuts down all workers/sockets.
- [ ] Compare transmitted fixture hashes with the pre-port expected values checked into the test source.
- [ ] Run `test-platform-network`, `test-platform-system`, and `test-network-system-gate` natively on Windows/Linux; compile macOS.
- [ ] Run Windows game startup smoke to detect module or service regressions.
- [ ] Record native versus cross status in the target matrix.
- [ ] Commit: `test: validate portable network and system services`

**Evidence:** fixture hashes, native gate transcript, and Windows smoke checkpoints.
