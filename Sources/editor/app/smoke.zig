//! MapEditor --smoke: the interactive loop (main.zig `run`), hidden, driven
//! by synthetic SDL events instead of a person. One table, `script`, holds
//! every step: what is pushed onto SDL's queue before a frame, and what must
//! be true of the editor after it. Plan 6's BK_EDITOR_AUTO generalises the
//! table; nothing else here knows which steps there are.
//!
//! Screen positions are offsets from the screen's centre, where State.init
//! centres the camera on the map's middle: the same map point on every
//! screen size (1280x800 on the macOS runner, 1008x681 on the Windows one),
//! and clear of the panels on both (the left panel ends at 280, the right
//! one starts 320 from the right edge). Each step asserts the edit it made
//! actually landed - a tile painted, an object added, the selection moved -
//! so events swallowed on the way (a panel's capture, a hidden window) fail
//! the smoke rather than pass it silently.
//!
//! Select, turn, drag and delete act on the object the script placed,
//! clicked where it was put: a unit, which the engine turns (it turns none
//! of the map's static objects near the centre). One click on an object of
//! the map keeps a loaded object's pick covered.
const std = @import("std");
const sdl3 = @import("sdl3");
const core = @import("editor_core");
const c_bridge = @import("c_bridge.zig");
const view_mod = @import("view.zig");
const view_math = @import("view_math.zig");
const panels = @import("panels.zig");
const panels_logic = @import("panels_logic.zig");
const imgui = @import("editor_imgui");
const auto_mod = @import("auto.zig");

const sdl = sdl3.c;
const c = c_bridge.c;
const Editor = core.editor.Editor;
const Pose = core.editor.Pose;
const View = view_mod.View;
const RealBridge = c_bridge.RealBridge;

/// Pixels from the screen's centre.
pub const Pos = struct { dx: f32, dy: f32 };

pub const Key = struct {
    key: sdl.SDL_Keycode,
    scancode: sdl.SDL_Scancode,
    mod: sdl.SDL_Keymod = 0,
};

pub const Input = union(enum) {
    /// Key down and up.
    key: Key,
    /// Left button down, after a motion to the point as a real pointer
    /// would make.
    press: Pos,
    /// A motion with the left button held.
    drag: Pos,
    /// A motion to the point with the button held, then the button up.
    release: Pos,
    /// What the Save As and Open dialogs hand the panels (the dialog itself
    /// needs a person): the smoke's output path.
    save_as,
    open_saved,
    /// A motion to the point, then `count` wheel events of these deltas
    /// there, as a trackpad swipe sends them: small fractions, one per
    /// NSEvent.
    wheel: Wheel,
    /// Pushes SDL_EVENT_WINDOW_CLOSE_REQUESTED for the window, as clicking
    /// its close button would (D-23).
    window_close,
    /// Sets state.actions.save_requested, as the Save menu item would - to
    /// prove a shipped map's Save redirects to Save As (D-18) without a
    /// widget.
    save_requested,
    /// Calls the unsaved-changes prompt's answer as the modal's Save/Don't
    /// save/Cancel button would (D-23) - the smoke has no widget to click.
    answer: panels_logic.UnsavedPrompt.Choice,
    /// Calls panels.addSoundAtViewCentre directly, as the Sounds panel's
    /// "Add at view centre" button would - no widget here to click either.
    add_sound_at_view_centre,
    /// Builds a second game installation's data tree beside the smoke's
    /// output (`foreign_tree`: a Data folder with a data-root marker and a
    /// copy of the smoke's own map), records the copy's bytes, inode and
    /// mtime, and opens it by its absolute path through the Open dialog's
    /// slot - 03-15's hand try, where MapEditor opened another checkout's
    /// coldwinter.bzm and Save wrote into it.
    open_foreign,
    /// Two autosave ticks an interval apart (panels.tickAutosave, as the
    /// interactive loop calls it - the smoke's loop never does): the second
    /// one is due, and must go to a recovery copy (D-22), never the file.
    autosave_due,
    /// Calls editor.save on the document's own path directly - a caller that
    /// forgot the read-only rule - which Editor.save itself must refuse.
    save_in_place_forced,
    /// Chooses a mod in File > Mod, as its menu item would (D-26): this
    /// folder, "" for None, through `requestSwitchMod` with the mod active
    /// now - no widget here to click.
    switch_mod: []const u8,
    /// Cancels the file dialog the slot is waiting on, as the dialog's own
    /// Cancel button would - unlike `save_became_save_as`'s own drain, the
    /// cancel reaches `act`, so the unsaved-changes prompt that asked for
    /// the dialog hears it.
    cancel_dialog,
};

pub const Wheel = struct {
    /// Where the pointer is; `over_left_panel` puts it inside the left
    /// panel instead, at the centre's height.
    at: Pos = .{ .dx = 0, .dy = 0 },
    over_left_panel: bool = false,
    x: f32,
    y: f32,
    count: u8,
    flipped: bool = false,
    /// Held for a Shift+wheel zoom step (D-10). Pushed key events do not
    /// change SDL's own modifier state (SDL_GetModState, which view.zig's
    /// handleWheel reads), so the script sets it directly before pushing the
    /// wheel events and restores it to none right after.
    mods: sdl.SDL_Keymod = 0,
};

/// What must be true after the step's frame.
pub const Expect = enum {
    tool_brush,
    tool_place,
    tool_select,
    /// The cells under `ground_a` and `ground_b` show another tile in the
    /// engine, as one undo step, and the engine agrees with the map.
    painted,
    /// One more object, selected: the placer's.
    placed,
    nothing_selected,
    /// The map's object under `target_pick` is selected.
    target_selected,
    /// The placed object, found by a click where it was put, is selected.
    placed_selected,
    /// The placed object turned a sixteenth clockwise, and the engine agrees.
    rotated,
    /// The placed object has left where it was put, is still selected, a
    /// click at the drag's end finds it, and the engine agrees.
    moved,
    /// The placed object is gone, and the engine agrees.
    deleted,
    /// Nothing left to undo: the count, the target's pose and the two cells
    /// as the map had them, the placed object gone, the document clean, and
    /// the engine agreeing with it.
    all_undone,
    saved,
    /// The saved map, opened again, has the original's object count.
    reopened,
    /// The camera moved from where the step started by what the step's
    /// wheel events sum to (view_math.wheelPan), every event the same way.
    panned,
    /// The camera is where the step started: the step's wheel was not the
    /// view's.
    camera_unchanged,
    /// ImGui wants the mouse (the pointer is over a panel), within
    /// `max_wait_frames` frames; the camera has not moved meanwhile.
    panel_has_pointer,
    /// The unsaved-changes prompt is asking (D-23): the map is still dirty
    /// and the loop is still running - closing the window did not discard
    /// anything.
    unsaved_prompt_open,
    /// Cancel resolved the prompt without discarding the map: not asking
    /// any more, still dirty, still at the same path.
    prompt_cancelled,
    /// Save on a shipped map redirected to Save As (D-18) instead of
    /// writing the shipped file: the dialog slot is waiting for one.
    save_became_save_as,
    /// A Shift+wheel step zoomed in (D-10): zoom_steps rose, and the world
    /// point under the wheel's own screen position stayed put (D-14).
    zoomed_at_pointer,
    /// Home reset the zoom to 0 (D-13).
    view_reset,
    /// "Add at view centre" added one sound through the bridge's own list
    /// (T-03-10-01/02's own ABI), as one undo step.
    sound_added,
    /// Ctrl+Z undid the add: the sound list is back to what the map had.
    sound_removed,
    /// The second tree's map is open by its absolute path, and the panels
    /// call it read-only (D-18): Save would be Save As.
    foreign_read_only,
    /// The title says "(read-only)", Save redirected to Save As (as
    /// `save_became_save_as`), and the second
    /// tree's map is byte for byte, inode and mtime what it was, with no
    /// `.bak` and no temporary file beside it.
    foreign_save_became_save_as,
    /// The due autosave wrote a recovery copy into the (smoke's own) user
    /// folder, and the second tree's map is untouched.
    foreign_autosaved_to_recovery,
    /// Editor.save refused the forced in-place save (error.Refused, a
    /// "read-only" status), and the map is untouched.
    foreign_save_refused,
    /// File > Mod chose the mod already active (D-26, revised 2026-09-29):
    /// nothing happened - no prompt, the same map open and still dirty, the
    /// same mod, the palette not re-read.
    mod_switch_no_op,
    /// File > Mod on a dirty map asked first (D-23): the prompt is asking,
    /// the map is still open and dirty, and the mod has not changed yet.
    mod_switch_asked,
    /// Save on the prompt, for a read-only map, became Save As: the dialog
    /// slot waits for one, no real dialog was opened, the mod unchanged.
    mod_switch_save_as_waiting,
    /// The prompt's Cancel, or a cancelled Save As, cancelled the switch:
    /// nothing asking or saving, the map open and dirty, the mod unchanged.
    mod_switch_cancelled,
    /// Don't save closed the map and switched (D-26 revised): no map open,
    /// nothing to undo, the new mod active in the panels and the bridge, the
    /// palette re-read from it, the view and the per-map panels as with no
    /// map, autosave idle, and the map's recovery copy deleted (D-22).
    mod_switched_map_closed,
    /// A switch with no map open went straight through (nothing to guard):
    /// None again, still no map, the palette re-read once more.
    mod_switched_without_map,
};

pub const Step = struct {
    name: []const u8,
    inputs: []const Input,
    expect: Expect,
};

// Measured on coldwinter by resolving a 20-pixel grid around the centre:
// the band 100-180 pixels above it has no object from 200 left to 160
// right, and the poplar with link ID 101 answers every point from 80 to
// 200 below it and 60 left of it to the centre.
const ground_a: Pos = .{ .dx = -120, .dy = -160 };
const ground_between: Pos = .{ .dx = -95, .dy = -160 };
const ground_b: Pos = .{ .dx = -70, .dy = -160 };
const place_at: Pos = .{ .dx = -40, .dy = -120 };
/// Where a click finds the placed object: above its foot, as a sprite's hit
/// box rises from it (the engine tier's PICK_RISE).
const placed_pick: Pos = .{ .dx = -40, .dy = -132 };
const drag_via: Pos = .{ .dx = -20, .dy = -132 };
const drag_to: Pos = .{ .dx = 0, .dy = -132 };
/// Ground with no object on it, for a click that selects nothing.
const empty_ground: Pos = .{ .dx = 100, .dy = -140 };
/// An object of the map, editable (not one of several sharing a link ID).
const target_pick: Pos = .{ .dx = -20, .dy = 140 };
/// Inside the left panel, which ends 280 pixels from the window's left edge
/// on every screen: an absolute x, unlike the `Pos` offsets from the centre.
const left_panel_x: f32 = 100;

fn plain(key: sdl.SDL_Keycode, scancode: sdl.SDL_Scancode) Input {
    return .{ .key = .{ .key = key, .scancode = scancode } };
}

/// Ctrl+Z; view.zig takes Ctrl or Cmd on every platform.
const undo_key: Input = .{ .key = .{ .key = sdl.SDLK_Z, .scancode = sdl.SDL_SCANCODE_Z, .mod = sdl.SDL_KMOD_LCTRL } };

