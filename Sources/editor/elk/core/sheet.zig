//! The spreadsheet exchange (D-10): `CELK::ExportToXLS` and
//! `CELK::ImportFromXLS` from Sources/src/ELK/ELK_StaticMethods.cpp, over
//! `.xlsx` (read and write, the default; xlsx.zig), `.csv` (UTF-8 with a BOM,
//! RFC 4180, read and write; here) and the legacy `.xls` (read only;
//! xls.zig), the format picked by the file's extension. The MFC ELK went
//! through Excel's ODBC driver, which cannot be ported.
//!
//! A sheet is the table `BlitzkriegELK`: a header row naming the columns
//! `Path`, `Original`, `Translation`, `State`, `Description`, found by name
//! in any order as the driver bound them, then a row per text. `Path` is
//! `<element>\<key>` with backslashes.
//!
//! The export writes every text of every element, keys in the database's
//! order: the path, the original, the translation, the state's name and the
//! description. The MFC export trimmed trailing `\t\r\n ` off the three
//! texts and wrote an apostrophe as a backtick (the SQL quoting of its
//! INSERT); this one writes them as they are, so a sheet carries every text
//! unchanged and importing it back changes nothing (38 tracked translations
//! end in a space).
//!
//! The import, as `ImportFromXLS` did it, row by row:
//! - A row with an empty translation is skipped.
//! - The path's element and text must exist; one that does not is reported
//!   and skipped, where the MFC import wrote the files of a text the
//!   database did not have. Both separators are accepted, and element and
//!   key match without regard to case (D-05).
//! - The sheet's original and the database's, both with trailing
//!   `\t\r\n ` trimmed, are compared: when they differ the text becomes
//!   Outdated, else Translated.
//! - The translation is written only when its bytes would change; the state
//!   is written when the translation was or the originals differ, keeping
//!   `Changed` as it was. The State and Description columns are not read.
//! - Only a legacy `.xls` gets its backticks turned back into apostrophes,
//!   in the path, the original and the translation.
//! - Every element without a Version gets `<file>, [dd:mm:yyyy, hh.mm.ss]`
//!   from the sheet's modification time, as an update's import records it.
//!
//! A row that fails to write is counted and the rest go on; a sheet that
//! cannot be read fails before anything is written. Both directions take a
//! `Progress` (D-14), one unit per text or row: a cancelled export writes
//! no sheet, a cancelled import stops between two rows, the rows before it
//! imported, each file whole, and no Version set.
const std = @import("std");
const database = @import("database.zig");
const state = @import("state.zig");
const packages = @import("packages.zig");
const xlsx = @import("xlsx.zig");
const xls = @import("xls.zig");
const Progress = @import("progress.zig").Progress;

pub const Grid = xlsx.Grid;
pub const sheet_name = xlsx.sheet_name;

/// The columns, in the order the export writes them.
pub const columns = [_][]const u8{ "Path", "Original", "Translation", "State", "Description" };

/// `SELKTextProperty::STATE_NAMES`, the State column's values.
pub const state_names = [_][]const u8{ "Not translated", "Outdated", "Translated", "Approved" };

pub const Format = enum { xlsx, csv, xls };

/// The format of a sheet by its extension; null for any other.
pub fn formatOf(path: []const u8) ?Format {
    const extension = std.fs.path.extension(path);
    var lower: [4]u8 = undefined;
    if (extension.len < 2 or extension.len - 1 > lower.len) return null;
    return std.meta.stringToEnum(Format, std.ascii.lowerString(&lower, extension[1..]));
}

pub const Error = error{
    /// Not `.xlsx`, `.csv` or `.xls`.
    UnknownSpreadsheetFormat,
    /// `.xls` is read only (D-10).
    XlsIsReadOnly,
    /// The header row lacks a column the import reads.
    NoPathColumn,
    NoOriginalColumn,
    NoTranslationColumn,
    /// A `.csv` that is not UTF-8.
    CsvNotUtf8,
    /// A quoted `.csv` field that never ends.
    CsvUnterminatedQuote,
};

// ---------------------------------------------------------------- export

