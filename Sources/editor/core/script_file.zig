//! The map's script file on disk (04-10, D-20). A map names a Lua file by a
//! bare name in `szScriptFile`; the game loads `<the map's own folder>/<name>.lua`
//! (GameTT/iMissionInternal.cpp keeps only the file's own name, and adds the
//! folder of the map it loaded). So the file has to be where the game looks:
//! beside the map, beside the test copy of the map (the generated-data folder
//! BkEditorTestMapPath answers), and beside a Save As of a shipped map.
//!
//! std-only, like the rest of the core: every copy goes through `Files`, so the
//! tests run against `FakeFiles`. Every destination is a fixed directory plus a
//! name that passed `isBareName`, never typed text (T-04-10-01): a name that
//! carries a folder, "..", a drive or ".lua" builds no path at all.
const std = @import("std");
const builtin = @import("builtin");
const files_mod = @import("files.zig");
const shipped_mod = @import("shipped.zig");

const Files = files_mod.Files;

/// The longest bare name: BkEditorScriptFileRecord holds 63 characters and the
/// NUL, and the game appends ".lua".
pub const max_name = 63;

/// True for None (empty) and for a bare script name: letters, digits, '_', '-'
/// and '.' only, no leading dot, no "..", no ".lua" suffix (the game adds it),
/// at most 63 characters, so it can never name a path. The same rule as
/// NMapRecords::IsBareScriptName (MapFile/MapRecords.cpp), reimplemented here so
/// the core needs no bridge to judge a name; both are tested on the same cases.
pub fn isBareName(name: []const u8) bool {
    if (name.len == 0) return true;
    if (name.len > max_name or name[0] == '.') return false;
    if (std.mem.indexOf(u8, name, "..") != null) return false;
    for (name) |char| {
        const ok = std.ascii.isAlphanumeric(char) or char == '_' or char == '-' or char == '.';
        if (!ok) return false;
    }
    if (name.len >= 4 and std.ascii.eqlIgnoreCase(name[name.len - 4 ..], ".lua")) return false;
    return true;
}

/// The file name the GAME will look for when the map's script file is `value`:
/// the game keeps only the last component of a script path and puts the map's own
/// folder in front of it (GameTT/iMissionInternal.cpp), so a shipped map naming
/// "maps\\BattleOfBulge" loads "BattleOfBulge.lua" from beside the map. That
/// component is the name a copy is made under - and it is one component with no
/// separator in it, so it can only pass `isBareName` if it is safe. Null for None
/// and for a value whose last component is not a bare name (an empty one, "..",
/// one ending in ".lua", odd characters).
pub fn gameScriptName(value: []const u8) ?[]const u8 {
    const cut = std.mem.lastIndexOfAny(u8, value, "/\\");
    const last = if (cut) |c| value[c + 1 ..] else value;
    if (last.len == 0 or !isBareName(last)) return null;
    return last;
}

/// The directory part of `path`, separator included ("" when it has none);
/// either separator counts, as the engine's paths use backslash and an OS path
/// on macOS or Linux slash.
pub fn directoryOf(path: []const u8) []const u8 {
    const cut = std.mem.lastIndexOfAny(u8, path, "/\\") orelse return path[0..0];
    return path[0 .. cut + 1];
}

/// `text` with its backslashes turned into the OS separator, in place, except
/// on Windows (files.osPathFromEngine's rule).
fn toOsSeparators(text: []u8) void {
    if (builtin.os.tag == .windows) return;
    for (text) |*char| {
        if (char.* == '\\') char.* = '/';
    }
}

/// `<directory><name>.lua` in an OS path, for a `directory` that is empty or
/// ends in a separator.
fn luaPathIn(buffer: []u8, directory: []const u8, name: []const u8) ?[]const u8 {
    const text = std.fmt.bufPrint(buffer, "{s}{s}.lua", .{ directory, name }) catch return null;
    toOsSeparators(text);
    return text;
}

/// The script `name` beside the map `map_path` (engine or OS form): the map's
/// directory, the name and ".lua", as an OS path in `buffer`. Null for None
/// (empty), for a name failing `isBareName`, and when it does not fit.
pub fn scriptPathBeside(buffer: []u8, map_path: []const u8, name: []const u8) ?[]const u8 {
    if (name.len == 0 or !isBareName(name)) return null;
    return luaPathIn(buffer, directoryOf(map_path), name);
}

