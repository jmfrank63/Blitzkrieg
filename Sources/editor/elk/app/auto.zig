//! `BK_ELK_AUTO` and the smoke: the kit's schedule grammar
//! (`kit.auto_schedule`) replayed into ELK's own frame loop. Keys and typed
//! text become SDL events handed to ImGui's backend exactly as real ones
//! are, so the edit boxes see what a translator's keyboard would send;
//! `do=` and `expect=` resolve against ELK's own commands and state.
//!
//! What ELK drives in this slice: `key=`, `text=`, `do=`, `expect=`, `exit`.
//! The pointer actions, `open=`/`save`/`saveas=`, test-in-game and the
//! screenshot actions fail the run as not driven yet.
//!
//!   do=select:<index>        select the <index>th text, leaving the current
//!                            one; the tree opens its folders and scrolls to it
//!   do=select_key:<key>      the same, by the text's key
//!   do=expand:<folder>       open the folder (slash-separated, below the
//!                            element's root) and those above it, as their
//!                            arrows would
//!   do=click:<key>           click the text's tree row; the last frame must
//!                            have drawn it
//!   do=state:<state>         press a state button (the last frame must have
//!                            drawn them): not_translated, translated, approved
//!   do=filter:<name>         choose the named filter in the filter bar
//!   do=browse:<step>         Browse > First, Previous, Next or Last
//!                            (first, previous, next, last); the hot keys
//!                            are `key=n+ctrl` and `key=p+ctrl`
//!   do=collapse_item         Browse > Collapse Item, turned over
//!   do=filter_dialog         Browse > Create Filter...; the commands below
//!                            need the dialog drawn in the last frame
//!   do=filter_pick:<name>    click the filter in the dialog's list
//!   do=filter_add:<name>     type the name into the Name box, press Add
//!   do=filter_rename:<name>  the same, press Rename
//!   do=filter_delete         press Delete
//!   do=filter_states:<4 0/1> the four state boxes, in state order
//!   do=filter_changed:<0|1>  the Changed only box
//!   do=filter_condition:<a&b>  type the condition, press Add Condition
//!   do=filter_ok  do=filter_cancel
//!   do=find_dialog           Edit > Find...; the commands below need the
//!                            Find window drawn in the last frame
//!   do=find_next:<text>      type the text into Find what, press Find Next
//!   do=find_option:<option>=<0|1>  tick a box or pick the direction:
//!                            match_case, whole_word, down (0 is Up)
//!   do=find_close            Cancel
//!   do=statistics            View > ELK Statistic
//!   do=statistics_close      the statistic window's OK
//!   do=spell                 Tools > Spelling (the hot key is `key=f7`)
//!   do=spell_add:<word>      the Spelling window's Add to dictionary on the
//!                            word's first row (the window drawn last frame)
//!   do=spell_close           the Spelling window's Close
//!   do=spell_options         Tools > Spelling Options...
//!   do=spell_folder:<path>   the options' Dictionary folder, relative to the
//!                            runner's dialog folder if set; `-` is empty
//!   do=spell_language:<name> the options' language combo
//!   do=export_mod            File > Export to language mod...; the commands
//!                            below need the dialog drawn in the last frame
//!   do=export_name:<text>    type a mod's name or folder into the Mod box
//!   do=export_browse         press Browse...; `do=answer` answers it
//!   do=export_ok  do=export_cancel
//!   do=sheet_import          File > Import from spreadsheet...; `do=answer`
//!                            answers its dialog with the sheet
//!   do=sheet_export          File > Export to spreadsheet...; the same
//!   do=install               File > Install to game...
//!   do=install_choice:<c>    a button of the shadow warning (drawn in the
//!                            last frame): mod, install or cancel
//!   do=run_game              File > Run Game
//!   do=wait_game             block until the game Run Game started exits
//!                            (30 s at most); it must exit 0
//!   do=wait:<seconds>        the autosave clock moves on that far
//!   do=kill                  the frame loop stops at once, no save point
//!   do=focus:translation     the translation box takes the keyboard
//!   do=save  do=close  do=reopen  (File > Save, File > Close, open again)
//!   do=open                  File > Open; the run shows no OS dialog
//!   do=answer:<path>         the waiting File > Open or Browse... dialog
//!                            chose <path>, relative to the runner's dialog
//!                            folder if set
//!   expect=open  expect=closed  expect=rows  expect=glyphs
//!   expect=texts:<n>  expect=title:<file>  expect=dirty:<0|1>
//!   expect=original_has:<text>  expect=translation:<text>
//!   expect=disk:<key>=<text>   the translation file as the core reads it
//!   expect=state:<key>=<not_translated|outdated|translated|approved>
//!   expect=row:<key>=<state|hidden>  the last frame drew the text's tree
//!                            row in that state's colour, or did not draw it
//!   expect=folder:<folder>=<state>   the same for a folder's row
//!   expect=selected:<key>      the selected text
//!   expect=filter:<name>       the current filter
//!   expect=browsable:<step>=<0|1>   whether that Browse command is enabled
//!   expect=dimmed:<path>=<0|1>  the last frame drew the text's (or, with no
//!                            such text, the folder's) row dimmed or not
//!   expect=collapse_item:<0|1>  the Collapse Item mode
//!   expect=expanded:<folder>=<0|1>  whether the folder's row is open
//!   expect=filter_dialog:<0|1> whether the dialog is open
//!   expect=filter_count:<n>    how many filters the bar offers
//!   expect=filter_at:<index>=<name>  the bar's filter at that place
//!   expect=find_dialog:<0|1>   whether the Find window is open
//!   expect=found:<key>=<field>:<offset>  the last Find Next's hit (field
//!                            original, description or translation, the
//!                            byte offset in it), its text selected
//!   expect=found:none          the last Find Next found nothing
//!   expect=find_text:<text>  expect=find_option:<option>=<0|1>  as elk.cfg keeps them
//!   expect=statistics:<0|1>    whether the last frame drew the statistic window
//!   expect=statistic:<half>/<row>=<items>/<words>/<word symbols>/<symbols>
//!                            a row of that window: half original or
//!                            translation; row total, a state (summed over
//!                            the elements), an element's number, or
//!                            <number>.<state>
//!   expect=can_spell:<0|1>   whether Tools > Spelling is enabled
//!   expect=spell_reason_has:<text>  why spell check is off
//!   expect=spell_language:<name>  the language loaded
//!   expect=spelling:<0|1>    whether the last frame drew the Spelling window
//!   expect=misspelt:<word>=<0|1>  whether the last check listed the word
//!   expect=suggestion:<word>=<suggestion>  the word's first suggestion
//!   expect=user_word:<word>  the word is a line of custom.dic on disk
//!   expect=can_export:<0|1>  expect=can_run_game:<0|1>  whether the File
//!                            menu's export commands and Run Game are enabled
//!   expect=export_dialog:<0|1>  expect=install_warning:<0|1>  whether open
//!   expect=warning:<count>=<folder>  the shadow warning's count and the
//!                            last part of the loose folder it names
//!   expect=pak:<mod>=<entries|none>  the pak of the mod (`game` for
//!                            Data/Texts.pak) in the game folder, read back by
//!                            the core: its text entries, or no such file
//!   expect=notice_has:<text>   the status line's export, import or launch
//!                            summary
//!   expect=imported:<rows>/<changed>/<outdated>/<empty>/<missing>  the last
//!                            spreadsheet import's counts: rows read, texts
//!                            changed and made Outdated, rows skipped for an
//!                            empty translation and for a missing text
//!   expect=failure_has:<text>  the status line's failure
//!   expect=launched:<index>=<argument>  the last Run Game's command line
//!   expect=untouched:<index>   the row's text files are byte-identical to
//!                              the source the smoke copied (smoke only)
//!   expect=identical           every file of the opened copy is byte-identical
//!                              to the source and none was added (smoke only)
const std = @import("std");
const kit = @import("editor_kit");
const sdl3 = @import("sdl3");
const elk = @import("elk_core");
const ui_mod = @import("ui.zig");
const fonts = @import("fonts.zig");
const window_mod = kit.window;

