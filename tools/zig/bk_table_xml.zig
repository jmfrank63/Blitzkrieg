//! The XML reader and patcher behind `bk-table` (Phase 8, D-09, D-11). Zig's
//! std has no XML parser, and the legacy ExcelExporter's MSXML is Windows only
//! and re-serialised every file it touched, so this is a small one written for
//! exactly two jobs:
//!
//! - `flatten`: read a stat file and list its fields the way ExcelExporter's
//!   `CXMLReader::ReadInformation` did (below), each with the byte span its
//!   value was read from.
//! - `encode` + `applyPatches`: replace the bytes of those spans and nothing
//!   else, so comments, ordering, quoting and whitespace of everything a sheet
//!   did not change survive byte for byte.
//!
//! It is strict rather than forgiving. It accepts what the game's own data
//! files use, UTF-8 or ASCII (an optional UTF-8 BOM, an optional XML
//! declaration), elements, attributes in either quote, comments, CDATA, the
//! five predefined entities and character references; anything else (a
//! DOCTYPE, a processing instruction, another encoding, an unknown entity,
//! malformed markup) fails with the line and column where it went wrong, so a
//! modder fixes the file instead of the tool guessing what was meant.
//!
//! The flattening rules, from ExcelExporter's `XmlReader.cpp`: the stat root
//! (`RPG`, or `desc`/`effect`, see `bk_table.zig`) is the first child element
//! of the document element with that name. Below it, an element's attributes
//! are `<prefix><name>`, its non-empty text, CDATA and comment children are
//! `<prefix>#text`, `<prefix>#cdata-section` and `<prefix>#comment` (MSXML's
//! node names), and a child element adds `<name>;` to the prefix, where the
//! n-th child element called `item` is named `item(NN)` (`%.2d`, so `item(00)`
//! up to `item(99)`, then `item(100)`). The root's own prefix is empty. Text is
//! trimmed of surrounding whitespace and whitespace-only text is no field, as
//! MSXML's default whitespace handling did. Two fields can share a path (two
//! siblings with one name); the first is the one a sheet sees, as before.

const std = @import("std");
const bk_tool = @import("bk_tool.zig");
const bk_l10n = @import("bk_l10n.zig");

pub const Kind = enum { attribute, text, cdata, comment };

pub const Field = struct {
    /// The column name, e.g. `Guns;item(01);Weapon` or `KeyName;#text`.
    path: []const u8,
    /// The value with entities and character references decoded.
    value: []const u8,
    /// The raw bytes `value` was read from: inside an attribute's quotes, the
    /// trimmed text, the inside of a CDATA section or comment.
    start: usize,
    end: usize,
    kind: Kind,
    /// The attribute's quote character, `"` or `'`; 0 for the other kinds.
    quote: u8 = 0,
};

/// Where and why a file was refused. `offset` is a byte offset into the
/// source; `locate` turns it into the line and column a text editor shows.
pub const Diagnostic = struct {
    offset: usize = 0,
    /// The string-table key of the reason and its arguments (the table fills `{0}`..`{2}`), so the
    /// reason can be said in the run's language; see `render`.
    key: []const u8 = "",
    args: [max_args][]const u8 = @splat(""),
    arg_count: usize = 0,
    storage: [256]u8 = undefined,

    const max_args = 3;

    fn set(diag: *Diagnostic, offset: usize, comptime key: []const u8, args: anytype) void {
        bk_tool.requireKey(key);
        diag.offset = offset;
        diag.key = key;
        diag.arg_count = args.len;
        var used: usize = 0;
        inline for (args, 0..) |arg, i| {
            const text = switch (@typeInfo(@TypeOf(arg))) {
                .int, .comptime_int => std.fmt.bufPrint(diag.storage[used..], "{d}", .{arg}) catch "",
                else => arg,
            };
            // A name longer than the room left is cut: a reason that names half a tag still points at the line.
            const room = @min(text.len, diag.storage.len - used);
            if (@typeInfo(@TypeOf(arg)) == .int or @typeInfo(@TypeOf(arg)) == .comptime_int) {
                diag.args[i] = text;
                used += text.len;
            } else {
                @memcpy(diag.storage[used..][0..room], text[0..room]);
                diag.args[i] = diag.storage[used..][0..room];
                used += room;
            }
        }
    }

    /// The reason in the language of `texts`, into `arena`.
    pub fn render(diag: *const Diagnostic, arena: std.mem.Allocator, texts: *bk_l10n.Set) []const u8 {
        const template = texts.get(diag.key);
        const buffer = arena.alloc(u8, template.len + 3 * diag.storage.len) catch return template;
        return bk_l10n.expand(buffer, template, diag.args[0..diag.arg_count]) catch template;
    }
};

