//! Tests for `bk-table`'s sheet work; the command-line contract (exit codes,
//! streams) is in `bk_tool_conventions_test.zig`.
//!
//! What is proved here, each against bytes:
//!
//! - `export` of the fixture stat folder (`fixtures/small-tools/stats`) is the
//!   expected legacy sheet byte for byte, with and without recursion and with
//!   `--only`.
//! - `import` changes only the edited value's bytes: a file with a BOM,
//!   comments, CDATA, entities, both quote styles and odd whitespace comes back
//!   identical except for that value. `_`, empty and unchanged cells write
//!   nothing.
//! - export -> import -> export is stable, and an edited sheet reads back as
//!   edited.
//! - A missing file or path is reported and skipped and nothing is created.
//! - The root table (`*.bld`/`*.obt` -> desc, `*.eff` -> effect, else RPG) and
//!   `--root` pick the stat root on export and on import.
//! - The strict reader's errors name the exact line and column.

const std = @import("std");
const bk_table = @import("bk_table.zig");
const xml = @import("bk_table_xml.zig");

const fixture_files = [_]struct { path: []const u8, data: []const u8 }{
    .{ .path = "stats/tank.xml", .data = @embedFile("fixtures/small-tools/stats/tank.xml") },
    .{ .path = "stats/truck.xml", .data = @embedFile("fixtures/small-tools/stats/truck.xml") },
    .{ .path = "stats/heavy/tiger.xml", .data = @embedFile("fixtures/small-tools/stats/heavy/tiger.xml") },
    .{ .path = "stats/readme.txt", .data = @embedFile("fixtures/small-tools/stats/readme.txt") },
};

/// The fixture folder's sheet, as ExcelExporter would have written it for
/// `*.xml` with `/` paths: columns in byte order, rows sorted, LF line ends.
const expected_sheet =
    "FileName\t#comment\tArmor;Front\tArmor;Side\tCargo;#cdata-section\tGuns;item(00);Ammo\tGuns;item(00);Weapon\tGuns;item(01);Ammo\tGuns;item(01);Weapon\tKeyName;#text\tMaxHP\tPrice\n" ++
    "stats/heavy/tiger.xml\t heavy \t100\t80\t\t92\t88mm\t\t\tTiger\t300\t90\n" ++
    "stats/tank.xml\t\t30\t20\t\t80\t75mm\t3000\tMG34\tTank & crew\t120\t40\n" ++
    "stats/truck.xml\t\t5\t5\tshells <75mm>\t\t\t\t\tTruck\t40\t10\n";

fn writeFile(io: std.Io, dir: std.Io.Dir, sub_path: []const u8, data: []const u8) !void {
    if (std.fs.path.dirname(sub_path)) |parent| try dir.createDirPath(io, parent);
    try dir.writeFile(io, .{ .sub_path = sub_path, .data = data });
}

fn readAll(io: std.Io, dir: std.Io.Dir, sub_path: []const u8, gpa: std.mem.Allocator) ![]u8 {
    return dir.readFileAlloc(io, sub_path, gpa, .unlimited);
}

fn writeFixture(io: std.Io, dir: std.Io.Dir) !void {
    for (fixture_files) |file| try writeFile(io, dir, file.path, file.data);
}

fn expectFixtureUnchanged(io: std.Io, dir: std.Io.Dir) !void {
    const gpa = std.testing.allocator;
    for (fixture_files) |file| {
        const data = try readAll(io, dir, file.path, gpa);
        defer gpa.free(data);
        try std.testing.expectEqualStrings(file.data, data);
    }
}

/// Export into a fresh sheet; problems are printed so a failure explains itself.
fn exportOk(dir: std.Io.Dir, options: bk_table.ExportOptions) ![]u8 {
    const gpa = std.testing.allocator;
    var problems: std.Io.Writer.Allocating = .init(gpa);
    defer problems.deinit();
    const result = bk_table.exportSheet(std.testing.io, gpa, dir, options, &problems.writer) catch |err| {
        std.debug.print("export failed: {s}\n", .{problems.written()});
        return err;
    };
    try std.testing.expectEqualStrings("", problems.written());
    return result.sheet;
}

