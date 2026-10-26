//! What the translator edits, without a window: the open database, its
//! text tree (the core's, with each text's and folder's state), the selected
//! text's original and translation buffers, its state, and the save points
//! of D-15 (leaving a text, Save, Close, Exit, and every 30 seconds while
//! the edit is dirty). The UI draws this and the scripted runs drive it, so
//! both go through the same commit. Browsing (First, Previous, Next, Last)
//! asks the core's tree for the text a step reaches through the current
//! filter and selects it, which is a save point like any other selection.
//! Find Next asks the core's search for the next hit from where the last
//! one stopped and selects its text the same way; the statistics are the
//! core's, counted over the tree. The pak export is the core's too (D-08,
//! D-09): to a language mod, `mods/<Name>/data/texts.pak`, or into the game,
//! `Data/Texts.pak`, after the shadow check that names the loose folder
//! hiding it; Run Game's command line is the kit's test launch in ELK's own
//! profile. The spreadsheet export and import are the core's (D-10), as are
//! the imports from the game's language folder and from a `.pak` or `.upd`
//! (D-05); an import reopens the database so the tree shows the states it
//! set.
//!
//! The long operations (D-14: the pak and spreadsheet exports, the
//! spreadsheet and package imports, Find Next) are three steps so the UI can run the
//! middle one on a worker thread: `begin` on the UI thread saves the edit
//! and fixes what the work reads (the element, where the search starts);
//! `perform` is the work, which touches nothing of the session but the
//! database's files and reads the tree, both left alone while it runs; and
//! `finish`, on the UI thread again, reports a failure, reopens the
//! database after an import (a cancelled one too, its first rows being in)
//! and selects a hit. The synchronous commands are the three in a row.
const std = @import("std");
const elk = @import("elk_core");
const testlaunch = @import("editor_kit").testlaunch;
const l10n = @import("editor_kit").l10n;

const Database = elk.database.Database;
pub const Tree = elk.tree.Tree;
pub const Text = elk.tree.Text;
pub const State = elk.state.State;
pub const Filter = elk.filter.Filter;
pub const Step = elk.tree.Step;
pub const FindOptions = elk.search.Options;
pub const Hit = elk.search.Hit;
pub const Statistic = elk.stats.Statistic;

/// D-15: a dirty edit is written this long after it became dirty.
pub const autosave_ms: u64 = 30_000;

/// D-09: Run Game's profile, never the player's. The kit's `-editor-test`
/// keeps it session-only, leaves `profiles/active.cfg` alone and cloud sync
/// off; E-06 ran the real Game in the same profile.
pub const game_profile = "ELKTest";

pub const Destination = elk.packages.Destination;
pub const Shadow = elk.packages.Shadow;
pub const Progress = elk.progress.Progress;

/// What an export wrote: the `.pak`, owned by the caller, and its entries.
pub const Exported = elk.packages.Written;

/// A long operation's input. Its slices are borrowed; the worker's copy
/// owns them (jobs.zig).
pub const Work = union(enum) {
    /// File > Export to language mod... and Install to game..., by the
    /// filter when one is given (D6). `begin` sets the element.
    export_pak: struct { game_folder: []const u8, destination: Destination, filter: ?Filter = null, element: usize = 0 },
    /// File > Export to spreadsheet... and Import from spreadsheet...: the
    /// sheet's path.
    export_sheet: []const u8,
    import_sheet: []const u8,
    /// File > Import from Game...: the game's language folder, imported as
    /// the package `<element>.pak`, a partial update of the element the
    /// exports write (`game` in a database that has none yet), so nothing
    /// is deleted. `begin` sets the element.
    import_game: struct { folder: []const u8, element: []const u8 = "" },
    /// File > Import from PAK/UPD...: the file, whose name picks the
    /// element and the kind.
    import_pak: []const u8,
    /// Edit > Find...'s Find Next. `begin` sets where it starts.
    find: struct { options: FindOptions, from: ?elk.search.Cursor = null },
};

pub const Outcome = union(enum) {
    exported: Exported,
    sheet_rows: usize,
    /// The caller deinits the report.
    imported: elk.sheet.Report,
    /// What a package import wrote.
    updated: elk.packages.Report,
    found: ?Hit,
};

/// The mod a name or a folder means: a name is taken as typed, a folder must
/// be one directly below `<game_folder>/mods` (whatever its case and
/// separators), and its name is the mod's. Null for any other folder or an
/// empty name; the core refuses names it cannot use (`InvalidModName`).
pub fn modName(game_folder: []const u8, typed: []const u8) ?[]const u8 {
    const trimmed = std.mem.trim(u8, typed, " \t");
    const text = std.mem.trimEnd(u8, trimmed, "/\\");
    if (text.len == 0) return null;
    // Either separator on every OS: a folder typed the Windows way splits too.
    const cut = std.mem.lastIndexOfAny(u8, text, "/\\") orelse return text;
    const mods = std.mem.trimEnd(u8, text[0..cut], "/\\");
    const at = std.mem.lastIndexOfAny(u8, mods, "/\\") orelse return null;
    if (!std.ascii.eqlIgnoreCase(mods[at + 1 ..], "mods") or !samePath(mods[0..at], game_folder)) return null;
    return text[cut + 1 ..];
}

/// Two folders compared as the game finds files: without regard to case,
/// either separator, a trailing one ignored.
fn samePath(a: []const u8, b: []const u8) bool {
    const x = std.mem.trimEnd(u8, a, "/\\");
    const y = std.mem.trimEnd(u8, b, "/\\");
    if (x.len != y.len) return false;
    for (x, y) |p, q| {
        const p_sep = p == '/' or p == '\\';
        const q_sep = q == '/' or q == '\\';
        if (p_sep != q_sep) return false;
        if (!p_sep and std.ascii.toLower(p) != std.ascii.toLower(q)) return false;
    }
    return true;
}