/// The script `name` inside the directory `dir` (engine or OS form, with or
/// without a trailing separator), as an OS path in `buffer`. Null as
/// `scriptPathBeside`.
pub fn scriptPathIn(buffer: []u8, dir: []const u8, name: []const u8) ?[]const u8 {
    if (name.len == 0 or !isBareName(name)) return null;
    const trimmed = std.mem.trimEnd(u8, dir, "/\\");
    // The joiner the directory itself uses: the engine's backslash, or a slash.
    const joiner: u8 = if (std.mem.indexOfScalar(u8, trimmed, '\\') != null) '\\' else '/';
    const text = (if (dir.len == 0)
        std.fmt.bufPrint(buffer, "{s}.lua", .{name})
    else
        std.fmt.bufPrint(buffer, "{s}{c}{s}.lua", .{ trimmed, joiner, name })) catch return null;
    toOsSeparators(text);
    return text;
}

/// What a copy of a script did.
pub const CopyOutcome = enum {
    /// The file is where it was wanted (copied, or it already was that file).
    copied,
    /// There is no such file beside the map: a warning, never an error - the
    /// game runs the map and reports the missing script itself.
    missing,
    /// The disk refused.
    failed,
    /// The name is None or not a bare name, so no path was built: nothing was
    /// copied.
    not_a_bare_name,
    /// The destination is inside a game's data folder (`shipped.isShipped`),
    /// which is never written: nothing was copied.
    shipped,
};

/// True when `a` and `b` are the same file: the same text, or the same real
/// path once both exist. Public for Save As's copy-along question (04-13).
pub fn sameFile(files: Files, a: []const u8, b: []const u8) bool {
    if (std.mem.eql(u8, a, b)) return true;
    var a_buffer: [files_mod.max_path]u8 = undefined;
    var b_buffer: [files_mod.max_path]u8 = undefined;
    const real_a = files.realPath(a, &a_buffer) orelse return false;
    const real_b = files.realPath(b, &b_buffer) orelse return false;
    return std.mem.eql(u8, real_a, real_b);
}

/// Test in game (D-20): copies `<name>.lua` from beside the open map to the
/// directory `test_dir` of the test copy (BkEditorTestMapPath's), overwriting
/// there - the test folder is ours, so no question is asked. `map_path` is the
/// open map's path (engine form), `test_dir` the folder of the test map's path,
/// `value` the map's script file as the map holds it: a bare name, or - read
/// from a shipped map - a path whose last component is the name the game
/// loads (`gameScriptName`). Both ends are a fixed directory plus that one
/// validated component.
/// `base_root` is the installation the editor runs from (`Editor.baseRoot`): a
/// destination inside a game's data folder is `.shipped`, whichever caller
/// asked (WR-B05, as `Editor.save`).
/// When no copy is made (`.missing`, `.failed`), a `<name>.lua` an earlier test
/// left in the test folder is deleted, so the test game can never run a stale
/// script in place of the one the map has now (WR-C06).
pub fn copyForTest(files: Files, base_root: []const u8, map_path: []const u8, test_dir: []const u8, value: []const u8) CopyOutcome {
    var from_buffer: [files_mod.max_path]u8 = undefined;
    var to_buffer: [files_mod.max_path]u8 = undefined;
    const name = gameScriptName(value) orelse return .not_a_bare_name;
    const from = scriptPathBeside(&from_buffer, map_path, name) orelse return .not_a_bare_name;
    const to = scriptPathIn(&to_buffer, test_dir, name) orelse return .failed;
    if (shipped_mod.isShipped(to, base_root, files)) return .shipped;
    if (!files.exists(from)) {
        if (files.exists(to)) files.delete(to);
        return .missing;
    }
    if (sameFile(files, from, to)) return .copied;
    files.copy(from, to) catch {
        if (files.exists(to)) files.delete(to);
        return .failed;
    };
    return .copied;
}

/// The bare names of the `.lua` files beside the map `map_path` (engine or OS
/// form), sorted, each without the extension: only files whose name before
/// ".lua" passes `isBareName` (a file the game could be told to load), owned by
/// `allocator` - free with `files.freeNames`. The Script dialog offers "None"
/// and these. A folder that will not list gives an empty list.
pub fn listBeside(files: Files, allocator: std.mem.Allocator, map_path: []const u8, out: *std.ArrayListUnmanaged([]u8)) Files.Error!void {
    var directory: [files_mod.max_path]u8 = undefined;
    const beside = directoryOf(map_path);
    // The directory without its trailing separator (a root keeps its own), "."
    // for a map named with no folder at all.
    var dir_len = beside.len;
    while (dir_len > 1 and (beside[dir_len - 1] == '/' or beside[dir_len - 1] == '\\')) dir_len -= 1;
    if (dir_len > directory.len) return;
    @memcpy(directory[0..dir_len], beside[0..dir_len]);
    toOsSeparators(directory[0..dir_len]);
    const dir_os: []const u8 = if (dir_len == 0) "." else directory[0..dir_len];
    var raw: std.ArrayListUnmanaged([]u8) = .empty;
    defer files_mod.freeNames(allocator, &raw);
    try files.list(dir_os, lua_extension, allocator, &raw);
    errdefer files_mod.freeNames(allocator, out);
    for (raw.items) |file_name| {
        const stem = file_name[0 .. file_name.len - lua_extension.len];
        if (stem.len == 0 or !isBareName(stem)) continue;
        const owned = allocator.dupe(u8, stem) catch return error.Failed;
        out.append(allocator, owned) catch {
            allocator.free(owned);
            return error.Failed;
        };
    }
}

