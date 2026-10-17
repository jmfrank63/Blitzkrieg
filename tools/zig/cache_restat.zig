//! Lets a Zig cache made on one machine hit on another.
//!
//! A cache manifest records every input file's size, inode and modification
//! time beside the hash of its contents. Zig 0.17 reuses a C or C++ object
//! only when the stat of its own source matches exactly: a fresh checkout or
//! an unpacked cache has new inodes and times, so every object compiles again
//! although no byte changed (measured: touching AILogic/AAFeedBacks.cpp alone
//! recompiles its object; touching a header it includes does not, Zig hashes
//! the header again). That is why a restored CI cache never saved the engine
//! build, 23 to 45 minutes per job.
//!
//! This tool walks the manifests of a local cache and, for every entry whose
//! file on disk hashes to the digest the manifest holds, writes the file's
//! current size, inode and time into the entry. A file whose contents differ
//! keeps its old stat, so Zig still sees the change and rebuilds what depends
//! on it; nothing is ever marked as a hit that is not one. A manifest that
//! does not parse to its exact end is left alone.
//!
//! usage: cache_restat [<local cache> <zig lib dir> <global cache> <build root>]
//! Run from the directory zig build runs in (the repository root), before it.
//! Without arguments: .zig-cache, the lib and global cache directories that
//! `zig env` names, and the current directory - what CI runs.
const std = @import("std");
const Cache = std.Build.Cache;
const File = Cache.Manifest.File;

const Known = struct {
    digest: Cache.BinDigest,
    size: u64,
    inode: std.Io.File.INode,
    mtime: i64,
};

const header_len = @offsetOf(File, "path_start");