/// Run Game (D-09): `Game -editor-test -profile=ELKTest -mod=<mod> -windowed`,
/// no map, so the game opens at its main menu in the exported language.
pub fn runGameOptions(game_path: []const u8, mod: []const u8, log_path: []const u8) testlaunch.Options {
    return .{
        .game_path = game_path,
        .mod_folder = mod,
        .log_path = log_path,
        .profile = game_profile,
        .map_name = null,
    };
}

/// A NUL-terminated byte buffer ImGui edits in place and grows through its
/// resize callback. `bytes.items` always ends in the terminator.
pub const TextBuffer = struct {
    bytes: std.ArrayListUnmanaged(u8) = .empty,

    pub fn deinit(self: *TextBuffer, gpa: std.mem.Allocator) void {
        self.bytes.deinit(gpa);
    }

    pub fn set(self: *TextBuffer, gpa: std.mem.Allocator, value: []const u8) !void {
        self.bytes.clearRetainingCapacity();
        try self.bytes.ensureTotalCapacity(gpa, value.len + 1);
        self.bytes.appendSliceAssumeCapacity(value);
        self.bytes.appendAssumeCapacity(0);
    }

    pub fn text(self: *const TextBuffer) []const u8 {
        if (self.bytes.items.len == 0) return "";
        return std.mem.sliceTo(self.bytes.items, 0);
    }

    /// The whole allocation, for ImGui's `buf` and `buf_size`.
    pub fn storage(self: *TextBuffer, gpa: std.mem.Allocator) ![]u8 {
        if (self.bytes.items.len == 0) try self.set(gpa, "");
        return self.bytes.allocatedSlice();
    }

    /// ImGui asks for `size` bytes (terminator included) holding `len` bytes
    /// of text; returns the new storage, or null when it could not grow.
    pub fn grow(self: *TextBuffer, gpa: std.mem.Allocator, size: usize, len: usize) ?[]u8 {
        self.bytes.ensureTotalCapacity(gpa, size) catch return null;
        self.bytes.items.len = @min(len + 1, self.bytes.capacity);
        return self.bytes.allocatedSlice();
    }

    /// After ImGui wrote into `storage`: the text ends at the first NUL.
    pub fn settle(self: *TextBuffer) void {
        const all = self.bytes.allocatedSlice();
        const len = std.mem.indexOfScalar(u8, all, 0) orelse all.len - 1;
        all[len] = 0;
        self.bytes.items.len = len + 1;
    }
};

