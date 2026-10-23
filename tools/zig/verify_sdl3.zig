const std = @import("std");
const sdl3 = @import("sdl3");

/// The SDL library the Zig package links is the one its headers describe (build.zig.zon pins the
/// package; 2026-10: the pin moved from 3.4.0 to 3.4.18 and a literal version here went stale).
pub fn main() !void {
    const linked = sdl3.c.SDL_GetVersion();
    const built = sdl3.c.SDL_VERSION;
    if (linked != built or sdl3.c.SDL_VERSIONNUM_MAJOR(linked) != 3) {
        std.log.err("linked SDL {}.{}.{}, the headers are {}.{}.{}", .{
            sdl3.c.SDL_VERSIONNUM_MAJOR(linked), sdl3.c.SDL_VERSIONNUM_MINOR(linked), sdl3.c.SDL_VERSIONNUM_MICRO(linked),
            sdl3.c.SDL_VERSIONNUM_MAJOR(built),  sdl3.c.SDL_VERSIONNUM_MINOR(built),  sdl3.c.SDL_VERSIONNUM_MICRO(built),
        });
        return error.UnexpectedSdlVersion;
    }
}
