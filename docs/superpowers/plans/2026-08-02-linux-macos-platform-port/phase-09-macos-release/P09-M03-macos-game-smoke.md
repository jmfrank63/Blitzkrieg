# P09-M03 — Automate macOS Startup, Reference Scene, and Save/Load

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Run the packaged `.app` through the same automatic game checkpoints as Linux.

**Dependencies:** P09-M02.

**Allowed files:** `tools/zig/game_smoke.zig`, `build.zig`, `docs/superpowers/evidence/platform-port/macos-acceptance.md`.

- [ ] Launch `Contents/MacOS/Game` directly from the Zig controller with bundle resource root and temporary preference root; do not use `open` or shell commands.
- [ ] Require module load, SDL/metal/MSL init, miniaudio init/no-device acceptance, menu checkpoint, reference capture/hash, save/reload, normal quit, and zero renderer counts.
- [ ] Run three times on native Apple Silicon with a read-only bundle copy and confirm writes occur only under the temporary preference root.
- [ ] Record OS/CPU/GPU/scale, SDL/Metal selection, package hash, checkpoints, and generated artifact locations.
- [ ] Commit: `test: automate Apple Silicon game smoke`

**Evidence:** three native transcripts, capture/save hashes, write-root audit.
