//! The `.xlsx` side of the spreadsheet exchange (D-10), the new default in
//! place of the MFC ELK's Excel ODBC route: an Office Open XML workbook is a
//! zip of XML parts, written here with `zip_writer.zig` and read with the
//! engine's own `zip.zig` (D-08), so no library comes in for it.
//!
//! The writer makes the smallest workbook Excel and LibreOffice open: the
//! content types, the package and workbook relationships, the workbook with
//! one sheet named `BlitzkriegELK`, and that sheet with every cell an inline
//! string. A text is the cell's string as it is; what XML cannot hold (a
//! control character, a lone surrogate) and a literal `_xHHHH_` are escaped
//! the way Excel escapes them, `_xHHHH_` and `_x005F_`, so every text
//! survives the round trip.
//!
//! The reader takes what Excel, LibreOffice and other tools write: the
//! workbook found through the package relationships, the sheet named
//! `BlitzkriegELK` through the workbook's, its cells as shared strings,
//! inline strings (rich runs joined, phonetic runs left out) or plain values
//! taken as their text. Element names are matched without their namespace
//! prefix. It has a scanner of its own rather than the engine's `xml.zig`,
//! which trims text and reads `&#233;` as one byte: a cell's leading spaces
//! and every character must come through unchanged.
const std = @import("std");
const zip = @import("zip");
const zip_writer = @import("zip_writer.zig");

pub const sheet_name = "BlitzkriegELK";

/// Rows of cells, a missing cell "".
pub const Grid = []const []const []const u8;

const declaration = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n";
const main_ns = "http://schemas.openxmlformats.org/spreadsheetml/2006/main";
const relationships_ns = "http://schemas.openxmlformats.org/package/2006/relationships";
const office_relationships = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";

const content_types = declaration ++
    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">" ++
    "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>" ++
    "<Default Extension=\"xml\" ContentType=\"application/xml\"/>" ++
    "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>" ++
    "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>" ++
    "</Types>";
const package_rels = declaration ++
    "<Relationships xmlns=\"" ++ relationships_ns ++ "\">" ++
    "<Relationship Id=\"rId1\" Type=\"" ++ office_relationships ++ "/officeDocument\" Target=\"xl/workbook.xml\"/>" ++
    "</Relationships>";
const workbook = declaration ++
    "<workbook xmlns=\"" ++ main_ns ++ "\" xmlns:r=\"" ++ office_relationships ++ "\">" ++
    "<sheets><sheet name=\"" ++ sheet_name ++ "\" sheetId=\"1\" r:id=\"rId1\"/></sheets></workbook>";
const workbook_rels = declaration ++
    "<Relationships xmlns=\"" ++ relationships_ns ++ "\">" ++
    "<Relationship Id=\"rId1\" Type=\"" ++ office_relationships ++ "/worksheet\" Target=\"worksheets/sheet1.xml\"/>" ++
    "</Relationships>";

/// The workbook holding `rows` (the first is the header) as its one sheet;
/// the caller owns the bytes. The texts are WTF-8, as the database reads.
pub fn write(gpa: std.mem.Allocator, rows: Grid) ![]u8 {
    var sheet: std.ArrayListUnmanaged(u8) = .empty;
    defer sheet.deinit(gpa);
    try sheet.appendSlice(gpa, declaration ++ "<worksheet xmlns=\"" ++ main_ns ++ "\"><sheetData>");
    for (rows, 1..) |row, number| {
        try sheet.print(gpa, "<row r=\"{d}\">", .{number});
        for (row, 0..) |cell, column| {
            if (cell.len == 0) continue;
            var name: [8]u8 = undefined;
            try sheet.print(gpa, "<c r=\"{s}{d}\" t=\"inlineStr\"><is><t xml:space=\"preserve\">", .{ columnName(&name, column), number });
            try appendEscaped(gpa, &sheet, cell);
            try sheet.appendSlice(gpa, "</t></is></c>");
        }
        try sheet.appendSlice(gpa, "</row>");
    }
    try sheet.appendSlice(gpa, "</sheetData></worksheet>");
    const files = [_]zip_writer.File{
        .{ .name = "[Content_Types].xml", .bytes = content_types },
        .{ .name = "_rels/.rels", .bytes = package_rels },
        .{ .name = "xl/workbook.xml", .bytes = workbook },
        .{ .name = "xl/_rels/workbook.xml.rels", .bytes = workbook_rels },
        .{ .name = "xl/worksheets/sheet1.xml", .bytes = sheet.items },
    };
    return zip_writer.write(gpa, &files);
}

