const std = @import("std");

const Entry = struct { path: []u8 };

fn lessThan(left: Entry, right: Entry) bool {
    return std.mem.order(u8, left.path, right.path) == .lt;
}

fn sortEntries(entries: []Entry) void {
    var index: usize = 1;
    while (index < entries.len) : (index += 1) {
        const value = entries[index];
        var position = index;
        while (position > 0 and lessThan(value, entries[position - 1])) : (position -= 1)
            entries[position] = entries[position - 1];
        entries[position] = value;
    }
}

fn collectFiles(init: std.process.Init, allocator: std.mem.Allocator, root_path: []const u8) ![]Entry {
    const cwd = std.Io.Dir.cwd();
    var root = try cwd.openDir(init.io, root_path, .{ .iterate = true });
    defer root.close(init.io);
    var walker = try root.walk(allocator);
    defer walker.deinit();

    var entries = std.ArrayList(Entry).empty;
    errdefer {
        for (entries.items) |entry| allocator.free(entry.path);
        entries.deinit(allocator);
    }
    while (try walker.next(init.io)) |entry| {
        if (entry.kind != .file) continue;
        const path = try allocator.dupe(u8, entry.path);
        std.mem.replaceScalar(u8, path, '\\', '/');
        try entries.append(allocator, .{ .path = path });
    }
    sortEntries(entries.items);
    return entries.toOwnedSlice(allocator);
}

fn freeEntries(allocator: std.mem.Allocator, entries: []Entry) void {
    for (entries) |entry| allocator.free(entry.path);
    allocator.free(entries);
}

fn readFile(init: std.process.Init, allocator: std.mem.Allocator, root_path: []const u8, path: []const u8) ![]u8 {
    const cwd = std.Io.Dir.cwd();
    var root = try cwd.openDir(init.io, root_path, .{});
    defer root.close(init.io);
    return root.readFileAlloc(init.io, path, allocator, .limited(128 * 1024 * 1024));
}

pub fn compare(init: std.process.Init, allocator: std.mem.Allocator, left_path: []const u8, right_path: []const u8) !void {
    const left = try collectFiles(init, allocator, left_path);
    defer freeEntries(allocator, left);
    const right = try collectFiles(init, allocator, right_path);
    defer freeEntries(allocator, right);
    if (left.len != right.len) return error.TreeMismatch;

    for (left, right) |left_entry, right_entry| {
        if (!std.mem.eql(u8, left_entry.path, right_entry.path)) return error.TreeMismatch;
        const left_bytes = try readFile(init, allocator, left_path, left_entry.path);
        defer allocator.free(left_bytes);
        const right_bytes = try readFile(init, allocator, right_path, right_entry.path);
        defer allocator.free(right_bytes);
        var left_hash: [32]u8 = undefined;
        var right_hash: [32]u8 = undefined;
        std.crypto.hash.sha2.Sha256.hash(left_bytes, &left_hash, .{});
        std.crypto.hash.sha2.Sha256.hash(right_bytes, &right_hash, .{});
        if (!std.mem.eql(u8, &left_hash, &right_hash) or !std.mem.eql(u8, left_bytes, right_bytes)) return error.TreeMismatch;
    }
    std.debug.print("shader outputs are deterministic: {d} files\n", .{left.len});
}

pub fn main(init: std.process.Init) !void {
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.skip();
    const left = args.next() orelse return error.MissingLeftTree;
    const right = args.next() orelse return error.MissingRightTree;
    try compare(init, init.gpa, left, right);
}

test "orders tree entries by normalized relative path" {
    var entries = [_]Entry{ .{ .path = @constCast("b") }, .{ .path = @constCast("a") } };
    sortEntries(&entries);
    try std.testing.expectEqualStrings("a", entries[0].path);
    try std.testing.expectEqualStrings("b", entries[1].path);
}
