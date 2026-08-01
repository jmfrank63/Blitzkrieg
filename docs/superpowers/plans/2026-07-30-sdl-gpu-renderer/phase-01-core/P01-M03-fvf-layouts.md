# P01-M03 — Decode Legacy FVF Vertex Layouts

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Turn supported `EGFXFVF` combinations into validated vertex attributes and strides.

**Dependencies:** P01-M02.

**Allowed files:** `Sources/src/GFXGPU/vertex_layout.zig`, `Sources/src/GFXGPU/root.zig`.

**Required semantic order:** position, blend weights/indices, normal, point size, diffuse, specular, texture coordinates 0–7.

- [ ] Inventory FVF values actually constructed in `Sources/src` with `rg -n "EGFXFVF|GFXFVF|FVF_" Sources/src`.
- [ ] Add golden tests for every used combination: expected stride, offsets, attribute locations, and formats.
- [ ] Define `VertexLayout` with at most 16 attributes and no heap allocation.
- [ ] Reject XYZRHW, unsupported blend-weight counts, overlapping semantics, invalid texture-coordinate dimensions, and stride over `u16`.
- [ ] Add compile-time assertions matching legacy packed color size and scalar widths.
- [ ] Run pure tests and preserve the inventory output in the completion report.
- [ ] Commit: `feat: decode legacy FVF vertex layouts`

**Evidence:** inventory and golden layout table.