pub const Session = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    database: ?Database = null,
    /// Owns the copy of the opened path (`path`).
    path_storage: std.ArrayListUnmanaged(u8) = .empty,
    /// The open database's tree; it borrows the database's keys.
    tree: ?Tree = null,
    /// The selected text, an index into `tree.texts`.
    selected: ?usize = null,
    original: TextBuffer = .{},
    /// The selected text's description (C1), read-only like the original.
    description: TextBuffer = .{},
    translation: TextBuffer = .{},
    /// The translation as it was when the text was selected or last saved:
    /// the edit is dirty while the buffer differs from it.
    saved_translation: std.ArrayListUnmanaged(u8) = .empty,
    state: State = .not_translated,
    /// A state button set the selected text's state, so an edit no longer
    /// turns it Translated (the MFC input view's `bManualState`).
    manual_state: bool = false,
    /// Where Find Next goes on from, on the text the last hit selected; a
    /// search from any other selected text starts at its beginning.
    find_cursor: ?elk.search.Cursor = null,
    /// The last Find Next's hit; null after a miss.
    last_hit: ?Hit = null,
    /// View > ELK Statistic counted since the database was opened, so
    /// Close has a new snapshot to keep.
    counted: bool = false,
    /// When the edit became dirty, on the caller's clock; null while clean.
    dirty_since_ms: ?u64 = null,
    /// The last save point's failure, for the status bar; "" when it worked.
    failure_storage: [256]u8 = undefined,
    failure_len: usize = 0,

    pub fn init(gpa: std.mem.Allocator, io: std.Io) Session {
        return .{ .gpa = gpa, .io = io };
    }

    /// Drops everything without saving; `close` is the save point.
    pub fn deinit(self: *Session) void {
        self.drop();
        self.path_storage.deinit(self.gpa);
        self.original.deinit(self.gpa);
        self.description.deinit(self.gpa);
        self.translation.deinit(self.gpa);
        self.saved_translation.deinit(self.gpa);
    }

    pub fn isOpen(self: *const Session) bool {
        return self.database != null;
    }

    /// The opened `elk.xml` as given; "" when nothing is open.
    pub fn openPath(self: *const Session) []const u8 {
        return if (self.database != null) self.path_storage.items else "";
    }

    pub fn failureText(self: *const Session) []const u8 {
        return self.failure_storage[0..self.failure_len];
    }

    pub fn dirty(self: *const Session) bool {
        if (self.selected == null) return false;
        return !std.mem.eql(u8, self.translation.text(), self.saved_translation.items);
    }

    /// How many texts the open database has; 0 when nothing is open.
    pub fn textCount(self: *const Session) usize {
        return if (self.tree) |tree| tree.texts.len else 0;
    }

    pub fn textAt(self: *const Session, index: usize) Text {
        return self.tree.?.texts[index];
    }

    /// Opens `elk_path`, closing (and so saving) what was open first. On
    /// failure the previous database stays closed and the reason is kept.
    pub fn open(self: *Session, elk_path: []const u8) !void {
        try self.close();
        var database = Database.open(self.gpa, self.io, elk_path) catch |err| {
            self.fail("elk.fail.open", .{ elk_path, @errorName(err) });
            return err;
        };
        errdefer database.deinit();
        var tree = Tree.build(self.gpa, &database) catch |err| {
            self.fail("elk.fail.read_texts", .{ elk_path, @errorName(err) });
            return err;
        };
        errdefer tree.deinit();
        self.path_storage.clearRetainingCapacity();
        try self.path_storage.appendSlice(self.gpa, elk_path);
        self.database = database;
        self.tree = tree;
        self.failure_len = 0;
    }

    /// File > Close: saves the selected text, then closes the database. A
    /// failed save keeps the database open so the edit is not lost. As
    /// `CELK::Close` did, the counts the statistic window made become the
    /// previous snapshot and `elk.xml` is written; that write failing is
    /// named in the status line but closes anyway, as the MFC ELK ignored
    /// it. *Change:* the MFC ELK wrote `elk.xml` on every Close, moving the
    /// statistic it had read into the previous one; this writes it only when
    /// the window counted since the open, so opening and closing leaves the
    /// file as it was.
    pub fn close(self: *Session) !void {
        if (self.database == null) return;
        try self.save();
        if (!self.counted) return self.drop();
        const database = &self.database.?;
        database.project.previous_statistic = database.project.statistic;
        database.save() catch |err| {
            self.drop();
            self.fail("elk.fail.keep_statistic", .{@errorName(err)});
            return;
        };
        self.drop();
    }

    /// Selects `index`, saving the text being left first. A failed save
    /// keeps the current text selected.
    pub fn select(self: *Session, index: usize) !void {
        if (index >= self.textCount()) return error.NoSuchText;
        if (self.selected == index) return;
        try self.save();
        const database = &self.database.?;
        const chosen = self.textAt(index);
        const original = try database.readOriginal(self.gpa, chosen.element, chosen.key);
        defer self.gpa.free(original);
        const description = try database.readDescription(self.gpa, chosen.element, chosen.key);
        defer self.gpa.free(description);
        const translation = try database.readTranslation(self.gpa, chosen.element, chosen.key);
        defer self.gpa.free(translation);
        const property = try database.readState(chosen.element, chosen.key);
        try self.original.set(self.gpa, original);
        try self.description.set(self.gpa, description);
        try self.translation.set(self.gpa, translation);
        self.saved_translation.clearRetainingCapacity();
        try self.saved_translation.appendSlice(self.gpa, translation);
        self.state = property.state;
        self.manual_state = false;
        self.dirty_since_ms = null;
        self.selected = index;
    }

    /// The text `step` reaches through `filter`, null when none does (the
    /// Browse commands are then disabled, as `IsNextItemExists` made them).
    pub fn browseTarget(self: *const Session, step: Step, filter: ?Filter) ?usize {
        const tree = &(self.tree orelse return null);
        const from: ?u32 = if (self.selected) |index| @intCast(index) else null;
        const target = tree.browse(step, from, filter) orelse return null;
        return target;
    }

    /// Browse First, Previous, Next or Last: selects the text the step
    /// reaches, saving the one being left. Returns false when none is
    /// reached, the selection then unchanged.
    pub fn browse(self: *Session, step: Step, filter: ?Filter) !bool {
        const target = self.browseTarget(step, filter) orelse return false;
        try self.select(target);
        return true;
    }

    /// The index of `key`, the first element that has it.
    pub fn find(self: *const Session, key: []const u8) ?usize {
        const tree = self.tree orelse return null;
        for (tree.texts, 0..) |text, index| {
            if (std.mem.eql(u8, text.key, key)) return index;
        }
        return null;
    }

    /// Edit > Find...'s Find Next: the edit is saved first, so the search
    /// reads what the box holds, then the hit's text is selected (a save
    /// point like any other). Null when nothing is found; the selection and
    /// the cursor then stay.
    pub fn findNext(self: *Session, options: FindOptions) !?Hit {
        if (self.tree == null) return null;
        return (try self.run(.{ .find = .{ .options = options } })).found;
    }

    /// Closing the Find dialog sends the next search back to the start of
    /// the selected text, as `FR_DIALOGTERM` reset the search position.
    pub fn resetFind(self: *Session) void {
        self.find_cursor = null;
        self.last_hit = null;
    }

    /// View > ELK Statistic: the counts of every text under the state the
    /// tree shows, its slices in `allocator`. They are also the project's
    /// `statistic` (a copy in its arena), which Close keeps as the previous
    /// snapshot, as `OnViewStatistic` counted into `elk.statistic`.
    pub fn statistics(self: *Session, allocator: std.mem.Allocator) !Statistic {
        const tree = &(self.tree orelse return error.NoDatabaseOpen);
        const database = &self.database.?;
        const counted = elk.stats.compute(allocator, self.gpa, database, tree) catch |err| {
            self.fail("elk.fail.count", .{@errorName(err)});
            return err;
        };
        const arena = database.project.arena.allocator();
        database.project.statistic = .{
            .valid = true,
            .original = try arena.dupe(elk.stats.ElementStatistic, counted.original),
            .translation = try arena.dupe(elk.stats.ElementStatistic, counted.translation),
        };
        self.counted = true;
        return counted;
    }

    /// The snapshot the last Close stored in `elk.xml`, the statistic
    /// window's "as of last close"; null when there is none.
    pub fn lastClose(self: *const Session) ?Statistic {
        const database = &(self.database orelse return null);
        const previous = database.project.previous_statistic;
        return if (previous.valid) previous else null;
    }

    /// The element the exports write: the selected text's, else the first,
    /// as the MFC ELK exported the selected project.
    fn exportElement(self: *const Session) usize {
        return if (self.selected) |index| self.textAt(index).element else 0;
    }

    /// File > Export to language mod... and Install to game...: the edit is
    /// saved first, so the pak holds it, then the core exports the element
    /// to `destination` below `game_folder`.
    pub fn exportPak(self: *Session, game_folder: []const u8, destination: Destination) !Exported {
        return (try self.run(.{ .export_pak = .{ .game_folder = game_folder, .destination = destination } })).exported;
    }

    /// File > Export to spreadsheet...: the edit is saved first, then the
    /// core writes every text to `path` in the format its extension names.
    /// Returns the rows written.
    pub fn exportSheet(self: *Session, path: []const u8) !usize {
        return (try self.run(.{ .export_sheet = path })).sheet_rows;
    }

    /// File > Import from spreadsheet...: the edit is saved first, the core
    /// imports the sheet at `path` (D-10), and the database is opened again
    /// so the tree shows the new states, as the MFC ELK closed and reopened
    /// it. The caller deinits the report.
    pub fn importSheet(self: *Session, path: []const u8) !elk.sheet.Report {
        return (try self.run(.{ .import_sheet = path })).imported;
    }

    /// File > Import from Game... on this thread.
    pub fn importGame(self: *Session, folder: []const u8) !elk.packages.Report {
        return (try self.run(.{ .import_game = .{ .folder = folder } })).updated;
    }

    /// File > Import from PAK/UPD... on this thread.
    pub fn importPak(self: *Session, path: []const u8) !elk.packages.Report {
        return (try self.run(.{ .import_pak = path })).updated;
    }

    /// A long operation on this thread, start to end.
    fn run(self: *Session, work: Work) !Outcome {
        const begun = try self.begin(work);
        return self.finish(begun, self.perform(begun, null));
    }

    /// The UI thread's first step: the edit saved, and the work's element
    /// and the search's start fixed.
    pub fn begin(self: *Session, work: Work) !Work {
        if (self.database == null) return error.NoDatabaseOpen;
        try self.save();
        var begun = work;
        switch (begun) {
            .export_pak => |*pak| pak.element = self.exportElement(),
            .find => |*search| {
                search.from = null;
                if (self.selected) |index| {
                    search.from = .{ .text = @intCast(index) };
                    if (self.find_cursor) |cursor| if (cursor.text == index) {
                        search.from = cursor;
                    };
                }
            },
            .import_game => |*game| {
                const elements = self.database.?.project.elements;
                game.element = if (elements.len == 0) "game" else elements[self.exportElement()].name;
            },
            .export_sheet, .import_sheet, .import_pak => {},
        }
        return begun;
    }

    /// The work itself, on any thread: only the database's files are
    /// written and the tree is only read, so the UI thread may draw the
    /// tree meanwhile as long as it changes nothing. `progress` counts and
    /// cancels it (D-14).
    pub fn perform(self: *Session, work: Work, progress: ?*Progress) !Outcome {
        const database = &self.database.?;
        return switch (work) {
            .export_pak => |pak| .{ .exported = try elk.packages.exportTo(self.gpa, self.io, database, pak.element, pak.game_folder, pak.destination, .{ .filter = pak.filter, .progress = progress }) },
            .export_sheet => |path| .{ .sheet_rows = try elk.sheet.exportTo(self.gpa, self.io, database, path, progress) },
            .import_sheet => |path| .{ .imported = try elk.sheet.importFile(self.gpa, self.io, database, path, progress) },
            .import_game => |game| .{ .updated = try self.importPackage(game.folder, game.element, progress) },
            .import_pak => |path| .{ .updated = try self.importPackage(path, null, progress) },
            .find => |search| .{ .found = try elk.search.findNext(self.gpa, database, &self.tree.?, search.options, search.from, progress) },
        };
    }

    /// Reads the package, the game's folder when `element` names the
    /// element it updates, else the archive at `path`, and imports it into
    /// the open database's files.
    fn importPackage(self: *Session, path: []const u8, element: ?[]const u8, progress: ?*Progress) !elk.packages.Report {
        var package = if (element != null) try elk.packages.readGameFolder(self.gpa, self.io, path) else try elk.packages.readArchive(self.gpa, self.io, path);
        defer package.deinit();
        const file_name = try std.mem.concat(self.gpa, u8, &.{ element orelse std.fs.path.basename(path), if (element != null) ".pak" else "" });
        defer self.gpa.free(file_name);
        return elk.packages.importPackage(self.gpa, self.io, self.openPath(), &package, file_name, progress);
    }

    /// The UI thread's last step: a failure in the status line (a Cancel is
    /// none), the database reopened after an import, a hit selected.
    pub fn finish(self: *Session, work: Work, result: anyerror!Outcome) !Outcome {
        if (work == .import_sheet or work == .import_game or work == .import_pak) {
            // Reopened whatever the import did, a cancelled one having
            // written its first rows.
            const opened = try self.gpa.dupe(u8, self.openPath());
            defer self.gpa.free(opened);
            // Dropped, not closed: Close would write the project as read
            // before the import over the elk.xml the import wrote.
            self.drop();
            self.open(opened) catch |err| {
                if (result) |outcome| switch (outcome) {
                    .imported => |imported| {
                        var report = imported;
                        report.deinit(self.gpa);
                    },
                    else => {},
                } else |_| {}
                return err;
            };
        }
        const outcome = result catch |err| {
            if (err == error.Cancelled) return err;
            switch (work) {
                .export_pak => |pak| switch (pak.destination) {
                    .mod => |name| self.fail("elk.fail.export_mod", .{ name, pak.game_folder, @errorName(err) }),
                    .game => self.fail("elk.fail.install", .{ pak.game_folder, @errorName(err) }),
                },
                .export_sheet => |path| self.fail("elk.fail.export_sheet", .{ path, @errorName(err) }),
                .import_sheet, .import_pak => |path| self.fail("elk.fail.import", .{ path, @errorName(err) }),
                .import_game => |game| self.fail("elk.fail.import_game", .{ game.folder, @errorName(err) }),
                .find => |search| self.fail("elk.fail.search", .{ search.options.text, @errorName(err) }),
            }
            return err;
        };
        switch (outcome) {
            .found => |found| {
                const hit = found orelse {
                    self.last_hit = null;
                    return outcome;
                };
                try self.select(hit.text);
                self.find_cursor = hit.next();
                self.last_hit = hit;
            },
            .exported, .sheet_rows, .updated => self.failure_len = 0,
            .imported => {},
        }
        return outcome;
    }

    /// Install to game's check: the texts a loose file in `game_folder`
    /// shadows; the caller deinits it.
    pub fn shadowed(self: *Session, game_folder: []const u8) !Shadow {
        const database = &(self.database orelse return error.NoDatabaseOpen);
        return elk.packages.shadowedTexts(self.gpa, self.io, database, self.exportElement(), game_folder) catch |err| {
            self.fail("elk.fail.loose", .{ game_folder, @errorName(err) });
            return err;
        };
    }

    /// Every save point: writes the selected text's translation when it was
    /// edited. An edited translation turns the text Translated, as the MFC
    /// input view did when its translation changed, unless a state button
    /// chose the state for this text.
    pub fn save(self: *Session) !void {
        const index = self.selected orelse return;
        if (!self.dirty()) return;
        const database = &self.database.?;
        const chosen = self.textAt(index);
        const edited = self.translation.text();
        _ = database.writeTranslation(chosen.element, chosen.key, edited) catch |err| {
            self.fail("elk.fail.save", .{ chosen.key, @errorName(err) });
            return err;
        };
        if (!self.manual_state) try self.writeState(index, .translated);
        self.saved_translation.clearRetainingCapacity();
        try self.saved_translation.appendSlice(self.gpa, edited);
        self.dirty_since_ms = null;
        self.failure_len = 0;
    }

    /// The edit view's state buttons (`CInputViewDialog::On*RadioButton`):
    /// the selected text's state, written at once and shown up the tree.
    /// Outdated is set only by an update, never by hand.
    pub fn setState(self: *Session, new_state: State) !void {
        const index = self.selected orelse return error.NoTextSelected;
        if (new_state == .outdated) return error.OutdatedIsNotSetByHand;
        try self.writeState(index, new_state);
        self.manual_state = true;
        self.failure_len = 0;
    }

    fn writeState(self: *Session, index: usize, new_state: State) !void {
        const chosen = self.textAt(index);
        _ = self.database.?.setState(chosen.element, chosen.key, new_state) catch |err| {
            self.fail("elk.fail.save_state", .{ chosen.key, @errorName(err) });
            return err;
        };
        self.tree.?.setTextState(@intCast(index), new_state);
        self.state = new_state;
    }

    /// D-15's timed save point, called every frame with a millisecond clock:
    /// a dirty edit is saved `autosave_ms` after it became dirty, and again
    /// that long after each later change. A failed autosave keeps the
    /// reason in the status line and tries again after the next interval.
    /// Returns whether it saved.
    pub fn tick(self: *Session, now_ms: u64) bool {
        if (!self.dirty()) {
            self.dirty_since_ms = null;
            return false;
        }
        const since = self.dirty_since_ms orelse {
            self.dirty_since_ms = now_ms;
            return false;
        };
        if (now_ms -| since < autosave_ms) return false;
        self.save() catch {
            self.dirty_since_ms = now_ms;
            return false;
        };
        return true;
    }

    fn drop(self: *Session) void {
        if (self.tree) |*tree| tree.deinit();
        self.tree = null;
        if (self.database) |*database| database.deinit();
        self.database = null;
        self.selected = null;
        self.state = .not_translated;
        self.manual_state = false;
        self.dirty_since_ms = null;
        self.counted = false;
        self.resetFind();
    }

    /// Keeps a failure for the status line, the text of the table key `key`; ELK's own (a failed
    /// file dialog) go here too.
    pub fn fail(self: *Session, comptime key: []const u8, args: anytype) void {
        const written = l10n.format(&self.failure_storage, key, args) catch {
            // Cut to the buffer: the start of the message says what failed.
            self.failure_len = self.failure_storage.len;
            return;
        };
        self.failure_len = written.len;
    }
};

