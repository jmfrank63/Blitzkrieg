# P00-M04 — package permissions and handle exhaustion

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the release archive produce a runnable install — preserving the executable bit and completing at all.

**Dependencies:** P00-M03.

**Allowed files:** `tools/zig/package.zig`, `tools/zig/package_test.zig`, `build.zig`.

Found while packaging the bundled binary in P00-M03. Both defects predate this
plan and affect `Game` and every dylib too, but the bundled rclone is what
makes them ship-breaking rather than merely untidy.

- [ ] Write the failing test first: package a small fixture tree containing a `0755` file, extract it, and assert the extracted file is still executable.
- [ ] **Preserve the executable bit.** The writer emits no external file attributes, so `0755` becomes `0644` on extraction — measured. Set the version-made-by to indicate UNIX and put the file mode in the high sixteen bits of the external attributes, the standard encoding every extractor honours.
- [ ] Without this the feature ships broken: a player installing from the release zip gets a non-executable rclone, discovery reports `.not_executable`, and cloud sync is unavailable with no obvious cause. `Game` itself has the same problem today.
- [ ] **Stop holding every file handle open.** The walk opens each entry and keeps it until the end; at 63,728 entries against a 61,440 `kern.maxfilesperproc` it dies with `ProcessFdQuotaExceeded` and leaves a zero-byte archive. Open, write and close each entry in turn — no `ulimit` raise can fix a design that scales handles with file count.
- [ ] Fail loudly rather than leaving a zero-byte archive behind. A truncated output that looks like a product is worse than no output.
- [ ] Keep determinism: the same input must still produce the same package hash. Verify by packaging twice and comparing, and confirm the hash matches what P00-M03 recorded if nothing else changed.
- [ ] Confirm the extracted layout still passes the runtime verifier, and that the extracted `rclone` runs and reports its version.

**Evidence:** A packaged and extracted tree whose `rclone` is executable and runs, `package-game` completing on a host where it previously exhausted handles, and two runs producing an identical hash.
