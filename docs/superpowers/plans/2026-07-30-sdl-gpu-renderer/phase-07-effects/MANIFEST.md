# Phase 07 — Legacy Effect Families

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Map every legacy `SetShadingEffect`/fixed-function rendering path to a finite shader family plus normalized pipeline and binding state.

**Architecture:** One typed `EffectId` catalog replaces ad hoc shader/state mutation. Families share HLSL helpers and binding layouts. Unsupported IDs fail before drawing and include the original value in diagnostics.

**Tech Stack:** Canonical HLSL, shader manifest, Zig effect catalog, CPU reference tests, GPU probe scene.

---

| Packet | Depends on | Owns |
|---|---|---|
| P07-M01 | P06-M06 | exhaustive legacy effect inventory/catalog |
| P07-M02 | M01 | UI, text, unlit opaque, alpha-test |
| P07-M03 | M02 | alpha blend, particles, fog |
| P07-M04 | M03 | multitexture, lightmaps, material/lights |
| P07-M05 | M04 | stencil/shadow, water, special effects |

Exit: every effect ID reachable in current game source has a catalog record, shader pair, state policy, test fixture, and successful pipeline creation.
