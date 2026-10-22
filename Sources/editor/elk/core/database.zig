//! An open ELK database (D-05): the project file `elk.xml` and, per
//! element, the folder `<element>_data_base/` beside it holding a file group
//! per text. A text's key is its path below that folder without extension,
//! with forward slashes: `<key>.elk` is the original, `<key>.txt` the
//! translation, `<key>.xml` the state, `<key>.dsc` an optional description.
//! The texts are the `.elk` files, as the MFC tree was filled from them, so
//! a state file without an original (the tracked database has 3,777 under
//! `data/`) is no text and is left alone, like every other unknown file.
//!
//! Names resolve case-insensitively, so a database written on Windows opens
//! on a case-sensitive file system: the MFC code says `_DATA_BASE`, the
//! shipped folder is `game_data_base`. The `_DATA_BASE_RESERVE` folder the
//! MFC ELK declared but never used is not ported. Every write goes to a
//! temporary file that is then renamed over the target, so a failed write
//! leaves the previous file whole.
const std = @import("std");
const builtin = @import("builtin");
const text_codec = @import("text_codec.zig");
const state = @import("state.zig");
const project_file = @import("project_file.zig");

pub const State = state.State;
pub const TextProperty = state.TextProperty;

pub const Extension = enum {
    original,
    translation,
    state,
    description,

    pub fn suffix(self: Extension) []const u8 {
        return switch (self) {
            .original => ".elk",
            .translation => ".txt",
            .state => ".xml",
            .description => ".dsc",
        };
    }
};

/// What the MFC ELK appended to an element's path for its texts' folder.
pub const data_base_suffix = "_data_base";
const folder_description = "_folder.dsc";
pub const temp_suffix = ".~elk";

pub const Base = struct {
    /// The element's texts folder as found on disk, or null when there is
    /// none (the element then has no texts, as in the MFC tree).
    path: ?[]const u8,
    /// Sorted byte-wise.
    keys: []const []const u8,
};

