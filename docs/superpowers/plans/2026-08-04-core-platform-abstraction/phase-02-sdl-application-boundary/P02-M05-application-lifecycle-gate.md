# P02-M05 — Close the SDL Application Lifecycle Gate

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prove all SDL application services share one runtime and obey startup/shutdown order.

**Dependencies:** P02-M04.

**Allowed files:** `tools/zig/game_bootstrap_smoke.cpp`, `tools/zig/platform_application_gate.cpp`, `tools/zig/runtime_platform_audit.zig`, `build.zig`.

- [ ] Build a real executable plus consumer module that polls events, toggles focus/fullscreen, uses clipboard/controller fixtures, and borrows the window for GfxGpu smoke.
- [ ] Assert runtime create precedes module load and renderer create; reverse the order during shutdown.
- [ ] Run three complete application/renderer restart cycles and compare lifecycle traces.
- [ ] Compile the gate for all triples and run natively on Windows and Linux.
- [ ] Remove SDL window/event/cursor temporary allowlist entries outside Platform and renderer bridges.
- [ ] Commit: `test: close SDL application boundary`

**Evidence:** identical ordered lifecycle traces and zero live counts.