const testing = std.testing;
const scratch = "zig-out/local-test/elk/app";

/// A copy of the tracked database's project file and one text, so the test
/// never writes into Data/ELK (D-19).
fn fixture(io: std.Io, name: []const u8) ![]u8 {
    const root = try std.fs.path.join(testing.allocator, &.{ scratch, name });
    errdefer testing.allocator.free(root);
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, root) catch {};
    const files = [_][]const u8{ "elk.xml", "game_data_base/medals/1/name.elk", "game_data_base/medals/1/name.xml" };
    for (files) |relative| {
        const source = try std.fs.path.join(testing.allocator, &.{ "Data/ELK", relative });
        defer testing.allocator.free(source);
        const bytes = try cwd.readFileAlloc(io, source, testing.allocator, .limited(1 << 20));
        defer testing.allocator.free(bytes);
        const out = try std.fs.path.join(testing.allocator, &.{ root, relative });
        defer testing.allocator.free(out);
        try cwd.createDirPath(io, std.fs.path.dirname(out).?);
        try cwd.writeFile(io, .{ .sub_path = out, .data = bytes });
    }
    return root;
}

test "the tracked database lists every text and shows the selected one" {
    const io = testing.io;
    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open("Data/ELK/elk.xml");
    try testing.expectEqual(@as(usize, 3081), session.textCount());
    const index = session.find("medals/1/name").?;
    try session.select(index);
    try testing.expectEqualStrings("Test Medal", session.original.text());
    try testing.expectEqualStrings("", session.translation.text());
    try testing.expect(!session.dirty());
    // Leaving an untouched text writes nothing (Data/ELK is never written).
    try session.select(0);
    try session.close();
    try testing.expect(!session.isOpen());
}

