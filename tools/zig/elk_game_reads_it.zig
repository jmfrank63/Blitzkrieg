//! E-06 on the real Game: a text the ELK database exported to a mod is the
//! text the game shows. The tracked Data/ELK is copied, exported unchanged to
//! `mods/ELKTest/data/texts.pak` of the installed game (D-09) and the Game
//! started on it with its own `-reference-scene` capture of the main menu; then
//! the main menu's Exit Game label gets a marker translation, the database is
//! exported again and the Game captured the same way. The two frames must
//! differ, and only inside the Exit Game button: the game drew the marker
//! where it draws that label, and nothing else changed. The Game's own traces
//! back it up: BK_MOD_TRACE says the label's text was opened from the mod and
//! BK_UI_TRACE that the main menu is laid out 1:1 on the 1024x768 canvas the
//! button's rect below is in.
//!
//! The user's data is never touched: the Game runs from a scratch folder (its
//! `profiles/` are relative to the working directory) in the dedicated ELKTest
//! profile, with XDG_DATA_HOME and BK_USER_ROOT in that folder too. The mod is
//! removed from the installation afterwards. BK_ELK_GAME names the installed
//! Game executable; `zig build elk-game-reads-it` sets it.
const std = @import("std");
const elk = @import("elk");
const database = elk.database;
const packages = elk.packages;
const testing = std.testing;

const scratch = "zig-out/local-test/elk/game-reads-it";
const mod_name = "ELKTest";
/// `imheader-gbutton-exitgame` in Data/UI/MainMenu.xml: the button in the
/// main menu the game opens on.
const label_key = "textes/strings/imheader-gbutton-exitgame";
const marker = "ELK E-06 MARKER";
const width = 1024;
const height = 768;
/// The Exit Game button on the 1024x768 canvas, as CSimpleWindow::Reposition
/// places it: its panel (element 2000, LEFT|TOP) at (610,267) sized 400x328,
/// the button (element 10006, HMID|TOP) at (0,260) sized 320x50, so x from
/// 610 + 200 - 160 = 650 to 970 and y from 267 + 260 = 527 to 577. Exclusive
/// right and bottom.
const label_rect = Rect{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 };

const Rect = struct {
    x1: usize,
    y1: usize,
    x2: usize,
    y2: usize,

    fn contains(self: Rect, other: Rect) bool {
        return other.x1 >= self.x1 and other.y1 >= self.y1 and other.x2 <= self.x2 and other.y2 <= self.y2;
    }
};

