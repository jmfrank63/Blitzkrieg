# P01-M04 — provider selection and destination filtering

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Let the player choose any backend without pushing its name through a legacy option, and stop offering backends that are not destinations.

**Dependencies:** P01-M03.

**Allowed files:** `Data/Configs/defconf.cfg`, `Sources/src/CloudSync/catalogue.zig`, `Sources/src/CloudSync/catalogue_test.zig`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] **`Cloud.Provider` must not carry the backend name.** `COptionSystem::Set` truncates any string over 12 characters to 8 (`OptionSystemInternal.cpp:186-192`), and `googlecloudstorage`, `internetarchive` and `oracleobjectstorage` all exceed it — they would be stored as `googlecl`, `internet`, `oracleob` and never resolve. The count grows every time rclone adds a long name.
- [ ] Reduce `Cloud.Provider` to a short fixed state the option system can hold — `OFF` and `ON` — and keep the backend identity in `cloud.credentials`, which has no such limit. Selection happens in the credentials dialog, not the options list.
- [ ] Implement `isDestination(backend)` and filter the offered list. Twelve of the 69 wrap another remote or are not independent destinations: `alias`, `archive`, `cache`, `chunker`, `combine`, `compress`, `crypt`, `hasher`, `local`, `memory`, `overview`, `union`. Offering `crypt` as a cloud provider invites a configuration that cannot work.
- [ ] Derive the filter from catalogue data where possible, and where it must be a list, keep that list in one named place with a comment explaining the criterion — this is one of the plan's declared exceptions to "no provider names in source".
- [ ] Preserve a backend already configured in a profile even if a newer rclone drops it or the filter changes; a working configuration must not vanish because of our list.
- [ ] Sort the offered list alphabetically. Catalogue order is not a menu.
- [ ] Commit checkpoint: `settings: provider selection without the legacy option value`.

**Evidence:** Tests show the three long names surviving a save/load round trip intact, wrappers absent from the offered list, and a configured-then-filtered backend still loading.