const Import = struct {
    result: bk_table.ImportResult,
    problems: []u8,

    fn deinit(import: Import) void {
        std.testing.allocator.free(import.problems);
    }
};

fn importSheet(dir: std.Io.Dir, sheet: []const u8, root: ?[]const u8) !Import {
    const gpa = std.testing.allocator;
    var problems: std.Io.Writer.Allocating = .init(gpa);
    defer problems.deinit();
    const result = try bk_table.importSheet(std.testing.io, gpa, dir, sheet, root, &problems.writer);
    return .{ .result = result, .problems = try problems.toOwnedSlice() };
}

/// `text` with its only occurrence of `old` replaced by `new`.
fn replaceOnce(text: []const u8, old: []const u8, new: []const u8) ![]u8 {
    try std.testing.expectEqual(@as(usize, 1), std.mem.count(u8, text, old));
    return std.mem.replaceOwned(u8, std.testing.allocator, text, old, new);
}

test "export of the fixture stat folder is the legacy sheet byte for byte" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);

    const sheet = try exportOk(tmp.dir, .{ .mask = "*.xml", .folder = "stats" });
    defer std.testing.allocator.free(sheet);
    try std.testing.expectEqualStrings(expected_sheet, sheet);

    // A trailing separator or a backslash in the folder changes nothing.
    const again = try exportOk(tmp.dir, .{ .mask = "*.XML", .folder = "stats\\" });
    defer std.testing.allocator.free(again);
    try std.testing.expectEqualStrings(expected_sheet, again);

    const flat = try exportOk(tmp.dir, .{ .mask = "*.xml", .folder = "stats", .recurse = false });
    defer std.testing.allocator.free(flat);
    try std.testing.expectEqualStrings(
        "FileName\tArmor;Front\tArmor;Side\tCargo;#cdata-section\tGuns;item(00);Ammo\tGuns;item(00);Weapon\tGuns;item(01);Ammo\tGuns;item(01);Weapon\tKeyName;#text\tMaxHP\tPrice\n" ++
            "stats/tank.xml\t30\t20\t\t80\t75mm\t3000\tMG34\tTank & crew\t120\t40\n" ++
            "stats/truck.xml\t5\t5\tshells <75mm>\t\t\t\t\tTruck\t40\t10\n",
        flat,
    );

    const only = try exportOk(tmp.dir, .{
        .mask = "*.xml",
        .folder = "stats",
        .filter = .{ .mode = .only, .fields = &.{ "Armor;Front", "Guns;item;Weapon" } },
    });
    defer std.testing.allocator.free(only);
    try std.testing.expectEqualStrings(
        "FileName\tArmor;Front\tGuns;item(00);Weapon\tGuns;item(01);Weapon\n" ++
            "stats/heavy/tiger.xml\t100\t88mm\t\n" ++
            "stats/tank.xml\t30\t75mm\tMG34\n" ++
            "stats/truck.xml\t5\t\t\n",
        only,
    );
    try expectFixtureUnchanged(io, tmp.dir);
}

test "the field list keeps the legacy prefix and item rules, and ignoring a field drops only it" {
    const arena_state = std.testing.allocator;
    var arena: std.heap.ArenaAllocator = .init(arena_state);
    defer arena.deinit();
    const fields = try bk_table.Filter.parseFieldList(arena.allocator(), "Armor;Front  the front plate\r\n\r\n  Guns;item;Weapon\n");
    try std.testing.expectEqual(@as(usize, 2), fields.len);
    try std.testing.expectEqualStrings("Armor;Front", fields[0]);
    try std.testing.expectEqualStrings("Guns;item;Weapon", fields[1]);

    const only: bk_table.Filter = .{ .mode = .only, .fields = fields };
    try std.testing.expect(only.keeps("Armor;Front"));
    try std.testing.expect(!only.keeps("Armor;Side"));
    try std.testing.expect(only.keeps("Guns;item(07);Weapon"));
    try std.testing.expect(!only.keeps("Guns;item(07);Ammo"));
    // Only two-digit numbers read as `item`, as in the legacy tool.
    try std.testing.expect(!only.keeps("Guns;item(100);Weapon"));

    const ignore: bk_table.Filter = .{ .mode = .ignore, .fields = fields };
    try std.testing.expect(!ignore.keeps("Armor;Front"));
    try std.testing.expect(ignore.keeps("Armor;Side"));
    try std.testing.expect(!ignore.keeps("Guns;item(01);Weapon"));
    try std.testing.expect(ignore.keeps("Guns;item(01);Ammo"));

    // A field is a path prefix; an empty --only list keeps everything.
    const prefix: bk_table.Filter = .{ .mode = .only, .fields = &.{"Armor"} };
    try std.testing.expect(prefix.keeps("Armor;Side"));
    try std.testing.expect((bk_table.Filter{ .mode = .only }).keeps("MaxHP"));
}