test "a selected text shows its own description, else its folder's" {
    const io = testing.io;
    const root = try fixture(io, "description");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    const own = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base/medals/1/name.dsc" });
    defer testing.allocator.free(own);
    const folder = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base/medals/_folder.dsc" });
    defer testing.allocator.free(folder);
    const cwd = std.Io.Dir.cwd();
    for ([_][]const u8{ own, folder }, [_][]const u8{ "Медаль за отвагу", "All medals" }) |path, text| {
        const bytes = try elk.text_codec.fromEditor(testing.allocator, text);
        defer testing.allocator.free(bytes);
        try cwd.writeFile(io, .{ .sub_path = path, .data = bytes });
    }
    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open(elk_path);
    try session.select(session.find("medals/1/name").?);
    try testing.expectEqualStrings("Медаль за отвагу", session.description.text());
    try cwd.deleteFile(io, own);
    try session.close();
    try session.open(elk_path);
    try session.select(session.find("medals/1/name").?);
    try testing.expectEqualStrings("All medals", session.description.text());
}

test "each save point writes an edited translation and turns it Translated" {
    const io = testing.io;
    const root = try fixture(io, "save-points");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    const txt = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base/medals/1/name.txt" });
    defer testing.allocator.free(txt);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open(elk_path);
    try testing.expectEqual(@as(usize, 1), session.textCount());
    try session.select(0);
    try session.translation.set(testing.allocator, "Orden");
    try testing.expect(session.dirty());
    try session.save();
    try testing.expect(!session.dirty());
    try testing.expectEqual(State.translated, session.state);

    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    const written = try database.readTranslation(testing.allocator, 0, "medals/1/name");
    defer testing.allocator.free(written);
    try testing.expectEqualStrings("Orden", written);
    try testing.expectEqual(State.translated, (try database.readState(0, "medals/1/name")).state);

    // Close is a save point too.
    try session.translation.set(testing.allocator, "Orden 2");
    try session.close();
    const closed = try database.readTranslation(testing.allocator, 0, "medals/1/name");
    defer testing.allocator.free(closed);
    try testing.expectEqualStrings("Orden 2", closed);
}