/// `A` for column 0, `Z`, `AA` and on.
fn columnName(buffer: *[8]u8, column: usize) []const u8 {
    var at: usize = buffer.len;
    var rest = column + 1;
    while (rest > 0) {
        at -= 1;
        buffer[at] = @intCast('A' + (rest - 1) % 26);
        rest = (rest - 1) / 26;
    }
    return buffer[at..];
}

fn appendEscaped(gpa: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), text: []const u8) !void {
    const view = std.unicode.Wtf8View.init(text) catch return error.InvalidWtf8;
    var it = view.iterator();
    var at: usize = 0;
    while (it.nextCodepointSlice()) |slice| : (at += slice.len) {
        const code = std.unicode.wtf8Decode(slice) catch unreachable;
        switch (code) {
            '&' => try out.appendSlice(gpa, "&amp;"),
            '<' => try out.appendSlice(gpa, "&lt;"),
            '>' => try out.appendSlice(gpa, "&gt;"),
            '\t', '\n' => try out.append(gpa, @intCast(code)),
            0...0x08, 0x0B...0x1F, 0xD800...0xDFFF, 0xFFFE, 0xFFFF => try out.print(gpa, "_x{X:0>4}_", .{code}),
            '_' => try out.appendSlice(gpa, if (escapeAt(text[at..]) != null) "_x005F_" else "_"),
            else => try out.appendSlice(gpa, slice),
        }
    }
}

/// The unit an Excel escape `_xHHHH_` at the start of `text` stands for.
fn escapeAt(text: []const u8) ?u16 {
    if (text.len < 7 or text[0] != '_' or (text[1] != 'x' and text[1] != 'X') or text[6] != '_') return null;
    return std.fmt.parseInt(u16, text[2..6], 16) catch null;
}

pub const Error = error{
    /// Not a zip, or a part is missing or broken.
    InvalidXlsx,
    /// No sheet of that name: the sheet the MFC ELK's query read.
    NoBlitzkriegElkSheet,
};

/// The rows of the sheet `BlitzkriegELK`, every slice in `arena`.
pub fn read(arena: std.mem.Allocator, bytes: []const u8) !Grid {
    var archive = zip.Archive.parse(arena, bytes) catch return Error.InvalidXlsx;
    const package = try part(arena, &archive, "_rels/.rels") orelse return Error.InvalidXlsx;
    const workbook_path = try relationTarget(arena, package, "", .{ .type_suffix = "/officeDocument" }) orelse "xl/workbook.xml";
    const book = try part(arena, &archive, workbook_path) orelse return Error.InvalidXlsx;
    const folder = std.fs.path.dirnamePosix(workbook_path) orelse "";
    const rels_path = try std.fmt.allocPrint(arena, "{s}{s}_rels/{s}.rels", .{ folder, if (folder.len == 0) "" else "/", std.fs.path.basenamePosix(workbook_path) });
    const rels = try part(arena, &archive, rels_path) orelse return Error.InvalidXlsx;
    const id = try sheetRelation(book) orelse return Error.NoBlitzkriegElkSheet;
    const sheet_path = try relationTarget(arena, rels, folder, .{ .id = id }) orelse return Error.InvalidXlsx;
    const sheet = try part(arena, &archive, sheet_path) orelse return Error.InvalidXlsx;
    const strings: []const []const u8 = if (try relationTarget(arena, rels, folder, .{ .type_suffix = "/sharedStrings" })) |path|
        try sharedStrings(arena, try part(arena, &archive, path) orelse return Error.InvalidXlsx)
    else
        &.{};
    return cells(arena, sheet, strings);
}

/// The part's bytes, checked to be UTF-8 as the format requires; null when
/// the archive has none of that name.
fn part(arena: std.mem.Allocator, archive: *const zip.Archive, name: []const u8) !?[]const u8 {
    const entry = archive.find(name) orelse return null;
    const bytes = archive.extract(arena, entry) catch return Error.InvalidXlsx;
    if (!std.unicode.utf8ValidateSlice(bytes)) return Error.InvalidXlsx;
    return bytes;
}