test "masks match like the legacy Windows ones" {
    try std.testing.expect(bk_table.maskMatches("*.xml", "Tank.XML"));
    try std.testing.expect(bk_table.maskMatches("1.xml", "1.xml"));
    try std.testing.expect(!bk_table.maskMatches("1.xml", "11.xml"));
    try std.testing.expect(bk_table.maskMatches("t?nk.*", "tank.xml"));
    try std.testing.expect(bk_table.maskMatches("*", "anything"));
    try std.testing.expect(bk_table.maskMatches("*a*b*", "xxaxxbxx"));
    try std.testing.expect(!bk_table.maskMatches("*.bld", "a.bld.bak"));
}

/// A stat file written to be awkward: a BOM, a declaration in single quotes,
/// comments outside and inside the root, CRLF mixed with LF, tabs, spaces
/// around `=`, entities and character references, CDATA, an attribute split
/// over two lines, and content after the root.
const odd_source =
    "\xEF\xBB\xBF<?xml version='1.0' encoding=\"utf-8\"?>\n" ++
    "<!-- top comment -->\r\n" ++
    "<base>\n" ++
    "  <RPG   MaxHP = '1500'\tName=\"Tom &amp; Jerry\" >\r\n" ++
    "\t<!--  keep me  -->\n" ++
    "    <KeyName>   a &lt;b&gt; &#65;&#x42;  </KeyName>\r\n" ++
    "<Script><![CDATA[if (a < b) && c]]></Script>\n" ++
    "  <Segments><item  v='1' /><item\n v=\"2\"/></Segments>\n" ++
    "  </RPG>\n" ++
    "  <Other x=\"1\"/>\n" ++
    "</base>\n<!-- trailing -->\n";

/// Import a one-cell sheet against `odd.xml` and check the file is the
/// original with `old` (the value in its context) replaced by `new`.
fn expectOnlyValueChanged(column: []const u8, cell: []const u8, old: []const u8, new: []const u8) !void {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFile(io, tmp.dir, "odd.xml", odd_source);

    const sheet = try std.fmt.allocPrint(gpa, "FileName\t{s}\nodd.xml\t{s}\n", .{ column, cell });
    defer gpa.free(sheet);
    const import = try importSheet(tmp.dir, sheet, null);
    defer import.deinit();
    try std.testing.expectEqualStrings("", import.problems);
    try std.testing.expectEqual(@as(usize, 1), import.result.cells_changed);
    try std.testing.expectEqual(@as(usize, 1), import.result.files_changed);

    const expected = try replaceOnce(odd_source, old, new);
    defer gpa.free(expected);
    const actual = try readAll(io, tmp.dir, "odd.xml", gpa);
    defer gpa.free(actual);
    try std.testing.expectEqualStrings(expected, actual);
}

test "import patches only the edited value and every other byte survives" {
    try expectOnlyValueChanged("MaxHP", "1600", "'1500'", "'1600'");
    try expectOnlyValueChanged("Name", "Tom & \"Jerry\" <3", "\"Tom &amp; Jerry\"", "\"Tom &amp; &quot;Jerry&quot; &lt;3\"");
    try expectOnlyValueChanged("KeyName;#text", "x<y & z>", "   a &lt;b&gt; &#65;&#x42;  ", "   x&lt;y &amp; z&gt;  ");
    try expectOnlyValueChanged("Script;#cdata-section", "b > a", "if (a < b) && c", "b > a");
    try expectOnlyValueChanged("#comment", " edited ", "<!--  keep me  -->", "<!-- edited -->");
    try expectOnlyValueChanged("Segments;item(01);v", "it's", "v=\"2\"", "v=\"it's\"");
    try expectOnlyValueChanged("Segments;item(00);v", "it's", "v='1'", "v='it&apos;s'");
}

