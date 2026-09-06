# P07-M02 — backup browser

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Show the config history the backup engine has been quietly accumulating.

**Dependencies:** P07-M01 and P04-M02.

**Allowed files:** `Sources/src/GameTT/InterfaceCloudBackups.h`, `Sources/src/GameTT/InterfaceCloudBackups.cpp`, `Data/UI/CloudBackups.xml`, `Data/Textes/UI/CloudBackups`.

- [ ] Build a list screen over the pollable `NCloudSync` backup listing, grouped by host and sorted newest first, using the existing list screens under `Data/UI/Lists/` as the layout precedent.
- [ ] Show host, timestamp, and size per row. Host is the field that makes the list meaningful — "my desktop's settings from Tuesday" is the actual question a player has.
- [ ] Fetch on open and poll for completion; the listing is a network call and must not block the screen. Show a loading state and a failure state, both from the P02-M03 outcome vocabulary.
- [ ] Handle the empty case explicitly: a player who has never enabled backup sees an explanation, not a blank list.
- [ ] Reach it from the Cloud tab as a second screen button.
- [ ] Commit checkpoint: `settings: cloud backup browser`.

**Evidence:** Headless capture shows a populated list across two hosts, the loading state, and the empty state.
