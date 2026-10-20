//! `bk-table`: flatten a folder of stat files into one tab-separated sheet and
//! write a sheet's edited cells back (Phase 8, D-08..D-11). It replaces
//! ExcelExporter.exe, whose "Excel file" was this same tab text; Excel,
//! Numbers and LibreOffice open it directly.
//!
//! `export` writes the legacy sheet: a `FileName<TAB>path...` header, one row
//! per file (sorted, `/` separators, so a sheet made on one OS imports on
//! another), one column per field path in byte order, the union over all
//! files; a file without a field gets an empty cell. Paths follow the legacy
//! flattening, see `bk_table_xml.zig`.
//!
//! `import` patches only the value bytes of cells that differ from the file
//! (`bk_table_xml.applyPatches`); untouched content keeps every byte, where the
//! legacy tool re-serialised whole files through MSXML. An empty cell or `_`
//! leaves the value alone. A row whose file is missing and a cell whose path
//! the file does not have are reported and skipped, never created.
//!
//! The stat root of a file comes from its extension (`root_table`, the legacy
//! one) unless `--root` names it, so a mod's own stat type needs no new build.
//!
//! The `--only`/`--ignore` field list is the legacy "crap file": the first
//! word of each line is a path prefix, `item(NN)` in a path matching plain
//! `item`. One deliberate change from the legacy rule: there, ignoring
//! `Armor;Front` also dropped `Armor;Side`, because the subtree test matched
//! any path sharing a prefix with an ignored field; here an ignored field
//! drops only what it names.

const std = @import("std");
const bk_tool = @import("bk_tool.zig");
const xml = @import("bk_table_xml.zig");
const bk_l10n = @import("bk_l10n.zig");

const usage: bk_tool.Usage = .{
    .synopsis =
    \\usage: bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
    \\                       [--no-recurse] [--root <name>]
    \\       bk-table import <sheet.tsv> [--root <name>]
    \\
    ,
    .body = &.{"table.usage.body"},
};

pub fn main(init: std.process.Init) u8 {
    return bk_tool.main(init, "bk-table", usage, run);
}

fn run(ctx: *bk_tool.Context) !void {
    if (ctx.args.len == 0) return error.Usage;
    const Command = enum { @"export", import };
    const which = std.meta.stringToEnum(Command, ctx.args[0]) orelse return error.Usage;

    var positional: [3][]const u8 = undefined;
    var positional_count: usize = 0;
    var filter_mode: Filter.Mode = .none;
    var filter_path: ?[]const u8 = null;
    var recurse = true;
    var root: ?[]const u8 = null;
    var i: usize = 1;
    while (i < ctx.args.len) : (i += 1) {
        const arg = ctx.args[i];
        if (std.mem.eql(u8, arg, "--ignore") or std.mem.eql(u8, arg, "--only")) {
            if (which != .@"export" or filter_path != null or i + 1 == ctx.args.len) return error.Usage;
            filter_mode = if (arg[2] == 'i') .ignore else .only;
            i += 1;
            filter_path = ctx.args[i];
        } else if (std.mem.eql(u8, arg, "--no-recurse")) {
            if (which != .@"export") return error.Usage;
            recurse = false;
        } else if (std.mem.eql(u8, arg, "--root")) {
            if (root != null or i + 1 == ctx.args.len) return error.Usage;
            i += 1;
            root = ctx.args[i];
            if (root.?.len == 0) return error.Usage;
        } else if (std.mem.startsWith(u8, arg, "-") or positional_count == positional.len) {
            return error.Usage;
        } else {
            positional[positional_count] = arg;
            positional_count += 1;
        }
    }

    const cwd = std.Io.Dir.cwd();
    switch (which) {
        .@"export" => {
            if (positional_count != 3) return error.Usage;
            if (!std.ascii.endsWithIgnoreCase(positional[2], ".tsv")) return error.Usage;
            const folder = try bk_tool.inputPath(ctx.gpa, positional[1]);
            defer ctx.gpa.free(folder);
            const out_path = try bk_tool.inputPath(ctx.gpa, positional[2]);
            defer ctx.gpa.free(out_path);

            var arena_state: std.heap.ArenaAllocator = .init(ctx.gpa);
            defer arena_state.deinit();
            const arena = arena_state.allocator();
            var filter: Filter = .{ .mode = filter_mode };
            if (filter_path) |path| {
                const host_path = try bk_tool.inputPath(arena, path);
                const text = cwd.readFileAlloc(ctx.io, host_path, arena, .limited(16 * 1024 * 1024)) catch |err| switch (err) {
                    error.FileNotFound => return ctx.failKey("table.err.no_file", .{host_path}),
                    else => return ctx.failKey("tool.err.path_reason", .{ host_path, @errorName(err) }),
                };
                filter.fields = try Filter.parseFieldList(arena, text);
            }

            const result = exportSheet(ctx.io, ctx.gpa, cwd, .{
                .mask = positional[0],
                .folder = positional[1],
                .recurse = recurse,
                .filter = filter,
                .root = root,
            }, ctx.texts, ctx.err) catch |err| switch (err) {
                error.Failed => return error.Failed,
                error.FileNotFound, error.NotDir => return ctx.failKey("table.err.no_dir", .{folder}),
                else => return err,
            };
            defer ctx.gpa.free(result.sheet);

            if (std.fs.path.dirname(out_path)) |parent| try cwd.createDirPath(ctx.io, parent);
            cwd.writeFile(ctx.io, .{ .sub_path = out_path, .data = result.sheet }) catch |err|
                return ctx.failKey("tool.err.path_reason", .{ out_path, @errorName(err) });
            try ctx.out.print("exported {d} files ({d} columns) into {s}\n", .{ result.files, result.columns, out_path });
        },
        .import => {
            if (positional_count != 1) return error.Usage;
            if (!std.ascii.endsWithIgnoreCase(positional[0], ".tsv") and !std.ascii.endsWithIgnoreCase(positional[0], ".txt"))
                return error.Usage;
            const sheet_path = try bk_tool.inputPath(ctx.gpa, positional[0]);
            defer ctx.gpa.free(sheet_path);
            const sheet = cwd.readFileAlloc(ctx.io, sheet_path, ctx.gpa, .limited(256 * 1024 * 1024)) catch |err| switch (err) {
                error.FileNotFound => return ctx.failKey("table.err.no_file", .{sheet_path}),
                else => return ctx.failKey("tool.err.path_reason", .{ sheet_path, @errorName(err) }),
            };
            defer ctx.gpa.free(sheet);

            const result = try importSheet(ctx.io, ctx.gpa, cwd, sheet, root, ctx.texts, ctx.err);
            if (result.problems > 0)
                return ctx.failKey("table.err.problems", .{ result.problems, result.cells_changed, result.files_changed, sheet_path });
            try ctx.out.print("changed {d} values in {d} files from {s}\n", .{ result.cells_changed, result.files_changed, sheet_path });
        },
    }
}

