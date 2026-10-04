//! Whether a map path is a shipped one - read-only (D-18): Save becomes Save
//! As, autosave writes only a recovery copy (D-22), and `Editor.save` refuses
//! to write there at all. std-only, like the rest of the core: the two
//! questions that need a real disk (does this directory look like a game's
//! data root, and where does this path really point) go through the `Files`
//! interface, which `StdFiles` answers from the disk and `FakeFiles` from a
//! table.
//!
//! A path is shipped when any of these holds, for the path as given AND for
//! its real path (symlinks resolved - the release stage's own `Data` is a
//! symlink, and a file dialog may hand back either side of one):
//!
//!   1. It lies under `<base_root>Data/` or `<base_root>mods/<any>/data/`
//!      (the installation the editor runs from; a relative path is taken as
//!      relative to it - every shipped map is opened by one), compared
//!      against both `base_root` and `base_root`'s own real path.
//!   2. It lies under `.../mods/<any>/data/` anywhere - a mod's data inside
//!      ANY installation, not only this one.
//!   3. One of its ancestor directories is named `data` (any case) and holds
//!      a game data root's marker (`isDataRootMarker`): the `Data` of ANY
//!      game installation, e.g. another checkout's, opened by an absolute
//!      path. The marker keeps a person's own folder that merely happens to
//!      be called "Data" writable.
//!
//! The user's own folders - `<user_root>maps`, `<user_root>mods/<N>/maps`,
//! the recovery folder, the generated test-map root - never match: none of
//! them has a `data` segment (NGeneratedData::ModKey strips separators, so a
//! mod's generated root is `modsX`, not `mods/X`).
const std = @import("std");
const builtin = @import("builtin");
const files_mod = @import("files.zig");
const Files = files_mod.Files;

/// The longest path this classifies (the file dialogs' own bound,
/// panels_logic.PathSlot.max_path) - not `files_mod.max_path`, which is
/// ~96 KiB on Windows, and this keeps several buffers of it on the stack.
/// Also at least PATH_MAX on every POSIX host, as `realpath` requires.
pub const max_path = 4096;

/// A file that only a game data root holds at its top: the engine's own
/// tables (`consts.xml`, `objects.xml`), a mod's `mod.xml` (every mod's data
/// has one - BkEditorSetMod refuses a mod without it, and the base game's
/// Data ships an empty one), the resource description, or a packed `*.pak`
/// (a retail installation, and a packed mod - MODCollector.cpp reads
/// `data\*.pak`). Case-insensitive, as Windows is.
pub fn isDataRootMarker(name: []const u8) bool {
    const markers = [_][]const u8{ "consts.xml", "objects.xml", "mod.xml", "resource.description" };
    for (markers) |marker| if (std.ascii.eqlIgnoreCase(name, marker)) return true;
    return name.len > ".pak".len and std.ascii.endsWithIgnoreCase(name, ".pak");
}

/// Lowercases and unifies separators to '/' in place; null when `path` does
/// not fit `buffer`. Used only to compare paths, never to build one the
/// engine or the OS will see.
pub fn normalizeForCompare(buffer: []u8, path: []const u8) ?[]const u8 {
    if (path.len > buffer.len) return null;
    for (path, buffer[0..path.len]) |char, *out| out.* = std.ascii.toLower(if (char == '\\') '/' else char);
    return buffer[0..path.len];
}

/// `/...`, `\\server\...` once normalised, or a Windows drive letter
/// (`c:/...`).
pub fn isAbsolutePath(path: []const u8) bool {
    if (path.len == 0) return false;
    if (path[0] == '/' or path[0] == '\\') return true;
    return path.len >= 2 and std.ascii.isAlphabetic(path[0]) and path[1] == ':';
}