test "unchanged, empty and _ cells write nothing; values a syntax cannot hold are skipped" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFile(io, tmp.dir, "odd.xml", odd_source);

    {
        const import = try importSheet(tmp.dir, "FileName\tMaxHP\tName\tKeyName;#text\tScript;#cdata-section\nodd.xml\t1500\t_\ta <b> AB\t\n", null);
        defer import.deinit();
        try std.testing.expectEqualStrings("", import.problems);
        try std.testing.expectEqual(@as(usize, 0), import.result.files_changed);
    }
    {
        const import = try importSheet(tmp.dir, "FileName\t#comment\tScript;#cdata-section\nodd.xml\ta -- b\tx ]]> y\n", null);
        defer import.deinit();
        try std.testing.expectEqual(@as(usize, 2), import.result.problems);
        try std.testing.expectEqual(@as(usize, 0), import.result.files_changed);
        try std.testing.expectEqualStrings(
            "bk-table: odd.xml: #comment: a comment cannot hold '--' or end with '-'; cell skipped\n" ++
                "bk-table: odd.xml: Script;#cdata-section: a CDATA section cannot hold ']]>'; cell skipped\n",
            import.problems,
        );
    }
    const actual = try readAll(io, tmp.dir, "odd.xml", gpa);
    defer gpa.free(actual);
    try std.testing.expectEqualStrings(odd_source, actual);
}

test "export -> import -> export is stable and an edited sheet reads back as edited" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);

    const first = try exportOk(tmp.dir, .{ .mask = "*.xml", .folder = "stats" });
    defer gpa.free(first);
    {
        const import = try importSheet(tmp.dir, first, null);
        defer import.deinit();
        try std.testing.expectEqualStrings("", import.problems);
        try std.testing.expectEqual(@as(usize, 3), import.result.rows);
        try std.testing.expectEqual(@as(usize, 0), import.result.files_changed);
    }
    try expectFixtureUnchanged(io, tmp.dir);
    const second = try exportOk(tmp.dir, .{ .mask = "*.xml", .folder = "stats" });
    defer gpa.free(second);
    try std.testing.expectEqualStrings(first, second);

    const edit_1 = try replaceOnce(first, "\tTank & crew\t", "\tTank & <crew>\t");
    defer gpa.free(edit_1);
    const edit_2 = try replaceOnce(edit_1, "\t3000\t", "\t2500\t");
    defer gpa.free(edit_2);
    const edited = try replaceOnce(edit_2, "\tTiger\t300\t", "\tTiger\t320\t");
    defer gpa.free(edited);
    {
        const import = try importSheet(tmp.dir, edited, null);
        defer import.deinit();
        try std.testing.expectEqualStrings("", import.problems);
        try std.testing.expectEqual(@as(usize, 3), import.result.cells_changed);
        try std.testing.expectEqual(@as(usize, 2), import.result.files_changed);
    }
    const third = try exportOk(tmp.dir, .{ .mask = "*.xml", .folder = "stats" });
    defer gpa.free(third);
    try std.testing.expectEqualStrings(edited, third);

    const tank_1 = try replaceOnce(fixture_files[0].data, "Tank &amp; crew", "Tank &amp; &lt;crew&gt;");
    defer gpa.free(tank_1);
    const tank = try replaceOnce(tank_1, "Ammo=\"3000\"", "Ammo=\"2500\"");
    defer gpa.free(tank);
    const tank_actual = try readAll(io, tmp.dir, "stats/tank.xml", gpa);
    defer gpa.free(tank_actual);
    try std.testing.expectEqualStrings(tank, tank_actual);
    const truck_actual = try readAll(io, tmp.dir, "stats/truck.xml", gpa);
    defer gpa.free(truck_actual);
    try std.testing.expectEqualStrings(fixture_files[1].data, truck_actual);
}

