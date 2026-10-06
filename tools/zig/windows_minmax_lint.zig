const std = @import("std");

// windows.h defines function-like min/max macros unless NOMINMAX is set, and the ResourceModel and
// EditorBridge sources, and the tools/zig tests that build them, compile for Windows MSVC with it included. A bare std::min( or file_time_type::max()
// then fails there, which Linux and macOS builds never show. The sources spell these calls parenthesized,
// (std::min)(a, b), which no macro can capture; this lint keeps it that way without needing a Windows run.
const roots = [_][]const u8{ "Sources/src/ResourceModel", "Sources/src/EditorBridge", "tools/zig" };

/// tools/zig also holds Linux-only tests, so only the sources the ResourceModel tiers build for MSVC are linted there.
fn inScope(root: []const u8, path: []const u8) bool {
    if (!std.mem.eql(u8, root, "tools/zig")) return true;
    if (std.mem.indexOfScalar(u8, path, '/') != null) return false;
    return std.mem.startsWith(u8, path, "resource_") or std.mem.startsWith(u8, path, "dxt_tolerance");
}

fn isSource(path: []const u8) bool {
    const extensions = [_][]const u8{ ".cpp", ".h", ".hpp", ".inl" };
    for (extensions) |extension| {
        if (std.mem.endsWith(u8, path, extension)) return true;
    }
    return false;
}

/// True when the line holds a min/max call a windows.h macro would capture. Comment-only lines are skipped.
fn lineHasBareMinMax(line: []const u8) bool {
    const trimmed = std.mem.trim(u8, line, " \t\r");
    if (std.mem.startsWith(u8, trimmed, "//") or std.mem.startsWith(u8, trimmed, "*")) return false;
    const names = [_][]const u8{ "min", "max" };
    for (names) |name| {
        var start: usize = 0;
        while (std.mem.indexOfPos(u8, line, start, name)) |position| {
            start = position + name.len;
            if (position >= 2 and std.mem.eql(u8, line[position - 2 .. position], "::")) {
                // X::min( is a macro hit; X::min)( is the safe spelling.
                var after = start;
                while (after < line.len and line[after] == ' ') after += 1;
                if (after < line.len and line[after] == '(') return true;
                continue;
            }
            // A bare min( or max( call, but not an identifier that merely ends in the word, a member, or a pointer access.
            if (position > 0) {
                const before = line[position - 1];
                if (std.ascii.isAlphanumeric(before) or before == '_' or before == '.' or before == '>') continue;
            }
            if (start < line.len and (std.ascii.isAlphanumeric(line[start]) or line[start] == '_')) continue;
            var after = start;
            while (after < line.len and line[after] == ' ') after += 1;
            if (after < line.len and line[after] == '(') return true;
        }
    }
    // std::min and std::max not directly preceded by '(' are caught by the :: rule only for member calls, so check them here.
    const qualified = [_][]const u8{ "std::min", "std::max" };
    for (qualified) |name| {
        var start: usize = 0;
        while (std.mem.indexOfPos(u8, line, start, name)) |position| {
            start = position + name.len;
            if (position == 0 or line[position - 1] != '(') return true;
        }
    }
    return false;
}

fn lintText(text: []const u8) bool {
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |line| {
        if (lineHasBareMinMax(line)) return true;
    }
    return false;
}

fn lintTree(io: std.Io, gpa: std.mem.Allocator) !usize {
    var scanned: usize = 0;
    for (roots) |root| {
        var dir = try std.Io.Dir.cwd().openDir(io, root, .{ .iterate = true });
        defer dir.close(io);
        var walker = try dir.walk(gpa);
        defer walker.deinit();
        while (try walker.next(io)) |entry| {
            if (entry.kind != .file or !isSource(entry.path) or !inScope(root, entry.path)) continue;
            const text = try dir.readFileAlloc(io, entry.path, gpa, .limited(8 * 1024 * 1024));
            defer gpa.free(text);
            scanned += 1;
            if (lintText(text)) {
                std.debug.print("bare min/max in {s}/{s}: write (std::min)(...) so windows.h macros cannot capture it\n", .{ root, entry.path });
                return error.BareMinMax;
            }
        }
    }
    return scanned;
}

pub fn main(init: std.process.Init) !void {
    const scanned = try lintTree(init.io, init.gpa);
    std.debug.print("windows min/max lint passed: {d} files\n", .{scanned});
}

test "flags the spellings windows.h macros capture" {
    try std.testing.expect(lineHasBareMinMax("\tconst int n = std::min( a, b );"));
    try std.testing.expect(lineHasBareMinMax("\tx = std::max<float>( a, b );"));
    try std.testing.expect(lineHasBareMinMax("\tauto t = std::filesystem::file_time_type::min();"));
    try std.testing.expect(lineHasBareMinMax("\tint n = max( a, b );"));
}

test "accepts the parenthesized spellings and unrelated names" {
    try std.testing.expect(!lineHasBareMinMax("\tconst int n = (std::min)( a, b );"));
    try std.testing.expect(!lineHasBareMinMax("\tx = (std::max<float>)( a, b );"));
    try std.testing.expect(!lineHasBareMinMax("\tauto t = (std::filesystem::file_time_type::min)();"));
    try std.testing.expect(!lineHasBareMinMax("\t// std::min( a, b ) is spelled with parentheses"));
    try std.testing.expect(!lineHasBareMinMax("\tint nMax = rect.max( 1 ); int xmax(3);"));
}

test "the ResourceModel, EditorBridge and their tools/zig test sources are clean" {
    _ = try lintTree(std.testing.io, std.testing.allocator);
}
