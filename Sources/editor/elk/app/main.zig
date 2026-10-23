//! ELK, the Blitzkrieg localisation kit (D-01): its own executable, no engine.
//!
//!   ELK [elk.xml] [--developer | -developer]
//!   ELK --short | -short
//!   ELK --smoke <database folder> <scratch folder>
//!   ELK --auto <elk.xml>            (the schedule in BK_ELK_AUTO)
//!
//! Without a path ELK reopens the most recent database from `elk.cfg`, in
//! the extended layout (D-04's default; `--developer` names it). `--short`
//! is the translator layout: the `elk.xml` beside the executable, brought up
//! to date with the newest `.upd` there first (ui.zig).
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
//! numbers of the tracked database and its "at last close" against the
//! snapshot elk.xml holds, and Edit > Find...: hits in the
//! translation and the original, the resume within a field, the wrap, Match
//! case and Whole word missing, Up reaching the text above; View > ELK Tree
//! and Status Bar turned off and Help > About; and a second run finding the
//! text and options in elk.cfg, the tree and the status line still off, and
//! the counts the first run's Exit kept as the snapshot. A sixth copy proves the pak
//! export against a scratch game folder, never a real one: File > Export to
//! language mod by name and by a folder answered for Browse..., each pak
//! read back by the core with all 3,072 texts; Install to game warning
//! about the scratch game's loose Data/Textes, Enter taking the mod route,
//! then Install anyway writing Data/Texts.pak; Run Game's command line, and
//! on Linux and macOS a stand-in Game script recording the arguments it got;
//! the commands disabled once the database is closed. A seventh copy proves
//! spell check (E-08): off with the reason while ELK's own dictionaries
//! folder is missing, on with a generated test dictionary chosen in Spelling
//! Options, F7 listing a typed misspelling with its suggestion, Add to
//! dictionary writing custom.dic beside the run's elk.cfg and clearing the
//! flag, the walk's Ignore, Change All, Ignore All, Check Again and Change
//! (the corrections in the box, then saved), and a second run still
//! accepting the added word but not the ignored one; the same copy's
//! description view (C1) shows a text's own `.dsc`, Find's hit in it and a
//! folder's `_folder.dsc`. An eighth copy proves
//! the spreadsheet exchange (D-10): Export to spreadsheet writing all 3,072
//! texts to `.xlsx` (the default for a name without extension) and `.csv`
//! and refusing `.xls`; Import from spreadsheet setting an edited
//! translation back from the earlier export, importing it again and the
//! `.csv` changing nothing, and the legacy `desc.xls` reading all its rows.
//! A ninth copy, with `game_23.upd` beside it and the same bytes as
//! `game_24.upd`, proves the short layout: it opens in the translator layout
//! with its one toolbar and the five fixed filters, game_24 applied (the
//! copy's elk.xml then records it), Help opens by F1 with the whole
//! embedded text drawn; a second run on the same elk.cfg starts at All
//! again, applies nothing and opens Help from the menu. The export pass
//! also exports by the Translated filter (D6), the pak then holding exactly
//! the four texts it admits, and cancels an export held mid-run, which
//! writes nothing. A tenth copy proves D-14's worker: an import of a sheet
//! that changes every text, held at row 1000 with the Working modal
//! repainting over it and cancelled from its Cancel, leaves every file
//! whole (the first 1000 texts imported, every other file the source's,
//! elk.xml unsaved, no temporary file); a Find Next that misses, held and
//! released, then held and cancelled; and the whole import after. An
//! eleventh folder, a new database with no element, takes File > Import
//! from Game... from a copy of Data/Textes (1742 texts, the element `game`
//! added) and Import from PAK/UPD... of usa01.pak: held at its 50th file and
//! cancelled, the 50 files in and elk.xml unchanged; then whole (the other
//! 63), and again, which changes nothing.
//! `--auto` replays `BK_ELK_AUTO` on the database given.
//!
//! `--l10n-auto <elk.xml> <scratch folder>` is the localisation tier's pass (tools/zig/elk_l10n_auto.zig):
//! in the language of the scratch elk.cfg (`BK_ELK_SETTINGS`) it captures the main window, the File
//! menu opened and the help window into the folder, and prints the rect of every label those frames
//! drew as `l10n-auto: rect <key> <x> <y> <w> <h>`.
const std = @import("std");
const builtin = @import("builtin");
const kit = @import("editor_kit");
const l10n = kit.l10n;
const sdl3 = @import("sdl3");
const elk = @import("elk_core");
const bk_memory = @import("bk_memory_allocator");
const window_mod = kit.window;
const session_mod = @import("session.zig");
const settings_mod = @import("settings.zig");
const ui_mod = @import("ui.zig");
const spelling_mod = @import("spelling.zig");
const auto = @import("auto.zig");

const crt = kit.crt;
const c = sdl3.c;

