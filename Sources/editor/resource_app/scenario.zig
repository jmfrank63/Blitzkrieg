//! The ResourceEditor's two scripted tiers, run on the engine session main.zig
//! has started on a hidden window:
//!
//!   ResourceEditor --smoke-edit <project.unt> <scratch folder>
//!   ResourceEditor --auto <fixtures folder> <scratch folder>   (BK_EDITOR_AUTO's schedule)
//!
//! --smoke-edit (resource-editor-smoke) is the project half of the smoke:
//! it copies the tracked project under <scratch>, opens it through the
//! lifecycle the File menu uses, edits one property, saves, undoes, saves
//! again and compares the file with the original byte for byte, then reopens
//! it. The frames of the interactive loop's own panels are drawn between the
//! steps and the last one is captured and measured: the project tree and the
//! inspector must have painted over the engine's clear colour.
//!
//! --auto (resource-editor-auto) replays BK_EDITOR_AUTO's schedule through
//! the kit's parser (kit/auto_schedule.zig), one action per frame, the way
//! the Map Editor's auto does, over the resource command registry:
//!
//!   do=new:<ext>            do=undo            do=redo
//!   do=set_prop:<name>=<value>                 the first property of that name
//!   do=close                do=import:unt      the shipped Gunner as infantry
//!   do=copy:<from>><to>     a tracked fixture to a scratch path, so Open locks the copy
//!   do=mod_dir:<folder>     the MOD's export folder (MOD Settings)
//!   do=export               Export Result into it
//!   do=export_refused       the same while the kind has no exporter: refused, naming the kind
//!   do=pack:<file.pak>      Compress to PAK, read back by the engine
//!   do=run_game             Game with -mod=<export folder>, then waitgame=<s>
//!   do=tree:<action>        a weapon or trench tree action on the first fitting node:
//!                           add_shoot_type, add_crater, add_source (one undo step)
//!   do=squad_drag:<slot>/<dx>/<dy>   a formation member dragged by a world offset
//!   do=squad_zero:<x>/<y>   do=squad_dir:<radians>   the zero point, the direction arrow
//!   do=squad_arrow:<x>/<y>   the arrow gesture (press, move, release) at a world point, through arrowAngle
//!   do=frame:<picture>      a thumbnail double-click: a frame under the Sprites item or the first animation
//!   do=delete_frame         the last frame item of the project, as the thumbnail list's Delete
//!   do=mesh_variant:<0|1|2>   the unit preview's combat, install or transportable model
//!   do=locators:<0|1>       the locator markers of the unit preview off or on
//!   do=pick_locator:<name>  the right-click of the unit preview at that locator's screen point, through mesh_logic.pickLocator
//!   do=preview_on           from here the preview scene follows the open project, as in the docks
//!   do=preview_run          do=preview_stop   Run (F5) and Stop of the preview, through previewPlayback
//!   do=preview_refused:<text>   Run on a project whose export fails: refused, naming that text
//!   do=pause:<ms>           real time passes, for the preview's clock
//!   do=grid_cell:<x>/<y>/<v>   a locked tile of the Object or Fence grid (0 erases), as the Draw grid tool's click
//!   do=grid_trans:<x>/<y>/<v>  a transparency tile, value 1..7 (0 erases), as the Draw transparency tool's click
//!   do=trans_line:<x1>/<y1>/<x2>/<y2>   a one-way line dragged between two tile centres (Object)
//!   do=grid_zero:<x>/<y>    Set zero clicked on a tile centre (Object)
//!   do=fence_centre:<x>/<y>   Centre on tile clicked on a tile (Fence)
//!   do=sprite_move:<dx>/<dy>  Move: the sprite dragged by that many grid pixels
//!   do=entrance:<x>/<y>     the Building's Entrance tool clicked on a tile centre
//!   do=point:<shoot|fire|smoke|dir>/<x>/<y>   a point of that family clicked on a tile centre (it becomes the active one; dir only selects)
//!   do=point_select:<mode>/<i>   point i of a family made the active one, by a click on it
//!   do=point_move:<i>/<x>/<y>   the active family's point i dragged by the Move point tool to a tile centre
//!   do=point_angle:<i>/<deg>   do=point_cone:<i>/<deg>   its direction handle or its cone handle dragged
//!                           (the Angle and cone tool) to where that direction, or that cone, points
//!   do=generate_points:<smoke|dir>   the Generate points button in that family's mode
//!   do=span_mark:<begin|end|front|back>/<x>/<y>   the Bridge's Span marks tool clicked on a tile centre, for that mark
//!   do=curve:<track>        the Function window's curve: the first key-frame node of that display name
//!   do=keyframe:add/<x>/<y>   a key added by a press and release at that value (one command)
//!   do=keyframe:move/<i>/<x>/<y>   key i pressed, dragged to that value and released (one command)
//!   do=keyframe:delete/<i>  key i made the active one and Delete pressed (key 0 is protected)
//!   {dir}, {fix}, {mods} and {data} stand in a path for the scratch folder, the fixtures, the installation's mods and Data folders
//!   do=image_open           the Image window drawn over the open mip, chc, cgc or mdc, placed clear of the tree
//!   do=image_select         the first objective, mission or chapter made the active item (the tree's selection)
//!   do=image_click:<x>/<y>  a pointer click on that picture pixel (place mode: the active item goes there)
//!   do=image_crosses:<on|off>   the Show crosses checkbox clicked
//!   do=image_drag_cross:<dx>/<dy>   the first cross pressed on, dragged by that many picture pixels, released
//!   do=keyframe:reset       the dock's Reset all
//!   do=keyframe:zoomx_in|zoomx_out|zoomy_in|zoomy_out   the curve's zoom menu (view only, no undo step)
//!   do=function_open        the Function window opened by Ctrl+F through the event queue, the docks drawn from then on (do=curve first)
//!   do=curve_click:<t>/<v>  a left press and release on empty graph space of the displayed widget: adds a key (read back, then undone and redone)
//!   do=curve_drag:<i>/<t>/<v>   key i pressed on its displayed handle, moved over four frames, released (read back, undone, redone)
//!   do=curve_delete:<i>     key i selected by a click on its handle, then the Delete key event (read back, undone, redone)
//!   do=function_close       the Function window closed
//!   The curve gestures are real SDL mouse and key events on the host's queue, aimed by the widget's own key-to-pixel
//!   mapping, one frame per step; the stored keys are asserted within one pixel's worth of value.
//!   do=camera               the preview's Camera button (horizontal against default camera)
//!   do=particle_info        the Get particle info button, through docks_logic.ParticleStatus; prints the four numbers
//!   do=source_mode:complex|simple   the Particle source button (docks_logic.SourceToggle: one undo step, the tree items open or close); prints the mode read back
//!   do=effect_direction:<deg>   the Effect Direction dock's needle turned to that value of its degrees text (docks_logic.turnEffect); prints the angle read back
//!   do=wireframe:on|off     the Wireframe check of a road or river preview, through the bridge (the flag only moves when the engine took it)
//!   do=tile_add:<picture>   a thumbnail double-click of the terrains list on the first Tiles item: one undo step (terrain_logic.addTile)
//!   do=tile_import:<terrains|crossets>/<path>   Import terrains / Import crossets of the tileset editor (terrain_logic.importFile); prints the tile count
//!   do=import_file:<ext>/<path>   Import a runtime file (a shipped particle xml for pcp) as a new project, through the bridge's reader
//!   do=import_refused:<ext>/<path>   the same for a kind with no import (eff): refused, with the bridge's reason
//!   The grid verbs need a frame drawn since the project opened (the grid editor lives in the panels)
//!   and go through GridEditor's press, move and release, the path of the mouse.
//!   open=<path> save saveas=<path> shot=<name> differ=<a>/<b>@<percent> exit
//!   expect=kind:<ext>  dirty:<true|false>  untitled  nodes_min:<n>  nodes:<n>
//!          prop:<name>=<value>  exported  file:<path>  shot_lit:<name>
//!          shot_same:<a>/<b>  the two frames are pixel for pixel equal
//!          slot:<n>=moved|home  the formation member against where the drag found it
//!          direction:<radians>  the formation's direction
//!          selected:<name>  the tree's selected node shows that name (a picked locator)
//!          grid_cell:<x>/<y>=<v>  trans_cell:<x>/<y>=<v>  the stored locked / transparency value of a tile
//!          lines:<n>  the Object's one-way line count  zero_tile:<x>/<y>  the zero point's tile (Object)
//!          sprite_tile:<x>/<y>  the sprite's tile (Fence)  sprite:moved|home  against where sprite_move found it
//!          shot_colour:<shot>/<RRGGBB>/min|max/<n>  the pixels of exactly that colour in a shot
//!          points:<shoot|fire|smoke|dir>=<n>  the Building's point count of a family
//!          point:<shoot|fire|smoke|dir>/<i>=<angle>/<cone>  one point's stored direction and cone (angle within 1 degree)
//!          entrance_tile:<x>/<y>  the Building's entrance tile
//!          span_mark:<begin|end|front|back>=moved|home  the Bridge's mark against the frame's default
//!          keys:<n>  the curve's stored key count  key:<i>=<x>/<y>  one stored key (within 0.02)
//!          effect_angle:<deg>  the Direction dock's degrees text for the bridge's stored angle (within 0.1)
//!          shot_curve_handle:<shot>/<i>  the displayed handle of key i is in the shot, measured at its drawn place
//!          zoom:<xs>/<ys>  the curve's pixels per step  camera:horizontal|default  the Camera button's state
//!
//! `{dir}` (the scratch folder), `{fix}` (the fixtures folder) and `{mods}`
//! (the installation's mods folder) are replaced in every path and argument,
//! since an argument may not hold a space. The actions of the pointer and
//! the Map Editor's own tools (press, drag, tool, compare, test) are refused
//! as unsupported rather than ignored. Shots are TGAs of the presented frame
//! under <scratch>, measured by code: `differ` fails unless the two frames
//! differ in more than the given share of their pixels, `shot_lit` unless
//! more than 2% of a frame is not the engine's clear colour. A failing
//! action prints the frame and the entry, and exits 1.
const std = @import("std");
const sdl3 = @import("sdl3");
const kit = @import("editor_kit");
const core = @import("resource_core");
const host_mod = kit.host;
const schedule = kit.auto_schedule;
const testlaunch = kit.testlaunch;
const c_bridge = @import("c_bridge.zig");
const edit = @import("edit_logic.zig");
const logic = @import("panels_logic.zig");
const panels_mod = @import("panels.zig");
const tools = @import("tools_logic.zig");
const squad = @import("squad_logic.zig");
const docks_logic = @import("docks_logic.zig");
const mesh = @import("mesh_logic.zig");
const grid = @import("grid_logic.zig");
const keyframe = @import("keyframe_logic.zig");
const docks_mod = @import("docks.zig");
const terrain = @import("terrain_logic.zig");

const c = c_bridge.c;

const Kind = core.bridge.Kind;
const ResBridge = core.bridge.ResBridge;
const PropRecord = core.bridge.PropRecord;
const sub_tools = core.sub_editor_tools;
const il = @import("image_logic.zig");
const Point2 = core.bridge.Point2;

const owner = "resource-editor-auto";
const gunner_folder = "Data/Units/Humans/German/Gunner";
/// The curve window's size for do=curve (the Function window's, made tall).
const curve_window = [2]i32{ 640, 1400 };
/// A frame counts as drawn when more than this share of it is not the clear colour.
const lit_share_percent: f64 = 2.0;
const max_tga_bytes = 64 << 20;

// --- Shared pieces -----------------------------------------------------------

/// One frame of the interactive loop's drawing: events, the panels over the
/// engine's frame.
fn drawFrame(host: *host_mod.Host, panels: *panels_mod.Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) host_mod.HostError!void {
    return drawFrameWithDocks(host, panels, gpa, b, life, null, null, null);
}

/// The same frame with the docks drawn after the panels, as the interactive
/// loop does, once do=function_open has made them.
fn drawFrameWithDocks(host: *host_mod.Host, panels: *panels_mod.Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, docks: ?*docks_mod.Docks, folder: ?[]const u8, selected: ?i32) host_mod.HostError!void {
    var event: sdl3.c.SDL_Event = undefined;
    while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
    host.beginFrame();
    panels.draw(gpa, b, life, host.window);
    if (docks) |d| {
        d.handleShortcuts();
        d.drawDocks(folder, life, selected);
    }
    try host.endFrame();
}

fn capture(real: *c_bridge.RealResBridge, gpa: std.mem.Allocator, path: []const u8) bool {
    const z = gpa.dupeZ(u8, path) catch return false;
    defer gpa.free(z);
    return c.BkEditorCaptureFrame(real.session, z.ptr) == c.BK_EDITOR_OK;
}

/// The share (0..100) of a captured frame's pixels that are not the engine's
/// clear colour (black).
fn litPercent(tga: schedule.Tga) f64 {
    var lit: usize = 0;
    var y: u32 = 0;
    while (y < tga.height) : (y += 1) {
        var x: u32 = 0;
        while (x < tga.width) : (x += 1) {
            const p = tga.pixel(x, y);
            if (p[0] > 2 or p[1] > 2 or p[2] > 2) lit += 1;
        }
    }
    const total = @as(f64, @floatFromInt(tga.width)) * @as(f64, @floatFromInt(tga.height));
    if (total == 0) return 0;
    return @as(f64, @floatFromInt(lit)) * 100.0 / total;
}

fn readFile(io: std.Io, gpa: std.mem.Allocator, path: []const u8) ![]u8 {
    return std.Io.Dir.cwd().readFileAlloc(io, path, gpa, .limited(max_tga_bytes));
}

fn failLine(comptime tier: []const u8, comptime format: []const u8, args: anytype) bool {
    std.debug.print("resource-editor: " ++ tier ++ " FAIL: " ++ format ++ "\n", args);
    return false;
}

// --- Smoke -------------------------------------------------------------------

/// The first property of the project a single edit can change: a whole
/// number, else a float, else free text.
const Pick = struct { node: i32, prop: PropRecord, widget: edit.Widget };

fn pickProp(life: *const logic.Lifecycle) ?Pick {
    const wanted = [_]edit.Widget{ .int, .float, .text };
    for (wanted) |widget| {
        for (life.doc.tree.props.items) |entry| {
            if (edit.widgetFor(edit.domainOf(&entry.record)) != widget) continue;
            return .{ .node = entry.node, .prop = entry.record, .widget = widget };
        }
    }
    return null;
}

/// A value that differs from `pick`'s.
fn changedValue(buffer: []u8, pick: Pick) ?[]const u8 {
    const current = pick.prop.valueSlice();
    switch (pick.widget) {
        .int => {
            const n = std.fmt.parseInt(i64, current, 10) catch return null;
            return std.fmt.bufPrint(buffer, "{d}", .{n + 1}) catch null;
        },
        .float => {
            const f = std.fmt.parseFloat(f64, current) catch return null;
            return std.fmt.bufPrint(buffer, "{d}", .{f + 1}) catch null;
        },
        else => return std.fmt.bufPrint(buffer, "{s}x", .{current}) catch null,
    }
}

/// Whether `a` and `b` are the same value as `widget` reads it.
fn sameValue(widget: edit.Widget, a: []const u8, b: []const u8) bool {
    switch (widget) {
        .int => {
            const x = std.fmt.parseInt(i64, a, 10) catch return false;
            const y = std.fmt.parseInt(i64, b, 10) catch return false;
            return x == y;
        },
        .float => {
            const x = std.fmt.parseFloat(f64, a) catch return false;
            const y = std.fmt.parseFloat(f64, b) catch return false;
            return @abs(x - y) < 1e-4;
        },
        else => return std.mem.eql(u8, a, b),
    }
}

fn valueOf(life: *const logic.Lifecycle, pick: Pick) ?[]const u8 {
    for (life.doc.tree.props.items) |*entry| {
        if (entry.node == pick.node and entry.record.id == pick.prop.id) return entry.record.valueSlice();
    }
    return null;
}