/// The sheet's rows for every text of `db`, the header first; the texts are
/// in `arena`.
pub fn exportGrid(arena: std.mem.Allocator, db: *database.Database, progress: ?*Progress) !Grid {
    var rows: std.ArrayListUnmanaged([]const []const u8) = .empty;
    try rows.append(arena, &columns);
    var texts: usize = 0;
    for (db.bases) |base| texts += base.keys.len;
    Progress.start(progress, texts);
    for (db.project.elements, db.bases, 0..) |element, base, index| {
        for (base.keys) |key| {
            try Progress.advance(progress, db.io);
            const row = try arena.alloc([]const u8, columns.len);
            const path = try std.mem.concat(arena, u8, &.{ element.name, "\\", key });
            std.mem.replaceScalar(u8, path, '/', '\\');
            row[0] = path;
            row[1] = try db.readOriginal(arena, index, key);
            row[2] = try db.readTranslation(arena, index, key);
            row[3] = state_names[@intFromEnum((try db.readState(index, key)).state)];
            row[4] = try db.readDescription(arena, index, key);
            try rows.append(arena, row);
        }
    }
    return rows.items;
}

/// Writes every text of `db` to the sheet at `path`, in the format its
/// extension names, through a temporary file. Returns the rows written,
/// the header not counted.
pub fn exportTo(gpa: std.mem.Allocator, io: std.Io, db: *database.Database, path: []const u8, progress: ?*Progress) !usize {
    const format = formatOf(path) orelse return Error.UnknownSpreadsheetFormat;
    if (format == .xls) return Error.XlsIsReadOnly;
    var arena_state = std.heap.ArenaAllocator.init(gpa);
    defer arena_state.deinit();
    const grid = try exportGrid(arena_state.allocator(), db, progress);
    const bytes = switch (format) {
        .xlsx => try xlsx.write(gpa, grid),
        .csv => try writeCsv(gpa, grid),
        .xls => unreachable,
    };
    defer gpa.free(bytes);
    try Progress.check(progress);
    try database.writeFileAtomic(io, gpa, path, bytes);
    return grid.len - 1;
}

// ---------------------------------------------------------------- csv

const utf8_bom = "\xEF\xBB\xBF";

/// The rows as UTF-8 with a BOM, every field quoted, CR LF between records;
/// a line end inside a text stays the text's own LF.
pub fn writeCsv(gpa: std.mem.Allocator, rows: Grid) ![]u8 {
    var out: std.ArrayListUnmanaged(u8) = .empty;
    errdefer out.deinit(gpa);
    try out.appendSlice(gpa, utf8_bom);
    for (rows) |row| {
        for (row, 0..) |cell, index| {
            if (index != 0) try out.append(gpa, ',');
            try out.append(gpa, '"');
            for (cell) |byte| {
                if (byte == '"') try out.append(gpa, '"');
                try out.append(gpa, byte);
            }
            try out.append(gpa, '"');
        }
        try out.appendSlice(gpa, "\r\n");
    }
    return out.toOwnedSlice(gpa);
}

/// RFC 4180 with comma separators, a BOM optional, CR LF or LF record ends;
/// the fields are in `arena`. The text must be UTF-8 (WTF-8, so a lone
/// surrogate an export wrote comes back).
pub fn readCsv(arena: std.mem.Allocator, bytes: []const u8) !Grid {
    const text = if (std.mem.startsWith(u8, bytes, utf8_bom)) bytes[utf8_bom.len..] else bytes;
    if (!std.unicode.wtf8ValidateSlice(text)) return Error.CsvNotUtf8;
    var rows: std.ArrayListUnmanaged([]const []const u8) = .empty;
    var row: std.ArrayListUnmanaged([]const u8) = .empty;
    var field: std.ArrayListUnmanaged(u8) = .empty;
    var at: usize = 0;
    var quoted_field = false;
    while (at < text.len) {
        const byte = text[at];
        if (byte == '"' and field.items.len == 0 and !quoted_field) {
            quoted_field = true;
            at += 1;
            while (true) {
                const quote = std.mem.indexOfScalarPos(u8, text, at, '"') orelse return Error.CsvUnterminatedQuote;
                try field.appendSlice(arena, text[at..quote]);
                at = quote + 1;
                if (at < text.len and text[at] == '"') {
                    try field.append(arena, '"');
                    at += 1;
                } else break;
            }
            continue;
        }
        if (byte == ',' or byte == '\n' or byte == '\r') {
            try row.append(arena, field.items);
            field = .empty;
            quoted_field = false;
            if (byte != ',') {
                if (byte == '\r' and at + 1 < text.len and text[at + 1] == '\n') at += 1;
                try rows.append(arena, row.items);
                row = .empty;
            }
            at += 1;
            continue;
        }
        try field.append(arena, byte);
        at += 1;
    }
    if (field.items.len != 0 or quoted_field or row.items.len != 0) {
        try row.append(arena, field.items);
        try rows.append(arena, row.items);
    }
    return rows.items;
}

