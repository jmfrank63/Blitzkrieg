# Phase 03 — GPU Resources and Lifetime

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Own upload/readback transfer buffers, geometry buffers, textures, samplers, depth/render targets, and deferred destruction in Zig.

**Architecture:** Resource modules expose only generational handles. Initial data reaches GPU storage through copy passes. Destruction removes the public handle immediately and defers native release until submission safety is known.

**Tech Stack:** SDL_GPU buffer/texture/transfer/copy APIs and Zig registries.

---

| Packet | Depends on | Owns |
|---|---|---|
| P03-M01 | P01-M01, P02-M03 | transfer allocator and lifetime queue |
| P03-M02 | M01, P01-M03 | static/dynamic/temp buffers |
| P03-M03 | M02, P01-M02 | textures and uploads |
| P03-M04 | M03, P01-M04 | sampler cache |
| P03-M05 | M04, P02-M04 | depth/color targets |
| P03-M06 | M05 | screenshot readback and resource stress |

Exit: GPU resource smoke creates, uploads, uses, destroys, drains deferred releases, and reports all live counts zero.