/// The smoke's text and what it types into it: one leave, one Ctrl+S, one
/// Close and one Exit, each adding a letter the disk must then hold.
const smoke_key = "medals/1/name";
const smoke_final = "ELK-SMOKEXYZ";
const smoke_schedule =
    "2:expect=open,2:expect=texts:3072,2:expect=glyphs,3:expect=rows,3:expect=title:elk.xml," ++
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
    "2:expect=closed,3:do=open,4:do=answer:elk.xml,6:expect=open,6:expect=texts:3072,6:expect=title:elk.xml," ++
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
/// Africa (below it). The copy's elk.xml holds the MFC ELK's snapshot of an
/// older database (2,968 texts) as the statistic window's "at last close";
/// Exit keeps the counts made here in its place, which the second run shows.
/// The tree and the status line are turned off before Exit, About opened.
const kuban_desc = "medals/german/kuban/desc";
const kuban_name = "medals/german/kuban/name";
const tunis = "scenarios/chapters/allies/tunis/description";
const tobruk = "scenarios/chapters/allies/tobruk/description";
const find_schedule =
    "2:expect=open,2:do=statistics,4:expect=statistics:1," ++
    "4:expect=statistic:original/total=3072/43224/229442/286094,4:expect=statistic:original/0=3072/43224/229442/286094," ++
    "4:expect=statistic:original/not_translated=1121/20280/106331/129481,4:expect=statistic:translation/translated=4/55/263/316," ++
    "4:expect=statistic:translation/0.approved=589/183/1059/1180," ++
    "4:expect=last_close:original/total=2968/53860/277016/344482,4:expect=last_close:translation/0.approved=1017/7080/35259/42123," ++
    "4:expect=last_close:original/1=none,4:expect=tree:1,4:expect=status_bar:1,5:do=statistics_close,7:expect=statistics:0," ++
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
    "37:do=find_next:Rommel,39:expect=found:" ++ tobruk ++ "=original:858,40:do=find_close,42:expect=find_dialog:0," ++
    "42:do=view_tree,42:do=view_status_bar,44:expect=tree:0,44:expect=status_bar:0,44:do=about,46:expect=about:1," ++
    "46:do=about_close,48:expect=about:0,49:exit";
/// The second run on the same elk.cfg: the text and Up are kept; Down from
/// nothing selected finds Tobruk, the first text that has it. The tree and
/// the status line stay off; "at last close" is what the first run counted.
const find_reopen_schedule =
    "2:expect=open,2:expect=tree:0,2:expect=status_bar:0,2:do=statistics," ++
    "4:expect=last_close:original/total=3072/43224/229442/286094,4:expect=last_close:translation/0.approved=589/183/1059/1180," ++
    "4:do=statistics_close,5:do=view_tree,7:expect=tree:1,7:expect=status_bar:0," ++
    "8:expect=find_text:Rommel,2:expect=find_option:down=0,2:expect=find_option:match_case=0," ++
    "8:expect=find_option:whole_word=0,9:key=f+ctrl,11:expect=find_dialog:1,11:do=find_option:down=1," ++
    "12:do=find_next:Rommel,14:expect=found:" ++ tobruk ++ "=original:858,15:exit";

/// The pak export pass. The scratch game folder holds one loose text of the
/// database, the Exit Game label E-06 changed, so Install to game warns.
const export_mod = "ELKSmoke";
const folder_mod = "ELKFolder";
const loose_text = "Data/Textes/strings/imheader-gbutton-exitgame.txt";
const export_texts = "3072";
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
    // By the Translated filter (D6): its four texts and no other.
    "37:do=filter:Translated,38:do=export_mod,40:do=export_name:" ++ filtered_mod ++ ",40:do=export_filtered:1,40:do=export_ok," ++
    "42:expect=export_dialog:0,42:expect=pak:" ++ filtered_mod ++ "=4,42:expect=pak_filtered:" ++ filtered_mod ++ "," ++
    "42:expect=notice_has:Translated," ++
    // Held at its 100th text and cancelled from the Working modal: nothing written.
    "43:do=filter:All,43:do=hold:100,43:do=export_mod,45:do=export_name:" ++ cancelled_mod ++ ",45:do=export_ok,46:do=wait_held," ++
    "47:expect=working:100/" ++ export_texts ++ ",47:do=job_cancel,49:expect=busy:0,49:expect=pak:" ++ cancelled_mod ++ "=none," ++
    "49:expect=notice_has:cancelled," ++
    "50:do=close,52:expect=can_export:0,52:expect=can_run_game:0,53:exit";
const filtered_mod = "ELKFiltered";
const cancelled_mod = "ELKCancelled";
/// The spell check pass. The install has no `dictionaries` folder beside
/// ELK, so spell check starts off; the copy's own folder then gets the
/// generated test dictionary (spelling.zig), chosen through the options.
/// The walk: Ignore passes "are", Change All turns both "teh" into "the",
/// Ignore All takes "zorg", Check Again finds "are" again, Change makes it
/// "is"; the translation box, not yet the disk, holds each correction
/// until Save. The description view then shows the text's own `.dsc`
/// (written into the copy, Data/ELK has none), Find's hit in it, and the
/// Kuban texts' `_folder.dsc`. A schedule's argument holds no space, so
/// these texts part their words with other spelling delimiters.
const spell_typed = "The_tanks_are_redy_and_teh_medals;teh_tank:zorg";
const spell_changed_all = "The_tanks_are_redy_and_the_medals;the_tank:zorg";
const spell_changed = "The_tanks_is_redy_and_the_medals;the_tank:zorg";
const own_description = "The-Smoke-medal-of-the-smoke-test";
const folder_description = "German-medals";
const spell_schedule =
    "2:expect=open,2:do=select_key:" ++ smoke_key ++ ",3:expect=can_spell:0,3:expect=spell_reason_has:dictionaries," ++
    "4:do=spell_options,5:do=spell_folder:dictionaries,6:expect=spell_language:" ++ spelling_mod.test_language ++ ",6:expect=can_spell:1," ++
    "7:do=focus:translation,9:key=a+ctrl,10:text=" ++ spell_typed ++ "," ++
    "13:key=f7,15:expect=spelling:1,15:expect=misspelt:redy=1,15:expect=misspelt:teh=1,15:expect=misspelt:tanks=0," ++
    "15:expect=suggestion:redy=ready,15:expect=suggestion:teh=the,15:expect=spell_current:are@10," ++
    "16:do=spell_add:redy,18:expect=misspelt:redy=0,18:expect=misspelt:teh=1,18:expect=user_word:redy,18:expect=spell_current:are@10," ++
    "19:do=spell_ignore,20:expect=spell_current:teh@23,20:expect=spell_change:the,20:expect=misspelt:are=1," ++
    "21:do=spell_change_all:the,23:expect=translation:" ++ spell_changed_all ++ ",23:expect=spell_current:zorg@43,23:expect=dirty:1," ++
    "24:do=spell_ignore_all,26:expect=spell_current:-,26:expect=misspelt:zorg=0," ++
    "27:do=spell_check,28:expect=spell_current:are@10,28:expect=misspelt:zorg=0," ++
    "29:do=spell_change:is,31:expect=spell_current:-,33:expect=translation:" ++ spell_changed ++ ",33:expect=disk:" ++ smoke_key ++ "=," ++
    "34:do=save,36:expect=dirty:0,36:expect=disk:" ++ smoke_key ++ "=" ++ spell_changed ++ ",36:expect=state:" ++ smoke_key ++ "=translated," ++
    "37:do=spell_close,39:expect=spelling:0," ++
    "39:expect=description:" ++ own_description ++ ",40:do=find_dialog,42:do=find_next:Smoke-medal," ++
    "44:expect=found:" ++ smoke_key ++ "=description:4,44:expect=description:" ++ own_description ++ ",45:do=find_close," ++
    "46:do=select_key:" ++ kuban_desc ++ ",48:expect=description:" ++ folder_description ++ ",49:exit";
