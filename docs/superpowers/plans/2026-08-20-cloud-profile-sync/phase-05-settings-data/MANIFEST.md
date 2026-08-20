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

P05-M02 Windows checkpoint: verified headlessly on the release build with
screenshots — five tabs (VIDEO, GAMEPLAY, MULTIPLAYER, SOUND, CLOUD), the
Cloud list carrying all six labeled options, `set=Cloud.Enabled=ON` through
the open screen persisting to the profile config and reading back ON on the
next launch. Commit `ca0f91565`.

Carried forward from P05-M02:

- **Tab order is a name tiebreak, not a design.** All four legacy divisions
  open at Order 1 and sort alphabetically (GFX < GamePlay < Multiplayer <
  Sound); Cloud's items start at Order 15 so its first encounter comes
  last. Anyone adding a division must reason about the global
  Order-then-name sort, not just within-division orders.
- `GetCloudProvider` (Off/S3/WebDAV) was added to the Zig options bridge,
  the legacy bridge, and the legacy `COptionSystem` — three files beyond
  the packet allowlist, recorded per the amendment custom. The Zig bridge
  is the one that actually serves the menu.
- Value labels fall back to the raw droplist strings when no
  `<key>.<VALUE>.name.txt` exists — the same convention `GFX.FullScreen`
  ships with. Localisation can add label files later without code.
- The harness combination that proves a UI packet end to end:
  `cmd=0x100e00d8` to open, `msg=<10007+n>` to switch tabs,
  `set=<option>=<value>` to change state, `shot` + ImageMagick
  (`magick -size WxH -depth 8 rgba:<file>`) to see it.

**Phase 05 exit: met on Windows.** The Cloud tab appears as the fifth
division with all six options (screenshots in the checkpoint runs), and the
P05-M01 measurement showed the synthetic seventh division skipped with a
trace instead of the pre-guard 0xC0000005.