// ---------------------------------------------------------------- import

/// What the import reads of a row.
pub const Row = struct {
    path: []const u8,
    original: []const u8,
    translation: []const u8,
};

pub const Sheet = struct {
    arena: std.heap.ArenaAllocator,
    rows: []const Row,
    /// The file's modification time, seconds since the epoch.
    mtime: i64,

    pub fn deinit(self: *Sheet) void {
        self.arena.deinit();
    }
};

/// The rows below the header, which is the first row holding a cell; a
/// row with no cell at all is no row. With `legacy` (an `.xls`) backticks
/// become apostrophes, the MFC export's quoting undone.
pub fn rowsOf(arena: std.mem.Allocator, grid: Grid, legacy: bool) ![]const Row {
    var first: usize = 0;
    while (first < grid.len and empty(grid[first])) first += 1;
    if (first == grid.len) return Error.NoPathColumn;
    const header = grid[first];
    const path = column(header, "Path") orelse return Error.NoPathColumn;
    const original = column(header, "Original") orelse return Error.NoOriginalColumn;
    const translation = column(header, "Translation") orelse return Error.NoTranslationColumn;
    var rows: std.ArrayListUnmanaged(Row) = .empty;
    for (grid[first + 1 ..]) |cells| {
        if (empty(cells)) continue;
        try rows.append(arena, .{
            .path = try cellText(arena, cells, path, legacy),
            .original = try cellText(arena, cells, original, legacy),
            .translation = try cellText(arena, cells, translation, legacy),
        });
    }
    return rows.items;
}

fn empty(cells: []const []const u8) bool {
    for (cells) |text| if (text.len != 0) return false;
    return true;
}

fn column(header: []const []const u8, name: []const u8) ?usize {
    for (header, 0..) |text, index| if (std.ascii.eqlIgnoreCase(std.mem.trim(u8, text, " \t"), name)) return index;
    return null;
}

fn cellText(arena: std.mem.Allocator, cells: []const []const u8, index: usize, legacy: bool) ![]const u8 {
    const text = if (index < cells.len) cells[index] else "";
    if (!legacy or std.mem.indexOfScalar(u8, text, '`') == null) return text;
    const copy = try arena.dupe(u8, text);
    std.mem.replaceScalar(u8, copy, '`', '\'');
    return copy;
}

/// The sheet at `path`, read in the format its extension names.
pub fn readFile(gpa: std.mem.Allocator, io: std.Io, path: []const u8) !Sheet {
    const format = formatOf(path) orelse return Error.UnknownSpreadsheetFormat;
    var sheet: Sheet = .{ .arena = std.heap.ArenaAllocator.init(gpa), .rows = &.{}, .mtime = 0 };
    errdefer sheet.deinit();
    const arena = sheet.arena.allocator();
    const cwd = std.Io.Dir.cwd();
    const bytes = try cwd.readFileAlloc(io, path, arena, .limited(256 << 20));
    sheet.mtime = (try cwd.statFile(io, path, .{})).mtime.toSeconds();
    const grid = switch (format) {
        .xlsx => try xlsx.read(arena, bytes),
        .csv => try readCsv(arena, bytes),
        .xls => try xls.read(arena, bytes),
    };
    sheet.rows = try rowsOf(arena, grid, format == .xls);
    return sheet;
}

pub const Report = struct {
    /// The rows below the header.
    rows: usize = 0,
    /// Skipped for an empty translation.
    empty: usize = 0,
    /// Rows whose translation or state was written.
    changed: usize = 0,
    translations: usize = 0,
    states: usize = 0,
    /// Rows whose original differs from the database's.
    outdated: usize = 0,
    /// Elements that got the sheet's Version.
    versions: usize = 0,
    /// Rows whose write failed; the first error is kept.
    failed: usize = 0,
    first_failure: ?anyerror = null,
    /// The paths of the rows naming no element or text of the database,
    /// owned by the report.
    missing: []const []const u8 = &.{},

    pub fn deinit(self: *Report, gpa: std.mem.Allocator) void {
        for (self.missing) |path| gpa.free(path);
        gpa.free(self.missing);
        self.missing = &.{};
    }
};

/// Imports the sheet at `path` into `db` and saves `elk.xml` when a Version
/// was set. The caller owns the report.
pub fn importFile(gpa: std.mem.Allocator, io: std.Io, db: *database.Database, path: []const u8, progress: ?*Progress) !Report {
    var sheet = try readFile(gpa, io, path);
    defer sheet.deinit();
    return importSheet(gpa, db, &sheet, std.fs.path.basename(path), progress);
}

