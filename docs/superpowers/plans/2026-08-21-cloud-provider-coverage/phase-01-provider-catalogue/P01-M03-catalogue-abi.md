# P01-M03 — catalogue and credentials exports

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Make the catalogue and the new schema reachable from C++, since nothing above Zig can see either yet.

**Dependencies:** P01-M02.

**Allowed files:** `Sources/src/CloudSync/catalogue.zig`, `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/cloudsync.zig`, `Sources/src/CloudSync/CloudSync.def`, `Sources/src/CloudSync/CloudSync.x64.def`, `tools/zig/cloudsync_abi_test.cpp`, `Sources/src/Main/CloudSyncFacade.h`, `Sources/src/Main/CloudSyncFacade.cpp`.

- [ ] Extend the C++ ABI smoke test first to enumerate providers and round-trip a credentials document, and watch it fail.
- [ ] Add the exports and their facade wrappers in this one commit, per the ABI amendment rule: provider enumeration, per-provider option enumeration, and credentials load/save against the generic schema.
- [ ] **This packet exists because the previous draft had none.** The catalogue lived in Zig and the dialog lived in C++ with no packet owning the chain between them, so the form could never have been rendered. Any later packet that needs a new export owns its chain the same way.
- [ ] Serialise across the boundary as JSON into a caller-supplied buffer, matching `discovery_status`, rather than inventing a second marshalling style.
- [ ] Report a too-small buffer as a required size rather than truncating, since option sets vary by orders of magnitude between backends.
- [ ] Keep the withheld-secret contract at the boundary: the load export returns per-field `has_value` flags, never secret values.
- [ ] **Replace `bk_cloudsync_creds_clear_secret()`, which takes no arguments.** It was written when a backend had one secret; the generic schema can carry several — an S3 secret key, an SFTP passphrase, an OAuth token — and an argument-free clear cannot say which. Provide a per-field clear naming the option, and keep the old export only if something still calls it, in which case document what it clears.
- [ ] Decide and document the save representation of "clear this field": an explicitly empty value must be distinguishable from an omitted one, because omitted means *preserve* under the existing contract. Getting this wrong silently keeps a credential the player asked to remove.
- [ ] **Preservation holds only while the backend is unchanged.** Option names are generic — `user`, `pass` and `token` recur across backends — so an omitted value carrying over a provider switch would apply one service's password to another. Changing backend starts a fresh option set, root and fingerprint; nothing is inherited.
- [ ] **A vendor change is not a backend change, and needs its own rule.** Switching S3 from AWS to Wasabi leaves the backend as `s3`, so backend-scoped preservation keeps every stored option — including genuinely AWS-only ones such as `requester_pays` or `use_accelerate_endpoint`, which Wasabi never declares. Those would still be sent to rclone on the next save.
- [ ] When the `provider` value changes, drop options whose `Provider` expression no longer matches, and keep the rest. Use `catalogue.matchProvider` from P01-M01; do not reimplement the expression rules here.
- [ ] **An option can survive the switch while its stored value stops being valid.** Provider filtering applies to `Examples` as well, so a closed (`Exclusive`) field may remain applicable and yet hold a value the new vendor never offers. Clear a stored value that is absent from the newly filtered examples of an `Exclusive` field; leave editable fields alone, since an arbitrary value there is legitimate.
- [ ] Only one option across all 69 backends is `Exclusive` today, so cover this with a **synthetic** fixture rather than hunting for a real pair — the rule must hold for the backend that gains one tomorrow.
- [ ] **Apply the cleanup to the final merged submission, not only to what was previously stored.** The UI submits values it preserved across a rebuild, so a value cleaned out of the stored set can arrive again in the same save. Filtering only the old map would let it through; the save path must validate what it is about to write.
- [ ] For the AWS-to-Wasabi persistence test, pick a genuinely AWS-only option: `requester_pays`, `use_accelerate_endpoint`, `leave_parts_on_error`, `sts_endpoint` or `directory_bucket`, whose own `Provider` is exactly `AWS`. **Do not use `region`** — its expression names 39 vendors including Wasabi, so it survives the switch. What changes for `region` is its examples (153 of them carry a provider), which is the separate concern above and is already covered in P02.
- [ ] Add a cross-backend isolation test: save a secret under one backend, switch to another declaring an identically named field, save without touching it, and assert the value did not follow.
- [ ] Export `ensureCatalogue` from P01-M01 as part of this chain, since the dialog in P02-M03 needs to trigger it and cannot reach Zig directly. **Export it as a pollable job — begin, poll, cancel, release — not as a blocking call**, matching every other export that can touch a socket. A synchronous catalogue fetch would stall the UI thread through a daemon spawn.
- [ ] Commit checkpoint: `cloudsync: expose the catalogue and credentials through the ABI`.

**Evidence:** The C++ consumer enumerates providers, reads one backend's option list, round-trips a credentials document with secrets withheld, clears one named secret while leaving another intact, and triggers `ensureCatalogue`.
