# Windows Data Junction Fix

## Problem

The Zig-native staging tool attempts to expose the repository `Data` directory through a Windows symbolic link. On systems where unprivileged symbolic links are unavailable, link creation fails and staging silently falls back to copying 63,687 files (about 2.68 GB). This makes `zig build run` appear to hang before `Game.exe` starts.

## Design

When staging in the default link mode on Windows, create a directory junction from `<install-dir>/Data` to the repository's `Data` directory. This restores the behavior of the previous PowerShell staging script and does not require Windows Developer Mode or elevation.

Explicit `--copy-data` staging remains unchanged. Junction-creation errors must propagate rather than silently triggering a multi-gigabyte copy; copying should occur only when the caller explicitly requests it.

The implementation will remain in `tools/zig/stage.zig`. The existing staging sequence will continue to remove an old `Data` entry before creating the new junction.

## Error Handling

If junction creation fails, `stage-game` exits with an error and `zig build run` reports the failure. It must not silently change from link mode to copy mode.

## Verification

Add a focused test that stages a temporary source and destination in link mode and verifies that:

- `Data` is represented by a directory reparse point rather than a copied directory.
- A file beneath the source `Data` directory is visible through the staged path.
- Link mode does not create an independent copy.

Then run the focused test and `zig build run`, confirming staging completes promptly and the game process is launched.
