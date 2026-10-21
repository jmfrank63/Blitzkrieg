//! `verify-install-l10n <install dir>`: checks that a staged install holds the ten
//! languages (M002 D087) - the nine language mods, the Noto faces beside the editors
//! and ELK, and the tools guide - and prints `verify-runtime: missing <path>` for
//! every item it lacks. Run through `b.addRunArtifact` by the `verify-install-l10n`
//! build step; it exits 1 when anything is missing.
const std = @import("std");
const verify = @import("verify_runtime.zig");

pub fn main(init: std.process.Init) !void {
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.skip();
    const install_dir = args.next() orelse return error.MissingInstallDir;
    var root = try std.Io.Dir.cwd().openDir(init.io, install_dir, .{ .access_sub_paths = true });
    defer root.close(init.io);
    verify.verifyL10nInstall(init.io, init.gpa, root) catch |err| switch (err) {
        error.MissingL10nInstall => std.process.exit(1),
        else => return err,
    };
    std.debug.print("verify-install-l10n: {s} holds the ten languages\n", .{install_dir});
}
