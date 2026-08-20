# Phase 05 — Settings Data

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Declare the Cloud division and repair the tab machinery, before anything tries to read an option.

| Packet | Depends on | Owns |
|---|---|---|
| P05-M01 | P02-M05 | division constants repair and bounds guard |
| P05-M02 | M01 | Cloud.* option declarations and labels |

Exit: the Cloud tab appears as the fifth division with all six options, and a synthetic seventh division does not crash the screen.

P05-M01 Windows checkpoint: verified headlessly on the release build with
`BK_AUTO_UI` and `cmd=0x100e00d8`. Without the guard, a synthetic seventh
division dies at the settings command with 0xC0000005; with it, the overflow
is skipped with a stderr trace naming the division and the screen opens,
cancels and exits cleanly. Four-division baseline unchanged, exit 0. Commit
`23af45ed7`.

Carried forward from P05-M01:

- **The settings screen opens via `cmd=0x100e00d8`**, not via the settings
  message (10005) posted at the main menu — the message produced no screen
  in this build. P05-M02 and every phase-07 packet should drive the screen
  with the command.
- `NStr::DebugTrace` reaches stderr headless (DebugWrite falls back when no
  platform client is attached), so guard diagnostics are assertable in
  `BK_AUTO_UI` runs.
- Today's real division count is four, `GFX` first in tab order; six UI
  slots exist. Cloud (P05-M02) becomes the fifth.
