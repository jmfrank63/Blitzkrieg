//! ELK, the Blitzkrieg localisation kit (D-01): its own executable, no engine.
//!
//!   ELK [elk.xml] [--developer | --short | -short]
//!   ELK --smoke <database folder> <scratch folder>
//!   ELK --auto <elk.xml>            (the schedule in BK_ELK_AUTO)
//!
//! Without a path ELK reopens the most recent database from `elk.cfg`. This
//! slice is the extended layout (D-04's default); `--short` is accepted and
//! starts the extended layout until the translator layout exists.
//! `--smoke` copies the database's `elk.xml` and `<element>_data_base/`
//! folders into the scratch folder, opens the copy in a hidden window and
//! replays a fixed schedule over the tree, the edit boxes and every save
//! point, then copies it again beside the first and opens that one through
//! File > Open, answering the dialog itself: an untouched Save leaves every
//! file byte-identical, an edit reaches the disk, and the path lands first
//! in the run's own elk.cfg. A third copy proves the tree, the state buttons
//! and the autosave: the tree is opened down to a text, its state set by the
//! buttons and seen in the folders' colours, an edit autosaved, the frame
//! loop killed with a later edit unsaved, and the copy reopened with the
//! autosaved text in it. A fourth copy proves the filters and browsing:
//! the Translated filter chosen in the bar, the tree dimming what it
//! rejects, the Browse commands and hot keys walking only its four texts,
//! and a second run on the same elk.cfg starting with that filter again.
//! A third run there proves Collapse Item and the Create Filter dialog: a
//! step closes the folders it leaves only while the mode is on, a filter is
//! created, applied, renamed in place and deleted while current (the bar
//! falls back to All), Cancel keeps what it would have dropped, and a
//! fourth run finds the dialog's result and the mode in elk.cfg. A fifth
//! copy proves View > ELK Statistic, its counts asserted against the
//! numbers of the tracked database, and Edit > Find...: hits in the
//! translation and the original, the resume within a field, the wrap, Match
//! case and Whole word missing, Up reaching the text above, and a second
//! run finding the text and options in elk.cfg. A sixth copy proves the pak
//! export against a scratch game folder, never a real one: File > Export to
//! language mod by name and by a folder answered for Browse..., each pak
//! read back by the core with all 2,852 texts; Install to game warning
//! about the scratch game's loose Data/Textes, Enter taking the mod route,
//! then Install anyway writing Data/Texts.pak; Run Game's command line, and
//! on Linux and macOS a stand-in Game script recording the arguments it got;
//! the commands disabled once the database is closed.
//! `--auto` replays `BK_ELK_AUTO` on the database given.
const std = @import("std");
const builtin = @import("builtin");
const kit = @import("editor_kit");
const sdl3 = @import("sdl3");
const elk = @import("elk_core");
const bk_memory = @import("bk_memory_allocator");
const window_mod = kit.window;
const fonts = @import("fonts.zig");
const session_mod = @import("session.zig");
const settings_mod = @import("settings.zig");
const ui_mod = @import("ui.zig");
const auto = @import("auto.zig");

const crt = kit.crt;
const c = sdl3.c;

/// The smoke's text and what it types into it: one leave, one Ctrl+S, one
/// Close and one Exit, each adding a letter the disk must then hold.
const smoke_key = "medals/1/name";
const smoke_final = "ELK-SMOKEXYZ";
const smoke_schedule =
    "2:expect=open,2:expect=texts:2852,2:expect=glyphs,3:expect=rows,3:expect=title:elk.xml," ++
    "4:do=select_key:" ++ smoke_key ++ ",6:expect=original_has:Medal,6:expect=dirty:0," ++
    "6:do=focus:translation,9:text=ELK-SMOKE,12:expect=translation:ELK-SMOKE,12:expect=dirty:1," ++
    "12:expect=disk:" ++ smoke_key ++ "=," ++
    // Leaving the text is a save point; visiting row 0 writes nothing.
    "13:do=select:0,15:expect=disk:" ++ smoke_key ++ "=ELK-SMOKE,15:expect=state:" ++ smoke_key ++ "=translated," ++
    "16:do=select_key:" ++ smoke_key ++ ",17:expect=untouched:0," ++
    // Ctrl+S, typed while the translation box has the keyboard.
    "18:do=focus:translation,21:key=end+ctrl,23:text=X,25:expect=translation:ELK-SMOKEX," ++
    "26:key=s+ctrl,28:expect=disk:" ++ smoke_key ++ "=ELK-SMOKEX,28:expect=dirty:0," ++
    // Close.
    "29:key=end+ctrl,31:text=Y,33:expect=translation:ELK-SMOKEXY,33:do=close,35:expect=closed," ++
    "35:expect=disk:" ++ smoke_key ++ "=ELK-SMOKEXY," ++
    // Exit, checked on disk after the window is gone.
    "36:do=reopen,38:expect=open,38:do=select_key:" ++ smoke_key ++ ",40:do=focus:translation," ++
    "43:key=end+ctrl,45:text=Z,47:expect=dirty:1,48:exit";

/// The File > Open pass: nothing open at the start, File > Open answered
/// with the copy, a visit and a Save that must change no byte, then an edit
/// that must reach the disk.
const open_final = "ELK-OPEN";
const open_schedule =
    "2:expect=closed,3:do=open,4:do=answer:elk.xml,6:expect=open,6:expect=texts:2852,6:expect=title:elk.xml," ++
    "7:do=select_key:" ++ smoke_key ++ ",9:do=select:0,10:do=save,11:expect=identical," ++
    "12:do=select_key:" ++ smoke_key ++ ",13:do=focus:translation,16:text=" ++ open_final ++ ",18:expect=dirty:1," ++
    "19:do=save,20:expect=disk:" ++ smoke_key ++ "=" ++ open_final ++ ",20:expect=dirty:0,21:exit";