const Wanted = union(enum) { id: []const u8, type_suffix: []const u8 };

/// The target of the first relationship with that Id or Type, as a path in
/// the archive: relative to `folder`, or from the root when it starts with `/`.
fn relationTarget(arena: std.mem.Allocator, rels: []const u8, folder: []const u8, wanted: Wanted) !?[]const u8 {
    var scanner: Scanner = .{ .bytes = rels };
    while (try scanner.next()) |token| {
        const tag = switch (token) {
            .open => |tag| tag,
            else => continue,
        };
        if (!std.mem.eql(u8, localName(tag.name), "Relationship")) continue;
        const matches = switch (wanted) {
            .id => |id| std.mem.eql(u8, try attribute(arena, tag.attributes, "Id") orelse "", id),
            .type_suffix => |suffix| std.mem.endsWith(u8, try attribute(arena, tag.attributes, "Type") orelse "", suffix),
        };
        if (!matches) continue;
        const target = try attribute(arena, tag.attributes, "Target") orelse return Error.InvalidXlsx;
        if (target.len != 0 and target[0] == '/') return target[1..];
        if (folder.len == 0) return target;
        return try std.fmt.allocPrint(arena, "{s}/{s}", .{ folder, target });
    }
    return null;
}

/// The relationship Id of the sheet `BlitzkriegELK` (any case, as the
/// Excel driver found its table).
fn sheetRelation(book: []const u8) !?[]const u8 {
    var scanner: Scanner = .{ .bytes = book };
    var buffer: [256]u8 = undefined;
    while (try scanner.next()) |token| {
        const tag = switch (token) {
            .open => |tag| tag,
            else => continue,
        };
        if (!std.mem.eql(u8, localName(tag.name), "sheet")) continue;
        var fixed = std.heap.FixedBufferAllocator.init(&buffer);
        const name = attribute(fixed.allocator(), tag.attributes, "name") catch continue orelse continue;
        if (!std.ascii.eqlIgnoreCase(name, sheet_name)) continue;
        // `r:id`, whatever the prefix; the id itself holds no entities.
        return rawAttribute(tag.attributes, "id") orelse Error.InvalidXlsx;
    }
    return null;
}

/// Each `<si>`'s text: its `<t>` runs joined, the phonetic ones left out.
fn sharedStrings(arena: std.mem.Allocator, bytes: []const u8) ![]const []const u8 {
    var strings: std.ArrayListUnmanaged([]const u8) = .empty;
    var text: std.ArrayListUnmanaged(u8) = .empty;
    var runs: Runs = .{};
    var scanner: Scanner = .{ .bytes = bytes };
    while (try scanner.next()) |token| {
        switch (token) {
            .open => |tag| {
                if (std.mem.eql(u8, localName(tag.name), "si")) text = .empty;
                if (!tag.empty) runs.open(localName(tag.name));
            },
            .close => |name| {
                runs.close(localName(name));
                if (std.mem.eql(u8, localName(name), "si")) try strings.append(arena, try unescape(arena, text.items));
            },
            .text => |raw| if (runs.inText()) try decodeText(arena, &text, raw),
            .cdata => |raw| if (runs.inText()) try appendNormalized(arena, &text, raw),
        }
    }
    return strings.items;
}

/// Whether the scanner is inside a `<t>` that is no phonetic run's.
const Runs = struct {
    t: bool = false,
    phonetic: u32 = 0,

    fn open(self: *Runs, name: []const u8) void {
        if (std.mem.eql(u8, name, "t")) self.t = true;
        if (std.mem.eql(u8, name, "rPh")) self.phonetic += 1;
    }

    fn close(self: *Runs, name: []const u8) void {
        if (std.mem.eql(u8, name, "t")) self.t = false;
        if (std.mem.eql(u8, name, "rPh")) self.phonetic -|= 1;
    }

    fn inText(self: Runs) bool {
        return self.t and self.phonetic == 0;
    }
};

const Cell = struct {
    row: usize,
    column: usize,
    kind: []const u8,
    /// The `<v>`'s text.
    value: std.ArrayListUnmanaged(u8) = .empty,
    /// An inline string's text.
    text: std.ArrayListUnmanaged(u8) = .empty,
    in_value: bool = false,
    in_inline: bool = false,
};

