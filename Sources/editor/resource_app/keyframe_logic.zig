//! The Particle and Effect curve editor's logic with no window and no ImGui: a
//! port of CKeyFrameEditor (Sources/src/editor/KeyFrame.cpp) - the mapping
//! between curve values and the Function window's pixels, its scroll model,
//! the mouse gestures that add, move and delete keys, and the dock's Reset all.
//! The quirks of the MFC code are kept on purpose and named where they live:
//! a press hits a key on screen x only, key 0 never moves in x, a neighbour
//! can be approached no closer than NODE_SPACE pixels, and Delete removes the
//! key last touched, not the one under the cursor.
//!
//! Keys are the vec3 list of the `.particle_keyframes` channel (x is time, y
//! the value, z stays 0 as MFC keeps pairs). The editor works on a local copy
//! while a drag runs, as MFC edits its framesList and tells the item only on
//! the button release (WM_KEY_FRAME_UPDATE); the release, a delete and Reset
//! all each commit ONE geometry command through `sub_editor_tools.commit`, so
//! a whole drag is one undo step. Zoom changes the view only and is not an undo
//! step. Runs under `zig build test-resource-app-logic` against the fake bridge.
const std = @import("std");
const core = @import("resource_core");

const tools = core.sub_editor_tools;
const bridge_mod = core.bridge;
const ResBridge = bridge_mod.ResBridge;
const Vec3 = bridge_mod.Vec3;
const KeyframeKnobs = bridge_mod.KeyframeKnobs;
const Document = core.document.Document;
const History = core.history.History;
const ResourceCommand = core.history.ResourceCommand;

pub const Error = bridge_mod.EditError;

// The constants of KeyFrame.cpp.
/// Pixels per step on both axes by default (XS, YS).
const default_step_px: i32 = 25;
/// Pixels from the window's left edge to the x axis origin (LEFT).
pub const left: i32 = 43;
/// Pixels from the window's bottom edge to the y axis origin (BOTTOM).
pub const bottom: i32 = 30;
/// A press hits a key within this many pixels of it on screen x (SELECT_SIZE).
pub const select_size: i32 = 3;
/// Neighbouring keys keep at least this many pixels between them while one
/// is dragged (NODE_SPACE).
pub const node_space: i32 = 3;
/// The room CKeyFrameEditor leaves for the vertical scroll bar when the
/// window resizes the curve to fit (the literal 15 in SetHDimention).
const resize_margin: f32 = 15;

/// The pixels per step the zoom commands step through. MFC's handlers are
/// commented out (they scaled by SIZES = {5, 10, 20, 50, 100}), so this list
/// is this port's: SIZES with the default of 25 between 20 and 50.
pub const zoom_levels = [_]i32{ 5, 10, 20, 25, 50, 100 };
const default_zoom_index: usize = 3;

/// A scroll bar as the model needs it: Windows clamps a position to
/// `[0, max - page + 1]`, and a hidden bar scrolls nothing.
const Scroll = struct {
    visible: bool = false,
    max: i32 = 0,
    page: i32 = 0,
    pos: i32 = 0,

    fn limit(self: Scroll) i32 {
        return @max(0, self.max - self.page + 1);
    }

    fn set(self: *Scroll, pos: i32) void {
        self.pos = std.math.clamp(pos, 0, self.limit());
    }
};

/// A float to a pixel the way C++ assigns it: truncated toward zero.
fn pixel(value: f32) i32 {
    return @intFromFloat(@trunc(value));
}

pub const Mode = enum { free, drag };

