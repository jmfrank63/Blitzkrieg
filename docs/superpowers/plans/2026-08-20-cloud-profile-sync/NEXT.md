# Next Packet

Start at `phase-00-rc-transport/P00-M01-rc-json-client.md`.

Nothing in this plan is implemented yet. The design at
`docs/superpowers/specs/2026-08-20-cloud-profile-sync-design.md` had its
behavioural claims measured against rclone v1.75.0 on macOS arm64.

## Corrections applied after review

The first draft of this plan would have failed in two ways that testing
confirmed, and both are now designed against rather than discovered later:

- **One trash was assigned to both sides.** rclone requires `backupDir1` on
  Path1's filesystem and `backupDir2` on Path2's; a local path given as
  `backupDir2` against a remote Path2 fails the run outright with `parameter
  to --backup-dir has to be on the same remote as destination` (measured).
  There are now two trashes, local and remote — see P01-M04 and P02-M04.
- **Pairing could destroy a save.** `conflictResolve` is ignored during a
  resync, which defaults to Path1 winning and renames nothing. Measured: a
  machine holding an older save overwrote the newer cloud copy with no
  conflict file and no trash entry. Every pairing call now carries
  `resyncMode: "newer"` — see P01-M04 and P02-M01.

### Second review pass

Six more, all reproduced or confirmed in source before being designed against:

- **Trash entries were not versioned.** rclone overwrites a backup at an
  existing path, so deleting `saves/m2.sav`, recreating it and deleting it
  again left only the second version (measured). Save filenames recur every
  time — `quick.sav`, the autosaves — so this destroyed recovery copies during
  ordinary play. Both trashes are now `<trash>/<run_id>/` (P01-M04, P02-M04).
- **Config backups sat inside the synced prefix**, at
  `<remote>/profiles/<name>/config-backups/`, so bisync would have pulled the
  whole history onto every machine. `config-backups/` and `trash/` are now
  siblings of `profiles/` (P01-M03, P04-M01).
- **The credentials struct could not express WebDAV**, and conflated rclone's
  S3 `provider` with the game's protocol choice. It is now a tagged union with
  `s3` and `webdav` arms and an `s3_provider` field (P03-M01, P07-M01).
- **A restored config was overwritten by the game itself.** `config.cfg` is
  rewritten from memory at shutdown (`GameMain.cpp:1182`) and on settings OK
  (`InterfaceOptionsSettings.cpp:344`), so a live restore never survived to
  the next launch. Restores and undos are now staged to
  `config.cfg.pending-restore` and applied at startup, and acceptance tests
  across a real restart (P04-M03, P04-M04, P06-M02, P07-M03, P08-M01, P08-M04).
- **Saving credentials did not define what an omitted secret meant.** Since
  the load path withholds it, omission now explicitly preserves, with a
  separate clear action (P03-M01).
- **First pairing never created the remote directories.** bisync aborts with
  `directory not found` against an empty remote (measured, even locally), which
  is every player's first sync. `operations/mkdir` now precedes pairing
  (P02-M01).

Testing the first-pass corrections turned up a third: **the sentinel must never be seeded
on both sides.** Two independently created `.bkprofile` files differ in
modification time, and bisync then aborts the resync with `Modtime not equal
in listing` followed by `path1 and path2 are out of sync`. P01-M03 and P02-M01
check the remote before writing one.

Structural corrections in the same pass: the C ABI is now amended by whichever
packet adds an export rather than stubbed early (EXECUTION.md carries the
rule, and P02-M05, P03-M01, P03-M04, P04-M02, P04-M03, P04-M04 each own their
export path); machine-local state moved out of Path1 to `<gamedir>/cloudsync/`;
a worker thread carries every rc call because `_async` only makes the *job*
asynchronous, not the POST; `Cloud.Sync.OnSave` is declared; and the phases
were reordered so every option exists before a hook reads it.

## Still unproven

Both are Windows-first and should not be discovered late:

- the short link is a junction (`mklink /J`), and whether it can be created
  without administrator rights is asserted, not tested (P01-M01, P08-M02);
- whether rclone leaves a junction root unresolved in the session name, as it
  does a POSIX symlink, is verified on macOS only (P01-M01, P08-M02).

## Important working-tree files

The branch is `feature/cloud-profile-sync`. Only documentation exists so far.
The pinned rclone binary used for verification is not in the repository and
must be fetched per machine.
