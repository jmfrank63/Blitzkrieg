//! What resource-editor-game-reads-it asks of the Game's log: that the engine's
//! stream layer (BK_MOD_TRACE, StreamIOZig/streamio.zig) opened a given file
//! from the MOD overlay, so the file was read from the exported mod and not
//! from the shipped Data it replaces. Kept apart so `test-resource-app-logic`
//! covers it without a Game.
const std = @import("std");

/// The line the stream layer prints for a stream the MOD overlay answered.
pub const trace_prefix = "BK_MOD_TRACE: open \"";
const trace_suffix = "\" from MOD";

/// True when `log` holds a trace line for `path` from the MOD. The engine asks
/// for names in any case and with either separator, so both sides compare as
/// lower case with `/`.
pub fn modOpened(log: []const u8, path: []const u8) bool {
    var rows = std.mem.splitScalar(u8, log, '\n');
    while (rows.next()) |raw| {
        const row = std.mem.trim(u8, raw, "\r ");
        if (!std.mem.startsWith(u8, row, trace_prefix) or !std.mem.endsWith(u8, row, trace_suffix)) continue;
        const name = row[trace_prefix.len .. row.len - trace_suffix.len];
        if (sameName(name, path)) return true;
    }
    return false;
}

fn sameName(a: []const u8, b: []const u8) bool {
    if (a.len != b.len) return false;
    for (a, b) |x, y| {
        const nx = if (x == '\\') '/' else std.ascii.toLower(x);
        const ny = if (y == '\\') '/' else std.ascii.toLower(y);
        if (nx != ny) return false;
    }
    return true;
}

/// One row of tools/zig/fixtures/resource_editor/game-reads-it.auto, which says where a kind's export lands
/// and which shipped resource it takes the place of: `<ext> <folder> <exported> <shipped> <file>`, all but the
/// first two relative to the mod's data folder, `-` for none. The export of a project is named after the
/// folder it sits in, one level deep; `shipped` is the deeper path of the resource the Game loads, which the
/// export folder is copied to, and `file` is the stream the Game's trace must show from the mod.
pub const Row = struct {
    ext: []const u8,
    folder: []const u8,
    exported: []const u8,
    shipped: []const u8,
    file: []const u8,
};

/// The row of `ext` in the manifest text; `#` lines and blank lines are skipped. Null when there is no row
/// or it has fewer than five columns.
pub fn findRow(text: []const u8, ext: []const u8) ?Row {
    var rows = std.mem.splitScalar(u8, text, '\n');
    while (rows.next()) |raw| {
        const row = std.mem.trim(u8, raw, "\r \t");
        if (row.len == 0 or row[0] == '#') continue;
        var columns = std.mem.tokenizeAny(u8, row, " \t");
        const first = columns.next() orelse continue;
        if (!std.mem.eql(u8, first, ext)) continue;
        return .{
            .ext = first,
            .folder = columns.next() orelse return null,
            .exported = columns.next() orelse return null,
            .shipped = columns.next() orelse return null,
            .file = columns.next() orelse return null,
        };
    }
    return null;
}

/// One line of result.log: `KIND=<ext> PROOF=<game|reader> FILE=<path> RESULT=<PASS|FAIL>`.
pub fn resultLine(buffer: []u8, kind: []const u8, proof: []const u8, file: []const u8, passed: bool) ?[]const u8 {
    return std.fmt.bufPrint(buffer, "KIND={s} PROOF={s} FILE={s} RESULT={s}\n", .{ kind, proof, file, if (passed) "PASS" else "FAIL" }) catch null;
}

test "modOpened finds the traced file whatever the case and separator" {
    const log =
        "BK_MOD_TRACE: open \"modobjects.xml\" from MOD\r\n" ++
        "BK_MOD_TRACE: open \"weapons\\Mosin.xml\" from MOD\r\n" ++
        "BK_MOD_TRACE: open \"units/humans/ussr/mosin/1.xml\" from ELK\n";
    try std.testing.expect(modOpened(log, "weapons/mosin.xml"));
    try std.testing.expect(modOpened(log, "Weapons\\MOSIN.xml"));
    try std.testing.expect(modOpened(log, "modobjects.xml"));
}

test "modOpened refuses a file another overlay answered, a prefix and a missing one" {
    const log =
        "BK_MOD_TRACE: open \"units/humans/ussr/mosin/1.xml\" from ELK\n" ++
        "BK_MOD_TRACE: open \"weapons/mosin.xml.bak\" from MOD\n" ++
        "weapons/mg_34.xml\n";
    try std.testing.expect(!modOpened(log, "units/humans/ussr/mosin/1.xml"));
    try std.testing.expect(!modOpened(log, "weapons/mosin.xml"));
    try std.testing.expect(!modOpened(log, "weapons/mg_34.xml"));
    try std.testing.expect(!modOpened("", "weapons/mosin.xml"));
}

test "resultLine names the kind, the proof, the file and the verdict" {
    var buffer: [128]u8 = undefined;
    try std.testing.expectEqualStrings("KIND=wpn PROOF=game FILE=weapons/mosin.xml RESULT=PASS\n", resultLine(&buffer, "wpn", "game", "weapons/mosin.xml", true).?);
    try std.testing.expectEqualStrings("KIND=wpn PROOF=reader FILE=x RESULT=FAIL\n", resultLine(&buffer, "wpn", "reader", "x", false).?);
}

test "findRow reads the kind's five columns and skips comments" {
    const text = "# ext folder exported shipped file\r\n\r\nwpn mosin - - weapons/mosin.xml\r\nunt mosin units/humans/mosin units/humans/ussr/mosin units/humans/ussr/mosin/1.xml\r\nshort a b\r\n";
    const wpn = findRow(text, "wpn").?;
    try std.testing.expectEqualStrings("mosin", wpn.folder);
    try std.testing.expectEqualStrings("-", wpn.shipped);
    try std.testing.expectEqualStrings("weapons/mosin.xml", wpn.file);
    const unt = findRow(text, "unt").?;
    try std.testing.expectEqualStrings("units/humans/mosin", unt.exported);
    try std.testing.expectEqualStrings("units/humans/ussr/mosin/1.xml", unt.file);
    try std.testing.expect(findRow(text, "msh") == null);
    try std.testing.expect(findRow(text, "short") == null);
}
