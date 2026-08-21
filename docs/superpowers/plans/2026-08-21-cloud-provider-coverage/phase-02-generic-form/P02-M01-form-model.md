# P02-M01 — form model from the catalogue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Turn a backend entry into a list of widgets, in Zig, testable without a UI.

**Dependencies:** P01-M03.

**Allowed files:** `Sources/src/CloudSync/form.zig`, `Sources/src/CloudSync/form_test.zig`, `build.zig`.

- [ ] Write the failing test over the committed fixture before writing the model.
- [ ] Implement `buildForm(catalogue, backend) -> []Field`, mapping catalogue entries to widgets: `Examples` with `Exclusive` becomes a closed droplist, `Examples` without it an editable one, `IsPassword` or `Sensitive` a masked field, everything else a text field.
- [ ] Honour `Hide` by omitting the field entirely, and `Advanced` by flagging it rather than dropping it.
- [ ] **Split basic from advanced.** s3 has 78 options and 14 basic ones; rendering all 78 is not a form, it is a wall. The advanced set must exist and must be collapsed by default.
- [ ] Carry `Help` through as the tooltip source. It is the only per-field documentation the player will ever get, and writing our own would go stale against upstream.
- [ ] Carry `Default`/`DefaultStr` as placeholder text, and mark the field so the save path knows not to persist a value equal to it.
- [ ] **No provider-specific branches.** If a field cannot be rendered from catalogue data alone, stop and report rather than adding a special case — that special case is a defect the next rclone release exposes.
- [ ] Test with `s3` (78 options, 53 vendor examples), `webdav` (15), `sftp` (48) and `drive` (52) from the fixture, asserting counts and widget kinds rather than exact labels, so an rclone update does not fail the suite spuriously.
- [ ] Commit checkpoint: `cloudsync: build a form model from the provider catalogue`.

**Evidence:** Unit tests show correct widget kinds and basic/advanced split for four backends, with an unknown option type degrading to a text field.
