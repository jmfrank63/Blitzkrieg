//! Compares `zig build -l` with a tracked baseline step list.
//!
//! The Zig 0.17 build-API migration must not add, drop or rename a build
//! step. The 0.16 list from 4d7645fc5 is tracked as
//! .planning/gsd2/zig-0.16-steps.txt; this tool runs `zig build -l` (the zig
//! on PATH, in the current directory, which must be the repository root),
//! parses both lists into name -> description and prints every step that is
//! missing, extra or changed. Whitespace is normalised because a newer Zig may
//! pad the description column differently. It exits non-zero on any
//! difference and when `zig build -l` itself fails, so a broken configure step
//! cannot pass as "no difference".
//!
//! usage: check_step_list <baseline file>
const std = @import("std");

const Steps = std.StringArrayHashMapUnmanaged([]const u8);

/// One line is "  <name><padding><description>"; collapse the padding and any
/// other run of blanks so only the words are compared.
fn parse(arena: std.mem.Allocator, text: []const u8) !Steps {
    var steps: Steps = .empty;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0) continue;
        var words = std.mem.tokenizeAny(u8, line, " \t");
        const name = words.next().?;
        var description: std.ArrayList(u8) = .empty;
        while (words.next()) |word| {
            if (description.items.len != 0) try description.append(arena, ' ');
            try description.appendSlice(arena, word);
        }
        try steps.put(arena, name, description.items);
    }
    return steps;
}

pub fn main(init: std.process.Init) !void {
    var arena_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = init.io;

    var iterator = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    defer iterator.deinit();
    _ = iterator.skip();
    const baseline_path = iterator.next() orelse {
        std.debug.print("usage: check_step_list <baseline file>\n", .{});
        std.process.exit(2);
    };

    const baseline_text = try std.Io.Dir.cwd().readFileAlloc(io, baseline_path, arena, .limited(1024 * 1024));
    const result = try std.process.run(arena, io, .{
        .argv = &.{ "zig", "build", "-l" },
        .stdout_limit = .limited(1024 * 1024),
        .stderr_limit = .limited(1024 * 1024),
    });
    switch (result.term) {
        .exited => |code| if (code == 0) {} else {
            std.debug.print("zig build -l failed ({d})\n{s}\n", .{ code, result.stderr });
            std.process.exit(1);
        },
        else => {
            std.debug.print("zig build -l did not exit normally\n{s}\n", .{result.stderr});
            std.process.exit(1);
        },
    }

    const baseline = try parse(arena, baseline_text);
    const current = try parse(arena, result.stdout);

    var differences: usize = 0;
    for (baseline.keys(), baseline.values()) |name, description| {
        if (current.get(name)) |now| {
            if (!std.mem.eql(u8, description, now)) {
                std.debug.print("changed: {s}\n  was: {s}\n  now: {s}\n", .{ name, description, now });
                differences += 1;
            }
        } else {
            std.debug.print("missing: {s}\n", .{name});
            differences += 1;
        }
    }
    for (current.keys()) |name| {
        if (!baseline.contains(name)) {
            std.debug.print("extra: {s}\n", .{name});
            differences += 1;
        }
    }

    if (differences != 0) {
        std.debug.print("{d} step difference(s) against {s}\n", .{ differences, baseline_path });
        std.process.exit(1);
    }
    std.debug.print("{d} steps match {s}\n", .{ baseline.count(), baseline_path });
}