/// The second run on the same elk.cfg: the folder and language are kept,
/// and in the saved translation the added word is still accepted, the
/// word Ignore All took is flagged again.
const spell_reopen_schedule =
    "2:expect=open,2:expect=spell_language:" ++ spelling_mod.test_language ++ ",2:do=select_key:" ++ smoke_key ++ "," ++
    "3:do=spell,5:expect=spelling:1,5:expect=misspelt:redy=0,5:expect=misspelt:teh=0,5:expect=misspelt:zorg=1," ++
    "5:expect=spell_current:zorg@42,6:exit";

/// The spreadsheet pass: exports to `.xlsx` (the name without extension)
/// and `.csv`, `.xls` refused; an edited translation set back by importing
/// the earlier export, which turns it Translated; the same import again and
/// the `.csv` change nothing; the legacy `desc.xls` reads all its rows,
/// every one without a translation.
const sheet_key = "buildings/africa/summer/a_cisterns/01/name";
/// The tracked texts with no translation (3,072 less 1,555).
const untranslated = "1517";
const sheet_schedule =
    "2:expect=open,2:do=sheet_export,2:do=answer:texts,4:expect=notice_has:" ++ export_texts ++ ",4:expect=notice_has:texts.xlsx," ++
    "5:do=sheet_export,5:do=answer:texts.csv,7:expect=notice_has:texts.csv," ++
    "8:do=sheet_export,8:do=answer:texts.xls,10:expect=failure_has:XlsIsReadOnly," ++
    "11:do=select_key:" ++ sheet_key ++ ",15:do=focus:translation,17:key=end+ctrl,18:text=X,20:do=save," ++
    "21:expect=disk:" ++ sheet_key ++ "=CisternX," ++
    "22:do=sheet_import,22:do=answer:texts.xlsx,24:expect=imported:" ++ export_texts ++ "/1/0/" ++ untranslated ++ "/0,24:expect=notice_has:texts.xlsx," ++
    "24:expect=disk:" ++ sheet_key ++ "=Cistern,24:expect=state:" ++ sheet_key ++ "=translated," ++
    "25:do=sheet_import,25:do=answer:texts.xlsx,27:expect=imported:" ++ export_texts ++ "/0/0/" ++ untranslated ++ "/0," ++
    "28:do=sheet_import,28:do=answer:texts.csv,30:expect=imported:" ++ export_texts ++ "/0/0/" ++ untranslated ++ "/0," ++
    "31:do=sheet_import,31:do=answer:desc.xls,33:expect=imported:153/0/0/153/0,33:expect=notice_has:desc.xls," ++
    "34:exit";

/// The short layout pass. The copy's folder stands in for ELK's own; it
/// holds game_23.upd, which Data/ELK records, and the same bytes as
/// game_24.upd, the newest. The filter chosen is not kept, and Collapse Item
/// is on whatever elk.cfg says.
const short_schedule =
    "2:expect=open,2:expect=layout:short,2:expect=title:elk.xml,2:expect=texts:3072,2:expect=notice_has:1/2," ++
    "3:expect=toolbar:short,3:expect=filter_count:5,3:expect=filter_at:4=Approved,3:expect=filter:All,3:expect=collapse_item:1," ++
    "3:do=filter:Translated,4:expect=filter:Translated,4:do=select_key:" ++ tree_name ++ ",5:key=n+ctrl,7:expect=selected:" ++ walk[0] ++ "," ++
    "8:expect=help:0,8:key=f1,10:expect=help:1,10:expect=help_has:--short,10:expect=help_has:Language,11:do=help_close,13:expect=help:0,14:exit";
const short_reopen_schedule =
    "2:expect=open,2:expect=layout:short,2:expect=notice_has:0/2,3:expect=toolbar:short,3:expect=filter:All," ++
    "4:do=help,6:expect=help:1,8:shot=" ++ short_shot_name ++ ",9:do=probe_hindi,11:shot=" ++ hindi_shot_name ++ ",12:exit";
/// The frame `short_reopen_schedule` captures with Help open; `smokeShort` reads it back.
const short_shot_name = "short-help";
/// The frame with the Hindi probe (Ui.probe_rect); `smokeShort` measures it.
const hindi_shot_name = "hindi";

