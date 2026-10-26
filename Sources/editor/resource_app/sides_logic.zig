//! The Building editor's Sides view without a window (M005 S08): where a
//! building variant and its three turns live, which pack a cell shows, the
//! picture a pack holds, and the command lines of Generate sides (`bk-views
//! side`) and Import side picture (`bk-sprite compose-pack`). The layout is
//! the one the Game loads (docs/superpowers/specs/2026-10-25-building-views-
//! design.md): side k + 1 of `<variant>` is the sibling folder `<variant>_r<k>`,
//! a variant folder like the exported one with its 1.xml turned.
//! Runs under `zig build test-resource-app-logic`.
const std = @import("std");
const views = @import("bk_views_pack");

/// The variant as exported and its three quarter turns.
pub const cells = 4;

/// The folder of turn `k` (0 is the variant itself).
pub fn turnFolder(buffer: []u8, variant: []const u8, k: usize) ![]const u8 {
    if (k == 0) return std.fmt.bufPrint(buffer, "{s}", .{variant});
    return std.fmt.bufPrint(buffer, "{s}_r{d}", .{ variant, k });
}

/// Where the export writes a building project: `<export dir>/data/buildings/
/// <the project's folder>` (StatsFileName with no export_file_name of its own).
pub fn defaultVariant(buffer: []u8, export_dir: []const u8, project_path: []const u8) ?[]const u8 {
    if (export_dir.len == 0) return null;
    const folder = std.fs.path.basename(std.fs.path.dirname(project_path) orelse return null);
    return std.fmt.bufPrint(buffer, "{s}/data/buildings/{s}", .{ std.mem.trimEnd(u8, export_dir, "/\\"), folder }) catch null;
}

/// What a cell shows: damage state 1 (intact) to 9, in winter, its shadow.
pub const Look = struct {
    state: u8 = 1,
    winter: bool = false,
    shadow: bool = false,

    /// The pack's name: `2`, `2w`, `2s`, `2ws`.
    pub fn pack(self: Look, buffer: *[4]u8) []const u8 {
        return std.fmt.bufPrint(buffer, "{d}{s}{s}", .{ self.state, if (self.winter) "w" else "", if (self.shadow) "s" else "" }) catch unreachable;
    }

    /// The picture Import replaces: the state's sprite, never its shadow.
    pub fn sprite(self: Look) Look {
        return .{ .state = self.state, .winter = self.winter };
    }
};

/// A pack's picture on the canvas `bk-views decompose` writes: its zero cross
/// at `anchor`.
pub const Picture = struct { image: views.Image, anchor: [2]i32 };

/// The picture of `<folder>/<name>.san` with its `_h.dds`, all in `arena`;
/// null when the folder has no such pack.
pub fn readPicture(arena: std.mem.Allocator, io: std.Io, folder: []const u8, name: []const u8) !?Picture {
    const cwd = std.Io.Dir.cwd();
    const san = cwd.readFileAlloc(io, try std.fmt.allocPrint(arena, "{s}/{s}.san", .{ folder, name }), arena, .limited(64 << 20)) catch |err| switch (err) {
        error.FileNotFound => return null,
        else => return err,
    };
    const dds = cwd.readFileAlloc(io, try std.fmt.allocPrint(arena, "{s}/{s}_h.dds", .{ folder, name }), arena, .limited(64 << 20)) catch |err| switch (err) {
        error.FileNotFound => return null,
        else => return err,
    };
    const pack = try views.decodePack(arena, san);
    if (pack.sprites.len == 0) return null;
    const sprite = pack.sprites[0];
    var bounds: views.Bounds = .{};
    for (sprite.squares) |square| bounds.add(square);
    bounds.min_x = -sprite.center[0];
    bounds.min_y = -sprite.center[1];
    bounds.max_x = @max(bounds.max_x, bounds.min_x + 1);
    bounds.max_y = @max(bounds.max_y, bounds.min_y + 1);
    return .{ .image = try views.unpack(arena, sprite, try views.readDds(arena, dds), bounds), .anchor = sprite.center };
}

