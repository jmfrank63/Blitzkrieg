//! `bk-pak`: make, unpack, list and check the `.pak` archives the game mounts
//! (mods, GeneratedData). A `.pak` is a plain zip; this replaces the Info-ZIP
//! `zip.exe`/`unzip.exe` modders used to carry (Phase 8, D-05..D-07).
//!
//! `pack` is the release packager's own writer (`stored_zip.zig`): stored
//! entries, sorted, `/` names, fixed timestamps, so the same tree always gives
//! the same bytes. Reading accepts stored and deflate entries (`std.zip`) and
//! trusts nothing in the archive: names that would land outside the output
//! directory, data whose CRC-32 or size disagrees with the directory and Zip64
//! archives are refused, and `unpack` checks the whole archive before it
//! writes the first byte, so a hostile or damaged archive leaves nothing
//! behind. `std.zip` does not check CRCs itself (a TODO in 0.17), hence the
//! hashing writers here.

const std = @import("std");
const builtin = @import("builtin");
const bk_tool = @import("bk_tool.zig");
const stored_zip = @import("stored_zip.zig");

const usage: bk_tool.Usage = .{
    .synopsis =
    \\usage: bk-pak pack <dir> <out.pak> [--force]
    \\       bk-pak unpack <in.pak> <dir> [--force]
    \\       bk-pak list <in.pak>
    \\       bk-pak verify <in.pak>
    \\
    ,
    .body = &.{ "pak.usage.body", "pak.usage.note" },
};

pub fn main(init: std.process.Init) u8 {
    return bk_tool.main(init, "bk-pak", usage, run);
}

fn run(ctx: *bk_tool.Context) !void {
    if (ctx.args.len == 0) return error.Usage;
    const command = ctx.args[0];

    var positional: [2][]const u8 = undefined;
    var positional_count: usize = 0;
    var force = false;
    for (ctx.args[1..]) |arg| {
        if (std.mem.eql(u8, arg, "--force")) {
            force = true;
        } else if (std.mem.startsWith(u8, arg, "-") or positional_count == positional.len) {
            return error.Usage;
        } else {
            positional[positional_count] = arg;
            positional_count += 1;
        }
    }

    const Command = enum { pack, unpack, list, verify };
    const which = std.meta.stringToEnum(Command, command) orelse return error.Usage;
    const wanted: usize = switch (which) {
        .pack, .unpack => 2,
        .list, .verify => 1,
    };
    if (positional_count != wanted) return error.Usage;
    if (force and (which == .list or which == .verify)) return error.Usage;

    const first = try bk_tool.inputPath(ctx.gpa, positional[0]);
    defer ctx.gpa.free(first);
    const second = if (wanted == 2) try bk_tool.inputPath(ctx.gpa, positional[1]) else null;
    defer if (second) |path| ctx.gpa.free(path);

    var problem: Problem = .{};
    switch (which) {
        .pack => {
            const totals = pack(ctx.io, ctx.gpa, first, second.?, force, &problem) catch |err|
                return explain(ctx, err, &problem, second.?);
            try ctx.out.print("packed {d} files ({d} bytes) into {s}\n", .{ totals.files, totals.bytes, second.? });
        },
        .unpack => {
            const totals = unpack(ctx.io, ctx.gpa, first, second.?, force, &problem) catch |err|
                return explain(ctx, err, &problem, first);
            try ctx.out.print("unpacked {d} files ({d} bytes) into {s}\n", .{ totals.files, totals.bytes, second.? });
        },
        .list => {
            var entries = scan(ctx.io, ctx.gpa, first, .names_only, &problem) catch |err|
                return explain(ctx, err, &problem, first);
            defer entries.deinit(ctx.gpa);
            for (entries.items) |entry| {
                try ctx.out.print("{d:>12}  {s:<7}  {s}\n", .{ entry.size, @tagName(entry.method), entry.name });
            }
            const totals = entries.totals();
            try ctx.out.print("{d} files ({d} bytes) in {s}\n", .{ totals.files, totals.bytes, first });
        },
        .verify => {
            var entries = scan(ctx.io, ctx.gpa, first, .check_data, &problem) catch |err|
                return explain(ctx, err, &problem, first);
            defer entries.deinit(ctx.gpa);
            const totals = entries.totals();
            try ctx.out.print("{s}: {d} files ({d} bytes) verified\n", .{ first, totals.files, totals.bytes });
        },
    }
}

