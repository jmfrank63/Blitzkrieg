# Zig-assisted 64-bit foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a testable Zig static library with a C ABI and make the playable Windows build select the correct x86 or x64 MSVC/SDK library directories.

**Architecture:** `Blitz64` is a Zig static library with a C header and no C++ dependencies. A C++ ABI smoke executable links against it to prove the exported symbols and fixed-width types work across the language boundary. `build.zig` centralizes the selected target architecture so all native runtime artifacts use matching Windows libraries.

**Tech Stack:** Zig 0.16 build system and test runner, Zig C ABI exports, C++ compiled through `zig build`, MSVC/Windows SDK import libraries.

---

## File structure

- Create: `Sources/src/Blitz64/blitz64.zig` — Zig implementation and unit tests for fixed-width ABI primitives.
- Create: `Sources/src/Blitz64/blitz64.h` — C/C++ compatible public declarations for exported Zig functions.
- Create: `tools/zig/blitz64_abi_test.cpp` — real C++ consumer used by the integration build step.
- Modify: `build.zig` — defines the `Blitz64` static library, ABI test step, and target-aware MSVC library paths.
- Modify: `.gitignore` only if a new test-output path is generated and not ignored; do not add generated artifacts to source control.

## Task 1: Add the test-first Zig ABI primitive

**Files:**

- Create: `Sources/src/Blitz64/blitz64_test.zig`
- Create: `Sources/src/Blitz64/blitz64.zig`

- [ ] **Step 1: Write the failing Zig unit test**

Create `Sources/src/Blitz64/blitz64_test.zig`:

```zig
const std = @import("std");
const blitz64 = @import("blitz64");

test "bit conversion preserves every IEEE-754 single-precision bit" {
    const bits: u32 = 0x7fc0_1234;
    try std.testing.expectEqual(bits, blitz64.bk_f32_bits(@bitCast(bits)));
}
```

- [ ] **Step 2: Run the test and verify it fails because the module is absent**

Run:

```powershell
zig test Sources/src/Blitz64/blitz64_test.zig --dep blitz64 -Mroot=Sources/src/Blitz64/blitz64_test.zig -Mblitz64=Sources/src/Blitz64/blitz64.zig
```

Expected: a non-zero exit status reporting that `Sources/src/Blitz64/blitz64.zig` does not exist.

- [ ] **Step 3: Implement the minimum Zig module**

Create `Sources/src/Blitz64/blitz64.zig`:

```zig
pub fn bk_f32_bits(value: f32) u32 {
    return @bitCast(value);
}

export fn bk_f32_bits_c(value: f32) u32 {
    return bk_f32_bits(value);
}
```

- [ ] **Step 4: Run the unit test and verify it passes**

Run:

```powershell
zig test Sources/src/Blitz64/blitz64_test.zig --dep blitz64 -Mroot=Sources/src/Blitz64/blitz64_test.zig -Mblitz64=Sources/src/Blitz64/blitz64.zig
```

Expected: exit status 0 and one passed test.

- [ ] **Step 5: Commit the test and implementation**

```powershell
git add Sources/src/Blitz64/blitz64.zig Sources/src/Blitz64/blitz64_test.zig
git commit -m "feat: add Blitz64 Zig ABI primitive"
```

## Task 2: Publish and prove the C ABI

**Files:**

- Create: `Sources/src/Blitz64/blitz64.h`
- Create: `tools/zig/blitz64_abi_test.cpp`
- Modify: `build.zig:611-747`

- [ ] **Step 1: Write the failing C++ integration caller**

Create `Sources/src/Blitz64/blitz64.h`:

```c
#ifndef BLITZ64_H
#define BLITZ64_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t bk_f32_bits_c(float value);

#ifdef __cplusplus
}
#endif

#endif
```

Create `tools/zig/blitz64_abi_test.cpp`:

```cpp
#include <cstdint>
#include "blitz64.h"

int main()
{
    union { float value; std::uint32_t bits; } nan = { 0.0f };
    nan.bits = 0x7fc01234u;
    return bk_f32_bits_c(nan.value) == nan.bits ? 0 : 1;
}
```

- [ ] **Step 2: Add the failing build step**

Immediately after `const optimize = b.standardOptimizeOption(.{});` in `build.zig`, add:

```zig
const blitz64 = addBlitz64(b, target, optimize);
```

