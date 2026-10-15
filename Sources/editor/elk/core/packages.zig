//! Imports into the text database (D-05): `CELK::UpdateELK` given one file
//! and `CELK::ImportFromPAK`, from Sources/src/ELK/ELK_StaticMethods.cpp. A
//! package is what an import reads: the `.txt` originals, `.dsc` descriptions
//! and `.description` element descriptions of a `.pak` or `.upd` zip (read
//! with the engine's own `zip.zig`, D-08), or of the game's loose data folder
//! for "Import from game", which in the MFC ELK zipped those same files into
//! a `.pak` that was then imported like any other.
//!
//! The rules, as the MFC code applied them:
//! - The file name picks the element and the kind: `<title>_<n>.upd` is a
//!   complete update of element `<title>`, `<title>_<n>.pak` a partial one,
//!   `<n>` becomes `LastUpdateNumber` (-1 without one). An element of that
//!   name is added, and `elk.xml` created, when there is none: that is how a
//!   new database came to be.
//! - An original that is new or whose bytes differ is copied. Its state
//!   becomes Outdated when it was Translated or Approved, otherwise Not
//!   translated - so an Outdated text whose original changes again drops to
//!   Not translated - and `Changed` is kept. An unchanged original leaves the
//!   text's files alone.
//! - `.dsc` and `.description` files are copied as they are, then the
//!   element's description is read back from `resource.description`,
//!   a field it lacks keeping the element's value.
//! - A complete update deletes the `.elk` of every text it does not carry and
//!   nothing else: the translation and the state file stay, as does every
//!   file that is no original (the tracked database's orphan state files).
//! - The element's Version becomes `<file>, [dd:mm:yyyy, hh.mm.ss]` from the
//!   package's modification time, seconds at the DOS time's two-second step.
//!
//! Changes for the portable stack: names are lower case on import (D-05) and
//! resolve against existing files without regard to case, as does the
//! element's name against the file title; a file whose bytes would not change
//! is not rewritten, so importing the same package twice writes nothing the
//! second time; the Version time is UTC, std having no local time zone; the
//! Version names the bare file (the MFC code cut the package path at the
//! element path's last backslash, the bare name when the package lies beside
//! `elk.xml` as updates do). A broken archive fails the read, before anything
//! is written. A game import's time is its newest file's, where the MFC one
//! took the time it zipped them, so it too repeats unchanged.
const std = @import("std");
const zip = @import("zip");
const state = @import("state.zig");
const project_file = @import("project_file.zig");
const database = @import("database.zig");

pub const Entry = struct {
    /// The path in the package, forward slashes, as spelled there.
    name: []const u8,
    bytes: []const u8,
};

pub const Package = struct {
    /// Owns every entry.
    arena: std.heap.ArenaAllocator,
    entries: []Entry = &.{},
    /// Seconds since the epoch: the archive's modification time, or the
    /// newest game file's.
    mtime: i64 = 0,

    pub fn deinit(self: *Package) void {
        self.arena.deinit();
    }
};

const Kind = enum {
    /// A text's original, `<key>.txt` in the package, `<key>.elk` here.
    original,
    /// A `.dsc` or `.description`, copied under its own name.
    copied,
};

fn kindOf(name: []const u8) ?Kind {
    if (std.ascii.endsWithIgnoreCase(name, ".txt")) return .original;
    if (std.ascii.endsWithIgnoreCase(name, ".dsc") or std.ascii.endsWithIgnoreCase(name, ".description")) return .copied;
    return null;
}

/// The importable entries of the `.pak` or `.upd` at `path`, every one
/// extracted and CRC-checked.
pub fn readArchive(gpa: std.mem.Allocator, io: std.Io, path: []const u8) !Package {
    var package: Package = .{ .arena = std.heap.ArenaAllocator.init(gpa) };
    errdefer package.deinit();
    const arena = package.arena.allocator();
    const cwd = std.Io.Dir.cwd();
    const bytes = try cwd.readFileAlloc(io, path, arena, .limited(256 << 20));
    package.mtime = (try cwd.statFile(io, path, .{})).mtime.toSeconds();
    var archive = try zip.Archive.parse(gpa, bytes);
    defer archive.deinit();
    var entries: std.ArrayListUnmanaged(Entry) = .empty;
    for (archive.entries) |*entry| {
        if (kindOf(entry.name) == null) continue;
        const name = try arena.dupe(u8, entry.name);
        std.mem.replaceScalar(u8, name, '\\', '/');
        try entries.append(arena, .{ .name = name, .bytes = try archive.extract(arena, entry) });
    }
    package.entries = entries.items;
    return package;
}

