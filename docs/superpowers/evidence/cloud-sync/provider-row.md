# Provider row — end to end through the shipped game

Measured 2026-08-31 on Apple Silicon macOS (Darwin 25.6.0), the already-staged
release build (`zig build install-game -Dtarget=aarch64-macos --release=fast`,
unchanged for this packet), run from `zig-out/game/macos/arm64/release`,
bundled rclone v1.75.0. Profile `P06` (`./Game -windowed -profile=P06`),
created fresh for this packet with `rm -rf profiles/P06; mkdir -p
profiles/P06/saves`; no `profiles/P04` exists on this machine (only
`Johannes`, `USSR` and the earlier test profile `P05`), so the seed saves are
copied from `profiles/Johannes/saves` instead — three `.sav` files, all
copied (`Mission Start Auto.sav`, `USSR Chapter Start Auto.sav`, `USSR
Mission Start Auto.sav`; at most four is the rule, and there were only three
available). Every run drove the shipped game headlessly with `BK_AUTO_UI`;
verification against the remote used the independent client
`rclone --config /tmp/bk-p04/rc.conf` (remotes `minio:`, `dav:`, `sftp:`),
never the game's own daemon. Captures live in `provider-row/`.

## The corrected harness route

The plan's route `60:settings,120:msg=10011` never lands on the tabbed
settings screen — confirmed again here, exactly as Task 4 found it: `msg =
10005` (`"settings"`) only switches the main menu to its `E_OPTIONS`
sub-state (a plain "Settings / Cutscenes / Credits / Load Mod / Back" list),
and `msg=10011` is not handled there. Every schedule below instead uses the
working route at this build's default `-windowed` 1440×900:

```
40:var=notransition=1,60:settings,120:click=1067x341,240:msg=10011,...
```

`click=1067x341` is a real click on the submenu's "Settings" button (window
pixel coordinates read off the rendered capture, not derived from a linear
scale), landing on `CInterfaceOptionsSettings`; `msg=10011` then switches
that screen to the Cloud division. This is the only route used for every
capture in this file.

## Counting the row's steps

The Provider row's list is `Off` plus the catalogue's candidates, sorted
alphabetically. Candidates are `rclone config providers`' 69 backends minus
the eleven wrapper backends (`alias archive cache chunker combine compress
crypt hasher local memory union`) — 58 candidates. Counted this way, `s3` is
candidate 44 (`key=RIGHT` ×44 from `Off`), and `webdav` is candidate 56
(`key=RIGHT` ×56 from `Off`, or `key=LEFT` ×12 back from `webdav` to `s3` —
shorter, and what Step 2 uses).

Verified directly: from a freshly created `P06` (`Cloud.Provider=Off`), 44
`key=RIGHT` presses at a 24-frame cadence with a verifying `shot` every ten
presses landed on exactly `azureblob`(1) → `fichier`(10) → `hidrive`(20) →
`mailru`(30) → `protondrive`(40) → `s3`(44), matching the computed
alphabetical position at every checkpoint. A denser cadence is unreliable —
an early attempt at `key=RIGHT` ×44 spaced 8 frames apart glitched off the
Cloud tab entirely (landed on the Video tab with a stray Help overlay open),
and even a 24-frame cadence with no intermediate `shot` checkpoints drifted
off by a few steps over a run this long. The periodic checkpoint `shot` is
not just an evidence nicety here — it is what kept the count honest. All
`key=RIGHT`/`key=LEFT` sequences below use this checkpointed 24-frame
cadence.

## Step 1 — the full cycle through the row

```
cd zig-out/game/macos/arm64/release
rm -rf profiles/P06; mkdir -p profiles/P06/saves
cp profiles/Johannes/saves/*.sav profiles/P06/saves/
```

**1. Off: no indicator, no cloud trace.**

```
BK_AUTO_UI="40:var=notransition=1,200:shot,220:exit" ./Game -windowed -profile=P06 2>&1 | grep -i 'cloud sync'
```

The grep matched nothing — no cloud-sync trace line at all, matching the
Off-state expectation. Capture `off-tab.png` (frame 200): the main menu,
`Profile: P06` at the lower left, no cloud indicator anywhere.

