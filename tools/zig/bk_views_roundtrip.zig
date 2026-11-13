//! bk-views-roundtrip: the building views round trip on tracked buildings.
//! For each variant folder: `bk-views decompose` into a/, `bk-views-check`
//! (the engine's own reading of the packs against a/), `bk-sprite
//! compose-pack` of every damage state's sprite, shadow and noise pictures
//! into b/, then
//!
//! - every .san and _h.dds in b/ must equal the shipped one byte for byte
//!   (the 2003 export, redone from the decomposed pictures), and
//! - `bk-views decompose` of b/ into c/ must equal a/ byte for byte.
//!
//! Silent on success; every difference is one line on stderr and exit 1. A variant's
//! a/, b/ and c/ stay in <work-dir> only where it differed: kept for all 412 full 3D
//! objects they came to 6 GiB that nothing read.
//!
//! With `--kit=<kit-views-dir>` the folders are what `bk-views kit` wrote for one
//! building (its own folder, its turns under every light and their shadows alone,
//! tools/zig/fixtures/bk-views/kit-views): each folder makes the round trip above,
//! and each state the kit views hold must also decompose to the kit view of the
//! folder's turn and light, pixel for pixel with its zero cross on the pack's, and
//! its shadow to the kit view's shadow where the picture does not cover it.
//!
//! usage: bk-views-roundtrip <bk-views> <bk-sprite> <bk-views-check> <module-root> <work-dir> [--kit=<kit-views-dir>] <variant-dir>...

const std = @import("std");
const bk_views = @import("bk_views.zig");

const Building = struct {
    canvas: [2]i32,
    anchor: [2]i32,
    origin: [2]f64,
    locked_center: [2]i32,
    passability: []const []const u8,
    pictures: []const []const u8,
};

var differences: usize = 0;

fn differ(comptime fmt: []const u8, args: anytype) void {
    std.debug.print("bk-views-roundtrip: " ++ fmt ++ "\n", args);
    differences += 1;
}

pub fn main(init: std.process.Init) !u8 {
    const arena = init.arena.allocator();
    const io = init.io;
    const args = try std.process.Args.toSlice(init.minimal.args, arena);
    const kit: ?[]const u8 = if (args.len > 6 and std.mem.startsWith(u8, args[6], "--kit=")) args[6]["--kit=".len..] else null;
    const first: usize = if (kit != null) 7 else 6;
    if (args.len < first + 1) {
        std.debug.print("usage: bk-views-roundtrip <bk-views> <bk-sprite> <bk-views-check> <module-root> <work-dir> [--kit=<kit-views-dir>] <variant-dir>...\n", .{});
        return 2;
    }
    const views, const sprite, const check, const modules, const work = args[1..6].*;
    for (args[first..]) |variant| {
        const before = differences;
        try roundTrip(arena, io, views, sprite, check, modules, work, variant, kit);
        if (differences == before) std.Io.Dir.cwd().deleteTree(io, try std.fs.path.join(arena, &.{ work, std.fs.path.basename(variant) })) catch {};
    }
    return if (differences == 0) 0 else 1;
}

