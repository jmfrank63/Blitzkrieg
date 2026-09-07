# P01-M04 — provider selection and destination filtering

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Let the player choose any backend without pushing its name through a legacy option, and stop offering backends that are not destinations.

**Dependencies:** P01-M03.

**Allowed files:** `Data/Configs/defconf.cfg`, `Sources/src/CloudSync/catalogue.zig`, `Sources/src/CloudSync/catalogue_test.zig`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] **`Cloud.Provider` must not carry the backend name.** `COptionSystem::Set` truncates any string over 12 characters to 8 (`OptionSystemInternal.cpp:186-192`), and `googlecloudstorage`, `internetarchive` and `oracleobjectstorage` all exceed it — they would be stored as `googlecl`, `internet`, `oracleob` and never resolve. The count grows every time rclone adds a long name.
- [ ] Reduce `Cloud.Provider` to a short fixed state the option system can hold — `OFF` and `ON` — and keep the backend identity in `cloud.credentials`, which has no such limit. Selection happens in the credentials dialog, not the options list.
- [ ] **Distinguish a configurable candidate from a verified destination, and do not claim a count of either.** Filtering wrappers says what is *not* offerable; it does not establish that the remainder supports writable bisync. Some backends are read-only or have constrained write and delete behaviour, and no catalogue field states this.
- [ ] Implement `isCandidate(backend)`, excluding the eleven backends that wrap another remote or are not cloud destinations: `alias`, `archive`, `cache`, `chunker`, `combine`, `compress`, `crypt`, `hasher`, `local`, `memory`, `union`. Verified from `config/providers` on v1.75.0 — note that `overview` appears in some derived lists but is **not** a backend, so a list built by scraping rather than by asking rclone will be wrong.
- [ ] Treat everything else as a *candidate*, offered to the player, and let the writable connection test in P02-M04 decide whether it is a usable destination. A backend that lists but cannot write must fail there with a clear reason, not at the first sync.
- [ ] Derive the filter from catalogue data where possible, and where it must be a list, keep that list in one named place with a comment explaining the criterion — this is one of the plan's declared exceptions to "no provider names in source".
- [ ] Preserve a backend already configured in a profile even if a newer rclone drops it or the filter changes; a working configuration must not vanish because of our list.
- [ ] Sort the offered list alphabetically. Catalogue order is not a menu.
- [ ] Commit checkpoint: `settings: provider selection without the legacy option value`.

**Evidence:** Tests show the three long names surviving a save/load round trip intact, the eleven non-candidates absent from the offered list, `overview` absent from the catalogue entirely, and a configured-then-filtered backend still loading.
