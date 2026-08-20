//! Tests for the credentials file.
//!
//! The lengths in the round-trip cases are not arbitrary: a real S3 secret
//! is 40 characters and an access key 20, and those are exactly the values
//! the option system's field lengths would have destroyed — the reason this
//! file exists at all.

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
};

fn s3Fixture() creds.Credentials {
    return .{
        .payload = .{ .s3 = .{
            .s3_provider = "Cloudflare",
            .endpoint = "https://abc123.r2.cloudflarestorage.com",
            .bucket = "bk-saves",
            .region = "auto",
            .access_key = fixture_access_key,
            .secret = fixture_secret,
        } },
        .rclone_path = "/opt/rclone/rclone",
    };
}

test "a 40-character secret and 20-character key survive the round trip" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    try creds.save(gpa, io, fixture.file, s3Fixture());

    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();

    const s3 = loaded.creds.payload.s3;
    try std.testing.expectEqualStrings(fixture_secret, s3.secret);
    try std.testing.expectEqualStrings(fixture_access_key, s3.access_key);
    try std.testing.expectEqualStrings("Cloudflare", s3.s3_provider);
    try std.testing.expectEqualStrings("https://abc123.r2.cloudflarestorage.com", s3.endpoint);
    try std.testing.expectEqualStrings("bk-saves", s3.bucket);
    try std.testing.expectEqualStrings("auto", s3.region);
    try std.testing.expectEqualStrings("/opt/rclone/rclone", loaded.creds.rclone_path.?);

    // No `.tmp` residue: the write is rename-published.
    const tmp_residue = try std.fmt.allocPrint(gpa, "{s}.tmp", .{fixture.file});
    defer gpa.free(tmp_residue);
    try std.testing.expectError(
        error.FileNotFound,
        std.Io.Dir.cwd().statFile(io, tmp_residue, .{}),
    );
}

test "webdav credentials round-trip through the tagged schema" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    try creds.save(gpa, io, fixture.file, .{
        .payload = .{ .webdav = .{
            .url = "https://cloud.example.net/remote.php/dav/files/player",
            .vendor = "nextcloud",
            .user = "player",
            .pass = "correct horse battery staple",
        } },
    });

    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();

    const dav = loaded.creds.payload.webdav;
    try std.testing.expectEqualStrings("nextcloud", dav.vendor);
    try std.testing.expectEqualStrings("player", dav.user);
    try std.testing.expectEqualStrings("correct horse battery staple", dav.pass);
    try std.testing.expect(loaded.creds.rclone_path == null);
}

test "the file is written owner-only on posix" {
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    try creds.save(gpa, io, fixture.file, s3Fixture());

    const stat = try std.Io.Dir.cwd().statFile(io, fixture.file, .{});
    // 0o600: the secret is readable by the player's account and nobody
    // else's on a shared machine.
    try std.testing.expectEqual(@as(u32, 0o600), @as(u32, @intCast(stat.permissions.toMode() & 0o777)));
}

test "a missing, malformed, or newer file reads as no credentials" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    // Missing.
    try std.testing.expect((try creds.load(gpa, io, fixture.file)) == null);

    // Malformed: must not make the game unstartable.
    try std.Io.Dir.cwd().createDirPath(io, path.dirname(fixture.file).?);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = fixture.file, .data = "{corrupt" });
    try std.testing.expect((try creds.load(gpa, io, fixture.file)) == null);

    // A protocol from a newer build: tolerated, not fatal.
    try std.Io.Dir.cwd().writeFile(io, .{
        .sub_path = fixture.file,
        .data =
        \\{"protocol":"quantum-entanglement","rclone_path":null}
        ,
    });
    try std.testing.expect((try creds.load(gpa, io, fixture.file)) == null);
}

test "edit_endpoint_only_preserves_secret" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    try creds.save(gpa, io, fixture.file, s3Fixture());

    // The dialog's save: every field except the secret, exactly what a
    // caller of creds_load can produce. The endpoint changed.
    var incoming = (try creds.parse(gpa,
        \\{"protocol":"s3","s3":{"s3_provider":"Cloudflare",
        \\"endpoint":"https://other.r2.cloudflarestorage.com","bucket":"bk-saves",
        \\"region":"auto","access_key":"AKIAIOSFODNN7EXAMPLE"},"rclone_path":null}
    )).?;
    defer incoming.deinit();
    try std.testing.expect(!incoming.creds.hasSecret());

    var stored = (try creds.load(gpa, io, fixture.file)).?;
    defer stored.deinit();
    creds.mergeOmittedSecret(&incoming.creds, stored.creds);
    try creds.save(gpa, io, fixture.file, incoming.creds);

    // The endpoint edit landed and the secret still authenticates.
    var reloaded = (try creds.load(gpa, io, fixture.file)).?;
    defer reloaded.deinit();
    try std.testing.expectEqualStrings(
        "https://other.r2.cloudflarestorage.com",
        reloaded.creds.payload.s3.endpoint,
    );
    try std.testing.expectEqualStrings(fixture_secret, reloaded.creds.payload.s3.secret);
}