/// The importable files below the game's data folder, named relative to it
/// and sorted, so an import is the same whatever order the file system lists.
pub fn readGameFolder(gpa: std.mem.Allocator, io: std.Io, folder: []const u8) !Package {
    var package: Package = .{ .arena = std.heap.ArenaAllocator.init(gpa) };
    errdefer package.deinit();
    const arena = package.arena.allocator();
    var dir = try std.Io.Dir.cwd().openDir(io, folder, .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(gpa);
    defer walker.deinit();
    var entries: std.ArrayListUnmanaged(Entry) = .empty;
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file or kindOf(entry.path) == null) continue;
        const name = try arena.dupe(u8, entry.path);
        if (std.fs.path.sep != '/') std.mem.replaceScalar(u8, name, std.fs.path.sep, '/');
        const bytes = try dir.readFileAlloc(io, entry.path, arena, .limited(16 << 20));
        package.mtime = @max(package.mtime, (try dir.statFile(io, entry.path, .{})).mtime.toSeconds());
        try entries.append(arena, .{ .name = name, .bytes = bytes });
    }
    std.mem.sort(Entry, entries.items, {}, entryLessThan);
    package.entries = entries.items;
    return package;
}

fn entryLessThan(_: void, a: Entry, b: Entry) bool {
    return std.mem.lessThan(u8, a.name, b.name);
}

pub const UpdateName = struct {
    /// The element the package is for, a slice of the file name.
    title: []const u8,
    /// `LastUpdateNumber`; -1 when the name carries none.
    number: i32,
    /// A `.upd`: a complete update, which deletes the texts it lacks.
    complete: bool,
};

/// `UpdateELK`'s reading of a package's file name: `.upd` or `.pak` dropped
/// (any other name is kept whole and partial), then the title cut at its last
/// underscore and what follows read as by `sscanf( "%d" )` - the cut happens
/// even when no number follows, so `game_new.pak` is element `game`.
pub fn parseUpdateName(file_name: []const u8) UpdateName {
    var title = project_file.bareName(file_name);
    var complete = false;
    if (std.ascii.endsWithIgnoreCase(title, ".upd")) {
        title = title[0 .. title.len - ".upd".len];
        complete = true;
    } else if (std.ascii.endsWithIgnoreCase(title, ".pak")) {
        title = title[0 .. title.len - ".pak".len];
    }
    var number: i32 = -1;
    if (std.mem.lastIndexOfScalar(u8, title, '_')) |cut| {
        number = scanInt(title[cut + 1 ..]) orelse -1;
        title = title[0..cut];
    }
    return .{ .title = title, .number = number, .complete = complete };
}

/// `sscanf( "%d" )`: leading white space, a sign, then the digits up to the
/// first other character; null when there are none.
fn scanInt(text: []const u8) ?i32 {
    const rest = std.mem.trimStart(u8, text, " \t\r\n\x0b\x0c");
    var end: usize = 0;
    if (end < rest.len and (rest[end] == '+' or rest[end] == '-')) end += 1;
    const digits = end;
    while (end < rest.len and std.ascii.isDigit(rest[end])) end += 1;
    if (end == digits) return null;
    return std.fmt.parseInt(i32, rest[0..end], 10) catch null;
}