const schedule = kit.auto_schedule;
const c = sdl3.c;

pub const Runner = struct {
    entries: []const schedule.Scheduled,
    next: usize = 0,
    /// The database the smoke copied, for `expect=untouched`; "" otherwise.
    source_folder: []const u8 = "",
    /// How many files the smoke copied, for `expect=identical`.
    source_files: usize = 0,
    /// Where a scripted dialog answers, standing in for the folder the user
    /// browsed to: the grammar's arguments are short, the paths are not.
    dialog_folder: []const u8 = "",
    /// Text events borrow their bytes until ImGui copies them in.
    text_storage: [schedule.max_arg_len + 1]u8 = undefined,
    failure_storage: [512]u8 = undefined,
    failure: []const u8 = "",
    /// The last database the run had open, for `do=reopen` and the
    /// on-disk expectations (which also hold after File > Close).
    last_path: [std.Io.Dir.max_path_bytes]u8 = undefined,
    last_path_len: usize = 0,

    /// Runs every entry scheduled for `frame`; false when one failed
    /// (`failure` says which and why).
    pub fn step(self: *Runner, frame: u32, window: *window_mod.Window, ui: *ui_mod.Ui) bool {
        // File > Open opens in a frame, not in a command.
        self.remember(ui.session.openPath());
        while (self.next < self.entries.len and self.entries[self.next].frame <= frame) : (self.next += 1) {
            const entry = self.entries[self.next];
            std.debug.print("BK_ELK_AUTO: frame {d} action {s}\n", .{ frame, entry.text });
            self.run(entry.action, window, ui) catch |err| {
                if (self.failure.len == 0) self.fail("{s}: {s}", .{ entry.text, @errorName(err) });
                return false;
            };
            if (self.failure.len != 0) return false;
        }
        return true;
    }

    pub fn done(self: *const Runner) bool {
        return self.next >= self.entries.len;
    }

    fn run(self: *Runner, action: schedule.Action, window: *window_mod.Window, ui: *ui_mod.Ui) !void {
        switch (action) {
            .key => |pressed| try self.press(window, pressed),
            .text => |text| {
                @memcpy(self.text_storage[0..text.len], text);
                self.text_storage[text.len] = 0;
                var event = std.mem.zeroes(c.SDL_Event);
                event.text.type = c.SDL_EVENT_TEXT_INPUT;
                event.text.windowID = c.SDL_GetWindowID(window.window);
                event.text.text = &self.text_storage;
                _ = window.handleEvent(&event);
            },
            .do => |named| try self.command(window, ui, named),
            .expect => |named| try self.expect(window, ui, named),
            .exit => ui.requestExit(),
            else => self.fail("{s} is not driven by ELK yet", .{@tagName(action)}),
        }
    }

    fn press(self: *Runner, window: *window_mod.Window, key_action: schedule.Key) !void {
        var name_buffer: [64:0]u8 = undefined;
        const name = std.mem.printSentinel(&name_buffer, "{s}", .{key_action.name}, 0) catch return error.BadKey;
        const scancode = c.SDL_GetScancodeFromName(name.ptr);
        if (scancode == c.SDL_SCANCODE_UNKNOWN) return self.fail("unknown key {s}", .{key_action.name});
        var mods: c.SDL_Keymod = 0;
        if (key_action.mods.ctrl) mods |= c.SDL_KMOD_LCTRL;
        if (key_action.mods.shift) mods |= c.SDL_KMOD_LSHIFT;
        if (key_action.mods.alt) mods |= c.SDL_KMOD_LALT;
        if (key_action.mods.cmd) mods |= c.SDL_KMOD_LGUI;
        for ([_]bool{ true, false }) |down| {
            var event = std.mem.zeroes(c.SDL_Event);
            event.key.type = if (down) c.SDL_EVENT_KEY_DOWN else c.SDL_EVENT_KEY_UP;
            event.key.windowID = c.SDL_GetWindowID(window.window);
            event.key.scancode = scancode;
            event.key.key = c.SDL_GetKeyFromScancode(scancode, 0, false);
            // The backend takes ImGui's modifiers from each event, so the
            // release carries none: the modifiers go up with the key, as a
            // real keyboard's do, and Ctrl does not stay held over typing.
            event.key.mod = if (down) mods else 0;
            event.key.down = down;
            _ = window.handleEvent(&event);
        }
    }

    fn command(self: *Runner, window: *window_mod.Window, ui: *ui_mod.Ui, named: schedule.Named) !void {
        const session = ui.session;
        if (is(named, "select")) {
            const index = std.fmt.parseInt(usize, named.arg, 10) catch return self.fail("select needs a row number, got {s}", .{named.arg});
            try ui.select(index);
        } else if (is(named, "select_key")) {
            const index = session.find(named.arg) orelse return self.fail("no text {s}", .{named.arg});
            try ui.select(index);
        } else if (is(named, "expand")) {
            const node = folderNode(session, named.arg) orelse return self.fail("do=expand: no folder {s}", .{named.arg});
            ui.reveal(node);
            ui.expanded.items[node] = true;
        } else if (is(named, "click")) {
            const node = textNode(session, named.arg) orelse return self.fail("do=click: no text {s}", .{named.arg});
            if (drawnColour(ui, node) == null) return self.fail("do=click: the tree does not show {s}", .{named.arg});
            ui.clickRow(node);
            if (session.selected != session.tree.?.nodes[node].text.?) return self.fail("do=click: {s} was not selected ({s})", .{ named.arg, session.failureText() });
        } else if (is(named, "state")) {
            const state = std.meta.stringToEnum(elk.state.State, named.arg) orelse return self.fail("do=state: no state {s}", .{named.arg});
            if (!ui.state_buttons_drawn) return self.fail("do=state: the state buttons are not shown", .{});
            ui.pressState(state);
            if (session.state != state) return self.fail("do=state: the text is {s}, wanted {s} ({s})", .{ @tagName(session.state), named.arg, session.failureText() });
        } else if (is(named, "filter")) {
            const known = for (0..ui.filterCount()) |index| {
                if (std.mem.eql(u8, ui.filterAt(index).name, named.arg)) break true;
            } else false;
            if (!known) return self.fail("do=filter: the filter bar offers no {s}", .{named.arg});
            ui.chooseFilter(named.arg);
        } else if (is(named, "browse")) {
            const browse_step = std.meta.stringToEnum(ui_mod.Step, named.arg) orelse return self.fail("do=browse: no step {s}", .{named.arg});
            if (!ui.canBrowse(browse_step)) return self.fail("do=browse: Browse > {s} is disabled", .{named.arg});
            if (!ui.browse(browse_step)) return self.fail("do=browse: {s} reached no text ({s})", .{ named.arg, session.failureText() });
        } else if (is(named, "collapse_item")) {
            ui.toggleCollapseItem();
        } else if (is(named, "filter_dialog")) {
            if (!session.isOpen()) return self.fail("do=filter_dialog: Create Filter... is disabled with no database", .{});
            ui.openFilterDialog();
            if (ui.filter_dialog == null) return self.fail("do=filter_dialog: the dialog did not open", .{});
        } else if (std.mem.startsWith(u8, named.name, "filter_")) {
            try self.dialogCommand(ui, named);
        } else if (is(named, "find_dialog")) {
            if (!session.isOpen()) return self.fail("do=find_dialog: Find... is disabled with no database", .{});
            ui.openFind();
        } else if (is(named, "find_next") or is(named, "find_option") or is(named, "find_close")) {
            if (!ui.find_drawn) return self.fail("do={s}: the Find window is not shown", .{named.name});
            if (is(named, "find_next")) {
                _ = std.mem.printSentinel(&ui.find_buffer, "{s}", .{named.arg}, 0) catch return self.fail("do=find_next: the text is too long", .{});
                _ = ui.findNext();
            } else if (is(named, "find_option")) {
                const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("find_option needs <option>=<0|1>", .{});
                const option = std.meta.stringToEnum(ui_mod.Ui.FindOption, named.arg[0..eq]) orelse return self.fail("do=find_option: no option {s}", .{named.arg[0..eq]});
                ui.setFindOption(option, std.mem.eql(u8, named.arg[eq + 1 ..], "1"));
            } else ui.closeFind();
        } else if (is(named, "statistics")) {
            if (!session.isOpen()) return self.fail("do=statistics: ELK Statistic is disabled with no database", .{});
            ui.openStatistics();
            if (ui.statistics == null) return self.fail("do=statistics: nothing was counted ({s})", .{session.failureText()});
        } else if (is(named, "statistics_close")) {
            if (!ui.statistics_drawn) return self.fail("do=statistics_close: the statistic window is not shown", .{});
            ui.closeStatistics();
        } else if (is(named, "spell")) {
            if (!ui.canSpell()) return self.fail("do=spell: Tools > Spelling is disabled ({s})", .{ui.spelling().reason()});
            ui.openSpelling();
        } else if (is(named, "spell_add") or is(named, "spell_close")) {
            if (!ui.spell_drawn) return self.fail("do={s}: the Spelling window is not shown", .{named.name});
            if (is(named, "spell_close")) {
                ui.closeSpelling();
            } else {
                const index = spellRow(ui, named.arg) orelse return self.fail("do=spell_add: the window lists no {s}", .{named.arg});
                ui.addToDictionary(index);
            }
        } else if (is(named, "spell_options")) {
            ui.openSpellOptions();
        } else if (is(named, "spell_folder")) {
            var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            const path = if (std.mem.eql(u8, named.arg, "-"))
                ""
            else if (self.dialog_folder.len == 0)
                named.arg
            else
                try std.fmt.bufPrint(&path_buffer, "{s}{c}{s}", .{ self.dialog_folder, std.fs.path.sep, named.arg });
            ui.setSpellFolder(path);
        } else if (is(named, "spell_language")) {
            ui.setSpellLanguage(named.arg);
        } else if (is(named, "export_mod")) {
            if (!ui.canExport()) return self.fail("do=export_mod: Export to language mod... is disabled", .{});
            ui.requestExportMod(false);
        } else if (is(named, "export_name") or is(named, "export_browse") or is(named, "export_ok") or is(named, "export_cancel")) {
            if (!ui.export_drawn) return self.fail("do={s}: the export dialog is not shown", .{named.name});
            if (is(named, "export_name")) {
                ui.setExportName(named.arg);
            } else if (is(named, "export_browse")) {
                ui.requestModFolder(window);
            } else ui.export_pending = is(named, "export_ok");
        } else if (is(named, "sheet_import") or is(named, "sheet_export")) {
            if (!ui.canExport()) return self.fail("do={s}: the spreadsheet commands are disabled", .{named.name});
            ui.requestSheet(window, if (is(named, "sheet_import")) .import else .@"export");
        } else if (is(named, "install")) {
            if (!ui.canExport()) return self.fail("do=install: Install to game... is disabled", .{});
            ui.requestInstall();
        } else if (is(named, "install_choice")) {
            if (!ui.install_drawn) return self.fail("do=install_choice: the shadow warning is not shown", .{});
            ui.install_pending = std.meta.stringToEnum(ui_mod.Ui.InstallChoice, named.arg) orelse return self.fail("do=install_choice: no button {s}", .{named.arg});
        } else if (is(named, "run_game")) {
            if (!ui.canRunGame()) return self.fail("do=run_game: Run Game is disabled", .{});
            ui.requestRunGame();
        } else if (is(named, "wait_game")) {
            // A game that exited before this frame was already polled.
            const exit = if (ui.running) |*running|
                running.waitBlocking(ui.io, 30_000) orelse return self.fail("do=wait_game: the game did not exit within 30 s", .{})
            else
                ui.game_exit orelse return self.fail("do=wait_game: Run Game started no game ({s})", .{session.failureText()});
            if (kit.testlaunch.describe(exit) != .clean) return self.fail("do=wait_game: the game ended with exit {?d}, signal {?d}", .{ exit.code, exit.signal });
        } else if (is(named, "wait")) {
            const seconds = std.fmt.parseInt(u64, named.arg, 10) catch return self.fail("wait needs seconds, got {s}", .{named.arg});
            ui.clock_offset_ms += seconds * std.time.ms_per_s;
        } else if (is(named, "kill")) {
            ui.killed = true;
        } else if (is(named, "focus") and std.mem.eql(u8, named.arg, "translation")) {
            ui.focus_translation = true;
        } else if (is(named, "save")) {
            try session.save();
        } else if (is(named, "close")) {
            try session.close();
        } else if (is(named, "open")) {
            ui.requestOpen(window);
        } else if (is(named, "answer")) {
            var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            const path = if (self.dialog_folder.len == 0) named.arg else try std.fmt.bufPrint(&path_buffer, "{s}{c}{s}", .{ self.dialog_folder, std.fs.path.sep, named.arg });
            if (!ui.answerDialog(path)) return self.fail("do=answer: no file or folder dialog is waiting", .{});
        } else if (is(named, "reopen")) {
            var copy: [std.Io.Dir.max_path_bytes]u8 = undefined;
            const last = self.last_path[0..self.last_path_len];
            @memcpy(copy[0..last.len], last);
            try session.open(copy[0..last.len]);
        } else return self.fail("unknown command do={s}", .{named.name});
        self.remember(session.openPath());
    }

    /// The Create Filter dialog's controls, each pressed as its button is.
    fn dialogCommand(self: *Runner, ui: *ui_mod.Ui, named: schedule.Named) !void {
        const dialog = ui.filter_dialog orelse return self.fail("do={s}: the Create Filter dialog is not open", .{named.name});
        if (!dialog.drawn) return self.fail("do={s}: the Create Filter dialog is not shown", .{named.name});
        const accepted = if (is(named, "filter_pick")) picked: {
            dialog.pick(dialog.find(named.arg) orelse return self.fail("do=filter_pick: the dialog lists no {s}", .{named.arg}));
            break :picked true;
        } else if (is(named, "filter_add") or is(named, "filter_rename")) typed: {
            _ = std.mem.printSentinel(&dialog.name, "{s}", .{named.arg}, 0) catch return self.fail("do={s}: the name is too long", .{named.name});
            break :typed if (is(named, "filter_add")) dialog.add() else dialog.rename();
        } else if (is(named, "filter_delete"))
            dialog.delete()
        else if (is(named, "filter_states")) states: {
            if (named.arg.len != 4) return self.fail("do=filter_states needs four 0/1, got {s}", .{named.arg});
            var states: [4]bool = undefined;
            for (named.arg, &states) |digit, *keep| keep.* = digit == '1';
            const filter = dialog.selectedFilter() orelse return self.fail("do=filter_states: no filter is selected", .{});
            break :states dialog.setCriteria(states, filter.changed);
        } else if (is(named, "filter_changed")) changed: {
            const filter = dialog.selectedFilter() orelse return self.fail("do=filter_changed: no filter is selected", .{});
            break :changed dialog.setCriteria(filter.states, std.mem.eql(u8, named.arg, "1"));
        } else if (is(named, "filter_condition"))
            dialog.addCondition(named.arg)
        else if (is(named, "filter_ok") or is(named, "filter_cancel")) closed: {
            dialog.pending = if (is(named, "filter_ok")) .ok else .cancel;
            break :closed true;
        } else return self.fail("unknown command do={s}", .{named.name});
        if (!accepted) return self.fail("do={s}:{s} was refused", .{ named.name, named.arg });
    }

    pub fn remember(self: *Runner, opened: []const u8) void {
        if (opened.len == 0 or opened.len > self.last_path.len) return;
        @memcpy(self.last_path[0..opened.len], opened);
        self.last_path_len = opened.len;
    }

    fn expect(self: *Runner, window: *window_mod.Window, ui: *ui_mod.Ui, named: schedule.Named) !void {
        const session = ui.session;
        const gpa = ui.gpa;
        if (is(named, "open")) {
            if (!session.isOpen()) return self.fail("expect=open: no database is open ({s})", .{session.failureText()});
        } else if (is(named, "closed")) {
            if (session.isOpen()) return self.fail("expect=closed: {s} is still open", .{session.openPath()});
        } else if (is(named, "rows")) {
            if (ui.rows_drawn == 0) return self.fail("expect=rows: the tree drew no rows", .{});
        } else if (is(named, "glyphs")) {
            for (fonts.samples) |codepoint| {
                if (!fonts.covers(codepoint)) return self.fail("expect=glyphs: the font stack has no glyph for U+{X:0>4}", .{codepoint});
            }
        } else if (is(named, "texts")) {
            const wanted = std.fmt.parseInt(usize, named.arg, 10) catch return self.fail("texts needs a number", .{});
            if (session.textCount() != wanted) return self.fail("expect=texts: {d} texts, wanted {d}", .{ session.textCount(), wanted });
        } else if (is(named, "title")) {
            const shown = std.mem.span(c.SDL_GetWindowTitle(window.window));
            var buffer: [512]u8 = undefined;
            const wanted = std.fmt.bufPrint(&buffer, "{s} - {s}", .{ named.arg, ui_mod.app_title }) catch return error.TitleTooLong;
            if (!std.mem.eql(u8, shown, wanted)) return self.fail("expect=title: the window says \"{s}\", wanted \"{s}\"", .{ shown, wanted });
        } else if (is(named, "dirty")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if (session.dirty() != wanted) return self.fail("expect=dirty: dirty is {}, wanted {}", .{ session.dirty(), wanted });
        } else if (is(named, "original_has")) {
            if (std.mem.indexOf(u8, session.original.text(), named.arg) == null) return self.fail("expect=original_has: the original \"{s}\" lacks \"{s}\"", .{ session.original.text(), named.arg });
        } else if (is(named, "translation")) {
            if (!std.mem.eql(u8, session.translation.text(), named.arg)) return self.fail("expect=translation: the box holds \"{s}\", wanted \"{s}\"", .{ session.translation.text(), named.arg });
        } else if (is(named, "disk") or is(named, "state")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("{s} needs <key>=<value>", .{named.name});
            const text_key = named.arg[0..eq];
            const wanted = named.arg[eq + 1 ..];
            var database = try elk.database.Database.open(gpa, ui.io, self.last_path[0..self.last_path_len]);
            defer database.deinit();
            const element = elementOf(&database, text_key) orelse return self.fail("{s}: no text {s} in the database", .{ named.name, text_key });
            if (is(named, "disk")) {
                const on_disk = try database.readTranslation(gpa, element, text_key);
                defer gpa.free(on_disk);
                if (!std.mem.eql(u8, on_disk, wanted)) return self.fail("expect=disk: {s} holds \"{s}\" on disk, wanted \"{s}\"", .{ text_key, on_disk, wanted });
            } else {
                const property = try database.readState(element, text_key);
                if (!std.mem.eql(u8, @tagName(property.state), wanted)) return self.fail("expect=state: {s} is {s} on disk, wanted {s}", .{ text_key, @tagName(property.state), wanted });
            }
        } else if (is(named, "row") or is(named, "folder")) {
            const eq = std.mem.lastIndexOfScalar(u8, named.arg, '=') orelse return self.fail("{s} needs <path>=<state>", .{named.name});
            const path = named.arg[0..eq];
            const wanted = named.arg[eq + 1 ..];
            const node = (if (is(named, "row")) textNode(session, path) else folderNode(session, path)) orelse
                return self.fail("expect={s}: no {s} in the tree", .{ named.name, path });
            const drawn = drawnColour(ui, node);
            if (std.mem.eql(u8, wanted, "hidden")) {
                if (drawn != null) return self.fail("expect={s}: the tree shows {s}", .{ named.name, path });
                return;
            }
            const state = std.meta.stringToEnum(elk.state.State, wanted) orelse return self.fail("expect={s}: no state {s}", .{ named.name, wanted });
            const colour = drawn orelse return self.fail("expect={s}: the tree does not show {s}", .{ named.name, path });
            const shown = session.tree.?.nodes[node].state;
            if (shown != state) return self.fail("expect={s}: {s} is {s} in the tree, wanted {s}", .{ named.name, path, @tagName(shown), wanted });
            if (colour != ui_mod.stateColour(state)) return self.fail("expect={s}: {s} was drawn in {X:0>8}, {s} is {X:0>8}", .{ named.name, path, colour, wanted, ui_mod.stateColour(state) });
        } else if (is(named, "selected")) {
            const index = session.selected orelse return self.fail("expect=selected: no text is selected", .{});
            const shown = session.textAt(index).key;
            if (!std.mem.eql(u8, shown, named.arg)) return self.fail("expect=selected: {s} is selected, wanted {s}", .{ shown, named.arg });
        } else if (is(named, "filter")) {
            if (!std.mem.eql(u8, ui.currentFilterName(), named.arg)) return self.fail("expect=filter: the filter is {s}, wanted {s}", .{ ui.currentFilterName(), named.arg });
        } else if (is(named, "browsable")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("browsable needs <step>=<0|1>", .{});
            const browse_step = std.meta.stringToEnum(ui_mod.Step, named.arg[0..eq]) orelse return self.fail("expect=browsable: no step {s}", .{named.arg[0..eq]});
            const wanted = std.mem.eql(u8, named.arg[eq + 1 ..], "1");
            if (ui.canBrowse(browse_step) != wanted) return self.fail("expect=browsable: Browse > {s} enabled is {}, wanted {}", .{ named.arg[0..eq], !wanted, wanted });
        } else if (is(named, "dimmed")) {
            const eq = std.mem.lastIndexOfScalar(u8, named.arg, '=') orelse return self.fail("dimmed needs <path>=<0|1>", .{});
            const path = named.arg[0..eq];
            const node = textNode(session, path) orelse folderNode(session, path) orelse return self.fail("expect=dimmed: no {s} in the tree", .{path});
            const wanted = std.mem.eql(u8, named.arg[eq + 1 ..], "1");
            const row = drawnRow(ui, node) orelse return self.fail("expect=dimmed: the tree does not show {s}", .{path});
            if (row.dimmed != wanted) return self.fail("expect=dimmed: {s} dimmed is {}, wanted {}", .{ path, row.dimmed, wanted });
        } else if (is(named, "collapse_item")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if (ui.settings.collapse_item != wanted) return self.fail("expect=collapse_item: it is {}, wanted {}", .{ !wanted, wanted });
        } else if (is(named, "expanded")) {
            const eq = std.mem.lastIndexOfScalar(u8, named.arg, '=') orelse return self.fail("expanded needs <folder>=<0|1>", .{});
            const node = folderNode(session, named.arg[0..eq]) orelse return self.fail("expect=expanded: no folder {s}", .{named.arg[0..eq]});
            const wanted = std.mem.eql(u8, named.arg[eq + 1 ..], "1");
            if (ui.expanded.items[node] != wanted) return self.fail("expect=expanded: {s} open is {}, wanted {}", .{ named.arg[0..eq], !wanted, wanted });
        } else if (is(named, "filter_dialog")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if ((ui.filter_dialog != null) != wanted) return self.fail("expect=filter_dialog: open is {}, wanted {}", .{ !wanted, wanted });
        } else if (is(named, "filter_count")) {
            const wanted = std.fmt.parseInt(usize, named.arg, 10) catch return self.fail("filter_count needs a number", .{});
            if (ui.filterCount() != wanted) return self.fail("expect=filter_count: the bar offers {d}, wanted {d}", .{ ui.filterCount(), wanted });
        } else if (is(named, "filter_at")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("filter_at needs <index>=<name>", .{});
            const index = std.fmt.parseInt(usize, named.arg[0..eq], 10) catch return self.fail("filter_at needs a number", .{});
            if (index >= ui.filterCount()) return self.fail("expect=filter_at: the bar offers only {d}", .{ui.filterCount()});
            const shown = ui.filterAt(index).name;
            if (!std.mem.eql(u8, shown, named.arg[eq + 1 ..])) return self.fail("expect=filter_at: {d} is {s}, wanted {s}", .{ index, shown, named.arg[eq + 1 ..] });
        } else if (is(named, "find_dialog")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if (ui.find_open != wanted) return self.fail("expect=find_dialog: open is {}, wanted {}", .{ !wanted, wanted });
        } else if (is(named, "found")) {
            try self.found(ui, named.arg);
        } else if (is(named, "find_text")) {
            if (!std.mem.eql(u8, ui.settings.findText(), named.arg)) return self.fail("expect=find_text: elk.cfg keeps \"{s}\", wanted \"{s}\"", .{ ui.settings.findText(), named.arg });
        } else if (is(named, "find_option")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("find_option needs <option>=<0|1>", .{});
            const option = std.meta.stringToEnum(ui_mod.Ui.FindOption, named.arg[0..eq]) orelse return self.fail("expect=find_option: no option {s}", .{named.arg[0..eq]});
            const on = switch (option) {
                .down => ui.settings.find_down,
                .match_case => ui.settings.find_match_case,
                .whole_word => ui.settings.find_whole_word,
            };
            if (on != std.mem.eql(u8, named.arg[eq + 1 ..], "1")) return self.fail("expect=find_option: {s} is {}", .{ named.arg[0..eq], on });
        } else if (is(named, "statistics")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if (ui.statistics_drawn != wanted) return self.fail("expect=statistics: drawn is {}, wanted {}", .{ !wanted, wanted });
        } else if (is(named, "statistic")) {
            try self.statistic(ui, named.arg);
        } else if (is(named, "can_spell")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if (ui.canSpell() != wanted) return self.fail("expect=can_spell: enabled is {}, wanted {} ({s})", .{ !wanted, wanted, ui.spelling().reason() });
        } else if (is(named, "spell_reason_has")) {
            const reason = ui.spelling().reason();
            if (std.mem.indexOf(u8, reason, named.arg) == null) return self.fail("expect=spell_reason_has: the reason \"{s}\" lacks \"{s}\"", .{ reason, named.arg });
        } else if (is(named, "spell_language")) {
            const loaded = ui.spelling().languageName();
            if (!std.mem.eql(u8, loaded, named.arg)) return self.fail("expect=spell_language: {s} is loaded, wanted {s}", .{ loaded, named.arg });
        } else if (is(named, "spelling")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if (ui.spell_drawn != wanted) return self.fail("expect=spelling: drawn is {}, wanted {}", .{ !wanted, wanted });
        } else if (is(named, "misspelt")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("misspelt needs <word>=<0|1>", .{});
            const listed = spellRow(ui, named.arg[0..eq]) != null;
            if (listed != std.mem.eql(u8, named.arg[eq + 1 ..], "1")) return self.fail("expect=misspelt: {s} listed is {}", .{ named.arg[0..eq], listed });
        } else if (is(named, "suggestion")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("suggestion needs <word>=<suggestion>", .{});
            const index = spellRow(ui, named.arg[0..eq]) orelse return self.fail("expect=suggestion: {s} is not listed", .{named.arg[0..eq]});
            const suggestions = ui.spell_results[index].suggestions;
            if (suggestions.len == 0 or !std.mem.eql(u8, suggestions[0], named.arg[eq + 1 ..]))
                return self.fail("expect=suggestion: {s} has {d} suggestions, wanted {s} first", .{ named.arg[0..eq], suggestions.len, named.arg[eq + 1 ..] });
        } else if (is(named, "user_word")) {
            try self.userWord(ui, named.arg);
        } else if (is(named, "can_export") or is(named, "can_run_game")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            const enabled = if (is(named, "can_export")) ui.canExport() else ui.canRunGame();
            if (enabled != wanted) return self.fail("expect={s}: enabled is {}, wanted {}", .{ named.name, enabled, wanted });
        } else if (is(named, "export_dialog")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if (ui.export_open != wanted) return self.fail("expect=export_dialog: open is {}, wanted {}", .{ ui.export_open, wanted });
        } else if (is(named, "install_warning")) {
            const wanted = std.mem.eql(u8, named.arg, "1");
            if ((ui.install_warning != null) != wanted) return self.fail("expect=install_warning: open is {}, wanted {}", .{ !wanted, wanted });
        } else if (is(named, "warning")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("warning needs <count>=<folder>", .{});
            const shadow = ui.install_warning orelse return self.fail("expect=warning: no shadow warning is up", .{});
            const count = std.fmt.parseInt(usize, named.arg[0..eq], 10) catch return self.fail("warning needs a count", .{});
            const folder = std.fs.path.basename(shadow.folder.?);
            if (shadow.count != count or !std.mem.eql(u8, folder, named.arg[eq + 1 ..]))
                return self.fail("expect=warning: {d} texts in {s}, wanted {s}", .{ shadow.count, shadow.folder.?, named.arg });
        } else if (is(named, "pak")) {
            try self.pak(ui, named.arg);
        } else if (is(named, "notice_has")) {
            if (std.mem.indexOf(u8, ui.notice(), named.arg) == null) return self.fail("expect=notice_has: the summary \"{s}\" lacks \"{s}\"", .{ ui.notice(), named.arg });
        } else if (is(named, "imported")) {
            const counts = ui.imported orelse return self.fail("expect=imported: nothing was imported", .{});
            var buffer: [96]u8 = undefined;
            const shown = try std.fmt.bufPrint(&buffer, "{d}/{d}/{d}/{d}/{d}", .{ counts.rows, counts.changed, counts.outdated, counts.empty, counts.missing });
            if (!std.mem.eql(u8, shown, named.arg)) return self.fail("expect=imported: the import counted {s}, wanted {s} ({s})", .{ shown, named.arg, ui.notice() });
        } else if (is(named, "failure_has")) {
            if (std.mem.indexOf(u8, session.failureText(), named.arg) == null) return self.fail("expect=failure_has: the failure \"{s}\" lacks \"{s}\"", .{ session.failureText(), named.arg });
        } else if (is(named, "launched")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("launched needs <index>=<argument>", .{});
            const index = std.fmt.parseInt(usize, named.arg[0..eq], 10) catch return self.fail("launched needs a number", .{});
            const shown = ui.launchedArgument(index) orelse return self.fail("expect=launched: Run Game started no argument {d}", .{index});
            if (!std.mem.eql(u8, shown, named.arg[eq + 1 ..])) return self.fail("expect=launched: argument {d} is {s}, wanted {s}", .{ index, shown, named.arg[eq + 1 ..] });
        } else if (is(named, "untouched")) {
            try self.untouched(ui, named.arg);
        } else if (is(named, "identical")) {
            try self.identical(ui);
        } else return self.fail("unknown expectation expect={s}", .{named.name});
    }

    /// `<key>=<field>:<offset>` or `none` against the last Find Next.
    fn found(self: *Runner, ui: *ui_mod.Ui, arg: []const u8) !void {
        const session = ui.session;
        const last = session.last_hit;
        if (std.mem.eql(u8, arg, "none")) {
            if (last) |hit| return self.fail("expect=found: the last Find Next found {s}", .{session.textAt(hit.text).key});
            if (!ui.find_missed) return self.fail("expect=found: no Find Next missed", .{});
            return;
        }
        const eq = std.mem.lastIndexOfScalar(u8, arg, '=') orelse return self.fail("found needs <key>=<field>:<offset>", .{});
        const colon = std.mem.indexOfScalarPos(u8, arg, eq, ':') orelse return self.fail("found needs <key>=<field>:<offset>", .{});
        const hit = last orelse return self.fail("expect=found: the last Find Next found nothing", .{});
        const key = session.textAt(hit.text).key;
        const offset = std.fmt.parseInt(usize, arg[colon + 1 ..], 10) catch return self.fail("found needs a byte offset", .{});
        if (!std.mem.eql(u8, key, arg[0..eq]) or !std.mem.eql(u8, @tagName(hit.field), arg[eq + 1 .. colon]) or hit.offset != offset)
            return self.fail("expect=found: the hit is {s}={s}:{d}, wanted {s}", .{ key, @tagName(hit.field), hit.offset, arg });
        if (session.selected != hit.text) return self.fail("expect=found: {s} is not selected", .{key});
    }

    /// `<half>/<row>=<items>/<words>/<word symbols>/<symbols>` against the
    /// counts the statistic window draws.
    fn statistic(self: *Runner, ui: *ui_mod.Ui, arg: []const u8) !void {
        if (!ui.statistics_drawn) return self.fail("expect=statistic: the statistic window is not shown", .{});
        const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return self.fail("statistic needs <half>/<row>=<counts>", .{});
        const slash = std.mem.indexOfScalar(u8, arg[0..eq], '/') orelse return self.fail("statistic needs <half>/<row>", .{});
        const half = arg[0..slash];
        if (!std.mem.eql(u8, half, "original") and !std.mem.eql(u8, half, "translation")) return self.fail("expect=statistic: no half {s}", .{half});
        const row = arg[slash + 1 .. eq];
        var element: ?usize = null;
        var state: ?elk.state.State = null;
        if (!std.mem.eql(u8, row, "total")) {
            const dot = std.mem.indexOfScalar(u8, row, '.');
            const head = row[0 .. dot orelse row.len];
            if (std.fmt.parseInt(usize, head, 10)) |number| {
                element = number;
                if (dot) |at| state = std.meta.stringToEnum(elk.state.State, row[at + 1 ..]) orelse return self.fail("expect=statistic: no state {s}", .{row[at + 1 ..]});
            } else |_| {
                state = std.meta.stringToEnum(elk.state.State, row) orelse return self.fail("expect=statistic: no row {s}", .{row});
            }
        }
        const counts = ui.statisticRow(std.mem.eql(u8, half, "translation"), element, state) orelse return self.fail("expect=statistic: no row {s}", .{row});
        var buffer: [64]u8 = undefined;
        const shown = try std.fmt.bufPrint(&buffer, "{d}/{d}/{d}/{d}", .{ counts.texts, counts.words, counts.word_symbols, counts.symbols });
        if (!std.mem.eql(u8, shown, arg[eq + 1 ..])) return self.fail("expect=statistic: {s} is {s}, wanted {s}", .{ arg[0..eq], shown, arg[eq + 1 ..] });
    }

    /// `<mod|game>=<entries|none>`: the pak where the core exports to,
    /// read back by the core's own reader.
    fn pak(self: *Runner, ui: *ui_mod.Ui, arg: []const u8) !void {
        const eq = std.mem.lastIndexOfScalar(u8, arg, '=') orelse return self.fail("pak needs <mod|game>=<entries|none>", .{});
        var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const game = ui.gameFolder(&game_buffer) orelse return self.fail("expect=pak: the game folder is unknown", .{});
        const where = arg[0..eq];
        const destination: elk.packages.Destination = if (std.mem.eql(u8, where, "game")) .game else .{ .mod = where };
        const path = try elk.packages.destinationPath(ui.gpa, ui.io, game, destination);
        defer ui.gpa.free(path);
        const wanted = arg[eq + 1 ..];
        var package = elk.packages.readArchive(ui.gpa, ui.io, path) catch |err| {
            if (err == error.FileNotFound and std.mem.eql(u8, wanted, "none")) return;
            return self.fail("expect=pak: {s} does not read back: {s}", .{ path, @errorName(err) });
        };
        defer package.deinit();
        if (std.mem.eql(u8, wanted, "none")) return self.fail("expect=pak: {s} exists", .{path});
        const count = std.fmt.parseInt(usize, wanted, 10) catch return self.fail("pak needs a count or none", .{});
        if (package.entries.len != count) return self.fail("expect=pak: {s} holds {d} texts, wanted {d}", .{ path, package.entries.len, count });
    }

    /// The row's translation and state files compare byte for byte with
    /// the source the smoke copied: visiting a text never rewrites it.
    fn untouched(self: *Runner, ui: *ui_mod.Ui, arg: []const u8) !void {
        if (self.source_folder.len == 0) return self.fail("expect=untouched needs the smoke's source", .{});
        const index = std.fmt.parseInt(usize, arg, 10) catch return self.fail("untouched needs a row number", .{});
        const session = ui.session;
        const text = session.textAt(index);
        const database = &session.database.?;
        const base = database.bases[text.element].path.?;
        const relative_base = base[database.folder.len + 1 ..];
        const cwd = std.Io.Dir.cwd();
        for ([_][]const u8{ ".txt", ".xml", ".elk" }) |extension| {
            var copy_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            var source_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            const copy_path = try std.fmt.bufPrint(&copy_buffer, "{s}/{s}{s}", .{ base, text.key, extension });
            const source_path = try std.fmt.bufPrint(&source_buffer, "{s}/{s}/{s}{s}", .{ self.source_folder, relative_base, text.key, extension });
            const copied = cwd.readFileAlloc(ui.io, copy_path, ui.gpa, .limited(16 << 20)) catch null;
            defer if (copied) |bytes| ui.gpa.free(bytes);
            const source = cwd.readFileAlloc(ui.io, source_path, ui.gpa, .limited(16 << 20)) catch null;
            defer if (source) |bytes| ui.gpa.free(bytes);
            const same = if (copied != null and source != null) std.mem.eql(u8, copied.?, source.?) else copied == null and source == null;
            if (!same) return self.fail("expect=untouched: {s}{s} changed though it was only visited", .{ text.key, extension });
        }
    }

    /// Every file under the opened database's folder equals the source's
    /// file of the same name, and there are as many as the smoke copied: a
    /// Save of an untouched database wrote nothing that differs.
    fn identical(self: *Runner, ui: *ui_mod.Ui) !void {
        if (self.source_folder.len == 0) return self.fail("expect=identical needs the smoke's source", .{});
        const folder = std.fs.path.dirname(self.last_path[0..self.last_path_len]) orelse return self.fail("expect=identical: no database was opened", .{});
        const cwd = std.Io.Dir.cwd();
        var copy = try cwd.openDir(ui.io, folder, .{ .iterate = true });
        defer copy.close(ui.io);
        var source = try cwd.openDir(ui.io, self.source_folder, .{});
        defer source.close(ui.io);
        var walker = try copy.walk(ui.gpa);
        defer walker.deinit();
        var count: usize = 0;
        while (try walker.next(ui.io)) |file| {
            if (file.kind != .file) continue;
            count += 1;
            const copied = try copy.readFileAlloc(ui.io, file.path, ui.gpa, .limited(16 << 20));
            defer ui.gpa.free(copied);
            const original = source.readFileAlloc(ui.io, file.path, ui.gpa, .limited(16 << 20)) catch
                return self.fail("expect=identical: {s} is new", .{file.path});
            defer ui.gpa.free(original);
            if (!std.mem.eql(u8, copied, original)) return self.fail("expect=identical: {s} differs from the source", .{file.path});
        }
        if (count != self.source_files) return self.fail("expect=identical: {d} files, the smoke copied {d}", .{ count, self.source_files });
    }

    /// The word read back from custom.dic by the core, as a restart reads it.
    fn userWord(self: *Runner, ui: *ui_mod.Ui, word: []const u8) !void {
        const path = ui.spelling().user_path orelse return self.fail("expect=user_word: this run keeps no custom.dic", .{});
        var user = try elk.spell.UserDictionary.load(ui.gpa, ui.io, path);
        defer user.deinit(ui.gpa);
        if (!user.contains(word)) return self.fail("expect=user_word: {s} does not hold {s}", .{ path, word });
    }

    fn fail(self: *Runner, comptime format: []const u8, args: anytype) void {
        self.failure = std.fmt.bufPrint(&self.failure_storage, format, args) catch self.failure_storage[0..];
    }
};

