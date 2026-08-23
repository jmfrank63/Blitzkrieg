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

P02-M02 macOS checkpoint: the C++ consumer failed first on the one missing
symbol, then abi 10/10 + consumer green natively and with a live rclone —
where all four real backends build across the boundary and the s3 rebuild
under AWS versus Wasabi changes the region example set (`us-east-2`
present, then absent), the same assertion the Zig tests make, repeated
across the ABI because a boundary that drops the provider argument passes
every Zig test and still renders the wrong form. Facade, form, and the
full sweep green; both cross-targets compile; `install-game` builds.
Commit `3135a2fed`.

Findings the packet text does not carry:

- **The wire format omits a per-field `advanced` flag on purpose**: the
  split into `basic` and `advanced` arrays *is* the encoding, and a flag
  that could disagree with the array a field sits in would be a second
  source of truth.
- **An unknown backend distinguishes its two causes** in the error text:
  an empty cache says "no provider catalogue is cached; fetch it first" —
  the actionable half — while a populated cache says the backend does not
  exist.
- The real s3 form under an empty provider is far too large for a stack
  buffer — 153 region examples with help text among 75 visible options —
  so the consumer's live branch reads through a 256 KiB static buffer,
  which the run proved sufficient. The required-size contract is what a
  caller without such a bound relies on.

P02-M02 review follow-up (`d76d1d8b2`): the daemon's fifteen-second
`waitReady` never observed the worker's cancel flag, so a cancel or
shutdown landing during daemon startup sat out the whole readiness window
— contradicting `destroy`'s documented bound. `waitReadyAbortable` checks
an abort signal after every probe (recording no failure: nothing is wrong
with the daemon when the caller leaves), the worker passes its cancel
flag, and a new worker test drives a never-ready daemon script and asserts
the job settles as `Cancelled` well inside the window. Worker suite is 9
now.
