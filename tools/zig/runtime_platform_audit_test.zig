const std = @import("std");
const audit = @import("runtime_platform_audit.zig");

const Fixture = struct {
    token: []const u8,
    file: []const u8,
    line: usize,
};

const required_fixtures = [_]Fixture{
    .{ .token = "windows.h", .file = "tools/zig/fixtures/runtime_platform/windows_header.cpp", .line = 1 },
    .{ .token = "dinput.h", .file = "tools/zig/fixtures/runtime_platform/direct_input.h", .line = 1 },
    .{ .token = "winsock2.h", .file = "tools/zig/fixtures/runtime_platform/socket.cpp", .line = 1 },
    .{ .token = "HANDLE", .file = "tools/zig/fixtures/runtime_platform/handle.cpp", .line = 1 },
    .{ .token = "SOCKET", .file = "tools/zig/fixtures/runtime_platform/socket_type.cpp", .line = 1 },
    .{ .token = "GetTickCount", .file = "tools/zig/fixtures/runtime_platform/clock.cpp", .line = 1 },
    .{ .token = "HeapAlloc", .file = "tools/zig/fixtures/runtime_platform/heap.cpp", .line = 1 },
    .{ .token = "OutputDebugString", .file = "tools/zig/fixtures/runtime_platform/debug.cpp", .line = 1 },
    .{ .token = "wrong-case relative include", .file = "tools/zig/fixtures/runtime_platform/case_relative.cpp", .line = 1 },
};

test "required platform fixtures report token, file, and line" {
    const cwd = std.Io.Dir.cwd();
    for (required_fixtures) |fixture| {
        const fixture_text = try cwd.readFileAlloc(std.testing.io, fixture.file, std.testing.allocator, .limited(1024 * 1024));
        defer std.testing.allocator.free(fixture_text);
        const hits = try audit.scanTextWithCase(std.testing.allocator, fixture_text, fixture.file, cwd, std.testing.io);
        defer std.testing.allocator.free(hits);
        var found = false;
        for (hits) |hit| {
            std.debug.print("fixture output: token={s} file={s} line={d}\n", .{ hit.token, hit.file, hit.line });
            if (std.mem.eql(u8, hit.token, fixture.token) and std.mem.eql(u8, hit.file, fixture.file) and hit.line == fixture.line) found = true;
        }
        try std.testing.expect(found);
    }
}

test "platform tokens require exact identifier boundaries" {
    const hits = try audit.scanText(std.testing.allocator,
        "GetTickCount64(); HANDLE_value = 0; GetTickCount(); HANDLE value;", "fixture/boundaries.cpp");
    defer std.testing.allocator.free(hits);
    var get_tick_count: usize = 0;
    var handle: usize = 0;
    for (hits) |hit| {
        if (std.mem.eql(u8, hit.token, "GetTickCount")) get_tick_count += 1;
        if (std.mem.eql(u8, hit.token, "HANDLE")) handle += 1;
    }
    try std.testing.expectEqual(@as(usize, 1), get_tick_count);
    try std.testing.expectEqual(@as(usize, 1), handle);
}

test "playable source parser excludes declared non-playable arrays" {
    const text =
        "const runtime_platform_playable_source_arrays = &.{ \"input_sources\" };\n" ++
        "const runtime_platform_non_playable_source_arrays = &.{ \"editor_sources\" };\n" ++
        "const editor_sources = &.{ \"editor.cpp\" };\n" ++
        "const input_sources = &.{ \"input.cpp\" };\n";
    const sources = try audit.parsePlayableSources(std.testing.allocator, text);
    defer std.testing.allocator.free(sources);
    defer for (sources) |source| std.testing.allocator.free(source);
    try std.testing.expectEqual(@as(usize, 1), sources.len);
    try std.testing.expectEqualStrings("input.cpp", sources[0]);
}

test "new source arrays must be classified loudly" {
    const text =
        "const runtime_platform_playable_source_arrays = &.{ \"input_sources\" };\n" ++
        "const runtime_platform_non_playable_source_arrays = &.{ \"editor_sources\" };\n" ++
        "const editor_sources = &.{ \"editor.cpp\" };\n" ++
        "const input_sources = &.{ \"input.cpp\" };\n" ++
        "const newly_added_sources = &.{ \"new.cpp\" };\n";
    try std.testing.expectError(error.UnclassifiedSourceArray, audit.parsePlayableSources(std.testing.allocator, text));
}