/// `ImportFromXLS`'s rules (the file's doc comment) over `sheet`, read from
/// the file `file_name`.
pub fn importSheet(gpa: std.mem.Allocator, db: *database.Database, sheet: *const Sheet, file_name: []const u8, progress: ?*Progress) !Report {
    var report: Report = .{};
    var missing: std.ArrayListUnmanaged([]const u8) = .empty;
    errdefer {
        for (missing.items) |path| gpa.free(path);
        missing.deinit(gpa);
    }
    Progress.start(progress, sheet.rows.len);
    for (sheet.rows) |row| {
        try Progress.advance(progress, db.io);
        report.rows += 1;
        if (row.translation.len == 0) {
            report.empty += 1;
            continue;
        }
        const found = findText(db, row.path) orelse {
            try missing.append(gpa, try gpa.dupe(u8, row.path));
            continue;
        };
        importRow(gpa, db, found.element, found.key, row, &report) catch |err| {
            report.failed += 1;
            if (report.first_failure == null) report.first_failure = err;
        };
    }
    const version = try packages.versionString(gpa, file_name, sheet.mtime);
    defer gpa.free(version);
    for (db.project.elements) |*element| {
        if (element.version.len != 0) continue;
        element.version = try db.project.arena.allocator().dupe(u8, version);
        report.versions += 1;
    }
    if (report.versions != 0) try db.save();
    report.missing = try missing.toOwnedSlice(gpa);
    return report;
}

fn importRow(gpa: std.mem.Allocator, db: *database.Database, element: usize, key: []const u8, row: Row, report: *Report) !void {
    const trimmed = " \t\r\n";
    const on_disk = try db.readOriginal(gpa, element, key);
    defer gpa.free(on_disk);
    const differs = !std.mem.eql(u8, std.mem.trimEnd(u8, on_disk, trimmed), std.mem.trimEnd(u8, row.original, trimmed));
    if (differs) report.outdated += 1;
    const wrote = try db.writeTranslation(element, key, row.translation);
    if (wrote) report.translations += 1;
    var stated = false;
    if (wrote or differs) {
        const current = try db.readState(element, key);
        stated = try db.writeProperty(element, key, .{ .state = if (differs) .outdated else .translated, .changed = current.changed });
        if (stated) report.states += 1;
    }
    if (wrote or stated) report.changed += 1;
}

const Found = struct { element: usize, key: []const u8 };

/// The element and text a sheet path names: `<element>\<key>` or with
/// forward slashes, both matched without regard to case.
fn findText(db: *const database.Database, path: []const u8) ?Found {
    const cut = std.mem.indexOfAny(u8, path, "\\/") orelse return null;
    const name = path[0..cut];
    const element = for (db.project.elements, 0..) |candidate, index| {
        if (std.ascii.eqlIgnoreCase(candidate.name, name)) break index;
    } else return null;
    var buffer: [1024]u8 = undefined;
    const rest = path[cut + 1 ..];
    if (rest.len > buffer.len) return null;
    const key = buffer[0..rest.len];
    @memcpy(key, rest);
    std.mem.replaceScalar(u8, key, '\\', '/');
    const keys = db.bases[element].keys;
    if (std.sort.binarySearch([]const u8, keys, key, orderKey)) |index| return .{ .element = element, .key = keys[index] };
    for (keys) |candidate| if (std.ascii.eqlIgnoreCase(candidate, key)) return .{ .element = element, .key = candidate };
    return null;
}

fn orderKey(wanted: []const u8, key: []const u8) std.math.Order {
    return std.mem.order(u8, wanted, key);
}

// ---------------------------------------------------------------- tests

const testing = std.testing;
const utf16 = database.utf16;

test "the format is the extension's, in any case" {
    try testing.expectEqual(Format.xlsx, formatOf("a/texts.XLSX").?);
    try testing.expectEqual(Format.csv, formatOf("texts.csv").?);
    try testing.expectEqual(Format.xls, formatOf("C:\\ELK\\desc.xls").?);
    try testing.expectEqual(@as(?Format, null), formatOf("texts.ods"));
    try testing.expectEqual(@as(?Format, null), formatOf("texts"));
}

