//! Who the editors say they are to the desktop. On Wayland the compositor
//! takes a window's icon from the .desktop file named after its app_id, and
//! SDL3 sends the SDL_SetAppMetadata identifier as that app_id (else the
//! generic "SDL_App", which GNOME shows as a cogwheel). The ids follow the
//! Game's `org.blitzkrieg.game` (Sources/src/Platform/SDLApplication.cpp):
//! `org.blitzkrieg.<program>`, lower case, no separators. The desktop entries
//! `tools/install_desktop_entry.sh` writes are named by the same ids, and a
//! test below keeps the two lists equal.
const std = @import("std");
const sdl3 = @import("sdl3");

const c = sdl3.c;

pub const game: [:0]const u8 = "org.blitzkrieg.game";
pub const map_editor: [:0]const u8 = "org.blitzkrieg.mapeditor";
pub const resource_editor: [:0]const u8 = "org.blitzkrieg.resourceeditor";
pub const elk: [:0]const u8 = "org.blitzkrieg.elk";

/// Where the icon lives under a Data root: the BMP, because SDL3 decodes only
/// BMP itself (the PNG next to it is for the desktop entry and macOS).
pub const icon_file = "Data/icon.bmp";

/// Must run before the window is created: the Wayland backend caches the
/// app_id when it creates the window.
pub fn setMetadata(name: [*:0]const u8, app_id: [*:0]const u8) void {
    _ = c.SDL_SetAppMetadata(name, "2.0.0", app_id);
}

/// The icon surface for a Data root (the directory holding `Data/`), or null
/// when it cannot be read; an editor without its icon is still an editor, so
/// callers carry on. The caller owns the surface.
pub fn loadIcon(root: []const u8) ?*c.SDL_Surface {
    var path_buf: [std.fs.max_path_bytes]u8 = undefined;
    const sep: []const u8 = if (root.len == 0 or root[root.len - 1] == '/' or root[root.len - 1] == '\\') "" else "/";
    const path = std.fmt.bufPrint(&path_buf, "{s}{s}{s}\x00", .{ root, sep, icon_file }) catch return null;
    return c.SDL_LoadBMP(@ptrCast(path.ptr));
}

/// Sets the window icon from the data root given, else from the directory the
/// executable runs from (the install layout keeps `Data/` beside it). X11 and
/// compositors speaking xdg-toplevel-icon use it; GNOME ignores it and relies
/// on the desktop entry instead.
pub fn setWindowIcon(window: *c.SDL_Window, data_root: ?[*:0]const u8) void {
    const root: []const u8 = if (data_root) |r| std.mem.span(r) else if (c.SDL_GetBasePath()) |p| std.mem.span(p) else return;
    const icon = loadIcon(root) orelse return;
    defer c.SDL_DestroySurface(icon);
    _ = c.SDL_SetWindowIcon(window, icon);
}

fn readRepoFile(allocator: std.mem.Allocator, path: []const u8) ![]u8 {
    return std.Io.Dir.cwd().readFileAlloc(std.testing.io, path, allocator, .limited(1 << 20));
}

fn contains(text: []const u8, needle: []const u8) bool {
    return std.mem.indexOf(u8, text, needle) != null;
}

// The Game's id is a string literal in C++; the editors' are the constants above.
// All of them must be what the desktop-entry script names its files by.
test "the ids in code are the ids the desktop entry script installs" {
    const allocator = std.testing.allocator;
    const script = try readRepoFile(allocator, "tools/install_desktop_entry.sh");
    defer allocator.free(script);
    const cpp = try readRepoFile(allocator, "Sources/src/Platform/SDLApplication.cpp");
    defer allocator.free(cpp);

    var quoted: [64]u8 = undefined;
    const game_literal = try std.fmt.bufPrint(&quoted, "\"{s}\"", .{game});
    try std.testing.expect(contains(cpp, game_literal));
    for ([_][]const u8{ game, map_editor, resource_editor, elk }) |id| {
        try std.testing.expect(contains(script, id));
    }
}

test "every program has its own id in the org.blitzkrieg scheme" {
    const ids = [_][]const u8{ game, map_editor, resource_editor, elk };
    for (ids, 0..) |id, i| {
        try std.testing.expect(std.mem.startsWith(u8, id, "org.blitzkrieg."));
        for (ids[i + 1 ..]) |other| try std.testing.expect(!std.mem.eql(u8, id, other));
    }
}

test "the window icon loads from the shipped Data" {
    const icon = loadIcon(".") orelse return error.IconDidNotLoad;
    defer c.SDL_DestroySurface(icon);
    try std.testing.expect(icon.*.w >= 32 and icon.*.h >= 32);
}
