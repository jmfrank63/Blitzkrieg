# P01-M03 — dynamic provider list

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Fill the provider option from the catalogue instead of a fixed droplist.

**Dependencies:** P01-M02.

**Allowed files:** `Data/Configs/defconf.cfg`, `Sources/src/StreamIO/OptionSystemInternal.cpp`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] Change `Cloud.Provider` from a fixed `OFF/S3/WEBDAV` droplist to a code-filled one by setting `ActionFill` to `GetCloudProviders`, following how `GetGameSpeed` and `GetDifficulty` are dispatched at `OptionSystemInternal.cpp:341`.
- [ ] Implement the fill from the cached catalogue through the facade. It must not spawn a daemon or block: an empty cache yields a list containing only `OFF`.
- [ ] Keep `OFF` as the first entry and the default, so a fresh profile still has cloud sync disabled.
- [ ] Sort the list so it is navigable — 69 entries in catalogue order is not a menu. Alphabetical, with any previously chosen backend preserved even if a newer rclone drops it.
- [ ] This file carries CP1251 comments. After editing, restore any clobbered comment lines from `git show HEAD:Sources/src/StreamIO/OptionSystemInternal.cpp`, matching by ASCII skeleton and keeping CRLF.
- [ ] Commit checkpoint: `settings: fill the provider list from the catalogue`.

**Evidence:** Headless capture shows the Cloud tab listing backends from the cached catalogue, `OFF` first, and an empty cache degrading to `OFF` alone rather than an error.
