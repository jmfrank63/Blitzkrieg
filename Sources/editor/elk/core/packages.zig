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
//!
//! The export, `CELK::ExportToPAK`, goes the other way: one `<key>.txt` per
//! text, written by `zip_writer.zig` (D-08), to a mod or into the game
//! (D-09). Its rules and changes are at `exportElement` and `exportTo`.
const std = @import("std");
const zip = @import("zip");
const zip_writer = @import("zip_writer.zig");
const state = @import("state.zig");
const project_file = @import("project_file.zig");
const database = @import("database.zig");
const filter_mod = @import("filter.zig");
const Progress = @import("progress.zig").Progress;

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
/// the MFC ELK closed and reopened it. `progress` counts one unit per
/// entry (D-14); a cancelled import keeps the entries before it, each file
/// whole, deletes nothing and leaves `elk.xml` as it was.
pub fn importPackage(gpa: std.mem.Allocator, io: std.Io, elk_path: []const u8, package: *const Package, file_name: []const u8, progress: ?*Progress) !Report {
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

    var report = try importElement(gpa, io, base, package, update.complete, progress);
    element.version = try versionString(arena, file_name, package.mtime);
    const resource = try database.resolve(gpa, io, base, "resource.description");
    defer gpa.free(resource.path);
    if (resource.exists) {
        const bytes = try cwd.readFileAlloc(io, resource.path, arena, .limited(1 << 20));
        if (project_file.parseResourceDescription(arena, bytes, element.description)) |description| element.description = description;
    }

    const written = try project_file.write(gpa, &project, std.fs.path.basename(elk_path));
    defer gpa.free(written);
    try Progress.check(progress);
    report.project = try writeIfChanged(io, gpa, elk_path, previous, written);
    return report;
}

/// What `applyUpdates` found and did.
pub const Applied = struct {
    /// `.upd` files beside the database.
    found: usize = 0,
    /// The ones imported: the newest of each element, when newer than its
    /// `LastUpdateNumber`.
    imported: usize = 0,
};

/// `CELK::UpdateELK` without a file, as the short layout ran it at start:
/// of the `.upd` files beside `elk_path`, the one with the highest number
/// per element (`game_22.upd` before `game_3.upd` and `game.upd`, whose -1
/// loses to any number), imported when its element is new or recorded an
/// older update. The database is created when it does not exist. The MFC
/// ELK also dropped the elements no `.upd` named from `elk.xml`; this keeps
/// them, since dropping one loses its translations from the project.
pub fn applyUpdates(gpa: std.mem.Allocator, io: std.Io, elk_path: []const u8) !Applied {
    const folder_path = std.fs.path.dirname(elk_path) orelse ".";
    var arena_state = std.heap.ArenaAllocator.init(gpa);
    defer arena_state.deinit();
    const arena = arena_state.allocator();

    const Newest = struct { name: []const u8, update: UpdateName };
    var newest: std.ArrayList(Newest) = .empty;
    var applied: Applied = .{};
    {
        var folder = try std.Io.Dir.cwd().openDir(io, folder_path, .{ .iterate = true });
        defer folder.close(io);
        var entries = folder.iterate();
        while (try entries.next(io)) |entry| {
            if (entry.kind != .file or !std.ascii.endsWithIgnoreCase(entry.name, ".upd")) continue;
            applied.found += 1;
            const name = try arena.dupe(u8, entry.name);
            const update = parseUpdateName(name);
            for (newest.items) |*kept| {
                if (!std.ascii.eqlIgnoreCase(kept.update.title, update.title)) continue;
                if (kept.update.number < update.number) kept.* = .{ .name = name, .update = update };
                break;
            } else try newest.append(arena, .{ .name = name, .update = update });
        }
    }
    // The file system's order is not the same everywhere; the elements are
    // added to a new database in their names' order.
    std.mem.sort(Newest, newest.items, {}, struct {
        fn lessThan(_: void, a: Newest, b: Newest) bool {
            return std.ascii.lessThanIgnoreCase(a.update.title, b.update.title);
        }
    }.lessThan);

    for (newest.items) |file| {
        const recorded: ?i32 = recorded: {
            const bytes = std.Io.Dir.cwd().readFileAlloc(io, elk_path, arena, .limited(16 << 20)) catch |err| switch (err) {
                error.FileNotFound => break :recorded null,
                else => return err,
            };
            var project = try project_file.parse(gpa, bytes);
            defer project.deinit();
            for (project.elements) |element| {
                if (std.ascii.eqlIgnoreCase(element.name, file.update.title)) break :recorded element.last_update_number;
            }
            break :recorded null;
        };
        if (recorded) |number| if (number >= file.update.number) continue;
        const path = try std.fs.path.join(arena, &.{ folder_path, file.name });
        var package = try readArchive(gpa, io, path);
        defer package.deinit();
        _ = try importPackage(gpa, io, elk_path, &package, path, null);
        applied.imported += 1;
    }
    return applied;
}

