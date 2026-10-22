//! Every setting the settings screen lists has its texts (2026-10-22: GFX.Present.Mode and
//! GFX.Present.MaxFPS were added to Data/Configs/defconf.cfg without any, so the screen showed the raw
//! key and no tooltip in every language). OptionEntryWrapper.cpp draws an option's label from
//! `Textes\Options\<KeyName>.name`, its tooltip from `<KeyName>.tooltip`, and each value of a click
//! switch from `<KeyName>\<value>.name` and `.tooltip` (UIConsts.cpp ConstructOptionKey). The values
//! are the ones the Zig options bridge serves (`GetDropValues` in Sources/src/StreamIOZig/legacy_bridge.cpp),
//! read from that file so the test follows the bridge. Generated values (resolutions, displays, mods,
//! rclone remotes) are shown as they are and have no texts.
//!
//! Each text must exist twice: in the game's English `Data/Textes` and as an original in `Data/ELK`, so
//! the language mods, which ELK exports, can carry its translation. Files are found case-insensitively.
const std = @import("std");
const testing = std.testing;

const config_path = "Data/Configs/defconf.cfg";
const bridge_path = "Sources/src/StreamIOZig/legacy_bridge.cpp";

/// The fills whose values the game generates at run time (IsGeneratedSelectionText and the language picker).
const generated_fills = [_][]const u8{ "GetVideoModes", "GetMonitors", "GetLanguages", "GetCloudProvider" };

/// OPTION_FLAG_DEVELOPER_ONLY (StreamIO/OptionSystem.h): never on the player's settings screen.
const developer_only: u32 = 1 << 1;

const Option = struct { key: []const u8, fill: []const u8, flags: u32 };

fn between(text: []const u8, open: []const u8, close: []const u8) ?[]const u8 {
    const start = (std.mem.indexOf(u8, text, open) orelse return null) + open.len;
    const end = std.mem.indexOfPos(u8, text, start, close) orelse return null;
    return text[start..end];
}

/// The `<item ...>` entries of defconf.cfg that name a `<KeyName>`.
fn readOptions(allocator: std.mem.Allocator, config: []const u8) ![]Option {
    var options: std.ArrayList(Option) = .empty;
    var rest = config;
    while (std.mem.indexOf(u8, rest, "<KeyName>")) |at| {
        const item_start = std.mem.lastIndexOf(u8, rest[0..at], "<item ") orelse return error.MalformedConfig;
        const item = rest[item_start..at];
        const key = between(rest[at..], "<KeyName>", "</KeyName>") orelse return error.MalformedConfig;
        const flags_text = between(item, "Flags=\"", "\"") orelse "0";
        try options.append(allocator, .{
            .key = key,
            .fill = between(item, "<ActionFill>", "</ActionFill>") orelse "",
            .flags = try std.fmt.parseInt(u32, flags_text, 10),
        });
        rest = rest[at + "<KeyName>".len ..];
    }
    return options.toOwnedSlice(allocator);
}

/// The values the bridge serves for `fill`: the quoted strings assigned to `values[n]` on its line.
fn bridgeValues(allocator: std.mem.Allocator, bridge: []const u8, fill: []const u8) !?[]const []const u8 {
    var buffer: [128]u8 = undefined;
    const needle = try std.fmt.bufPrint(&buffer, "== \"{s}\")", .{fill});
    const at = std.mem.indexOf(u8, bridge, needle) orelse return null;
    const line_end = std.mem.indexOfScalarPos(u8, bridge, at, '\n') orelse bridge.len;
    var line = bridge[at..line_end];
    var values: std.ArrayList([]const u8) = .empty;
    while (std.mem.indexOf(u8, line, "] = \"")) |assign| {
        const value = between(line[assign..], "] = \"", "\"") orelse break;
        try values.append(allocator, value);
        line = line[assign + 5 + value.len ..];
    }
    return try values.toOwnedSlice(allocator);
}

