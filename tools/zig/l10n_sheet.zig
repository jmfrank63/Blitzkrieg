//! The overlay sheet helper of the translation tasks (M003 S01 T01, D096): a translation task
//! exports the English originals under some key prefixes into a UTF-8 text sheet, a person or an
//! agent writes the translations into the sheet, and the import writes the overlay files the
//! checker (tools/zig/l10n_check.zig) expects, so nobody types UTF-16LE by hand.
//!
//!   zig build l10n-sheet-export -- <code> <key prefix>...
//!   zig build l10n-sheet-import -- <code> <sheet>
//!
//! A sheet is UTF-8 with CRLF line breaks. Each text is a block: a header line `=== <key>`, then
//! the English text exactly as the original holds it (CR LF line breaks kept), then one CR LF that
//! only separates it from the next header. The existing overlay text, when there is one, follows
//! under a `--- current` line; the import ignores it. The translator replaces the English text
//! above that line and keeps or drops the rest. A trailing line break belongs to the text (1,053
//! of the 2,852 originals end in one), so the separator is the single CR LF before the next header.
//!
//! The import writes `Localization/game/<code>/game_data_base/<key>.txt` (UTF-16LE with a byte
//! order mark, CR LF inside the text, a bare LF becomes CR LF) and `<key>.xml` (State 2). It
//! refuses, and writes nothing, when a key has no `.elk` original or a block holds nothing but
//! white space. Adding the `prefix <code> <prefix>` line to Localization/game/scope.txt is left to
//! the task, so a half-translated prefix never reaches the gate.
const std = @import("std");
const check = @import("l10n_check.zig");
const languages = @import("l10n_languages.zig");

pub const sheet_dir = "zig-out/local-test/l10n-sheet";

/// A sheet is cut before a block would take it over this many UTF-8 bytes (one block is never split),
/// so a translation task works through pieces it can hold at once.
pub const max_sheet_bytes: usize = 40_000;

const state_xml = "<?xml version=\"1.0\"?>\r\n<base State=\"2\" Changed=\"1\"/>\r\n";
const current_marker = "--- current";

pub const Refusal = struct { key: []const u8, detail: []const u8 };

fn knownCode(code: []const u8) bool {
    for (languages.languages) |language| {
        if (std.mem.eql(u8, language.code, code)) return true;
    }
    return false;
}

/// The UTF-8 text of a UTF-16LE file with a byte order mark; null when it is not one.
pub fn decodeText(arena: std.mem.Allocator, bytes: []const u8) !?[]u8 {
    if (check.textProblem(bytes) == .no_bom or bytes.len % 2 != 0) return null;
    const units = try arena.alloc(u16, (bytes.len - 2) / 2);
    for (units, 0..) |*unit, i| unit.* = std.mem.readInt(u16, bytes[2 + i * 2 ..][0..2], .little);
    return std.unicode.utf16LeToUtf8Alloc(arena, units) catch null;
}

/// UTF-16LE with a byte order mark, every bare LF written as CR LF.
pub fn encodeText(arena: std.mem.Allocator, text: []const u8) ![]u8 {
    var normalized: std.ArrayList(u8) = .empty;
    for (text, 0..) |c, i| {
        if (c == '\n' and (i == 0 or text[i - 1] != '\r')) try normalized.append(arena, '\r');
        try normalized.append(arena, c);
    }
    const units = try std.unicode.utf8ToUtf16LeAlloc(arena, normalized.items);
    var out = try arena.alloc(u8, 2 + units.len * 2);
    out[0] = 0xff;
    out[1] = 0xfe;
    for (units, 0..) |unit, i| std.mem.writeInt(u16, out[2 + i * 2 ..][0..2], unit, .little);
    return out;
}

fn startsWithAny(key: []const u8, prefixes: []const []const u8) bool {
    for (prefixes) |prefix| {
        if (std.ascii.startsWithIgnoreCase(key, prefix)) return true;
    }
    return false;
}

fn overlayPath(arena: std.mem.Allocator, code: []const u8, key: []const u8, ext: []const u8) ![]u8 {
    return std.fmt.allocPrint(arena, "Localization/game/{s}/game_data_base/{s}.{s}", .{ code, key, ext });
}

