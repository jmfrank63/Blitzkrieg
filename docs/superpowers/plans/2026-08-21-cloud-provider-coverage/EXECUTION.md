# Cloud Provider Coverage Execution Rules

## Packet protocol

Identical to `docs/superpowers/plans/2026-08-20-cloud-profile-sync/EXECUTION.md`:
pull, read exactly one packet, modify only allowed files, failing test first,
smallest compliant change, run the packet commands and the listed regressions,
commit with the packet message, record the checkpoint in the phase manifest.

The **ABI amendment rule** from that document applies unchanged: a packet that
adds an export owns `cloudsync.zig`, both `.def` files, the facade and the ABI
smoke test in the same commit.

## Commands

```bash
zig build <step> -Dtarget=aarch64-macos        -Dtest-mode=run     # macOS
zig build <step> -Dtarget=x86_64-windows-msvc  -Dtest-mode=run     # Windows only
zig build <step> -Dtarget=x86_64-linux-gnu     -Dtest-mode=run     # Linux
```

Run from the repository root only. Cross targets use `-Dtest-mode=compile` and
cannot close a runtime gate.

## Catalogue fixtures

Tests must not require a live daemon to exercise catalogue handling. Capture a
real `config/providers` reply once, commit it as a fixture, and drive the
parser and form model from it. The fixture is large — keep one trimmed copy
for unit tests and note in the manifest which rclone version produced it.

A live daemon is still required for the connection-test and acceptance
packets; those follow the existing `BK_TEST_RCLONE` convention.

## Stop conditions

Stop and report if:

- a packet cannot be satisfied without naming a specific provider in code;
- the catalogue does not carry enough information to render a field, so the
  renderer would need a hardcoded exception;
- storing a value would require persisting one of rclone's defaults;
- a secret would have to cross the load path to make the UI work;
- bundling would require modifying discovery rather than being found by it.

## Evidence format

Target and native/cross status, exact command, pass/fail summary, relevant
output, commit hash, and anything deferred. Evidence files live under
`docs/superpowers/evidence/cloud-sync/`.