/// The Version an import records: the file's bare name and its time,
/// `<file>, [dd:mm:yyyy, hh.mm.ss]`, seconds rounded down to even as the DOS
/// time the MFC ELK formatted them from held them.
pub fn versionString(gpa: std.mem.Allocator, file_name: []const u8, seconds: i64) ![]u8 {
    const epoch: std.time.epoch.EpochSeconds = .{ .secs = @intCast(@max(seconds, 0)) };
    const year_day = epoch.getEpochDay().calculateYearDay();
    const month_day = year_day.calculateMonthDay();
    const day = epoch.getDaySeconds();
    return std.fmt.allocPrint(gpa, "{s}, [{d:0>2}:{d:0>2}:{d:0>4}, {d:0>2}.{d:0>2}.{d:0>2}]", .{
        project_file.bareName(file_name),
        @as(u32, month_day.day_index) + 1,
        month_day.month.numeric(),
        year_day.year,
        day.getHoursIntoDay(),
        day.getMinutesIntoHour(),
        day.getSecondsIntoMinute() / 2 * 2,
    });
}

/// What an import wrote, so a caller can report it and a test can see that a
/// repeated import writes nothing.
pub const Report = struct {
    /// Originals copied because they were new or had changed.
    originals: usize = 0,
    /// State files rewritten for them.
    states: usize = 0,
    /// `.dsc` and `.description` files whose bytes changed.
    descriptions: usize = 0,
    /// Originals a complete update deleted.
    deleted: usize = 0,
    /// Whether `elk.xml` changed (or was created).
    project: bool = false,

    pub fn changedNothing(self: Report) bool {
        return self.originals == 0 and self.states == 0 and self.descriptions == 0 and self.deleted == 0 and !self.project;
    }
};

/// `CELK::UpdateELK` with a file: imports `package`, read from the file
/// `file_name` (whose name decides element, number and kind), into the
/// database at `elk_path`, which is created when it does not exist. The
/// database is not held open: a caller with it open reopens it after, as
/// the MFC ELK closed and reopened it.
pub fn importPackage(gpa: std.mem.Allocator, io: std.Io, elk_path: []const u8, package: *const Package, file_name: []const u8) !Report {
    const cwd = std.Io.Dir.cwd();
    const previous: ?[]u8 = cwd.readFileAlloc(io, elk_path, gpa, .limited(16 << 20)) catch |err| switch (err) {
        error.FileNotFound => null,
        else => return err,
    };
    defer if (previous) |bytes| gpa.free(bytes);
    var project = if (previous) |bytes| try project_file.parse(gpa, bytes) else project_file.Project.empty(gpa);
    defer project.deinit();
    const arena = project.arena.allocator();

    const update = parseUpdateName(file_name);
    const index = for (project.elements, 0..) |element, at| {
        if (std.ascii.eqlIgnoreCase(element.name, update.title)) break at;
    } else added: {
        const grown = try arena.alloc(project_file.Element, project.elements.len + 1);
        @memcpy(grown[0..project.elements.len], project.elements);
        grown[project.elements.len] = .{ .name = try std.ascii.allocLowerString(arena, update.title) };
        project.elements = grown;
        break :added grown.len - 1;
    };
    const element = &project.elements[index];
    element.last_update_number = update.number;

    const folder = std.fs.path.dirname(elk_path) orelse ".";
    const wanted = try std.mem.concat(arena, u8, &.{ element.name, database.data_base_suffix });
    const base = try database.findEntry(arena, io, folder, wanted, .directory) orelse try std.fs.path.join(arena, &.{ folder, wanted });

    var report = try importElement(gpa, io, base, package, update.complete);
    element.version = try versionString(arena, file_name, package.mtime);
    const resource = try database.resolve(gpa, io, base, "resource.description");
    defer gpa.free(resource.path);
    if (resource.exists) {
        const bytes = try cwd.readFileAlloc(io, resource.path, arena, .limited(1 << 20));
        if (project_file.parseResourceDescription(arena, bytes, element.description)) |description| element.description = description;
    }

    const written = try project_file.write(gpa, &project, std.fs.path.basename(elk_path));
    defer gpa.free(written);
    report.project = try writeIfChanged(io, gpa, elk_path, previous, written);
    return report;
}

