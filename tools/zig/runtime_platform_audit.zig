const std = @import("std");
const fs = std.fs;

pub const Hit = struct {
    service: []const u8,
    packet: []const u8,
    token: []const u8,
    file: []const u8,
    line: usize,
};

const Rule = struct { token: []const u8, service: []const u8, packet: []const u8 };
const rules = [_]Rule{
    .{ .token = "windows.h", .service = "core", .packet = "P01" },
    .{ .token = "dinput.h", .service = "input", .packet = "P03" },
    .{ .token = "winsock2.h", .service = "net", .packet = "P04" },
    .{ .token = "HANDLE", .service = "core", .packet = "P01" },
    .{ .token = "SOCKET", .service = "net", .packet = "P04" },
    .{ .token = "GetTickCount", .service = "core", .packet = "P01" },
    .{ .token = "HeapAlloc", .service = "core", .packet = "P01" },
    .{ .token = "OutputDebugString", .service = "core", .packet = "P01" },
};

// These are the only places where the playable build is allowed to retain
// native implementation details.  The list is deliberately path-specific:
// adding a new native call to an ordinary gameplay source file must remain a
// failing audit result.
pub const approved_native_adapter_paths = [_][]const u8{
    "Sources/src/Platform/",
    "Sources/src/libpng/",
    "Sources/src/Input/",
    "Sources/src/GFX/VideoCheck.cpp",
    "Sources/src/Game/WindowsMain.cpp",
};

pub fn isApprovedNativeAdapterPath(file: []const u8) bool {
    for (approved_native_adapter_paths) |path| {
        if (std.mem.startsWith(u8, file, path)) return true;
    }
    return false;
}

fn wordAt(line: []const u8, start: usize, token: []const u8) bool {
    if (start > 0 and (std.ascii.isAlphanumeric(line[start - 1]) or line[start - 1] == '_')) return false;
    const end = start + token.len;
    if (end < line.len and (std.ascii.isAlphanumeric(line[end]) or line[end] == '_')) return false;
    return true;
}

fn appendHit(allocator: std.mem.Allocator, hits: *std.ArrayList(Hit), rule: Rule, file: []const u8, line: usize) !void {
    try hits.append(allocator, .{ .service = rule.service, .packet = rule.packet, .token = rule.token, .file = file, .line = line });
}

pub fn scanText(allocator: std.mem.Allocator, text: []const u8, file: []const u8) ![]Hit {
    var hits = std.ArrayList(Hit).empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    var line_no: usize = 0;
    while (lines.next()) |line| {
        line_no += 1;
        for (rules) |rule| {
            var offset: usize = 0;
            while (std.mem.indexOfPos(u8, line, offset, rule.token)) |found| {
                const boundary = if (rule.token.len == 1) true else wordAt(line, found, rule.token);
                if (boundary) try appendHit(allocator, &hits, rule, file, line_no);
                offset = found + rule.token.len;
            }
        }
        if (std.mem.indexOf(u8, line, "#pragma comment(lib") != null) {
            const open = std.mem.lastIndexOfScalar(u8, line, '"') orelse continue;
            if (open > 0) {
                const first_quote = std.mem.lastIndexOfScalar(u8, line[0..open], '"') orelse continue;
                const library = line[first_quote + 1 .. open];
                if (ruleForLibrary(library)) |rule| try appendHit(allocator, &hits, rule, file, line_no);
            }
        }
    }
    return hits.toOwnedSlice(allocator);
}

fn parseArrayName(line: []const u8) ?[]const u8 {
    const const_start = std.mem.indexOf(u8, line, "const ") orelse return null;
    const array_start = std.mem.indexOfPos(u8, line, const_start + 6, " = &.{") orelse return null;
    return line[const_start + 6 .. array_start];
}

fn appendQuotedStrings(allocator: std.mem.Allocator, values: *std.ArrayList([]const u8), line: []const u8) !void {
    var cursor: usize = 0;
    while (std.mem.indexOfPos(u8, line, cursor, "\"") ) |open| {
        const close = std.mem.indexOfPos(u8, line, open + 1, "\"") orelse break;
        try values.append(allocator, try allocator.dupe(u8, line[open + 1 .. close]));
        cursor = close + 1;
    }
}

fn collectNamedArray(allocator: std.mem.Allocator, build_text: []const u8, wanted: []const u8) ![][]const u8 {
    var values = std.ArrayList([]const u8).empty;
    var lines = std.mem.splitScalar(u8, build_text, '\n');
    var active = false;
    while (lines.next()) |line| {
        if (!active) {
            const name = parseArrayName(line) orelse continue;
            if (!std.mem.eql(u8, name, wanted)) continue;
            active = true;
        }
        try appendQuotedStrings(allocator, &values, line);
        if (std.mem.indexOf(u8, line, "};") != null) break;
    }
    return values.toOwnedSlice(allocator);
}