pub const Database = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    /// Owns `folder`, `file_name` and every `Base`.
    arena: std.heap.ArenaAllocator,
    /// The folder holding `elk.xml`, as an OS path ("." for the current one).
    folder: []const u8,
    file_name: []const u8,
    project: project_file.Project,
    /// One per `project.elements` entry.
    bases: []Base,

    pub fn open(gpa: std.mem.Allocator, io: std.Io, elk_path: []const u8) !Database {
        const bytes = try std.Io.Dir.cwd().readFileAlloc(io, elk_path, gpa, .limited(16 << 20));
        defer gpa.free(bytes);
        var project = try project_file.parse(gpa, bytes);
        errdefer project.deinit();

        var arena = std.heap.ArenaAllocator.init(gpa);
        errdefer arena.deinit();
        const allocator = arena.allocator();
        const folder = try allocator.dupe(u8, std.fs.path.dirname(elk_path) orelse ".");
        const bases = try allocator.alloc(Base, project.elements.len);
        for (project.elements, bases) |element, *base| {
            const wanted = try std.mem.concat(allocator, u8, &.{ element.name, data_base_suffix });
            const path = try findEntry(allocator, io, folder, wanted, .directory);
            base.* = .{ .path = path, .keys = if (path) |found| try listKeys(allocator, io, found) else &.{} };
        }
        return .{
            .gpa = gpa,
            .io = io,
            .arena = arena,
            .folder = folder,
            .file_name = try allocator.dupe(u8, std.fs.path.basename(elk_path)),
            .project = project,
            .bases = bases,
        };
    }

    pub fn deinit(self: *Database) void {
        self.project.deinit();
        self.arena.deinit();
    }

    /// Writes `elk.xml` back (with the bare paths of D-05).
    pub fn save(self: *Database) !void {
        const bytes = try project_file.write(self.gpa, &self.project, self.file_name);
        defer self.gpa.free(bytes);
        const path = try std.fs.path.join(self.gpa, &.{ self.folder, self.file_name });
        defer self.gpa.free(path);
        try writeFileAtomic(self.io, self.gpa, path, bytes);
    }

    /// The original as the editor shows it (WTF-8, LF); "" when it is missing.
    pub fn readOriginal(self: *Database, gpa: std.mem.Allocator, element: usize, key: []const u8) ![]u8 {
        return self.readText(gpa, element, key, .original);
    }

    /// The translation as the editor shows it; "" when there is none yet.
    pub fn readTranslation(self: *Database, gpa: std.mem.Allocator, element: usize, key: []const u8) ![]u8 {
        return self.readText(gpa, element, key, .translation);
    }

    /// `CELK::GetDescription`: the text's own `<key>.dsc`, else the first
    /// `_folder.dsc` going up from the text's parent folder - the MFC lookup
    /// skips the text's own folder, and so does this - to the database
    /// folder itself, where this stops (the MFC one climbed on to the drive
    /// root). "" when there is none.
    pub fn readDescription(self: *Database, gpa: std.mem.Allocator, element: usize, key: []const u8) ![]u8 {
        const base = self.bases[element].path orelse return gpa.dupe(u8, "");
        const own = try std.mem.concat(gpa, u8, &.{ key, Extension.description.suffix() });
        defer gpa.free(own);
        if (try self.readResolved(gpa, base, own)) |bytes| {
            defer gpa.free(bytes);
            return text_codec.toEditor(gpa, bytes);
        }
        var folder = std.fs.path.dirnamePosix(key);
        while (true) {
            folder = if (folder) |inner| std.fs.path.dirnamePosix(inner) else break;
            const relative = if (folder) |inner| try std.mem.concat(gpa, u8, &.{ inner, "/", folder_description }) else try gpa.dupe(u8, folder_description);
            defer gpa.free(relative);
            if (try self.readResolved(gpa, base, relative)) |bytes| {
                defer gpa.free(bytes);
                return text_codec.toEditor(gpa, bytes);
            }
            if (folder == null) break;
        }
        return gpa.dupe(u8, "");
    }

    pub fn readState(self: *Database, element: usize, key: []const u8) !TextProperty {
        const base = self.bases[element].path orelse return .{};
        const relative = try std.mem.concat(self.gpa, u8, &.{ key, Extension.state.suffix() });
        defer self.gpa.free(relative);
        const bytes = try self.readResolved(self.gpa, base, relative) orelse return .{};
        defer self.gpa.free(bytes);
        return state.parse(bytes);
    }

    /// Writes the translation unless it is untouched: when `text` encodes to
    /// what the file already reads as, the file is left as it is, so its
    /// bytes stay identical even where an old writer left a trailing run.
    /// Returns whether it wrote.
    pub fn writeTranslation(self: *Database, element: usize, key: []const u8, text: []const u8) !bool {
        const bytes = try text_codec.fromEditor(self.gpa, text);
        defer self.gpa.free(bytes);
        const current = try self.readTranslation(self.gpa, element, key);
        defer self.gpa.free(current);
        const current_bytes = try text_codec.fromEditor(self.gpa, current);
        defer self.gpa.free(current_bytes);
        if (std.mem.eql(u8, bytes, current_bytes)) return false;
        const path = try self.targetPath(element, key, .translation);
        defer self.gpa.free(path);
        try writeFileAtomic(self.io, self.gpa, path, bytes);
        return true;
    }

    /// `CELK::SetState`: sets the state, marks the text Changed once a state
    /// but "Not translated" was set, and writes the state file only when the
    /// state changes. Returns the previous state.
    pub fn setState(self: *Database, element: usize, key: []const u8, new_state: State) !State {
        const previous = try self.readState(element, key);
        if (previous.state == new_state) return previous.state;
        var buffer: [state.max_len]u8 = undefined;
        const path = try self.targetPath(element, key, .state);
        defer self.gpa.free(path);
        try writeFileAtomic(self.io, self.gpa, path, state.format(&buffer, previous.withState(new_state)));
        return previous.state;
    }

    /// Writes the state file when it would change: the spreadsheet import's
    /// write, which sets the state and leaves `Changed` as it was, as
    /// `CELK::ImportFromXLS` did. Returns whether it wrote.
    pub fn writeProperty(self: *Database, element: usize, key: []const u8, property: TextProperty) !bool {
        if (std.meta.eql(try self.readState(element, key), property)) return false;
        var buffer: [state.max_len]u8 = undefined;
        const path = try self.targetPath(element, key, .state);
        defer self.gpa.free(path);
        try writeFileAtomic(self.io, self.gpa, path, state.format(&buffer, property));
        return true;
    }

    /// The file's bytes as they are on disk, for what copies them unchanged
    /// (the export); null when it is missing.
    pub fn readBytes(self: *Database, gpa: std.mem.Allocator, element: usize, key: []const u8, extension: Extension) !?[]u8 {
        const base = self.bases[element].path orelse return null;
        const relative = try std.mem.concat(gpa, u8, &.{ key, extension.suffix() });
        defer gpa.free(relative);
        return self.readResolved(gpa, base, relative);
    }

    fn readText(self: *Database, gpa: std.mem.Allocator, element: usize, key: []const u8, extension: Extension) ![]u8 {
        const base = self.bases[element].path orelse return gpa.dupe(u8, "");
        const relative = try std.mem.concat(gpa, u8, &.{ key, extension.suffix() });
        defer gpa.free(relative);
        const bytes = try self.readResolved(gpa, base, relative) orelse return gpa.dupe(u8, "");
        defer gpa.free(bytes);
        return text_codec.toEditor(gpa, bytes);
    }

    /// The bytes of `relative` (forward slashes) below `base`, its last
    /// component matched without regard to case; null when it is missing.
    fn readResolved(self: *Database, gpa: std.mem.Allocator, base: []const u8, relative: []const u8) !?[]u8 {
        const resolved = try resolve(gpa, self.io, base, relative);
        defer gpa.free(resolved.path);
        if (!resolved.exists) return null;
        return try std.Io.Dir.cwd().readFileAlloc(self.io, resolved.path, gpa, .limited(16 << 20));
    }

    /// Where a write of `key`'s file goes: the existing file and folders
    /// whatever their case, the key's own spelling for what is missing.
    fn targetPath(self: *Database, element: usize, key: []const u8, extension: Extension) ![]u8 {
        const base = self.bases[element].path orelse return error.NoDatabaseFolder;
        const relative = try std.mem.concat(self.gpa, u8, &.{ key, extension.suffix() });
        defer self.gpa.free(relative);
        return (try resolve(self.gpa, self.io, base, relative)).path;
    }
};

