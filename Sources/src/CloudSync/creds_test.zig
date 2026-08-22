//! Tests for the generic credentials file.
//!
//! The migration cases run against the exact bytes the previous two-arm
//! build wrote — captured by running its serializer, not reconstructed from
//! memory — because the promise under test is that a player's existing
//! `cloud.credentials` keeps its pairing and its bucket routing across the
//! schema change. The lengths in the round-trip cases are not arbitrary
//! either: a real S3 secret is 40 characters and an access key 20, which are
//! exactly the values the option system's field lengths would have destroyed
//! — the reason this file exists at all.

const std = @import("std");
const builtin = @import("builtin");
const creds = @import("creds.zig");

const io = std.testing.io;
const path = std.Io.Dir.path;

/// Real-shaped fixture values. The secret is 40 characters, the key 20.
const fixture_secret = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
const fixture_access_key = "AKIAIOSFODNN7EXAMPLE";

comptime {
    std.debug.assert(fixture_secret.len == 40);
    std.debug.assert(fixture_access_key.len == 20);
}

/// Byte-for-byte what the two-arm build's `save` wrote for an S3 credential
/// — captured from that build's serializer, field order and all.
const legacy_s3_doc =
    \\{"protocol":"s3","s3":{"s3_provider":"Cloudflare","endpoint":"https://abc123.r2.cloudflarestorage.com","bucket":"bk-saves","region":"auto","access_key":"AKIAIOSFODNN7EXAMPLE","secret":"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"},"rclone_path":"/opt/rclone/rclone"}
;

/// The same capture for WebDAV.
const legacy_webdav_doc =
    \\{"protocol":"webdav","webdav":{"url":"https://cloud.example.net/remote.php/dav/files/player","vendor":"nextcloud","user":"player","pass":"correct horse battery staple"},"rclone_path":null}
;

/// The fingerprints those files carried implicitly — what the facade's
/// scraper produced from the legacy redacted document, which is the string
/// every production pairing record holds. Migration must persist them
/// byte-identically; the Zig-side `s3:`/`webdav:`-prefixed variants never
/// reached a pairing record and must not be resurrected here.
const legacy_s3_fingerprint = "https://abc123.r2.cloudflarestorage.com/bk-saves";
const legacy_webdav_fingerprint = "https://cloud.example.net/remote.php/dav/files/player";

const Fixture = struct {
    tmp: std.testing.TmpDir,
    gpa: std.mem.Allocator,
    root: []u8,
    file: []u8,

    fn init(gpa: std.mem.Allocator) !Fixture {
        var tmp = std.testing.tmpDir(.{});
        errdefer tmp.cleanup();

        var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const len = try tmp.dir.realPath(io, &buffer);
        const root = try gpa.dupe(u8, buffer[0..len]);
        errdefer gpa.free(root);

        const file = try path.join(gpa, &.{ root, "profiles", creds.file_name });
        return .{ .tmp = tmp, .gpa = gpa, .root = root, .file = file };
    }

    fn deinit(self: *Fixture) void {
        self.gpa.free(self.file);
        self.gpa.free(self.root);
        self.tmp.cleanup();
        self.* = undefined;
    }

    fn writeRaw(self: *Fixture, data: []const u8) !void {
        try std.Io.Dir.cwd().createDirPath(io, path.dirname(self.file).?);
        try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = self.file, .data = data });
    }

    fn readRaw(self: *Fixture, gpa: std.mem.Allocator) ![]u8 {
        return std.Io.Dir.cwd().readFileAlloc(io, self.file, gpa, .limited(creds.max_document_bytes));
    }
};

/// The generic shape the legacy S3 file migrates into: rclone option names,
/// bucket as the remote root, both `Sensitive` fields flagged secret — the
/// flags v1.75.0's catalogue declares for these options.
fn s3Options() [5]creds.Option {
    return .{
        .{ .name = "provider", .value = "Cloudflare" },
        .{ .name = "endpoint", .value = "https://abc123.r2.cloudflarestorage.com" },
        .{ .name = "region", .value = "auto" },
        .{ .name = "access_key_id", .value = fixture_access_key, .secret = true },
        .{ .name = "secret_access_key", .value = fixture_secret, .secret = true },
    };
}