/// `CELK::ImportFromPAK` into the element's texts folder `base`.
fn importElement(gpa: std.mem.Allocator, io: std.Io, base: []const u8, package: *const Package, complete: bool) !Report {
    var report: Report = .{};
    // The lower-case keys the package carries, for a complete update.
    var carried: std.StringHashMapUnmanaged(void) = .empty;
    defer {
        var keys = carried.keyIterator();
        while (keys.next()) |key| gpa.free(key.*);
        carried.deinit(gpa);
    }
    for (package.entries) |entry| switch (kindOf(entry.name) orelse continue) {
        .original => {
            const key = try database.importKey(gpa, entry.name);
            const slot = try carried.getOrPut(gpa, key);
            if (slot.found_existing) gpa.free(key);
            try importOriginal(gpa, io, base, slot.key_ptr.*, entry.bytes, &report);
        },
        .copied => {
            const relative = try std.ascii.allocLowerString(gpa, entry.name);
            defer gpa.free(relative);
            std.mem.replaceScalar(u8, relative, '\\', '/');
            if (try copyIfChanged(gpa, io, base, relative, entry.bytes)) report.descriptions += 1;
        },
    };
    if (complete) report.deleted = try deleteUncarried(gpa, io, base, &carried);
    return report;
}

fn importOriginal(gpa: std.mem.Allocator, io: std.Io, base: []const u8, key: []const u8, bytes: []const u8, report: *Report) !void {
    const original = try resolveFile(gpa, io, base, key, ".elk");
    defer gpa.free(original.path);
    const current = try readIfExists(gpa, io, original);
    defer if (current) |old| gpa.free(old);
    if (current) |old| if (std.mem.eql(u8, old, bytes)) return;
    try database.writeFileAtomic(io, gpa, original.path, bytes);
    report.originals += 1;

    const state_file = try resolveFile(gpa, io, base, key, ".xml");
    defer gpa.free(state_file.path);
    const state_bytes = try readIfExists(gpa, io, state_file);
    defer if (state_bytes) |old| gpa.free(old);
    var property: state.TextProperty = if (state_bytes) |old| state.parse(old) else .{};
    const was_done = property.state == .translated or property.state == .approved;
    property.state = if (original.exists and was_done) .outdated else .not_translated;
    var buffer: [state.max_len]u8 = undefined;
    if (try writeIfChanged(io, gpa, state_file.path, state_bytes, state.format(&buffer, property))) report.states += 1;
}

/// `CImportFromPAKEraseFile`: deletes every `.elk` below `base` whose key
/// the update did not carry; returns how many.
fn deleteUncarried(gpa: std.mem.Allocator, io: std.Io, base: []const u8, carried: *const std.StringHashMapUnmanaged(void)) !usize {
    var dir = std.Io.Dir.cwd().openDir(io, base, .{ .iterate = true }) catch |err| switch (err) {
        error.FileNotFound => return 0,
        else => return err,
    };
    defer dir.close(io);
    var doomed: std.ArrayListUnmanaged([]u8) = .empty;
    defer {
        for (doomed.items) |path| gpa.free(path);
        doomed.deinit(gpa);
    }
    var walker = try dir.walk(gpa);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file or !std.ascii.endsWithIgnoreCase(entry.path, ".elk")) continue;
        const key = try std.ascii.allocLowerString(gpa, entry.path[0 .. entry.path.len - ".elk".len]);
        defer gpa.free(key);
        if (std.fs.path.sep != '/') std.mem.replaceScalar(u8, key, std.fs.path.sep, '/');
        if (!carried.contains(key)) try doomed.append(gpa, try gpa.dupe(u8, entry.path));
    }
    for (doomed.items) |path| try dir.deleteFile(io, path);
    return doomed.items.len;
}

fn resolveFile(gpa: std.mem.Allocator, io: std.Io, base: []const u8, key: []const u8, suffix: []const u8) !database.Resolved {
    const relative = try std.mem.concat(gpa, u8, &.{ key, suffix });
    defer gpa.free(relative);
    return database.resolve(gpa, io, base, relative);
}

fn readIfExists(gpa: std.mem.Allocator, io: std.Io, file: database.Resolved) !?[]u8 {
    if (!file.exists) return null;
    return try std.Io.Dir.cwd().readFileAlloc(io, file.path, gpa, .limited(16 << 20));
}

fn copyIfChanged(gpa: std.mem.Allocator, io: std.Io, base: []const u8, relative: []const u8, bytes: []const u8) !bool {
    const target = try database.resolve(gpa, io, base, relative);
    defer gpa.free(target.path);
    const current = try readIfExists(gpa, io, target);
    defer if (current) |old| gpa.free(old);
    return writeIfChanged(io, gpa, target.path, current, bytes);
}

