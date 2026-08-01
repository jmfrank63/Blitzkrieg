# Phase 08 — Compatibility and Visual Parity

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Demonstrate that the SDL_GPU renderer preserves current Windows behavior in representative automated and human-observed scenarios.

**Architecture:** Automated compatibility tests catch numeric/state/resource regressions. Paired legacy/SDL_GPU captures use the same game data, settings, camera, and scenario. Differences are classified, not hidden by broad tolerances.

**Tech Stack:** Existing game, deterministic capture/readback, PowerShell evidence tools, renderer metrics.

---

| Packet | Depends on | Owns |
|---|---|---|
| P08-M01 | P07-M05 | compatibility matrix and automated suite |
| P08-M01.5 | M01 | renderer-neutral game asset bridge required by parity captures |
| P08-M02 | M01 | deterministic reference-scene comparison |
| P08-M03 | M02 | main-menu and representative-mission parity |
| P08-M04 | M03 | resize/fullscreen/restart/resource endurance |

Exit: all automated checks pass and a human accepts the main menu and representative mission against the legacy reference with no unexplained material differences.
