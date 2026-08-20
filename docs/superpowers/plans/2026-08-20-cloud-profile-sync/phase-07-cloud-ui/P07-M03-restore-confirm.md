# P07-M03 — restore confirmation and undo

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the destructive action explicit, and the recovery obvious.

**Dependencies:** P07-M02 and P04-M04.

**Allowed files:** `Sources/src/GameTT/InterfaceCloudBackups.cpp`, `Data/UI/CloudBackups.xml`, `Data/Textes/UI/CloudBackups`.

- [ ] Require a confirmation step before any restore. This is the "explicit player action" the design promises, and a single-click restore from a list row is not one.
- [ ] Default the confirmation to the `GFX`-preserving merge, and describe it in plain terms: game settings come from the backup, display settings stay as they are on this machine.
- [ ] **Warn specifically on full restore.** Adopting another machine's `GFX.Mode` and `GFX.Monitor` is survivable — an unknown resolution falls back to Auto, a missing monitor to display 0 — but both failures are silent, so the warning is the only signal the player gets.
- [ ] Offer undo after a restore, shown only when `restore_undo_available` reports a pre-restore copy exists.
- [ ] State that the restore is staged and takes effect at the next launch, and that settings changed in the meantime will not survive it. The game rewrites `config.cfg` from memory at shutdown (`GameMain.cpp:1182`) and on settings OK (`InterfaceOptionsSettings.cpp:344`), which is exactly why the restore is staged rather than applied live — see P04-M03.
- [ ] Verify headlessly across a **restart**, not within one session: restore in merge mode, exit normally, relaunch, and confirm the restored values are present and local `GFX.*` unchanged. A same-session check would pass while the feature is broken.
- [ ] Then undo and confirm the file is byte-identical to the original, again across a relaunch.
- [ ] Commit checkpoint: `settings: restore confirmation and undo`.

**Evidence:** Headless run shows the confirmation, a merge restore preserving local `GFX.*`, the full-restore warning, and a byte-identical undo.