/// The worker pass (D-14). `d14.csv` gives every text of the copy a new
/// translation starting with `auto.whole_marker`; its import is held before
/// row 1000, the modal seen twice at 1000 of 3072 rows, and cancelled. A
/// Find Next that misses reads all 3,072 texts and one again (3073 units):
/// held before its fifth and released, it reports the miss; held and
/// cancelled, it reports the Cancel. The whole import then changes the
/// other 2072 texts.
const progress_sheet = "d14.csv";
const progress_schedule =
    "2:expect=open,2:expect=busy:0,2:do=hold:1000,2:do=sheet_import,2:do=answer:" ++ progress_sheet ++ ",3:do=wait_held," ++
    "4:expect=busy:1,4:expect=working:1000/" ++ export_texts ++ ",6:expect=working:1000/" ++ export_texts ++ ",6:do=job_cancel," ++
    "8:expect=busy:0,8:expect=notice_has:cancelled,8:expect=whole:1000," ++
    "9:do=find_dialog,11:do=hold:5,11:do=find_next:no-such-text-anywhere,12:do=wait_held,13:expect=working:5/3073,13:do=release," ++
    "15:expect=found:none,16:do=hold:5,16:do=find_next:no-such-text-anywhere,17:do=wait_held,18:do=job_cancel," ++
    "20:expect=notice_has:Find,21:do=find_close," ++
    "22:do=sheet_import,22:do=answer:" ++ progress_sheet ++ ",24:expect=imported:" ++ export_texts ++ "/2072/0/0/0,24:expect=whole:" ++ export_texts ++ ",25:exit";

/// The package imports' pass (D-05, D-14) on a new database: the game
/// folder `game` holds a copy of Data/Textes and Data/resource.description,
/// `usa01.pak` is Data/AmericanELK's.
const package_schedule =
    "2:expect=open,2:expect=texts:0,2:expect=can_import:1,2:do=import_game,2:expect=can_import:0,2:do=answer:game," ++
    "4:expect=busy:0,4:expect=updated:1742/1742/1/0/1,4:expect=texts:1742,4:expect=notice_has:1742," ++
    // Held before its 50th file and cancelled: those 50 in, elk.xml as it was.
    "5:do=hold:50,5:do=import_pak,5:do=answer:" ++ package_pak ++ ",6:do=wait_held,7:expect=working:50/113,7:do=job_cancel," ++
    "9:expect=busy:0,9:expect=notice_has:cancelled,9:expect=package_whole:1792,9:expect=texts:1742," ++
    "10:do=import_pak,10:do=answer:" ++ package_pak ++ ",12:expect=updated:63/63/0/0/1,12:expect=texts:1855," ++
    // Again: nothing changes.
    "13:do=import_pak,13:do=answer:" ++ package_pak ++ ",15:expect=updated:0/0/0/0/0,15:expect=notice_has:nothing,15:expect=texts:1855," ++
    "16:do=close,18:expect=can_import:0,19:exit";
const package_pak = "usa01.pak";

/// Linux and macOS get a stand-in Game, a shell script that writes the
/// arguments it was started with; Windows has none, and its Run Game fails
/// to start after recording the same command line.
const stand_in_game = builtin.os.tag != .windows;
const stand_in_args = "elk-game.args";

/// A scripted run that has not exited by then has hung.
const smoke_frame_limit = 200;
const auto_frame_limit = 100_000;
const busy_limit_ms = 60_000;

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
    if (items.len > 0 and std.mem.eql(u8, items[0], "--l10n-auto")) {
        if (items.len != 3) usage();
        return l10nAutoMode(gpa, io, minimal.environ, items[1], items[2]);
    }
    if (items.len > 0 and std.mem.eql(u8, items[0], "--auto")) {
        if (items.len != 2) usage();
        return autoMode(gpa, io, minimal.environ, items[1]);
    }
    if (items.len > 1 or (items.len == 1 and std.mem.startsWith(u8, items[0], "-"))) usage();
    // The short layout opens only the database beside ELK.
    if (short and items.len != 0) usage();
    return interactive(gpa, io, minimal.environ, if (items.len == 1) items[0] else null, short);
}

fn usage() noreturn {
    crt.attachParentConsole();
    std.debug.print(
        \\usage: ELK [elk.xml] [--developer]
        \\       ELK --short           (the elk.xml beside ELK, updated from its .upd files)
        \\       ELK --smoke <database folder> <scratch folder>
        \\       ELK --auto <elk.xml>    (the schedule in BK_ELK_AUTO)
        \\       ELK --l10n-auto <elk.xml> <scratch folder>
        \\
    , .{});
    std.process.exit(2);
}