/// A key from a file name in an update or the game's data, as the MFC
/// import formed it: the extension dropped, ASCII lower case (D-05), and
/// forward slashes, the form keys take here.
pub fn importKey(gpa: std.mem.Allocator, name: []const u8) ![]u8 {
    const separator = std.mem.lastIndexOfAny(u8, name, "\\/");
    const dot = std.mem.lastIndexOfScalar(u8, name, '.');
    const end = if (dot) |at| (if (separator == null or at > separator.?) at else name.len) else name.len;
    const key = try std.ascii.allocLowerString(gpa, name[0..end]);
    std.mem.replaceScalar(u8, key, '\\', '/');
    return key;
}

/// Writes `bytes` to `path` through a temporary file beside it that is
/// renamed over the target, creating the parent folders it needs.
pub fn writeFileAtomic(io: std.Io, gpa: std.mem.Allocator, path: []const u8, bytes: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    if (std.fs.path.dirname(path)) |parent| try cwd.createDirPath(io, parent);
    const temp = try std.mem.concat(gpa, u8, &.{ path, temp_suffix });
    defer gpa.free(temp);
    try cwd.writeFile(io, .{ .sub_path = temp, .data = bytes });
    errdefer cwd.deleteFile(io, temp) catch {};
    try std.Io.Dir.rename(cwd, temp, cwd, path, io);
}

