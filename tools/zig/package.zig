//! The release zip writer: one stored (uncompressed) entry per file under a
//! staged layout, sorted by name so that the same tree always produces the same
//! bytes and therefore the same package hash.
//!
//! Two properties are load-bearing and easy to lose:
//!
//! - **Permissions.** A zip only carries a POSIX mode in the central
//!   directory's external file attributes, and extractors only look there when
//!   "version made by" claims a UNIX host. Without both, everything extracts
//!   `0644` and the shipped `Game` and bundled `rclone` are not executable.
//! - **File handles.** A full game layout is tens of thousands of files, more
//!   than `kern.maxfilesperproc` allows a process to hold at once, so each
//!   entry is opened, written and closed before the next one is opened. No
//!   `ulimit` raise fixes a design that scales handles with file count.
//!
//! The archive is built beside its destination and renamed into place, so a run
//! that fails partway leaves no half-written file that looks like a product.

const std = @import("std");
const builtin = @import("builtin");

const Entry = struct {
    /// Path relative to the source root, always with `/` separators — the form
    /// zip stores. Also the path the file is reopened by at write time.
    name: []u8,
    local_header_offset: u32 = 0,
    crc32: u32 = 0,
    size: u32 = 0,
    /// The source file's own POSIX mode, low twelve bits.
    mode: u16 = 0,
};

/// The high byte of "version made by" names the host system and the low byte
/// the zip specification version. 3 is UNIX; extractors that see anything else
/// treat the external attributes as MS-DOS flags and drop the mode entirely.
const version_made_by_unix: u16 = (3 << 8) | 20;

/// `S_IFREG`. The external attributes hold a complete `st_mode`, file type
/// included, not just the permission bits.
const s_ifreg: u16 = 0o100000;

/// The original zip format counts entries in sixteen bits and sizes and offsets
/// in thirty-two. The game layout is already at 63,728 of the 65,535 entries
/// allowed, so the ceiling is close enough to reach by accident — and a
/// truncated count or offset produces an archive that opens and lies about its
/// contents. Zip64 is the eventual answer; until then these fail loudly, and
/// `createPackage`'s errdefer means nothing is left behind when they do.
const max_entries = std.math.maxInt(u16);

fn narrow(value: u64) error{ArchiveTooLarge}!u32 {
    return std.math.cast(u32, value) orelse error.ArchiveTooLarge;
}

pub fn main(init: std.process.Init) !void {
    var args_it = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args_it.deinit();

    _ = args_it.skip();
    const source_root = args_it.next() orelse return error.InvalidArguments;
    const output_zip = args_it.next() orelse return error.InvalidArguments;

    try createPackage(init.io, init.gpa, source_root, output_zip);
}

pub fn createPackage(io: std.Io, gpa: std.mem.Allocator, source_root: []const u8, output_zip: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try ensureParentDir(io, output_zip);

    // Build beside the destination and rename only once the last byte is
    // flushed. The stale destination goes first as well: a failed run must not
    // leave yesterday's archive sitting where today's was expected.
    const partial = try std.fmt.allocPrint(gpa, "{s}.partial", .{output_zip});
    defer gpa.free(partial);
    cwd.deleteFile(io, partial) catch {};
    cwd.deleteFile(io, output_zip) catch {};
    errdefer cwd.deleteFile(io, partial) catch {};

    {
        var output_file = try cwd.createFile(io, partial, .{ .truncate = true, .read = false });
        defer output_file.close(io);

        var out_buffer: [64 * 1024]u8 = undefined;
        var writer = output_file.writer(io, &out_buffer);
        try writeArchive(io, gpa, source_root, &writer);
    }

    try cwd.rename(partial, cwd, output_zip, io);
}

