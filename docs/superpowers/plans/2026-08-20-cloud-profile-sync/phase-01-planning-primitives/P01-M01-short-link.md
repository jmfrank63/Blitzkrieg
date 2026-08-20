# P01-M01 — short link

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Give bisync a short Path1 so the install path never reaches the session name.

**Dependencies:** P00-M04.

**Allowed files:** `Sources/src/CloudSync/plan.zig`, `Sources/src/CloudSync/plan_test.zig`.

- [ ] Write the failing test: create a profile directory nested past 190 bytes, link it, and assert the link path is under 40 bytes and lists the same contents.
- [ ] **Move a real file across the link**, not just list through it. The Windows probe that settled session naming ran with both sides empty, so data movement through a junction is the half still unexercised; a save written on one side must be readable on the other.
- [ ] Implement `linkRoot(allocator) ![]u8` — `%LOCALAPPDATA%\bk\` on Windows, `~/Library/Caches/blitzkrieg/` on macOS, `~/.cache/blitzkrieg/` on Linux — creating it when absent.
- [ ] Implement `ensureShortLink(allocator, profile_dir) ![]u8` producing `<linkRoot>/p<slot>` where slot is a small integer index. Do not use the profile name: a profile called `Panzerkommandant` puts the length straight back.
- [ ] On POSIX use `std.posix.symlink`. On Windows create a **junction**, not a symlink — symlinks need administrator rights or Developer Mode, junctions do not. **Confirmed on a real Windows box**: `New-Item -ItemType Junction -Path test -Target $env:HOMEPATH` succeeds from an unelevated prompt. Only the *creation* privilege is settled; whether rclone resolves the junction is a separate question, below. Use `DeviceIoControl` with `FSCTL_SET_REPARSE_POINT` and an `IO_REPARSE_TAG_MOUNT_POINT` buffer, falling back to `cmd /c mklink /J` only if that fails, and record which path was taken.
- [ ] **Resolve the target to an absolute path before creating the link.** Windows junctions require it — `New-Item -ItemType Junction -Target` rejects a relative path, and the `DeviceIoControl` reparse buffer wants the `\??\C:\...` NT form — while `mklink /J` silently resolves a relative target against the current directory, which is worse. The game's profile paths are built relative to the game root (`NProfile::Segment` returns `profiles\<name>\`), so canonicalise before linking rather than passing them through.
- [ ] Implement `repointShortLink(allocator, slot, new_target)` for profile switching by removing and recreating. Do not assume a link can be retargeted in place.
- [ ] Prove the property the approach rests on: rclone does **not** resolve a symlinked root, so the short name survives into the session name. **Settled on both platforms** — macOS symlink: a 199-byte directory through an 8-byte link gave a 21-byte session name; Windows junction: `C:\bk\p0` pointing at a deep target gave `C__bk_p0..C__bk_remote`, with the marker in the target appearing nowhere. `bilib.FsPath` strips the `\\?\` prefix rclone canonicalises to and mangles the path as given, so the junction is never dereferenced. See `docs/superpowers/evidence/cloud-sync/junction-session-name.md`. Re-assert it here as a regression test rather than treating it as open.
- [ ] Commit checkpoint: `cloudsync: short link for the profile directory`.

**Evidence:** The test shows a >190-byte profile directory reachable through a <40-byte link on the host platform, with the junction-versus-symlink path recorded.