pub const Editor = struct {
    allocator: std.mem.Allocator,
    node: i32,
    knobs: KeyframeKnobs = .{ .min_x = 0, .max_x = 0, .min_y = 0, .max_y = 0 },
    /// The client rectangle in pixels (GetClientRect).
    width: i32 = 400,
    height: i32 = 200,
    keys: std.ArrayList(Vec3) = .empty,
    /// The keys at the press: what undo of the gesture returns to.
    before: []Vec3 = &.{},
    mode: Mode = .free,
    /// m_nDragIndex: the key last pressed, which Delete removes. It starts at
    /// 0 and survives the release, as in MFC.
    drag_index: usize = 0,
    /// m_nHighNodeIndex: the key the cursor hovers over, for highlighting.
    high_index: ?usize = null,
    scroll_x: Scroll = .{},
    scroll_y: Scroll = .{},
    /// m_XS and m_YS: pixels per step.
    xs: f32 = default_step_px,
    ys: i32 = default_step_px,
    zoom_x: usize = default_zoom_index,
    zoom_y: usize = default_zoom_index,
    /// The step zoom X resets xs to; resize mode ignores it.
    xs_base: f32 = default_step_px,

    pub fn init(allocator: std.mem.Allocator, node: i32) Editor {
        return .{ .allocator = allocator, .node = node };
    }

    pub fn deinit(self: *Editor) void {
        self.keys.deinit(self.allocator);
        self.freeBefore();
    }

    fn freeBefore(self: *Editor) void {
        if (self.before.len != 0) self.allocator.free(self.before);
        self.before = &.{};
    }

    /// Reads the curve's knobs and keys through the bridge and lays the view
    /// out (SetDimentions + SetFramesList).
    pub fn load(self: *Editor, bridge: ResBridge) Error!void {
        var knobs: KeyframeKnobs = .{};
        try bridge_mod.check(bridge.keyframeKnobs(self.node, &knobs));
        var read = try tools.readGeometry(bridge, self.node, .particle_keyframes);
        defer read.deinit(self.allocator);
        self.keys.clearRetainingCapacity();
        try self.keys.appendSlice(self.allocator, read.vec3);
        self.mode = .free;
        self.high_index = null;
        self.setDimensions(knobs);
    }

    // --- Dimensions and scrolling (SetDimentions, SetHDimention, SetVDimention) ---

    pub fn setDimensions(self: *Editor, knobs: KeyframeKnobs) void {
        self.knobs = knobs;
        self.layoutX();
        self.layoutY();
        if (self.scroll_y.visible) self.scroll_y.set(self.scroll_y.max);
    }

    /// OnSize: the window changed, so the bars are recomputed.
    pub fn setSize(self: *Editor, width: i32, height: i32) void {
        self.width = width;
        self.height = height;
        self.layoutX();
        self.layoutY();
    }

    /// SetXResizeMode: the dock's per-curve flag; the x range then always fits
    /// the window and the bottom bar is hidden.
    pub fn setXResizeMode(self: *Editor, resize: bool) void {
        self.knobs.resize_mode = resize;
        self.layoutX();
    }

    fn layoutX(self: *Editor) void {
        const k = self.knobs;
        const width: f32 = @floatFromInt(self.width);
        if (k.resize_mode) {
            self.scroll_x.visible = false;
            // MFC divides by the range unguarded; an empty range keeps the old scale.
            if (k.max_x > k.min_x) self.xs = (width - @as(f32, @floatFromInt(left)) - resize_margin) / (k.max_x - k.min_x) * k.step_x;
            return;
        }
        self.xs = self.xs_base;
        const in_bar = pixel((k.max_x - k.min_x) / k.step_x + 0.5);
        const on_screen = pixel((width - @as(f32, @floatFromInt(left)) - self.xs / 2) / self.xs + 0.5);
        if (on_screen > in_bar) {
            self.scroll_x.visible = false;
            return;
        }
        self.scroll_x.visible = true;
        self.scroll_x.max = in_bar - 1;
        self.scroll_x.page = on_screen;
        self.scroll_x.set(self.scroll_x.pos);
    }

    fn layoutY(self: *Editor) void {
        const k = self.knobs;
        const in_bar = pixel((k.max_y - k.min_y) / k.step_y + 0.5);
        const on_screen = self.rowsOnScreen();
        if (on_screen >= in_bar) {
            self.scroll_y.visible = false;
            return;
        }
        self.scroll_y.visible = true;
        self.scroll_y.max = in_bar - 1;
        self.scroll_y.page = on_screen;
        self.scroll_y.set(self.scroll_y.pos);
    }

    /// (client height - BOTTOM - YS/4) / YS in C++ integer arithmetic.
    fn rowsOnScreen(self: Editor) i32 {
        return @divTrunc(self.height - bottom - @divTrunc(self.ys, 4), self.ys);
    }

    /// GetVisibleX: the first and last step index on the screen.
    fn visibleX(self: Editor) [2]i32 {
        const width: f32 = @floatFromInt(self.width);
        const first = self.scroll_x.pos;
        return .{ first, first + pixel((width - @as(f32, @floatFromInt(left)) - self.xs / 2) / self.xs) };
    }

    /// GetVisibleY: the bar counts from the top, the values from the bottom.
    fn visibleY(self: Editor) [2]i32 {
        const last = self.scroll_y.max - self.scroll_y.pos + 1;
        return .{ last - self.rowsOnScreen(), last };
    }

    pub const Screen = struct { x: f32, y: f32 };
    pub const Value = struct { x: f32, y: f32 };

    /// GetScreenByValue.
    pub fn screenByValue(self: Editor, x: f32, y: f32) Screen {
        const first_x: i32 = if (self.scroll_x.visible) self.visibleX()[0] else 0;
        const min_x = self.knobs.min_x + @as(f32, @floatFromInt(first_x)) * self.knobs.step_x;
        const scale_x = self.xs / self.knobs.step_x;
        const first_y: i32 = if (self.scroll_y.visible) self.visibleY()[0] else 0;
        const min_y = self.knobs.min_y + @as(f32, @floatFromInt(first_y)) * self.knobs.step_y;
        const scale_y = @as(f32, @floatFromInt(self.ys)) / self.knobs.step_y;
        return .{
            .x = (x - min_x) * scale_x + @as(f32, @floatFromInt(left)),
            .y = @as(f32, @floatFromInt(self.height)) - ((y - min_y) * scale_y + @as(f32, @floatFromInt(bottom))),
        };
    }

    /// GetValueByScreen.
    pub fn valueByScreen(self: Editor, x: i32, y: i32) Value {
        const first_x: i32 = if (self.scroll_x.visible) self.visibleX()[0] else 0;
        const min_x = self.knobs.min_x + @as(f32, @floatFromInt(first_x)) * self.knobs.step_x;
        const scale_x = self.knobs.step_x / self.xs;
        const first_y: i32 = if (self.scroll_y.visible) self.visibleY()[0] else 0;
        const min_y = self.knobs.min_y + @as(f32, @floatFromInt(first_y)) * self.knobs.step_y;
        const scale_y = self.knobs.step_y / @as(f32, @floatFromInt(self.ys));
        return .{
            .x = min_x + @as(f32, @floatFromInt(x - left)) * scale_x,
            .y = min_y + @as(f32, @floatFromInt(self.height - y - bottom)) * scale_y,
        };
    }

    // --- Hit testing -----------------------------------------------------------

    const Probe = struct { index: usize, found: bool };

    /// The walk shared by the press and the hover: keys are sorted by x, so the
    /// first one within SELECT_SIZE of the cursor's screen x is hit, and the
    /// first one to the right of the cursor is where a new key would go. Only
    /// x is tested, never y.
    fn probe(self: Editor, px: i32) Probe {
        var i: usize = 0;
        while (i < self.keys.items.len) : (i += 1) {
            const key = self.keys.items[i];
            const key_x = self.screenByValue(key.x, key.y).x;
            if (key_x >= @as(f32, @floatFromInt(px - select_size)) and key_x <= @as(f32, @floatFromInt(px + select_size))) return .{ .index = i, .found = true };
            if (@as(f32, @floatFromInt(px)) < key_x) break;
        }
        return .{ .index = i, .found = false };
    }

    /// OnMouseMove in free mode: which key to highlight.
    pub fn hover(self: *Editor, px: i32) void {
        const hit = self.probe(px);
        self.high_index = if (hit.found) hit.index else null;
    }

    // --- The mouse gesture -------------------------------------------------------

    /// OnLButtonDown. A hit key takes the click's y (clamped into the range)
    /// and starts a drag; a miss inside both ranges inserts a key there, sorted
    /// by x, and drags it; a miss outside does nothing.
    pub fn press(self: *Editor, px: i32, py: i32) Error!void {
        if (self.mode == .drag) return;
        const hit = self.probe(px);
        const at = self.valueByScreen(px, py);
        const k = self.knobs;
        if (hit.found) {
            try self.snapshot();
            self.keys.items[hit.index].y = std.math.clamp(at.y, k.min_y, k.max_y);
        } else if (at.x >= k.min_x and at.x <= k.max_x and at.y >= k.min_y and at.y <= k.max_y) {
            try self.snapshot();
            errdefer self.freeBefore();
            try self.keys.insert(self.allocator, hit.index, .{ .x = at.x, .y = at.y, .z = 0 });
        } else return;
        self.drag_index = hit.index;
        self.mode = .drag;
    }

    /// OnMouseMove in drag mode. The cursor is clamped the way MFC clamps it
    /// (the neighbours, the window, the range) and the bars scroll one step at
    /// an edge; key 0 only moves in y.
    pub fn move(self: *Editor, px: i32, py: i32) void {
        if (self.mode != .drag) return;
        var x = px;
        var y = py;
        const i = self.drag_index;
        if (i > 0) {
            const prev = self.keys.items[i - 1];
            const bound = self.screenByValue(prev.x, prev.y).x;
            if (@as(f32, @floatFromInt(x)) < bound + node_space) {
                x = pixel(bound + node_space);
            } else if (x < left) {
                if (self.scroll_x.visible and self.scroll_x.pos > 0) self.scroll_x.set(self.scroll_x.pos - 1);
                x = left;
            }
        }
        if (i + 1 < self.keys.items.len) {
            const next = self.keys.items[i + 1];
            const bound = self.screenByValue(next.x, next.y).x;
            if (@as(f32, @floatFromInt(x)) > bound - node_space) {
                x = pixel(bound - node_space);
            } else {
                const visible = self.visibleX();
                const edge = left + pixel(@as(f32, @floatFromInt(visible[1] - visible[0])) * self.xs);
                if (x > edge) {
                    if (self.scroll_x.visible and self.scroll_x.pos < self.scroll_x.max) self.scroll_x.set(self.scroll_x.pos + 1);
                    x = edge;
                }
            }
        }
        if (y > self.height - bottom) {
            if (self.scroll_y.visible and self.scroll_y.pos < self.scroll_y.max) self.scroll_y.set(self.scroll_y.pos + 1);
            y = self.height - bottom;
        } else if (y < @divTrunc(self.ys, 4)) {
            if (self.scroll_y.visible and self.scroll_y.pos > 0) self.scroll_y.set(self.scroll_y.pos - 1);
            y = @divTrunc(self.ys, 4);
        }
        const at = self.valueByScreen(x, y);
        const key = &self.keys.items[i];
        if (i != 0) key.x = @min(at.x, self.knobs.max_x);
        // No lower clamp: only the window edge above bounds the value from below.
        key.y = @min(at.y, self.knobs.max_y);
    }

    /// OnLButtonUp: the gesture ends and, when it changed the keys, its one
    /// command is committed. A press that landed outside, or a click that
    /// changed nothing, adds no undo step.
    pub fn release(self: *Editor, bridge: ResBridge, doc: *Document, history: *History) Error!void {
        if (self.mode != .drag) return;
        self.mode = .free;
        defer self.freeBefore();
        if (std.mem.eql(u8, std.mem.sliceAsBytes(self.before), std.mem.sliceAsBytes(self.keys.items))) return;
        self.commitChange(bridge, doc, history, self.before, self.keys.items) catch |err| {
            // The bridge still holds the keys at the press; show them.
            self.keys.clearRetainingCapacity();
            self.keys.appendSlice(self.allocator, self.before) catch {};
            return err;
        };
    }

    /// Escape during a drag: the keys go back to the press.
    pub fn cancel(self: *Editor) void {
        if (self.mode != .drag) return;
        self.mode = .free;
        self.keys.clearRetainingCapacity();
        self.keys.appendSlice(self.allocator, self.before) catch {};
        self.freeBefore();
    }

    fn snapshot(self: *Editor) Error!void {
        self.freeBefore();
        self.before = try self.allocator.dupe(Vec3, self.keys.items);
    }

    /// One geometry command from `old` to `new`, committed as one undo step.
    fn commitChange(self: *Editor, bridge: ResBridge, doc: *Document, history: *History, old: []const Vec3, new: []const Vec3) Error!void {
        const before = try self.allocator.dupe(Vec3, old);
        errdefer self.allocator.free(before);
        const after = try self.allocator.dupe(Vec3, new);
        errdefer self.allocator.free(after);
        const command: ResourceCommand = .{ .geometry = .{
            .node = self.node,
            .channel = .particle_keyframes,
            .before = .{ .vec3 = before },
            .after = .{ .vec3 = after },
        } };
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
    }

    // --- Delete and Reset all ----------------------------------------------------

    /// DeleteActiveNode: removes the key last touched. Key 0 is protected, and
    /// an index past the end (after Reset all) removes nothing. The index is
    /// not moved, so a second Delete removes the key that slid into its place.
    /// Returns whether a key went.
    pub fn deleteActive(self: *Editor, bridge: ResBridge, doc: *Document, history: *History) Error!bool {
        if (self.mode == .drag) return false;
        const i = self.drag_index;
        if (i == 0 or i >= self.keys.items.len) return false;
        const old = try self.allocator.dupe(Vec3, self.keys.items);
        defer self.allocator.free(old);
        _ = self.keys.orderedRemove(i);
        self.high_index = null;
        self.commitChange(bridge, doc, history, old, self.keys.items) catch |err| {
            self.keys.clearRetainingCapacity();
            self.keys.appendSlice(self.allocator, old) catch {};
            return err;
        };
        return true;
    }

    /// ResetNodes, the dock's Reset all: only the first key stays. Returns
    /// whether anything changed (a curve of one key has nothing to reset).
    pub fn resetAll(self: *Editor, bridge: ResBridge, doc: *Document, history: *History) Error!bool {
        if (self.mode == .drag or self.keys.items.len <= 1) return false;
        const old = try self.allocator.dupe(Vec3, self.keys.items);
        defer self.allocator.free(old);
        self.keys.shrinkRetainingCapacity(1);
        self.high_index = null;
        self.commitChange(bridge, doc, history, old, self.keys.items) catch |err| {
            self.keys.clearRetainingCapacity();
            self.keys.appendSlice(self.allocator, old) catch {};
            return err;
        };
        return true;
    }

    // --- Zoom (view only, not an undo step) ----------------------------------------

    /// The zoom commands step the pixels per step through `zoom_levels`.
    /// Returns whether the level changed. Zoom X does nothing while the curve
    /// resizes to fit the window.
    pub fn zoomX(self: *Editor, direction: enum { in, out }) bool {
        if (self.knobs.resize_mode) return false;
        const next = stepIndex(self.zoom_x, direction == .in) orelse return false;
        self.zoom_x = next;
        self.xs_base = @floatFromInt(zoom_levels[next]);
        self.layoutX();
        return true;
    }

    pub fn zoomY(self: *Editor, direction: enum { in, out }) bool {
        const next = stepIndex(self.zoom_y, direction == .in) orelse return false;
        self.zoom_y = next;
        self.ys = zoom_levels[next];
        self.layoutY();
        return true;
    }

    fn stepIndex(index: usize, up: bool) ?usize {
        if (up) return if (index + 1 < zoom_levels.len) index + 1 else null;
        return if (index > 0) index - 1 else null;
    }
};