/// The Spelling window's first row for `word`.
fn spellRow(ui: *const ui_mod.Ui, word: []const u8) ?usize {
    for (ui.spell_results, 0..) |result, index| if (std.mem.eql(u8, result.text, word)) return index;
    return null;
}

/// The tree row of the text `key`, in the first element that has it.
fn textNode(session: *const ui_mod.Session, key: []const u8) ?u32 {
    const index = session.find(key) orelse return null;
    return session.tree.?.node_of_text[index];
}

/// The tree row of the folder `path`, in the first element that has it.
fn folderNode(session: *const ui_mod.Session, path: []const u8) ?u32 {
    const tree = &(session.tree orelse return null);
    const database = &session.database.?;
    for (0..database.bases.len) |element| {
        if (tree.findFolder(element, path)) |node| return node;
    }
    return null;
}

/// The state colour of `node`'s row in the last frame; null when it drew none.
fn drawnColour(ui: *const ui_mod.Ui, node: u32) ?u32 {
    const row = drawnRow(ui, node) orelse return null;
    return row.colour;
}

fn drawnRow(ui: *const ui_mod.Ui, node: u32) ?ui_mod.Ui.Drawn {
    for (ui.drawn.items) |row| if (row.node == node) return row;
    return null;
}

/// The first element whose texts include `key`.
fn elementOf(database: *const elk.database.Database, key: []const u8) ?usize {
    for (database.bases, 0..) |base, element| {
        for (base.keys) |candidate| if (std.mem.eql(u8, candidate, key)) return element;
    }
    return null;
}

fn is(named: schedule.Named, name: []const u8) bool {
    return std.mem.eql(u8, named.name, name);
}