/// `picture` moved onto a canvas of `size` whose zero cross is at `anchor`,
/// so a shadow or ground picture lies under an imported sprite as before.
pub fn placed(arena: std.mem.Allocator, picture: Picture, size: [2]u32, anchor: [2]i32) !views.Image {
    const out: views.Image = .{ .width = size[0], .height = size[1], .pixels = try arena.alloc([4]u8, @as(usize, size[0]) * size[1]) };
    @memset(out.pixels, .{ 0, 0, 0, 0 });
    for (0..size[1]) |y| for (0..size[0]) |x| {
        const sx = @as(i64, @intCast(x)) - anchor[0] + picture.anchor[0];
        const sy = @as(i64, @intCast(y)) - anchor[1] + picture.anchor[1];
        if (sx < 0 or sy < 0 or sx >= picture.image.width or sy >= picture.image.height) continue;
        out.pixels[y * size[0] + x] = picture.image.pixels[@intCast(sy * picture.image.width + sx)];
    };
    return out;
}

/// An 8-bit RGB or RGBA PNG, not interlaced (what paint programs write), as
/// RGBA: the picture a modder imports is laid on its shadow before packing.
pub fn readPng(arena: std.mem.Allocator, bytes: []const u8) !views.Image {
    if (bytes.len < 33 or !std.mem.eql(u8, bytes[0..8], "\x89PNG\r\n\x1a\n")) return error.NotAPng;
    var width: u32 = 0;
    var height: u32 = 0;
    var channels: usize = 0;
    var idat: std.ArrayList(u8) = .empty;
    var at: usize = 8;
    while (at + 12 <= bytes.len) {
        const length = std.mem.readInt(u32, bytes[at..][0..4], .big);
        if (at + 12 + length > bytes.len) return error.NotAPng;
        const kind = bytes[at + 4 ..][0..4];
        const data = bytes[at + 8 ..][0..length];
        at += 12 + length;
        if (std.mem.eql(u8, kind, "IHDR")) {
            if (length < 13 or data[8] != 8 or data[12] != 0) return error.UnsupportedPng;
            width = std.mem.readInt(u32, data[0..4], .big);
            height = std.mem.readInt(u32, data[4..8], .big);
            channels = switch (data[9]) {
                2 => 3,
                6 => 4,
                else => return error.UnsupportedPng,
            };
        } else if (std.mem.eql(u8, kind, "IDAT")) try idat.appendSlice(arena, data);
    }
    if (channels == 0 or width == 0 or height == 0) return error.NotAPng;
    const stride = width * channels;
    var input: std.Io.Reader = .fixed(idat.items);
    const window = try arena.alloc(u8, std.compress.flate.max_window_len);
    var inflate: std.compress.flate.Decompress = .init(&input, .zlib, window);
    const raw = try inflate.reader.readAlloc(arena, (stride + 1) * height);
    // Undo each row's filter in place: Sub, Up, Average, Paeth.
    const rows = try arena.alloc(u8, stride * height);
    for (0..height) |y| {
        const filter = raw[y * (stride + 1)];
        const row = rows[y * stride ..][0..stride];
        @memcpy(row, raw[y * (stride + 1) + 1 ..][0..stride]);
        for (row, 0..) |*byte, i| {
            const left: u8 = if (i >= channels) row[i - channels] else 0;
            const up: u8 = if (y > 0) rows[(y - 1) * stride + i] else 0;
            const corner: u8 = if (y > 0 and i >= channels) rows[(y - 1) * stride + i - channels] else 0;
            byte.* +%= switch (filter) {
                0 => 0,
                1 => left,
                2 => up,
                3 => @intCast((@as(u16, left) + up) / 2),
                4 => paeth(left, up, corner),
                else => return error.UnsupportedPng,
            };
        }
    }
    const pixels = try arena.alloc([4]u8, @as(usize, width) * height);
    for (pixels, 0..) |*pixel, i| {
        const p = rows[i * channels ..];
        pixel.* = .{ p[0], p[1], p[2], if (channels == 4) p[3] else 255 };
    }
    return .{ .width = width, .height = height, .pixels = pixels };
}

