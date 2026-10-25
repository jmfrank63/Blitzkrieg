//! `zig-allocator-audit <dir>...`: keeps the Zig sources off their own heap
//! allocators. Every Zig host and library in the process allocates through the
//! one BkMemory SafeAllocator (bk_memory_allocator.zig, D083), so a stray
//! DebugAllocator, smp_allocator, page_allocator or c_allocator would split the
//! leak report and let a Zig leak go unseen. Each `*.zig` under the given
//! directories is scanned line by line; a hit that is not in `allowlist` is
//! printed as `file:line` and fails the run.
//!
//! Comment text is skipped, so a doc comment may name a forbidden allocator to
//! explain why it is not used. The allowlist is for code that has a real
//! reason, which is written next to the entry.
//!
//! Run through `b.addRunArtifact`; the directories are relative to the build
//! root, where the step sets the working directory.
const std = @import("std");

const forbidden = [_][]const u8{
    "DebugAllocator",
    "GeneralPurposeAllocator",
    "smp_allocator",
    "page_allocator",
    "c_allocator",
};

/// A hit that is allowed: the file's path ends with `path_suffix` (forward
/// slashes) and the line holds `token`. Empty today; each entry needs a reason.
const AllowEntry = struct { path_suffix: []const u8, token: []const u8, reason: []const u8 };
const allowlist = [_]AllowEntry{};

pub const Hit = struct { path: []const u8, line: usize, token: []const u8 };

/// The code part of a line: everything before a `//` that is outside a string.
fn codePart(line: []const u8) []const u8 {
    var in_string = false;
    var i: usize = 0;
    while (i < line.len) : (i += 1) {
        const ch = line[i];
        if (in_string) {
            if (ch == '\\') i += 1 else if (ch == '"') in_string = false;
        } else if (ch == '"') {
            in_string = true;
        } else if (ch == '/' and i + 1 < line.len and line[i + 1] == '/') {
            return line[0..i];
        }
    }
    return line;
}

fn allowed(allow: []const AllowEntry, path: []const u8, token: []const u8) bool {
    for (allow) |entry| {
        if (std.mem.endsWith(u8, path, entry.path_suffix) and std.mem.eql(u8, token, entry.token)) return true;
    }
    return false;
}

/// Appends one Hit per forbidden token on a code line of `text`, in line order.
/// `path` must outlive the hits.
pub fn scanSource(
    allocator: std.mem.Allocator,
    allow: []const AllowEntry,
    path: []const u8,
    text: []const u8,
    hits: *std.ArrayList(Hit),
) !void {
    var line_no: usize = 0;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |line| {
        line_no += 1;
        const code = codePart(line);
        for (forbidden) |token| {
            if (std.mem.indexOf(u8, code, token) == null) continue;
            if (allowed(allow, path, token)) continue;
            try hits.append(allocator, .{ .path = path, .line = line_no, .token = token });
        }
    }
}

pub fn main(init: std.process.Init) !void {
    const arena = init.arena.allocator();
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    defer args.deinit();
    _ = args.skip();
    var dirs: std.ArrayList([]const u8) = .empty;
    while (args.next()) |arg| try dirs.append(arena, arg);
    if (dirs.items.len == 0) {
        std.debug.print("usage: zig-allocator-audit <dir>...\n", .{});
        std.process.exit(2);
    }

    var hits: std.ArrayList(Hit) = .empty;
    var files: usize = 0;
    for (dirs.items) |dir_path| {
        var dir = std.Io.Dir.cwd().openDir(init.io, dir_path, .{ .iterate = true }) catch |err| {
            // A missing directory must not make the audit pass by scanning nothing.
            std.debug.print("zig-allocator-audit: cannot open {s}: {s}\n", .{ dir_path, @errorName(err) });
            std.process.exit(2);
        };
        defer dir.close(init.io);
        var walker = try dir.walk(arena);
        defer walker.deinit();
        while (try walker.next(init.io)) |entry| {
            if (entry.kind != .file or !std.mem.endsWith(u8, entry.path, ".zig")) continue;
            const text = try dir.readFileAlloc(init.io, entry.path, arena, .limited(16 * 1024 * 1024));
            const full = try std.fmt.allocPrint(arena, "{s}/{s}", .{ dir_path, entry.path });
            std.mem.replaceScalar(u8, full, '\\', '/');
            try scanSource(arena, &allowlist, full, text, &hits);
            files += 1;
        }
    }

    for (hits.items) |hit| {
        std.debug.print("{s}:{d}: forbidden allocator `{s}`; use bk_memory_allocator\n", .{ hit.path, hit.line, hit.token });
    }
    if (hits.items.len != 0) {
        std.debug.print("zig-allocator-audit: FAIL, {d} hit(s) in {d} file(s)\n", .{ hits.items.len, files });
        std.process.exit(1);
    }
}

test "a planted forbidden line is reported with its file and line" {
    const allocator = std.testing.allocator;
    var hits: std.ArrayList(Hit) = .empty;
    defer hits.deinit(allocator);
    const text =
        "const std = @import(\"std\");\n" ++
        "pub fn main() void {\n" ++
        "    const gpa = std.heap.smp_allocator;\n" ++
        "    _ = gpa;\n" ++
        "}\n";
    try scanSource(allocator, &.{}, "Sources/editor/x.zig", text, &hits);
    try std.testing.expectEqual(@as(usize, 1), hits.items.len);
    try std.testing.expectEqualStrings("Sources/editor/x.zig", hits.items[0].path);
    try std.testing.expectEqual(@as(usize, 3), hits.items[0].line);
    try std.testing.expectEqualStrings("smp_allocator", hits.items[0].token);
}

test "comments and strings do not hide or invent hits" {
    const allocator = std.testing.allocator;
    var hits: std.ArrayList(Hit) = .empty;
    defer hits.deinit(allocator);
    // Comment-only and trailing comments are skipped; a token in a string is code.
    const text =
        "/// not the page_allocator: the shared one\n" ++
        "const a = 1; // c_allocator would split the report\n" ++
        "const b = \"// DebugAllocator\"; \n" ++
        "var d: std.heap.DebugAllocator(.{}) = .init;\n";
    try scanSource(allocator, &.{}, "a.zig", text, &hits);
    try std.testing.expectEqual(@as(usize, 2), hits.items.len);
    try std.testing.expectEqual(@as(usize, 3), hits.items[0].line);
    try std.testing.expectEqual(@as(usize, 4), hits.items[1].line);
}

test "an allowlisted file and token is skipped, others are not" {
    const allocator = std.testing.allocator;
    var hits: std.ArrayList(Hit) = .empty;
    defer hits.deinit(allocator);
    const allow = [_]AllowEntry{.{ .path_suffix = "fixture.zig", .token = "page_allocator", .reason = "test" }};
    const text = "const a = std.heap.page_allocator;\nconst b = std.heap.c_allocator;\n";
    try scanSource(allocator, &allow, "x/fixture.zig", text, &hits);
    try std.testing.expectEqual(@as(usize, 1), hits.items.len);
    try std.testing.expectEqualStrings("c_allocator", hits.items[0].token);
}