fn roundTrip(arena: std.mem.Allocator, io: std.Io, views: []const u8, sprite: []const u8, check: []const u8, modules: []const u8, work: []const u8, variant: []const u8, kit: ?[]const u8) !void {
    const name = std.fs.path.basename(variant);
    const a = try std.fs.path.join(arena, &.{ work, name, "a" });
    const b = try std.fs.path.join(arena, &.{ work, name, "b" });
    const c = try std.fs.path.join(arena, &.{ work, name, "c" });
    // A turn's shadow alone (`bk-views side`'s `<k>s<s>`, `<variant>_r<k>_s<s>`)
    // holds shadow packs and no picture; turn 0 of an object without a full 3D
    // model has none, and no folder.
    const alone = shadowAlone(name);
    if (alone) std.Io.Dir.cwd().access(io, variant, .{}) catch return;
    if (!try runTool(arena, io, &.{ views, "decompose", variant, a })) return;
    _ = try runTool(arena, io, &.{ check, try withoutStrays(arena, io, kit != null, work, variant), a, modules });

    const json = try std.Io.Dir.cwd().readFileAlloc(io, try std.fs.path.join(arena, &.{ a, "building.json" }), arena, .limited(16 * 1024 * 1024));
    const building = try std.json.parseFromSliceLeaky(Building, arena, json, .{});
    const zero = try std.fmt.allocPrint(arena, "{d},{d}", .{ building.anchor[0], building.anchor[1] });
    const locked = try std.fmt.allocPrint(arena, "{d},{d}", .{ building.locked_center[0], building.locked_center[1] });
    const pass = try std.mem.join(arena, "/", building.passability);
    var packs: std.ArrayList([]const u8) = .empty;
    var kit_states: usize = 0;
    for (building.pictures) |picture| {
        const pack = picture[0..std.mem.lastIndexOfScalar(u8, picture, '_').?];
        try packs.append(arena, pack);
        if (alone) {
            try shadowTrip(arena, io, sprite, modules, work, name, a, b, pack, zero, pass, locked);
            // A kit folder's shadows alone are the kit views' of its turn and light.
            if (kit) |kit_dir| {
                if (pack.len > 1 and pack[pack.len - 1] == 's' and try kitShadow(arena, io, kit_dir, name, pack[0 .. pack.len - 1], a, building.anchor)) kit_states += 1;
            }
            continue;
        }
        // A damage state is a pack named by its number, `w` after it in winter; its shadow and
        // noise follow its name.
        const digits = std.mem.indexOfNone(u8, pack, "0123456789") orelse pack.len;
        if (digits == 0 or !(digits == pack.len or std.mem.eql(u8, pack[digits..], "w"))) continue;
        // A kit folder's states are the kit views of its turn and light, their shadows with them.
        if (kit) |kit_dir| {
            if (try kitPicture(arena, io, kit_dir, name, pack, a, building.anchor)) {
                kit_states += 1;
                _ = try kitShadow(arena, io, kit_dir, name, pack, a, building.anchor);
            }
        }
        // compose-pack needs a shadow; a state without one (a pillbox) gets
        // a stand-in covering the canvas, whose pack is not compared.
        var shadow = try picturePath(arena, a, pack, "s");
        if (std.Io.Dir.cwd().access(io, shadow, .{})) |_| {} else |_| {
            const canvas = try bk_views.readStoredPng(arena, try std.Io.Dir.cwd().readFileAlloc(io, try picturePath(arena, a, pack, ""), arena, .limited(256 * 1024 * 1024)));
            @memset(canvas.pixels, .{ 0, 0, 0, 255 });
            shadow = try std.fs.path.join(arena, &.{ work, name, try std.fmt.allocPrint(arena, "{s}s_stand_in.png", .{pack}) });
            try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = shadow, .data = try bk_views.writePng(arena, canvas) });
        }
        var argv: std.ArrayList([]const u8) = .empty;
        try argv.appendSlice(arena, &.{
            sprite,                                     "compose-pack",
            try picturePath(arena, a, pack, ""),        shadow,
            try std.fs.path.join(arena, &.{ b, pack }), "--zero",
            zero,                                       "--force",
            "--modules",                                modules,
        });
        if (pass.len > 0) try argv.appendSlice(arena, &.{ "--pass", pass, "--locked", locked });
        const noise = try picturePath(arena, a, pack, "g");
        if (std.Io.Dir.cwd().access(io, noise, .{})) |_| try argv.appendSlice(arena, &.{ "--noise", noise }) else |_| {}
        _ = try runTool(arena, io, argv.items);
        if (std.mem.endsWith(u8, shadow, "_stand_in.png")) for ([_][]const u8{ ".san", "_c.dds", "_h.dds", "_l.dds" }) |suffix| {
            std.Io.Dir.cwd().deleteFile(io, try std.fmt.allocPrint(arena, "{s}s{s}", .{ try std.fs.path.join(arena, &.{ b, pack }), suffix })) catch {};
        };
    }
    if (kit != null and kit_states == 0) differ("{s}: the kit views hold none of its states", .{variant});
    for (packs.items) |pack| {
        for ([_][]const u8{ ".san", "_h.dds" }) |suffix| {
            const file = try std.fmt.allocPrint(arena, "{s}{s}", .{ pack, suffix });
            try compare(arena, io, try std.fs.path.join(arena, &.{ variant, file }), try std.fs.path.join(arena, &.{ b, file }));
        }
    }
    // The stats are not composed; b/ takes the shipped ones for building.json.
    try std.Io.Dir.copyFile(std.Io.Dir.cwd(), try std.fs.path.join(arena, &.{ variant, "1.xml" }), std.Io.Dir.cwd(), try std.fs.path.join(arena, &.{ b, "1.xml" }), io, .{});
    if (!try runTool(arena, io, &.{ views, "decompose", b, c })) return;
    var dir = try std.Io.Dir.cwd().openDir(io, a, .{ .iterate = true });
    defer dir.close(io);
    var iterator = dir.iterate();
    while (try iterator.next(io)) |entry| {
        try compare(arena, io, try std.fs.path.join(arena, &.{ a, entry.name }), try std.fs.path.join(arena, &.{ c, entry.name }));
    }
}


