# P06-M04 — sync indicator

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make sync state visible, and never trap the player behind it.

**Dependencies:** P06-M03.

**Allowed files:** `Sources/src/GameTT/MainMenu.cpp`, `Data/UI/MainMenu.xml`, `Data/Textes/UI/CloudSync`.

- [ ] Show a syncing state in the main menu, using the lower-left profile label convention (element id 21000) as the placement precedent.
- [ ] Offer skip-to-offline while a startup sync runs. The profile stays dirty and syncs later; the player is never blocked from playing.
- [ ] Surface the P02-M03 outcomes as distinct messages, not one generic failure. `.too_many_deletes` in particular is a question, not an error, and `.needs_resync` offers a repair.
- [ ] Add text keys under `Data/Textes/UI/CloudSync`; the settings screen reads labels from `Textes\Options\`, but screen text lives under `Textes\UI\`.
- [ ] Verify with `BK_AUTO_UI` and `shot`, capturing the indicator in each state.
- [ ] Commit checkpoint: `cloudsync: show sync state in the main menu`.

**Evidence:** Screenshots for syncing, done, and each failure outcome, captured headlessly at 1024x768.