/// `CELK::ImportFromPAK` into the element's texts folder `base`.
fn importElement(gpa: std.mem.Allocator, io: std.Io, base: []const u8, package: *const Package, complete: bool, progress: ?*Progress) !Report {
    var report: Report = .{};
    // The lower-case keys the package carries, for a complete update.
    var carried: std.StringHashMapUnmanaged(void) = .empty;
    defer {
        var keys = carried.keyIterator();
        while (keys.next()) |key| gpa.free(key.*);
        carried.deinit(gpa);
    }
    Progress.start(progress, package.entries.len);
    for (package.entries) |entry| {
        try Progress.advance(progress, io);
        try importEntry(gpa, io, base, entry, &carried, &report);
    }
    try Progress.check(progress);
    if (complete) report.deleted = try deleteUncarried(gpa, io, base, &carried);
    return report;
}

fn importEntry(gpa: std.mem.Allocator, io: std.Io, base: []const u8, entry: Entry, carried: *std.StringHashMapUnmanaged(void), report: *Report) !void {
    switch (kindOf(entry.name) orelse return) {
        .original => {
            const key = try database.importKey(gpa, entry.name);
            const slot = try carried.getOrPut(gpa, key);
            if (slot.found_existing) gpa.free(key);
            try importOriginal(gpa, io, base, slot.key_ptr.*, entry.bytes, report);
        },
        .copied => {
            const relative = try std.ascii.allocLowerString(gpa, entry.name);
            defer gpa.free(relative);
            std.mem.replaceScalar(u8, relative, '\\', '/');
            if (try copyIfChanged(gpa, io, base, relative, entry.bytes)) report.descriptions += 1;
        },
    }
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

pub const ExportOptions = struct {
    /// `bOnlyFilled`: a translation no longer than its byte-order mark counts
    /// as none, and the original goes out instead. Every MFC caller passed
    /// true.
    only_filled: bool = true,
    /// Generated fonts, named below `fonts/` (`medium/1.tfd`, `medium/1_c.dds`
    /// and so on), for an element whose `Fonts` flag is set.
    fonts: []const Entry = &.{},
    /// "Export to PAK by filter" (D6): only the texts the filter admits
    /// (`SSimpleFilter::Check` on each key and its state file) go into the
    /// pak; the MFC export wrote no file at all for the others.
    filter: ?filter_mod.Filter = null,
    /// D-14: one unit per text; a cancelled export writes nothing.
    progress: ?*Progress = null,
};

/// A pak and how many entries it holds.
pub const Pak = struct { bytes: []u8, entries: usize };

/// `CELK::ExportToPAK` for one element: the `.pak` bytes, which the caller
/// owns. Each text is `<key>.txt` holding, unchanged, its translation when
/// its state is not Not translated and the translation file exists (and, with
/// `only_filled`, has more than its byte-order mark), otherwise its original.
/// The MFC export copied the files the same way into a folder that zip.exe
/// then packed; the entries here are sorted and at a fixed time
/// (`zip_writer.zig`), so the same database always exports to the same
/// bytes, and exporting what an import of an export left changes nothing.
pub fn exportElement(gpa: std.mem.Allocator, db: *database.Database, element: usize, options: ExportOptions) ![]u8 {
    return (try buildPak(gpa, db, element, options)).bytes;
}

fn buildPak(gpa: std.mem.Allocator, db: *database.Database, element: usize, options: ExportOptions) !Pak {
    var arena = std.heap.ArenaAllocator.init(gpa);
    defer arena.deinit();
    const allocator = arena.allocator();
    var files: std.ArrayListUnmanaged(zip_writer.File) = .empty;
    const keys = db.bases[element].keys;
    Progress.start(options.progress, keys.len);
    for (keys) |key| {
        try Progress.advance(options.progress, db.io);
        const property = try db.readState(element, key);
        if (options.filter) |filter| if (!filter.matches(key, property)) continue;
        const translation = if (property.state != .not_translated) try db.readBytes(allocator, element, key, .translation) else null;
        const filled = if (translation) |bytes| !options.only_filled or bytes.len > 2 else false;
        const bytes = if (filled) translation.? else try db.readBytes(allocator, element, key, .original) orelse "";
        try files.append(allocator, .{ .name = try std.mem.concat(allocator, u8, &.{ key, ".txt" }), .bytes = bytes });
    }
    for (options.fonts) |font| {
        try files.append(allocator, .{ .name = try std.mem.concat(allocator, u8, &.{ "fonts/", font.name }), .bytes = font.bytes });
    }
    try Progress.check(options.progress);
    return .{ .bytes = try zip_writer.write(gpa, files.items), .entries = files.items.len };
}

/// Where an export goes below the game folder (D-09).
pub const Destination = union(enum) {
    /// The default: `mods/<name>/data/texts.pak`. `-mod=<name>` mounts the
    /// mod ahead of the loose files and the archives, so nothing shadows it.
    mod: []const u8,
    /// "Install to game": `Data/Texts.pak`, where the MFC ELK wrote it. A
    /// loose `Data/Textes` text of the same key shadows it.
    game,
};

pub const DestinationError = error{
    /// Empty, `.` or `..`, or with a path separator: not one folder below
    /// `mods`.
    InvalidModName,
};

/// The `.pak` path of `destination` below `game_folder`, its existing
/// folders and file found whatever their case; the caller owns it.
pub fn destinationPath(gpa: std.mem.Allocator, io: std.Io, game_folder: []const u8, destination: Destination) ![]u8 {
    const relative = switch (destination) {
        .mod => |name| mod: {
            if (name.len == 0 or std.mem.eql(u8, name, ".") or std.mem.eql(u8, name, "..") or std.mem.indexOfAny(u8, name, "/\\") != null)
                return DestinationError.InvalidModName;
            break :mod try std.mem.concat(gpa, u8, &.{ "mods/", name, "/data/texts.pak" });
        },
        .game => try gpa.dupe(u8, "Data/Texts.pak"),
    };
    defer gpa.free(relative);
    return (try database.resolve(gpa, io, game_folder, relative)).path;
}

/// Exports `element` to `destination` below `game_folder` and returns the
/// path written, which the caller owns, and the entries the pak holds. A
/// cancelled export (`options.progress`) writes nothing. A mod without a `data/mod.xml` gets
/// one naming it, version 1.0, in the form the Resource Editor writes:
/// the game mounts no mod folder without it. An existing one is left alone.
pub fn exportTo(gpa: std.mem.Allocator, io: std.Io, db: *database.Database, element: usize, game_folder: []const u8, destination: Destination, options: ExportOptions) !Written {
    const path = try destinationPath(gpa, io, game_folder, destination);
    errdefer gpa.free(path);
    const pak = try buildPak(gpa, db, element, options);
    defer gpa.free(pak.bytes);
    try database.writeFileAtomic(io, gpa, path, pak.bytes);
    switch (destination) {
        .mod => |name| {
            const mod_xml = try database.resolve(gpa, io, std.fs.path.dirname(path).?, "mod.xml");
            defer gpa.free(mod_xml.path);
            if (!mod_xml.exists) {
                var out: std.ArrayListUnmanaged(u8) = .empty;
                defer out.deinit(gpa);
                try out.appendSlice(gpa, "<?xml version=\"1.0\"?>\r\n<base>");
                try project_file.textElement(gpa, &out, "MODName", name);
                try out.appendSlice(gpa, "<MODVersion>1.0</MODVersion></base>\r\n");
                try database.writeFileAtomic(io, gpa, mod_xml.path, out.items);
            }
        },
        .game => {},
    }
    return .{ .path = path, .entries = pak.entries };
}

/// `exportTo`'s result: the pak's path, which the caller owns, and its
/// entries, one per text exported and one per font file.
pub const Written = struct { path: []u8, entries: usize };

/// What D-09's warning for "Install to game" names: the game mounts loose
/// files before its archives, so a loose `<game>/Data/<key>.txt` hides that
/// text in `Data/Texts.pak`.
pub const Shadow = struct {
    /// The element's texts a loose file exists for.
    count: usize = 0,
    /// The loose folder below `Data` the first of them is in (`Data/Textes`
    /// in the repository), as spelled on disk; the caller owns it. Null when
    /// `count` is 0.
    folder: ?[]u8 = null,

    pub fn deinit(self: *Shadow, gpa: std.mem.Allocator) void {
        if (self.folder) |folder| gpa.free(folder);
        self.folder = null;
    }
};

/// The texts of `element` a loose file below `<game_folder>/Data` shadows,
/// each file found whatever its case, as the game finds it.
pub fn shadowedTexts(gpa: std.mem.Allocator, io: std.Io, db: *const database.Database, element: usize, game_folder: []const u8) !Shadow {
    var shadow: Shadow = .{};
    errdefer shadow.deinit(gpa);
    for (db.bases[element].keys) |key| {
        const relative = try std.mem.concat(gpa, u8, &.{ "Data/", key, ".txt" });
        defer gpa.free(relative);
        const loose = try database.resolve(gpa, io, game_folder, relative);
        defer gpa.free(loose.path);
        if (!loose.exists) continue;
        shadow.count += 1;
        if (shadow.folder != null) continue;
        // Up from the file to the key's first folder, spelled as on disk.
        var folder: []const u8 = loose.path;
        for (0..std.mem.count(u8, key, "/")) |_| folder = std.fs.path.dirname(folder).?;
        shadow.folder = try gpa.dupe(u8, folder);
    }
    return shadow;
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

    const partial = try importPackage(testing.allocator, io, elk_path, &package, "game_2.pak", null);
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
    try testing.expect((try importPackage(testing.allocator, io, elk_path, &package, "game_2.pak", null)).changedNothing());
    var after = try Snapshot.take(io, root);
    defer after.deinit();
    try before.expectEqual(after);

    // As a complete update it deletes only the original of the text it lacks.
    const complete = try importPackage(testing.allocator, io, elk_path, &package, "game_3.upd", null);
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
    const report = try importPackage(testing.allocator, io, elk_path, &package, "Data/ELK/game_1.upd", null);
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
    try testing.expect((try importPackage(testing.allocator, io, elk_path, &package, "Data/ELK/game_1.upd", null)).changedNothing());
    var after = try Snapshot.take(io, root);
    defer after.deinit();
    try before.expectEqual(after);
}

test "applying the updates beside a database imports only the newest, and only once" {
    // Data/ELK records game_1.upd. Beside a copy lie it, the same bytes as
    // game_2.upd and as the unnumbered game.upd: game_2 is the newest.
    const io = testing.io;
    const root = try database.freshScratch(io, "apply-updates");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    try testing.expectEqual(Applied{}, try applyUpdates(testing.allocator, io, elk_path));

    const copy = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base" });
    defer testing.allocator.free(copy);
    try database.copyTree(io, "Data/ELK/game_data_base", copy);
    const tracked_xml = try database.readFixture(io, "Data/ELK", "elk.xml");
    defer testing.allocator.free(tracked_xml);
    try database.writeFixture(io, root, "elk.xml", tracked_xml);
    const upd = try std.Io.Dir.cwd().readFileAlloc(io, "Data/ELK/game_1.upd", testing.allocator, .limited(16 << 20));
    defer testing.allocator.free(upd);
    for ([_][]const u8{ "game_1.upd", "game_2.upd", "game.upd" }) |name| try database.writeFixture(io, root, name, upd);

    try testing.expectEqual(Applied{ .found = 3, .imported = 1 }, try applyUpdates(testing.allocator, io, elk_path));
    var opened = try database.Database.open(testing.allocator, io, elk_path);
    defer opened.deinit();
    try testing.expectEqual(@as(i32, 2), opened.project.elements[0].last_update_number);
    try testing.expect(std.mem.startsWith(u8, opened.project.elements[0].version, "game_2.upd, ["));
    try testing.expectEqual(@as(usize, 2_852), opened.bases[0].keys.len);

    try testing.expectEqual(Applied{ .found = 3, .imported = 0 }, try applyUpdates(testing.allocator, io, elk_path));
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
    try testing.expectEqual(@as(usize, 1_710), originals);

    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk", "elk.xml" });
    defer testing.allocator.free(elk_path);
    const report = try importPackage(testing.allocator, io, elk_path, &package, "game.upd", null);
    try testing.expectEqual(Report{ .originals = 1_710, .states = 1_710, .descriptions = 1, .project = true }, report);

    var opened = try database.Database.open(testing.allocator, io, elk_path);
    defer opened.deinit();
    try testing.expectEqual(@as(usize, 1), opened.project.elements.len);
    const element = opened.project.elements[0];
    try testing.expectEqualStrings("game", element.name);
    try testing.expectEqual(@as(i32, -1), element.last_update_number);
    try testing.expectEqualStrings("Game Resources", element.description.name);
    try testing.expect(element.description.fonts);
    try testing.expectEqual(@as(usize, 1_710), opened.bases[0].keys.len);
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
    try testing.expect((try importPackage(testing.allocator, io, elk_path, &package, "game.upd", null)).changedNothing());
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

/// The entries of `bytes` through the engine's reader, each extracted (so
/// its CRC checked), as name and bytes in archive order.
fn readBack(arena: std.mem.Allocator, bytes: []const u8) ![]Entry {
    var archive = try zip.Archive.parse(testing.allocator, bytes);
    defer archive.deinit();
    const entries = try arena.alloc(Entry, archive.entries.len);
    for (archive.entries, entries) |*entry, *out| {
        try testing.expectEqual(zip_writer.dos_modified, entry.dos_modified);
        out.* = .{ .name = try arena.dupe(u8, entry.name), .bytes = try archive.extract(arena, entry) };
    }
    return entries;
}

test "a cancelled import keeps the entries before it, deletes nothing and writes no elk.xml" {
    // usa01.pak into a new database, held before its 50th entry on another
    // thread and cancelled (D-14), then imported whole.
    const io = testing.io;
    const root = try database.freshScratch(io, "cancelled-import");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var package = try readArchive(testing.allocator, io, "Data/AmericanELK/usa01.pak");
    defer package.deinit();
    const Worker = struct {
        fn run(path: []const u8, imported: *const Package, progress: *Progress, result: *anyerror!Report) void {
            result.* = importPackage(testing.allocator, testing.io, path, imported, "usa01.pak", progress);
        }
    };
    var progress: Progress = .{};
    progress.hold.store(49, .release);
    var result: anyerror!Report = error.NotFinished;
    const thread = try std.Thread.spawn(.{}, Worker.run, .{ elk_path, &package, &progress, &result });
    while (!progress.held.load(.acquire)) try io.sleep(.fromMilliseconds(1), .awake);
    try testing.expectEqual(@as(usize, 113), progress.range.load(.acquire));
    progress.cancel();
    thread.join();
    try testing.expectError(error.Cancelled, result);
    try testing.expectError(error.FileNotFound, std.Io.Dir.cwd().access(io, elk_path, .{}));
    var cancelled = try Snapshot.take(io, root);
    defer cancelled.deinit();
    var originals: usize = 0;
    for (cancelled.files) |file| {
        try testing.expect(!std.mem.endsWith(u8, file.path, database.temp_suffix));
        if (std.mem.endsWith(u8, file.path, ".elk")) originals += 1;
    }
    try testing.expectEqual(@as(usize, 49), originals);

    const report = try importPackage(testing.allocator, io, elk_path, &package, "usa01.pak", null);
    try testing.expectEqual(Report{ .originals = 113 - 49, .states = 113 - 49, .project = true }, report);
}

test "an export carries the translation or the original as the MFC export chose" {
    const io = testing.io;
    const root = try database.freshScratch(io, "export-rules");
    defer testing.allocator.free(root);
    try database.writeFixture(io, root, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item LastUpdateNumber=\"1\"><Path>game</Path></item></Elements></base>\r\n");
    const fixtures = [_][2][]const u8{
        // Translated: the translation.
        .{ "a.elk", utf16("A") },               .{ "a.txt", utf16("A-tr") },        .{ "a.xml", stateBytes(2, 1) },
        // Not translated: the original, whatever translation there is.
        .{ "b.elk", utf16("B") },               .{ "b.txt", utf16("B-tr") },        .{ "b.xml", stateBytes(0, 1) },
        // Outdated with an empty translation: the original when only filled.
        .{ "c.elk", utf16("C") },               .{ "c.txt", utf16("") },            .{ "c.xml", stateBytes(1, 1) },
        // Approved without a translation file: the original.
        .{ "d.elk", utf16("D") },               .{ "d.xml", stateBytes(3, 1) },
        // No state file reads as Not translated.
        .{ "e.elk", utf16("E") },               .{ "e.txt", utf16("E-tr") },
        // The key's own spelling names the entry.
        .{ "Units/Tank/name.elk", utf16("N") }, .{ "Units/Tank/name.txt", utf16("N-tr") }, .{ "Units/Tank/name.xml", stateBytes(3, 1) },
    };
    for (fixtures) |fixture| {
        const relative = try std.mem.concat(testing.allocator, u8, &.{ "game_data_base/", fixture[0] });
        defer testing.allocator.free(relative);
        try database.writeFixture(io, root, relative, fixture[1]);
    }
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var db = try database.Database.open(testing.allocator, io, elk_path);
    defer db.deinit();

    const fonts = [_]Entry{ .{ .name = "medium/1.tfd", .bytes = "tfd" }, .{ .name = "medium/1_c.dds", .bytes = "DDS " } };
    const bytes = try exportElement(testing.allocator, &db, 0, .{ .fonts = &fonts });
    defer testing.allocator.free(bytes);
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const expected = [_]Entry{
        .{ .name = "Units/Tank/name.txt", .bytes = utf16("N-tr") },
        .{ .name = "a.txt", .bytes = utf16("A-tr") },
        .{ .name = "b.txt", .bytes = utf16("B") },
        .{ .name = "c.txt", .bytes = utf16("C") },
        .{ .name = "d.txt", .bytes = utf16("D") },
        .{ .name = "e.txt", .bytes = utf16("E") },
        .{ .name = "fonts/medium/1.tfd", .bytes = "tfd" },
        .{ .name = "fonts/medium/1_c.dds", .bytes = "DDS " },
    };
    const entries = try readBack(arena.allocator(), bytes);
    try testing.expectEqual(expected.len, entries.len);
    for (expected, entries) |want, got| {
        try testing.expectEqualStrings(want.name, got.name);
        try testing.expectEqualSlices(u8, want.bytes, got.bytes);
    }

    // Without only-filled, an empty translation goes out as it is.
    const unfilled = try exportElement(testing.allocator, &db, 0, .{ .only_filled = false });
    defer testing.allocator.free(unfilled);
    var archive = try zip.Archive.parse(testing.allocator, unfilled);
    defer archive.deinit();
    const c = try archive.extract(arena.allocator(), archive.find("c.txt").?);
    try testing.expectEqualSlices(u8, utf16(""), c);

    // By filter (D6): only the texts it admits, the others left out, the
    // fonts kept. Translated and Approved are a, d and Units/Tank/name; the
    // condition `tank` keeps the one whose folder holds it.
    const cases = [_]struct { conditions: []const u8, names: []const []const u8 }{
        .{ .conditions = "", .names = &.{ "Units/Tank/name.txt", "a.txt", "d.txt", "fonts/medium/1.tfd", "fonts/medium/1_c.dds" } },
        .{ .conditions = "tank", .names = &.{ "Units/Tank/name.txt", "fonts/medium/1.tfd", "fonts/medium/1_c.dds" } },
    };
    for (cases) |case| {
        const filter: filter_mod.Filter = .{ .name = "Done", .states = .{ false, false, true, true }, .conditions = case.conditions };
        const pak = try buildPak(testing.allocator, &db, 0, .{ .fonts = &fonts, .filter = filter });
        defer testing.allocator.free(pak.bytes);
        const filtered = try readBack(arena.allocator(), pak.bytes);
        try testing.expectEqual(case.names.len, pak.entries);
        try testing.expectEqual(case.names.len, filtered.len);
        for (case.names, filtered) |want, got| try testing.expectEqualStrings(want, got.name);
    }
}

test "an export goes to a mod, with a mod.xml when it has none, or into the game's Data" {
    const io = testing.io;
    const root = try database.freshScratch(io, "export-destinations");
    defer testing.allocator.free(root);
    try database.writeFixture(io, root, "elk/elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item LastUpdateNumber=\"1\"><Path>game</Path></item></Elements></base>\r\n");
    try database.writeFixture(io, root, "elk/game_data_base/a.elk", utf16("A"));
    // The game folder's own spelling of an existing Data/Texts.pak is kept.
    try database.writeFixture(io, root, "game/data/TEXTS.pak", "old");
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk", "elk.xml" });
    defer testing.allocator.free(elk_path);
    const game = try std.fs.path.join(testing.allocator, &.{ root, "game" });
    defer testing.allocator.free(game);
    var db = try database.Database.open(testing.allocator, io, elk_path);
    defer db.deinit();
    const bytes = try exportElement(testing.allocator, &db, 0, .{});
    defer testing.allocator.free(bytes);

    const mod_written = try exportTo(testing.allocator, io, &db, 0, game, .{ .mod = "ELK Test" }, .{});
    const mod_path = mod_written.path;
    defer testing.allocator.free(mod_path);
    try testing.expectEqual(@as(usize, 1), mod_written.entries);
    const want_mod = try std.fs.path.join(testing.allocator, &.{ game, "mods", "ELK Test", "data", "texts.pak" });
    defer testing.allocator.free(want_mod);
    try testing.expectEqualStrings(want_mod, mod_path);
    try expectFile(io, game, "mods/ELK Test/data/texts.pak", bytes);
    const mod_xml = "<?xml version=\"1.0\"?>\r\n<base><MODName>ELK Test</MODName><MODVersion>1.0</MODVersion></base>\r\n";
    try expectFile(io, game, "mods/ELK Test/data/mod.xml", mod_xml);
    // The engine's own XML reader finds the name the game shows.
    {
        const xml = @import("xml");
        var document = try xml.parse(testing.allocator, mod_xml);
        defer document.deinit();
        try testing.expectEqualStrings("ELK Test", xml.child(document.root, "MODName").?.text);
    }
    // A mod's own mod.xml stays.
    try database.writeFixture(io, game, "mods/ELK Test/data/mod.xml", "mine");
    testing.allocator.free((try exportTo(testing.allocator, io, &db, 0, game, .{ .mod = "ELK Test" }, .{})).path);
    try expectFile(io, game, "mods/ELK Test/data/mod.xml", "mine");

    // A cancelled export (D-14) leaves the pak there as it was.
    var progress: Progress = .{};
    progress.cancel();
    try testing.expectError(error.Cancelled, exportTo(testing.allocator, io, &db, 0, game, .game, .{ .progress = &progress }));
    try expectFile(io, game, "data/TEXTS.pak", "old");

    const game_path = (try exportTo(testing.allocator, io, &db, 0, game, .game, .{})).path;
    defer testing.allocator.free(game_path);
    const want_game = try std.fs.path.join(testing.allocator, &.{ game, "data", "TEXTS.pak" });
    defer testing.allocator.free(want_game);
    try testing.expectEqualStrings(want_game, game_path);
    try expectFile(io, game, "data/TEXTS.pak", bytes);

    for ([_][]const u8{ "", ".", "..", "a/b", "a\\b" }) |name| {
        try testing.expectError(DestinationError.InvalidModName, destinationPath(testing.allocator, io, game, .{ .mod = name }));
    }
}

test "a loose Data/Textes shadows the texts it holds, found whatever their case" {
    const io = testing.io;
    const root = try database.freshScratch(io, "export-shadow");
    defer testing.allocator.free(root);
    try database.writeFixture(io, root, "elk/elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item LastUpdateNumber=\"1\"><Path>game</Path></item></Elements></base>\r\n");
    for ([_][]const u8{ "textes/strings/a", "textes/strings/b", "textes/c" }) |key| {
        const relative = try std.mem.concat(testing.allocator, u8, &.{ "elk/game_data_base/", key, ".elk" });
        defer testing.allocator.free(relative);
        try database.writeFixture(io, root, relative, utf16("A"));
    }
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk", "elk.xml" });
    defer testing.allocator.free(elk_path);
    const game = try std.fs.path.join(testing.allocator, &.{ root, "game" });
    defer testing.allocator.free(game);
    var db = try database.Database.open(testing.allocator, io, elk_path);
    defer db.deinit();
    const pak = try buildPak(testing.allocator, &db, 0, .{ .fonts = &.{ .{ .name = "a", .bytes = "" }, .{ .name = "b", .bytes = "" } } });
    defer testing.allocator.free(pak.bytes);
    try testing.expectEqual(@as(usize, 5), pak.entries);

    var none = try shadowedTexts(testing.allocator, io, &db, 0, game);
    defer none.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 0), none.count);
    try testing.expectEqual(@as(?[]u8, null), none.folder);

    try database.writeFixture(io, game, "Data/Textes/Strings/A.txt", "loose");
    try database.writeFixture(io, game, "Data/Textes/c.txt", "loose");
    try database.writeFixture(io, game, "Data/Textes/Strings/other.txt", "not a text of the database");
    var shadow = try shadowedTexts(testing.allocator, io, &db, 0, game);
    defer shadow.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 2), shadow.count);
    const want = try std.fs.path.join(testing.allocator, &.{ game, "Data", "Textes" });
    defer testing.allocator.free(want);
    try testing.expectEqualStrings(want, shadow.folder.?);
}

