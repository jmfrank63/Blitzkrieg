const std = @import("std");
const sdl3 = @import("sdl3");

pub fn main() !void {
    const version_number = sdl3.c.SDL_GetVersion();
    const major: u32 = @intCast(sdl3.c.SDL_VERSIONNUM_MAJOR(version_number));
    const minor: u32 = @intCast(sdl3.c.SDL_VERSIONNUM_MINOR(version_number));
    const micro: u32 = @intCast(sdl3.c.SDL_VERSIONNUM_MICRO(version_number));
    if (major != 3 or minor != 4 or micro != 0) {
        std.log.err("unexpected linked SDL version: {}.{}.{}", .{ major, minor, micro });
        return error.UnexpectedSdlVersion;
    }
    std.debug.print("SDL3 Zig package linked SDL {}.{}.{}\n", .{ major, minor, micro });
}