/// The entry a failure is about, copied out because the reading buffers are
/// gone by the time the message is written.
pub const Problem = struct {
    name_buffer: [256]u8 = undefined,
    name_len: usize = 0,

    fn set(problem: *Problem, entry_name: []const u8) void {
        problem.name_len = @min(entry_name.len, problem.name_buffer.len);
        @memcpy(problem.name_buffer[0..problem.name_len], entry_name[0..problem.name_len]);
    }

    pub fn name(problem: *const Problem) []const u8 {
        return problem.name_buffer[0..problem.name_len];
    }
};

/// One line of reason per failure a modder can act on; anything else falls
/// through to its error name, which is still one line on stderr.
fn explain(ctx: *bk_tool.Context, err: anyerror, problem: *const Problem, path: []const u8) error{Failed} {
    const entry = problem.name();
    return switch (err) {
        error.OutputExists => ctx.failKey("pak.err.exists", .{entry}),
        error.OutputInsideSource => ctx.failKey("pak.err.inside", .{path}),
        error.SourceNotDirectory => ctx.failKey("pak.err.not_directory", .{entry}),
        error.FileNotFound => ctx.failKey("pak.err.not_found", .{if (entry.len > 0) entry else path}),
        error.TooManyEntries => ctx.failKey("pak.err.too_many", .{}),
        error.ArchiveTooLarge => ctx.failKey("pak.err.too_large", .{}),
        error.Zip64Unsupported => ctx.failKey("pak.err.zip64", .{path}),
        error.UnsafeEntryName => ctx.failKey("pak.err.unsafe_name", .{ path, entry }),
        error.InvalidEntryName => ctx.failKey("pak.err.invalid_name", .{ path, entry }),
        error.DuplicateEntry => ctx.failKey("pak.err.duplicate", .{ path, entry }),
        error.ChecksumMismatch => ctx.failKey("pak.err.crc", .{ path, entry }),
        error.SizeMismatch, error.ZipDecompressTruncated, error.ZipMismatchCompLen, error.ZipMismatchUncompLen, error.ZipBadDirectorySize => ctx.failKey("pak.err.size", .{ path, entry }),
        error.UnsupportedCompressionMethod => ctx.failKey("pak.err.method", .{ path, entry }),
        error.ZipEncryptionUnsupported => ctx.failKey("pak.err.encrypted", .{path}),
        error.ZipNoEndRecord, error.EndOfStream => ctx.failKey("pak.err.not_zip", .{path}),
        error.PathAlreadyExists => ctx.failKey("pak.err.exists", .{entry}),
        else => if (entry.len > 0)
            ctx.failKey("pak.err.entry_other", .{ path, entry, @errorName(err) })
        else
            ctx.failKey("pak.err.other", .{ path, @errorName(err) }),
    };
}

pub const Totals = struct {
    files: u64 = 0,
    bytes: u64 = 0,
};

pub const Method = enum { stored, deflate };

pub const EntryInfo = struct {
    /// The archive name with `/` separators (backslashes in legacy archives
    /// already turned into `/`); a trailing `/` marks a directory.
    name: []u8,
    size: u64,
    method: Method,

    fn isDirectory(entry: EntryInfo) bool {
        return entry.name[entry.name.len - 1] == '/';
    }
};

pub const EntryList = struct {
    items: []EntryInfo,

    pub fn deinit(list: *EntryList, gpa: std.mem.Allocator) void {
        for (list.items) |entry| gpa.free(entry.name);
        gpa.free(list.items);
    }

    pub fn totals(list: EntryList) Totals {
        var result: Totals = .{};
        for (list.items) |entry| {
            if (entry.isDirectory()) continue;
            result.files += 1;
            result.bytes += entry.size;
        }
        return result;
    }
};

/// Write `source_dir` as a stored archive at `output`. The writer is the
/// release packager's, unchanged; what is added here is the modder-facing
/// safety: an existing output is kept unless `force`, and an output inside the
/// source tree is refused (the walk would pick up the half-written archive).
/// The result is read back through `scan`, so the summary counts what was
/// actually written.
pub fn pack(io: std.Io, gpa: std.mem.Allocator, source_dir: []const u8, output: []const u8, force: bool, problem: *Problem) !Totals {
    const cwd = std.Io.Dir.cwd();
    problem.set(source_dir);
    const source_stat = try cwd.statFile(io, source_dir, .{});
    if (source_stat.kind != .directory) return error.SourceNotDirectory;

    problem.set(output);
    if (!force) {
        if (cwd.access(io, output, .{})) |_| return error.OutputExists else |err| switch (err) {
            error.FileNotFound => {},
            else => return err,
        }
    }
    if (try isInside(io, gpa, output, source_dir)) return error.OutputInsideSource;

    problem.* = .{};
    try stored_zip.createPackage(io, gpa, source_dir, output);
    var entries = try scan(io, gpa, output, .check_data, problem);
    defer entries.deinit(gpa);
    return entries.totals();
}