fn s3Fixture(options: []creds.Option) creds.Credentials {
    return .{
        .backend = "s3",
        .remote_root = "bk-saves",
        .options = options,
        .rclone_path = "/opt/rclone/rclone",
    };
}

test "a legacy s3 file migrates with a byte-identical fingerprint" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    try fixture.writeRaw(legacy_s3_doc);

    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();

    try std.testing.expectEqualStrings("s3", loaded.creds.backend);
    // The bucket is a path component, not an option: migrating it as an
    // option would route every sync at the account root instead.
    try std.testing.expectEqualStrings("bk-saves", loaded.creds.remote_root);
    try std.testing.expect(loaded.creds.option("bucket") == null);

    try std.testing.expectEqualStrings("Cloudflare", loaded.creds.option("provider").?.value);
    try std.testing.expectEqualStrings(
        "https://abc123.r2.cloudflarestorage.com",
        loaded.creds.option("endpoint").?.value,
    );
    try std.testing.expectEqualStrings("auto", loaded.creds.option("region").?.value);

    // Both `Sensitive` fields carry the flag; neither is `IsPassword` —
    // rclone obscures neither, and the phase-03 read-back may touch both.
    const key = loaded.creds.option("access_key_id").?;
    try std.testing.expectEqualStrings(fixture_access_key, key.value);
    try std.testing.expect(key.secret and !key.is_password);
    const secret = loaded.creds.option("secret_access_key").?;
    try std.testing.expectEqualStrings(fixture_secret, secret.value);
    try std.testing.expect(secret.secret and !secret.is_password);

    // The pairing identity carries over exactly; a changed fingerprint would
    // demand a re-pair and look like a new remote.
    try std.testing.expectEqualStrings(legacy_s3_fingerprint, loaded.creds.fingerprint);
    try std.testing.expectEqualStrings("/opt/rclone/rclone", loaded.creds.rclone_path.?);

    // Same bucket path as the two-arm build built.
    const target = try creds.aliasTarget(gpa, loaded.creds);
    defer gpa.free(target);
    try std.testing.expectEqualStrings("bkraw:bk-saves", target);
    try std.testing.expectEqualStrings("bkremote", creds.remoteName(loaded.creds));

    // Saving rewrites the file in the generic schema, fingerprint intact.
    try creds.save(gpa, io, fixture.file, loaded.creds);
    const raw = try fixture.readRaw(gpa);
    defer gpa.free(raw);
    try std.testing.expect(std.mem.indexOf(u8, raw, "\"backend\":\"s3\"") != null);
    try std.testing.expect(std.mem.indexOf(u8, raw, "\"protocol\"") == null);
    try std.testing.expect(std.mem.indexOf(u8, raw, legacy_s3_fingerprint) != null);

    var reloaded = (try creds.load(gpa, io, fixture.file)).?;
    defer reloaded.deinit();
    try std.testing.expectEqualStrings(legacy_s3_fingerprint, reloaded.creds.fingerprint);
    try std.testing.expectEqualStrings(fixture_secret, reloaded.creds.option("secret_access_key").?.value);
}

test "a legacy webdav file migrates with its password flagged" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    try fixture.writeRaw(legacy_webdav_doc);

    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();

    try std.testing.expectEqualStrings("webdav", loaded.creds.backend);
    // The URL already names the tree; WebDAV has no separate root.
    try std.testing.expectEqualStrings("", loaded.creds.remote_root);
    try std.testing.expectEqualStrings(
        "https://cloud.example.net/remote.php/dav/files/player",
        loaded.creds.option("url").?.value,
    );
    try std.testing.expectEqualStrings("nextcloud", loaded.creds.option("vendor").?.value);

    // `user` is `Sensitive` in the catalogue, so it is withheld like a
    // secret; `pass` is the one `IsPassword` field, which is what the
    // phase-03 token read-back must never overwrite.
    const user = loaded.creds.option("user").?;
    try std.testing.expectEqualStrings("player", user.value);
    try std.testing.expect(user.secret and !user.is_password);
    const pass = loaded.creds.option("pass").?;
    try std.testing.expectEqualStrings("correct horse battery staple", pass.value);
    try std.testing.expect(pass.secret and pass.is_password);

    try std.testing.expectEqualStrings(legacy_webdav_fingerprint, loaded.creds.fingerprint);
    try std.testing.expect(loaded.creds.rclone_path == null);

    const target = try creds.aliasTarget(gpa, loaded.creds);
    defer gpa.free(target);
    try std.testing.expectEqualStrings("bkraw:", target);

    try creds.save(gpa, io, fixture.file, loaded.creds);
    var reloaded = (try creds.load(gpa, io, fixture.file)).?;
    defer reloaded.deinit();
    try std.testing.expectEqualStrings(legacy_webdav_fingerprint, reloaded.creds.fingerprint);
    try std.testing.expect(reloaded.creds.option("pass").?.is_password);
}

