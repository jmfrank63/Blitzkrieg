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
//! usage: bk-views-roundtrip <bk-views> <bk-sprite> <bk-views-check> <module-root> <work-dir> <variant-dir>...

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
    if (args.len < 7) {
        std.debug.print("usage: bk-views-roundtrip <bk-views> <bk-sprite> <bk-views-check> <module-root> <work-dir> <variant-dir>...\n", .{});
        return 2;
    }
    const views, const sprite, const check, const modules, const work = args[1..6].*;
    for (args[6..]) |variant| {
        const before = differences;
        try roundTrip(arena, io, views, sprite, check, modules, work, variant);
        if (differences == before) std.Io.Dir.cwd().deleteTree(io, try std.fs.path.join(arena, &.{ work, std.fs.path.basename(variant) })) catch {};
    }
    return if (differences == 0) 0 else 1;
}

fn roundTrip(arena: std.mem.Allocator, io: std.Io, views: []const u8, sprite: []const u8, check: []const u8, modules: []const u8, work: []const u8, variant: []const u8) !void {
    const name = std.fs.path.basename(variant);
    const a = try std.fs.path.join(arena, &.{ work, name, "a" });
    const b = try std.fs.path.join(arena, &.{ work, name, "b" });
    const c = try std.fs.path.join(arena, &.{ work, name, "c" });
    if (!try runTool(arena, io, &.{ views, "decompose", variant, a })) return;
    _ = try runTool(arena, io, &.{ check, variant, a, modules });

    const json = try std.Io.Dir.cwd().readFileAlloc(io, try std.fs.path.join(arena, &.{ a, "building.json" }), arena, .limited(16 * 1024 * 1024));
    const building = try std.json.parseFromSliceLeaky(Building, arena, json, .{});
    const zero = try std.fmt.allocPrint(arena, "{d},{d}", .{ building.anchor[0], building.anchor[1] });
    const locked = try std.fmt.allocPrint(arena, "{d},{d}", .{ building.locked_center[0], building.locked_center[1] });
    const pass = try std.mem.join(arena, "/", building.passability);
    var packs: std.ArrayList([]const u8) = .empty;
    for (building.pictures) |picture| {
        const pack = picture[0..std.mem.lastIndexOfScalar(u8, picture, '_').?];
        try packs.append(arena, pack);
        // A damage state is a pack named by its number, `w` after it in winter; its shadow and
        // noise follow its name.
        const digits = std.mem.indexOfNone(u8, pack, "0123456789") orelse pack.len;
        if (digits == 0 or !(digits == pack.len or std.mem.eql(u8, pack[digits..], "w"))) continue;
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