test "a state button writes at once, shows up the tree and outlasts an edit" {
    const io = testing.io;
    const root = try fixture(io, "state-buttons");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open(elk_path);
    try session.select(0);
    const tree = &session.tree.?;
    const folder = tree.findFolder(0, "medals/1").?;
    try testing.expectEqual(State.not_translated, tree.nodes[folder].state);
    try testing.expectError(error.OutdatedIsNotSetByHand, session.setState(.outdated));
    try session.setState(.approved);
    try testing.expectEqual(State.approved, session.state);
    try testing.expectEqual(State.approved, tree.nodes[folder].state);
    try testing.expectEqual(State.approved, tree.nodes[0].state);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    try testing.expectEqual(State.approved, (try database.readState(0, "medals/1/name")).state);

    // The button chose the state: saving an edit keeps it Approved.
    try session.translation.set(testing.allocator, "Orden");
    try session.save();
    try testing.expectEqual(State.approved, (try database.readState(0, "medals/1/name")).state);
    try session.setState(.not_translated);
    try testing.expectEqual(State.not_translated, tree.nodes[0].state);
    const property = try database.readState(0, "medals/1/name");
    try testing.expectEqual(State.not_translated, property.state);
    try testing.expect(property.changed);
}

test "a dirty edit autosaves 30 seconds after it became dirty, and only then" {
    const io = testing.io;
    const root = try fixture(io, "autosave");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    const txt = try std.fs.path.join(testing.allocator, &.{ root, "game_data_base/medals/1/name.txt" });
    defer testing.allocator.free(txt);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open(elk_path);
    try session.select(0);
    // Clean: the clock runs on without a save.
    try testing.expect(!session.tick(0));
    try testing.expect(!session.tick(autosave_ms * 3));
    try testing.expect(!session.tick(autosave_ms * 4));
    try session.translation.set(testing.allocator, "Orden");
    try testing.expect(!session.tick(1_000));
    try testing.expect(!session.tick(1_000 + autosave_ms - 1));
    try testing.expectError(error.FileNotFound, std.Io.Dir.cwd().access(io, txt, .{}));
    try testing.expect(session.tick(1_000 + autosave_ms));
    try testing.expect(!session.dirty());

    // Dropped without a save point, as a crash would: the disk has the
    // autosaved text, not the later edit.
    try session.translation.set(testing.allocator, "Orden 2");
    try testing.expect(!session.tick(40_000));
    session.drop();
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    const written = try database.readTranslation(testing.allocator, 0, "medals/1/name");
    defer testing.allocator.free(written);
    try testing.expectEqualStrings("Orden", written);
    try testing.expectEqual(State.translated, (try database.readState(0, "medals/1/name")).state);
}

