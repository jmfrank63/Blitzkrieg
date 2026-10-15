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
const std = @import("std");
const imgui = @import("editor_imgui");
const sdl3 = @import("sdl3");
const kit = @import("editor_kit");
const session_mod = @import("session.zig");
const settings_mod = @import("settings.zig");
const window_mod = @import("editor_kit").window;
const FilterDialog = @import("filter_dialog.zig").FilterDialog;

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

    /// `colour` is the row's state colour; a dimmed row was drawn fainter.
    pub const Drawn = struct { node: u32, colour: u32, dimmed: bool };

    pub fn deinit(self: *Ui) void {
        if (self.filter_dialog) |dialog| self.gpa.destroy(dialog);
        self.closeStatistics();
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

    /// A scripted run's stand-in for the OS dialog: answers the File > Open
    /// that is waiting. False when none is.
    pub fn answerOpen(self: *Ui, path: []const u8) bool {
        _ = self;
        if (!open_slot.waiting()) return false;
        open_slot.deliver(path);
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
        window.setTitle(self.title().ptr);
        if (ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_S, ig.ImGuiInputFlags_RouteGlobal)) self.session.save() catch {};
        if (ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_O, ig.ImGuiInputFlags_RouteGlobal)) self.requestOpen(window);
        if (ig.igShortcut(ig.ImGuiMod_Ctrl | ig.ImGuiKey_F, ig.ImGuiInputFlags_RouteGlobal)) self.openFind();
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
        defer self.drawStatisticsWindow();
        defer self.drawFindWindow();
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

    fn drawMenu(self: *Ui, window: *window_mod.Window) void {
        if (!ig.igBeginMenuBar()) return;
        defer ig.igEndMenuBar();
        self.drawFileMenu(window);
        self.drawEditMenu();
        self.drawBrowseMenu();
        self.drawViewMenu();
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
        if (self.focus_translation and self.filter_dialog == null) {
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
        var buffer: [768:0]u8 = undefined;
        const failure = self.session.failureText();
        // The MFC ELK selected the hit in its edit box; this names where it is.
        var found_buffer: [96]u8 = undefined;
        const found = if (self.session.last_hit) |hit| (if (self.session.selected == hit.text)
            std.fmt.bufPrint(&found_buffer, " | found in the {s} at {d}", .{ @tagName(hit.field), hit.offset }) catch ""
        else
            "") else "";
        const text = if (failure.len != 0)
            std.mem.printSentinel(&buffer, "{s}{s}", .{ failure, if (self.exit_refused) " | Exit again to quit without saving" else "" }, 0)
        else if (self.session.selected) |index|
            std.mem.printSentinel(&buffer, "{d} texts | {s} | {s}{s}{s}", .{
                self.session.textCount(),
                self.session.textAt(index).key,
                stateName(self.session.state),
                if (self.session.dirty()) " | unsaved" else "",
                found,
            }, 0)
        else
            std.mem.printSentinel(&buffer, "{d} texts", .{self.session.textCount()}, 0);
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