pub const Location = struct { line: usize, column: usize };

/// The 1-based line and column of `offset`. Columns count characters (UTF-8
/// sequences), not bytes, and a leading BOM is not one, so the numbers match
/// an editor's.
pub fn locate(source: []const u8, offset: usize) Location {
    const before = source[0..@min(offset, source.len)];
    const line_start = if (std.mem.lastIndexOfScalar(u8, before, '\n')) |i| i + 1 else 0;
    var column: usize = 1;
    for (before[line_start..]) |c| {
        if (c & 0xC0 != 0x80) column += 1;
    }
    if (line_start == 0 and std.mem.startsWith(u8, source, bom) and offset >= bom.len) column -= 1;
    return .{ .line = std.mem.count(u8, before, "\n") + 1, .column = column };
}

pub const Error = error{ Malformed, RootNotFound } || std.mem.Allocator.Error;

/// Two upper-case hex digits of a byte, for the control-character reason.
fn hex(c: u8) [2]u8 {
    var out: [2]u8 = undefined;
    _ = std.fmt.bufPrint(&out, "{X:0>2}", .{c}) catch unreachable;
    return out;
}

const bom = "\xEF\xBB\xBF";
const max_depth = 256;

/// Parse `source` and return the fields under its `root_name` element.
/// Everything returned is allocated in `arena` or points into `source`. On
/// `error.Malformed` or `error.RootNotFound`, `diag` says where and why.
pub fn flatten(arena: std.mem.Allocator, source: []const u8, root_name: []const u8, diag: *Diagnostic) Error![]Field {
    var parser: Parser = .{ .src = source, .arena = arena, .diag = diag, .root_name = root_name };
    try parser.parseDocument();
    return parser.fields.toOwnedSlice(arena);
}

const Mode = enum {
    /// The document element: its children are searched for the root.
    document,
    /// A child of the document element: it is the root if its name matches.
    root_search,
    /// The root or below it: fields are collected with the current prefix.
    collect,
    /// Anything else: checked, not collected.
    skip,
};