fn writeIfChanged(io: std.Io, gpa: std.mem.Allocator, path: []const u8, current: ?[]const u8, bytes: []const u8) !bool {
    if (current) |old| if (std.mem.eql(u8, old, bytes)) return false;
    try database.writeFileAtomic(io, gpa, path, bytes);
    return true;
}

const testing = std.testing;
const utf16 = database.utf16;

/// Every file below `root` with a hash of its bytes, sorted by path: equal
/// snapshots mean an import wrote and deleted nothing.
const Snapshot = struct {
    arena: std.heap.ArenaAllocator,
    files: []const File,

    const File = struct { path: []const u8, hash: u64 };

    fn take(io: std.Io, root: []const u8) !Snapshot {
        var snapshot: Snapshot = .{ .arena = std.heap.ArenaAllocator.init(testing.allocator), .files = &.{} };
        errdefer snapshot.arena.deinit();
        const arena = snapshot.arena.allocator();
        var dir = try std.Io.Dir.cwd().openDir(io, root, .{ .iterate = true });
        defer dir.close(io);
        var walker = try dir.walk(testing.allocator);
        defer walker.deinit();
        var files: std.ArrayListUnmanaged(File) = .empty;
        while (try walker.next(io)) |entry| {
            if (entry.kind != .file) continue;
            const bytes = try dir.readFileAlloc(io, entry.path, testing.allocator, .limited(16 << 20));
            defer testing.allocator.free(bytes);
            try files.append(arena, .{ .path = try arena.dupe(u8, entry.path), .hash = std.hash.Wyhash.hash(0, bytes) });
        }
        std.mem.sort(File, files.items, {}, struct {
            fn lessThan(_: void, a: File, b: File) bool {
                return std.mem.lessThan(u8, a.path, b.path);
            }
        }.lessThan);
        snapshot.files = files.items;
        return snapshot;
    }

    fn expectEqual(self: Snapshot, other: Snapshot) !void {
        try testing.expectEqual(self.files.len, other.files.len);
        for (self.files, other.files) |a, b| {
            try testing.expectEqualStrings(a.path, b.path);
            try testing.expectEqual(a.hash, b.hash);
        }
    }

    fn deinit(self: *Snapshot) void {
        self.arena.deinit();
    }
};

fn expectFile(io: std.Io, root: []const u8, relative: []const u8, expected: []const u8) !void {
    const bytes = try database.readFixture(io, root, relative);
    defer testing.allocator.free(bytes);
    try testing.expectEqualSlices(u8, expected, bytes);
}

fn stateBytes(comptime value: u2, comptime changed: u1) []const u8 {
    return std.fmt.comptimePrint("<?xml version=\"1.0\"?>\r\n<base State=\"{d}\" Changed=\"{d}\"/>\r\n", .{ value, changed });
}

test "update file names pick the element, the number and the kind" {
    const cases = [_]struct { name: []const u8, title: []const u8, number: i32, complete: bool }{
        .{ .name = "game_22.upd", .title = "game", .number = 22, .complete = true },
        .{ .name = "C:\\ELK\\game_1.UPD", .title = "game", .number = 1, .complete = true },
        .{ .name = "/elk/game.upd", .title = "game", .number = -1, .complete = true },
        .{ .name = "texts.pak", .title = "texts", .number = -1, .complete = false },
        .{ .name = "usa01.pak", .title = "usa01", .number = -1, .complete = false },
        .{ .name = "game_data.pak", .title = "game", .number = -1, .complete = false },
        .{ .name = "game_7b.pak", .title = "game", .number = 7, .complete = false },
        .{ .name = "maps_v2_ -3.pak", .title = "maps_v2", .number = -3, .complete = false },
        .{ .name = "notes.zip", .title = "notes.zip", .number = -1, .complete = false },
    };
    for (cases) |case| {
        const update = parseUpdateName(case.name);
        try testing.expectEqualStrings(case.title, update.title);
        try testing.expectEqual(case.number, update.number);
        try testing.expectEqual(case.complete, update.complete);
    }
}

