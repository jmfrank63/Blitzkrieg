//! Cloud credentials, outside the option system on purpose.
//!
//! `config.cfg` is rewritten from memory at shutdown and truncated to the
//! option system's value lengths — a 40-character S3 secret would not survive
//! it. Worse, `config.cfg` is backed up to the cloud (phase 04): a credential
//! inside it would be uploaded to the very service it unlocks. So connection
//! details live in their own file, `profiles/cloud.credentials`, which is
//! excluded from the sync filter set and never enters the backup set.
//!
//! The schema is `{ backend, options, remote_root }` — generic over every
//! rclone backend rather than a union of the two we happened to ship first.
//! `backend` is rclone's backend name, `options` a flat `Name` → value map
//! exactly as `config/create` wants it, and `remote_root` the path component
//! the sync alias appends (the S3 bucket, an sftp directory). The root is
//! deliberately not an option: for S3 it is a path component carried by the
//! alias target, and migrating it as an option would route every sync at the
//! account root instead of the bucket.
//!
//! On disk that is:
//!
//! ```json
//! { "backend": "s3", "remote_root": "bk", "fingerprint": "…",
//!   "options": { "endpoint": "…", "secret_access_key": "…" },
//!   "secret_options": ["access_key_id", "secret_access_key"],
//!   "password_options": [],
//!   "rclone_path": null }
//! ```
//!
//! `secret_options` and `password_options` persist the catalogue's
//! classification per field, because the two flags answer different
//! questions — `secret` (from `IsPassword` or `Sensitive`) decides what a
//! load withholds, `is_password` decides what the phase-03 token read-back
//! must not overwrite, since rclone returns those fields obscured. They are
//! persisted at save time precisely so both contracts hold with no catalogue
//! cache on the machine at all.
//!
//! A file written by the previous two-arm build (`{"protocol":"s3",…}`) is
//! migrated on read. The migration necessarily names the two legacy
//! backends' fields — it is one of this plan's three declared exceptions to
//! the no-hardcoded-names rule — and maps them to their rclone option names
//! with the flags v1.75.0's catalogue declares for them, the S3 bucket
//! becoming the remote root.
//!
//! The **fingerprint** is the pairing record's connection identity, distinct
//! from the alias target, and it is persisted explicitly rather than
//! recomputed from whatever fields a backend happens to have. Migration
//! stores the value **production pairing records actually hold** — the C++
//! facade's scrape of the legacy redacted document, `{endpoint}/{bucket}`
//! for S3 and `{url}` for WebDAV. (This module's old `fingerprint()` wrote
//! `s3:`/`webdav:`-prefixed strings, but nothing shipping ever consumed
//! them; the facade scraped its own.) So an already-paired profile keeps
//! its pairing once the facade reads the persisted value. It
//! rotates only when the backend, the remote root, or the canonical
//! non-secret option projection changes — sorted by key, secrets excluded —
//! so reserialising an unchanged configuration cannot rotate it and a
//! password edit keeps a migrated value verbatim. No secret ever enters it;
//! "no secret material" is a contract, not a description.
//!
//! Secrets have exactly three rules, and every export honours them:
//!
//! - **They never leave through a load.** The dialog- and log-safe form
//!   carries values only for non-secret options; stored secrets appear as
//!   names in `secret_options`, never as values.
//! - **Omission is preservation.** The dialog cannot send back a secret it
//!   was never given, so it echoes the stored secret's *name* and `merge`
//!   fills the value from the stored file — editing the endpoint must not
//!   silently destroy the credential. Preservation holds only while the
//!   backend is unchanged: `pass` recurs across backends, and a backend
//!   switch must not apply one service's password to another.
//! - **Clearing is deliberate.** A separate operation, never a side effect
//!   of saving.
//!
//! Documents are sized dynamically under `max_document_bytes`; the old fixed
//! 16 KiB cap was sized for "endpoints and keys", and a backend with dozens
//! of options plus an OAuth token document can exceed that. Exceeding the
//! limit on read is a **reported error**, never the old silent null that
//! read as "no credentials saved".
//!
//! Writes are temp-file-then-rename with owner-only permissions on POSIX, so
//! a crash mid-write cannot leave a truncated credential file, and other
//! accounts on a shared machine cannot read the secret at rest.

const std = @import("std");
const builtin = @import("builtin");

const Allocator = std.mem.Allocator;
const Io = std.Io;
const Sha256 = std.crypto.hash.sha2.Sha256;

