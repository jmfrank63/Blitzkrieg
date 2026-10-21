//! The extended layout's minimum for a translator: File and Browse menus,
//! the filter bar, the text tree coloured by state, the selected text's
//! original, its description (C1, read-only like the MFC view's), its
//! translation and the state buttons, and a status line.
//! Every save point goes through the session (D-15): leaving a text, Save
//! (Ctrl+S), Close, Exit and the 30-second autosave. Browse First,
//! Previous (Ctrl+P, Alt+P), Next (Ctrl+N, Alt+N) and Last walk the texts
//! through the filter chosen in the bar; the tree dims the texts the filter
//! rejects and the folders that hold none it keeps, so the walk's path
//! stays visible in the whole tree. With Collapse Item on (B5, the MFC
//! default) a step first closes the whole element it leaves, as
//! `CELKTreeWindow::DeselectItem` collapsed the item's root, so only the
//! way to the new text stays open. Browse > Create Filter... edits the
//! named filters (filter_dialog.zig). Edit > Find... (Ctrl+F) is the MFC
//! frame's modeless Find dialog over the core's search, its text and
//! options kept in elk.cfg; View > ELK Statistic counts the database
//! (`CStatisticDialog`) and shows the counts as the MFC dialog's tree.
//! File > Export to language mod..., Install to game... and Run Game take
//! the database to the game through the core's export (D-08, D-09): the mod
//! route is the default, since the game reads a mod before its loose files;
//! Install to game warns when a loose folder would hide its `Data/Texts.pak`
//! and offers the mod instead; Run Game plays the mod exported this session
//! in ELK's own test profile. Tools > Spelling (F7) walks the misspelt words
//! of the selected text's translation from its start: the current one with
//! a Change to box, Ignore, Ignore All, Change and Change All, then every
//! one with Hunspell's suggestions and an Add to dictionary button; a
//! correction edits the translation box, which the D-15 save points write.
//! Tools > Spelling Options... picks the
//! dictionary folder and the language. Without a dictionary both say why
//! spell check is off, and Spelling is disabled (spelling.zig). File >
//! Import from spreadsheet... and Export to spreadsheet... exchange the
//! texts with a translator's sheet through the core (D-10): `.xlsx` (the
//! default when the name has no extension), `.csv`, and legacy `.xls` read
//! only; the status line sums up what was read, written and skipped.
//! The long operations - the pak export (Export to language mod..., by the
//! current filter when that box is ticked, D6; Install to game; Run Game's
//! export), the spreadsheet export and import, and Find Next - run on a
//! worker thread (jobs.zig, D-14) behind the Working modal: its message, a
//! bar of the position in the range, and Cancel, which stops the work
//! between two texts (an export then writes nothing, an import keeps the
//! rows before, each file whole). The window keeps repainting; the modal
//! holds the rest of the UI off, and the shortcuts and the autosave wait,
//! until the work is done. A quick Find Next shows nothing: the modal stays
//! invisible for its first quarter second.
//! The short layout (D-04, `--short`) is the MFC translator frame: it opens
//! the `elk.xml` beside ELK after applying the newest `.upd` updates there,
//! has the reduced menu, one toolbar and the five fixed filters, and starts
//! the same way each launch: its filter choice is not written to elk.cfg
//! and Collapse Item is always on. Help > Contents (F1) shows the help
//! embedded in ELK (`Sources/editor/elk/help`) in both layouts, Help > About
//! its name, version and what it is. View > ELK Tree (both layouts) and View
//! > Status Bar (the extended one; the short layout always has it) hide the
//! tree and the status line, kept in elk.cfg; a failure still shows its
//! status line, so a refused save is never silent.
const std = @import("std");
const builtin = @import("builtin");
const imgui = @import("editor_imgui");
const sdl3 = @import("sdl3");
const kit = @import("editor_kit");
const session_mod = @import("session.zig");
const settings_mod = @import("settings.zig");
const window_mod = @import("editor_kit").window;
const FilterDialog = @import("filter_dialog.zig").FilterDialog;
const spelling_mod = @import("spelling.zig");
const jobs = @import("jobs.zig");
const help = @import("elk_help");
const elk_build = @import("elk_build");

const ig = imgui.c;
pub const Session = session_mod.Session;
const State = session_mod.State;
const Filter = session_mod.Filter;
pub const Step = session_mod.Step;
const Statistic = session_mod.Statistic;
const Counts = elk_core.stats.Counts;
const elk_core = @import("elk_core");
const l10n = kit.l10n;
const language_picker = kit.language_picker;

/// The name the window opens with, before elk.cfg and the fonts are read; `title` shows
/// `elk.app.title` from the first frame on.
pub const app_title = "Blitzkrieg ELK";

/// File > Open's answer. SDL may call back on another thread or after the
/// window is gone, so it is a global, as the editors' own slots are.
var open_slot: OpenSlot = .{};

const OpenSlot = kit.dialogs.PathSlot(void, settings_mod.max_path);

const open_filters = [_]sdl3.c.SDL_DialogFileFilter{.{ .name = "ELK projects (elk.xml)", .pattern = "xml" }};

/// The export dialog's Browse...: the mod folder the OS dialog chose.
var mod_slot: ModSlot = .{};

const ModSlot = kit.dialogs.PathSlot(void, settings_mod.max_path);

/// File > Import from spreadsheet... and Export to spreadsheet...: the sheet
/// the OS dialog chose, and which of the two asked for it.
var sheet_slot: SheetSlot = .{};

pub const SheetAction = enum { import, @"export" };
const SheetSlot = kit.dialogs.PathSlot(SheetAction, settings_mod.max_path);

/// File > Import from Game... and Import from PAK/UPD...: the folder or
/// the file the OS dialog chose, and which of the two asked for it.
var package_slot: PackageSlot = .{};

pub const PackageAction = enum { game, pak };
const PackageSlot = kit.dialogs.PathSlot(PackageAction, settings_mod.max_path);

const package_filters = [_]sdl3.c.SDL_DialogFileFilter{.{ .name = "Updates (pak, upd)", .pattern = "pak;upd" }};

const import_filters = [_]sdl3.c.SDL_DialogFileFilter{.{ .name = "Spreadsheets (xlsx, csv, legacy xls)", .pattern = "xlsx;csv;xls" }};
const export_filters = [_]sdl3.c.SDL_DialogFileFilter{
    .{ .name = "Excel workbook (xlsx)", .pattern = "xlsx" },
    .{ .name = "CSV, UTF-8 (csv)", .pattern = "csv" },
};
const testlaunch = kit.testlaunch;
/// How long a job runs before its modal shows (it holds the UI off from the
/// start), so a Find Next that hits at once does not flash it.
const job_grace_ms = 250;