const Parser = struct {
    src: []const u8,
    pos: usize = 0,
    arena: std.mem.Allocator,
    diag: *Diagnostic,
    root_name: []const u8,
    root_found: bool = false,
    fields: std.ArrayList(Field) = .empty,
    /// The current element's path prefix, `Guns;item(01);` (empty at the root).
    prefix: std.ArrayList(u8) = .empty,

    fn fail(p: *Parser, offset: usize, comptime key: []const u8, args: anytype) error{Malformed} {
        p.diag.set(offset, key, args);
        return error.Malformed;
    }

    fn rest(p: *const Parser) []const u8 {
        return p.src[p.pos..];
    }

    fn startsWith(p: *const Parser, text: []const u8) bool {
        return std.mem.startsWith(u8, p.rest(), text);
    }

    fn skipWhitespace(p: *Parser) bool {
        const start = p.pos;
        while (p.pos < p.src.len and isWhitespace(p.src[p.pos])) p.pos += 1;
        return p.pos != start;
    }

    fn parseDocument(p: *Parser) Error!void {
        if (std.mem.startsWith(u8, p.src, "\xFF\xFE") or std.mem.startsWith(u8, p.src, "\xFE\xFF"))
            return p.fail(0, "table.xml.utf16", .{});
        if (std.mem.startsWith(u8, p.src, bom)) p.pos = bom.len;
        try p.checkCharacters();
        if (p.startsWith("<?xml") and p.pos + 5 < p.src.len and (isWhitespace(p.src[p.pos + 5]) or p.src[p.pos + 5] == '?'))
            try p.parseDeclaration();
        try p.skipMisc();
        if (p.pos >= p.src.len or p.src[p.pos] != '<')
            return p.fail(p.pos, "table.xml.expected_document", .{});
        const document_start = p.pos;
        try p.parseElement(0, .document);
        try p.skipMisc();
        if (p.pos < p.src.len) return p.fail(p.pos, "table.xml.trailing_content", .{});
        if (!p.root_found) {
            p.diag.set(document_start, "table.xml.no_root", .{p.root_name});
            return error.RootNotFound;
        }
    }

    /// The whole file is valid UTF-8 and holds no control character XML
    /// forbids, checked once up front so later errors can assume both.
    fn checkCharacters(p: *Parser) error{Malformed}!void {
        var i = p.pos;
        while (i < p.src.len) {
            const c = p.src[i];
            if (c < 0x80) {
                if (c < 0x20 and c != '\t' and c != '\n' and c != '\r')
                    return p.fail(i, "table.xml.control_char", .{hex(c)});
                i += 1;
                continue;
            }
            const len = std.unicode.utf8ByteSequenceLength(c) catch return p.fail(i, "table.xml.invalid_utf8", .{});
            if (i + len > p.src.len) return p.fail(i, "table.xml.invalid_utf8", .{});
            _ = std.unicode.utf8Decode(p.src[i .. i + len]) catch return p.fail(i, "table.xml.invalid_utf8", .{});
            i += len;
        }
    }

    /// `<?xml version="1.0" encoding="..." standalone="..."?>`; the encoding,
    /// if given, must be one this reader decodes.
    fn parseDeclaration(p: *Parser) error{Malformed}!void {
        p.pos += "<?xml".len;
        while (true) {
            const had_whitespace = p.skipWhitespace();
            if (p.startsWith("?>")) {
                p.pos += 2;
                return;
            }
            if (p.pos >= p.src.len) return p.fail(p.pos, "table.xml.decl_unterminated", .{});
            if (!had_whitespace) return p.fail(p.pos, "table.xml.decl_whitespace", .{});
            const name_start = p.pos;
            const name = try p.parseName();
            _ = p.skipWhitespace();
            if (!p.startsWith("=")) return p.fail(p.pos, "table.xml.decl_equals", .{name});
            p.pos += 1;
            _ = p.skipWhitespace();
            const value_start = p.pos + 1;
            const value = try p.parseDeclarationValue();
            if (std.mem.eql(u8, name, "version")) {
                if (!std.mem.eql(u8, value, "1.0")) return p.fail(value_start, "table.xml.decl_version", .{value});
            } else if (std.mem.eql(u8, name, "encoding")) {
                if (!std.ascii.eqlIgnoreCase(value, "UTF-8") and !std.ascii.eqlIgnoreCase(value, "US-ASCII") and !std.ascii.eqlIgnoreCase(value, "ASCII"))
                    return p.fail(value_start, "table.xml.decl_encoding", .{value});
            } else if (std.mem.eql(u8, name, "standalone")) {
                if (!std.mem.eql(u8, value, "yes") and !std.mem.eql(u8, value, "no"))
                    return p.fail(value_start, "table.xml.decl_standalone", .{});
            } else {
                return p.fail(name_start, "table.xml.decl_unexpected", .{name});
            }
        }
    }

    fn parseDeclarationValue(p: *Parser) error{Malformed}![]const u8 {
        if (p.pos >= p.src.len or (p.src[p.pos] != '"' and p.src[p.pos] != '\''))
            return p.fail(p.pos, "table.xml.decl_quote", .{});
        const quote = p.src[p.pos];
        const start = p.pos + 1;
        const end = std.mem.indexOfScalarPos(u8, p.src, start, quote) orelse
            return p.fail(p.pos, "table.xml.decl_value_open", .{});
        p.pos = end + 1;
        return p.src[start..end];
    }

    /// Whitespace and comments before or after the document element.
    fn skipMisc(p: *Parser) error{Malformed}!void {
        while (true) {
            _ = p.skipWhitespace();
            if (p.startsWith("<!--")) {
                _ = try p.parseComment();
            } else if (p.startsWith("<?xml")) {
                return p.fail(p.pos, "table.xml.decl_position", .{});
            } else if (p.startsWith("<?")) {
                return p.fail(p.pos, "table.xml.pi", .{});
            } else if (p.startsWith("<!DOCTYPE")) {
                return p.fail(p.pos, "table.xml.doctype", .{});
            } else return;
        }
    }

    /// At `<!--`: returns the content span and leaves `pos` after `-->`.
    fn parseComment(p: *Parser) error{Malformed}![2]usize {
        const start = p.pos;
        const content_start = p.pos + "<!--".len;
        const dashes = std.mem.indexOfPos(u8, p.src, content_start, "--") orelse
            return p.fail(start, "table.xml.comment_open", .{});
        if (dashes + 2 >= p.src.len or p.src[dashes + 2] != '>') {
            if (std.mem.indexOfPos(u8, p.src, content_start, "-->") == null)
                return p.fail(start, "table.xml.comment_open", .{});
            return p.fail(dashes, "table.xml.comment_dashes", .{});
        }
        p.pos = dashes + 3;
        return .{ content_start, dashes };
    }

    fn parseName(p: *Parser) error{Malformed}![]const u8 {
        const start = p.pos;
        if (p.pos >= p.src.len or !isNameStart(p.src[p.pos])) return p.fail(p.pos, "table.xml.expected_name", .{});
        p.pos += 1;
        while (p.pos < p.src.len and isNameChar(p.src[p.pos])) p.pos += 1;
        return p.src[start..p.pos];
    }

    fn addField(p: *Parser, suffix: []const u8, value: []const u8, start: usize, end: usize, kind: Kind, quote: u8) Error!void {
        const path = try std.mem.concat(p.arena, u8, &.{ p.prefix.items, suffix });
        try p.fields.append(p.arena, .{ .path = path, .value = value, .start = start, .end = end, .kind = kind, .quote = quote });
    }

    /// At `<`: an element, its attributes and its content up to the matching
    /// end tag. In `.collect` mode `prefix` already holds this element's prefix.
    fn parseElement(p: *Parser, depth: usize, mode_in: Mode) Error!void {
        const start = p.pos;
        if (depth >= max_depth) return p.fail(start, "table.xml.too_deep", .{max_depth});
        p.pos += 1;
        const name = try p.parseName();

        var mode = mode_in;
        if (mode == .root_search) {
            if (!p.root_found and std.mem.eql(u8, name, p.root_name)) {
                p.root_found = true;
                p.prefix.clearRetainingCapacity();
                mode = .collect;
            } else mode = .skip;
        }

        var attribute_names: std.ArrayList([]const u8) = .empty;
        while (true) {
            const had_whitespace = p.skipWhitespace();
            if (p.startsWith("/>")) {
                p.pos += 2;
                return;
            }
            if (p.startsWith(">")) {
                p.pos += 1;
                break;
            }
            if (p.pos >= p.src.len) return p.fail(start, "table.xml.never_closed", .{name});
            if (!had_whitespace) return p.fail(p.pos, "table.xml.tag_whitespace", .{name});

            const attribute_start = p.pos;
            const attribute = try p.parseName();
            for (attribute_names.items) |seen| {
                if (std.mem.eql(u8, seen, attribute)) return p.fail(attribute_start, "table.xml.attr_twice", .{attribute});
            }
            try attribute_names.append(p.arena, attribute);
            _ = p.skipWhitespace();
            if (!p.startsWith("=")) return p.fail(p.pos, "table.xml.attr_equals", .{attribute});
            p.pos += 1;
            _ = p.skipWhitespace();
            if (p.pos >= p.src.len or (p.src[p.pos] != '"' and p.src[p.pos] != '\''))
                return p.fail(p.pos, "table.xml.attr_quote", .{attribute});
            const quote = p.src[p.pos];
            const value_start = p.pos + 1;
            var i = value_start;
            while (i < p.src.len and p.src[i] != quote) : (i += 1) {
                if (p.src[i] == '<') return p.fail(i, "table.xml.attr_lt", .{});
            }
            if (i >= p.src.len) return p.fail(p.pos, "table.xml.attr_open", .{attribute});
            const value = try p.decode(value_start, i);
            if (mode == .collect) try p.addField(attribute, value, value_start, i, .attribute, quote);
            p.pos = i + 1;
        }

        const child_mode: Mode = switch (mode) {
            .document => .root_search,
            .collect => .collect,
            .root_search, .skip => .skip,
        };
        var item_index: usize = 0;
        while (true) {
            if (p.pos >= p.src.len) return p.fail(start, "table.xml.never_closed", .{name});
            if (p.startsWith("</")) {
                p.pos += 2;
                const end_name_start = p.pos;
                const end_name = try p.parseName();
                if (!std.mem.eql(u8, end_name, name))
                    return p.fail(end_name_start, "table.xml.end_mismatch", .{ name, end_name });
                _ = p.skipWhitespace();
                if (!p.startsWith(">")) return p.fail(p.pos, "table.xml.end_gt", .{name});
                p.pos += 1;
                return;
            } else if (p.startsWith("<!--")) {
                const span = try p.parseComment();
                if (mode == .collect and span[1] > span[0])
                    try p.addField("#comment", p.src[span[0]..span[1]], span[0], span[1], .comment, 0);
            } else if (p.startsWith("<![CDATA[")) {
                const content_start = p.pos + "<![CDATA[".len;
                const end = std.mem.indexOfPos(u8, p.src, content_start, "]]>") orelse
                    return p.fail(p.pos, "table.xml.cdata_open", .{});
                if (mode == .collect and end > content_start)
                    try p.addField("#cdata-section", p.src[content_start..end], content_start, end, .cdata, 0);
                p.pos = end + 3;
            } else if (p.startsWith("<?")) {
                return p.fail(p.pos, "table.xml.pi", .{});
            } else if (p.startsWith("<!")) {
                return p.fail(p.pos, "table.xml.bang", .{});
            } else if (p.src[p.pos] == '<') {
                if (child_mode != .collect) {
                    try p.parseElement(depth + 1, child_mode);
                    continue;
                }
                const saved = p.prefix.items.len;
                const child_name = peekName(p.src, p.pos + 1);
                if (std.mem.eql(u8, child_name, "item")) {
                    try p.prefix.print(p.arena, "item({d:0>2});", .{item_index});
                    item_index += 1;
                } else {
                    try p.prefix.appendSlice(p.arena, child_name);
                    try p.prefix.append(p.arena, ';');
                }
                try p.parseElement(depth + 1, .collect);
                p.prefix.shrinkRetainingCapacity(saved);
            } else {
                const text_start = p.pos;
                const text_end = std.mem.indexOfScalarPos(u8, p.src, p.pos, '<') orelse p.src.len;
                if (std.mem.indexOf(u8, p.src[text_start..text_end], "]]>")) |i|
                    return p.fail(text_start + i, "table.xml.cdata_end_in_text", .{});
                var trimmed_start = text_start;
                var trimmed_end = text_end;
                while (trimmed_start < trimmed_end and isWhitespace(p.src[trimmed_start])) trimmed_start += 1;
                while (trimmed_end > trimmed_start and isWhitespace(p.src[trimmed_end - 1])) trimmed_end -= 1;
                if (trimmed_end > trimmed_start) {
                    const value = try p.decode(trimmed_start, trimmed_end);
                    if (mode == .collect) try p.addField("#text", value, trimmed_start, trimmed_end, .text, 0);
                }
                p.pos = text_end;
            }
        }
    }

    /// The text of `src[start..end]` with entities and character references
    /// replaced; the source slice itself when there are none.
    fn decode(p: *Parser, start: usize, end: usize) Error![]const u8 {
        const raw = p.src[start..end];
        if (std.mem.indexOfScalar(u8, raw, '&') == null) return raw;
        var out: std.ArrayList(u8) = .empty;
        var i = start;
        while (i < end) {
            if (p.src[i] != '&') {
                try out.append(p.arena, p.src[i]);
                i += 1;
                continue;
            }
            const semicolon = std.mem.indexOfScalarPos(u8, p.src[0..end], i, ';') orelse
                return p.fail(i, "table.xml.amp", .{});
            const reference = p.src[i + 1 .. semicolon];
            if (reference.len > 0 and reference[0] == '#') {
                const digits = reference[1..];
                const code = (if (digits.len > 1 and digits[0] == 'x')
                    std.fmt.parseInt(u21, digits[1..], 16)
                else
                    std.fmt.parseInt(u21, digits, 10)) catch
                    return p.fail(i, "table.xml.char_ref", .{reference});
                if (!isXmlChar(code)) return p.fail(i, "table.xml.char_ref", .{reference});
                var buffer: [4]u8 = undefined;
                const len = std.unicode.utf8Encode(code, &buffer) catch unreachable;
                try out.appendSlice(p.arena, buffer[0..len]);
            } else {
                const replacement: u8 = if (std.mem.eql(u8, reference, "lt"))
                    '<'
                else if (std.mem.eql(u8, reference, "gt"))
                    '>'
                else if (std.mem.eql(u8, reference, "amp"))
                    '&'
                else if (std.mem.eql(u8, reference, "quot"))
                    '"'
                else if (std.mem.eql(u8, reference, "apos"))
                    '\''
                else if (reference.len == 0 or !isNameStart(reference[0]))
                    return p.fail(i, "table.xml.amp", .{})
                else
                    return p.fail(i, "table.xml.entity", .{reference});
                try out.append(p.arena, replacement);
            }
            i = semicolon + 1;
        }
        return out.items;
    }
};

