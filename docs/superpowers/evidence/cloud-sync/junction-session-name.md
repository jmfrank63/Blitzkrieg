# Windows: rclone does not resolve a junction root

Settles the assumption P01-M01 and P08-M02 were both blocked on. Run on a real
Windows machine against rclone v1.75.0, unelevated.

## Setup

`C:\bk\p0` is a junction created with `mklink /J` pointing at
`C:\Users\<user>\Panzerkommandant\bisync-probe\p0`. The marker string
`Panzerkommandant` exists only in the junction's **target**, never in the path
handed to rclone, so its presence or absence in the bisync state filenames is
the whole answer.

```powershell
cmd /c mklink /J "C:\bk\p0" "$env:USERPROFILE\Panzerkommandant\bisync-probe\p0"
rclone bisync "C:\bk\p0" "C:\bk\remote" --workdir "C:\bk\work" --resync -vv
```

## Result: PASS

```text
C__bk_p0..C__bk_remote.path1.lst
C__bk_p0..C__bk_remote.path2.lst
```

Session name `C__bk_p0..C__bk_remote`, 22 bytes. No `Panzerkommandant`.

## Mechanism

The debug log shows the canonicalisation and the trim that follows it:

```text
fs cache: renaming cache item "C:\bk\p0" to be canonical "//?/C:/bk/p0"
Lock file created: C:\bk\work\C__bk_p0..C__bk_remote.lck
```

rclone canonicalises to the `\\?\` long-path form for its own cache, but
`bilib.FsPath` strips that prefix again before mangling:

```go
if runtime.GOOS == "windows" {
    path = strings.ReplaceAll(path, "/", slash)
    path = strings.TrimPrefix(path, `\\?\`)
}
```

so the session name is built from `C:\bk\p0`, the path as given. The junction is
never dereferenced. This matches the macOS symlink behaviour measured earlier (a
199-byte directory reached through an 8-byte link produced a 21-byte session
name), so the short link behaves the same way on both platforms.

## Budget note

A Windows drive letter costs three characters in the session name (`C:\` →
`C__`); separators remain one each. The 241-byte budget in P01-M02 is unaffected.

## What this does not prove

Both sides were empty — `There was nothing to transfer`, `Got 0 results for
resync`. Session naming is settled; data actually moving through a junction is
not exercised here. P01-M01's test must carry a file across the link, and
P08-M02 must confirm it in the shipped build.