**2. Choose `s3` through the tab, open Config..., Test, OK.**

```
BK_AUTO_UI="40:var=notransition=1,60:settings,120:click=1067x341,240:msg=10011,300:shot,320:key=RIGHT,344:key=RIGHT,368:key=RIGHT,392:key=RIGHT,416:key=RIGHT,440:key=RIGHT,464:key=RIGHT,488:key=RIGHT,512:key=RIGHT,536:key=RIGHT,640:shot,650:key=RIGHT,674:key=RIGHT,698:key=RIGHT,722:key=RIGHT,746:key=RIGHT,770:key=RIGHT,794:key=RIGHT,818:key=RIGHT,842:key=RIGHT,866:key=RIGHT,970:shot,980:key=RIGHT,1004:key=RIGHT,1028:key=RIGHT,1052:key=RIGHT,1076:key=RIGHT,1100:key=RIGHT,1124:key=RIGHT,1148:key=RIGHT,1172:key=RIGHT,1196:key=RIGHT,1300:shot,1310:key=RIGHT,1334:key=RIGHT,1358:key=RIGHT,1382:key=RIGHT,1406:key=RIGHT,1430:key=RIGHT,1454:key=RIGHT,1478:key=RIGHT,1502:key=RIGHT,1526:key=RIGHT,1630:shot,1640:key=RIGHT,1664:key=RIGHT,1688:key=RIGHT,1712:key=RIGHT,1816:shot,1826:msg=10013,1986:msg=10021,2286:shot,2306:ok,2386:ok,2426:exit" ./Game -windowed -profile=P06
```

Every scheduled action fired (trace confirmed 44 `key=RIGHT`, both `msg=`
opens, both `shot`s, both `ok`s, `exit`). Capture `s3-tab.png` (frame 1816):
Cloud tab, `Provider  s3`, all four timing rows (`Sync on startup ON` — set
in an earlier round of this same investigation and left on —, `Sync after
saving OFF`, `Sync on exit OFF`, `Back up settings after sync OFF`) and both
`Config...`/`Backups...` buttons. This is the expected, working half of
Step 1.2.

**The `Test connection` / `OK` half did not reach `Connection OK`** — see
the Findings section below; this is the central discovery of this packet,
not a harness slip. Verbatim trace line, reproduced identically across three
independent runs (this run, a repeat with the same schedule, and a minimal
isolated repro with no `key=RIGHT` navigation at all):

```
cloud credentials: connection test failed: unknown: error in ListJSON: operation error S3: ListBuckets, https response error StatusCode: 200, RequestID: , HostID: , deserialization failed, failed to decode response body, XML syntax error on line 71: attribute name without = in element
```