// --- Tests -------------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = core.fake_bridge.FakeResBridge;
const GeometryValue = bridge_mod.GeometryValue;

const Rig = struct {
    fake: FakeResBridge,
    doc: Document = .{},
    history: History = .{},
    curve: i32 = 0,
    editor: Editor = undefined,

    /// A particle project with one curve node holding `keys`, declared with `knobs`.
    fn init(allocator: std.mem.Allocator, keys: []const Vec3, knobs: KeyframeKnobs) !Rig {
        var rig: Rig = .{ .fake = FakeResBridge.init(allocator) };
        errdefer rig.fake.deinit();
        const res = rig.fake.bridge();
        try bridge_mod.check(res.new(.particle));
        const root = rig.fake.nodes.items[0].id;
        try bridge_mod.check(res.insertNode(root, "Track", 0, &rig.curve));
        const seed: GeometryValue = .{ .vec3 = @constCast(keys) };
        try bridge_mod.check(res.geometryWrite(rig.curve, .particle_keyframes, &seed));
        try rig.fake.setKeyframeKnobs(rig.curve, knobs);
        try rig.doc.reload(allocator, res);
        rig.editor = Editor.init(allocator, rig.curve);
        errdefer rig.editor.deinit();
        try rig.editor.load(res);
        return rig;
    }

    fn deinit(self: *Rig, allocator: std.mem.Allocator) void {
        self.editor.deinit();
        self.history.deinit(allocator);
        self.doc.deinit(allocator);
        self.fake.deinit();
    }

    fn bridge(self: *Rig) ResBridge {
        return self.fake.bridge();
    }

    /// The keys the bridge holds, owned by the test.
    fn stored(self: *Rig) !GeometryValue {
        return tools.readGeometry(self.bridge(), self.curve, .particle_keyframes);
    }

    fn undo(self: *Rig) !void {
        var entry = self.history.undo_stack.pop().?;
        try self.doc.undoOne(testing.allocator, self.bridge(), &entry.command);
        try self.history.redo_stack.append(testing.allocator, entry);
    }

    fn redo(self: *Rig) !void {
        var entry = self.history.redo_stack.pop().?;
        try self.doc.redoOne(testing.allocator, self.bridge(), &entry.command);
        try self.history.undo_stack.append(testing.allocator, entry);
    }

    fn release(self: *Rig) !void {
        try self.editor.release(self.bridge(), &self.doc, &self.history);
    }

    fn expectStored(self: *Rig, expected: []const Vec3) !void {
        var read = try self.stored();
        defer read.deinit(testing.allocator);
        try testing.expectEqualSlices(Vec3, expected, read.vec3);
    }
};

