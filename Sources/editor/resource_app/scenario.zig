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
//!   do=keyframe:reset       the dock's Reset all
//!   do=keyframe:zoomx_in|zoomx_out|zoomy_in|zoomy_out   the curve's zoom menu (view only, no undo step)
//!   do=camera               the preview's Camera button (horizontal against default camera)
//!   do=import_file:<ext>/<path>   Import a runtime file (a shipped particle xml for pcp) as a new project, through the bridge's reader
//!   do=import_refused:<ext>/<path>   the same for a kind with no import (eff): refused, with the bridge's reason
//!   The grid verbs need a frame drawn since the project opened (the grid editor lives in the panels)
//!   and go through GridEditor's press, move and release, the path of the mouse.
//!   open=<path> save saveas=<path> shot=<name> differ=<a>/<b>@<percent> exit
//!   expect=kind:<ext>  dirty:<true|false>  untitled  nodes_min:<n>
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

const c = c_bridge.c;

const Kind = core.bridge.Kind;
const ResBridge = core.bridge.ResBridge;
const PropRecord = core.bridge.PropRecord;
const sub_tools = core.sub_editor_tools;
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
    var event: sdl3.c.SDL_Event = undefined;
    while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
    host.beginFrame();
    panels.draw(gpa, b, life, host.window);
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

    fn findProp(self: *Runner, name: []const u8) ?struct { node: i32, record: PropRecord } {
        for (self.life.doc.tree.props.items) |entry| {
            if (std.ascii.eqlIgnoreCase(entry.record.defaultSlice(), name) or std.ascii.eqlIgnoreCase(entry.record.displaySlice(), name))
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
        if (eql(u8, name, "camera")) {
            const was = self.horizontal_camera;
            self.horizontal_camera = docks_logic.toggledCamera(b, was);
            if (self.horizontal_camera == was) return self.fail("camera: the engine refused the change: {s}", .{b.lastMessage()});
            std.debug.print("resource-editor: auto: camera is now {s}\n", .{if (self.horizontal_camera) "horizontal" else "default"});
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
        if (eql(u8, name, "shot_colour")) return self.shotColour(arg);
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
                drawFrame(host, &runner.panels, gpa, runner.bridge(), &runner.life) catch |err|
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