/// `path` below `root` with every component matched whatever its case, as the game finds it.
fn existsFolded(io: std.Io, root: []const u8, path: []const u8) bool {
    var dir = std.Io.Dir.cwd().openDir(io, root, .{ .iterate = true }) catch return false;
    var parts = std.mem.splitScalar(u8, path, '/');
    while (parts.next()) |part| {
        var found: ?[]const u8 = null;
        var kind: std.Io.File.Kind = .file;
        var it = dir.iterate();
        while (it.next(io) catch null) |entry| {
            if (std.ascii.eqlIgnoreCase(entry.name, part)) {
                found = entry.name;
                kind = entry.kind;
                break;
            }
        }
        const name = found orelse {
            dir.close(io);
            return false;
        };
        if (parts.peek() == null) {
            dir.close(io);
            return kind == .file;
        }
        const next = dir.openDir(io, name, .{ .iterate = true }) catch {
            dir.close(io);
            return false;
        };
        dir.close(io);
        dir = next;
    }
    dir.close(io);
    return false;
}

fn expectText(io: std.Io, allocator: std.mem.Allocator, missing: *std.ArrayList([]const u8), key: []const u8) !void {
    const game = try std.fmt.allocPrint(allocator, "Options/{s}.txt", .{key});
    if (!existsFolded(io, "Data/Textes", game)) try missing.append(allocator, try std.fmt.allocPrint(allocator, "Data/Textes/{s}", .{game}));
    const elk = try std.fmt.allocPrint(allocator, "textes/options/{s}.elk", .{key});
    if (!existsFolded(io, "Data/ELK/game_data_base", elk)) try missing.append(allocator, try std.fmt.allocPrint(allocator, "Data/ELK/game_data_base/{s}", .{elk}));
}

test "every setting has its label, tooltip and value texts in the game and in ELK" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const config = try std.Io.Dir.cwd().readFileAlloc(io, config_path, allocator, .limited(4 << 20));
    const bridge = try std.Io.Dir.cwd().readFileAlloc(io, bridge_path, allocator, .limited(4 << 20));
    const options = try readOptions(allocator, config);
    try testing.expect(options.len >= 30);

    var missing: std.ArrayList([]const u8) = .empty;
    var checked: usize = 0;
    for (options) |option| {
        if (option.flags & developer_only != 0) continue;
        checked += 1;
        try expectText(io, allocator, &missing, try std.fmt.allocPrint(allocator, "{s}.name", .{option.key}));
        try expectText(io, allocator, &missing, try std.fmt.allocPrint(allocator, "{s}.tooltip", .{option.key}));
        if (option.fill.len == 0) continue;
        const generated = for (generated_fills) |fill| {
            if (std.mem.eql(u8, fill, option.fill)) break true;
        } else false;
        if (generated) continue;
        const values = (try bridgeValues(allocator, bridge, option.fill)) orelse {
            try missing.append(allocator, try std.fmt.allocPrint(allocator, "{s}: the bridge serves no values for {s}", .{ option.key, option.fill }));
            continue;
        };
        for (values) |value| {
            try expectText(io, allocator, &missing, try std.fmt.allocPrint(allocator, "{s}/{s}.name", .{ option.key, value }));
            try expectText(io, allocator, &missing, try std.fmt.allocPrint(allocator, "{s}/{s}.tooltip", .{ option.key, value }));
        }
    }
    for (missing.items) |line| std.debug.print("option-texts: missing {s}\n", .{line});
    try testing.expect(checked >= 30);
    try testing.expectEqual(@as(usize, 0), missing.items.len);
}

test "the bridge's value lines are read as the game serves them" {
    const allocator = testing.allocator;
    const line = "        else if (fill && std::string(fill) == \"GetPresentMode\") { values[0] = \"VSync\"; values[1] = \"Mailbox\"; count = 2; }\n";
    const values = (try bridgeValues(allocator, line, "GetPresentMode")).?;
    defer allocator.free(values);
    try testing.expectEqual(@as(usize, 2), values.len);
    try testing.expectEqualStrings("VSync", values[0]);
    try testing.expectEqualStrings("Mailbox", values[1]);
    try testing.expect((try bridgeValues(allocator, line, "GetOnOff")) == null);
}