/// The name starting at `pos`, or "" if there is none (the caller then parses
/// the element and reports the error properly).
fn peekName(src: []const u8, pos: usize) []const u8 {
    if (pos >= src.len or !isNameStart(src[pos])) return "";
    var end = pos + 1;
    while (end < src.len and isNameChar(src[end])) end += 1;
    return src[pos..end];
}

fn isWhitespace(c: u8) bool {
    return c == ' ' or c == '\t' or c == '\r' or c == '\n';
}

/// ASCII by the XML rules; any byte of a multi-byte UTF-8 sequence is
/// accepted, which covers the non-ASCII name characters without a table.
fn isNameStart(c: u8) bool {
    return std.ascii.isAlphabetic(c) or c == '_' or c == ':' or c >= 0x80;
}

fn isNameChar(c: u8) bool {
    return isNameStart(c) or std.ascii.isDigit(c) or c == '-' or c == '.';
}

fn isXmlChar(code: u21) bool {
    return code == 0x9 or code == 0xA or code == 0xD or
        (code >= 0x20 and code <= 0xD7FF) or
        (code >= 0xE000 and code <= 0xFFFD) or
        (code >= 0x10000 and code <= 0x10FFFF);
}

pub const EncodeError = error{
    /// CDATA cannot hold `]]>`.
    CdataTerminator,
    /// A comment cannot hold `--` or end with `-`.
    CommentDashes,
    /// XML cannot hold this control character, escaped or not.
    ControlCharacter,
} || std.mem.Allocator.Error;

