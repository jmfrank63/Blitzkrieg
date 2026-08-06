const std = @import("std");
const audit = @import("runtime_platform_audit.zig");

const Fixture = struct {
    token: []const u8,
    file: []const u8,
    line: usize,
};

const required_fixtures = [_]Fixture{
    .{ .token = "windows.h", .file = "fixture/windows_header.cpp", .line = 1 },
    .{ .token = "dinput.h", .file = "fixture/direct_input.h", .line = 1 },
    .{ .token = "winsock2.h", .file = "fixture/socket.cpp", .line = 1 },
    .{ .token = "HANDLE", .file = "fixture/handle.cpp", .line = 1 },
    .{ .token = "SOCKET", .file = "fixture/socket_type.cpp", .line = 1 },
    .{ .token = "GetTickCount", .file = "fixture/clock.cpp", .line = 1 },
    .{ .token = "HeapAlloc", .file = "fixture/heap.cpp", .line = 1 },
    .{ .token = "OutputDebugString", .file = "fixture/debug.cpp", .line = 1 },
    .{ .token = "wrong-case relative include", .file = "fixture/case.cpp", .line = 1 },
};

test "required platform fixtures fail with token, file, and line" {
    const fixture_texts = [_][]const u8{
        "#include <windows.h>", "#include <dinput.h>", "#include <winsock2.h>",
        "HANDLE value;", "SOCKET value;", "GetTickCount();", "HeapAlloc();",
        "OutputDebugString(\"fixture\");", "#include \"WrongCase/file.h\"",
    };
    for (required_fixtures) |fixture| {
        const index = for (required_fixtures, 0..) |candidate, i| {
            if (std.mem.eql(u8, candidate.token, fixture.token)) break i;
        } else unreachable;
        const hits = try audit.scanText(std.testing.allocator, fixture_texts[index], fixture.file);
        defer std.testing.allocator.free(hits);
        var found = false;
        for (hits) |hit| {
            if (std.mem.eql(u8, hit.token, fixture.token) and std.mem.eql(u8, hit.file, fixture.file) and hit.line == fixture.line) found = true;
        }
        try std.testing.expect(found);
        std.debug.print("fixture failure: token={s} file={s} line={d}\n", .{ fixture.token, fixture.file, fixture.line });
    }
}

test "playable source parser excludes non-playable arrays" {
    const text = "const editor_sources = &.{ \"editor.cpp\" };\nconst input_sources = &.{ \"input.cpp\" };";
    const sources = try audit.parsePlayableSources(std.testing.allocator, text);
    defer std.testing.allocator.free(sources);
    defer for (sources) |source| std.testing.allocator.free(source);
    try std.testing.expectEqual(@as(usize, 1), sources.len);
    try std.testing.expectEqualStrings("input.cpp", sources[0]);
}

test "playable source platform inventory matches narrow allowlist" {
    const allocator = std.testing.allocator;
    const cwd = std.Io.Dir.cwd();
    const build_text = try cwd.readFileAlloc(std.testing.io, "build.zig", allocator, .limited(20 * 1024 * 1024));
    defer allocator.free(build_text);
    const source_paths = try audit.parsePlayableSources(allocator, build_text);
    defer allocator.free(source_paths);
    defer for (source_paths) |source| allocator.free(source);

    var hits = std.ArrayList([]const u8).empty;
    defer {
        for (hits.items) |key| allocator.free(key);
        hits.deinit(allocator);
    }
    for (source_paths) |source| {
        const text = try cwd.readFileAlloc(std.testing.io, source, allocator, .limited(20 * 1024 * 1024));
        defer allocator.free(text);
        const source_hits = try audit.scanText(allocator, text, source);
        defer allocator.free(source_hits);
        for (source_hits) |hit| try hits.append(allocator, try audit.hitKey(allocator, hit));
    }
    std.sort.heap([]const u8, hits.items, {}, struct {
        fn lessThan(_: void, lhs: []const u8, rhs: []const u8) bool { return std.mem.lessThan(u8, lhs, rhs); }
    }.lessThan);

    const allowlist = try cwd.readFileAlloc(std.testing.io, "tools/zig/runtime_platform_allowlist.txt", allocator, .limited(20 * 1024 * 1024));
    defer allocator.free(allowlist);
    var allowed = std.StringHashMap(void).init(allocator);
    defer allowed.deinit();
    var lines = std.mem.splitScalar(u8, allowlist, '\n');
    while (lines.next()) |line| {
        if (line.len == 0 or line[0] == '#') continue;
        try allowed.put(line, {});
    }
    var unknown: usize = 0;
    for (hits.items) |key| if (!allowed.contains(key)) {
        unknown += 1;
        std.debug.print("unknown platform hit: {s}\n", .{key});
    };
    var allowed_iter = allowed.iterator();
    while (allowed_iter.next()) |entry| {
        const key = entry.key_ptr.*;
        var present = false;
        for (hits.items) |hit| {
            if (std.mem.eql(u8, hit, key)) present = true;
        }
        if (!present) {
            unknown += 1;
            std.debug.print("stale platform allowlist entry: {s}\n", .{key});
        }
    }
    try std.testing.expectEqual(@as(usize, 0), unknown);
    std.debug.print("playable platform hits: {d}, allowlist ownership count: {d}\n", .{ hits.items.len, allowed.count() });
}