/// A 0..1 curve in steps of 0.1: 25 px per step is 250 px, so with a 400 x 200
/// window the bottom bar hides and the left bar shows (6 rows of 10).
const plain_knobs: KeyframeKnobs = .{ .min_x = 0, .max_x = 1, .min_y = 0, .max_y = 1, .step_x = 0.1, .step_y = 0.1 };
const three_keys = [_]Vec3{ .{ .x = 0, .y = 0.5 }, .{ .x = 0.4, .y = 0.2 }, .{ .x = 0.8, .y = 0.9 } };

/// Pixel x of a value under `plain_knobs` with no scrolling.
fn sx(x: f32) i32 {
    return left + pixel(x * 250);
}

test "mapping: hand values, both ways, and the scroll bars of plain knobs" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    // 10 steps of 25 px fit in 400 px; 10 rows of 25 px do not fit in 200.
    try testing.expect(!e.scroll_x.visible);
    try testing.expect(e.scroll_y.visible);
    try testing.expectEqual(@as(i32, 9), e.scroll_y.max);
    try testing.expectEqual(@as(i32, 6), e.scroll_y.page);
    // SetDimentions puts the left bar at max, which Windows clamps to the top
    // (max - page + 1), so values 0..0.6 show.
    try testing.expectEqual(@as(i32, 4), e.scroll_y.pos);
    const s = e.screenByValue(0.5, 0.5);
    try testing.expectEqual(@as(f32, 168), s.x);
    try testing.expectEqual(@as(f32, 45), s.y);
    const v = e.valueByScreen(168, 45);
    try testing.expectApproxEqAbs(@as(f32, 0.5), v.x, 1e-5);
    try testing.expectApproxEqAbs(@as(f32, 0.5), v.y, 1e-5);
}