fn cells(arena: std.mem.Allocator, bytes: []const u8, strings: []const []const u8) !Grid {
    var rows: std.ArrayListUnmanaged(std.ArrayListUnmanaged([]const u8)) = .empty;
    var row: usize = 0;
    var next_row: usize = 0;
    var next_column: usize = 0;
    var cell: ?Cell = null;
    var runs: Runs = .{};
    var scanner: Scanner = .{ .bytes = bytes };
    while (try scanner.next()) |token| {
        switch (token) {
            .open => |tag| {
                const name = localName(tag.name);
                if (std.mem.eql(u8, name, "row")) {
                    row = if (rawAttribute(tag.attributes, "r")) |r| (std.fmt.parseInt(usize, r, 10) catch return Error.InvalidXlsx) -| 1 else next_row;
                    next_row = row + 1;
                    next_column = 0;
                } else if (std.mem.eql(u8, name, "c")) {
                    const column = if (rawAttribute(tag.attributes, "r")) |r| referenceColumn(r) orelse return Error.InvalidXlsx else next_column;
                    next_column = column + 1;
                    cell = if (tag.empty) null else .{ .row = row, .column = column, .kind = rawAttribute(tag.attributes, "t") orelse "n" };
                } else if (cell) |*open| {
                    if (std.mem.eql(u8, name, "v") and !tag.empty) open.in_value = true;
                    if (std.mem.eql(u8, name, "is") and !tag.empty) open.in_inline = true;
                    if (open.in_inline and !tag.empty) runs.open(name);
                }
            },
            .close => |raw_name| {
                const name = localName(raw_name);
                const open = if (cell) |*open| open else continue;
                if (std.mem.eql(u8, name, "v")) open.in_value = false;
                if (std.mem.eql(u8, name, "is")) open.in_inline = false;
                runs.close(name);
                if (!std.mem.eql(u8, name, "c")) continue;
                const value = try cellValue(arena, open, strings);
                while (rows.items.len <= open.row) try rows.append(arena, .empty);
                const target = &rows.items[open.row];
                while (target.items.len <= open.column) try target.append(arena, "");
                target.items[open.column] = value;
                cell = null;
            },
            .text => |raw| if (cell) |*open| {
                if (open.in_value) try decodeText(arena, &open.value, raw);
                if (open.in_inline and runs.inText()) try decodeText(arena, &open.text, raw);
            },
            .cdata => |raw| if (cell) |*open| {
                if (open.in_value) try appendNormalized(arena, &open.value, raw);
                if (open.in_inline and runs.inText()) try appendNormalized(arena, &open.text, raw);
            },
        }
    }
    const grid = try arena.alloc([]const []const u8, rows.items.len);
    for (rows.items, grid) |cells_of_row, *out| out.* = cells_of_row.items;
    return grid;
}

/// A cell's text: a shared string by its index, an inline string, or the
/// value as written (a number, a formula's string, a boolean's 0 or 1).
fn cellValue(arena: std.mem.Allocator, cell: *const Cell, strings: []const []const u8) ![]const u8 {
    if (std.mem.eql(u8, cell.kind, "s")) {
        const index = std.fmt.parseInt(usize, std.mem.trim(u8, cell.value.items, " \t\r\n"), 10) catch return Error.InvalidXlsx;
        if (index >= strings.len) return Error.InvalidXlsx;
        return strings[index];
    }
    if (std.mem.eql(u8, cell.kind, "inlineStr")) return unescape(arena, cell.text.items);
    return unescape(arena, cell.value.items);
}

/// The column of a reference such as `AB12`.
fn referenceColumn(reference: []const u8) ?usize {
    var column: usize = 0;
    var letters: usize = 0;
    for (reference) |byte| {
        if (!std.ascii.isAlphabetic(byte)) break;
        column = column * 26 + (std.ascii.toUpper(byte) - 'A' + 1);
        letters += 1;
        if (letters > 3) return null;
    }
    if (letters == 0) return null;
    return column - 1;
}

fn localName(name: []const u8) []const u8 {
    return if (std.mem.lastIndexOfScalar(u8, name, ':')) |colon| name[colon + 1 ..] else name;
}