/// The folder the engine's check reads: `variant`, or for a kit folder that
/// holds a pack without its texture a copy without that pack. A kit building's
/// own folder is a copy of its 2003 one, and ussr/summer/townhouse03_1 ships
/// 2gw.san and 3gw.san with no `_h.dds` (its winter ground pictures are 2wg
/// and 3wg): the Game never draws them, `decompose` passes them over, and the
/// check, which reads every .san, would stop at them.
fn withoutStrays(arena: std.mem.Allocator, io: std.Io, kit: bool, work: []const u8, variant: []const u8) ![]const u8 {
    if (!kit) return variant;
    const cwd = std.Io.Dir.cwd();
    var dir = try cwd.openDir(io, variant, .{ .iterate = true });
    defer dir.close(io);
    var names: std.ArrayList([]const u8) = .empty;
    var strays: usize = 0;
    var iterator = dir.iterate();
    while (try iterator.next(io)) |entry| {
        if (entry.kind != .file) continue;
        if (std.ascii.endsWithIgnoreCase(entry.name, ".san")) {
            const texture = try std.fmt.allocPrint(arena, "{s}_h.dds", .{entry.name[0 .. entry.name.len - 4]});
            if (cwd.access(io, try std.fs.path.join(arena, &.{ variant, texture }), .{})) |_| {} else |_| {
                strays += 1;
                continue;
            }
        }
        try names.append(arena, try arena.dupe(u8, entry.name));
    }
    if (strays == 0) return variant;
    const copy = try std.fs.path.join(arena, &.{ work, std.fs.path.basename(variant), "v" });
    try cwd.createDirPath(io, copy);
    for (names.items) |name| try std.Io.Dir.copyFile(cwd, try std.fs.path.join(arena, &.{ variant, name }), cwd, try std.fs.path.join(arena, &.{ copy, name }), io, .{});
    return copy;
}

/// The turn and the light index a kit folder shows: `<variant>` turn 0 at
/// light 0, `<variant>_r<k>` turn k at light 0, `<variant>_r<k>_l<l>` and the
/// shadow alone `<variant>_r<k>_s<l>` turn k at light l.
const KitFolder = struct { turn: usize, light: usize };
fn kitFolder(name: []const u8) KitFolder {
    var stem = name;
    var light: usize = 0;
    if (stem.len > 3 and stem[stem.len - 3] == '_' and (stem[stem.len - 2] == 'l' or stem[stem.len - 2] == 's') and stem[stem.len - 1] >= '0' and stem[stem.len - 1] <= '3') {
        light = stem[stem.len - 1] - '0';
        stem = stem[0 .. stem.len - 3];
    }
    if (stem.len > 3 and stem[stem.len - 3] == '_' and stem[stem.len - 2] == 'r' and stem[stem.len - 1] >= '0' and stem[stem.len - 1] <= '3') return .{ .turn = stem[stem.len - 1] - '0', .light = light };
    return .{ .turn = 0, .light = 0 };
}