pub fn main(init: std.process.Init) !void {
    var arena_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const gpa = std.heap.page_allocator;
    const io = init.io;

    var iterator = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    defer iterator.deinit();
    _ = iterator.skip();
    var paths: [4][]const u8 = undefined;
    if (iterator.next()) |first| {
        paths[0] = first;
        for (paths[1..]) |*path| path.* = iterator.next() orelse {
            std.debug.print("usage: cache_restat [<local cache> <zig lib dir> <global cache> <build root>]\n", .{});
            std.process.exit(2);
        };
    } else {
        const env = try std.process.run(arena, io, .{
            .argv = &.{ "zig", "env" },
            .stdout_limit = .limited(1024 * 1024),
            .stderr_limit = .limited(1024 * 1024),
        });
        paths = .{ ".zig-cache", try zonString(arena, env.stdout, ".lib_dir"), try zonString(arena, env.stdout, ".global_cache_dir"), "." };
    }
    var roots: [5]std.Io.Dir = undefined;
    roots[0] = std.Io.Dir.cwd();
    for (roots[1..], paths) |*root, path| {
        root.* = std.Io.Dir.cwd().openDir(io, path, .{}) catch |err| {
            std.debug.print("cache_restat: cannot open {s}: {t}\n", .{ path, err });
            std.process.exit(2);
        };
    }
    // The prefixes in the order the compiler adds them (Compilation.zig):
    // cwd, zig lib dir, local cache, global cache, build root. The arguments
    // come as local cache, zig lib, global cache, build root.
    const local = roots[1];
    const prefixes = [5]std.Io.Dir{ roots[0], roots[2], roots[1], roots[3], roots[4] };

    var known: std.StringHashMapUnmanaged(?Known) = .empty;
    var manifests_dir = try local.openDir(io, "h", .{ .iterate = true });
    defer manifests_dir.close(io);

    var n_manifests: usize = 0;
    var n_rewritten: usize = 0;
    var n_skipped: usize = 0;
    var n_entries: usize = 0;
    var n_updated: usize = 0;
    var n_changed: usize = 0;

    var walker = manifests_dir.iterate();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        n_manifests += 1;
        const bytes = manifests_dir.readFileAlloc(io, entry.name, gpa, .limited(1 << 30)) catch {
            n_skipped += 1;
            continue;
        };
        defer gpa.free(bytes);

        // First pass: the manifest must parse to its exact end, the entries
        // followed by the one terminating zero byte Cache writes after them.
        if (bytes.len == 0 or bytes[bytes.len - 1] != 0) {
            n_skipped += 1;
            continue;
        }
        const entries = bytes[0 .. bytes.len - 1];
        var off: usize = 0;
        var parses = true;
        while (off < entries.len) {
            if (off + header_len >= entries.len) {
                parses = false;
                break;
            }
            const end = std.mem.indexOfScalarPos(u8, entries, off + header_len, 0) orelse {
                parses = false;
                break;
            };
            off = end + 1;
        }
        if (!parses or off != entries.len) {
            n_skipped += 1;
            continue;
        }

        var dirty = false;
        off = 0;
        while (off < entries.len) {
            const end = std.mem.indexOfScalarPos(u8, entries, off + header_len, 0).?;
            const file: *align(1) File = @ptrCast(bytes[off..][0..@sizeOf(File)]);
            const path = bytes[off + header_len .. end];
            off = end + 1;
            n_entries += 1;
            if (file.flags.is_directory or file.flags.metadata_only) continue;
            const prefix: usize = @intCast(@as(u6, @bitCast(file.flags.prefix)));
            if (prefix >= prefixes.len) continue;

            const key = try std.fmt.allocPrint(arena, "{d}:{s}", .{ prefix, path });
            const gop = try known.getOrPut(arena, key);
            if (!gop.found_existing) gop.value_ptr.* = try look(io, gpa, prefixes[prefix], path);
            const now = gop.value_ptr.* orelse continue;
            if (!std.mem.eql(u8, &now.digest, &file.digest)) {
                n_changed += 1;
                continue;
            }
            if (file.size == now.size and file.inode == now.inode and file.mtime == now.mtime) continue;
            file.size = now.size;
            file.inode = now.inode;
            file.mtime = now.mtime;
            dirty = true;
            n_updated += 1;
        }
        if (dirty) {
            try manifests_dir.writeFile(io, .{ .sub_path = entry.name, .data = bytes });
            n_rewritten += 1;
        }
    }
    std.debug.print("cache_restat: {d} manifests ({d} rewritten, {d} left alone), {d} entries, {d} restated, {d} with other contents\n", .{ n_manifests, n_rewritten, n_skipped, n_entries, n_updated, n_changed });
}

/// The string value of `field` in `zig env`'s ZON output, unescaped
/// (Windows paths come with doubled backslashes).
fn zonString(arena: std.mem.Allocator, zon: []const u8, field: []const u8) ![]const u8 {
    const at = std.mem.indexOf(u8, zon, field) orelse return error.ZigEnvField;
    const open = std.mem.indexOfScalarPos(u8, zon, at, '"') orelse return error.ZigEnvField;
    var out: std.ArrayList(u8) = .empty;
    var i = open + 1;
    while (i < zon.len and zon[i] != '"') : (i += 1) {
        if (zon[i] == '\\' and i + 1 < zon.len) i += 1;
        try out.append(arena, zon[i]);
    }
    return out.items;
}

/// The file's digest as Cache hashes it, with its stat; null when it is not
/// there or cannot be read.
fn look(io: std.Io, gpa: std.mem.Allocator, dir: std.Io.Dir, path: []const u8) !?Known {
    const stat = dir.statFile(io, path, .{}) catch return null;
    if (stat.kind != .file) return null;
    const contents = dir.readFileAlloc(io, path, gpa, .limited(1 << 32)) catch return null;
    defer gpa.free(contents);
    var hasher = Cache.hasher_init;
    hasher.update(contents);
    var digest: Cache.BinDigest = undefined;
    hasher.final(&digest);
    return .{
        .digest = digest,
        .size = stat.size,
        .inode = stat.inode,
        .mtime = @intCast(stat.mtime.toNanoseconds()),
    };
}