test "a sheet saved by a spreadsheet (CRLF, quoted cells) imports, and a leading quote round-trips" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);

    {
        const import = try importSheet(tmp.dir, "FileName\tKeyName;#text\tMaxHP\r\n\"stats/tank.xml\"\t\"\"\"Tiger\"\" II\"\t121\r\n", null);
        defer import.deinit();
        try std.testing.expectEqualStrings("", import.problems);
        try std.testing.expectEqual(@as(usize, 2), import.result.cells_changed);
    }
    const sheet = try exportOk(tmp.dir, .{ .mask = "tank.xml", .folder = "stats" });
    defer gpa.free(sheet);
    try std.testing.expect(std.mem.indexOf(u8, sheet, "\t\"\"\"Tiger\"\" II\"\t121\t") != null);
    const import = try importSheet(tmp.dir, sheet, null);
    defer import.deinit();
    try std.testing.expectEqual(@as(usize, 0), import.result.files_changed);
}

test "a missing file or path is reported and skipped and nothing is created" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);

    const import = try importSheet(tmp.dir, "FileName\tMaxHP\tCargo;#cdata-section\tArmor;Front\n" ++
        "stats/gone.xml\t1\t\t\n" ++
        "stats/tank.xml\t125\tcannons\t_\n", null);
    defer import.deinit();
    try std.testing.expectEqual(@as(usize, 2), import.result.problems);
    try std.testing.expectEqual(@as(usize, 1), import.result.cells_changed);
    const sep = std.fs.path.sep_str;
    try std.testing.expectEqualStrings(
        "bk-table: stats" ++ sep ++ "gone.xml: no such file; row skipped\n" ++
            "bk-table: stats" ++ sep ++ "tank.xml: Cargo;#cdata-section is not in the file; cell skipped\n",
        import.problems,
    );
    try std.testing.expectError(error.FileNotFound, tmp.dir.access(io, "stats/gone.xml", .{}));
    const expected = try replaceOnce(fixture_files[0].data, "MaxHP=\"120\"", "MaxHP=\"125\"");
    defer gpa.free(expected);
    const actual = try readAll(io, tmp.dir, "stats/tank.xml", gpa);
    defer gpa.free(actual);
    try std.testing.expectEqualStrings(expected, actual);
}