fn writeArchive(io: std.Io, gpa: std.mem.Allocator, source_root: []const u8, writer: *std.Io.File.Writer) !void {
    const cwd = std.Io.Dir.cwd();
    var source_dir = try cwd.openDir(io, source_root, .{ .iterate = true });
    defer source_dir.close(io);

    var entries = std.ArrayList(Entry).empty;
    defer {
        for (entries.items) |entry| gpa.free(entry.name);
        entries.deinit(gpa);
    }

    // The walk collects names only. Opening here and holding the handle until
    // the archive is finished is what exhausted the process file limit.
    {
        var walker = try source_dir.walk(gpa);
        defer walker.deinit();

        while (try walker.next(io)) |entry| {
            if (entry.kind != .file) continue;

            const rel_name = try gpa.dupe(u8, entry.path);
            std.mem.replaceScalar(u8, rel_name, std.fs.path.sep, '/');
            entries.append(gpa, .{ .name = rel_name }) catch |err| {
                gpa.free(rel_name);
                return err;
            };
        }
    }

    std.mem.sort(Entry, entries.items, {}, struct {
        fn lessThan(_: void, lhs: Entry, rhs: Entry) bool {
            return std.mem.order(u8, lhs.name, rhs.name) == .lt;
        }
    }.lessThan);

    if (entries.items.len > max_entries) return error.TooManyEntries;

    for (entries.items) |*entry| {
        // One handle at a time: measured, written, closed. The CRC has to be
        // known before the local header, so the file is read twice, but it is
        // read through a single open and the second pass is warm.
        var file = try source_dir.openFile(io, entry.name, .{});
        defer file.close(io);

        const file_info = try measureFile(io, file);
        entry.crc32 = file_info.crc32;
        entry.size = file_info.size;
        entry.mode = file_info.mode;

        entry.local_header_offset = try narrow(writer.logicalPos());
        try writeLocalHeader(&writer.interface, entry.name, entry.crc32, entry.size);
        try writer.interface.writeAll(entry.name);
        try streamFile(io, file, &writer.interface);
    }

    const central_dir_offset: u32 = try narrow(writer.logicalPos());
    for (entries.items) |entry| {
        try writeCentralHeader(&writer.interface, entry);
        try writer.interface.writeAll(entry.name);
    }
    const central_dir_size: u32 = try narrow(writer.logicalPos() - central_dir_offset);

    try writer.interface.writeAll("PK\x05\x06");
    try writer.interface.writeInt(u16, 0, .little);
    try writer.interface.writeInt(u16, 0, .little);
    try writer.interface.writeInt(u16, @intCast(entries.items.len), .little);
    try writer.interface.writeInt(u16, @intCast(entries.items.len), .little);
    try writer.interface.writeInt(u32, central_dir_size, .little);
    try writer.interface.writeInt(u32, central_dir_offset, .little);
    try writer.interface.writeInt(u16, 0, .little);
    try writer.interface.flush();
}

const FileInfo = struct {
    crc32: u32,
    size: u32,
    mode: u16,
};

fn measureFile(io: std.Io, file: std.Io.File) !FileInfo {
    const stat = try file.stat(io);
    const size = try narrow(stat.size);

    var crc = std.hash.Crc32.init();
    var chunk: [64 * 1024]u8 = undefined;
    var offset: u64 = 0;
    while (true) {
        const n = try file.readPositional(io, &.{chunk[0..]}, offset);
        if (n == 0) break;
        crc.update(chunk[0..n]);
        offset += n;
    }
    return .{ .crc32 = crc.final(), .size = size, .mode = unixMode(stat.permissions) };
}

/// The mode to record for a source file. Each file keeps its own — a data file
/// stays `0644` and only what was executable on disk extracts executable.
///
/// Windows has no mode at all, and packaging from a Windows host still has to
/// produce an archive a POSIX extractor can use, so its one meaningful bit is
/// mapped onto the conventional pair.
fn unixMode(permissions: std.Io.File.Permissions) u16 {
    if (builtin.os.tag == .windows) return if (permissions.readOnly()) 0o444 else 0o644;
    return @intCast(permissions.toMode() & 0o7777);
}

fn streamFile(io: std.Io, file: std.Io.File, writer: *std.Io.Writer) !void {
    var chunk: [64 * 1024]u8 = undefined;
    var offset: u64 = 0;
    while (true) {
        const n = try file.readPositional(io, &.{chunk[0..]}, offset);
        if (n == 0) break;
        try writer.writeAll(chunk[0..n]);
        offset += n;
    }
}

fn ensureParentDir(io: std.Io, path: []const u8) !void {
    if (std.fs.path.dirname(path)) |dir_name| {
        const cwd = std.Io.Dir.cwd();
        try cwd.createDirPath(io, dir_name);
    }
}

fn writeLocalHeader(writer: *std.Io.Writer, name: []const u8, crc32: u32, size: u32) !void {
    const name_len: u16 = @intCast(name.len);
    try writer.writeAll("PK\x03\x04");
    try writer.writeInt(u16, 20, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u32, crc32, .little);
    try writer.writeInt(u32, size, .little);
    try writer.writeInt(u32, size, .little);
    try writer.writeInt(u16, name_len, .little);
    try writer.writeInt(u16, 0, .little);
}

fn writeCentralHeader(writer: *std.Io.Writer, entry: Entry) !void {
    const name_len: u16 = @intCast(entry.name.len);
    try writer.writeAll("PK\x01\x02");
    try writer.writeInt(u16, version_made_by_unix, .little);
    try writer.writeInt(u16, 20, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u32, entry.crc32, .little);
    try writer.writeInt(u32, entry.size, .little);
    try writer.writeInt(u32, entry.size, .little);
    try writer.writeInt(u16, name_len, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    try writer.writeInt(u16, 0, .little);
    // External file attributes. The low sixteen bits are MS-DOS flags, left
    // clear; the mode goes in the high sixteen, which is the encoding every
    // extractor honours.
    try writer.writeInt(u32, @as(u32, s_ifreg | entry.mode) << 16, .little);
    try writer.writeInt(u32, entry.local_header_offset, .little);
}
