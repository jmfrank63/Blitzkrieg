//! The completeness gate of M002 (D063, D076): `zig build l10n-check` reads the tracked tree and
//! fails when a translation is missing, stale or malformed. Every finding is one line on stderr,
//! `l10n-check: <rule>: <code>: <where>: <detail>`, where `<code>` is a language code (or `all`
//! for a finding that belongs to no language) and `<where>` names the ELK key or file. The run
//! ends with `l10n-check: <n> findings, <m> checked`, and one `l10n-check: <scope>: not active`
//! note per scope that has nothing to check yet, so a CI log shows what was and was not checked.
//! Exit status is 1 on any finding, 2 on a bad command line.
//!
//! The rules are `pub` and take a `Report`, so tests can run them on in-memory data and later
//! rules plug in beside `checkOverlays` without touching the format.
//!
//! Scope: `Localization/game/scope.txt` lists the ELK keys (relative to `game_data_base`, no
//! extension) every language overlay must hold. An overlay text is `<key>.txt` (UTF-16LE with a
//! byte order mark) beside its state file `<key>.xml` (`<base State="n" .../>`, 0 not translated,
//! 1 outdated, 2 translated, 3 approved, as Sources/editor/elk/core/state.zig names them).
const std = @import("std");
const languages = @import("l10n_languages.zig");

pub const Finding = struct {
    rule: []const u8,
    code: []const u8,
    where: []const u8,
    detail: []const u8,
};

pub const Report = struct {
    arena: std.mem.Allocator,
    findings: std.ArrayList(Finding) = .empty,
    inactive: std.ArrayList([]const u8) = .empty,
    checked: usize = 0,

    pub fn add(self: *Report, rule: []const u8, code: []const u8, where: []const u8, detail: []const u8) !void {
        try self.findings.append(self.arena, .{
            .rule = rule,
            .code = code,
            .where = try self.arena.dupe(u8, where),
            .detail = try self.arena.dupe(u8, detail),
        });
    }

    pub fn addFmt(self: *Report, rule: []const u8, code: []const u8, where: []const u8, comptime fmt: []const u8, args: anytype) !void {
        try self.add(rule, code, where, try std.fmt.allocPrint(self.arena, fmt, args));
    }

    /// The finding as it is printed, without the line break.
    pub fn line(self: *Report, finding: Finding) ![]u8 {
        return std.fmt.allocPrint(self.arena, "l10n-check: {s}: {s}: {s}: {s}", .{ finding.rule, finding.code, finding.where, finding.detail });
    }
};

/// The state numbers of state.zig.
pub const state_translated = 2;
pub const state_approved = 3;

pub const TextProblem = union(enum) {
    ok,
    no_bom,
    odd_length,
    unpaired_surrogate: usize,
    empty,
};

/// Whether `bytes` is a UTF-16LE text with a byte order mark, an even length, paired surrogates
/// and at least one character that is not white space.
pub fn textProblem(bytes: []const u8) TextProblem {
    if (bytes.len < 2 or bytes[0] != 0xff or bytes[1] != 0xfe) return .no_bom;
    if (bytes.len % 2 != 0) return .odd_length;
    var visible = false;
    var unit_index: usize = 0;
    var at: usize = 2;
    while (at < bytes.len) : (at += 2) {
        const unit = std.mem.readInt(u16, bytes[at..][0..2], .little);
        if (unit >= 0xD800 and unit <= 0xDBFF) {
            const next: ?u16 = if (at + 3 < bytes.len) std.mem.readInt(u16, bytes[at + 2 ..][0..2], .little) else null;
            if (next == null or next.? < 0xDC00 or next.? > 0xDFFF) return .{ .unpaired_surrogate = unit_index };
            at += 2;
            unit_index += 1;
            visible = true;
        } else if (unit >= 0xDC00 and unit <= 0xDFFF) {
            return .{ .unpaired_surrogate = unit_index };
        } else if (unit != ' ' and unit != '\t' and unit != '\r' and unit != '\n' and unit != 0xFEFF) {
            visible = true;
        }
        unit_index += 1;
    }
    return if (visible) .ok else .empty;
}

