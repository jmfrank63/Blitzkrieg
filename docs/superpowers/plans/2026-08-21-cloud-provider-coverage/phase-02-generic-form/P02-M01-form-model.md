# P02-M01 — form model from the catalogue

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Turn a backend entry into a list of widgets, in Zig, testable without a UI.

**Dependencies:** P01-M04.

**Allowed files:** `Sources/src/CloudSync/form.zig`, `Sources/src/CloudSync/form_test.zig`, `build.zig`.

- [ ] Write the failing test over the committed fixture before writing the model.
- [ ] Implement `buildForm(catalogue, backend, selected_provider) -> []Field`: `Examples` with `Exclusive` becomes a closed droplist, `Examples` without it an editable one, `IsPassword` or `Sensitive` a masked field, everything else text. `Advanced` flags a field rather than dropping it.
- [ ] **Filter options and examples by `Provider`, and rebuild the form when the selected provider changes.** Both carry it, and the form is wrong without it: s3 has 51 vendors behind its `provider` option and 664 provider-tagged examples, so a Wasabi user shown AWS regions is being offered values that do not exist. The selected provider is itself just an option value, so this is a re-derivation, not a special case.
- [ ] Use `catalogue.matchProvider` from P01-M01 for every filtering decision. **Do not reimplement the expression rules**: a comma list matching any member, a leading `!` negating, and either side being empty matching everything are subtle enough that a second copy will drift from the first.
- [ ] Test across at least two S3 vendors — AWS and one non-AWS — asserting the region examples differ.
- [ ] **`Hide` is a bitmask, not a boolean.** Values observed: `{0: 915, 3: 36, 2: 13, 1: 4}`. Bit 1 hides from the command line and bit 2 from the configurator; omit a field only when the **configurator** bit is set. Dropping everything non-zero would wrongly hide the four `Hide=1` options. Test `1` as visible and `2` and `3` as hidden.
- [ ] **Split basic from advanced, collapsed by default.** s3 has 78 options against 14 basic ones; rendering all 78 is a wall, not a form.
- [ ] Carry `Help` through as the tooltip source — it is the only per-field documentation the player gets, and writing our own would go stale against upstream.
- [ ] Carry `Default`/`DefaultStr` as placeholder text and mark the field so the save path knows not to persist a value equal to it.
- [ ] **Surface the remote root as a field, with a label we own.** The schema carries it separately from options (P01-M02) and the player still has to type it, but the catalogue does **not** generally describe the remote path — no label, no help, no requiredness. Supply a generic label and help text. **Leave it optional at form validation** — deciding which backends need one would require exactly the per-backend knowledge this plan forbids, and the catalogue does not supply it. The writability test in P02-M04 is what discovers that a given configuration needs a root, and it can say so with a real error from the service rather than a guess from us.
- [ ] **No provider-specific branches.** If a field cannot be rendered from catalogue data alone, stop and report rather than adding a special case.
- [ ] Test `s3` (78 options, 53 vendor examples), `webdav` (15), `sftp` (48) and `drive` (52), asserting widget kinds and counts rather than exact labels so an rclone update does not fail the suite spuriously.
- [ ] Commit checkpoint: `cloudsync: build a form model from the provider catalogue`.

**Evidence:** Unit tests show correct widget kinds and the basic/advanced split for four backends, the remote-root field present, and an unknown option type degrading to text.