/// A kit view of the folder's turn and light: its picture (`kind` "l") or its
/// shadow ("s") and the zero cross in it; null where the kit views do not hold
/// `state`.
const KitFixture = struct { path: []const u8, image: bk_views.Image, zero: [2]i32 };
fn kitFixture(arena: std.mem.Allocator, io: std.Io, kit: []const u8, name: []const u8, state: []const u8, kind: []const u8) !?KitFixture {
    const cwd = std.Io.Dir.cwd();
    const folder = kitFolder(name);
    const views_json = try cwd.readFileAlloc(io, try std.fs.path.join(arena, &.{ kit, "views.json" }), arena, .limited(16 * 1024 * 1024));
    const parsed = try std.json.parseFromSliceLeaky(std.json.Value, arena, views_json, .{});
    const states = parsed.object.get("states") orelse return error.NoStates;
    const turns = states.object.get(state) orelse return null;
    const path = try std.fs.path.join(arena, &.{ kit, try std.fmt.allocPrint(arena, "{s}_{d}_{s}{d}.png", .{ state, folder.turn, kind, folder.light }) });
    const of_turn = turns.object.get(try std.fmt.allocPrint(arena, "{d}", .{folder.turn})) orelse return error.KitTurnMissing;
    const lights = of_turn.object.get(kind) orelse return error.KitLightsMissing;
    const view = lights.object.get(try std.fmt.allocPrint(arena, "{d}", .{folder.light})) orelse return error.KitLightMissing;
    const placed = try std.json.parseFromValueLeaky(struct { zero: [2]i32 }, arena, view, .{ .ignore_unknown_fields = true });
    return .{ .path = path, .image = try bk_views.readRgbaPng(arena, try cwd.readFileAlloc(io, path, arena, .limited(256 * 1024 * 1024))), .zero = placed.zero };
}

/// Whether the kit views hold `state`, and if so that the folder's decomposed
/// picture of it (a/, its zero cross at `anchor`) is the kit view of the
/// folder's turn and light laid zero on zero: every pixel of the view of alpha
/// 128 or more there with its colour, and nothing else.
fn kitPicture(arena: std.mem.Allocator, io: std.Io, kit: []const u8, name: []const u8, state: []const u8, a: []const u8, anchor: [2]i32) !bool {
    const cwd = std.Io.Dir.cwd();
    const found = (kitFixture(arena, io, kit, name, state, "l") catch |err| {
        differ("{s}/{s}: the kit views {s} do not hold its picture: {s}", .{ name, state, kit, @errorName(err) });
        return true;
    }) orelse return false;
    const placed = found;
    const fixture_path = found.path;
    const fixture = found.image;
    const read = try bk_views.readStoredPng(arena, try cwd.readFileAlloc(io, try picturePath(arena, a, state, ""), arena, .limited(256 * 1024 * 1024)));
    // The view's pixels in the pack's picture, and the pack's in the view.
    var missing: usize = 0;
    var extra: usize = 0;
    var recoloured: usize = 0;
    var opaque_pixels: usize = 0;
    for (0..fixture.height) |y| for (0..fixture.width) |x| {
        const want = fixture.pixels[y * fixture.width + x];
        if (want[3] < 128) continue;
        opaque_pixels += 1;
        const px = @as(i64, @intCast(x)) - placed.zero[0] + anchor[0];
        const py = @as(i64, @intCast(y)) - placed.zero[1] + anchor[1];
        if (px < 0 or py < 0 or px >= read.width or py >= read.height) {
            missing += 1;
            continue;
        }
        const got = read.pixels[@as(usize, @intCast(py)) * read.width + @as(usize, @intCast(px))];
        if (got[3] < 128) missing += 1 else if (!std.mem.eql(u8, got[0..3], want[0..3])) recoloured += 1;
    };
    for (0..read.height) |y| for (0..read.width) |x| {
        if (read.pixels[y * read.width + x][3] == 0) continue;
        const kx = @as(i64, @intCast(x)) - anchor[0] + placed.zero[0];
        const ky = @as(i64, @intCast(y)) - anchor[1] + placed.zero[1];
        if (kx < 0 or ky < 0 or kx >= fixture.width or ky >= fixture.height or fixture.pixels[@as(usize, @intCast(ky)) * fixture.width + @as(usize, @intCast(kx))][3] < 128) extra += 1;
    };
    if (opaque_pixels == 0 or missing + extra + recoloured != 0)
        differ("{s}/{s}: its picture is not the kit view {s} laid zero on zero: of {d} pixels {d} missing, {d} recoloured, and {d} that are not the view's", .{ name, state, fixture_path, opaque_pixels, missing, recoloured, extra });
    return true;
}