/// XML text: CR LF and a lone CR read as LF, as an XML processor reads line
/// ends, and the five named and the numeric entities decoded. An unknown
/// entity is an error.
fn decodeText(arena: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), raw: []const u8) !void {
    var at: usize = 0;
    while (at < raw.len) {
        const byte = raw[at];
        if (byte != '&') {
            const end = std.mem.indexOfScalarPos(u8, raw, at, '&') orelse raw.len;
            try appendNormalized(arena, out, raw[at..end]);
            at = end;
            continue;
        }
        const end = std.mem.indexOfScalarPos(u8, raw, at, ';') orelse return Error.InvalidXlsx;
        const entity = raw[at + 1 .. end];
        at = end + 1;
        const named = [_]struct { []const u8, u8 }{ .{ "amp", '&' }, .{ "lt", '<' }, .{ "gt", '>' }, .{ "quot", '"' }, .{ "apos", '\'' } };
        for (named) |pair| {
            if (std.mem.eql(u8, entity, pair[0])) break try out.append(arena, pair[1]);
        } else {
            if (entity.len < 2 or entity[0] != '#') return Error.InvalidXlsx;
            const code = (if (entity[1] == 'x' or entity[1] == 'X') std.fmt.parseInt(u21, entity[2..], 16) else std.fmt.parseInt(u21, entity[1..], 10)) catch return Error.InvalidXlsx;
            var buffer: [4]u8 = undefined;
            const len = std.unicode.utf8Encode(code, &buffer) catch return Error.InvalidXlsx;
            try out.appendSlice(arena, buffer[0..len]);
        }
    }
}

fn appendNormalized(arena: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), raw: []const u8) !void {
    var at: usize = 0;
    while (at < raw.len) : (at += 1) {
        if (raw[at] != '\r') {
            try out.append(arena, raw[at]);
            continue;
        }
        try out.append(arena, '\n');
        if (at + 1 < raw.len and raw[at + 1] == '\n') at += 1;
    }
}

/// Excel's `_xHHHH_` escapes undone; a surrogate pair written as two of
/// them is one character, a lone surrogate stays one (WTF-8).
fn unescape(arena: std.mem.Allocator, text: []const u8) ![]const u8 {
    if (std.mem.indexOf(u8, text, "_x") == null and std.mem.indexOf(u8, text, "_X") == null) return arena.dupe(u8, text);
    var out: std.ArrayListUnmanaged(u8) = .empty;
    var at: usize = 0;
    while (at < text.len) {
        const unit = escapeAt(text[at..]) orelse {
            try out.append(arena, text[at]);
            at += 1;
            continue;
        };
        at += 7;
        var code: u21 = unit;
        if (std.unicode.utf16IsHighSurrogate(unit)) if (escapeAt(text[at..])) |low| if (std.unicode.utf16IsLowSurrogate(low)) {
            code = 0x10000 + ((@as(u21, unit) - 0xD800) << 10) + (low - 0xDC00);
            at += 7;
        };
        var buffer: [4]u8 = undefined;
        const len = std.unicode.wtf8Encode(code, &buffer) catch unreachable;
        try out.appendSlice(arena, buffer[0..len]);
    }
    return out.items;
}

/// The value of the attribute whose name, without its prefix, is `name`,
/// entities decoded; null when there is none.
fn attribute(arena: std.mem.Allocator, attributes: []const u8, name: []const u8) !?[]const u8 {
    const raw = rawAttribute(attributes, name) orelse return null;
    if (std.mem.indexOfScalar(u8, raw, '&') == null) return raw;
    var out: std.ArrayListUnmanaged(u8) = .empty;
    try decodeText(arena, &out, raw);
    return out.items;
}

fn rawAttribute(attributes: []const u8, name: []const u8) ?[]const u8 {
    var at: usize = 0;
    while (at < attributes.len) {
        while (at < attributes.len and std.ascii.isWhitespace(attributes[at])) at += 1;
        const start = at;
        while (at < attributes.len and attributes[at] != '=' and !std.ascii.isWhitespace(attributes[at])) at += 1;
        const found = attributes[start..at];
        while (at < attributes.len and (attributes[at] == '=' or std.ascii.isWhitespace(attributes[at]))) at += 1;
        if (at >= attributes.len) return null;
        const quote = attributes[at];
        if (quote != '"' and quote != '\'') return null;
        const end = std.mem.indexOfScalarPos(u8, attributes, at + 1, quote) orelse return null;
        if (std.mem.eql(u8, localName(found), name)) return attributes[at + 1 .. end];
        at = end + 1;
    }
    return null;
}