test "dimensions: a wide window hides the bottom bar, a narrow one shows it, resize mode fits" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    e.setSize(150, 200);
    try testing.expect(e.scroll_x.visible);
    // (150 - 43 - 12.5) / 25 + 0.5 = 4.28 -> 4 steps on the screen of 10.
    try testing.expectEqual(@as(i32, 4), e.scroll_x.page);
    try testing.expectEqual(@as(i32, 9), e.scroll_x.max);
    e.setSize(400, 400);
    try testing.expect(!e.scroll_x.visible);
    try testing.expect(!e.scroll_y.visible);

    e.setXResizeMode(true);
    // (400 - 43 - 15) / (1 - 0) * 0.1
    try testing.expectApproxEqAbs(@as(f32, 34.2), e.xs, 1e-3);
    try testing.expect(!e.scroll_x.visible);
    e.setXResizeMode(false);
    try testing.expectEqual(@as(f32, 25), e.xs);
}

test "press: a key is hit on screen x only, takes the click's y clamped, and starts a drag" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    // 3 px off key 1 in x and far from it in y: still the hit.
    try e.press(sx(0.4) + 3, 10);
    try testing.expectEqual(Mode.drag, e.mode);
    try testing.expectEqual(@as(usize, 1), e.drag_index);
    try testing.expectEqual(@as(usize, 3), e.keys.items.len);
    // y 10 px from the top of a 200 px window is 0.5 + (200-10-30-... ) above the 0.6 top, clamped to 1.
    try testing.expect(e.keys.items[1].y >= 0 and e.keys.items[1].y <= 1);
    try testing.expectEqual(@as(f32, 0.4), e.keys.items[1].x);
    // A press below the window clamps up to MinY.
    e.cancel();
    try e.press(sx(0.4), 400);
    try testing.expectEqual(@as(f32, 0), e.keys.items[1].y);
    e.cancel();
    // 4 px off is a miss and inserts instead.
    try e.press(sx(0.4) + 4, 100);
    try testing.expectEqual(@as(usize, 4), e.keys.items.len);
}

