# Windows Data Junction Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make default game staging create a Windows directory junction promptly instead of silently copying 2.68 GB when symbolic links are unavailable.

**Architecture:** Keep staging and its regression test together in `tools/zig/stage.zig`. Replace symbolic-link creation with a junction operation, preserve explicit `--copy-data`, and propagate link-mode errors instead of changing modes implicitly.

**Tech Stack:** Zig 0.16, Windows reparse points, Zig build runner.

---

### Task 1: Reproduce the link-mode contract in a test

**Files:**
- Modify/Test: `tools/zig/stage.zig`

- [ ] **Step 1: Expose the staging function to Zig tests and add a focused test**

Add a `test "link mode stages Data as a junction"` block that creates temporary repository and install directories, places a sentinel under source `Data`, invokes `stage` with `.data_mode = .link`, and asserts the sentinel is visible through install `Data` while the installed entry has the reparse-point attribute.

- [ ] **Step 2: Run the test and verify RED**

Run: `zig test tools/zig/stage.zig`

Expected: FAIL because current symbolic-link creation is rejected on this Windows configuration and falls back to an ordinary copied directory.

- [ ] **Step 3: Record the exact failure before implementation**

Confirm the assertion reports that staged `Data` is not a reparse point. Do not proceed if failure is caused by test setup or compilation.

### Task 2: Replace symbolic links and remove implicit copying

**Files:**
- Modify: `tools/zig/stage.zig`

- [ ] **Step 1: Make link mode propagate failures**

Replace the catch-and-copy block with:

```zig
try linkData(io, allocator, repo, cwd, destination, options.install_dir);
```

Keep the `.copy` branch unchanged so copying remains available only through `--copy-data`.

- [ ] **Step 2: Implement junction creation**

Replace `createDirectoryLink` with `createDirectoryJunction`. Create the empty link directory, open it as a reparse-point handle, construct an `IO_REPARSE_TAG_MOUNT_POINT` buffer whose substitute name is the absolute target prefixed with `\\??\\`, and submit it using `FSCTL_SET_REPARSE_POINT`. Close the handle on every path and propagate the Windows error returned by the failing operation.

- [ ] **Step 3: Update the caller**

Call:

```zig
try createDirectoryJunction(io, cwd, allocator, data_path, link_path);
```

The helper must leave no ordinary directory behind when junction creation fails.

- [ ] **Step 4: Run the focused test and verify GREEN**

Run: `zig test tools/zig/stage.zig`

Expected: PASS, with the staged `Data` path reported as a directory reparse point and the sentinel accessible.

### Task 3: Verify the real build path

**Files:**
- No source changes expected.

- [ ] **Step 1: Remove the incomplete copied staging directory**

Verify the resolved path is the repository's `zig-out/game/Data`, then remove only that incomplete generated directory.

- [ ] **Step 2: Run the staging command**

Run: `zig build install-game`

Expected: exits successfully without a multi-minute copy; `zig-out/game/Data` is a junction targeting the repository `Data` directory.

- [ ] **Step 3: Run the game path**

Run: `zig build run`

Expected: staging completes promptly and `Game.exe` launches. Closing the game allows the build command to exit.

- [ ] **Step 4: Run relevant build verification**

Run: `zig build game-all`

Expected: successful exit with no compilation failures.

- [ ] **Step 5: Review the diff**

Run: `git diff --check` and `git diff -- tools/zig/stage.zig`

Expected: no whitespace errors and only the focused junction/test changes.
