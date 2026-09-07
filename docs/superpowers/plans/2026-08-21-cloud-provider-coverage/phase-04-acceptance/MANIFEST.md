# Phase 04 — Acceptance

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Prove the coverage claim on real services and a real upgrade.

| Packet | Depends on | Owns |
|---|---|---|
| P04-M01 | P03-M03 | static-credential backends end to end |
| P04-M02 | M01 | OAuth backend and forward compatibility |

Exit: three static backends and one OAuth backend sync, and a newer rclone exposes a new provider with no game change.

P04-M01 macOS checkpoint: three static-credential services accepted end to
end through the shipped release game — MinIO (s3), `rclone serve webdav`
(ownCloud vendor), `rclone serve sftp` — each configured **entirely through
the generic form** (headless `BK_AUTO_UI` driving the real dialog: chooser,
example-cycle, typed URLs, masked secrets, Test connection to a visible
`Connection OK`), each then run through the full phase-02 cycle by the
game's own startup sync: paired, diverged both sides, converged newer-wins
in both directions, conflict loser preserved, both deletes recovered from
their trashes, remote trash a sibling of `profiles/`. Session names 82/73/81
bytes of the 241 budget. Evidence with captures in
`evidence/cloud-sync/p04-m01-backends.md` + `p04-m01/`. **Human approval
pending** — recorded in the evidence when given.

P04-M02 macOS checkpoint (forward-compatibility half): rclone
v1.76.0-beta staged over the bundled v1.75.0 with the game untouched —
the catalogue cache refetched on the version change in both directions
(stamp v1.75.0 ↔ v1.76.0, never stale), the beta's re-regioned MEGA S4
endpoint examples reached the endpoint example-cycle on screen, and a
synthetic `bkfuture` backend appended to the served catalogue was
offered, rendered and saved through the generic form — no backend
allowlist exists (the destination filter is a wrapper deny-list). The
downgrade leg: with `bkfuture` configured and v1.75.0 restored, the
credentials survive intact, the dialog opens explained and escapable
(fix `3a07ec9ad`, found by this packet: the old behaviour trapped the
chooser in the missing-catalogue retry), and the startup sync fails
classified. No current upstream release adds a real backend (v1.75→beta
adds none of the 69), so the literal new-backend fetch is the one
substitution — re-run when upstream ships one. **OAuth and human
approval pending**: needs a real Drive/Dropbox/OneDrive account and a
human at the consent screen; the procedure is in
`evidence/cloud-sync/p04-m02-oauth-forward.md`.

Findings the packet text does not carry:

- **Typing into `provider` scrambles**: the per-keystroke TEXT_CHANGED
  rebuild resets the edit cursor, so typed vendors arrive reversed
  ("Minio" → "oinM") and reshape the form mid-entry. The example-cycle
  button is the safe path (used here). **Fixed in `1a21f382b`** (owner
  approved going outside the packet allowlist): the caret is captured
  before the rebuild and restored into the same slot after, the pattern
  OnSecretEdited already used; re-verified headlessly — "Minio" typed
  through `text=` lands intact and the form filters under it.
- **No UI owns the changed-fingerprint confirmation.** Switching backends
  correctly refuses the next sync with `FingerprintChanged`, but the
  facade's transparent pair fallback fires only on `NotPaired` and no
  screen offers "pair anew" — the evidence cleared the machine-local
  `cloudsync/state/<profile>.json` by hand. **Fixed in `129dcc166`**
  (owner approved): the deliberate credentials save is the confirmation —
  after a successful save, `engine.retireMismatchedPairings` removes every
  pairing record naming a remote other than the saved document's
  fingerprint, and the next sync takes the designed NotPaired → pair
  bootstrap. The engine's gate is unchanged and still guards rotations
  arriving without a save (hand-edited documents). Failing-test-first in
  `engine_test.zig` (engine suite now 20); verified live s3 → webdav
  through the dialog with no manual state surgery. The recorded residue —
  a save landing mid-sync having its retirement overwritten by that
  run's success record — was upgraded to a finding by review and is
  **fixed in `bdd6e5868`**: the worker's post-job baseline check
  re-applies the retirement for the document that won, before any
  network and again inside the locked in-request re-check, with a
  deterministic regression (worker suite 17).
- **The P02 manifest's `text=`-cannot-carry-URLs note is stale**: the
  schedule splits an entry at its first colon only, so full
  `http://host:port` values type fine; commas remain the separator
  constraint.
- The `evidence/cloud-sync/p04-m01/` capture directory is a sibling of the
  allowlisted evidence file, following the established evidence-directory
  precedent (credentials-form, oauth-consent); recorded here rather than
  stopped over.