fn interactive(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, first: ?[]const u8, short: bool) u8 {
    var window = window_mod.Window.start(.{ .title = ui_mod.app_title }) catch |err| {
        crt.attachParentConsole();
        std.debug.print("elk: the window did not start ({s}: {s})\n", .{ @errorName(err), window_mod.failureReason() });
        return 1;
    };
    defer window.stop();
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

    // The language comes with the setting (ui_language), the fonts for it.
    _ = kit.language_picker.setup(gpa, io, environ, &settings, false) catch |err| {
        crt.attachParentConsole();
        std.debug.print("elk: the fonts did not load ({s})\n", .{@errorName(err)});
        return 1;
    };

    var session = session_mod.Session.init(gpa, io);
    defer session.deinit();
    var ui: ui_mod.Ui = .{ .gpa = gpa, .io = io, .session = &session, .settings = &settings, .settings_path = settings_path, .environ = environ };
    defer ui.deinit();
    var recent_copy: [settings_mod.max_path]u8 = undefined;
    if (short) {
        var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const len = std.process.executableDirPath(io, &folder_buffer) catch |err| {
            crt.attachParentConsole();
            std.debug.print("elk: the folder ELK runs from is not known: {s}\n", .{@errorName(err)});
            return 1;
        };
        ui.startShort(folder_buffer[0..len]);
    } else if (first) |path| {
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
/// frame failed or `frame_limit` passed first. The frames drawn while a
/// long operation runs (D-14) do not count against the limit, the work
/// taking as long as it takes; one that runs past a minute has hung.
fn loop(window: *window_mod.Window, ui: *ui_mod.Ui, runner: ?*auto.Runner, frame_limit: u32) bool {
    var frame: u32 = 0;
    var busy_frames: u32 = 0;
    var busy_since: ?u64 = null;
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
        if (ui.busy()) {
            busy_frames += 1;
            const since = busy_since orelse c.SDL_GetTicks();
            busy_since = since;
            if (runner) |r| if (c.SDL_GetTicks() - since > busy_limit_ms) {
                r.failure = "a long operation ran for more than a minute";
                return false;
            };
        } else busy_since = null;
        if (frame - busy_frames >= frame_limit) {
            if (runner) |r| r.failure = "the schedule did not exit in time";
            return false;
        }
        ui.applyPendingLanguage();
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
    const spelt = smokeSpell(gpa, io, environ, source, scratch);
    if (spelt.code != 0) return spelt.code;
    const sheets = smokeSheet(gpa, io, environ, source, scratch);
    if (sheets.code != 0) return sheets.code;
    const shorts = smokeShort(gpa, io, environ, source, scratch);
    if (shorts.code != 0) return shorts.code;
    const worked = smokeProgress(gpa, io, environ, source, scratch);
    if (worked.code != 0) return worked.code;
    const updated = smokePackages(gpa, io, environ, source, scratch);
    if (updated.code != 0) return updated.code;
    std.debug.print("elk: smoke PASS: {d} + {d} + {d} + {d} + {d} + {d} + {d} + {d} + {d} + {d} + {d} frames, {d} fonts, tree, edit, the four save points, File > Open, the state buttons, the autosave through a kill, browsing through a filter kept over a restart, Collapse Item, the Create Filter dialog, ELK Statistic with the snapshot Exit keeps, Find, View > ELK Tree and Status Bar kept over a restart, About, the export to a mod (by filter too) and into the game and Run Game, spell check's walk with Add to dictionary kept over a restart, the description view, the spreadsheet export and import, the short layout with its update and Help, the worker's progress modal and Cancel leaving every file whole, Import from Game and Import from PAK/UPD on {s}\n", .{ result.frames, opened.frames, crashed.frames, filtered.frames, found.frames, exported.frames, spelt.frames, sheets.frames, shorts.frames, worked.frames, updated.frames, result.fonts, elk_path });
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

/// `shot=`'s proof: `<folder>/<name>.tga` is a 32-bit TGA of the window's size whose pixels are
/// not all one colour (a cleared target that was never drawn into would be).
fn shotHasInk(gpa: std.mem.Allocator, io: std.Io, folder: []const u8, name: []const u8) bool {
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const path = std.fmt.bufPrint(&path_buffer, "{s}{c}{s}.tga", .{ folder, std.fs.path.sep, name }) catch return false;
    const bytes = std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(64 << 20)) catch |err| {
        std.debug.print("elk: smoke FAIL: shot {s} did not read back: {s}\n", .{ path, @errorName(err) });
        return false;
    };
    defer gpa.free(bytes);
    const wanted_w: u16 = 1280;
    const wanted_h: u16 = 800;
    if (bytes.len < 18 + @as(usize, wanted_w) * wanted_h * 4 or bytes[2] != 2 or bytes[16] != 32) {
        std.debug.print("elk: smoke FAIL: shot {s} is not a 32-bit TGA of the window ({d} bytes)\n", .{ path, bytes.len });
        return false;
    }
    const w = std.mem.readInt(u16, bytes[12..14], .little);
    const h = std.mem.readInt(u16, bytes[14..16], .little);
    if (w != wanted_w or h != wanted_h) {
        std.debug.print("elk: smoke FAIL: shot {s} is {d}x{d}, wanted {d}x{d}\n", .{ path, w, h, wanted_w, wanted_h });
        return false;
    }
    const pixels = bytes[18..][0 .. @as(usize, w) * h * 4];
    const first = pixels[0..4];
    var different: usize = 0;
    var at: usize = 4;
    while (at < pixels.len) : (at += 4) {
        if (!std.mem.eql(u8, pixels[at..][0..4], first)) different += 1;
    }
    if (different < 100) {
        std.debug.print("elk: smoke FAIL: shot {s} has no ink: {d} pixels differ from the first\n", .{ path, different });
        return false;
    }
    return true;
}

/// The Hindi probe's proof (the full hb-view comparison is the l10n tier's): inside `Ui.probe_rect`
/// the capture has white ink, spread over a run as wide as the clusters it holds and as tall as
/// a headline with a vowel sign above and a letter below it. A cluster with no glyph is a single
/// fallback box a few pixels wide, so a run this wide cannot be one.
fn hindiHasClusters(gpa: std.mem.Allocator, io: std.Io, folder: []const u8, name: []const u8) bool {
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const path = std.fmt.bufPrint(&path_buffer, "{s}{c}{s}.tga", .{ folder, std.fs.path.sep, name }) catch return false;
    const bytes = std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(64 << 20)) catch |err| {
        std.debug.print("elk: smoke FAIL: shot {s} did not read back: {s}\n", .{ path, @errorName(err) });
        return false;
    };
    defer gpa.free(bytes);
    const w: usize = std.mem.readInt(u16, bytes[12..14], .little);
    const h: usize = std.mem.readInt(u16, bytes[14..16], .little);
    if (bytes.len < 18 + w * h * 4 or bytes[2] != 2 or bytes[16] != 32) {
        std.debug.print("elk: smoke FAIL: shot {s} is not a 32-bit TGA\n", .{path});
        return false;
    }
    const pixels = bytes[18..][0 .. w * h * 4];
    const rect = ui_mod.Ui.probe_rect;
    const x0: usize = @intFromFloat(rect[0]);
    const y0: usize = @intFromFloat(rect[1]);
    const x1: usize = @min(w, x0 + @as(usize, @intFromFloat(rect[2])));
    const y1: usize = @min(h, y0 + @as(usize, @intFromFloat(rect[3])));
    var lit: usize = 0;
    var left: usize = x1;
    var right: usize = 0;
    var top: usize = y1;
    var bottom: usize = 0;
    for (y0..y1) |y| for (x0..x1) |x| {
        if (pixels[(y * w + x) * 4 + 1] < 128) continue;
        lit += 1;
        left = @min(left, x);
        right = @max(right, x);
        top = @min(top, y);
        bottom = @max(bottom, y);
    };
    const clusters = kit.fonts.clusterCount();
    std.debug.print("elk: hindi probe: {d} lit pixels, ink {d}..{d} x {d}..{d}, {d} clusters\n", .{ lit, left, right, top, bottom, clusters });
    // Measured once on Linux: 222 lit pixels, a run 71 pixels wide and 14 tall, 6 clusters.
    if (lit < 150 or right < left or right - left + 1 < 60 or bottom < top or bottom - top + 1 < 12 or clusters < 4) {
        std.debug.print("elk: smoke FAIL: the Hindi probe in {s} has no shaped clusters (wanted at least 150 lit pixels in a run 60x12 and 4 clusters)\n", .{path});
        return false;
    }
    return true;
}

/// The smoke's spell check pass on a seventh copy, `<scratch>-spell`, with
/// its own `<scratch>-spell.cfg` and so `custom.dic` beside it; the test
/// dictionary is written into the copy's `dictionaries` folder.
fn smokeSpell(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-spell", .{scratch}) catch return .{ .code = 1 };
    var cfg_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const cfg_path = std.fmt.bufPrint(&cfg_buffer, "{s}.cfg", .{folder}) catch return .{ .code = 1 };
    _ = copyDatabase(gpa, io, source, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    var dictionaries_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const dictionaries = std.fmt.bufPrint(&dictionaries_buffer, "{s}{c}dictionaries", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, dictionaries) catch {};
    spelling_mod.writeTestDictionary(io, dictionaries) catch |err| {
        std.debug.print("elk: smoke FAIL: could not write the test dictionary into {s}: {s}\n", .{ dictionaries, @errorName(err) });
        return .{ .code = 1 };
    };
    writeDescriptions(gpa, io, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not write the descriptions into {s}: {s}\n", .{ folder, @errorName(err) });
        return .{ .code = 1 };
    };
    cwd.deleteFile(io, cfg_path) catch {};
    var user_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const user_path = std.fmt.bufPrint(&user_buffer, "{s}{c}{s}", .{ std.fs.path.dirname(cfg_path) orelse ".", std.fs.path.sep, elk.spell.user_dictionary_name }) catch return .{ .code = 1 };
    cwd.deleteFile(io, user_path) catch {};
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    const first = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = spell_schedule, .dialog_folder = folder, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (spell check)" });
    if (first.code != 0) return first;
    const again = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = spell_reopen_schedule, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (spell check after a restart)" });
    if (again.code != 0) return again;
    return .{ .code = 0, .frames = first.frames + again.frames };
}

/// The spell pass's descriptions: the smoke text's own `.dsc` and a
/// `_folder.dsc` for the Kuban texts, in the editor's file encoding.
fn writeDescriptions(gpa: std.mem.Allocator, io: std.Io, folder: []const u8) !void {
    const files = [_]struct { relative: []const u8, text: []const u8 }{
        .{ .relative = "game_data_base/" ++ smoke_key ++ ".dsc", .text = own_description },
        .{ .relative = "game_data_base/medals/german/_folder.dsc", .text = folder_description },
    };
    for (files) |file| {
        const path = try std.fs.path.join(gpa, &.{ folder, file.relative });
        defer gpa.free(path);
        const bytes = try elk.text_codec.fromEditor(gpa, file.text);
        defer gpa.free(bytes);
        try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = bytes });
    }
}

