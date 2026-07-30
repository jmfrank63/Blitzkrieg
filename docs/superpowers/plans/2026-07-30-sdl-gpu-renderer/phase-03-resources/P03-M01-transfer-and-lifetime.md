# P03-M01 — Add Transfer Storage and Deferred Lifetime

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Provide bounded upload/readback staging and submission-safe native release.

**Dependencies:** P01-M01, P02-M03.

**Allowed files:** `Sources/src/GFXGPU/transfer.zig`, `Sources/src/GFXGPU/lifetime.zig`, `Sources/src/GFXGPU/renderer.zig`, `Sources/src/GFXGPU/root.zig`.

- [ ] Test allocation alignment, wrap/grow behavior, map failure, unmap-on-error, zero-byte rejection, and no reuse before submission completion with injected SDL calls.
- [ ] Implement separate upload and download transfer pools; each allocation records buffer, offset, length, map state, and submission serial.
- [ ] Define monotonic `u64` submission/completion serials and a deferred release queue tagged with the last possible use serial.
- [ ] Drain only entries whose serial is complete; device-idle shutdown drains all entries in reverse ownership order.
- [ ] Expose live counts by resource category and transfer bytes/high-water mark.
- [ ] Run Debug and ReleaseSafe pure tests.
- [ ] Commit: `feat: add GPU transfer and deferred lifetime core`

**Evidence:** serial-boundary and allocation-failure tests.