/// Rules 1 and 2 on text alone: `path` and `base_root` in any case and
/// either separator.
pub fn isShippedLexically(path_any: []const u8, base_root_any: []const u8) bool {
    var path_buffer: [max_path]u8 = undefined;
    var base_buffer: [max_path + 1]u8 = undefined;
    const path = normalizeForCompare(&path_buffer, path_any) orelse return false;
    if (hasModDataSegment(path)) return true;
    if (!isAbsolutePath(path)) return underInstallData(path);
    const base_plain = normalizeForCompare(&base_buffer, base_root_any) orelse return false;
    if (base_plain.len == 0) return false;
    // A root handed back by a realpath has no trailing separator.
    const base = if (base_plain[base_plain.len - 1] == '/') base_plain else blk: {
        base_buffer[base_plain.len] = '/';
        break :blk base_buffer[0 .. base_plain.len + 1];
    };
    if (!std.mem.startsWith(u8, path, base)) return false;
    return underInstallData(path[base.len..]);
}

/// `data/...` or `mods/<name>/data/...`, relative to an installation root.
fn underInstallData(rel: []const u8) bool {
    if (std.mem.startsWith(u8, rel, "data/")) return true;
    if (std.mem.startsWith(u8, rel, "mods/")) {
        const after_mods = rel["mods/".len..];
        const slash = std.mem.indexOfScalar(u8, after_mods, '/') orelse return false;
        return slash != 0 and std.mem.startsWith(u8, after_mods[slash + 1 ..], "data/");
    }
    return false;
}

/// `/mods/<name>/data/` anywhere in an already-normalised path.
fn hasModDataSegment(path: []const u8) bool {
    var start: usize = 0;
    while (std.mem.indexOfPos(u8, path, start, "/mods/")) |at| {
        if (underInstallData(path[at + 1 ..])) return true;
        start = at + 1;
    }
    return false;
}

/// Rule 3: an ancestor directory of `os_path` named `data` (any case) that
/// `files` says is a game data root. The prefixes handed to `files` are
/// slices of `os_path` itself, so they keep its own separators and case.
pub fn hasDataRootAncestor(os_path: []const u8, files: Files) bool {
    var end: usize = 0;
    while (end < os_path.len) {
        const separator = std.mem.indexOfAnyPos(u8, os_path, end, "/\\") orelse return false; // the last component is the file
        const component_start = if (std.mem.lastIndexOfAny(u8, os_path[0..separator], "/\\")) |cut| cut + 1 else 0;
        const component = os_path[component_start..separator];
        if (std.ascii.eqlIgnoreCase(component, "data") and files.isDataRoot(os_path[0..separator])) return true;
        end = separator + 1;
    }
    return false;
}

/// `path`'s real path in `buffer`: through `files.realPath` when the file
/// exists, otherwise its folder's real path plus its own name (a Save As
/// target that does not exist yet). Null when neither resolves.
fn resolve(buffer: []u8, os_path: []const u8, files: Files) ?[]const u8 {
    if (files.realPath(os_path, buffer)) |real| return real;
    const cut = std.mem.lastIndexOfAny(u8, os_path, "/\\") orelse return null;
    const folder = if (cut == 0) os_path[0..1] else os_path[0..cut];
    var folder_buffer: [max_path]u8 = undefined;
    const real_folder = files.realPath(folder, &folder_buffer) orelse return null;
    const name = os_path[cut + 1 ..];
    const sep: u8 = if (builtin.os.tag == .windows) '\\' else '/';
    const needs_sep = real_folder.len == 0 or (real_folder[real_folder.len - 1] != '/' and real_folder[real_folder.len - 1] != '\\');
    return std.fmt.bufPrint(buffer, "{s}{s}{s}", .{ real_folder, if (needs_sep) &[_]u8{sep} else "", name }) catch null;
}