/// The legacy ExcelExporter's per-extension stat roots (`main.cpp`).
pub const root_table = [_]struct { extension: []const u8, root: []const u8 }{
    .{ .extension = ".bld", .root = "desc" },
    .{ .extension = ".obt", .root = "desc" },
    .{ .extension = ".eff", .root = "effect" },
};
pub const default_root = "RPG";

/// The stat root of `path`: `override` if given, else by extension (any case).
pub fn rootFor(path: []const u8, override: ?[]const u8) []const u8 {
    if (override) |root| return root;
    const extension = std.fs.path.extension(path);
    for (root_table) |entry| {
        if (std.ascii.eqlIgnoreCase(extension, entry.extension)) return entry.root;
    }
    return default_root;
}

pub const Filter = struct {
    pub const Mode = enum { none, ignore, only };

    mode: Mode = .none,
    fields: []const []const u8 = &.{},

    /// The first whitespace-separated word of every line; blank lines skipped.
    pub fn parseFieldList(arena: std.mem.Allocator, text: []const u8) ![]const []const u8 {
        var fields: std.ArrayList([]const u8) = .empty;
        var lines = std.mem.splitScalar(u8, text, '\n');
        while (lines.next()) |line| {
            var words = std.mem.tokenizeAny(u8, line, " \t\r\x0b\x0c");
            if (words.next()) |word| try fields.append(arena, word);
        }
        return fields.toOwnedSlice(arena);
    }

    /// Whether a sheet shows `path`. A field matches a path it is a prefix of,
    /// once `item(NN)` in the path reads `item`; `--only` with an empty list
    /// keeps everything, as the legacy tool did.
    pub fn keeps(filter: Filter, path: []const u8) bool {
        if (filter.mode == .none or (filter.mode == .only and filter.fields.len == 0)) return true;
        var buffer: [1024]u8 = undefined;
        const plain = stripItemNumbers(path, &buffer);
        const matched = for (filter.fields) |field| {
            if (std.mem.startsWith(u8, plain, field)) break true;
        } else false;
        return if (filter.mode == .only) matched else !matched;
    }

    /// `item(NN)` -> `item` for two-digit NN, exactly the legacy rule (so a
    /// 101st `item(100)` keeps its number). A path longer than `buffer` is
    /// compared as it is.
    fn stripItemNumbers(path: []const u8, buffer: []u8) []const u8 {
        if (path.len > buffer.len) return path;
        var len: usize = 0;
        var i: usize = 0;
        while (i < path.len) {
            if (std.mem.startsWith(u8, path[i..], "item(") and i + 8 <= path.len and
                std.ascii.isDigit(path[i + 5]) and std.ascii.isDigit(path[i + 6]) and path[i + 7] == ')')
            {
                @memcpy(buffer[len..][0..4], "item");
                len += 4;
                i += 8;
                continue;
            }
            buffer[len] = path[i];
            len += 1;
            i += 1;
        }
        return buffer[0..len];
    }
};