fn freeStrings(allocator: std.mem.Allocator, values: [][]const u8) void {
    for (values) |value| allocator.free(value);
    allocator.free(values);
}

pub const ParseError = error{
    MissingPlayableSourceArrayManifest,
    MissingNonPlayableSourceArrayManifest,
    UnclassifiedSourceArray,
    UnknownManifestSourceArray,
};

pub fn parsePlayableSources(allocator: std.mem.Allocator, build_text: []const u8) ![][]const u8 {
    const playable_names = try collectNamedArray(allocator, build_text, "runtime_platform_playable_source_arrays");
    defer freeStrings(allocator, playable_names);
    if (playable_names.len == 0) return error.MissingPlayableSourceArrayManifest;
    const non_playable_names = try collectNamedArray(allocator, build_text, "runtime_platform_non_playable_source_arrays");
    defer freeStrings(allocator, non_playable_names);
    if (non_playable_names.len == 0) return error.MissingNonPlayableSourceArrayManifest;

    var sources = std.ArrayList([]const u8).empty;
    errdefer {
        for (sources.items) |source| allocator.free(source);
        sources.deinit(allocator);
    }
    var lines = std.mem.splitScalar(u8, build_text, '\n');
    var active_playable = false;
    var declared = std.StringHashMap(void).init(allocator);
    defer declared.deinit();
    while (lines.next()) |line| {
        if (!active_playable) {
            const name = parseArrayName(line) orelse continue;
            if (!std.mem.endsWith(u8, name, "_sources")) continue;
            try declared.put(name, {});
            var is_playable = false;
            var is_non_playable = false;
            for (playable_names) |allowed| is_playable = is_playable or std.mem.eql(u8, name, allowed);
            for (non_playable_names) |allowed| is_non_playable = is_non_playable or std.mem.eql(u8, name, allowed);
            if (!is_playable and !is_non_playable) return error.UnclassifiedSourceArray;
            active_playable = is_playable;
        }
        if (active_playable) try appendQuotedStrings(allocator, &sources, line);
        if (std.mem.indexOf(u8, line, "};") != null) active_playable = false;
    }

    for (playable_names) |name| if (!declared.contains(name)) return error.UnknownManifestSourceArray;
    for (non_playable_names) |name| if (!declared.contains(name)) return error.UnknownManifestSourceArray;
    return sources.toOwnedSlice(allocator);
}

pub fn ruleForLibrary(name: []const u8) ?Rule {
    if (std.mem.eql(u8, name, "dinput8")) return .{ .token = name, .service = "input", .packet = "P03" };
    if (std.mem.eql(u8, name, "ws2_32")) return .{ .token = name, .service = "net", .packet = "P04" };
    if (std.mem.eql(u8, name, "winmm")) return .{ .token = name, .service = "audio", .packet = "P05" };
    if (std.mem.eql(u8, name, "d3d9") or std.mem.eql(u8, name, "dxguid")) return .{ .token = name, .service = "graphics", .packet = "P08" };
    return null;
}

pub fn scanBuildLibraries(allocator: std.mem.Allocator, build_text: []const u8, file: []const u8, playable_functions: []const []const u8) ![]Hit {
    var hits = std.ArrayList(Hit).empty;
    var lines = std.mem.splitScalar(u8, build_text, '\n');
    var line_no: usize = 0;
    var current_function: ?[]const u8 = null;
    var windows_guard_depth: usize = 0;
    while (lines.next()) |line| {
        line_no += 1;
        const trimmed = std.mem.trim(u8, line, " \t");
        if (std.mem.startsWith(u8, trimmed, "fn ")) {
            const function_start = std.mem.indexOf(u8, trimmed, "fn ").? + 3;
            const function_end = std.mem.indexOfPos(u8, trimmed, function_start, "(") orelse function_start;
            current_function = trimmed[function_start..function_end];
        }
        const function_is_playable = if (current_function) |name| blk: {
            var matched = false;
            for (playable_functions) |allowed| matched = matched or std.mem.eql(u8, name, allowed);
            break :blk matched;
        } else false;
        if (function_is_playable and windows_guard_depth == 0) {
            var cursor: usize = 0;
            while (std.mem.indexOfPos(u8, line, cursor, "linkSystemLibrary(\"") ) |open| {
                const name_start = open + "linkSystemLibrary(\"".len;
                const name_end = std.mem.indexOfPos(u8, line, name_start, "\"") orelse break;
                const name = line[name_start..name_end];
                if (ruleForLibrary(name)) |rule| try appendHit(allocator, &hits, rule, file, line_no);
                cursor = name_end + 1;
            }
        }

        // Playable native libraries are permitted only inside an explicit
        // Windows target block.  Track the simple block form used by build.zig
        // and leave unguarded links visible to the audit.
        if (std.mem.indexOf(u8, line, "if (target.result.os.tag == .windows") != null) {
            const opens = std.mem.count(u8, line, "{");
            const closes = std.mem.count(u8, line, "}");
            if (opens > closes) windows_guard_depth += opens - closes;
        } else if (windows_guard_depth > 0) {
            const opens = std.mem.count(u8, line, "{");
            const closes = std.mem.count(u8, line, "}");
            if (closes > opens) {
                const decrement = @min(windows_guard_depth, closes - opens);
                windows_guard_depth -= decrement;
            }
        }
    }
    return hits.toOwnedSlice(allocator);
}