/// The whole rule (this file's doc comment). `path` may be in the engine's
/// form (backslashes) or the OS's; `base_root` is the installation the
/// editor runs from ("" when unknown - rules 2 and 3 still apply). `files`
/// null means no disk to ask: rules 1 and 2 on the text alone.
pub fn isShipped(path: []const u8, base_root: []const u8, files: ?Files) bool {
    if (path.len == 0) return false;
    if (isShippedLexically(path, base_root)) return true;
    const fs = files orelse return false;

    var os_buffer: [max_path]u8 = undefined;
    const os_path = files_mod.osPathFromEngine(&os_buffer, path) orelse return false;
    if (hasDataRootAncestor(os_path, fs)) return true;

    var real_buffer: [max_path]u8 = undefined;
    const real = resolve(&real_buffer, os_path, fs) orelse return false;
    if (isShippedLexically(real, base_root)) return true;
    if (base_root.len != 0) {
        var base_os_buffer: [max_path]u8 = undefined;
        var real_base_buffer: [max_path]u8 = undefined;
        if (files_mod.osPathFromEngine(&base_os_buffer, base_root)) |base_os| {
            // A trailing separator would make some realpaths fail; the
            // folder itself resolves the same.
            const trimmed = if (base_os.len > 1 and (base_os[base_os.len - 1] == '/' or base_os[base_os.len - 1] == '\\')) base_os[0 .. base_os.len - 1] else base_os;
            if (fs.realPath(trimmed, &real_base_buffer)) |real_base| {
                if (isShippedLexically(real, real_base)) return true;
            }
        }
    }
    return hasDataRootAncestor(real, fs);
}

// ---------------------------------------------------------------- tests

const FakeFiles = files_mod.FakeFiles;

fn fakeWith(data_roots: []const []const u8, links: []const FakeFiles.Link) FakeFiles {
    var fake = FakeFiles.init(std.testing.allocator);
    fake.data_roots = data_roots;
    fake.links = links;
    return fake;
}

test "isDataRootMarker: the engine's tables, mod.xml, resource.description and *.pak, any case" {
    try std.testing.expect(isDataRootMarker("consts.xml"));
    try std.testing.expect(isDataRootMarker("OBJECTS.XML"));
    try std.testing.expect(isDataRootMarker("mod.xml"));
    try std.testing.expect(isDataRootMarker("resource.description"));
    try std.testing.expect(isDataRootMarker("data.pak"));
    try std.testing.expect(isDataRootMarker("Texts.PAK"));
    try std.testing.expect(!isDataRootMarker(".pak"));
    try std.testing.expect(!isDataRootMarker("coldwinter.bzm"));
    try std.testing.expect(!isDataRootMarker("mapeditor.cfg"));
}

test "isShipped: the editor's own Data, relative or absolute, any case or separator" {
    const base = "/Users/me/MapEditor/";
    try std.testing.expect(isShipped("Data\\Maps\\Multiplayer\\coldwinter.bzm", base, null));
    try std.testing.expect(isShipped("DATA/maps/a.bzm", base, null));
    try std.testing.expect(isShipped("/Users/me/MapEditor/Data/Maps/Multiplayer/coldwinter.bzm", base, null));
    try std.testing.expect(isShipped("\\Users\\me\\MapEditor\\Data\\Maps\\a.bzm", base, null));
    try std.testing.expect(isShipped("/USERS/ME/MAPEDITOR/DATA/maps/a.BZM", base, null));
    // The root without its trailing separator, as a realpath returns it.
    try std.testing.expect(isShipped("/Users/me/MapEditor/Data/a.bzm", "/Users/me/MapEditor", null));
    try std.testing.expect(!isShipped("/Users/me/MapEditorX/Data2/a.bzm", "/Users/me/MapEditor", null));
    try std.testing.expect(!isShipped("", base, null));
}