/// The file's name inside `profiles/`. A sibling of the profile directories,
/// so it is outside every Path1; the filter entry naming it is the second
/// fence for a copy mistakenly dropped *into* a profile.
pub const file_name = "cloud.credentials";

/// Where the file lives relative to the game's working directory, which is
/// the game root by the engine's own convention (`NProfile::Segment` is
/// relative for the same reason).
pub const default_path = "profiles/" ++ file_name;

/// The raw backend remote's name in `rclone.conf`. Never used as Path2
/// directly — the sync always goes through the `bkremote` alias, so the
/// session name stays short and constant regardless of root or URL length.
pub const backend_remote_name = "bkraw";

/// The alias every sync run uses as Path2's remote. Its target is
/// `bkraw:<remote_root>` — see `aliasTarget`.
pub const sync_remote_name = "bkremote";

/// Safety limit for a credentials document, read and written. Reading a
/// larger file fails with `error.CredentialsTooLarge` — reported, because
/// the alternative is returning null and silently losing the credentials.
/// An OAuth token document is a few KiB; a mebibyte is no longer a
/// credentials file.
pub const max_document_bytes: usize = 1 << 20;

/// One saved backend option: rclone's `Option.Name`, the player's value, and
/// the persisted secret classification. Only what the player set is stored —
/// a value equal to a catalogue default must not be persisted, or a default
/// that changes upstream would be pinned here forever; that exclusion is the
/// saver's contract, since only the caller holding the catalogue can test it.
pub const Option = struct {
    name: []const u8 = "",
    value: []const u8 = "",
    /// From the catalogue's `IsPassword` or `Sensitive` at save time: the
    /// load path withholds this value.
    secret: bool = false,
    /// From `IsPassword` alone: rclone returns this field obscured, so the
    /// phase-03 token read-back must never write it back.
    is_password: bool = false,

    /// Secret-designated by the catalogue at save time: what serialization
    /// withholds, and what a failure text must never carry.
    pub fn withheld(self: Option) bool {
        return self.secret or self.is_password;
    }
};

pub const Credentials = struct {
    /// rclone's backend name — `s3`, `webdav`, `sftp`, `drive`, …
    backend: []const u8 = "",
    /// The path component the sync alias appends to the raw remote: the S3
    /// bucket, a directory, or empty when the backend's own configuration
    /// already names the tree. Whether a backend requires one cannot be
    /// derived from the catalogue, so it is optional here and a wrong one is
    /// discovered by the connection test's writability probe.
    remote_root: []const u8 = "",
    /// The persisted connection identity; empty means "not established yet",
    /// and `save` derives it then. See the module doc for the rotation rule.
    fingerprint: []const u8 = "",
    options: []Option = &.{},
    /// The player's explicit rclone override, or null to use discovery's
    /// search order. Saving this is what re-runs discovery.
    rclone_path: ?[]const u8 = null,

    pub fn option(self: *const Credentials, name: []const u8) ?*const Option {
        for (self.options) |*candidate| {
            if (std.mem.eql(u8, candidate.name, name)) return candidate;
        }
        return null;
    }

    /// True when any withheld field holds a value.
    pub fn hasSecret(self: *const Credentials) bool {
        for (self.options) |opt| {
            if (opt.withheld() and opt.value.len != 0) return true;
        }
        return false;
    }
};

/// A parsed credentials file and the arena its strings live in.
pub const Loaded = struct {
    arena: std.heap.ArenaAllocator,
    creds: Credentials,

    pub fn deinit(self: *Loaded) void {
        self.arena.deinit();
        self.* = undefined;
    }
};

pub const LoadError = Allocator.Error || error{CredentialsTooLarge};

/// Read and parse `path`. Null when the file is missing, malformed, or in a
/// schema this build does not know — a file from a newer build must not make
/// the game unstartable, and "no credentials" is a state every caller
/// already handles. A file over `max_document_bytes` is the one read failure
/// that is an error instead: null there would silently lose the credentials.
pub fn load(gpa: Allocator, io: Io, path: []const u8) LoadError!?Loaded {
    const text = Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(max_document_bytes)) catch |err|
        switch (err) {
            error.OutOfMemory => return error.OutOfMemory,
            error.StreamTooLong => return error.CredentialsTooLarge,
            else => return null,
        };
    defer gpa.free(text);
    return parse(gpa, text);
}

