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
- [ ] **Rebuild the form when the `provider` field changes**, preserving what the player has already typed into fields that still apply. The preservation happens here, matched by field name — the model is asked only for the new field list, so no typed secret crosses the ABI during a rebuild.
- [ ] **A field surviving the rebuild does not mean its value did.** Preserve a value when the field is editable, or when a closed (`Exclusive`) field's value is still among its newly filtered examples; otherwise clear it. Preserving on field existence alone would resubmit a value the new vendor never offers — and because it arrives as an explicit submission rather than a stale stored option, it would sail past a cleanup that only inspected what was already saved. Selecting a vendor is not an ordinary edit: it changes which fields exist. Without this the dialog shows whatever the first build produced and quietly offers the wrong options.
- [ ] Verify the rebuild headlessly: choose `s3`, capture the form under `AWS`, switch to `Wasabi`, and capture again — the region choices must differ, and values typed into still-applicable fields must survive the switch.
- [ ] Some backends have dozens of basic fields. The list scrolls; it does not truncate.
- [ ] Trigger `ensureCatalogue` when the dialog opens and **poll it** like any other job, showing a fetching state; the fetch can spawn a daemon and must not block the UI thread.
- [ ] Handle the empty catalogue explicitly — say it has not been fetched yet, show the failure reason when there is one, and offer a retry, rather than presenting an empty form. A fresh install is legitimately in this state until the first fetch succeeds.
- [ ] Cancel an in-flight fetch if the player closes the dialog.
- [ ] Verify headlessly with `BK_AUTO_UI` `click=` actions, capturing s3, webdav and one OAuth-bearing backend so the differing field sets are visible in the evidence.
- [ ] Commit checkpoint: `settings: render the credentials form from the catalogue`.

**Evidence:** Headless captures of three backends showing different field sets from one renderer, an AWS-to-Wasabi rebuild changing the region choices while preserving typed values, the advanced toggle, the provider chooser, and the empty-catalogue state.