pub const lua_extension = ".lua";

/// The script name a picked file gives: its own name without ".lua", when that
/// passes `isBareName` and is not empty. `picked_path` is what the file dialog
/// answered (an OS path). Null for anything else, which is refused.
pub fn pickedName(picked_path: []const u8) ?[]const u8 {
    const cut = std.mem.lastIndexOfAny(u8, picked_path, "/\\");
    const file_name = if (cut) |c| picked_path[c + 1 ..] else picked_path;
    if (file_name.len <= lua_extension.len or !std.ascii.eqlIgnoreCase(file_name[file_name.len - lua_extension.len ..], lua_extension)) return null;
    const stem = file_name[0 .. file_name.len - lua_extension.len];
    return if (isBareName(stem)) stem else null;
}

/// What "Choose other..." did with a picked file.
pub const CopyIntoOutcome = enum {
    /// The script is beside the map now (copied, or it already was that file).
    copied,
    /// A different file of that name is already beside the map: nothing was
    /// copied, and the caller asks before calling again with `overwrite`.
    exists,
    /// The picked file's name is not a bare name plus ".lua".
    not_a_bare_name,
    /// The picked file is not there, or the disk refused.
    failed,
    /// The map is inside a game's data folder, which is never written.
    shipped,
};

/// "Choose other...": copies the picked file beside the map `map_path` under
/// its own name (`pickedName`). The destination is the map's folder plus that
/// validated name, never the picked path. When a different file is already
/// there this is `.exists` unless `overwrite` says the person agreed.
pub fn copyInto(files: Files, base_root: []const u8, map_path: []const u8, picked_path: []const u8, overwrite: bool) CopyIntoOutcome {
    const name = pickedName(picked_path) orelse return .not_a_bare_name;
    var to_buffer: [files_mod.max_path]u8 = undefined;
    const to = scriptPathBeside(&to_buffer, map_path, name) orelse return .not_a_bare_name;
    if (!files.exists(picked_path)) return .failed;
    if (sameFile(files, picked_path, to)) return .copied;
    if (shipped_mod.isShipped(to, base_root, files)) return .shipped;
    if (files.exists(to) and !overwrite) return .exists;
    files.copy(picked_path, to) catch return .failed;
    return .copied;
}

/// What Save As's copy-along did.
pub const CopyAlongOutcome = enum {
    /// The script is beside the new map now (copied, or it already was that file).
    copied,
    /// A different file of that name is already beside the new map: nothing was
    /// copied, and the caller asks "Replace it?" before calling again with
    /// `overwrite` (as `copyInto`).
    exists,
    /// There is no such file beside the old map: nothing to copy.
    missing,
    /// The disk refused.
    failed,
    /// The name is None or not a bare name, so no path was built.
    not_a_bare_name,
    /// The new map is inside a game's data folder, which is never written.
    shipped,
};

/// Save As (D-20; any map since 04-13): copies the script the map names from beside
/// `from_map` to beside `to_map`, both engine or OS paths, `value` the map's
/// script file as it holds it (`gameScriptName`). `.missing` when there is no
/// such file beside the old map (nothing to ask about, nothing failed), and a
/// copy onto itself - both maps in one folder - is `.copied`. A different file
/// of that name already beside the new map is never replaced unless `overwrite`
/// says the person agreed: `.exists` (the contract of `copyInto`).
pub fn copyAlong(files: Files, base_root: []const u8, from_map: []const u8, to_map: []const u8, value: []const u8, overwrite: bool) CopyAlongOutcome {
    const name = gameScriptName(value) orelse return .not_a_bare_name;
    var from_buffer: [files_mod.max_path]u8 = undefined;
    var to_buffer: [files_mod.max_path]u8 = undefined;
    const from = scriptPathBeside(&from_buffer, from_map, name) orelse return .not_a_bare_name;
    const to = scriptPathBeside(&to_buffer, to_map, name) orelse return .not_a_bare_name;
    if (!files.exists(from)) return .missing;
    if (sameFile(files, from, to)) return .copied;
    if (shipped_mod.isShipped(to, base_root, files)) return .shipped;
    if (files.exists(to) and !overwrite) return .exists;
    files.copy(from, to) catch return .failed;
    return .copied;
}