/// resource-editor-smoke: open a copy of the tracked .unt, edit, save, undo,
/// save, byte-compare, reopen. The exit is 0 for a pass and for a host with no
/// GPU device (the skip is printed), 1 for a failure.
pub fn smokeEdit(gpa: std.mem.Allocator, io: std.Io, host: *host_mod.Host, fixture: []const u8, scratch: []const u8) bool {
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, scratch) catch |err| return failLine("smoke", "{s} could not be cleared: {s}", .{ scratch, @errorName(err) });
    cwd.createDirPath(io, scratch) catch |err| return failLine("smoke", "{s}: {s}", .{ scratch, @errorName(err) });
    const original = readFile(io, gpa, fixture) catch |err| return failLine("smoke", "{s}: {s}", .{ fixture, @errorName(err) });
    defer gpa.free(original);
    var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const project = std.fmt.bufPrint(&path_buffer, "{s}{c}project.unt", .{ scratch, std.fs.path.sep }) catch return failLine("smoke", "the scratch path is too long", .{});
    cwd.writeFile(io, .{ .sub_path = project, .data = original }) catch |err| return failLine("smoke", "{s}: {s}", .{ project, @errorName(err) });

    var real = c_bridge.RealResBridge.init(gpa, host.session);
    const b = real.bridge();
    var life: logic.Lifecycle = .{};
    defer life.deinit(gpa);
    var panels: panels_mod.Panels = .{};
    defer panels.deinit(gpa);
    defer _ = b.close();

    // 1. Open: the lifecycle takes the lock, the tree is there, nothing is dirty.
    const outcome = life.openProject(gpa, b, project, owner) catch return failLine("smoke", "Open {s}: {s}", .{ project, b.lastMessage() });
    if (outcome != .opened) return failLine("smoke", "{s} opened read-only", .{project});
    if (life.doc.kind != .animation_infantry) return failLine("smoke", "{s} opened as {s}", .{ project, life.doc.kind.extension() });
    if (life.doc.tree.nodes.items.len < 2) return failLine("smoke", "the project has {d} nodes", .{life.doc.tree.nodes.items.len});
    if (life.dirty()) return failLine("smoke", "a freshly opened project is dirty", .{});
    if (!frames(host, &panels, gpa, b, &life, 3)) return false;

    // 2. Edit one property: one undo step, dirty, the mirror shows it.
    const pick = pickProp(&life) orelse return failLine("smoke", "the project has no int, float or text property to edit ({d} props on {d} nodes)", .{ life.doc.tree.props.items.len, life.doc.tree.nodes.items.len });
    var changed_buffer: [64]u8 = undefined;
    const changed = changedValue(&changed_buffer, pick) orelse return failLine("smoke", "property {s} = '{s}' cannot be changed", .{ pick.prop.displaySlice(), pick.prop.valueSlice() });
    var before_buffer: [core.bridge.value_text_capacity]u8 = undefined;
    const before = std.fmt.bufPrint(&before_buffer, "{s}", .{pick.prop.valueSlice()}) catch unreachable;
    const target: edit.Target = .{ .allocator = gpa, .bridge = b, .doc = &life.doc, .history = &life.history };
    edit.setProp(target, &.{pick.node}, pick.prop.id, changed, 0) catch return failLine("smoke", "setting {s} to {s}: {s}", .{ pick.prop.displaySlice(), changed, b.lastMessage() });
    if (!life.dirty() or !life.history.canUndo()) return failLine("smoke", "the edit left the project clean or without an undo step", .{});
    if (!frames(host, &panels, gpa, b, &life, 3)) return false;

    // 3. Save: the file on disk is no longer the original.
    life.saveProject(gpa, b, project) catch return failLine("smoke", "Save: {s}", .{b.lastMessage()});
    if (life.dirty()) return failLine("smoke", "the project is dirty after Save", .{});
    const edited = readFile(io, gpa, project) catch |err| return failLine("smoke", "{s}: {s}", .{ project, @errorName(err) });
    defer gpa.free(edited);
    if (std.mem.eql(u8, edited, original)) return failLine("smoke", "the saved file is byte for byte the original: the edit never reached it", .{});

    // 4. Undo past the save mark, save again: the original, byte for byte.
    if (!(edit.undo(target) catch return failLine("smoke", "Undo: {s}", .{b.lastMessage()}))) return failLine("smoke", "there was nothing to undo", .{});
    if (!life.dirty()) return failLine("smoke", "undoing past the save mark left the project clean", .{});
    const undone = valueOf(&life, pick) orelse return failLine("smoke", "the property is gone after Undo", .{});
    if (!sameValue(pick.widget, undone, before)) return failLine("smoke", "after Undo {s} is '{s}', not '{s}'", .{ pick.prop.displaySlice(), undone, before });
    life.saveProject(gpa, b, project) catch return failLine("smoke", "second Save: {s}", .{b.lastMessage()});
    const restored = readFile(io, gpa, project) catch |err| return failLine("smoke", "{s}: {s}", .{ project, @errorName(err) });
    defer gpa.free(restored);
    if (!std.mem.eql(u8, restored, original)) return failLine("smoke", "after edit, undo and save the file is {d} bytes, not the original's {d}", .{ restored.len, original.len });
    var bak_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const bak = std.fmt.bufPrint(&bak_buffer, "{s}.bak", .{project}) catch unreachable;
    cwd.access(io, bak, .{}) catch return failLine("smoke", "no {s} beside the saved project", .{bak});
    var tmp_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const tmp = std.fmt.bufPrint(&tmp_buffer, "{s}.tmp", .{project}) catch unreachable;
    if (cwd.access(io, tmp, .{})) |_| return failLine("smoke", "{s} was left behind", .{tmp}) else |_| {}

    // 5. Reopen: the engine's reader sees the same project.
    life.closeProject(gpa, b) catch return failLine("smoke", "Close: {s}", .{b.lastMessage()});
    _ = life.openProject(gpa, b, project, owner) catch return failLine("smoke", "reopening {s}: {s}", .{ project, b.lastMessage() });
    const reopened = valueOf(&life, pick) orelse return failLine("smoke", "the property is gone after reopening", .{});
    if (!sameValue(pick.widget, reopened, before)) return failLine("smoke", "reopened {s} is '{s}', not '{s}'", .{ pick.prop.displaySlice(), reopened, before });
    if (!frames(host, &panels, gpa, b, &life, 3)) return false;

    // 6. The panels painted: the capture is not the clear colour alone.
    var shot_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const shot = std.fmt.bufPrint(&shot_buffer, "{s}{c}smoke.tga", .{ scratch, std.fs.path.sep }) catch unreachable;
    if (!capture(&real, gpa, shot)) return failLine("smoke", "the frame was not captured: {s}", .{b.lastMessage()});
    const bytes = readFile(io, gpa, shot) catch |err| return failLine("smoke", "{s}: {s}", .{ shot, @errorName(err) });
    defer gpa.free(bytes);
    const tga = schedule.Tga.parse(bytes) catch |err| return failLine("smoke", "{s} is not a 32-bit TGA: {s}", .{ shot, @errorName(err) });
    const lit = litPercent(tga);
    if (lit <= lit_share_percent) return failLine("smoke", "only {d:.2}% of {s} is drawn: the panels did not paint", .{ lit, shot });

    std.debug.print("resource-editor: smoke-edit PASS (.unt, {s} {s} -> {s}, undo and save restored the original {d} bytes, reopened, {d:.1}% of the frame drawn)\n", .{ pick.prop.displaySlice(), before, changed, original.len, lit });
    return true;
}

fn frames(host: *host_mod.Host, panels: *panels_mod.Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, count: u32) bool {
    var i: u32 = 0;
    while (i < count) : (i += 1) {
        drawFrame(host, panels, gpa, b, life) catch |err| return failLine("smoke", "frame: {s}: {s}", .{ @errorName(err), b.lastMessage() });
    }
    return true;
}

// --- Auto --------------------------------------------------------------------