/// `load` without the file: parse one JSON document into owned credentials.
/// Accepts the generic schema, and — the migration path — the previous
/// build's two-arm `{"protocol": …}` document.
pub fn parse(gpa: Allocator, text: []const u8) Allocator.Error!?Loaded {
    var arena: std.heap.ArenaAllocator = .init(gpa);
    errdefer arena.deinit();
    const alloc = arena.allocator();

    // Copied into the arena before parsing: the JSON parser references the
    // input buffer for any string that needs no unescaping, so the document
    // must live exactly as long as the slices handed out of it.
    const owned = try alloc.dupe(u8, text);
    const parsed = std.json.parseFromSliceLeaky(std.json.Value, alloc, owned, .{}) catch |err|
        switch (err) {
            error.OutOfMemory => return error.OutOfMemory,
            else => {
                arena.deinit();
                return null;
            },
        };
    const root = switch (parsed) {
        .object => |o| o,
        else => {
            arena.deinit();
            return null;
        },
    };

    const creds = (try parseGeneric(alloc, root)) orelse
        (try migrateLegacy(alloc, root)) orelse {
        arena.deinit();
        return null;
    };
    return .{ .arena = arena, .creds = creds };
}

/// The generic schema: present whenever the root carries a string `backend`.
fn parseGeneric(alloc: Allocator, root: std.json.ObjectMap) Allocator.Error!?Credentials {
    const backend = stringField(root, "backend") orelse return null;

    var options: std.ArrayList(Option) = .empty;
    if (root.get("options")) |raw| {
        if (raw == .object) {
            var entries = raw.object.iterator();
            while (entries.next()) |entry| {
                // A non-string value is skipped, not fatal — the unknown-
                // field tolerance from the other direction.
                const value = switch (entry.value_ptr.*) {
                    .string => |s| s,
                    else => continue,
                };
                try options.append(alloc, .{ .name = entry.key_ptr.*, .value = value });
            }
        }
    }

    // The flag arrays mark existing entries and materialise the rest as
    // empty-valued placeholders. The placeholders are what lets a dialog
    // echo a stored secret's *name* without its value and have
    // `mergeOmittedSecret` fill it back in — omission as preservation,
    // with nothing secret ever having crossed outward.
    try applyFlags(alloc, &options, root, "secret_options", .secret);
    try applyFlags(alloc, &options, root, "password_options", .password);

    var creds: Credentials = .{
        .backend = backend,
        .remote_root = stringField(root, "remote_root") orelse "",
        .fingerprint = stringField(root, "fingerprint") orelse "",
        .options = options.items,
        .rclone_path = stringField(root, "rclone_path"),
    };
    creds.fingerprint = try upgradeTransitionalFingerprint(alloc, creds);
    return creds;
}

/// Repair the one build window (`06601b7c6`, before the format correction)
/// in which migration persisted this module's old `s3:`/`webdav:`-prefixed
/// derivation instead of the string pairing records hold. The stored
/// fingerprint is rewritten **only** when it is byte-equal to the old
/// derivation of these same credentials — provably that window's output, so
/// no established identity can be rewritten — and the rewrite produces what
/// the corrected migration would have: the facade scrape of the same
/// components. Anything else, including a value that merely resembles the
/// old shape, is an identity and stays verbatim. A second use of the
/// declared legacy-migration naming exception.
fn upgradeTransitionalFingerprint(
    alloc: Allocator,
    creds: Credentials,
) Allocator.Error![]const u8 {
    if (creds.fingerprint.len == 0) return creds.fingerprint;

    if (std.mem.eql(u8, creds.backend, "s3")) {
        const endpoint = optionValue(creds, "endpoint");
        const old = try std.fmt.allocPrint(alloc, "s3:{s}/{s}", .{ endpoint, creds.remote_root });
        if (std.mem.eql(u8, creds.fingerprint, old))
            return legacyPairingIdentity(alloc, &.{ endpoint, creds.remote_root });
    } else if (std.mem.eql(u8, creds.backend, "webdav")) {
        const url = optionValue(creds, "url");
        const old = try std.fmt.allocPrint(alloc, "webdav:{s}", .{url});
        if (std.mem.eql(u8, creds.fingerprint, old))
            return legacyPairingIdentity(alloc, &.{url});
    }
    return creds.fingerprint;
}