test "the root comes from the extension table or --root, on export and on import" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    try std.testing.expectEqualStrings("desc", bk_table.rootFor("a/b.bld", null));
    try std.testing.expectEqualStrings("desc", bk_table.rootFor("B.OBT", null));
    try std.testing.expectEqualStrings("effect", bk_table.rootFor("fire.Eff", null));
    try std.testing.expectEqualStrings("RPG", bk_table.rootFor("1.xml", null));
    try std.testing.expectEqualStrings("Stats", bk_table.rootFor("a.bld", "Stats"));

    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    // Each file also holds the roots it must not use.
    try writeFile(io, tmp.dir, "roots/a.bld", "<base><RPG HP=\"wrong\"/><desc HP=\"1\"/></base>");
    try writeFile(io, tmp.dir, "roots/b.OBT", "<base><desc HP=\"2\"/><RPG HP=\"wrong\"/></base>");
    try writeFile(io, tmp.dir, "roots/c.eff", "<base><desc HP=\"wrong\"/><effect HP=\"3\"/></base>");
    try writeFile(io, tmp.dir, "roots/d.xml", "<base><desc HP=\"wrong\"/><RPG HP=\"4\"/></base>");
    try writeFile(io, tmp.dir, "roots/e.stat", "<base><RPG HP=\"wrong\"/><Stats HP=\"5\"/></base>");

    const cases = [_]struct { mask: []const u8, root: ?[]const u8, sheet: []const u8 }{
        .{ .mask = "*.bld", .root = null, .sheet = "FileName\tHP\nroots/a.bld\t1\n" },
        .{ .mask = "*.obt", .root = null, .sheet = "FileName\tHP\nroots/b.OBT\t2\n" },
        .{ .mask = "*.eff", .root = null, .sheet = "FileName\tHP\nroots/c.eff\t3\n" },
        .{ .mask = "*.xml", .root = null, .sheet = "FileName\tHP\nroots/d.xml\t4\n" },
        .{ .mask = "*.stat", .root = null, .sheet = "FileName\tHP\nroots/e.stat\twrong\n" },
        .{ .mask = "*.stat", .root = "Stats", .sheet = "FileName\tHP\nroots/e.stat\t5\n" },
        .{ .mask = "*.bld", .root = "RPG", .sheet = "FileName\tHP\nroots/a.bld\twrong\n" },
    };
    for (cases) |case| {
        const sheet = try exportOk(tmp.dir, .{ .mask = case.mask, .folder = "roots", .root = case.root });
        defer gpa.free(sheet);
        try std.testing.expectEqualStrings(case.sheet, sheet);
    }

    // Import picks the root per row the same way: only the right HP changes.
    {
        const import = try importSheet(tmp.dir, "FileName\tHP\nroots/a.bld\t10\nroots/c.eff\t30\nroots/d.xml\t40\n", null);
        defer import.deinit();
        try std.testing.expectEqualStrings("", import.problems);
        try std.testing.expectEqual(@as(usize, 3), import.result.files_changed);
    }
    {
        const import = try importSheet(tmp.dir, "FileName\tHP\nroots/e.stat\t50\n", "Stats");
        defer import.deinit();
        try std.testing.expectEqualStrings("", import.problems);
    }
    const expected = [_][2][]const u8{
        .{ "roots/a.bld", "<base><RPG HP=\"wrong\"/><desc HP=\"10\"/></base>" },
        .{ "roots/c.eff", "<base><desc HP=\"wrong\"/><effect HP=\"30\"/></base>" },
        .{ "roots/d.xml", "<base><desc HP=\"wrong\"/><RPG HP=\"40\"/></base>" },
        .{ "roots/e.stat", "<base><RPG HP=\"wrong\"/><Stats HP=\"50\"/></base>" },
    };
    for (expected) |pair| {
        const actual = try readAll(io, tmp.dir, pair[0], gpa);
        defer gpa.free(actual);
        try std.testing.expectEqualStrings(pair[1], actual);
    }

    // A file without its root is an error on export, with where to look.
    try writeFile(io, tmp.dir, "noroot/x.eff", "<base>\n<RPG/>\n</base>");
    var problems: std.Io.Writer.Allocating = .init(gpa);
    defer problems.deinit();
    try std.testing.expectError(error.Failed, bk_table.exportSheet(io, gpa, tmp.dir, .{ .mask = "*", .folder = "noroot" }, &problems.writer));
    const sep = std.fs.path.sep_str;
    try std.testing.expectEqualStrings("bk-table: noroot" ++ sep ++ "x.eff:1:1: no <effect> element under the document element (--root <name> picks another stat root)\n", problems.written());
}

test "export fails whole, with line and column, on a file it cannot put in a sheet" {
    const io = std.testing.io;
    const gpa = std.testing.allocator;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFile(io, tmp.dir, "bad/a.xml", "<base><RPG><Note>a&#9;b</Note></RPG></base>");
    try writeFile(io, tmp.dir, "bad/b.xml", "<base>\n<RPG>\n<Note>x</Nope></RPG></base>");
    try writeFile(io, tmp.dir, "bad/c.xml", "<base><RPG HP=\"1\"/></base>");
    var problems: std.Io.Writer.Allocating = .init(gpa);
    defer problems.deinit();
    try std.testing.expectError(error.Failed, bk_table.exportSheet(io, gpa, tmp.dir, .{ .mask = "*.xml", .folder = "bad" }, &problems.writer));
    const sep = std.fs.path.sep_str;
    try std.testing.expectEqualStrings(
        "bk-table: bad" ++ sep ++ "a.xml:1:18: the value of Note;#text holds a tab or line break, which a sheet cannot\n" ++
            "bk-table: bad" ++ sep ++ "b.xml:3:10: expected </Note>, found </Nope>\n",
        problems.written(),
    );
}