test "browsing through a filter saves the text it leaves and skips the rest" {
    const io = testing.io;
    const root = try fixture(io, "browse");
    defer testing.allocator.free(root);
    // A second text beside the fixture's, so there is somewhere to go.
    const cwd = std.Io.Dir.cwd();
    for ([_][]const u8{ "game_data_base/medals/1/desc.elk", "game_data_base/medals/1/desc.xml" }) |relative| {
        const source = try std.fs.path.join(testing.allocator, &.{ "Data/ELK", relative });
        defer testing.allocator.free(source);
        const bytes = try cwd.readFileAlloc(io, source, testing.allocator, .limited(1 << 20));
        defer testing.allocator.free(bytes);
        const out = try std.fs.path.join(testing.allocator, &.{ root, relative });
        defer testing.allocator.free(out);
        try cwd.writeFile(io, .{ .sub_path = out, .data = bytes });
    }
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open(elk_path);
    const desc = session.find("medals/1/desc").?;
    const name = session.find("medals/1/name").?;
    // Nothing selected: Next and Previous reach nothing, First and Last do.
    try testing.expectEqual(@as(?usize, null), session.browseTarget(.next, null));
    try testing.expect(try session.browse(.first, null));
    try testing.expectEqual(desc, session.selected.?);
    try testing.expect(try session.browse(.next, null));
    try testing.expectEqual(name, session.selected.?);
    try testing.expect(!try session.browse(.next, null));
    try testing.expectEqual(name, session.selected.?);

    // Approved only: name is the one; desc, edited, is left by Last.
    const approved = elk.filter.built_in[4];
    try session.setState(.approved);
    try testing.expect(try session.browse(.previous, null));
    try testing.expectEqual(desc, session.selected.?);
    try session.translation.set(testing.allocator, "Beschreibung");
    try testing.expect(try session.browse(.last, approved));
    try testing.expectEqual(name, session.selected.?);
    try testing.expectEqual(@as(?usize, null), session.browseTarget(.previous, approved));
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    const written = try database.readTranslation(testing.allocator, 0, "medals/1/desc");
    defer testing.allocator.free(written);
    try testing.expectEqualStrings("Beschreibung", written);
}

test "Find Next goes on from the last hit and starts over on another text" {
    const io = testing.io;
    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try session.open("Data/ELK/elk.xml");
    const desc = session.find("medals/german/kuban/desc").?;
    const name = session.find("medals/german/kuban/name").?;
    const kuban: FindOptions = .{ .text = "Kuban" };
    try testing.expectEqual(@as(usize, 4), (try session.findNext(kuban)).?.offset);
    try testing.expectEqual(desc, session.selected.?);
    try testing.expectEqual(@as(usize, 68), (try session.findNext(kuban)).?.offset);
    try testing.expectEqual(elk.search.Field.original, (try session.findNext(kuban)).?.field);
    try testing.expectEqual(name, session.selected.?);
    // Chosen by hand, the desc is searched from its start again.
    try session.select(desc);
    try testing.expectEqual(@as(usize, 4), (try session.findNext(kuban)).?.offset);
    // A miss keeps the selection.
    try testing.expectEqual(@as(?Hit, null), try session.findNext(.{ .text = "kuban", .match_case = true }));
    try testing.expectEqual(desc, session.selected.?);
    try testing.expectEqual(@as(?Hit, null), session.last_hit);
    try session.close();
}

test "Close keeps the counted statistic in elk.xml as the previous snapshot" {
    const io = testing.io;
    const root = try fixture(io, "snapshot");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    const cwd = std.Io.Dir.cwd();
    const before = try cwd.readFileAlloc(io, elk_path, testing.allocator, .limited(1 << 20));
    defer testing.allocator.free(before);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    // Opened and closed without counting: elk.xml is not written.
    try session.open(elk_path);
    const stored = session.lastClose().?;
    try testing.expectEqual(@as(i32, 2968), elk.stats.elementTotal(stored.original[0]).texts);
    try session.close();
    const untouched = try cwd.readFileAlloc(io, elk_path, testing.allocator, .limited(1 << 20));
    defer testing.allocator.free(untouched);
    try testing.expectEqualStrings(before, untouched);

    try session.open(elk_path);
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const statistic = try session.statistics(arena.allocator());
    try testing.expectEqual(@as(i32, 1), elk.stats.elementTotal(statistic.original[0]).texts);
    // Until Close the window's "as of last close" is still the stored one.
    try testing.expectEqual(@as(i32, 2968), elk.stats.elementTotal(session.lastClose().?.original[0]).texts);
    try session.close();

    const bytes = try cwd.readFileAlloc(io, elk_path, testing.allocator, .limited(1 << 20));
    defer testing.allocator.free(bytes);
    var project = try elk.project_file.parse(testing.allocator, bytes);
    defer project.deinit();
    try testing.expect(project.previous_statistic.valid and project.statistic.valid);
    try testing.expectEqualDeep(statistic.original, project.previous_statistic.original);
    try testing.expectEqualDeep(statistic.translation, project.previous_statistic.translation);
    try testing.expectEqualDeep(statistic.original, project.statistic.original);
    try session.open(elk_path);
    try testing.expectEqualDeep(statistic.translation, session.lastClose().?.translation);
    try session.close();
}

