# Zig StreamIO Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and stage an x64 `StreamIO.dll` whose stable exports and C++ ABI bridge can replace the x86 StreamIO DLL without changing the legacy callers.

**Architecture:** A narrow C++ shim owns the legacy C++ virtual interfaces and forwards fixed C ABI calls to Zig. Zig owns temporary buffers and the new request service; the C++-visible methods remain synchronous and wait on that service. The first cut implements singleton registration and the hook surface, then adds storage incrementally; it must not pretend to make the remaining x86 DLLs loadable.

**Tech Stack:** Zig 0.16 `std.Io.Threaded`, Zig shared library build, clang-cl C++17 bridge, legacy StreamIO headers, Windows x64 ABI, `dumpbin` validation.

---

### Task 1: Define the Zig request service and prove synchronous waiting

**Files:**
- Create: `Sources/src/StreamIOZig/io_service.zig`
- Create: `Sources/src/StreamIOZig/io_service_test.zig`
- Modify: `build.zig`

- [ ] **Step 1: Write the failing Zig tests for request completion and cancellation**

```zig
test "submitted read completes and synchronous wait returns its byte count" {
    var service = try IoService.init(std.testing.allocator, .threaded);
    defer service.deinit();
    var bytes = [_]u8{ 1, 2, 3 };
    var request = try service.submitRead(.{ .buffer = &bytes, .complete_bytes = 3 });
    try std.testing.expectEqual(@as(usize, 3), try request.await(service.io()));
}

test "canceled request reaches the canceled terminal state" {
    var service = try IoService.init(std.testing.allocator, .threaded);
    defer service.deinit();
    var request = try service.submitRead(.{ .buffer = &[_]u8{}, .complete_bytes = 0 });
    request.cancel();
    try std.testing.expectError(error.Canceled, request.await(service.io()));
}
```

- [ ] **Step 2: Run the focused test and verify it fails because `IoService` does not exist**

Run: `zig test Sources/src/StreamIOZig/io_service_test.zig`

Expected: a compile failure naming missing `IoService`.

- [ ] **Step 3: Implement the minimal request service**

```zig
pub const Backend = enum { threaded, evented };
pub const Request = struct {
    state: enum { pending, completed, canceled } = .pending,
    complete_bytes: usize,
    pub fn cancel(self: *Request) void { self.state = .canceled; }
    pub fn await(self: *Request, io: std.Io) error{Canceled}!usize {
        _ = io;
        return switch (self.state) { .canceled => error.Canceled, .pending, .completed => self.complete_bytes };
    }
};
```

Use `std.Io.Threaded` for the initialized service; reject `.evented` on Windows at build configuration time rather than silently selecting a different backend.

- [ ] **Step 4: Run the focused test and the existing aggregate test target**

Run: `zig test Sources/src/StreamIOZig/io_service_test.zig; zig build test '-Dtarget=x86_64-windows-msvc'`

Expected: both commands pass.

- [ ] **Step 5: Commit**

```powershell
git add Sources/src/StreamIOZig/io_service.zig Sources/src/StreamIOZig/io_service_test.zig build.zig
git commit -m "feat: add StreamIO Zig request service"
```

### Task 2: Add a C ABI owned by Zig and test its temporary buffers

**Files:**
- Create: `Sources/src/StreamIOZig/streamio.zig`
- Create: `Sources/src/StreamIOZig/streamio_c.h`
- Create: `tools/zig/streamio_abi_test.cpp`
- Modify: `build.zig`

- [ ] **Step 1: Write the failing C++ ABI smoke test**

```cpp
#include "streamio_c.h"
#include <cassert>
int main() {
    void *first = bk_streamio_temp_buffer(64, 0);
    void *second = bk_streamio_temp_buffer(64, 0);
    assert(first != nullptr);
    assert(second != nullptr);
    assert(bk_streamio_temp_buffer(1, 10) == nullptr);
}
```

- [ ] **Step 2: Run it and verify it fails because the header/library do not exist**

Run: `zig build streamio-abi-test '-Dtarget=x86_64-windows-msvc'`

Expected: compile or link failure for `bk_streamio_temp_buffer`.

- [ ] **Step 3: Export the fixed C ABI from Zig**

```zig
export fn bk_streamio_temp_buffer(size: c_int, index: c_int) callconv(.c) ?*anyopaque {
    if (size < 0 or index < 0 or index >= 10) return null;
    return buffers[@intCast(index)].resize(@intCast(size));
}
```

The header declares only `extern "C"` fixed-width/C ABI functions. It must not expose Zig structs, slices, allocators, or request handles.

- [ ] **Step 4: Run the ABI smoke test and x64 unit tests**

Run: `zig build streamio-abi-test '-Dtarget=x86_64-windows-msvc'; zig build test '-Dtarget=x86_64-windows-msvc'`

Expected: both commands pass.

- [ ] **Step 5: Commit**

```powershell
git add Sources/src/StreamIOZig/streamio.zig Sources/src/StreamIOZig/streamio_c.h tools/zig/streamio_abi_test.cpp build.zig
git commit -m "feat: expose StreamIO Zig C ABI"
```

### Task 3: Implement the C++ legacy-interface bridge and DLL exports