fn isUnreserved(char: u8) bool {
    return std.ascii.isAlphanumeric(char) or char == '-' or char == '.' or char == '_' or char == '~';
}

/// The URL of the FOLDER that holds the script `value` (as the map holds it)
/// beside `map_path`, for "Open script folder" (`SDL_OpenURL`): "file://" and the
/// resolved absolute path of the file's folder (`Files.realPath`), percent-encoded,
/// with a trailing slash, in `buffer`. The folder, never the file: the `.lua`
/// came with a map that may have been downloaded, and the system's default
/// "open" of a `.lua` may run it where an interpreter is associated (ShellExecute
/// "open" on Windows) - showing the folder lets the person open it in the editor
/// of their choice (WR-B04). Built from nothing typed: the name is
/// `gameScriptName`'s and only checks the file is there; the folder is the map's
/// own. Null for None or a name that fails the rule, a file that is not there
/// (the caller warns) and a path that does not fit.
pub fn folderUrl(files: Files, buffer: []u8, map_path: []const u8, value: []const u8) ?[]const u8 {
    const name = gameScriptName(value) orelse return null;
    var path_buffer: [files_mod.max_path]u8 = undefined;
    const script_path = scriptPathBeside(&path_buffer, map_path, name) orelse return null;
    if (!files.exists(script_path)) return null;
    var dir_buffer: [files_mod.max_path]u8 = undefined;
    const script_dir = script_path[0 .. std.mem.lastIndexOfAny(u8, script_path, "/\\") orelse 0];
    const real_dir = files.realPath(if (script_dir.len == 0) "." else script_dir, &dir_buffer) orelse return null;
    var out: std.ArrayListUnmanaged(u8) = .initBuffer(buffer);
    out.appendSliceBounded("file://") catch return null;
    // A drive path ("C:\...") gets the slash a URL needs before it.
    if (real_dir.len == 0 or (real_dir[0] != '/' and real_dir[0] != '\\')) out.appendBounded('/') catch return null;
    for (real_dir, 0..) |char, index| {
        const drive_colon = char == ':' and index == 1 and std.ascii.isAlphabetic(real_dir[0]);
        if (char == '\\') {
            out.appendBounded('/') catch return null;
        } else if (char == '/' or isUnreserved(char) or drive_colon) {
            out.appendBounded(char) catch return null;
        } else {
            var escape: [3]u8 = undefined;
            _ = std.fmt.bufPrint(&escape, "%{X:0>2}", .{char}) catch return null;
            out.appendSliceBounded(&escape) catch return null;
        }
    }
    if (out.items.len == 0 or out.items[out.items.len - 1] != '/') out.appendBounded('/') catch return null;
    return out.items;
}

test "isBareName holds the rule NMapRecords::IsBareScriptName holds" {
    // The same cases map_file_test.cpp and editor_bridge_test.cpp give the C++ rule.
    const good = [_][]const u8{ "", "m2_script", "coldwinter", "a", "A-b_c.d", "script1", "x" ** 63 };
    for (good) |name| try std.testing.expect(isBareName(name));
    const bad = [_][]const u8{ "..\\x", "a/b", "x.lua", "x.LUA", "x.Lua", "..", ".hidden", "a b", "a:b", "dir\\name", "x" ** 64, "a..b", ".lua", "C:x", "na\u{e9}me", "a\x00b" };
    for (bad) |name| try std.testing.expect(!isBareName(name));
}