fn paeth(a: u8, b: u8, c: u8) u8 {
    const p = @as(i16, a) + b - c;
    const pa = @abs(p - a);
    const pb = @abs(p - b);
    const pc = @abs(p - c);
    return if (pa <= pb and pa <= pc) a else if (pb <= pc) b else c;
}

/// The footprint compose-pack packs with: the passability rows of a turn's
/// 1.xml (turned by `bk-views side`'s turnXml) and its grid origin on screen.
pub const Footprint = struct { rows: []const u8, locked: [2]i32 };

pub fn footprint(arena: std.mem.Allocator, xml: []const u8) !Footprint {
    const stats = try views.readStats(arena, xml);
    if (stats.passability.len == 0) return error.NoPassability;
    return .{ .rows = try std.mem.join(arena, "/", stats.passability), .locked = views.lockedCenter(stats.origin) };
}

/// `bk-views side <variant> 1,2,3 <its folder> <work>`: the three turns as
/// sibling folders; the source maps go to `work`, outside the mod.
pub fn sideArgv(arena: std.mem.Allocator, tool: []const u8, variant: []const u8, work: []const u8) ![]const []const u8 {
    const parent = std.fs.path.dirname(variant) orelse ".";
    return arena.dupe([]const u8, &.{ tool, "side", variant, "1,2,3", parent, work });
}

/// `bk-sprite compose-pack` of a new picture for one pack of a turn: the
/// picture's zero cross where the old one had it, the turn's footprint, the
/// old shadow (and ground picture of a damage state) under it, and for the
/// intact summer state the icon.
pub const Compose = struct {
    tool: []const u8,
    picture: []const u8,
    shadow: []const u8,
    out_name: []const u8,
    anchor: [2]i32,
    footprint: Footprint,
    ground: ?[]const u8 = null,
    icon: ?[]const u8 = null,
};

pub fn composeArgv(arena: std.mem.Allocator, c: Compose) ![]const []const u8 {
    var argv: std.ArrayList([]const u8) = .empty;
    const zero = try std.fmt.allocPrint(arena, "{d},{d}", .{ c.anchor[0], c.anchor[1] });
    const locked = try std.fmt.allocPrint(arena, "{d},{d}", .{ c.footprint.locked[0], c.footprint.locked[1] });
    try argv.appendSlice(arena, &.{ c.tool, "compose-pack", c.picture, c.shadow, c.out_name, "--zero", zero, "--pass", c.footprint.rows, "--locked", locked, "--force" });
    if (c.ground) |g| try argv.appendSlice(arena, &.{ "--noise", g });
    if (c.icon) |i| try argv.appendSlice(arena, &.{ "--icon", i });
    return argv.items;
}

const testing = std.testing;

test "a turn's folder is the variant's sibling named _r<k>" {
    var buffer: [64]u8 = undefined;
    try testing.expectEqualStrings("mods/m/data/buildings/house", try turnFolder(&buffer, "mods/m/data/buildings/house", 0));
    try testing.expectEqualStrings("mods/m/data/buildings/house_r3", try turnFolder(&buffer, "mods/m/data/buildings/house", 3));
}

test "the export puts a building under data/buildings by its project's folder" {
    var buffer: [128]u8 = undefined;
    try testing.expectEqualStrings("C:/mods/my/data/buildings/house", defaultVariant(&buffer, "C:/mods/my/", "D:/src/house/project.bld").?);
    try testing.expect(defaultVariant(&buffer, "", "D:/src/house/project.bld") == null);
}

