# Captured bisync failure texts — rclone v1.75.0, Windows 11

Captured 2026-08-21 with the pinned v1.75.0 binary against local fixtures at
`C:\bk-ef` (short on purpose — the first attempt under the deep scratchpad
path failed on the session-name budget itself, see below). These are the
fixtures `engine_test.zig`'s classification cases quote; they are captures,
not inventions.

## Too many deletes

Resync three files, delete two of three on Path1, bisync:

```
ERROR : Safety abort: too many deletes (>50%, 2 of 3) on Path1 "C:\bk-ef\a\". Run with --force if desired.
NOTICE: Bisync aborted. Please try again.
NOTICE: Failed to bisync: too many deletes
```

## No prior resync

bisync on a pair that never resynced:

```
ERROR : Bisync critical error: cannot find prior Path1 or Path2 listings, likely due to critical error on prior run
ERROR : Bisync aborted. Must run --resync to recover.
NOTICE: Failed to bisync: bisync aborted
```

Note the rc-visible error is only `bisync aborted`; the cause exists only in
the log.

## Auth failed (WebDAV 401)

`rclone serve webdav --user bk --pass rightpass`, bisync with a wrong
obscured password (connection string; the url value must be quoted or its
colon is eaten by connection-string parsing):

```
ERROR : webdav root '': error reading source root directory: couldn't list files: 401 Unauthorized: 401 Unauthorized
ERROR : Bisync critical error: couldn't list files: 401 Unauthorized: 401 Unauthorized
ERROR : Bisync aborted. Must run --resync to recover.
NOTICE: Failed to bisync with 2 errors: last error was: bisync aborted
```

**The auth failure ends in the same `Must run --resync to recover` trailer.**
Classification must test cause patterns before that trailer, or a wrong
password is offered a re-pair.

## Remote unreachable (Windows connect text)

Same, against a closed port:

```
ERROR : webdav root '': error reading source root directory: couldn't list files: Propfind "http://127.0.0.1:19999/": dial tcp 127.0.0.1:19999: connectex: No connection could be made because the target machine actively refused it.
ERROR : Bisync critical error: couldn't list files: Propfind "http://127.0.0.1:19999/": dial tcp 127.0.0.1:19999: connectex: No connection could be made because the target machine actively refused it.
ERROR : Bisync aborted. Must run --resync to recover.
NOTICE: Failed to bisync with 2 errors: last error was: bisync aborted
```

Same trailer. The Windows connect refusal is `connectex: No connection could
be made because the target machine actively refused it.` — POSIX says
`connection refused` — and both sit behind Go's `dial tcp` prefix, which is
the robust pattern.

Note: the connection string, and therefore the log, carries
`user=bk,pass=<obscured>` in the filesystem name — live proof that log tails
need credential redaction before they are shown to anyone.

## Session name over budget (Windows text)

bisync under the deep scratchpad path, whose mangled pair exceeds 255 bytes:

```
NOTICE: Failed to bisync: syntax error detected in your path(s). Please check your command and try again.
        Note that on Windows, quoted paths must not have a trailing slash, [...]
        error: CreateFile <workdir>\<path1-mangled>..<path2-mangled>: The filename, directory name, or volume label syntax is incorrect.
```

On Windows the OS-level text is `The filename, directory name, or volume
label syntax is incorrect.` wrapped in bisync's canned `syntax error detected
in your path(s)`; the POSIX flavour is `file name too long` (measured on
macOS during planning). Both are classification patterns.