/// The smoke's spreadsheet pass on an eighth copy, `<scratch>-sheet`, the
/// sheets written and read in the copy's folder, with the source's legacy
/// `desc.xls` beside them.
fn smokeSheet(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-sheet", .{scratch}) catch return .{ .code = 1 };
    _ = copyDatabase(gpa, io, source, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    {
        const cwd = std.Io.Dir.cwd();
        var from = cwd.openDir(io, source, .{}) catch return .{ .code = 1 };
        defer from.close(io);
        var to = cwd.openDir(io, folder, .{}) catch return .{ .code = 1 };
        defer to.close(io);
        copyFile(gpa, io, from, to, "desc.xls") catch |err| {
            std.debug.print("elk: smoke FAIL: could not copy {s}{c}desc.xls: {s}\n", .{ source, std.fs.path.sep, @errorName(err) });
            return .{ .code = 1 };
        };
    }
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    return scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = sheet_schedule, .dialog_folder = folder, .frame_limit = smoke_frame_limit, .tier = "smoke (spreadsheet export and import)" });
}

/// The smoke's worker pass on a tenth copy, `<scratch>-progress`, with
/// `d14.csv` beside it: the copy's own sheet, every translation replaced by
/// `auto.whole_marker` and the row's number.
fn smokeProgress(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-progress", .{scratch}) catch return .{ .code = 1 };
    const copied = copyDatabase(gpa, io, source, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    writeProgressSheet(gpa, io, elk_path, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not write {s} into {s}: {s}\n", .{ progress_sheet, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    return scripted(gpa, io, environ, .{
        .elk_path = elk_path,
        .text = progress_schedule,
        .source = source,
        .source_files = copied,
        .dialog_folder = folder,
        .frame_limit = smoke_frame_limit,
        .tier = "smoke (worker, progress modal, Cancel)",
    });
}