test "press: a miss inside the ranges inserts sorted by x, outside it does nothing" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    try e.press(sx(0.6), 100);
    try testing.expectEqual(@as(usize, 4), e.keys.items.len);
    try testing.expectEqual(@as(usize, 2), e.drag_index);
    try testing.expectApproxEqAbs(@as(f32, 0.6), e.keys.items[2].x, 1e-4);
    try testing.expectEqual(@as(f32, 0.8), e.keys.items[3].x);
    e.cancel();
    try testing.expectEqual(@as(usize, 3), e.keys.items.len);
    // Left of the origin (x < MinX) and past MaxX: no key, no drag.
    try e.press(left - 20, 100);
    try testing.expectEqual(Mode.free, e.mode);
    try e.press(sx(1.0) + 30, 100);
    try testing.expectEqual(Mode.free, e.mode);
    try testing.expectEqual(@as(usize, 3), e.keys.items.len);
}

test "move: neighbours keep NODE_SPACE pixels, key 0 stays at x, x stops at MaxX" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    try e.press(sx(0.4), 100);
    // Drag far right: stops node_space px before key 2 (x 0.8).
    e.move(sx(0.8) + 100, 100);
    const gap = e.screenByValue(0.8, 0).x - e.screenByValue(e.keys.items[1].x, 0).x;
    try testing.expect(gap >= @as(f32, node_space) - 1 and gap <= @as(f32, node_space) + 1);
    // Far left: no closer than node_space to key 0.
    e.move(0, 100);
    const gap0 = e.screenByValue(e.keys.items[1].x, 0).x - e.screenByValue(0, 0).x;
    try testing.expect(gap0 >= @as(f32, node_space) - 1 and gap0 <= @as(f32, node_space) + 1);
    e.cancel();

    // Key 0 moves in y only.
    try e.press(left, 100);
    try testing.expectEqual(@as(usize, 0), e.drag_index);
    e.move(left + 80, 60);
    try testing.expectEqual(@as(f32, 0), e.keys.items[0].x);
    try testing.expectApproxEqAbs(@as(f32, 0.44), e.keys.items[0].y, 1e-4);
    e.cancel();

    // The last key cannot pass MaxX.
    try e.press(sx(0.8), 100);
    e.move(sx(0.8) + 300, 100);
    try testing.expectEqual(@as(f32, 1), e.keys.items[2].x);
}