test "csv quotes every field and reads RFC 4180 back" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const row = [_][]const u8{ "a,b", "say \"hi\"", "two\nlines", "", "\u{1F600}" };
    const rows = [_][]const []const u8{ &columns, &row };
    const bytes = try writeCsv(testing.allocator, &rows);
    defer testing.allocator.free(bytes);
    try testing.expect(std.mem.startsWith(u8, bytes, utf8_bom ++ "\"Path\",\"Original\""));
    const grid = try readCsv(arena, bytes);
    try testing.expectEqual(@as(usize, 2), grid.len);
    for (row, grid[1]) |want, got| try testing.expectEqualStrings(want, got);

    // Unquoted fields, LF records, no BOM, no final line end.
    const plain = try readCsv(arena, "Path,Original\nx\\y,\"q\"\"\"\r\nlast,");
    try testing.expectEqual(@as(usize, 3), plain.len);
    try testing.expectEqualStrings("x\\y", plain[1][0]);
    try testing.expectEqualStrings("q\"", plain[1][1]);
    try testing.expectEqualStrings("", plain[2][1]);
    try testing.expectError(Error.CsvUnterminatedQuote, readCsv(arena, "\"open"));
    try testing.expectError(Error.CsvNotUtf8, readCsv(arena, "Path\n\xE9t\xE9\n"));
}

test "the header finds the columns in any order, and only .xls swaps backticks" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const grid = [_][]const []const u8{
        &.{},
        &.{ "translation", "State", "Path", "Original" },
        &.{ "it`s", "", "game\\a`b", "don`t" },
        &.{ "", "", "", "" },
        &.{"short row"},
    };
    const modern = try rowsOf(arena, &grid, false);
    try testing.expectEqual(@as(usize, 2), modern.len);
    try testing.expectEqualStrings("it`s", modern[0].translation);
    try testing.expectEqualStrings("game\\a`b", modern[0].path);
    try testing.expectEqualStrings("", modern[1].path);
    const legacy = try rowsOf(arena, &grid, true);
    try testing.expectEqualStrings("it's", legacy[0].translation);
    try testing.expectEqualStrings("game\\a'b", legacy[0].path);
    try testing.expectEqualStrings("don't", legacy[0].original);
    try testing.expectError(Error.NoOriginalColumn, rowsOf(arena, &.{&.{ "Path", "Translation" }}, false));
    try testing.expectError(Error.NoPathColumn, rowsOf(arena, &.{}, false));
}

test "desc.xls imports exactly the rows LibreOffice decodes from it" {
    // E-04: tools/zig/fixtures/elk/desc-libreoffice.csv is LibreOffice's
    // own decoding of Data/ELK/desc.xls (xls.zig's first test names the
    // command); every row, cell for cell. Its one apostrophe is a real one.
    const io = testing.io;
    var sheet = try readFile(testing.allocator, io, "Data/ELK/desc.xls");
    defer sheet.deinit();
    var reference = try readFile(testing.allocator, io, "tools/zig/fixtures/elk/desc-libreoffice.csv");
    defer reference.deinit();
    try testing.expectEqual(@as(usize, 153), sheet.rows.len);
    try testing.expectEqual(reference.rows.len, sheet.rows.len);
    var apostrophes: usize = 0;
    for (reference.rows, sheet.rows) |want, got| {
        try testing.expectEqualStrings(want.path, got.path);
        try testing.expectEqualStrings(want.original, got.original);
        try testing.expectEqualStrings(want.translation, got.translation);
        if (std.mem.indexOfScalar(u8, got.original, '\'') != null) apostrophes += 1;
    }
    try testing.expectEqual(@as(usize, 1), apostrophes);
}

test "a backtick in a legacy .xls reads as an apostrophe" {
    // desc.xls with the space of "Secure bridge" made a backtick in its SST,
    // as the MFC export wrote every apostrophe.
    const io = testing.io;
    const root = try database.freshScratch(io, "sheet-backtick");
    defer testing.allocator.free(root);
    const bytes = try database.readFixture(io, "Data/ELK", "desc.xls");
    defer testing.allocator.free(bytes);
    const at = std.mem.indexOf(u8, bytes, "Secure bridge").?;
    bytes[at + "Secure".len] = '`';
    try database.writeFixture(io, root, "desc.xls", bytes);
    const path = try std.fs.path.join(testing.allocator, &.{ root, "desc.xls" });
    defer testing.allocator.free(path);
    var sheet = try readFile(testing.allocator, io, path);
    defer sheet.deinit();
    try testing.expectEqualStrings("Secure'bridge", sheet.rows[0].original);
}

