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

Findings the packet text does not carry:

- **Typing into `provider` scrambles**: the per-keystroke TEXT_CHANGED
  rebuild resets the edit cursor, so typed vendors arrive reversed
  ("Minio" → "oinM") and reshape the form mid-entry. The example-cycle
  button is the safe path (used here); a fix belongs to a dialog-owning
  packet (defer the rebuild to focus loss).
- **No UI owns the changed-fingerprint confirmation.** Switching backends
  correctly refuses the next sync with `FingerprintChanged`, but the
  facade's transparent pair fallback fires only on `NotPaired` and no
  screen offers "pair anew" — the evidence cleared the machine-local
  `cloudsync/state/<profile>.json` by hand. A player switching services
  today is stuck at a failed sync until something owns that flow.
- **The P02 manifest's `text=`-cannot-carry-URLs note is stale**: the
  schedule splits an entry at its first colon only, so full
  `http://host:port` values type fine; commas remain the separator
  constraint.
- The `evidence/cloud-sync/p04-m01/` capture directory is a sibling of the
  allowlisted evidence file, following the established evidence-directory
  precedent (credentials-form, oauth-consent); recorded here rather than
  stopped over.