test "move: y has no lower clamp short of the window edge and MaxY clamps above" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    // The bar is at pos 4: the window shows 0.. 0.6, and its bottom edge is value 0.
    try e.press(sx(0.4), 100);
    e.move(sx(0.4), 190);
    try testing.expect(e.keys.items[1].y <= 0.0001 and e.keys.items[1].y >= -0.0001);
    // Dragging up to the top scrolls the left bar one step toward the top.
    const before_pos = e.scroll_y.pos;
    e.move(sx(0.4), 0);
    try testing.expect(e.scroll_y.pos == before_pos - 1);
    try testing.expect(e.keys.items[1].y <= 1);
}

test "move: the left edge scrolls the bottom bar back and the right edge scrolls it forward" {
    // Ten steps of 25 px in a 150 px window: four on the screen, bar 0..9.
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    e.setSize(150, 200);
    try testing.expect(e.scroll_x.visible);
    try testing.expectEqual(@as(i32, 6), e.scroll_x.limit());
    e.scroll_x.set(2);
    // Key 1 (x 0.4) is step 4, drawn 2 steps from the left edge.
    const at = e.screenByValue(0.4, 0.2);
    try e.press(pixel(at.x), pixel(at.y));
    try testing.expectEqual(@as(usize, 1), e.drag_index);
    // Past the left edge (but clear of key 0): back one step, x pinned at LEFT.
    e.move(left - 10, pixel(at.y));
    try testing.expectEqual(@as(i32, 1), e.scroll_x.pos);
    // Key 2 is off the right edge, so the neighbour clamp wins there; with
    // the next key out of reach the edge of the screen scrolls forward.
    e.cancel();
    try e.press(pixel(e.screenByValue(0.8, 0.9).x), pixel(e.screenByValue(0.8, 0.9).y));
    try testing.expect(e.mode == .drag or e.mode == .free);
}

test "release: a drag of many moves is one undo step and undo/redo are exact" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    try e.press(sx(0.4), 100);
    e.move(sx(0.4) + 10, 90);
    e.move(sx(0.4) + 20, 80);
    e.move(sx(0.4) + 30, 70);
    // MFC tells the item on the release only: the bridge still has the old keys.
    try rig.expectStored(&three_keys);
    try rig.release();
    try testing.expectEqual(Mode.free, e.mode);
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    const moved = try testing.allocator.dupe(Vec3, e.keys.items);
    defer testing.allocator.free(moved);
    try rig.expectStored(moved);
    for (moved) |key| try testing.expectEqual(@as(f32, 0), key.z);
    try testing.expect(moved[1].x > 0.4);

    try rig.undo();
    try rig.expectStored(&three_keys);
    try rig.redo();
    try rig.expectStored(moved);
}

test "release: a click that changes nothing, and a press outside, add no undo step" {
    // A key already at the clicked y: the press writes the same value.
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    const at = e.screenByValue(0.4, 0.2);
    try e.press(pixel(at.x), pixel(at.y));
    e.keys.items[1].y = 0.2;
    try rig.release();
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
    try e.press(left - 30, 100);
    try rig.release();
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
}

test "add: one undo step; undo and redo restore the keys exactly" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    try e.press(sx(0.6), 100);
    try rig.release();
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    try testing.expectEqual(@as(usize, 4), e.keys.items.len);
    const added = try testing.allocator.dupe(Vec3, e.keys.items);
    defer testing.allocator.free(added);
    try rig.expectStored(added);
    try rig.undo();
    try rig.expectStored(&three_keys);
    try rig.redo();
    try rig.expectStored(added);
}

test "add: two keys never share x" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    // Every press inside the hit radius of a key edits it; none inserts a twin.
    var offset: i32 = -select_size;
    while (offset <= select_size) : (offset += 1) {
        try e.press(sx(0.4) + offset, 120);
        try testing.expectEqual(@as(usize, 3), e.keys.items.len);
        e.cancel();
    }
    try e.press(sx(0.6), 100);
    e.move(sx(0.8) + 50, 100);
    try rig.release();
    for (e.keys.items[1..], 1..) |key, i| try testing.expect(key.x > e.keys.items[i - 1].x);
}