/// The tree, state and autosave pass. medals/1 holds two texts, both Not
/// translated in Data/ELK: approving one leaves the folder red (the lowest
/// state below it), approving both turns it blue. The autosave clock is
/// moved 29 s (no write), then 2 s more (the write); the kill then drops a
/// later edit, which no save point may have written.
const tree_name = "medals/1/name";
const tree_desc = "medals/1/desc";
const autosaved = "ELK-AUTO";
const tree_schedule =
    "2:expect=open,3:expect=rows,3:expect=folder:medals=not_translated,3:expect=row:" ++ tree_name ++ "=hidden," ++
    "3:do=expand:medals/1,5:expect=folder:medals/1=not_translated,5:expect=row:" ++ tree_name ++ "=not_translated," ++
    "5:do=click:" ++ tree_name ++ ",7:expect=original_has:Medal,7:do=state:approved," ++
    "9:expect=state:" ++ tree_name ++ "=approved,9:expect=row:" ++ tree_name ++ "=approved,9:expect=folder:medals/1=not_translated," ++
    "9:do=click:" ++ tree_desc ++ ",11:do=state:approved,13:expect=row:" ++ tree_desc ++ "=approved,13:expect=folder:medals/1=approved," ++
    "13:do=state:not_translated,15:expect=folder:medals/1=not_translated,15:expect=state:" ++ tree_desc ++ "=not_translated," ++
    // The autosave, on the approved name selected again: the button chose
    // the state of the earlier visit only, so the edit turns it Translated
    // (InputViewWindow cleared bManualState on each text it loaded).
    "16:do=click:" ++ tree_name ++ ",18:do=focus:translation,21:text=" ++ autosaved ++ ",23:expect=dirty:1," ++
    "24:do=wait:29,26:expect=dirty:1,26:expect=disk:" ++ tree_name ++ "=," ++
    "27:do=wait:2,29:expect=dirty:0,29:expect=disk:" ++ tree_name ++ "=" ++ autosaved ++ ",29:expect=state:" ++ tree_name ++ "=translated," ++
    "30:key=end+ctrl,32:text=LOST,34:expect=dirty:1,35:do=kill";
/// The reopen after the kill: the autosaved text is in the edit box and
/// the tree shows the text where it was left.
const reopen_schedule =
    "2:expect=open,2:do=select_key:" ++ tree_name ++ ",5:expect=translation:" ++ autosaved ++ "," ++
    "5:expect=row:" ++ tree_name ++ "=translated,5:expect=dirty:0,6:exit";

/// The filter pass. Data/ELK's four Translated texts, in the tree's order;
/// everything else (medals/1 among it) is dimmed while the filter is
/// current. The walk starts with Alt+N from the medal, a text the filter
/// rejects, before the translation box has the keyboard (ImGui keeps Alt
/// chords for the box while it is active), then goes by Ctrl+N, Ctrl+P and
/// the menu's commands, and stops at either end.
const walk = [_][]const u8{
    "scenarios/descriptions/defend_area00/1h",
    "scenarios/descriptions/defend_area00/header",
    "scenarios/descriptions/escort00/0h",
    "scenarios/descriptions/sa_10_12_13_15_16/0",
};
const filter_schedule =
    "2:expect=open,2:expect=filter:All,3:expect=rows,3:expect=browsable:next=0,3:expect=browsable:first=1," ++
    "3:do=expand:medals/1,5:expect=dimmed:" ++ tree_name ++ "=0,5:do=filter:Translated,7:expect=filter:Translated," ++
    "7:expect=dimmed:" ++ tree_name ++ "=1,7:expect=dimmed:medals=1," ++
    "8:do=select_key:" ++ tree_name ++ ",9:key=n+alt,11:expect=selected:" ++ walk[0] ++ ",11:expect=dimmed:" ++ walk[0] ++ "=0," ++
    "11:expect=dimmed:scenarios/descriptions=0,11:expect=browsable:previous=0,11:expect=browsable:next=1," ++
    "12:do=browse:next,14:expect=selected:" ++ walk[1] ++ ",15:key=n+ctrl,17:expect=selected:" ++ walk[2] ++ "," ++
    "18:key=n+ctrl,20:expect=selected:" ++ walk[3] ++ ",20:expect=browsable:next=0," ++
    "21:key=n+ctrl,23:expect=selected:" ++ walk[3] ++ "," ++
    "24:key=p+ctrl,26:expect=selected:" ++ walk[2] ++ ",27:do=browse:previous,29:expect=selected:" ++ walk[1] ++ "," ++
    "29:do=browse:last,31:expect=selected:" ++ walk[3] ++ ",32:do=browse:first,34:expect=selected:" ++ walk[0] ++ "," ++
    "35:exit";
/// The second run on the same elk.cfg: the filter is still Translated and
/// the walk still keeps to it.
const filter_reopen_schedule =
    "2:expect=open,2:expect=filter:Translated,3:do=browse:first,5:expect=selected:" ++ walk[0] ++ "," ++
    "6:do=browse:next,8:expect=selected:" ++ walk[1] ++ ",9:exit";

