//! The Game draws the generated building views (M005 S04,
//! docs/superpowers/specs/2026-10-25-building-views-design.md). `bk-views side`
//! makes side 3 of e_ctownhouse01 (from `_2`) and the three other sides of the
//! single-sprite e_frenchshed01 into a scratch mod, whose modobjects.xml
//! registers them as buildings. For each side k a mission map written from the
//! shipped `Data/Maps/river3d.xml` holds side k of the town house (the 2003
//! ones by their own objects.xml names) and side k of the shed, apart, and the
//! real Game draws it under a BK_AUTO_UI schedule and takes a shot.
//! BK_SCENE_DISABLE turns every pass but the objects off (terrain, shadows,
//! fog...), so what is not black is drawn by a building or the interface.
//!
//! The measurement: the frame's drawn pixels against the intact sprite the
//! building's pack holds (`bk-views decompose`, the `_h.dds` the Game loads),
//! its zero cross where the building stands on the screen, at the best offset
//! within a few pixels: the outline IoU, and the mean colour difference where
//! the sprite is opaque. Every side must match its own sprite and match it
//! better than any other side of its family, so a missing building, the wrong
//! picture or a picture drawn wrong fails.
//!
//! The Game runs from a scratch folder with HOME, XDG_*, BK_USER_ROOT,
//! BK_PROFILES_ROOT and BK_MODS_ROOT there; frames (`side<k>.png`), sprites and
//! logs stay in zig-out/local-test/building-views/game. BK_VIEWS_GAME names the
//! installed Game and BK_VIEWS_TOOL bk-views (bk-sprite beside it); `zig build
//! building-views-game-reads-it` sets both.
const std = @import("std");
const views = @import("bk_views.zig");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

/// Its own folder: the batch keeps its work under zig-out/local-test/building-views/batch.
const scratch = "zig-out/local-test/building-views/game";
const mod_name = "building_views_test";
/// Where the generated variants live in the mod's data, and so their objects.xml path.
const mod_folder = "buildings\\views";
const width = 1024;
const height = 768;
/// Where the town house and the shed stand, in the map XML's visual
/// coordinates, and where their zero crosses land on the screen. Measured on
/// the Game: `camera=` takes the AI's coordinates (the visual ones over
/// sqrt 2), so camera 512 puts a building at the map's middle (724, 724) on
/// the screen's (511, 383); a step of (1, 1) moves it 1 pixel right and one of
/// (1, -1) a quarter pixel down. The town house goes 250 pixels left and the
/// shed 250 right, both 50 down, clear of the chat line at the top.
const camera = 512;
const places = [_]struct { at: [2]u32, screen: [2]i32 }{
    .{ .at = .{ 574, 374 }, .screen = .{ 261, 433 } },
    .{ .at = .{ 1074, 874 }, .screen = .{ 761, 433 } },
};

const Side = struct {
    /// Its objects.xml name.
    name: []const u8,
    /// Its variant folder: in Data for a 2003 side, in the mod for a generated one.
    folder: []const u8,
    generated: bool,
};

const families = [_][4]Side{
    .{
        .{ .name = "E_CTownHouse01_1", .folder = "Data/Buildings/europe/summer/e_ctownhouse01_1", .generated = false },
        .{ .name = "E_CTownHouse01_2", .folder = "Data/Buildings/europe/summer/e_ctownhouse01_2", .generated = false },
        .{ .name = "Views_E_CTownHouse01_2_r1", .folder = "e_ctownhouse01_2_r1", .generated = true },
        .{ .name = "E_CTownHouse01_4", .folder = "Data/Buildings/europe/summer/e_ctownhouse01_4", .generated = false },
    },
    .{
        .{ .name = "E_FrenchShed01", .folder = "Data/Buildings/europe/summer/e_frenchshed01", .generated = false },
        .{ .name = "Views_E_FrenchShed01_r1", .folder = "e_frenchshed01_r1", .generated = true },
        .{ .name = "Views_E_FrenchShed01_r2", .folder = "e_frenchshed01_r2", .generated = true },
        .{ .name = "Views_E_FrenchShed01_r3", .folder = "e_frenchshed01_r3", .generated = true },
    },
};