**Files:**
- Create: `Sources/src/StreamIOZig/legacy_bridge.h`
- Create: `Sources/src/StreamIOZig/legacy_bridge.cpp`
- Create: `Sources/src/StreamIOZig/StreamIO.def`
- Modify: `Sources/src/StreamIOZig/streamio.zig`
- Modify: `build.zig`

- [ ] **Step 1: Write the failing DLL export test**

```cpp
HMODULE module = LoadLibraryA("StreamIO.dll");
assert(module != nullptr);
assert(GetProcAddress(module, "GetModuleDescriptor") != nullptr);
assert(GetProcAddress(module, "GetTempRawBuffer_Hook") != nullptr);
assert(GetProcAddress(module, "GetSLS_Hook") != nullptr);
assert(GetProcAddress(module, "GetSingletonGlobal_Hook") != nullptr);
```

- [ ] **Step 2: Run the test and verify it fails because no x64 StreamIO DLL is staged**

Run: `zig build streamio-dll-test '-Dtarget=x86_64-windows-msvc'`

Expected: `LoadLibraryA` fails before the replacement is built and staged.

- [ ] **Step 3: Implement the bridge using exact legacy vtable order**

`legacy_bridge.h` declares only the required `IRefCount`, `ISingleton`, and `ISaveLoadSystem` virtual methods in the order in `Sources/src/Misc/Basic.h`, `Sources/src/StreamIO/Globals.h`, and `Sources/src/StreamIO/StructureSaver.h`. `legacy_bridge.cpp` owns the C++ classes, forwards temporary-buffer acquisition to `bk_streamio_temp_buffer`, and exports these four `extern "C"` hooks:

```cpp
const SModuleDescriptor *STDCALL GetModuleDescriptor();
void *STDCALL GetTempRawBuffer_Hook(int size, int index);
ISaveLoadSystem *STDCALL GetSLS_Hook();
ISingleton *STDCALL GetSingletonGlobal_Hook();
```

The initial `ISaveLoadSystem` only stores factories and GDB and returns null for unimplemented storage/serialization methods. This makes failure explicit while preserving the ABI; storage arrives in the next task.

- [ ] **Step 4: Build and inspect the DLL**

Run: `zig build streamio '-Dtarget=x86_64-windows-msvc'; dumpbin /headers zig-out/bin/StreamIO.dll; dumpbin /exports zig-out/bin/StreamIO.dll`

Expected: headers report `8664 machine (x64)` and all four undecorated exports are present.

- [ ] **Step 5: Run the DLL export test and commit**

```powershell
zig build streamio-dll-test '-Dtarget=x86_64-windows-msvc'
git add Sources/src/StreamIOZig build.zig tools/zig/streamio_dll_test.cpp
git commit -m "feat: build x64 StreamIO compatibility DLL"
```

### Task 4: Stage the selected StreamIO DLL and prevent mixed architectures

**Files:**
- Modify: `build.zig`
- Modify: `tools/zig/stage.zig`
- Create: `tools/zig/stage_architecture_test.zig`

- [ ] **Step 1: Write the failing staging test**

```zig
test "x64 staging rejects an x86 StreamIO DLL" {
    try std.testing.expectError(error.ArchitectureMismatch,
        validatePeMachine(.x86_64, .x86));
}
```

- [ ] **Step 2: Run it and verify it fails because validation is absent**

Run: `zig test tools/zig/stage_architecture_test.zig`

Expected: a compile failure naming missing `validatePeMachine`.

- [ ] **Step 3: Implement staging selection and PE-machine validation**

Make `game-all` depend on the Zig `StreamIO.dll` for x64 and exclude the legacy staged `StreamIO.dll` for that target. Parse the PE COFF machine field and fail staging with the DLL name, target machine, and found machine when any selected runtime DLL differs.

- [ ] **Step 4: Verify the build product and staging guard**

Run: `zig build test '-Dtarget=x86_64-windows-msvc'; zig build game-all '-Dtarget=x86_64-windows-msvc' '-Doptimize=ReleaseFast'; dumpbin /headers zig-out/bin/StreamIO.dll`

Expected: tests and build pass; StreamIO reports `8664 machine (x64)`; the staging command identifies the next remaining x86 DLL instead of launching a mixed-architecture process.

- [ ] **Step 5: Commit**

```powershell
git add build.zig tools/zig/stage.zig tools/zig/stage_architecture_test.zig
git commit -m "feat: stage x64 StreamIO safely"
```

## Scope boundary

This foundation fixes the first x64 runtime boundary, `StreamIO.dll`. `Scene.dll`, `AILogic.dll`, `GameTT.dll`, FMOD, and legacy CRT/MFC DLLs remain x86 and will still prevent an x64 game launch until each has a port or replacement. No task may report `zig build run` working before the guard reports an architecture-consistent runtime set.

## Review

- The approved design's sync/async contract is covered by Task 1; the C++ ABI remains synchronous in Tasks 2–3.
- All four required exports, their x64 machine type, C++ ABI, and staging behavior are covered by Tasks 2–4.
- XML, ZIP/PAK, save/load format support, and options are intentionally deferred to their approved later phases.
- The plan contains no implementation placeholders; each code-producing task has an explicit failing test and verification command.
