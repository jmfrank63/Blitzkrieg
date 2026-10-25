//! Tests of the overlay sheet helper (M003 S01 T01): a fixture tree under
//! zig-out/local-test/l10n-sheet-test/<case>/ holds a few UTF-16LE originals; the sheet is exported,
//! edited as text, imported, and the written overlay is read back through the checker's
//! `textProblem`. Nothing writes into Data/ or Localization/.
const std = @import("std");
const testing = std.testing;
const sheet = @import("l10n_sheet.zig");
const check = @import("l10n_check.zig");

const scratch = "zig-out/local-test/l10n-sheet-test";

fn write(io: std.Io, dir: std.Io.Dir, path: []const u8, data: []const u8) !void {
    if (std.fs.path.dirname(path)) |parent| try dir.createDirPath(io, parent);
    try dir.writeFile(io, .{ .sub_path = path, .data = data });
}

fn original(io: std.Io, arena: std.mem.Allocator, root: std.Io.Dir, key: []const u8, text: []const u8) !void {
    const path = try std.fmt.allocPrint(arena, "Data/ELK/game_data_base/{s}.elk", .{key});
    try write(io, root, path, try sheet.encodeText(arena, text));
}

fn tree(io: std.Io, arena: std.mem.Allocator, case: []const u8) !std.Io.Dir {
    const path = try std.fmt.allocPrint(arena, "{s}/{s}", .{ scratch, case });
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, path) catch {};
    try cwd.createDirPath(io, path);
    const root = try cwd.openDir(io, path, .{ .iterate = true });
    try original(io, arena, root, "units/tank/name", "Tank");
    try original(io, arena, root, "units/tank/desc", "Heavy tank.\r\nSecond line, with a trailing break.\r\n");
    try original(io, arena, root, "units/gun/name", "Gun");
    try original(io, arena, root, "menu/start", "Start");
    return root;
}

test "export writes one block per original under the prefixes and keeps the line breaks" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "export");
    defer root.close(io);
    const paths = try sheet.exportSheets(io, arena, root, "de", &.{"units/"}, sheet.sheet_dir);
    try testing.expectEqual(@as(usize, 1), paths.len);
    try testing.expectEqualStrings("zig-out/local-test/l10n-sheet/de-1.txt", paths[0]);
    const text = try root.readFileAlloc(io, paths[0], arena, .limited(1 << 20));
    try testing.expectEqualStrings("=== units/gun/name\r\nGun\r\n=== units/tank/desc\r\nHeavy tank.\r\nSecond line, with a trailing break.\r\n\r\n=== units/tank/name\r\nTank\r\n", text);
}

test "export shows the current translation under a --- current line" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "export_current");
    defer root.close(io);
    try write(io, root, "Localization/game/de/game_data_base/menu/start.txt", try sheet.encodeText(arena, "Starten"));
    const paths = try sheet.exportSheets(io, arena, root, "de", &.{"menu/"}, sheet.sheet_dir);
    const text = try root.readFileAlloc(io, paths[0], arena, .limited(1 << 20));
    try testing.expectEqualStrings("=== menu/start\r\nStart\r\n--- current\r\nStarten\r\n", text);
}

test "export reads an original that ends in a stray half unit, as textes/gametypes/game_type_unknown does" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "export_odd");
    defer root.close(io);
    const whole = try sheet.encodeText(arena, "Singl");
    try write(io, root, "Data/ELK/game_data_base/menu/odd.elk", try std.mem.concat(arena, u8, &.{ whole, "e" }));
    const paths = try sheet.exportSheets(io, arena, root, "de", &.{"menu/odd"}, sheet.sheet_dir);
    const text = try root.readFileAlloc(io, paths[0], arena, .limited(1 << 20));
    try testing.expectEqualStrings("=== menu/odd\r\nSingl\r\n", text);
}

test "export reads a text the database lacks from the Game's own text tree, as the checker does" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "export_text_tree");
    defer root.close(io);
    try write(io, root, "Data/Textes/Strings/Save_Game.txt", try sheet.encodeText(arena, "Save Game"));
    const paths = try sheet.exportSheets(io, arena, root, "de", &.{"textes/strings/"}, sheet.sheet_dir);
    const text = try root.readFileAlloc(io, paths[0], arena, .limited(1 << 20));
    try testing.expectEqualStrings("=== textes/strings/save_game\r\nSave Game\r\n", text);
}

test "export errors: an unknown language and a prefix that matches nothing" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "export_errors");
    defer root.close(io);
    try testing.expectError(error.UnknownLanguage, sheet.exportSheets(io, arena, root, "xx", &.{"units/"}, sheet.sheet_dir));
    try testing.expectError(error.PrefixMatchesNothing, sheet.exportSheets(io, arena, root, "de", &.{ "units/", "nothing/" }, sheet.sheet_dir));
}

