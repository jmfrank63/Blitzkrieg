# Phase 02 — Generic Credentials Form

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** One renderer for every backend, driven entirely by the catalogue.

| Packet | Depends on | Owns |
|---|---|---|
| P02-M01 | P01-M04 | form model derived from the catalogue |
| P02-M02 | M01 | form exports |
| P02-M03 | M02 | the dialog renders the model |
| P02-M04 | M03 | validation and connection test for any backend |

Exit: an arbitrary destination backend is configurable and testable with no provider-specific code.

P02-M01 macOS checkpoint: `test-cloudsync-form` 8/8, written failing-first
over the committed fixture; the full sweep stays green and
`x86_64-linux-gnu` / `x86_64-windows` compile. Commit `35428f79a`.

Measured from the fixture and asserted (empty provider, +1 for the
remote-root field in basic): s3 15 basic / 61 advanced of 78 options,
webdav 6/10 of 15, sftp 14/34 of 48, drive 5/41 of 52 — the differences
against raw counts are the configurator-hidden options (3, 0, 1, 7) and
drive's one hidden basic option. Region examples: 26 under AWS, 2 under
Wasabi, of 153 total.

Findings the packet text does not carry:

- **No Exclusive option exists in the five fixture backends** (the plan's
  "exactly one across all 69" is outside them), so the closed-droplist rule
  is covered synthetically, as the later packets already planned for the
  vendor-cleanup rule.
- **A non-text `kind` still renders `.text` by the widget rule** — the
  packet's rule names only masked/droplists/text — but `Field.kind`
  carries the catalogue classification (`boolean`, `integer`, `number`) so
  P02-M03's renderer can refine the widget without the model guessing.
- The remote-root strings (`remote_root_label`, `remote_root_help`) are
  the canonical fallback text; the renderer may localise them, and they
  are public constants so the dialog and the model cannot drift.
- Form slices borrow from the catalogue: the catalogue must outlive the
  form, which the exports in P02-M02 must arrange (the ABI serialises, so
  nothing borrowed crosses the boundary).