/// A database of four texts in the shape the import rules need.
fn ruleFixture(io: std.Io, name: []const u8) ![]u8 {
    const root = try database.freshScratch(io, name);
    errdefer testing.allocator.free(root);
    try database.writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item><Path>game</Path><Version>kept</Version></item><item><Path>maps</Path></item></Elements></base>\r\n");
    try database.writeFixture(io, root, "game_data_base/units/tank/name.elk", utf16("Tank\r\n"));
    try database.writeFixture(io, root, "game_data_base/units/tank/name.xml", "<?xml version=\"1.0\"?>\r\n<base State=\"3\" Changed=\"0\"/>\r\n");
    try database.writeFixture(io, root, "game_data_base/units/gun/name.elk", utf16("Gun"));
    try database.writeFixture(io, root, "game_data_base/units/gun/name.txt", utf16("Kanone"));
    try database.writeFixture(io, root, "game_data_base/units/gun/name.xml", "<?xml version=\"1.0\"?>\r\n<base State=\"2\" Changed=\"1\"/>\r\n");
    try database.writeFixture(io, root, "game_data_base/units/car/name.elk", utf16("Car"));
    try database.writeFixture(io, root, "game_data_base/Units/Truck/Name.elk", utf16("Truck"));
    try database.writeFixture(io, root, "maps_data_base/one.elk", utf16("One"));
    return root;
}

const rule_rows = [_]Row{
    // The same original with trailing white space, a new translation:
    // Translated, Changed kept at 0.
    .{ .path = "game\\units\\tank\\name", .original = "Tank \t", .translation = "Panzer" },
    // The same translation, but the original differs: Outdated, no text write.
    .{ .path = "game\\units\\gun\\name", .original = "Old gun", .translation = "Kanone" },
    // An empty translation is skipped.
    .{ .path = "game\\units\\car\\name", .original = "Car", .translation = "" },
    // Forward slashes and another case find the text.
    .{ .path = "GAME/units/truck/name", .original = "Truck", .translation = "Laster" },
    // No such text, no such element, no separator.
    .{ .path = "game\\units\\plane\\name", .original = "Plane", .translation = "Flugzeug" },
    .{ .path = "0\\units\\tank\\name", .original = "Tank", .translation = "Panzer" },
    .{ .path = "game", .original = "", .translation = "x" },
};

test "import rules: skip empty, Outdated on another original, report what is missing" {
    const io = testing.io;
    const root = try ruleFixture(io, "sheet-rules");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var db = try database.Database.open(testing.allocator, io, elk_path);
    defer db.deinit();
    const sheet: Sheet = .{ .arena = undefined, .rows = &rule_rows, .mtime = 1_000_000_000 };

    var report = try importSheet(testing.allocator, &db, &sheet, "texts.xlsx", null);
    defer report.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 7), report.rows);
    try testing.expectEqual(@as(usize, 1), report.empty);
    try testing.expectEqual(@as(usize, 2), report.translations);
    try testing.expectEqual(@as(usize, 3), report.states);
    try testing.expectEqual(@as(usize, 3), report.changed);
    try testing.expectEqual(@as(usize, 1), report.outdated);
    try testing.expectEqual(@as(usize, 0), report.failed);
    try testing.expectEqual(@as(usize, 3), report.missing.len);
    try testing.expectEqualStrings("game\\units\\plane\\name", report.missing[0]);

    const panzer = try db.readTranslation(testing.allocator, 0, "units/tank/name");
    defer testing.allocator.free(panzer);
    try testing.expectEqualStrings("Panzer", panzer);
    try testing.expectEqual(state.TextProperty{ .state = .translated, .changed = false }, try db.readState(0, "units/tank/name"));
    try testing.expectEqual(state.TextProperty{ .state = .outdated, .changed = true }, try db.readState(0, "units/gun/name"));
    try testing.expectEqual(state.TextProperty{}, try db.readState(0, "units/car/name"));
    try testing.expect(!database.fixtureExists(io, root, "game_data_base/units/car/name.txt"));
    try testing.expectEqual(state.TextProperty{ .state = .translated, .changed = false }, try db.readState(0, "Units/Truck/Name"));
    try testing.expect(database.fixtureExists(io, root, "game_data_base/Units/Truck/Name.txt"));
    try testing.expect(!database.fixtureExists(io, root, "game_data_base/units/plane/name.txt"));

    // The element without a Version got the sheet's, the other kept its own.
    try testing.expectEqual(@as(usize, 1), report.versions);
    var reopened = try database.Database.open(testing.allocator, io, elk_path);
    defer reopened.deinit();
    try testing.expectEqualStrings("kept", reopened.project.elements[0].version);
    try testing.expectEqualStrings("texts.xlsx, [09:09:2001, 01.46.40]", reopened.project.elements[1].version);
}