test "a 40-character secret and 20-character key survive the round trip" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    var options = s3Options();
    try creds.save(gpa, io, fixture.file, s3Fixture(&options));

    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();

    try std.testing.expectEqualStrings("s3", loaded.creds.backend);
    try std.testing.expectEqualStrings("bk-saves", loaded.creds.remote_root);
    try std.testing.expectEqualStrings(fixture_secret, loaded.creds.option("secret_access_key").?.value);
    try std.testing.expectEqualStrings(fixture_access_key, loaded.creds.option("access_key_id").?.value);
    try std.testing.expect(loaded.creds.option("secret_access_key").?.secret);
    try std.testing.expect(loaded.creds.option("access_key_id").?.secret);
    try std.testing.expectEqualStrings("Cloudflare", loaded.creds.option("provider").?.value);
    try std.testing.expectEqualStrings("auto", loaded.creds.option("region").?.value);
    try std.testing.expectEqualStrings("/opt/rclone/rclone", loaded.creds.rclone_path.?);

    // No `.tmp` residue: the write is rename-published.
    const tmp_residue = try std.fmt.allocPrint(gpa, "{s}.tmp", .{fixture.file});
    defer gpa.free(tmp_residue);
    try std.testing.expectError(
        error.FileNotFound,
        std.Io.Dir.cwd().statFile(io, tmp_residue, .{}),
    );
}

test "the file is written owner-only on posix" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    var options = s3Options();
    try creds.save(gpa, io, fixture.file, s3Fixture(&options));

    const stat = try std.Io.Dir.cwd().statFile(io, fixture.file, .{});
    // 0o600: the secret is readable by the player's account and nobody
    // else's on a shared machine.
    try std.testing.expectEqual(@as(u32, 0o600), @as(u32, @intCast(stat.permissions.toMode() & 0o777)));
}

test "a missing, malformed, or unknown-schema file reads as no credentials" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    // Missing.
    try std.testing.expect((try creds.load(gpa, io, fixture.file)) == null);

    // Malformed: must not make the game unstartable.
    try fixture.writeRaw("{corrupt");
    try std.testing.expect((try creds.load(gpa, io, fixture.file)) == null);

    // A protocol from a build neither old nor new: tolerated, not fatal.
    try fixture.writeRaw(
        \\{"protocol":"quantum-entanglement","rclone_path":null}
    );
    try std.testing.expect((try creds.load(gpa, io, fixture.file)) == null);

    // A document that names no schema at all.
    try fixture.writeRaw(
        \\{"answer":42}
    );
    try std.testing.expect((try creds.load(gpa, io, fixture.file)) == null);
}

test "an oversized file is a reported error, never silent loss" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    // One byte past the documented limit. The old 16 KiB cap returned null
    // here, which read as "no credentials saved" — silent loss.
    const oversized = try gpa.alloc(u8, creds.max_document_bytes + 1);
    defer gpa.free(oversized);
    @memset(oversized, 'x');
    try fixture.writeRaw(oversized);

    try std.testing.expectError(
        error.CredentialsTooLarge,
        creds.load(gpa, io, fixture.file),
    );
}