test "scriptPathBeside is the map's directory, the name and .lua, and never anything else" {
    var buffer: [128]u8 = undefined;
    const beside = scriptPathBeside(&buffer, "Data\\Maps\\Multiplayer\\coldwinter.bzm", "m2_script").?;
    if (builtin.os.tag == .windows) {
        try std.testing.expectEqualStrings("Data\\Maps\\Multiplayer\\m2_script.lua", beside);
    } else {
        try std.testing.expectEqualStrings("Data/Maps/Multiplayer/m2_script.lua", beside);
    }
    try std.testing.expectEqualStrings("m2_script.lua", scriptPathBeside(&buffer, "coldwinter.bzm", "m2_script").?);
    try std.testing.expect(scriptPathBeside(&buffer, "/maps/a.bzm", "") == null);
    for ([_][]const u8{ "..\\x", "a/b", "x.lua", "..", "/etc/passwd", "C:x" }) |name| {
        try std.testing.expect(scriptPathBeside(&buffer, "/maps/a.bzm", name) == null);
    }
    var tiny: [8]u8 = undefined;
    try std.testing.expect(scriptPathBeside(&tiny, "/maps/a.bzm", "m2_script") == null);
    // The test folder, with or without its trailing separator.
    if (builtin.os.tag != .windows) {
        try std.testing.expectEqualStrings("/home/u/generated/maps/m2_script.lua", scriptPathIn(&buffer, "/home/u/generated/maps/", "m2_script").?);
        try std.testing.expectEqualStrings("/a/b/m2_script.lua", scriptPathIn(&buffer, "/a/b", "m2_script").?);
        try std.testing.expectEqualStrings("/a/b/m2_script.lua", scriptPathIn(&buffer, "\\a\\b\\", "m2_script").?);
    }
    try std.testing.expect(scriptPathIn(&buffer, "/a/b", "..\\x") == null);
}

test "copyForTest copies beside the test map, overwriting there, and warns when the script is missing" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("/maps/mine/m2_script.lua", "function Init() end");
    try fake.write("/gen/maps/m2_script.lua", "stale");

    try std.testing.expectEqual(CopyOutcome.copied, copyForTest(files, "", "/maps/mine/a.bzm", "/gen/maps", "m2_script"));
    try std.testing.expectEqualStrings("function Init() end", fake.contents("/gen/maps/m2_script.lua").?);
    // The source is untouched, and the copy was the one op.
    try std.testing.expectEqualStrings("function Init() end", fake.contents("/maps/mine/m2_script.lua").?);
    var copies: usize = 0;
    for (fake.op_log.items) |op| {
        if (op.kind != .copy) continue;
        copies += 1;
        try std.testing.expectEqualStrings("/maps/mine/m2_script.lua", op.from);
        try std.testing.expectEqualStrings("/gen/maps/m2_script.lua", op.to);
    }
    try std.testing.expectEqual(@as(usize, 1), copies);

    // No file beside the map: a warning outcome and no copy.
    const ops_before = fake.op_log.items.len;
    try std.testing.expectEqual(CopyOutcome.missing, copyForTest(files, "", "/maps/mine/a.bzm", "/gen/maps", "other"));
    try std.testing.expectEqual(ops_before, fake.op_log.items.len);
}

test "copyForTest builds no path for a name that is not a bare name, and keeps a folder value to its last component" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("/maps/mine/x.lua", "beside the map");
    try fake.write("/maps/x.lua", "one folder up");
    try fake.write("/x.lua", "at the root");
    for ([_][]const u8{ "x.lua", "..", "a b", "", "dir\\", "a/..", "C:", "sub\\y.lua" }) |value| {
        try std.testing.expectEqual(CopyOutcome.not_a_bare_name, copyForTest(files, "", "/maps/mine/a.bzm", "/gen/maps", value));
    }
    try std.testing.expectEqual(@as(usize, 3), fake.op_log.items.len); // only the three writes above
    try std.testing.expect(fake.contents("/gen/maps/x.lua") == null);
    // A shipped map's folder value keeps only its last component, as the game does:
    // however many ".." lead to it, the file is the one beside the map and it lands
    // in the test folder.
    try std.testing.expectEqual(CopyOutcome.copied, copyForTest(files, "", "/maps/mine/a.bzm", "/gen/maps", "..\\..\\x"));
    try std.testing.expectEqualStrings("beside the map", fake.contents("/gen/maps/x.lua").?);
    for (fake.op_log.items) |op| {
        if (op.kind != .copy) continue;
        try std.testing.expectEqualStrings("/maps/mine/x.lua", op.from);
        try std.testing.expectEqualStrings("/gen/maps/x.lua", op.to);
    }
    try std.testing.expectEqualStrings("one folder up", fake.contents("/maps/x.lua").?);
    try std.testing.expectEqualStrings("at the root", fake.contents("/x.lua").?);
}