fn osPath(gpa: std.mem.Allocator, base: []const u8, relative: []const u8) ![]u8 {
    const path = try std.mem.concat(gpa, u8, &.{ base, std.fs.path.sep_str, relative });
    if (std.fs.path.sep != '/') std.mem.replaceScalar(u8, path, '/', std.fs.path.sep);
    return path;
}

pub const Resolved = struct {
    /// Owned by the caller.
    path: []u8,
    exists: bool,
};

/// The OS path of `relative` (forward slashes) below `base`, each component
/// matched without regard to case when its exact spelling is missing: the
/// folders, then the file, each spelled as on disk. From the first component
/// that is missing at all, the rest is taken as spelled and `exists` is false.
pub fn resolve(gpa: std.mem.Allocator, io: std.Io, base: []const u8, relative: []const u8) !Resolved {
    // On Linux a file found as spelled is spelled so on disk. Windows and
    // macOS find it in any case, which returned the caller's spelling (Data/
    // textes for a Data/Textes on disk, main run 39086936588), so there the
    // components are looked up below as when the exact spelling is missing.
    if (builtin.os.tag == .linux) {
        const exact = try osPath(gpa, base, relative);
        if (std.Io.Dir.cwd().statFile(io, exact, .{})) |stat| {
            if (stat.kind != .directory) return .{ .path = exact, .exists = true };
        } else |_| {}
        gpa.free(exact);
    }

    var path = try gpa.dupe(u8, base);
    errdefer gpa.free(path);
    var components = std.mem.splitScalar(u8, relative, '/');
    var exists = true;
    while (components.next()) |component| {
        const kind: std.Io.File.Kind = if (components.peek() == null) .file else .directory;
        const found = if (exists) try findEntry(gpa, io, path, component, kind) else null;
        const next = found orelse next: {
            exists = false;
            break :next try std.fs.path.join(gpa, &.{ path, component });
        };
        gpa.free(path);
        path = next;
    }
    return .{ .path = path, .exists = exists };
}

/// The path of the entry of `kind` named `name` (without regard to case) in
/// the folder `parent`, or null.
pub fn findEntry(gpa: std.mem.Allocator, io: std.Io, parent: []const u8, name: []const u8, kind: std.Io.File.Kind) !?[]u8 {
    var dir = std.Io.Dir.cwd().openDir(io, parent, .{ .iterate = true }) catch return null;
    defer dir.close(io);
    var it = dir.iterate();
    var found: ?[]u8 = null;
    while (try it.next(io)) |entry| {
        if (entry.kind != kind or !std.ascii.eqlIgnoreCase(entry.name, name)) continue;
        // The exact spelling wins over another case of it.
        if (found != null and !std.mem.eql(u8, entry.name, name)) continue;
        if (found) |previous| gpa.free(previous);
        found = try std.fs.path.join(gpa, &.{ parent, entry.name });
        if (std.mem.eql(u8, entry.name, name)) break;
    }
    return found;
}

/// Every `.elk` below `base`, as keys, sorted.
fn listKeys(arena: std.mem.Allocator, io: std.Io, base: []const u8) ![]const []const u8 {
    var dir = try std.Io.Dir.cwd().openDir(io, base, .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(arena);
    defer walker.deinit();
    var keys: std.ArrayListUnmanaged([]const u8) = .empty;
    const suffix = Extension.original.suffix();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        const path = entry.path;
        if (path.len <= suffix.len or !std.ascii.eqlIgnoreCase(path[path.len - suffix.len ..], suffix)) continue;
        const key = try arena.dupe(u8, path[0 .. path.len - suffix.len]);
        if (std.fs.path.sep != '/') std.mem.replaceScalar(u8, key, std.fs.path.sep, '/');
        try keys.append(arena, key);
    }
    std.mem.sort([]const u8, keys.items, {}, lessThan);
    return keys.items;
}