/// The Collapse Item and Create Filter pass, a third run on the filter
/// pass's copy and elk.cfg. Collapse Item is on as the MFC ELK started:
/// stepping from defend_area00 to escort00 closes the first; turned off,
/// the way back leaves escort00 open. The created filter keeps the
/// Outdated texts below medals/german/krest1 to krest5: each desc, and
/// krest4's name, the other names being Approved in Data/ELK.
const krest_first = "medals/german/krest1/desc";
const krest_second = "medals/german/krest2/desc";
const krest_last = "medals/german/krest5/desc";
const defend = "scenarios/descriptions/defend_area00";
const escort = "scenarios/descriptions/escort00";
const filter_edit_schedule =
    "2:expect=open,2:expect=filter:Translated,2:expect=collapse_item:1," ++
    "3:do=browse:first,5:expect=selected:" ++ walk[0] ++ ",5:expect=expanded:" ++ defend ++ "=1," ++
    "6:do=browse:next,8:do=browse:next,10:expect=selected:" ++ walk[2] ++ "," ++
    "10:expect=expanded:" ++ defend ++ "=0,10:expect=expanded:" ++ escort ++ "=1," ++
    "11:do=collapse_item,12:expect=collapse_item:0,12:do=browse:previous,14:expect=selected:" ++ walk[1] ++ "," ++
    "14:expect=expanded:" ++ escort ++ "=1,14:expect=expanded:" ++ defend ++ "=1," ++
    // Create, then apply.
    "15:do=filter_dialog,17:expect=filter_dialog:1,17:do=filter_add:Krest,18:do=filter_states:0100," ++
    "18:do=filter_condition:medals&krest,18:do=filter_ok,20:expect=filter_dialog:0,20:expect=filter_count:6," ++
    "20:expect=filter_at:5=Krest,20:expect=filter:Translated," ++
    "21:do=filter:Krest,22:do=browse:first,24:expect=selected:" ++ krest_first ++ ",24:do=browse:next,26:expect=selected:" ++ krest_second ++ "," ++
    "26:do=browse:last,26:expect=selected:" ++ krest_last ++ "," ++
    "26:expect=dimmed:medals/german=0,26:expect=dimmed:medals/german/kuban=1," ++
    // Rename in place: the current filter follows the name.
    "27:do=filter_dialog,29:do=filter_rename:Iron_crosses,29:do=filter_ok,31:expect=filter_at:5=Iron_crosses," ++
    "31:expect=filter:Iron_crosses,31:expect=filter_count:6," ++
    // Cancel drops a delete; OK on a delete of the current one falls back to All.
    "32:do=filter_dialog,34:do=filter_delete,34:do=filter_cancel,36:expect=filter_dialog:0,36:expect=filter_count:6," ++
    "37:do=filter_dialog,39:do=filter_pick:Iron_crosses,39:do=filter_delete,39:do=filter_ok,41:expect=filter:All," ++
    "41:expect=filter_count:5,41:expect=browsable:next=1," ++
    // One filter for the restart to find.
    "42:do=filter_dialog,44:do=filter_add:Kept,44:do=filter_ok,46:expect=filter_at:5=Kept,47:exit";
const filter_edit_reopen_schedule =
    "2:expect=open,2:expect=filter:All,2:expect=collapse_item:0,2:expect=filter_count:6,2:expect=filter_at:5=Kept,3:exit";

/// The find and statistics pass. Data/ELK's counts (stats.zig's test has
/// the same numbers). "kuban" is in medals/german/kuban/desc's translation
/// twice and in the name's original once, nowhere else; the Russian
/// original of the desc says "Кубанском", which Whole word "Kuba" misses.
/// "Rommel" is in Tunis's original and translation, Tobruk (above it) and
/// Africa (below it).
const kuban_desc = "medals/german/kuban/desc";
const kuban_name = "medals/german/kuban/name";
const tunis = "scenarios/chapters/allies/tunis/description";
const tobruk = "scenarios/chapters/allies/tobruk/description";
const find_schedule =
    "2:expect=open,2:do=statistics,4:expect=statistics:1," ++
    "4:expect=statistic:original/total=2852/42146/223983/279715,4:expect=statistic:original/0=2852/42146/223983/279715," ++
    "4:expect=statistic:original/not_translated=885/19166/100693/122903,4:expect=statistic:translation/translated=4/55/263/316," ++
    "4:expect=statistic:translation/0.approved=593/183/1059/1180,5:do=statistics_close,7:expect=statistics:0," ++
    // From nothing selected: the three hits in the tree's order, then round.
    "8:do=find_dialog,10:expect=find_dialog:1,10:do=find_next:kuban,12:expect=found:" ++ kuban_desc ++ "=translation:4," ++
    "12:expect=expanded:medals/german/kuban=1,13:do=find_next:kuban,15:expect=found:" ++ kuban_desc ++ "=translation:68," ++
    "16:do=find_next:kuban,18:expect=found:" ++ kuban_name ++ "=original:0," ++
    "19:do=find_next:kuban,21:expect=found:" ++ kuban_desc ++ "=translation:4," ++
    // Match case and Whole word: nothing, the selection kept.
    "22:do=find_option:match_case=1,23:do=find_next:kuban,25:expect=found:none,25:expect=selected:" ++ kuban_desc ++ "," ++
    "26:do=find_option:match_case=0,26:do=find_option:whole_word=1,27:do=find_next:Kuba,29:expect=found:none," ++
    // Up from Tunis: its own two hits, then Tobruk above it.
    "30:do=find_option:whole_word=0,30:do=find_option:down=0,30:do=select_key:" ++ tunis ++ "," ++
    "31:do=find_next:Rommel,33:expect=found:" ++ tunis ++ "=original:617,34:do=find_next:Rommel,36:expect=found:" ++ tunis ++ "=translation:618," ++
    "37:do=find_next:Rommel,39:expect=found:" ++ tobruk ++ "=original:858,40:do=find_close,42:expect=find_dialog:0,43:exit";
