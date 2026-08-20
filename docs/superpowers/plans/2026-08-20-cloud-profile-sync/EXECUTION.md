# Cloud Profile Sync Execution Rules

## Packet protocol

1. Pull `main` with `git pull --ff-only` and require a clean worktree.
2. Read this plan's README, the phase manifest, and exactly one packet.
3. Confirm the packet dependencies are committed and their evidence gates passed.
4. Modify only packet-allowed files. Stop on overlap with unrelated user changes.
5. Write the failing test first and run the exact failing command.
6. Implement the smallest contract-compliant change.
7. Run the packet commands and all listed regression gates.
8. Run `git diff --check`, inspect `git status --short`, commit with the packet message, and push.
9. Record the commit and evidence in the phase manifest before moving to the next packet.

## Cross-platform command policy

macOS native:

```bash
zig build <step> -Dtarget=aarch64-macos -Dtest-mode=run
```

Windows native:

```powershell
zig build <step> -Dtarget=x86_64-windows-msvc -Dtest-mode=run
```

Linux native:

```bash
zig build <step> -Dtarget=x86_64-linux-gnu -Dtest-mode=run
```

`zig build` runs from the repository root only; running it from an install directory aborts with a FileNotFound panic. Cross-compilation always changes `run` to `compile`, and a packet cannot claim runtime completion from a cross target. Windows MSVC C++ cannot be cross-built from macOS — Windows packets are verified on the Windows machine.

## rclone test fixture

Packets that need a live daemon use a pinned binary, not whatever is on PATH:

```bash
rclone rcd --rc-addr 127.0.0.1:<port> --rc-user <user> --rc-pass <pass> \
           --rc-serve=false --log-file <log> --log-level INFO
```

Two rules learned the hard way:

- Keep fixture paths short. bisync mangles both canonical paths into one state
  filename and dies past 255 bytes, so a fixture under a long scratch path
  fails for reasons that have nothing to do with the packet.
- rc error replies are terse (`{"error": "bisync aborted", "status": 500}`).
  The actionable detail is in the log, or in `output.output` of a `job/status`
  reply for an `_async` call. Always capture one of the two.

## Headless UI verification

UI packets are driven with `BK_AUTO_UI` rather than by hand — see the project
memory for the full action list. Always schedule `40:var=notransition=1`
first, or the curtain transition swallows every injected message. `shot` dumps
raw RGBA at game resolution.

## Stop conditions

Stop and report if:

- a packet would let a sync destroy a save;
- `force: true` appears necessary;
- `config.cfg` must enter the sync set;
- a credential must pass through the option system or `config.cfg`;
- bisync must be replaced by hand-written diff logic;
- the rclone binary must be bundled or auto-downloaded to make a gate pass;
- a required native test host is unavailable;
- an allowed file contains unrelated user edits that cannot be preserved.

Compile errors, missing wrappers, JSON shape mismatches, and incomplete stubs
are implementation work, not architectural blockers.

## Evidence format

Each completed packet records:

- target and native/cross status;
- exact command;
- pass/fail summary;
- relevant output, rc reply, or fixture hashes;
- commit hash;
- any behaviour deferred to a later packet.

Evidence files live under `docs/superpowers/evidence/cloud-sync/`.
