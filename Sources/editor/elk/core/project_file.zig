//! The ELK project file `elk.xml` (D-05): `CELK`'s data tree from
//! Sources/src/ELK/ELK_Methods.cpp - the elements (`SELKElement` with its
//! `SELKDescription`), the project's own path and the two statistics
//! snapshots - plus the element description file `resource.description` an
//! update carries. Read with the engine's own XML reader, written in the
//! shape the engine's data tree writes: the declaration, CRLF, the whole tree
//! on one line, CRLF; numbers and flags as attributes, strings as child
//! elements, vectors as `item` children, an empty one self-closed.
//!
//! Paths: the MFC ELK wrote absolute host paths, but read back only the part
//! of an element's `Path` after the last backslash and replaced the project's
//! own `Path` with where it opened the file. So this writes an element's
//! `Path` as the bare element name and the project's `Path` as the bare file
//! name: no host path leaks into the file and it moves between machines.
const std = @import("std");
const xml = @import("xml");
const state = @import("state.zig");

pub const Description = struct {
    name: []const u8 = "",
    /// The `.pak` the export writes.
    pak: []const u8 = "",
    /// The prefix of the element's `<prefix>_<n>.upd` update files.
    upd: []const u8 = "",
    fonts: bool = false,
};

pub const Element = struct {
    description: Description = .{},
    /// The bare element name; its texts live in `<name>_data_base/`.
    name: []const u8 = "",
    version: []const u8 = "",
    /// `SELKElement`'s own default.
    last_update_number: i32 = -2,
};

/// One state's counts in a statistics snapshot (`SELKElementStatistic::SState`).
pub const Counts = struct {
    texts: i32 = 0,
    words: i32 = 0,
    word_symbols: i32 = 0,
    symbols: i32 = 0,
};

/// Per element, the counts for each of the four states.
pub const ElementStatistic = [4]Counts;

pub const Statistic = struct {
    valid: bool = false,
    /// "Orininal" in the file, a typo the format keeps.
    original: []ElementStatistic = &.{},
    translation: []ElementStatistic = &.{},
};

/// A parsed `elk.xml`. Every slice is owned by `arena`.
pub const Project = struct {
    arena: std.heap.ArenaAllocator,
    elements: []Element = &.{},
    previous_statistic: Statistic = .{},
    statistic: Statistic = .{},

    pub fn deinit(self: *Project) void {
        self.arena.deinit();
    }
};

pub const ParseError = error{ MalformedProject, OutOfMemory };

pub fn parse(gpa: std.mem.Allocator, bytes: []const u8) ParseError!Project {
    var project: Project = .{ .arena = std.heap.ArenaAllocator.init(gpa) };
    errdefer project.deinit();
    const arena = project.arena.allocator();
    const owned = try arena.dupe(u8, bytes);
    const document = xml.parse(arena, owned) catch |err| switch (err) {
        error.OutOfMemory => return error.OutOfMemory,
        else => return error.MalformedProject,
    };
    const root = document.root;

    var elements: std.ArrayListUnmanaged(Element) = .empty;
    if (xml.child(root, "Elements")) |list| {
        for (list.children.items) |item| {
            if (!std.mem.eql(u8, item.name, "item")) continue;
            var element: Element = .{
                .name = bareName(value(item, "Path") orelse ""),
                .version = value(item, "Version") orelse "",
                .last_update_number = intValue(item, "LastUpdateNumber") orelse -2,
            };
            if (xml.child(item, "Description")) |node| element.description = parseDescriptionNode(node);
            try elements.append(arena, element);
        }
    }
    project.elements = elements.items;
    if (xml.child(root, "PreviousStatistic")) |node| project.previous_statistic = try parseStatistic(arena, node);
    if (xml.child(root, "Statistic")) |node| project.statistic = try parseStatistic(arena, node);
    return project;
}

/// The element description in `<element>_data_base/resource.description`,
/// slices into `bytes`; null when it does not parse.
pub fn parseResourceDescription(arena: std.mem.Allocator, bytes: []const u8) ?Description {
    const document = xml.parse(arena, bytes) catch return null;
    return parseDescriptionNode(document.root);
}

/// The part of a stored path after its last separator: the MFC ELK split at
/// the last backslash, this also at a forward slash.
pub fn bareName(path: []const u8) []const u8 {
    const cut = std.mem.lastIndexOfAny(u8, path, "\\/") orelse return path;
    return path[cut + 1 ..];
}

