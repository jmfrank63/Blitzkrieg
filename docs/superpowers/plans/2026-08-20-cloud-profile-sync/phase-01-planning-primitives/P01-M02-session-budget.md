# P01-M02 — session-name budget

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Reproduce rclone's session-name arithmetic and refuse a run that would abort on filename length.

**Dependencies:** P01-M01.

**Allowed files:** `Sources/src/CloudSync/plan.zig`, `Sources/src/CloudSync/plan_test.zig`.

- [ ] Write the failing test from the measured cases: a 249-byte session name must be rejected, and `tmp_bkp..tmp_bkremote` must be accepted.
- [ ] Implement `canonicalPath(allocator, remote) ![]u8` mirroring `bilib.CanonicalPath`: trim leading and trailing `\` and `/`, then replace every character matching `[\s\\/:?*]` with `_`.
- [ ] Implement `fsPath(allocator, path, kind) ![]u8` mirroring `bilib.FsPath`: a local path contributes its raw absolute form with a trailing separator, a named remote contributes only `name + ":" + root`. Only the local branch is expensive — this is exactly why Path2 must always be a named remote.
- [ ] Implement `sessionName(allocator, p1, p2) ![]u8` as `canonicalPath(fsPath(p1)) ++ ".." ++ canonicalPath(fsPath(p2))`.
- [ ] Define `SESSION_SUFFIX_MAX = 14` — `.path1.lst-new`, with `-old` and `-err` the same length and `.lck` shorter — and `SESSION_BUDGET = 255 - SESSION_SUFFIX_MAX`.
- [ ] Implement `checkSessionBudget(p1, p2) !void` returning `error.SessionNameTooLong` carrying the projected length, so the caller can report a number instead of "sync failed".
- [ ] Call it before every run, not only at setup: the profile name is part of Path2 and can change under a rename.
- [ ] Commit checkpoint: `cloudsync: validate the bisync session-name budget`.

**Evidence:** Unit tests cover the measured 249-byte reject, the 21-byte accept, and a boundary case at exactly 241 bytes.