test "the version string is the bare file name and its time at two-second steps" {
    // 2026-06-15 18:15:27 UTC, the odd second of Data/ELK/elk.xml's Version.
    const version = try versionString(testing.allocator, "/x/Data/ELK/game_1.upd", 1_781_547_327);
    defer testing.allocator.free(version);
    try testing.expectEqualStrings("game_1.upd, [15:06:2026, 18.15.26]", version);
}

test "import rules: changed, new, unchanged and missing texts, partial then complete" {
    const io = testing.io;
    const root = try database.freshScratch(io, "import-rules");
    defer testing.allocator.free(root);
    const corrupted = "<?xml version=\"1.0\"?>\r\n<base><Elements/><Path>x.xml</Path></base>\r\n";
    try database.writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item LastUpdateNumber=\"1\"><Path>game</Path></item></Elements></base>\r\n");
    const fixtures = [_][2][]const u8{
        .{ "a.elk", utf16("A") },               .{ "a.xml", stateBytes(2, 1) },
        .{ "b.elk", utf16("B") },               .{ "b.xml", stateBytes(3, 1) },
        .{ "c.elk", utf16("C") },               .{ "c.xml", stateBytes(1, 1) },
        .{ "e.elk", utf16("E") },               .{ "e.txt", utf16("Ee") },
        .{ "e.xml", stateBytes(2, 1) },         .{ "orphan.xml", stateBytes(3, 1) },
        .{ "k.elk", utf16("K") },               .{ "k.xml", corrupted },
        .{ "Units/Tank/name.elk", utf16("N") },
    };
    for (fixtures) |fixture| {
        const relative = try std.mem.concat(testing.allocator, u8, &.{ "game_data_base/", fixture[0] });
        defer testing.allocator.free(relative);
        try database.writeFixture(io, root, relative, fixture[1]);
    }
    var entries = [_]Entry{
        .{ .name = "a.txt", .bytes = utf16("A") },
        .{ .name = "b.txt", .bytes = utf16("B2") },
        .{ .name = "c.txt", .bytes = utf16("C2") },
        .{ .name = "d.txt", .bytes = utf16("D") },
        .{ .name = "k.txt", .bytes = utf16("K") },
        .{ .name = "units/tank/NAME.TXT", .bytes = utf16("N2") },
        .{ .name = "New/Folder/X.TXT", .bytes = utf16("X") },
        .{ .name = "New/X.DSC", .bytes = utf16("About X") },
        .{ .name = "resource.description", .bytes = "<base Fonts=\"1\"><Name>Game</Name><PAK>texts.pak</PAK><UPD>game</UPD></base>" },
    };
    var package: Package = .{ .arena = std.heap.ArenaAllocator.init(testing.allocator), .entries = &entries, .mtime = 1_781_547_327 };
    defer package.deinit();
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    const db = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base" });
    defer testing.allocator.free(db);

    const partial = try importPackage(testing.allocator, io, elk_path, &package, "game_2.pak");
    try testing.expectEqual(Report{ .originals = 5, .states = 5, .descriptions = 2, .project = true }, partial);
    // Unchanged: untouched. Translated or Approved and changed: Outdated.
    // Outdated and changed again: Not translated. Changed is kept.
    try expectFile(io, db, "a.xml", stateBytes(2, 1));
    try expectFile(io, db, "b.elk", utf16("B2"));
    try expectFile(io, db, "b.xml", stateBytes(1, 1));
    try expectFile(io, db, "c.xml", stateBytes(0, 1));
    try expectFile(io, db, "d.elk", utf16("D"));
    try expectFile(io, db, "d.xml", stateBytes(0, 0));
    // A partial update leaves a text it lacks, and orphan and corrupted
    // state files survive.
    try expectFile(io, db, "e.elk", utf16("E"));
    try expectFile(io, db, "orphan.xml", stateBytes(3, 1));
    try expectFile(io, db, "k.xml", corrupted);
    // Names are lower case, existing folders found whatever their case.
    try expectFile(io, db, "Units/Tank/name.elk", utf16("N2"));
    try expectFile(io, db, "Units/Tank/name.xml", stateBytes(0, 0));
    try testing.expect(!database.fixtureExists(io, db, "units"));
    try expectFile(io, db, "new/folder/x.elk", utf16("X"));
    try expectFile(io, db, "new/x.dsc", utf16("About X"));

    {
        var opened = try database.Database.open(testing.allocator, io, elk_path);
        defer opened.deinit();
        const element = opened.project.elements[0];
        try testing.expectEqual(@as(i32, 2), element.last_update_number);
        try testing.expectEqualStrings("game_2.pak, [15:06:2026, 18.15.26]", element.version);
        try testing.expectEqualStrings("Game", element.description.name);
        try testing.expectEqualStrings("game", element.description.upd);
        const description = try opened.readDescription(testing.allocator, 0, "new/x");
        defer testing.allocator.free(description);
        try testing.expectEqualStrings("About X", description);
    }

    // The same package again writes nothing.
    var before = try Snapshot.take(io, root);
    defer before.deinit();
    try testing.expect((try importPackage(testing.allocator, io, elk_path, &package, "game_2.pak")).changedNothing());
    var after = try Snapshot.take(io, root);
    defer after.deinit();
    try before.expectEqual(after);

    // As a complete update it deletes only the original of the text it lacks.
    const complete = try importPackage(testing.allocator, io, elk_path, &package, "game_3.upd");
    try testing.expectEqual(Report{ .deleted = 1, .project = true }, complete);
    try testing.expect(!database.fixtureExists(io, db, "e.elk"));
    try expectFile(io, db, "e.txt", utf16("Ee"));
    try expectFile(io, db, "e.xml", stateBytes(2, 1));
    try expectFile(io, db, "orphan.xml", stateBytes(3, 1));
    var opened = try database.Database.open(testing.allocator, io, elk_path);
    defer opened.deinit();
    const keys = [_][]const u8{ "Units/Tank/name", "a", "b", "c", "d", "k", "new/folder/x" };
    try testing.expectEqual(keys.len, opened.bases[0].keys.len);
    for (keys, opened.bases[0].keys) |expected, key| try testing.expectEqualStrings(expected, key);
}

