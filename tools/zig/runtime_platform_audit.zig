const std = @import("std");

pub const Hit = struct {
    service: []const u8,
    packet: []const u8,
    token: []const u8,
    file: []const u8,
    line: usize,
};

const playable_arrays = [_][]const u8{
    "misc_sources", "image_sources", "lualib_c_sources", "lualib_cpp_sources",
    "net_sources", "input_sources", "formats_sources", "anim_sources",
    "common_sources", "ui_sources", "sfx_cpp_sources", "sfx_c_sources",
    "gfx_sources", "gfx_gpu_sources", "randommapgen_sources", "main_sources",
    "game_sources",
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
        if (std.mem.indexOf(u8, line, "#include \"") != null and
            (std.mem.indexOf(u8, line, "WrongCase") != null or std.mem.indexOf(u8, line, "wrongcase") != null)) {
            try hits.append(allocator, .{ .service = "case", .packet = "P01", .token = "wrong-case relative include", .file = file, .line = line_no });
        }
    }
    return hits.toOwnedSlice(allocator);
}

pub fn parsePlayableSources(allocator: std.mem.Allocator, build_text: []const u8) ![][]const u8 {
    var sources = std.ArrayList([]const u8).empty;
    var lines = std.mem.splitScalar(u8, build_text, '\n');
    var active = false;
    while (lines.next()) |line| {
        if (!active) {
            if (std.mem.indexOf(u8, line, "const ") == null or std.mem.indexOf(u8, line, " = &.{") == null) continue;
            const name_start = std.mem.indexOf(u8, line, "const ").? + 6;
            const name_end = std.mem.indexOfPos(u8, line, name_start, " ").?;
            const name = line[name_start..name_end];
            active = false;
            for (playable_arrays) |allowed| {
                if (std.mem.eql(u8, name, allowed)) active = true;
            }
            if (!active) continue;
        }
        var cursor: usize = 0;
        while (std.mem.indexOfPos(u8, line, cursor, "\"") ) |open| {
            const close = std.mem.indexOfPos(u8, line, open + 1, "\"") orelse break;
            try sources.append(allocator, try allocator.dupe(u8, line[open + 1 .. close]));
            cursor = close + 1;
        }
        if (std.mem.indexOf(u8, line, "};") != null) active = false;
    }
    return sources.toOwnedSlice(allocator);
}

pub fn ruleForLibrary(name: []const u8) ?Rule {
    if (std.mem.eql(u8, name, "dinput8") or std.mem.eql(u8, name, "dxguid")) return .{ .token = name, .service = "input", .packet = "P03" };
    if (std.mem.eql(u8, name, "ws2_32")) return .{ .token = name, .service = "net", .packet = "P04" };
    if (std.mem.eql(u8, name, "winmm")) return .{ .token = name, .service = "audio", .packet = "P05" };
    if (std.mem.eql(u8, name, "d3d9") or std.mem.eql(u8, name, "dxguid")) return .{ .token = name, .service = "graphics", .packet = "P08" };
    return null;
}

pub fn hitKey(allocator: std.mem.Allocator, hit: Hit) ![]const u8 {
    return std.fmt.allocPrint(allocator, "{s}|{s}|{s}|{s}|{d}", .{ hit.service, hit.packet, hit.token, hit.file, hit.line });
}