fn optionValue(creds: Credentials, name: []const u8) []const u8 {
    const opt = creds.option(name) orelse return "";
    return opt.value;
}

const Flag = enum { secret, password };

fn applyFlags(
    alloc: Allocator,
    options: *std.ArrayList(Option),
    root: std.json.ObjectMap,
    key: []const u8,
    flag: Flag,
) Allocator.Error!void {
    const raw = root.get(key) orelse return;
    if (raw != .array) return;
    for (raw.array.items) |item| {
        const name = switch (item) {
            .string => |s| s,
            else => continue,
        };
        const opt = existing: {
            for (options.items) |*candidate| {
                if (std.mem.eql(u8, candidate.name, name)) break :existing candidate;
            }
            try options.append(alloc, .{ .name = name });
            break :existing &options.items[options.items.len - 1];
        };
        switch (flag) {
            .secret => opt.secret = true,
            // `IsPassword` implies withheld: a password is always a secret,
            // whatever a hand-edited document claims.
            .password => {
                opt.is_password = true;
                opt.secret = true;
            },
        }
    }
}

/// The previous build's two-arm document. This function is a declared
/// exception to the no-hardcoded-names rule: migrating a legacy file means
/// knowing the legacy fields. The names map onto rclone's option names, the
/// flags onto what v1.75.0's catalogue declares for them (`access_key_id`
/// and `user` are `Sensitive`; `pass` is the one `IsPassword`), and the S3
/// bucket becomes the remote root. The fingerprint is computed exactly as
/// the facade's scraper computed it from the legacy redacted document —
/// the string every existing pairing record holds — so the pairing
/// survives byte-for-byte.
fn migrateLegacy(alloc: Allocator, root: std.json.ObjectMap) Allocator.Error!?Credentials {
    const protocol = stringField(root, "protocol") orelse return null;

    var options: std.ArrayList(Option) = .empty;
    var creds: Credentials = .{
        .rclone_path = stringField(root, "rclone_path"),
    };

    if (std.mem.eql(u8, protocol, "s3")) {
        const section = objectField(root, "s3") orelse return null;
        const endpoint = stringField(section, "endpoint") orelse "";
        const bucket = stringField(section, "bucket") orelse "";
        try appendSet(alloc, &options, "provider", stringField(section, "s3_provider"), .{});
        try appendSet(alloc, &options, "endpoint", endpoint, .{});
        try appendSet(alloc, &options, "region", stringField(section, "region"), .{});
        // The withheld fields are materialised even when empty, so a legacy
        // dialog save without them still has entries for the merge to fill.
        try options.append(alloc, .{
            .name = "access_key_id",
            .value = stringField(section, "access_key") orelse "",
            .secret = true,
        });
        try options.append(alloc, .{
            .name = "secret_access_key",
            .value = stringField(section, "secret") orelse "",
            .secret = true,
        });
        creds.backend = "s3";
        creds.remote_root = bucket;
        creds.fingerprint = try legacyPairingIdentity(alloc, &.{ endpoint, bucket });
    } else if (std.mem.eql(u8, protocol, "webdav")) {
        const section = objectField(root, "webdav") orelse return null;
        const url = stringField(section, "url") orelse "";
        try appendSet(alloc, &options, "url", url, .{});
        try appendSet(alloc, &options, "vendor", stringField(section, "vendor"), .{});
        try options.append(alloc, .{
            .name = "user",
            .value = stringField(section, "user") orelse "",
            .secret = true,
        });
        try options.append(alloc, .{
            .name = "pass",
            .value = stringField(section, "pass") orelse "",
            .secret = true,
            .is_password = true,
        });
        creds.backend = "webdav";
        creds.fingerprint = try legacyPairingIdentity(alloc, &.{url});
    } else {
        // A protocol from a newer build: tolerated, reported as "none saved".
        return null;
    }

    creds.options = options.items;
    return creds;
}

/// The facade scraper's join, replicated byte-for-byte: each part is
/// appended behind a `/` separator that appears only when something has
/// been written already — so an empty leading part vanishes while an empty
/// trailing one leaves the separator. Faithfulness to those edge cases,
/// not elegance, is the point: the pairing record compares by string
/// equality, and any deviation reads as a changed remote.
fn legacyPairingIdentity(alloc: Allocator, parts: []const []const u8) Allocator.Error![]u8 {
    var out: std.ArrayList(u8) = .empty;
    for (parts) |part| {
        if (out.items.len != 0) try out.append(alloc, '/');
        try out.appendSlice(alloc, part);
    }
    return out.items;
}

