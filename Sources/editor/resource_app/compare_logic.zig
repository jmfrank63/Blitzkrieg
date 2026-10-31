//! The Remastered compare frame's logic, with no window and no ImGui: the
//! two pictures the compare shows for the selected building state, the strip
//! of the generated turns, and the display transforms of the Remastered
//! pane. The left pane is the shipped 2003 building folder, the right pane a
//! generated turn folder (`<variant>_r<k>`); both are read through
//! sides_logic's pack reader, the same one the Sides window uses, so what
//! the compare shows is what the Game reads. Nothing writes, nothing calls
//! the bridge. Runs under `zig build test-resource-app-logic`.
const std = @import("std");
const sides = @import("sides_logic.zig");
const views = @import("bk_views_pack");

pub const Picture = sides.Picture;
pub const Look = sides.Look;

/// The display-only transform of the Remastered pane. The 2003 pane never
/// transforms: the judgement is "would the mirrored render fit the original
/// as it stands". A mirrored or rotated picture takes its anchor cross with
/// it, so the crosses of the two panes only coincide when the mode is
/// `upright`.
pub const Mode = enum { upright, mirrored, rotated_180 };

/// The picture's pixels under the display transform, the anchor cross with
/// them. Mirror: every row reversed, x -> w-1-x. Rotate 180: the picture
/// reversed both ways. The 2003 pane is never transformed.
pub fn transformPicture(arena: std.mem.Allocator, picture: Picture, mode: Mode) !Picture {
    const w: i32 = @intCast(picture.image.width);
    const h: i32 = @intCast(picture.image.height);
    return switch (mode) {
        .upright => picture,
        .mirrored => .{
            .image = try mirrorImage(arena, picture.image),
            .anchor = .{ w - 1 - picture.anchor[0], picture.anchor[1] },
        },
        .rotated_180 => .{
            .image = try mirrorImage(arena, try flipImage(arena, picture.image)),
            .anchor = .{ w - 1 - picture.anchor[0], h - 1 - picture.anchor[1] },
        },
    };
}

/// The picture mirrored horizontally.
fn mirrorImage(arena: std.mem.Allocator, image: views.Image) !views.Image {
    const out: views.Image = .{ .width = image.width, .height = image.height, .pixels = try arena.alloc([4]u8, image.pixels.len) };
    for (0..image.height) |y| for (0..image.width) |x| {
        out.pixels[y * image.width + x] = image.pixels[y * image.width + (image.width - 1 - x)];
    };
    return out;
}

/// The picture mirrored vertically (rotate 180 = both mirrors).
fn flipImage(arena: std.mem.Allocator, image: views.Image) !views.Image {
    const out: views.Image = .{ .width = image.width, .height = image.height, .pixels = try arena.alloc([4]u8, image.pixels.len) };
    for (0..image.height) |y| for (0..image.width) |x| {
        out.pixels[y * image.width + x] = image.pixels[(image.height - 1 - y) * image.width + x];
    };
    return out;
}

/// The shipped 2003 picture of the state: `<data building folder>/<pack>`,
/// read by the same reader the Sides window uses. Null when the building
/// has no such state's pack.
pub fn originalPicture(arena: std.mem.Allocator, io: std.Io, data_building_folder: []const u8, look: Look) !?Picture {
    var name: [4]u8 = undefined;
    return sides.readPicture(arena, io, data_building_folder, look.pack(&name));
}

/// The generated turn's picture of the state: `<project variant folder>/
/// <variant>_r<k>/<pack>`, the folder layout the Game loads (the Sides
/// window's own). Null when that turn or state was never generated.
pub fn turnPicture(arena: std.mem.Allocator, io: std.Io, project_variant_folder: []const u8, look: Look, k: usize) !?Picture {
    var turn: [128]u8 = undefined;
    const folder = try sides.turnFolder(&turn, project_variant_folder, k);
    var name: [4]u8 = undefined;
    return sides.readPicture(arena, io, folder, look.pack(&name));
}