pub const script = [_]Step{
    .{ .name = "key 2 chooses the brush", .inputs = &.{plain(sdl.SDLK_2, sdl.SDL_SCANCODE_2)}, .expect = .tool_brush },
    .{ .name = "a brush stroke paints", .inputs = &.{ .{ .press = ground_a }, .{ .drag = ground_between }, .{ .drag = ground_b }, .{ .release = ground_b } }, .expect = .painted },
    .{ .name = "key 3 chooses the placer", .inputs = &.{plain(sdl.SDLK_3, sdl.SDL_SCANCODE_3)}, .expect = .tool_place },
    .{ .name = "a click places an object", .inputs = &.{ .{ .press = place_at }, .{ .release = place_at } }, .expect = .placed },
    .{ .name = "key 1 chooses the selector, the new object still selected", .inputs = &.{plain(sdl.SDLK_1, sdl.SDL_SCANCODE_1)}, .expect = .tool_select },
    .{ .name = "a click on bare ground selects nothing", .inputs = &.{ .{ .press = empty_ground }, .{ .release = empty_ground } }, .expect = .nothing_selected },
    .{ .name = "a click on an object of the map selects it", .inputs = &.{ .{ .press = target_pick }, .{ .release = target_pick } }, .expect = .target_selected },
    .{ .name = "a click on the placed object selects it", .inputs = &.{ .{ .press = placed_pick }, .{ .release = placed_pick } }, .expect = .placed_selected },
    .{ .name = "E turns it", .inputs = &.{plain(sdl.SDLK_E, sdl.SDL_SCANCODE_E)}, .expect = .rotated },
    .{ .name = "a drag moves it", .inputs = &.{ .{ .press = placed_pick }, .{ .drag = drag_via }, .{ .drag = drag_to }, .{ .release = drag_to } }, .expect = .moved },
    .{ .name = "Delete deletes it", .inputs = &.{plain(sdl.SDLK_DELETE, sdl.SDL_SCANCODE_DELETE)}, .expect = .deleted },
    // Paint, place, turn, move, delete: five edits, five undos.
    .{ .name = "Ctrl+Z undoes all of it", .inputs = &.{ undo_key, undo_key, undo_key, undo_key, undo_key }, .expect = .all_undone },
    // D-18: the smoke's own map is opened by its shipped relative path
    // ("Data\..."), so Save on it must redirect to Save As rather than
    // overwrite the shipped file.
    .{ .name = "Save on a shipped map becomes Save As", .inputs = &.{.save_requested}, .expect = .save_became_save_as },
    .{ .name = "Save As writes the map", .inputs = &.{.save_as}, .expect = .saved },
    .{ .name = "the saved map opens again", .inputs = &.{.open_saved}, .expect = .reopened },
    // The Sounds panel (Task 3): "Add at view centre" adds a sound as one
    // undo step; Ctrl+Z removes it, back to what the reopened map had.
    .{ .name = "Add at view centre adds a sound", .inputs = &.{.add_sound_at_view_centre}, .expect = .sound_added },
    .{ .name = "Ctrl+Z removes it", .inputs = &.{undo_key}, .expect = .sound_removed },
    // D-23: the unsaved-changes prompt on a window close, and Cancel.
    .{ .name = "key 2 chooses the brush again", .inputs = &.{plain(sdl.SDLK_2, sdl.SDL_SCANCODE_2)}, .expect = .tool_brush },
    .{ .name = "a brush stroke makes the map dirty", .inputs = &.{ .{ .press = ground_a }, .{ .drag = ground_between }, .{ .drag = ground_b }, .{ .release = ground_b } }, .expect = .painted },
    .{ .name = "closing the window asks", .inputs = &.{.window_close}, .expect = .unsaved_prompt_open },
    .{ .name = "Cancel keeps it", .inputs = &.{.{ .answer = .cancel }}, .expect = .prompt_cancelled },
    .{ .name = "Ctrl+Z undoes the stroke", .inputs = &.{undo_key}, .expect = .all_undone },
    // Task 7.3: a two-finger swipe, as SDL delivers it on macOS - many
    // small fractional deltas on both axes - pans the map; the same swipe
    // back pans it back; a flipped (natural scrolling) swipe pans by the
    // sign SDL delivered; a wheel over a panel leaves the map alone.
    .{ .name = "a swipe pans the map", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = 0.15, .y = 0.35, .count = 12 } }}, .expect = .panned },
    .{ .name = "the swipe back pans it back", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = -0.15, .y = -0.35, .count = 12 } }}, .expect = .panned },
    .{ .name = "a natural-scrolling swipe pans by SDL's sign", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = -0.2, .y = 0.1, .count = 6, .flipped = true } }}, .expect = .panned },
    // Plan 6, D-10/D-13/D-14: run right after the swipe-pan steps above, so
    // every step before this ran unzoomed and the pointer is still over the
    // map (not stuck over the left panel, whose WantCaptureMouse lag the
    // steps below this comment work around) - Shift+wheel zooms in at the
    // pointer, a plain swipe at that zoom still follows the fingers 1:1, and
    // Home resets the zoom.
    .{ .name = "Shift + wheel zooms in at the pointer", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = 0, .y = 1, .count = 1, .mods = sdl.SDL_KMOD_LSHIFT } }}, .expect = .zoomed_at_pointer },
    .{ .name = "a swipe at zoom follows the fingers", .inputs = &.{.{ .wheel = .{ .at = empty_ground, .x = 0.15, .y = 0.35, .count = 12 } }}, .expect = .panned },
    .{ .name = "Home resets the view", .inputs = &.{plain(sdl.SDLK_HOME, sdl.SDL_SCANCODE_HOME)}, .expect = .view_reset },
    // ImGui decides WantCaptureMouse in a later frame from where the
    // pointer is (its input queue trickles one kind of event per frame, and
    // the steps before left a backlog), so the pointer rests on the panel
    // until ImGui has it, as a hand's would before it swipes.
    .{ .name = "the pointer rests on the left panel", .inputs = &.{.{ .wheel = .{ .over_left_panel = true, .x = 0, .y = 0, .count = 0 } }}, .expect = .panel_has_pointer },
    .{ .name = "a wheel over a panel leaves the map alone", .inputs = &.{.{ .wheel = .{ .over_left_panel = true, .x = 0, .y = -1, .count = 3 } }}, .expect = .camera_unchanged },
    // 03-15's gap fix (D-18/D-20/D-22): a map inside ANOTHER installation's
    // Data, opened by its absolute path, is read-only too - Save becomes
    // Save As, autosave writes a recovery copy, and Editor.save itself
    // refuses to write there. Last, so every step above ran on the smoke's
    // own map; the map is dirtied by a sound (no pointer involved - the
    // pointer is still resting on the left panel).
    .{ .name = "another installation's map opens read-only", .inputs = &.{.open_foreign}, .expect = .foreign_read_only },
    .{ .name = "Add at view centre dirties it", .inputs = &.{.add_sound_at_view_centre}, .expect = .sound_added },
    .{ .name = "Save on it becomes Save As, the file untouched", .inputs = &.{.save_requested}, .expect = .foreign_save_became_save_as },
    .{ .name = "a due autosave writes a recovery copy, the file untouched", .inputs = &.{.autosave_due}, .expect = .foreign_autosaved_to_recovery },
    .{ .name = "a forced in-place save is refused, the file untouched", .inputs = &.{.save_in_place_forced}, .expect = .foreign_save_refused },
    // D-26, revised 2026-09-29 in the hand try: switching the mod closes the
    // map (a map read under one object database and shown under another
    // mixed them). The read-only map from the steps above is still open and
    // dirty, with a recovery copy active. The fixture mod (EditorTestMod,
    // staged for this step by build.zig - never AchtungPanzer2).
    .{ .name = "File > Mod None, already active, changes nothing", .inputs = &.{.{ .switch_mod = "" }}, .expect = .mod_switch_no_op },
    .{ .name = "File > Mod on the dirty map asks first", .inputs = &.{.{ .switch_mod = smoke_mod }}, .expect = .mod_switch_asked },
    .{ .name = "Cancel keeps the map and the mod", .inputs = &.{.{ .answer = .cancel }}, .expect = .mod_switch_cancelled },
    .{ .name = "File > Mod asks again", .inputs = &.{.{ .switch_mod = smoke_mod }}, .expect = .mod_switch_asked },
    .{ .name = "Save on the read-only map becomes Save As", .inputs = &.{.{ .answer = .save }}, .expect = .mod_switch_save_as_waiting },
    .{ .name = "cancelling that Save As cancels the switch", .inputs = &.{.cancel_dialog}, .expect = .mod_switch_cancelled },
    .{ .name = "File > Mod asks a third time", .inputs = &.{.{ .switch_mod = smoke_mod }}, .expect = .mod_switch_asked },
    .{ .name = "Don't save closes the map and switches the mod", .inputs = &.{.{ .answer = .dont_save }}, .expect = .mod_switched_map_closed },
    .{ .name = "File > Mod None with no map open switches back", .inputs = &.{.{ .switch_mod = "" }}, .expect = .mod_switched_without_map },
};

/// The mod the smoke switches to: the tracked fixture
/// (tools/zig/fixtures/editor_mod), which build.zig stages at
/// `<stage>/mods/EditorTestMod` before `map-editor-smoke` runs.
pub const smoke_mod = "EditorTestMod";

/// The second installation `open_foreign` builds, beside the smoke's output
/// (zig-out/local-test): `<dir>/foreign_tree/Data/...`, never inside the
/// staged installation's own Data.
pub const foreign_tree = "map-editor-smoke-foreign";
const foreign_map = "Data" ++ std.fs.path.sep_str ++ "Maps" ++ std.fs.path.sep_str ++ "Multiplayer" ++ std.fs.path.sep_str ++ "coldwinter.bzm";
/// Where the smoke's user root points (main.zig's smokeRun), so the due
/// autosave's recovery copy never lands in the person's own user folder.
pub const smoke_user_root = "map-editor-smoke-user";

/// What the second tree's map was when `open_foreign` copied it.
const FileFacts = struct {
    size: u64,
    inode: std.Io.File.INode,
    mtime: i96,
    hash: u64,
};

/// Frames drawn before the first step, so the panels have been laid out,
/// ImGui's capture flags describe them, and the bridge has a drawn frame to
/// resolve screen points against.
const settle_frames = 2;

/// How long a `panel_has_pointer` step may wait for ImGui.
const max_wait_frames = 60;

/// An SDL event the loop polled that the script never pushes - a focus
/// change, the pointer entering or leaving, a resize: what a hidden window
/// on a CI desktop still gets from the OS. Kept for a FAIL's state line.
const Observed = struct { frame: usize, type: u32, x: f32 = 0, y: f32 = 0 };
const observed_capacity = 12;

/// The `which` of every mouse event the script pushes, so `observe` tells
/// them from the OS's own (SDL_GLOBAL_MOUSE_ID, 0). Anything but
/// SDL_TOUCH_MOUSEID reads as a mouse to ImGui's backend, which is all it
/// reads `which` for; the view does not read it.
const smoke_mouse_id: sdl.SDL_MouseID = 0x534D4B45;

/// Where ImGui had the pointer during the current step, each time it moved.
const PointerSample = struct { frame: usize, x: f32, y: f32 };
const pointer_trail_capacity = 8;

/// The sibling `<dir>/<stem>.~save<ext>` a safe save writes to before the
/// swap (core/files.zig's tempPathFor, plan 6's D-19) - null when `path` does
/// not fit `buffer` or has no extension to preserve.
fn tempSiblingPath(buffer: []u8, path: []const u8) ?[]const u8 {
    const ext = std.fs.path.extension(path);
    const dir = std.fs.path.dirname(path) orelse ".";
    const base = std.fs.path.basename(path);
    const stem = base[0 .. base.len - ext.len];
    return std.fmt.bufPrint(buffer, "{s}{c}{s}.~save{s}", .{ dir, std.fs.path.sep, stem, ext }) catch null;
}