fn isSeparator(c: u8) bool {
    return c == '/' or c == '\\';
}

fn removeLastPathComponent(path: *std.ArrayList(u8)) void {
    while (path.items.len > 0 and isSeparator(path.items[path.items.len - 1])) path.items.len -= 1;
    while (path.items.len > 0 and !isSeparator(path.items[path.items.len - 1])) path.items.len -= 1;
    while (path.items.len > 0 and isSeparator(path.items[path.items.len - 1])) path.items.len -= 1;
}

fn findActualEntry(allocator: std.mem.Allocator, cwd: std.Io.Dir, io: std.Io, parent: []const u8, wanted: []const u8) !?[]const u8 {
    var dir = cwd.openDir(io, if (parent.len == 0) "." else parent, .{ .iterate = true }) catch return null;
    defer std.Io.Dir.close(dir, io);
    var iterator = dir.iterate();
    var folded: ?[]const u8 = null;
    while (try iterator.next(io)) |entry| {
        if (std.mem.eql(u8, entry.name, wanted)) {
            if (folded) |value| allocator.free(value);
            return try allocator.dupe(u8, entry.name);
        }
        if (folded == null and std.ascii.eqlIgnoreCase(entry.name, wanted)) folded = try allocator.dupe(u8, entry.name);
    }
    return folded;
}

fn hasWrongCaseRelativeInclude(allocator: std.mem.Allocator, cwd: std.Io.Dir, io: std.Io, source_file: []const u8, include_path: []const u8) !bool {
    if (include_path.len == 0 or fs.path.isAbsolute(include_path)) return false;
    var resolved = std.ArrayList(u8).empty;
    defer resolved.deinit(allocator);
    try resolved.appendSlice(allocator, fs.path.dirname(source_file) orelse ".");

    var cursor: usize = 0;
    while (cursor < include_path.len) {
        while (cursor < include_path.len and isSeparator(include_path[cursor])) cursor += 1;
        const component_start = cursor;
        while (cursor < include_path.len and !isSeparator(include_path[cursor])) cursor += 1;
        if (component_start == cursor) continue;
        const component = include_path[component_start..cursor];
        if (std.mem.eql(u8, component, ".")) continue;
        if (std.mem.eql(u8, component, "..")) {
            removeLastPathComponent(&resolved);
            continue;
        }
        const actual = try findActualEntry(allocator, cwd, io, resolved.items, component) orelse return false;
        defer allocator.free(actual);
        const wrong_case = !std.mem.eql(u8, actual, component);
        if (wrong_case) return true;
        if (resolved.items.len > 0 and !isSeparator(resolved.items[resolved.items.len - 1])) try resolved.append(allocator, '/');
        try resolved.appendSlice(allocator, actual);
    }
    return false;
}

fn quotedInclude(line: []const u8) ?[]const u8 {
    const include_start = std.mem.indexOf(u8, line, "#include") orelse return null;
    const open = std.mem.indexOfPos(u8, line, include_start + "#include".len, "\"") orelse return null;
    const close = std.mem.indexOfPos(u8, line, open + 1, "\"") orelse return null;
    return line[open + 1 .. close];
}

pub fn scanTextWithCase(allocator: std.mem.Allocator, text: []const u8, file: []const u8, cwd: std.Io.Dir, io: std.Io) ![]Hit {
    var hits = std.ArrayList(Hit).empty;
    const token_hits = try scanText(allocator, text, file);
    defer allocator.free(token_hits);
    try hits.appendSlice(allocator, token_hits);

    var lines = std.mem.splitScalar(u8, text, '\n');
    var line_no: usize = 0;
    while (lines.next()) |line| {
        line_no += 1;
        if (quotedInclude(line)) |include_path| {
            if (try hasWrongCaseRelativeInclude(allocator, cwd, io, file, include_path)) {
                try hits.append(allocator, .{ .service = "case", .packet = "P01", .token = "wrong-case relative include", .file = file, .line = line_no });
            }
        }
    }
    return hits.toOwnedSlice(allocator);
}

pub fn hitKey(allocator: std.mem.Allocator, hit: Hit) ![]const u8 {
    return std.fmt.allocPrint(allocator, "{s}|{s}|{s}|{s}|{d}", .{ hit.service, hit.packet, hit.token, hit.file, hit.line });
}
