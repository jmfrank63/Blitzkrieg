# Zig-native run and packaging Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace PowerShell staging with Zig so `zig build run` stages once, and create both distribution ZIPs without duplicate data copies or whole-file allocations.

**Architecture:** A new Zig command-line staging utility copies a build layout, creates/copies Data, and conditionally adds editor executables. `build.zig` invokes that utility and launches the staged executable directly. The existing ZIP utility is converted to a bounded-memory, two-pass stored-entry writer and gains a testable library surface.

**Tech Stack:** Zig 0.16 standard library, Windows directory junctions, Zig build graph, ZIP stored entries.

---

### Task 1: Establish a testable Zig utility module

**Files:**
- Create: `tools/zig/layout.zig`
- Create: `tools/zig/layout_test.zig`
- Modify: `tools/zig/package.zig`

- [ ] **Step 1: Write failing tests for editor discovery and stream copy**

```zig
test "findEditors returns only present editor files" {
    // Create a temporary root with editor.exe and verify its relative destination.
}

test "copyFile streams every source byte" {
    // Copy a multi-buffer fixture and compare destination content.
}
```

- [ ] **Step 2: Run the tests to verify failure**

Run: `zig test tools/zig/layout_test.zig`

Expected: failure because `layout.zig` does not exist.

- [ ] **Step 3: Implement layout helpers**

```zig
pub const Editor = struct { source_rel: []const u8, dest_rel: []const u8 };
pub fn copyFile(io: std.Io, source_dir: std.Io.Dir, source_rel: []const u8, dest_dir: std.Io.Dir, dest_rel: []const u8) !void;
pub fn discoverEditors(allocator: std.mem.Allocator, io: std.Io, repo_root: std.Io.Dir) ![]Editor;
```

Implement buffered file copying, parent creation, recursive directory copying, and the fixed editor source list.

- [ ] **Step 4: Run the tests to verify success**

Run: `zig test tools/zig/layout_test.zig`

Expected: all layout tests pass.

### Task 2: Add Zig-native staging command

**Files:**
- Create: `tools/zig/stage.zig`
- Modify: `tools/zig/layout.zig`
- Modify: `tools/zig/layout_test.zig`

- [ ] **Step 1: Write failing tests for staging argument parsing**

```zig
test "parseArgs chooses junction data mode by default" { /* expected `.junction` */ }
test "parseArgs enables copied data and editors" { /* expected `.copy` and true */ }
```

- [ ] **Step 2: Run test to verify failure**

Run: `zig test tools/zig/layout_test.zig`

Expected: failure because `parseArgs` is missing.

- [ ] **Step 3: Implement the staging CLI**

```text
stage <repo-root> <install-dir> [--copy-data] [--include-editors]
```

Copy `zig-out/bin`, copy `Data/Configs/config.cfg` and `defconf.cfg`, create a Windows `Data` junction by default, copy Data with `--copy-data`, and add discovered editors with `--include-editors`. Report source/destination context for errors.

- [ ] **Step 4: Run the tests to verify success**

Run: `zig test tools/zig/layout_test.zig`

Expected: all layout tests pass.

### Task 3: Stream ZIP entries and make archive output atomic

**Files:**
- Modify: `tools/zig/package.zig`
- Create: `tools/zig/package_test.zig`

- [ ] **Step 1: Write failing ZIP tests**

```zig
test "stored ZIP preserves a file larger than the transfer buffer" {
    // Create a fixture, invoke packaging, and assert the EOCD and payload.
}

test "partial archive is not published as output" {
    // Simulate a failing source and assert output path is absent.
}
```

- [ ] **Step 2: Run test to verify failure**

Run: `zig test tools/zig/package_test.zig`

Expected: failure because package helpers are not exported.

- [ ] **Step 3: Implement a two-pass writer**

For each source file, first stream chunks to calculate `u32` CRC and size, then stream chunks into the ZIP after its local header. Track offsets with `writer.logicalPos()`. Write to `<output>.partial`, flush and close it, then rename it to the requested output only after a valid central directory and EOCD are written.

- [ ] **Step 4: Run test to verify success**

Run: `zig test tools/zig/package_test.zig`

Expected: all ZIP tests pass.

### Task 4: Rewire the Zig build graph and remove PowerShell

**Files:**
- Modify: `build.zig:649-809`
- Delete: `tools/zig/game_install.ps1`
- Modify: `docs/zig-build-transition.md`

- [ ] **Step 1: Add a failing build-graph smoke check**

Run: `zig build --help | Select-String 'install-game|package-game|run'`

Expected: this preserves the command names before the graph rewrite.

- [ ] **Step 2: Compile stage and package tools as Zig executables**

Replace all `b.addSystemCommand("powershell", ...)` calls. `install-game` runs the stage executable. `run` depends on `install-game` and invokes `Game.exe` with `.setCwd` set to the installed directory, without running staging again.

- [ ] **Step 3: Reuse package staging**

Make the game archive stage into `zig-out/package-staging/game` with copied data. The editors archive depends on that base stage, adds editors to the same directory, then packages it. Preserve standalone package subcommands.

- [ ] **Step 4: Delete the obsolete script and update docs**

Remove `tools/zig/game_install.ps1`. Document that Zig performs staging and packaging, and that the ZIP writer has bounded memory use.

- [ ] **Step 5: Verify build graph compilation**

Run: `zig build --help`

Expected: exit code 0 and the five documented commands listed.

### Task 5: Full verification

**Files:**
- Modify: `docs/zig-build-transition.md`

- [ ] **Step 1: Verify staging and repeated run preparation**

Run: `zig build install-game; zig build install-game`

Expected: both staging passes finish without PowerShell and the second does not fail because of duplicate `run` staging.

- [ ] **Step 2: Build both archives and measure elapsed time**

Run: `Measure-Command { zig build package }`

Expected: exit code 0 and both ZIP outputs under `zig-out/packages`.

- [ ] **Step 3: Probe every ZIP entry with .NET ZipArchive**

Run: the full `ZipArchive` entry stream probe supplied by the user for both outputs.

Expected: `Bad=0` for both archives, and the editors archive contains the expected `Editors/` entries.

- [ ] **Step 4: Verify no PowerShell path remains**

Run: `rg -n 'powershell|game_install\.ps1' build.zig tools docs`

Expected: no build implementation references; documentation may only mention the removed historical script if explicitly labelled historical.