/// Append a non-secret option only when the legacy field held a value —
/// store only what the player set.
fn appendSet(
    alloc: Allocator,
    options: *std.ArrayList(Option),
    name: []const u8,
    value: ?[]const u8,
    flags: struct { secret: bool = false, is_password: bool = false },
) Allocator.Error!void {
    const set = value orelse return;
    if (set.len == 0) return;
    try options.append(alloc, .{
        .name = name,
        .value = set,
        .secret = flags.secret,
        .is_password = flags.is_password,
    });
}

fn stringField(object: std.json.ObjectMap, name: []const u8) ?[]const u8 {
    const value = object.get(name) orelse return null;
    return switch (value) {
        .string => |s| s,
        else => null,
    };
}

fn objectField(object: std.json.ObjectMap, name: []const u8) ?std.json.ObjectMap {
    const value = object.get(name) orelse return null;
    return switch (value) {
        .object => |o| o,
        else => null,
    };
}

pub const SaveError = Allocator.Error || error{CredentialsUnwritable};

/// Serialise and persist atomically: the document is written to `<path>.tmp`
/// and renamed over the target, so a crash mid-write leaves either the old
/// file or the new one, never a truncated hybrid. Owner-only permissions on
/// POSIX; Windows scopes by the profile directory's ACL.
pub fn save(gpa: Allocator, io: Io, path: []const u8, creds: Credentials) SaveError!void {
    const text = serialize(gpa, creds, .{ .include_secret = true }) catch
        return error.OutOfMemory;
    defer gpa.free(text);

    if (std.Io.Dir.path.dirname(path)) |dir| {
        Io.Dir.cwd().createDirPath(io, dir) catch return error.CredentialsUnwritable;
    }

    const tmp_path = std.fmt.allocPrint(gpa, "{s}.tmp", .{path}) catch return error.OutOfMemory;
    defer gpa.free(tmp_path);

    Io.Dir.cwd().writeFile(io, .{
        .sub_path = tmp_path,
        .data = text,
        .flags = .{
            .truncate = true,
            .permissions = if (builtin.os.tag == .windows)
                .default_file
            else
                .fromMode(0o600),
        },
    }) catch return error.CredentialsUnwritable;

    Io.Dir.rename(.cwd(), tmp_path, .cwd(), path, io) catch
        return error.CredentialsUnwritable;
}

pub const SerializeOptions = struct {
    /// True only for the on-disk form. Every other consumer — the dialog,
    /// the logs, the ABI — gets stored secrets as names, never as values.
    include_secret: bool,
};

/// The JSON document for `creds`, secrets included or withheld. Sized
/// dynamically — the fixed cap this replaces silently nulled any document it
/// could not hold.
pub fn serialize(gpa: Allocator, creds: Credentials, options: SerializeOptions) Allocator.Error![]u8 {
    // The on-disk form always carries the identity; deriving it here rather
    // than in `save` keeps "serialise then persist" one operation with no
    // caller able to write a fingerprint-less file.
    const computed: ?[]u8 = if (options.include_secret and creds.fingerprint.len == 0)
        try computeFingerprint(gpa, creds)
    else
        null;
    defer if (computed) |print| gpa.free(print);

    var out: std.Io.Writer.Allocating = .init(gpa);
    errdefer out.deinit();
    var json: std.json.Stringify = .{ .writer = &out.writer };
    write(&json, creds, computed orelse creds.fingerprint, options) catch return error.OutOfMemory;
    return out.toOwnedSlice();
}

/// The dialog- and log-safe form: non-secret values flat, stored secrets by
/// name in `secret_options`, plus `has_secret`. This is the only
/// serialisation allowed to reach a log, a daemon argument, or the ABI.
pub fn redacted(gpa: Allocator, creds: Credentials) Allocator.Error![]u8 {
    return serialize(gpa, creds, .{ .include_secret = false });
}