/// Writes the sheets of every original whose key starts with one of `prefixes` into `out` (a
/// directory below `root`) and returns their paths. A prefix that matches nothing is an error.
pub fn exportSheets(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, code: []const u8, prefixes: []const []const u8, out: []const u8) ![]const []const u8 {
    if (!knownCode(code)) return error.UnknownLanguage;
    const all = try check.databaseKeys(io, arena, root);
    for (prefixes) |prefix| {
        var found = false;
        for (all) |key| found = found or std.ascii.startsWithIgnoreCase(key, prefix);
        if (!found) return error.PrefixMatchesNothing;
    }
    try root.createDirPath(io, out);
    var dir = try root.openDir(io, out, .{ .iterate = true });
    defer dir.close(io);
    // Sheets of an earlier export of this language would mix with the new ones.
    const stem = try std.fmt.allocPrint(arena, "{s}-", .{code});
    var old = dir.iterate();
    while (try old.next(io)) |entry| {
        if (entry.kind == .file and std.mem.startsWith(u8, entry.name, stem) and std.mem.endsWith(u8, entry.name, ".txt")) try dir.deleteFile(io, entry.name);
    }

    var paths: std.ArrayList([]const u8) = .empty;
    var sheet: std.ArrayList(u8) = .empty;
    for (all) |key| {
        if (!startsWithAny(key, prefixes)) continue;
        const english_path = try std.fmt.allocPrint(arena, "Data/ELK/game_data_base/{s}.elk", .{key});
        const english_bytes = (try check.readOptional(io, arena, root, english_path)) orelse return error.OriginalUnreadable;
        // The 2003 original of textes/gametypes/game_type_unknown ends in a stray half unit.
        const whole_units = english_bytes[0 .. english_bytes.len & ~@as(usize, 1)];
        const english = (try decodeText(arena, whole_units)) orelse return error.OriginalUnreadable;

        var block: std.ArrayList(u8) = .empty;
        try block.appendSlice(arena, try std.fmt.allocPrint(arena, "=== {s}\r\n", .{key}));
        try block.appendSlice(arena, english);
        try block.appendSlice(arena, "\r\n");
        if (try check.readOptional(io, arena, root, try overlayPath(arena, code, key, "txt"))) |current_bytes| {
            if (try decodeText(arena, current_bytes)) |current| {
                try block.appendSlice(arena, current_marker ++ "\r\n");
                try block.appendSlice(arena, current);
                try block.appendSlice(arena, "\r\n");
            }
        }
        if (sheet.items.len != 0 and sheet.items.len + block.items.len > max_sheet_bytes) try flushSheet(io, arena, dir, out, code, &sheet, &paths);
        try sheet.appendSlice(arena, block.items);
    }
    if (sheet.items.len != 0) try flushSheet(io, arena, dir, out, code, &sheet, &paths);
    return paths.items;
}

fn flushSheet(io: std.Io, arena: std.mem.Allocator, dir: std.Io.Dir, out: []const u8, code: []const u8, sheet: *std.ArrayList(u8), paths: *std.ArrayList([]const u8)) !void {
    const name = try std.fmt.allocPrint(arena, "{s}-{d}.txt", .{ code, paths.items.len + 1 });
    try dir.writeFile(io, .{ .sub_path = name, .data = sheet.items });
    try paths.append(arena, try std.fmt.allocPrint(arena, "{s}/{s}", .{ out, name }));
    sheet.clearRetainingCapacity();
}

pub const Block = struct { key: []const u8, text: []const u8 };

/// The blocks of a sheet. The text of a block is what lies between its header and the next header
/// or the `--- current` line, less the one CR LF (or LF) that separates blocks.
pub fn parseSheet(arena: std.mem.Allocator, sheet: []const u8) ![]const Block {
    const bytes = if (std.mem.startsWith(u8, sheet, "\xEF\xBB\xBF")) sheet[3..] else sheet;
    var blocks: std.ArrayList(Block) = .empty;
    var key: ?[]const u8 = null;
    var body_start: usize = 0;
    var body_end: ?usize = null;
    var at: usize = 0;
    while (at <= bytes.len) {
        const line_end = std.mem.indexOfScalarPos(u8, bytes, at, '\n') orelse bytes.len;
        const line = std.mem.trimEnd(u8, bytes[at..line_end], "\r");
        const next = @min(line_end + 1, bytes.len + 1);
        if (std.mem.startsWith(u8, line, "=== ")) {
            if (key) |k| try blocks.append(arena, .{ .key = k, .text = blockText(bytes, body_start, body_end orelse at) });
            key = std.mem.trim(u8, line["=== ".len..], " \t");
            body_start = @min(next, bytes.len);
            body_end = null;
        } else if (key != null and body_end == null and std.mem.eql(u8, line, current_marker)) {
            body_end = at;
        }
        if (line_end >= bytes.len) break;
        at = next;
    }
    if (key) |k| try blocks.append(arena, .{ .key = k, .text = blockText(bytes, body_start, body_end orelse bytes.len) });
    return blocks.items;
}