test "an export saves the edit first and writes the mod or the game's pak" {
    const io = testing.io;
    const root = try fixture(io, "export");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    const game = try std.fs.path.join(testing.allocator, &.{ root, "game" });
    defer testing.allocator.free(game);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try testing.expectError(error.NoDatabaseOpen, session.exportPak(game, .{ .mod = "Deutsch" }));
    try session.open(elk_path);
    try session.select(0);
    try session.translation.set(testing.allocator, "Orden");
    const exported = try session.exportPak(game, .{ .mod = "Deutsch" });
    defer testing.allocator.free(exported.path);
    try testing.expectEqual(@as(usize, 1), exported.entries);
    try testing.expect(!session.dirty());
    try testing.expect(std.mem.endsWith(u8, exported.path, "texts.pak"));
    var package = try elk.packages.readArchive(testing.allocator, io, exported.path);
    defer package.deinit();
    try testing.expectEqual(@as(usize, 1), package.entries.len);
    try testing.expectEqualStrings("medals/1/name.txt", package.entries[0].name);
    const orden = [_]u8{ 0xff, 0xfe, 'O', 0, 'r', 0, 'd', 0, 'e', 0, 'n', 0 };
    try testing.expectEqualSlices(u8, &orden, package.entries[0].bytes);

    // Into the game: nothing loose shadows it, then a loose copy does.
    var clean = try session.shadowed(game);
    defer clean.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 0), clean.count);
    const installed = try session.exportPak(game, .game);
    defer testing.allocator.free(installed.path);
    const want = try std.fs.path.join(testing.allocator, &.{ game, "Data", "Texts.pak" });
    defer testing.allocator.free(want);
    try testing.expectEqualStrings(want, installed.path);
    const loose = try std.fs.path.join(testing.allocator, &.{ game, "Data", "Medals", "1", "NAME.txt" });
    defer testing.allocator.free(loose);
    try std.Io.Dir.cwd().createDirPath(io, std.fs.path.dirname(loose).?);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = loose, .data = "loose" });
    var shadow = try session.shadowed(game);
    defer shadow.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 1), shadow.count);
    try testing.expect(std.mem.endsWith(u8, shadow.folder.?, "Medals"));

    try testing.expectError(error.InvalidModName, session.exportPak(game, .{ .mod = ".." }));
    try testing.expect(std.mem.indexOf(u8, session.failureText(), "InvalidModName") != null);
}

test "a spreadsheet export saves the edit first, and its import reopens with the new state" {
    const io = testing.io;
    const root = try fixture(io, "sheet");
    defer testing.allocator.free(root);
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root, "elk.xml" });
    defer testing.allocator.free(elk_path);
    const xlsx = try std.fs.path.join(testing.allocator, &.{ root, "texts.xlsx" });
    defer testing.allocator.free(xlsx);

    var session = Session.init(testing.allocator, io);
    defer session.deinit();
    try testing.expectError(error.NoDatabaseOpen, session.exportSheet(xlsx));
    try session.open(elk_path);
    try session.select(0);
    try session.translation.set(testing.allocator, "Orden");
    try testing.expectEqual(@as(usize, 1), try session.exportSheet(xlsx));
    try testing.expect(!session.dirty());

    // Another translation since, set back by the import.
    try session.translation.set(testing.allocator, "Medaille");
    try session.setState(.approved);
    try session.save();
    var report = try session.importSheet(xlsx);
    defer report.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 1), report.rows);
    try testing.expectEqual(@as(usize, 1), report.changed);
    try testing.expect(session.isOpen());
    try session.select(0);
    try testing.expectEqual(State.translated, session.state);
    try testing.expectEqualStrings("Orden", session.translation.text());

    var again = try session.importSheet(xlsx);
    defer again.deinit(testing.allocator);
    try testing.expectEqual(@as(usize, 0), again.changed);
    try testing.expectError(error.XlsIsReadOnly, session.exportSheet("texts.xls"));
    try testing.expect(std.mem.indexOf(u8, session.failureText(), "XlsIsReadOnly") != null);
    try testing.expectError(error.FileNotFound, session.importSheet("missing.csv"));
    try testing.expect(session.isOpen());
}

test "a mod is a name or a folder directly below the game's mods" {
    try testing.expectEqualStrings("Deutsch", modName("/g", " Deutsch ").?);
    try testing.expectEqualStrings("Deutsch", modName("/g", "/g/mods/Deutsch").?);
    try testing.expectEqualStrings("Deutsch", modName("/G/", "/g/MODS/Deutsch/").?);
    try testing.expectEqualStrings("Deutsch", modName("C:\\Games\\BK", "c:\\games\\bk\\mods\\Deutsch").?);
    try testing.expectEqual(@as(?[]const u8, null), modName("/g", ""));
    try testing.expectEqual(@as(?[]const u8, null), modName("/g", "/elsewhere/mods/Deutsch"));
    try testing.expectEqual(@as(?[]const u8, null), modName("/g", "/g/mods/Deutsch/data"));
    try testing.expectEqual(@as(?[]const u8, null), modName("/g", "/g/notmods/Deutsch"));
    try testing.expectEqual(@as(?[]const u8, null), modName("/g", "/mods/Deutsch"));
}

test "Run Game: the mod at the main menu in ELK's own test profile" {
    var storage: testlaunch.ArgvStorage = .{};
    const argv = testlaunch.buildArgv(&storage, runGameOptions("/g/Game", "Deutsch", "/u/elk/game.log"));
    const want = [_][]const u8{ "/g/Game", "-editor-test", "-profile=ELKTest", "-mod=Deutsch", "-windowed" };
    try testing.expectEqual(want.len, argv.len);
    for (want, argv) |expected, actual| try testing.expectEqualStrings(expected, actual);
}

test "the text buffer grows for ImGui and settles at the terminator" {
    var buffer: TextBuffer = .{};
    defer buffer.deinit(testing.allocator);
    try buffer.set(testing.allocator, "ab");
    const grown = buffer.grow(testing.allocator, 64, 2).?;
    try testing.expect(grown.len >= 64);
    @memcpy(grown[0..4], "abcd");
    grown[4] = 0;
    buffer.settle();
    try testing.expectEqualStrings("abcd", buffer.text());
}
