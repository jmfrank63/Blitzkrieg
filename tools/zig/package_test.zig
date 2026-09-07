//! Tests for the release zip writer.
//!
//! Two properties matter here and neither is visible from the archive's size:
//!
//! - **The mode survives.** A zip carries POSIX permissions only in the
//!   central directory's external file attributes, and only when the
//!   "version made by" field claims a UNIX host. Get either wrong and every
//!   file extracts `0644` — which for `Game` and the bundled `rclone` means an
//!   install that cannot run. The encoding is checked directly *and*, where a
//!   system `unzip` exists, through a real extraction, because the point is
//!   what other extractors do with the bytes, not what we think they mean.
//! - **The output is whole or absent.** A failed run must not leave a
//!   plausible-looking archive behind.
//!
//! Determinism is asserted by packaging the same tree twice and comparing the
//! bytes: the release hash is only meaningful if it is reproducible.

const std = @import("std");
const builtin = @import("builtin");
const package = @import("package.zig");

const exec_mode: u16 = 0o755;
const data_mode: u16 = 0o644;

/// Build a fixture tree with one executable, one plain data file and one
/// executable a directory down, so the mode is exercised on a nested path too.
fn writeFixture(io: std.Io, dir: std.Io.Dir) !void {
    try dir.createDirPath(io, "tree/bin");
    try writeMode(io, dir, "tree/bin/tool", "#!/bin/sh\nexit 0\n", exec_mode);
    try writeMode(io, dir, "tree/notes.txt", "plain data\n", data_mode);
    try writeMode(io, dir, "tree/launch", "#!/bin/sh\nexit 0\n", exec_mode);
}

/// `createFile`'s permissions are filtered by the process umask, so the mode is
/// set explicitly afterwards; otherwise a `0o022` umask would silently turn the
/// fixture's intent into something else.
fn writeMode(io: std.Io, dir: std.Io.Dir, sub_path: []const u8, data: []const u8, mode: u16) !void {
    var file = try dir.createFile(io, sub_path, .{ .truncate = true });
    defer file.close(io);
    try file.writeStreamingAll(io, data);
    if (builtin.os.tag != .windows) try file.setPermissions(io, .fromMode(mode));
}

fn readAll(io: std.Io, dir: std.Io.Dir, sub_path: []const u8, gpa: std.mem.Allocator) ![]u8 {
    var file = try dir.openFile(io, sub_path, .{});
    defer file.close(io);
    const stat = try file.stat(io);
    const bytes = try gpa.alloc(u8, @intCast(stat.size));
    errdefer gpa.free(bytes);
    var offset: u64 = 0;
    while (offset < bytes.len) {
        const n = try file.readPositional(io, &.{bytes[@intCast(offset)..]}, offset);
        if (n == 0) break;
        offset += n;
    }
    return bytes;
}

/// The central directory record for `name`, located by scanning for its
/// signature. Enough of a reader to assert on the fields the writer owns.
const CentralRecord = struct {
    version_made_by: u16,
    external_attributes: u32,
    mod_time: u16,
    mod_date: u16,

    fn find(zip: []const u8, name: []const u8) ?CentralRecord {
        var index: usize = 0;
        while (std.mem.indexOfPos(u8, zip, index, "PK\x01\x02")) |at| {
            index = at + 4;
            if (at + 46 > zip.len) return null;
            const name_len = std.mem.readInt(u16, zip[at + 28 ..][0..2], .little);
            if (at + 46 + name_len > zip.len) return null;
            if (std.mem.eql(u8, zip[at + 46 ..][0..name_len], name)) return .{
                .version_made_by = std.mem.readInt(u16, zip[at + 4 ..][0..2], .little),
                .mod_time = std.mem.readInt(u16, zip[at + 12 ..][0..2], .little),
                .mod_date = std.mem.readInt(u16, zip[at + 14 ..][0..2], .little),
                .external_attributes = std.mem.readInt(u32, zip[at + 38 ..][0..4], .little),
            };
        }
        return null;
    }
};