/// The smoke's package import pass in an eleventh folder,
/// `<scratch>-packages`: a new `elk.xml` without elements, as the core
/// writes an empty project, the game folder `game` and `usa01.pak` beside
/// it, all from the tracked Data beside `source`.
fn smokePackages(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-packages", .{scratch}) catch return .{ .code = 1 };
    writePackageFolder(gpa, io, std.fs.path.dirname(source) orelse ".", folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not lay out {s}: {s}\n", .{ folder, @errorName(err) });
        return .{ .code = 1 };
    };
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    return scripted(gpa, io, environ, .{
        .elk_path = elk_path,
        .text = package_schedule,
        .dialog_folder = folder,
        .frame_limit = smoke_frame_limit,
        .tier = "smoke (Import from Game, Import from PAK/UPD)",
    });
}

fn writePackageFolder(gpa: std.mem.Allocator, io: std.Io, data: []const u8, folder: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, folder) catch {};
    try cwd.createDirPath(io, folder);
    var to = try cwd.openDir(io, folder, .{});
    defer to.close(io);
    var project = elk.project_file.Project.empty(gpa);
    defer project.deinit();
    const xml = try elk.project_file.write(gpa, &project, "elk.xml");
    defer gpa.free(xml);
    try to.writeFile(io, .{ .sub_path = "elk.xml", .data = xml });

    var from = try cwd.openDir(io, data, .{});
    defer from.close(io);
    const textes = try elk.database.findEntry(gpa, io, data, "textes", .directory) orelse return error.FileNotFound;
    defer gpa.free(textes);
    var texts = try cwd.openDir(io, textes, .{ .iterate = true });
    defer texts.close(io);
    var walker = try texts.walk(gpa);
    defer walker.deinit();
    while (try walker.next(io)) |file| {
        if (file.kind != .file) continue;
        const relative = try std.fs.path.join(gpa, &.{ "game", "Textes", file.path });
        defer gpa.free(relative);
        try to.createDirPath(io, std.fs.path.dirname(relative).?);
        const bytes = try texts.readFileAlloc(io, file.path, gpa, .limited(16 << 20));
        defer gpa.free(bytes);
        try to.writeFile(io, .{ .sub_path = relative, .data = bytes });
    }
    const resource = try from.readFileAlloc(io, "resource.description", gpa, .limited(1 << 20));
    defer gpa.free(resource);
    try to.writeFile(io, .{ .sub_path = "game" ++ std.fs.path.sep_str ++ "resource.description", .data = resource });
    const pak = try from.readFileAlloc(io, "AmericanELK" ++ std.fs.path.sep_str ++ package_pak, gpa, .limited(16 << 20));
    defer gpa.free(pak);
    try to.writeFile(io, .{ .sub_path = package_pak, .data = pak });
}

fn writeProgressSheet(gpa: std.mem.Allocator, io: std.Io, elk_path: []const u8, folder: []const u8) !void {
    var database = try elk.database.Database.open(gpa, io, elk_path);
    defer database.deinit();
    var arena_state = std.heap.ArenaAllocator.init(gpa);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const grid = try arena.dupe([]const []const u8, try elk.sheet.exportGrid(arena, &database, null));
    for (grid[1..], 1..) |row, number| {
        const changed = try arena.dupe([]const u8, row);
        changed[2] = try std.fmt.allocPrint(arena, "{s}{d}", .{ auto.whole_marker, number });
        grid[number] = changed;
    }
    const bytes = try elk.sheet.writeCsv(gpa, grid);
    defer gpa.free(bytes);
    var dir = try std.Io.Dir.cwd().openDir(io, folder, .{});
    defer dir.close(io);
    try dir.writeFile(io, .{ .sub_path = progress_sheet, .data = bytes });
}