pub const Script = struct {
    editor: *Editor,
    view: *View,
    real: *RealBridge,
    state: *panels.State,
    window_id: sdl.SDL_WindowID,
    save_path: []const u8,

    centre_x: f32 = 0,
    centre_y: f32 = 0,
    frame: usize = 0,
    step: usize = 0,
    passed: bool = false,
    /// A FAIL line has been printed.
    reported: bool = false,

    original_objects: usize,
    /// The map's own sound count, read once at init through the same bridge
    /// call `sound_added`/`sound_removed` check against.
    original_sounds: usize = 0,
    cell_a: [2]i32 = .{ 0, 0 },
    cell_b: [2]i32 = .{ 0, 0 },
    tile_a: u8 = 0,
    tile_b: u8 = 0,
    placed: i32 = -1,
    target: i32 = -1,
    target_pose: Pose = .{ .x = 0, .y = 0, .dir = 0, .player = 0 },
    placed_pose: Pose = .{ .x = 0, .y = 0, .dir = 0, .player = 0 },
    /// The view's camera when the step's inputs were pushed.
    camera_before: [2]f32 = .{ 0, 0 },
    /// The world point the drawn frame showed at the screen's centre then.
    centre_before: ?core.tools.Pointer = null,
    /// The view's zoom_steps when the step's inputs were pushed.
    zoom_steps_before: i32 = 0,
    /// The world point under a wheel step's own `at` position, before its
    /// inputs were pushed - for `zoomed_at_pointer` (D-14), since the zoom
    /// point is not always the screen's centre.
    wheel_point_before: ?core.tools.Pointer = null,
    /// Frames the current step has waited (panel_has_pointer); its inputs
    /// are not pushed again meanwhile.
    waited: usize = 0,

    /// For a FAIL's state line (`printState`): the last point the script
    /// put the pointer at, the OS's own events the loop polled (the last
    /// `observed_capacity` of `observed_total`), how many mouse events the
    /// script pushed against how many the loop polled, and the current
    /// step's trail of ImGui pointer positions.
    last_pointer: [2]f32 = .{ 0, 0 },
    observed: [observed_capacity]Observed = undefined,
    observed_total: usize = 0,
    /// The OS's events that came once the settle frames were drawn, while
    /// the script ran - `printNote` reports them on a PASS.
    observed_during_script: usize = 0,
    pushed_mouse: usize = 0,
    polled_mouse: usize = 0,
    pointer_trail: [pointer_trail_capacity]PointerSample = undefined,
    pointer_trail_len: usize = 0,

    /// `open_foreign`'s map: its absolute OS path, and what it was then.
    foreign_path: panels_logic.PathText = .{},
    foreign_facts: ?FileFacts = null,
    /// `save_in_place_forced`'s outcome: the error Editor.save returned, or
    /// null when it (wrongly) saved.
    forced_save_error: ?anyerror = null,
    /// The recovery copy `foreign_autosaved_to_recovery` found, for
    /// `mod_switched_map_closed` to see deleted.
    recovery_path: panels_logic.PathText = .{},
    /// The palette's `catalogue_generation` and the document's path when the
    /// step's inputs were pushed.
    catalogue_generation_before: u32 = 0,
    path_before: panels_logic.PathText = .{},

    /// After the map is open and State built.
    pub fn init(editor: *Editor, view: *View, real: *RealBridge, state: *panels.State, window: *sdl.SDL_Window, save_path: []const u8) Script {
        var none: [0]core.bridge.SoundRecord = .{};
        var sounds_count: usize = 0;
        _ = editor.bridge.sounds(&none, &sounds_count);
        return .{
            .editor = editor,
            .view = view,
            .real = real,
            .state = state,
            .window_id = sdl.SDL_GetWindowID(window),
            .save_path = save_path,
            .original_objects = editor.document.objects.items.len,
            .original_sounds = sounds_count,
        };
    }

    /// The screen and what is under the script's positions, read once the
    /// settle frames are drawn: the bridge resolves a screen point through
    /// the last frame's camera, so before any frame there is nothing under
    /// it, and a window the desktop was too small for has been resized by
    /// then (1008x681 on the Windows runner).
    fn prepare(self: *Script) bool {
        const size = self.real.screenSize() orelse return self.fail("no screen size", .{});
        self.centre_x = @as(f32, @floatFromInt(size[0])) / 2;
        self.centre_y = @as(f32, @floatFromInt(size[1])) / 2;
        self.cell_a = self.cellAt(ground_a) orelse return self.fail("no map cell under ground_a", .{});
        self.cell_b = self.cellAt(ground_b) orelse return self.fail("no map cell under ground_b", .{});
        self.tile_a = self.engineTile(self.cell_a) orelse return self.fail("the engine has no tile at {any}", .{self.cell_a});
        self.tile_b = self.engineTile(self.cell_b) orelse return self.fail("the engine has no tile at {any}", .{self.cell_b});
        // The brush's tile, as the palette would choose it. The engine
        // shows a tile of the painted one's terrain type, not always that
        // index (CTerrain::SetTile derives it), so the paint must change
        // terrain type to show: 0 and 14 are in every shipped tileset, in
        // different terrain types (c_bridge_test.zig picks them the same way).
        // Task 7, carried from plan 5: through State.chooseBrushTile, the
        // same call the brush combo itself makes, so the palette-to-brush
        // path is not left out of what the smoke exercises.
        self.state.chooseBrushTile(if (self.tile_a != 0 and self.tile_b != 0) 0 else 14);
        if (std.mem.indexOfScalar(u8, self.state.tiles(), self.view.brush.tile) == null)
            return self.fail("the tileset does not offer tile {d}", .{self.view.brush.tile});
        const empty = self.resolveAt(empty_ground) orelse return self.fail("empty_ground is off the terrain", .{});
        if (empty.object) |link_id| return self.fail("empty_ground has object {d} on it", .{link_id});
        // Nothing of the map where the placed object is put and clicked, so
        // a click there that answers can only answer with it. drag_via and
        // drag_to too (plan 5 Task 7.1, carried): the drag step's own move
        // must not cross another object on the way.
        for ([_]Pos{ ground_a, ground_b, place_at, placed_pick, drag_via, drag_to }) |pos| {
            const point = self.resolveAt(pos) orelse return self.fail("{any} is off the terrain", .{pos});
            if (point.object) |link_id| return self.fail("{any} has object {d} on it", .{ pos, link_id });
        }
        const target = self.resolveAt(target_pick) orelse return self.fail("target_pick is off the terrain", .{});
        self.target = target.object orelse return self.fail("no object under target_pick", .{});
        const objects = self.editor.document.objects.items;
        const index = self.editor.document.indexOf(self.target) orelse return self.fail("the map has no object {d}", .{self.target});
        if (panels_logic.readOnlyReason(objects, objects[index])) |reason|
            return self.fail("object {d} under target_pick is kept as it is: {s}", .{ self.target, reason });
        const object = objects[index];
        self.target_pose = .{ .x = object.x, .y = object.y, .dir = object.dir, .player = object.player };
        if (self.view.placer.name.len == 0) return self.fail("the placer has no object", .{});
        return true;
    }

    fn screen(self: *const Script, pos: Pos) [2]f32 {
        return .{ self.centre_x + pos.dx, self.centre_y + pos.dy };
    }

    fn resolveAt(self: *Script, pos: Pos) ?core.tools.Pointer {
        const point = self.screen(pos);
        return self.editor.resolve(point[0], point[1]) catch null;
    }

    fn cellAt(self: *Script, pos: Pos) ?[2]i32 {
        return (self.resolveAt(pos) orelse return null).tile;
    }

    fn engineTile(self: *Script, cell: [2]i32) ?u8 {
        var tile: u8 = 0;
        if (c.BkEditorEngineTile(self.real.session, cell[0], cell[1], &tile) != c.BK_EDITOR_OK) return null;
        return tile;
    }

    /// Before the frame's events are polled: the current step's inputs onto
    /// SDL's queue. False when the loop should stop.
    pub fn beforeFrame(self: *Script) bool {
        if (self.frame < settle_frames or self.step >= script.len) return true;
        if (self.frame == settle_frames and !self.prepare()) return false;
        if (self.waited != 0) return true;
        self.camera_before = .{ self.view.camera_x, self.view.camera_y };
        self.centre_before = self.resolveAt(.{ .dx = 0, .dy = 0 });
        self.zoom_steps_before = self.view.zoom_steps;
        self.catalogue_generation_before = self.state.catalogue_generation;
        self.path_before.set(self.editor.document.path.items);
        const inputs = script[self.step].inputs;
        self.wheel_point_before = if (inputs.len != 0 and inputs[0] == .wheel) self.resolveAt(inputs[0].wheel.at) else null;
        self.pointer_trail_len = 0;
        for (script[self.step].inputs) |input| {
            if (!self.deliver(input)) return false;
        }
        return true;
    }

    /// After the frame and the panels' file actions: the step's check.
    /// False when the loop should stop - the script finished or failed.
    pub fn afterFrame(self: *Script) bool {
        defer self.frame += 1;
        // A Shift+wheel step's SDL_SetModState was only meant to be seen by
        // this frame's own wheel processing, which has already happened by
        // the time afterFrame runs; clearing it here (rather than right
        // after pushing, in `deliver`) is what lets it survive to be read at
        // all. Unconditional and harmless on every other frame.
        sdl.SDL_SetModState(0);
        if (self.frame < settle_frames) return true;
        const step = script[self.step];
        self.notePointer();
        if (step.expect == .panel_has_pointer and !view_mod.captureFlags().mouse and self.waited < max_wait_frames) {
            self.waited += 1;
            return true;
        }
        // Reset only once the check passed, so a FAIL's state line still
        // says how long the step waited.
        if (!self.check(step)) return false;
        self.waited = 0;
        self.step += 1;
        if (self.step < script.len) return true;
        self.passed = true;
        return false;
    }

    fn deliver(self: *Script, input: Input) bool {
        switch (input) {
            .key => |key| return self.pushKey(key, true) and self.pushKey(key, false),
            .press => |pos| return self.pushMotion(pos, false) and self.pushButton(pos, true),
            .drag => |pos| return self.pushMotion(pos, true),
            .release => |pos| return self.pushMotion(pos, true) and self.pushButton(pos, false),
            .save_as, .open_saved => {
                const kind: panels_logic.DialogKind = if (input == .save_as) .save_as else .open;
                if (!self.state.actions.dialog.request(kind)) return self.fail("the dialog slot was busy", .{});
                self.state.actions.dialog.deliver(self.save_path);
                return true;
            },
            .window_close => {
                var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
                event.window.type = sdl.SDL_EVENT_WINDOW_CLOSE_REQUESTED;
                event.window.windowID = self.window_id;
                return self.push(&event);
            },
            .answer => |choice| {
                self.state.actions.answer_pending = choice;
                return true;
            },
            .save_requested => {
                self.state.actions.save_requested = true;
                return true;
            },
            .add_sound_at_view_centre => {
                panels.addSoundAtViewCentre(self.state);
                return true;
            },
            .open_foreign => {
                const path = self.buildForeignTree() orelse return false;
                if (!self.state.actions.dialog.request(.open)) return self.fail("the dialog slot was busy", .{});
                self.state.actions.dialog.deliver(path);
                return true;
            },
            .autosave_due => {
                self.state.settings.autosave = true;
                const interval_ms = @as(u64, self.state.settings.autosave_minutes) * std.time.ms_per_min;
                const start_ms: u64 = 1_000_000;
                panels.tickAutosave(self.state, start_ms);
                panels.tickAutosave(self.state, start_ms + interval_ms);
                return true;
            },
            .switch_mod => |folder| {
                self.state.actions.requestSwitchMod(folder, self.state.modFolder());
                return true;
            },
            .cancel_dialog => {
                if (!self.state.actions.dialog.waiting()) return self.fail("no dialog is waiting to be cancelled", .{});
                self.state.actions.dialog.deliver(null);
                return true;
            },
            .save_in_place_forced => {
                self.forced_save_error = null;
                self.editor.save(self.editor.document.path.items) catch |err| {
                    self.forced_save_error = err;
                };
                return true;
            },
            .wheel => |wheel| {
                const point = if (wheel.over_left_panel) [2]f32{ left_panel_x, self.centre_y } else self.screen(wheel.at);
                if (!self.pushMotionAt(point, false)) return false;
                // Held through the rest of this frame (cleared in
                // afterFrame): pushed key events do not update SDL's own
                // modifier state, which view.zig's handleWheel reads.
                if (wheel.mods != 0) sdl.SDL_SetModState(wheel.mods);
                for (0..wheel.count) |_| {
                    var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
                    event.wheel.type = sdl.SDL_EVENT_MOUSE_WHEEL;
                    event.wheel.windowID = self.window_id;
                    event.wheel.which = smoke_mouse_id;
                    event.wheel.x = wheel.x;
                    event.wheel.y = wheel.y;
                    event.wheel.direction = if (wheel.flipped) sdl.SDL_MOUSEWHEEL_FLIPPED else sdl.SDL_MOUSEWHEEL_NORMAL;
                    event.wheel.mouse_x = point[0];
                    event.wheel.mouse_y = point[1];
                    if (!self.push(&event)) return false;
                }
                return true;
            },
        }
    }

    fn push(self: *Script, event: *sdl.SDL_Event) bool {
        if (isMouseEvent(event.type)) self.pushed_mouse += 1;
        if (sdl.SDL_PushEvent(event)) return true;
        return self.fail("SDL_PushEvent: {s}", .{sdl.SDL_GetError()});
    }

    /// Every event the loop polls, before it is routed (main.zig's `run`):
    /// counts the mouse events and keeps the OS's own, for `printState`.
    pub fn observe(self: *Script, event: *const sdl.SDL_Event) void {
        var observed: Observed = .{ .frame = self.frame, .type = event.type };
        switch (event.type) {
            sdl.SDL_EVENT_MOUSE_MOTION => {
                self.polled_mouse += 1;
                if (event.motion.which == smoke_mouse_id) return;
                observed.x = event.motion.x;
                observed.y = event.motion.y;
            },
            sdl.SDL_EVENT_MOUSE_BUTTON_DOWN, sdl.SDL_EVENT_MOUSE_BUTTON_UP => {
                self.polled_mouse += 1;
                if (event.button.which == smoke_mouse_id) return;
                observed.x = event.button.x;
                observed.y = event.button.y;
            },
            sdl.SDL_EVENT_MOUSE_WHEEL => {
                self.polled_mouse += 1;
                if (event.wheel.which == smoke_mouse_id) return;
                observed.x = event.wheel.mouse_x;
                observed.y = event.wheel.mouse_y;
            },
            sdl.SDL_EVENT_KEY_DOWN, sdl.SDL_EVENT_KEY_UP, sdl.SDL_EVENT_WINDOW_CLOSE_REQUESTED => return,
            else => {},
        }
        self.observed[self.observed_total % observed_capacity] = observed;
        self.observed_total += 1;
        if (self.frame >= settle_frames) self.observed_during_script += 1;
    }

    /// After each frame of a step: ImGui's pointer, when it moved since the
    /// step's last sample.
    fn notePointer(self: *Script) void {
        var state: imgui.c.BkImguiPointerState = undefined;
        imgui.c.bk_imgui_backend_pointer_state(&state);
        if (self.pointer_trail_len != 0) {
            const last = self.pointer_trail[self.pointer_trail_len - 1];
            if (last.x == state.mouse_x and last.y == state.mouse_y) return;
        }
        if (self.pointer_trail_len == pointer_trail_capacity) return;
        self.pointer_trail[self.pointer_trail_len] = .{ .frame = self.frame, .x = state.mouse_x, .y = state.mouse_y };
        self.pointer_trail_len += 1;
    }

    fn pushMotion(self: *Script, pos: Pos, left_held: bool) bool {
        return self.pushMotionAt(self.screen(pos), left_held);
    }

    fn pushMotionAt(self: *Script, point: [2]f32, left_held: bool) bool {
        var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
        event.motion.type = sdl.SDL_EVENT_MOUSE_MOTION;
        event.motion.windowID = self.window_id;
        event.motion.which = smoke_mouse_id;
        event.motion.state = if (left_held) sdl.SDL_BUTTON_LMASK else 0;
        event.motion.x = point[0];
        event.motion.y = point[1];
        self.last_pointer = point;
        return self.push(&event);
    }

    fn pushButton(self: *Script, pos: Pos, down: bool) bool {
        const point = self.screen(pos);
        var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
        event.button.type = if (down) sdl.SDL_EVENT_MOUSE_BUTTON_DOWN else sdl.SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = self.window_id;
        event.button.which = smoke_mouse_id;
        event.button.button = sdl.SDL_BUTTON_LEFT;
        event.button.down = down;
        event.button.clicks = 1;
        event.button.x = point[0];
        event.button.y = point[1];
        return self.push(&event);
    }

    fn pushKey(self: *Script, key: Key, down: bool) bool {
        var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
        event.key.type = if (down) sdl.SDL_EVENT_KEY_DOWN else sdl.SDL_EVENT_KEY_UP;
        event.key.windowID = self.window_id;
        event.key.key = key.key;
        event.key.scancode = key.scancode;
        event.key.mod = key.mod;
        event.key.down = down;
        return self.push(&event);
    }

    fn check(self: *Script, step: Step) bool {
        const editor = self.editor;
        const objects = editor.document.objects.items.len;
        // The due autosave says where it wrote on the status bar; that line
        // is its own check's to read.
        if (step.expect != .foreign_autosaved_to_recovery and self.view.statusLine().len != 0) return self.stepFail(step, "{s}", .{self.view.statusLine()});
        switch (step.expect) {
            .tool_brush => if (self.view.tool != .brush) return self.stepFail(step, "the tool is {s}", .{@tagName(self.view.tool)}),
            .tool_place => if (self.view.tool != .place) return self.stepFail(step, "the tool is {s}", .{@tagName(self.view.tool)}),
            .tool_select => {
                if (self.view.tool != .select) return self.stepFail(step, "the tool is {s}", .{@tagName(self.view.tool)});
                if (editor.selection != self.placed) return self.stepFail(step, "the selection is {?d}, want the new object {d}", .{ editor.selection, self.placed });
            },
            .painted => {
                if (editor.history.undo_stack.items.len != 1) return self.stepFail(step, "{d} edits recorded, want the stroke as one", .{editor.history.undo_stack.items.len});
                for ([_][2]i32{ self.cell_a, self.cell_b }, [_]u8{ self.tile_a, self.tile_b }) |cell, was| {
                    const now = self.engineTile(cell) orelse return self.stepFail(step, "no tile at {any}", .{cell});
                    if (now == was) return self.stepFail(step, "cell {any} still holds tile {d} after painting {d}", .{ cell, now, self.view.brush.tile });
                }
                if (!self.engineAgrees(step)) return false;
            },
            .placed => {
                if (objects != self.original_objects + 1) return self.stepFail(step, "{d} objects, the map had {d}", .{ objects, self.original_objects });
                const last = editor.document.objects.items[objects - 1];
                if (editor.selection != last.link_id) return self.stepFail(step, "the new object {d} is not selected", .{last.link_id});
                self.placed = last.link_id;
                self.placed_pose = .{ .x = last.x, .y = last.y, .dir = last.dir, .player = last.player };
            },
            .nothing_selected => if (editor.selection) |link_id| return self.stepFail(step, "object {d} is selected", .{link_id}),
            .target_selected => if (editor.selection != self.target) return self.stepFail(step, "the selection is {?d}, want {d}", .{ editor.selection, self.target }),
            .placed_selected => if (editor.selection != self.placed) return self.stepFail(step, "the selection is {?d}, want the placed object {d}", .{ editor.selection, self.placed }),
            .rotated => {
                const object = editor.document.find(self.placed) orelse return self.stepFail(step, "the placed object {d} is gone", .{self.placed});
                const want = @mod(self.placed_pose.dir + core.tools.rotate_step, 65536);
                if (object.dir != want) return self.stepFail(step, "direction {d}, want {d}", .{ object.dir, want });
                if (!self.engineAgrees(step)) return false;
            },
            .moved => {
                const object = editor.document.find(self.placed) orelse return self.stepFail(step, "the placed object {d} is gone", .{self.placed});
                if (object.x == self.placed_pose.x and object.y == self.placed_pose.y)
                    return self.stepFail(step, "the placed object is still at {d},{d}", .{ object.x, object.y });
                if (editor.selection != self.placed) return self.stepFail(step, "the selection is {?d}, want {d}", .{ editor.selection, self.placed });
                // It went where the cursor went: a click at the drag's end
                // finds it.
                const there = self.resolveAt(drag_to) orelse return self.stepFail(step, "drag_to is off the terrain", .{});
                if (there.object != self.placed) return self.stepFail(step, "a click at the drag's end finds {?d}, want the placed object {d}", .{ there.object, self.placed });
                if (!self.engineAgrees(step)) return false;
            },
            .deleted => {
                if (editor.document.find(self.placed) != null) return self.stepFail(step, "the placed object {d} is still there", .{self.placed});
                if (objects != self.original_objects) return self.stepFail(step, "{d} objects, want {d} (one placed and deleted)", .{ objects, self.original_objects });
                if (editor.selection != null) return self.stepFail(step, "the deleted object is still selected", .{});
                if (!self.engineAgrees(step)) return false;
            },
            .all_undone => {
                if (editor.history.canUndo()) return self.stepFail(step, "{d} edits are left to undo", .{editor.history.undo_stack.items.len});
                if (objects != self.original_objects) return self.stepFail(step, "{d} objects, the map had {d}", .{ objects, self.original_objects });
                if (editor.document.find(self.placed) != null) return self.stepFail(step, "the placed object is still there", .{});
                const object = editor.document.find(self.target) orelse return self.stepFail(step, "object {d} did not come back", .{self.target});
                const pose: Pose = .{ .x = object.x, .y = object.y, .dir = object.dir, .player = object.player };
                if (!std.meta.eql(pose, self.target_pose)) return self.stepFail(step, "object {d} came back as {any}, the map had {any}", .{ self.target, pose, self.target_pose });
                if (self.engineTile(self.cell_a) != self.tile_a or self.engineTile(self.cell_b) != self.tile_b)
                    return self.stepFail(step, "the painted cells were not put back", .{});
                if (editor.dirty()) return self.stepFail(step, "the document is still dirty", .{});
                if (!self.engineAgrees(step)) return false;
            },
            .saved => {
                if (editor.status().len != 0) return self.stepFail(step, "{s}", .{editor.status()});
                if (!std.mem.eql(u8, panels_logic.baseName(editor.document.path.items), std.fs.path.basename(self.save_path)))
                    return self.stepFail(step, "the document's path is {s}", .{editor.document.path.items});
                if (editor.dirty()) return self.stepFail(step, "the document is dirty after saving", .{});
                var temp_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
                if (tempSiblingPath(&temp_buffer, self.save_path)) |temp_path| {
                    const left_behind = blk: {
                        _ = std.Io.Dir.cwd().statFile(self.state.io, temp_path, .{}) catch break :blk false;
                        break :blk true;
                    };
                    if (left_behind) return self.stepFail(step, "the temporary save file {s} was left behind", .{temp_path});
                }
            },
            .reopened => {
                if (editor.status().len != 0) return self.stepFail(step, "{s}", .{editor.status()});
                if (objects != self.original_objects) return self.stepFail(step, "{d} objects, the map had {d}", .{ objects, self.original_objects });
                if (editor.document.find(self.target) == null) return self.stepFail(step, "object {d} is not in the saved map", .{self.target});
            },
            .panned => {
                const wheel = step.inputs[0].wheel;
                // Divided the same way view.zig's own handleWheel divides a
                // screen pan by scale: at zoom the map must follow the
                // fingers 1:1 on screen, not by panScreen's zoom-1 gain.
                const scale = if (self.view.scale > 0) self.view.scale else 1;
                var want: view_math.Camera = .{ .x = self.camera_before[0], .y = self.camera_before[1] };
                for (0..wheel.count) |_| {
                    const pan = view_math.wheelPan(.{ .x = wheel.x, .y = wheel.y, .flipped = wheel.flipped }, view_math.wheel_sensitivity);
                    want.panScreen(pan.right_px / scale, pan.up_px / scale, self.view.map);
                }
                const moved = @abs(self.view.camera_x - self.camera_before[0]) + @abs(self.view.camera_y - self.camera_before[1]);
                if (moved < 1) return self.stepFail(step, "the camera did not move from {any}", .{self.camera_before});
                if (@abs(self.view.camera_x - want.x) > 0.5 or @abs(self.view.camera_y - want.y) > 0.5)
                    return self.stepFail(step, "the camera went from {any} to {d},{d}, want {d},{d}", .{ self.camera_before, self.view.camera_x, self.view.camera_y, want.x, want.y });
                // The drawn frame moved with the view's camera: the world
                // point at the screen's centre moved by the same amount.
                const before = self.centre_before orelse return self.stepFail(step, "the centre was off the terrain", .{});
                const after = self.resolveAt(.{ .dx = 0, .dy = 0 }) orelse return self.stepFail(step, "the centre is off the terrain", .{});
                const drawn_x = after.world_x - before.world_x;
                const drawn_y = after.world_y - before.world_y;
                if (@abs(drawn_x - (want.x - self.camera_before[0])) > 4 or @abs(drawn_y - (want.y - self.camera_before[1])) > 4)
                    return self.stepFail(step, "the drawn frame's centre moved by {d},{d}, the camera by {d},{d}", .{ drawn_x, drawn_y, want.x - self.camera_before[0], want.y - self.camera_before[1] });
            },
            .panel_has_pointer => {
                if (!view_mod.captureFlags().mouse) return self.stepFail(step, "ImGui does not want the mouse after {d} frames", .{max_wait_frames});
                if (self.view.camera_x != self.camera_before[0] or self.view.camera_y != self.camera_before[1])
                    return self.stepFail(step, "the camera moved from {any} to {d},{d}", .{ self.camera_before, self.view.camera_x, self.view.camera_y });
            },
            .camera_unchanged => {
                if (self.view.camera_x != self.camera_before[0] or self.view.camera_y != self.camera_before[1])
                    return self.stepFail(step, "the camera moved from {any} to {d},{d}", .{ self.camera_before, self.view.camera_x, self.view.camera_y });
            },
            .unsaved_prompt_open => {
                if (!self.state.actions.prompt.isAsking()) return self.stepFail(step, "the prompt is not asking", .{});
                if (!editor.dirty()) return self.stepFail(step, "the document is not dirty while the prompt asks", .{});
            },
            .prompt_cancelled => {
                if (self.state.actions.prompt.isAsking()) return self.stepFail(step, "the prompt is still asking after Cancel", .{});
                if (!editor.dirty()) return self.stepFail(step, "the document is not dirty after Cancel", .{});
                if (!std.mem.eql(u8, panels_logic.baseName(editor.document.path.items), std.fs.path.basename(self.save_path)))
                    return self.stepFail(step, "the path changed to {s}", .{editor.document.path.items});
            },
            .save_became_save_as => {
                if (!self.state.actions.dialog.waiting() or self.state.actions.dialog.kind != .save_as)
                    return self.stepFail(step, "Save did not redirect to Save As on the shipped map", .{});
                if (self.state.os_dialogs_opened != 0)
                    return self.stepFail(step, "Save As opened {d} real file dialog(s); the script answers the slot itself", .{self.state.os_dialogs_opened});
                // Cancels it and drains the result, freeing the slot for the
                // next step's Save As. No OS dialog is up behind the slot
                // (main.zig's smokeRun turns State.os_dialogs off), so
                // nothing can answer it later and touch the shipped file.
                self.state.actions.dialog.deliver(null);
                _ = self.state.actions.dialog.take();
            },
            .zoomed_at_pointer => {
                if (self.view.zoom_steps <= self.zoom_steps_before)
                    return self.stepFail(step, "zoom_steps is {d}, want more than {d}", .{ self.view.zoom_steps, self.zoom_steps_before });
                const wheel = step.inputs[0].wheel;
                const before = self.wheel_point_before orelse return self.stepFail(step, "the zoom point was off the terrain before zooming", .{});
                const after = self.resolveAt(wheel.at) orelse return self.stepFail(step, "the zoom point is off the terrain after zooming", .{});
                const moved = @abs(after.world_x - before.world_x) + @abs(after.world_y - before.world_y);
                if (moved > 2) return self.stepFail(step, "the zoom point moved by {d} world units (D-14)", .{moved});
            },
            .view_reset => {
                if (self.view.zoom_steps != 0) return self.stepFail(step, "zoom_steps is {d}, want 0 (D-13)", .{self.view.zoom_steps});
            },
            .sound_added => {
                var count: usize = 0;
                var none: [0]core.bridge.SoundRecord = .{};
                _ = editor.bridge.sounds(&none, &count);
                if (count != self.original_sounds + 1) return self.stepFail(step, "{d} sounds, the map had {d}", .{ count, self.original_sounds });
                if (editor.history.undo_stack.items.len != 1) return self.stepFail(step, "{d} edits recorded, want the add as one", .{editor.history.undo_stack.items.len});
                if (self.state.selected_sound == null) return self.stepFail(step, "the added sound is not selected", .{});
            },
            .sound_removed => {
                var count: usize = 0;
                var none: [0]core.bridge.SoundRecord = .{};
                _ = editor.bridge.sounds(&none, &count);
                if (count != self.original_sounds) return self.stepFail(step, "{d} sounds, want the original {d} back", .{ count, self.original_sounds });
                if (editor.history.canUndo()) return self.stepFail(step, "{d} edits are left to undo", .{editor.history.undo_stack.items.len});
            },
            .foreign_read_only => {
                if (editor.status().len != 0) return self.stepFail(step, "{s}", .{editor.status()});
                var os_buffer: [core.files.max_path]u8 = undefined;
                const doc_os = core.files.osPathFromEngine(&os_buffer, editor.document.path.items) orelse return self.stepFail(step, "the document path does not fit", .{});
                if (!std.mem.eql(u8, doc_os, self.foreign_path.slice()))
                    return self.stepFail(step, "the document is {s}, want {s}", .{ doc_os, self.foreign_path.slice() });
                if (!panels.documentIsShipped(self.state)) return self.stepFail(step, "{s} is not read-only", .{self.foreign_path.slice()});
                if (!panels.documentNeedsSaveAs(self.state)) return self.stepFail(step, "Save on {s} would not be Save As", .{self.foreign_path.slice()});
                if (!self.foreignUntouched(step)) return false;
            },
            .foreign_save_became_save_as => {
                // The title follows a frame behind the open (it is drawn
                // before `act` opens the map), so it is read here.
                const title = self.state.title[0..self.state.title_len];
                if (std.mem.indexOf(u8, title, "(read-only)") == null) return self.stepFail(step, "the title is \"{s}\"", .{title});
                if (!self.state.actions.dialog.waiting() or self.state.actions.dialog.kind != .save_as)
                    return self.stepFail(step, "Save did not redirect to Save As on another installation's map", .{});
                if (self.state.os_dialogs_opened != 0)
                    return self.stepFail(step, "Save As opened {d} real file dialog(s); the script answers the slot itself", .{self.state.os_dialogs_opened});
                self.state.actions.dialog.deliver(null);
                _ = self.state.actions.dialog.take();
                if (!editor.dirty()) return self.stepFail(step, "the map is clean - something saved it", .{});
                if (!self.foreignUntouched(step)) return false;
            },
            .foreign_autosaved_to_recovery => {
                const status = self.view.statusLine();
                if (!std.mem.eql(u8, status, "recovery copy written")) return self.stepFail(step, "the autosave said \"{s}\"", .{status});
                self.view.clearStatus();
                const active = self.state.recovery_active orelse return self.stepFail(step, "no recovery copy is active", .{});
                if (std.mem.indexOf(u8, active.slice(), smoke_user_root) == null)
                    return self.stepFail(step, "the recovery copy went to {s}, outside the smoke's own user root", .{active.slice()});
                _ = std.Io.Dir.cwd().statFile(self.state.io, active.slice(), .{}) catch
                    return self.stepFail(step, "the recovery copy {s} is not there", .{active.slice()});
                self.recovery_path = active;
                if (!editor.dirty()) return self.stepFail(step, "the map is clean - the autosave saved it in place", .{});
                if (!self.foreignUntouched(step)) return false;
            },
            .foreign_save_refused => {
                const err = self.forced_save_error orelse return self.stepFail(step, "Editor.save wrote into {s}", .{self.foreign_path.slice()});
                if (err != error.Refused) return self.stepFail(step, "Editor.save failed with {s}, want Refused", .{@errorName(err)});
                if (std.mem.indexOf(u8, editor.status(), "read-only") == null) return self.stepFail(step, "the status is \"{s}\"", .{editor.status()});
                if (!editor.dirty()) return self.stepFail(step, "the map is clean after a refused save", .{});
                if (!self.foreignUntouched(step)) return false;
            },
            .mod_switch_no_op => {
                if (self.state.actions.prompt.isAsking()) return self.stepFail(step, "choosing the active mod asked about unsaved changes", .{});
                if (!self.mapStillOpen(step)) return false;
                if (!self.modIs(step, null)) return false;
                if (self.state.catalogue_generation != self.catalogue_generation_before)
                    return self.stepFail(step, "the palette was re-read for a switch that did not happen", .{});
            },
            .mod_switch_asked => {
                if (!self.state.actions.prompt.isAsking()) return self.stepFail(step, "the prompt is not asking", .{});
                if (!self.mapStillOpen(step)) return false;
                if (!self.modIs(step, null)) return false;
            },
            .mod_switch_save_as_waiting => {
                if (!self.state.actions.dialog.waiting() or self.state.actions.dialog.kind != .save_as)
                    return self.stepFail(step, "Save on the read-only map did not become Save As", .{});
                if (self.state.os_dialogs_opened != 0)
                    return self.stepFail(step, "Save As opened {d} real file dialog(s); the script answers the slot itself", .{self.state.os_dialogs_opened});
                if (!self.mapStillOpen(step)) return false;
                if (!self.modIs(step, null)) return false;
                if (!self.foreignUntouched(step)) return false;
            },
            .mod_switch_cancelled => {
                if (self.state.actions.prompt.isAsking()) return self.stepFail(step, "the prompt is still asking", .{});
                if (self.state.actions.prompt.phase != .idle) return self.stepFail(step, "the prompt is still {t}", .{self.state.actions.prompt.phase});
                if (self.state.actions.dialog.waiting()) return self.stepFail(step, "a dialog is still waiting", .{});
                if (!self.mapStillOpen(step)) return false;
                if (!self.modIs(step, null)) return false;
                if (self.state.catalogue_generation != self.catalogue_generation_before)
                    return self.stepFail(step, "the palette was re-read for a cancelled switch", .{});
                if (!self.foreignUntouched(step)) return false;
            },
            .mod_switched_map_closed => {
                if (self.state.actions.prompt.isAsking()) return self.stepFail(step, "the prompt is still asking", .{});
                if (!self.noMapOpen(step)) return false;
                if (!self.modIs(step, smoke_mod)) return false;
                if (!self.paletteReRead(step)) return false;
                if (!self.openFolderFollows(step, smoke_mod)) return false;
                if (self.state.autosave.dirty_since_ms != null) return self.stepFail(step, "autosave still counts a dirty map", .{});
                if (self.state.recovery_active != null) return self.stepFail(step, "a recovery copy is still active", .{});
                const recovery = self.recovery_path.slice();
                if (recovery.len == 0) return self.stepFail(step, "no recovery copy was recorded to check", .{});
                if (std.Io.Dir.cwd().statFile(self.state.io, recovery, .{})) |_| {
                    return self.stepFail(step, "the closed map's recovery copy {s} is still there", .{recovery});
                } else |_| {}
                // Don't save: the file on disk is what it always was.
                if (!self.foreignUntouched(step)) return false;
            },
            .mod_switched_without_map => {
                if (self.state.actions.prompt.isAsking()) return self.stepFail(step, "a switch with no map open asked", .{});
                if (!self.noMapOpen(step)) return false;
                if (!self.modIs(step, null)) return false;
                if (!self.paletteReRead(step)) return false;
                if (!self.openFolderFollows(step, null)) return false;
            },
        }
        return true;
    }

    /// The map the step started with is still the one open, and still dirty.
    fn mapStillOpen(self: *Script, step: Step) bool {
        const path = self.editor.document.path.items;
        if (path.len == 0) return self.stepFail(step, "the map was closed", .{});
        if (!std.mem.eql(u8, path, self.path_before.slice())) return self.stepFail(step, "the document is {s}, was {s}", .{ path, self.path_before.slice() });
        if (!self.editor.dirty()) return self.stepFail(step, "the map is no longer dirty", .{});
        return true;
    }

    /// The editor has no document, and nothing that followed the old one
    /// is left over: history, selection, the view's map, the per-map panels.
    fn noMapOpen(self: *Script, step: Step) bool {
        const editor = self.editor;
        if (panels.mapIsOpen(editor)) return self.stepFail(step, "{s} is still open", .{editor.document.path.items});
        if (editor.document.objects.items.len != 0) return self.stepFail(step, "{d} objects are left in the document", .{editor.document.objects.items.len});
        if (editor.dirty()) return self.stepFail(step, "the empty document is dirty", .{});
        if (editor.history.canUndo() or editor.history.canRedo()) return self.stepFail(step, "the undo history was kept", .{});
        if (editor.selection != null) return self.stepFail(step, "object {?d} is still selected", .{editor.selection});
        if (self.view.current_path.items.len != 0) return self.stepFail(step, "the view still shows {s}", .{self.view.current_path.items});
        if (self.view.map.width_tiles != 0 or self.view.map.height_tiles != 0) return self.stepFail(step, "the view still has a {d}x{d} map", .{ self.view.map.width_tiles, self.view.map.height_tiles });
        if (self.state.tile_count != 0) return self.stepFail(step, "the brush still offers {d} tiles", .{self.state.tile_count});
        if (self.state.sounds.len != 0) return self.stepFail(step, "the Sounds panel still lists {d} sounds", .{self.state.sounds.len});
        if (self.state.unknown_objects_total != 0) return self.stepFail(step, "{d} unknown objects are still reported", .{self.state.unknown_objects_total});
        return true;
    }

    /// The panels' mod and the bridge's agree, and are `want` (null: None).
    fn modIs(self: *Script, step: Step, want: ?[]const u8) bool {
        const panels_mod = self.state.modFolder();
        // `|*m|`: a slice of a by-value capture would dangle once the `if` ends.
        var bridge_mod = self.real.activeMod();
        const bridge_folder: ?[]const u8 = if (bridge_mod) |*m| std.mem.sliceTo(&m.folder, 0) else null;
        const want_text = want orelse "None";
        if (!std.mem.eql(u8, panels_mod orelse "None", want_text))
            return self.stepFail(step, "the panels' mod is {s}, want {s}", .{ panels_mod orelse "None", want_text });
        const bridge_text = if (bridge_folder) |f| (if (f.len == 0) "None" else f) else "None";
        if (!std.mem.eql(u8, bridge_text, want_text))
            return self.stepFail(step, "the bridge's mod is {s}, want {s}", .{ bridge_text, want_text });
        return true;
    }

    /// The palette was read again this step, and holds what the bridge's
    /// object database (the new mod's) holds now.
    fn paletteReRead(self: *Script, step: Step) bool {
        if (self.state.catalogue_generation == self.catalogue_generation_before)
            return self.stepFail(step, "the palette was not re-read", .{});
        if (self.state.catalogue.len == 0) return self.stepFail(step, "the palette is empty", .{});
        const fresh = self.real.catalogue(self.state.allocator) catch return self.stepFail(step, "the bridge's catalogue did not read", .{});
        defer self.state.allocator.free(fresh);
        if (fresh.len != self.state.catalogue.len)
            return self.stepFail(step, "the palette has {d} entries, the new mod's database {d}", .{ self.state.catalogue.len, fresh.len });
        return true;
    }

    /// The Open dialog would start in the user maps folder of `want`'s mod
    /// (null: None) - `<user root>mods/<want>/maps`, or `<user root>maps`.
    fn openFolderFollows(self: *Script, step: Step, want: ?[]const u8) bool {
        var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const folder = panels.dialogFolder(self.state, &buffer) orelse return self.stepFail(step, "the Open dialog's folder does not fit", .{});
        const sep = std.fs.path.sep_str;
        var tail_buffer: [128]u8 = undefined;
        const tail = if (want) |mod|
            std.fmt.bufPrint(&tail_buffer, sep ++ "mods" ++ sep ++ "{s}" ++ sep ++ "maps", .{mod}) catch unreachable
        else
            smoke_user_root ++ sep ++ "maps";
        if (!std.mem.endsWith(u8, folder, tail)) return self.stepFail(step, "the Open dialog would start in {s}, want ...{s}", .{ folder, tail });
        return true;
    }

    /// `open_foreign`'s setup: `<dir of save_path>/foreign_tree/Data` with a
    /// `consts.xml` marker (core/shipped.zig's `isDataRootMarker`, as every
    /// shipped Data has one) and the smoke's own map copied under it, rebuilt
    /// from nothing each run. Returns the copy's absolute OS path (in
    /// `foreign_path`), or null after a FAIL line.
    fn buildForeignTree(self: *Script) ?[]const u8 {
        const io = self.state.io;
        const cwd = std.Io.Dir.cwd();
        const out_dir = std.fs.path.dirname(self.save_path) orelse ".";
        var root_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
        const root = std.fmt.bufPrint(&root_buffer, "{s}{c}{s}", .{ out_dir, std.fs.path.sep, foreign_tree }) catch {
            _ = self.fail("the second tree's path is too long", .{});
            return null;
        };
        cwd.deleteTree(io, root) catch {};
        var path_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
        const maps = std.fmt.bufPrint(&path_buffer, "{s}{c}Data{c}Maps{c}Multiplayer", .{ root, std.fs.path.sep, std.fs.path.sep, std.fs.path.sep }) catch unreachable;
        cwd.createDirPath(io, maps) catch |err| {
            _ = self.fail("{s} would not be created: {s}", .{ maps, @errorName(err) });
            return null;
        };
        var marker_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
        const marker = std.fmt.bufPrint(&marker_buffer, "{s}{c}Data{c}consts.xml", .{ root, std.fs.path.sep, std.fs.path.sep }) catch unreachable;
        cwd.writeFile(io, .{ .sub_path = marker, .data = "<?xml version=\"1.0\"?>\n<base/>\n" }) catch |err| {
            _ = self.fail("{s} would not be written: {s}", .{ marker, @errorName(err) });
            return null;
        };
        var map_buffer: [panels_logic.PathSlot.max_path]u8 = undefined;
        const map = std.fmt.bufPrint(&map_buffer, "{s}{c}{s}", .{ root, std.fs.path.sep, foreign_map }) catch unreachable;
        // The staged installation's own copy (the smoke runs in the stage
        // root), read and written - never touched.
        std.Io.Dir.copyFile(cwd, foreign_map, cwd, map, io, .{}) catch |err| {
            _ = self.fail("{s} would not be copied to {s}: {s}", .{ foreign_map, map, @errorName(err) });
            return null;
        };
        self.foreign_path.set(map);
        self.foreign_facts = self.readFacts(map) orelse {
            _ = self.fail("{s} would not read back", .{map});
            return null;
        };
        return self.foreign_path.slice();
    }

    fn readFacts(self: *Script, path: []const u8) ?FileFacts {
        const io = self.state.io;
        const stat = std.Io.Dir.cwd().statFile(io, path, .{}) catch return null;
        const bytes = std.Io.Dir.cwd().readFileAlloc(io, path, self.state.allocator, .limited(64 << 20)) catch return null;
        defer self.state.allocator.free(bytes);
        return .{ .size = stat.size, .inode = stat.inode, .mtime = stat.mtime.nanoseconds, .hash = std.hash.Wyhash.hash(0, bytes) };
    }

    /// The second tree's map is exactly what `open_foreign` copied - bytes,
    /// size, inode (a safe save's rename-over would change it) and mtime -
    /// and nothing was written beside it: no `.bak`, no `.~save` temp.
    fn foreignUntouched(self: *Script, step: Step) bool {
        const path = self.foreign_path.slice();
        const before = self.foreign_facts orelse return self.stepFail(step, "the second tree was never built", .{});
        const now = self.readFacts(path) orelse return self.stepFail(step, "{s} is gone", .{path});
        if (!std.meta.eql(before, now)) return self.stepFail(step, "{s} changed: was {any}, now {any}", .{ path, before, now });
        var bak_buffer: [panels_logic.PathSlot.max_path + 8]u8 = undefined;
        const bak = std.fmt.bufPrint(&bak_buffer, "{s}.bak", .{path}) catch return self.stepFail(step, "the .bak path does not fit", .{});
        if (std.Io.Dir.cwd().statFile(self.state.io, bak, .{})) |_| {
            return self.stepFail(step, "{s} was written", .{bak});
        } else |_| {}
        var temp_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        if (tempSiblingPath(&temp_buffer, path)) |temp| {
            if (std.Io.Dir.cwd().statFile(self.state.io, temp, .{})) |_| {
                return self.stepFail(step, "{s} was written", .{temp});
            } else |_| {}
        }
        return true;
    }

    fn engineAgrees(self: *Script, step: Step) bool {
        if (self.real.engineMatches() == .ok) return true;
        return self.stepFail(step, "the engine disagrees with the map: {s}", .{std.mem.span(c.BkEditorLastMessage(self.real.session))});
    }

    fn stepFail(self: *Script, step: Step, comptime format: []const u8, args: anytype) bool {
        std.debug.print("map-editor: smoke FAIL: {s}: " ++ format ++ " (editor status: {s})\n", .{step.name} ++ args ++ .{self.editor.status()});
        self.printState();
        self.reported = true;
        return false;
    }

    /// A FAIL's second line: what SDL and ImGui each make of the window and
    /// the pointer, so a failure on a CI runner no one can watch says which
    /// of them lost the script's input, and to what.
    fn printState(self: *Script) void {
        var text_buffer: [4096]u8 = undefined;
        var writer: std.Io.Writer = .fixed(&text_buffer);
        self.writeState(&writer) catch {};
        std.debug.print("map-editor: smoke state: {s}\n", .{writer.buffered()});
    }

    fn writeState(self: *Script, w: *std.Io.Writer) std.Io.Writer.Error!void {
        const window = sdl.SDL_GetWindowFromID(self.window_id);
        var width: c_int = 0;
        var height: c_int = 0;
        var window_x: c_int = 0;
        var window_y: c_int = 0;
        var flags: sdl.SDL_WindowFlags = 0;
        if (window) |win| {
            _ = sdl.SDL_GetWindowSize(win, &width, &height);
            _ = sdl.SDL_GetWindowPosition(win, &window_x, &window_y);
            flags = sdl.SDL_GetWindowFlags(win);
        }
        try w.print("frame {d}, step waited {d}; script pointer {d:.0},{d:.0}; window {d}x{d} at {d},{d}", .{ self.frame, self.waited, self.last_pointer[0], self.last_pointer[1], width, height, window_x, window_y });
        if (self.real.screenSize()) |size| try w.print(" (engine {d}x{d})", .{ size[0], size[1] });
        try w.print(", hidden {} input focus {} mouse focus {} occluded {}", .{ flags & sdl.SDL_WINDOW_HIDDEN != 0, flags & sdl.SDL_WINDOW_INPUT_FOCUS != 0, flags & sdl.SDL_WINDOW_MOUSE_FOCUS != 0, flags & sdl.SDL_WINDOW_OCCLUDED != 0 });
        var global_x: f32 = 0;
        var global_y: f32 = 0;
        const global_buttons = sdl.SDL_GetGlobalMouseState(&global_x, &global_y);
        try w.print("; SDL keyboard focus {s}, mouse focus {s}, global mouse {d:.0},{d:.0} buttons {d}", .{ focusName(sdl.SDL_GetKeyboardFocus(), window), focusName(sdl.SDL_GetMouseFocus(), window), global_x, global_y, global_buttons });

        var state: imgui.c.BkImguiPointerState = undefined;
        imgui.c.bk_imgui_backend_pointer_state(&state);
        try w.writeAll("; ImGui mouse ");
        try writePos(w, state.mouse_x, state.mouse_y);
        try w.print(" display {d:.0}x{d:.0} wants mouse {} hovered '{s}' (before clear '{s}', window at pointer '{s}')", .{ state.display_w, state.display_h, state.want_capture_mouse, std.mem.sliceTo(&state.hovered_window, 0), std.mem.sliceTo(&state.hovered_before_clear, 0), std.mem.sliceTo(&state.window_at_pointer, 0) });
        try w.print(" down {any} owned {any} popups {d} capture override {d} focus lost {}", .{ state.mouse_down, state.mouse_down_owned, state.open_popups, state.want_capture_mouse_next_frame, state.app_focus_lost });
        try w.print("; ImGui queue {d} (pos {d} button {d} wheel {d} key {d} focus {d})", .{ state.queued_events, state.queued_mouse_pos, state.queued_mouse_button, state.queued_mouse_wheel, state.queued_key, state.queued_focus });
        if (state.queued_mouse_pos_valid) {
            try w.writeAll(" heading to ");
            try writePos(w, state.queued_mouse_x, state.queued_mouse_y);
        }
        try w.writeAll("; ImGui pointer this step:");
        for (self.pointer_trail[0..self.pointer_trail_len]) |sample| {
            try w.print(" {d}:", .{sample.frame});
            try writePos(w, sample.x, sample.y);
        }
        try w.print("; mouse events pushed {d} polled {d}", .{ self.pushed_mouse, self.polled_mouse });
        try self.writeObserved(w);
    }

    /// The OS's own events, the last `observed_capacity` of them, each with
    /// the frame it was polled in (and a mouse event's position).
    fn writeObserved(self: *const Script, w: *std.Io.Writer) std.Io.Writer.Error!void {
        try w.print("; OS events ({d}):", .{self.observed_total});
        const kept = @min(self.observed_total, observed_capacity);
        for (0..kept) |n| {
            const event = self.observed[(self.observed_total - kept + n) % observed_capacity];
            if (eventName(event.type)) |name| try w.print(" {s}@{d}", .{ name, event.frame }) else try w.print(" 0x{x}@{d}", .{ event.type, event.frame });
            if (isMouseEvent(event.type)) try w.print("({d:.0},{d:.0})", .{ event.x, event.y });
        }
    }

    /// On a PASS: says so when the OS reached the window while the script
    /// ran. The window is not really hidden (GFXGPU's SetMode shows it), so
    /// a focus change or the real pointer can land on it mid-step - silent
    /// when nothing did, a warning of a race the steps only won this time
    /// when something did.
    pub fn printNote(self: *const Script) void {
        if (self.observed_during_script == 0) return;
        var text_buffer: [1024]u8 = undefined;
        var writer: std.Io.Writer = .fixed(&text_buffer);
        const window = sdl.SDL_GetWindowFromID(self.window_id);
        writer.print("{d} OS event(s) reached the window while the script ran (from frame {d}); SDL keyboard focus {s}, mouse focus {s}", .{ self.observed_during_script, settle_frames, focusName(sdl.SDL_GetKeyboardFocus(), window), focusName(sdl.SDL_GetMouseFocus(), window) }) catch {};
        self.writeObserved(&writer) catch {};
        std.debug.print("map-editor: smoke note: {s}\n", .{writer.buffered()});
    }

    fn fail(self: *Script, comptime format: []const u8, args: anytype) bool {
        std.debug.print("map-editor: smoke FAIL: " ++ format ++ "\n", args);
        self.reported = true;
        return false;
    }
};