fn lessThan(_: void, a: []const u8, b: []const u8) bool {
    return std.mem.lessThan(u8, a, b);
}

const testing = std.testing;
/// Scratch for the core tests; the fixture helpers below are shared with
/// the package tests.
const scratch = "zig-out/local-test/elk/core";

pub fn copyTree(io: std.Io, from: []const u8, to: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    var dir = try cwd.openDir(io, from, .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(testing.allocator);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        const bytes = try dir.readFileAlloc(io, entry.path, testing.allocator, .limited(16 << 20));
        defer testing.allocator.free(bytes);
        const out = try std.fs.path.join(testing.allocator, &.{ to, entry.path });
        defer testing.allocator.free(out);
        if (std.fs.path.dirname(out)) |parent| try cwd.createDirPath(io, parent);
        try cwd.writeFile(io, .{ .sub_path = out, .data = bytes });
    }
}

/// A clean scratch folder for one test.
pub fn freshScratch(io: std.Io, name: []const u8) ![]u8 {
    const path = try std.fs.path.join(testing.allocator, &.{ scratch, name });
    std.Io.Dir.cwd().deleteTree(io, path) catch {};
    try std.Io.Dir.cwd().createDirPath(io, path);
    return path;
}

pub fn writeFixture(io: std.Io, root: []const u8, relative: []const u8, bytes: []const u8) !void {
    const path = try std.fs.path.join(testing.allocator, &.{ root, relative });
    defer testing.allocator.free(path);
    if (std.fs.path.dirname(path)) |parent| try std.Io.Dir.cwd().createDirPath(io, parent);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = bytes });
}

pub fn readFixture(io: std.Io, root: []const u8, relative: []const u8) ![]u8 {
    const path = try std.fs.path.join(testing.allocator, &.{ root, relative });
    defer testing.allocator.free(path);
    return std.Io.Dir.cwd().readFileAlloc(io, path, testing.allocator, .limited(1 << 20));
}

pub fn fixtureExists(io: std.Io, root: []const u8, relative: []const u8) bool {
    const path = std.fs.path.join(testing.allocator, &.{ root, relative }) catch return false;
    defer testing.allocator.free(path);
    _ = std.Io.Dir.cwd().statFile(io, path, .{}) catch return false;
    return true;
}

pub fn utf16(comptime text: []const u8) []const u8 {
    comptime var out: []const u8 = &text_codec.bom_le;
    inline for (text) |byte| out = out ++ [_]u8{ byte, 0 };
    return out;
}

test "an untouched Data/ELK copy saves back byte for byte" {
    // E-01's file half on the tracked database: open a copy, read and write
    // back every translation and state unchanged, save the project, and every
    // database file is still byte-identical to Data/ELK (elk.xml differs only
    // by its bare paths, which project_file's tests pin).
    const io = testing.io;
    const root = try freshScratch(io, "untouched");
    defer testing.allocator.free(root);
    const copy = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base" });
    defer testing.allocator.free(copy);
    try copyTree(io, "Data/ELK/game_data_base", copy);
    const elk_xml = try readFixture(io, "Data/ELK", "elk.xml");
    defer testing.allocator.free(elk_xml);
    try writeFixture(io, root, "elk.xml", elk_xml);

    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    try testing.expectEqual(@as(usize, 1), database.bases.len);
    // The 2,852 tracked originals, and nothing for the orphan state files.
    try testing.expectEqual(@as(usize, 3_072), database.bases[0].keys.len);

    var translated: usize = 0;
    for (database.bases[0].keys) |key| {
        const text = try database.readTranslation(testing.allocator, 0, key);
        defer testing.allocator.free(text);
        if (text.len != 0) translated += 1;
        try testing.expect(!try database.writeTranslation(0, key, text));
        const property = try database.readState(0, key);
        try testing.expectEqual(property.state, try database.setState(0, key, property.state));
    }
    try testing.expect(translated > 1_000);
    try database.save();

    var dir = try std.Io.Dir.cwd().openDir(io, "Data/ELK/game_data_base", .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(testing.allocator);
    defer walker.deinit();
    var compared: usize = 0;
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        const original = try dir.readFileAlloc(io, entry.path, testing.allocator, .limited(16 << 20));
        defer testing.allocator.free(original);
        const saved = try readFixture(io, copy, entry.path);
        defer testing.allocator.free(saved);
        try testing.expectEqualSlices(u8, original, saved);
        compared += 1;
    }
    try testing.expect(compared > 11_000);

    var reopened = try Database.open(testing.allocator, io, elk_path);
    defer reopened.deinit();
    try testing.expectEqualStrings("game", reopened.project.elements[0].name);
    try testing.expectEqual(@as(usize, 3_072), reopened.bases[0].keys.len);
}

