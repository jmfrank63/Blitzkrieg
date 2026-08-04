# Core Platform Abstraction Execution Rules

## Packet protocol

1. Pull `main` with `git pull --ff-only` and require a clean worktree.
2. Read this plan's README, the phase manifest, and exactly one packet.
3. Confirm the packet dependencies are committed and their evidence gates passed.
4. Modify only packet-allowed files. Stop on overlap with unrelated user changes.
5. Write the failing test or audit fixture first and run the exact failing command.
6. Implement the smallest contract-compliant change.
7. Run the packet commands and all listed regression gates.
8. Run `git diff --check`, inspect `git status --short`, commit with the packet message, and push.
9. Record the commit and evidence in the phase manifest before moving to the next packet.

## Cross-platform command policy

Windows native:

```powershell
zig build <step> -Dtarget=x86_64-windows-msvc -Dtest-mode=run
```

Linux native:

```bash
zig build <step> -Dtarget=x86_64-linux-gnu -Dtest-mode=run
```

macOS native:

```bash
zig build <step> -Dtarget=aarch64-macos -Dtest-mode=run
```

Cross-compilation always changes `run` to `compile`. A packet cannot claim runtime completion from a cross target.

## Stop conditions

Stop and report if:

- an ABI field must be reordered or removed;
- a native type must cross the ABI;
- SDL ownership would move away from the main-thread runtime;
- a packet requires a new third-party runtime dependency;
- protocol, save, control-ID, or gameplay behavior would change;
- a required native test host is unavailable;
- an allowed file contains unrelated user edits that cannot be preserved.

Compile errors, missing wrappers, case-sensitive includes, target link failures, and incomplete stubs are implementation work, not architectural blockers.

## Evidence format

Each completed packet records:

- target and native/cross status;
- exact command;
- pass/fail summary;
- relevant output or fixture hashes;
- commit hash;
- any remaining allowlist entries owned by later packets.
