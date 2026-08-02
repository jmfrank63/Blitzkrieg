const std = @import("std");

const process_markers = [_][]const u8{ "addSystemCommand", "std.process.run", "std.process.Child", "std.process.exec" };
const forbidden_executables = [_][]const u8{ "pwsh", "powershell", "powershell.exe", "bash", "sh", "cmd", "cmd.exe", "cmake", "ninja", "make", "ln", "xcopy", "robocopy" };
const shader_build_closure = [_][]const u8{
    "build.zig",
    "vendor/zig-sdl3/build.zig",
    "vendor/zig-sdl3/build/shadercross.zig",
    "tools/zig/compile_gfxgpu_shaders.zig",
    "tools/zig/verify_shadercross.zig",
    "tools/zig/compare_trees.zig",
};

fn isTokenChar(value: u8) bool {
    return std.ascii.isAlphanumeric(value) or value == '_' or value == '.';
}

fn containsToken(text: []const u8, token: []const u8) bool {
    var start: usize = 0;
    while (std.mem.indexOfPos(u8, text, start, token)) |position| {
        const left_ok = position == 0 or !isTokenChar(text[position - 1]);
        const end = position + token.len;
        const right_ok = end == text.len or !isTokenChar(text[end]);
        if (left_ok and right_ok) return true;
        start = end;
    }
    return false;
}

fn auditText(path: []const u8, text: []const u8) !void {
    _ = path;
    var creates_process = false;
    for (process_markers) |marker| {
        if (std.mem.indexOf(u8, text, marker) != null) {
            creates_process = true;
            break;
        }
    }
    if (!creates_process) return;
    for (forbidden_executables) |executable| {
        if (containsToken(text, executable)) {
            return error.ForbiddenBuildProcess;
        }
    }
}

pub fn main(init: std.process.Init) !void {
    for (shader_build_closure) |path| {
        const text = try std.Io.Dir.cwd().readFileAlloc(init.io, path, init.gpa, .limited(4 * 1024 * 1024));
        defer init.gpa.free(text);
        try auditText(path, text);
    }
    std.debug.print("build hermeticity audit passed: {d} files\n", .{shader_build_closure.len});
}

test "rejects forbidden process executable" {
    try std.testing.expectError(error.ForbiddenBuildProcess, auditText("fixture", "b.addSystemCommand(&.{\"pwsh\"})"));
}

test "accepts Zig artifact process" {
    try auditText("fixture", "const run = b.addRunArtifact(exe); run.addArtifactArg(tool);");
}

test "current shader build closure is hermetic" {
    for (shader_build_closure) |path| {
        const text = try std.Io.Dir.cwd().readFileAlloc(std.testing.io, path, std.testing.allocator, .limited(4 * 1024 * 1024));
        defer std.testing.allocator.free(text);
        try auditText(path, text);
    }
}