test "the tracked Data/ELK exports to a pak the engine and unzip read, the same every time, also after an import of it" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    // Read only: the export writes nothing into the database.
    var tracked = try database.Database.open(testing.allocator, io, "Data/ELK/elk.xml");
    defer tracked.deinit();
    const first = try exportElement(testing.allocator, &tracked, 0, .{});
    defer testing.allocator.free(first);
    const entries = try readBack(allocator, first);
    try testing.expectEqual(@as(usize, 2_852), entries.len);
    // Sorted by entry name, which is not quite the key order: '-' sorts
    // before the '.' of ".txt".
    var by_name: std.StringHashMapUnmanaged([]const u8) = .empty;
    for (entries, 0..) |entry, at| {
        if (at > 0) try testing.expect(std.mem.lessThan(u8, entries[at - 1].name, entry.name));
        try by_name.put(allocator, entry.name, entry.bytes);
    }
    for (tracked.bases[0].keys) |key| {
        const exported = by_name.get(try std.mem.concat(allocator, u8, &.{ key, ".txt" })).?;
        const property = try tracked.readState(0, key);
        const translation = try tracked.readBytes(allocator, 0, key, .translation);
        const original = (try tracked.readBytes(allocator, 0, key, .original)).?;
        const wanted = if (property.state != .not_translated and translation != null and translation.?.len > 2) translation.? else original;
        try testing.expectEqualSlices(u8, wanted, exported);
    }

    // unzip agrees, when the machine has it.
    const root = try database.freshScratch(io, "export-tracked");
    defer testing.allocator.free(root);
    try database.writeFixture(io, root, "texts.pak", first);
    const pak_path = try std.fs.path.join(allocator, &.{ root, "texts.pak" });
    if (std.process.run(testing.allocator, io, .{ .argv = &.{ "unzip", "-tq", pak_path } })) |result| {
        defer testing.allocator.free(result.stdout);
        defer testing.allocator.free(result.stderr);
        if (!result.term.success()) std.debug.print("unzip -t: {s}{s}\n", .{ result.stdout, result.stderr });
        try testing.expect(result.term.success());
    } else |err| switch (err) {
        error.FileNotFound => std.debug.print("unzip is not installed; only zip.zig checked the export\n", .{}),
        else => return err,
    }

    // E-02: entry by entry against Data/ELK/texts.pak, the MFC ELK's export
    // of 2002-12-03, which predates the database (built from game_1.upd):
    // 2,213 of the 2,712 texts both carry are byte-identical; the other 499
    // were since re-imported (the original changed, or the state dropped to
    // Not translated so the original now goes out instead of the
    // translation); 227 texts (movies/, ...) left the game since and 140
    // (medals/, ...) joined it. No .txt there has a name the export spells
    // differently: both are lower case.
    const shipped = try std.Io.Dir.cwd().readFileAlloc(io, "Data/ELK/texts.pak", allocator, .limited(16 << 20));
    var reference = try zip.Archive.parse(testing.allocator, shipped);
    defer reference.deinit();
    var equal: usize = 0;
    var different: usize = 0;
    for (entries) |entry| {
        const old = reference.find(entry.name) orelse continue;
        try testing.expectEqualStrings(old.name, entry.name);
        const old_bytes = try reference.extract(allocator, old);
        if (std.mem.eql(u8, old_bytes, entry.bytes)) equal += 1 else different += 1;
    }
    try testing.expectEqual(@as(usize, 2_939), reference.entries.len);
    try testing.expectEqual(@as(usize, 2_213), equal);
    try testing.expectEqual(@as(usize, 499), different);
    try testing.expectEqual(@as(usize, 140), entries.len - equal - different);

    // Two exports of the same database are byte-identical.
    const second = try exportElement(testing.allocator, &tracked, 0, .{});
    defer testing.allocator.free(second);
    try testing.expectEqualSlices(u8, first, second);

    // Export, import back, export again: the same bytes. The import takes
    // the element from the file name, so the export is imported as the
    // element's update `game_23.pak`; it turns exported translations into
    // originals (Translated and Approved become Outdated, Outdated Not
    // translated), and each such text still exports the same bytes.
    const copy = try std.fs.path.join(allocator, &.{ root, "elk", "game_data_base" });
    try database.copyTree(io, "Data/ELK/game_data_base", copy);
    const tracked_xml = try database.readFixture(io, "Data/ELK", "elk.xml");
    defer testing.allocator.free(tracked_xml);
    try database.writeFixture(io, root, "elk/elk.xml", tracked_xml);
    try database.writeFixture(io, root, "game_23.pak", first);
    const elk_path = try std.fs.path.join(allocator, &.{ root, "elk", "elk.xml" });
    var package = try readArchive(testing.allocator, io, try std.fs.path.join(allocator, &.{ root, "game_23.pak" }));
    defer package.deinit();
    const report = try importPackage(testing.allocator, io, elk_path, &package, "game_23.pak", null);
    try testing.expect(report.originals > 0);
    var imported = try database.Database.open(testing.allocator, io, elk_path);
    defer imported.deinit();
    const again = try exportElement(testing.allocator, &imported, 0, .{});
    defer testing.allocator.free(again);
    try testing.expectEqualSlices(u8, first, again);
}
