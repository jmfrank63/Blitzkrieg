//! The extended layout's minimum for a translator: File and Browse menus,
//! the filter bar, the text tree coloured by state, the selected text's
//! original, its translation and the state buttons, and a status line.
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
//! in ELK's own test profile. Tools > Spelling (F7) lists the misspelt words
//! of the selected text's translation with Hunspell's suggestions and an Add
//! to dictionary button for each; Tools > Spelling Options... picks the
//! dictionary folder and the language. Without a dictionary both say why
//! spell check is off, and Spelling is disabled (spelling.zig).
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

const ig = imgui.c;
pub const Session = session_mod.Session;
const State = session_mod.State;
const Filter = session_mod.Filter;
pub const Step = session_mod.Step;
const Statistic = session_mod.Statistic;
const Counts = elk_core.stats.Counts;
const elk_core = @import("elk_core");

pub const app_title = "Blitzkrieg ELK";

/// File > Open's answer. SDL may call back on another thread or after the
/// window is gone, so it is a global, as the editors' own slots are.
var open_slot: OpenSlot = .{};

const OpenSlot = kit.dialogs.PathSlot(void, settings_mod.max_path);

const open_filters = [_]sdl3.c.SDL_DialogFileFilter{.{ .name = "ELK projects (elk.xml)", .pattern = "xml" }};

/// The export dialog's Browse...: the mod folder the OS dialog chose.
var mod_slot: ModSlot = .{};