/// The `State` of a state file, or null when it holds no readable `State="n"`.
pub fn parseState(xml: []const u8) ?u8 {
    const marker = "State=\"";
    const start = (std.mem.indexOf(u8, xml, marker) orelse return null) + marker.len;
    const end = std.mem.indexOfScalarPos(u8, xml, start, '"') orelse return null;
    return std.fmt.parseInt(u8, xml[start..end], 10) catch null;
}

/// The scope keys of a `scope.txt`: one per line, `#` lines and blank lines skipped.
pub fn parseScope(arena: std.mem.Allocator, text: []const u8) ![]const []const u8 {
    var keys: std.ArrayList([]const u8) = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw| {
        const key = std.mem.trim(u8, raw, " \t\r");
        if (key.len == 0 or key[0] == '#') continue;
        try keys.append(arena, key);
    }
    return keys.items;
}

fn readOptional(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, path: []const u8) !?[]u8 {
    return root.readFileAlloc(io, path, arena, .limited(16 << 20)) catch |err| switch (err) {
        error.FileNotFound, error.NotDir => null,
        else => err,
    };
}

/// Whether `rel` (forward slashes) exists below `root`, matching every component case-insensitively
/// when the exact spelling is absent, as the Data tree of a case-sensitive file system needs.
pub fn existsIgnoreCase(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, rel: []const u8) !bool {
    if (root.statFile(io, rel, .{})) |_| return true else |_| {}
    var current = try root.openDir(io, ".", .{ .iterate = true });
    var parts = std.mem.tokenizeScalar(u8, rel, '/');
    while (parts.next()) |part| {
        const last = parts.peek() == null;
        var found: ?[]const u8 = null;
        var it = current.iterate();
        while (try it.next(io)) |entry| {
            if (!std.ascii.eqlIgnoreCase(entry.name, part)) continue;
            if (last != (entry.kind != .directory)) continue;
            found = try arena.dupe(u8, entry.name);
            break;
        }
        const name = found orelse {
            current.close(io);
            return false;
        };
        if (last) {
            current.close(io);
            return true;
        }
        const next = current.openDir(io, name, .{ .iterate = true }) catch {
            current.close(io);
            return false;
        };
        current.close(io);
        current = next;
    }
    current.close(io);
    return false;
}

/// Overlay completeness and state (rules missing, missing-xml, state, stale, encoding, empty,
/// extra, base-missing): every language of the table holds every scope key as a text with a
/// Translated or Approved state, and holds nothing else; every scope key exists in the base.
pub fn checkOverlays(io: std.Io, report: *Report, root: std.Io.Dir) !void {
    const arena = report.arena;
    const scope_text = (try readOptional(io, arena, root, "Localization/game/scope.txt")) orelse {
        try report.add("scope", "all", "Localization/game/scope.txt", "file is missing");
        return;
    };
    const keys = try parseScope(arena, scope_text);
    if (keys.len == 0) try report.add("scope", "all", "Localization/game/scope.txt", "lists no key");

    for (keys) |key| {
        report.checked += 1;
        const as_elk = try std.fmt.allocPrint(arena, "Data/ELK/game_data_base/{s}.elk", .{key});
        const as_txt = try std.fmt.allocPrint(arena, "Data/ELK/game_data_base/{s}.txt", .{key});
        if (!try existsIgnoreCase(io, arena, root, as_elk) and !try existsIgnoreCase(io, arena, root, as_txt)) {
            try report.add("base-missing", "all", key, "no original text in Data/ELK/game_data_base");
        }
    }

    for (languages.languages) |language| {
        const base = try std.fmt.allocPrint(arena, "Localization/game/{s}/game_data_base", .{language.code});
        for (keys) |key| {
            report.checked += 1;
            const txt_path = try std.fmt.allocPrint(arena, "{s}/{s}.txt", .{ base, key });
            const xml_path = try std.fmt.allocPrint(arena, "{s}/{s}.xml", .{ base, key });
            if (try readOptional(io, arena, root, txt_path)) |bytes| {
                switch (textProblem(bytes)) {
                    .ok => {},
                    .no_bom => try report.add("encoding", language.code, key, "text lacks the UTF-16LE byte order mark"),
                    .odd_length => try report.addFmt("encoding", language.code, key, "text has an odd byte count ({d})", .{bytes.len}),
                    .unpaired_surrogate => |unit| try report.addFmt("encoding", language.code, key, "unpaired surrogate at unit {d}", .{unit}),
                    .empty => try report.add("empty", language.code, key, "text holds no characters"),
                }
            } else {
                try report.add("missing", language.code, key, "no translation");
            }
            if (try readOptional(io, arena, root, xml_path)) |xml| {
                const state = parseState(xml);
                if (state == null) {
                    try report.add("state", language.code, key, "state file has no State attribute");
                } else switch (state.?) {
                    state_translated, state_approved => {},
                    1 => try report.add("stale", language.code, key, "State=1 (outdated): the English text changed after the translation"),
                    0 => try report.add("state", language.code, key, "State=0 (not translated), want 2 or 3"),
                    else => try report.addFmt("state", language.code, key, "State={d} is not a state, want 2 or 3", .{state.?}),
                }
            } else {
                try report.add("missing-xml", language.code, key, "no state file");
            }
        }
        try checkExtra(io, report, root, language.code, base, keys);
    }
}