test "no script writer writes into a game's data folder, whichever caller asked (WR-B05)" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const data_roots = [_][]const u8{"/other/game/Data"};
    fake.data_roots = &data_roots;
    const files = fake.files();
    try fake.write("/other/game/Data/Maps/shipped.bzm", "map");
    try fake.write("/home/maps/m2_script.lua", "mine");
    try fake.write("/dl/m2_script.lua", "picked");
    const ops = fake.op_log.items.len;
    try std.testing.expectEqual(CopyIntoOutcome.shipped, copyInto(files, "/game/", "/other/game/Data/Maps/shipped.bzm", "/dl/m2_script.lua", true));
    try std.testing.expectEqual(CopyAlongOutcome.shipped, copyAlong(files, "/game/", "/home/maps/a.bzm", "/other/game/Data/Maps/shipped.bzm", "m2_script", true));
    try std.testing.expectEqual(CopyOutcome.shipped, copyForTest(files, "/game/", "/home/maps/a.bzm", "/other/game/Data/Maps", "m2_script"));
    // The installation's own Data by rule 1, with no marker needed.
    try std.testing.expectEqual(CopyIntoOutcome.shipped, copyInto(files, "/game/", "/game/Data/Maps/x.bzm", "/dl/m2_script.lua", true));
    try std.testing.expectEqual(ops, fake.op_log.items.len);
    try std.testing.expect(fake.contents("/other/game/Data/Maps/m2_script.lua") == null);
}

test "copyForTest never leaves an earlier test's script in place of one it could not copy (WR-C06)" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("/gen/maps/m2_script.lua", "stale, from an earlier test");
    // Nothing beside the map now: the stale copy goes too.
    try std.testing.expectEqual(CopyOutcome.missing, copyForTest(files, "", "/maps/mine/a.bzm", "/gen/maps", "m2_script"));
    try std.testing.expect(fake.contents("/gen/maps/m2_script.lua") == null);
    // A copy the disk refuses: the stale copy goes too.
    try fake.write("/gen/maps/m2_script.lua", "stale, from an earlier test");
    try fake.write("/maps/mine/m2_script.lua", "now");
    fake.fail_copy = true;
    try std.testing.expectEqual(CopyOutcome.failed, copyForTest(files, "", "/maps/mine/a.bzm", "/gen/maps", "m2_script"));
    try std.testing.expect(fake.contents("/gen/maps/m2_script.lua") == null);
    // The map's own script is never deleted when it is the test folder's file.
    fake.fail_copy = false;
    try std.testing.expectEqual(CopyOutcome.copied, copyForTest(files, "", "/maps/mine/a.bzm", "/maps/mine", "m2_script"));
    try std.testing.expectEqualStrings("now", fake.contents("/maps/mine/m2_script.lua").?);
}

test "gameScriptName is the last component when that is a bare name" {
    try std.testing.expectEqualStrings("m2_script", gameScriptName("m2_script").?);
    try std.testing.expectEqualStrings("BattleOfBulge", gameScriptName("maps\\BattleOfBulge").?);
    try std.testing.expectEqualStrings("c", gameScriptName("a/b/c").?);
    for ([_][]const u8{ "", "maps\\", "x.lua", "a\\..", "a b" }) |value| try std.testing.expect(gameScriptName(value) == null);
}

test "copyForTest reports a refusing disk, and leaves a script already in the test folder alone" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("/maps/mine/m2_script.lua", "one");
    fake.fail_copy = true;
    try std.testing.expectEqual(CopyOutcome.failed, copyForTest(files, "", "/maps/mine/a.bzm", "/gen/maps", "m2_script"));
    fake.fail_copy = false;
    // The map itself lives in the test folder: the file is where it is wanted.
    try std.testing.expectEqual(CopyOutcome.copied, copyForTest(files, "", "/maps/mine/a.bzm", "/maps/mine", "m2_script"));
    var copies: usize = 0;
    for (fake.op_log.items) |op| {
        if (op.kind == .copy) copies += 1;
    }
    try std.testing.expectEqual(@as(usize, 1), copies); // the failed one; the same-file one never copied
}

test "listBeside offers the bare names of the .lua files beside the map, sorted" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("/maps/mine/a.bzm", "map");
    try fake.write("/maps/mine/zeta.lua", "1");
    try fake.write("/maps/mine/Alpha.lua", "2");
    try fake.write("/maps/mine/two words.lua", "3"); // a name the rule refuses is not offered
    try fake.write("/maps/mine/x.lua.lua", "4");
    try fake.write("/maps/mine/readme.txt", "5");
    try fake.write("/maps/other/beta.lua", "6");
    var names: std.ArrayListUnmanaged([]u8) = .empty;
    defer files_mod.freeNames(std.testing.allocator, &names);
    try listBeside(files, std.testing.allocator, "\\maps\\mine\\a.bzm", &names);
    // "x.lua.lua" is the file "x.lua" plus the extension: its stem fails the rule.
    try std.testing.expectEqual(@as(usize, 2), names.items.len);
    try std.testing.expectEqualStrings("Alpha", names.items[0]);
    try std.testing.expectEqualStrings("zeta", names.items[1]);
}

