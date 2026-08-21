# P02-M02 — render the form

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace the fixed credentials dialog with one that renders whatever the model says.

**Dependencies:** P02-M01.

**Allowed files:** `Sources/src/GameTT/InterfaceCloudCredentials.h`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`, `Data/UI/CloudCredentials.xml`, `Data/Textes/UI/CloudCredentials`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] Replace the fixed S3/WebDAV field set with a scrolling list built from the form model. The existing dialog is the starting point, not a thing to preserve.
- [ ] Provide a show-advanced toggle, collapsed by default.
- [ ] Bind masked fields to the withheld-secret contract: a stored secret shows a placeholder and is only sent when the player types a new one.
- [ ] Populate droplists from `Examples`, showing the example help text where the catalogue provides it.
- [ ] Handle the long case honestly: some backends have dozens of basic fields. The list scrolls; it does not truncate.
- [ ] The screen must open with an empty catalogue and say why, rather than presenting an empty form.
- [ ] Verify headlessly with `BK_AUTO_UI` `click=` actions, capturing s3, webdav and one OAuth-bearing backend so the field-set differences are visible in the evidence.
- [ ] Commit checkpoint: `settings: render the credentials form from the catalogue`.

**Evidence:** Headless captures of three backends showing different field sets from one renderer, the advanced toggle, and the empty-catalogue state.
