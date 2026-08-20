# P07-M03 — restore confirmation and undo

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the destructive action explicit, and the recovery obvious.

**Dependencies:** P07-M02 and P04-M04.

**Allowed files:** `Sources/src/GameTT/InterfaceCloudBackups.cpp`, `Data/UI/CloudBackups.xml`, `Data/Textes/UI/CloudBackups`.

- [ ] Require a confirmation step before any restore. This is the "explicit player action" the design promises, and a single-click restore from a list row is not one.
- [ ] Default the confirmation to the `GFX`-preserving merge, and describe it in plain terms: game settings come from the backup, display settings stay as they are on this machine.
- [ ] **Warn specifically on full restore.** Adopting another machine's `GFX.Mode` and `GFX.Monitor` is survivable — an unknown resolution falls back to Auto, a missing monitor to display 0 — but both failures are silent, so the warning is the only signal the player gets.
- [ ] Offer undo after a restore, shown only when `restore_undo_available` reports a pre-restore copy exists.
- [ ] State that a restored config takes effect for display-related keys at the next launch, so a player is not left wondering why nothing changed.
- [ ] Verify headlessly: restore in merge mode, confirm local `GFX.*` unchanged in the written config, then undo and confirm the file is byte-identical to the original.
- [ ] Commit checkpoint: `settings: restore confirmation and undo`.

**Evidence:** Headless run shows the confirmation, a merge restore preserving local `GFX.*`, the full-restore warning, and a byte-identical undo.
