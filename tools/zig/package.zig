//! The release packager: `package <staged root> <out.zip>`. The writer itself
//! lives in `stored_zip.zig`, shared with `bk-pak pack`, so a modder's `.pak`
//! and the release zip come from the same bytes-for-bytes code.

const std = @import("std");
const stored_zip = @import("stored_zip.zig");

pub const createPackage = stored_zip.createPackage;

pub fn main(init: std.process.Init) !void {
    var args_it = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args_it.deinit();

    _ = args_it.skip();
    const source_root = args_it.next() orelse return error.InvalidArguments;
    const output_zip = args_it.next() orelse return error.InvalidArguments;

    try createPackage(init.io, init.gpa, source_root, output_zip);
}