test "isShipped: mods/<name>/data of this or any installation; mods/<name>/maps is the user's" {
    const base = "/Users/me/MapEditor/";
    try std.testing.expect(isShipped("mods\\X\\data\\maps\\a.bzm", base, null));
    try std.testing.expect(isShipped("/Users/me/MapEditor/mods/X/data/maps/a.bzm", base, null));
    try std.testing.expect(isShipped("/Volumes/Games/Blitzkrieg/mods/Other/Data/Maps/a.bzm", base, null));
    try std.testing.expect(isShipped("D:\\Games\\Blitzkrieg\\MODS\\Other\\data\\maps\\a.bzm", base, null));
    try std.testing.expect(!isShipped("/Users/me/MapEditor/mods/X/maps/a.bzm", base, null));
    try std.testing.expect(!isShipped("/Users/me/.local/share/Nival/Blitzkrieg/mods/X/maps/a.bzm", base, null));
    try std.testing.expect(!isShipped("/Users/me/MapEditor/mods//data/a.bzm", base, null)); // no mod name
}

test "isShipped: another installation's Data is shipped - found by its marker, not by the editor's root" {
    const base = "/Users/me/Blitzkrieg/.worktrees/map-editor-6/zig-out/game/macos/arm64/release/";
    var fake = fakeWith(&.{ "/Users/me/Blitzkrieg/Data", "C:\\Games\\Blitzkrieg\\Data" }, &.{});
    defer fake.deinit();
    const files = fake.files();
    // The hand try's own path: the main checkout's Data, not the stage's.
    try std.testing.expect(!isShipped("/Users/me/Blitzkrieg/Data/Maps/Multiplayer/coldwinter.bzm", base, null));
    try std.testing.expect(isShipped("/Users/me/Blitzkrieg/Data/Maps/Multiplayer/coldwinter.bzm", base, files));
    try std.testing.expect(isShipped("\\Users\\me\\Blitzkrieg\\Data\\Maps\\Multiplayer\\coldwinter.bzm", base, files));
    // Windows: a drive letter and backslashes, any case.
    try std.testing.expect(isShipped("C:\\Games\\Blitzkrieg\\Data\\Maps\\a.bzm", "D:\\MapEditor\\", files));
    try std.testing.expect(isShipped("c:\\games\\blitzkrieg\\DATA\\Maps\\a.bzm", "D:\\MapEditor\\", files));
}

test "isShipped: a folder called Data with no game marker stays the user's" {
    var fake = fakeWith(&.{"/Users/me/Blitzkrieg/Data"}, &.{});
    defer fake.deinit();
    const files = fake.files();
    try std.testing.expect(!isShipped("/Volumes/Data/maps/mine.bzm", "/Users/me/MapEditor/", files));
    try std.testing.expect(!isShipped("/Users/me/Documents/data/mine.bzm", "/Users/me/MapEditor/", files));
    try std.testing.expect(!isShipped("E:\\Data\\maps\\mine.bzm", "D:\\MapEditor\\", files));
}

test "isShipped: a symlink resolved to a data tree is shipped, from either side" {
    const base = "/Users/me/stage/";
    var fake = fakeWith(&.{"/Users/me/checkout/Data"}, &.{
        // The stage's Data is a symlink to the checkout's.
        .{ .from = "/Users/me/stage/Data", .to = "/Users/me/checkout/Data" },
        // A person's shortcut to a shipped maps folder, named nothing like it.
        .{ .from = "/Users/me/Desktop/shipped-maps", .to = "/Users/me/checkout/Data/Maps" },
        // A shortcut to the stage root itself.
        .{ .from = "/Users/me/stage-link", .to = "/Users/me/stage" },
    });
    defer fake.deinit();
    const files = fake.files();
    try std.testing.expect(isShipped("/Users/me/Desktop/shipped-maps/Multiplayer/coldwinter.bzm", base, files));
    // Reached through the checkout's real path while the editor's root is
    // the stage: rule 3 on the given path already knows it.
    try std.testing.expect(isShipped("/Users/me/checkout/Data/Maps/a.bzm", base, files));
    // The stage reached through a link to it: its real path is under base.
    try std.testing.expect(isShipped("/Users/me/stage-link/Data/Maps/a.bzm", base, files));
    // A link to somewhere that is not a data tree stays writable.
    try std.testing.expect(!isShipped("/Users/me/Desktop/elsewhere/a.bzm", base, files));
}