/// The second run on the same elk.cfg: the text and Up are kept; Down from
/// nothing selected finds Tobruk, the first text that has it.
const find_reopen_schedule =
    "2:expect=open,2:expect=find_text:Rommel,2:expect=find_option:down=0,2:expect=find_option:match_case=0," ++
    "2:expect=find_option:whole_word=0,3:key=f+ctrl,5:expect=find_dialog:1,5:do=find_option:down=1," ++
    "6:do=find_next:Rommel,8:expect=found:" ++ tobruk ++ "=original:858,9:exit";

/// The pak export pass. The scratch game folder holds one loose text of the
/// database, the Exit Game label E-06 changed, so Install to game warns.
const export_mod = "ELKSmoke";
const folder_mod = "ELKFolder";
const loose_text = "Data/Textes/strings/imheader-gbutton-exitgame.txt";
const export_texts = "2852";
const export_schedule =
    "2:expect=open,2:expect=can_export:1,2:expect=can_run_game:1,2:expect=pak:game=none," ++
    // Run Game before any export asks for the mod first.
    "3:do=run_game,5:expect=export_dialog:1,5:do=export_cancel,7:expect=export_dialog:0,7:expect=pak:" ++ export_mod ++ "=none," ++
    // By name, then by the folder Browse... chose.
    "8:do=export_mod,10:do=export_name:" ++ export_mod ++ ",10:do=export_ok,12:expect=export_dialog:0," ++
    "12:expect=pak:" ++ export_mod ++ "=" ++ export_texts ++ ",12:expect=notice_has:" ++ export_texts ++ "," ++
    "13:do=export_mod,15:do=export_browse,15:do=answer:mods/" ++ folder_mod ++ ",17:do=export_ok,19:expect=export_dialog:0," ++
    "19:expect=pak:" ++ folder_mod ++ "=" ++ export_texts ++ ",19:expect=notice_has:" ++ folder_mod ++ "," ++
    // Install to game: the warning names the loose folder; Enter is the mod route.
    "20:do=install,22:expect=install_warning:1,22:expect=warning:1=Textes,23:key=return,25:expect=install_warning:0," ++
    "25:expect=export_dialog:1,25:expect=pak:game=none,25:do=export_cancel," ++
    "27:do=install,29:do=install_choice:install,31:expect=install_warning:0,31:expect=pak:game=" ++ export_texts ++ "," ++
    "31:expect=notice_has:installed," ++
    // Run Game on this session's mod, in ELK's own profile.
    "32:do=run_game,33:expect=launched:1=-editor-test,33:expect=launched:2=-profile=ELKTest," ++
    "33:expect=launched:3=-mod=" ++ folder_mod ++ ",33:expect=launched:4=-windowed," ++
    (if (stand_in_game) "34:do=wait_game,36:expect=can_run_game:1," else "34:expect=failure_has:Game.exe,") ++
    "37:do=close,39:expect=can_export:0,39:expect=can_run_game:0,40:exit";
/// Linux and macOS get a stand-in Game, a shell script that writes the
/// arguments it was started with; Windows has none, and its Run Game fails
/// to start after recording the same command line.
const stand_in_game = builtin.os.tag != .windows;
const stand_in_args = "elk-game.args";

/// A scripted run that has not exited by then has hung.
const smoke_frame_limit = 200;
const auto_frame_limit = 100_000;

pub fn main(minimal: std.process.Init.Minimal) !void {
    crt.routeCrtReportsToStderr();
    // The exit comes after run's own defers, so BkMemory's exit report
    // counts no argument list as leaked.
    std.process.exit(try run(minimal));
}

fn run(minimal: std.process.Init.Minimal) !u8 {
    const gpa = bk_memory.allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{ .environ = minimal.environ });
    defer threaded.deinit();
    const io = threaded.io();

    var args = try std.process.Args.Iterator.initAllocator(minimal.args, gpa);
    defer args.deinit();
    _ = args.next();
    var rest: std.ArrayList([]const u8) = .empty;
    defer rest.deinit(gpa);
    var short = false;
    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--short") or std.mem.eql(u8, arg, "-short")) {
            short = true;
        } else if (std.mem.eql(u8, arg, "--developer") or std.mem.eql(u8, arg, "-developer")) {
            short = false;
        } else try rest.append(gpa, arg);
    }
    const items = rest.items;
    if (items.len > 0 and std.mem.eql(u8, items[0], "--smoke")) {
        if (items.len != 3) usage();
        return smoke(gpa, io, minimal.environ, items[1], items[2]);
    }
    if (items.len > 0 and std.mem.eql(u8, items[0], "--auto")) {
        if (items.len != 2) usage();
        return autoMode(gpa, io, minimal.environ, items[1]);
    }
    if (items.len > 1 or (items.len == 1 and std.mem.startsWith(u8, items[0], "-"))) usage();
    if (short) std.debug.print("elk: the short translator layout comes in a later slice; starting the extended layout\n", .{});
    return interactive(gpa, io, minimal.environ, if (items.len == 1) items[0] else null);
}