test "an edited translation is written UTF-16LE with CRLF and reads back with LF" {
    const io = testing.io;
    const root = try freshScratch(io, "edited");
    defer testing.allocator.free(root);
    try writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item><Path>game</Path></item></Elements></base>\r\n");
    try writeFixture(io, root, "game_data_base/units/tank/name.elk", utf16("Tank\r\n"));
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();

    const original = try database.readOriginal(testing.allocator, 0, "units/tank/name");
    defer testing.allocator.free(original);
    try testing.expectEqualStrings("Tank", original);
    try testing.expect(!try database.writeTranslation(0, "units/tank/name", ""));
    try testing.expect(!fixtureExists(io, root, "game_data_base/units/tank/name.txt"));

    try testing.expect(try database.writeTranslation(0, "units/tank/name", "\u{0422}\u{0430}\u{043d}\u{043a}\nT-34\n"));
    const bytes = try readFixture(io, root, "game_data_base/units/tank/name.txt");
    defer testing.allocator.free(bytes);
    try testing.expectEqualSlices(u8, &[_]u8{ 0xFF, 0xFE, 0x22, 0x04, 0x30, 0x04, 0x3d, 0x04, 0x3a, 0x04, '\r', 0, '\n', 0, 'T', 0, '-', 0, '3', 0, '4', 0 }, bytes);
    const back = try database.readTranslation(testing.allocator, 0, "units/tank/name");
    defer testing.allocator.free(back);
    try testing.expectEqualStrings("\u{0422}\u{0430}\u{043d}\u{043a}\nT-34", back);
    // No temporary file is left behind.
    try testing.expect(!fixtureExists(io, root, "game_data_base/units/tank/name.txt" ++ temp_suffix));
}

test "a state write sets Changed and happens only when the state changes" {
    const io = testing.io;
    const root = try freshScratch(io, "state");
    defer testing.allocator.free(root);
    try writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item><Path>game</Path></item></Elements></base>\r\n");
    try writeFixture(io, root, "game_data_base/a.elk", utf16("A"));
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();

    // No state file is "Not translated", and setting that again writes nothing.
    try testing.expectEqual(State.not_translated, try database.setState(0, "a", .not_translated));
    try testing.expect(!fixtureExists(io, root, "game_data_base/a.xml"));

    try testing.expectEqual(State.not_translated, try database.setState(0, "a", .approved));
    const approved = try readFixture(io, root, "game_data_base/a.xml");
    defer testing.allocator.free(approved);
    try testing.expectEqualStrings("<?xml version=\"1.0\"?>\r\n<base State=\"3\" Changed=\"1\"/>\r\n", approved);

    try testing.expectEqual(State.approved, try database.setState(0, "a", .not_translated));
    try testing.expectEqual(TextProperty{ .state = .not_translated, .changed = true }, try database.readState(0, "a"));
}