test "clear_secret_removes_it" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    try creds.save(gpa, io, fixture.file, s3Fixture());

    var loaded = (try creds.load(gpa, io, fixture.file)).?;
    defer loaded.deinit();
    creds.clearSecret(&loaded.creds);
    try creds.save(gpa, io, fixture.file, loaded.creds);

    var reloaded = (try creds.load(gpa, io, fixture.file)).?;
    defer reloaded.deinit();
    try std.testing.expect(!reloaded.creds.hasSecret());
    try std.testing.expectEqualStrings("", reloaded.creds.payload.s3.secret);

    // And after a clear there is nothing for a later merge to resurrect.
    var incoming = (try creds.parse(gpa,
        \\{"protocol":"s3","s3":{"endpoint":"e"},"rclone_path":null}
    )).?;
    defer incoming.deinit();
    creds.mergeOmittedSecret(&incoming.creds, reloaded.creds);
    try std.testing.expect(!incoming.creds.hasSecret());
}

test "the redacted form withholds every secret-bearing field" {
    const gpa = std.testing.allocator;

    const s3_doc = try creds.redacted(gpa, s3Fixture());
    defer gpa.free(s3_doc);
    try std.testing.expect(std.mem.indexOf(u8, s3_doc, fixture_secret) == null);
    try std.testing.expect(std.mem.indexOf(u8, s3_doc, "\"secret\"") == null);
    try std.testing.expect(std.mem.indexOf(u8, s3_doc, "\"has_secret\":true") != null);
    // The non-secret fields all survive — the dialog needs them.
    try std.testing.expect(std.mem.indexOf(u8, s3_doc, fixture_access_key) != null);
    try std.testing.expect(std.mem.indexOf(u8, s3_doc, "bk-saves") != null);

    const dav_doc = try creds.redacted(gpa, .{
        .payload = .{ .webdav = .{ .url = "https://u", .vendor = "other", .user = "p", .pass = "hunter2" } },
    });
    defer gpa.free(dav_doc);
    try std.testing.expect(std.mem.indexOf(u8, dav_doc, "hunter2") == null);
    try std.testing.expect(std.mem.indexOf(u8, dav_doc, "\"pass\"") == null);
    try std.testing.expect(std.mem.indexOf(u8, dav_doc, "\"has_secret\":true") != null);

    const empty_doc = try creds.redacted(gpa, .{
        .payload = .{ .webdav = .{ .url = "https://u", .vendor = "", .user = "p", .pass = "" } },
    });
    defer gpa.free(empty_doc);
    try std.testing.expect(std.mem.indexOf(u8, empty_doc, "\"has_secret\":false") != null);
}

test "remote parameters and names keep the session-name contribution constant" {
    const gpa = std.testing.allocator;

    var params = try creds.remoteParams(gpa, s3Fixture());
    defer params.deinit();
    const object = params.value.object;
    try std.testing.expectEqualStrings("s3", object.get("type").?.string);
    // rclone's vendor word, from the field that is deliberately not called
    // "provider" on our side of the fence.
    try std.testing.expectEqualStrings("Cloudflare", object.get("provider").?.string);
    try std.testing.expectEqualStrings(fixture_access_key, object.get("access_key_id").?.string);
    try std.testing.expectEqualStrings(fixture_secret, object.get("secret_access_key").?.string);
    // The bucket is a path component, not a remote parameter.
    try std.testing.expect(object.get("bucket") == null);

    // Path2's contribution to the session name is the alias name, constant
    // regardless of endpoint or bucket length.
    try std.testing.expectEqualStrings("bkremote", creds.remoteName(s3Fixture()));
    const target = try creds.aliasTarget(gpa, s3Fixture());
    defer gpa.free(target);
    try std.testing.expectEqualStrings("bkraw:bk-saves", target);

    const dav: creds.Credentials = .{
        .payload = .{ .webdav = .{ .url = "https://u", .vendor = "other", .user = "p", .pass = "x" } },
    };
    const dav_target = try creds.aliasTarget(gpa, dav);
    defer gpa.free(dav_target);
    try std.testing.expectEqualStrings("bkraw:", dav_target);

    const print = try creds.fingerprint(gpa, s3Fixture());
    defer gpa.free(print);
    try std.testing.expectEqualStrings("s3:https://abc123.r2.cloudflarestorage.com/bk-saves", print);
    try std.testing.expect(std.mem.indexOf(u8, print, fixture_secret) == null);
}