const Runner = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    environ: std.process.Environ,
    host: *host_mod.Host,
    real: c_bridge.RealResBridge,
    life: logic.Lifecycle = .{},
    panels: panels_mod.Panels = .{},
    dir: []const u8,
    fixtures: []const u8,
    base_root: []const u8,
    running: ?testlaunch.Running = null,
    exported: bool = false,
    /// The docks' preview state (Run, Stop), driven here the way Docks drives
    /// it, since the auto tier draws only the panels.
    preview: docks_logic.PreviewSync = .{},
    preview_on: bool = false,
    /// The slot a squad_drag moved and where it stood before, for expect=slot.
    dragged: ?struct { formation: i32, slot: usize, home: Point2 } = null,
    /// Where the sprite stood before the first sprite_move, for expect=sprite.
    sprite_home: ?Point2 = null,
    /// The Function window's curve for do=curve, and the Camera button's flag
    /// (CParticleFrame::bHorizontalCamera) that do=camera flips.
    curve: ?keyframe.Editor = null,
    horizontal_camera: bool = false,
    /// The docks, drawn from do=function_open on, so the Function window's
    /// widget takes the pointer events the pointer verbs queue.
    docks: ?docks_mod.Docks = null,
    /// From do=image_open on, the frames pass the docks the project's folder and the tree's selection,
    /// as main.zig's loop does, so the Image window finds its picture and its active item.
    image_on: bool = false,
    /// The Get particle info button's numbers (do=particle_info), the ones the
    /// status bar shows.
    particle_status: docks_logic.ParticleStatus = .{},
    /// The Particle source button (do=source_mode) with its remembered name.
    particle_source: docks_logic.SourceToggle = .{},
    frame: u32 = 0,
    message: [768]u8 = undefined,
    /// The last text `fail` made, for a helper that reports through its caller.
    failure: []const u8 = "",

    fn bridge(self: *Runner) ResBridge {
        return self.real.bridge();
    }

    fn fail(self: *Runner, comptime format: []const u8, args: anytype) []const u8 {
        self.failure = std.fmt.bufPrint(&self.message, format, args) catch "the failure text did not fit";
        return self.failure;
    }

    fn target(self: *Runner) edit.Target {
        return .{ .allocator = self.gpa, .bridge = self.bridge(), .doc = &self.life.doc, .history = &self.life.history, .read_only = self.life.read_only };
    }

    /// `{dir}`, `{fix}` and `{mods}` replaced; null when it does not fit.
    fn expand(self: *Runner, buffer: []u8, text: []const u8) ?[]const u8 {
        var len: usize = 0;
        var i: usize = 0;
        while (i < text.len) {
            const tokens = [_]struct { token: []const u8, value: []const u8, suffix: []const u8 }{
                .{ .token = "{dir}", .value = self.dir, .suffix = "" },
                .{ .token = "{fix}", .value = self.fixtures, .suffix = "" },
                .{ .token = "{mods}", .value = self.base_root, .suffix = "mods" },
                .{ .token = "{data}", .value = self.base_root, .suffix = "Data" },
            };
            var matched = false;
            for (tokens) |t| {
                if (!std.mem.startsWith(u8, text[i..], t.token)) continue;
                const piece = std.fmt.bufPrint(buffer[len..], "{s}{s}", .{ t.value, t.suffix }) catch return null;
                len += piece.len;
                i += t.token.len;
                matched = true;
                break;
            }
            if (matched) continue;
            if (len >= buffer.len) return null;
            buffer[len] = text[i];
            len += 1;
            i += 1;
        }
        return buffer[0..len];
    }

    fn shotPath(self: *Runner, buffer: []u8, name: []const u8) ?[]const u8 {
        return std.fmt.bufPrint(buffer, "{s}{c}{s}.tga", .{ self.dir, std.fs.path.sep, name }) catch null;
    }

    // --- Actions -----------------------------------------------------------

    /// null when the action held, else why not.
    fn run(self: *Runner, action: schedule.Action, before_draw: bool) ?[]const u8 {
        switch (action) {
            .do => |named| return if (before_draw) self.command(named) else null,
            .expect => |named| {
                if (std.mem.startsWith(u8, named.name, "shot_") != !before_draw) return null;
                return self.predicate(named);
            },
            .open => |text| {
                if (!before_draw) return null;
                var buffer: [logic.path_capacity]u8 = undefined;
                const path = self.expand(&buffer, text) orelse return self.fail("the path is too long", .{});
                const result = self.life.openProject(self.gpa, self.bridge(), path, owner) catch return self.fail("open {s}: {s}", .{ path, self.bridge().lastMessage() });
                if (result == .read_only) return self.fail("{s} opened read-only", .{path});
                return null;
            },
            .save => {
                if (!before_draw) return null;
                const path = self.life.doc.pathSlice() orelse return self.fail("save: the project has no path yet (saveas first)", .{});
                var copy: [logic.path_capacity]u8 = undefined;
                const own = std.fmt.bufPrint(&copy, "{s}", .{path}) catch return self.fail("the path is too long", .{});
                self.life.saveProject(self.gpa, self.bridge(), own) catch return self.fail("save {s}: {s}", .{ own, self.bridge().lastMessage() });
                return null;
            },
            .saveas => |text| {
                if (!before_draw) return null;
                var buffer: [logic.path_capacity]u8 = undefined;
                const path = self.expand(&buffer, text) orelse return self.fail("the path is too long", .{});
                self.life.saveProject(self.gpa, self.bridge(), path) catch return self.fail("saveas {s}: {s}", .{ path, self.bridge().lastMessage() });
                return null;
            },
            .waitgame => |seconds| {
                if (!before_draw) return null;
                const running = if (self.running) |*r| r else return self.fail("waitgame: no game was started (do=run_game first)", .{});
                const exit = running.waitBlocking(self.io, seconds * 1000) orelse {
                    running.terminate(self.io);
                    return self.fail("the game did not exit within {d} s", .{seconds});
                };
                if (exit.code == null or exit.code.? != 0) return self.fail("the game exited with code {any}, signal {any}", .{ exit.code, exit.signal });
                self.running = null;
                return null;
            },
            .shot => |name| {
                if (before_draw) return null;
                var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
                const path = self.shotPath(&buffer, name) orelse return self.fail("the shot path is too long", .{});
                if (!capture(&self.real, self.gpa, path)) return self.fail("the frame was not captured: {s}", .{self.bridge().lastMessage()});
                return null;
            },
            .differ => |d| {
                if (before_draw) return null;
                return self.differ(d);
            },
            .exit => return null,
            else => return self.fail("this action belongs to the Map Editor's tools and is not supported here", .{}),
        }
    }

    /// Two captured shots compared into `diff`; null when they could be, else why not.
    fn compareShots(self: *Runner, a_name: []const u8, b_name: []const u8, diff: *schedule.Diff) ?[]const u8 {
        var a_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        var b_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const a_path = self.shotPath(&a_buffer, a_name) orelse return self.fail("the shot path is too long", .{});
        const b_path = self.shotPath(&b_buffer, b_name) orelse return self.fail("the shot path is too long", .{});
        const a_bytes = readFile(self.io, self.gpa, a_path) catch |err| return self.fail("{s}: {s}", .{ a_path, @errorName(err) });
        defer self.gpa.free(a_bytes);
        const b_bytes = readFile(self.io, self.gpa, b_path) catch |err| return self.fail("{s}: {s}", .{ b_path, @errorName(err) });
        defer self.gpa.free(b_bytes);
        const a = schedule.Tga.parse(a_bytes) catch |err| return self.fail("{s}: {s}", .{ a_path, @errorName(err) });
        const b = schedule.Tga.parse(b_bytes) catch |err| return self.fail("{s}: {s}", .{ b_path, @errorName(err) });
        diff.* = schedule.compareTga(a, b, schedule.default_channel_tolerance);
        if (!diff.same_size) return self.fail("{s} and {s} are different sizes", .{ a_name, b_name });
        return null;
    }

    fn differ(self: *Runner, d: schedule.Differ) ?[]const u8 {
        var diff: schedule.Diff = undefined;
        if (self.compareShots(d.a, d.b, &diff)) |reason| return reason;
        const percent = @as(f64, diff.fraction()) * 100.0;
        if (percent <= d.percent) return self.fail("{s} and {s} differ in {d:.3}% of their pixels, not more than {d:.3}%", .{ d.a, d.b, percent, d.percent });
        std.debug.print("resource-editor: auto: {s} and {s} differ in {d:.2}% of their pixels\n", .{ d.a, d.b, percent });
        return null;
    }

    /// A token holds no space, so '_' stands for one: X_position finds "X position".
    fn findProp(self: *Runner, token: []const u8) ?struct { node: i32, record: PropRecord } {
        var spaced: [64]u8 = undefined;
        const name = if (token.len <= spaced.len) blk: {
            for (token, 0..) |ch, i| spaced[i] = if (ch == '_') ' ' else ch;
            break :blk spaced[0..token.len];
        } else token;
        for (self.life.doc.tree.props.items) |entry| {
            if (std.ascii.eqlIgnoreCase(entry.record.defaultSlice(), name) or std.ascii.eqlIgnoreCase(entry.record.displaySlice(), name) or
                std.ascii.eqlIgnoreCase(entry.record.defaultSlice(), token) or std.ascii.eqlIgnoreCase(entry.record.displaySlice(), token))
                return .{ .node = entry.node, .record = entry.record };
        }
        return null;
    }

    fn command(self: *Runner, named: schedule.Named) ?[]const u8 {
        const b = self.bridge();
        const name = named.name;
        const eql = std.mem.eql;
        var buffer: [logic.path_capacity]u8 = undefined;
        if (eql(u8, name, "new")) {
            const kind = logic.kindFromExtension(named.arg) orelse return self.fail("new: '{s}' is not a project extension", .{named.arg});
            self.life.newProject(self.gpa, b, kind) catch return self.fail("new {s}: {s}", .{ named.arg, b.lastMessage() });
            return null;
        }
        if (eql(u8, name, "close")) {
            self.life.closeProject(self.gpa, b) catch return self.fail("close: {s}", .{b.lastMessage()});
            return null;
        }
        if (eql(u8, name, "undo") or eql(u8, name, "redo")) {
            const moved = (if (name[0] == 'u') edit.undo(self.target()) else edit.redo(self.target())) catch return self.fail("{s}: {s}", .{ name, b.lastMessage() });
            return if (moved) null else self.fail("there was nothing to {s}", .{name});
        }
        if (eql(u8, name, "set_prop")) {
            const eq = std.mem.indexOfScalar(u8, named.arg, '=') orelse return self.fail("set_prop needs <name>=<value>", .{});
            const found = self.findProp(named.arg[0..eq]) orelse return self.fail("no property named {s}", .{named.arg[0..eq]});
            const value = self.expand(&buffer, named.arg[eq + 1 ..]) orelse return self.fail("the value is too long", .{});
            edit.setProp(self.target(), &.{found.node}, found.record.id, value, 0) catch return self.fail("set {s} = {s}: {s}", .{ named.arg[0..eq], value, b.lastMessage() });
            return null;
        }
        if (eql(u8, name, "copy")) {
            const arrow = std.mem.indexOfScalar(u8, named.arg, '>') orelse return self.fail("copy needs <from>><to>", .{});
            var from_buffer: [logic.path_capacity]u8 = undefined;
            const from = self.expand(&from_buffer, named.arg[0..arrow]) orelse return self.fail("the path is too long", .{});
            const to = self.expand(&buffer, named.arg[arrow + 1 ..]) orelse return self.fail("the path is too long", .{});
            const bytes = readFile(self.io, self.gpa, from) catch |err| return self.fail("copy {s}: {s}", .{ from, @errorName(err) });
            defer self.gpa.free(bytes);
            const cwd = std.Io.Dir.cwd();
            if (std.fs.path.dirname(to)) |parent| cwd.createDirPath(self.io, parent) catch |err| return self.fail("{s}: {s}", .{ parent, @errorName(err) });
            cwd.writeFile(self.io, .{ .sub_path = to, .data = bytes }) catch |err| return self.fail("copy to {s}: {s}", .{ to, @errorName(err) });
            return null;
        }
        if (eql(u8, name, "import")) {
            const kind = logic.kindFromExtension(named.arg) orelse return self.fail("import: '{s}' is not a project extension", .{named.arg});
            if (kind != .animation_infantry) return self.fail("import: only .unt has a tracked folder to import from", .{});
            var folder_buffer: [logic.path_capacity]u8 = undefined;
            const folder = std.fmt.bufPrint(&folder_buffer, "{s}{s}", .{ self.base_root, gunner_folder }) catch return self.fail("the installation path is too long", .{});
            self.life.importFromGame(self.gpa, b, kind, folder) catch return self.fail("import {s} from {s}: {s}", .{ named.arg, folder, b.lastMessage() });
            return null;
        }
        if (eql(u8, name, "mod_dir")) {
            const dir = self.expand(&buffer, named.arg) orelse return self.fail("the folder is too long", .{});
            // The mod folder of an earlier run would pass for this one.
            if (std.mem.indexOf(u8, dir, "reseditor_auto") != null) std.Io.Dir.cwd().deleteTree(self.io, dir) catch {};
            var mod: core.bridge.ModSettings = .{};
            if (b.modSettingsGet(&mod) != .ok) return self.fail("MOD settings: {s}", .{b.lastMessage()});
            if (!mod.setExportDir(dir)) return self.fail("the folder does not fit MOD Settings", .{});
            if (mod.nameSlice().len == 0) _ = mod.setName("reseditor_auto");
            if (b.modSettingsSet(&mod) != .ok) return self.fail("MOD settings: {s}", .{b.lastMessage()});
            return null;
        }
        if (eql(u8, name, "export")) {
            self.exported = false;
            var outcome: tools.ExportOutcome = .{};
            tools.runExport(b, &self.life, false, false, true, &outcome);
            if (outcome.status != .ok) return self.fail("export: {s}", .{outcome.message()});
            std.debug.print("resource-editor: auto: export written {d}, skipped {d}, warnings {d}\n", .{ outcome.report.written, outcome.report.skipped, outcome.report.warning_total });
            self.exported = outcome.report.written > 0;
            return null;
        }
        if (eql(u8, name, "export_refused")) {
            // The kind's exporter comes with its sub-editor; until it does the
            // refusal is the behaviour, and it must name the kind.
            var outcome: tools.ExportOutcome = .{};
            tools.runExport(b, &self.life, false, false, true, &outcome);
            if (outcome.status != .refused) return self.fail("export_refused: the export answered {s}", .{@tagName(outcome.status)});
            var needle_buffer: [8]u8 = undefined;
            const needle = std.fmt.bufPrint(&needle_buffer, ".{s}", .{self.life.doc.kind.extension()}) catch unreachable;
            if (std.mem.indexOf(u8, outcome.message(), needle) == null) return self.fail("the refusal does not name {s}: {s}", .{ needle, outcome.message() });
            std.debug.print("resource-editor: auto: export refused as expected: {s}\n", .{outcome.message()});
            return null;
        }
        if (eql(u8, name, "pack")) {
            const path = self.expand(&buffer, named.arg) orelse return self.fail("the path is too long", .{});
            if (b.packMod(path) != .ok) return self.fail("pack {s}: {s}", .{ path, b.lastMessage() });
            return null;
        }
        if (eql(u8, name, "tree")) return self.treeAction(named.arg);
        if (eql(u8, name, "squad_drag")) return self.squadDrag(named.arg);
        if (eql(u8, name, "squad_zero")) {
            const at = parsePoint(named.arg) orelse return self.fail("squad_zero needs <x>/<y>", .{});
            const node = self.firstFormation() orelse return self.fail("squad_zero: the project has no formation", .{});
            const step = sub_tools.setZeroPoint(self.gpa, b, node, at) catch return self.fail("squad_zero: {s}", .{b.lastMessage()});
            sub_tools.commit(self.gpa, b, &self.life.doc, &self.life.history, step, 0) catch return self.fail("squad_zero: {s}", .{b.lastMessage()});
            return null;
        }
        if (eql(u8, name, "squad_dir")) {
            const angle = std.fmt.parseFloat(f32, named.arg) catch return self.fail("squad_dir needs radians", .{});
            const node = self.firstFormation() orelse return self.fail("squad_dir: the project has no formation", .{});
            const step = sub_tools.setFormationDirection(self.gpa, b, node, angle, null) catch return self.fail("squad_dir: {s}", .{b.lastMessage()});
            sub_tools.commit(self.gpa, b, &self.life.doc, &self.life.history, step, 0) catch return self.fail("squad_dir: {s}", .{b.lastMessage()});
            return null;
        }
        if (eql(u8, name, "squad_arrow")) return self.squadArrow(named.arg);
        if (eql(u8, name, "run_game")) return self.runGame();
        if (eql(u8, name, "frame")) {
            docks_logic.addFrameFromPicture(self.gpa, b, &self.life, null, named.arg) catch return self.fail("frame:{s}: {s}", .{ named.arg, b.lastMessage() });
            return null;
        }
        if (eql(u8, name, "delete_frame")) {
            const frame = self.lastFrame() orelse return self.fail("delete_frame: the project has no frame item", .{});
            docks_logic.deleteSelectedFrame(self.gpa, b, &self.life, frame) catch return self.fail("delete_frame: {s}", .{b.lastMessage()});
            return null;
        }
        if (eql(u8, name, "preview_on")) {
            self.preview_on = true;
            return null;
        }
        if (eql(u8, name, "mesh_variant")) {
            const index = std.fmt.parseInt(u8, named.arg, 10) catch return self.fail("mesh_variant needs 0, 1 or 2", .{});
            if (index > 2) return self.fail("mesh_variant needs 0, 1 or 2, not {d}", .{index});
            self.panels.mesh_toolbar.setVariant(b, @enumFromInt(index)) catch return self.fail("mesh_variant:{d}: {s}", .{ index, b.lastMessage() });
            return null;
        }
        if (eql(u8, name, "locators")) {
            const on = eql(u8, named.arg, "1");
            if (!on and !eql(u8, named.arg, "0")) return self.fail("locators needs 0 or 1", .{});
            self.panels.mesh_toolbar.setShow(b, on, false) catch return self.fail("locators:{s}: {s}", .{ named.arg, b.lastMessage() });
            return null;
        }
        if (eql(u8, name, "pick_locator")) return self.pickLocator(named.arg);
        if (eql(u8, name, "preview_run")) {
            if (!self.preview.run(b)) return self.fail("preview_run: {s}", .{self.preview.message()});
            return null;
        }
        if (eql(u8, name, "preview_refused")) {
            // The effect's function particle names a source the preview's data does not
            // hold: Run says so, naming the file, and nothing runs.
            if (self.preview.run(b)) return self.fail("preview_refused: Run showed the project", .{});
            if (std.mem.indexOf(u8, self.preview.message(), named.arg) == null) return self.fail("preview_refused: the refusal does not name {s}: {s}", .{ named.arg, self.preview.message() });
            std.debug.print("resource-editor: auto: preview refused as expected: {s}\n", .{self.preview.message()});
            return null;
        }
        if (eql(u8, name, "preview_stop")) {
            self.preview.halt(b);
            if (self.preview.running) return self.fail("preview_stop: the preview is still running", .{});
            return null;
        }
        if (eql(u8, name, "grid_cell") or eql(u8, name, "grid_trans")) {
            const parts = parseInts(3, named.arg) orelse return self.fail("{s} needs <x>/<y>/<value>", .{name});
            const trans = name[5] == 't';
            if (parts[2] < 0 or parts[2] > grid_value_max) return self.fail("{s}: the value {d} is out of range", .{ name, parts[2] });
            const value: u8 = @intCast(parts[2]);
            if (trans and value != 0) {
                const editor = self.gridEditor(name) orelse return self.fail("{s}: no grid editor is open (draw a frame after opening an obt or fnc)", .{name});
                editor.setTransparency(value);
            }
            const tile = [2]i32{ parts[0], parts[1] };
            return self.gridStroke(name, if (trans) .draw_transparency else .draw_grid, tile, tile, value == 0);
        }
        if (eql(u8, name, "trans_line")) {
            const p = parseInts(4, named.arg) orelse return self.fail("trans_line needs <x1>/<y1>/<x2>/<y2>", .{});
            return self.gridStroke(name, .one_way_line, .{ p[0], p[1] }, .{ p[2], p[3] }, false);
        }
        if (eql(u8, name, "grid_zero") or eql(u8, name, "fence_centre")) {
            const p = parseInts(2, named.arg) orelse return self.fail("{s} needs <x>/<y>", .{name});
            const tile = [2]i32{ p[0], p[1] };
            return self.gridStroke(name, if (name[0] == 'g') .set_zero else .centre_on_tile, tile, tile, false);
        }
        if (eql(u8, name, "entrance")) {
            const p = parseInts(2, named.arg) orelse return self.fail("entrance needs <x>/<y>", .{});
            const tile = [2]i32{ p[0], p[1] };
            return self.gridStroke(name, .entrance, tile, tile, false);
        }
        if (eql(u8, name, "point")) {
            const slash = std.mem.indexOfScalar(u8, named.arg, '/') orelse return self.fail("point needs <mode>/<x>/<y>", .{});
            const mode = parseMode(named.arg[0..slash]) orelse return self.fail("point: '{s}' is not shoot, fire, smoke or dir", .{named.arg[0..slash]});
            const p = parseInts(2, named.arg[slash + 1 ..]) orelse return self.fail("point needs <mode>/<x>/<y>", .{});
            const tile = [2]i32{ p[0], p[1] };
            return self.gridStroke(name, pointTool(mode), tile, tile, false);
        }
        if (eql(u8, name, "point_select")) return self.pointSelect(named.arg);
        if (eql(u8, name, "point_move")) return self.pointGesture(name, .move, named.arg);
        if (eql(u8, name, "point_angle")) return self.pointGesture(name, .direction, named.arg);
        if (eql(u8, name, "point_cone")) return self.pointGesture(name, .cone, named.arg);
        if (eql(u8, name, "generate_points")) {
            const mode = parseMode(named.arg) orelse return self.fail("generate_points needs smoke or dir", .{});
            if (mode != .smoke and mode != .dir_explosion) return self.fail("generate_points needs smoke or dir", .{});
            const editor = self.gridEditor(name) orelse return self.fail("generate_points: no grid editor is open (draw a frame after opening a bld)", .{});
            editor.setTool(b, pointTool(mode)) catch return self.fail("generate_points: the {s} tool is not offered for .{s}", .{ pointTool(mode).label(), self.life.doc.kind.extension() });
            editor.generate(b, &self.life.doc, &self.life.history) catch |err| return self.fail("generate_points:{s}: {s} {s}", .{ named.arg, @errorName(err), b.lastMessage() });
            return null;
        }
        if (eql(u8, name, "span_mark")) {
            const slash = std.mem.indexOfScalar(u8, named.arg, '/') orelse return self.fail("span_mark needs <mark>/<x>/<y>", .{});
            const mark = parseSpanMark(named.arg[0..slash]) orelse return self.fail("span_mark: '{s}' is not begin, end, front or back", .{named.arg[0..slash]});
            const p = parseInts(2, named.arg[slash + 1 ..]) orelse return self.fail("span_mark needs <mark>/<x>/<y>", .{});
            const editor = self.gridEditor(name) orelse return self.fail("span_mark: no grid editor is open (draw a frame after opening a bdg)", .{});
            editor.span_mark = mark;
            const tile = [2]i32{ p[0], p[1] };
            return self.gridStroke(name, .span_marks, tile, tile, false);
        }
        if (eql(u8, name, "sprite_move")) return self.spriteMove(named.arg);
        if (eql(u8, name, "curve")) return self.selectCurve(named.arg);
        if (eql(u8, name, "keyframe")) return self.keyframeVerb(named.arg);
        if (eql(u8, name, "function_open")) return self.functionOpen();
        if (eql(u8, name, "image_open")) return self.imageOpen();
        if (eql(u8, name, "image_select")) return self.imageSelect();
        if (eql(u8, name, "image_click")) return self.imageClick(named.arg);
        if (eql(u8, name, "image_crosses")) return self.imageCrosses(named.arg);
        if (eql(u8, name, "image_drag_cross")) return self.imageDragCross(named.arg);
        if (eql(u8, name, "function_close")) {
            const docks = if (self.docks) |*d| d else return self.fail("function_close: the Function window is not open", .{});
            docks.show_function = false;
            return null;
        }
        if (eql(u8, name, "curve_click")) return self.curveClick(named.arg);
        if (eql(u8, name, "curve_drag")) return self.curveDrag(named.arg);
        if (eql(u8, name, "curve_delete")) return self.curveDelete(named.arg);
        if (eql(u8, name, "camera")) {
            const was = self.horizontal_camera;
            self.horizontal_camera = docks_logic.toggledCamera(b, was);
            if (self.horizontal_camera == was) return self.fail("camera: the engine refused the change: {s}", .{b.lastMessage()});
            std.debug.print("resource-editor: auto: camera is now {s}\n", .{if (self.horizontal_camera) "horizontal" else "default"});
            return null;
        }
        if (eql(u8, name, "particle_info")) {
            if (!self.particle_status.press(b)) return self.fail("particle_info: {s}", .{self.particle_status.note()});
            const info = self.particle_status.info.?;
            var line: [160]u8 = undefined;
            std.debug.print("resource-editor: auto: particle info: {s} (max_count={d} max_size={d} average_size={d} average_count={d})\n", .{ docks_logic.infoLine(&line, info), info.max_count, info.max_size, info.average_size, info.average_count });
            return null;
        }
        if (eql(u8, name, "effect_direction")) {
            const degrees = std.fmt.parseFloat(f32, named.arg) catch return self.fail("effect_direction needs degrees, got '{s}'", .{named.arg});
            const angle = docks_logic.angleOfDegrees(degrees);
            docks_logic.turnEffect(b, angle) catch return self.fail("effect_direction: {s}", .{b.lastMessage()});
            var stored: f32 = 0;
            if (!docks_logic.syncEffectAngle(b, &stored)) return self.fail("effect_direction: {s}", .{b.lastMessage()});
            std.debug.print("resource-editor: auto: effect direction: dock {d:.2} degrees = {d:.4} rad, bridge reads {d:.4} rad\n", .{ degrees, angle, stored });
            return null;
        }
        if (eql(u8, name, "source_mode")) {
            const want_complex = if (eql(u8, named.arg, "complex")) true else if (eql(u8, named.arg, "simple")) false else return self.fail("source_mode needs complex or simple", .{});
            const have = docks_logic.SourceToggle.mode(b) orelse return self.fail("source_mode: {s}", .{b.lastMessage()});
            if (have == want_complex) return self.fail("source_mode: the project is already {s}", .{named.arg});
            // The auto run has no dialog to ask the name in, so it offers one.
            if (self.particle_source.toggle(self.target(), if (want_complex) "effects\\particles\\flame" else null) != .switched)
                return self.fail("source_mode: {s}", .{self.particle_source.note()});
            const now = docks_logic.SourceToggle.mode(b) orelse return self.fail("source_mode: {s}", .{b.lastMessage()});
            std.debug.print("resource-editor: auto: source mode is now {s} (bridge reads {s}, undo steps {d})\n", .{ named.arg, if (now) "complex" else "simple", self.life.history.undo_stack.items.len });
            return null;
        }
        if (eql(u8, name, "wireframe")) {
            const on = if (eql(u8, named.arg, "on")) true else if (eql(u8, named.arg, "off")) false else return self.fail("wireframe needs on or off", .{});
            if (b.previewWireframe(on) != .ok) return self.fail("wireframe:{s}: {s}", .{ named.arg, b.lastMessage() });
            std.debug.print("resource-editor: auto: wireframe is now {s}\n", .{named.arg});
            return null;
        }
        if (eql(u8, name, "tile_add")) {
            const tiles = self.firstOfClass(sub_tools.item_type.tileset_tiles) orelse return self.fail("tile_add: the project has no Tiles item", .{});
            const before = self.life.doc.tree.nodes.items.len;
            terrain.addTile(self.gpa, b, &self.life, tiles, .terrains, named.arg) catch return self.fail("tile_add:{s}: {s}", .{ named.arg, b.lastMessage() });
            std.debug.print("resource-editor: auto: tile {s} added: {d} nodes -> {d}, undo steps {d}\n", .{ named.arg, before, self.life.doc.tree.nodes.items.len, self.life.history.undo_stack.items.len });
            return null;
        }
        if (eql(u8, name, "tile_import")) {
            const slash = std.mem.indexOfScalar(u8, named.arg, '/') orelse return self.fail("tile_import needs <terrains|crossets>/<path>", .{});
            const mode: terrain.Mode = if (eql(u8, named.arg[0..slash], "terrains")) .terrains else if (eql(u8, named.arg[0..slash], "crossets")) .crossets else return self.fail("tile_import needs terrains or crossets, not '{s}'", .{named.arg[0..slash]});
            const path = self.expand(&buffer, named.arg[slash + 1 ..]) orelse return self.fail("the path is too long", .{});
            const count = terrain.importFile(self.gpa, b, &self.life, path, mode) catch return self.fail("tile_import {s}: {s}", .{ path, b.lastMessage() });
            std.debug.print("resource-editor: auto: imported {d} {s} tiles from {s}, {d} nodes\n", .{ count, named.arg[0..slash], path, self.life.doc.tree.nodes.items.len });
            return null;
        }
        if (eql(u8, name, "import_file") or eql(u8, name, "import_refused")) {
            const slash = std.mem.indexOfScalar(u8, named.arg, '/') orelse return self.fail("{s} needs <ext>/<path>", .{name});
            const kind = logic.kindFromExtension(named.arg[0..slash]) orelse return self.fail("{s}: '{s}' is not a project extension", .{ name, named.arg[0..slash] });
            const path = self.expand(&buffer, named.arg[slash + 1 ..]) orelse return self.fail("the path is too long", .{});
            const refused = name[7] == 'r';
            if (self.life.importFromGame(self.gpa, b, kind, path)) |_| {
                if (refused) return self.fail("import_refused: importing .{s} from {s} succeeded", .{ named.arg[0..slash], path });
            } else |_| {
                if (!refused) return self.fail("import_file {s}: {s}", .{ path, b.lastMessage() });
                std.debug.print("resource-editor: auto: import refused as expected: {s}\n", .{b.lastMessage()});
            }
            return null;
        }
        if (eql(u8, name, "pause")) {
            const ms = std.fmt.parseInt(i64, named.arg, 10) catch return self.fail("pause needs milliseconds", .{});
            self.io.sleep(.fromMilliseconds(ms), .awake) catch {};
            return null;
        }
        return self.fail("unknown command '{s}'", .{name});
    }

    /// The id of the first tree node of class `class`, or null.
    fn firstOfClass(self: *Runner, class: i32) ?i32 {
        for (self.life.doc.tree.nodes.items) |node| {
            const have = std.fmt.parseInt(i32, node.classSlice(), 10) catch continue;
            if (have == class) return node.id;
        }
        return null;
    }

    /// do=curve: the Function window's editor for the first key-frame node
    /// named `wanted`; the bridge refuses the knobs of a node that is no curve.
    fn selectCurve(self: *Runner, wanted: []const u8) ?[]const u8 {
        const b = self.bridge();
        for (self.life.doc.tree.nodes.items) |*node| {
            if (!std.ascii.eqlIgnoreCase(node.displaySlice(), wanted)) continue;
            var knobs: core.bridge.KeyframeKnobs = .{};
            if (b.keyframeKnobs(node.id, &knobs) != .ok) continue;
            if (self.curve) |*old| old.deinit();
            var editor = keyframe.Editor.init(self.gpa, node.id);
            // A window tall enough for the whole value range, so the gestures
            // never scroll and a value maps to one pixel row.
            editor.setSize(curve_window[0], curve_window[1]);
            editor.load(b) catch return self.fail("curve:{s}: {s}", .{ wanted, b.lastMessage() });
            self.curve = editor;
            std.debug.print("resource-editor: auto: curve {s} is node {d}: {d} keys, x {d:.2}..{d:.2}, y {d:.2}..{d:.2}\n", .{ wanted, node.id, editor.keys.items.len, knobs.min_x, knobs.max_x, knobs.min_y, knobs.max_y });
            return null;
        }
        return self.fail("curve:{s}: the project has no key-frame node of that name", .{wanted});
    }

    /// do=keyframe: one curve gesture through keyframe_logic, the way the
    /// Function window drives it. The editor is reloaded first, so an undo or
    /// redo since the last verb is seen; zoom lives in the editor and stays.
    fn keyframeVerb(self: *Runner, text: []const u8) ?[]const u8 {
        const b = self.bridge();
        const editor = if (self.curve) |*e| e else return self.fail("keyframe:{s}: no curve was selected (do=curve first)", .{text});
        editor.load(b) catch return self.fail("keyframe:{s}: {s}", .{ text, b.lastMessage() });
        const before = editor.keys.items.len;
        var parts = std.mem.splitScalar(u8, text, '/');
        const verb = parts.next().?;
        const eql = std.mem.eql;
        var numbers: [3]f32 = undefined;
        var count: usize = 0;
        while (parts.next()) |piece| : (count += 1) {
            if (count == numbers.len) return self.fail("keyframe:{s}: too many arguments", .{text});
            numbers[count] = std.fmt.parseFloat(f32, piece) catch return self.fail("keyframe:{s}: '{s}' is not a number", .{ text, piece });
        }
        const doc = &self.life.doc;
        const history = &self.life.history;
        if (eql(u8, verb, "add")) {
            if (count != 2) return self.fail("keyframe:add needs <x>/<y>", .{});
            const at = editor.screenByValue(numbers[0], numbers[1]);
            editor.press(@intFromFloat(at.x), @intFromFloat(at.y)) catch return self.fail("keyframe:{s}: {s}", .{ text, b.lastMessage() });
            if (editor.mode != .drag) return self.fail("keyframe:{s}: the press landed outside the curve's ranges", .{text});
            editor.release(b, doc, history) catch return self.fail("keyframe:{s}: {s}", .{ text, b.lastMessage() });
        } else if (eql(u8, verb, "move")) {
            if (count != 3) return self.fail("keyframe:move needs <i>/<x>/<y>", .{});
            const index: usize = @intFromFloat(numbers[0]);
            if (index >= editor.keys.items.len) return self.fail("keyframe:{s}: the curve has {d} keys", .{ text, before });
            const key = editor.keys.items[index];
            const from = editor.screenByValue(key.x, key.y);
            editor.press(@intFromFloat(from.x), @intFromFloat(from.y)) catch return self.fail("keyframe:{s}: {s}", .{ text, b.lastMessage() });
            if (editor.mode != .drag or editor.drag_index != index) return self.fail("keyframe:{s}: the press grabbed key {d}", .{ text, editor.drag_index });
            const to = editor.screenByValue(numbers[1], numbers[2]);
            editor.move(@intFromFloat(to.x), @intFromFloat(to.y));
            editor.release(b, doc, history) catch return self.fail("keyframe:{s}: {s}", .{ text, b.lastMessage() });
        } else if (eql(u8, verb, "delete")) {
            if (count != 1) return self.fail("keyframe:delete needs <i>", .{});
            editor.drag_index = @intFromFloat(numbers[0]);
            const went = editor.deleteActive(b, doc, history) catch return self.fail("keyframe:{s}: {s}", .{ text, b.lastMessage() });
            if (!went) return self.fail("keyframe:{s}: nothing was deleted (key 0 is protected)", .{text});
        } else if (eql(u8, verb, "reset")) {
            const changed = editor.resetAll(b, doc, history) catch return self.fail("keyframe:reset: {s}", .{b.lastMessage()});
            if (!changed) return self.fail("keyframe:reset: the curve has one key, nothing to reset", .{});
        } else if (eql(u8, verb, "zoomx_in") or eql(u8, verb, "zoomx_out") or eql(u8, verb, "zoomy_in") or eql(u8, verb, "zoomy_out")) {
            const zoom_in = verb[verb.len - 1] == 'n';
            const changed = if (verb[4] == 'x')
                editor.zoomX(if (zoom_in) .in else .out)
            else
                editor.zoomY(if (zoom_in) .in else .out);
            // A curve that resizes to fit (every particle curve) ignores Zoom X, and a
            // level at its end stays: the view is unchanged, and expect=zoom says so.
            if (!changed) std.debug.print("resource-editor: auto: keyframe {s}: the zoom did not change\n", .{verb});
        } else return self.fail("keyframe: unknown verb '{s}'", .{verb});
        // The view after a gesture is the bridge's keys, as the dock reloads it.
        editor.load(b) catch return self.fail("keyframe:{s}: {s}", .{ text, b.lastMessage() });
        std.debug.print("resource-editor: auto: keyframe {s}: {d} keys -> {d}, zoom {d:.0}/{d} px per step\n", .{ text, before, editor.keys.items.len, editor.xs, editor.ys });
        return null;
    }

    // --- Pointer-driven curve gestures -------------------------------------

    /// The Function window as the auto tier places it: tall, so the keys of a
    /// curve are on screen and never scroll.
    const function_window = docks_mod.Rect{ .x = 330, .y = 40, .w = 930, .h = 740 };

    /// One frame of the loop, with the docks once they exist.
    fn drawFrame(self: *Runner) host_mod.HostError!void {
        const selected: ?i32 = if (self.curve) |curve| curve.node else if (self.image_on) self.panels.selection.primary else null;
        const docks: ?*docks_mod.Docks = if (self.docks) |*d| d else null;
        const folder: ?[]const u8 = if (self.image_on) (if (self.life.doc.pathSlice()) |p| std.fs.path.dirname(p) else null) else null;
        try drawFrameWithDocks(self.host, &self.panels, self.gpa, self.bridge(), &self.life, docks, folder, selected);
    }

    fn pumpFrame(self: *Runner, what: []const u8) ?[]const u8 {
        self.drawFrame() catch |err| return self.fail("{s}: frame: {s}: {s}", .{ what, @errorName(err), self.bridge().lastMessage() });
        return null;
    }

    /// An SDL event on the queue the host reads real input from.
    fn push(self: *Runner, event: *sdl3.c.SDL_Event) void {
        _ = self;
        _ = sdl3.c.SDL_PushEvent(event);
    }

    fn windowId(self: *Runner) sdl3.c.SDL_WindowID {
        return sdl3.c.SDL_GetWindowID(self.host.window);
    }

    fn windowWidth(self: *Runner) f32 {
        var w: c_int = 0;
        var h: c_int = 0;
        _ = sdl3.c.SDL_GetWindowSize(self.host.window, &w, &h);
        return @floatFromInt(@max(1, w));
    }

    fn pushMotion(self: *Runner, x: f32, y: f32) void {
        var event = std.mem.zeroes(sdl3.c.SDL_Event);
        event.motion = .{ .type = sdl3.c.SDL_EVENT_MOUSE_MOTION, .windowID = self.windowId(), .x = x, .y = y };
        self.push(&event);
    }

    fn pushButton(self: *Runner, x: f32, y: f32, down: bool) void {
        var event = std.mem.zeroes(sdl3.c.SDL_Event);
        event.button = .{
            .type = if (down) sdl3.c.SDL_EVENT_MOUSE_BUTTON_DOWN else sdl3.c.SDL_EVENT_MOUSE_BUTTON_UP,
            .windowID = self.windowId(),
            .button = sdl3.c.SDL_BUTTON_LEFT,
            .down = down,
            .clicks = 1,
            .x = x,
            .y = y,
        };
        self.push(&event);
    }

    fn pushKey(self: *Runner, key: sdl3.c.SDL_Keycode, scancode: sdl3.c.SDL_Scancode, mod: sdl3.c.SDL_Keymod, down: bool) void {
        var event = std.mem.zeroes(sdl3.c.SDL_Event);
        event.key = .{
            .type = if (down) sdl3.c.SDL_EVENT_KEY_DOWN else sdl3.c.SDL_EVENT_KEY_UP,
            .windowID = self.windowId(),
            .scancode = scancode,
            .key = key,
            .mod = mod,
            .down = down,
        };
        self.push(&event);
    }

    /// do=function_open: the View menu's Function window, opened by the
    /// Ctrl+F shortcut the way the keyboard does it (Ctrl down, F down, both
    /// up, a frame each), with the docks created on first use.
    fn functionOpen(self: *Runner) ?[]const u8 {
        if (self.curve == null) return self.fail("function_open: no curve was selected (do=curve first)", .{});
        if (self.docks == null) self.docks = docks_mod.Docks.init(self.gpa, self.io, &self.real);
        const docks = &self.docks.?;
        docks.function_override = function_window;
        if (self.pumpFrame("function_open")) |why| return why;
        if (docks.show_function) return self.fail("function_open: the Function window was open already", .{});
        const ctrl = sdl3.c.SDL_KMOD_LCTRL;
        self.pushKey(sdl3.c.SDLK_LCTRL, sdl3.c.SDL_SCANCODE_LCTRL, ctrl, true);
        if (self.pumpFrame("function_open")) |why| return why;
        self.pushKey(sdl3.c.SDLK_F, sdl3.c.SDL_SCANCODE_F, ctrl, true);
        if (self.pumpFrame("function_open")) |why| return why;
        self.pushKey(sdl3.c.SDLK_F, sdl3.c.SDL_SCANCODE_F, ctrl, false);
        self.pushKey(sdl3.c.SDLK_LCTRL, sdl3.c.SDL_SCANCODE_LCTRL, 0, false);
        if (self.pumpFrame("function_open")) |why| return why;
        if (self.pumpFrame("function_open")) |why| return why;
        if (!docks.show_function) return self.fail("function_open: Ctrl+F did not open the Function window", .{});
        const rect = docks.curve_rect orelse return self.fail("function_open: the Function window drew no curve", .{});
        std.debug.print("resource-editor: auto: function window open, curve widget at {d:.0},{d:.0} {d:.0}x{d:.0}\n", .{ rect.x, rect.y, rect.w, rect.h });
        return null;
    }

    // --- Image window ---------------------------------------------------------

    /// The Image window as the auto tier places it: left of the tree window and
    /// clear of the project panel, so no other window takes its pointer events.
    const image_window = docks_mod.Rect{ .x = 330, .y = 40, .w = 530, .h = 440 };

    /// do=image_open: the docks exist and draw the Image window over the open
    /// project; its picture must have loaded (a Mission without one has the
    /// engine make it), and the size it was drawn at is printed.
    fn imageOpen(self: *Runner) ?[]const u8 {
        if (!self.life.is_open or il.Kind.of(self.life.doc.kind) == null) return self.fail("image_open: the project has no image frame (mip, chc, cgc and mdc have)", .{});
        if (self.docks == null) self.docks = docks_mod.Docks.init(self.gpa, self.io, &self.real);
        const docks = &self.docks.?;
        docks.image_override = image_window;
        self.image_on = true;
        var pumped: u32 = 0;
        while (pumped < 3) : (pumped += 1) if (self.pumpFrame("image_open")) |why| return why;
        const frame = &docks.image;
        if (frame.texture == null) return self.fail("image_open: no picture is shown: {s}", .{frame.note[0..frame.note_len]});
        const shown = frame.shown orelse return self.fail("image_open: the picture was not drawn", .{});
        std.debug.print("resource-editor: auto: image window shows a {d}x{d} picture at {d:.0},{d:.0}\n", .{ frame.width, frame.height, shown.x, shown.y });
        return null;
    }

    /// The screen point of a picture pixel, or why the picture is not shown.
    fn imageScreen(self: *Runner, what: []const u8, x: f32, y: f32) union(enum) { at: [2]f32, refused: []const u8 } {
        const docks = if (self.docks) |*d| d else return .{ .refused = self.fail("{s}: the Image window is not open (do=image_open first)", .{what}) };
        const shown = docks.image.shown orelse return .{ .refused = self.fail("{s}: the Image window shows no picture", .{what}) };
        return .{ .at = .{ shown.x + x, shown.y + y } };
    }

    /// The first item of the picture's first list, the one every image verb works on.
    fn imageFirstCross(self: *Runner, what: []const u8) ?struct { kind: il.Kind, list: il.List, point: Point2 } {
        const kind = il.Kind.of(self.life.doc.kind) orelse {
            _ = self.fail("{s}: the project has no image frame", .{what});
            return null;
        };
        var lists: [2]il.List = undefined;
        const found = il.crossLists(&self.life.doc, kind, &lists);
        if (found.len == 0) {
            _ = self.fail("{s}: the project has no list of positions", .{what});
            return null;
        }
        var read = sub_tools.readGeometry(self.bridge(), found[0].node, found[0].channel) catch {
            _ = self.fail("{s}: {s}", .{ what, self.bridge().lastMessage() });
            return null;
        };
        defer read.deinit(self.gpa);
        if (read.points2.len == 0) {
            _ = self.fail("{s}: the first list holds no position", .{what});
            return null;
        }
        return .{ .kind = kind, .list = found[0], .point = read.points2[0] };
    }

    /// do=image_select: the first objective, mission or chapter is the tree's selection.
    fn imageSelect(self: *Runner) ?[]const u8 {
        const kind = il.Kind.of(self.life.doc.kind) orelse return self.fail("image_select: the project has no image frame", .{});
        const item = sub_tools.item_type;
        const class = switch (kind) {
            .mission => item.mission_objective_props,
            .chapter => item.chapter_mission_props,
            .campaign => item.campaign_chapter_props,
            .medal => return self.fail("image_select: a Medal has no positions", .{}),
        };
        const id = sub_tools.firstOfClass(&self.life.doc, class) orelse return self.fail("image_select: the project has no such item", .{});
        self.panels.selection.only(self.gpa, id) catch return self.fail("image_select: out of memory", .{});
        return self.pumpFrame("image_select");
    }

    /// do=image_click:<x>/<y>: press and release on that picture pixel, through the window's pointer path.
    fn imageClick(self: *Runner, arg: []const u8) ?[]const u8 {
        const want = parsePoint(arg) orelse return self.fail("image_click needs <x>/<y>", .{});
        const at = switch (self.imageScreen("image_click", want.x, want.y)) {
            .at => |p| p,
            .refused => |why| return why,
        };
        std.debug.print("resource-editor: auto: image_click at picture {d:.0}/{d:.0}, screen {d:.0}/{d:.0}\n", .{ want.x, want.y, at[0], at[1] });
        return self.gesture("image_click", at, null);
    }

    /// do=image_crosses:<on|off>: the Show crosses checkbox clicked when it is not in that state already.
    fn imageCrosses(self: *Runner, arg: []const u8) ?[]const u8 {
        const want = std.mem.eql(u8, arg, "on");
        if (!want and !std.mem.eql(u8, arg, "off")) return self.fail("image_crosses needs on or off", .{});
        const docks = if (self.docks) |*d| d else return self.fail("image_crosses: the Image window is not open", .{});
        const box = docks.image.crosses_toggle orelse return self.fail("image_crosses: the Image window has no Show crosses checkbox", .{});
        if (self.gesture("image_crosses", .{ box.x + box.w / 2, box.y + box.h / 2 }, null)) |why| return why;
        const mode = if (docks.image.overlay) |o| o.mode else il.Mode.place;
        if ((mode == .drag_crosses) != want) return self.fail("image_crosses:{s}: the checkbox click left the mode at {s}", .{ arg, @tagName(mode) });
        return null;
    }

    /// do=image_drag_cross:<dx>/<dy>: the first cross pressed on its drawn place and dragged by that many
    /// picture pixels over four frames, then released: one gesture, so one undo step.
    fn imageDragCross(self: *Runner, arg: []const u8) ?[]const u8 {
        const delta = parsePoint(arg) orelse return self.fail("image_drag_cross needs <dx>/<dy>", .{});
        const first = self.imageFirstCross("image_drag_cross") orelse return self.failure;
        const from = switch (self.imageScreen("image_drag_cross", first.point.x, first.point.y)) {
            .at => |p| p,
            .refused => |why| return why,
        };
        const to = switch (self.imageScreen("image_drag_cross", first.point.x + delta.x, first.point.y + delta.y)) {
            .at => |p| p,
            .refused => |why| return why,
        };
        std.debug.print("resource-editor: auto: image_drag_cross from {d:.1}/{d:.1} by {d:.0}/{d:.0}\n", .{ first.point.x, first.point.y, delta.x, delta.y });
        return self.gesture("image_drag_cross", from, to);
    }

    /// `<x>/<y>`: the first position of the picture's first list is there, within a pixel.
    fn crossAt(self: *Runner, arg: []const u8) ?[]const u8 {
        const want = parsePoint(arg) orelse return self.fail("expect=cross needs <x>/<y>", .{});
        const first = self.imageFirstCross("expect=cross") orelse return self.failure;
        std.debug.print("resource-editor: auto: first cross is at {d:.1}/{d:.1}, expected {d:.1}/{d:.1}\n", .{ first.point.x, first.point.y, want.x, want.y });
        if (@abs(first.point.x - want.x) > 1.0 or @abs(first.point.y - want.y) > 1.0)
            return self.fail("expect=cross:{s} was false: the cross is at {d:.2}/{d:.2}", .{ arg, first.point.x, first.point.y });
        return null;
    }

    /// The capture and the scale from screen points to its pixels.
    fn loadShot(self: *Runner, what: []const u8, name: []const u8, bytes_out: *[]u8) ?struct { tga: schedule.Tga, scale: f32 } {
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const path = self.shotPath(&path_buffer, name) orelse {
            _ = self.fail("{s}: the path is too long", .{what});
            return null;
        };
        const bytes = readFile(self.io, self.gpa, path) catch |err| {
            _ = self.fail("{s}: {s}: {s}", .{ what, path, @errorName(err) });
            return null;
        };
        const tga = schedule.Tga.parse(bytes) catch {
            self.gpa.free(bytes);
            _ = self.fail("{s}: {s} is not a TGA", .{ what, path });
            return null;
        };
        bytes_out.* = bytes;
        return .{ .tga = tga, .scale = @as(f32, @floatFromInt(tga.width)) / self.windowWidth() };
    }

    /// The cross's colours: the red of a cross and the yellow of the active one.
    fn isMarker(p: [4]u8) bool {
        return p[2] >= 200 and p[0] <= 80;
    }

    /// `<shot>/<x>/<y>/<on|off>`: the 3x3 pixels at that picture pixel hold a cross's colour (on) or none (off).
    /// The centre pixel's colour and the marker count are printed either way, so a failure names the frame and position.
    fn shotMarker(self: *Runner, arg: []const u8) ?[]const u8 {
        var parts = std.mem.splitScalar(u8, arg, '/');
        const shot = parts.next() orelse "";
        const x = std.fmt.parseFloat(f32, parts.next() orelse "") catch return self.fail("shot_marker needs <shot>/<x>/<y>/on|off", .{});
        const y = std.fmt.parseFloat(f32, parts.next() orelse "") catch return self.fail("shot_marker needs <shot>/<x>/<y>/on|off", .{});
        const state = parts.next() orelse "";
        const want_on = std.mem.eql(u8, state, "on");
        if (!want_on and !std.mem.eql(u8, state, "off")) return self.fail("shot_marker needs on or off, not '{s}'", .{state});
        const at = switch (self.imageScreen("shot_marker", x, y)) {
            .at => |p| p,
            .refused => |why| return why,
        };
        var bytes: []u8 = undefined;
        const loaded = self.loadShot("shot_marker", shot, &bytes) orelse return self.failure;
        defer self.gpa.free(bytes);
        const cx: i32 = @intFromFloat(@round(at[0] * loaded.scale));
        const cy: i32 = @intFromFloat(@round(at[1] * loaded.scale));
        if (cx < 1 or cy < 1 or cx + 1 >= loaded.tga.width or cy + 1 >= loaded.tga.height)
            return self.fail("shot_marker:{s}: {d},{d} is outside the {d}x{d} shot", .{ arg, cx, cy, loaded.tga.width, loaded.tga.height });
        var marked: u32 = 0;
        var oy: i32 = -1;
        while (oy <= 1) : (oy += 1) {
            var ox: i32 = -1;
            while (ox <= 1) : (ox += 1) {
                if (isMarker(loaded.tga.pixel(@intCast(cx + ox), @intCast(cy + oy)))) marked += 1;
            }
        }
        const centre = loaded.tga.pixel(@intCast(cx), @intCast(cy));
        std.debug.print("resource-editor: auto: {s} at picture {d:.0}/{d:.0} (pixel {d},{d}): R{d} G{d} B{d}, {d} of 9 pixels are a cross's colour, wanted {s}\n", .{ shot, x, y, cx, cy, centre[2], centre[1], centre[0], marked, state });
        if (want_on and marked == 0) return self.fail("expect=shot_marker:{s} was false: no cross at {d},{d} of {s}", .{ arg, cx, cy, shot });
        if (!want_on and marked != 0) return self.fail("expect=shot_marker:{s} was false: a cross is still at {d},{d} of {s}", .{ arg, cx, cy, shot });
        return null;
    }

    /// `<shot>`: the picture is on screen: its centre differs from the window's own background, sampled just
    /// right of it, by at least 30 over the three channels. Both colours are printed.
    fn shotPicture(self: *Runner, arg: []const u8) ?[]const u8 {
        const docks = if (self.docks) |*d| d else return self.fail("shot_picture: the Image window is not open", .{});
        const shown = docks.image.shown orelse return self.fail("shot_picture: the Image window shows no picture", .{});
        var bytes: []u8 = undefined;
        const loaded = self.loadShot("shot_picture", arg, &bytes) orelse return self.failure;
        defer self.gpa.free(bytes);
        const px: u32 = @intFromFloat(@round((shown.x + shown.w / 2) * loaded.scale));
        const py: u32 = @intFromFloat(@round((shown.y + shown.h / 2) * loaded.scale));
        const bx: u32 = @intFromFloat(@round((shown.x + shown.w + 4) * loaded.scale));
        if (px >= loaded.tga.width or py >= loaded.tga.height or bx >= loaded.tga.width) return self.fail("shot_picture: the picture is outside the {d}x{d} shot", .{ loaded.tga.width, loaded.tga.height });
        const picture = loaded.tga.pixel(px, py);
        const ground = loaded.tga.pixel(bx, py);
        var contrast: u32 = 0;
        for (0..3) |i| contrast += @abs(@as(i32, picture[i]) - @as(i32, ground[i]));
        std.debug.print("resource-editor: auto: {s}: picture centre R{d} G{d} B{d}, background R{d} G{d} B{d}, contrast {d}\n", .{ arg, picture[2], picture[1], picture[0], ground[2], ground[1], ground[0], contrast });
        if (contrast < 30) return self.fail("expect=shot_picture:{s} was false: the picture's centre differs from the background by only {d}", .{ arg, contrast });
        return null;
    }

    /// `<shot>`: the Image window shows the project's map_h.dds, not the map.tga beside it (CMissionFrame). The
    /// path the frame decoded must end in map_h.dds; then 16 x 16 picture pixels of the shot are compared with that
    /// file decoded by the engine and with map.tga decoded the same way. The mean differences and the counts of samples
    /// within 12 of each file are printed; the shot must match map_h.dds and map.tga clearly less.
    fn shotMinimap(self: *Runner, arg: []const u8) ?[]const u8 {
        const docks = if (self.docks) |*d| d else return self.fail("shot_minimap: the Image window is not open", .{});
        const shown = docks.image.shown orelse return self.fail("shot_minimap: the Image window shows no picture", .{});
        const loaded_path = docks.image.key[0..docks.image.key_len];
        if (!std.mem.endsWith(u8, loaded_path, "/map_h.dds")) return self.fail("expect=shot_minimap:{s} was false: the frame decoded {s}, not map_h.dds", .{ arg, loaded_path });
        var bytes: []u8 = undefined;
        const loaded = self.loadShot("shot_minimap", arg, &bytes) orelse return self.failure;
        defer self.gpa.free(bytes);
        const side: usize = @intCast(il.max_side);
        const hd = self.gpa.alloc(u8, side * side * 4) catch return self.fail("shot_minimap: out of memory", .{});
        defer self.gpa.free(hd);
        const tga = self.gpa.alloc(u8, side * side * 4) catch return self.fail("shot_minimap: out of memory", .{});
        defer self.gpa.free(tga);
        var hd_w: c_int = 0;
        var hd_h: c_int = 0;
        var tga_w: c_int = 0;
        var tga_h: c_int = 0;
        const session = self.real.session;
        var dds_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const dds = std.fmt.bufPrintZ(&dds_buffer, "{s}", .{loaded_path}) catch return self.fail("shot_minimap: the path is too long", .{});
        if (c.BkEditorMinimapImage(session, dds.ptr, hd.ptr, @intCast(hd.len), il.max_side, &hd_w, &hd_h) != c.BK_EDITOR_OK) return self.fail("shot_minimap: map_h.dds does not decode: {s}", .{self.bridge().lastMessage()});
        var xml_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const stem = loaded_path[0 .. loaded_path.len - "_h.dds".len];
        const xml = std.fmt.bufPrintZ(&xml_buffer, "{s}.xml", .{stem}) catch return self.fail("shot_minimap: the path is too long", .{});
        if (c.BkEditorMinimapImage(session, xml.ptr, tga.ptr, @intCast(tga.len), il.max_side, &tga_w, &tga_h) != c.BK_EDITOR_OK) return self.fail("shot_minimap: map.tga does not decode: {s}", .{self.bridge().lastMessage()});
        if (hd_w != tga_w or hd_h != tga_h) std.debug.print("resource-editor: auto: {s}: map_h.dds is {d}x{d}, map.tga is {d}x{d}\n", .{ arg, hd_w, hd_h, tga_w, tga_h });
        var diff_hd: u64 = 0;
        var diff_tga: u64 = 0;
        var samples: u64 = 0;
        var near_hd: u32 = 0;
        var near_tga: u32 = 0;
        for (0..16) |gy| {
            for (0..16) |gx| {
                const ix: usize = @min(@as(usize, @intCast(hd_w)) - 1, (gx * 2 + 1) * @as(usize, @intCast(hd_w)) / 32);
                const iy: usize = @min(@as(usize, @intCast(hd_h)) - 1, (gy * 2 + 1) * @as(usize, @intCast(hd_h)) / 32);
                const px: u32 = @intFromFloat(@round((shown.x + @as(f32, @floatFromInt(ix)) + 0.5) * loaded.scale));
                const py: u32 = @intFromFloat(@round((shown.y + @as(f32, @floatFromInt(iy)) + 0.5) * loaded.scale));
                if (px >= loaded.tga.width or py >= loaded.tga.height) return self.fail("shot_minimap: picture pixel {d},{d} is outside the shot", .{ ix, iy });
                const seen = loaded.tga.pixel(px, py);
                const hd_at = (iy * @as(usize, @intCast(hd_w)) + ix) * 4;
                const tx: usize = @min(@as(usize, @intCast(tga_w)) - 1, ix);
                const ty: usize = @min(@as(usize, @intCast(tga_h)) - 1, iy);
                const tga_at = (ty * @as(usize, @intCast(tga_w)) + tx) * 4;
                // The shot's channels are B, G, R; the decoder's are R, G, B.
                var point_hd: u32 = 0;
                var point_tga: u32 = 0;
                for (0..3) |ch| {
                    point_hd += @abs(@as(i32, seen[2 - ch]) - @as(i32, hd[hd_at + ch]));
                    point_tga += @abs(@as(i32, seen[2 - ch]) - @as(i32, tga[tga_at + ch]));
                }
                diff_hd += point_hd;
                diff_tga += point_tga;
                if (point_hd <= 3 * 12) near_hd += 1;
                if (point_tga <= 3 * 12) near_tga += 1;
                samples += 3;
            }
        }
        const mean_hd = @as(f64, @floatFromInt(diff_hd)) / @as(f64, @floatFromInt(samples));
        const mean_tga = @as(f64, @floatFromInt(diff_tga)) / @as(f64, @floatFromInt(samples));
        std.debug.print("resource-editor: auto: {s}: the picture shown is {s} ({d}x{d}); mean channel difference from map_h.dds {d:.2}, from map.tga {d:.2}; samples within 12 of map_h.dds {d}/256, of map.tga {d}/256\n", .{ arg, loaded_path, hd_w, hd_h, mean_hd, mean_tga, near_hd, near_tga });
        // The window may clip the picture's edge, so only samples that lie in view can agree: most must match map_h.dds
        // and no more than a quarter as many may match map.tga.
        if (near_hd * 2 < 256) return self.fail("expect=shot_minimap:{s} was false: only {d} of 256 samples match map_h.dds", .{ arg, near_hd });
        if (near_tga * 4 > near_hd) return self.fail("expect=shot_minimap:{s} was false: {d} samples match map.tga against {d} for map_h.dds", .{ arg, near_tga, near_hd });
        return null;
    }

    /// The screen point of a curve value in the displayed widget, or why it is
    /// not on it. Frames are drawn first so the widget shows the stored keys.
    fn aim(self: *Runner, what: []const u8, x: f32, y: f32) union(enum) { at: struct { x: f32, y: f32 }, refused: []const u8 } {
        const docks = if (self.docks) |*d| d else return .{ .refused = self.fail("{s}: the Function window is not open (do=function_open first)", .{what}) };
        const rect = docks.curve_rect orelse return .{ .refused = self.fail("{s}: the Function window shows no curve", .{what}) };
        const at = docks.curveScreen(x, y) orelse return .{ .refused = self.fail("{s}: the widget has no curve", .{what}) };
        // The ranges' margins (left, bottom) are the widget's own axes.
        if (at.x < rect.x + @as(f32, @floatFromInt(keyframe.left)) or at.x > rect.x + rect.w or at.y < rect.y or at.y > rect.y + rect.h - @as(f32, @floatFromInt(keyframe.bottom)))
            return .{ .refused = self.fail("{s}: value {d:.3}/{d:.1} is at {d:.0},{d:.0}, off the widget {d:.0},{d:.0} {d:.0}x{d:.0}", .{ what, x, y, at.x, at.y, rect.x, rect.y, rect.w, rect.h }) };
        return .{ .at = .{ .x = at.x, .y = at.y } };
    }

    /// The widget's value per pixel, the tolerance of a gesture that lands on a
    /// whole pixel: one pixel in x and in y plus the float noise.
    fn pixelTolerance(self: *Runner) [2]f32 {
        const e = self.docks.?.curve.?;
        return .{ e.knobs.step_x / e.xs + 0.001, e.knobs.step_y / @as(f32, @floatFromInt(e.ys)) + 0.001 };
    }

    /// The stored keys, through the bridge; the caller frees.
    fn storedKeys(self: *Runner, what: []const u8) ?[]core.bridge.Vec3 {
        const node = self.curve.?.node;
        var read = sub_tools.readGeometry(self.bridge(), node, .particle_keyframes) catch {
            _ = self.fail("{s}: {s}", .{ what, self.bridge().lastMessage() });
            return null;
        };
        defer read.deinit(self.gpa);
        return self.gpa.dupe(core.bridge.Vec3, read.vec3) catch {
            _ = self.fail("{s}: out of memory", .{what});
            return null;
        };
    }

    fn printKeys(label: []const u8, keys: []const core.bridge.Vec3) void {
        std.debug.print("resource-editor: auto:   {s}: {d} keys", .{ label, keys.len });
        for (keys, 0..) |k, i| std.debug.print("{s}{d}={d:.3}/{d:.1}", .{ if (i == 0) " " else ", ", i, k.x, k.y });
        std.debug.print("\n", .{});
    }

    fn sameKeys(a: []const core.bridge.Vec3, b: []const core.bridge.Vec3, tolerance: [2]f32) bool {
        if (a.len != b.len) return false;
        for (a, b) |p, q| {
            if (@abs(p.x - q.x) > tolerance[0] or @abs(p.y - q.y) > tolerance[1]) return false;
        }
        return true;
    }

    /// One press, optional move steps and a release, a frame each, so ImGui
    /// sees down, move and up on separate frames.
    fn gesture(self: *Runner, what: []const u8, from: [2]f32, to: ?[2]f32) ?[]const u8 {
        self.pushMotion(from[0], from[1]);
        if (self.pumpFrame(what)) |why| return why;
        self.pushButton(from[0], from[1], true);
        if (self.pumpFrame(what)) |why| return why;
        if (to) |end| {
            const steps = 4;
            var i: u32 = 1;
            while (i <= steps) : (i += 1) {
                const t = @as(f32, @floatFromInt(i)) / steps;
                self.pushMotion(from[0] + (end[0] - from[0]) * t, from[1] + (end[1] - from[1]) * t);
                if (self.pumpFrame(what)) |why| return why;
            }
        }
        const last = to orelse from;
        self.pushButton(last[0], last[1], false);
        if (self.pumpFrame(what)) |why| return why;
        return self.pumpFrame(what);
    }

    /// After a gesture: the stored keys must equal `want` (within the pixel
    /// tolerance), then undo returns `before`, redo returns `want` again, each
    /// read back through the bridge and printed.
    fn gestureUndoRedo(self: *Runner, what: []const u8, before: []const core.bridge.Vec3, want: []const core.bridge.Vec3, tolerance: [2]f32) ?[]const u8 {
        const b = self.bridge();
        const after = self.storedKeys(what) orelse return self.failure;
        defer self.gpa.free(after);
        printKeys("after the gesture", after);
        if (!sameKeys(after, want, tolerance)) return self.fail("{s}: the stored keys are not the expected ones (within {d:.4} x, {d:.3} y)", .{ what, tolerance[0], tolerance[1] });
        const moved_undo = edit.undo(self.target()) catch return self.fail("{s}: undo: {s}", .{ what, b.lastMessage() });
        if (!moved_undo) return self.fail("{s}: nothing to undo after the gesture", .{what});
        if (self.pumpFrame(what)) |why| return why;
        const undone = self.storedKeys(what) orelse return self.failure;
        defer self.gpa.free(undone);
        printKeys("after undo", undone);
        if (!sameKeys(undone, before, tolerance)) return self.fail("{s}: undo did not restore the keys of before the gesture", .{what});
        const moved_redo = edit.redo(self.target()) catch return self.fail("{s}: redo: {s}", .{ what, b.lastMessage() });
        if (!moved_redo) return self.fail("{s}: nothing to redo", .{what});
        if (self.pumpFrame(what)) |why| return why;
        const redone = self.storedKeys(what) orelse return self.failure;
        defer self.gpa.free(redone);
        printKeys("after redo", redone);
        if (!sameKeys(redone, after, tolerance)) return self.fail("{s}: redo did not return the keys of the gesture", .{what});
        return null;
    }

    /// do=curve_click:<t>/<v>: a click on empty graph space at that value adds
    /// a key there, as the widget does; the stored key is within one pixel.
    fn curveClick(self: *Runner, text: []const u8) ?[]const u8 {
        const want = parsePoint(text) orelse return self.fail("curve_click needs <t>/<v>", .{});
        if (self.pumpFrame("curve_click")) |why| return why;
        const at = switch (self.aim("curve_click", want.x, want.y)) {
            .at => |p| p,
            .refused => |why| return why,
        };
        const before = self.storedKeys("curve_click") orelse return self.failure;
        defer self.gpa.free(before);
        printKeys("before curve_click", before);
        // The press lands on a whole pixel, so a value is within a pixel's worth.
        const tolerance = self.pixelTolerance();
        if (self.gesture("curve_click", .{ at.x, at.y }, null)) |why| return why;
        var expected = std.ArrayList(core.bridge.Vec3).initCapacity(self.gpa, before.len + 1) catch return self.fail("curve_click: out of memory", .{});
        defer expected.deinit(self.gpa);
        const index = for (before, 0..) |k, i| {
            if (k.x > want.x) break i;
        } else before.len;
        expected.appendSlice(self.gpa, before[0..index]) catch unreachable;
        expected.appendAssumeCapacity(.{ .x = want.x, .y = want.y, .z = 0 });
        expected.appendSlice(self.gpa, before[index..]) catch unreachable;
        return self.gestureUndoRedo("curve_click", before, expected.items, tolerance);
    }

    /// do=curve_drag:<i>/<t>/<v>: key i pressed on its displayed handle,
    /// dragged over four frames to that value and released.
    fn curveDrag(self: *Runner, text: []const u8) ?[]const u8 {
        var numbers: [3]f32 = undefined;
        var parts = std.mem.splitScalar(u8, text, '/');
        for (&numbers) |*n| n.* = std.fmt.parseFloat(f32, parts.next() orelse return self.fail("curve_drag needs <i>/<t>/<v>", .{})) catch return self.fail("curve_drag needs <i>/<t>/<v>", .{});
        const index: usize = @intFromFloat(numbers[0]);
        if (self.pumpFrame("curve_drag")) |why| return why;
        const before = self.storedKeys("curve_drag") orelse return self.failure;
        defer self.gpa.free(before);
        printKeys("before curve_drag", before);
        if (index >= before.len) return self.fail("curve_drag: the curve holds {d} keys", .{before.len});
        const from = switch (self.aim("curve_drag", before[index].x, before[index].y)) {
            .at => |p| p,
            .refused => |why| return why,
        };
        const to = switch (self.aim("curve_drag", numbers[1], numbers[2])) {
            .at => |p| p,
            .refused => |why| return why,
        };
        const tolerance = self.pixelTolerance();
        if (self.gesture("curve_drag", .{ from.x, from.y }, .{ to.x, to.y })) |why| return why;
        const expected = self.gpa.dupe(core.bridge.Vec3, before) catch return self.fail("curve_drag: out of memory", .{});
        defer self.gpa.free(expected);
        // Key 0 only moves in y; the others keep x between their neighbours.
        if (index != 0) expected[index].x = numbers[1];
        expected[index].y = numbers[2];
        return self.gestureUndoRedo("curve_drag", before, expected, tolerance);
    }

    /// do=curve_delete:<i>: a click on key i's handle makes it the active
    /// key, then the Delete key goes through the event queue, as MFC's
    /// CKeyFrameEditor takes it. Key 0 is protected.
    fn curveDelete(self: *Runner, text: []const u8) ?[]const u8 {
        const index = std.fmt.parseInt(usize, text, 10) catch return self.fail("curve_delete needs <i>", .{});
        if (index == 0) return self.fail("curve_delete: key 0 is protected", .{});
        if (self.pumpFrame("curve_delete")) |why| return why;
        const first = self.storedKeys("curve_delete") orelse return self.failure;
        defer self.gpa.free(first);
        if (index >= first.len) return self.fail("curve_delete: the curve holds {d} keys", .{first.len});
        const at = switch (self.aim("curve_delete", first[index].x, first[index].y)) {
            .at => |p| p,
            .refused => |why| return why,
        };
        const tolerance = self.pixelTolerance();
        if (self.gesture("curve_delete", .{ at.x, at.y }, null)) |why| return why;
        // The click itself may snap the key's y to its pixel row, so the keys
        // the delete starts from are read after it.
        const before = self.storedKeys("curve_delete") orelse return self.failure;
        defer self.gpa.free(before);
        printKeys("before curve_delete", before);
        if (before.len != first.len) return self.fail("curve_delete: the selecting click changed the key count", .{});
        self.pushKey(sdl3.c.SDLK_DELETE, sdl3.c.SDL_SCANCODE_DELETE, 0, true);
        if (self.pumpFrame("curve_delete")) |why| return why;
        self.pushKey(sdl3.c.SDLK_DELETE, sdl3.c.SDL_SCANCODE_DELETE, 0, false);
        if (self.pumpFrame("curve_delete")) |why| return why;
        const expected = self.gpa.alloc(core.bridge.Vec3, before.len - 1) catch return self.fail("curve_delete: out of memory", .{});
        defer self.gpa.free(expected);
        @memcpy(expected[0..index], before[0..index]);
        @memcpy(expected[index..], before[index + 1 ..]);
        return self.gestureUndoRedo("curve_delete", before, expected, tolerance);
    }

    /// The stored keys of the selected curve, read through the bridge.
    fn curveKeys(self: *Runner, what: []const u8) ?core.bridge.GeometryValue {
        const editor = if (self.curve) |*e| e else {
            _ = self.fail("expect={s}: no curve was selected (do=curve first)", .{what});
            return null;
        };
        return sub_tools.readGeometry(self.bridge(), editor.node, .particle_keyframes) catch {
            _ = self.fail("expect={s}: {s}", .{ what, self.bridge().lastMessage() });
            return null;
        };
    }

    /// The right-click of the unit preview at the screen point of the named
    /// locator: the pick goes through the same mesh_logic entry the panel's
    /// click does, so the name only chooses where to click.
    fn pickLocator(self: *Runner, wanted: []const u8) ?[]const u8 {
        const b = self.bridge();
        var buffer: [mesh.locator_capacity]core.bridge.MeshLocator = undefined;
        const markers = mesh.readLocators(b, &buffer) catch return self.fail("pick_locator:{s}: {s}", .{ wanted, b.lastMessage() });
        var at: ?Point2 = null;
        for (markers) |marker| {
            if (std.ascii.eqlIgnoreCase(marker.nameSlice(), wanted)) at = .{ .x = marker.sx, .y = marker.sy };
        }
        const point = at orelse return self.fail("pick_locator: the preview has no locator named {s} among {d}", .{ wanted, markers.len });
        const pick = mesh.pickAndSelect(self.gpa, &self.life.doc, &self.panels.selection, markers, point) catch return self.fail("pick_locator:{s}: out of memory", .{wanted});
        switch (pick) {
            .node => return null,
            .miss => |nearest| {
                var text: [160]u8 = undefined;
                return self.fail("pick_locator:{s}: {s}", .{ wanted, mesh.missText(&text, point, nearest) });
            },
        }
    }

    /// The last sprite or infantry frame item in tree order.
    fn lastFrame(self: *Runner) ?i32 {
        var found: ?i32 = null;
        for (self.life.doc.tree.nodes.items) |*node| {
            if (sub_tools.isClass(node, sub_tools.item_type.sprite_props) or sub_tools.isClass(node, sub_tools.item_type.unit_frame_props)) found = node.id;
        }
        return found;
    }

    fn firstFormation(self: *Runner) ?i32 {
        return sub_tools.firstOfClass(&self.life.doc, sub_tools.item_type.squad_formation_props);
    }

    /// The tree action on the first node it applies to, through the same
    /// squad_logic entry the tree's context menu uses.
    fn treeAction(self: *Runner, arg: []const u8) ?[]const u8 {
        const item = sub_tools.item_type;
        const Choice = struct { action: squad.TreeAction, class: i32 };
        const choice: Choice = if (std.mem.eql(u8, arg, "add_shoot_type"))
            .{ .action = .add_shoot_type, .class = item.weapon_shoot_types }
        else if (std.mem.eql(u8, arg, "add_crater"))
            .{ .action = .add_crater, .class = item.weapon_damage_props }
        else if (std.mem.eql(u8, arg, "add_source"))
            .{ .action = .add_source, .class = item.trench_sources }
        else
            return self.fail("tree: '{s}' is not add_shoot_type, add_crater or add_source", .{arg});
        const node = sub_tools.firstOfClass(&self.life.doc, choice.class) orelse return self.fail("tree:{s}: the project has no such node", .{arg});
        squad.runTreeAction(self.gpa, self.bridge(), &self.life.doc, &self.life.history, choice.action, node) catch return self.fail("tree:{s}: {s}", .{ arg, self.bridge().lastMessage() });
        return null;
    }

    /// One member dragged by a world offset: the same FormationDrag the
    /// overlay feeds, so it is one undo step.
    fn squadDrag(self: *Runner, arg: []const u8) ?[]const u8 {
        const b = self.bridge();
        var parts = std.mem.splitScalar(u8, arg, '/');
        const slot = std.fmt.parseInt(usize, parts.next() orelse "", 10) catch return self.fail("squad_drag needs <slot>/<dx>/<dy>", .{});
        const dx = std.fmt.parseFloat(f32, parts.next() orelse "") catch return self.fail("squad_drag needs <slot>/<dx>/<dy>", .{});
        const dy = std.fmt.parseFloat(f32, parts.next() orelse "") catch return self.fail("squad_drag needs <slot>/<dx>/<dy>", .{});
        const node = self.firstFormation() orelse return self.fail("squad_drag: the project has no formation", .{});
        var drag = sub_tools.FormationDrag.begin(self.gpa, b, node) catch return self.fail("squad_drag: {s}", .{b.lastMessage()});
        if (slot >= drag.current.len) {
            const count = drag.current.len;
            drag.deinit(self.gpa);
            return self.fail("squad_drag: slot {d} of {d}", .{ slot, count });
        }
        const home = drag.current[slot];
        drag.moveSlot(b, slot, .{ .x = home.x + dx, .y = home.y + dy }) catch {
            drag.cancel(self.gpa, b);
            return self.fail("squad_drag: {s}", .{b.lastMessage()});
        };
        const step = drag.finish(self.gpa) orelse return self.fail("squad_drag: the member did not move", .{});
        sub_tools.commit(self.gpa, b, &self.life.doc, &self.life.history, step, 0) catch return self.fail("squad_drag: {s}", .{b.lastMessage()});
        self.dragged = .{ .formation = node, .slot = slot, .home = home };
        return null;
    }

    /// The overlay's arrow gesture at a world point: the point goes through the
    /// overlay's view to the screen (world +Y is up there), so the angle goes
    /// through the same toWorld, arrowAngle and setFormationDirection a mouse
    /// drag uses.
    fn squadArrow(self: *Runner, arg: []const u8) ?[]const u8 {
        const b = self.bridge();
        const world = parsePoint(arg) orelse return self.fail("squad_arrow needs <x>/<y>", .{});
        const node = self.firstFormation() orelse return self.fail("squad_arrow: the project has no formation", .{});
        var overlay = squad.Overlay.init(self.gpa, node);
        const at = overlay.view.toScreen(world);
        overlay.setMode(b, .direction);
        overlay.press(b, at) catch return self.fail("squad_arrow: {s}", .{b.lastMessage()});
        overlay.move(b, at) catch {
            overlay.cancel(b);
            return self.fail("squad_arrow: {s}", .{b.lastMessage()});
        };
        overlay.release(b, &self.life.doc, &self.life.history, at) catch return self.fail("squad_arrow: {s}", .{b.lastMessage()});
        return null;
    }

    /// The panels' grid editor of the open Object or Fence, null before the
    /// first frame drawn over such a project.
    fn gridEditor(self: *Runner, verb: []const u8) ?*grid.GridEditor {
        _ = verb;
        if (self.panels.grid_editor) |*editor| return editor;
        return null;
    }

    /// One mouse gesture of a grid tool, press at the centre of tile `from`,
    /// move and release at the centre of `to`, through the editor the panels
    /// feed the mouse to, so it is the same undo step a click makes.
    fn gridStroke(self: *Runner, verb: []const u8, tool: grid.Tool, from: [2]i32, to: [2]i32, erase: bool) ?[]const u8 {
        const b = self.bridge();
        const editor = self.gridEditor(verb) orelse return self.fail("{s}: no grid editor is open (draw a frame after opening an obt or fnc)", .{verb});
        for ([_][2]i32{ from, to }) |tile| {
            if (tile[0] < 0 or tile[1] < 0 or tile[0] >= grid.grid_tiles or tile[1] >= grid.grid_tiles)
                return self.fail("{s}: tile {d}/{d} is outside the {d} x {d} grid", .{ verb, tile[0], tile[1], grid.grid_tiles, grid.grid_tiles });
        }
        editor.setTool(b, tool) catch return self.fail("{s}: the {s} tool is not offered for .{s}", .{ verb, tool.label(), self.life.doc.kind.extension() });
        const start = editor.view.toScreen(grid.tileCentre(from[0], from[1]));
        const end = editor.view.toScreen(grid.tileCentre(to[0], to[1]));
        editor.press(b, start, erase) catch return self.fail("{s}: {s}", .{ verb, b.lastMessage() });
        editor.move(b, end) catch {
            editor.cancel(b);
            return self.fail("{s}: {s}", .{ verb, b.lastMessage() });
        };
        editor.release(b, &self.life.doc, &self.life.history, end) catch return self.fail("{s}: {s}", .{ verb, b.lastMessage() });
        return null;
    }

    /// A click of the family's tool on point `i`: it becomes the active one, so
    /// the next shot draws its cone edges and arrow.
    fn pointSelect(self: *Runner, arg: []const u8) ?[]const u8 {
        const b = self.bridge();
        const editor = self.gridEditor("point_select") orelse return self.fail("point_select: no grid editor is open (draw a frame after opening a bld)", .{});
        const slash = std.mem.indexOfScalar(u8, arg, '/') orelse return self.fail("point_select needs <mode>/<i>", .{});
        const mode = parseMode(arg[0..slash]) orelse return self.fail("point_select: '{s}' is not a point family", .{arg[0..slash]});
        const index = std.fmt.parseInt(usize, arg[slash + 1 ..], 10) catch return self.fail("point_select needs <mode>/<i>", .{});
        var read = sub_tools.readGeometry(b, editor.node, pointChannel(mode)) catch return self.fail("point_select: {s}", .{b.lastMessage()});
        defer read.deinit(self.gpa);
        if (index >= read.aimed.len) return self.fail("point_select: the {s} family has {d} points, not {d}", .{ @tagName(mode), read.aimed.len, index + 1 });
        editor.setTool(b, pointTool(mode)) catch return self.fail("point_select: the tool is not offered for .{s}", .{self.life.doc.kind.extension()});
        const at = editor.view.toScreen(grid.worldToGrid(read.aimed[index].at));
        editor.press(b, at, false) catch return self.fail("point_select: {s}", .{b.lastMessage()});
        editor.release(b, &self.life.doc, &self.life.history, at) catch return self.fail("point_select: {s}", .{b.lastMessage()});
        return null;
    }

    /// The Move point and Angle and cone tools on point `i` of the family the
    /// last point verb chose: press on the point (or on its direction or cone
    /// handle), drag to the place the target asks for, release, the gesture
    /// of the mouse and so one undo step. The angle and cone targets are a
    /// handle position that GridEditor's own angleToward and coneToward turn
    /// back into whole degrees.
    fn pointGesture(self: *Runner, verb: []const u8, part: core.point_tools.Part, arg: []const u8) ?[]const u8 {
        const b = self.bridge();
        const editor = self.gridEditor(verb) orelse return self.fail("{s}: no grid editor is open (draw a frame after opening a bld)", .{verb});
        const slash = std.mem.indexOfScalar(u8, arg, '/') orelse return self.fail("{s} needs <i>/<...>", .{verb});
        const index = std.fmt.parseInt(usize, arg[0..slash], 10) catch return self.fail("{s} needs a point index", .{verb});
        var read = sub_tools.readGeometry(b, editor.node, pointChannel(editor.family)) catch return self.fail("{s}: {s}", .{ verb, b.lastMessage() });
        defer read.deinit(self.gpa);
        if (index >= read.aimed.len) return self.fail("{s}: the {s} family has {d} points, not {d}", .{ verb, @tagName(editor.family), read.aimed.len, index + 1 });
        const point = read.aimed[index];
        const handles = grid.handlesOf(point);
        var end_grid: Point2 = undefined;
        switch (part) {
            .move => {
                const p = parseInts(2, arg[slash + 1 ..]) orelse return self.fail("point_move needs <i>/<x>/<y>", .{});
                if (p[0] < 0 or p[1] < 0 or p[0] >= grid.grid_tiles or p[1] >= grid.grid_tiles) return self.fail("point_move: tile {d}/{d} is outside the grid", .{ p[0], p[1] });
                end_grid = grid.tileCentre(p[0], p[1]);
            },
            .direction, .cone => {
                const deg = std.fmt.parseFloat(f32, arg[slash + 1 ..]) catch return self.fail("{s} needs <i>/<degrees>", .{verb});
                const angle: f32 = @floatFromInt(point.angle);
                // The cone handle sits half the cone off the direction; the pointer's offset is half the cone asked for.
                const toward = if (part == .direction) deg else angle + deg / 2;
                end_grid = grid.worldToGrid(grid.aimTip(point.at, toward));
            },
            .horizontal, .aim => return self.fail("{s}: not a scripted gesture", .{verb}),
        }
        const start_grid = switch (part) {
            .move => handles.origin,
            .direction => handles.direction,
            .cone => handles.cone_plus,
            .horizontal, .aim => unreachable,
        };
        // The angle tool grabs a handle of the active point only: select it by a click on the point first.
        editor.setTool(b, if (part == .move) .move_point else .angle) catch return self.fail("{s}: the tool is not offered for .{s}", .{ verb, self.life.doc.kind.extension() });
        if (part != .move) {
            const click = editor.view.toScreen(handles.origin);
            editor.press(b, click, false) catch return self.fail("{s}: {s}", .{ verb, b.lastMessage() });
            editor.release(b, &self.life.doc, &self.life.history, click) catch return self.fail("{s}: {s}", .{ verb, b.lastMessage() });
        }
        const start = editor.view.toScreen(start_grid);
        const end = editor.view.toScreen(end_grid);
        editor.press(b, start, false) catch return self.fail("{s}: {s}", .{ verb, b.lastMessage() });
        editor.move(b, end) catch {
            editor.cancel(b);
            return self.fail("{s}: {s}", .{ verb, b.lastMessage() });
        };
        editor.release(b, &self.life.doc, &self.life.history, end) catch return self.fail("{s}: {s}", .{ verb, b.lastMessage() });
        return null;
    }

    /// The Move tool: press on the sprite's own spot, drag by a grid offset.
    fn spriteMove(self: *Runner, arg: []const u8) ?[]const u8 {
        const b = self.bridge();
        const offset = parsePoint(arg) orelse return self.fail("sprite_move needs <dx>/<dy>", .{});
        const editor = self.gridEditor("sprite_move") orelse return self.fail("sprite_move: no grid editor is open (draw a frame after opening an obt or fnc)", .{});
        editor.setTool(b, .move) catch return self.fail("sprite_move: the Move tool is not offered", .{});
        const home = (sub_tools.readGeometry(b, editor.node, .sprite_pos) catch return self.fail("sprite_move: {s}", .{b.lastMessage()})).point2;
        if (self.sprite_home == null) self.sprite_home = home;
        const at = grid.worldToGrid(home);
        const start = editor.view.toScreen(at);
        const end = editor.view.toScreen(.{ .x = at.x + offset.x, .y = at.y + offset.y });
        editor.press(b, start, false) catch return self.fail("sprite_move: {s}", .{b.lastMessage()});
        editor.move(b, end) catch {
            editor.cancel(b);
            return self.fail("sprite_move: {s}", .{b.lastMessage()});
        };
        editor.release(b, &self.life.doc, &self.life.history, end) catch return self.fail("sprite_move: {s}", .{b.lastMessage()});
        return null;
    }

    /// The stored byte of a tile in the passability or transparency grid.
    fn cellValue(self: *Runner, comptime transparency: bool, x: i32, y: i32) ?u8 {
        const editor = self.gridEditor("cell") orelse return null;
        const channel = if (transparency) editor.registration.transparency orelse return null else editor.registration.passability;
        var read = sub_tools.readGeometry(self.bridge(), editor.gridNode() orelse return null, channel) catch return null;
        defer read.deinit(self.gpa);
        const width: i32 = @intCast(read.bytes_grid.width);
        if (x < 0 or y < 0) return null;
        // The grid grows as tiles are painted, so a tile past its end holds 0.
        if (x >= width) return 0;
        const index: usize = @intCast(y * width + x);
        if (index >= read.bytes_grid.bytes.len) return 0;
        return read.bytes_grid.bytes[index];
    }

    /// `<path>` with `want` 0 or 1: the ComplexParticleSource flag of the exported particle
    /// file, the one the game's reader takes as the source's kind. Printed
    /// with the mode the bridge reads, whether the two agree or not.
    fn exportFlag(self: *Runner, arg: []const u8, want: []const u8) ?[]const u8 {
        var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const path = self.expand(&buffer, arg) orelse return self.fail("the path is too long", .{});
        const bytes = readFile(self.io, self.gpa, path) catch |err| return self.fail("expect=export_complex|export_simple: {s}: {s}", .{ path, @errorName(err) });
        defer self.gpa.free(bytes);
        const key = "ComplexParticleSource=\"";
        const at = std.mem.indexOf(u8, bytes, key) orelse return self.fail("expect=export_complex|export_simple: {s} has no ComplexParticleSource", .{path});
        const value = bytes[at + key.len ..];
        const end = std.mem.indexOfScalar(u8, value, '"') orelse value.len;
        const bridge_mode = docks_logic.SourceToggle.mode(self.bridge());
        std.debug.print("resource-editor: auto: exported ComplexParticleSource={s}, bridge mode {s}\n", .{ value[0..end], if (bridge_mode == null) "unreadable" else if (bridge_mode.?) "complex" else "simple" });
        if (!std.mem.eql(u8, value[0..end], want)) return self.fail("expect=export_complex|export_simple:{s} was false: the file says {s}", .{ arg, value[0..end] });
        return null;
    }

    /// Pixels of a captured shot that are exactly `rgb`, alpha ignored.
    fn shotColourCount(self: *Runner, name: []const u8, rgb: u32) ?usize {
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const path = self.shotPath(&path_buffer, name) orelse return null;
        const bytes = readFile(self.io, self.gpa, path) catch return null;
        defer self.gpa.free(bytes);
        const tga = schedule.Tga.parse(bytes) catch return null;
        var count: usize = 0;
        var y: u32 = 0;
        while (y < tga.height) : (y += 1) {
            var x: u32 = 0;
            while (x < tga.width) : (x += 1) {
                const p = tga.pixel(x, y);
                if (p[2] == (rgb >> 16 & 0xff) and p[1] == (rgb >> 8 & 0xff) and p[0] == (rgb & 0xff)) count += 1;
            }
        }
        return count;
    }

    /// `<shot>/<RRGGBB>/min|max/<n>`: the shot's count of that exact colour
    /// against a bound. The count is printed whether the bound holds or not.
    fn shotColour(self: *Runner, arg: []const u8) ?[]const u8 {
        var parts = std.mem.splitScalar(u8, arg, '/');
        const shot = parts.next() orelse "";
        const hex = parts.next() orelse "";
        const bound = parts.next() orelse "";
        const limit_text = parts.next() orelse "";
        const rgb = std.fmt.parseInt(u32, hex, 16) catch return self.fail("shot_colour needs <shot>/<RRGGBB>/min|max/<n>", .{});
        const limit = std.fmt.parseInt(usize, limit_text, 10) catch return self.fail("shot_colour needs <shot>/<RRGGBB>/min|max/<n>", .{});
        const is_min = std.mem.eql(u8, bound, "min");
        if (!is_min and !std.mem.eql(u8, bound, "max")) return self.fail("shot_colour needs min or max, not '{s}'", .{bound});
        const count = self.shotColourCount(shot, rgb) orelse return self.fail("expect=shot_colour:{s}: the shot could not be read", .{arg});
        std.debug.print("resource-editor: auto: {s} has {d} pixels of #{s} ({s} {d})\n", .{ shot, count, hex, bound, limit });
        if (is_min and count < limit) return self.fail("expect=shot_colour:{s} was false: #{s} fills {d} pixels, at least {d} wanted", .{ arg, hex, count, limit });
        if (!is_min and count > limit) return self.fail("expect=shot_colour:{s} was false: #{s} fills {d} pixels, at most {d} wanted", .{ arg, hex, count, limit });
        return null;
    }

    /// `<shot>/<i>`: the displayed handle of stored key i is in the shot. The
    /// 5x5 pixels at the handle's drawn place (the widget's own key-to-pixel
    /// mapping, scaled to the capture) must hold one colour, at least 20 of
    /// them, and that colour must not be the widget's background, sampled at
    /// its far corner. The counts are printed.
    fn shotCurveHandle(self: *Runner, arg: []const u8) ?[]const u8 {
        const slash = std.mem.indexOfScalar(u8, arg, '/') orelse return self.fail("shot_curve_handle needs <shot>/<i>", .{});
        const index = std.fmt.parseInt(usize, arg[slash + 1 ..], 10) catch return self.fail("shot_curve_handle needs <shot>/<i>", .{});
        const docks = if (self.docks) |*d| d else return self.fail("shot_curve_handle: the Function window is not open", .{});
        const rect = docks.curve_rect orelse return self.fail("shot_curve_handle: the Function window shows no curve", .{});
        const keys = self.storedKeys("shot_curve_handle") orelse return self.failure;
        defer self.gpa.free(keys);
        if (index >= keys.len) return self.fail("shot_curve_handle: the curve holds {d} keys", .{keys.len});
        const at = docks.curveScreen(keys[index].x, keys[index].y) orelse return self.fail("shot_curve_handle: the widget has no curve", .{});
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const path = self.shotPath(&path_buffer, arg[0..slash]) orelse return self.fail("shot_curve_handle: the path is too long", .{});
        const bytes = readFile(self.io, self.gpa, path) catch |err| return self.fail("shot_curve_handle: {s}: {s}", .{ path, @errorName(err) });
        defer self.gpa.free(bytes);
        const tga = schedule.Tga.parse(bytes) catch return self.fail("shot_curve_handle: {s} is not a TGA", .{path});
        const scale = @as(f32, @floatFromInt(tga.width)) / self.windowWidth();
        const cx: i32 = @intFromFloat(@round(at.x * scale));
        const cy: i32 = @intFromFloat(@round(at.y * scale));
        const bx: i32 = @intFromFloat((rect.x + rect.w - 12) * scale);
        const by: i32 = @intFromFloat((rect.y + 12) * scale);
        if (cx < 2 or cy < 2 or cx + 2 >= tga.width or cy + 2 >= tga.height or bx >= tga.width or by >= tga.height)
            return self.fail("shot_curve_handle: key {d} at {d},{d} is outside the {d}x{d} shot", .{ index, cx, cy, tga.width, tga.height });
        const background = tga.pixel(@intCast(bx), @intCast(by));
        // The capture is of the swapchain image, so the handle may sit a pixel or two from the computed place:
        // the best 5x5 block within 4 pixels of it counts.
        var same: u32 = 0;
        var contrast: u32 = 0;
        var best_dx: i32 = 0;
        var best_dy: i32 = 0;
        var oy: i32 = -4;
        while (oy <= 4) : (oy += 1) {
            var ox: i32 = -4;
            while (ox <= 4) : (ox += 1) {
                const mx = cx + ox;
                const my = cy + oy;
                if (mx < 2 or my < 2 or mx + 2 >= tga.width or my + 2 >= tga.height) continue;
                const centre = tga.pixel(@intCast(mx), @intCast(my));
                var count: u32 = 0;
                var dy: i32 = -2;
                while (dy <= 2) : (dy += 1) {
                    var dx: i32 = -2;
                    while (dx <= 2) : (dx += 1) {
                        const p = tga.pixel(@intCast(mx + dx), @intCast(my + dy));
                        if (std.mem.eql(u8, &p, &centre)) count += 1;
                    }
                }
                var diff: u32 = 0;
                for (0..3) |i| diff = @max(diff, @abs(@as(i32, centre[i]) - @as(i32, background[i])));
                if (diff >= 30 and count > same) {
                    same = count;
                    contrast = diff;
                    best_dx = ox;
                    best_dy = oy;
                }
            }
        }
        std.debug.print("resource-editor: auto: {s}: key {d} handle at {d},{d} (found {d},{d} away, scale {d:.2}): {d}/25 pixels of one colour (min 20), contrast {d} against the background (min 30)\n", .{ arg[0..slash], index, cx, cy, best_dx, best_dy, scale, same, contrast });
        if (same < 20) return self.fail("expect=shot_curve_handle:{s} was false: only {d} of the 25 pixels at the handle are one colour", .{ arg, same });
        if (contrast < 30) return self.fail("expect=shot_curve_handle:{s} was false: the handle's colour differs from the background by only {d}", .{ arg, contrast });
        return null;
    }

    fn runGame(self: *Runner) ?[]const u8 {
        const b = self.bridge();
        var mod: core.bridge.ModSettings = .{};
        if (b.modSettingsGet(&mod) != .ok) return self.fail("MOD settings: {s}", .{b.lastMessage()});
        const folder = tools.modFolderForGame(mod.exportDirSlice(), self.base_root) orelse
            return self.fail("the export folder {s} is not directly in {s}mods", .{ mod.exportDirSlice(), self.base_root });
        var installed_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const installed = testlaunch.gamePath(self.io, &installed_buffer) catch |err| return self.fail("the Game beside the editor: {s}", .{@errorName(err)});
        var log_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const log = std.fmt.bufPrint(&log_buffer, "{s}{c}testgame.log", .{ self.dir, std.fs.path.sep }) catch return self.fail("the log path is too long", .{});
        // BK_AUTO_UI: a shot, then exit, the way map-editor-auto's game ends.
        const environment = [_][2][]const u8{ .{ "BK_AUTO_UI", "400:shot,440:exit" }, .{ "BK_NO_HELP", "1" }, .{ "BK_AUDIO_NULL", "1" } };
        self.running = testlaunch.start(self.gpa, self.io, self.environ, .{
            .game_path = installed,
            .mod_folder = folder,
            .log_path = log,
            .profile = tools.test_profile,
            .map_name = null,
            .extra_env = &environment,
        }) catch |err| return self.fail("{s} did not start: {s}", .{ installed, @errorName(err) });
        std.debug.print("resource-editor: auto: Run Blitzkrieg: {s} -mod={s} (log {s})\n", .{ installed, folder, log });
        return null;
    }

    fn predicate(self: *Runner, named: schedule.Named) ?[]const u8 {
        const eql = std.mem.eql;
        const name = named.name;
        const arg = named.arg;
        var buffer: [logic.path_capacity]u8 = undefined;
        if (eql(u8, name, "kind")) {
            const kind = logic.kindFromExtension(arg) orelse return self.fail("kind: '{s}' is not a project extension", .{arg});
            if (!self.life.is_open) return self.fail("expect=kind:{s} was false: no project is open", .{arg});
            if (self.life.doc.kind != kind) return self.fail("expect=kind:{s} was false: the project is {s}", .{ arg, self.life.doc.kind.extension() });
            return null;
        }
        if (eql(u8, name, "dirty")) {
            const want = eql(u8, arg, "true");
            if (self.life.dirty() != want) return self.fail("expect=dirty:{s} was false", .{arg});
            return null;
        }
        if (eql(u8, name, "untitled")) {
            if (!self.life.is_open or self.life.doc.pathSlice() != null) return self.fail("expect=untitled was false", .{});
            return null;
        }
        if (eql(u8, name, "nodes")) {
            const want = std.fmt.parseInt(usize, arg, 10) catch return self.fail("nodes needs a number", .{});
            if (self.life.doc.tree.nodes.items.len != want) return self.fail("expect=nodes:{d} was false: {d} nodes", .{ want, self.life.doc.tree.nodes.items.len });
            return null;
        }
        if (eql(u8, name, "nodes_min")) {
            const want = std.fmt.parseInt(usize, arg, 10) catch return self.fail("nodes_min needs a number", .{});
            if (self.life.doc.tree.nodes.items.len < want) return self.fail("expect=nodes_min:{d} was false: {d} nodes", .{ want, self.life.doc.tree.nodes.items.len });
            return null;
        }
        if (eql(u8, name, "prop")) {
            const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return self.fail("prop needs <name>=<value>", .{});
            const found = self.findProp(arg[0..eq]) orelse return self.fail("no property named {s}", .{arg[0..eq]});
            const want = self.expand(&buffer, arg[eq + 1 ..]) orelse return self.fail("the value is too long", .{});
            if (!std.mem.eql(u8, found.record.valueSlice(), want)) return self.fail("expect=prop:{s} was false: it is '{s}'", .{ arg, found.record.valueSlice() });
            return null;
        }
        if (eql(u8, name, "exported")) {
            if (!self.exported) return self.fail("expect=exported was false: the export wrote no file", .{});
            return null;
        }
        if (eql(u8, name, "file")) {
            const path = self.expand(&buffer, arg) orelse return self.fail("the path is too long", .{});
            const stat = std.Io.Dir.cwd().statFile(self.io, path, .{}) catch return self.fail("expect=file:{s} was false: it does not exist", .{path});
            if (stat.size == 0) return self.fail("expect=file:{s} was false: it is empty", .{path});
            return null;
        }
        if (eql(u8, name, "shot_lit")) {
            var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
            const path = self.shotPath(&path_buffer, arg) orelse return self.fail("the shot path is too long", .{});
            const bytes = readFile(self.io, self.gpa, path) catch |err| return self.fail("{s}: {s}", .{ path, @errorName(err) });
            defer self.gpa.free(bytes);
            const tga = schedule.Tga.parse(bytes) catch |err| return self.fail("{s}: {s}", .{ path, @errorName(err) });
            const lit = litPercent(tga);
            if (lit <= lit_share_percent) return self.fail("expect=shot_lit:{s} was false: {d:.2}% of the frame is drawn", .{ arg, lit });
            std.debug.print("resource-editor: auto: {s} has {d:.1}% of its frame drawn\n", .{ arg, lit });
            return null;
        }
        if (eql(u8, name, "shot_same")) {
            const slash = std.mem.indexOfScalar(u8, arg, '/') orelse return self.fail("shot_same needs <a>/<b>", .{});
            // The frames may not differ in a single pixel (past the channel tolerance).
            var diff: schedule.Diff = undefined;
            if (self.compareShots(arg[0..slash], arg[slash + 1 ..], &diff)) |reason| return reason;
            if (diff.differing != 0) return self.fail("expect=shot_same:{s} was false: {d:.3}% of the pixels differ", .{ arg, @as(f64, diff.fraction()) * 100.0 });
            std.debug.print("resource-editor: auto: {s} are equal ({d:.3}% differ)\n", .{ arg, @as(f64, diff.fraction()) * 100.0 });
            return null;
        }
        if (eql(u8, name, "selected")) {
            const node_id = self.panels.selection.primary orelse return self.fail("expect=selected:{s} was false: nothing is selected", .{arg});
            const node = self.life.doc.tree.findNode(node_id) orelse return self.fail("expect=selected:{s} was false: node {d} is gone", .{ arg, node_id });
            if (!std.ascii.eqlIgnoreCase(node.displaySlice(), arg)) return self.fail("expect=selected:{s} was false: the selected node is '{s}'", .{ arg, node.displaySlice() });
            return null;
        }
        if (eql(u8, name, "slot")) {
            const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return self.fail("slot needs <n>=moved|home", .{});
            const drag = self.dragged orelse return self.fail("expect=slot:{s}: no squad_drag ran", .{arg});
            const want_moved = eql(u8, arg[eq + 1 ..], "moved");
            var read = sub_tools.readGeometry(self.bridge(), drag.formation, .formation_positions) catch return self.fail("expect=slot:{s}: {s}", .{ arg, self.bridge().lastMessage() });
            defer read.deinit(self.gpa);
            if (drag.slot >= read.points2.len) return self.fail("expect=slot:{s}: only {d} members", .{ arg, read.points2.len });
            const at = read.points2[drag.slot];
            const moved = @abs(at.x - drag.home.x) > 1e-3 or @abs(at.y - drag.home.y) > 1e-3;
            if (moved != want_moved) return self.fail("expect=slot:{s} was false: the member is at {d:.3}/{d:.3}, it started at {d:.3}/{d:.3}", .{ arg, at.x, at.y, drag.home.x, drag.home.y });
            return null;
        }
        if (eql(u8, name, "squad_dir")) {
            const want = std.fmt.parseFloat(f32, arg) catch return self.fail("squad_dir needs radians", .{});
            const node = self.firstFormation() orelse return self.fail("expect=squad_dir: the project has no formation", .{});
            const read = sub_tools.readGeometry(self.bridge(), node, .formation_direction) catch return self.fail("expect=squad_dir:{s}: {s}", .{ arg, self.bridge().lastMessage() });
            if (@abs(read.point2.x - want) > 1e-4) return self.fail("expect=squad_dir:{s} was false: expected {d:.5}, stored {d:.5}", .{ arg, want, read.point2.x });
            return null;
        }
        if (eql(u8, name, "direction")) {
            const want = std.fmt.parseFloat(f32, arg) catch return self.fail("direction needs radians", .{});
            const node = self.firstFormation() orelse return self.fail("expect=direction: the project has no formation", .{});
            const read = sub_tools.readGeometry(self.bridge(), node, .formation_direction) catch return self.fail("expect=direction:{s}: {s}", .{ arg, self.bridge().lastMessage() });
            if (@abs(read.point2.x - want) > 1e-3) return self.fail("expect=direction:{s} was false: it is {d:.4}", .{ arg, read.point2.x });
            return null;
        }
        if (eql(u8, name, "grid_cell") or eql(u8, name, "trans_cell")) {
            const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return self.fail("{s} needs <x>/<y>=<value>", .{name});
            const at = parseInts(2, arg[0..eq]) orelse return self.fail("{s} needs <x>/<y>=<value>", .{name});
            const want = std.fmt.parseInt(u8, arg[eq + 1 ..], 10) catch return self.fail("{s} needs a value 0..255", .{name});
            const got = (if (name[0] == 'g') self.cellValue(false, at[0], at[1]) else self.cellValue(true, at[0], at[1])) orelse return self.fail("expect={s}:{s} was false: the cell cannot be read", .{ name, arg });
            if (got != want) return self.fail("expect={s}:{s} was false: the tile holds {d}", .{ name, arg, got });
            return null;
        }
        if (eql(u8, name, "lines")) {
            const want = std.fmt.parseInt(usize, arg, 10) catch return self.fail("lines needs a count", .{});
            const editor = self.gridEditor("lines") orelse return self.fail("expect=lines: no grid editor is open", .{});
            var read = sub_tools.readGeometry(self.bridge(), editor.node, .transparency_lines) catch return self.fail("expect=lines:{s}: {s}", .{ arg, self.bridge().lastMessage() });
            defer read.deinit(self.gpa);
            if (read.points2.len / 2 != want) return self.fail("expect=lines:{s} was false: {d} lines", .{ arg, read.points2.len / 2 });
            return null;
        }
        if (eql(u8, name, "zero_tile") or eql(u8, name, "sprite_tile")) {
            const want = parseInts(2, arg) orelse return self.fail("{s} needs <x>/<y>", .{name});
            const editor = self.gridEditor("tile") orelse return self.fail("expect={s}: no grid editor is open", .{name});
            const channel: core.bridge.GeometryChannel = if (name[0] == 'z') .zero_point else .sprite_pos;
            const read = sub_tools.readGeometry(self.bridge(), editor.node, channel) catch return self.fail("expect={s}:{s}: {s}", .{ name, arg, self.bridge().lastMessage() });
            const tile = grid.tileAt(grid.worldToGrid(read.point2)) orelse return self.fail("expect={s}:{s} was false: the point is off the grid", .{ name, arg });
            if (tile[0] != want[0] or tile[1] != want[1]) return self.fail("expect={s}:{s} was false: it is tile {d}/{d}", .{ name, arg, tile[0], tile[1] });
            return null;
        }
        if (eql(u8, name, "sprite")) {
            const home = self.sprite_home orelse return self.fail("expect=sprite:{s}: no sprite_move ran", .{arg});
            const editor = self.gridEditor("sprite") orelse return self.fail("expect=sprite: no grid editor is open", .{});
            const at = (sub_tools.readGeometry(self.bridge(), editor.node, .sprite_pos) catch return self.fail("expect=sprite:{s}: {s}", .{ arg, self.bridge().lastMessage() })).point2;
            const moved = @abs(at.x - home.x) > 1e-3 or @abs(at.y - home.y) > 1e-3;
            if (moved != eql(u8, arg, "moved")) return self.fail("expect=sprite:{s} was false: it is at {d:.2}/{d:.2}, it started at {d:.2}/{d:.2}", .{ arg, at.x, at.y, home.x, home.y });
            return null;
        }
        if (eql(u8, name, "points") or eql(u8, name, "point")) {
            const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return self.fail("{s} needs <mode>...=<value>", .{name});
            const key = arg[0..eq];
            const value = arg[eq + 1 ..];
            const editor = self.gridEditor("points") orelse return self.fail("expect={s}: no grid editor is open", .{name});
            const b = self.bridge();
            const family_end = std.mem.indexOfScalar(u8, key, '/') orelse key.len;
            const mode = parseMode(key[0..family_end]) orelse return self.fail("expect={s}: '{s}' is not a point family", .{ name, key[0..family_end] });
            var read = sub_tools.readGeometry(b, editor.node, pointChannel(mode)) catch return self.fail("expect={s}:{s}: {s}", .{ name, arg, b.lastMessage() });
            defer read.deinit(self.gpa);
            if (name.len == 6) {
                const want = std.fmt.parseInt(usize, value, 10) catch return self.fail("points needs a count", .{});
                if (read.aimed.len != want) return self.fail("expect=points:{s} was false: {d} points", .{ arg, read.aimed.len });
                return null;
            }
            const index = std.fmt.parseInt(usize, if (family_end < key.len) key[family_end + 1 ..] else "", 10) catch return self.fail("point needs <mode>/<i>=<angle>/<cone>", .{});
            const want = parseInts(2, value) orelse return self.fail("point needs <mode>/<i>=<angle>/<cone>", .{});
            if (index >= read.aimed.len) return self.fail("expect=point:{s} was false: the family has {d} points", .{ arg, read.aimed.len });
            const got = read.aimed[index];
            const angle_off = @abs(@mod(got.angle - want[0] + 180, 360) - 180);
            if (angle_off > 1 or @abs(got.cone - want[1]) > 1) return self.fail("expect=point:{s} was false: the point has angle {d} and cone {d}", .{ arg, got.angle, got.cone });
            return null;
        }
        if (eql(u8, name, "entrance_tile")) {
            const want = parseInts(2, arg) orelse return self.fail("entrance_tile needs <x>/<y>", .{});
            const editor = self.gridEditor("entrance_tile") orelse return self.fail("expect=entrance_tile: no grid editor is open", .{});
            const read = sub_tools.readGeometry(self.bridge(), editor.node, .entrance) catch return self.fail("expect=entrance_tile:{s}: {s}", .{ arg, self.bridge().lastMessage() });
            const tile = grid.tileAt(grid.worldToGrid(read.point2)) orelse return self.fail("expect=entrance_tile:{s} was false: the entrance is off the grid", .{arg});
            if (tile[0] != want[0] or tile[1] != want[1]) return self.fail("expect=entrance_tile:{s} was false: it is tile {d}/{d}", .{ arg, tile[0], tile[1] });
            return null;
        }
        if (eql(u8, name, "span_mark")) {
            const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return self.fail("span_mark needs <mark>=moved|home", .{});
            const mark = parseSpanMark(arg[0..eq]) orelse return self.fail("expect=span_mark: '{s}' is not begin, end, front or back", .{arg[0..eq]});
            const editor = self.gridEditor("span_mark") orelse return self.fail("expect=span_mark: no grid editor is open", .{});
            const marks = grid.readSpanMarks(self.gpa, self.bridge(), editor.node) catch return self.fail("expect=span_mark:{s}: {s}", .{ arg, self.bridge().lastMessage() });
            const now: Point2 = switch (mark) {
                .begin => marks[0],
                .end => marks[1],
                .front => .{ .x = marks[2].x },
                .back => .{ .x = marks[2].y },
            };
            const home: Point2 = if (mark == .begin or mark == .end) core.point_tools.default_span_mark else .{ .x = 0 };
            const moved = now.x != home.x or (mark != .front and mark != .back and now.y != home.y);
            if (moved != eql(u8, arg[eq + 1 ..], "moved")) return self.fail("expect=span_mark:{s} was false: the mark is at {d:.2}/{d:.2}", .{ arg, now.x, now.y });
            return null;
        }
        if (eql(u8, name, "keys")) {
            const want = std.fmt.parseInt(usize, arg, 10) catch return self.fail("keys needs a count", .{});
            var read = self.curveKeys(name) orelse return self.failure;
            defer read.deinit(self.gpa);
            if (read.vec3.len != want) return self.fail("expect=keys:{s} was false: the curve holds {d} keys", .{ arg, read.vec3.len });
            return null;
        }
        if (eql(u8, name, "key")) {
            const eq = std.mem.indexOfScalar(u8, arg, '=') orelse return self.fail("key needs <i>=<x>/<y>", .{});
            const index = std.fmt.parseInt(usize, arg[0..eq], 10) catch return self.fail("key needs <i>=<x>/<y>", .{});
            const want = parsePoint(arg[eq + 1 ..]) orelse return self.fail("key needs <i>=<x>/<y>", .{});
            var read = self.curveKeys(name) orelse return self.failure;
            defer read.deinit(self.gpa);
            if (index >= read.vec3.len) return self.fail("expect=key:{s} was false: the curve holds {d} keys", .{ arg, read.vec3.len });
            const got = read.vec3[index];
            if (@abs(got.x - want.x) > 0.02 or @abs(got.y - want.y) > 0.02) return self.fail("expect=key:{s} was false: key {d} is at {d:.4}/{d:.4}", .{ arg, index, got.x, got.y });
            return null;
        }
        if (eql(u8, name, "zoom")) {
            const want = parsePoint(arg) orelse return self.fail("zoom needs <xs>/<ys>", .{});
            const editor = if (self.curve) |*e| e else return self.fail("expect=zoom: no curve was selected", .{});
            if (@abs(editor.xs - want.x) > 0.5 or @abs(@as(f32, @floatFromInt(editor.ys)) - want.y) > 0.5) return self.fail("expect=zoom:{s} was false: {d:.1}/{d} px per step", .{ arg, editor.xs, editor.ys });
            return null;
        }
        if (eql(u8, name, "camera")) {
            if ((eql(u8, arg, "horizontal")) != self.horizontal_camera) return self.fail("expect=camera:{s} was false", .{arg});
            return null;
        }
        if (eql(u8, name, "particle_info")) {
            const info = self.particle_status.info orelse return self.fail("expect=particle_info:{s} was false: do=particle_info has not succeeded", .{arg});
            const finite = std.math.isFinite(info.max_count) and std.math.isFinite(info.max_size) and std.math.isFinite(info.average_size) and std.math.isFinite(info.average_count);
            if (!finite or info.max_count <= 0) return self.fail("expect=particle_info:{s} was false: max_count {d}, max_size {d}, average_size {d}, average_count {d}", .{ arg, info.max_count, info.max_size, info.average_size, info.average_count });
            return null;
        }
        if (eql(u8, name, "effect_angle")) {
            const want = std.fmt.parseFloat(f32, arg) catch return self.fail("expect=effect_angle needs degrees, got '{s}'", .{arg});
            var stored: f32 = 0;
            if (!docks_logic.syncEffectAngle(self.bridge(), &stored)) return self.fail("expect=effect_angle:{s}: {s}", .{ arg, self.bridge().lastMessage() });
            const have = docks_logic.directionDegrees(stored);
            // The text of the dock reads 360 where 0 is meant (MFC's boundary), so compare around the circle.
            if (@abs(@mod(have - want + 180, 360) - 180) > 0.1) return self.fail("expect=effect_angle:{s} was false: the dock reads {d:.2} degrees", .{ arg, have });
            return null;
        }
        if (eql(u8, name, "source_mode")) {
            const want_complex = if (eql(u8, arg, "complex")) true else if (eql(u8, arg, "simple")) false else return self.fail("source_mode needs complex or simple", .{});
            const have = docks_logic.SourceToggle.mode(self.bridge()) orelse return self.fail("expect=source_mode:{s}: {s}", .{ arg, self.bridge().lastMessage() });
            if (have != want_complex) return self.fail("expect=source_mode:{s} was false: the bridge reads {s}", .{ arg, if (have) "complex" else "simple" });
            return null;
        }
        if (eql(u8, name, "export_complex")) return self.exportFlag(arg, "1");
        if (eql(u8, name, "export_simple")) return self.exportFlag(arg, "0");
        if (eql(u8, name, "cross")) return self.crossAt(arg);
        if (eql(u8, name, "shot_marker")) return self.shotMarker(arg);
        if (eql(u8, name, "shot_picture")) return self.shotPicture(arg);
        if (eql(u8, name, "shot_minimap")) return self.shotMinimap(arg);
        if (eql(u8, name, "shot_colour")) return self.shotColour(arg);
        if (eql(u8, name, "shot_curve_handle")) return self.shotCurveHandle(arg);
        return self.fail("unknown predicate '{s}'", .{name});
    }
};