/// Whether `path` (which need not exist yet) would land under `dir`. Both are
/// compared as real paths, so `x/../x` and symlinked parents are seen through:
/// the deepest existing ancestor of `path` is resolved and the part below it,
/// which `createPackage` will create, is appended as written.
fn isInside(io: std.Io, gpa: std.mem.Allocator, path: []const u8, dir: []const u8) !bool {
    const cwd = std.Io.Dir.cwd();
    const dir_real = try cwd.realPathFileAlloc(io, dir, gpa);
    defer gpa.free(dir_real);
    const cwd_real = try cwd.realPathFileAlloc(io, ".", gpa);
    defer gpa.free(cwd_real);
    const absolute = try std.fs.path.resolveAlloc(gpa, &.{ cwd_real, path });
    defer gpa.free(absolute);

    var ancestor: []const u8 = absolute;
    const resolved = while (std.fs.path.dirname(ancestor)) |parent| {
        ancestor = parent;
        const parent_real = cwd.realPathFileAlloc(io, parent, gpa) catch continue;
        defer gpa.free(parent_real);
        break try std.mem.concat(gpa, u8, &.{ parent_real, absolute[parent.len..] });
    } else try gpa.dupe(u8, absolute);
    defer gpa.free(resolved);

    return resolved.len > dir_real.len and
        std.mem.startsWith(u8, resolved, dir_real) and
        std.fs.path.isSep(resolved[dir_real.len]);
}

/// Extract `archive_path` into `dest_dir` (created if missing). The whole
/// archive is scanned first, data included, and every target is checked for
/// an existing file, so an unsafe, damaged or conflicting archive is refused
/// before anything is written. Each entry's CRC is checked again as it is
/// written, in case the archive changed in between.
pub fn unpack(io: std.Io, gpa: std.mem.Allocator, archive_path: []const u8, dest_dir: []const u8, force: bool, problem: *Problem) !Totals {
    var entries = try scan(io, gpa, archive_path, .check_data, problem);
    defer entries.deinit(gpa);

    const cwd = std.Io.Dir.cwd();
    if (!force) existing: {
        var dest = cwd.openDir(io, dest_dir, .{}) catch |err| switch (err) {
            error.FileNotFound => break :existing,
            else => return err,
        };
        defer dest.close(io);
        for (entries.items) |entry| {
            if (entry.isDirectory()) continue;
            if (dest.access(io, entry.name, .{})) |_| {
                const target = try std.fs.path.join(gpa, &.{ dest_dir, entry.name });
                defer gpa.free(target);
                bk_tool.toHostSeparators(target);
                problem.set(target);
                return error.OutputExists;
            } else |err| switch (err) {
                error.FileNotFound => {},
                else => return err,
            }
        }
    }

    try cwd.createDirPath(io, dest_dir);
    var dest = try cwd.openDir(io, dest_dir, .{});
    defer dest.close(io);

    var file = try cwd.openFile(io, archive_path, .{});
    defer file.close(io);
    var read_buffer: [64 * 1024]u8 = undefined;
    var reader = file.reader(io, &read_buffer);
    var iterator = try std.zip.Iterator.init(&reader);
    var name_buffer: [std.fs.max_path_bytes]u8 = undefined;
    while (try iterator.next()) |entry| {
        const name = try entry.getFilename(&reader, &name_buffer, .{ .allow_backslashes = true });
        problem.set(name);
        try checkEntryName(name);
        if (name[name.len - 1] == '/') {
            try dest.createDirPath(io, name[0 .. name.len - 1]);
            continue;
        }
        if (std.fs.path.dirname(name)) |parent| try dest.createDirPath(io, parent);

        var out_file = try dest.createFile(io, name, .{ .truncate = true, .exclusive = !force });
        defer out_file.close(io);
        var write_buffer: [64 * 1024]u8 = undefined;
        var file_writer = out_file.writer(io, &write_buffer);
        var hash_buffer: [4096]u8 = undefined;
        var hashed = std.Io.Writer.Hashed(std.hash.Crc32).initHasher(&file_writer.interface, .init(), &hash_buffer);
        entry.extractTo(&reader, &hashed.writer) catch |err| switch (err) {
            error.WriteFailed => return file_writer.err orelse err,
            else => return err,
        };
        hashed.writer.flush() catch return file_writer.err orelse error.WriteFailed;
        file_writer.end() catch |err| return file_writer.err orelse err;
        if (hashed.hasher.final() != entry.crc32) return error.ChecksumMismatch;
    }
    problem.* = .{};
    return entries.totals();
}

const ScanMode = enum { names_only, check_data };