/// Whether the kit views hold `state`, and if so that the folder's decomposed
/// shadow of it (a/, its zero cross at `anchor`) is the kit view's shadow of
/// the folder's turn and light laid zero on zero, where the kit view's picture
/// does not cover it: compose-pack keeps a shadow outside its picture only. A
/// shadow that lies all under its picture keeps one pixel below the picture
/// (keepShadow), and a model that shades no ground has no shadow pack.
fn kitShadow(arena: std.mem.Allocator, io: std.Io, kit: []const u8, name: []const u8, state: []const u8, a: []const u8, anchor: [2]i32) !bool {
    const cwd = std.Io.Dir.cwd();
    const shadow = (kitFixture(arena, io, kit, name, state, "s") catch |err| {
        differ("{s}/{s}s: the kit views {s} do not hold its shadow: {s}", .{ name, state, kit, @errorName(err) });
        return true;
    }) orelse return false;
    const picture = (try kitFixture(arena, io, kit, name, state, "l")).?;
    // The pack's shadow; a state without one decomposes to no picture.
    const read: ?bk_views.Image = if (cwd.readFileAlloc(io, try picturePath(arena, a, state, "s"), arena, .limited(256 * 1024 * 1024))) |bytes| try bk_views.readStoredPng(arena, bytes) else |_| null;
    const covered = struct {
        fn at(image: bk_views.Image, zero: [2]i32, x: i64, y: i64, least: u8) bool {
            const ix = x + zero[0];
            const iy = y + zero[1];
            if (ix < 0 or iy < 0 or ix >= image.width or iy >= image.height) return false;
            return image.pixels[@as(usize, @intCast(iy)) * image.width + @as(usize, @intCast(ix))][3] >= least;
        }
    }.at;
    // The view's shadow outside its picture, in the pack's shadow.
    var shown: usize = 0;
    var missing: usize = 0;
    for (0..shadow.image.height) |y| for (0..shadow.image.width) |x| {
        if (shadow.image.pixels[y * shadow.image.width + x][3] == 0) continue;
        const gx = @as(i64, @intCast(x)) - shadow.zero[0];
        const gy = @as(i64, @intCast(y)) - shadow.zero[1];
        if (covered(picture.image, picture.zero, gx, gy, 128)) continue;
        shown += 1;
        if (read == null or !covered(read.?, anchor, gx, gy, 1)) missing += 1;
    };
    // The pack's shadow that is not the view's.
    var extra: usize = 0;
    if (read) |r| for (0..r.height) |y| for (0..r.width) |x| {
        if (r.pixels[y * r.width + x][3] == 0) continue;
        const gx = @as(i64, @intCast(x)) - anchor[0];
        const gy = @as(i64, @intCast(y)) - anchor[1];
        if (!covered(shadow.image, shadow.zero, gx, gy, 1) or covered(picture.image, picture.zero, gx, gy, 128)) extra += 1;
    };
    if (missing != 0 or extra > @as(usize, if (shown == 0) 1 else 0))
        differ("{s}/{s}s: its shadow is not the kit view's {s} outside its picture, laid zero on zero: of {d} pixels {d} missing, and {d} that are not the view's", .{ name, state, shadow.path, shown, missing, extra });
    return true;
}

