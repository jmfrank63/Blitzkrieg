//! Cloud credentials, outside the option system on purpose.
//!
//! `config.cfg` is rewritten from memory at shutdown and truncated to the
//! option system's value lengths — a 40-character S3 secret would not survive
//! it. Worse, `config.cfg` is backed up to the cloud (phase 04): a credential
//! inside it would be uploaded to the very service it unlocks. So connection
//! details live in their own file, `profiles/cloud.credentials`, which is
//! excluded from the sync filter set (P01-M03) and never enters the backup
//! set.
//!
//! The schema is a tagged union, not one flat S3-shaped struct: the two
//! backends share almost no fields, and a flat struct either cannot express
//! WebDAV or fills half its fields with nulls. rclone's S3 `provider` and the
//! game's protocol choice are different questions wearing the same word —
//! `Cloud.Provider` picks S3-or-WebDAV, rclone's names the vendor behind S3
//! (AWS, Cloudflare, Minio, Wasabi) — so the field here is `s3_provider` and
//! never `provider`.
//!
//! Secrets have exactly three rules, and every export honours them:
//!
//! - **They never leave through a load.** Serialisation for the dialog and
//!   for logs replaces the secret with a `has_secret` flag; only `save`'s
//!   on-disk form carries it.
//! - **Omission is preservation.** The dialog cannot send back a secret it
//!   was never given, so an incoming save without one merges the stored
//!   secret rather than writing an empty string — editing the endpoint must
//!   not silently destroy the credential.
//! - **Clearing is deliberate.** A separate operation, never a side effect
//!   of saving.
//!
//! Writes are temp-file-then-rename with owner-only permissions on POSIX, so
//! a crash mid-write cannot leave a truncated credential file, and other
//! accounts on a shared machine cannot read the secret at rest.

const std = @import("std");
const builtin = @import("builtin");

const Allocator = std.mem.Allocator;
const Io = std.Io;

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
/// session name stays short and constant regardless of bucket or URL length.
pub const backend_remote_name = "bkraw";

/// The alias every sync run uses as Path2's remote. Its target is
/// `bkraw:<bucket>` for S3 and `bkraw:` for WebDAV — see `aliasTarget`.
pub const sync_remote_name = "bkremote";

pub const Protocol = enum { s3, webdav };

pub const S3 = struct {
    /// rclone's vendor string: "AWS", "Cloudflare", "Minio", "Wasabi", or
    /// "Other". Not the game's protocol choice.
    s3_provider: []const u8 = "",
    endpoint: []const u8 = "",
    bucket: []const u8 = "",
    region: []const u8 = "",
    access_key: []const u8 = "",
    secret: []const u8 = "",
};

pub const WebDav = struct {
    url: []const u8 = "",
    /// rclone's webdav vendor: "nextcloud", "owncloud", "other", ...
    vendor: []const u8 = "",
    user: []const u8 = "",
    pass: []const u8 = "",
};

pub const Payload = union(Protocol) {
    s3: S3,
    webdav: WebDav,
};