test "folder names resolve without regard to case and the reserve folder is ignored" {
    const io = testing.io;
    const root = try freshScratch(io, "case");
    defer testing.allocator.free(root);
    // Path as the MFC ELK wrote it on Windows, folder in the code's own case.
    try writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item><Path>C:\\ELK\\Game</Path></item></Elements></base>\r\n");
    try writeFixture(io, root, "Game_DATA_BASE/Units/Name.ELK", utf16("Tank"));
    try writeFixture(io, root, "Game_DATA_BASE/Units/Name.TXT", utf16("Panzer"));
    try writeFixture(io, root, "Game_DATA_BASE_RESERVE/units/name.elk", utf16("Reserve"));
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();

    try testing.expectEqual(@as(usize, 1), database.bases[0].keys.len);
    const key = database.bases[0].keys[0];
    try testing.expectEqualStrings("Units/Name", key);
    // Siblings resolve whatever their case, for reads and for writes.
    const translation = try database.readTranslation(testing.allocator, 0, "Units/name");
    defer testing.allocator.free(translation);
    try testing.expectEqualStrings("Panzer", translation);
    try testing.expect(try database.writeTranslation(0, "units/name", "Tiger"));
    const written = try readFixture(io, root, "Game_DATA_BASE/Units/Name.TXT");
    defer testing.allocator.free(written);
    try testing.expectEqualSlices(u8, utf16("Tiger"), written);

    // Saved with the bare element name.
    try database.save();
    var reopened = try Database.open(testing.allocator, io, elk_path);
    defer reopened.deinit();
    try testing.expectEqualStrings("Game", reopened.project.elements[0].name);
}

test "a description is the text's own .dsc, else a _folder.dsc above its folder" {
    const io = testing.io;
    const root = try freshScratch(io, "description");
    defer testing.allocator.free(root);
    try writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item><Path>game</Path></item></Elements></base>\r\n");
    try writeFixture(io, root, "game_data_base/_folder.dsc", utf16("Root"));
    try writeFixture(io, root, "game_data_base/a/_FOLDER.DSC", utf16("Folder A\r\n"));
    // The MFC lookup never reads the text's own folder's _folder.dsc.
    try writeFixture(io, root, "game_data_base/a/b/_folder.dsc", utf16("Skipped"));
    try writeFixture(io, root, "game_data_base/a/b/own.dsc", utf16("Own"));
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();

    const cases = [_]struct { key: []const u8, description: []const u8 }{
        .{ .key = "a/b/own", .description = "Own" },
        .{ .key = "a/b/other", .description = "Folder A" },
        .{ .key = "a/other", .description = "Root" },
        .{ .key = "top", .description = "" },
    };
    for (cases) |case| {
        const description = try database.readDescription(testing.allocator, 0, case.key);
        defer testing.allocator.free(description);
        try testing.expectEqualStrings(case.description, description);
    }
}

test "an element without a texts folder opens with no texts" {
    const io = testing.io;
    const root = try freshScratch(io, "empty");
    defer testing.allocator.free(root);
    try writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item><Path>maps</Path></item></Elements></base>\r\n");
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    try testing.expectEqual(@as(?[]const u8, null), database.bases[0].path);
    try testing.expectEqual(@as(usize, 0), database.bases[0].keys.len);
    try testing.expectError(error.NoDatabaseFolder, database.writeTranslation(0, "x", "y"));
}

test "import keys are lower case, without extension, with forward slashes" {
    const cases = [_][2][]const u8{
        .{ "Units\\Technics\\Name.TXT", "units/technics/name" },
        .{ "scenarios/Chapters/description.txt", "scenarios/chapters/description" },
        .{ "folder.v2\\Name", "folder.v2/name" },
    };
    for (cases) |case| {
        const key = try importKey(testing.allocator, case[0]);
        defer testing.allocator.free(key);
        try testing.expectEqualStrings(case[1], key);
    }
}