/// The turns of the selected state that have packs, ascending, at most
/// `sides.cells - 1` entries: the strip's rows. The caller owns the slice.
pub fn stripFor(arena: std.mem.Allocator, io: std.Io, project_variant_folder: []const u8, look: Look) ![]usize {
    var turns: [sides.cells - 1]usize = undefined;
    var count: usize = 0;
    for (1..sides.cells) |k| {
        if (try turnPicture(arena, io, project_variant_folder, look, k) != null) {
            turns[count] = k;
            count += 1;
        }
    }
    return allocatorOwnedTurns(arena, turns[0..count]);
}

fn allocatorOwnedTurns(arena: std.mem.Allocator, turns: []const usize) ![]usize {
    return arena.dupe(usize, turns);
}

test "transformPicture mirrors the pixels and moves the cross" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var pixels: [6][4]u8 = undefined;
    for (&pixels, 0..) |*p, i| p.* = .{ @intCast(i), @intCast(i), @intCast(i), 255 };
    const picture: Picture = .{ .image = .{ .width = 3, .height = 2, .pixels = &pixels }, .anchor = .{ 0, 0 } };
    const mirrored = try transformPicture(arena, picture, .mirrored);
    try std.testing.expectEqual(@as(i32, 2), mirrored.anchor[0]);
    // Row 0's first pixel is now row 0's last.
    try std.testing.expectEqual(pixels[2], mirrored.image.pixels[0]);
    try std.testing.expectEqual(pixels[0], mirrored.image.pixels[2]);
    try std.testing.expectEqual(pixels[3], mirrored.image.pixels[5]);
}

test "transformPicture rotates 180: pixels reversed both ways, cross with them" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    var pixels: [6][4]u8 = undefined;
    for (&pixels, 0..) |*p, i| p.* = .{ @intCast(i), 0, 0, 255 };
    const picture: Picture = .{ .image = .{ .width = 3, .height = 2, .pixels = &pixels }, .anchor = .{ 0, 1 } };
    const rotated = try transformPicture(arena, picture, .rotated_180);
    try std.testing.expectEqual(@as(i32, 2), rotated.anchor[0]);
    try std.testing.expectEqual(@as(i32, 0), rotated.anchor[1]);
    try std.testing.expectEqual(pixels[0], rotated.image.pixels[5]);
    try std.testing.expectEqual(pixels[5], rotated.image.pixels[0]);
}

test "the shipped shed is the original side and a copied pack reads as its turn" {
    var arena_state = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = std.testing.io;
    const shed = "Data/Buildings/europe/summer/e_frenchshed01";
    const original = (try originalPicture(arena, io, shed, .{})).?;
    try std.testing.expect(original.image.width > 0 and original.image.height > 0);

    // The Remastered side, staged the way camera-turn-mod stages a turn: a
    // sibling `<name>_r1` folder with the state's pack copied in.
    const turn_folder = "zig-out/local-test/compare/shed_r1";
    var cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, "zig-out/local-test/compare") catch {};
    try cwd.createDirPath(io, turn_folder);
    for ([_][]const u8{ "1.san", "1_h.dds" }) |file| {
        const bytes = try cwd.readFileAlloc(io, try std.fmt.allocPrint(arena, shed ++ "/{s}", .{file}), arena, .limited(64 << 20));
        try cwd.writeFile(io, .{ .sub_path = try std.fmt.allocPrint(arena, turn_folder ++ "/{s}", .{file}), .data = bytes });
    }
    const turn = (try turnPicture(arena, io, "zig-out/local-test/compare/shed", .{}, 1)).?;
    try std.testing.expectEqual(original.image.width, turn.image.width);
    try std.testing.expectEqual(original.anchor, turn.anchor);

    // The strip knows turn 1 exists and turns 2 and 3 do not.
    const strip = try stripFor(arena, io, "zig-out/local-test/compare/shed", .{});
    try std.testing.expectEqualSlices(usize, &.{1}, strip);
}