pub const Credentials = struct {
    payload: Payload,
    /// The player's explicit rclone override, or null to use discovery's
    /// search order. Saving this is what re-runs discovery.
    rclone_path: ?[]const u8 = null,

    /// The secret-bearing field's current value.
    pub fn secretValue(self: *const Credentials) []const u8 {
        return switch (self.payload) {
            .s3 => |s3| s3.secret,
            .webdav => |dav| dav.pass,
        };
    }

    pub fn hasSecret(self: *const Credentials) bool {
        return self.secretValue().len != 0;
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

/// Read and parse `path`. Null when the file is missing, malformed, or names
/// a protocol this build does not know — a file from a newer build must not
/// make the game unstartable, and "no credentials" is a state every caller
/// already handles.
pub fn load(gpa: Allocator, io: Io, path: []const u8) Allocator.Error!?Loaded {
    const text = Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(16384)) catch return null;
    defer gpa.free(text);
    return parse(gpa, text);
}

/// `load` without the file: parse one JSON document into owned credentials.
pub fn parse(gpa: Allocator, text: []const u8) Allocator.Error!?Loaded {
    var arena: std.heap.ArenaAllocator = .init(gpa);
    errdefer arena.deinit();
    const alloc = arena.allocator();

    const parsed = std.json.parseFromSliceLeaky(std.json.Value, alloc, text, .{}) catch {
        arena.deinit();
        return null;
    };
    const root = switch (parsed) {
        .object => |o| o,
        else => {
            arena.deinit();
            return null;
        },
    };

    const protocol_name = stringField(root, "protocol") orelse {
        arena.deinit();
        return null;
    };
    const protocol = std.meta.stringToEnum(Protocol, protocol_name) orelse {
        // A newer build's protocol: tolerated, reported as "none saved".
        arena.deinit();
        return null;
    };

    const payload: Payload = switch (protocol) {
        .s3 => blk: {
            const section = objectField(root, "s3") orelse {
                arena.deinit();
                return null;
            };
            break :blk .{ .s3 = .{
                .s3_provider = stringField(section, "s3_provider") orelse "",
                .endpoint = stringField(section, "endpoint") orelse "",
                .bucket = stringField(section, "bucket") orelse "",
                .region = stringField(section, "region") orelse "",
                .access_key = stringField(section, "access_key") orelse "",
                .secret = stringField(section, "secret") orelse "",
            } };
        },
        .webdav => blk: {
            const section = objectField(root, "webdav") orelse {
                arena.deinit();
                return null;
            };
            break :blk .{ .webdav = .{
                .url = stringField(section, "url") orelse "",
                .vendor = stringField(section, "vendor") orelse "",
                .user = stringField(section, "user") orelse "",
                .pass = stringField(section, "pass") orelse "",
            } };
        },
    };

    return .{ .arena = arena, .creds = .{
        .payload = payload,
        .rclone_path = stringField(root, "rclone_path"),
    } };
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
    /// the logs, the ABI — gets `has_secret` instead of the value.
    include_secret: bool,
};

/// The JSON document for `creds`, secret included or withheld. Bounded by
/// the same 16K cap `load` reads under; endpoints and keys are hundreds of
/// bytes at the outside.
pub fn serialize(gpa: Allocator, creds: Credentials, options: SerializeOptions) Allocator.Error![]u8 {
    var buffer: [16384]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    var json: std.json.Stringify = .{ .writer = &writer };

    write(&json, creds, options) catch return error.OutOfMemory;
    return gpa.dupe(u8, writer.buffered());
}

/// The dialog- and log-safe form: every field except the secret, plus
/// `has_secret`. This is the only serialisation allowed to reach a log, a
/// daemon argument, or the ABI.
pub fn redacted(gpa: Allocator, creds: Credentials) Allocator.Error![]u8 {
    return serialize(gpa, creds, .{ .include_secret = false });
}

fn write(json: *std.json.Stringify, creds: Credentials, options: SerializeOptions) !void {
    try json.beginObject();
    try json.objectField("protocol");
    try json.write(@tagName(creds.payload));
    switch (creds.payload) {
        .s3 => |s3| {
            try json.objectField("s3");
            try json.beginObject();
            try json.objectField("s3_provider");
            try json.write(s3.s3_provider);
            try json.objectField("endpoint");
            try json.write(s3.endpoint);
            try json.objectField("bucket");
            try json.write(s3.bucket);
            try json.objectField("region");
            try json.write(s3.region);
            try json.objectField("access_key");
            try json.write(s3.access_key);
            if (options.include_secret) {
                try json.objectField("secret");
                try json.write(s3.secret);
            }
            try json.endObject();
        },
        .webdav => |dav| {
            try json.objectField("webdav");
            try json.beginObject();
            try json.objectField("url");
            try json.write(dav.url);
            try json.objectField("vendor");
            try json.write(dav.vendor);
            try json.objectField("user");
            try json.write(dav.user);
            if (options.include_secret) {
                try json.objectField("pass");
                try json.write(dav.pass);
            }
            try json.endObject();
        },
    }
    if (!options.include_secret) {
        try json.objectField("has_secret");
        try json.write(creds.hasSecret());
    }
    try json.objectField("rclone_path");
    if (creds.rclone_path) |path| try json.write(path) else try json.write(null);
    try json.endObject();
}

/// Omission is preservation: when `incoming` carries no secret — the dialog
/// was never given one to send back — take the stored one. An incoming
/// secret always wins; clearing is `creds_clear_secret`'s job, never this
/// function's.
pub fn mergeOmittedSecret(incoming: *Credentials, stored: Credentials) void {
    if (incoming.hasSecret()) return;
    switch (incoming.payload) {
        .s3 => |*s3| switch (stored.payload) {
            .s3 => |stored_s3| s3.secret = stored_s3.secret,
            .webdav => {},
        },
        .webdav => |*dav| switch (stored.payload) {
            .webdav => |stored_dav| dav.pass = stored_dav.pass,
            .s3 => {},
        },
    }
}

/// Blank the secret in place — the deliberate act, distinct from saving.
pub fn clearSecret(creds: *Credentials) void {
    switch (creds.payload) {
        .s3 => |*s3| s3.secret = "",
        .webdav => |*dav| dav.pass = "",
    }
}

/// The rc parameter object for `config/create` of the raw backend remote.
pub const RemoteParams = struct {
    arena: std.heap.ArenaAllocator,
    /// An `.object` value: rclone remote parameters, secret included — this
    /// travels only to the loopback daemon's config, never to a log.
    value: std.json.Value,

    pub fn deinit(self: *RemoteParams) void {
        self.arena.deinit();
        self.* = undefined;
    }
};

/// Build the raw backend remote's parameters. The bucket is deliberately not
/// here — for S3 it is a path component, carried by the alias target.
pub fn remoteParams(gpa: Allocator, creds: Credentials) Allocator.Error!RemoteParams {
    var arena: std.heap.ArenaAllocator = .init(gpa);
    errdefer arena.deinit();
    const alloc = arena.allocator();

    var object: std.json.ObjectMap = .empty;
    switch (creds.payload) {
        .s3 => |s3| {
            try object.put(alloc, "type", .{ .string = "s3" });
            try object.put(alloc, "provider", .{ .string = try alloc.dupe(u8, s3.s3_provider) });
            try object.put(alloc, "endpoint", .{ .string = try alloc.dupe(u8, s3.endpoint) });
            try object.put(alloc, "region", .{ .string = try alloc.dupe(u8, s3.region) });
            try object.put(alloc, "access_key_id", .{ .string = try alloc.dupe(u8, s3.access_key) });
            try object.put(alloc, "secret_access_key", .{ .string = try alloc.dupe(u8, s3.secret) });
        },
        .webdav => |dav| {
            try object.put(alloc, "type", .{ .string = "webdav" });
            try object.put(alloc, "url", .{ .string = try alloc.dupe(u8, dav.url) });
            try object.put(alloc, "vendor", .{ .string = try alloc.dupe(u8, dav.vendor) });
            try object.put(alloc, "user", .{ .string = try alloc.dupe(u8, dav.user) });
            try object.put(alloc, "pass", .{ .string = try alloc.dupe(u8, dav.pass) });
        },
    }
    return .{ .arena = arena, .value = .{ .object = object } };
}

/// The short stable name the sync uses. Path2 contributes only `name:root`
/// to the session name — `bilib.FsPath` charges full length to the `local`
/// branch alone — and this keeps that contribution constant.
pub fn remoteName(creds: Credentials) []const u8 {
    _ = creds;
    return sync_remote_name;
}

/// What the `bkremote` alias points at: the raw remote plus the bucket for
/// S3, the raw remote's own root for WebDAV (the URL already names it).
pub fn aliasTarget(gpa: Allocator, creds: Credentials) Allocator.Error![]u8 {
    return switch (creds.payload) {
        .s3 => |s3| std.fmt.allocPrint(gpa, "{s}:{s}", .{ backend_remote_name, s3.bucket }),
        .webdav => std.fmt.allocPrint(gpa, "{s}:", .{backend_remote_name}),
    };
}

/// The remote identity for the pairing record: pointing the same profile at
/// a different bucket or server is a new pairing decision, and this is the
/// string that changes when that happens. No secret material.
pub fn fingerprint(gpa: Allocator, creds: Credentials) Allocator.Error![]u8 {
    return switch (creds.payload) {
        .s3 => |s3| std.fmt.allocPrint(gpa, "s3:{s}/{s}", .{ s3.endpoint, s3.bucket }),
        .webdav => |dav| std.fmt.allocPrint(gpa, "webdav:{s}", .{dav.url}),
    };
}