Pass `blitz64` as a final argument at the `addGame(...)` call and add a matching final `blitz64: *std.Build.Step.Compile` parameter to `addGame`. Before the existing `return game;` in `addGame`, add:

```zig
game_module.linkLibrary(blitz64);
```

Run:

```powershell
zig build blitz64-abi-test -Dtarget=x86_64-windows-msvc
```

Expected: the build command is unknown because the `blitz64-abi-test` step has not been defined.

- [ ] **Step 3: Implement the library and integration build step**

Add this function before `addRandomMapGen` in `build.zig`:

```zig
fn addBlitz64(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) *std.Build.Step.Compile {
    return b.addStaticLibrary(.{
        .name = "Blitz64",
        .root_source_file = b.path("Sources/src/Blitz64/blitz64.zig"),
        .target = target,
        .optimize = optimize,
    });
}
```

After the existing package build-step declarations, add:

```zig
const abi_test = b.addExecutable(.{
    .name = "blitz64-abi-test",
    .target = target,
    .optimize = optimize,
});
abi_test.addCSourceFile(.{
    .file = b.path("tools/zig/blitz64_abi_test.cpp"),
    .flags = &.{ "-std=c++17" },
});
abi_test.addIncludePath(b.path("Sources/src/Blitz64"));
abi_test.linkLibrary(blitz64);
linkMsvcRuntime(abi_test.root_module, optimize);
const run_abi_test = b.addRunArtifact(abi_test);
const abi_test_step = b.step("blitz64-abi-test", "Run the Blitz64 C++ ABI smoke test");
abi_test_step.dependOn(&run_abi_test.step);
```

- [ ] **Step 4: Run the C++ ABI smoke test for both supported Windows widths**

Run:

```powershell
zig build blitz64-abi-test -Dtarget=x86-windows-msvc
zig build blitz64-abi-test -Dtarget=x86_64-windows-msvc
```

Expected: both commands exit 0; each executable returns 0 after comparing an NaN payload bit pattern through the Zig-exported C function.

- [ ] **Step 5: Commit the ABI boundary**

```powershell
git add build.zig Sources/src/Blitz64/blitz64.h tools/zig/blitz64_abi_test.cpp
git commit -m "feat: link C++ runtime with Blitz64 ABI library"
```

## Task 3: Add a project-wide Zig test step

**Files:**

- Modify: `build.zig` near the `blitz64-abi-test` declaration added in Task 2

- [ ] **Step 1: Write the failing build invocation**

Run:

```powershell
zig build test -Dtarget=x86_64-windows-msvc
```

Expected: a non-zero exit status because `build.zig` does not yet define a `test` step.

- [ ] **Step 2: Define the Zig unit-test artifact and aggregate test step**

Add after the ABI smoke-test setup in `build.zig`:

```zig
const blitz64_unit_tests = b.addTest(.{
    .root_source_file = b.path("Sources/src/Blitz64/blitz64.zig"),
    .target = target,
    .optimize = optimize,
});
const run_blitz64_unit_tests = b.addRunArtifact(blitz64_unit_tests);
const test_step = b.step("test", "Run Zig unit tests and the Blitz64 ABI smoke test");
test_step.dependOn(&run_blitz64_unit_tests.step);
test_step.dependOn(&run_abi_test.step);
```

Move the bit-preservation test from `blitz64_test.zig` into `blitz64.zig` so `b.addTest` owns the test:

```zig
test "bit conversion preserves every IEEE-754 single-precision bit" {
    const std = @import("std");
    const bits: u32 = 0x7fc0_1234;
    try std.testing.expectEqual(bits, bk_f32_bits(@bitCast(bits)));
}
```

Delete `Sources/src/Blitz64/blitz64_test.zig` after moving the test.

- [ ] **Step 3: Run the aggregate test step for x86 and x64**

Run:

```powershell
zig build test -Dtarget=x86-windows-msvc
zig build test -Dtarget=x86_64-windows-msvc
```

Expected: both commands exit 0, run the Zig unit test, and run the C++ ABI executable.

- [ ] **Step 4: Commit the test build graph**

```powershell
git add -u Sources/src/Blitz64
git add build.zig Sources/src/Blitz64/blitz64.zig
git commit -m "test: add Blitz64 ABI test step"
```

## Task 4: Select MSVC and Windows SDK libraries by target architecture