test "a sheet that cannot be read fails before any file is touched" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    try writeFixture(io, tmp.dir);
    const cases = [_][2][]const u8{
        .{ "Name\tMaxHP\nstats/tank.xml\t1\n", "bk-table: the sheet does not start with a FileName column\n" },
        .{ "FileName\tMaxHP\tMaxHP\nstats/tank.xml\t1\t2\n", "bk-table: column MaxHP appears twice in the sheet\n" },
        .{ "FileName\tKeyName;#text\nstats/tank.xml\tPanzer\nstats/truck.xml\tLKW \xE4\n", "bk-table: the sheet is not UTF-8 (line 3); save it as UTF-8 text\n" },
    };
    for (cases) |case| {
        var problems: std.Io.Writer.Allocating = .init(std.testing.allocator);
        defer problems.deinit();
        try std.testing.expectError(error.Failed, bk_table.importSheet(io, std.testing.allocator, tmp.dir, case[0], null, &problems.writer));
        try std.testing.expectEqualStrings(case[1], problems.written());
    }
    try expectFixtureUnchanged(io, tmp.dir);
}

test "flattening follows the legacy paths: item(NN) per parent, text, CDATA, comments" {
    var arena_state: std.heap.ArenaAllocator = .init(std.testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();

    // The shape of a shipped file (Data/Bridges/asphaltbridge/01/1.xml).
    const source =
        "<?xml version=\"1.0\"?>\r\n" ++
        "<base><History><Action>Exported as</Action></History><RPG MaxHP=\"1500\" Burn=\"0\"><KeyName></KeyName>" ++
        "<StatsType>Bridge</StatsType><DamagedHPs/><Defence0 MinArmor=\"40\"/>" ++
        "<Segments><item FrameIndex=\"0\"><Passability><item size_x=\"8\"/><item>0001</item></Passability></item>" ++
        "<Other/><item FrameIndex=\"1\">  </item></Segments></RPG><RPG Ignored=\"1\"/></base>\r\n";
    var diag: xml.Diagnostic = .{};
    const fields = try xml.flatten(arena, source, "RPG", &diag);
    const expected = [_][2][]const u8{
        .{ "MaxHP", "1500" },
        .{ "Burn", "0" },
        .{ "StatsType;#text", "Bridge" },
        .{ "Defence0;MinArmor", "40" },
        .{ "Segments;item(00);FrameIndex", "0" },
        .{ "Segments;item(00);Passability;item(00);size_x", "8" },
        .{ "Segments;item(00);Passability;item(01);#text", "0001" },
        .{ "Segments;item(01);FrameIndex", "1" },
    };
    try std.testing.expectEqual(expected.len, fields.len);
    for (expected, fields) |pair, field| {
        try std.testing.expectEqualStrings(pair[0], field.path);
        try std.testing.expectEqualStrings(pair[1], field.value);
        try std.testing.expectEqualStrings(pair[1], source[field.start..field.end]);
    }

    // `%.2d`: the 101st item is item(100).
    var long: std.ArrayList(u8) = .empty;
    try long.appendSlice(arena, "<base><RPG><L>");
    for (0..101) |_| try long.appendSlice(arena, "<item>v</item>");
    try long.appendSlice(arena, "</L></RPG></base>");
    const items = try xml.flatten(arena, long.items, "RPG", &diag);
    try std.testing.expectEqual(@as(usize, 101), items.len);
    try std.testing.expectEqualStrings("L;item(00);#text", items[0].path);
    try std.testing.expectEqualStrings("L;item(99);#text", items[99].path);
    try std.testing.expectEqualStrings("L;item(100);#text", items[100].path);
}

test "the strict reader names the exact line and column of what it refuses" {
    var arena_state: std.heap.ArenaAllocator = .init(std.testing.allocator);
    defer arena_state.deinit();
    const cases = [_]struct { source: []const u8, line: usize, column: usize, message: []const u8 }{
        .{ .source = "<base><RPG></base>", .line = 1, .column = 14, .message = "expected </RPG>, found </base>" },
        .{ .source = "\xEF\xBB\xBF<base><RPG></base>", .line = 1, .column = 14, .message = "expected </RPG>, found </base>" },
        .{ .source = "<base>\n  <RPG a=\"1\" a=\"2\"/>\n</base>", .line = 2, .column = 14, .message = "attribute 'a' appears twice" },
        .{ .source = "<base><RPG Name=\"\xC3\x84\" Name=\"x\"/></base>", .line = 1, .column = 21, .message = "attribute 'Name' appears twice" },
        .{ .source = "<base>\r\n<RPG>&nbsp;</RPG></base>", .line = 2, .column = 6, .message = "unknown entity '&nbsp;'" },
        .{ .source = "<base><RPG>a & b</RPG></base>", .line = 1, .column = 14, .message = "'&' must start an entity such as &amp;" },
        .{ .source = "<base><RPG>&#0;</RPG></base>", .line = 1, .column = 12, .message = "invalid character reference '&#0;'" },
        .{ .source = "<!DOCTYPE base>\n<base/>", .line = 1, .column = 1, .message = "DOCTYPE declarations are not supported" },
        .{ .source = "<?xml version=\"1.0\" encoding=\"windows-1251\"?><base/>", .line = 1, .column = 31, .message = "encoding 'windows-1251' is not supported; save the file as UTF-8" },
        .{ .source = "<base>\n<RPG>\xE9t\xE9</RPG></base>", .line = 2, .column = 6, .message = "invalid UTF-8" },
        .{ .source = "\xFF\xFE<\x00", .line = 1, .column = 1, .message = "UTF-16 is not supported; save the file as UTF-8" },
        .{ .source = "<base><RPG>\x01</RPG></base>", .line = 1, .column = 12, .message = "control character 0x01 is not allowed in XML" },
        .{ .source = "<base><RPG a=\"<\"/></base>", .line = 1, .column = 15, .message = "'<' is not allowed in an attribute value" },
        .{ .source = "<base><RPG a=1/></base>", .line = 1, .column = 14, .message = "expected a quoted value for attribute 'a'" },
        .{ .source = "<base>\n  <RPG>\n", .line = 2, .column = 3, .message = "<RPG> is never closed" },
        .{ .source = "<base/>\n<more/>", .line = 2, .column = 1, .message = "content after the document element" },
        .{ .source = "<base><RPG><!-- a -- b --></RPG></base>", .line = 1, .column = 19, .message = "'--' is not allowed inside a comment" },
        .{ .source = "<base><RPG><!-- open </RPG></base>", .line = 1, .column = 12, .message = "unterminated comment" },
        .{ .source = "<base><RPG><![CDATA[x</RPG></base>", .line = 1, .column = 12, .message = "unterminated CDATA section" },
        .{ .source = "<base><RPG><?pi x?></RPG></base>", .line = 1, .column = 12, .message = "processing instructions are not supported" },
        .{ .source = "<base><RPG>a ]]> b</RPG></base>", .line = 1, .column = 14, .message = "']]>' is not allowed in text" },
        .{ .source = "\n<?xml version=\"1.0\"?><base/>", .line = 2, .column = 1, .message = "the XML declaration must be at the very start of the file" },
        .{ .source = "<base><RPG a=\"1\"b=\"2\"/></base>", .line = 1, .column = 17, .message = "expected whitespace, '>' or '/>' in <RPG>" },
        .{ .source = "   ", .line = 1, .column = 4, .message = "expected the document element" },
    };
    for (cases) |case| {
        var diag: xml.Diagnostic = .{};
        const result = xml.flatten(arena_state.allocator(), case.source, "RPG", &diag);
        std.testing.expectError(error.Malformed, result) catch |err| {
            std.debug.print("accepted: {s}\n", .{case.source});
            return err;
        };
        const at = xml.locate(case.source, diag.offset);
        std.testing.expectEqualStrings(case.message, diag.message()) catch |err| {
            std.debug.print("for: {s}\n", .{case.source});
            return err;
        };
        std.testing.expectEqual(Location{ .line = case.line, .column = case.column }, Location{ .line = at.line, .column = at.column }) catch |err| {
            std.debug.print("for: {s} ({s})\n", .{ case.source, case.message });
            return err;
        };
    }

    var diag: xml.Diagnostic = .{};
    try std.testing.expectError(error.RootNotFound, xml.flatten(arena_state.allocator(), "<base>\n<Other/>\n</base>", "RPG", &diag));
    try std.testing.expectEqualStrings("no <RPG> element under the document element", diag.message());
}

const Location = struct { line: usize, column: usize };