/// The XML of one part as tags and text, without the declaration,
/// comments, processing instructions and a DOCTYPE.
const Scanner = struct {
    bytes: []const u8,
    at: usize = 0,

    const Tag = struct { name: []const u8, attributes: []const u8, empty: bool };
    const Token = union(enum) { open: Tag, close: []const u8, text: []const u8, cdata: []const u8 };

    fn next(self: *Scanner) !?Token {
        while (self.at < self.bytes.len) {
            const rest = self.bytes[self.at..];
            if (rest[0] != '<') {
                const end = std.mem.indexOfScalarPos(u8, self.bytes, self.at, '<') orelse self.bytes.len;
                defer self.at = end;
                return .{ .text = self.bytes[self.at..end] };
            }
            if (std.mem.startsWith(u8, rest, "<?")) {
                try self.skipPast("?>");
            } else if (std.mem.startsWith(u8, rest, "<!--")) {
                try self.skipPast("-->");
            } else if (std.mem.startsWith(u8, rest, "<![CDATA[")) {
                const start = self.at + "<![CDATA[".len;
                const end = std.mem.indexOfPos(u8, self.bytes, start, "]]>") orelse return Error.InvalidXlsx;
                self.at = end + 3;
                return .{ .cdata = self.bytes[start..end] };
            } else if (std.mem.startsWith(u8, rest, "<!")) {
                try self.skipPast(">");
            } else if (std.mem.startsWith(u8, rest, "</")) {
                const end = std.mem.indexOfScalarPos(u8, self.bytes, self.at, '>') orelse return Error.InvalidXlsx;
                defer self.at = end + 1;
                return .{ .close = std.mem.trim(u8, self.bytes[self.at + 2 .. end], " \t\r\n") };
            } else {
                // The tag ends at the first `>` outside quotes.
                var end = self.at + 1;
                var quote: ?u8 = null;
                while (end < self.bytes.len) : (end += 1) {
                    const byte = self.bytes[end];
                    if (quote) |q| {
                        if (byte == q) quote = null;
                    } else if (byte == '"' or byte == '\'') {
                        quote = byte;
                    } else if (byte == '>') break;
                }
                if (end >= self.bytes.len) return Error.InvalidXlsx;
                var inner = self.bytes[self.at + 1 .. end];
                self.at = end + 1;
                const empty = inner.len != 0 and inner[inner.len - 1] == '/';
                if (empty) inner = inner[0 .. inner.len - 1];
                const name_end = std.mem.indexOfAny(u8, inner, " \t\r\n") orelse inner.len;
                if (name_end == 0) return Error.InvalidXlsx;
                return .{ .open = .{ .name = inner[0..name_end], .attributes = inner[name_end..], .empty = empty } };
            }
        }
        return null;
    }

    fn skipPast(self: *Scanner, marker: []const u8) !void {
        const found = std.mem.indexOfPos(u8, self.bytes, self.at, marker) orelse return Error.InvalidXlsx;
        self.at = found + marker.len;
    }
};

const testing = std.testing;

test "texts that need escaping come back unchanged" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const texts = [_][]const u8{
        "Path",
        "  leading and trailing  ",
        "two\nlines\n",
        "quotes \" and 'apostrophes' and `backticks`",
        "<tag> & &amp; entities",
        "a literal _x0041_ and _X00e9_ and _x12_",
        "control \x01 and tab\t",
        "\u{0422}\u{0430}\u{043d}\u{043a} \u{1F600} \u{10FFFF}",
        "lone \xed\xa0\x80 surrogate",
    };
    var row: [texts.len][]const u8 = undefined;
    @memcpy(&row, &texts);
    const rows = [_][]const []const u8{ &row, &.{ "", "B2 only" } };
    const bytes = try write(testing.allocator, &rows);
    defer testing.allocator.free(bytes);
    const grid = try read(arena, bytes);
    try testing.expectEqual(@as(usize, 2), grid.len);
    try testing.expectEqual(texts.len, grid[0].len);
    for (texts, grid[0]) |want, got| try testing.expectEqualStrings(want, got);
    try testing.expectEqual(@as(usize, 2), grid[1].len);
    try testing.expectEqualStrings("", grid[1][0]);
    try testing.expectEqualStrings("B2 only", grid[1][1]);
}