/// The grid verbs' largest value: a transparency step (a locked tile is any non-zero value).
const grid_value_max: i32 = 7;

/// `n` slash-separated whole numbers; null when there are not exactly that many.
fn parseInts(comptime n: usize, text: []const u8) ?[n]i32 {
    var out: [n]i32 = undefined;
    var parts = std.mem.splitScalar(u8, text, '/');
    for (&out) |*slot| slot.* = std.fmt.parseInt(i32, parts.next() orelse return null, 10) catch return null;
    if (parts.next() != null) return null;
    return out;
}

/// A family's name in the schedule: shoot, fire, smoke or dir.
fn parseMode(text: []const u8) ?core.point_tools.Mode {
    if (std.mem.eql(u8, text, "shoot")) return .shoot;
    if (std.mem.eql(u8, text, "fire")) return .fire;
    if (std.mem.eql(u8, text, "smoke")) return .smoke;
    if (std.mem.eql(u8, text, "dir")) return .dir_explosion;
    return null;
}

fn parseSpanMark(text: []const u8) ?core.point_tools.SpanMark {
    if (std.mem.eql(u8, text, "begin")) return .begin;
    if (std.mem.eql(u8, text, "end")) return .end;
    if (std.mem.eql(u8, text, "front")) return .front;
    if (std.mem.eql(u8, text, "back")) return .back;
    return null;
}