/// `name` against a `*`/`?` mask, ASCII case-insensitively as on Windows,
/// where the legacy masks were typed.
pub fn maskMatches(mask: []const u8, name: []const u8) bool {
    var m: usize = 0;
    var n: usize = 0;
    var star: ?usize = null;
    var star_n: usize = 0;
    while (n < name.len) {
        if (m < mask.len and mask[m] == '*') {
            star = m;
            m += 1;
            star_n = n;
        } else if (m < mask.len and (mask[m] == '?' or std.ascii.toLower(mask[m]) == std.ascii.toLower(name[n]))) {
            m += 1;
            n += 1;
        } else if (star) |s| {
            m = s + 1;
            star_n += 1;
            n = star_n;
        } else return false;
    }
    while (m < mask.len and mask[m] == '*') m += 1;
    return m == mask.len;
}

pub const ExportOptions = struct {
    mask: []const u8,
    /// As typed; both separators accepted. Rows name files as `folder/rel`.
    folder: []const u8,
    recurse: bool = true,
    filter: Filter = .{},
    root: ?[]const u8 = null,
};

pub const ExportResult = struct {
    files: usize,
    columns: usize,
    /// The whole sheet; caller owns it.
    sheet: []u8,
};

/// Where the export and import say what they skipped or refused: `bk-table: <reason>` lines, the
/// reason from the string table. The prefix stays English like every tool's.
const Report = struct {
    writer: *std.Io.Writer,
    texts: *bk_l10n.Set,
    arena: std.mem.Allocator,

    fn line(report: Report, comptime key: []const u8, args: anytype) std.Io.Writer.Error!void {
        try report.writer.print("bk-table: {s}\n", .{bk_tool.formatText(report.arena, report.texts, key, args)});
    }

    /// The reason a file was refused, with the `--root` hint when the root was the problem.
    fn refusal(report: Report, diag: *const xml.Diagnostic, err: anyerror) []const u8 {
        const reason = diag.render(report.arena, report.texts);
        if (err != error.RootNotFound) return reason;
        return std.fmt.allocPrint(report.arena, "{s} {s}", .{ reason, report.texts.get("table.hint.root") }) catch reason;
    }
};

