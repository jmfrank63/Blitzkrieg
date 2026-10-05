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
//!   open=<path> save saveas=<path> shot=<name> differ=<a>/<b>@<percent> exit
//!   expect=kind:<ext>  dirty:<true|false>  untitled  nodes_min:<n>
//!          prop:<name>=<value>  exported  file:<path>  shot_lit:<name>
//!          slot:<n>=moved|home  the formation member against where the drag found it
//!          direction:<radians>  the formation's direction
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

const c = c_bridge.c;

const Kind = core.bridge.Kind;
const ResBridge = core.bridge.ResBridge;
const PropRecord = core.bridge.PropRecord;
const sub_tools = core.sub_editor_tools;
const Point2 = core.bridge.Point2;

const owner = "resource-editor-auto";
const gunner_folder = "Data/Units/Humans/German/Gunner";
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
    /// The slot a squad_drag moved and where it stood before, for expect=slot.
    dragged: ?struct { formation: i32, slot: usize, home: Point2 } = null,
    frame: u32 = 0,
    message: [768]u8 = undefined,

    fn bridge(self: *Runner) ResBridge {
        return self.real.bridge();
    }

    fn fail(self: *Runner, comptime format: []const u8, args: anytype) []const u8 {
        return std.fmt.bufPrint(&self.message, format, args) catch "the failure text did not fit";
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

    fn differ(self: *Runner, d: schedule.Differ) ?[]const u8 {
        var a_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        var b_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const a_path = self.shotPath(&a_buffer, d.a) orelse return self.fail("the shot path is too long", .{});
        const b_path = self.shotPath(&b_buffer, d.b) orelse return self.fail("the shot path is too long", .{});
        const a_bytes = readFile(self.io, self.gpa, a_path) catch |err| return self.fail("{s}: {s}", .{ a_path, @errorName(err) });
        defer self.gpa.free(a_bytes);
        const b_bytes = readFile(self.io, self.gpa, b_path) catch |err| return self.fail("{s}: {s}", .{ b_path, @errorName(err) });
        defer self.gpa.free(b_bytes);
        const a = schedule.Tga.parse(a_bytes) catch |err| return self.fail("{s}: {s}", .{ a_path, @errorName(err) });
        const b = schedule.Tga.parse(b_bytes) catch |err| return self.fail("{s}: {s}", .{ b_path, @errorName(err) });
        const diff = schedule.compareTga(a, b, schedule.default_channel_tolerance);
        if (!diff.same_size) return self.fail("{s} and {s} are different sizes", .{ d.a, d.b });
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
        if (eql(u8, name, "run_game")) return self.runGame();
        return self.fail("unknown command '{s}'", .{name});
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
        if (eql(u8, name, "direction")) {
            const want = std.fmt.parseFloat(f32, arg) catch return self.fail("direction needs radians", .{});
            const node = self.firstFormation() orelse return self.fail("expect=direction: the project has no formation", .{});
            const read = sub_tools.readGeometry(self.bridge(), node, .formation_direction) catch return self.fail("expect=direction:{s}: {s}", .{ arg, self.bridge().lastMessage() });
            if (@abs(read.point2.x - want) > 1e-3) return self.fail("expect=direction:{s} was false: it is {d:.4}", .{ arg, read.point2.x });
            return null;
        }
        return self.fail("unknown predicate '{s}'", .{name});
    }
};

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
    defer _ = runner.bridge().close();
    defer if (runner.running) |*r| r.terminate(io);

    var last: u32 = 0;
    for (entries) |entry| last = @max(last, entry.frame);
    var exiting = false;
    while (runner.frame <= last and !exiting) : (runner.frame += 1) {
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