test "isShipped: a symlinked editor root compares by its real path" {
    // The document's real path lies under the real base root, but neither
    // Data carries a marker (rule 3 cannot answer) - rule 1 on the real
    // base root must.
    var fake = fakeWith(&.{}, &.{
        .{ .from = "/Users/me/link-root", .to = "/Users/me/real-root" },
    });
    defer fake.deinit();
    const files = fake.files();
    try std.testing.expect(isShipped("/Users/me/real-root/Data/Maps/a.bzm", "/Users/me/link-root/", files));
    try std.testing.expect(!isShipped("/Users/me/real-root/maps/a.bzm", "/Users/me/link-root/", files));
}

test "isShipped: the user's maps folder and the recovery folder stay writable" {
    const base = "/Users/me/MapEditor/";
    var fake = fakeWith(&.{ "/Users/me/MapEditor/Data", "/Users/me/Blitzkrieg/Data" }, &.{});
    defer fake.deinit();
    const files = fake.files();
    const user_root = "/Users/me/.local/share/Nival/Blitzkrieg/";
    try std.testing.expect(!isShipped(user_root ++ "maps/a.bzm", base, files));
    try std.testing.expect(!isShipped(user_root ++ "mods/MyMod/maps/a.bzm", base, files));
    try std.testing.expect(!isShipped(user_root ++ "mapeditor/recovery/coldwinter.bzm", base, files));
    try std.testing.expect(!isShipped(user_root ++ "cache/generated/default/modsMyMod/maps/editor-test.bzm", base, files));
    try std.testing.expect(!isShipped("C:\\Users\\me\\AppData\\Roaming\\Nival\\Blitzkrieg\\maps\\a.bzm", "D:\\MapEditor\\", files));
    try std.testing.expect(!isShipped("C:\\Users\\me\\AppData\\Roaming\\Nival\\Blitzkrieg\\mapeditor\\recovery\\a.bzm", "D:\\MapEditor\\", files));
}

test "isShipped on a real disk: a marked Data, a symlink to it, and an unmarked Data" {
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const io = std.testing.io;
    try tmp.dir.createDirPath(io, "game/Data/Maps/Multiplayer");
    try tmp.dir.writeFile(io, .{ .sub_path = "game/Data/consts.xml", .data = "<base/>" });
    try tmp.dir.writeFile(io, .{ .sub_path = "game/Data/Maps/Multiplayer/a.bzm", .data = "map" });
    try tmp.dir.createDirPath(io, "home/Data/maps");
    try tmp.dir.writeFile(io, .{ .sub_path = "home/Data/maps/b.bzm", .data = "map" });
    try tmp.dir.createDirPath(io, "home/maps");
    var std_files: files_mod.StdFiles = .{ .io = io, .dir = tmp.dir };
    const files = std_files.files();

    try std.testing.expect(files.isDataRoot("game/Data"));
    try std.testing.expect(!files.isDataRoot("home/Data"));
    try std.testing.expect(isShipped("game/Data/Maps/Multiplayer/a.bzm", "", files));
    try std.testing.expect(isShipped("game\\Data\\Maps\\Multiplayer\\a.bzm", "", files));
    try std.testing.expect(!isShipped("home/Data/maps/b.bzm", "", files));
    // A Save As target that does not exist yet, in either place.
    try std.testing.expect(isShipped("game/Data/Maps/new.bzm", "", files));
    try std.testing.expect(!isShipped("home/maps/new.bzm", "", files));

    // Symlinks need a privilege Windows runners do not grant by default.
    if (builtin.os.tag == .windows) return;
    try tmp.dir.symLink(io, "game/Data/Maps", "shortcut", .{ .is_directory = true });
    try std.testing.expect(isShipped("shortcut/Multiplayer/a.bzm", "", files));
    try std.testing.expect(isShipped("shortcut/Multiplayer/new.bzm", "", files));
}