test "a 64 KiB document round-trips" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    // An OAuth token document is a JSON blob of rclone's choosing; 64 KiB is
    // far past any real one and far past the old cap that would have zeroed
    // the whole credential.
    const big = try gpa.alloc(u8, 64 * 1024);
    defer gpa.free(big);
    @memset(big, 'A');

    var options = [_]creds.Option{
        .{ .name = "client_id", .value = "player-app" },
        .{ .name = "token", .value = big, .secret = true, .is_password = false },
    };
    try creds.save(gpa, io, fixture.file, .{
        .backend = "drive",
        .options = &options,
    });

    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();
    try std.testing.expectEqualStrings(big, loaded.creds.option("token").?.value);
    try std.testing.expect(loaded.creds.option("token").?.secret);
    try std.testing.expectEqualStrings("player-app", loaded.creds.option("client_id").?.value);
}

test "editing the endpoint preserves the omitted secrets and rotates the fingerprint" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    var options = s3Options();
    try creds.save(gpa, io, fixture.file, s3Fixture(&options));

    var before = (try creds.load(gpa, io, fixture.file)).?;
    defer before.deinit();
    const old_fingerprint = try gpa.dupe(u8, before.creds.fingerprint);
    defer gpa.free(old_fingerprint);
    try std.testing.expect(old_fingerprint.len != 0);

    // The dialog's save: the redacted document's shape sent back with the
    // endpoint changed — stored secrets named, never valued.
    var incoming = (try creds.parse(gpa,
        \\{"backend":"s3","remote_root":"bk-saves",
        \\"options":{"provider":"Cloudflare","endpoint":"https://other.r2.cloudflarestorage.com","region":"auto"},
        \\"secret_options":["access_key_id","secret_access_key"],
        \\"rclone_path":null}
    )).?;
    defer incoming.deinit();
    try std.testing.expect(!incoming.creds.hasSecret());

    creds.mergeOmittedSecret(&incoming.creds, before.creds);
    try std.testing.expect(incoming.creds.hasSecret());
    try creds.save(gpa, io, fixture.file, incoming.creds);

    var reloaded = (try creds.load(gpa, io, fixture.file)).?;
    defer reloaded.deinit();
    try std.testing.expectEqualStrings(
        "https://other.r2.cloudflarestorage.com",
        reloaded.creds.option("endpoint").?.value,
    );
    // Both withheld fields still authenticate.
    try std.testing.expectEqualStrings(fixture_secret, reloaded.creds.option("secret_access_key").?.value);
    try std.testing.expectEqualStrings(fixture_access_key, reloaded.creds.option("access_key_id").?.value);
    // A different server is a different remote: the fingerprint rotated.
    try std.testing.expect(reloaded.creds.fingerprint.len != 0);
    try std.testing.expect(!std.mem.eql(u8, old_fingerprint, reloaded.creds.fingerprint));
}

test "a password-only edit keeps a migrated fingerprint verbatim" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    try fixture.writeRaw(legacy_s3_doc);

    var stored = (try creds.load(gpa, io, fixture.file)).?;
    defer stored.deinit();
    try std.testing.expectEqualStrings(legacy_s3_fingerprint, stored.creds.fingerprint);

    // Same connection, new secret. The generic digest of this configuration
    // will never equal the legacy string, so recomputing on any change would
    // turn a password edit into an apparent new remote and a re-pair demand.
    var incoming = (try creds.parse(gpa,
        \\{"backend":"s3","remote_root":"bk-saves",
        \\"options":{"provider":"Cloudflare","endpoint":"https://abc123.r2.cloudflarestorage.com",
        \\"region":"auto","secret_access_key":"REPLACEMENTSECRETwJalrXUtnFEMIK7MDENGbPx"},
        \\"secret_options":["access_key_id","secret_access_key"],
        \\"rclone_path":"/opt/rclone/rclone"}
    )).?;
    defer incoming.deinit();

    creds.mergeOmittedSecret(&incoming.creds, stored.creds);
    try std.testing.expectEqualStrings(legacy_s3_fingerprint, incoming.creds.fingerprint);
    try creds.save(gpa, io, fixture.file, incoming.creds);

    var reloaded = (try creds.load(gpa, io, fixture.file)).?;
    defer reloaded.deinit();
    try std.testing.expectEqualStrings(legacy_s3_fingerprint, reloaded.creds.fingerprint);
    try std.testing.expectEqualStrings(
        "REPLACEMENTSECRETwJalrXUtnFEMIK7MDENGbPx",
        reloaded.creds.option("secret_access_key").?.value,
    );
    // The omitted access key was preserved alongside the replaced secret.
    try std.testing.expectEqualStrings(fixture_access_key, reloaded.creds.option("access_key_id").?.value);
}