/// Floors from the first passing runs: the worst side scored IoU 0.994 and
/// every side colour 0.00 (the Game draws the `_h.dds` pixels unchanged); the
/// best IoU of a wrong side was 0.654.
const min_iou = 0.95;
const max_colour = 2.0;

/// An opaque sprite pixel; the export keeps alpha 0 or 255 almost everywhere.
const opaque_alpha = 128;
/// A drawn pixel: brighter than the cleared screen and the dark blue band the
/// Game still draws at the top (up to 18 in its blue channel). A sprite pixel
/// counts as drawn by the same rule.
const drawn_level = 24;

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupe(u8, value);
}

const Sprite = struct { image: views.Image, anchor: [2]i32 };

const Score = struct { iou: f64, colour: f64, dx: i32, dy: i32 };

test "the Game draws all four sides of e_ctownhouse01 and e_frenchshed01, the generated ones from a mod" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_VIEWS_GAME")) orelse {
        std.debug.print("BK_VIEWS_GAME is not set: run zig build building-views-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try envValue(allocator, "BK_VIEWS_TOOL")) orelse return error.MissingTool;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const data = try std.fs.path.join(allocator, &.{ root, "mods", mod_name, "data" });
    const buildings = try std.fs.path.join(allocator, &.{ data, "buildings", "views" });
    try cwd.createDirPath(io, buildings);
    try cwd.createDirPath(io, try std.fs.path.join(allocator, &.{ data, "maps" }));

    // The generated sides, straight into the mod.
    try runTool(io, allocator, tool, &.{ "side", "Data/Buildings/europe/summer/e_ctownhouse01_2", "1", buildings });
    try runTool(io, allocator, tool, &.{ "side", "Data/Buildings/europe/summer/e_frenchshed01", "1,2,3", buildings });
    try writeMod(io, allocator, data);

    var sprites: [families.len][4]Sprite = undefined;
    for (families, 0..) |family, f| for (family, 0..) |side, s| {
        const folder = if (side.generated) try std.fs.path.join(allocator, &.{ buildings, side.folder }) else side.folder;
        sprites[f][s] = try decompose(io, allocator, tool, folder, try std.fs.path.join(allocator, &.{ root, "sprites", side.name }));
    };

    var failed = false;
    var report: std.ArrayList(u8) = .empty;
    try report.appendSlice(allocator, "side: IoU / colour, offset from where it stands; best IoU of another side there\n");
    for (0..4) |s| {
        const frame = try runGame(io, allocator, game, root, s + 1);
        for (families, places, 0..) |family, place, f| {
            const own = score(frame, sprites[f][s], place.screen);
            var other_best: f64 = 0;
            for (0..4) |o| {
                if (o != s) other_best = @max(other_best, score(frame, sprites[f][o], place.screen).iou);
            }
            try report.print(allocator, "{s}: {d:.3} / {d:.2} at {d},{d}; {d:.3}\n", .{ family[s].name, own.iou, own.colour, own.dx, own.dy, other_best });
            if (own.iou < min_iou or own.colour > max_colour or other_best >= own.iou) failed = true;
        }
    }
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ root, "scores.txt" }), .data = report.items });
    if (failed) {
        std.debug.print("{s}a side is missing or drawn wrong (IoU floor {d}, colour ceiling {d}); the frames are in {s}\n", .{ report.items, min_iou, max_colour, root });
        return error.DrawnWrong;
    }
}

fn runTool(io: std.Io, allocator: std.mem.Allocator, tool: []const u8, args: []const []const u8) !void {
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.append(allocator, tool);
    try argv.appendSlice(allocator, args);
    const result = try timed_run.run(allocator, io, .{
        .argv = argv.items,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(300), .clock = .awake } },
    });
    if (result.term.success()) return;
    std.debug.print("bk-views {s} ended {any}:\n{s}{s}\n", .{ args[0], result.term, result.stdout, result.stderr });
    return error.ToolFailed;
}

