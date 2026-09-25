//! The mission data tier: Data/Scenarios against GOG Blitzkrieg 1.2, and the
//! rules the chapter screen relies on now that it has no fallback.
//!
//! Runs from the repository root (build.zig sets the cwd). The normalisation
//! below must stay identical to normalise() in tools/data/gog_scenarios.py,
//! which wrote the manifest.
const std = @import("std");

const io = std.testing.io;
const max_file = 64 * 1024 * 1024;

const campaigns = [_]struct { file: []const u8, template_count: usize }{
    .{ .file = "scenarios/campaigns/german/german.xml", .template_count = 76 },
    .{ .file = "scenarios/campaigns/allies/allies.xml", .template_count = 34 },
    .{ .file = "scenarios/campaigns/ussr/ussr.xml", .template_count = 63 },
};

fn endsWithIgnoreCase(text: []const u8, suffix: []const u8) bool {
    return text.len >= suffix.len and std.ascii.eqlIgnoreCase(text[text.len - suffix.len ..], suffix);
}

fn isSpace(c: u8) bool {
    return c == ' ' or c == '\t' or c == '\n' or c == '\r' or c == 0x0b or c == 0x0c;
}

fn removeBlocks(a: std.mem.Allocator, text: []const u8, open: []const u8, close: []const u8) ![]const u8 {
    var out = std.ArrayList(u8).empty;
    var i: usize = 0;
    while (i < text.len) {
        const start = std.mem.indexOfPos(u8, text, i, open) orelse break;
        const end = std.mem.indexOfPos(u8, text, start + open.len, close) orelse break;
        try out.appendSlice(a, text[i..start]);
        i = end + close.len;
    }
    try out.appendSlice(a, text[i..]);
    return out.items;
}

fn collapseBetweenTags(a: std.mem.Allocator, text: []const u8) ![]const u8 {
    var out = std.ArrayList(u8).empty;
    var i: usize = 0;
    while (i < text.len) : (i += 1) {
        try out.append(a, text[i]);
        if (text[i] != '>') continue;
        var j = i + 1;
        while (j < text.len and isSpace(text[j])) j += 1;
        if (j > i + 1 and j < text.len and text[j] == '<') i = j - 1;
    }
    return out.items;
}

fn isNameStart(c: u8) bool {
    return std.ascii.isAlphabetic(c) or c == '_';
}

fn isNameChar(c: u8) bool {
    return std.ascii.isAlphanumeric(c) or c == '_' or c == '.';
}

fn expandEmptyTags(a: std.mem.Allocator, text: []const u8) ![]const u8 {
    var out = std.ArrayList(u8).empty;
    var i: usize = 0;
    while (i < text.len) {
        if (text[i] == '<' and i + 1 < text.len and isNameStart(text[i + 1])) {
            var j = i + 2;
            while (j < text.len and isNameChar(text[j])) j += 1;
            if (j + 1 < text.len and text[j] == '/' and text[j + 1] == '>') {
                const name = text[i + 1 .. j];
                try out.append(a, '<');
                try out.appendSlice(a, name);
                try out.appendSlice(a, "></");
                try out.appendSlice(a, name);
                try out.append(a, '>');
                i = j + 2;
                continue;
            }
        }
        try out.append(a, text[i]);
        i += 1;
    }
    return out.items;
}

/// normalise() of tools/data/gog_scenarios.py.
fn normalise(a: std.mem.Allocator, path: []const u8, bytes: []const u8) ![]const u8 {
    const xml = endsWithIgnoreCase(path, ".xml");
    if (!xml and !endsWithIgnoreCase(path, ".lua")) return bytes;
    var text = std.ArrayList(u8).empty;
    for (bytes) |c| if (c != '\r') try text.append(a, c);
    if (!xml) return text.items;
    var s: []const u8 = text.items;
    s = try removeBlocks(a, s, "<History>", "</History>");
    s = try removeBlocks(a, s, "<!--", "-->");
    s = try collapseBetweenTags(a, s);
    s = std.mem.trim(u8, s, " \t\n\r\x0b\x0c");
    return expandEmptyTags(a, s);
}