test "secrets never cross a backend change" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    var options = s3Options();
    try creds.save(gpa, io, fixture.file, s3Fixture(&options));

    var stored = (try creds.load(gpa, io, fixture.file)).?;
    defer stored.deinit();

    // `pass` recurs across backends; omission means preserve — but only
    // while the backend is unchanged, or one service's password would be
    // applied to another.
    var incoming = (try creds.parse(gpa,
        \\{"backend":"sftp","remote_root":"",
        \\"options":{"host":"sftp.example.net","user":"player"},
        \\"secret_options":["pass"],"password_options":["pass"],
        \\"rclone_path":null}
    )).?;
    defer incoming.deinit();

    creds.mergeOmittedSecret(&incoming.creds, stored.creds);
    try std.testing.expect(!incoming.creds.hasSecret());
    try std.testing.expectEqualStrings("", incoming.creds.fingerprint);

    try creds.save(gpa, io, fixture.file, incoming.creds);
    const raw = try fixture.readRaw(gpa);
    defer gpa.free(raw);
    try std.testing.expect(std.mem.indexOf(u8, raw, fixture_secret) == null);
    try std.testing.expect(std.mem.indexOf(u8, raw, "secret_access_key") == null);
}

test "a transitional generic document upgrades its fingerprint to the record's format" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    // Byte-for-byte what the first generic-schema build (06601b7c6) wrote
    // for a migrated legacy file: the fingerprint in this module's old
    // s3:-prefixed derivation, which no pairing record ever held.
    try fixture.writeRaw(
        \\{"backend":"s3","remote_root":"bk-saves","fingerprint":"s3:https://abc123.r2.cloudflarestorage.com/bk-saves","options":{"provider":"Cloudflare","endpoint":"https://abc123.r2.cloudflarestorage.com","region":"auto","access_key_id":"AKIAIOSFODNN7EXAMPLE","secret_access_key":"wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"},"secret_options":["access_key_id","secret_access_key"],"password_options":[],"rclone_path":"/opt/rclone/rclone"}
    );

    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();
    try std.testing.expectEqualStrings(legacy_s3_fingerprint, loaded.creds.fingerprint);
    try std.testing.expectEqualStrings(fixture_secret, loaded.creds.option("secret_access_key").?.value);

    // The upgrade persists, and a later password-only edit carries the
    // upgraded value verbatim like any other stored fingerprint.
    try creds.save(gpa, io, fixture.file, loaded.creds);
    var reloaded = (try creds.load(gpa, io, fixture.file)).?;
    defer reloaded.deinit();
    try std.testing.expectEqualStrings(legacy_s3_fingerprint, reloaded.creds.fingerprint);

    // The WebDAV variant of the same window.
    try fixture.writeRaw(
        \\{"backend":"webdav","remote_root":"","fingerprint":"webdav:https://cloud.example.net/remote.php/dav/files/player","options":{"url":"https://cloud.example.net/remote.php/dav/files/player","vendor":"nextcloud","user":"player","pass":"correct horse battery staple"},"secret_options":["user","pass"],"password_options":["pass"],"rclone_path":null}
    );
    var dav = (try creds.load(gpa, io, fixture.file)).?;
    defer dav.deinit();
    try std.testing.expectEqualStrings(legacy_webdav_fingerprint, dav.creds.fingerprint);
}