fn usage() noreturn {
    crt.attachParentConsole();
    std.debug.print(
        \\usage: ELK [elk.xml] [--developer | --short]
        \\       ELK --smoke <database folder> <scratch folder>
        \\       ELK --auto <elk.xml>    (the schedule in BK_ELK_AUTO)
        \\
    , .{});
    std.process.exit(2);
}

fn interactive(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, first: ?[]const u8) u8 {
    var window = window_mod.Window.start(.{ .title = ui_mod.app_title }) catch |err| {
        crt.attachParentConsole();
        std.debug.print("elk: the window did not start ({s}: {s})\n", .{ @errorName(err), window_mod.failureReason() });
        return 1;
    };
    defer window.stop();
    const windir = envValue(gpa, environ, "WINDIR");
    defer if (windir) |value| gpa.free(value);
    _ = fonts.load(io, windir);

    var settings: settings_mod.Settings = .{};
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    var root_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const override = envValue(gpa, environ, settings_mod.settings_env);
    defer if (override) |value| gpa.free(value);
    const settings_path = settings_mod.settingsPath(&path_buffer, override, userRoot(gpa, environ, &root_buffer)) orelse "";
    if (settings_path.len != 0) {
        if (std.Io.Dir.cwd().readFileAlloc(io, settings_path, gpa, .limited(64 * 1024))) |bytes| {
            defer gpa.free(bytes);
            settings = settings_mod.parse(bytes);
        } else |_| {}
    }

    var session = session_mod.Session.init(gpa, io);
    defer session.deinit();
    var ui: ui_mod.Ui = .{ .gpa = gpa, .io = io, .session = &session, .settings = &settings, .settings_path = settings_path, .environ = environ };
    defer ui.deinit();
    var recent_copy: [settings_mod.max_path]u8 = undefined;
    if (first) |path| {
        ui.openDatabase(path);
    } else if (settings.recentCount() != 0) {
        const recent = settings.recentAt(0);
        @memcpy(recent_copy[0..recent.len], recent);
        ui.openDatabase(recent_copy[0..recent.len]);
    }
    const clean = loop(&window, &ui, null, std.math.maxInt(u32));
    ui.saveSettings();
    if (!clean) std.debug.print("elk: a frame failed: {s}\n", .{window_mod.failureReason()});
    return if (clean) 0 else 1;
}

/// The frame loop every mode shares. With a runner, its schedule's entries
/// for a frame run before that frame; returns false when one failed, the
/// frame failed or `frame_limit` passed first.
fn loop(window: *window_mod.Window, ui: *ui_mod.Ui, runner: ?*auto.Runner, frame_limit: u32) bool {
    var frame: u32 = 0;
    while (!ui.quit) : (frame += 1) {
        var event: c.SDL_Event = undefined;
        while (c.SDL_PollEvent(&event)) {
            _ = window.handleEvent(&event);
            if (event.type == c.SDL_EVENT_QUIT or event.type == c.SDL_EVENT_WINDOW_CLOSE_REQUESTED) ui.requestExit();
        }
        if (runner) |r| {
            if (!r.step(frame, window, ui)) return false;
            if (ui.quit or ui.killed) break;
        }
        if (frame >= frame_limit) {
            if (runner) |r| r.failure = "the schedule did not exit in time";
            return false;
        }
        window.beginFrame();
        ui.draw(window);
        window.endFrame() catch {
            if (runner) |r| r.failure = window_mod.failureReason();
            return false;
        };
    }
    return true;
}

fn smoke(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) u8 {
    crt.attachParentConsole();
    const copied = copyDatabase(gpa, io, source, scratch) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, scratch, @errorName(err) });
        return 1;
    };
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ scratch, std.fs.path.sep }) catch return 1;
    const result = scripted(gpa, io, environ, .{
        .elk_path = elk_path,
        .text = smoke_schedule,
        .source = source,
        .source_files = copied,
        .frame_limit = smoke_frame_limit,
        .tier = "smoke",
    });
    if (result.code != 0) return result.code;

    // Exit was the last save point: the disk holds the last letter typed.
    var database = elk.database.Database.open(gpa, io, elk_path) catch |err| {
        std.debug.print("elk: smoke FAIL: the copy did not reopen: {s}\n", .{@errorName(err)});
        return 1;
    };
    defer database.deinit();
    const saved = database.readTranslation(gpa, 0, smoke_key) catch |err| {
        std.debug.print("elk: smoke FAIL: {s} did not read back: {s}\n", .{ smoke_key, @errorName(err) });
        return 1;
    };
    defer gpa.free(saved);
    if (!std.mem.eql(u8, saved, smoke_final)) {
        std.debug.print("elk: smoke FAIL: after Exit {s} holds \"{s}\", wanted \"{s}\"\n", .{ smoke_key, saved, smoke_final });
        return 1;
    }
    const opened = smokeOpen(gpa, io, environ, source, scratch);
    if (opened.code != 0) return opened.code;
    const crashed = smokeTree(gpa, io, environ, source, scratch);
    if (crashed.code != 0) return crashed.code;
    const filtered = smokeFilter(gpa, io, environ, source, scratch);
    if (filtered.code != 0) return filtered.code;
    const found = smokeFind(gpa, io, environ, source, scratch);
    if (found.code != 0) return found.code;
    const exported = smokeExport(gpa, io, environ, source, scratch);
    if (exported.code != 0) return exported.code;
    std.debug.print("elk: smoke PASS: {d} + {d} + {d} + {d} + {d} + {d} frames, {d} fonts, tree, edit, the four save points, File > Open, the state buttons, the autosave through a kill, browsing through a filter kept over a restart, Collapse Item, the Create Filter dialog, ELK Statistic, Find, the export to a mod and into the game and Run Game on {s}\n", .{ result.frames, opened.frames, crashed.frames, filtered.frames, found.frames, exported.frames, result.fonts, elk_path });
    return 0;
}