/// `relative` (any case, / or \) under `root`, each component matched without
/// case as the game's data storage matches it. Null when absent.
fn resolve(a: std.mem.Allocator, root: []const u8, relative: []const u8) !?[]const u8 {
    const cwd = std.Io.Dir.cwd();
    var current: []const u8 = root;
    var parts = std.mem.tokenizeAny(u8, relative, "/\\");
    while (parts.next()) |part| {
        var dir = cwd.openDir(io, current, .{ .iterate = true }) catch return null;
        defer dir.close(io);
        var it = dir.iterate();
        var found: ?[]const u8 = null;
        while (try it.next(io)) |entry| {
            if (std.ascii.eqlIgnoreCase(entry.name, part)) {
                found = try a.dupe(u8, entry.name);
                break;
            }
        }
        current = try std.fmt.allocPrint(a, "{s}/{s}", .{ current, found orelse return null });
    }
    return current;
}

fn readData(a: std.mem.Allocator, relative: []const u8) !?[]const u8 {
    const path = (try resolve(a, "Data", relative)) orelse return null;
    return try std.Io.Dir.cwd().readFileAlloc(io, path, a, .limited(max_file));
}

/// A data name ("scenarios\\chapters\\ussr\\kursk\\1") plus an extension.
fn dataFile(a: std.mem.Allocator, name: []const u8, extension: []const u8) ![]const u8 {
    return std.fmt.allocPrint(a, "{s}{s}", .{ name, extension });
}

/// The text of the first <tag>…</tag> in normalised XML, or "".
fn tagValue(xml: []const u8, comptime tag: []const u8) []const u8 {
    const open = "<" ++ tag ++ ">";
    const start = (std.mem.indexOf(u8, xml, open) orelse return "") + open.len;
    const end = std.mem.indexOfScalarPos(u8, xml, start, '<') orelse return "";
    return xml[start..end];
}

/// The section between <tag> and </tag>, or "".
fn section(xml: []const u8, comptime tag: []const u8) []const u8 {
    const open = "<" ++ tag ++ ">";
    const start = (std.mem.indexOf(u8, xml, open) orelse return "") + open.len;
    const end = std.mem.indexOfPos(u8, xml, start, "</" ++ tag ++ ">") orelse return "";
    return xml[start..end];
}

/// Every <tag>value</tag> value in xml.
fn allValues(a: std.mem.Allocator, xml: []const u8, comptime tag: []const u8) ![]const []const u8 {
    var values = std.ArrayList([]const u8).empty;
    const open = "<" ++ tag ++ ">";
    var i: usize = 0;
    while (std.mem.indexOfPos(u8, xml, i, open)) |at| {
        const start = at + open.len;
        const end = std.mem.indexOfScalarPos(u8, xml, start, '<') orelse break;
        try values.append(a, xml[start..end]);
        i = end;
    }
    return values.items;
}

fn readXml(a: std.mem.Allocator, relative: []const u8) !?[]const u8 {
    const bytes = (try readData(a, relative)) orelse return null;
    return try normalise(a, relative, bytes);
}

test "normalise matches the Python rules" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const a = arena.allocator();
    const input = "<?xml version=\"1.0\"?>\r\n<base><History><A>x</A></History>\r\n\t<RPG>\r\n<KeyName/><!-- gone -->\r\n<X a=\"1\"/>\r\n<MODName>\r\n</MODName></RPG></base>\r\n";
    try std.testing.expectEqualStrings(
        "<?xml version=\"1.0\"?><base><RPG><KeyName></KeyName><X a=\"1\"/><MODName></MODName></RPG></base>",
        try normalise(a, "x.xml", input),
    );
    try std.testing.expectEqualStrings("a\nb\n", try normalise(a, "x.lua", "a\r\nb\r\n"));
    try std.testing.expectEqualStrings("a\r\n", try normalise(a, "x.txt", "a\r\n"));
}

test "Data/Scenarios matches GOG 1.2 apart from the recorded deviations" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const a = arena.allocator();
    const cwd = std.Io.Dir.cwd();

    var deviations = std.StringHashMapUnmanaged(void){};
    const deviation_text = try cwd.readFileAlloc(io, "tools/data/gog-1.2-deviations.txt", a, .limited(1024 * 1024));
    var deviation_lines = std.mem.tokenizeScalar(u8, deviation_text, '\n');
    while (deviation_lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, "\r ");
        if (line.len == 0 or line[0] == '#') continue;
        const tab = std.mem.indexOfScalar(u8, line, '\t') orelse return error.DeviationWithoutReason;
        try deviations.put(a, line[0..tab], {});
    }

    const manifest = try cwd.readFileAlloc(io, "tools/data/gog-1.2-scenarios.sha256", a, .limited(16 * 1024 * 1024));
    var checked: usize = 0;
    var failures: usize = 0;
    var lines = std.mem.tokenizeScalar(u8, manifest, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, "\r ");
        if (line.len < 67) continue;
        const expected = line[0..64];
        const path = line[66..];
        if (deviations.contains(path)) continue;
        checked += 1;
        const bytes = (try readData(a, path)) orelse {
            std.debug.print("missing: {s}\n", .{path});
            failures += 1;
            continue;
        };
        var digest: [32]u8 = undefined;
        std.crypto.hash.sha2.Sha256.hash(try normalise(a, path, bytes), &digest, .{});
        const actual = std.fmt.bytesToHex(digest, .lower);
        if (!std.mem.eql(u8, &actual, expected)) {
            std.debug.print("differs from GOG 1.2: {s}\n", .{path});
            failures += 1;
        }
    }
    // The ruling's floor: below 1000 manifest lines this would be 90% of the
    // manifest's actual line count instead, so an empty sweep still fails.
    try std.testing.expect(checked > 1000);
    try std.testing.expectEqual(@as(usize, 0), failures);
}

