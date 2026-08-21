# Cloud Provider Coverage

Cloud sync works, but it ships nothing and offers two providers. This document
covers bundling rclone with the game and offering every backend rclone can
configure as a destination, discovered at runtime rather than enumerated in
our source.

Supersedes the credentials portion of
`docs/superpowers/specs/2026-08-20-cloud-profile-sync-design.md`. Everything
else in that document — the rc transport, bisync semantics, the trash and
restore protocols — stands unchanged.

## The two problems

**Nothing is shipped.** A fresh install reports "rclone not found", so the
feature is invisible until the player installs rclone themselves.

**Two providers.** `Protocol = enum { s3, webdav }` and a three-value
`Cloud.Provider` droplist. The binary would carry 69 backends; the game offers
two of them.

## Decision

**Bundle the stock rclone binary, and build every provider screen from
rclone's own catalogue.**

The game therefore offers every backend rclone can configure, minus the
wrappers that are not cloud destinations, and confirms writability per
configuration rather than promising a supported list.

Not a trimmed build, and not librclone. Measured on v1.75.0 macOS arm64: the
official archive is 31.0 MB and extracts to 84.3 MB, of which 33.3 MB is code
and 30.4 MB is `__gopclntab` that the Go runtime requires — `strip` saves
nothing. librclone would be the same order, has no published prebuilt for any
platform, and would put a Go runtime inside the game process. A custom build
importing only `backend/s3` and `backend/webdav` would genuinely be smaller,
but it costs a Go toolchain per platform **and** it is the opposite of what we
want here: the goal is every provider, not two.

Discovery already searches the game's own directory before `PATH`
(`daemon.zig`), so bundling needs no code change to be found.

## The catalogue is the design

`config/providers` returns all 69 backends, each option fully self-describing:

```
Name  Type  Help  Required  Advanced  Examples  IsPassword
Sensitive  Hide  Default  Exclusive  FieldName  NoPrefix  Value  ValueStr
```

Measured shapes:

| backend | options | basic | oauth fields |
|---|---|---|---|
| s3 | 78 | 14 | 0 |
| webdav | 15 | 5 | 0 |
| sftp | 48 | 13 | 0 |
| drive | 52 | 5 | 3 |
| dropbox | 24 | 2 | 3 |

It goes deeper than backends: S3's own `provider` field carries **53
examples** — AWS, Alibaba, Ceph, Wasabi and the rest — so even the vendor list
inside a backend is data, not code.

That makes one generic form renderer sufficient for every provider, and it
means **a newer rclone brings new providers with no game change**. Any
provider name, field name or vendor list written into our source is a defect
that the next rclone release exposes.

## Consequences

- `Protocol` and the `Payload` union are **deleted**, replaced by
  `{ backend, options, remote_root }`. The remote root is not optional
  bookkeeping: `remoteParams` states that for S3 the bucket is "deliberately
  not here — for S3 it is a path component, carried by the alias target", so a
  schema without it would migrate the bucket into an ordinary option and route
  every sync at the account root instead of the bucket. Silent misplacement,
  not a visible error.
- **`Cloud.Provider` cannot carry the backend name at all.** `COptionSystem::Set`
  truncates any string over 12 characters to 8, and `googlecloudstorage`,
  `internetarchive` and `oracleobjectstorage` already exceed it — they would be
  stored as `googlecl`, `internet`, `oracleob`. The option is reduced to
  `OFF`/`ON`; the backend identity lives in `cloud.credentials`, and selection
  moves into the credentials dialog.
- **Not every backend is a provider, and the remainder is not a number.**
  Eleven of the 69 — `alias`, `archive`, `cache`, `chunker`, `combine`,
  `compress`, `crypt`, `hasher`, `local`, `memory`, `union` — wrap another
  remote or are not cloud destinations. (Note `overview` is *not* a backend;
  it appears in lists built by scraping rather than by asking
  `config/providers`.) The rest are **candidates**, not verified destinations:
  nothing in the catalogue states whether a backend supports the writable,
  deletable semantics bisync needs, and some are read-only or restrict
  deletion. A writable connection test decides, per configuration.
- The catalogue needs the daemon, and the daemon needs rclone — so it is
  **cached to disk** after the first successful fetch. A cold start shows the
  cached list; no cache yet is an empty list, never an error. Something must
  perform that first fetch, or a fresh install never acquires one.

  Startup is the wrong trigger, though it looks like the obvious one.
  `GameMain.cpp` reaches `NCloudSync::Available()` only inside
  `Cloud.Enabled && Cloud.Sync.OnStartup`, and a fresh profile has both off —
  so a startup bootstrap would never run for precisely the players who have no
  catalogue. The fetch happens on first need instead: opening the credentials
  dialog, which is a deliberate action where spawning a daemon is acceptable,
  plus an opportunistic refresh after a successful sync where the daemon is
  already running.
- Credentials already saved under the two-arm union must keep working. The
  migration is mechanical, since `s3` and `webdav` are backend names rclone
  itself uses.

## Widget mapping

| catalogue field | UI behaviour |
|---|---|
| `Examples` + `Exclusive` | droplist, values only |
| `Examples` without `Exclusive` | editable droplist |
| `IsPassword` or `Sensitive` | masked; never returned by the load path |
| `Advanced` | hidden behind a toggle — this is what keeps s3's 78 options usable |
| `Required` | validated before the connection test |
| remote root | not described by the catalogue at all — we supply a generic label and help |
| `Hide` | not rendered at all |
| `Default` / `DefaultStr` | placeholder, and omitted from what we store |

Storing only what the player set — never a copy of the defaults — matters for
the same reason as everything else here: a default that changes in a later
rclone release should follow the release, not our saved file.

Two storage consequences. Which fields are secret comes from catalogue
metadata, but credentials must load with the cache absent, so the secret flag
is persisted per field at save time rather than re-derived. And the existing
16 KiB caps on reading and serialising `cloud.credentials` were sized when
"endpoints and keys are hundreds of bytes at the outside" held; a backend with
dozens of set options plus an OAuth token document can exceed that, and the
read path's failure mode is to return null — silently losing the credentials.
Sizing becomes dynamic under a documented limit, and exceeding it reports.

## OAuth

`config/oauthstatus` and `config/oauthstop` exist, and `config/create` drives
rclone's interactive state machine, which is how GUI wrappers perform OAuth.
Drive, Dropbox, OneDrive and the rest are therefore reachable, but they need a
browser launch and a loopback callback, so they are their own phase rather
than a case in the generic renderer.

Refreshed tokens are rclone's to produce and ours to keep. rclone writes them
into its own config, so they must be read back with `config/get` after
authorisation and after any operation that may have refreshed one, and again
before the daemon exits — otherwise a refresh late in a session is lost with
the process.

## Size

31.0 MB fetched, 84.3 MB installed, per platform. rclone is MIT, so
redistribution is fine. The bundled binary must be signed and notarized with
the macOS app.