fn checkExtra(io: std.Io, report: *Report, root: std.Io.Dir, code: []const u8, base: []const u8, keys: []const []const u8) !void {
    const arena = report.arena;
    var dir = root.openDir(io, base, .{ .iterate = true }) catch return;
    defer dir.close(io);
    var walker = try dir.walk(arena);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file or !std.mem.endsWith(u8, entry.path, ".txt")) continue;
        const key = try arena.dupe(u8, entry.path[0 .. entry.path.len - ".txt".len]);
        for (key) |*c| {
            if (c.* == '\\') c.* = '/';
        }
        var in_scope = false;
        for (keys) |one| {
            if (std.mem.eql(u8, one, key)) {
                in_scope = true;
                break;
            }
        }
        if (!in_scope) try report.add("extra", code, key, "not listed in Localization/game/scope.txt");
    }
}

fn usage() noreturn {
    std.debug.print("usage: l10n_check [--root <dir>]\n", .{});
    std.process.exit(2);
}

pub fn main(init: std.process.Init) !void {
    var arena_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = init.io;

    var iterator = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    defer iterator.deinit();
    _ = iterator.skip();
    var root_path: []const u8 = ".";
    while (iterator.next()) |arg| {
        if (std.mem.eql(u8, arg, "--root")) {
            root_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else usage();
    }

    var root = std.Io.Dir.cwd().openDir(io, root_path, .{ .iterate = true }) catch |err| {
        std.debug.print("l10n-check: root: all: {s}: cannot open ({s})\n", .{ root_path, @errorName(err) });
        std.process.exit(1);
    };
    defer root.close(io);

    var report: Report = .{ .arena = arena };
    try checkOverlays(io, &report, root);

    for (report.findings.items) |finding| std.debug.print("{s}\n", .{try report.line(finding)});
    for (report.inactive.items) |scope| std.debug.print("l10n-check: {s}: not active\n", .{scope});
    std.debug.print("l10n-check: {d} findings, {d} checked\n", .{ report.findings.items.len, report.checked });
    if (report.findings.items.len != 0) std.process.exit(1);
}

test "textProblem tells the byte order mark, length, surrogates and empty text apart" {
    const testing = std.testing;
    try testing.expectEqual(TextProblem.ok, textProblem(&.{ 0xff, 0xfe, 'A', 0 }));
    try testing.expectEqual(TextProblem.no_bom, textProblem(&.{ 'A', 0 }));
    try testing.expectEqual(TextProblem.odd_length, textProblem(&.{ 0xff, 0xfe, 'A', 0, 'B' }));
    try testing.expectEqual(TextProblem.empty, textProblem(&.{ 0xff, 0xfe, ' ', 0, '\r', 0 }));
    try testing.expectEqual(TextProblem{ .unpaired_surrogate = 1 }, textProblem(&.{ 0xff, 0xfe, 'A', 0, 0x3d, 0xd8 }));
    try testing.expectEqual(TextProblem.ok, textProblem(&.{ 0xff, 0xfe, 0x3d, 0xd8, 0x00, 0xde }));
}

test "parseState reads the State attribute" {
    const testing = std.testing;
    try testing.expectEqual(@as(?u8, 3), parseState("<?xml version=\"1.0\"?>\r\n<base State=\"3\" Changed=\"1\"/>\r\n"));
    try testing.expectEqual(@as(?u8, null), parseState("<base/>"));
}
