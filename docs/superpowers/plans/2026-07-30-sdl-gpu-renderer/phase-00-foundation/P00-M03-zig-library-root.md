# P00-M03 — Add the Zig Renderer Library Root

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Build and test an empty renderer context without SDL initialization.

**Dependencies:** P00-M01.

**Allowed files:** `build.zig`, `Sources/src/GFXGPU/root.zig`, `Sources/src/GFXGPU/renderer.zig`.

**Required Zig surface:**

```zig
pub const Renderer = struct {
    allocator: std.mem.Allocator,
    pub fn init(allocator: std.mem.Allocator) Renderer {
        return .{ .allocator = allocator };
    }
    pub fn deinit(self: *Renderer) void {
        self.* = undefined;
    }
};
```

- [ ] First add a `root.zig` test that initializes and deinitializes `Renderer`.
- [ ] Run `zig build test-gfxgpu-core`; expected failure because the build step/module is absent.
- [ ] Add a target-aware static library named `GfxGpuZig`, include the SDL headers, link SDL3, and add `test-gfxgpu-core`.
- [ ] Implement the minimum `Renderer`.
- [ ] Run the new test step twice and `zig build sdl3`.
- [ ] Commit: `feat: add Zig GPU renderer library root`

**Evidence:** two passing test runs and artifact name/path.