pub const Ui = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    session: *Session,
    /// A scripted run without a settings file keeps its settings in memory
    /// only; no run reads or writes the user's own.
    settings: *settings_mod.Settings,
    /// "" writes no file.
    settings_path: []const u8 = "",
    /// Set by `do=focus:translation`: the translation box takes the keyboard
    /// at the next frame.
    focus_translation: bool = false,
    /// Part of the translation box's ID, moved on when spell check changes
    /// the translation: an active box keeps its own copy of the text and
    /// would write it back, a new one reads the buffer.
    translation_generation: u32 = 0,
    /// How many tree rows the last frame drew (the smoke's proof the tree
    /// was laid out).
    rows_drawn: usize = 0,
    /// Which folders are open, one flag per tree node; laid out again for
    /// each database opened (`expanded_for`).
    expanded: std.ArrayListUnmanaged(bool) = .empty,
    expanded_for: ?[*]const @import("elk_core").tree.Node = null,
    /// The nodes the tree shows, every node whose folders are open.
    visible: std.ArrayListUnmanaged(u32) = .empty,
    /// The rows the last frame drew and the colour each was drawn in, for
    /// the scripted runs' tree expectations.
    drawn: std.ArrayListUnmanaged(Drawn) = .empty,
    /// The node the tree scrolls to in the next frame.
    scroll_to: ?u32 = null,
    /// The last frame drew the state buttons (a scripted press needs them).
    state_buttons_drawn: bool = false,
    /// The last frame drew the description view.
    description_drawn: bool = false,
    /// Added to the autosave clock: the scripted runs' `do=wait`, so 30
    /// seconds pass without waiting them out.
    clock_offset_ms: u64 = 0,
    /// A scripted run's `do=kill`: the frame loop stops at once, without
    /// a save point, as a crash would.
    killed: bool = false,
    quit: bool = false,
    /// The last Exit could not save; the next one quits anyway.
    exit_refused: bool = false,
    title_storage: [512:0]u8 = @splat(0),
    /// False in scripted runs: File > Open requests the slot but shows no OS
    /// dialog, and the schedule answers it (`do=answer:<path>`).
    os_dialogs: bool = true,
    /// Browse > Create Filter... while it is open.
    filter_dialog: ?*FilterDialog = null,
    /// Per tree node, whether the current filter keeps it (`markMatches`),
    /// worked out each frame the tree is drawn.
    marks: std.ArrayListUnmanaged(bool) = .empty,
    /// Edit > Find... is open; like the MFC dialog it stays open beside
    /// the editing.
    find_open: bool = false,
    /// The Find what box; it starts with the text elk.cfg kept.
    find_buffer: [settings_mod.max_path:0]u8 = @splat(0),
    /// The Find what box takes the keyboard in the next frame.
    find_focus: bool = false,
    /// The last Find Next found nothing.
    find_missed: bool = false,
    /// The last frame drew the Find window (a scripted press needs it).
    find_drawn: bool = false,
    /// View > ELK Statistic's counts while its window is open, their slices
    /// in `statistics_arena`.
    statistics: ?Statistic = null,
    statistics_arena: ?std.heap.ArenaAllocator = null,
    statistics_drawn: bool = false,
    /// The last frame drew the tree, and the status line.
    tree_drawn: bool = false,
    status_drawn: bool = false,
    /// Help > About.
    about_open: bool = false,
    about_drawn: bool = false,
    /// Spell check, configured from elk.cfg at its first use.
    spelling_state: ?spelling_mod.Spelling = null,
    /// Tools > Spelling's window: the last check's words, in `spell_arena`
    /// with the text they were found in, which tells when they are stale.
    spell_open: bool = false,
    spell_drawn: bool = false,
    spell_results: []const SpellResult = &.{},
    spell_arena: ?std.heap.ArenaAllocator = null,
    spell_text: ?usize = null,
    spell_checked: []const u8 = "",
    /// The walk's place in the checked translation: the current word is the
    /// first listed at or after it. Ignore moves it past the word, so it
    /// holds for this run only; Check Again starts it over.
    spell_cursor: usize = 0,
    /// The Change to box, filled with the current word's first suggestion.
    spell_change_buffer: [256:0]u8 = @splat(0),
    /// Tools > Spelling Options...: the Dictionary folder box.
    spell_options_open: bool = false,
    spell_options_drawn: bool = false,
    spell_folder_buffer: [settings_mod.max_path:0]u8 = @splat(0),
    /// Run Game's environment for the Game it starts.
    environ: std.process.Environ,
    /// File > Export to language mod... is open; its popup opens in the
    /// next frame, ImGui opening popups inside one.
    export_open: bool = false,
    export_opening: bool = false,
    /// Run Game opened the dialog: a successful export starts the game.
    export_then_run: bool = false,
    /// The Mod box: a mod's name, or its folder in `<game>/mods`.
    export_name: [settings_mod.max_path:0]u8 = @splat(0),
    /// Part of the Mod box's ID, moved on when the text is set from outside
    /// (Browse..., a scripted run): an active box keeps its own copy of the
    /// text and would write it back.
    export_name_generation: c_int = 0,
    /// The dialog's last Export wrote nothing (the reason is the session's).
    export_failed: bool = false,
    export_drawn: bool = false,
    /// A scripted Export (true) or Cancel, taken inside the popup as the
    /// buttons are.
    export_pending: ?bool = null,
    /// Install to game's D-09 warning while it is up: what hides the pak.
    install_warning: ?session_mod.Shadow = null,
    install_opening: bool = false,
    install_drawn: bool = false,
    install_pending: ?InstallChoice = null,
    /// The mod exported to in this session, which Run Game plays.
    played_mod: [settings_mod.max_path]u8 = undefined,
    played_mod_len: usize = 0,
    /// The last spreadsheet import's counts, for the scripted runs.
    imported: ?Imported = null,
    /// The last package import's report, for the scripted runs.
    updated: ?elk_core.packages.Report = null,
    /// The folder or file the last package import read, where the next
    /// one's dialog starts; this session only.
    package_last: [settings_mod.max_path]u8 = undefined,
    package_last_len: usize = 0,
    /// The last export's, import's or launch's summary, shown in the status
    /// line.
    notice_storage: [768]u8 = undefined,
    notice_len: usize = 0,
    /// Run Game's Game while it runs: polled each frame, never waited on.
    running: ?testlaunch.Running = null,
    /// How the last game Run Game started ended.
    game_exit: ?testlaunch.Exit = null,
    game_log: [std.Io.Dir.max_path_bytes]u8 = undefined,
    game_log_len: usize = 0,
    /// The last Run Game's command line, one argument per line, for the
    /// scripted runs.
    launched: [2048]u8 = undefined,
    launched_len: usize = 0,
    /// The short translator layout (D-04); extended when false.
    short: bool = false,
    /// The short layout's filter, an index into the five fixed ones; it
    /// starts at All each launch.
    short_filter: usize = 0,
    /// The toolbar the last frame drew, for the scripted runs.
    toolbar_drawn: Toolbar = .none,
    help_open: bool = false,
    help_drawn: bool = false,
    /// The picker's choice, applied at the start of the next frame.
    language_choice: [16]u8 = undefined,
    language_choice_len: usize = 0,
    /// The Hindi probe (`do=probe_hindi`): a fixed Hindi text, PUA-encoded, drawn over a black
    /// strip at `probe_rect` so a captured frame proves the cluster loader. Empty is off.
    probe: [256:0]u8 = undefined,
    probe_len: usize = 0,
    /// `ELK --l10n-auto`: the labels the last frame drew, each with its rect in window pixels, so the
    /// tier that measures the captured frame knows where to look. Off unless the run asks.
    rect_log: bool = false,
    rects: [rect_capacity]DrawnRect = undefined,
    rect_count: usize = 0,
    /// The menu `do=open_menu:<key>` asked for; the frame that draws it opens its popup.
    open_menu: [32]u8 = undefined,
    open_menu_len: usize = 0,
    /// The help's blocks the last frame drew, all of them when it is open.
    help_blocks_drawn: usize = 0,
    /// The heading the contents list was clicked on, scrolled to next frame.
    help_jump: ?usize = null,
    /// The long operation running on the worker thread (D-14), if any.
    job: ?*jobs.Job = null,
    /// Its export starts the game when it succeeds (Run Game).
    job_then_run: bool = false,
    job_opening: bool = false,
    /// The last frame drew the Working modal visibly, at this position of
    /// this range (the scripted runs' proof it repaints mid-run).
    job_drawn: bool = false,
    job_drawn_position: usize = 0,
    job_drawn_range: usize = 0,
    /// A scripted press of the modal's Cancel, taken inside the popup.
    job_cancel_pending: bool = false,
    /// `do=hold`: the next job waits before this unit (`Progress.hold`).
    hold_next: ?usize = null,
    /// The export dialog's "only the texts the filter admits" box (D6).
    export_filtered: bool = false,

    pub const Toolbar = enum { none, filter_bar, short };

    pub const Imported = struct { rows: usize, changed: usize, outdated: usize, empty: usize, missing: usize };

    /// `colour` is the row's state colour; a dimmed row was drawn fainter.
    pub const Drawn = struct { node: u32, colour: u32, dimmed: bool };

    pub fn deinit(self: *Ui) void {
        self.stopJob();
        if (self.filter_dialog) |dialog| self.gpa.destroy(dialog);
        self.closeInstallWarning();
        self.closeStatistics();
        self.closeSpelling();
        if (self.spelling_state) |*state| state.deinit();
        self.marks.deinit(self.gpa);
        self.expanded.deinit(self.gpa);
        self.visible.deinit(self.gpa);
        self.drawn.deinit(self.gpa);
    }

    /// Opens `elk_path` and records it as the most recent database.
    pub fn openDatabase(self: *Ui, elk_path: []const u8) void {
        self.session.open(elk_path) catch {
            self.settings.forgetRecent(elk_path);
            self.saveSettings();
            return;
        };
        self.settings.pushRecent(elk_path);
        self.saveSettings();
    }

    /// File > Exit and the window's close button: Exit is a save point. A
    /// failed save keeps ELK open with the reason in the status line, and a
    /// second Exit then quits without saving, so a database that cannot be
    /// written never holds the window open. A running job is cancelled and
    /// waited for first.
    pub fn requestExit(self: *Ui) void {
        self.stopJob();
        self.session.close() catch {
            if (self.exit_refused) self.quit = true;
            self.exit_refused = true;
            return;
        };
        self.quit = true;
    }

    /// File > Open (Ctrl+O): the OS dialog, starting beside the open or the
    /// most recent database. Its answer opens in a later frame (`draw`).
    pub fn requestOpen(self: *Ui, window: *window_mod.Window) void {
        if (!open_slot.request({})) return;
        if (!self.os_dialogs) return;
        const near = if (self.session.isOpen()) self.session.openPath() else if (self.settings.recentCount() != 0) self.settings.recentAt(0) else "";
        var folder_buffer: [settings_mod.max_path + 1]u8 = undefined;
        const folder = std.fs.path.dirname(near) orelse "";
        const location: ?[*:0]const u8 = if (folder.len == 0) null else if (std.mem.printSentinel(&folder_buffer, "{s}", .{folder}, 0)) |z| z.ptr else |_| null;
        sdl3.c.SDL_ShowOpenFileDialog(OpenSlot.callback, &open_slot, window.window, &open_filters, open_filters.len, location, false);
    }

    /// A scripted run's stand-in for the OS dialogs: answers the File > Open
    /// or the export's Browse... that is waiting. False when none is.
    pub fn answerDialog(self: *Ui, path: []const u8) bool {
        _ = self;
        if (open_slot.waiting()) {
            open_slot.deliver(path);
        } else if (mod_slot.waiting()) {
            mod_slot.deliver(path);
        } else if (sheet_slot.waiting()) {
            sheet_slot.deliver(path);
        } else if (package_slot.waiting()) {
            package_slot.deliver(path);
        } else return false;
        return true;
    }

    fn takeOpen(self: *Ui) void {
        switch (open_slot.take() orelse return) {
            .path => |chosen| self.openDatabase(chosen.path),
            .cancelled => {},
            .failed => |reason| self.session.fail("elk.fail.dialog_file", .{reason}),
        }
    }

    /// Selects the text, saving the one being left, and shows it in the
    /// tree: its folders open and the tree scrolled to it, as the MFC tree
    /// selected its item.
    pub fn select(self: *Ui, index: usize) !void {
        try self.session.select(index);
        self.reveal(self.session.tree.?.node_of_text[index]);
    }

    /// A click on a tree row: a text is selected, a folder is left to its
    /// arrow.
    pub fn clickRow(self: *Ui, node: u32) void {
        const text = self.session.tree.?.nodes[node].text orelse return;
        self.select(text) catch {};
    }

    /// Opens every folder above `node` and scrolls the tree to it.
    pub fn reveal(self: *Ui, node: u32) void {
        const tree = &self.session.tree.?;
        self.syncExpanded() catch return;
        var parent = tree.nodes[node].parent;
        while (parent) |folder| : (parent = tree.nodes[folder].parent) self.expanded.items[folder] = true;
        self.scroll_to = node;
    }

    /// The named filters the bar offers: the settings file's, or the five
    /// built-in ones when it has none.
    pub fn filterCount(self: *const Ui) usize {
        if (self.short) return elk_core.filter.built_in.len;
        return self.settings.filterCount();
    }

    pub fn filterAt(self: *const Ui, index: usize) Filter {
        if (self.short) return elk_core.filter.built_in[index];
        return self.settings.filterAt(index);
    }

    pub fn currentFilterName(self: *const Ui) []const u8 {
        if (self.short) return elk_core.filter.built_in[self.short_filter].name;
        return self.settings.currentFilter();
    }

    /// The current filter; null (every text passes) when its name is not
    /// among the named filters, as `GetCurrentFilter` returned none then.
    pub fn currentFilter(self: *const Ui) ?Filter {
        const name = self.currentFilterName();
        for (0..self.filterCount()) |index| {
            const filter = self.filterAt(index);
            if (std.mem.eql(u8, filter.name, name)) return filter;
        }
        return null;
    }

    /// The filter bar's choice, kept in elk.cfg at once. The selection stays
    /// where it is, as `OnChangeFilter` left it.
    pub fn chooseFilter(self: *Ui, name: []const u8) void {
        if (self.short) {
            for (elk_core.filter.built_in, 0..) |filter, index| {
                if (std.mem.eql(u8, filter.name, name)) self.short_filter = index;
            }
            return;
        }
        self.settings.setCurrentFilter(name);
        self.saveSettings();
    }

    /// Browse > Create Filter...: the dialog opens on a copy of the filters.
    pub fn openFilterDialog(self: *Ui) void {
        if (self.filter_dialog != null or self.short) return;
        const dialog = self.gpa.create(FilterDialog) catch return;
        dialog.* = .start(self.settings);
        self.filter_dialog = dialog;
    }

    /// The dialog's OK (`keep`) or Cancel; OK writes elk.cfg at once.
    pub fn closeFilterDialog(self: *Ui, keep: bool) void {
        const dialog = self.filter_dialog orelse return;
        if (keep) {
            dialog.keep(self.settings);
            self.saveSettings();
        }
        self.gpa.destroy(dialog);
        self.filter_dialog = null;
    }

    /// Browse > Collapse Item, kept in elk.cfg at once.
    pub fn toggleCollapseItem(self: *Ui) void {
        if (self.short) return;
        self.settings.collapse_item = !self.settings.collapse_item;
        self.saveSettings();
    }

    /// View > ELK Tree.
    pub fn toggleTree(self: *Ui) void {
        self.settings.show_tree = !self.settings.show_tree;
        self.saveSettings();
    }

    /// View > Status Bar, the extended layout's only.
    pub fn toggleStatusBar(self: *Ui) void {
        if (self.short) return;
        self.settings.show_status_bar = !self.settings.show_status_bar;
        self.saveSettings();
    }

    /// Whether a Browse step or Find closes the element it leaves: always
    /// in the short layout, which forced the mode on.
    pub fn collapseItem(self: *const Ui) bool {
        return self.short or self.settings.collapse_item;
    }

    /// The short layout's start, `OpenDefaultELK`: the newest `.upd` per
    /// element in `folder` (ELK's own) applied to the `elk.xml` there, which
    /// it creates when missing, and that database opened. With no `.upd`
    /// there it opens nothing and says so, as the MFC ELK refused.
    pub fn startShort(self: *Ui, folder: []const u8) void {
        self.short = true;
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return self.session.fail("elk.fail.folder_path", .{});
        const applied = elk_core.packages.applyUpdates(self.gpa, self.io, elk_path) catch |err|
            return self.session.fail("elk.fail.updates_apply", .{ elk_path, @errorName(err) });
        if (applied.found == 0) return self.session.fail("elk.fail.no_upd", .{folder});
        self.openDatabase(elk_path);
        if (self.session.isOpen()) self.notify("elk.notice.updates_applied", .{ applied.imported, applied.found });
    }

    /// Help > Contents (F1).
    pub fn openHelp(self: *Ui) void {
        self.help_open = true;
    }

    /// A Browse command: the text the step reaches through the current
    /// filter, selected and shown, the translation box then taking the
    /// keyboard as the MFC input view took the focus. False when no text is
    /// reached or the text being left could not be saved.
    pub fn browse(self: *Ui, step: Step) bool {
        const left: ?u32 = if (self.session.selected) |text| self.session.tree.?.node_of_text[text] else null;
        const moved = self.session.browse(step, self.currentFilter()) catch return false;
        if (!moved) return false;
        if (self.collapseItem()) if (left) |node| self.collapseElement(node);
        self.reveal(self.session.tree.?.node_of_text[self.session.selected.?]);
        self.focus_translation = true;
        return true;
    }

    pub fn canBrowse(self: *const Ui, step: Step) bool {
        return self.session.browseTarget(step, self.currentFilter()) != null;
    }

    /// A state button: the selected text's state, written at once.
    pub fn pressState(self: *Ui, state: State) void {
        self.session.setState(state) catch {};
    }

    /// Closes every folder of the element `node` is in, the element's own
    /// row included, as `CollapseCompletely` on the item's root did.
    fn collapseElement(self: *Ui, node: u32) void {
        const tree = &self.session.tree.?;
        self.syncExpanded() catch return;
        var root = node;
        while (tree.nodes[root].parent) |parent| root = parent;
        @memset(self.expanded.items[root..tree.nodes[root].end], false);
    }

    /// Edit > Find... (Ctrl+F): opens the Find window with the kept text,
    /// or gives the open one the keyboard again.
    pub fn openFind(self: *Ui) void {
        if (!self.session.isOpen()) return;
        if (!self.find_open) {
            const kept = self.settings.findText();
            const len = @min(kept.len, self.find_buffer.len);
            @memcpy(self.find_buffer[0..len], kept[0..len]);
            self.find_buffer[len] = 0;
            self.find_missed = false;
        }
        self.find_open = true;
        self.find_focus = true;
    }

    pub fn closeFind(self: *Ui) void {
        self.find_open = false;
        self.find_missed = false;
        self.session.resetFind();
    }

    /// Find Next with the box's text and elk.cfg's options, the text kept
    /// in elk.cfg at once, searched on the worker (a miss reads every text).
    /// The hit's text is selected and shown as a Browse step shows it,
    /// Collapse Item closing the element it leaves (FindItem called
    /// `DeselectItem` too); `foundOrMissed` does that when it is done.
    pub fn findNext(self: *Ui) void {
        const text = std.mem.sliceTo(&self.find_buffer, 0);
        self.settings.setFindText(text);
        self.saveSettings();
        _ = self.startJob(.{ .find = .{ .options = .{
            .text = text,
            .down = self.settings.find_down,
            .match_case = self.settings.find_match_case,
            .whole_word = self.settings.find_whole_word,
        } } }, "elk.job.finding", .{text});
    }

    fn foundOrMissed(self: *Ui, search_from: ?elk_core.search.Cursor, found: ?session_mod.Hit) void {
        const hit = found orelse {
            self.find_missed = true;
            return;
        };
        self.find_missed = false;
        const left: ?u32 = if (search_from) |from| self.session.tree.?.node_of_text[from.text] else null;
        const reached = self.session.tree.?.node_of_text[hit.text];
        if (self.collapseItem()) if (left) |node| if (node != reached) self.collapseElement(node);
        self.reveal(reached);
    }

    pub const FindOption = enum { down, match_case, whole_word };

    /// A Find window box or the direction, kept in elk.cfg at once.
    pub fn setFindOption(self: *Ui, option: FindOption, on: bool) void {
        switch (option) {
            .down => self.settings.find_down = on,
            .match_case => self.settings.find_match_case = on,
            .whole_word => self.settings.find_whole_word = on,
        }
        self.saveSettings();
    }

    /// View > ELK Statistic: counts the database now, as `OnViewStatistic`
    /// did each time, and opens the window.
    pub fn openStatistics(self: *Ui) void {
        if (!self.session.isOpen()) return;
        self.closeStatistics();
        var arena = std.heap.ArenaAllocator.init(self.gpa);
        const statistic = self.session.statistics(arena.allocator()) catch {
            arena.deinit();
            return;
        };
        self.statistics_arena = arena;
        self.statistics = statistic;
    }

    pub fn closeStatistics(self: *Ui) void {
        if (self.statistics_arena) |*arena| arena.deinit();
        self.statistics_arena = null;
        self.statistics = null;
    }

    /// The statistic window's two sets of counts: counted now, and the
    /// snapshot the last Close kept in elk.xml.
    pub const Counted = enum { now, last_close };

    /// One row of the statistic window: a half's total (no element, no
    /// state), a state summed over the elements, an element's total or one
    /// state of an element. Null when the window is closed, there is no
    /// snapshot, or it has no such element (one added since).
    pub fn statisticRow(self: *const Ui, counted: Counted, translation: bool, element: ?usize, state: ?State) ?Counts {
        if (self.statistics == null) return null;
        const statistic = switch (counted) {
            .now => self.statistics.?,
            .last_close => self.session.lastClose() orelse return null,
        };
        const half = if (translation) statistic.translation else statistic.original;
        const stats = elk_core.stats;
        if (element) |index| {
            if (index >= half.len) return null;
            return if (state) |chosen| half[index][@intFromEnum(chosen)] else stats.elementTotal(half[index]);
        }
        if (state) |chosen| return stats.stateTotal(half, @intFromEnum(chosen));
        var total: Counts = .{};
        for (half) |counts| total = stats.sum(total, stats.elementTotal(counts));
        return total;
    }

    /// A misspelt word of the checked translation, with its suggestions.
    pub const SpellResult = struct { word: spelling_mod.Word, text: []const u8, suggestions: []const []const u8 };

    /// Spell check's state, loading the dictionary elk.cfg names the first
    /// time it is asked for.
    pub fn spelling(self: *Ui) *spelling_mod.Spelling {
        if (self.spelling_state == null) {
            self.spelling_state = spelling_mod.Spelling.init(self.gpa, self.io);
            self.configureSpelling();
        }
        return &self.spelling_state.?;
    }

    /// Loads the dictionary folder and language elk.cfg names, and the user
    /// dictionary beside elk.cfg (none for a run without the file).
    fn configureSpelling(self: *Ui) void {
        var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const user_folder: ?[]const u8 = if (self.settings_path.len == 0) null else std.fs.path.dirname(self.settings_path) orelse ".";
        self.spelling_state.?.configure(self.spellFolder(&buffer), self.settings.spellLanguage(), user_folder);
        if (self.spell_open) self.checkSpelling();
    }

    /// elk.cfg's dictionary folder, else `dictionaries` beside ELK; "" when
    /// neither is known.
    pub fn spellFolder(self: *const Ui, buffer: *[std.Io.Dir.max_path_bytes]u8) []const u8 {
        const chosen = self.settings.spellFolder();
        if (chosen.len != 0) return chosen;
        const len = std.process.executableDirPath(self.io, buffer) catch return "";
        const suffix = std.fs.path.sep_str ++ spelling_mod.default_folder_name;
        if (len + suffix.len > buffer.len) return "";
        @memcpy(buffer[len..][0..suffix.len], suffix);
        return buffer[0 .. len + suffix.len];
    }

    /// Tools > Spelling wants a text's translation and a dictionary, as
    /// `OnUpdateToolsSpelling` wanted the edit box and the speller.
    pub fn canSpell(self: *Ui) bool {
        return self.session.selected != null and self.spelling().available();
    }

    /// Tools > Spelling (F7): checks the selected text's translation and
    /// shows its misspelt words.
    pub fn openSpelling(self: *Ui) void {
        if (!self.canSpell()) return;
        self.spell_open = true;
        self.checkAgain();
    }

    /// Check Again: a new run, the walk from the start of the translation.
    pub fn checkAgain(self: *Ui) void {
        self.spell_cursor = 0;
        self.checkSpelling();
    }

    pub fn closeSpelling(self: *Ui) void {
        self.spell_open = false;
        self.dropSpellResults();
    }

    fn dropSpellResults(self: *Ui) void {
        if (self.spell_arena) |*arena| arena.deinit();
        self.spell_arena = null;
        self.spell_results = &.{};
        self.spell_text = null;
        self.spell_checked = "";
    }

    /// The pass over the translation as the edit box holds it now.
    fn checkSpelling(self: *Ui) void {
        self.dropSpellResults();
        const index = self.session.selected orelse return;
        var arena = std.heap.ArenaAllocator.init(self.gpa);
        const allocator = arena.allocator();
        const state = self.spelling();
        const results = collect: {
            const text = allocator.dupe(u8, self.session.translation.text()) catch break :collect null;
            const words = state.check(allocator, text) catch break :collect null;
            const results = allocator.alloc(SpellResult, words.len) catch break :collect null;
            for (words, results) |word, *result| result.* = .{
                .word = word,
                .text = word.of(text),
                .suggestions = state.suggest(allocator, word.of(text)) catch break :collect null,
            };
            self.spell_checked = text;
            break :collect results;
        } orelse {
            arena.deinit();
            self.session.fail("elk.fail.spell_memory", .{});
            return;
        };
        self.spell_arena = arena;
        self.spell_results = results;
        self.spell_text = index;
        self.fillChange();
    }

    /// The walk's current word, an index into `spell_results`; null when
    /// the walk is through or the list no longer matches the text.
    pub fn currentMisspelling(self: *const Ui) ?usize {
        if (self.spellStale()) return null;
        for (self.spell_results, 0..) |result, index| if (result.word.offset >= self.spell_cursor) return index;
        return null;
    }

    /// The Change to box takes the current word's first suggestion, else
    /// the word itself to be typed over.
    fn fillChange(self: *Ui) void {
        const index = self.currentMisspelling() orelse return self.setChange("");
        const result = self.spell_results[index];
        self.setChange(if (result.suggestions.len != 0) result.suggestions[0] else result.text);
    }

    pub fn setChange(self: *Ui, text: []const u8) void {
        const len = @min(text.len, self.spell_change_buffer.len);
        @memcpy(self.spell_change_buffer[0..len], text[0..len]);
        self.spell_change_buffer[len] = 0;
    }

    /// Ignore: the walk goes on past the current word; Check Again finds it
    /// again.
    pub fn ignoreWord(self: *Ui) void {
        const index = self.currentMisspelling() orelse return;
        const word = self.spell_results[index].word;
        self.spell_cursor = word.offset + word.len;
        self.fillChange();
    }

    /// Ignore All: the word passes every check until ELK exits.
    pub fn ignoreAll(self: *Ui) void {
        const index = self.currentMisspelling() orelse return;
        self.spelling().ignore(self.spell_results[index].text) catch {
            self.session.fail("elk.fail.spell_memory", .{});
            return;
        };
        self.checkSpelling();
    }

    /// Change: the current word replaced by the Change to box in the
    /// translation, the walk on past the correction.
    pub fn changeWord(self: *Ui) void {
        const index = self.currentMisspelling() orelse return;
        const word = self.spell_results[index].word;
        const with = std.mem.sliceTo(&self.spell_change_buffer, 0);
        const changed = elk_core.spell.replaceWord(self.gpa, self.session.translation.text(), word, with) catch return self.session.fail("elk.fail.spell_memory", .{});
        defer self.gpa.free(changed);
        self.correct(changed, word.offset + with.len);
    }

    /// Change All: every occurrence of the current word in the translation
    /// replaced, the walk on past the current one's correction.
    pub fn changeAll(self: *Ui) void {
        const index = self.currentMisspelling() orelse return;
        const result = self.spell_results[index];
        const with = std.mem.sliceTo(&self.spell_change_buffer, 0);
        const replaced = elk_core.spell.replaceAll(self.gpa, self.session.translation.text(), result.text, with, result.word.offset + result.word.len) catch
            return self.session.fail("elk.fail.spell_memory", .{});
        defer self.gpa.free(replaced.text);
        self.correct(replaced.text, replaced.kept);
    }

    /// A correction is an edit of the translation box like a typed one:
    /// dirty until a D-15 save point writes it.
    fn correct(self: *Ui, text: []const u8, cursor: usize) void {
        self.session.translation.set(self.gpa, text) catch return self.session.fail("elk.fail.spell_memory", .{});
        self.translation_generation +%= 1;
        self.spell_cursor = cursor;
        self.checkSpelling();
    }

    /// The list no longer matches the text: another one is selected or
    /// this one was edited since the check.
    pub fn spellStale(self: *const Ui) bool {
        return self.spell_text != self.session.selected or !std.mem.eql(u8, self.spell_checked, self.session.translation.text());
    }

    /// The window's Add to dictionary for the result at `index`: the word
    /// goes to custom.dic and the text is checked again.
    pub fn addToDictionary(self: *Ui, index: usize) void {
        if (index >= self.spell_results.len) return;
        self.spelling().addWord(self.spell_results[index].text) catch |err| {
            self.session.fail("elk.fail.dictionary_write", .{@errorName(err)});
            return;
        };
        self.checkSpelling();
    }

    /// Spelling Options...: another dictionary folder ("" is the default),
    /// kept in elk.cfg and loaded at once.
    pub fn setSpellFolder(self: *Ui, folder: []const u8) void {
        self.settings.spell_folder_storage.set(folder);
        self.saveSettings();
        _ = self.spelling();
        self.configureSpelling();
    }

    /// The language combo's choice, kept in elk.cfg and loaded at once.
    pub fn setSpellLanguage(self: *Ui, language: []const u8) void {
        self.settings.spell_language_storage.set(language);
        self.saveSettings();
        _ = self.spelling();
        self.configureSpelling();
    }

    pub fn openSpellOptions(self: *Ui) void {
        const kept = self.settings.spellFolder();
        const len = @min(kept.len, self.spell_folder_buffer.len);
        @memcpy(self.spell_folder_buffer[0..len], kept[0..len]);
        self.spell_folder_buffer[len] = 0;
        self.spell_options_open = true;
    }

    /// The game installation the exports and Run Game use: elk.cfg's, else
    /// the folder ELK runs from, ELK being installed beside Game. Null when
    /// neither is known.
    pub fn gameFolder(self: *const Ui, buffer: *[std.Io.Dir.max_path_bytes]u8) ?[]const u8 {
        const chosen = self.settings.gameFolder();
        if (chosen.len != 0) return chosen;
        const len = std.process.executableDirPath(self.io, buffer) catch return null;
        return buffer[0..len];
    }

    fn knownGameFolder(self: *Ui, buffer: *[std.Io.Dir.max_path_bytes]u8) ?[]const u8 {
        return self.gameFolder(buffer) orelse {
            self.session.fail("elk.fail.game_unknown", .{});
            return null;
        };
    }

    /// The export commands need an open database; Run Game also no game of
    /// its own still running.
    pub fn canExport(self: *const Ui) bool {
        return self.session.isOpen();
    }

    /// File > Import from Game... and Import from PAK/UPD... import into the
    /// open database, while no dialog of theirs waits.
    pub fn canImportPackage(self: *const Ui) bool {
        return self.canExport() and !package_slot.waiting();
    }

    pub fn canRunGame(self: *Ui) bool {
        return self.canExport() and !self.gameRunning();
    }

    pub fn gameRunning(self: *Ui) bool {
        if (self.running) |*running| return running.poll(self.io) == null;
        return false;
    }

    /// File > Export to language mod...: the dialog opens with the mod of
    /// this session's last export.
    pub fn requestExportMod(self: *Ui, then_run: bool) void {
        if (!self.canExport()) return;
        self.setExportName(self.played_mod[0..self.played_mod_len]);
        self.export_open = true;
        self.export_opening = true;
        self.export_then_run = then_run;
        self.export_failed = false;
        self.export_pending = null;
        self.export_filtered = false;
    }

    /// The dialog's Browse...: the OS folder dialog, starting in the game's
    /// `mods`. Its answer fills the Mod box in a later frame (`draw`).
    pub fn requestModFolder(self: *Ui, window: *window_mod.Window) void {
        if (!mod_slot.request({})) return;
        if (!self.os_dialogs) return;
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = self.gameFolder(&game_buffer) orelse "";
        var folder_buffer: [std.Io.Dir.max_path_bytes + 8]u8 = undefined;
        const location: ?[*:0]const u8 = if (game.len == 0) null else if (std.mem.printSentinel(&folder_buffer, "{s}{c}mods", .{ game, std.fs.path.sep }, 0)) |z| z.ptr else |_| null;
        sdl3.c.SDL_ShowOpenFolderDialog(ModSlot.callback, &mod_slot, window.window, location, false);
    }

    fn takeModFolder(self: *Ui) void {
        switch (mod_slot.take() orelse return) {
            .path => |chosen| self.setExportName(chosen.path),
            .cancelled => {},
            .failed => |reason| self.session.fail("elk.fail.dialog_folder", .{reason}),
        }
    }

    /// The Mod box's text, set as a chosen folder or a scripted run sets it;
    /// cut to the box.
    pub fn setExportName(self: *Ui, text: []const u8) void {
        const len = @min(text.len, self.export_name.len);
        @memcpy(self.export_name[0..len], text[0..len]);
        self.export_name[len] = 0;
        self.export_name_generation +%= 1;
    }

    /// The dialog's Export: the Mod box's mod, `mods/<Name>/data/texts.pak`
    /// in the game folder, written by the core on the worker, only the texts
    /// the current filter admits when the box is ticked (D6); the summary
    /// names the entry count. False when it did not start, the reason in
    /// the status line.
    pub fn exportToMod(self: *Ui) bool {
        self.export_failed = true;
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = self.knownGameFolder(&game_buffer) orelse return false;
        const typed = std.mem.sliceTo(&self.export_name, 0);
        const name = session_mod.modName(game, typed) orelse {
            self.session.fail("elk.fail.mod_name", .{ typed, game, std.fs.path.sep_str });
            return false;
        };
        const filter = if (self.export_filtered) self.exportFilter() else null;
        if (!self.startJob(.{ .export_pak = .{ .game_folder = game, .destination = .{ .mod = name }, .filter = filter } }, "elk.job.exporting_mod", .{name})) return false;
        self.job_then_run = self.export_then_run;
        self.export_failed = false;
        return true;
    }

    /// The filter the export dialog offers (D6, the MFC "Export To PAK by
    /// Filter..." of the extended menu): the current one, unless it admits
    /// every text.
    pub fn exportFilter(self: *const Ui) ?Filter {
        if (self.short) return null;
        const filter = self.currentFilter() orelse return null;
        const admits_all = std.mem.allEqual(bool, &filter.states, true) and !filter.changed and filter.conditions.len == 0;
        return if (admits_all) null else filter;
    }

    /// File > Import from spreadsheet... and Export to spreadsheet...: the
    /// OS dialog, starting beside the open database. Its answer imports or
    /// exports in a later frame (`draw`).
    pub fn requestSheet(self: *Ui, window: *window_mod.Window, action: SheetAction) void {
        if (!self.canExport() or !sheet_slot.request(action)) return;
        if (!self.os_dialogs) return;
        var folder_buffer: [settings_mod.max_path + 1]u8 = undefined;
        const folder = std.fs.path.dirname(self.session.openPath()) orelse "";
        const location: ?[*:0]const u8 = if (folder.len == 0) null else if (std.mem.printSentinel(&folder_buffer, "{s}", .{folder}, 0)) |z| z.ptr else |_| null;
        switch (action) {
            .import => sdl3.c.SDL_ShowOpenFileDialog(SheetSlot.callback, &sheet_slot, window.window, &import_filters, import_filters.len, location, false),
            .@"export" => sdl3.c.SDL_ShowSaveFileDialog(SheetSlot.callback, &sheet_slot, window.window, &export_filters, export_filters.len, location),
        }
    }

    fn takeSheet(self: *Ui) void {
        switch (sheet_slot.take() orelse return) {
            .path => |chosen| switch (chosen.kind) {
                .import => self.importSheet(chosen.path),
                .@"export" => self.exportSheet(chosen.path),
            },
            .cancelled => {},
            .failed => |reason| self.session.fail("elk.fail.dialog_file", .{reason}),
        }
    }

    /// Writes every text to the sheet at `chosen`; a name without an
    /// extension gets `.xlsx`, as the MFC dialog added `.xls`.
    pub fn exportSheet(self: *Ui, chosen: []const u8) void {
        var buffer: [settings_mod.max_path + 8]u8 = undefined;
        const path = if (std.fs.path.extension(chosen).len == 0) std.fmt.bufPrint(&buffer, "{s}.xlsx", .{chosen}) catch chosen else chosen;
        _ = self.startJob(.{ .export_sheet = path }, "elk.job.exporting_sheet", .{path});
    }

    /// Imports the sheet at `path` on the worker; `imported` sums it up.
    pub fn importSheet(self: *Ui, path: []const u8) void {
        _ = self.startJob(.{ .import_sheet = path }, "elk.job.importing", .{path});
    }

    /// The import's summary: the rows read, the texts changed and what was
    /// skipped, and the first path the database lacks.
    fn importedSheet(self: *Ui, path: []const u8, report: *const elk_core.sheet.Report) void {
        self.imported = .{ .rows = report.rows, .changed = report.changed, .outdated = report.outdated, .empty = report.empty, .missing = report.missing.len };
        var writer: std.Io.Writer = .fixed(&self.notice_storage);
        defer self.notice_len = writer.buffered().len;
        var part: [640]u8 = undefined;
        writer.writeAll(l10n.format(&part, "elk.notice.imported_sheet", .{
            std.fs.path.basename(path), report.rows, report.changed, report.outdated, report.empty, report.missing.len,
        }) catch return) catch return;
        if (report.missing.len != 0) {
            writer.writeAll(" ") catch return;
            writer.writeAll(l10n.format(&part, "elk.notice.sheet_first_missing", .{report.missing[0]}) catch return) catch return;
        }
        if (report.failed != 0) {
            writer.writeAll(", ") catch return;
            writer.writeAll(l10n.format(&part, "elk.notice.sheet_failed", .{ report.failed, @errorName(report.first_failure.?) }) catch return) catch return;
        }
    }

    /// File > Import from Game... and Import from PAK/UPD...: the OS folder
    /// or file dialog, starting where the last package import read, else
    /// beside the open database. Its answer imports in a later frame
    /// (`draw`).
    pub fn requestPackage(self: *Ui, window: *window_mod.Window, action: PackageAction) void {
        if (!self.canImportPackage() or !package_slot.request(action)) return;
        if (!self.os_dialogs) return;
        const last = self.package_last[0..self.package_last_len];
        const folder = if (last.len == 0) std.fs.path.dirname(self.session.openPath()) orelse "" else if (action == .pak) std.fs.path.dirname(last) orelse "" else last;
        var folder_buffer: [settings_mod.max_path + 1]u8 = undefined;
        const location: ?[*:0]const u8 = if (folder.len == 0) null else if (std.mem.printSentinel(&folder_buffer, "{s}", .{folder}, 0)) |z| z.ptr else |_| null;
        switch (action) {
            .game => sdl3.c.SDL_ShowOpenFolderDialog(PackageSlot.callback, &package_slot, window.window, location, false),
            .pak => sdl3.c.SDL_ShowOpenFileDialog(PackageSlot.callback, &package_slot, window.window, &package_filters, package_filters.len, location, false),
        }
    }

    fn takePackage(self: *Ui) void {
        switch (package_slot.take() orelse return) {
            .path => |chosen| self.importPackage(chosen.kind, chosen.path),
            .cancelled => {},
            .failed => |reason| self.session.fail("elk.fail.dialog_import", .{reason}),
        }
    }

    /// Imports the game's language folder or the `.pak`/`.upd` at `path` on
    /// the worker; `updated` sums it up.
    pub fn importPackage(self: *Ui, action: PackageAction, path: []const u8) void {
        const len = @min(path.len, self.package_last.len);
        @memcpy(self.package_last[0..len], path[0..len]);
        self.package_last_len = len;
        _ = switch (action) {
            .game => self.startJob(.{ .import_game = .{ .folder = path } }, "elk.job.importing_game", .{path}),
            .pak => self.startJob(.{ .import_pak = path }, "elk.job.importing", .{path}),
        };
    }

    /// A package import's summary: the texts imported (new or changed
    /// originals), the descriptions copied and the texts a complete update
    /// deleted.
    fn importedPackage(self: *Ui, work: session_mod.Work, report: elk_core.packages.Report) void {
        self.updated = report;
        const source = switch (work) {
            .import_game => |game| game.folder,
            else => work.import_pak,
        };
        if (report.changedNothing()) return self.notify("elk.notice.imported_nothing", .{source});
        self.notify("elk.notice.imported_package", .{ source, report.originals, report.descriptions, report.deleted });
    }

    /// File > Install to game...: `Data/Texts.pak` in the game folder, at
    /// once when nothing loose hides it, else after D-09's warning.
    pub fn requestInstall(self: *Ui) void {
        if (!self.canExport()) return;
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = self.knownGameFolder(&game_buffer) orelse return;
        var shadow = self.session.shadowed(game) catch return;
        if (shadow.count == 0) {
            shadow.deinit(self.gpa);
            _ = self.installToGame();
            return;
        }
        self.closeInstallWarning();
        self.install_warning = shadow;
        self.install_opening = true;
        self.install_pending = null;
    }

    pub const InstallChoice = enum { mod, install, cancel };

    /// The warning's buttons: the mod route (the default), Install anyway
    /// or Cancel.
    pub fn chooseInstall(self: *Ui, choice: InstallChoice) void {
        self.closeInstallWarning();
        switch (choice) {
            .mod => self.requestExportMod(false),
            .install => _ = self.installToGame(),
            .cancel => {},
        }
    }

    fn closeInstallWarning(self: *Ui) void {
        if (self.install_warning) |*shadow| shadow.deinit(self.gpa);
        self.install_warning = null;
    }

    /// Writes `Data/Texts.pak` in the game folder, where the MFC ELK wrote it.
    pub fn installToGame(self: *Ui) bool {
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = self.knownGameFolder(&game_buffer) orelse return false;
        return self.startJob(.{ .export_pak = .{ .game_folder = game, .destination = .game } }, "elk.job.installing", .{game});
    }

    /// File > Run Game (D-09): the mod exported this session, exported
    /// again so the edits since go with it (the MFC Run Game packed first),
    /// then the game started on it. With no mod exported yet the export
    /// dialog asks for one and starts the game after its Export.
    pub fn requestRunGame(self: *Ui) void {
        if (!self.canRunGame()) return;
        if (self.played_mod_len == 0) return self.requestExportMod(true);
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = self.knownGameFolder(&game_buffer) orelse return;
        const mod = self.played_mod[0..self.played_mod_len];
        if (self.startJob(.{ .export_pak = .{ .game_folder = game, .destination = .{ .mod = mod } } }, "elk.job.exporting_mod", .{mod})) self.job_then_run = true;
    }

    /// Starts `work` on the worker after `Session.begin` (the edit saved),
    /// `key` the table key of the modal's message. False when a job is running already or
    /// it did not start, the reason in the status line.
    fn startJob(self: *Ui, work: session_mod.Work, comptime key: []const u8, args: anytype) bool {
        if (self.job != null) return false;
        const begun = self.session.begin(work) catch return false;
        var message: [768]u8 = undefined;
        const text = l10n.format(&message, key, args) catch key;
        const hold = self.hold_next;
        self.hold_next = null;
        self.job = jobs.Job.start(self.gpa, self.session, begun, text, self.now(), hold) catch |err| {
            self.session.fail("elk.fail.work_start", .{@errorName(err)});
            return false;
        };
        self.job_then_run = false;
        self.job_opening = true;
        self.job_cancel_pending = false;
        return true;
    }

    pub fn busy(self: *const Ui) bool {
        return self.job != null;
    }

    /// The Working modal's Cancel.
    pub fn cancelJob(self: *Ui) void {
        if (self.job) |job| job.progress.cancel();
    }

    /// Each frame: a job that is done is finished.
    fn pollJob(self: *Ui) void {
        const job = self.job orelse return;
        if (job.done()) self.finishJob();
    }

    /// Cancels a running job and waits for it (Exit, and before the UI
    /// goes away).
    fn stopJob(self: *Ui) void {
        if (self.job == null) return;
        self.cancelJob();
        self.finishJob();
    }

    /// The job's result back on the UI thread: `Session.finish`, then the
    /// summary, the found text shown, the game started.
    fn finishJob(self: *Ui) void {
        const job = self.job orelse return;
        defer {
            job.destroy();
            self.job = null;
            self.job_drawn = false;
        }
        const work = job.work;
        const range = job.range();
        const position = job.position();
        const outcome = self.session.finish(work, job.join()) catch |err| {
            if (err != error.Cancelled) return;
            switch (work) {
                .export_pak => self.notify("elk.notice.export_cancelled", .{}),
                .export_sheet => |path| self.notify("elk.notice.export_sheet_cancelled", .{path}),
                .import_sheet => |path| self.notify("elk.notice.import_sheet_cancelled", .{ std.fs.path.basename(path), position, range }),
                .import_pak => |path| self.notify("elk.notice.import_pak_cancelled", .{ std.fs.path.basename(path), position, range }),
                .import_game => |game| self.notify("elk.notice.import_game_cancelled", .{ game.folder, position, range }),
                .find => self.notify("elk.notice.find_cancelled", .{}),
            }
            return;
        };
        switch (outcome) {
            .exported => |exported| {
                defer self.gpa.free(exported.path);
                const pak = work.export_pak;
                switch (pak.destination) {
                    .mod => |name| {
                        @memcpy(self.played_mod[0..name.len], name);
                        self.played_mod_len = name.len;
                        if (pak.filter) |filter|
                            self.notify("elk.notice.exported_filtered", .{ exported.entries, filter.name, exported.path })
                        else
                            self.notify("elk.notice.exported", .{ exported.entries, exported.path });
                        if (self.job_then_run) self.launchGame();
                    },
                    .game => self.notify("elk.notice.installed", .{ exported.entries, exported.path }),
                }
            },
            .sheet_rows => |rows| self.notify("elk.notice.exported_rows", .{ rows, work.export_sheet }),
            .imported => |report| {
                var owned = report;
                defer owned.deinit(self.gpa);
                self.importedSheet(work.import_sheet, &owned);
            },
            .updated => |report| self.importedPackage(work, report),
            .found => |hit| self.foundOrMissed(work.find.from, hit),
        }
    }

    /// `Game -editor-test -profile=ELKTest -mod=<mod> -windowed` in the game
    /// folder, its output in `game.log` beside elk.cfg (in the game folder
    /// when there is no settings file).
    fn launchGame(self: *Ui) void {
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = self.knownGameFolder(&game_buffer) orelse return;
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const executable = if (builtin.os.tag == .windows) "Game.exe" else "Game";
        const game_path = std.fmt.bufPrint(&path_buffer, "{s}{c}{s}", .{ game, std.fs.path.sep, executable }) catch return self.session.fail("elk.fail.game_path", .{});
        const log_folder = std.fs.path.dirname(self.settings_path) orelse game;
        const log = std.fmt.bufPrint(&self.game_log, "{s}{c}{s}", .{ log_folder, std.fs.path.sep, if (self.settings_path.len != 0) "game.log" else "elk-game.log" }) catch return self.session.fail("elk.fail.game_log_path", .{});
        self.game_log_len = log.len;
        const mod = self.played_mod[0..self.played_mod_len];
        const options = session_mod.runGameOptions(game_path, mod, log);
        var storage: testlaunch.ArgvStorage = .{};
        var writer: std.Io.Writer = .fixed(&self.launched);
        for (testlaunch.buildArgv(&storage, options)) |argument| writer.print("{s}\n", .{argument}) catch break;
        self.launched_len = writer.buffered().len;
        self.game_exit = null;
        self.running = testlaunch.start(self.gpa, self.io, self.environ, options) catch |err| {
            self.session.fail("elk.fail.game_start", .{ game_path, @errorName(err) });
            return;
        };
        self.notify("elk.notice.run_game", .{ mod, session_mod.game_profile, log });
    }

    /// The game Run Game started: a failed exit is reported with its log.
    fn pollGame(self: *Ui) void {
        const running = if (self.running) |*running| running else return;
        const exit = running.poll(self.io) orelse return;
        self.running = null;
        self.game_exit = exit;
        switch (testlaunch.describe(exit)) {
            .clean => {},
            .early_failure, .failure => {
                var code: [16]u8 = undefined;
                var signal: [16]u8 = undefined;
                self.session.fail("elk.fail.game_ended", .{ optionalInt(&code, exit.code), optionalInt(&signal, exit.signal), self.game_log[0..self.game_log_len] });
            },
        }
    }

    /// The argument of the last Run Game's command line at `index`.
    pub fn launchedArgument(self: *const Ui, index: usize) ?[]const u8 {
        var lines = std.mem.splitScalar(u8, self.launched[0..self.launched_len], '\n');
        var at: usize = 0;
        while (lines.next()) |line| : (at += 1) {
            if (line.len == 0) return null;
            if (at == index) return line;
        }
        return null;
    }

    pub fn notice(self: *const Ui) []const u8 {
        return self.notice_storage[0..self.notice_len];
    }

    fn notify(self: *Ui, comptime key: []const u8, args: anytype) void {
        const written = l10n.format(&self.notice_storage, key, args) catch {
            self.notice_len = self.notice_storage.len;
            return;
        };
        self.notice_len = written.len;
    }

    /// A dialog of ELK's own is up: the translation box must not take the
    /// focus, which would close its modal.
    fn modalOpen(self: *const Ui) bool {
        return self.filter_dialog != null or self.export_open or self.install_warning != null or self.job != null;
    }

    /// The autosave clock: SDL's milliseconds plus the scripted offset.
    pub fn now(self: *const Ui) u64 {
        return sdl3.c.SDL_GetTicks() + self.clock_offset_ms;
    }

    /// The expanded flags for the open tree: all closed but the roots, as
    /// the MFC tree started, whenever another database was opened.
    fn syncExpanded(self: *Ui) !void {
        const tree = &self.session.tree.?;
        if (self.expanded_for == tree.nodes.ptr and self.expanded.items.len == tree.nodes.len) return;
        try self.expanded.resize(self.gpa, tree.nodes.len);
        for (tree.nodes, self.expanded.items) |node, *open| open.* = node.parent == null;
        self.expanded_for = tree.nodes.ptr;
        self.scroll_to = null;
    }

    pub fn saveSettings(self: *Ui) void {
        const settings = self.settings;
        if (self.settings_path.len == 0) return;
        var buffer: [16 * 1024]u8 = undefined;
        var writer: std.Io.Writer = .fixed(&buffer);
        settings_mod.format(settings, &writer) catch return;
        const cwd = std.Io.Dir.cwd();
        if (std.fs.path.dirname(self.settings_path)) |folder| cwd.createDirPath(self.io, folder) catch return;
        cwd.writeFile(self.io, .{ .sub_path = self.settings_path, .data = writer.buffered() }) catch return;
    }

    /// The window title: `<file> - Blitzkrieg ELK`, as the MFC document
    /// frame showed it, or the application's name alone.
    pub fn title(self: *Ui) [:0]const u8 {
        const opened = self.session.openPath();
        const name = std.mem.span(l10n.t("elk.app.title"));
        const text = if (opened.len == 0)
            std.mem.printSentinel(&self.title_storage, "{s}", .{name}, 0)
        else
            std.mem.printSentinel(&self.title_storage, "{s} - {s}", .{ std.fs.path.basename(opened), name }, 0);
        return text catch app_title;
    }

    pub const rect_capacity = 48;

    /// A label the last frame drew: its table key and the box that holds its ink, in window pixels.
    pub const DrawnRect = struct { key: []const u8, x: f32, y: f32, w: f32, h: f32 };

    /// Notes the item just submitted as the rect of the label `key` (menu items; the ink box of the
    /// rect is the label, so only items without a shortcut text are noted).
    fn noteRect(self: *Ui, comptime key: []const u8) void {
        if (!self.rect_log or self.rect_count == self.rects.len) return;
        const low = ig.igGetItemRectMin();
        const high = ig.igGetItemRectMax();
        self.rects[self.rect_count] = .{ .key = key, .x = low.x, .y = low.y, .w = high.x - low.x, .h = high.y - low.y };
        self.rect_count += 1;
    }

    /// `ig.igBeginMenu` for a menu bar title: opens its popup first when `do=open_menu` asked for it,
    /// and notes the title's box. The box is worked out before the call, because a menu that opens
    /// leaves its popup as the last item.
    fn beginMenu(self: *Ui, comptime key: []const u8) bool {
        if (self.open_menu_len != 0 and std.mem.eql(u8, self.open_menu[0..self.open_menu_len], key)) {
            self.open_menu_len = 0;
            _ = ig.igOpenPopup(l10n.id(key), 0);
        }
        const at = ig.igGetCursorScreenPos();
        const size = ig.igCalcTextSize(l10n.t(key));
        const style = ig.igGetStyle().*;
        if (self.rect_log and self.rect_count < self.rects.len) {
            self.rects[self.rect_count] = .{ .key = key, .x = at.x, .y = at.y + 1, .w = size.x + style.ItemSpacing.x, .h = size.y + 2 * style.FramePadding.y - 2 };
            self.rect_count += 1;
        }
        return ig.igBeginMenu(l10n.id(key));
    }

    /// Notes the title bar text of the window just begun, for `key`.
    fn noteTitle(self: *Ui, comptime key: []const u8) void {
        if (!self.rect_log or self.rect_count == self.rects.len) return;
        const low = ig.igGetItemRectMin();
        const high = ig.igGetItemRectMax();
        const size = ig.igCalcTextSize(l10n.t(key));
        const padding = ig.igGetStyle().*.FramePadding.x;
        self.rects[self.rect_count] = .{ .key = key, .x = low.x + padding - 2, .y = low.y + 1, .w = size.x + 4, .h = high.y - low.y - 2 };
        self.rect_count += 1;
    }

    /// The window's size in ImGui pixels, as the last frame was laid out.
    pub fn displaySize(_: *const Ui) ig.ImVec2 {
        return ig.igGetIO().*.DisplaySize;
    }

    /// `do=open_menu:<key>`: the next frame opens that menu, as a click on its title would.
    pub fn openMenu(self: *Ui, key: []const u8) bool {
        if (key.len == 0 or key.len > self.open_menu.len) return false;
        @memcpy(self.open_menu[0..key.len], key);
        self.open_menu_len = key.len;
        return true;
    }

    /// Where the Hindi probe is drawn, in window pixels: x, y, width, height.
    pub const probe_rect = [4]f32{ 20, 740, 600, 40 };
    /// The probe's text: the native name of Hindi, a conjunct and a short-i matra word.
    pub const probe_source = "\u{939}\u{93F}\u{928}\u{94D}\u{926}\u{940} \u{915}\u{94D}\u{937} \u{915}\u{93F}\u{924}\u{93E}\u{92C}";

    /// Encodes `probe_source` and turns the probe on; false when the Devanagari face is not loaded.
    pub fn startProbe(self: *Ui) bool {
        const encoded = kit.fonts.encodeHindi(&self.probe, probe_source) orelse return false;
        self.probe_len = encoded.len;
        return true;
    }

    fn drawProbe(self: *Ui) void {
        if (self.probe_len == 0) return;
        const list = ig.igGetForegroundDrawList();
        const min: ig.ImVec2 = .{ .x = probe_rect[0], .y = probe_rect[1] };
        ig.ImDrawList_AddRectFilled(list, min, .{ .x = min.x + probe_rect[2], .y = min.y + probe_rect[3] }, 0xFF000000);
        ig.ImDrawList_AddText(list, .{ .x = min.x + 4, .y = min.y + 10 }, 0xFFFFFFFF, &self.probe);
    }

    pub fn draw(self: *Ui, window: *window_mod.Window) void {
        self.rect_count = 0;
        self.pollJob();
        if (self.job == null) {
            self.takeOpen();
            self.takeModFolder();
            self.takeSheet();
            self.takePackage();
        }
        self.pollGame();
        window.setTitle(self.title().ptr);
        if (self.job == null) self.drawShortcuts(window);

        const viewport = ig.igGetMainViewport();
        ig.igSetNextWindowPos(viewport.*.WorkPos, 0);
        ig.igSetNextWindowSize(viewport.*.WorkSize, 0);
        const flags = ig.ImGuiWindowFlags_NoDecoration | ig.ImGuiWindowFlags_NoMove | ig.ImGuiWindowFlags_NoSavedSettings |
            ig.ImGuiWindowFlags_MenuBar | ig.ImGuiWindowFlags_NoBringToFrontOnFocus;
        _ = ig.igBegin(l10n.id("elk.window.main"), null, flags);
        defer ig.igEnd();
        // Last in the frame: a translation box activated in this frame
        // focuses the main window, which would close a modal opened before.
        // The Working modal comes after every dialog a job starts from.
        defer self.drawJobModal();
        defer self.drawProbe();
        defer self.drawFilterDialog();
        defer self.drawExportDialog(window);
        defer self.drawInstallWarning();
        defer self.drawStatisticsWindow();
        defer self.drawFindWindow();
        defer self.drawSpellingWindow();
        defer self.drawSpellOptions();
        defer self.drawHelpWindow();
        defer self.drawAboutWindow();
        if (self.short) self.drawShortMenu(window) else self.drawMenu(window);
        self.rows_drawn = 0;
        self.drawn.clearRetainingCapacity();
        self.state_buttons_drawn = false;
        self.description_drawn = false;
        self.toolbar_drawn = .none;
        self.tree_drawn = false;
        self.status_drawn = false;
        if (!self.session.isOpen()) {
            ig.igTextUnformatted(if (self.short) l10n.t("elk.nodb.short") else l10n.t("elk.nodb.long"));
            self.drawFailure();
            return;
        }
        if (self.short) self.drawShortToolbar(window) else self.drawFilterBar();
        const avail = ig.igGetContentRegionAvail();
        const status = self.short or self.settings.show_status_bar or self.session.failureText().len != 0;
        const body_height = if (status) avail.y - ig.igGetFrameHeightWithSpacing() else 0;
        if (self.settings.show_tree) {
            if (ig.igBeginChild("##texts", .{ .x = avail.x * 0.35, .y = body_height }, ig.ImGuiChildFlags_Borders, 0)) self.drawTree();
            ig.igEndChild();
            ig.igSameLine();
            self.tree_drawn = true;
        }
        if (ig.igBeginChild("##text", .{ .x = 0, .y = body_height }, 0, 0)) self.drawText();
        ig.igEndChild();
        if (status) self.drawStatus();
        self.status_drawn = status;
    }

    /// The global shortcuts and the autosave clock, held while a job runs.
    fn drawShortcuts(self: *Ui, window: *window_mod.Window) void {
        if (ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_S, ig.ImGuiInputFlags_RouteGlobal)) self.session.save() catch {};
        if (!self.short and ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_O, ig.ImGuiInputFlags_RouteGlobal)) self.requestOpen(window);
        if (ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_F, ig.ImGuiInputFlags_RouteGlobal)) self.openFind();
        if (ig.igShortcut(ig.ImGuiKey_F7, ig.ImGuiInputFlags_RouteGlobal)) self.openSpelling();
        if (ig.igShortcut(ig.ImGuiKey_F1, ig.ImGuiInputFlags_RouteGlobal)) self.openHelp();
        // IDR_MAINFRAME's accelerators: Ctrl or Alt with N and P. ImGui does
        // not route Alt with a letter while the translation box is active
        // (AltGr and macOS's Option type characters with it), so there the
        // Alt chords are the edit box's and the Ctrl ones, which the menu
        // shows, browse.
        for ([_]c_int{ ig.ImGuiMod_Ctrl, ig.ImGuiMod_Alt }) |modifier| {
            if (ig.igShortcut(modifier | ig.ImGuiKey_N, ig.ImGuiInputFlags_RouteGlobal)) _ = self.browse(.next);
            if (ig.igShortcut(modifier | ig.ImGuiKey_P, ig.ImGuiInputFlags_RouteGlobal)) _ = self.browse(.previous);
        }
        _ = self.session.tick(self.now());
    }

    /// D-14's progress modal: the job's message, the position in its range
    /// as a bar, and Cancel. Invisible (it still holds the UI off) for the
    /// job's first `job_grace_ms`, unless the job is held.
    fn drawJobModal(self: *Ui) void {
        self.job_drawn = false;
        const job = self.job orelse return;
        if (self.job_opening) {
            _ = ig.igOpenPopup(l10n.id("elk.job.title"), 0);
            self.job_opening = false;
        }
        const shown = job.progress.held.load(.acquire) or self.now() -| job.started_ms >= job_grace_ms;
        const font = ig.igGetFontSize();
        ig.igSetNextWindowSize(.{ .x = font * 30, .y = 0 }, ig.ImGuiCond_Always);
        if (!shown) ig.igPushStyleColor(ig.ImGuiCol_ModalWindowDimBg, 0);
        defer if (!shown) ig.igPopStyleColor();
        const hidden_flags = ig.ImGuiWindowFlags_NoDecoration | ig.ImGuiWindowFlags_NoBackground;
        if (!ig.igBeginPopupModal(l10n.id("elk.job.title"), null, ig.ImGuiWindowFlags_NoSavedSettings | ig.ImGuiWindowFlags_NoResize | if (shown) 0 else hidden_flags)) return;
        defer ig.igEndPopup();
        if (self.job_cancel_pending) {
            self.job_cancel_pending = false;
            self.cancelJob();
        }
        if (!shown) return;
        const position = job.position();
        const range = job.range();
        self.job_drawn = true;
        self.job_drawn_position = position;
        self.job_drawn_range = range;
        ig.igPushTextWrapPos(0);
        ig.igTextUnformatted(&job.message);
        ig.igPopTextWrapPos();
        var overlay: [64:0]u8 = undefined;
        const counted = std.mem.printSentinel(&overlay, "{d} / {d}", .{ position, range }, 0) catch "";
        const fraction: f32 = if (range == 0) 0 else @floatCast(@as(f64, @floatFromInt(position)) / @as(f64, @floatFromInt(range)));
        ig.igProgressBar(fraction, .{ .x = -1, .y = 0 }, counted.ptr);
        const cancelled = job.progress.cancelled.load(.acquire);
        ig.igBeginDisabled(cancelled);
        if (ig.igButton(if (cancelled) l10n.id("elk.job.cancelling") else l10n.id("elk.common.cancel")) or ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) self.cancelJob();
        ig.igEndDisabled();
    }

    fn drawFilterDialog(self: *Ui) void {
        const dialog = self.filter_dialog orelse return;
        switch (dialog.draw()) {
            .open => {},
            .ok => self.closeFilterDialog(true),
            .cancel => self.closeFilterDialog(false),
        }
    }

    /// File > Export to language mod...: the Mod box takes a mod's name or,
    /// through Browse..., its folder in the game's `mods`; Export writes
    /// `mods/<Name>/data/texts.pak` there.
    fn drawExportDialog(self: *Ui, window: *window_mod.Window) void {
        self.export_drawn = false;
        if (!self.export_open) return;
        if (self.export_opening) {
            _ = ig.igOpenPopup(l10n.id("elk.export.title"), 0);
            self.export_opening = false;
        }
        const font = ig.igGetFontSize();
        ig.igSetNextWindowSize(.{ .x = font * 36, .y = 0 }, ig.ImGuiCond_Appearing);
        if (!ig.igBeginPopupModal(l10n.id("elk.export.title"), null, 0)) {
            self.export_open = false;
            return;
        }
        defer ig.igEndPopup();
        self.export_drawn = true;
        if (self.export_pending) |export_now| {
            self.export_pending = null;
            if (!export_now or self.exportToMod()) {
                ig.igCloseCurrentPopup();
                self.export_open = false;
            }
            return;
        }
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = self.gameFolder(&game_buffer) orelse "";
        var line: [std.Io.Dir.max_path_bytes + 256:0]u8 = undefined;
        ig.igPushTextWrapPos(0);
        ig.igTextUnformatted(l10n.t("elk.export.intro"));
        ig.igPopTextWrapPos();
        ig.igTextUnformatted(l10n.t("elk.export.mod_label"));
        if (ig.igIsWindowAppearing()) ig.igSetKeyboardFocusHere();
        ig.igSetNextItemWidth(font * 26);
        ig.igPushIDInt(self.export_name_generation);
        const entered = ig.igInputText("##mod", &self.export_name, self.export_name.len, ig.ImGuiInputTextFlags_EnterReturnsTrue);
        ig.igPopID();
        ig.igSameLine();
        ig.igBeginDisabled(mod_slot.waiting());
        if (ig.igButton(l10n.id("elk.export.browse"))) self.requestModFolder(window);
        ig.igEndDisabled();
        const typed = std.mem.sliceTo(&self.export_name, 0);
        const name = session_mod.modName(game, typed);
        const sep = std.fs.path.sep_str;
        const shown: [:0]const u8 = if (name) |chosen|
            l10n.formatZ(&line, "elk.export.writes", .{ game, sep, chosen }) orelse ""
        else if (typed.len != 0)
            l10n.formatZ(&line, "elk.export.bad_name", .{ game, sep }) orelse ""
        else
            "";
        ig.igTextUnformatted(shown.ptr);
        if (self.exportFilter()) |filter| {
            const label = l10n.formatZ(&line, "elk.export.only_filter", .{filter.name}) orelse "";
            _ = ig.igCheckbox(label.ptr, &self.export_filtered);
        }
        if (self.export_failed) {
            const failure = std.mem.printSentinel(&line, "{s}", .{self.session.failureText()}, 0) catch "";
            ig.igTextUnformatted(failure.ptr);
        }
        ig.igSeparator();
        ig.igBeginDisabled(name == null);
        const pressed = ig.igButton(if (self.export_then_run) l10n.id("elk.export.export_run") else l10n.id("elk.export.export"));
        ig.igEndDisabled();
        if ((pressed or entered) and name != null and self.exportToMod()) {
            ig.igCloseCurrentPopup();
            self.export_open = false;
            return;
        }
        ig.igSameLine();
        if (ig.igButton(l10n.id("elk.common.cancel")) or ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) {
            ig.igCloseCurrentPopup();
            self.export_open = false;
        }
    }

    /// Install to game's D-09 warning: the loose folder named, the mod
    /// route as the default button (Enter), Install anyway, Cancel (Escape).
    fn drawInstallWarning(self: *Ui) void {
        self.install_drawn = false;
        const shadow = self.install_warning orelse return;
        if (self.install_opening) {
            _ = ig.igOpenPopup(l10n.id("elk.install.title"), 0);
            self.install_opening = false;
        }
        ig.igSetNextWindowSize(.{ .x = ig.igGetFontSize() * 36, .y = 0 }, ig.ImGuiCond_Appearing);
        if (!ig.igBeginPopupModal(l10n.id("elk.install.title"), null, 0)) return self.closeInstallWarning();
        var choice: ?InstallChoice = null;
        {
            defer ig.igEndPopup();
            self.install_drawn = true;
            choice = self.install_pending;
            self.install_pending = null;
            var line: [std.Io.Dir.max_path_bytes + 512:0]u8 = undefined;
            const text = l10n.formatZ(&line, "elk.install.body", .{ shadow.count, shadow.folder.? }) orelse "";
            ig.igPushTextWrapPos(0);
            ig.igTextUnformatted(text.ptr);
            ig.igPopTextWrapPos();
            ig.igSeparator();
            if (ig.igButton(l10n.id("elk.file.export_mod"))) choice = .mod;
            ig.igSetItemDefaultFocus();
            ig.igSameLine();
            if (ig.igButton(l10n.id("elk.install.anyway"))) choice = .install;
            ig.igSameLine();
            if (ig.igButton(l10n.id("elk.common.cancel"))) choice = .cancel;
            if (choice == null and (ig.igIsKeyPressedEx(ig.ImGuiKey_Enter, false) or ig.igIsKeyPressedEx(ig.ImGuiKey_KeypadEnter, false))) choice = .mod;
            if (choice == null and ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) choice = .cancel;
            if (choice != null) ig.igCloseCurrentPopup();
        }
        if (choice) |chosen| self.chooseInstall(chosen);
    }

    fn drawMenu(self: *Ui, window: *window_mod.Window) void {
        if (!ig.igBeginMenuBar()) return;
        defer ig.igEndMenuBar();
        self.drawFileMenu(window);
        self.drawEditMenu();
        self.drawBrowseMenu();
        self.drawViewMenu();
        self.drawToolsMenu();
        self.drawHelpMenu();
    }

    fn drawHelpMenu(self: *Ui) void {
        if (!self.beginMenu("elk.menu.help")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx(l10n.id("elk.help.contents"), "F1", false, true)) self.openHelp();
        ig.igSeparator();
        if (ig.igMenuItem(l10n.id("elk.help.about"))) self.about_open = true;
    }

    /// Help > About: the name, the version and what the kit is.
    fn drawAboutWindow(self: *Ui) void {
        self.about_drawn = false;
        if (!self.about_open) return;
        const viewport = ig.igGetMainViewport();
        const centre: ig.ImVec2 = .{ .x = viewport.*.WorkPos.x + viewport.*.WorkSize.x * 0.5, .y = viewport.*.WorkPos.y + viewport.*.WorkSize.y * 0.4 };
        ig.igSetNextWindowPosEx(centre, ig.ImGuiCond_Appearing, .{ .x = 0.5, .y = 0.5 });
        var open = true;
        if (ig.igBegin(l10n.id("elk.about.title"), &open, ig.ImGuiWindowFlags_AlwaysAutoResize | ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.about_drawn = true;
            var version: [128:0]u8 = undefined;
            ig.igTextUnformatted(l10n.t("elk.app.title"));
            const version_text: [:0]const u8 = l10n.formatZ(&version, "elk.about.version", .{elk_build.version}) orelse "";
            ig.igTextUnformatted(version_text.ptr);
            ig.igPushTextWrapPos(ig.igGetFontSize() * 28);
            ig.igTextUnformatted(l10n.t("elk.about.kit"));
            ig.igPopTextWrapPos();
            if (ig.igButton(l10n.id("elk.common.ok"))) open = false;
        }
        ig.igEnd();
        if (!open) self.about_open = false;
    }

    /// `IDR_SHORT_MAINFRAME`'s menu: Exit; Next and Previous; the statistic;
    /// Find, spell check, the export to a mod, Run Game and the spreadsheets
    /// under Tools; the help.
    fn drawShortMenu(self: *Ui, window: *window_mod.Window) void {
        if (!ig.igBeginMenuBar()) return;
        defer ig.igEndMenuBar();
        if (self.beginMenu("elk.menu.file")) {
            defer ig.igEndMenu();
            if (ig.igMenuItem(l10n.id("elk.file.exit"))) self.requestExit();
        }
        if (self.beginMenu("elk.menu.browse")) {
            defer ig.igEndMenu();
            if (ig.igMenuItemEx(l10n.id("elk.browse.next"), "Ctrl+N", false, self.canBrowse(.next))) _ = self.browse(.next);
            if (ig.igMenuItemEx(l10n.id("elk.browse.previous"), "Ctrl+P", false, self.canBrowse(.previous))) _ = self.browse(.previous);
        }
        self.drawViewMenu();
        if (self.beginMenu("elk.menu.tools")) {
            defer ig.igEndMenu();
            if (ig.igMenuItemEx(l10n.id("elk.edit.find"), "Ctrl+F", false, self.session.isOpen())) self.openFind();
            if (ig.igMenuItemEx(l10n.id("elk.tools.spelling"), "F7", false, self.canSpell())) self.openSpelling();
            if (ig.igMenuItem(l10n.id("elk.tools.spelling_options"))) self.openSpellOptions();
            ig.igSeparator();
            self.drawLanguage();
            ig.igSeparator();
            if (ig.igMenuItemEx(l10n.id("elk.file.export_mod"), null, false, self.canExport())) self.requestExportMod(false);
            if (ig.igMenuItemEx(l10n.id("elk.file.run_game"), null, false, self.canRunGame())) self.requestRunGame();
            ig.igSeparator();
            const sheet_free = self.canExport() and !sheet_slot.waiting();
            if (ig.igMenuItemEx(l10n.id("elk.file.export_sheet"), null, false, sheet_free)) self.requestSheet(window, .@"export");
            if (ig.igMenuItemEx(l10n.id("elk.file.import_sheet"), null, false, sheet_free)) self.requestSheet(window, .import);
        }
        self.drawHelpMenu();
    }

    /// The short layout's one toolbar, `SHORT_BUTTONS` in its order: Run
    /// Game and the export; the filter and Back and Next; the spreadsheets;
    /// Find and Spelling; the statistic; Exit.
    fn drawShortToolbar(self: *Ui, window: *window_mod.Window) void {
        self.toolbar_drawn = .short;
        const Button = struct { label: [*:0]const u8, enabled: bool };
        const sheet_free = self.canExport() and !sheet_slot.waiting();
        const first = [_]Button{
            .{ .label = l10n.id("elk.toolbar.run_game"), .enabled = self.canRunGame() },
            .{ .label = l10n.id("elk.toolbar.export_mod"), .enabled = self.canExport() },
        };
        for (first, 0..) |button, index| {
            if (index != 0) ig.igSameLine();
            ig.igBeginDisabled(!button.enabled);
            if (ig.igButton(button.label)) switch (index) {
                0 => self.requestRunGame(),
                else => self.requestExportMod(false),
            };
            ig.igEndDisabled();
        }
        ig.igSameLine();
        self.drawFilterCombo();
        const rest = [_]Button{
            .{ .label = l10n.id("elk.toolbar.back"), .enabled = self.canBrowse(.previous) },
            .{ .label = l10n.id("elk.toolbar.next"), .enabled = self.canBrowse(.next) },
            .{ .label = l10n.id("elk.toolbar.export_sheet"), .enabled = sheet_free },
            .{ .label = l10n.id("elk.toolbar.import_sheet"), .enabled = sheet_free },
            .{ .label = l10n.id("elk.toolbar.find"), .enabled = true },
            .{ .label = l10n.id("elk.toolbar.spelling"), .enabled = self.canSpell() },
            .{ .label = l10n.id("elk.toolbar.statistic"), .enabled = true },
            .{ .label = l10n.id("elk.toolbar.exit"), .enabled = true },
        };
        for (rest, 0..) |button, index| {
            ig.igSameLine();
            ig.igBeginDisabled(!button.enabled);
            if (ig.igButton(button.label)) switch (index) {
                0 => _ = self.browse(.previous),
                1 => _ = self.browse(.next),
                2 => self.requestSheet(window, .@"export"),
                3 => self.requestSheet(window, .import),
                4 => self.openFind(),
                5 => self.openSpelling(),
                6 => self.openStatistics(),
                else => self.requestExit(),
            };
            ig.igEndDisabled();
        }
    }

    /// Help > Contents: the embedded help, its headings listed on the left
    /// as a table of contents that scrolls the text to the one clicked.
    fn drawHelpWindow(self: *Ui) void {
        self.help_drawn = false;
        self.help_blocks_drawn = 0;
        if (!self.help_open) return;
        const font = ig.igGetFontSize();
        ig.igSetNextWindowSize(.{ .x = font * 50, .y = font * 32 }, ig.ImGuiCond_Appearing);
        var open = true;
        if (ig.igBegin(l10n.id("elk.helpwin.title"), &open, ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.help_drawn = true;
            self.noteTitle("elk.helpwin.title");
            var line: [help_line_bytes:0]u8 = undefined;
            var scratch: [help_line_bytes:0]u8 = undefined;
            const help_text = help.forLanguage(l10n.language());
            if (ig.igBeginChild("##contents", .{ .x = font * 14, .y = 0 }, ig.ImGuiChildFlags_Borders, 0)) {
                var blocks = help.Blocks.init(help_text);
                var heading: usize = 0;
                while (blocks.next()) |block| {
                    if (block.kind != .heading) continue;
                    const label = helpLine(&line, &scratch, block.text) orelse continue;
                    ig.igPushIDInt(@intCast(heading));
                    if (ig.igSelectableEx(label.ptr, false, 0, .{ .x = 0, .y = 0 })) self.help_jump = heading;
                    ig.igPopID();
                    heading += 1;
                }
            }
            ig.igEndChild();
            ig.igSameLine();
            if (ig.igBeginChild("##help", .{ .x = 0, .y = 0 }, 0, 0)) {
                ig.igPushTextWrapPos(0);
                defer ig.igPopTextWrapPos();
                var blocks = help.Blocks.init(help_text);
                var heading: usize = 0;
                while (blocks.next()) |block| {
                    const text = helpLine(&line, &scratch, block.text) orelse continue;
                    switch (block.kind) {
                        .title => ig.igSeparatorText(text.ptr),
                        .heading => {
                            ig.igSpacing();
                            ig.igSeparatorText(text.ptr);
                            if (self.help_jump == heading) {
                                ig.igSetScrollHereY(0);
                                self.help_jump = null;
                            }
                            heading += 1;
                        },
                        .bullet => {
                            ig.igBullet();
                            ig.igTextUnformatted(text.ptr);
                        },
                        .paragraph => ig.igTextUnformatted(text.ptr),
                    }
                    self.help_blocks_drawn += 1;
                }
            }
            ig.igEndChild();
        }
        ig.igEnd();
        if (!open) self.help_open = false;
    }

    /// The MFC Tools menu's Spelling, enabled as it was (a text and a
    /// speller; its tooltip says why not), and the options it had in the
    /// registry.
    fn drawToolsMenu(self: *Ui) void {
        if (!self.beginMenu("elk.menu.tools")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx(l10n.id("elk.tools.spelling"), "F7", false, self.canSpell())) self.openSpelling();
        const reason = self.spelling().reason();
        if (reason.len != 0 and ig.igIsItemHovered(ig.ImGuiHoveredFlags_AllowWhenDisabled)) {
            var buffer: [1024:0]u8 = undefined;
            const text = std.mem.printSentinel(&buffer, "{s}", .{reason}, 0) catch "";
            if (ig.igBeginTooltip()) {
                ig.igTextUnformatted(text.ptr);
                ig.igEndTooltip();
            }
        }
        if (ig.igMenuItem(l10n.id("elk.tools.spelling_options"))) self.openSpellOptions();
        ig.igSeparator();
        self.drawLanguage();
    }

    /// Tools > Spelling's window: the walk's current word with Change to,
    /// Ignore, Ignore All, Change and Change All, as the MFC ELK stopped at
    /// each word in turn; below it each misspelt word of the translation,
    /// its place and suggestions, and Add to dictionary.
    fn drawSpellingWindow(self: *Ui) void {
        self.spell_drawn = false;
        if (!self.spell_open) return;
        if (!self.session.isOpen()) return self.closeSpelling();
        const font = ig.igGetFontSize();
        ig.igSetNextWindowSize(.{ .x = font * 36, .y = font * 24 }, ig.ImGuiCond_Appearing);
        var open = true;
        if (ig.igBegin(l10n.id("elk.spell.title"), &open, ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.spell_drawn = true;
            var line: [1024:0]u8 = undefined;
            const state = self.spelling();
            const heading = if (state.available())
                l10n.formatZ(&line, "elk.spell.language", .{state.languageName()}) orelse ""
            else
                std.mem.printSentinel(&line, "{s}", .{state.reason()}, 0) catch "";
            ig.igTextUnformatted(heading.ptr);
            if (self.spellStale()) ig.igTextUnformatted(l10n.t("elk.spell.stale"));
            self.drawSpellWalk(&line);
            if (self.spell_results.len == 0) {
                ig.igTextUnformatted(l10n.t("elk.spell.none"));
            } else if (ig.igBeginTableEx("words", 4, ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_ScrollY, .{ .x = 0, .y = -ig.igGetFrameHeightWithSpacing() }, 0)) {
                ig.igTableSetupScrollFreeze(0, 1);
                ig.igTableSetupColumnEx(l10n.id("elk.spell.word"), ig.ImGuiTableColumnFlags_WidthFixed, font * 8, 0);
                ig.igTableSetupColumnEx(l10n.id("elk.spell.at"), ig.ImGuiTableColumnFlags_WidthFixed, font * 3, 0);
                ig.igTableSetupColumnEx(l10n.id("elk.spell.suggestions"), ig.ImGuiTableColumnFlags_WidthStretch, 0, 0);
                ig.igTableSetupColumnEx("", ig.ImGuiTableColumnFlags_WidthFixed, font * 9, 0);
                ig.igTableHeadersRow();
                var added: ?usize = null;
                for (self.spell_results, 0..) |result, index| {
                    ig.igPushIDInt(@intCast(index));
                    defer ig.igPopID();
                    ig.igTableNextRow();
                    _ = ig.igTableNextColumn();
                    printText(&line, "{s}", .{result.text});
                    _ = ig.igTableNextColumn();
                    printText(&line, "{d}", .{result.word.offset});
                    _ = ig.igTableNextColumn();
                    var writer: std.Io.Writer = .fixed(line[0 .. line.len - 1]);
                    for (result.suggestions, 0..) |suggestion, at| writer.print("{s}{s}", .{ if (at == 0) "" else ", ", suggestion }) catch break;
                    line[writer.end] = 0;
                    ig.igTextUnformatted(&line);
                    _ = ig.igTableNextColumn();
                    if (ig.igButton(l10n.id("elk.spell.add"))) added = index;
                }
                ig.igEndTable();
                // After the table: adding checks again, which frees the rows.
                if (added) |index| self.addToDictionary(index);
            }
            ig.igBeginDisabled(!self.canSpell());
            if (ig.igButton(l10n.id("elk.spell.check_again"))) self.checkAgain();
            ig.igEndDisabled();
            ig.igSameLine();
            if (ig.igButton(l10n.id("elk.common.close"))) open = false;
        }
        ig.igEnd();
        if (!open) self.closeSpelling();
    }

    /// The walk's part of the Spelling window. A suggestion clicked fills
    /// Change to; the buttons act on the current word.
    fn drawSpellWalk(self: *Ui, line: *[1024:0]u8) void {
        const index = self.currentMisspelling() orelse {
            if (!self.spellStale() and self.spell_results.len != 0) ig.igTextUnformatted(l10n.t("elk.spell.walk_done"));
            ig.igSeparator();
            return;
        };
        const result = self.spell_results[index];
        printKey(line, "elk.spell.not_in_dictionary", .{ result.text, result.word.offset });
        ig.igTextUnformatted(l10n.t("elk.spell.change_to"));
        ig.igSameLine();
        ig.igSetNextItemWidth(ig.igGetFontSize() * 14);
        _ = ig.igInputText("##change", &self.spell_change_buffer, self.spell_change_buffer.len, 0);
        for (result.suggestions[0..@min(result.suggestions.len, 6)], 0..) |suggestion, at| {
            ig.igPushIDInt(@intCast(at));
            defer ig.igPopID();
            if (at != 0) ig.igSameLine();
            const label = std.mem.printSentinel(line, "{s}", .{suggestion}, 0) catch continue;
            if (ig.igSmallButton(label.ptr)) self.setChange(suggestion);
        }
        // Acted on after the buttons: each checks again, which frees `result`.
        const actions = [_]struct { label: [*:0]const u8, act: *const fn (*Ui) void }{
            .{ .label = l10n.id("elk.spell.ignore"), .act = ignoreWord },
            .{ .label = l10n.id("elk.spell.ignore_all"), .act = ignoreAll },
            .{ .label = l10n.id("elk.spell.change"), .act = changeWord },
            .{ .label = l10n.id("elk.spell.change_all"), .act = changeAll },
        };
        var pressed: ?*const fn (*Ui) void = null;
        for (actions, 0..) |action, at| {
            if (at != 0) ig.igSameLine();
            if (ig.igButton(action.label)) pressed = action.act;
        }
        ig.igSeparator();
        if (pressed) |act| act(self);
    }

    /// Tools > Spelling Options...: the dictionary folder (Enter or Use
    /// takes it; empty is `dictionaries` beside ELK), the language combo
    /// of the dictionaries found there, and why spell check is off.
    fn drawSpellOptions(self: *Ui) void {
        self.spell_options_drawn = false;
        if (!self.spell_options_open) return;
        const font = ig.igGetFontSize();
        ig.igSetNextWindowSize(.{ .x = font * 36, .y = 0 }, ig.ImGuiCond_Appearing);
        var open = true;
        if (ig.igBegin(l10n.id("elk.spell_options.title"), &open, ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.spell_options_drawn = true;
            const state = self.spelling();
            var line: [std.Io.Dir.max_path_bytes + 64:0]u8 = undefined;
            ig.igTextUnformatted(l10n.t("elk.spell_options.folder"));
            ig.igSetNextItemWidth(font * 26);
            const entered = ig.igInputText("##folder", &self.spell_folder_buffer, self.spell_folder_buffer.len, ig.ImGuiInputTextFlags_EnterReturnsTrue);
            ig.igSameLine();
            if (ig.igButton(l10n.id("elk.spell_options.use")) or entered) self.setSpellFolder(std.mem.sliceTo(&self.spell_folder_buffer, 0));
            var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            printKey(&line, "elk.spell_options.reading", .{self.spellFolder(&folder_buffer)});
            ig.igTextUnformatted(l10n.t("elk.spell_options.language"));
            ig.igSameLine();
            ig.igSetNextItemWidth(font * 12);
            const current = std.mem.printSentinel(&line, "{s}", .{state.languageName()}, 0) catch "";
            ig.igBeginDisabled(state.languages.len == 0);
            if (ig.igBeginCombo("##language", current.ptr, 0)) {
                var chosen: ?usize = null;
                for (state.languages, 0..) |name, index| {
                    var label: [256:0]u8 = undefined;
                    const text = std.mem.printSentinel(&label, "{s}", .{name}, 0) catch continue;
                    if (ig.igSelectableEx(text.ptr, state.language == index, 0, .{ .x = 0, .y = 0 })) chosen = index;
                }
                ig.igEndCombo();
                // Copied out: loading the language frees the list.
                if (chosen) |index| if (state.languages[index].len <= settings_mod.max_path) {
                    var name: [settings_mod.max_path]u8 = undefined;
                    const picked = state.languages[index];
                    @memcpy(name[0..picked.len], picked);
                    self.setSpellLanguage(name[0..picked.len]);
                };
            }
            ig.igEndDisabled();
            const reason = state.reason();
            if (reason.len != 0) {
                ig.igPushTextWrapPos(0);
                printText(&line, "{s}", .{reason});
                ig.igPopTextWrapPos();
            }
            if (ig.igButton(l10n.id("elk.common.close"))) open = false;
        }
        ig.igEnd();
        if (!open) self.spell_options_open = false;
    }

    /// The kit's language picker, in the Tools menu. The choice is applied between frames
    /// (`applyPendingLanguage`), the font stack and the tables not changing under a frame in progress.
    fn drawLanguage(self: *Ui) void {
        if (language_picker.draw(kit.settings.uiLanguage(self.settings))) |choice| {
            const len = @min(choice.value.len, self.language_choice.len);
            @memcpy(self.language_choice[0..len], choice.value[0..len]);
            self.language_choice_len = len;
        }
    }

    /// Between two frames: the picker's choice written to `ui_language` in elk.cfg, the fonts
    /// rebuilt for it and the tables switched.
    pub fn applyPendingLanguage(self: *Ui) void {
        if (self.language_choice_len == 0) return;
        const setting = self.language_choice[0..self.language_choice_len];
        self.language_choice_len = 0;
        self.settings.ui_language_storage.set(setting);
        self.saveSettings();
        const code = chooseLanguage(self.settings, !self.os_dialogs);
        _ = kit.fonts.reload(code) catch |err| {
            std.debug.print("elk: the fonts did not reload for {s} ({s})\n", .{ code, @errorName(err) });
            return;
        };
        l10n.setLanguage(code);
    }

    /// The MFC Edit menu's Find; Undo and the clipboard commands are the
    /// edit boxes' own keys.
    fn drawEditMenu(self: *Ui) void {
        if (!self.beginMenu("elk.menu.edit")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx(l10n.id("elk.edit.find"), "Ctrl+F", false, self.session.isOpen())) self.openFind();
    }

    fn drawViewMenu(self: *Ui) void {
        if (!self.beginMenu("elk.menu.view")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx(l10n.id("elk.view.tree"), null, self.settings.show_tree, true)) self.toggleTree();
        if (ig.igMenuItemEx(l10n.id("elk.view.statistic"), null, false, self.session.isOpen())) self.openStatistics();
        if (self.short) return;
        ig.igSeparator();
        if (ig.igMenuItemEx(l10n.id("elk.view.status_bar"), null, self.settings.show_status_bar, true)) self.toggleStatusBar();
    }

    /// The Find window, laid out as the Windows Find dialog the MFC ELK
    /// used: Find what, Find Next, Match whole word only, Match case, the
    /// direction and Cancel. Enter in the box is Find Next.
    fn drawFindWindow(self: *Ui) void {
        self.find_drawn = false;
        if (!self.find_open) return;
        if (!self.session.isOpen()) return self.closeFind();
        const viewport = ig.igGetMainViewport();
        const centre: ig.ImVec2 = .{ .x = viewport.*.WorkPos.x + viewport.*.WorkSize.x * 0.5, .y = viewport.*.WorkPos.y + viewport.*.WorkSize.y * 0.3 };
        ig.igSetNextWindowPosEx(centre, ig.ImGuiCond_Appearing, .{ .x = 0.5, .y = 0.5 });
        var open = true;
        if (ig.igBegin(l10n.id("elk.find.title"), &open, ig.ImGuiWindowFlags_AlwaysAutoResize | ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.find_drawn = true;
            ig.igTextUnformatted(l10n.t("elk.find.what"));
            ig.igSameLine();
            if (self.find_focus) {
                ig.igSetKeyboardFocusHere();
                self.find_focus = false;
            }
            ig.igSetNextItemWidth(ig.igGetFontSize() * 18);
            const entered = ig.igInputText("##find", &self.find_buffer, self.find_buffer.len, ig.ImGuiInputTextFlags_EnterReturnsTrue);
            ig.igSameLine();
            const empty = self.find_buffer[0] == 0;
            ig.igBeginDisabled(empty);
            const pressed = ig.igButton(l10n.id("elk.find.next"));
            ig.igEndDisabled();
            if ((entered or pressed) and !empty) {
                self.findNext();
                self.find_focus = entered;
            }
            var whole_word = self.settings.find_whole_word;
            if (ig.igCheckbox(l10n.id("elk.find.whole_word"), &whole_word)) self.setFindOption(.whole_word, whole_word);
            ig.igSameLine();
            ig.igTextUnformatted(l10n.t("elk.find.direction"));
            ig.igSameLine();
            if (ig.igRadioButton(l10n.id("elk.find.up"), !self.settings.find_down)) self.setFindOption(.down, false);
            ig.igSameLine();
            if (ig.igRadioButton(l10n.id("elk.find.down"), self.settings.find_down)) self.setFindOption(.down, true);
            var match_case = self.settings.find_match_case;
            if (ig.igCheckbox(l10n.id("elk.find.match_case"), &match_case)) self.setFindOption(.match_case, match_case);
            ig.igSameLine();
            if (ig.igButton(l10n.id("elk.common.cancel"))) open = false;
            if (self.find_missed) ig.igTextUnformatted(l10n.t("elk.find.not_found"));
        }
        ig.igEnd();
        if (!open) self.closeFind();
    }

    /// `CStatisticDialog`'s tree as a table: Original Text and Translation,
    /// each with its four states summed over the elements and then each
    /// element ("Name, VERSION: v") with its four states; Items, Words,
    /// Word Symbols and Symbols, the thousands set apart. The two halves
    /// start open, the elements closed. The same four again "at last close"
    /// are the snapshot elk.xml kept (B10), "-" where it has none.
    fn drawStatisticsWindow(self: *Ui) void {
        self.statistics_drawn = false;
        const statistic = self.statistics orelse return;
        if (!self.session.isOpen()) return self.closeStatistics();
        const font = ig.igGetFontSize();
        ig.igSetNextWindowSize(.{ .x = font * 80, .y = font * 24 }, ig.ImGuiCond_Appearing);
        var open = true;
        if (ig.igBegin(l10n.id("elk.statistic.title"), &open, ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.statistics_drawn = true;
            const table_flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollY;
            if (ig.igBeginTableEx("statistic", 9, table_flags, .{ .x = 0, .y = -ig.igGetFrameHeightWithSpacing() }, 0)) {
                ig.igTableSetupScrollFreeze(0, 1);
                ig.igTableSetupColumnEx(l10n.id("elk.statistic.tree"), ig.ImGuiTableColumnFlags_WidthStretch, 0, 0);
                for ([_][*:0]const u8{ l10n.t("elk.statistic.items"), l10n.t("elk.statistic.words"), l10n.t("elk.statistic.word_symbols"), l10n.t("elk.statistic.symbols") }) |heading| ig.igTableSetupColumnEx(heading, ig.ImGuiTableColumnFlags_WidthFixed, font * 6, 0);
                for ([_][*:0]const u8{ l10n.t("elk.statistic.items_last_close"), l10n.t("elk.statistic.words"), l10n.t("elk.statistic.word_symbols"), l10n.t("elk.statistic.symbols") }) |heading| ig.igTableSetupColumnEx(heading, ig.ImGuiTableColumnFlags_WidthFixed, font * 6, 0);
                ig.igTableHeadersRow();
                const tree = &self.session.tree.?;
                for ([_]bool{ false, true }) |translation| {
                    ig.igPushIDInt(@intFromBool(translation));
                    defer ig.igPopID();
                    const label = std.mem.span(if (translation) l10n.t("elk.statistic.translation") else l10n.t("elk.statistic.original"));
                    if (tableRow(label, ig.ImGuiTreeNodeFlags_DefaultOpen, null, self.statisticRow(.now, translation, null, null).?, self.statisticRow(.last_close, translation, null, null))) {
                        defer ig.igTreePop();
                        for (std.enums.values(State)) |state| _ = tableRow(stateName(state), ig.ImGuiTreeNodeFlags_Leaf, stateColour(state), self.statisticRow(.now, translation, null, state).?, self.statisticRow(.last_close, translation, null, state));
                        const half = if (translation) statistic.translation else statistic.original;
                        for (0..half.len) |element| {
                            ig.igPushIDInt(@intCast(element));
                            defer ig.igPopID();
                            const root = tree.rootOf(element) orelse continue;
                            if (tableRow(tree.nodes[root].name, 0, null, self.statisticRow(.now, translation, element, null).?, self.statisticRow(.last_close, translation, element, null))) {
                                defer ig.igTreePop();
                                for (std.enums.values(State)) |state| _ = tableRow(stateName(state), ig.ImGuiTreeNodeFlags_Leaf, stateColour(state), self.statisticRow(.now, translation, element, state).?, self.statisticRow(.last_close, translation, element, state));
                            }
                        }
                    }
                }
                ig.igEndTable();
            }
            if (ig.igButton(l10n.id("elk.common.ok"))) open = false;
        }
        ig.igEnd();
        if (!open) self.closeStatistics();
    }

    /// The MFC Browse menu's commands, in its order; each step is enabled
    /// when it reaches a text.
    fn drawBrowseMenu(self: *Ui) void {
        if (!self.beginMenu("elk.menu.browse")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx(l10n.id("elk.browse.collapse"), null, self.settings.collapse_item, true)) self.toggleCollapseItem();
        ig.igSeparator();
        const items = [_]struct { label: [*:0]const u8, keys: ?[*:0]const u8, step: Step }{
            .{ .label = l10n.id("elk.browse.last"), .keys = null, .step = .last },
            .{ .label = l10n.id("elk.browse.next"), .keys = "Ctrl+N", .step = .next },
            .{ .label = l10n.id("elk.browse.previous"), .keys = "Ctrl+P", .step = .previous },
            .{ .label = l10n.id("elk.browse.first"), .keys = null, .step = .first },
        };
        for (items) |item| {
            if (ig.igMenuItemEx(item.label, item.keys, false, self.canBrowse(item.step))) _ = self.browse(item.step);
        }
        ig.igSeparator();
        if (ig.igMenuItemEx(l10n.id("elk.browse.create_filter"), null, false, self.session.isOpen())) self.openFilterDialog();
    }

    /// The extended layout's filter bar (the MFC browse toolbar): the named
    /// filters' combo and the four Browse buttons.
    fn drawFilterBar(self: *Ui) void {
        self.toolbar_drawn = .filter_bar;
        self.drawFilterCombo();
        const buttons = [_]struct { label: [*:0]const u8, step: Step }{
            .{ .label = "|<", .step = .first },
            .{ .label = "<", .step = .previous },
            .{ .label = ">", .step = .next },
            .{ .label = ">|", .step = .last },
        };
        for (buttons) |button| {
            ig.igSameLine();
            ig.igBeginDisabled(!self.canBrowse(button.step));
            if (ig.igButton(button.label)) _ = self.browse(button.step);
            ig.igEndDisabled();
        }
        ig.igSameLine();
        var collapse = self.settings.collapse_item;
        if (ig.igCheckbox(l10n.id("elk.browse.collapse"), &collapse)) self.toggleCollapseItem();
        ig.igSameLine();
        if (ig.igButton(l10n.id("elk.browse.create_filter"))) self.openFilterDialog();
    }

    /// The filter combo of either layout's toolbar.
    fn drawFilterCombo(self: *Ui) void {
        var name_buffer: [settings_mod.max_path:0]u8 = undefined;
        const current = std.mem.printSentinel(&name_buffer, "{s}", .{self.currentFilterName()}, 0) catch "";
        var preview_buffer: [settings_mod.max_path:0]u8 = undefined;
        ig.igTextUnformatted(l10n.t("elk.filter.label"));
        ig.igSameLine();
        ig.igSetNextItemWidth(ig.igGetFontSize() * 12);
        if (ig.igBeginCombo("##filter", filterLabel(&preview_buffer, current), 0)) {
            defer ig.igEndCombo();
            var label_buffer: [settings_mod.max_path:0]u8 = undefined;
            for (0..self.filterCount()) |index| {
                const filter = self.filterAt(index);
                if (ig.igSelectableEx(filterLabel(&label_buffer, filter.name), std.mem.eql(u8, filter.name, current), 0, .{ .x = 0, .y = 0 })) {
                    // The name borrows the settings it is written into.
                    var copy: [settings_mod.max_path]u8 = undefined;
                    @memcpy(copy[0..filter.name.len], filter.name);
                    self.chooseFilter(copy[0..filter.name.len]);
                }
            }
        }
    }

    fn drawFileMenu(self: *Ui, window: *window_mod.Window) void {
        if (!self.beginMenu("elk.menu.file")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx(l10n.id("elk.file.open"), "Ctrl+O", false, !open_slot.waiting())) self.requestOpen(window);
        const settings_count = self.settings.recentCount();
        if (ig.igBeginMenuEx(l10n.id("elk.file.recent"), settings_count != 0)) {
            defer ig.igEndMenu();
            var path_buffer: [settings_mod.max_path:0]u8 = undefined;
            var index: usize = 0;
            while (index < settings_count) : (index += 1) {
                const recent = self.settings.recentAt(index);
                const label = std.mem.printSentinel(&path_buffer, "{s}", .{recent}, 0) catch continue;
                if (ig.igMenuItem(label.ptr)) {
                    // The list changes as it opens; open a copy.
                    var copy: [settings_mod.max_path]u8 = undefined;
                    @memcpy(copy[0..recent.len], recent);
                    self.openDatabase(copy[0..recent.len]);
                    break;
                }
            }
        }
        const open = self.session.isOpen();
        if (ig.igMenuItemEx(l10n.id("elk.file.save"), "Ctrl+S", false, open)) self.session.save() catch {};
        if (ig.igMenuItemEx(l10n.id("elk.file.close"), null, false, open)) self.session.close() catch {};
        self.noteRect("elk.file.close");
        ig.igSeparator();
        if (ig.igMenuItemEx(l10n.id("elk.file.import_game"), null, false, self.canImportPackage())) self.requestPackage(window, .game);
        self.noteRect("elk.file.import_game");
        if (ig.igMenuItemEx(l10n.id("elk.file.import_pak"), null, false, self.canImportPackage())) self.requestPackage(window, .pak);
        self.noteRect("elk.file.import_pak");
        const sheet_free = self.canExport() and !sheet_slot.waiting();
        if (ig.igMenuItemEx(l10n.id("elk.file.import_sheet"), null, false, sheet_free)) self.requestSheet(window, .import);
        self.noteRect("elk.file.import_sheet");
        if (ig.igMenuItemEx(l10n.id("elk.file.export_sheet"), null, false, sheet_free)) self.requestSheet(window, .@"export");
        self.noteRect("elk.file.export_sheet");
        ig.igSeparator();
        if (ig.igMenuItemEx(l10n.id("elk.file.export_mod"), null, false, self.canExport())) self.requestExportMod(false);
        self.noteRect("elk.file.export_mod");
        if (ig.igMenuItemEx(l10n.id("elk.file.install"), null, false, self.canExport())) self.requestInstall();
        self.noteRect("elk.file.install");
        if (ig.igMenuItemEx(l10n.id("elk.file.run_game"), null, false, self.canRunGame())) self.requestRunGame();
        self.noteRect("elk.file.run_game");
        ig.igSeparator();
        if (ig.igMenuItem(l10n.id("elk.file.exit"))) self.requestExit();
        self.noteRect("elk.file.exit");
    }

    /// The tree as rows: the open part of it, clipped to what the child
    /// window shows, each row in its node's state colour.
    fn drawTree(self: *Ui) void {
        self.syncExpanded() catch return;
        const tree = &self.session.tree.?;
        self.visible.clearRetainingCapacity();
        var scroll_row: ?usize = null;
        var index: u32 = 0;
        while (index < tree.nodes.len) {
            if (self.scroll_to == index) scroll_row = self.visible.items.len;
            self.visible.append(self.gpa, index) catch return;
            index = if (!tree.nodes[index].isText() and self.expanded.items[index]) index + 1 else tree.nodes[index].end;
        }
        const selected_node: ?u32 = if (self.session.selected) |text| tree.node_of_text[text] else null;
        self.marks.resize(self.gpa, tree.nodes.len) catch return;
        tree.markMatches(self.currentFilter(), self.marks.items);
        const indent = ig.igGetTreeNodeToLabelSpacing();

        var clipper = std.mem.zeroes(ig.ImGuiListClipper);
        ig.ImGuiListClipper_Begin(&clipper, @intCast(self.visible.items.len), -1.0);
        defer ig.ImGuiListClipper_End(&clipper);
        if (scroll_row) |row| ig.ImGuiListClipper_IncludeItemByIndex(&clipper, @intCast(row));
        var label: [512:0]u8 = undefined;
        while (ig.ImGuiListClipper_Step(&clipper)) {
            var row: usize = @intCast(clipper.DisplayStart);
            while (row < clipper.DisplayEnd) : (row += 1) {
                const at = self.visible.items[row];
                const node = tree.nodes[at];
                const text = std.mem.printSentinel(&label, "{s}", .{node.name}, 0) catch continue;
                ig.igPushIDInt(@intCast(at));
                defer ig.igPopID();
                const offset = indent * @as(f32, @floatFromInt(node.depth));
                if (offset > 0) ig.igIndentEx(offset);
                const colour = stateColour(node.state);
                const dimmed = !self.marks.items[at];
                ig.igPushStyleColor(ig.ImGuiCol_Text, if (dimmed) dim(colour) else colour);
                var flags: ig.ImGuiTreeNodeFlags = ig.ImGuiTreeNodeFlags_NoTreePushOnOpen | ig.ImGuiTreeNodeFlags_SpanAvailWidth;
                if (selected_node == at) flags |= ig.ImGuiTreeNodeFlags_Selected;
                if (node.isText()) {
                    _ = ig.igTreeNodeEx(text.ptr, flags | ig.ImGuiTreeNodeFlags_Leaf);
                    if (ig.igIsItemClicked()) self.clickRow(at);
                } else {
                    ig.igSetNextItemOpen(self.expanded.items[at], ig.ImGuiCond_Always);
                    self.expanded.items[at] = ig.igTreeNodeEx(text.ptr, flags | ig.ImGuiTreeNodeFlags_OpenOnArrow | ig.ImGuiTreeNodeFlags_OpenOnDoubleClick);
                }
                ig.igPopStyleColor();
                if (offset > 0) ig.igUnindentEx(offset);
                if (self.scroll_to == at) {
                    ig.igSetScrollHereY(0.5);
                    self.scroll_to = null;
                }
                self.drawn.append(self.gpa, .{ .node = at, .colour = colour, .dimmed = dimmed }) catch {};
                self.rows_drawn += 1;
            }
        }
    }

    fn drawText(self: *Ui) void {
        const index = self.session.selected orelse {
            ig.igTextUnformatted(l10n.t("elk.text.select"));
            return;
        };
        const third = (ig.igGetContentRegionAvail().y - 3 * ig.igGetTextLineHeightWithSpacing() - ig.igGetFrameHeightWithSpacing()) / 3;
        // The text's own ID, so an edit box active on the previous text does
        // not carry its buffer over to this one.
        ig.igPushIDInt(@intCast(index));
        defer ig.igPopID();
        ig.igTextUnformatted(l10n.t("elk.text.original"));
        if (self.session.original.storage(self.gpa)) |storage| {
            _ = ig.igInputTextMultilineEx("##original", storage.ptr, storage.len, .{ .x = -1, .y = third }, ig.ImGuiInputTextFlags_ReadOnly, null, null);
        } else |_| {}
        // The MFC view's "Item description:", read-only with select and
        // copy; where Find shows a hit in a description.
        ig.igTextUnformatted(l10n.t("elk.text.description"));
        if (self.session.description.storage(self.gpa)) |storage| {
            _ = ig.igInputTextMultilineEx("##description", storage.ptr, storage.len, .{ .x = -1, .y = third }, ig.ImGuiInputTextFlags_ReadOnly, null, null);
            self.description_drawn = true;
        } else |_| {}
        self.drawStateButtons();
        ig.igTextUnformatted(l10n.t("elk.text.translation"));
        // Held while the dialog is open, whose modal the focus would close.
        if (self.focus_translation and !self.modalOpen()) {
            ig.igSetKeyboardFocusHere();
            self.focus_translation = false;
        }
        ig.igPushIDInt(@bitCast(self.translation_generation));
        defer ig.igPopID();
        if (self.session.translation.storage(self.gpa)) |storage| {
            _ = ig.igInputTextMultilineEx("##translation", storage.ptr, storage.len, .{ .x = -1, .y = -1 }, ig.ImGuiInputTextFlags_CallbackResize, resizeTranslation, self);
            self.session.translation.settle();
        } else |_| {}
    }

    /// The edit view's state radio buttons. Outdated shows when an update
    /// set it but cannot be chosen.
    fn drawStateButtons(self: *Ui) void {
        ig.igTextUnformatted(l10n.t("elk.text.state"));
        for ([_]State{ .not_translated, .outdated, .translated, .approved }) |state| {
            ig.igSameLine();
            ig.igBeginDisabled(state == .outdated);
            ig.igPushStyleColor(ig.ImGuiCol_Text, stateColour(state));
            if (ig.igRadioButton(stateId(state), self.session.state == state)) self.pressState(state);
            ig.igPopStyleColor();
            ig.igEndDisabled();
        }
        self.state_buttons_drawn = true;
    }

    fn drawStatus(self: *Ui) void {
        var buffer: [1536:0]u8 = undefined;
        var writer: std.Io.Writer = .fixed(buffer[0 .. buffer.len - 1]);
        var part: [256]u8 = undefined;
        const failure = self.session.failureText();
        // The last export's or launch's summary closes the line.
        const notice_text = self.notice();
        if (failure.len != 0) {
            writer.writeAll(failure) catch {};
            if (self.exit_refused) {
                writer.writeAll(" | ") catch {};
                writer.writeAll(std.mem.span(l10n.t("elk.status.exit_again"))) catch {};
            }
        } else {
            writer.writeAll(l10n.format(&part, "elk.status.count", .{self.session.textCount()}) catch "") catch {};
            if (self.session.selected) |index| {
                writer.print(" | {s} | {s}", .{ self.session.textAt(index).key, stateName(self.session.state) }) catch {};
                if (self.session.dirty()) writer.print(" | {s}", .{std.mem.span(l10n.t("elk.status.unsaved"))}) catch {};
                // The MFC ELK selected the hit in its edit box; this names where it is.
                if (self.session.last_hit) |hit| if (self.session.selected == hit.text) {
                    writer.print(" | {s}", .{l10n.format(&part, "elk.status.found", .{ fieldName(hit.field), hit.offset }) catch ""}) catch {};
                };
            }
            if (notice_text.len != 0) writer.print(" | {s}", .{notice_text}) catch {};
        }
        buffer[writer.end] = 0;
        ig.igTextUnformatted(&buffer);
    }

    fn drawFailure(self: *Ui) void {
        const failure = self.session.failureText();
        if (failure.len == 0) return;
        var buffer: [512:0]u8 = undefined;
        const text = std.mem.printSentinel(&buffer, "{s}", .{failure}, 0) catch return;
        ig.igTextUnformatted(text.ptr);
    }
};

/// The state's name in the current language.
pub fn stateName(state: session_mod.State) [:0]const u8 {
    return std.mem.span(switch (state) {
        .not_translated => l10n.t("elk.state.not_translated"),
        .outdated => l10n.t("elk.state.outdated"),
        .translated => l10n.t("elk.state.translated"),
        .approved => l10n.t("elk.state.approved"),
    });
}

/// The state's name as a widget label, whose ID stays when the language changes.
fn stateId(state: session_mod.State) [*:0]const u8 {
    return switch (state) {
        .not_translated => l10n.id("elk.state.not_translated"),
        .outdated => l10n.id("elk.state.outdated"),
        .translated => l10n.id("elk.state.translated"),
        .approved => l10n.id("elk.state.approved"),
    };
}

/// A filter's name for display: the five built-in filters (their names are data, kept as the MFC
/// ELK wrote them) in the current language, a filter the translator made as typed.
fn filterLabel(buffer: *[settings_mod.max_path:0]u8, name: []const u8) [*:0]const u8 {
    if (std.mem.eql(u8, name, "All")) return l10n.t("elk.filter.all");
    if (std.mem.eql(u8, name, "Not Translated")) return l10n.t("elk.filter.not_translated");
    if (std.mem.eql(u8, name, "Outdated")) return l10n.t("elk.filter.outdated");
    if (std.mem.eql(u8, name, "Translated")) return l10n.t("elk.filter.translated");
    if (std.mem.eql(u8, name, "Approved")) return l10n.t("elk.filter.approved");
    const text = std.mem.printSentinel(buffer, "{s}", .{name}, 0) catch return "";
    return text.ptr;
}

/// A find hit's field as the status line names it.
fn fieldName(field: elk_core.search.Field) [:0]const u8 {
    return std.mem.span(switch (field) {
        .original => l10n.t("elk.field.original"),
        .description => l10n.t("elk.field.description"),
        .translation => l10n.t("elk.field.translation"),
    });
}

/// The four states' colours (07-CONTEXT: red, yellow, green, blue), light
/// enough to read on the dark theme, as ImGui's packed ABGR.
pub fn stateColour(state: State) u32 {
    return switch (state) {
        .not_translated => rgb(235, 80, 80),
        .outdated => rgb(230, 200, 60),
        .translated => rgb(90, 200, 90),
        .approved => rgb(100, 155, 245),
    };
}

/// A row of the statistic table: a tree node in the first column, the four
/// counts right-aligned after it, then the last close's four or dashes.
/// Returns whether a node with children is open (the caller pops it then).
fn tableRow(label: []const u8, flags: ig.ImGuiTreeNodeFlags, colour: ?u32, counts: Counts, last_close: ?Counts) bool {
    ig.igTableNextRow();
    _ = ig.igTableNextColumn();
    var label_buffer: [512:0]u8 = undefined;
    const text = std.mem.printSentinel(&label_buffer, "{s}", .{label}, 0) catch "";
    if (colour) |chosen| ig.igPushStyleColor(ig.ImGuiCol_Text, chosen);
    const leaf = flags & ig.ImGuiTreeNodeFlags_Leaf != 0;
    const open = ig.igTreeNodeEx(text.ptr, flags | ig.ImGuiTreeNodeFlags_SpanFullWidth | (if (leaf) ig.ImGuiTreeNodeFlags_NoTreePushOnOpen else 0));
    if (colour != null) ig.igPopStyleColor();
    const then = last_close orelse Counts{};
    const values = [_]i32{ counts.texts, counts.words, counts.word_symbols, counts.symbols, then.texts, then.words, then.word_symbols, then.symbols };
    for (values, 0..) |value, column| {
        _ = ig.igTableNextColumn();
        var number_buffer: [24:0]u8 = undefined;
        const number = if (column >= 4 and last_close == null) "-" else elk_core.stats.formatCount(number_buffer[0 .. number_buffer.len - 1], value);
        if (number.ptr != &number_buffer) @memcpy(number_buffer[0..number.len], number);
        number_buffer[number.len] = 0;
        const width = ig.igCalcTextSize(&number_buffer).x;
        ig.igSetCursorPosX(ig.igGetCursorPosX() + @max(0, ig.igGetContentRegionAvail().x - width));
        ig.igTextUnformatted(&number_buffer);
    }
    return open and !leaf;
}

/// The table code elk.cfg's `ui_language` stands for (the kit's rule, shared by every editor).
pub fn chooseLanguage(settings: *const settings_mod.Settings, scripted: bool) []const u8 {
    return language_picker.chooseLanguage(settings, scripted);
}

/// The longest help block, in bytes, the window draws: a Hindi paragraph is a few thousand bytes.
const help_line_bytes = 6144;

/// A help block as a drawable line: copied, and for Hindi turned into the merged font's
/// cluster characters. Null when it does not fit.
fn helpLine(line: *[help_line_bytes:0]u8, scratch: *[help_line_bytes:0]u8, text: []const u8) ?[:0]const u8 {
    if (text.len >= help_line_bytes) return null;
    if (!std.mem.eql(u8, l10n.language(), "hi")) return std.mem.printSentinel(line, "{s}", .{text}, 0) catch null;
    const copied = std.mem.printSentinel(scratch, "{s}", .{text}, 0) catch return null;
    return kit.fonts.encodeHindi(line, copied) orelse copied;
}

/// An exit code or signal for a message: its digits, or "none".
fn optionalInt(buffer: []u8, value: anytype) []const u8 {
    const number = value orelse return "none";
    return std.fmt.bufPrint(buffer, "{d}", .{number}) catch "none";
}

/// One line of formatted text from the table; an empty line when it does not fit.
fn printKey(buffer: anytype, comptime key: []const u8, args: anytype) void {
    const text: [:0]const u8 = l10n.formatZ(buffer, key, args) orelse "";
    ig.igTextUnformatted(text.ptr);
}

/// One line of formatted text; an empty line when it does not fit.
fn printText(buffer: anytype, comptime format: []const u8, args: anytype) void {
    const text: [:0]const u8 = std.mem.printSentinel(buffer, format, args, 0) catch "";
    ig.igTextUnformatted(text.ptr);
}

fn rgb(r: u32, g: u32, b: u32) u32 {
    return 0xFF00_0000 | (b << 16) | (g << 8) | r;
}

/// A row the current filter does not keep: its state colour at a third of
/// the opacity, still telling its state.
pub fn dim(colour: u32) u32 {
    return (colour & 0x00FF_FFFF) | 0x5500_0000;
}

/// ImGui grows the translation buffer through this when an edit needs more
/// room than it has.
fn resizeTranslation(data: [*c]ig.ImGuiInputTextCallbackData) callconv(.c) c_int {
    const event = &data[0];
    if (event.EventFlag != ig.ImGuiInputTextFlags_CallbackResize) return 0;
    const self: *Ui = @ptrCast(@alignCast(event.UserData));
    if (self.session.translation.grow(self.gpa, @intCast(event.BufSize), @intCast(event.BufTextLen))) |storage| event.Buf = storage.ptr;
    return 0;
}