test "listBeside lists the working directory for a map named with no folder (WR-B06)" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("here.lua", "1");
    try fake.write("a.bzm", "map");
    try fake.write("/elsewhere/there.lua", "2");
    var names: std.ArrayListUnmanaged([]u8) = .empty;
    defer files_mod.freeNames(std.testing.allocator, &names);
    try listBeside(files, std.testing.allocator, "a.bzm", &names);
    try std.testing.expectEqual(@as(usize, 1), names.items.len);
    try std.testing.expectEqualStrings("here", names.items[0]);
}

test "pickedName is the picked file's own name when it is a bare name plus .lua" {
    try std.testing.expectEqualStrings("m2_script", pickedName("/home/u/Downloads/m2_script.lua").?);
    try std.testing.expectEqualStrings("m2_script", pickedName("C:\\Users\\u\\m2_script.LUA").?);
    try std.testing.expectEqualStrings("s", pickedName("s.lua").?);
    for ([_][]const u8{ "/x/notes.txt", "/x/.lua", "/x/a b.lua", "/x/..lua", "/x/x.lua.lua", "/x/dir/", "" }) |picked| {
        try std.testing.expect(pickedName(picked) == null);
    }
}

test "copyInto puts a picked script beside the map, asks first when one is there, and refuses a bad name" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("/dl/mine.lua", "new");
    try fake.write("/dl/bad name.lua", "x");
    try fake.write("/dl/notes.txt", "x");
    const map = "/maps/mine/a.bzm";
    try std.testing.expectEqual(CopyIntoOutcome.copied, copyInto(files, "", map, "/dl/mine.lua", false));
    try std.testing.expectEqualStrings("new", fake.contents("/maps/mine/mine.lua").?);
    // A different file of that name is there now: ask, do not copy.
    try fake.write("/dl/mine.lua", "newer");
    try std.testing.expectEqual(CopyIntoOutcome.exists, copyInto(files, "", map, "/dl/mine.lua", false));
    try std.testing.expectEqualStrings("new", fake.contents("/maps/mine/mine.lua").?);
    // Told to overwrite, it does.
    try std.testing.expectEqual(CopyIntoOutcome.copied, copyInto(files, "", map, "/dl/mine.lua", true));
    try std.testing.expectEqualStrings("newer", fake.contents("/maps/mine/mine.lua").?);
    // A script already beside the map is chosen without a copy or a question.
    try std.testing.expectEqual(CopyIntoOutcome.copied, copyInto(files, "", map, "/maps/mine/mine.lua", false));
    // The name must be a bare name plus .lua; a missing file fails.
    try std.testing.expectEqual(CopyIntoOutcome.not_a_bare_name, copyInto(files, "", map, "/dl/bad name.lua", true));
    try std.testing.expectEqual(CopyIntoOutcome.not_a_bare_name, copyInto(files, "", map, "/dl/notes.txt", true));
    try std.testing.expectEqual(CopyIntoOutcome.failed, copyInto(files, "", map, "/dl/gone.lua", true));
    try std.testing.expect(fake.contents("/maps/mine/bad name.lua") == null);
}

test "copyAlong copies a script between the two maps' folders and reports a missing one without failing" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("/game/Data/Maps/Multiplayer/coldwinter.lua", "shipped");
    try std.testing.expectEqual(CopyAlongOutcome.copied, copyAlong(files, "", "/game/Data/Maps/Multiplayer/coldwinter.bzm", "/home/maps/mine.bzm", "coldwinter", false));
    try std.testing.expectEqualStrings("shipped", fake.contents("/home/maps/coldwinter.lua").?);
    // A value the shipped map holds as a folder path keeps its last component, as the game does.
    try std.testing.expectEqual(CopyAlongOutcome.copied, copyAlong(files, "", "/game/Data/Maps/Multiplayer/coldwinter.bzm", "/home/other/mine.bzm", "maps\\coldwinter", false));
    try std.testing.expectEqualStrings("shipped", fake.contents("/home/other/coldwinter.lua").?);
    // Nothing beside the old map: missing, nothing written.
    const ops = fake.op_log.items.len;
    try std.testing.expectEqual(CopyAlongOutcome.missing, copyAlong(files, "", "/game/Data/Maps/Multiplayer/coldwinter.bzm", "/home/maps/mine.bzm", "absent", false));
    try std.testing.expectEqual(ops, fake.op_log.items.len);
    // Both maps in one folder: nothing to copy.
    try std.testing.expectEqual(CopyAlongOutcome.copied, copyAlong(files, "", "/home/maps/a.bzm", "/home/maps/b.bzm", "coldwinter", false));
    // A name that builds no path copies nothing.
    try std.testing.expectEqual(CopyAlongOutcome.not_a_bare_name, copyAlong(files, "", "/game/a.bzm", "/home/b.bzm", "x.lua", false));
    try std.testing.expectEqual(CopyAlongOutcome.not_a_bare_name, copyAlong(files, "", "/game/a.bzm", "/home/b.bzm", "", false));
}

