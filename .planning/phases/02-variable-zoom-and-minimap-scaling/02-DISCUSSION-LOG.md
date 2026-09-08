# Phase 2: Variable zoom and minimap scaling - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-08
**Phase:** 02-variable-zoom-and-minimap-scaling
**Areas discussed:** Zoom key mapping / T-key conflict, Zoom feel, Minimap sizing rule, Zoom vs resolution change, Zoom persistence

---

## Initial gray-area selection

User was asked which areas to discuss; responded freeform: "T-Key Conflict is
interesting. I just learned about this. Looks like an unimplemented feature.
Can you tell me more about the T key?" — investigation followed (see T-Key
Conflict below), then the remaining areas were discussed in sequence.

## T-Key Conflict (and full key-mapping investigation)

Investigation findings presented to user:

| Binding | Where | Alive in single-player? |
|--------|-------------|----------|
| Bare T | `begin_timeout` → CMD_GAME_TIMEOUT_SEND | No — guarded by `temp.LocalPlayer.TimeOutEnable`, set only by MP transceiver (`CTimeOut::InitGameStart`, MultiPlayerTransceiver.cpp:864); `CSinglePlayerTransceiver::CommandTimeOut` is an empty stub (SinglePlayerTransceiver.h:39) |
| Bare B | `show_status_bar` → MC_TOGGLE_UNIT_INFO | Yes — toggles unit-info status bar; tooltip text "[B] Show Status Bar" (showsbbutton.txt) is user-visible |
| G | only Ctrl+Shift+G (show_grid) | Yes (debug toggle) |
| N | only Ctrl+Shift+N (show_noise) | Yes (debug toggle) |
| H/J/Y | H: Ctrl+Shift+H (show_hp_info), J: nothing, Y: Ctrl+Shift+Y (show_haze) | — |
| J, K, L | **no reference anywhere in defconf.cfg** | Fully free |

Options presented across the conversation:

| Option | Description | Selected |
|--------|-------------|----------|
| T = zoom in, drop MP begin_timeout | Bare T is dead in SP anyway | |
| T = zoom in, move begin_timeout | Keep MP working via Ctrl+T | |
| Keep T for timeout | Pick a different zoom key | |
| G in / B out / N reset | User proposal — B turned out to be taken (show_status_bar) | |
| G in / N out / M reset | M is taken (action_place_marker) | |
| H in / J out / Y reset | All bare-free, but not fully free (H, Y in Ctrl+Shift debug combos) | |
| **J in / K out / L reset** | The only three character keys not referenced by ANY binding in ANY section; adjacent cluster | ✓ |

**User's choice:** J = zoom in, K = zoom out, L = reset.
**Notes:** User asked "Are Ctrl+Shift keys officially documented?" — answer:
only in project docs/tooltips (no upstream manual). User asked "Find me other
fully free character keys" — complete scan produced exactly J, K, L.

---

## Zoom feel

| Option | Description | Selected |
|--------|-------------|----------|
| Stepped zoom | Discrete levels per press/notch, classic RTS feel | ✓ |
| Smooth/continuous | Interpolated, needs smoothing + anchor tracking | |
| Zoom at cursor | World point under cursor stays fixed (SupCom/BAI standard) | ✓ |
| Zoom at screen center | Simpler, matches existing projection machinery | |
| Fine steps ~1.2x | ~5–7 levels 1024×768 → 640×480 | ✓ |
| Coarse steps ~1.4x | Fewer presses, coarser feel | |
| Hold-to-repeat | Step every ~150–250ms while key held | ✓ |
| Single step per press | No repeat | |
| 640x480 floor, exact | Zero zoom range at min resolution | ✓ |
| Floor + headroom | Small range even at 640×480 | |

**User's choice:** Stepped / cursor anchor / ~1.2× / hold-to-repeat / exact floor.
**Notes:** User accepted recommendations across all five zoom-feel questions.

---

## Minimap sizing rule

| Option | Description | Selected |
|--------|-------------|----------|
| Half of drawable width | Minimap + status bar together = 50% of drawable width | ✓ |
| Panel fixed, minimap gets rest | Panel keeps legacy size, minimap absorbs remainder | |
| On resolution change | Recompute on resolution change only (window is fixed-size) | ✓ |
| Per-frame check | Defensive recompute every frame | |
| Keep original layout | Diamond top-right, status bar beneath | ✓ |
| Redesign layout | Stacked/docked/collapsible rework | |
| Exact 50% always | Strict even at low resolutions | ✓ |
| 50% with legacy minimum | Protect panel minimum content size | |

**User's choice:** Minimap + panel = exactly 50% of drawable width, resize on
resolution change only, original arrangement, strict 50% at all resolutions.
**Notes:** Texture recreation on resize is implied (textures sized from widget
rect); pow2 overlay invariant must be preserved.

---

## Zoom vs resolution change

| Option | Description | Selected |
|--------|-------------|----------|
| Keep zoom level, clamp | Preserve step count, recompute viewport, clamp to floor | ✓ |
| Reset on resolution change | Simplest, loses player context | |
| Keep world view size | Preserve visible world in pixels | |

**User's choice:** Keep zoom level, clamp to new bounds.

---

## Zoom persistence

| Option | Description | Selected |
|--------|-------------|----------|
| Reset each mission | Zoom is session-local convenience | ✓ |
| Persist in savegame | Restore saved zoom on load | |
| Persist globally | config.cfg global, survives everything | |

**User's choice:** Reset each mission (start, restart, load).

---

## Final readiness check

| Option | Description | Selected |
|--------|-------------|----------|
| I'm ready for context | All areas covered | ✓ |
| Explore more gray areas | e.g. zoom indicator, wheel direction, editor mode | |

## Claude's Discretion

- Exact step factor (within ~1.15–1.25×)
- Exact hold-to-repeat interval (within ~150–250 ms)
- Wheel-notch magnitude vs key step
- Exact implementation mechanism/plumbing (projection-scale knob expected)

## Deferred Ideas

- Zoom UI indicator (zoom-level readout) — potential small future phase
- Removing/rebinding dead-in-SP bare-T begin_timeout — MP behavior, out of scope
- Zoom sound feedback / smoothing at limits — not requested