/// The raw bytes that make `field` read `value`: `&`, `<` and the field's
/// quote escaped in an attribute, `&`, `<` and `>` in text, CDATA and comment
/// text verbatim when the syntax allows it.
pub fn encode(gpa: std.mem.Allocator, field: Field, value: []const u8) EncodeError![]u8 {
    for (value) |c| {
        if (c < 0x20 and c != '\t' and c != '\n' and c != '\r') return error.ControlCharacter;
    }
    switch (field.kind) {
        .cdata => {
            if (std.mem.indexOf(u8, value, "]]>") != null) return error.CdataTerminator;
            return gpa.dupe(u8, value);
        },
        .comment => {
            if (std.mem.indexOf(u8, value, "--") != null or std.mem.endsWith(u8, value, "-")) return error.CommentDashes;
            return gpa.dupe(u8, value);
        },
        .attribute, .text => {
            var out: std.ArrayList(u8) = .empty;
            errdefer out.deinit(gpa);
            for (value) |c| {
                const escaped: ?[]const u8 = switch (c) {
                    '&' => "&amp;",
                    '<' => "&lt;",
                    '>' => if (field.kind == .text) "&gt;" else null,
                    '"' => if (field.quote == '"') "&quot;" else null,
                    '\'' => if (field.quote == '\'') "&apos;" else null,
                    else => null,
                };
                if (escaped) |text| try out.appendSlice(gpa, text) else try out.append(gpa, c);
            }
            return out.toOwnedSlice(gpa);
        },
    }
}

pub const Patch = struct {
    start: usize,
    end: usize,
    bytes: []const u8,
};

/// `source` with each patch's span replaced by its bytes and every other byte
/// as it was. Patches may come in any order but must not overlap (distinct
/// fields never do). Caller owns the result.
pub fn applyPatches(gpa: std.mem.Allocator, source: []const u8, patches: []Patch) ![]u8 {
    std.mem.sort(Patch, patches, {}, struct {
        fn lessThan(_: void, lhs: Patch, rhs: Patch) bool {
            return lhs.start < rhs.start;
        }
    }.lessThan);
    var out: std.ArrayList(u8) = .empty;
    errdefer out.deinit(gpa);
    var copied: usize = 0;
    for (patches) |patch| {
        if (patch.start < copied or patch.end < patch.start or patch.end > source.len) return error.OverlappingPatches;
        try out.appendSlice(gpa, source[copied..patch.start]);
        try out.appendSlice(gpa, patch.bytes);
        copied = patch.end;
    }
    try out.appendSlice(gpa, source[copied..]);
    return out.toOwnedSlice(gpa);
}