Capture `test-failed.png` (frame 2286, named for what it actually shows
rather than the brief's anticipated `test-ok.png`): the credentials dialog
on `Service: s3` with `provider`, `env_auth`, `access_key_id`,
`secret_access_key`, `region`, `endpoint`, `location_constraint` all
rendered **blank** — no pre-filled values, not even the masked placeholder a
stored secret should show — and the status line reads `Cloud: sync failed -
changes will sync later`.

**3. Sync on startup ON, relaunch: the pairing runs.**

Because Step 1.2's `Test`/`OK` sequence saves whatever the (blank) dialog
holds, it silently overwrote the real, working `profiles/cloud.credentials`
with an empty document (`options: {}` — see Findings). The document was
restored byte-for-byte from this session's own earlier inspection of it
before continuing:

```
{"backend":"s3","remote_root":"bk-saves","fingerprint":"s3:bk-saves#07fd83633d72a84973f18c0c3cb25832499c39ca97952292ddea570da84d51e2","options":{"provider":"Minio","region":"us-east-1","endpoint":"http://127.0.0.1:19100","access_key_id":"bkadmin","secret_access_key":"Zdh2Vc1PInJUKoBJ2bAF6Lmy"},"secret_options":["access_key_id","secret_access_key"],"password_options":[],"rclone_path":null}
```

With the real credentials restored and `Cloud.Provider=s3` confirmed still
set on the row (`InstantApply` — the row's own value persists across a raw
`exit`, no `OK` required), the startup-sync half of Step 1 was run exactly
as the brief specifies, unaffected by the dialog bug because the real sync
path reads `profiles/cloud.credentials` directly (`Worker.applyCredentials`,
"run at every job start"), never the dialog's in-memory form:

```
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Sync.OnStartup=ON,130:exit" ./Game -windowed -profile=P06
BK_AUTO_UI="900:exit" ./Game -windowed -profile=P06 2>&1 | grep -i 'cloud sync'
```

```
cloud sync: startup sync begun for "P06"
cloud sync: sync finished (paired)
```

```
rclone --config /tmp/bk-p04/rc.conf lsl minio:bk-saves/profiles/P06 --max-depth 3
```

```
        4 2026-08-31 14:05:10.210888854 .bkprofile
  3284098 2026-08-31 12:58:33.460689957 saves/ Mission Start Auto.sav
   307056 2026-08-31 12:58:33.462761416 saves/USSR Chapter Start Auto.sav
  3924647 2026-08-31 12:58:33.467284687 saves/USSR Mission Start Auto.sav
```

All three saves and the `.bkprofile` sentinel landed on the remote, and
`cloudsync/state/P06.json` exists afterward. **The core feature — the row
choosing `s3`, the saved real credentials pairing a fresh profile through
the shipped startup sync — works.** The failure is scoped to the
dialog's re-open-and-test path (below), not to the row or to the sync
engine.

## Step 2 — the unconfigured indicator

```
BK_AUTO_UI="40:var=notransition=1,120:set=Cloud.Provider=webdav,130:exit" ./Game -windowed -profile=P06
BK_AUTO_UI="200:shot,220:exit" ./Game -windowed -profile=P06 2>&1 | grep -i 'cloud sync'
```

```
cloud sync: provider chosen but not set up
```

Capture `unconfigured.png` (frame 200): main menu, lower-left reads `Cloud:
storage not set up - Settings > Cloud > Config...` — matches the brief's
expectation exactly.

Setting it back to `s3` through the settings screen, at the same 24-frame
checkpointed cadence (`key=LEFT` ×12 from `webdav`, per the counting above):
the first attempt at this specific 12-step walk overshot by one step (a
`shot` checkpoint at LEFT-10 read `seafile` instead of the expected `sftp`,
and LEFT-12 read `quatrix` instead of `s3` — the count needs a checkpoint at
this length too, not just at 44). Corrected with one more `key=RIGHT` and
verified before saving:

```
BK_AUTO_UI="40:var=notransition=1,60:settings,120:click=1067x341,240:msg=10011,300:shot,320:key=RIGHT,420:shot,440:ok,500:cancel,560:shot,600:exit" ./Game -windowed -profile=P06
```

Capture (frame 420): Cloud tab, `Provider  s3`, confirmed before `OK`.
After `ok` (closing the settings screen) and `cancel` (returning from the
`OPTIONS` submenu to the true main menu), capture `cleared.png` (frame 560):
main menu, lower-left shows only `Profile: P06` and the version string — no
indicator. No `cloud sync: provider chosen but not set up` line fired after
the `ok`, confirming the indicator's clearing is real, not just a stale
screenshot.

## The entry-0 result, restated (Task 4)

Task 4's own acceptance testing found that with the catalogue cache
deliberately moved aside (`cloudsync/providers.json` absent) and
`Cloud.Provider` set directly to `sftp` in a profile's `config.cfg` (a value
not present in the — at that point unavailable — catalogue), opening the
settings screen and pressing `OK` left `Cloud.Provider=sftp` **untouched** in
the saved config, rather than collapsing to entry 0 (`Off`) the way an
absent/unmatched value would if the row's list construction dropped it. The
row's own current value and the saved credential's backend are kept in the
drop list even when the catalogue is empty or does not (yet) name them —
confirmed again here indirectly: `Cloud.Sync.OnStartup=ON` and
`Cloud.Provider=s3` both survived multiple raw `exit`s (no `OK`) across this
packet's many runs without ever reverting to a default.

## Findings