/// The intact sprite of a variant folder as its pack holds it, and its zero cross.
fn decompose(io: std.Io, allocator: std.mem.Allocator, tool: []const u8, folder: []const u8, out: []const u8) !Sprite {
    try runTool(io, allocator, tool, &.{ "decompose", folder, out });
    const cwd = std.Io.Dir.cwd();
    const png = try cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ out, "1_0.png" }), allocator, .limited(64 << 20));
    const json = try cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ out, "building.json" }), allocator, .limited(1 << 20));
    const parsed = try std.json.parseFromSliceLeaky(struct { anchor: [2]i32 }, allocator, json, .{ .ignore_unknown_fields = true });
    return .{ .image = try views.readStoredPng(allocator, png), .anchor = parsed.anchor };
}

/// mod.xml, modobjects.xml with the generated sides, and a map per side,
/// `side<k>.xml`.
fn writeMod(io: std.Io, allocator: std.mem.Allocator, data: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>Building views test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });
    var objects: std.ArrayList(u8) = .empty;
    try objects.appendSlice(allocator, "<?xml version=\"1.0\"?>\r\n<ObjectDB>\r\n\t<Objects>\r\n");
    const map = try cwd.readFileAlloc(io, "Data/Maps/river3d.xml", allocator, .limited(4 << 20));
    try testing.expectEqual(@as(usize, 1), std.mem.count(u8, map, "<Objects/>"));
    for (families) |family| for (family) |side| {
        if (side.generated) try objects.print(allocator, "\t\t<item>\r\n\t\t\t<name>{s}</name>\r\n\t\t\t<type>sprite</type>\r\n\t\t\t<game_type>building</game_type>\r\n\t\t\t<path>{s}\\{s}</path>\r\n\t\t</item>\r\n", .{ side.name, mod_folder, side.folder });
    };
    for (0..4) |s| {
        var items: std.ArrayList(u8) = .empty;
        try items.appendSlice(allocator, "<Objects>");
        for (families, places, 1..) |family, place, link| try items.print(allocator, "<item Direction=\"0\" Player=\"-1\" ScriptID=\"-1\" HP=\"1\" FrameIndex=\"-1\"><Name>{s}</Name>" ++
            "<Position x=\"{d}\" y=\"{d}\" z=\"0\"/><Link LinkID=\"{d}\" Intention=\"0\" LinkWith=\"0\"/></item>", .{ family[s].name, place.at[0], place.at[1], link });
        try items.appendSlice(allocator, "</Objects>");
        try cwd.writeFile(io, .{
            .sub_path = try std.fs.path.join(allocator, &.{ data, "maps", try std.fmt.allocPrint(allocator, "side{d}.xml", .{s + 1}) }),
            .data = try std.mem.replaceOwned(u8, allocator, map, "<Objects/>", items.items),
        });
    }
    try objects.appendSlice(allocator, "\t</Objects>\r\n</ObjectDB>\r\n");
    try cwd.writeFile(io, .{ .sub_path = try std.fs.path.join(allocator, &.{ data, "modobjects.xml" }), .data = objects.items });
}

/// Runs the Game on side k's map and returns its shot (RGBA), also kept as
/// `side<k>.png`; it must exit 0 without a leak.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, root: []const u8, k: usize) ![]u8 {
    const cwd = std.Io.Dir.cwd();
    const name = try std.fmt.allocPrint(allocator, "side{d}", .{k});
    const user = try std.fs.path.join(allocator, &.{ root, "user", name });
    try cwd.createDirPath(io, user);
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ root, name });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("HOME", user);
    try environ.put("XDG_DATA_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-data" }));
    try environ.put("XDG_CONFIG_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-config" }));
    try environ.put("XDG_CACHE_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-cache" }));
    try environ.put("XDG_STATE_HOME", try std.fs.path.join(allocator, &.{ user, "xdg-state" }));
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MODS_ROOT", try std.fs.path.join(allocator, &.{ root, "mods" }));
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_NO_HELP", "1");
    try environ.put("BK_SCENE_DISABLE", "shadows,effects,terrain,noise,warfog,border,haze,grid");
    try environ.put("BK_AUTO_UI", std.fmt.comptimePrint("g1000:camera={d}x{d},g10:hover=1000x400,g1500:shot,g200:exit", .{ camera, camera }));
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-windowed", std.fmt.comptimePrint("-mode={d}x{d}", .{ width, height }), "-profile=Views", "-mod=" ++ mod_name, try std.fmt.allocPrint(allocator, "{s}.xml", .{name}) },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try cwd.writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    const shot_name = std.fmt.comptimePrint("autoshot_g1500_{d}x{d}.rgba", .{ width, height });
    const frame = cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ user, shot_name }), allocator, .limited(64 << 20)) catch |err| {
        std.debug.print("no shot {s} from the Game ({s}); its output is in {s}\n", .{ shot_name, name, log_path });
        return err;
    };
    try testing.expectEqual(@as(usize, width * height * 4), frame.len);
    const image: views.Image = .{ .width = width, .height = height, .pixels = std.mem.bytesAsSlice([4]u8, frame) };
    try cwd.writeFile(io, .{ .sub_path = try std.fmt.allocPrint(allocator, "{s}/{s}.png", .{ root, name }), .data = try views.writePng(allocator, image) });
    return frame;
}