test "importing game_1.upd into a copy of Data/ELK, which it built, changes no database file" {
    // Data/ELK's 2,852 originals are game_1.upd's, byte for byte, and its
    // Version names it: a full open after re-importing it finds every file
    // as tracked - originals, translations, states (the four corrupted ones
    // too), descriptions and the 3,777 orphan state files under data/.
    const io = testing.io;
    const root = try database.freshScratch(io, "reimport");
    defer testing.allocator.free(root);
    const copy = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base" });
    defer testing.allocator.free(copy);
    try database.copyTree(io, "Data/ELK/game_data_base", copy);
    const tracked_xml = try database.readFixture(io, "Data/ELK", "elk.xml");
    defer testing.allocator.free(tracked_xml);
    try database.writeFixture(io, root, "elk.xml", tracked_xml);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);

    var package = try readArchive(testing.allocator, io, "Data/ELK/game_1.upd");
    defer package.deinit();
    try testing.expectEqual(@as(usize, 2_853), package.entries.len);
    const report = try importPackage(testing.allocator, io, elk_path, &package, "Data/ELK/game_1.upd");
    try testing.expectEqual(@as(usize, 0), report.originals + report.states + report.descriptions + report.deleted);

    var tracked = try Snapshot.take(io, "Data/ELK/game_data_base");
    defer tracked.deinit();
    var imported = try Snapshot.take(io, copy);
    defer imported.deinit();
    try tracked.expectEqual(imported);

    // elk.xml is the 07-01 writer's form of the tracked one with only the
    // Version renewed from the package's time.
    var opened = try database.Database.open(testing.allocator, io, elk_path);
    defer opened.deinit();
    try testing.expectEqual(@as(usize, 2_852), opened.bases[0].keys.len);
    var expected = try project_file.parse(testing.allocator, tracked_xml);
    defer expected.deinit();
    try testing.expect(std.mem.startsWith(u8, opened.project.elements[0].version, "game_1.upd, ["));
    expected.elements[0].version = opened.project.elements[0].version;
    const expected_bytes = try project_file.write(testing.allocator, &expected, "elk.xml");
    defer testing.allocator.free(expected_bytes);
    try expectFile(io, root, "elk.xml", expected_bytes);

    // And once more, nothing at all changes.
    var before = try Snapshot.take(io, root);
    defer before.deinit();
    try testing.expect((try importPackage(testing.allocator, io, elk_path, &package, "Data/ELK/game_1.upd")).changedNothing());
    var after = try Snapshot.take(io, root);
    defer after.deinit();
    try before.expectEqual(after);
}

