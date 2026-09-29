//! `delete-matching-files <dir> <prefix> <suffix>`: deletes every file
//! directly in `<dir>` (not recursive) whose name starts with `<prefix>` and
//! ends with `<suffix>`. Best-effort cleanup, run through `b.addRunArtifact`
//! (never `b.addSystemCommand`) so build.zig's own hermeticity audit
//! (tools/zig/build_hermeticity_test.zig) never sees a forbidden shell -
//! `map-editor-auto`'s own step uses this to remove the `autoshot_*.rgba`
//! files Test in game leaves in the stage root (03-12-PLAN.md Task 2).
//!
//! A missing directory, or no matching files, is not an error - there is
//! nothing to clean up either way. A file that will not delete is reported
//! but does not fail the run; this is tidying, not a correctness check.
const std = @import("std");

pub fn deleteMatching(init: std.process.Init, dir_path: []const u8, prefix: []const u8, suffix: []const u8) !void {
    var dir = std.Io.Dir.cwd().openDir(init.io, dir_path, .{ .iterate = true }) catch |err| switch (err) {
        error.FileNotFound => return,
        else => return err,
    };
    defer dir.close(init.io);
    var it = dir.iterate();
    var deleted: usize = 0;
    while (try it.next(init.io)) |entry| {
        if (entry.kind != .file) continue;
        if (!std.mem.startsWith(u8, entry.name, prefix) or !std.mem.endsWith(u8, entry.name, suffix)) continue;
        dir.deleteFile(init.io, entry.name) catch |err| {
            std.debug.print("delete-matching-files: {s}/{s} did not delete: {s}\n", .{ dir_path, entry.name, @errorName(err) });
            continue;
        };
        deleted += 1;
    }
    std.debug.print("delete-matching-files: removed {d} file(s) matching {s}*{s} in {s}\n", .{ deleted, prefix, suffix, dir_path });
}

pub fn main(init: std.process.Init) !void {
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.skip();
    const dir_path = args.next() orelse return error.MissingDirectory;
    const prefix = args.next() orelse return error.MissingPrefix;
    const suffix = args.next() orelse return error.MissingSuffix;
    try deleteMatching(init, dir_path, prefix, suffix);
}