fn drawn(p: []const u8) bool {
    return @max(p[0], p[1], p[2]) > drawn_level;
}

/// The outline IoU and colour of the sprite with its zero cross at (x0, y0)
/// on the frame, within the box of its drawn pixels grown by 16 (the building
/// stands alone there, so drawn pixels in it are its own).
fn scoreAt(frame: []const u8, sprite: Sprite, box: [4]i64, x0: i32, y0: i32) Score {
    var both: f64 = 0;
    var either: f64 = 0;
    var colour: f64 = 0;
    var compared: f64 = 0;
    var y = box[1] - 16;
    while (y < box[3] + 16) : (y += 1) {
        var x = box[0] - 16;
        while (x < box[2] + 16) : (x += 1) {
            const pixel: ?[4]u8 = if (x < 0 or y < 0 or x >= sprite.image.width or y >= sprite.image.height) null else sprite.image.pixels[@intCast(y * sprite.image.width + x)];
            const in_sprite = if (pixel) |p| p[3] >= opaque_alpha and drawn(&p) else false;
            const sx = x + x0 - sprite.anchor[0];
            const sy = y + y0 - sprite.anchor[1];
            const on_screen = sx >= 0 and sy >= 0 and sx < width and sy < height and drawn(frame[@intCast((sy * width + sx) * 4)..][0..3]);
            if (in_sprite and on_screen) {
                both += 1;
                if (pixel.?[3] == 255) {
                    const f = frame[@intCast((sy * width + sx) * 4)..][0..3];
                    for (0..3) |c| colour += @abs(@as(f64, @floatFromInt(f[c])) - @as(f64, @floatFromInt(pixel.?[c])));
                    compared += 3;
                }
            }
            if (in_sprite or on_screen) either += 1;
        }
    }
    return .{ .iou = if (either == 0) 0 else both / either, .colour = if (compared == 0) 255 else colour / compared, .dx = 0, .dy = 0 };
}

/// The best of the offsets within 4 pixels of where the building stands.
fn score(frame: []const u8, sprite: Sprite, screen: [2]i32) Score {
    var box = [4]i64{ sprite.image.width, sprite.image.height, 0, 0 };
    for (sprite.image.pixels, 0..) |p, i| {
        if (p[3] < opaque_alpha or !drawn(&p)) continue;
        const x: i64 = @intCast(i % sprite.image.width);
        const y: i64 = @intCast(i / sprite.image.width);
        box = .{ @min(box[0], x), @min(box[1], y), @max(box[2], x + 1), @max(box[3], y + 1) };
    }
    var best: Score = .{ .iou = -1, .colour = 255, .dx = 0, .dy = 0 };
    var dy: i32 = -4;
    while (dy <= 4) : (dy += 1) {
        var dx: i32 = -4;
        while (dx <= 4) : (dx += 1) {
            var s = scoreAt(frame, sprite, box, screen[0] + dx, screen[1] + dy);
            s.dx = dx;
            s.dy = dy;
            if (s.iou > best.iou) best = s;
        }
    }
    return best;
}