test "build library rules classify dxguid exactly once as graphics" {
    const build_text =
        "module.linkSystemLibrary(\"dinput8\", .{});\n" ++
        "module.linkSystemLibrary(\"dxguid\", .{});\n" ++
        "module.linkSystemLibrary(\"ws2_32\", .{});\n";
    const hits = try audit.scanBuildLibraries(std.testing.allocator, "fn addPlayable() void {\n" ++ build_text ++ "}\n", "build.zig", &.{ "addPlayable" });
    defer std.testing.allocator.free(hits);
    try std.testing.expectEqual(@as(usize, 3), hits.len);
    try std.testing.expectEqualStrings("input", hits[0].service);
    try std.testing.expectEqualStrings("graphics", hits[1].service);
    try std.testing.expectEqualStrings("net", hits[2].service);
}

test "build library audit excludes non-playable module links" {
    const build_text =
        "fn addDeveloperOnly() void {\n" ++
        "    editor_module.linkSystemLibrary(\"d3d9\", .{});\n" ++
        "}\n" ++
        "fn addPlayable() void {\n" ++
        "    game_module.linkSystemLibrary(\"d3d9\", .{});\n" ++
        "}\n";
    const hits = try audit.scanBuildLibraries(std.testing.allocator, build_text, "build.zig", &.{ "addPlayable" });
    defer std.testing.allocator.free(hits);
    try std.testing.expectEqual(@as(usize, 1), hits.len);
    try std.testing.expectEqualStrings("d3d9", hits[0].token);
    try std.testing.expectEqual(@as(usize, 5), hits[0].line);
}

test "build library scanner excludes developer-only target functions" {
    const build_text =
        "fn addPlayable() void {\n" ++
        "    module.linkSystemLibrary(\"ws2_32\", .{});\n" ++
        "}\n" ++
        "fn addDeveloperOnly() void {\n" ++
        "    module.linkSystemLibrary(\"dinput8\", .{});\n" ++
        "}\n";
    const hits = try audit.scanBuildLibraries(std.testing.allocator, build_text, "build.zig", &.{ "addPlayable" });
    defer std.testing.allocator.free(hits);
    try std.testing.expectEqual(@as(usize, 1), hits.len);
    try std.testing.expectEqualStrings("ws2_32", hits[0].token);
    try std.testing.expectEqual(@as(usize, 2), hits[0].line);
}

test "token scanner matches exact identifiers only" {
    const hits = try audit.scanText(std.testing.allocator, "GetTickCount64(); HANDLE_value = 0; GetTickCount(); HANDLE value;", "fixture.cpp");
    defer std.testing.allocator.free(hits);
    try std.testing.expectEqual(@as(usize, 2), hits.len);
    var saw_get_tick_count = false;
    var saw_handle = false;
    for (hits) |hit| {
        saw_get_tick_count = saw_get_tick_count or std.mem.eql(u8, hit.token, "GetTickCount");
        saw_handle = saw_handle or std.mem.eql(u8, hit.token, "HANDLE");
    }
    try std.testing.expect(saw_get_tick_count);
    try std.testing.expect(saw_handle);
}

test "playable source platform inventory matches build manifest and allowlist" {
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
        const source_text = try cwd.readFileAlloc(std.testing.io, source, allocator, .limited(20 * 1024 * 1024));
        defer allocator.free(source_text);
        const source_hits = try audit.scanTextWithCase(allocator, source_text, source, cwd, std.testing.io);
        defer allocator.free(source_hits);
        for (source_hits) |hit| try hits.append(allocator, try audit.hitKey(allocator, hit));
    }
    const library_hits = try audit.scanBuildLibraries(allocator, build_text, "build.zig", &.{
        "addLegacyProjectDll", "addGame", "addNet", "addInput", "addSFX", "addGFX", "addGFXGPU",
    });
    defer allocator.free(library_hits);
    for (library_hits) |hit| try hits.append(allocator, try audit.hitKey(allocator, hit));

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
    for (hits.items) |key| {
        std.debug.print("platform inventory: {s}\n", .{key});
        if (!allowed.contains(key)) {
            unknown += 1;
            std.debug.print("unknown platform hit: {s}\n", .{key});
        }
    }
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
    std.debug.print("platform inventory count: {d}\n", .{hits.items.len});
    std.debug.print("platform allowlist ownership count: {d}\n", .{allowed.count()});
    try std.testing.expectEqual(@as(usize, 0), unknown);
}