/// Whether a folder is a turn's shadow alone, `<variant>_r<k>_s<s>`.
fn shadowAlone(name: []const u8) bool {
    return name.len > 6 and std.mem.eql(u8, name[name.len - 6 .. name.len - 4], "_r") and name[name.len - 3] == '_' and name[name.len - 2] == 's';
}

/// Composes the shadow pack `pack` (`<state>s`) of a turn's shadow alone back
/// into b/: compose-pack keeps a shadow only outside its picture and packs no
/// empty picture, so the picture is one opaque pixel off the shadow, whose
/// pack is dropped.
fn shadowTrip(arena: std.mem.Allocator, io: std.Io, sprite: []const u8, modules: []const u8, work: []const u8, name: []const u8, a: []const u8, b: []const u8, pack: []const u8, zero: []const u8, pass: []const u8, locked: []const u8) !void {
    if (pack.len < 2 or pack[pack.len - 1] != 's') return differ("{s}/{s}: a shadow alone holds a pack that is no shadow", .{ name, pack });
    const state = pack[0 .. pack.len - 1];
    const shadow = try picturePath(arena, a, state, "s");
    const canvas = try bk_views.readStoredPng(arena, try std.Io.Dir.cwd().readFileAlloc(io, shadow, arena, .limited(256 * 1024 * 1024)));
    const off = for (canvas.pixels, 0..) |p, i| {
        if (p[3] == 0) break i;
    } else return differ("{s}/{s}: the shadow covers its canvas", .{ name, pack });
    @memset(canvas.pixels, .{ 0, 0, 0, 0 });
    canvas.pixels[off] = .{ 255, 255, 255, 255 };
    const stand_in = try std.fs.path.join(arena, &.{ work, name, try std.fmt.allocPrint(arena, "{s}_stand_in.png", .{state}) });
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = stand_in, .data = try bk_views.writePng(arena, canvas) });
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(arena, &.{ sprite, "compose-pack", stand_in, shadow, try std.fs.path.join(arena, &.{ b, state }), "--zero", zero, "--force", "--modules", modules });
    if (pass.len > 0) try argv.appendSlice(arena, &.{ "--pass", pass, "--locked", locked });
    _ = try runTool(arena, io, argv.items);
    for ([_][]const u8{ ".san", "_c.dds", "_h.dds", "_l.dds" }) |suffix| {
        std.Io.Dir.cwd().deleteFile(io, try std.fmt.allocPrint(arena, "{s}{s}", .{ try std.fs.path.join(arena, &.{ b, state }), suffix })) catch {};
    }
}

fn picturePath(arena: std.mem.Allocator, dir: []const u8, pack: []const u8, suffix: []const u8) ![]const u8 {
    return std.fs.path.join(arena, &.{ dir, try std.fmt.allocPrint(arena, "{s}{s}_0.png", .{ pack, suffix }) });
}

/// Runs a tool; a failure is a difference with the tool's own message.
fn runTool(arena: std.mem.Allocator, io: std.Io, argv: []const []const u8) !bool {
    const result = try std.process.run(arena, io, .{ .argv = argv, .stdout_limit = .limited(1024 * 1024), .stderr_limit = .limited(1024 * 1024) });
    if (result.term == .exited and result.term.exited == 0) return true;
    differ("{s} {s} failed:\n{s}", .{ std.fs.path.basename(argv[0]), argv[1], result.stderr });
    return false;
}

fn compare(arena: std.mem.Allocator, io: std.Io, expected_path: []const u8, actual_path: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const expected = try cwd.readFileAlloc(io, expected_path, arena, .limited(256 * 1024 * 1024));
    const actual = cwd.readFileAlloc(io, actual_path, arena, .limited(256 * 1024 * 1024)) catch |err|
        return differ("{s}: {s}", .{ actual_path, @errorName(err) });
    if (std.mem.indexOfDiff(u8, expected, actual)) |at|
        differ("{s} differs from {s} at byte {d} (sizes {d} and {d})", .{ actual_path, expected_path, at, actual.len, expected.len });
}