**Files:**

- Modify: `build.zig:611-645`
- Modify: `build.zig:1538-1557`

- [ ] **Step 1: Capture the failing x64 game build**

Run:

```powershell
zig build game -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
```

Expected: the command currently reaches x64 source diagnostics, while its emitted linker command still contains `...\\x86` MSVC and Windows SDK library directories. Record the first diagnostic and the three x86 library paths in the implementation log before editing.

- [ ] **Step 2: Make the selected architecture explicit in `ToolchainIncludes`**

Replace `ToolchainIncludes` with:

```zig
const ToolchainIncludes = struct {
    msvc_include: []const u8,
    windows_sdk_include: []const u8,
    msvc_lib: []const u8,
    windows_sdk_lib: []const u8,
    library_arch: []const u8,
};
```

Immediately before constructing `toolchain` in `build`, add:

```zig
const library_arch = switch (target.result.cpu.arch) {
    .x86 => "x86",
    .x86_64 => "x64",
    else => @panic("The Windows runtime build supports only x86 and x86_64 targets"),
};
```

Set `.library_arch = library_arch` in the `ToolchainIncludes` initializer.

- [ ] **Step 3: Replace hardcoded x86 directories**

Replace `addMsvcLibraryPaths` with:

```zig
fn addMsvcLibraryPaths(b: *std.Build, module: *std.Build.Module, toolchain: ToolchainIncludes) void {
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}\\{s}", .{ toolchain.msvc_lib, toolchain.library_arch }) });
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}\\ucrt\\{s}", .{ toolchain.windows_sdk_lib, toolchain.library_arch }) });
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}\\um\\{s}", .{ toolchain.windows_sdk_lib, toolchain.library_arch }) });
}
```

Set the default `.msvc_lib` in `build` to the MSVC `lib` directory without an architecture suffix:

```zig
.msvc_lib = "C:\\Program Files\\Microsoft Visual Studio\\18\\Insiders\\VC\\Tools\\MSVC\\14.51.36231\\lib",
```

- [ ] **Step 4: Verify x86 remains buildable and x64 progresses without x86 import libraries**

Run:

```powershell
zig build game -Dtarget=x86-windows-msvc -Doptimize=ReleaseFast --summary all
zig build game -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
```

Expected: x86 preserves its current result. x64 no longer passes any `...\\x86` library directory to the linker. Fix the first source-level x64 diagnostic only when it belongs to the game launcher; leave renderer/input/runtime library migration to the next plan.

- [ ] **Step 5: Commit the architecture-aware toolchain configuration**

```powershell
git add build.zig
git commit -m "build: select MSVC libraries by target architecture"
```

## Task 5: Record the x64 compiler frontier for the next plan

**Files:**

- Create: `docs/superpowers/plans/2026-07-11-zig-64bit-compiler-frontier.md`

- [ ] **Step 1: Run focused x64 builds for every playable artifact**

Run:

```powershell
zig build game-all -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
zig build gfx -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
zig build input -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
zig build sfx -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
zig build net -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
zig build image -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
zig build anim -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
zig build ui -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
zig build game -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast --summary all
```

- [ ] **Step 2: Write the compiler-frontier report**

Create `docs/superpowers/plans/2026-07-11-zig-64bit-compiler-frontier.md` with a table containing each command, exit status, first compiler or linker diagnostic, source file, line, and classification: `pointer truncation`, `Win32 width`, `x86 assembly`, `DLL ABI`, or `unrelated pre-existing issue`.

- [ ] **Step 3: Verify the foundation tests after the report**

Run:

```powershell
zig build test -Dtarget=x86-windows-msvc
zig build test -Dtarget=x86_64-windows-msvc
git diff --check
```

Expected: both test commands exit 0 and `git diff --check` produces no output.

- [ ] **Step 4: Commit the observed frontier**

```powershell
git add docs/superpowers/plans/2026-07-11-zig-64bit-compiler-frontier.md
git commit -m "docs: record Windows x64 compiler frontier"
```

## Coverage review

This plan implements the design’s first delivery item: a Zig library, C ABI header, test infrastructure, and x64-aware toolchain paths. It does not migrate assembly, create handles, or change persistence/network layouts; those are intentionally separate plans because each changes runtime semantics and requires the compiler-frontier evidence from Task 5.