/// Build the sheet for every matching file under `options.folder` (relative
/// to `base`). Every file is read before anything is returned: a file that is
/// not well-formed, lacks its root or holds a value a sheet cannot (a tab or
/// line break) is reported on `problems` and the export fails as a whole
/// (`error.Failed`), so no half sheet is written.
pub fn exportSheet(io: std.Io, gpa: std.mem.Allocator, base: std.Io.Dir, options: ExportOptions, texts: *bk_l10n.Set, problems: *std.Io.Writer) !ExportResult {
    var arena_state: std.heap.ArenaAllocator = .init(gpa);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const report: Report = .{ .writer = problems, .texts = texts, .arena = arena };

    const folder_host = try bk_tool.inputPath(arena, options.folder);
    const folder_sheet = try arena.dupe(u8, options.folder);
    std.mem.replaceScalar(u8, folder_sheet, '\\', '/');
    const folder_prefix = std.mem.trimEnd(u8, folder_sheet, "/");

    var names: std.ArrayList([]const u8) = .empty;
    {
        var dir = try base.openDir(io, folder_host, .{ .iterate = true });
        defer dir.close(io);
        if (options.recurse) {
            var walker = try dir.walk(gpa);
            defer walker.deinit();
            while (try walker.next(io)) |entry| {
                if (entry.kind != .file or !maskMatches(options.mask, entry.basename)) continue;
                const rel = try arena.dupe(u8, entry.path);
                std.mem.replaceScalar(u8, rel, std.fs.path.sep, '/');
                try names.append(arena, rel);
            }
        } else {
            var iterator = dir.iterate();
            while (try iterator.next(io)) |entry| {
                if (entry.kind != .file or !maskMatches(options.mask, entry.name)) continue;
                try names.append(arena, try arena.dupe(u8, entry.name));
            }
        }
    }
    std.mem.sort([]const u8, names.items, {}, lessThanBytes);

    if (names.items.len == 0) {
        try report.line("table.problem.no_match", .{ folder_host, options.mask });
        return error.Failed;
    }

    const Row = struct { name: []const u8, values: std.StringHashMapUnmanaged([]const u8) };
    const rows = try arena.alloc(Row, names.items.len);
    var columns: std.StringArrayHashMapUnmanaged(void) = .empty;
    var failed = false;
    for (names.items, rows) |name, *row| {
        row.* = .{ .name = try std.mem.concat(arena, u8, &.{ folder_prefix, "/", name }), .values = .empty };
        const host_path = try bk_tool.inputPath(arena, row.name);
        if (std.mem.indexOfAny(u8, row.name, "\t\r\n") != null) {
            try report.line("table.problem.name_tab", .{host_path});
            failed = true;
            continue;
        }
        const source = try base.readFileAlloc(io, host_path, arena, .limited(64 * 1024 * 1024));
        var diag: xml.Diagnostic = .{};
        const fields = xml.flatten(arena, source, rootFor(name, options.root), &diag) catch |err| switch (err) {
            error.Malformed, error.RootNotFound => {
                const at = xml.locate(source, diag.offset);
                try report.line("table.problem.xml", .{ host_path, at.line, at.column, report.refusal(&diag, err) });
                failed = true;
                continue;
            },
            else => |e| return e,
        };
        for (fields) |field| {
            if (!options.filter.keeps(field.path)) continue;
            const entry = try row.values.getOrPut(arena, field.path);
            if (entry.found_existing) continue;
            if (std.mem.indexOfAny(u8, field.value, "\t\r\n") != null) {
                const at = xml.locate(source, field.start);
                try report.line("table.problem.value_tab", .{ host_path, at.line, at.column, field.path });
                failed = true;
            }
            entry.value_ptr.* = field.value;
            try columns.put(arena, field.path, {});
        }
    }
    if (failed) return error.Failed;

    const paths = columns.keys();
    std.mem.sort([]const u8, paths, {}, lessThanBytes);

    var sheet: std.Io.Writer.Allocating = .init(gpa);
    errdefer sheet.deinit();
    const out = &sheet.writer;
    try out.writeAll("FileName");
    for (paths) |path| {
        try out.writeByte('\t');
        try out.writeAll(path);
    }
    try out.writeByte('\n');
    for (rows) |row| {
        try out.writeAll(row.name);
        for (paths) |path| {
            try out.writeByte('\t');
            try writeCell(out, row.values.get(path) orelse "");
        }
        try out.writeByte('\n');
    }
    return .{ .files = rows.len, .columns = paths.len, .sheet = try sheet.toOwnedSlice() };
}

fn lessThanBytes(_: void, lhs: []const u8, rhs: []const u8) bool {
    return std.mem.order(u8, lhs, rhs) == .lt;
}

/// A value as it is, except one starting with `"`, which a spreadsheet would
/// read as a quoted cell: that one is quoted (inner quotes doubled), the way
/// Excel writes it and `readCell` reads it back.
fn writeCell(out: *std.Io.Writer, value: []const u8) !void {
    if (value.len == 0 or value[0] != '"') return out.writeAll(value);
    try out.writeByte('"');
    for (value) |c| {
        if (c == '"') try out.writeByte('"');
        try out.writeByte(c);
    }
    try out.writeByte('"');
}

/// A cell as a spreadsheet saved it: `"..."` unquoted with `""` read as `"`.
fn readCell(arena: std.mem.Allocator, cell: []const u8) ![]const u8 {
    if (cell.len < 2 or cell[0] != '"' or cell[cell.len - 1] != '"') return cell;
    return std.mem.replaceOwned(u8, arena, cell[1 .. cell.len - 1], "\"\"", "\"");
}

fn firstInvalidUtf8(text: []const u8) ?usize {
    var i: usize = 0;
    while (i < text.len) {
        const len = std.unicode.utf8ByteSequenceLength(text[i]) catch return i;
        if (i + len > text.len) return i;
        _ = std.unicode.utf8Decode(text[i .. i + len]) catch return i;
        i += len;
    }
    return null;
}

pub const ImportResult = struct {
    rows: usize = 0,
    files_changed: usize = 0,
    cells_changed: usize = 0,
    /// Rows and cells reported on the `problems` writer and skipped.
    problems: usize = 0,
};