fn write(
    json: *std.json.Stringify,
    creds: Credentials,
    print: []const u8,
    options: SerializeOptions,
) !void {
    try json.beginObject();
    try json.objectField("backend");
    try json.write(creds.backend);
    try json.objectField("remote_root");
    try json.write(creds.remote_root);
    if (options.include_secret) {
        try json.objectField("fingerprint");
        try json.write(print);
    }

    // Unset options are not persisted: an entry exists because the player
    // set a value, and a cleared secret leaves no entry behind.
    try json.objectField("options");
    try json.beginObject();
    for (creds.options) |opt| {
        if (opt.value.len == 0) continue;
        if (!options.include_secret and opt.withheld()) continue;
        try json.objectField(opt.name);
        try json.write(opt.value);
    }
    try json.endObject();

    try json.objectField("secret_options");
    try json.beginArray();
    for (creds.options) |opt| {
        if (opt.withheld() and opt.value.len != 0) try json.write(opt.name);
    }
    try json.endArray();

    try json.objectField("password_options");
    try json.beginArray();
    for (creds.options) |opt| {
        if (opt.is_password and opt.value.len != 0) try json.write(opt.name);
    }
    try json.endArray();

    if (!options.include_secret) {
        try json.objectField("has_secret");
        try json.write(creds.hasSecret());
    }
    try json.objectField("rclone_path");
    if (creds.rclone_path) |path| try json.write(path) else try json.write(null);
    try json.endObject();
}

/// Omission is preservation: an incoming withheld entry without a value —
/// the dialog echoed a stored secret's name, having never been given the
/// value — takes the stored one. An incoming value always wins; clearing is
/// `creds_clear_secret`'s job, never this function's. Nothing is preserved
/// across a backend change: `pass` recurs across backends, and a switch
/// must not apply one service's password to another.
///
/// The stored fingerprint rides along under the same principle — the dialog
/// is never shown it — carried verbatim while the backend, the root and the
/// canonical non-secret projection are unchanged, and left for `save` to
/// re-derive when they are not. That comparison is what keeps a
/// password-only edit from rotating a migrated identity.
///
/// Filled values alias `stored`'s arena; the caller keeps both alive until
/// the merged credentials are saved, which is the shape of every call site.
pub fn mergeOmittedSecret(incoming: *Credentials, stored: Credentials) void {
    if (!std.mem.eql(u8, incoming.backend, stored.backend)) return;

    for (incoming.options) |*opt| {
        if (!opt.withheld() or opt.value.len != 0) continue;
        const held = stored.option(opt.name) orelse continue;
        if (held.value.len == 0) continue;
        opt.value = held.value;
        opt.secret = opt.secret or held.secret;
        opt.is_password = opt.is_password or held.is_password;
    }

    if (incoming.fingerprint.len == 0 and sameIdentity(incoming.*, stored)) {
        incoming.fingerprint = stored.fingerprint;
    }
}

/// Blank every withheld value in place — the deliberate act, distinct from
/// saving. The next save drops the emptied entries.
pub fn clearSecret(creds: *Credentials) void {
    for (creds.options) |*opt| {
        if (opt.withheld()) opt.value = "";
    }
}

/// The per-field form of the deliberate act: blank one named value. A name
/// with no entry is already clear, so clearing it again succeeds — the
/// caller asked for a state, not an action. Clearing through a *save* is
/// deliberately impossible for withheld fields, because an empty or
/// omitted withheld entry always preserves (see `mergeOmittedSecret`);
/// this and `clearSecret` are the only ways a stored secret goes away.
pub fn clearOption(creds: *Credentials, name: []const u8) void {
    for (creds.options) |*opt| {
        if (std.mem.eql(u8, opt.name, name)) opt.value = "";
    }
}

/// True when an option belongs to the canonical non-secret projection the
/// fingerprint is derived and compared from: a set value, not withheld.
/// Values equal to a catalogue default are never stored (the saver's
/// contract), so the projection needs no catalogue to exclude them.
fn inProjection(opt: Option) bool {
    return !opt.withheld() and opt.value.len != 0;
}

/// The rotation comparison: backend, root, and the canonical non-secret
/// projection — nothing else. Two configurations differing only in a secret
/// are the same remote; two differing in any connection detail are not.
fn sameIdentity(a: Credentials, b: Credentials) bool {
    if (!std.mem.eql(u8, a.backend, b.backend)) return false;
    if (!std.mem.eql(u8, a.remote_root, b.remote_root)) return false;
    return projectionSubset(a, b) and projectionSubset(b, a);
}

fn projectionSubset(a: Credentials, b: Credentials) bool {
    for (a.options) |opt| {
        if (!inProjection(opt)) continue;
        const other = b.option(opt.name) orelse return false;
        if (other.withheld()) return false;
        if (!std.mem.eql(u8, opt.value, other.value)) return false;
    }
    return true;
}