test "delete: key 0 is refused, the last-touched key goes, and the index stays" {
    const keys = [_]Vec3{ .{ .x = 0, .y = 0.5 }, .{ .x = 0.2, .y = 0.2 }, .{ .x = 0.4, .y = 0.9 }, .{ .x = 0.6, .y = 0.1 } };
    var rig = try Rig.init(testing.allocator, &keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    // Nothing touched yet: index 0 is protected.
    try testing.expect(!try e.deleteActive(rig.bridge(), &rig.doc, &rig.history));
    try e.press(left, 100);
    try rig.release();
    try testing.expect(!try e.deleteActive(rig.bridge(), &rig.doc, &rig.history));
    try testing.expectEqual(@as(usize, 4), e.keys.items.len);

    const after_touch = try testing.allocator.dupe(Vec3, e.keys.items);
    defer testing.allocator.free(after_touch);
    const steps = rig.history.undo_stack.items.len;
    try e.press(sx(0.2), 100);
    try rig.release();
    const touched = try testing.allocator.dupe(Vec3, e.keys.items);
    defer testing.allocator.free(touched);
    try testing.expectEqual(@as(usize, 1), e.drag_index);
    try testing.expect(try e.deleteActive(rig.bridge(), &rig.doc, &rig.history));
    try testing.expectEqual(@as(usize, 3), e.keys.items.len);
    try testing.expectEqual(@as(f32, 0.4), e.keys.items[1].x);
    try rig.expectStored(e.keys.items);
    try testing.expect(rig.history.undo_stack.items.len > steps);

    try rig.undo();
    try rig.expectStored(touched);
    try rig.redo();
    try rig.expectStored(e.keys.items);

    // The index did not move: Delete again removes the key that slid into it.
    try testing.expect(try e.deleteActive(rig.bridge(), &rig.doc, &rig.history));
    try testing.expectEqual(@as(f32, 0.6), e.keys.items[1].x);
    try testing.expectEqual(@as(usize, 2), e.keys.items.len);
    try testing.expect(try e.deleteActive(rig.bridge(), &rig.doc, &rig.history));
    try testing.expectEqual(@as(usize, 1), e.keys.items.len);
    try testing.expect(!try e.deleteActive(rig.bridge(), &rig.doc, &rig.history));
}

test "reset all: keeps the first key, is one undo step, and refuses a curve of one key" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    try testing.expect(try e.resetAll(rig.bridge(), &rig.doc, &rig.history));
    try testing.expectEqual(@as(usize, 1), e.keys.items.len);
    try testing.expectEqual(three_keys[0], e.keys.items[0]);
    try rig.expectStored(three_keys[0..1]);
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    try testing.expect(!try e.resetAll(rig.bridge(), &rig.doc, &rig.history));
    try testing.expectEqual(@as(usize, 1), rig.history.undo_stack.items.len);
    try rig.undo();
    try rig.expectStored(&three_keys);
    try rig.redo();
    try rig.expectStored(three_keys[0..1]);
}

test "reset all: a stale delete index removes nothing" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    try e.press(sx(0.8), 100);
    try rig.release();
    try testing.expectEqual(@as(usize, 2), e.drag_index);
    _ = try e.resetAll(rig.bridge(), &rig.doc, &rig.history);
    try testing.expect(!try e.deleteActive(rig.bridge(), &rig.doc, &rig.history));
    try testing.expectEqual(@as(usize, 1), e.keys.items.len);
}

test "zoom: steps through the levels, stops at both ends, is not an undo step and leaves the keys" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    try testing.expectEqual(@as(f32, 25), e.xs);
    try testing.expect(e.zoomX(.in));
    try testing.expectEqual(@as(f32, 50), e.xs);
    try testing.expect(e.zoomX(.in));
    try testing.expectEqual(@as(f32, 100), e.xs);
    try testing.expect(!e.zoomX(.in));
    // 10 steps of 100 px in a 400 px window: the bottom bar shows.
    try testing.expect(e.scroll_x.visible);
    var steps: usize = 0;
    while (e.zoomX(.out)) steps += 1;
    try testing.expectEqual(@as(usize, zoom_levels.len - 1), steps);
    try testing.expectEqual(@as(f32, 5), e.xs);

    try testing.expect(e.zoomY(.out));
    try testing.expectEqual(@as(i32, 20), e.ys);
    while (e.zoomY(.in)) {}
    try testing.expectEqual(@as(i32, 100), e.ys);
    // Zoom touched the view only.
    try testing.expectEqual(@as(usize, 0), rig.history.undo_stack.items.len);
    try rig.expectStored(&three_keys);
    try testing.expectEqualSlices(Vec3, &three_keys, e.keys.items);

    // Resize mode fits the window, so zoom X has nothing to do.
    e.setXResizeMode(true);
    try testing.expect(!e.zoomX(.in));
}

test "hover: highlights the key under the cursor on x alone" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    const e = &rig.editor;
    e.hover(sx(0.8) - 2);
    try testing.expectEqual(@as(?usize, 2), e.high_index);
    e.hover(sx(0.6));
    try testing.expectEqual(@as(?usize, null), e.high_index);
}

test "load: a node that is not a curve is refused" {
    var rig = try Rig.init(testing.allocator, &three_keys, plain_knobs);
    defer rig.deinit(testing.allocator);
    var other = Editor.init(testing.allocator, rig.fake.nodes.items[0].id);
    defer other.deinit();
    try testing.expectError(error.BadArgument, other.load(rig.bridge()));
}