test "export cuts the sheet before a block would pass the byte limit" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "export_chunks");
    defer root.close(io);
    const long = try arena.alloc(u8, 30_000);
    @memset(long, 'x');
    try original(io, arena, root, "big/a", long);
    try original(io, arena, root, "big/b", long);
    const paths = try sheet.exportSheets(io, arena, root, "de", &.{"big/"}, sheet.sheet_dir);
    try testing.expectEqual(@as(usize, 2), paths.len);
    try testing.expectEqualStrings("zig-out/local-test/l10n-sheet/de-2.txt", paths[1]);
}

test "round trip: a translated sheet imports as UTF-16LE texts the checker accepts" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "round_trip");
    defer root.close(io);
    const edited =
        "=== units/tank/name\r\nPanzer\r\n" ++
        "=== units/tank/desc\r\nSchwerer Panzer.\nZweite Zeile, mit Umbruch am Ende.\r\n\r\n" ++
        "--- current\r\nalt\r\n" ++
        "=== menu/start\r\nStärke ☃ 𝄞\r\n";
    const result = try sheet.importSheet(io, arena, root, "de", edited);
    try testing.expectEqual(@as(usize, 3), result.written);

    const name = try root.readFileAlloc(io, "Localization/game/de/game_data_base/units/tank/name.txt", arena, .limited(1 << 20));
    try testing.expectEqual(check.TextProblem.ok, check.textProblem(name));
    try testing.expectEqualSlices(u8, try sheet.encodeText(arena, "Panzer"), name);

    // The lone LF became CR LF and the trailing break (the original has one) is kept.
    const desc = try root.readFileAlloc(io, "Localization/game/de/game_data_base/units/tank/desc.txt", arena, .limited(1 << 20));
    try testing.expectEqual(check.TextProblem.ok, check.textProblem(desc));
    try testing.expectEqualStrings("Schwerer Panzer.\r\nZweite Zeile, mit Umbruch am Ende.\r\n", (try sheet.decodeText(arena, desc)).?);

    const start = try root.readFileAlloc(io, "Localization/game/de/game_data_base/menu/start.txt", arena, .limited(1 << 20));
    try testing.expectEqual(check.TextProblem.ok, check.textProblem(start));
    try testing.expectEqualStrings("Stärke ☃ 𝄞", (try sheet.decodeText(arena, start)).?);

    const xml = try root.readFileAlloc(io, "Localization/game/de/game_data_base/menu/start.xml", arena, .limited(1 << 20));
    try testing.expectEqualStrings("<?xml version=\"1.0\"?>\r\n<base State=\"2\" Changed=\"1\"/>\r\n", xml);
    try testing.expectEqual(@as(?u8, 2), check.parseState(xml));
}

test "an exported sheet imports back to the same texts" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "identity");
    defer root.close(io);
    const paths = try sheet.exportSheets(io, arena, root, "de", &.{"units/tank/"}, sheet.sheet_dir);
    const text = try root.readFileAlloc(io, paths[0], arena, .limited(1 << 20));
    try testing.expectEqual(@as(usize, 2), (try sheet.importSheet(io, arena, root, "de", text)).written);
    for ([_][]const u8{ "units/tank/name", "units/tank/desc" }) |key| {
        const original_bytes = try root.readFileAlloc(io, try std.fmt.allocPrint(arena, "Data/ELK/game_data_base/{s}.elk", .{key}), arena, .limited(1 << 20));
        const written = try root.readFileAlloc(io, try std.fmt.allocPrint(arena, "Localization/game/de/game_data_base/{s}.txt", .{key}), arena, .limited(1 << 20));
        try testing.expectEqualSlices(u8, original_bytes, written);
    }
}

test "import refuses a key without an original and an empty block, and writes nothing" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "refuse");
    defer root.close(io);
    const result = try sheet.importSheet(io, arena, root, "de", "=== units/tank/name\r\nPanzer\r\n=== units/ghost/name\r\nGeist\r\n=== menu/start\r\n  \r\n=== units/gun/name\r\nKanone\r\n=== units/gun/name\r\nZweimal\r\n");
    const refused = result.refused;
    try testing.expectEqual(@as(usize, 3), refused.len);
    try testing.expectEqualStrings("units/ghost/name", refused[0].key);
    try testing.expectEqualStrings("not a text of the game (no original in Data/ELK/game_data_base or the Data text trees)", refused[0].detail);
    try testing.expectEqualStrings("menu/start", refused[1].key);
    try testing.expectEqualStrings("the block is empty", refused[1].detail);
    try testing.expectEqualStrings("the key is in the sheet twice", refused[2].detail);
    try testing.expectError(error.FileNotFound, root.statFile(io, "Localization/game/de/game_data_base/units/tank/name.txt", .{}));
}

test "import errors: an unknown language and a sheet without a block" {
    const io = testing.io;
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var root = try tree(io, arena, "import_errors");
    defer root.close(io);
    try testing.expectError(error.UnknownLanguage, sheet.importSheet(io, arena, root, "xx", "=== menu/start\r\nX\r\n"));
    try testing.expectError(error.SheetHasNoBlock, sheet.importSheet(io, arena, root, "de", "no header here\r\n"));
}