test "importing the same sheet again changes no file" {
    const io = testing.io;
    const root = try ruleFixture(io, "sheet-again");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var db = try database.Database.open(testing.allocator, io, elk_path);
    defer db.deinit();
    const sheet: Sheet = .{ .arena = undefined, .rows = &rule_rows, .mtime = 0 };
    var first = try importSheet(testing.allocator, &db, &sheet, "texts.csv", null);
    first.deinit(testing.allocator);

    var before = try snapshot(io, root);
    defer before.deinit();
    var again = try importSheet(testing.allocator, &db, &sheet, "texts.csv", null);
    defer again.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 0), again.changed);
    try testing.expectEqual(@as(usize, 0), again.versions);
    try testing.expectEqual(@as(usize, 3), again.missing.len);
    var after = try snapshot(io, root);
    defer after.deinit();
    try expectSameFiles(&before, &after);
}

test "a cancelled import stops between two rows, each file whole and no Version set" {
    // D-14: the worker is held before the second row, as the scripted runs
    // hold it, and cancelled there. The first row is imported, nothing after
    // it is, elk.xml is not saved, and no temporary file is left.
    const io = testing.io;
    const root = try ruleFixture(io, "sheet-cancel");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var db = try database.Database.open(testing.allocator, io, elk_path);
    defer db.deinit();
    var before = try snapshot(io, root);
    defer before.deinit();
    const sheet: Sheet = .{ .arena = undefined, .rows = &rule_rows, .mtime = 1_000_000_000 };

    const Worker = struct {
        fn run(target: *database.Database, rows: *const Sheet, progress: *Progress, result: *?anyerror) void {
            var report = importSheet(testing.allocator, target, rows, "texts.xlsx", progress) catch |err| {
                result.* = err;
                return;
            };
            report.deinit(testing.allocator);
        }
    };
    var progress: Progress = .{};
    progress.hold.store(1, .release);
    var result: ?anyerror = null;
    const thread = try std.Thread.spawn(.{}, Worker.run, .{ &db, &sheet, &progress, &result });
    while (!progress.held.load(.acquire)) try io.sleep(.fromMilliseconds(1), .awake);
    progress.cancel();
    thread.join();
    try testing.expectEqual(@as(?anyerror, error.Cancelled), result);
    try testing.expectEqual(@as(usize, 1), progress.position.load(.acquire));

    var after = try snapshot(io, root);
    defer after.deinit();
    var it = after.files.iterator();
    while (it.next()) |entry| {
        const name = entry.key_ptr.*;
        try testing.expect(!std.mem.endsWith(u8, name, database.temp_suffix));
        // Only the first row's text changed: its translation and its state.
        if (std.mem.indexOf(u8, name, "tank") != null and !std.mem.endsWith(u8, name, ".elk")) continue;
        try testing.expectEqualSlices(u8, before.files.get(name) orelse return error.FileAdded, entry.value_ptr.*);
    }
    // The tank had no translation file; the import made one.
    try testing.expectEqual(before.files.count() + 1, after.files.count());
    const panzer = try db.readTranslation(testing.allocator, 0, "units/tank/name");
    defer testing.allocator.free(panzer);
    try testing.expectEqualStrings("Panzer", panzer);
    try testing.expectEqual(state.TextProperty{ .state = .translated, .changed = false }, try db.readState(0, "units/tank/name"));
}

/// Every file below `root` and its bytes.
const Snapshot = struct {
    arena: std.heap.ArenaAllocator,
    files: std.StringArrayHashMapUnmanaged([]const u8) = .empty,

    fn deinit(self: *Snapshot) void {
        self.arena.deinit();
    }
};

fn snapshot(io: std.Io, root: []const u8) !Snapshot {
    var result: Snapshot = .{ .arena = std.heap.ArenaAllocator.init(testing.allocator) };
    errdefer result.deinit();
    const arena = result.arena.allocator();
    var dir = try std.Io.Dir.cwd().openDir(io, root, .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(testing.allocator);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        try result.files.put(arena, try arena.dupe(u8, entry.path), try dir.readFileAlloc(io, entry.path, arena, .limited(16 << 20)));
    }
    return result;
}

fn expectSameFiles(before: *const Snapshot, after: *const Snapshot) !void {
    try testing.expectEqual(before.files.count(), after.files.count());
    var it = before.files.iterator();
    while (it.next()) |entry| try testing.expectEqualSlices(u8, entry.value_ptr.*, after.files.get(entry.key_ptr.*) orelse return error.FileVanished);
}