/// Write `sheet`'s changed cells into the files its rows name (relative to
/// `base`). Row and cell problems are reported, counted and skipped; a sheet
/// that cannot be read at all (no `FileName` header, a column named twice,
/// not UTF-8) fails with `error.Failed` before any file is touched.
pub fn importSheet(io: std.Io, gpa: std.mem.Allocator, base: std.Io.Dir, sheet: []const u8, root: ?[]const u8, texts: *bk_l10n.Set, problems: *std.Io.Writer) !ImportResult {
    var arena_state: std.heap.ArenaAllocator = .init(gpa);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const report: Report = .{ .writer = problems, .texts = texts, .arena = arena };

    if (firstInvalidUtf8(sheet)) |offset| {
        const line = std.mem.count(u8, sheet[0..offset], "\n") + 1;
        try report.line("table.problem.not_utf8", .{line});
        return error.Failed;
    }

    var lines = std.mem.splitScalar(u8, sheet, '\n');
    const header_line = std.mem.trimEnd(u8, lines.first(), "\r");
    var headers: std.ArrayList([]const u8) = .empty;
    var header_cells = std.mem.splitScalar(u8, header_line, '\t');
    while (header_cells.next()) |cell| try headers.append(arena, try readCell(arena, cell));
    if (!std.mem.eql(u8, headers.items[0], "FileName")) {
        try report.line("table.problem.no_filename", .{});
        return error.Failed;
    }
    for (headers.items[1..], 1..) |header, i| {
        for (headers.items[1..i]) |earlier| {
            if (std.mem.eql(u8, earlier, header)) {
                try report.line("table.problem.column_twice", .{header});
                return error.Failed;
            }
        }
    }

    var result: ImportResult = .{};
    var line_number: usize = 1;
    while (lines.next()) |raw_line| {
        line_number += 1;
        const line = std.mem.trimEnd(u8, raw_line, "\r");
        if (line.len == 0) continue;
        result.rows += 1;

        var cells: std.ArrayList([]const u8) = .empty;
        var it = std.mem.splitScalar(u8, line, '\t');
        while (it.next()) |cell| try cells.append(arena, try readCell(arena, cell));
        const path = try bk_tool.inputPath(arena, cells.items[0]);
        if (cells.items.len > headers.items.len) {
            try report.line("table.problem.extra_cells", .{ line_number, path });
            result.problems += 1;
            continue;
        }

        const source = base.readFileAlloc(io, path, arena, .limited(64 * 1024 * 1024)) catch |err| switch (err) {
            error.FileNotFound => {
                try report.line("table.problem.file_missing", .{path});
                result.problems += 1;
                continue;
            },
            else => |e| return e,
        };
        var diag: xml.Diagnostic = .{};
        const fields = xml.flatten(arena, source, rootFor(path, root), &diag) catch |err| switch (err) {
            error.Malformed, error.RootNotFound => {
                const at = xml.locate(source, diag.offset);
                try report.line("table.problem.xml_row_skipped", .{ path, at.line, at.column, report.refusal(&diag, err) });
                result.problems += 1;
                continue;
            },
            else => |e| return e,
        };

        var patches: std.ArrayList(xml.Patch) = .empty;
        for (cells.items[1..], headers.items[1..cells.items.len]) |cell, column| {
            if (cell.len == 0 or std.mem.eql(u8, cell, "_")) continue;
            const field = for (fields) |field| {
                if (std.mem.eql(u8, field.path, column)) break field;
            } else {
                try report.line("table.problem.field_missing", .{ path, column });
                result.problems += 1;
                continue;
            };
            if (std.mem.eql(u8, field.value, cell)) continue;
            const bytes = xml.encode(arena, field, cell) catch |err| {
                const reason = switch (err) {
                    error.CdataTerminator => texts.get("table.reason.cdata"),
                    error.CommentDashes => texts.get("table.reason.comment"),
                    error.ControlCharacter => texts.get("table.reason.control"),
                    error.OutOfMemory => return error.OutOfMemory,
                };
                try report.line("table.problem.cell_refused", .{ path, column, reason });
                result.problems += 1;
                continue;
            };
            try patches.append(arena, .{ .start = field.start, .end = field.end, .bytes = bytes });
        }
        if (patches.items.len == 0) continue;

        const patched = try xml.applyPatches(arena, source, patches.items);
        try base.writeFile(io, .{ .sub_path = path, .data = patched });
        result.files_changed += 1;
        result.cells_changed += patches.items.len;
    }
    return result;
}
