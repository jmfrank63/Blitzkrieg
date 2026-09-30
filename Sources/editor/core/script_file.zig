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
};

/// True when `a` and `b` are the same file: the same text, or the same real
/// path once both exist.
fn sameFile(files: Files, a: []const u8, b: []const u8) bool {
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
pub fn copyForTest(files: Files, map_path: []const u8, test_dir: []const u8, value: []const u8) CopyOutcome {
    var from_buffer: [files_mod.max_path]u8 = undefined;
    var to_buffer: [files_mod.max_path]u8 = undefined;
    const name = gameScriptName(value) orelse return .not_a_bare_name;
    const from = scriptPathBeside(&from_buffer, map_path, name) orelse return .not_a_bare_name;
    const to = scriptPathIn(&to_buffer, test_dir, name) orelse return .failed;
    if (!files.exists(from)) return .missing;
    if (sameFile(files, from, to)) return .copied;
    files.copy(from, to) catch return .failed;
    return .copied;
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

    try std.testing.expectEqual(CopyOutcome.copied, copyForTest(files, "/maps/mine/a.bzm", "/gen/maps", "m2_script"));
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
    try std.testing.expectEqual(CopyOutcome.missing, copyForTest(files, "/maps/mine/a.bzm", "/gen/maps", "other"));
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
        try std.testing.expectEqual(CopyOutcome.not_a_bare_name, copyForTest(files, "/maps/mine/a.bzm", "/gen/maps", value));
    }
    try std.testing.expectEqual(@as(usize, 3), fake.op_log.items.len); // only the three writes above
    try std.testing.expect(fake.contents("/gen/maps/x.lua") == null);
    // A shipped map's folder value keeps only its last component, as the game does:
    // however many ".." lead to it, the file is the one beside the map and it lands
    // in the test folder.
    try std.testing.expectEqual(CopyOutcome.copied, copyForTest(files, "/maps/mine/a.bzm", "/gen/maps", "..\\..\\x"));
    try std.testing.expectEqualStrings("beside the map", fake.contents("/gen/maps/x.lua").?);
    for (fake.op_log.items) |op| {
        if (op.kind != .copy) continue;
        try std.testing.expectEqualStrings("/maps/mine/x.lua", op.from);
        try std.testing.expectEqualStrings("/gen/maps/x.lua", op.to);
    }
    try std.testing.expectEqualStrings("one folder up", fake.contents("/maps/x.lua").?);
    try std.testing.expectEqualStrings("at the root", fake.contents("/x.lua").?);
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
    try std.testing.expectEqual(CopyOutcome.failed, copyForTest(files, "/maps/mine/a.bzm", "/gen/maps", "m2_script"));
    fake.fail_copy = false;
    // The map itself lives in the test folder: the file is where it is wanted.
    try std.testing.expectEqual(CopyOutcome.copied, copyForTest(files, "/maps/mine/a.bzm", "/maps/mine", "m2_script"));
    var copies: usize = 0;
    for (fake.op_log.items) |op| {
        if (op.kind == .copy) copies += 1;
    }
    try std.testing.expectEqual(@as(usize, 1), copies); // the failed one; the same-file one never copied
}