- **Reopening `Config...` on an already-configured backend blanks the form,
  and the first `Test connection` (or `OK`) silently overwrites the real
  credentials with an empty document.** Reproduced three times: the full
  Step 1.2 schedule above (twice, identical trace both times) and a minimal
  isolated repro (`set=Cloud.Provider=s3` directly, then `msg=10013,
  msg=10021, cancel` — no row navigation at all, same failure). Also
  reproduced on `P05`, which the earlier P04-M01 packet had already paired.
  `InterfaceCloudCredentials.cpp`'s `RebuildForm()` has code that looks
  designed to do this correctly — `else if ( szBackend == szStoredBackend )`
  copies every `storedOptions` entry into the matching field, and marks
  masked (secret) fields `bStoredSecret` — but in practice every field,
  secret and non-secret alike, renders and saves blank. `BeginConnectionTest()`
  calls `SaveCredentials()` *before* probing ("the probe reads the saved
  document, so what is typed must be saved first"), and `SaveCredentials()`
  treats a blank, untouched field as "the player unset it" for non-secret
  options (`if (szValue.empty() ...) continue;` — nothing preserves the
  stored value the way the secret path's `bStoredSecret` name-only send
  does). The visible daemon-side symptom is `cloudsync/rclone.conf` gaining
  `[bkraw] type = s3` with no `provider` (confirmed in `cloudsync/rcd.log`:
  `NOTICE: s3: s3 provider "" not known - please set correctly`), and the
  saved `profiles/cloud.credentials` collapsing to
  `{"backend":"s3","remote_root":"","options":{},...}`. This is new: P04-M01's
  own dialog evidence always typed a backend fresh through the chooser and
  never reopened an already-saved one, and this exact reopen-on-the-row path
  (`InterfaceCloudCredentials.cpp`'s `ProviderRowValue()`/prefill-on-open
  logic) is this plan's own `a3ddec5f9` ("the credentials dialog sets up the
  row's backend"), landed the same day as this evidence run. **Not fixed
  here** — Task 7's brief scopes this packet to evidence and docs only, and
  the brief's own expectation (`Expect cloud credentials: connection test ok`)
  did not hold; recovery was to restore the exact original
  `cloud.credentials` document from this session's own earlier inspection of
  it (recorded above) rather than re-typing through the form. A player who
  opens `Config...` on an already-working backend just to look, and reflexively
  hits `OK` or `Test connection`, loses their saved credentials with no
  warning.
- **Side effect: `P05`'s pairing record was retired.** The credential wipe
  above rotated the stored fingerprint (from `s3:bk-saves#07fd8363...` to
  `s3:#6e88edc9...`, since an empty `remote_root`/`options` hashes
  differently), and per `129dcc166`'s designed behaviour a credentials save
  naming a different fingerprint retires pairing records that named the old
  one — `cloudsync/state/P05.json` (present at the start of this session,
  `146` bytes, from P04-M01's own earlier work) is gone after this packet's
  runs, with no `retired`-style archive found under `cloudsync/`. This is
  the system correctly reacting to what looked like a real credential
  rotation; restoring the byte-identical original document afterward did
  not restore the retired record (retirement does not appear to be
  reversible by restoring old bytes — a new pairing dialogue is what the
  design intends here). `P05`'s saves on disk are untouched; only the
  already-paired fast path is gone, and `P05` would simply re-pair
  (`NotPaired → pair`) on its next startup sync, same as any other
  fingerprint change.
- **The Provider row's `key=RIGHT`/`key=LEFT` navigation needs a checkpoint
  `shot` roughly every ten steps to stay accurate over a long walk.** At an
  8-frame cadence, 44 presses glitched off the Cloud tab entirely. At a
  24-frame cadence with no intermediate checkpoints, a 44-step walk drifted
  (one run landed on `shade`, three past `s3`); the same 24-frame cadence
  with a verifying `shot` every ten presses landed exactly on the computed
  alphabetical position at every checkpoint, repeatably. Also: `Cloud.Provider`
  is `InstantApply` and persists to `config.cfg` on a raw process `exit` with
  no `OK` needed — a launch's starting row position is always wherever the
  previous launch left it, not `Off`, unless explicitly reset with
  `set=Cloud.Provider=Off`. Both facts are harness/evidence-gathering notes,
  not game defects, and are recorded here for whoever next drives this UI
  headlessly.

## Human approval

**Pending.**