/// main.zig's `run` drives either the fixed `--smoke` table (`Script`) or
/// BK_EDITOR_AUTO's parsed schedule (`AutoRunner`) - one loop, one driver
/// interface, so `run` itself does not need to know which is which.
pub const Driver = union(enum) {
    table: *Script,
    auto: *AutoRunner,

    pub fn beforeFrame(self: Driver) bool {
        return switch (self) {
            .table => |s| s.beforeFrame(),
            .auto => |a| a.beforeFrame(),
        };
    }

    pub fn afterFrame(self: Driver) bool {
        return switch (self) {
            .table => |s| s.afterFrame(),
            .auto => |a| a.afterFrame(),
        };
    }

    /// `Script` keeps the OS's own events for a FAIL's state line;
    /// `AutoRunner` asserts nothing about them, so there is nothing to keep.
    pub fn observe(self: Driver, event: *const sdl.SDL_Event) void {
        switch (self) {
            .table => |s| s.observe(event),
            .auto => {},
        }
    }
};

/// BK_EDITOR_AUTO's runner: delivers auto.zig's parsed schedule through the
/// same synthetic-event machinery `Script`'s fixed table uses (plan 5's own
/// note: "BK_EDITOR_AUTO generalises the table"). Unlike `Script`, nothing
/// here asserts a specific outcome - BK_EDITOR_AUTO is a general automation
/// tool, not a fixed regression script; "did the action succeed" is judged
/// from the editor's own status line (open/save/saveas) or the action's own
/// pass/fail rule (compare's tolerance, waitgame's exit code).
pub const AutoRunner = struct {
    editor: *Editor,
    view: *View,
    real: *RealBridge,
    state: *panels.State,
    io: std.Io,
    window_id: sdl.SDL_WindowID,
    schedule: []const auto_mod.Scheduled,
    /// BK_EDITOR_AUTO_DIR: where `shot=`/`compare=` read and write, created
    /// once here (best-effort; a failure to create it surfaces naturally the
    /// first time an action tries to use it).
    dir: []const u8,
    /// BK_EDITOR_AUTO_GAME's raw text (becomes the test game's own
    /// BK_AUTO_UI), if set - read once by the caller (main.zig), since it
    /// must outlive this runner exactly as `dir` and `schedule` do.
    game_env_text: ?[]const u8,

    centre_x: f32 = 0,
    centre_y: f32 = 0,
    /// Wherever the last press/drag/release/click left the pointer - what a
    /// `wheel` action (which carries no point of its own) sends its events
    /// at, the way a trackpad's wheel does not reposition anything either.
    last_point: [2]f32 = .{ 0, 0 },
    frame: u32 = 0,
    actions_run: usize = 0,
    failed: bool = false,
    done: bool = false,
    /// Set by an `open`/`save`/`saveas` action so `afterFrame` (once
    /// `panels.act` has actually run, later in the same iteration of
    /// main.zig's `run`) can check its outcome - checking in `beforeFrame`
    /// itself would be too early, since `act()` has not processed the
    /// request yet.
    pending_file_action: ?[]const u8 = null,
    /// `test`'s own extra-environment pair storage (panels.State.test_extra_env
    /// borrows a slice of this) - owned here so it outlives the `startTestGame`
    /// call that reads it.
    game_env_pairs: [2][2][]const u8 = undefined,

    /// After the map is open and State built, exactly like `Script.init`.
    pub fn init(
        editor: *Editor,
        view: *View,
        real: *RealBridge,
        state: *panels.State,
        window: *sdl.SDL_Window,
        io: std.Io,
        schedule: []const auto_mod.Scheduled,
        dir: []const u8,
        game_env_text: ?[]const u8,
    ) AutoRunner {
        std.Io.Dir.cwd().createDirPath(io, dir) catch {};
        return .{
            .editor = editor,
            .view = view,
            .real = real,
            .state = state,
            .io = io,
            .window_id = sdl.SDL_GetWindowID(window),
            .schedule = schedule,
            .dir = dir,
            .game_env_text = game_env_text,
        };
    }

    fn screen(self: *const AutoRunner, point: auto_mod.Point) [2]f32 {
        return if (point.from_centre) .{ self.centre_x + point.x, self.centre_y + point.y } else .{ point.x, point.y };
    }

    /// Before the frame's events are polled (main.zig's `run`): this frame's
    /// scheduled actions, in the schedule's own order. False when the loop
    /// should stop - `exit` (clean) or a failed action (`self.failed`).
    pub fn beforeFrame(self: *AutoRunner) bool {
        if (self.frame < settle_frames) return true;
        if (self.frame == settle_frames) {
            const size = self.real.screenSize() orelse return self.fail("no screen size", .{});
            self.centre_x = @as(f32, @floatFromInt(size[0])) / 2;
            self.centre_y = @as(f32, @floatFromInt(size[1])) / 2;
            self.last_point = .{ self.centre_x, self.centre_y };
        }
        for (self.schedule) |item| {
            if (item.frame != self.frame) continue;
            if (!self.run(item)) return false;
        }
        return true;
    }

    /// After the frame and the panels' file actions (main.zig's `run`):
    /// checks an `open`/`save`/`saveas` this frame delivered, once `act()`
    /// has had a chance to process it.
    pub fn afterFrame(self: *AutoRunner) bool {
        defer self.frame += 1;
        // A wheel action's SDL_SetModState was only meant for this frame's
        // own wheel processing, already done by the time afterFrame runs -
        // clearing it here is what lets it survive to be read at all
        // (Script.afterFrame's own comment says the same).
        sdl.SDL_SetModState(0);
        if (self.pending_file_action) |what| {
            self.pending_file_action = null;
            if (self.state.actions.prompt.isAsking())
                return self.fail("{s}: the unsaved-changes prompt is asking; BK_EDITOR_AUTO cannot answer it", .{what});
            const view_status = self.state.view.statusLine();
            const editor_status = self.state.editor.status();
            if (view_status.len != 0 or editor_status.len != 0)
                return self.fail("{s}: {s}{s}", .{ what, view_status, editor_status });
        }
        return !self.done and !self.failed;
    }

    fn run(self: *AutoRunner, item: auto_mod.Scheduled) bool {
        self.actions_run += 1;
        std.debug.print("map-editor: BK_EDITOR_AUTO: frame {d} action {s}\n", .{ self.frame, item.text });
        switch (item.action) {
            .key => |key| return self.runKey(key),
            .press => |point| return self.runPress(point),
            .drag => |point| return self.runDrag(point),
            .release => |point| return self.runRelease(point),
            .click => |point| return self.runClick(point),
            .wheel => |wheel| return self.runWheel(wheel),
            .open => |path| {
                self.state.actions.requestOpenPath(path);
                self.pending_file_action = "open";
                return true;
            },
            .save => {
                self.state.actions.save_requested = true;
                self.pending_file_action = "save";
                return true;
            },
            .saveas => |path| return self.runSaveAs(path),
            .test_in_game => return self.runTest(),
            .waitgame => |seconds| return self.runWaitgame(seconds),
            .shot => |name| return self.runShot(name),
            .compare => |compare| return self.runCompare(compare),
            .exit => {
                std.debug.print("map-editor: BK_EDITOR_AUTO: done ({d} actions)\n", .{self.actions_run});
                self.done = true;
                return false;
            },
        }
    }

    fn runKey(self: *AutoRunner, key: auto_mod.Key) bool {
        const mapped = keyFromName(key.name) orelse return self.fail("key={s}: unknown key name", .{key.name});
        var mod: sdl.SDL_Keymod = 0;
        if (key.mods.ctrl) mod |= sdl.SDL_KMOD_CTRL;
        if (key.mods.shift) mod |= sdl.SDL_KMOD_SHIFT;
        if (key.mods.alt) mod |= sdl.SDL_KMOD_ALT;
        if (key.mods.cmd) mod |= sdl.SDL_KMOD_GUI;
        return self.pushKey(mapped, mod, true) and self.pushKey(mapped, mod, false);
    }

    fn runPress(self: *AutoRunner, point: auto_mod.Point) bool {
        const p = self.screen(point);
        return self.pushMotion(p, false) and self.pushButton(p, true);
    }

    fn runDrag(self: *AutoRunner, point: auto_mod.Point) bool {
        return self.pushMotion(self.screen(point), true);
    }

    fn runRelease(self: *AutoRunner, point: auto_mod.Point) bool {
        const p = self.screen(point);
        return self.pushMotion(p, true) and self.pushButton(p, false);
    }

    /// Presses and releases at the point in one action - there is no
    /// in-between frame for a drag to happen, unlike press/drag*/release.
    fn runClick(self: *AutoRunner, point: auto_mod.Point) bool {
        const p = self.screen(point);
        return self.pushMotion(p, false) and self.pushButton(p, true) and self.pushButton(p, false);
    }

    fn runWheel(self: *AutoRunner, wheel: auto_mod.Wheel) bool {
        var mod: sdl.SDL_Keymod = 0;
        if (wheel.mods.ctrl) mod |= sdl.SDL_KMOD_CTRL;
        if (wheel.mods.shift) mod |= sdl.SDL_KMOD_SHIFT;
        if (wheel.mods.alt) mod |= sdl.SDL_KMOD_ALT;
        if (wheel.mods.cmd) mod |= sdl.SDL_KMOD_GUI;
        // Held through the rest of this frame (cleared in afterFrame):
        // pushed key events do not update SDL's own modifier state
        // (SDL_GetModState, which view.zig's handleWheel reads) - the same
        // trick Script.deliver's own wheel case uses.
        if (mod != 0) sdl.SDL_SetModState(mod);
        var i: u8 = 0;
        while (i < wheel.count) : (i += 1) {
            var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
            event.wheel.type = sdl.SDL_EVENT_MOUSE_WHEEL;
            event.wheel.windowID = self.window_id;
            event.wheel.which = smoke_mouse_id;
            event.wheel.x = wheel.dx;
            event.wheel.y = wheel.dy;
            event.wheel.direction = sdl.SDL_MOUSEWHEEL_NORMAL;
            event.wheel.mouse_x = self.last_point[0];
            event.wheel.mouse_y = self.last_point[1];
            if (!self.push(&event)) return false;
        }
        return true;
    }

    fn runSaveAs(self: *AutoRunner, path: []const u8) bool {
        if (!self.state.actions.dialog.request(.save_as)) return self.fail("saveas={s}: the dialog slot was busy", .{path});
        self.state.actions.dialog.deliver(path);
        self.pending_file_action = "saveas";
        return true;
    }

    /// BK_EDITOR_AUTO_GAME (if set) becomes the child game's own BK_AUTO_UI;
    /// BK_NO_HELP=1 always, so a fresh test profile's one-time help screens
    /// never block an unattended run (project memory: "New-profile harness
    /// popups").
    fn runTest(self: *AutoRunner) bool {
        if (!panels.mapIsOpen(self.editor)) return self.fail("test: no map is open", .{});
        if (self.game_env_text) |auto_ui| {
            self.game_env_pairs[0] = .{ "BK_AUTO_UI", auto_ui };
            self.game_env_pairs[1] = .{ "BK_NO_HELP", "1" };
            self.state.test_extra_env = self.game_env_pairs[0..2];
        } else {
            self.game_env_pairs[0] = .{ "BK_NO_HELP", "1" };
            self.state.test_extra_env = self.game_env_pairs[0..1];
        }
        panels.requestTestLaunch(self.state);
        if (self.state.test_game == null) return self.fail("test: the game did not start: {s}", .{self.state.view.statusLine()});
        return true;
    }

    /// Headless-only, like `testlaunch.Running.waitBlocking`'s own doc
    /// comment warns: an interactive frame loop must never block like this.
    /// BK_EDITOR_AUTO's own loop draws no further frames while this runs,
    /// which is fine here - nothing is watching a hidden window anyway.
    fn runWaitgame(self: *AutoRunner, seconds: u32) bool {
        if (self.state.test_game == null) return self.fail("waitgame: no test game is running", .{});
        const exit = self.state.test_game.?.waitBlocking(self.io, seconds * 1000) orelse {
            self.state.test_game.?.terminate(self.io);
            self.state.test_game = null;
            return self.fail("waitgame: the game did not exit within {d}s", .{seconds});
        };
        self.state.test_game = null;
        if ((exit.code orelse 1) != 0 or exit.signal != null)
            return self.fail("waitgame: the game exited code={?d} signal={?d}", .{ exit.code, exit.signal });
        return true;
    }

    fn runShot(self: *AutoRunner, name: []const u8) bool {
        var path_buffer: [std.Io.Dir.max_path_bytes:0]u8 = undefined;
        const path = std.fmt.bufPrintZ(&path_buffer, "{s}{c}{s}.tga", .{ self.dir, std.fs.path.sep, name }) catch
            return self.fail("shot={s}: the path is too long", .{name});
        if (c.BkEditorCaptureFrame(self.real.session, path.ptr) != c.BK_EDITOR_OK)
            return self.fail("shot={s}: the frame was not captured: {s}", .{ name, std.mem.span(c.BkEditorLastMessage(self.real.session)) });
        return true;
    }

    /// No reference yet: the shot becomes it (a first run seeds; a person
    /// refreshes it when the rendering changes on purpose - 03-12-PLAN.md's
    /// own wording). Otherwise: `auto_mod.compareTga` at the default channel
    /// tolerance, failing when the sizes differ or more than `percent`
    /// (default 1.0) of the pixels do.
    fn runCompare(self: *AutoRunner, compare: auto_mod.Compare) bool {
        var shot_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const shot_path = std.fmt.bufPrint(&shot_buffer, "{s}{c}{s}.tga", .{ self.dir, std.fs.path.sep, compare.name }) catch
            return self.fail("compare={s}: the path is too long", .{compare.name});
        var ref_dir_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const ref_dir = std.fmt.bufPrint(&ref_dir_buffer, "{s}{c}reference", .{ self.dir, std.fs.path.sep }) catch
            return self.fail("compare={s}: the reference directory's path is too long", .{compare.name});
        std.Io.Dir.cwd().createDirPath(self.io, ref_dir) catch {};
        var ref_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const ref_path = std.fmt.bufPrint(&ref_buffer, "{s}{c}{s}.tga", .{ ref_dir, std.fs.path.sep, compare.name }) catch
            return self.fail("compare={s}: the reference path is too long", .{compare.name});

        const gpa = self.editor.allocator;
        const shot_bytes = std.Io.Dir.cwd().readFileAlloc(self.io, shot_path, gpa, .limited(64 << 20)) catch |err|
            return self.fail("compare={s}: the shot did not read: {s}", .{ compare.name, @errorName(err) });
        defer gpa.free(shot_bytes);

        const ref_bytes = std.Io.Dir.cwd().readFileAlloc(self.io, ref_path, gpa, .limited(64 << 20)) catch |err| switch (err) {
            error.FileNotFound => {
                std.Io.Dir.cwd().writeFile(self.io, .{ .sub_path = ref_path, .data = shot_bytes }) catch |werr|
                    return self.fail("compare={s}: the reference did not write: {s}", .{ compare.name, @errorName(werr) });
                std.debug.print("map-editor: BK_EDITOR_AUTO: compare {s}: no reference; this shot is the reference now\n", .{compare.name});
                return true;
            },
            else => return self.fail("compare={s}: the reference did not read: {s}", .{ compare.name, @errorName(err) }),
        };
        defer gpa.free(ref_bytes);

        const shot_tga = auto_mod.Tga.parse(shot_bytes) catch |err|
            return self.fail("compare={s}: the shot is not an uncompressed 32-bit TGA: {s}", .{ compare.name, @errorName(err) });
        const ref_tga = auto_mod.Tga.parse(ref_bytes) catch |err|
            return self.fail("compare={s}: the reference is not an uncompressed 32-bit TGA: {s}", .{ compare.name, @errorName(err) });
        if (shot_tga.width != ref_tga.width or shot_tga.height != ref_tga.height)
            return self.fail("compare={s}: the shot is {d}x{d}, the reference {d}x{d}", .{ compare.name, shot_tga.width, shot_tga.height, ref_tga.width, ref_tga.height });
        const diff = auto_mod.compareTga(shot_tga, ref_tga, auto_mod.default_channel_tolerance);
        const fraction = diff.fraction() * 100.0;
        std.debug.print("map-editor: BK_EDITOR_AUTO: compare {s}: {d:.4}% of pixels differ\n", .{ compare.name, fraction });
        if (fraction > compare.percent)
            return self.fail("compare={s}: {d:.4}% of pixels differ, want at most {d:.2}%", .{ compare.name, fraction, compare.percent });
        return true;
    }

    fn push(self: *AutoRunner, event: *sdl.SDL_Event) bool {
        if (sdl.SDL_PushEvent(event)) return true;
        return self.fail("SDL_PushEvent: {s}", .{sdl.SDL_GetError()});
    }

    fn pushMotion(self: *AutoRunner, point: [2]f32, left_held: bool) bool {
        self.last_point = point;
        var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
        event.motion.type = sdl.SDL_EVENT_MOUSE_MOTION;
        event.motion.windowID = self.window_id;
        event.motion.which = smoke_mouse_id;
        event.motion.state = if (left_held) sdl.SDL_BUTTON_LMASK else 0;
        event.motion.x = point[0];
        event.motion.y = point[1];
        return self.push(&event);
    }

    fn pushButton(self: *AutoRunner, point: [2]f32, down: bool) bool {
        var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
        event.button.type = if (down) sdl.SDL_EVENT_MOUSE_BUTTON_DOWN else sdl.SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.windowID = self.window_id;
        event.button.which = smoke_mouse_id;
        event.button.button = sdl.SDL_BUTTON_LEFT;
        event.button.down = down;
        event.button.clicks = 1;
        event.button.x = point[0];
        event.button.y = point[1];
        return self.push(&event);
    }

    fn pushKey(self: *AutoRunner, key: NamedKey, mod: sdl.SDL_Keymod, down: bool) bool {
        var event: sdl.SDL_Event = std.mem.zeroes(sdl.SDL_Event);
        event.key.type = if (down) sdl.SDL_EVENT_KEY_DOWN else sdl.SDL_EVENT_KEY_UP;
        event.key.windowID = self.window_id;
        event.key.key = key.key;
        event.key.scancode = key.scancode;
        event.key.mod = mod;
        event.key.down = down;
        return self.push(&event);
    }

    fn fail(self: *AutoRunner, comptime format: []const u8, args: anytype) bool {
        std.debug.print("map-editor: BK_EDITOR_AUTO: FAIL: " ++ format ++ "\n", args);
        self.failed = true;
        return false;
    }
};

