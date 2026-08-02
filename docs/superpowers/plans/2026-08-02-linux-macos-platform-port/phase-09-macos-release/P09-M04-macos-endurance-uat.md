# P09-M04 — Validate macOS Endurance and Prepare Human UAT

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Exercise lifecycle churn and prepare a reproducible human-verification bundle.

**Dependencies:** P09-M03.

**Allowed files:** `tools/zig/game_endurance.zig`, `build.zig`, `docs/superpowers/evidence/platform-port/macos-acceptance.md`.

- [ ] Run the Linux endurance sequence plus Retina display movement, Cmd+Tab focus cycles, Cmd+Q shutdown, and five complete bundle process restarts.
- [ ] Require stable memory/counters, joined workers, reverse module unload, zero renderer resources, and no Metal/SDL validation errors.
- [ ] Record exact bundle hash/launch command/log path and human checklist for menu, mission, UI/world/text/effects, keyboard/mouse/text/controller, audio, display lifecycle, save/load, LAN loopback, return, and quit.
- [ ] Do not claim visual/playability acceptance in this packet.
- [ ] Commit: `test: validate macOS game endurance`

**Evidence:** endurance counters and UAT-ready package metadata.