/// The smoke's tree, state button and autosave pass on a third copy,
/// `<scratch>-tree`: the run is killed mid-edit, the disk then holds the
/// autosaved text and not the later one, and a second run on the same copy
/// shows it.
fn smokeTree(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-tree", .{scratch}) catch return .{ .code = 1 };
    _ = copyDatabase(gpa, io, source, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    const killed = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = tree_schedule, .frame_limit = smoke_frame_limit, .tier = "smoke (tree, autosave)" });
    if (killed.code != 0) return killed;

    var database = elk.database.Database.open(gpa, io, elk_path) catch |err| {
        std.debug.print("elk: smoke FAIL: the killed copy did not reopen: {s}\n", .{@errorName(err)});
        return .{ .code = 1 };
    };
    defer database.deinit();
    const saved = database.readTranslation(gpa, 0, tree_name) catch |err| {
        std.debug.print("elk: smoke FAIL: {s} did not read back: {s}\n", .{ tree_name, @errorName(err) });
        return .{ .code = 1 };
    };
    defer gpa.free(saved);
    if (!std.mem.eql(u8, saved, autosaved)) {
        std.debug.print("elk: smoke FAIL: after the kill {s} holds \"{s}\", wanted the autosaved \"{s}\"\n", .{ tree_name, saved, autosaved });
        return .{ .code = 1 };
    }
    const reopened = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = reopen_schedule, .frame_limit = smoke_frame_limit, .tier = "smoke (reopen after the kill)" });
    if (reopened.code != 0) return reopened;
    return .{ .code = 0, .frames = killed.frames + reopened.frames };
}