const ModSlot = kit.dialogs.PathSlot(void, settings_mod.max_path);
const testlaunch = kit.testlaunch;
const export_popup = "Export to language mod";
const warning_popup = "Install to game";

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
    /// The last export's or launch's summary, shown in the status line.
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

    /// `colour` is the row's state colour; a dimmed row was drawn fainter.
    pub const Drawn = struct { node: u32, colour: u32, dimmed: bool };

    pub fn deinit(self: *Ui) void {
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
    /// written never holds the window open.
    pub fn requestExit(self: *Ui) void {
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
        } else return false;
        return true;
    }

    fn takeOpen(self: *Ui) void {
        switch (open_slot.take() orelse return) {
            .path => |chosen| self.openDatabase(chosen.path),
            .cancelled => {},
            .failed => |reason| self.session.fail("the file dialog failed: {s}", .{reason}),
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
        return self.settings.filterCount();
    }

    pub fn filterAt(self: *const Ui, index: usize) Filter {
        return self.settings.filterAt(index);
    }

    pub fn currentFilterName(self: *const Ui) []const u8 {
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
        self.settings.setCurrentFilter(name);
        self.saveSettings();
    }

    /// Browse > Create Filter...: the dialog opens on a copy of the filters.
    pub fn openFilterDialog(self: *Ui) void {
        if (self.filter_dialog != null) return;
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
        self.settings.collapse_item = !self.settings.collapse_item;
        self.saveSettings();
    }

    /// A Browse command: the text the step reaches through the current
    /// filter, selected and shown, the translation box then taking the
    /// keyboard as the MFC input view took the focus. False when no text is
    /// reached or the text being left could not be saved.
    pub fn browse(self: *Ui, step: Step) bool {
        const left: ?u32 = if (self.session.selected) |text| self.session.tree.?.node_of_text[text] else null;
        const moved = self.session.browse(step, self.currentFilter()) catch return false;
        if (!moved) return false;
        if (self.settings.collapse_item) if (left) |node| self.collapseElement(node);
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
    /// in elk.cfg at once. The hit's text is selected and shown as a Browse
    /// step shows it, Collapse Item closing the element it leaves (FindItem
    /// called `DeselectItem` too). Null when nothing is found.
    pub fn findNext(self: *Ui) ?session_mod.Hit {
        const text = std.mem.sliceTo(&self.find_buffer, 0);
        self.settings.setFindText(text);
        self.saveSettings();
        const left: ?u32 = if (self.session.selected) |index| self.session.tree.?.node_of_text[index] else null;
        const hit = (self.session.findNext(.{
            .text = text,
            .down = self.settings.find_down,
            .match_case = self.settings.find_match_case,
            .whole_word = self.settings.find_whole_word,
        }) catch null) orelse {
            self.find_missed = true;
            return null;
        };
        self.find_missed = false;
        const reached = self.session.tree.?.node_of_text[hit.text];
        if (self.settings.collapse_item) if (left) |node| if (node != reached) self.collapseElement(node);
        self.reveal(reached);
        return hit;
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

    /// One row of the statistic window: a half's total (no element, no
    /// state), a state summed over the elements, an element's total or one
    /// state of an element. Null when the window is closed or there is no
    /// such element.
    pub fn statisticRow(self: *const Ui, translation: bool, element: ?usize, state: ?State) ?Counts {
        const statistic = self.statistics orelse return null;
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
            self.session.fail("spell check ran out of memory", .{});
            return;
        };
        self.spell_arena = arena;
        self.spell_results = results;
        self.spell_text = index;
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
            self.session.fail("the user dictionary could not be written ({s})", .{@errorName(err)});
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
            self.session.fail("the game folder is unknown (game_folder in elk.cfg)", .{});
            return null;
        };
    }

    /// The export commands need an open database; Run Game also no game of
    /// its own still running.
    pub fn canExport(self: *const Ui) bool {
        return self.session.isOpen();
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
            .failed => |reason| self.session.fail("the folder dialog failed: {s}", .{reason}),
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
    /// in the game folder, written by the core; the summary names the entry
    /// count. False when nothing was written, the reason in the status line.
    pub fn exportToMod(self: *Ui) bool {
        self.export_failed = true;
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = self.knownGameFolder(&game_buffer) orelse return false;
        const typed = std.mem.sliceTo(&self.export_name, 0);
        const name = session_mod.modName(game, typed) orelse {
            self.session.fail("\"{s}\" is neither a mod's name nor a folder in {s}{c}mods", .{ typed, game, std.fs.path.sep });
            return false;
        };
        const exported = self.session.exportPak(game, .{ .mod = name }) catch return false;
        defer self.gpa.free(exported.path);
        @memcpy(self.played_mod[0..name.len], name);
        self.played_mod_len = name.len;
        self.export_failed = false;
        self.notify("exported {d} texts to {s}", .{ exported.entries, exported.path });
        if (self.export_then_run) self.launchGame();
        return true;
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
        const exported = self.session.exportPak(game, .game) catch return false;
        defer self.gpa.free(exported.path);
        self.notify("installed {d} texts to {s}", .{ exported.entries, exported.path });
        return true;
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
        const exported = self.session.exportPak(game, .{ .mod = self.played_mod[0..self.played_mod_len] }) catch return;
        self.gpa.free(exported.path);
        self.launchGame();
    }

    /// `Game -editor-test -profile=ELKTest -mod=<mod> -windowed` in the game
    /// folder, its output in `game.log` beside elk.cfg (in the game folder
    /// when there is no settings file).
    fn launchGame(self: *Ui) void {
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = self.knownGameFolder(&game_buffer) orelse return;
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const executable = if (builtin.os.tag == .windows) "Game.exe" else "Game";
        const game_path = std.fmt.bufPrint(&path_buffer, "{s}{c}{s}", .{ game, std.fs.path.sep, executable }) catch return self.session.fail("the game path is too long", .{});
        const log_folder = std.fs.path.dirname(self.settings_path) orelse game;
        const log = std.fmt.bufPrint(&self.game_log, "{s}{c}{s}", .{ log_folder, std.fs.path.sep, if (self.settings_path.len != 0) "game.log" else "elk-game.log" }) catch return self.session.fail("the game log path is too long", .{});
        self.game_log_len = log.len;
        const mod = self.played_mod[0..self.played_mod_len];
        const options = session_mod.runGameOptions(game_path, mod, log);
        var storage: testlaunch.ArgvStorage = .{};
        var writer: std.Io.Writer = .fixed(&self.launched);
        for (testlaunch.buildArgv(&storage, options)) |argument| writer.print("{s}\n", .{argument}) catch break;
        self.launched_len = writer.buffered().len;
        self.game_exit = null;
        self.running = testlaunch.start(self.gpa, self.io, self.environ, options) catch |err| {
            self.session.fail("{s} did not start: {s}", .{ game_path, @errorName(err) });
            return;
        };
        self.notify("Run Game: -mod={s} in the {s} profile (log {s})", .{ mod, session_mod.game_profile, log });
    }

    /// The game Run Game started: a failed exit is reported with its log.
    fn pollGame(self: *Ui) void {
        const running = if (self.running) |*running| running else return;
        const exit = running.poll(self.io) orelse return;
        self.running = null;
        self.game_exit = exit;
        switch (testlaunch.describe(exit)) {
            .clean => {},
            .early_failure, .failure => self.session.fail("the Game ended with a failure (exit {?d}, signal {?d}); its output is in {s}", .{ exit.code, exit.signal, self.game_log[0..self.game_log_len] }),
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

    fn notify(self: *Ui, comptime format: []const u8, args: anytype) void {
        const written = std.fmt.bufPrint(&self.notice_storage, format, args) catch {
            self.notice_len = self.notice_storage.len;
            return;
        };
        self.notice_len = written.len;
    }

    /// A dialog of ELK's own is up: the translation box must not take the
    /// focus, which would close its modal.
    fn modalOpen(self: *const Ui) bool {
        return self.filter_dialog != null or self.export_open or self.install_warning != null;
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
        const text = if (opened.len == 0)
            std.mem.printSentinel(&self.title_storage, "{s}", .{app_title}, 0)
        else
            std.mem.printSentinel(&self.title_storage, "{s} - {s}", .{ std.fs.path.basename(opened), app_title }, 0);
        return text catch app_title;
    }

    pub fn draw(self: *Ui, window: *window_mod.Window) void {
        self.takeOpen();
        self.takeModFolder();
        self.pollGame();
        window.setTitle(self.title().ptr);
        if (ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_S, ig.ImGuiInputFlags_RouteGlobal)) self.session.save() catch {};
        if (ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_O, ig.ImGuiInputFlags_RouteGlobal)) self.requestOpen(window);
        if (ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_F, ig.ImGuiInputFlags_RouteGlobal)) self.openFind();
        if (ig.igShortcut(ig.ImGuiKey_F7, ig.ImGuiInputFlags_RouteGlobal)) self.openSpelling();
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

        const viewport = ig.igGetMainViewport();
        ig.igSetNextWindowPos(viewport.*.WorkPos, 0);
        ig.igSetNextWindowSize(viewport.*.WorkSize, 0);
        const flags = ig.ImGuiWindowFlags_NoDecoration | ig.ImGuiWindowFlags_NoMove | ig.ImGuiWindowFlags_NoSavedSettings |
            ig.ImGuiWindowFlags_MenuBar | ig.ImGuiWindowFlags_NoBringToFrontOnFocus;
        _ = ig.igBegin("ELK", null, flags);
        defer ig.igEnd();
        // Last in the frame: a translation box activated in this frame
        // focuses the main window, which would close a modal opened before.
        defer self.drawFilterDialog();
        defer self.drawExportDialog(window);
        defer self.drawInstallWarning();
        defer self.drawStatisticsWindow();
        defer self.drawFindWindow();
        defer self.drawSpellingWindow();
        defer self.drawSpellOptions();
        self.drawMenu(window);
        self.rows_drawn = 0;
        self.drawn.clearRetainingCapacity();
        self.state_buttons_drawn = false;
        if (!self.session.isOpen()) {
            ig.igTextUnformatted("No database is open. Open an elk.xml under File > Open, or pick one under File > Recent.");
            self.drawFailure();
            return;
        }
        self.drawFilterBar();
        const avail = ig.igGetContentRegionAvail();
        const body_height = avail.y - ig.igGetFrameHeightWithSpacing();
        if (ig.igBeginChild("texts", .{ .x = avail.x * 0.35, .y = body_height }, ig.ImGuiChildFlags_Borders, 0)) self.drawTree();
        ig.igEndChild();
        ig.igSameLine();
        if (ig.igBeginChild("text", .{ .x = 0, .y = body_height }, 0, 0)) self.drawText();
        ig.igEndChild();
        self.drawStatus();
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
            _ = ig.igOpenPopup(export_popup, 0);
            self.export_opening = false;
        }
        const font = ig.igGetFontSize();
        ig.igSetNextWindowSize(.{ .x = font * 36, .y = 0 }, ig.ImGuiCond_Appearing);
        if (!ig.igBeginPopupModal(export_popup, null, 0)) {
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
        ig.igTextUnformatted("The game reads a language mod before its loose files and archives, so nothing hides it. Start the game on it with File > Run Game.");
        ig.igPopTextWrapPos();
        ig.igTextUnformatted("Mod (a name, or its folder in the game's mods):");
        if (ig.igIsWindowAppearing()) ig.igSetKeyboardFocusHere();
        ig.igSetNextItemWidth(font * 26);
        ig.igPushIDInt(self.export_name_generation);
        const entered = ig.igInputText("##mod", &self.export_name, self.export_name.len, ig.ImGuiInputTextFlags_EnterReturnsTrue);
        ig.igPopID();
        ig.igSameLine();
        ig.igBeginDisabled(mod_slot.waiting());
        if (ig.igButton("Browse...")) self.requestModFolder(window);
        ig.igEndDisabled();
        const typed = std.mem.sliceTo(&self.export_name, 0);
        const name = session_mod.modName(game, typed);
        const sep = std.fs.path.sep;
        const shown: [:0]const u8 = if (name) |chosen|
            std.mem.printSentinel(&line, "Writes {s}{c}mods{c}{s}{c}data{c}texts.pak", .{ game, sep, sep, chosen, sep, sep }, 0) catch ""
        else if (typed.len != 0)
            std.mem.printSentinel(&line, "Not a mod's name, nor a folder in {s}{c}mods", .{ game, sep }, 0) catch ""
        else
            "";
        ig.igTextUnformatted(shown.ptr);
        if (self.export_failed) {
            const failure = std.mem.printSentinel(&line, "{s}", .{self.session.failureText()}, 0) catch "";
            ig.igTextUnformatted(failure.ptr);
        }
        ig.igSeparator();
        ig.igBeginDisabled(name == null);
        const pressed = ig.igButton(if (self.export_then_run) "Export and Run" else "Export");
        ig.igEndDisabled();
        if ((pressed or entered) and name != null and self.exportToMod()) {
            ig.igCloseCurrentPopup();
            self.export_open = false;
            return;
        }
        ig.igSameLine();
        if (ig.igButton("Cancel") or ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) {
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
            _ = ig.igOpenPopup(warning_popup, 0);
            self.install_opening = false;
        }
        ig.igSetNextWindowSize(.{ .x = ig.igGetFontSize() * 36, .y = 0 }, ig.ImGuiCond_Appearing);
        if (!ig.igBeginPopupModal(warning_popup, null, 0)) return self.closeInstallWarning();
        var choice: ?InstallChoice = null;
        {
            defer ig.igEndPopup();
            self.install_drawn = true;
            choice = self.install_pending;
            self.install_pending = null;
            var line: [std.Io.Dir.max_path_bytes + 512:0]u8 = undefined;
            const text = std.mem.printSentinel(&line, "{d} texts of this project are also loose files in {s}. The game reads loose files before Data/Texts.pak, so it would show those instead of the installed translations.\n\nA language mod is read before the loose files. Export to a language mod instead?", .{ shadow.count, shadow.folder.? }, 0) catch "";
            ig.igPushTextWrapPos(0);
            ig.igTextUnformatted(text.ptr);
            ig.igPopTextWrapPos();
            ig.igSeparator();
            if (ig.igButton("Export to language mod...")) choice = .mod;
            ig.igSetItemDefaultFocus();
            ig.igSameLine();
            if (ig.igButton("Install anyway")) choice = .install;
            ig.igSameLine();
            if (ig.igButton("Cancel")) choice = .cancel;
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
    }

    /// The MFC Tools menu's Spelling, enabled as it was (a text and a
    /// speller; its tooltip says why not), and the options it had in the
    /// registry.
    fn drawToolsMenu(self: *Ui) void {
        if (!ig.igBeginMenu("Tools")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx("Spelling", "F7", false, self.canSpell())) self.openSpelling();
        const reason = self.spelling().reason();
        if (reason.len != 0 and ig.igIsItemHovered(ig.ImGuiHoveredFlags_AllowWhenDisabled)) {
            var buffer: [1024:0]u8 = undefined;
            const text = std.mem.printSentinel(&buffer, "{s}", .{reason}, 0) catch "";
            if (ig.igBeginTooltip()) {
                ig.igTextUnformatted(text.ptr);
                ig.igEndTooltip();
            }
        }
        if (ig.igMenuItem("Spelling Options...")) self.openSpellOptions();
    }

    /// Tools > Spelling's window: each misspelt word of the translation, its
    /// place and suggestions, and Add to dictionary. The MFC ELK stopped at
    /// each word in turn and asked to add it; this lists them all at once.
    fn drawSpellingWindow(self: *Ui) void {
        self.spell_drawn = false;
        if (!self.spell_open) return;
        if (!self.session.isOpen()) return self.closeSpelling();
        const font = ig.igGetFontSize();
        ig.igSetNextWindowSize(.{ .x = font * 36, .y = font * 18 }, ig.ImGuiCond_Appearing);
        var open = true;
        if (ig.igBegin("Spelling", &open, ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.spell_drawn = true;
            var line: [1024:0]u8 = undefined;
            const state = self.spelling();
            const heading = if (state.available())
                std.mem.printSentinel(&line, "Language: {s}", .{state.languageName()}, 0) catch ""
            else
                std.mem.printSentinel(&line, "{s}", .{state.reason()}, 0) catch "";
            ig.igTextUnformatted(heading.ptr);
            if (self.spellStale()) ig.igTextUnformatted("The translation changed since the check; press Check Again.");
            if (self.spell_results.len == 0) {
                ig.igTextUnformatted("No misspelt words in this translation.");
            } else if (ig.igBeginTableEx("words", 4, ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_ScrollY, .{ .x = 0, .y = -ig.igGetFrameHeightWithSpacing() }, 0)) {
                ig.igTableSetupScrollFreeze(0, 1);
                ig.igTableSetupColumnEx("Word", ig.ImGuiTableColumnFlags_WidthFixed, font * 8, 0);
                ig.igTableSetupColumnEx("At", ig.ImGuiTableColumnFlags_WidthFixed, font * 3, 0);
                ig.igTableSetupColumnEx("Suggestions", ig.ImGuiTableColumnFlags_WidthStretch, 0, 0);
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
                    if (ig.igButton("Add to dictionary")) added = index;
                }
                ig.igEndTable();
                // After the table: adding checks again, which frees the rows.
                if (added) |index| self.addToDictionary(index);
            }
            ig.igBeginDisabled(!self.canSpell());
            if (ig.igButton("Check Again")) self.checkSpelling();
            ig.igEndDisabled();
            ig.igSameLine();
            if (ig.igButton("Close")) open = false;
        }
        ig.igEnd();
        if (!open) self.closeSpelling();
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
        if (ig.igBegin("Spelling Options", &open, ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.spell_options_drawn = true;
            const state = self.spelling();
            var line: [std.Io.Dir.max_path_bytes + 64:0]u8 = undefined;
            ig.igTextUnformatted("Dictionary folder (empty: the dictionaries folder beside ELK):");
            ig.igSetNextItemWidth(font * 26);
            const entered = ig.igInputText("##folder", &self.spell_folder_buffer, self.spell_folder_buffer.len, ig.ImGuiInputTextFlags_EnterReturnsTrue);
            ig.igSameLine();
            if (ig.igButton("Use") or entered) self.setSpellFolder(std.mem.sliceTo(&self.spell_folder_buffer, 0));
            var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            printText(&line, "Reading {s}", .{self.spellFolder(&folder_buffer)});
            ig.igTextUnformatted("Language:");
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
            if (ig.igButton("Close")) open = false;
        }
        ig.igEnd();
        if (!open) self.spell_options_open = false;
    }

    /// The MFC Edit menu's Find; Undo and the clipboard commands are the
    /// edit boxes' own keys.
    fn drawEditMenu(self: *Ui) void {
        if (!ig.igBeginMenu("Edit")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx("Find...", "Ctrl+F", false, self.session.isOpen())) self.openFind();
    }

    fn drawViewMenu(self: *Ui) void {
        if (!ig.igBeginMenu("View")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx("ELK Statistic", null, false, self.session.isOpen())) self.openStatistics();
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
        if (ig.igBegin("Find", &open, ig.ImGuiWindowFlags_AlwaysAutoResize | ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.find_drawn = true;
            ig.igTextUnformatted("Find what:");
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
            const pressed = ig.igButton("Find Next");
            ig.igEndDisabled();
            if ((entered or pressed) and !empty) {
                _ = self.findNext();
                self.find_focus = entered;
            }
            var whole_word = self.settings.find_whole_word;
            if (ig.igCheckbox("Match whole word only", &whole_word)) self.setFindOption(.whole_word, whole_word);
            ig.igSameLine();
            ig.igTextUnformatted("  Direction:");
            ig.igSameLine();
            if (ig.igRadioButton("Up", !self.settings.find_down)) self.setFindOption(.down, false);
            ig.igSameLine();
            if (ig.igRadioButton("Down", self.settings.find_down)) self.setFindOption(.down, true);
            var match_case = self.settings.find_match_case;
            if (ig.igCheckbox("Match case", &match_case)) self.setFindOption(.match_case, match_case);
            ig.igSameLine();
            if (ig.igButton("Cancel")) open = false;
            if (self.find_missed) ig.igTextUnformatted("Not found.");
        }
        ig.igEnd();
        if (!open) self.closeFind();
    }

    /// `CStatisticDialog`'s tree as a table: Original Text and Translation,
    /// each with its four states summed over the elements and then each
    /// element ("Name, VERSION: v") with its four states; Items, Words,
    /// Word Symbols and Symbols, the thousands set apart. The two halves
    /// start open, the elements closed.
    fn drawStatisticsWindow(self: *Ui) void {
        self.statistics_drawn = false;
        const statistic = self.statistics orelse return;
        if (!self.session.isOpen()) return self.closeStatistics();
        const font = ig.igGetFontSize();
        ig.igSetNextWindowSize(.{ .x = font * 44, .y = font * 24 }, ig.ImGuiCond_Appearing);
        var open = true;
        if (ig.igBegin("ELK Statistic", &open, ig.ImGuiWindowFlags_NoCollapse | ig.ImGuiWindowFlags_NoSavedSettings)) {
            self.statistics_drawn = true;
            const table_flags = ig.ImGuiTableFlags_Borders | ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable | ig.ImGuiTableFlags_ScrollY;
            if (ig.igBeginTableEx("statistic", 5, table_flags, .{ .x = 0, .y = -ig.igGetFrameHeightWithSpacing() }, 0)) {
                ig.igTableSetupScrollFreeze(0, 1);
                ig.igTableSetupColumnEx("Tree", ig.ImGuiTableColumnFlags_WidthStretch, 0, 0);
                for ([_][*:0]const u8{ "Items", "Words", "Word Symbols", "Symbols" }) |heading| ig.igTableSetupColumnEx(heading, ig.ImGuiTableColumnFlags_WidthFixed, font * 6, 0);
                ig.igTableHeadersRow();
                const tree = &self.session.tree.?;
                for ([_]bool{ false, true }) |translation| {
                    ig.igPushIDInt(@intFromBool(translation));
                    defer ig.igPopID();
                    const label = if (translation) "Translation" else "Original Text";
                    if (tableRow(label, ig.ImGuiTreeNodeFlags_DefaultOpen, null, self.statisticRow(translation, null, null).?)) {
                        defer ig.igTreePop();
                        for (std.enums.values(State)) |state| _ = tableRow(stateName(state), ig.ImGuiTreeNodeFlags_Leaf, stateColour(state), self.statisticRow(translation, null, state).?);
                        const half = if (translation) statistic.translation else statistic.original;
                        for (0..half.len) |element| {
                            ig.igPushIDInt(@intCast(element));
                            defer ig.igPopID();
                            const root = tree.rootOf(element) orelse continue;
                            if (tableRow(tree.nodes[root].name, 0, null, self.statisticRow(translation, element, null).?)) {
                                defer ig.igTreePop();
                                for (std.enums.values(State)) |state| _ = tableRow(stateName(state), ig.ImGuiTreeNodeFlags_Leaf, stateColour(state), self.statisticRow(translation, element, state).?);
                            }
                        }
                    }
                }
                ig.igEndTable();
            }
            if (ig.igButton("OK")) open = false;
        }
        ig.igEnd();
        if (!open) self.closeStatistics();
    }

    /// The MFC Browse menu's commands, in its order; each step is enabled
    /// when it reaches a text.
    fn drawBrowseMenu(self: *Ui) void {
        if (!ig.igBeginMenu("Browse")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx("Collapse Item", null, self.settings.collapse_item, true)) self.toggleCollapseItem();
        ig.igSeparator();
        const items = [_]struct { label: [*:0]const u8, keys: ?[*:0]const u8, step: Step }{
            .{ .label = "Last", .keys = null, .step = .last },
            .{ .label = "Next", .keys = "Ctrl+N", .step = .next },
            .{ .label = "Previous", .keys = "Ctrl+P", .step = .previous },
            .{ .label = "First", .keys = null, .step = .first },
        };
        for (items) |item| {
            if (ig.igMenuItemEx(item.label, item.keys, false, self.canBrowse(item.step))) _ = self.browse(item.step);
        }
        ig.igSeparator();
        if (ig.igMenuItemEx("Create Filter...", null, false, self.session.isOpen())) self.openFilterDialog();
    }

    /// The extended layout's filter bar (the MFC browse toolbar): the named
    /// filters' combo and the four Browse buttons.
    fn drawFilterBar(self: *Ui) void {
        var name_buffer: [settings_mod.max_path:0]u8 = undefined;
        const current = std.mem.printSentinel(&name_buffer, "{s}", .{self.currentFilterName()}, 0) catch "";
        ig.igTextUnformatted("Filter:");
        ig.igSameLine();
        ig.igSetNextItemWidth(ig.igGetFontSize() * 12);
        if (ig.igBeginCombo("##filter", current.ptr, 0)) {
            defer ig.igEndCombo();
            var label_buffer: [settings_mod.max_path:0]u8 = undefined;
            for (0..self.filterCount()) |index| {
                const filter = self.filterAt(index);
                const label = std.mem.printSentinel(&label_buffer, "{s}", .{filter.name}, 0) catch continue;
                if (ig.igSelectableEx(label.ptr, std.mem.eql(u8, filter.name, current), 0, .{ .x = 0, .y = 0 })) {
                    // The name borrows the settings it is written into.
                    var copy: [settings_mod.max_path]u8 = undefined;
                    @memcpy(copy[0..filter.name.len], filter.name);
                    self.chooseFilter(copy[0..filter.name.len]);
                }
            }
        }
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
        if (ig.igCheckbox("Collapse Item", &collapse)) self.toggleCollapseItem();
        ig.igSameLine();
        if (ig.igButton("Create Filter...")) self.openFilterDialog();
    }

    fn drawFileMenu(self: *Ui, window: *window_mod.Window) void {
        if (!ig.igBeginMenu("File")) return;
        defer ig.igEndMenu();
        if (ig.igMenuItemEx("Open...", "Ctrl+O", false, !open_slot.waiting())) self.requestOpen(window);
        const settings_count = self.settings.recentCount();
        if (ig.igBeginMenuEx("Recent", settings_count != 0)) {
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
        if (ig.igMenuItemEx("Save", "Ctrl+S", false, open)) self.session.save() catch {};
        if (ig.igMenuItemEx("Close", null, false, open)) self.session.close() catch {};
        ig.igSeparator();
        if (ig.igMenuItemEx("Export to language mod...", null, false, self.canExport())) self.requestExportMod(false);
        if (ig.igMenuItemEx("Install to game...", null, false, self.canExport())) self.requestInstall();
        if (ig.igMenuItemEx("Run Game", null, false, self.canRunGame())) self.requestRunGame();
        ig.igSeparator();
        if (ig.igMenuItem("Exit")) self.requestExit();
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
            ig.igTextUnformatted("Select a text in the list.");
            return;
        };
        const half = (ig.igGetContentRegionAvail().y - 2 * ig.igGetTextLineHeightWithSpacing() - ig.igGetFrameHeightWithSpacing()) / 2;
        // The text's own ID, so an edit box active on the previous text does
        // not carry its buffer over to this one.
        ig.igPushIDInt(@intCast(index));
        defer ig.igPopID();
        ig.igTextUnformatted("Original");
        if (self.session.original.storage(self.gpa)) |storage| {
            _ = ig.igInputTextMultilineEx("##original", storage.ptr, storage.len, .{ .x = -1, .y = half }, ig.ImGuiInputTextFlags_ReadOnly, null, null);
        } else |_| {}
        self.drawStateButtons();
        ig.igTextUnformatted("Translation");
        // Held while the dialog is open, whose modal the focus would close.
        if (self.focus_translation and !self.modalOpen()) {
            ig.igSetKeyboardFocusHere();
            self.focus_translation = false;
        }
        if (self.session.translation.storage(self.gpa)) |storage| {
            _ = ig.igInputTextMultilineEx("##translation", storage.ptr, storage.len, .{ .x = -1, .y = -1 }, ig.ImGuiInputTextFlags_CallbackResize, resizeTranslation, self);
            self.session.translation.settle();
        } else |_| {}
    }

    /// The edit view's state radio buttons. Outdated shows when an update
    /// set it but cannot be chosen.
    fn drawStateButtons(self: *Ui) void {
        ig.igTextUnformatted("State:");
        for ([_]State{ .not_translated, .outdated, .translated, .approved }) |state| {
            var name: [32:0]u8 = undefined;
            const label = std.mem.printSentinel(&name, "{s}", .{stateName(state)}, 0) catch continue;
            ig.igSameLine();
            ig.igBeginDisabled(state == .outdated);
            ig.igPushStyleColor(ig.ImGuiCol_Text, stateColour(state));
            if (ig.igRadioButton(label.ptr, self.session.state == state)) self.pressState(state);
            ig.igPopStyleColor();
            ig.igEndDisabled();
        }
        self.state_buttons_drawn = true;
    }

    fn drawStatus(self: *Ui) void {
        var buffer: [1536:0]u8 = undefined;
        const failure = self.session.failureText();
        // The MFC ELK selected the hit in its edit box; this names where it is.
        var found_buffer: [96]u8 = undefined;
        const found = if (self.session.last_hit) |hit| (if (self.session.selected == hit.text)
            std.fmt.bufPrint(&found_buffer, " | found in the {s} at {d}", .{ @tagName(hit.field), hit.offset }) catch ""
        else
            "") else "";
        // The last export's or launch's summary closes the line.
        const notice_text = self.notice();
        const separator = if (notice_text.len != 0) " | " else "";
        const text = if (failure.len != 0)
            std.mem.printSentinel(&buffer, "{s}{s}", .{ failure, if (self.exit_refused) " | Exit again to quit without saving" else "" }, 0)
        else if (self.session.selected) |index|
            std.mem.printSentinel(&buffer, "{d} texts | {s} | {s}{s}{s}{s}{s}", .{
                self.session.textCount(),
                self.session.textAt(index).key,
                stateName(self.session.state),
                if (self.session.dirty()) " | unsaved" else "",
                found,
                separator,
                notice_text,
            }, 0)
        else
            std.mem.printSentinel(&buffer, "{d} texts{s}{s}", .{ self.session.textCount(), separator, notice_text }, 0);
        ig.igTextUnformatted((text catch return).ptr);
    }

    fn drawFailure(self: *Ui) void {
        const failure = self.session.failureText();
        if (failure.len == 0) return;
        var buffer: [512:0]u8 = undefined;
        const text = std.mem.printSentinel(&buffer, "{s}", .{failure}, 0) catch return;
        ig.igTextUnformatted(text.ptr);
    }
};

pub fn stateName(state: session_mod.State) []const u8 {
    return switch (state) {
        .not_translated => "Not translated",
        .outdated => "Outdated",
        .translated => "Translated",
        .approved => "Approved",
    };
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
/// counts right-aligned after it. Returns whether a node with children is
/// open (the caller pops it then).
fn tableRow(label: []const u8, flags: ig.ImGuiTreeNodeFlags, colour: ?u32, counts: Counts) bool {
    ig.igTableNextRow();
    _ = ig.igTableNextColumn();
    var label_buffer: [512:0]u8 = undefined;
    const text = std.mem.printSentinel(&label_buffer, "{s}", .{label}, 0) catch "";
    if (colour) |chosen| ig.igPushStyleColor(ig.ImGuiCol_Text, chosen);
    const leaf = flags & ig.ImGuiTreeNodeFlags_Leaf != 0;
    const open = ig.igTreeNodeEx(text.ptr, flags | ig.ImGuiTreeNodeFlags_SpanFullWidth | (if (leaf) ig.ImGuiTreeNodeFlags_NoTreePushOnOpen else 0));
    if (colour != null) ig.igPopStyleColor();
    for ([_]i32{ counts.texts, counts.words, counts.word_symbols, counts.symbols }) |value| {
        _ = ig.igTableNextColumn();
        var number_buffer: [24:0]u8 = undefined;
        const number = elk_core.stats.formatCount(number_buffer[0 .. number_buffer.len - 1], value);
        number_buffer[number.len] = 0;
        const width = ig.igCalcTextSize(&number_buffer).x;
        ig.igSetCursorPosX(ig.igGetCursorPosX() + @max(0, ig.igGetContentRegionAvail().x - width));
        ig.igTextUnformatted(&number_buffer);
    }
    return open and !leaf;
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
