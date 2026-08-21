# P01-M02 — generic schema with remote root

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Replace the two-arm union with a schema that can express any backend, without losing saved credentials or breaking S3 bucket routing.

**Dependencies:** P01-M01.

**Allowed files:** `Sources/src/CloudSync/creds.zig`, `Sources/src/CloudSync/creds_test.zig`.

- [ ] Write the failing migration test first, against a real `cloud.credentials` written by the current two-arm build — one S3, one WebDAV.
- [ ] **The schema is `{ backend, options, remote_root }`, not `{ backend, options }`.** `remoteParams` says it outright: *"The bucket is deliberately not here — for S3 it is a path component, carried by the alias target."* An options-only map has nowhere to put the bucket, and migrating it as an option would route every S3 sync at the account root instead of the bucket. That is silent data misplacement, not a config error.
- [ ] Migrate `s3` by moving its `bucket` into `remote_root` and its remaining fields into `options`; migrate `webdav` by moving its fields into `options` with an empty `remote_root`. Both protocol names are rclone backend names already, so the backend field is an identity mapping.
- [ ] The **alias target** is built from the backend remote name and the remote root, matching what the two-arm code already does.
- [ ] **The fingerprint is not the alias target, and must not be derived from `backend` + `remote_root`.** Today it is `s3:{endpoint}/{bucket}` and `webdav:{url}` — deliberately the *connection identity*. Dropping to backend and root would make two different S3 services sharing a bucket name indistinguishable, and would give **every WebDAV configuration the identical string `webdav:`**, since their root is empty. Silently treating a different server as the same pairing is the worst outcome this schema can produce.
- [ ] **Persist the fingerprint explicitly** rather than recomputing it from whatever fields a future backend happens to have. At migration, compute the legacy value exactly as the old code did and store it, so an already-paired profile keeps its pairing — a changed fingerprint demands a re-pair and looks like a new remote.
- [ ] Rotate it when the backend, the remote root, or the non-secret connection identity changes. Never let a secret enter it; the existing comment "No secret material" is a contract, not a description.
- [ ] For a backend with no obvious endpoint field, fall back to a stable digest of the non-secret, non-default options. Two configurations that differ only in a password are the same remote; two that differ in host are not.
- [ ] **Classify secrets independently of the catalogue.** Which fields are secret currently comes from catalogue metadata, but the cache can be absent while credentials still must load. Persist a per-field secret flag at save time, so the withheld-secret contract holds with no catalogue at all.
- [ ] **Replace the 16 KiB caps** at `load` and in the serializer. They were sized when the comment "endpoints and keys are hundreds of bytes at the outside" was true; a backend with dozens of set options plus an OAuth token document can exceed it, and the failure mode of `load` is to return null — silently losing the credentials. Size dynamically with a documented safety limit, and make exceeding it a reported error rather than a null.
- [ ] **Store only what the player set.** Never persist a value equal to the catalogue default.
- [ ] Commit checkpoint: `cloudsync: generic credentials schema with remote root`.

**Evidence:** A credentials file from the previous build migrates with a byte-identical fingerprint and syncs to the same bucket path; two WebDAV configurations on different servers produce different fingerprints; changing only a password leaves the fingerprint unchanged; a 64 KiB credentials document round-trips; secrets stay withheld with the catalogue cache deleted.