/// Read the central directory of `archive_path`, refusing anything unsafe:
/// Zip64, names that escape the output directory or are not UTF-8, duplicate
/// names and, with `.check_data`, entries whose data does not decompress to
/// the recorded size and CRC-32. On error `problem` names the entry.
pub fn scan(io: std.Io, gpa: std.mem.Allocator, archive_path: []const u8, mode: ScanMode, problem: *Problem) !EntryList {
    var file = try std.Io.Dir.cwd().openFile(io, archive_path, .{});
    defer file.close(io);
    var read_buffer: [64 * 1024]u8 = undefined;
    var reader = file.reader(io, &read_buffer);

    // std.zip reads Zip64 directories; this tool keeps the writer's limit so
    // what `list` accepts is what `pack` could have written.
    const end_record = try std.zip.EndRecord.findFile(&reader);
    if (end_record.need_zip64()) return error.Zip64Unsupported;

    var entries = std.ArrayList(EntryInfo).empty;
    errdefer {
        for (entries.items) |entry| gpa.free(entry.name);
        entries.deinit(gpa);
    }

    var iterator = try std.zip.Iterator.init(&reader);
    if (iterator.cd_record_count > stored_zip.max_entries) return error.Zip64Unsupported;
    var name_buffer: [std.fs.max_path_bytes]u8 = undefined;
    while (true) {
        problem.* = .{};
        const entry = (try iterator.next()) orelse break;
        if (entry.filename_len > name_buffer.len) return error.NameTooLong;
        // Read the raw name first so an error can report it.
        try reader.seekTo(entry.header_zip_offset + @sizeOf(std.zip.CentralDirectoryFileHeader));
        try reader.interface.readSliceAll(name_buffer[0..entry.filename_len]);
        problem.set(name_buffer[0..entry.filename_len]);

        const name = entry.getFilename(&reader, &name_buffer, .{ .allow_backslashes = true }) catch |err| switch (err) {
            error.ZipBadFilename => return error.UnsafeEntryName,
            else => return err,
        };
        try checkEntryName(name);
        if (entry.compression_method == .store and entry.compressed_size != entry.uncompressed_size)
            return error.SizeMismatch;

        if (mode == .check_data) {
            // `extractTo` streams exactly the recorded size or fails with
            // `ZipDecompressTruncated`, and compares the local header's sizes
            // with the directory's; the CRC is left to the caller.
            var hash_buffer: [64 * 1024]u8 = undefined;
            var hashing = std.Io.Writer.Hashing(std.hash.Crc32).initHasher(.init(), &hash_buffer);
            try entry.extractTo(&reader, &hashing.writer);
            try hashing.writer.flush();
            if (hashing.hasher.final() != entry.crc32) return error.ChecksumMismatch;
        }

        const owned = try gpa.dupe(u8, name);
        entries.append(gpa, .{
            .name = owned,
            .size = entry.uncompressed_size,
            .method = if (entry.compression_method == .store) .stored else .deflate,
        }) catch |err| {
            gpa.free(owned);
            return err;
        };
    }

    // Two entries with one name would make `unpack` depend on order.
    const sorted = try gpa.dupe(EntryInfo, entries.items);
    defer gpa.free(sorted);
    std.mem.sort(EntryInfo, sorted, {}, struct {
        fn lessThan(_: void, lhs: EntryInfo, rhs: EntryInfo) bool {
            return std.mem.order(u8, lhs.name, rhs.name) == .lt;
        }
    }.lessThan);
    for (sorted[0..sorted.len -| 1], 1..) |entry, i| {
        if (std.mem.eql(u8, entry.name, sorted[i].name)) {
            problem.set(entry.name);
            return error.DuplicateEntry;
        }
    }

    problem.* = .{};
    return .{ .items = try entries.toOwnedSlice(gpa) };
}

/// What `std.zip` already refuses (empty names, a leading `/`, `..`) plus what
/// it does not: drive letters, `:` anywhere (a Windows alternate data stream
/// or drive-relative path), NUL and names that are not UTF-8.
fn checkEntryName(name: []const u8) !void {
    if (name.len == 0 or name[0] == '/') return error.UnsafeEntryName;
    if (std.mem.indexOfScalar(u8, name, ':') != null) return error.UnsafeEntryName;
    var parts = std.mem.splitScalar(u8, name, '/');
    while (parts.next()) |part| {
        if (std.mem.eql(u8, part, "..")) return error.UnsafeEntryName;
    }
    if (std.mem.indexOfScalar(u8, name, 0) != null) return error.InvalidEntryName;
    if (!std.unicode.utf8ValidateSlice(name)) return error.InvalidEntryName;
}