test "the archive records each file's own POSIX mode" {
    if (builtin.os.tag == .windows) return;

    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);

    const root = try tmp.dir.realPathFileAlloc(io, ".", gpa);
    defer gpa.free(root);
    const source = try std.fs.path.join(gpa, &.{ root, "tree" });
    defer gpa.free(source);
    const zip_path = try std.fs.path.join(gpa, &.{ root, "out.zip" });
    defer gpa.free(zip_path);

    try package.createPackage(io, gpa, source, zip_path);

    const zip = try readAll(io, tmp.dir, "out.zip", gpa);
    defer gpa.free(zip);

    const tool = CentralRecord.find(zip, "bin/tool") orelse return error.MissingEntry;
    const notes = CentralRecord.find(zip, "notes.txt") orelse return error.MissingEntry;

    // The high byte of "version made by" is the host system; 3 is UNIX, and
    // without it extractors ignore the attributes entirely.
    try std.testing.expectEqual(@as(u16, 3), tool.version_made_by >> 8);
    // The mode lives in the high sixteen bits, alongside the S_IFREG file type.
    try std.testing.expectEqual(@as(u32, exec_mode), (tool.external_attributes >> 16) & 0o7777);
    try std.testing.expectEqual(@as(u32, 0o100000), (tool.external_attributes >> 16) & 0o170000);
    // A data file keeps its own mode rather than inheriting the executable's.
    try std.testing.expectEqual(@as(u32, data_mode), (notes.external_attributes >> 16) & 0o7777);

    // Reproducibility rests on the fixed 1980-00-00 stamp; permissions must not
    // have smuggled a real timestamp in with them.
    try std.testing.expectEqual(@as(u16, 0), tool.mod_time);
    try std.testing.expectEqual(@as(u16, 0), tool.mod_date);
}

test "a real extractor restores the executable bit" {
    if (builtin.os.tag == .windows) return;

    const io = std.testing.io;
    const gpa = std.testing.allocator;

    // No system unzip, nothing to prove; the encoding test above still runs.
    const unzip = "/usr/bin/unzip";
    _ = std.Io.Dir.cwd().statFile(io, unzip, .{}) catch return;

    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);
    try tmp.dir.createDirPath(io, "extracted");

    const root = try tmp.dir.realPathFileAlloc(io, ".", gpa);
    defer gpa.free(root);
    const source = try std.fs.path.join(gpa, &.{ root, "tree" });
    defer gpa.free(source);
    const zip_path = try std.fs.path.join(gpa, &.{ root, "out.zip" });
    defer gpa.free(zip_path);
    const dest = try std.fs.path.join(gpa, &.{ root, "extracted" });
    defer gpa.free(dest);

    try package.createPackage(io, gpa, source, zip_path);

    const result = try std.process.run(gpa, io, .{
        .argv = &.{ unzip, "-q", "-o", zip_path, "-d", dest },
        .stdout_limit = .limited(64 * 1024),
        .stderr_limit = .limited(64 * 1024),
    });
    defer gpa.free(result.stdout);
    defer gpa.free(result.stderr);
    try std.testing.expectEqual(std.process.Child.Term{ .exited = 0 }, result.term);

    const tool = try tmp.dir.statFile(io, "extracted/bin/tool", .{});
    const notes = try tmp.dir.statFile(io, "extracted/notes.txt", .{});
    try std.testing.expectEqual(@as(std.posix.mode_t, exec_mode), tool.permissions.toMode() & 0o7777);
    try std.testing.expectEqual(@as(std.posix.mode_t, data_mode), notes.permissions.toMode() & 0o7777);
}

test "two runs over the same tree produce identical bytes" {
    if (builtin.os.tag == .windows) return;

    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);

    const root = try tmp.dir.realPathFileAlloc(io, ".", gpa);
    defer gpa.free(root);
    const source = try std.fs.path.join(gpa, &.{ root, "tree" });
    defer gpa.free(source);
    const first_path = try std.fs.path.join(gpa, &.{ root, "first.zip" });
    defer gpa.free(first_path);
    const second_path = try std.fs.path.join(gpa, &.{ root, "second.zip" });
    defer gpa.free(second_path);

    try package.createPackage(io, gpa, source, first_path);
    try package.createPackage(io, gpa, source, second_path);

    const first = try readAll(io, tmp.dir, "first.zip", gpa);
    defer gpa.free(first);
    const second = try readAll(io, tmp.dir, "second.zip", gpa);
    defer gpa.free(second);
    try std.testing.expectEqualSlices(u8, first, second);
}

test "a failed run leaves no archive behind" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();

    const root = try tmp.dir.realPathFileAlloc(io, ".", gpa);
    defer gpa.free(root);
    const missing = try std.fs.path.join(gpa, &.{ root, "no-such-tree" });
    defer gpa.free(missing);
    const zip_path = try std.fs.path.join(gpa, &.{ root, "out.zip" });
    defer gpa.free(zip_path);

    // A previous good archive is not what is being checked; what must not
    // survive is a partial one that a release process would happily ship.
    try std.testing.expectError(error.FileNotFound, package.createPackage(io, gpa, missing, zip_path));
    try std.testing.expectError(error.FileNotFound, tmp.dir.statFile(io, "out.zip", .{}));
    try std.testing.expectError(error.FileNotFound, tmp.dir.statFile(io, "out.zip.partial", .{}));
}
