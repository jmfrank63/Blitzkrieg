# P01-M01 — short link

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Give bisync a short Path1 so the install path never reaches the session name.

**Dependencies:** P00-M04.

**Allowed files:** `Sources/src/CloudSync/plan.zig`, `Sources/src/CloudSync/plan_test.zig`.

- [ ] Write the failing test: create a profile directory nested past 190 bytes, link it, and assert the link path is under 40 bytes and lists the same contents.
- [ ] Implement `linkRoot(allocator) ![]u8` — `%LOCALAPPDATA%\bk\` on Windows, `~/Library/Caches/blitzkrieg/` on macOS, `~/.cache/blitzkrieg/` on Linux — creating it when absent.
- [ ] Implement `ensureShortLink(allocator, profile_dir) ![]u8` producing `<linkRoot>/p<slot>` where slot is a small integer index. Do not use the profile name: a profile called `Panzerkommandant` puts the length straight back.
- [ ] On POSIX use `std.posix.symlink`. On Windows create a **junction**, not a symlink — symlinks need administrator rights or Developer Mode, junctions do not. **Confirmed on a real Windows box**: `New-Item -ItemType Junction -Path test -Target $env:HOMEPATH` succeeds from an unelevated prompt. Only the *creation* privilege is settled; whether rclone resolves the junction is a separate question, below. Use `DeviceIoControl` with `FSCTL_SET_REPARSE_POINT` and an `IO_REPARSE_TAG_MOUNT_POINT` buffer, falling back to `cmd /c mklink /J` only if that fails, and record which path was taken.
- [ ] **Resolve the target to an absolute path before creating the link.** Windows junctions require it — `New-Item -ItemType Junction -Target` rejects a relative path, and the `DeviceIoControl` reparse buffer wants the `\??\C:\...` NT form — while `mklink /J` silently resolves a relative target against the current directory, which is worse. The game's profile paths are built relative to the game root (`NProfile::Segment` returns `profiles\<name>\`), so canonicalise before linking rather than passing them through.
- [ ] Implement `repointShortLink(allocator, slot, new_target)` for profile switching by removing and recreating. Do not assume a link can be retargeted in place.
- [ ] Prove the property the approach rests on: rclone does **not** resolve a symlinked root, so the short name survives into the session name. Measured on macOS — a 199-byte directory reached through an 8-byte link produced a 21-byte session name — and **still unproven for Windows junctions**. This is the load-bearing half: if Windows resolves the junction back to its target before mangling, the short link buys nothing and only the P01-M02 budget check remains. Prove it here or stop and report; a redesign that keeps the profile directory itself short is the fallback, not a longer link.
- [ ] Commit checkpoint: `cloudsync: short link for the profile directory`.

**Evidence:** The test shows a >190-byte profile directory reachable through a <40-byte link on the host platform, with the junction-versus-symlink path recorded.
