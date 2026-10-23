//! `leak-delta-check [--plant N] [--slack S] <exe> <args...>`: proves a Zig-side leak reaches BkMemory's
//! exit report. It runs the executable twice with BK_MEM_REPORT=count, the
//! second time with BK_ZIG_PLANT_LEAK=1 (the host's test seam, which leaks one
//! 4096-byte block from the shared allocator), reads `bk_mem: N leaked
//! block(s)` from stderr and requires the same exit code and
//! planted == clean + 1. `--plant N` leaks N blocks instead and `--slack S`
//! accepts a delta of N +/- S, for hosts whose own count jitters between runs
//! (the Resource Editor's engine threads move it by a block or two).
//!
//! A delta, not a zero: the C++ engine still leaks blocks the tiers tolerate
//! (they run in count mode), so the planted block is proved by the difference
//! of two runs of the same scenario (D083). If the clean count ever proves
//! non-deterministic between runs, fall back to planted > clean with
//! BK_MEM_REPORT=log in the planted run and the plant site in its stderr.
//!
//! Run through `b.addRunArtifact` so build.zig's hermeticity audit never sees
//! a shell; the child is spawned with std.process.run, like the other tools.
const std = @import("std");

const marker = "bk_mem: ";
const marker_tail = " leaked block(s)";

/// The N of the last `bk_mem: N leaked block(s)` line, or null if none.
pub fn parseLeakCount(stderr: []const u8) ?u64 {
    var found: ?u64 = null;
    var at: usize = 0;
    while (std.mem.indexOfPos(u8, stderr, at, marker)) |start| {
        const digits_start = start + marker.len;
        var end = digits_start;
        while (end < stderr.len and std.ascii.isDigit(stderr[end])) end += 1;
        at = digits_start;
        if (end == digits_start) continue;
        if (!std.mem.startsWith(u8, stderr[end..], marker_tail)) continue;
        found = std.fmt.parseInt(u64, stderr[digits_start..end], 10) catch continue;
    }
    return found;
}

const Run = struct { exit_code: u8, leaks: u64 };

fn runOnce(init: std.process.Init, argv: []const []const u8, plant: usize) !Run {
    const arena = init.arena.allocator();
    var env = try init.environ_map.clone(arena);
    try env.put("BK_MEM_REPORT", "count");
    if (plant != 0) {
        try env.put("BK_ZIG_PLANT_LEAK", try std.fmt.allocPrint(arena, "{d}", .{plant}));
    } else {
        _ = env.swapRemove("BK_ZIG_PLANT_LEAK");
    }
    const result = try std.process.run(arena, init.io, .{
        .argv = argv,
        .environ_map = &env,
        .stdout_limit = .limited(16 * 1024 * 1024),
        .stderr_limit = .limited(16 * 1024 * 1024),
    });
    const code: u8 = switch (result.term) {
        .exited => |code| code,
        else => {
            std.debug.print("leak-delta-check: {s} did not exit normally\n{s}\n", .{ argv[0], result.stderr });
            return error.AbnormalExit;
        },
    };
    const leaks = parseLeakCount(result.stderr) orelse {
        std.debug.print("leak-delta-check: no `bk_mem: N leaked block(s)` line in stderr (exit {d})\n{s}\n", .{ code, result.stderr });
        return error.NoLeakLine;
    };
    return .{ .exit_code = code, .leaks = leaks };
}

pub fn main(init: std.process.Init) !void {
    const arena = init.arena.allocator();
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    defer args.deinit();
    _ = args.skip();
    var argv: std.ArrayList([]const u8) = .empty;
    var plant: usize = 1;
    var slack: usize = 0;
    while (args.next()) |arg| {
        if (argv.items.len == 0 and std.mem.eql(u8, arg, "--plant")) {
            plant = try std.fmt.parseInt(usize, args.next() orelse return error.MissingValue, 10);
        } else if (argv.items.len == 0 and std.mem.eql(u8, arg, "--slack")) {
            slack = try std.fmt.parseInt(usize, args.next() orelse return error.MissingValue, 10);
        } else {
            try argv.append(arena, arg);
        }
    }
    if (argv.items.len == 0 or plant == 0) {
        std.debug.print("usage: leak-delta-check [--plant N] [--slack S] <exe> <args...>\n", .{});
        std.process.exit(2);
    }

    const clean = try runOnce(init, argv.items, 0);
    const planted = try runOnce(init, argv.items, plant);
    const measured = "leak-delta-check: clean {d} leaked block(s) (exit {d}), planted {d} (exit {d})\n";
    if (clean.exit_code != 0 or planted.exit_code != clean.exit_code) {
        std.debug.print(measured, .{ clean.leaks, clean.exit_code, planted.leaks, planted.exit_code });
        std.debug.print("leak-delta-check: FAIL, the runs must both exit 0\n", .{});
        std.process.exit(1);
    }
    const low = clean.leaks + plant -| slack;
    const high = clean.leaks + plant + slack;
    if (planted.leaks < low or planted.leaks > high) {
        std.debug.print(measured, .{ clean.leaks, clean.exit_code, planted.leaks, planted.exit_code });
        std.debug.print("leak-delta-check: FAIL, planted must be in [{d}, {d}] (clean + {d} +/- {d})\n", .{ low, high, plant, slack });
        std.process.exit(1);
    }
}

test "parseLeakCount reads the last count line" {
    try std.testing.expectEqual(@as(?u64, null), parseLeakCount("nothing here"));
    try std.testing.expectEqual(@as(?u64, 0), parseLeakCount("warning(BkMemory): bk_mem: 0 leaked block(s)\n"));
    try std.testing.expectEqual(@as(?u64, 7), parseLeakCount("bk_mem: 3 leaked block(s)\nx\nbk_mem: 7 leaked block(s)\n"));
    try std.testing.expectEqual(@as(?u64, null), parseLeakCount("bk_mem: leaked block(s)"));
}