/// `elk.xml`'s bytes for `project`; `file_name` is the project's own bare
/// file name (`elk.xml`).
pub fn write(gpa: std.mem.Allocator, project: *const Project, file_name: []const u8) ![]u8 {
    var out: std.ArrayListUnmanaged(u8) = .empty;
    errdefer out.deinit(gpa);
    try out.appendSlice(gpa, "<?xml version=\"1.0\"?>\r\n<base>");
    if (project.elements.len == 0) {
        try out.appendSlice(gpa, "<Elements/>");
    } else {
        try out.appendSlice(gpa, "<Elements>");
        for (project.elements) |element| {
            try out.print(gpa, "<item LastUpdateNumber=\"{d}\"><Description Fonts=\"{d}\">", .{ element.last_update_number, @intFromBool(element.description.fonts) });
            try textElement(gpa, &out, "Name", element.description.name);
            try textElement(gpa, &out, "PAK", element.description.pak);
            try textElement(gpa, &out, "UPD", element.description.upd);
            try out.appendSlice(gpa, "</Description>");
            try textElement(gpa, &out, "Path", element.name);
            try textElement(gpa, &out, "Version", element.version);
            try out.appendSlice(gpa, "</item>");
        }
        try out.appendSlice(gpa, "</Elements>");
    }
    try textElement(gpa, &out, "Path", file_name);
    try writeStatistic(gpa, &out, "PreviousStatistic", project.previous_statistic);
    try writeStatistic(gpa, &out, "Statistic", project.statistic);
    try out.appendSlice(gpa, "</base>\r\n");
    return out.toOwnedSlice(gpa);
}

fn parseDescriptionNode(node: *const xml.Node) Description {
    return .{
        .name = value(node, "Name") orelse "",
        .pak = value(node, "PAK") orelse "",
        .upd = value(node, "UPD") orelse "",
        .fonts = if (value(node, "Fonts")) |text| state.parseBool(text) else false,
    };
}

fn parseStatistic(arena: std.mem.Allocator, node: *const xml.Node) !Statistic {
    return .{
        .valid = if (value(node, "Valid")) |text| state.parseBool(text) else false,
        .original = try parseElementStatistics(arena, xml.child(node, "Orininal")),
        .translation = try parseElementStatistics(arena, xml.child(node, "Translation")),
    };
}

fn parseElementStatistics(arena: std.mem.Allocator, list: ?*const xml.Node) ![]ElementStatistic {
    const node = list orelse return &.{};
    var out: std.ArrayListUnmanaged(ElementStatistic) = .empty;
    for (node.children.items) |item| {
        if (!std.mem.eql(u8, item.name, "item")) continue;
        // `SELKElementStatistic` always holds four states, whatever was read.
        var statistic: ElementStatistic = @splat(.{});
        if (xml.child(item, "States")) |states| {
            var index: usize = 0;
            for (states.children.items) |entry| {
                if (!std.mem.eql(u8, entry.name, "item")) continue;
                if (index == statistic.len) break;
                statistic[index] = .{
                    .texts = intValue(entry, "TextsCount") orelse 0,
                    .words = intValue(entry, "WordCount") orelse 0,
                    .word_symbols = intValue(entry, "WordSymbolsCount") orelse 0,
                    .symbols = intValue(entry, "SymbolsCount") orelse 0,
                };
                index += 1;
            }
        }
        try out.append(arena, statistic);
    }
    return out.items;
}

fn writeStatistic(gpa: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), name: []const u8, statistic: Statistic) !void {
    try out.print(gpa, "<{s} Valid=\"{d}\">", .{ name, @intFromBool(statistic.valid) });
    try writeElementStatistics(gpa, out, "Orininal", statistic.original);
    try writeElementStatistics(gpa, out, "Translation", statistic.translation);
    try out.print(gpa, "</{s}>", .{name});
}

fn writeElementStatistics(gpa: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), name: []const u8, list: []const ElementStatistic) !void {
    if (list.len == 0) return out.print(gpa, "<{s}/>", .{name});
    try out.print(gpa, "<{s}>", .{name});
    for (list) |statistic| {
        try out.appendSlice(gpa, "<item><States>");
        for (statistic) |counts| {
            try out.print(gpa, "<item TextsCount=\"{d}\" WordCount=\"{d}\" WordSymbolsCount=\"{d}\" SymbolsCount=\"{d}\"/>", .{ counts.texts, counts.words, counts.word_symbols, counts.symbols });
        }
        try out.appendSlice(gpa, "</States></item>");
    }
    try out.print(gpa, "</{s}>", .{name});
}

fn textElement(gpa: std.mem.Allocator, out: *std.ArrayListUnmanaged(u8), name: []const u8, text: []const u8) !void {
    if (text.len == 0) return out.print(gpa, "<{s}/>", .{name});
    try out.print(gpa, "<{s}>", .{name});
    for (text) |byte| switch (byte) {
        '&' => try out.appendSlice(gpa, "&amp;"),
        '<' => try out.appendSlice(gpa, "&lt;"),
        '>' => try out.appendSlice(gpa, "&gt;"),
        else => try out.append(gpa, byte),
    };
    try out.print(gpa, "</{s}>", .{name});
}

/// A value the data tree stored as an attribute or, for strings, as a child
/// element's text.
fn value(node: *const xml.Node, name: []const u8) ?[]const u8 {
    if (xml.attribute(node, name)) |text| return text;
    if (xml.child(node, name)) |child| return child.text;
    return null;
}

fn intValue(node: *const xml.Node, name: []const u8) ?i32 {
    const text = value(node, name) orelse return null;
    return std.fmt.parseInt(i32, std.mem.trim(u8, text, " \t\r\n"), 10) catch null;
}