test "copyAlong never replaces a different script beside the new map unless told to (CR-B01)" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("/game/Data/Maps/Multiplayer/coldwinter.lua", "shipped");
    try fake.write("/home/maps/coldwinter.lua", "my own work");
    const ops = fake.op_log.items.len;
    try std.testing.expectEqual(CopyAlongOutcome.exists, copyAlong(files, "", "/game/Data/Maps/Multiplayer/coldwinter.bzm", "/home/maps/mine.bzm", "coldwinter", false));
    try std.testing.expectEqualStrings("my own work", fake.contents("/home/maps/coldwinter.lua").?);
    try std.testing.expectEqual(ops, fake.op_log.items.len);
    // Asked and answered Replace: the copy goes through.
    try std.testing.expectEqual(CopyAlongOutcome.copied, copyAlong(files, "", "/game/Data/Maps/Multiplayer/coldwinter.bzm", "/home/maps/mine.bzm", "coldwinter", true));
    try std.testing.expectEqualStrings("shipped", fake.contents("/home/maps/coldwinter.lua").?);
    // Nothing beside the old map is still missing, whatever `overwrite` says.
    try std.testing.expectEqual(CopyAlongOutcome.missing, copyAlong(files, "", "/game/Data/Maps/Multiplayer/coldwinter.bzm", "/home/maps/mine.bzm", "absent", true));
}

test "folderUrl is file:// and the resolved folder of a validated script, never the script itself (WR-B04)" {
    var fake = files_mod.FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try fake.write("/home/me/My Maps/m2_script.lua", "x");
    try fake.write("/home/me/My Maps/it's.lua", "x");
    var buffer: [512]u8 = undefined;
    if (builtin.os.tag != .windows) {
        try std.testing.expectEqualStrings("file:///home/me/My%20Maps/", folderUrl(files, &buffer, "/home/me/My Maps/a.bzm", "m2_script").?);
        // A folder value keeps its last component; a drive-less engine path works too.
        try std.testing.expectEqualStrings("file:///home/me/My%20Maps/", folderUrl(files, &buffer, "\\home\\me\\My Maps\\a.bzm", "maps\\m2_script").?);
    }
    // Every byte is unreserved, a slash, a percent escape or the drive's colon.
    const url = folderUrl(files, &buffer, "/home/me/My Maps/a.bzm", "m2_script").?;
    try std.testing.expect(std.mem.startsWith(u8, url, "file://"));
    try std.testing.expect(std.mem.endsWith(u8, url, "/"));
    try std.testing.expect(std.mem.indexOf(u8, url, ".lua") == null);
    for (url["file://".len..]) |char| try std.testing.expect(isUnreserved(char) or char == '/' or char == '%' or char == ':');
    // A file that is not there, None and a bad name give no URL - nothing to open.
    try std.testing.expect(folderUrl(files, &buffer, "/home/me/My Maps/a.bzm", "absent") == null);
    try std.testing.expect(folderUrl(files, &buffer, "/home/me/My Maps/a.bzm", "") == null);
    try std.testing.expect(folderUrl(files, &buffer, "/home/me/My Maps/a.bzm", "x.lua") == null);
    try std.testing.expect(folderUrl(files, &buffer, "/home/me/My Maps/a.bzm", "..\\..\\etc") == null);
    var tiny: [16]u8 = undefined;
    try std.testing.expect(folderUrl(files, &tiny, "/home/me/My Maps/a.bzm", "m2_script") == null);
    // The real path is the fake's own (symlinks resolved): a link is followed for the folder only.
    var linked = files_mod.FakeFiles.init(std.testing.allocator);
    defer linked.deinit();
    const link_list = [_]files_mod.FakeFiles.Link{.{ .from = "/home/me/maps", .to = "/mnt/disk/maps" }};
    linked.links = &link_list;
    try linked.write("/home/me/maps/s.lua", "x");
    if (builtin.os.tag != .windows) {
        try std.testing.expectEqualStrings("file:///mnt/disk/maps/", folderUrl(linked.files(), &buffer, "/home/me/maps/a.bzm", "s").?);
    }
}