/// The smoke's short layout pass on a ninth copy, `<scratch>-short`, with
/// its own `<scratch>-short.cfg`: after the first run the copy's elk.xml
/// records game_24.upd, read back by the core.
fn smokeShort(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, source: []const u8, scratch: []const u8) Scripted {
    var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const folder = std.fmt.bufPrint(&folder_buffer, "{s}-short", .{scratch}) catch return .{ .code = 1 };
    var cfg_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const cfg_path = std.fmt.bufPrint(&cfg_buffer, "{s}.cfg", .{folder}) catch return .{ .code = 1 };
    _ = copyDatabase(gpa, io, source, folder) catch |err| {
        std.debug.print("elk: smoke FAIL: could not copy {s} to {s}: {s}\n", .{ source, folder, @errorName(err) });
        return .{ .code = 1 };
    };
    const cwd = std.Io.Dir.cwd();
    cwd.deleteFile(io, cfg_path) catch {};
    {
        var from = cwd.openDir(io, source, .{}) catch return .{ .code = 1 };
        defer from.close(io);
        var to = cwd.openDir(io, folder, .{}) catch return .{ .code = 1 };
        defer to.close(io);
        const bytes = from.readFileAlloc(io, "game_23.upd", gpa, .limited(16 << 20)) catch |err| {
            std.debug.print("elk: smoke FAIL: could not read {s}{c}game_23.upd: {s}\n", .{ source, std.fs.path.sep, @errorName(err) });
            return .{ .code = 1 };
        };
        defer gpa.free(bytes);
        for ([_][]const u8{ "game_23.upd", "game_24.upd" }) |name| to.writeFile(io, .{ .sub_path = name, .data = bytes }) catch |err| {
            std.debug.print("elk: smoke FAIL: could not write {s} into {s}: {s}\n", .{ name, folder, @errorName(err) });
            return .{ .code = 1 };
        };
    }
    const first = scripted(gpa, io, environ, .{ .elk_path = null, .short_folder = folder, .text = short_schedule, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (short layout, Help)" });
    if (first.code != 0) return first;

    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const elk_path = std.fmt.bufPrint(&path_buffer, "{s}{c}elk.xml", .{ folder, std.fs.path.sep }) catch return .{ .code = 1 };
    {
        var database = elk.database.Database.open(gpa, io, elk_path) catch |err| {
            std.debug.print("elk: smoke FAIL: the short copy did not reopen: {s}\n", .{@errorName(err)});
            return .{ .code = 1 };
        };
        defer database.deinit();
        const element = database.project.elements[0];
        if (element.last_update_number != 24 or !std.mem.startsWith(u8, element.version, "game_24.upd, [")) {
            std.debug.print("elk: smoke FAIL: after the short start elk.xml records update {d}, \"{s}\", wanted game_24.upd\n", .{ element.last_update_number, element.version });
            return .{ .code = 1 };
        }
    }
    const again = scripted(gpa, io, environ, .{ .elk_path = null, .short_folder = folder, .text = short_reopen_schedule, .shot_folder = folder, .settings_path = cfg_path, .frame_limit = smoke_frame_limit, .tier = "smoke (short layout after a restart)" });
    if (again.code != 0) return again;
    if (!shotHasInk(gpa, io, folder, short_shot_name)) return .{ .code = 1 };
    if (!hindiHasClusters(gpa, io, folder, hindi_shot_name)) return .{ .code = 1 };
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
    // The scratch elk.cfg of BK_ELK_SETTINGS, if any: its ui_language picks the run's language.
    const settings_path = envValue(gpa, environ, settings_mod.settings_env);
    defer if (settings_path) |value| gpa.free(value);
    const result = scripted(gpa, io, environ, .{ .elk_path = elk_path, .text = text, .settings_path = settings_path orelse "", .frame_limit = auto_frame_limit, .tier = "auto" });
    if (result.code == 0) std.debug.print("elk: auto PASS: {d} frames\n", .{result.frames});
    return result.code;
}

/// What `--l10n-auto` captures: the main window, the File menu open, the help window. The frames
/// leave a gap after each command so the frame the shot reads has drawn its effect.
const l10n_auto_schedule =
    "3:shot=main,4:do=open_menu:elk.menu.file,7:shot=menu,8:key=escape,10:do=help,13:shot=help,14:exit";

fn l10nAutoMode(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, elk_path: []const u8, scratch: []const u8) u8 {
    crt.attachParentConsole();
    const settings_path = envValue(gpa, environ, settings_mod.settings_env);
    defer if (settings_path) |value| gpa.free(value);
    const result = scripted(gpa, io, environ, .{
        .elk_path = elk_path,
        .text = l10n_auto_schedule,
        .shot_folder = scratch,
        .rect_log = true,
        .settings_path = settings_path orelse "",
        .frame_limit = smoke_frame_limit,
        .tier = "l10n-auto",
    });
    if (result.code == 0) std.debug.print("elk: l10n-auto PASS: {d} frames\n", .{result.frames});
    return result.code;
}

const Scripted = struct { code: u8, frames: u32 = 0, fonts: usize = 0 };

const Script = struct {
    /// Opened before the first frame; null starts with nothing open.
    elk_path: ?[]const u8,
    /// Starts the short layout with this folder as ELK's own instead.
    short_folder: []const u8 = "",
    text: []const u8,
    /// The database the smoke copied and how many files, for its expectations.
    source: []const u8 = "",
    source_files: usize = 0,
    /// Where `do=answer` answers File > Open (Runner.dialog_folder).
    dialog_folder: []const u8 = "",
    /// Where `shot=` writes its TGAs (Runner.shot_folder).
    shot_folder: []const u8 = "",
    /// The run's own elk.cfg, read when it is there (a later run of the same
    /// pass) and written as ELK writes it; "" keeps the settings in memory.
    /// The user's own file is never touched.
    settings_path: []const u8 = "",
    /// The game folder the exports and Run Game use; "" keeps elk.cfg's.
    game_folder: []const u8 = "",
    /// Logs the rect of every label a shot's frame drew (Ui.rect_log).
    rect_log: bool = false,
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
    // A scripted run follows ui_language when the scratch cfg sets one and is English otherwise,
    // whatever the machine's locale.
    const font_dir = envValue(gpa, environ, kit.fonts.dir_env);
    defer if (font_dir) |value| gpa.free(value);
    const language = ui_mod.chooseLanguage(&settings, true);
    const loaded = kit.fonts.load(gpa, io, font_dir, language) catch |err| {
        std.debug.print("elk: {s} FAIL: the fonts did not load ({s})\n", .{ tier, @errorName(err) });
        return .{ .code = 1 };
    };
    const table_dir = envValue(gpa, environ, l10n.override_env);
    defer if (table_dir) |value| gpa.free(value);
    l10n.setOverrideDir(io, table_dir);
    l10n.setLanguage(language);
    var ui: ui_mod.Ui = .{
        .gpa = gpa,
        .io = io,
        .session = &session,
        .settings = &settings,
        .settings_path = script.settings_path,
        .environ = environ,
        .os_dialogs = false,
        .rect_log = script.rect_log,
    };
    defer ui.deinit();
    if (script.short_folder.len != 0) {
        ui.startShort(script.short_folder);
    } else if (script.elk_path) |path| ui.openDatabase(path);
    var runner: auto.Runner = .{
        .entries = entries,
        .source_folder = script.source,
        .source_files = script.source_files,
        .dialog_folder = script.dialog_folder,
        .shot_folder = script.shot_folder,
        .gpa = gpa,
        .io = io,
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

// The C main mainCRTStartup calls on Windows (crt.zig minimalFromPeb says why),
// as in the Map and Resource Editors: the build makes the CRT's entry ELK's too.
comptime {
    if (crt.exports_c_main) @export(&crtMain, .{ .name = "main" });
}

fn crtMain(argc: c_int, argv: ?*anyopaque) callconv(.c) c_int {
    _ = argc;
    _ = argv;
    main(crt.minimalFromPeb()) catch |err| {
        std.debug.print("elk: {s}\n", .{@errorName(err)});
        return 1;
    };
    return 0;
}
