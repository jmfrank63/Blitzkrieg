# P00-M04 — Publish the Versioned C ABI

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Publish a fixed-width C header and exported API table with lifecycle and diagnostics.

**Dependencies:** P00-M03.

**Allowed files:** `Sources/src/GFXGPU/gfxgpu_c.h`, `Sources/src/GFXGPU/abi.zig`, `Sources/src/GFXGPU/error.zig`, `Sources/src/GFXGPU/root.zig`.

**Required initial API table:**

```c
typedef struct GfxGpuApi {
    uint32_t abi_version;
    uint32_t struct_size;
    GfxGpuResult (*create)(const GfxGpuCreateInfo*, GfxGpuRenderer**);
    void (*destroy)(GfxGpuRenderer*);
    GfxGpuResult (*get_last_error)(GfxGpuRenderer*, char*, uint32_t, uint32_t*);
    GfxGpuResult (*get_live_counts)(GfxGpuRenderer*, GfxGpuLiveCounts*);
} GfxGpuApi;

GfxGpuResult gfxgpu_get_api(uint32_t requested_version, GfxGpuApi* out_api);
```

- [ ] Add Zig tests for C integer widths, zero invalid handle, null arguments, wrong version, short table, bounded diagnostic copy, and create/destroy balance.
- [ ] Run the test and capture the missing-module failures.
- [ ] Define header guards, `extern "C"`, explicit values, `struct_size`, live-count fields, create flags, and the README create structs.
- [ ] Export only `gfxgpu_get_api`; return function pointers to `callconv(.c)` wrappers.
- [ ] Catch every Zig error in the wrapper. No exported function may panic or unwind.
- [ ] Run `test-gfxgpu-core` and `zig build GfxGpuZig`.
- [ ] Commit: `feat: publish versioned GfxGpu C ABI`

**Evidence:** API/table sizes on x64 and passing failure-path tests.