test "a cell's pack names the state, the winter and the shadow" {
    var buffer: [4]u8 = undefined;
    try testing.expectEqualStrings("1", (Look{}).pack(&buffer));
    try testing.expectEqualStrings("3ws", (Look{ .state = 3, .winter = true, .shadow = true }).pack(&buffer));
    try testing.expectEqualStrings("2w", (Look{ .state = 2, .winter = true, .shadow = true }).sprite().pack(&buffer));
}

test "a shipped building's intact pack reads to its picture and zero cross, a missing one to null" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const shed = "Data/Buildings/europe/summer/e_frenchshed01";
    const picture = (try readPicture(arena, testing.io, shed, "1")).?;
    var opaque_pixels: usize = 0;
    for (picture.image.pixels) |p| opaque_pixels += @intFromBool(p[3] >= 128);
    try testing.expect(opaque_pixels > 1000);
    try testing.expect(picture.anchor[0] > 0 and picture.anchor[0] < picture.image.width);
    try testing.expect(try readPicture(arena, testing.io, shed, "9") == null);
    // Moved onto a larger canvas with the zero cross elsewhere, every pixel goes along.
    const moved = try placed(arena, picture, .{ picture.image.width + 10, picture.image.height + 4 }, .{ picture.anchor[0] + 10, picture.anchor[1] + 4 });
    try testing.expectEqual(picture.image.pixels[0], moved.pixels[4 * moved.width + 10]);
    const xml = try std.Io.Dir.cwd().readFileAlloc(testing.io, shed ++ "/1.xml", arena, .limited(1 << 20));
    const print = try footprint(arena, xml);
    try testing.expect(print.rows.len > 0 and std.mem.indexOfNone(u8, print.rows, "0123456789abcdefABCDEF/") == null);
}

test "Generate sides and Import side picture build the tools' command lines" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const side = try sideArgv(arena, "bk-views", "m/data/buildings/house", "m/.views-work");
    try testing.expectEqualStrings("side", side[1]);
    try testing.expectEqualStrings("1,2,3", side[3]);
    try testing.expectEqualStrings("m/data/buildings", side[4]);
    const compose = try composeArgv(arena, .{ .tool = "bk-sprite", .picture = "new.png", .shadow = "s.png", .out_name = "house_r1/2", .anchor = .{ 90, 120 }, .footprint = .{ .rows = "0101/0101", .locked = .{ 3, -4 } }, .ground = "g.png" });
    const want = [_][]const u8{ "bk-sprite", "compose-pack", "new.png", "s.png", "house_r1/2", "--zero", "90,120", "--pass", "0101/0101", "--locked", "3,-4", "--force", "--noise", "g.png" };
    try testing.expectEqual(want.len, compose.len);
    for (want, compose) |a, b| try testing.expectEqualStrings(a, b);
}

test "a PNG reads to its pixels: the stored ones bk-views writes and a compressed, filtered one" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var pixels: [12][4]u8 = undefined;
    for (&pixels, 0..) |*p, i| p.* = .{ @intCast(i * 20), @intCast(255 - i * 9), 7, @intCast(i * 21) };
    const image: views.Image = .{ .width = 4, .height = 3, .pixels = &pixels };
    const back = try readPng(arena, try views.writePng(arena, image));
    try testing.expectEqualSlices([4]u8, &pixels, back.pixels);
    // The tracked import fixture: zlib-compressed, its rows filtered None, Sub, Up, Average and Paeth in
    // turn, 80 x 60 of #10e0a0.
    const fixture = try readPng(arena, try std.Io.Dir.cwd().readFileAlloc(testing.io, "tools/zig/fixtures/resource_editor/bld/side-import.png", arena, .limited(1 << 20)));
    try testing.expectEqual(@as(u32, 80), fixture.width);
    for (fixture.pixels) |p| try testing.expectEqual([4]u8{ 0x10, 0xe0, 0xa0, 0xff }, p);
    try testing.expectError(error.NotAPng, readPng(arena, "not a png at all, not even close to one"));
}