fn pointTool(mode: core.point_tools.Mode) grid.Tool {
    return switch (mode) {
        .shoot => .shoot,
        .fire => .fire,
        .smoke => .smoke,
        .dir_explosion => .dir_explosion,
    };
}

fn pointChannel(mode: core.point_tools.Mode) core.bridge.GeometryChannel {
    return switch (mode) {
        .shoot => .shoot_points,
        .fire => .fire_points,
        .smoke => .smoke_points,
        .dir_explosion => .directed_explosion_points,
    };
}

fn parsePoint(text: []const u8) ?Point2 {
    var parts = std.mem.splitScalar(u8, text, '/');
    const x = std.fmt.parseFloat(f32, parts.next() orelse return null) catch return null;
    const y = std.fmt.parseFloat(f32, parts.next() orelse return null) catch return null;
    return .{ .x = x, .y = y };
}

/// resource-editor-auto: the schedule in `schedule_text` over the host. The
/// exit is true for a pass.
pub fn auto(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, host: *host_mod.Host, schedule_text: []const u8, fixtures: []const u8, scratch: []const u8) bool {
    var failure: schedule.Failure = .{};
    const entries = schedule.parse(gpa, schedule_text, &failure) catch |err|
        return failLine("auto", "bad token '{s}': {s} ({s})", .{ failure.token, failure.reason, @errorName(err) });
    defer gpa.free(entries);
    const cwd = std.Io.Dir.cwd();
    cwd.deleteTree(io, scratch) catch |err| return failLine("auto", "{s} could not be cleared: {s}", .{ scratch, @errorName(err) });
    cwd.createDirPath(io, scratch) catch |err| return failLine("auto", "{s}: {s}", .{ scratch, @errorName(err) });

    const real = c_bridge.RealResBridge.init(gpa, host.session);
    const runner_session = real.session;
    var paths = std.mem.zeroes(c.BkEditorPathSet);
    if (c.BkEditorPaths(runner_session, &paths) != c.BK_EDITOR_OK) return failLine("auto", "no installation paths", .{});
    var base_buffer: [logic.path_capacity]u8 = undefined;
    const root = std.mem.sliceTo(&paths.base_root, 0);
    const base_root = std.fmt.bufPrint(&base_buffer, "{s}{s}", .{ root, if (std.mem.endsWith(u8, root, "/") or std.mem.endsWith(u8, root, "\\")) "" else "/" }) catch
        return failLine("auto", "the installation path is too long", .{});

    var runner: Runner = .{
        .gpa = gpa,
        .io = io,
        .environ = environ,
        .host = host,
        .real = real,
        .dir = scratch,
        .fixtures = fixtures,
        .base_root = base_root,
    };
    defer runner.life.deinit(gpa);
    defer runner.panels.deinit(gpa);
    defer if (runner.curve) |*curve| curve.deinit();
    defer _ = runner.bridge().close();
    // Before the bridge closes, like the preview: the docks' textures belong to the engine.
    defer if (runner.docks) |*d| d.deinit();
    // Before the bridge closes: the preview scene belongs to the engine's modules.
    defer runner.preview.stop(runner.bridge());
    defer if (runner.running) |*r| r.terminate(io);

    var last: u32 = 0;
    for (entries) |entry| last = @max(last, entry.frame);
    var exiting = false;
    while (runner.frame <= last and !exiting) : (runner.frame += 1) {
        // The docks' once-a-frame sync: the scene of the open project is begun a
        // frame before Run. Off until do=preview_on, so the earlier blocks'
        // frames stay as they were measured.
        if (runner.preview_on) _ = runner.preview.sync(runner.bridge(), runner.life.is_open, runner.life.doc.kind);
        for ([_]bool{ true, false }) |before_draw| {
            if (!before_draw) {
                runner.drawFrame() catch |err|
                    return failLine("auto", "frame {d}: {s}: {s}", .{ runner.frame, @errorName(err), runner.bridge().lastMessage() });
            }
            for (entries) |entry| {
                if (entry.frame != runner.frame) continue;
                if (before_draw) std.debug.print("resource-editor: auto: frame {d} {s}\n", .{ entry.frame, entry.text });
                if (runner.run(entry.action, before_draw)) |reason|
                    return failLine("auto", "frame {d} {s}: {s}", .{ entry.frame, entry.text, reason });
                if (!before_draw and entry.action == .exit) exiting = true;
            }
        }
    }
    std.debug.print("resource-editor: auto PASS ({d} actions over {d} frames, shots under {s})\n", .{ entries.len, runner.frame, scratch });
    return true;
}