test "Data holds no generated random maps" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    // The game once generated into Data; generated maps now belong to the
    // player's cache (StreamIO/GeneratedData.h) and would shadow nothing but
    // confuse every comparison.
    try std.testing.expect((try resolve(arena.allocator(), "Data", "maps/templatemaps")) == null);
}

test "every campaign template resolves and every gated chapter can offer random missions" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const a = arena.allocator();
    var failures: usize = 0;

    for (campaigns) |campaign| {
        const campaign_xml = (try readXml(a, campaign.file)) orelse return error.CampaignMissing;
        const templates = try allValues(a, section(campaign_xml, "Templates"), "item");
        if (templates.len != campaign.template_count) {
            std.debug.print("{s}: {d} templates, GOG 1.2 has {d}\n", .{ campaign.file, templates.len, campaign.template_count });
            failures += 1;
        }

        var per_setting = std.StringHashMapUnmanaged(usize){};
        for (templates) |template| {
            const mission = (try readXml(a, try dataFile(a, template, ".xml"))) orelse {
                std.debug.print("{s}: template mission {s} missing\n", .{ campaign.file, template });
                failures += 1;
                continue;
            };
            const setting = tagValue(mission, "SettingName");
            const entry = try per_setting.getOrPut(a, setting);
            if (!entry.found_existing) entry.value_ptr.* = 0;
            entry.value_ptr.* += 1;

            const template_map = tagValue(mission, "TemplateMap");
            const map_xml = try readXml(a, try dataFile(a, template_map, ".xml"));
            if (map_xml == null) {
                std.debug.print("{s}: template map {s} missing\n", .{ template, template_map });
                failures += 1;
            } else {
                const script = tagValue(map_xml.?, "Scripts");
                if (script.len != 0 and (try readData(a, try dataFile(a, script, ".lua"))) == null) {
                    std.debug.print("{s}: script {s}.lua missing\n", .{ template_map, script });
                    failures += 1;
                }
            }
            inline for (.{ "HeaderText", "DescriptionText" }) |tag| {
                const text = tagValue(mission, tag);
                if ((try readData(a, try dataFile(a, text, ".txt"))) == null) {
                    std.debug.print("{s}: {s} {s}.txt missing\n", .{ template, tag, text });
                    failures += 1;
                }
            }
        }

        for (try allValues(a, section(campaign_xml, "AllChapters"), "Chapter")) |chapter| {
            const chapter_xml = (try readXml(a, try dataFile(a, chapter, ".xml"))) orelse {
                std.debug.print("{s}: chapter {s} missing\n", .{ campaign.file, chapter });
                failures += 1;
                continue;
            };
            const script = (try readData(a, try dataFile(a, tagValue(chapter_xml, "Script"), ".lua"))) orelse "";
            // The chapters whose script enables the historical mission only
            // after a random win - all but the first chapter of a campaign.
            if (std.mem.indexOf(u8, script, "Mission.Current.Random") == null) continue;
            const setting = tagValue(chapter_xml, "SettingName");
            const template_count = per_setting.get(setting) orelse 0;
            const placeholders = std.mem.count(u8, section(chapter_xml, "PlaceHolders"), "<Position");
            std.debug.print("{s}: {d} templates for {s}, {d} placeholders\n", .{ chapter, template_count, setting, placeholders });
            if (template_count < 3 or placeholders < 3) {
                std.debug.print("  cannot offer one random mission per difficulty\n", .{});
                failures += 1;
            }
        }
    }
    try std.testing.expectEqual(@as(usize, 0), failures);
}
