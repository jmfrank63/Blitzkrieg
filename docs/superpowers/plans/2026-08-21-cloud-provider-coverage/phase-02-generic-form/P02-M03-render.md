# P02-M03 — render the form

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace the fixed credentials dialog with one that renders whatever the model says.

**Dependencies:** P02-M02.

**Allowed files:** `Sources/src/GameTT/InterfaceCloudCredentials.h`, `Sources/src/GameTT/InterfaceCloudCredentials.cpp`, `Data/UI/CloudCredentials.xml`, `Data/Textes/UI/CloudCredentials`.

- [ ] Replace the fixed S3/WebDAV field set with a scrolling list built from the form model. The existing dialog is the starting point, not something to preserve.
- [ ] Add the provider chooser here, since P01-M04 removed it from the options list. It offers the filtered destination list from the catalogue.
- [ ] Provide a show-advanced toggle, collapsed by default.
- [ ] Bind masked fields to the withheld-secret contract: a stored secret shows a placeholder and is sent only when the player types a new one.
- [ ] Populate droplists from `Examples`, showing the example help where the catalogue provides it.
- [ ] Some backends have dozens of basic fields. The list scrolls; it does not truncate.
- [ ] Trigger `ensureCatalogue` when the dialog opens and **poll it** like any other job, showing a fetching state; the fetch can spawn a daemon and must not block the UI thread.
- [ ] Handle the empty catalogue explicitly — say it has not been fetched yet, show the failure reason when there is one, and offer a retry, rather than presenting an empty form. A fresh install is legitimately in this state until the first fetch succeeds.
- [ ] Cancel an in-flight fetch if the player closes the dialog.
- [ ] Verify headlessly with `BK_AUTO_UI` `click=` actions, capturing s3, webdav and one OAuth-bearing backend so the differing field sets are visible in the evidence.
- [ ] Commit checkpoint: `settings: render the credentials form from the catalogue`.

**Evidence:** Headless captures of three backends showing different field sets from one renderer, the advanced toggle, the provider chooser, and the empty-catalogue state.