/// The smoke's filter and browsing pass on a fourth copy,
/// `<scratch>-filter`, with its own `<scratch>-filter.cfg`: the filter
/// chosen in the first run is in the file and the second run starts with it;
/// the third and fourth runs edit the filters and find the edit kept.
fn smokeFilter(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-filter", .{scratch}) catch return .{ .code = 1 };
    var cfg_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const cfg_path = std.fmt.bufPrint(&cfg_buffer, "{s}.cfg", .{folder}) catch return .{ .code = 1 };
    _ = copyDatabase(gpa, io, source, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    std.Io.Dir.cwd().deleteFile(io, cfg_path) catch {};
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    const first = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = filter_schedule, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (filter, browse)" });
    if (first.code != 0) return first;
    const again = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = filter_reopen_schedule, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (filter after a restart)" });
    if (again.code != 0) return again;
    const edited = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = filter_edit_schedule, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (collapse item, Create Filter)" });
    if (edited.code != 0) return edited;
    const kept = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = filter_edit_reopen_schedule, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (Create Filter after a restart)" });
    if (kept.code != 0) return kept;
    return .{ .code = 0, .frames = first.frames + again.frames + edited.frames + kept.frames };
}

/// The smoke's statistics and find pass on a fifth copy, `<scratch>-find`,
/// with its own `<scratch>-find.cfg`, which the second run reads back.
fn smokeFind(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-find", .{scratch}) catch return .{ .code = 1 };
    var cfg_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const cfg_path = std.fmt.bufPrint(&cfg_buffer, "{s}.cfg", .{folder}) catch return .{ .code = 1 };
    _ = copyDatabase(gpa, io, source, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    std.Io.Dir.cwd().deleteFile(io, cfg_path) catch {};
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    const first = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = find_schedule, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (ELK Statistic, Find)" });
    if (first.code != 0) return first;
    const again = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = find_reopen_schedule, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (Find after a restart)" });
    if (again.code != 0) return again;
    return .{ .code = 0, .frames = first.frames + again.frames };
}

/// The smoke's pak export pass on a sixth copy, `<scratch>-export`, with
/// `<scratch>-game` as the game folder: one loose text, and on Linux and
/// macOS the stand-in Game, whose recorded arguments are checked after.
fn smokeExport(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-export", .{scratch}) catch return .{ .code = 1 };
    var cfg_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const cfg_path = std.fmt.bufPrint(&cfg_buffer, "{s}.cfg", .{folder}) catch return .{ .code = 1 };
    var game_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const game = std.fmt.bufPrint(&game_buffer, "{s}-game", .{scratch}) catch return .{ .code = 1 };
    _ = copyDatabase(gpa, io, source, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    const cwd = std.Io.Dir.cwd();
    cwd.deleteFile(io, cfg_path) catch {};
    cwd.deleteTree(io, game) catch {};
    makeGame(io, game) catch |err| {
        std.debug.print("elk: smoke FAIL: could not make the scratch game {s}: {s}\n", .{ game, @errorName(err) });
        return .{ .code = 1 };
    };
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    const result = scripted(gpa, io, environ, .{
        .elk_path = elk_path,
        .text = export_schedule,
        .dialog_folder = game,
        .settings_path = cfg_path,
        .game_folder = game,
        .frame_limit = smoke_frame_limit,
        .tier = "smoke (export, install, Run Game)",
    });
    if (result.code != 0 or !stand_in_game) return result;

    var args_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const args_path = std.fmt.bufPrint(&args_buffer, "{s}{c}{s}", .{ game, std.fs.path.sep, stand_in_args }) catch return .{ .code = 1 };
    const got = cwd.readFileAlloc(io, args_path, gpa, .limited(4096)) catch |err| {
        std.debug.print("elk: smoke FAIL: the stand-in Game wrote no {s}: {s}\n", .{ args_path, @errorName(err) });
        return .{ .code = 1 };
    };
    defer gpa.free(got);
    const wanted = "-editor-test\n-profile=ELKTest\n-mod=" ++ folder_mod ++ "\n-windowed\n";
    if (!std.mem.eql(u8, got, wanted)) {
        std.debug.print("elk: smoke FAIL: the stand-in Game got \"{s}\", wanted \"{s}\"\n", .{ got, wanted });
        return .{ .code = 1 };
    }
    return result;
}

/// The scratch game folder: one loose text, and the stand-in Game.
fn makeGame(io: std.Io, game: []const u8) !void {
    var dir = try std.Io.Dir.cwd().createDirPathOpen(io, game, .{});
    defer dir.close(io);
    try dir.createDirPath(io, std.fs.path.dirname(loose_text).?);
    try dir.writeFile(io, .{ .sub_path = loose_text, .data = "\xff\xfeL\x00" });
    if (stand_in_game) try dir.writeFile(io, .{
        .sub_path = "Game",
        .data = "#!/bin/sh\nprintf '%s\\n' \"$@\" > " ++ stand_in_args ++ "\n",
        .flags = .{ .permissions = .executable_file },
    });
}

/// The smoke's File > Open pass on a second copy, `<scratch>-open`, with
/// its own `<scratch>-open.cfg` in place of the user's elk.cfg.
fn smokeOpen(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-open", .{scratch}) catch return .{ .code = 1 };
    var cfg_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const cfg_path = std.fmt.bufPrint(&cfg_buffer, "{s}.cfg", .{folder}) catch return .{ .code = 1 };
    const copied = copyDatabase(gpa, io, source, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    std.Io.Dir.cwd().deleteFile(io, cfg_path) catch {};
    const result = scripted(gpa, io, environ, .{
        .elk_path = null,
        .text = open_schedule,
        .source = source,
        .source_files = copied,
        .dialog_folder = folder,
        .settings_path = cfg_path,
        .frame_limit = smoke_frame_limit,
        .tier = "smoke (File > Open)",
    });
    if (result.code != 0) return result;

    // The answered path is the first recent database in the file itself.
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    const bytes = std.Io.Dir.cwd().readFileAlloc(io, cfg_path, gpa, .limited(64 * 1024)) catch |err| {
        std.debug.print("elk: smoke FAIL: File > Open wrote no {s}: {s}\n", .{ cfg_path, @errorName(err) });
        return .{ .code = 1 };
    };
    defer gpa.free(bytes);
    const settings = settings_mod.parse(bytes);
    if (settings.recentCount() == 0 or !std.mem.eql(u8, settings.recentAt(0), elk_path)) {
        std.debug.print("elk: smoke FAIL: {s} does not list {s} first\n", .{ cfg_path, elk_path });
        return .{ .code = 1 };
    }
    return result;
}

fn autoMode(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, elk_path: []const u8) u8 {
    crt.attachParentConsole();
    const text = envValue(gpa, environ, "BK_ELK_AUTO") orelse {
        std.debug.print("elk: auto FAIL: BK_ELK_AUTO is not set\n", .{});
        return 2;
    };
    defer gpa.free(text);
    const result = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = text, .frame_limit = auto_frame_limit, .tier = "auto" });
    if (result.code == 0) std.debug.print("elk: auto PASS: {d} frames\n", .{result.frames});
    return result.code;
}

const Scripted = struct { code: u8, frames: u32 = 0, fonts: usize = 0 };

const Script = struct {
    /// Opened before the first frame; null starts with nothing open.
    elk_path: ?[]const u8,
    text: []const u8,
    /// The database the smoke copied and how many files, for its expectations.
    source: []const u8 = "",
    source_files: usize = 0,
    /// Where `do=answer` answers File > Open (Runner.dialog_folder).
    dialog_folder: []const u8 = "",
    /// The run's own elk.cfg, read when it is there (a later run of the same
    /// pass) and written as ELK writes it; "" keeps the settings in memory.
    /// The user's own file is never touched.
    settings_path: []const u8 = "",
    /// The game folder the exports and Run Game use; "" keeps elk.cfg's.
    game_folder: []const u8 = "",
    frame_limit: u32,
    tier: []const u8,
};

/// A hidden window, the schedule replayed over it. No OS file dialog opens;
/// the schedule answers File > Open itself. No GPU device is a failure,
/// never a pass (the spec's app tier).
fn scripted(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, script: Script) Scripted {
    const tier = script.tier;
    var failure: kit.auto_schedule.Failure = .{};
    const entries = kit.auto_schedule.parse(gpa, script.text, &failure) catch |err| {
        std.debug.print("elk: {s} FAIL: the schedule does not parse at \"{s}\": {s} ({s})\n", .{ tier, failure.token, failure.reason, @errorName(err) });
        return .{ .code = 2 };
    };
    defer gpa.free(entries);
    var window = window_mod.Window.start(.{ .title = ui_mod.app_title, .hidden = true }) catch |err| {
        std.debug.print("elk: {s} FAIL: the window did not start ({s}: {s})\n", .{ tier, @errorName(err), window_mod.failureReason() });
        return .{ .code = 1 };
    };
    defer window.stop();
    const windir = envValue(gpa, environ, "WINDIR");
    defer if (windir) |value| gpa.free(value);
    const loaded = fonts.load(io, windir);

    var session = session_mod.Session.init(gpa, io);
    defer session.deinit();
    var settings: settings_mod.Settings = .{};
    if (script.settings_path.len != 0) {
        if (std.Io.Dir.cwd().readFileAlloc(io, script.settings_path, gpa, .limited(64 * 1024))) |bytes| {
            defer gpa.free(bytes);
            settings = settings_mod.parse(bytes);
        } else |_| {}
    }
    if (script.game_folder.len != 0) settings.game_folder_storage.set(script.game_folder);
    var ui: ui_mod.Ui = .{
        .gpa = gpa,
        .io = io,
        .session = &session,
        .settings = &settings,
        .settings_path = script.settings_path,
        .environ = environ,
        .os_dialogs = false,
    };
    defer ui.deinit();
    if (script.elk_path) |path| ui.openDatabase(path);
    var runner: auto.Runner = .{
        .entries = entries,
        .source_folder = script.source,
        .source_files = script.source_files,
        .dialog_folder = script.dialog_folder,
    };
    var frames: u32 = 0;
    const passed = loopCounting(&window, &ui, &runner, script.frame_limit, &frames);
    if (!passed) {
        std.debug.print("elk: {s} FAIL: {s}\n", .{ tier, runner.failure });
        return .{ .code = 1, .frames = frames };
    }
    if (!runner.done()) {
        std.debug.print("elk: {s} FAIL: the run ended before its schedule did\n", .{tier});
        return .{ .code = 1, .frames = frames };
    }
    return .{ .code = 0, .frames = frames, .fonts = loaded };
}

fn loopCounting(window: *window_mod.Window, ui: *ui_mod.Ui, runner: *auto.Runner, frame_limit: u32, frames: *u32) bool {
    const passed = loop(window, ui, runner, frame_limit);
    frames.* = if (runner.next > 0) runner.entries[runner.next - 1].frame else 0;
    return passed;
}

/// The database's project file and its `<element>_data_base/` folders, the
/// rest of the folder (the old binaries, packs and updates) left out.
/// Returns how many files it copied.
fn copyDatabase(gpa: std.mem.Allocator, io: std.Io, source: []const u8, scratch: []const u8) !usize {
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, scratch) catch {};
    try cwd.createDirPath(io, scratch);
    var from = try cwd.openDir(io, source, .{ .iterate = true });
    defer from.close(io);
    var to = try cwd.openDir(io, scratch, .{});
    defer to.close(io);
    try copyFile(gpa, io, from, to, "elk.xml");
    var copied: usize = 1;
    var entries = from.iterate();
    while (try entries.next(io)) |entry| {
        if (entry.kind != .directory) continue;
        if (!std.ascii.endsWithIgnoreCase(entry.name, elk.database.data_base_suffix)) continue;
        var folder = try from.openDir(io, entry.name, .{ .iterate = true });
        defer folder.close(io);
        var walker = try folder.walk(gpa);
        defer walker.deinit();
        while (try walker.next(io)) |file| {
            if (file.kind != .file) continue;
            const relative = try std.fs.path.join(gpa, &.{ entry.name, file.path });
            defer gpa.free(relative);
            if (std.fs.path.dirname(relative)) |parent| try to.createDirPath(io, parent);
            try copyFile(gpa, io, from, to, relative);
            copied += 1;
        }
    }
    return copied;
}

fn copyFile(gpa: std.mem.Allocator, io: std.Io, from: std.Io.Dir, to: std.Io.Dir, relative: []const u8) !void {
    const bytes = try from.readFileAlloc(io, relative, gpa, .limited(16 << 20));
    defer gpa.free(bytes);
    try to.writeFile(io, .{ .sub_path = relative, .data = bytes });
}

fn envValue(gpa: std.mem.Allocator, environ: std.process.Environ, name: []const u8) ?[]u8 {
    return environ.getAlloc(gpa, name) catch null;
}

/// The game's user root, found as Platform/Paths.cpp finds it but without
/// the engine: `BK_USER_ROOT` or SDL's pref path on Windows, the XDG rule on
/// Linux and macOS. "" when it cannot be known.
fn userRoot(gpa: std.mem.Allocator, environ: std.process.Environ, buffer: []u8) []const u8 {
    if (builtin.os.tag == .windows) {
        if (envValue(gpa, environ, "BK_USER_ROOT")) |root| {
            defer gpa.free(root);
            if (root.len != 0) return std.fmt.bufPrint(buffer, "{s}{s}", .{ root, if (std.mem.endsWith(u8, root, "\\")) "" else "\\" }) catch "";
        }
        const preference = c.SDL_GetPrefPath("Nival", "Blitzkrieg") orelse return "";
        defer c.SDL_free(preference);
        return std.fmt.bufPrint(buffer, "{s}", .{std.mem.span(preference)}) catch "";
    }
    const xdg = envValue(gpa, environ, "XDG_DATA_HOME");
    defer if (xdg) |value| gpa.free(value);
    const home = envValue(gpa, environ, "HOME");
    defer if (home) |value| gpa.free(value);
    return settings_mod.posixUserRoot(buffer, xdg, home);
}
