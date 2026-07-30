# P08-M03 — Validate Main Menu and Representative Mission

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Prepare and record human parity acceptance for real game UI and world rendering.

**Dependencies:** P08-M02.

**Allowed files:** `tools/zig/capture_gfx_game.ps1`, `docs/superpowers/evidence/sdl-gpu/menu-mission-parity.md`.

- [ ] Build/install both renderer variants into separate explicit staging directories with identical data.
- [ ] Script startup arguments/settings and log collection; never overwrite the user's normal game settings.
- [ ] Capture main menu, loading screen, initial mission view, terrain close-up, units, selection markers, particles/explosions, shadows, water if present, pause UI, and return-to-menu.
- [ ] Record side-by-side checklist for geometry, textures, fonts, color/alpha, depth ordering, fog, lighting, shadows, water, clipping, flicker, and missing draws.
- [ ] Record renderer startup/shutdown lines and screenshots paths; do not commit captures.
- [ ] Human reviewer writes `accepted`, date, hardware/driver, and any classified non-material differences in evidence markdown.
- [ ] Commit: `test: record Windows renderer visual parity`

**Evidence:** signed checklist and log excerpts. Luna must report “awaiting human acceptance” until the reviewer records it.