test "a fingerprint that merely resembles the old derivation is preserved" {
    const gpa = std.testing.allocator;

    // The upgrade must fire only when the stored value is provably the old
    // derivation of these same credentials — anything else is an established
    // identity, and rewriting it is exactly the rotation the contract
    // forbids. "s3:" happens to prefix this value; the endpoint inside does
    // not match the stored options, so it stays byte-identical.
    var incoming = (try creds.parse(gpa,
        \\{"backend":"s3","remote_root":"bk-saves",
        \\"options":{"endpoint":"https://abc123.r2.cloudflarestorage.com"},
        \\"fingerprint":"s3:https://elsewhere.example.net/bk-saves",
        \\"rclone_path":null}
    )).?;
    defer incoming.deinit();
    try std.testing.expectEqualStrings(
        "s3:https://elsewhere.example.net/bk-saves",
        incoming.creds.fingerprint,
    );
}

test "two webdav servers get different fingerprints" {
    const gpa = std.testing.allocator;

    // Under the old derivation both of these were the identical string
    // `webdav:` — every WebDAV configuration collapsed into one remote.
    var one_options = [_]creds.Option{
        .{ .name = "url", .value = "https://one.example.net/dav" },
        .{ .name = "pass", .value = "hunter2", .secret = true, .is_password = true },
    };
    var two_options = [_]creds.Option{
        .{ .name = "url", .value = "https://two.example.net/dav" },
        .{ .name = "pass", .value = "hunter2", .secret = true, .is_password = true },
    };
    const one: creds.Credentials = .{ .backend = "webdav", .options = &one_options };
    const two: creds.Credentials = .{ .backend = "webdav", .options = &two_options };

    const print_one = try creds.fingerprint(gpa, one);
    defer gpa.free(print_one);
    const print_two = try creds.fingerprint(gpa, two);
    defer gpa.free(print_two);

    try std.testing.expect(!std.mem.eql(u8, print_one, print_two));
    // "No secret material" is a contract, not a description.
    try std.testing.expect(std.mem.indexOf(u8, print_one, "hunter2") == null);
    try std.testing.expect(std.mem.indexOf(u8, print_two, "hunter2") == null);
}

test "the redacted form withholds every secret value using persisted flags alone" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    var options = s3Options();
    try creds.save(gpa, io, fixture.file, s3Fixture(&options));

    // Loaded back from disk: the only classification available is what the
    // save persisted. No catalogue is anywhere near this test — the cache
    // can be absent while credentials still must load safely.
    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();

    const doc = try creds.redacted(gpa, loaded.creds);
    defer gpa.free(doc);
    try std.testing.expect(std.mem.indexOf(u8, doc, fixture_secret) == null);
    try std.testing.expect(std.mem.indexOf(u8, doc, fixture_access_key) == null);
    try std.testing.expect(std.mem.indexOf(u8, doc, "\"has_secret\":true") != null);
    // The stored secrets are named, so the dialog can offer to keep them.
    try std.testing.expect(std.mem.indexOf(u8, doc, "\"secret_access_key\"") != null);
    try std.testing.expect(std.mem.indexOf(u8, doc, "\"access_key_id\"") != null);
    // The editable fields survive, flat — the dialog needs them.
    try std.testing.expect(std.mem.indexOf(u8, doc,
        "\"endpoint\":\"https://abc123.r2.cloudflarestorage.com\"") != null);
    try std.testing.expect(std.mem.indexOf(u8, doc, "\"provider\":\"Cloudflare\"") != null);
    // The fingerprint stays home: an echoed stale identity must never ride
    // back in through a dialog save.
    try std.testing.expect(std.mem.indexOf(u8, doc, "\"fingerprint\"") == null);

    var empty_options = [_]creds.Option{
        .{ .name = "url", .value = "https://u" },
        .{ .name = "pass", .value = "", .secret = true, .is_password = true },
    };
    const empty_doc = try creds.redacted(gpa, .{ .backend = "webdav", .options = &empty_options });
    defer gpa.free(empty_doc);
    try std.testing.expect(std.mem.indexOf(u8, empty_doc, "\"has_secret\":false") != null);
}