test "the Game shows a main-menu label the ELK exported to a mod (E-06)" {
    const io = testing.io;
    const gpa = testing.allocator;
    var arena = std.heap.ArenaAllocator.init(gpa);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = testing.environ.getPosix("BK_ELK_GAME") orelse {
        std.debug.print("BK_ELK_GAME is not set: run zig build elk-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    try cwd.createDirPath(io, user);

    // A copy of the tracked database: the edit below must not reach Data/ELK.
    const db_folder = try std.fs.path.join(allocator, &.{ root, "db" });
    try database.copyTree(io, "Data/ELK/game_data_base", try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
    const elk_xml = try std.fs.path.join(allocator, &.{ db_folder, "elk.xml" });
    try cwd.writeFile(io, .{ .sub_path = elk_xml, .data = try cwd.readFileAlloc(io, "Data/ELK/elk.xml", allocator, .limited(1 << 20)) });
    var db = try database.Database.open(gpa, io, elk_xml);
    defer db.deinit();

    const mod_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", mod_name });
    defer cwd.deleteTree(io, mod_folder) catch {};

    try exportMod(io, allocator, &db, game_root, null);
    const control = try runGame(io, allocator, game, user, try std.fs.path.join(allocator, &.{ root, "control.rgba" }), "control");

    // The translator's edit: the label is Approved and has no translation,
    // so the export carries its original until a translation is written.
    try testing.expect((try db.readState(0, label_key)).state != .not_translated);
    try testing.expect(try db.writeTranslation(0, label_key, marker));
    try exportMod(io, allocator, &db, game_root, marker);
    const marked = try runGame(io, allocator, game, user, try std.fs.path.join(allocator, &.{ root, "marker.rgba" }), "marker");

    // The measurement: every pixel that differs lies in the label's button,
    // and enough of them differ to be glyphs rather than noise.
    try testing.expectEqual(@as(usize, width * height * 4), control.len);
    try testing.expectEqual(control.len, marked.len);
    var changed: usize = 0;
    var box = Rect{ .x1 = width, .y1 = height, .x2 = 0, .y2 = 0 };
    for (0..height) |y| for (0..width) |x| {
        const at = (y * width + x) * 4;
        if (std.mem.eql(u8, control[at..][0..3], marked[at..][0..3])) continue;
        changed += 1;
        box = .{ .x1 = @min(box.x1, x), .y1 = @min(box.y1, y), .x2 = @max(box.x2, x + 1), .y2 = @max(box.y2, y + 1) };
    };
    std.debug.print("E-06: {d} pixels differ, in ({d},{d})-({d},{d}); the Exit Game button is ({d},{d})-({d},{d})\n", .{ changed, box.x1, box.y1, box.x2, box.y2, label_rect.x1, label_rect.y1, label_rect.x2, label_rect.y2 });
    try testing.expect(changed >= 100);
    try testing.expect(label_rect.contains(box));
}

/// Exports the database to the mod and checks, through the engine's zip
/// reader, that the label's entry carries `expected` (UTF-16 LE with a BOM,
/// as the game reads texts) or, with null, the original.
fn exportMod(io: std.Io, allocator: std.mem.Allocator, db: *database.Database, game_root: []const u8, expected: ?[]const u8) !void {
    const path = try packages.exportTo(testing.allocator, io, db, 0, game_root, .{ .mod = mod_name }, .{});
    defer testing.allocator.free(path);
    var package = try packages.readArchive(testing.allocator, io, path);
    defer package.deinit();
    const name = label_key ++ ".txt";
    const bytes = for (package.entries) |entry| {
        if (std.mem.eql(u8, entry.name, name)) break entry.bytes;
    } else return error.LabelNotExported;
    const wanted = if (expected) |text| try utf16(allocator, text) else (try db.readBytes(allocator, 0, label_key, .original)).?;
    try testing.expectEqualSlices(u8, wanted, bytes);
}

fn utf16(allocator: std.mem.Allocator, text: []const u8) ![]u8 {
    const out = try allocator.alloc(u8, 2 + text.len * 2);
    out[0] = 0xff;
    out[1] = 0xfe;
    for (text, 0..) |byte, at| {
        out[2 + at * 2] = byte;
        out[3 + at * 2] = 0;
    }
    return out;
}

/// Runs the Game on the mod to its main-menu capture and returns the frame
/// (RGBA, alpha zero). It must exit 0, report the capture, open the label's
/// text from the mod and lay the main menu out at scale 1. Its output is kept
/// beside the capture as `<name>.log`.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, capture: []const u8, name: []const u8) ![]u8 {
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_MOD_TRACE", "1");
    try environ.put("BK_UI_TRACE", "1");
    const result = try std.process.run(allocator, io, .{
        .argv = &.{ game, "-reference-scene", capture, "-reference-resolution", "1024", "768", "-windowed", "-mod=" ++ mod_name, "-profile=ELKTest" },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    const log_path = try std.mem.concat(allocator, u8, &.{ capture[0 .. capture.len - ".rgba".len], ".log" });
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try expectLine(log, log_path, "BK_REFERENCE_SCENE: capture complete");
    try expectLine(log, log_path, "BK_UI_TRACE: reposition \"ui\\mainmenu\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000");
    if (!opensFromMod(log, label_key ++ ".txt")) {
        std.debug.print("Game ({s}) did not open {s}.txt from the mod; its output is in {s}\n", .{ name, label_key, log_path });
        return error.LabelNotReadFromMod;
    }
    return std.Io.Dir.cwd().readFileAlloc(io, capture, allocator, .limited(16 << 20));
}

fn expectLine(log: []const u8, log_path: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its output is in {s}\n", .{ line, log_path });
    return error.GameLineMissing;
}

/// Whether a BK_MOD_TRACE line says the MOD overlay answered `name`, the
/// game's spelling (backslashes, any case) compared without regard to either.
fn opensFromMod(log: []const u8, name: []const u8) bool {
    const prefix = "BK_MOD_TRACE: open \"";
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trimEnd(u8, raw, "\r");
        if (!std.mem.startsWith(u8, line, prefix) or !std.mem.endsWith(u8, line, "\" from MOD")) continue;
        const opened = line[prefix.len .. line.len - "\" from MOD".len];
        if (opened.len != name.len) continue;
        for (opened, name) |a, b| {
            const left = if (a == '\\') '/' else std.ascii.toLower(a);
            if (left != std.ascii.toLower(b)) break;
        } else return true;
    }
    return false;
}

test "a BK_MOD_TRACE line names the label whatever its slashes and case" {
    const log = "BK_MOD_TRACE: open \"Textes\\Strings\\imheader-gbutton-exitgame.txt\" from MOD\r\nother\n";
    try testing.expect(opensFromMod(log, label_key ++ ".txt"));
    try testing.expect(!opensFromMod("BK_MOD_TRACE: open \"textes\\strings\\imheader-gbutton-exitgame.txt\" from ELK\n", label_key ++ ".txt"));
}