const NamedKey = struct { key: sdl.SDL_Keycode, scancode: sdl.SDL_Scancode };

/// A single character ('0'-'9', 'A'-'Z') or a named key, matched
/// case-insensitively for the named form - auto.zig's own `Key.name` does
/// not know SDL, so the mapping lives here, beside the rest of the event
/// delivery.
fn keyFromName(name: []const u8) ?NamedKey {
    if (name.len == 1) {
        return switch (name[0]) {
            '0' => .{ .key = sdl.SDLK_0, .scancode = sdl.SDL_SCANCODE_0 },
            '1' => .{ .key = sdl.SDLK_1, .scancode = sdl.SDL_SCANCODE_1 },
            '2' => .{ .key = sdl.SDLK_2, .scancode = sdl.SDL_SCANCODE_2 },
            '3' => .{ .key = sdl.SDLK_3, .scancode = sdl.SDL_SCANCODE_3 },
            '4' => .{ .key = sdl.SDLK_4, .scancode = sdl.SDL_SCANCODE_4 },
            '5' => .{ .key = sdl.SDLK_5, .scancode = sdl.SDL_SCANCODE_5 },
            '6' => .{ .key = sdl.SDLK_6, .scancode = sdl.SDL_SCANCODE_6 },
            '7' => .{ .key = sdl.SDLK_7, .scancode = sdl.SDL_SCANCODE_7 },
            '8' => .{ .key = sdl.SDLK_8, .scancode = sdl.SDL_SCANCODE_8 },
            '9' => .{ .key = sdl.SDLK_9, .scancode = sdl.SDL_SCANCODE_9 },
            'A', 'a' => .{ .key = sdl.SDLK_A, .scancode = sdl.SDL_SCANCODE_A },
            'B', 'b' => .{ .key = sdl.SDLK_B, .scancode = sdl.SDL_SCANCODE_B },
            'C', 'c' => .{ .key = sdl.SDLK_C, .scancode = sdl.SDL_SCANCODE_C },
            'D', 'd' => .{ .key = sdl.SDLK_D, .scancode = sdl.SDL_SCANCODE_D },
            'E', 'e' => .{ .key = sdl.SDLK_E, .scancode = sdl.SDL_SCANCODE_E },
            'F', 'f' => .{ .key = sdl.SDLK_F, .scancode = sdl.SDL_SCANCODE_F },
            'G', 'g' => .{ .key = sdl.SDLK_G, .scancode = sdl.SDL_SCANCODE_G },
            'H', 'h' => .{ .key = sdl.SDLK_H, .scancode = sdl.SDL_SCANCODE_H },
            'I', 'i' => .{ .key = sdl.SDLK_I, .scancode = sdl.SDL_SCANCODE_I },
            'J', 'j' => .{ .key = sdl.SDLK_J, .scancode = sdl.SDL_SCANCODE_J },
            'K', 'k' => .{ .key = sdl.SDLK_K, .scancode = sdl.SDL_SCANCODE_K },
            'L', 'l' => .{ .key = sdl.SDLK_L, .scancode = sdl.SDL_SCANCODE_L },
            'M', 'm' => .{ .key = sdl.SDLK_M, .scancode = sdl.SDL_SCANCODE_M },
            'N', 'n' => .{ .key = sdl.SDLK_N, .scancode = sdl.SDL_SCANCODE_N },
            'O', 'o' => .{ .key = sdl.SDLK_O, .scancode = sdl.SDL_SCANCODE_O },
            'P', 'p' => .{ .key = sdl.SDLK_P, .scancode = sdl.SDL_SCANCODE_P },
            'Q', 'q' => .{ .key = sdl.SDLK_Q, .scancode = sdl.SDL_SCANCODE_Q },
            'R', 'r' => .{ .key = sdl.SDLK_R, .scancode = sdl.SDL_SCANCODE_R },
            'S', 's' => .{ .key = sdl.SDLK_S, .scancode = sdl.SDL_SCANCODE_S },
            'T', 't' => .{ .key = sdl.SDLK_T, .scancode = sdl.SDL_SCANCODE_T },
            'U', 'u' => .{ .key = sdl.SDLK_U, .scancode = sdl.SDL_SCANCODE_U },
            'V', 'v' => .{ .key = sdl.SDLK_V, .scancode = sdl.SDL_SCANCODE_V },
            'W', 'w' => .{ .key = sdl.SDLK_W, .scancode = sdl.SDL_SCANCODE_W },
            'X', 'x' => .{ .key = sdl.SDLK_X, .scancode = sdl.SDL_SCANCODE_X },
            'Y', 'y' => .{ .key = sdl.SDLK_Y, .scancode = sdl.SDL_SCANCODE_Y },
            'Z', 'z' => .{ .key = sdl.SDLK_Z, .scancode = sdl.SDL_SCANCODE_Z },
            else => null,
        };
    }
    const named = [_]struct { name: []const u8, key: NamedKey }{
        .{ .name = "DELETE", .key = .{ .key = sdl.SDLK_DELETE, .scancode = sdl.SDL_SCANCODE_DELETE } },
        .{ .name = "BACKSPACE", .key = .{ .key = sdl.SDLK_BACKSPACE, .scancode = sdl.SDL_SCANCODE_BACKSPACE } },
        .{ .name = "HOME", .key = .{ .key = sdl.SDLK_HOME, .scancode = sdl.SDL_SCANCODE_HOME } },
        .{ .name = "END", .key = .{ .key = sdl.SDLK_END, .scancode = sdl.SDL_SCANCODE_END } },
        .{ .name = "ESCAPE", .key = .{ .key = sdl.SDLK_ESCAPE, .scancode = sdl.SDL_SCANCODE_ESCAPE } },
        .{ .name = "ESC", .key = .{ .key = sdl.SDLK_ESCAPE, .scancode = sdl.SDL_SCANCODE_ESCAPE } },
        .{ .name = "SPACE", .key = .{ .key = sdl.SDLK_SPACE, .scancode = sdl.SDL_SCANCODE_SPACE } },
        .{ .name = "ENTER", .key = .{ .key = sdl.SDLK_RETURN, .scancode = sdl.SDL_SCANCODE_RETURN } },
        .{ .name = "RETURN", .key = .{ .key = sdl.SDLK_RETURN, .scancode = sdl.SDL_SCANCODE_RETURN } },
        .{ .name = "TAB", .key = .{ .key = sdl.SDLK_TAB, .scancode = sdl.SDL_SCANCODE_TAB } },
        .{ .name = "UP", .key = .{ .key = sdl.SDLK_UP, .scancode = sdl.SDL_SCANCODE_UP } },
        .{ .name = "DOWN", .key = .{ .key = sdl.SDLK_DOWN, .scancode = sdl.SDL_SCANCODE_DOWN } },
        .{ .name = "LEFT", .key = .{ .key = sdl.SDLK_LEFT, .scancode = sdl.SDL_SCANCODE_LEFT } },
        .{ .name = "RIGHT", .key = .{ .key = sdl.SDLK_RIGHT, .scancode = sdl.SDL_SCANCODE_RIGHT } },
    };
    for (named) |entry| if (std.ascii.eqlIgnoreCase(entry.name, name)) return entry.key;
    return null;
}