test "every text of Data/ELK round-trips through .xlsx and .csv, and importing it back changes nothing" {
    // E-04 on the tracked database: the export reads Data/ELK (never writes
    // it), each sheet reads back to the same texts, and imported into a copy
    // of the database it writes no file.
    const io = testing.io;
    const root = try database.freshScratch(io, "sheet-round-trip");
    defer testing.allocator.free(root);
    const copy = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base" });
    defer testing.allocator.free(copy);
    try database.copyTree(io, "Data/ELK/game_data_base", copy);
    const elk_xml = try database.readFixture(io, "Data/ELK", "elk.xml");
    defer testing.allocator.free(elk_xml);
    try database.writeFixture(io, root, "elk.xml", elk_xml);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);

    var db = try database.Database.open(testing.allocator, io, elk_path);
    defer db.deinit();
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const grid = try exportGrid(arena_state.allocator(), &db, null);
    try testing.expectEqual(@as(usize, 3_040 + 1), grid.len);
    var before = try snapshot(io, root);
    defer before.deinit();

    for ([_][]const u8{ "texts.xlsx", "texts.csv" }) |name| {
        const path = try std.fs.path.join(testing.allocator, &.{ root, "..", name });
        defer testing.allocator.free(path);
        try testing.expectEqual(@as(usize, 3_040), try exportTo(testing.allocator, io, &db, path, null));
        var sheet = try readFile(testing.allocator, io, path);
        defer sheet.deinit();
        try testing.expectEqual(grid.len - 1, sheet.rows.len);
        var lines: usize = 0;
        var quotes: usize = 0;
        for (grid[1..], sheet.rows) |want, got| {
            try testing.expectEqualStrings(want[0], got.path);
            try testing.expectEqualStrings(want[1], got.original);
            try testing.expectEqualStrings(want[2], got.translation);
            if (std.mem.indexOfScalar(u8, got.original, '\n') != null) lines += 1;
            if (std.mem.indexOfScalar(u8, got.translation, '"') != null) quotes += 1;
        }
        try testing.expect(lines > 100 and quotes > 10);

        var report = try importSheet(testing.allocator, &db, &sheet, name, null);
        defer report.deinit(testing.allocator);
        try testing.expectEqual(@as(usize, 0), report.changed);
        try testing.expectEqual(@as(usize, 0), report.missing.len);
        try testing.expectEqual(@as(usize, 0), report.failed);
        try testing.expect(report.empty > 1_000);
    }
    var after = try snapshot(io, root);
    defer after.deinit();
    try expectSameFiles(&before, &after);
    try testing.expectError(Error.XlsIsReadOnly, exportTo(testing.allocator, io, &db, "texts.xls", null));
    try testing.expectError(Error.UnknownSpreadsheetFormat, exportTo(testing.allocator, io, &db, "texts.ods", null));
}

test "texts beyond the tracked ones round-trip through both formats" {
    // What Data/ELK lacks: a character beyond the BMP, a lone surrogate, a
    // control character, a backtick and a literal Excel escape.
    const io = testing.io;
    const root = try database.freshScratch(io, "sheet-odd-texts");
    defer testing.allocator.free(root);
    try database.writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item><Path>game</Path></item></Elements></base>\r\n");
    try database.writeFixture(io, root, "game_data_base/a.elk", utf16("Original"));
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var db = try database.Database.open(testing.allocator, io, elk_path);
    defer db.deinit();
    const odd = "\u{1F600} \xed\xa0\x80 \x01 it`s _x0041_ \"q\"\n  indented";
    try testing.expect(try db.writeTranslation(0, "a", odd));
    for ([_][]const u8{ "odd.xlsx", "odd.csv" }) |name| {
        const path = try std.fs.path.join(testing.allocator, &.{ root, name });
        defer testing.allocator.free(path);
        try testing.expectEqual(@as(usize, 1), try exportTo(testing.allocator, io, &db, path, null));
        var sheet = try readFile(testing.allocator, io, path);
        defer sheet.deinit();
        try testing.expectEqualStrings("game\\a", sheet.rows[0].path);
        try testing.expectEqualStrings(odd, sheet.rows[0].translation);
        var report = try importSheet(testing.allocator, &db, &sheet, name, null);
        defer report.deinit(testing.allocator);
        try testing.expectEqual(@as(usize, 0), report.changed);
    }
}