/// `bytes[start..end]` without the separator break before the next header.
fn blockText(bytes: []const u8, start: usize, end: usize) []const u8 {
    var text = bytes[start..@max(start, end)];
    if (std.mem.endsWith(u8, text, "\r\n")) {
        text = text[0 .. text.len - 2];
    } else if (std.mem.endsWith(u8, text, "\n")) text = text[0 .. text.len - 1];
    return text;
}

pub const ImportResult = union(enum) {
    written: usize,
    refused: []const Refusal,
};

/// Writes every block of `sheet` as an overlay text of `code`, or refuses the whole sheet.
pub fn importSheet(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, code: []const u8, sheet: []const u8) !ImportResult {
    if (!knownCode(code)) return error.UnknownLanguage;
    const blocks = try parseSheet(arena, sheet);
    if (blocks.len == 0) return error.SheetHasNoBlock;
    var known: std.StringHashMapUnmanaged(void) = .empty;
    for (try check.databaseKeys(io, arena, root)) |key| try known.put(arena, key, {});

    var refusals: std.ArrayList(Refusal) = .empty;
    var seen: std.StringHashMapUnmanaged(void) = .empty;
    for (blocks) |block| {
        if (!known.contains(block.key)) {
            try refusals.append(arena, .{ .key = block.key, .detail = "not a text of the game (no original in Data/ELK/game_data_base or the Data text trees)" });
        } else if (std.mem.trim(u8, block.text, " \t\r\n").len == 0) {
            try refusals.append(arena, .{ .key = block.key, .detail = "the block is empty" });
        } else if ((try seen.fetchPut(arena, block.key, {})) != null) {
            try refusals.append(arena, .{ .key = block.key, .detail = "the key is in the sheet twice" });
        }
    }
    if (refusals.items.len != 0) return .{ .refused = refusals.items };

    for (blocks) |block| {
        const txt = try overlayPath(arena, code, block.key, "txt");
        if (std.fs.path.dirname(txt)) |parent| try root.createDirPath(io, parent);
        try root.writeFile(io, .{ .sub_path = txt, .data = try encodeText(arena, block.text) });
        try root.writeFile(io, .{ .sub_path = try overlayPath(arena, code, block.key, "xml"), .data = state_xml });
    }
    return .{ .written = blocks.len };
}

fn usage() noreturn {
    std.debug.print("usage: l10n_sheet [--root <dir>] export <code> <key prefix>... | import <code> <sheet>\n", .{});
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
    var args: std.ArrayList([]const u8) = .empty;
    var root_path: []const u8 = ".";
    while (iterator.next()) |arg| {
        if (std.mem.eql(u8, arg, "--root")) {
            root_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else try args.append(arena, try arena.dupe(u8, arg));
    }
    if (args.items.len < 3) usage();
    var root = std.Io.Dir.cwd().openDir(io, root_path, .{ .iterate = true }) catch |err| {
        std.debug.print("l10n-sheet: cannot open {s} ({s})\n", .{ root_path, @errorName(err) });
        std.process.exit(1);
    };
    defer root.close(io);

    const command = args.items[0];
    const code = args.items[1];
    if (std.mem.eql(u8, command, "export")) {
        const paths = exportSheets(io, arena, root, code, args.items[2..], sheet_dir) catch |err| {
            std.debug.print("l10n-sheet: export: {s}: {s}\n", .{ code, @errorName(err) });
            std.process.exit(1);
        };
        for (paths) |path| printOut(io, "l10n-sheet: wrote {s}\n", .{path});
    } else if (std.mem.eql(u8, command, "import") and args.items.len == 3) {
        const sheet = root.readFileAlloc(io, args.items[2], arena, .limited(64 << 20)) catch |err| {
            std.debug.print("l10n-sheet: import: {s}: cannot read ({s})\n", .{ args.items[2], @errorName(err) });
            std.process.exit(1);
        };
        const result = importSheet(io, arena, root, code, sheet) catch |err| {
            std.debug.print("l10n-sheet: import: {s}: {s}\n", .{ code, @errorName(err) });
            std.process.exit(1);
        };
        switch (result) {
            .written => |count| printOut(io, "l10n-sheet: wrote {d} texts for {s}\n", .{ count, code }),
            .refused => |list| {
                for (list) |one| std.debug.print("l10n-sheet: import: {s}: {s}: {s}\n", .{ code, one.key, one.detail });
                std.debug.print("l10n-sheet: nothing written\n", .{});
                std.process.exit(1);
            },
        }
    } else usage();
}

/// A result line on stdout: stderr is for what went wrong.
fn printOut(io: std.Io, comptime format: []const u8, args: anytype) void {
    var buffer: [1024]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(io, &buffer);
    stdout.interface.print(format, args) catch return;
    stdout.interface.flush() catch {};
}