fn isMouseEvent(event_type: u32) bool {
    return switch (event_type) {
        sdl.SDL_EVENT_MOUSE_MOTION, sdl.SDL_EVENT_MOUSE_BUTTON_DOWN, sdl.SDL_EVENT_MOUSE_BUTTON_UP, sdl.SDL_EVENT_MOUSE_WHEEL => true,
        else => false,
    };
}

fn focusName(focus: ?*sdl.SDL_Window, ours: ?*sdl.SDL_Window) []const u8 {
    const window = focus orelse return "none";
    return if (window == ours) "ours" else "another window";
}

/// ImGui's "no pointer" is -FLT_MAX on both axes.
fn writePos(w: *std.Io.Writer, x: f32, y: f32) std.Io.Writer.Error!void {
    if (x <= -std.math.floatMax(f32) or y <= -std.math.floatMax(f32)) return w.writeAll("none");
    try w.print("{d:.0},{d:.0}", .{ x, y });
}

/// The events a hidden window can still get from the OS, by name; null for
/// any other type (printed as a number).
fn eventName(event_type: u32) ?[]const u8 {
    return switch (event_type) {
        sdl.SDL_EVENT_WINDOW_SHOWN => "shown",
        sdl.SDL_EVENT_WINDOW_HIDDEN => "hidden",
        sdl.SDL_EVENT_WINDOW_EXPOSED => "exposed",
        sdl.SDL_EVENT_WINDOW_MOVED => "moved",
        sdl.SDL_EVENT_WINDOW_RESIZED => "resized",
        sdl.SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED => "pixel-size",
        sdl.SDL_EVENT_WINDOW_MINIMIZED => "minimized",
        sdl.SDL_EVENT_WINDOW_MAXIMIZED => "maximized",
        sdl.SDL_EVENT_WINDOW_RESTORED => "restored",
        sdl.SDL_EVENT_WINDOW_MOUSE_ENTER => "mouse-enter",
        sdl.SDL_EVENT_WINDOW_MOUSE_LEAVE => "mouse-leave",
        sdl.SDL_EVENT_WINDOW_FOCUS_GAINED => "focus-gained",
        sdl.SDL_EVENT_WINDOW_FOCUS_LOST => "focus-lost",
        sdl.SDL_EVENT_WINDOW_OCCLUDED => "occluded",
        sdl.SDL_EVENT_WINDOW_DISPLAY_CHANGED => "display-changed",
        sdl.SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED => "display-scale",
        sdl.SDL_EVENT_MOUSE_MOTION => "mouse-motion",
        sdl.SDL_EVENT_MOUSE_BUTTON_DOWN => "button-down",
        sdl.SDL_EVENT_MOUSE_BUTTON_UP => "button-up",
        sdl.SDL_EVENT_MOUSE_WHEEL => "wheel",
        sdl.SDL_EVENT_TEXT_INPUT => "text-input",
        sdl.SDL_EVENT_KEYMAP_CHANGED => "keymap",
        else => null,
    };
}