test "remote parameters emit the backend type and every saved option" {
    const gpa = std.testing.allocator;

    // Neither of the two backends the two-arm build shipped: the emission
    // must not rest on them. Names from the sftp backend in the fixture.
    var options = [_]creds.Option{
        .{ .name = "host", .value = "sftp.example.net", .secret = true },
        .{ .name = "user", .value = "player", .secret = true },
        .{ .name = "port", .value = "2022" },
        .{ .name = "pass", .value = "correct horse", .secret = true, .is_password = true },
        .{ .name = "key_file", .value = "" },
    };
    const sftp: creds.Credentials = .{
        .backend = "sftp",
        .remote_root = "backups/bk",
        .options = &options,
    };

    var params = try creds.remoteParams(gpa, sftp);
    defer params.deinit();
    const object = params.value.object;
    try std.testing.expectEqualStrings("sftp", object.get("type").?.string);
    // A flat `Name`-keyed map: backend configuration is keyed by `Name`, and
    // no v1.75.0 backend option has a differing non-empty `FieldName`.
    try std.testing.expectEqualStrings("sftp.example.net", object.get("host").?.string);
    try std.testing.expectEqualStrings("player", object.get("user").?.string);
    try std.testing.expectEqualStrings("2022", object.get("port").?.string);
    try std.testing.expectEqualStrings("correct horse", object.get("pass").?.string);
    // An unset option is not a parameter; sending `""` would override the
    // backend's default with an explicit empty.
    try std.testing.expect(object.get("key_file") == null);
    // The root is a path component, carried by the alias target.
    try std.testing.expect(object.get("remote_root") == null);

    const target = try creds.aliasTarget(gpa, sftp);
    defer gpa.free(target);
    try std.testing.expectEqualStrings("bkraw:backups/bk", target);
    // Path2's contribution to the session name is the alias name, constant
    // regardless of backend or root length.
    try std.testing.expectEqualStrings("bkremote", creds.remoteName(sftp));
}

test "reserialising an unchanged configuration keeps the fingerprint" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    var options = s3Options();
    try creds.save(gpa, io, fixture.file, s3Fixture(&options));

    var first = (try creds.load(gpa, io, fixture.file)).?;
    defer first.deinit();
    try std.testing.expect(first.creds.fingerprint.len != 0);

    try creds.save(gpa, io, fixture.file, first.creds);
    var second = (try creds.load(gpa, io, fixture.file)).?;
    defer second.deinit();
    try std.testing.expectEqualStrings(first.creds.fingerprint, second.creds.fingerprint);
}

test "clear_secret removes every secret and a merge cannot resurrect them" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    var options = s3Options();
    try creds.save(gpa, io, fixture.file, s3Fixture(&options));

    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();
    const fingerprint_before = try gpa.dupe(u8, loaded.creds.fingerprint);
    defer gpa.free(fingerprint_before);

    creds.clearSecret(&loaded.creds);
    try creds.save(gpa, io, fixture.file, loaded.creds);

    var reloaded = (try creds.load(gpa, io, fixture.file)).?;
    defer reloaded.deinit();
    try std.testing.expect(!reloaded.creds.hasSecret());
    // Both flagged fields went, values and all; the connection fields stay.
    try std.testing.expect(reloaded.creds.option("secret_access_key") == null);
    try std.testing.expect(reloaded.creds.option("access_key_id") == null);
    try std.testing.expectEqualStrings("Cloudflare", reloaded.creds.option("provider").?.value);
    const raw = try fixture.readRaw(gpa);
    defer gpa.free(raw);
    try std.testing.expect(std.mem.indexOf(u8, raw, fixture_secret) == null);
    try std.testing.expect(std.mem.indexOf(u8, raw, fixture_access_key) == null);
    // A secret-only change is not a new remote.
    try std.testing.expectEqualStrings(fingerprint_before, reloaded.creds.fingerprint);

    // And after a clear there is nothing for a later merge to resurrect.
    var incoming = (try creds.parse(gpa,
        \\{"backend":"s3","remote_root":"bk-saves",
        \\"options":{"endpoint":"https://abc123.r2.cloudflarestorage.com"},
        \\"secret_options":["access_key_id","secret_access_key"],
        \\"rclone_path":null}
    )).?;
    defer incoming.deinit();
    creds.mergeOmittedSecret(&incoming.creds, reloaded.creds);
    try std.testing.expect(!incoming.creds.hasSecret());
}