const testing = std.testing;

fn readTracked(path: []const u8) ![]u8 {
    return std.Io.Dir.cwd().readFileAlloc(testing.io, path, testing.allocator, .limited(1 << 20));
}

test "Data/ELK/elk.xml reads into its element and both statistics" {
    const bytes = try readTracked("Data/ELK/elk.xml");
    defer testing.allocator.free(bytes);
    var project = try parse(testing.allocator, bytes);
    defer project.deinit();

    try testing.expectEqual(@as(usize, 1), project.elements.len);
    const element = project.elements[0];
    try testing.expectEqualStrings("Game Resources", element.description.name);
    try testing.expectEqualStrings("texts.pak", element.description.pak);
    try testing.expectEqualStrings("elk", element.description.upd);
    try testing.expect(element.description.fonts);
    // The stored absolute Windows path reads as the bare element name.
    try testing.expectEqualStrings("game", element.name);
    try testing.expectEqualStrings("game_1.upd, [15:06:2026, 18.15.26]", element.version);
    try testing.expectEqual(@as(i32, 1), element.last_update_number);

    try testing.expect(project.statistic.valid and project.previous_statistic.valid);
    try testing.expectEqual(@as(usize, 1), project.statistic.original.len);
    try testing.expectEqual(Counts{ .texts = 1042, .words = 29398, .word_symbols = 151271, .symbols = 188738 }, project.statistic.original[0][1]);
    try testing.expectEqual(Counts{ .texts = 1017, .words = 7080, .word_symbols = 35259, .symbols = 42123 }, project.statistic.translation[0][3]);
}

test "both tracked elk.xml files write back byte for byte but the bare Path values" {
    for ([_][]const u8{ "Data/ELK/elk.xml", "Data/AmericanELK/elk.xml" }) |path| {
        const bytes = try readTracked(path);
        defer testing.allocator.free(bytes);
        var project = try parse(testing.allocator, bytes);
        defer project.deinit();
        const written = try write(testing.allocator, &project, "elk.xml");
        defer testing.allocator.free(written);

        // The expected file: the original with every <Path>...</Path> value
        // cut to its bare name, which is the only change D-05 makes.
        var expected: std.ArrayListUnmanaged(u8) = .empty;
        defer expected.deinit(testing.allocator);
        var rest: []const u8 = bytes;
        while (std.mem.indexOf(u8, rest, "<Path>")) |open| {
            const start = open + "<Path>".len;
            const close = std.mem.indexOfPos(u8, rest, start, "</Path>").?;
            try expected.appendSlice(testing.allocator, rest[0..start]);
            try expected.appendSlice(testing.allocator, bareName(rest[start..close]));
            rest = rest[close..];
        }
        try expected.appendSlice(testing.allocator, rest);
        try testing.expectEqualStrings(expected.items, written);
    }
}

test "a written project reads back equal, escapes included" {
    var project: Project = .{ .arena = std.heap.ArenaAllocator.init(testing.allocator) };
    defer project.deinit();
    var elements = [_]Element{
        .{ .description = .{ .name = "Maps & <Missions>", .pak = "maps.pak", .upd = "map" }, .name = "maps" },
        .{ .name = "game", .version = "game_22.upd", .last_update_number = 22 },
    };
    project.elements = &elements;
    const written = try write(testing.allocator, &project, "elk.xml");
    defer testing.allocator.free(written);
    try testing.expect(std.mem.indexOf(u8, written, "<Name>Maps &amp; &lt;Missions&gt;</Name>") != null);
    try testing.expect(std.mem.indexOf(u8, written, "<Orininal/>") != null);

    var back = try parse(testing.allocator, written);
    defer back.deinit();
    try testing.expectEqual(@as(usize, 2), back.elements.len);
    try testing.expectEqualStrings("Maps & <Missions>", back.elements[0].description.name);
    try testing.expectEqual(@as(i32, -2), back.elements[0].last_update_number);
    try testing.expectEqualStrings("", back.elements[1].description.name);
    try testing.expectEqual(@as(i32, 22), back.elements[1].last_update_number);
    try testing.expect(!back.statistic.valid);
}

test "Path values keep only the bare name, from either separator" {
    try testing.expectEqualStrings("game", bareName("C:\\Users\\x\\Data\\ELK\\game"));
    try testing.expectEqualStrings("game", bareName("/home/x/Data/ELK/game"));
    try testing.expectEqualStrings("game", bareName("game"));
}

test "a malformed project file is an error, not a crash" {
    try testing.expectError(error.MalformedProject, parse(testing.allocator, "<base><Elements>"));
}

test "resource.description reads the element description" {
    const bytes = try readTracked("Data/ELK/game_data_base/data/resource.description");
    defer testing.allocator.free(bytes);
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const description = parseResourceDescription(arena.allocator(), bytes).?;
    try testing.expectEqualStrings("Game Resources", description.name);
    try testing.expectEqualStrings("texts.pak", description.pak);
    try testing.expectEqualStrings("elk", description.upd);
    try testing.expect(description.fonts);
}