test "a new database from the game's language folder: lower-case keys, imported once" {
    // The repository's game data carries one language, Data/Textes, and the
    // game's element description Data/resource.description; a copy of both
    // is the game folder.
    const io = testing.io;
    const root = try database.freshScratch(io, "from-game");
    defer testing.allocator.free(root);
    const game = try std.fs.path.join(testing.allocator, &.{ root, "game" });
    defer testing.allocator.free(game);
    const textes = try std.fs.path.join(testing.allocator, &.{ game, "Textes" });
    defer testing.allocator.free(textes);
    try database.copyTree(io, "Data/Textes", textes);
    const resource = try database.readFixture(io, "Data", "resource.description");
    defer testing.allocator.free(resource);
    try database.writeFixture(io, game, "resource.description", resource);

    var package = try readGameFolder(testing.allocator, io, game);
    defer package.deinit();
    var originals: usize = 0;
    for (package.entries) |entry| {
        if (kindOf(entry.name).? == .original) originals += 1;
    }
    try testing.expectEqual(@as(usize, 1_706), originals);

    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk", "elk.xml" });
    defer testing.allocator.free(elk_path);
    const report = try importPackage(testing.allocator, io, elk_path, &package, "game.upd");
    try testing.expectEqual(Report{ .originals = 1_706, .states = 1_706, .descriptions = 1, .project = true }, report);

    var opened = try database.Database.open(testing.allocator, io, elk_path);
    defer opened.deinit();
    try testing.expectEqual(@as(usize, 1), opened.project.elements.len);
    const element = opened.project.elements[0];
    try testing.expectEqualStrings("game", element.name);
    try testing.expectEqual(@as(i32, -1), element.last_update_number);
    try testing.expectEqualStrings("Game Resources", element.description.name);
    try testing.expect(element.description.fonts);
    try testing.expectEqual(@as(usize, 1_706), opened.bases[0].keys.len);
    const base = opened.bases[0].path.?;
    for (opened.bases[0].keys) |key| {
        for (key) |byte| try testing.expect(!std.ascii.isUpper(byte));
    }
    // Every original is the game file's bytes under its lower-case key.
    for (package.entries) |entry| {
        if (kindOf(entry.name).? != .original) continue;
        const key = try database.importKey(testing.allocator, entry.name);
        defer testing.allocator.free(key);
        const relative = try std.mem.concat(testing.allocator, u8, &.{ key, ".elk" });
        defer testing.allocator.free(relative);
        try expectFile(io, base, relative, entry.bytes);
    }
    try expectFile(io, base, "resource.description", resource);

    var before = try Snapshot.take(io, root);
    defer before.deinit();
    try testing.expect((try importPackage(testing.allocator, io, elk_path, &package, "game.upd")).changedNothing());
    var after = try Snapshot.take(io, root);
    defer after.deinit();
    try before.expectEqual(after);
}

test "a tracked .pak reads its texts, and a broken archive fails before any write" {
    const io = testing.io;
    var package = try readArchive(testing.allocator, io, "Data/AmericanELK/usa01.pak");
    defer package.deinit();
    try testing.expectEqual(@as(usize, 113), package.entries.len);
    for (package.entries) |entry| try testing.expectEqual(Kind.original, kindOf(entry.name).?);

    const root = try database.freshScratch(io, "broken-pak");
    defer testing.allocator.free(root);
    try database.writeFixture(io, root, "game_2.pak", "PK not a zip");
    const path = try std.fs.path.join(testing.allocator, &.{ root, "game_2.pak" });
    defer testing.allocator.free(path);
    try testing.expectError(error.InvalidArchive, readArchive(testing.allocator, io, path));
}