/// Derive the connection identity from backend, root and the canonical
/// projection — sorted by name, so ordering and formatting cannot rotate
/// it, and by one generic rule, never by recognising endpoint-like fields:
/// deciding that `endpoint` or `url` carries identity is exactly the
/// field-name hardcoding this schema exists to remove.
fn computeFingerprint(gpa: Allocator, creds: Credentials) Allocator.Error![]u8 {
    const picked = try gpa.alloc(*const Option, creds.options.len);
    defer gpa.free(picked);
    var count: usize = 0;
    for (creds.options) |*opt| {
        if (!inProjection(opt.*)) continue;
        picked[count] = opt;
        count += 1;
    }
    std.mem.sort(*const Option, picked[0..count], {}, struct {
        fn lessThan(_: void, lhs: *const Option, rhs: *const Option) bool {
            return std.mem.lessThan(u8, lhs.name, rhs.name);
        }
    }.lessThan);

    var hasher = Sha256.init(.{});
    hasher.update(creds.backend);
    hasher.update(&[_]u8{0});
    hasher.update(creds.remote_root);
    hasher.update(&[_]u8{0});
    for (picked[0..count]) |opt| {
        hasher.update(opt.name);
        hasher.update(&[_]u8{0});
        hasher.update(opt.value);
        hasher.update(&[_]u8{0});
    }
    const hex = std.fmt.bytesToHex(hasher.finalResult(), .lower);
    return std.fmt.allocPrint(gpa, "{s}:{s}#{s}", .{ creds.backend, creds.remote_root, &hex });
}

/// The remote identity for the pairing record: the stored value when one is
/// established — a migrated legacy string survives here byte-for-byte — and
/// the generic derivation otherwise. No secret material.
pub fn fingerprint(gpa: Allocator, creds: Credentials) Allocator.Error![]u8 {
    if (creds.fingerprint.len != 0) return gpa.dupe(u8, creds.fingerprint);
    return computeFingerprint(gpa, creds);
}

/// The rc parameter object for `config/create` of the raw backend remote.
pub const RemoteParams = struct {
    arena: std.heap.ArenaAllocator,
    /// An `.object` value: rclone remote parameters, secrets included — this
    /// travels only to the loopback daemon's config, never to a log.
    value: std.json.Value,

    pub fn deinit(self: *RemoteParams) void {
        self.arena.deinit();
        self.* = undefined;
    }
};

/// Build the raw backend remote's parameters: `{"type": backend}` plus every
/// saved option, as a flat `Name` → value map. Backend configuration is
/// keyed by `Option.Name` — `FieldName` belongs to rclone's global RC
/// option-struct JSON, and no v1.75.0 backend option has a differing
/// non-empty one. The remote root is deliberately not here: it is a path
/// component, carried by the alias target.
pub fn remoteParams(gpa: Allocator, creds: Credentials) Allocator.Error!RemoteParams {
    var arena: std.heap.ArenaAllocator = .init(gpa);
    errdefer arena.deinit();
    const alloc = arena.allocator();

    var object: std.json.ObjectMap = .empty;
    try object.put(alloc, "type", .{ .string = try alloc.dupe(u8, creds.backend) });
    for (creds.options) |opt| {
        // An unset option is not a parameter — sending `""` would override
        // the backend's default with an explicit empty.
        if (opt.value.len == 0) continue;
        try object.put(
            alloc,
            try alloc.dupe(u8, opt.name),
            .{ .string = try alloc.dupe(u8, opt.value) },
        );
    }
    return .{ .arena = arena, .value = .{ .object = object } };
}

/// The short stable name the sync uses. Path2 contributes only `name:root`
/// to the session name — `bilib.FsPath` charges full length to the `local`
/// branch alone — and this keeps that contribution constant however long a
/// backend name grows.
pub fn remoteName(creds: Credentials) []const u8 {
    _ = creds;
    return sync_remote_name;
}

/// What the `bkremote` alias points at: the raw remote plus the remote root
/// — `bkraw:<bucket>` for S3, `bkraw:` where the backend's configuration
/// already names the tree. Exactly what the two-arm code built.
pub fn aliasTarget(gpa: Allocator, creds: Credentials) Allocator.Error![]u8 {
    return std.fmt.allocPrint(gpa, "{s}:{s}", .{ backend_remote_name, creds.remote_root });
}