test "shared strings, rich runs, prefixes and plain values read as Excel writes them" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    // The shape Excel saves: shared strings with a rich and a phonetic run,
    // an `x:` prefix, a number, a reference past a gap, CR LF in a value.
    const files = [_]zip_writer.File{
        .{ .name = "_rels/.rels", .bytes = package_rels },
        .{ .name = "xl/workbook.xml", .bytes = "<x:workbook xmlns:x=\"" ++ main_ns ++ "\" xmlns:rel=\"" ++ office_relationships ++ "\"><x:sheets>" ++
            "<x:sheet name=\"Other\" sheetId=\"1\" rel:id=\"rId1\"/><x:sheet name=\"blitzkriegelk\" sheetId=\"2\" rel:id=\"rId2\"/></x:sheets></x:workbook>" },
        .{ .name = "xl/_rels/workbook.xml.rels", .bytes = "<Relationships xmlns=\"" ++ relationships_ns ++ "\">" ++
            "<Relationship Id=\"rId2\" Type=\"" ++ office_relationships ++ "/worksheet\" Target=\"/xl/worksheets/second.xml\"/>" ++
            "<Relationship Id=\"rId3\" Type=\"" ++ office_relationships ++ "/sharedStrings\" Target=\"sharedStrings.xml\"/>" ++
            "<Relationship Id=\"rId1\" Type=\"" ++ office_relationships ++ "/worksheet\" Target=\"worksheets/first.xml\"/></Relationships>" },
        .{ .name = "xl/sharedStrings.xml", .bytes = "<?xml version=\"1.0\"?><sst xmlns=\"" ++ main_ns ++ "\" count=\"2\"><si><t>Path</t></si>" ++
            "<si><r><rPr><b/></rPr><t xml:space=\"preserve\">Rich </t></r><r><t>run&#233;</t></r><rPh sb=\"0\" eb=\"1\"><t>PHONETIC</t></rPh></si></sst>" },
        .{ .name = "xl/worksheets/first.xml", .bytes = "<worksheet/>" },
        .{ .name = "xl/worksheets/second.xml", .bytes = "<worksheet xmlns=\"" ++ main_ns ++ "\"><sheetData>" ++
            "<row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c><c r=\"C1\" t=\"s\" s=\"1\"><v>1</v></c></row>" ++
            "<row r=\"3\"><c t=\"n\"><v>42</v></c><c t=\"str\"><f>A1</f><v>one\r\ntwo</v></c><c r=\"D3\"/></row></sheetData></worksheet>" },
    };
    const bytes = try zip_writer.write(testing.allocator, &files);
    defer testing.allocator.free(bytes);
    const grid = try read(arena, bytes);
    try testing.expectEqual(@as(usize, 3), grid.len);
    try testing.expectEqualStrings("Path", grid[0][0]);
    try testing.expectEqualStrings("", grid[0][1]);
    try testing.expectEqualStrings("Rich run\u{e9}", grid[0][2]);
    try testing.expectEqual(@as(usize, 0), grid[1].len);
    try testing.expectEqualStrings("42", grid[2][0]);
    try testing.expectEqualStrings("one\ntwo", grid[2][1]);
    try testing.expectEqual(@as(usize, 2), grid[2].len);
}

test "a workbook without the sheet or with a broken part fails" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const other = [_]zip_writer.File{
        .{ .name = "_rels/.rels", .bytes = package_rels },
        .{ .name = "xl/workbook.xml", .bytes = "<workbook><sheets><sheet name=\"Sheet1\" r:id=\"rId1\"/></sheets></workbook>" },
        .{ .name = "xl/_rels/workbook.xml.rels", .bytes = workbook_rels },
    };
    const bytes = try zip_writer.write(testing.allocator, &other);
    defer testing.allocator.free(bytes);
    try testing.expectError(Error.NoBlitzkriegElkSheet, read(arena, bytes));
    try testing.expectError(Error.InvalidXlsx, read(arena, "not a zip"));
    try testing.expectError(Error.InvalidXlsx, read(arena, bytes[0 .. bytes.len - 10]));
}
