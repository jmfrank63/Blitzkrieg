//! The anchored geometry of a game UI screen (the GUI sub-editor, kind gui),
//! pure and without the bridge: a window's stored rect is an offset and a
//! size relative to its parent and to the edges its PositionFlag names, and
//! every gesture of the editor works on the canvas rect, so each edit goes
//! canvas rect -> inverse -> the window's own ints.
//!
//! The horizontal anchor is `flag & 0xf` (LEFT 1, HMID 2, RIGHT 3), the
//! vertical one `flag & 0xf0` (TOP 0x10, VMID 0x20, BOTTOM 0x30), as
//! UI.h numbers them.
//!
//! Which resolver is ported: CGUIFrame::GetElementRect / SetElementRect
//! (GUIFrame2.cpp:16-123) are NOT used for the resolve. They disagree with
//! the engine's CSimpleWindow::Reposition (UIBasic.cpp:643-675), which is
//! what the Game draws: for RIGHT and BOTTOM they return an inverted rect
//! (x1 > x2, so SetElementRect then reads a negative width and cannot undo
//! GetElementRect), HMID and VMID leave out the parent's origin, and VMID
//! counts the offset upwards. The slice is proved by measuring a Game frame,
//! so the canvas follows Reposition: LEFT, TOP: edge + offset; RIGHT, BOTTOM:
//! the far edge is the parent's far edge minus the offset; HMID, VMID: the
//! centre is the parent's centre plus the offset, each edge truncated to an
//! int as `(int)` does there. A flag that names no anchor on an axis behaves
//! as LEFT or TOP (Reposition leaves the rect as it was). The resize hit
//! test and the resize clamps are ported from GUIFrame2.cpp:142-167 and
//! :437-625 as they are. MFC has no arrow-key nudge (OnKeyDown at :125 only
//! handles Delete) and no align or equal size handler, so `moveBy`, `align`
//! and `equalize` are new, working in canvas space against the primary
//! (first selected) window.
const std = @import("std");
const bridge_mod = @import("bridge.zig");

const GuiWindow = bridge_mod.GuiWindow;
const GuiRect = bridge_mod.GuiRect;

/// The canvas the UI screens are laid out on (the game's 1024 x 768).
pub const canvas_width: f32 = 1024;
pub const canvas_height: f32 = 768;

/// Hit test half width of a resize handle, GUIFrame2.cpp's WIDTH.
pub const handle_width: f32 = 3;
/// The smallest a window can be resized to, GUIFrame2.cpp's MINIMAL.
pub const minimal: f32 = 5;

pub const place = struct {
    pub const left: i32 = 0x01;
    pub const hmid: i32 = 0x02;
    pub const right: i32 = 0x03;
    pub const top: i32 = 0x10;
    pub const vmid: i32 = 0x20;
    pub const bottom: i32 = 0x30;
};

pub const Rect = struct {
    x1: f32,
    y1: f32,
    x2: f32,
    y2: f32,

    pub fn width(self: Rect) f32 {
        return self.x2 - self.x1;
    }

    pub fn height(self: Rect) f32 {
        return self.y2 - self.y1;
    }

    pub fn eql(a: Rect, b: Rect) bool {
        return a.x1 == b.x1 and a.y1 == b.y1 and a.x2 == b.x2 and a.y2 == b.y2;
    }
};

/// A window's own ints: offset and size, as BkResGuiWindows reports them.
pub const Local = struct { x: i32, y: i32, w: i32, h: i32 };

const Span = struct { a: f32, b: f32 };

/// One axis of Reposition. `anchor` is 1 (near edge), 2 (middle) or 3 (far
/// edge); anything else counts as the near edge.
fn resolveAxis(lo: f32, hi: f32, anchor: i32, pos: i32, size: i32) Span {
    const p: f32 = @floatFromInt(pos);
    const s: f32 = @floatFromInt(size);
    return switch (anchor) {
        3 => .{ .a = hi - p - s, .b = hi - p },
        2 => .{
            .a = @trunc(lo + p + (hi - lo) / 2 - s / 2),
            .b = @trunc(lo + p + (hi - lo) / 2 + s / 2),
        },
        else => .{ .a = lo + p, .b = lo + p + s },
    };
}

/// The canvas rect of a window with `flag`, `local` ints and parent canvas
/// rect `parent`.
pub fn resolve(parent: Rect, flag: i32, local: Local) Rect {
    const h = resolveAxis(parent.x1, parent.x2, flag & 0xf, local.x, local.w);
    const v = resolveAxis(parent.y1, parent.y2, (flag & 0xf0) >> 4, local.y, local.h);
    return .{ .x1 = h.a, .y1 = v.a, .x2 = h.b, .y2 = v.b };
}

fn toInt(value: f32) i32 {
    return @intFromFloat(@trunc(value));
}

/// The offset of one axis that puts the window on `[a, b]`. The middle
/// anchor truncates both edges, so the rect can be one pixel narrower than
/// the stored size and the exact offset can sit half a pixel off an integer.
/// There the stored size `hint` is tried first, so a move does not shrink the
/// window, then the rect's own width; the offset that resolves back to the
/// same edges wins.
fn inverseAxis(lo: f32, hi: f32, anchor: i32, a: f32, b: f32, hint: i32) struct { pos: i32, size: i32 } {
    const size = toInt(b - a);
    switch (anchor) {
        3 => return .{ .pos = toInt(hi - b), .size = size },
        2 => {
            const sizes = [_]i32{ hint, size, size + 1 };
            for (sizes) |candidate_size| {
                const centre = a + @as(f32, @floatFromInt(candidate_size)) / 2;
                const guess = toInt(@floor(centre - (lo + (hi - lo) / 2)));
                var candidate = guess - 1;
                while (candidate <= guess + 1) : (candidate += 1) {
                    const got = resolveAxis(lo, hi, 2, candidate, candidate_size);
                    if (got.a == a and got.b == b) return .{ .pos = candidate, .size = candidate_size };
                }
            }
            const centre = a + @as(f32, @floatFromInt(size)) / 2;
            return .{ .pos = toInt(@floor(centre - (lo + (hi - lo) / 2))), .size = size };
        },
        else => return .{ .pos = toInt(a - lo), .size = size },
    }
}

/// The inverse of `resolve`: the ints a window with `flag` stores to sit on
/// `rc` under a parent at `parent`. `current` is the window's ints now; it
/// only decides between sizes that render the same on a middle anchor.
pub fn inverse(parent: Rect, flag: i32, rc: Rect, current: Local) Local {
    const h = inverseAxis(parent.x1, parent.x2, flag & 0xf, rc.x1, rc.x2, current.w);
    const v = inverseAxis(parent.y1, parent.y2, (flag & 0xf0) >> 4, rc.y1, rc.y2, current.h);
    return .{ .x = h.pos, .y = v.pos, .w = h.size, .h = v.size };
}

fn indexOf(windows: []const GuiWindow, id: i32) ?usize {
    for (windows, 0..) |w, i| if (w.id == id) return i;
    return null;
}

/// The window with `id`, or null.
pub fn find(windows: []const GuiWindow, id: i32) ?GuiWindow {
    const i = indexOf(windows, id) orelse return null;
    return windows[i];
}

/// A parent chain longer than this is a cycle in a broken list.
const max_depth = 64;

/// The canvas rect of window `id`, resolved up its parent chain. The root
/// (no parent) is its own rect, as GetElementRect returns it. Null when the
/// id or one of its parents is missing.
pub fn canvasRect(windows: []const GuiWindow, id: i32) ?Rect {
    return canvasRectAt(windows, id, 0);
}

fn canvasRectAt(windows: []const GuiWindow, id: i32, depth: usize) ?Rect {
    if (depth > max_depth) return null;
    const w = find(windows, id) orelse return null;
    const local: Local = .{ .x = w.x, .y = w.y, .w = w.w, .h = w.h };
    if (w.parent < 0) {
        const x: f32 = @floatFromInt(w.x);
        const y: f32 = @floatFromInt(w.y);
        // A root without a WindowPos size (MainMenu's) fills the screen, so children clamp to it and not to nothing.
        const rw: f32 = if (w.w > 0) @floatFromInt(w.w) else canvas_width;
        const rh: f32 = if (w.h > 0) @floatFromInt(w.h) else canvas_height;
        return .{ .x1 = x, .y1 = y, .x2 = x + rw, .y2 = y + rh };
    }
    const parent = canvasRectAt(windows, w.parent, depth + 1) orelse return null;
    return resolve(parent, w.flag, local);
}

/// The GuiRect that puts window `id` on the canvas rect `rc`, anchor flag
/// unchanged. Null when the id, its parent or the root case is missing: the
/// root has no anchor and is never moved.
pub fn rectFor(windows: []const GuiWindow, id: i32, rc: Rect) ?GuiRect {
    const w = find(windows, id) orelse return null;
    if (w.parent < 0) return null;
    const parent = canvasRect(windows, w.parent) orelse return null;
    const local = inverse(parent, w.flag, rc, .{ .x = w.x, .y = w.y, .w = w.w, .h = w.h });
    return .{ .id = id, .flag = w.flag, .x = local.x, .y = local.y, .w = local.w, .h = local.h };
}

// --- Resize handles ----------------------------------------------------------

pub const Handle = enum {
    none,
    left,
    top,
    right,
    bottom,
    left_top,
    right_top,
    right_bottom,
    left_bottom,
};

fn near(v: f32, center: f32) bool {
    return v >= center - handle_width and v <= center + handle_width;
}

/// GetResizeMode: which of the eight handles of `rc` the point is on. The
/// corners win over the edge middles; the middles are the truncated integer
/// centres, as MFC computes them.
pub fn hitTest(rc: Rect, x: f32, y: f32) Handle {
    if (near(x, rc.x1) and near(y, rc.y1)) return .left_top;
    if (near(x, rc.x2) and near(y, rc.y1)) return .right_top;
    if (near(x, rc.x1) and near(y, rc.y2)) return .left_bottom;
    if (near(x, rc.x2) and near(y, rc.y2)) return .right_bottom;

    const cx: f32 = @trunc((rc.x2 + rc.x1) / 2);
    const cy: f32 = @trunc((rc.y2 + rc.y1) / 2);

    if (near(x, cx) and near(y, rc.y1)) return .top;
    if (near(x, cx) and near(y, rc.y2)) return .bottom;
    if (near(x, rc.x1) and near(y, cy)) return .left;
    if (near(x, rc.x2) and near(y, cy)) return .right;
    return .none;
}

/// One edge of the resize switch: `edge` moves by `delta` unless that leaves
/// less than MINIMAL between it and `other`, and stops at `limit` instead of
/// passing it. `strict` is whether MFC refuses an exact MINIMAL (`<` and `>`)
/// or allows it (`<=` and `>=`); the cases differ by a pixel in the source
/// and are kept as they are.
fn moveNear(edge: *f32, other: f32, delta: f32, limit: f32, strict: bool) void {
    const moved = edge.* + delta;
    const ok = if (strict) moved + minimal < other else moved + minimal <= other;
    if (!ok) return;
    edge.* = if (moved >= limit) moved else limit;
}

fn moveFar(edge: *f32, other: f32, delta: f32, limit: f32, strict: bool) void {
    const moved = edge.* + delta;
    const ok = if (strict) moved > other + minimal else moved >= other + minimal;
    if (!ok) return;
    edge.* = if (moved <= limit) moved else limit;
}

/// The rect after dragging `handle` of `rc` by (`dx`, `dy`) with the
/// pointer, inside `client` (the container's canvas rect). An edge that
/// would leave less than MINIMAL stays where it was; one that would pass
/// `client` stops on it.
pub fn resize(rc: Rect, handle: Handle, dx: f32, dy: f32, client: Rect) Rect {
    var r = rc;
    switch (handle) {
        .none => {},
        .left_top => {
            moveNear(&r.x1, r.x2, dx, client.x1, true);
            moveNear(&r.y1, r.y2, dy, client.y1, true);
        },
        .right_top => {
            moveFar(&r.x2, r.x1, dx, client.x2, true);
            moveNear(&r.y1, r.y2, dy, client.y1, true);
        },
        .left_bottom => {
            moveNear(&r.x1, r.x2, dx, client.x1, false);
            moveFar(&r.y2, r.y1, dy, client.y2, false);
        },
        .right_bottom => {
            moveFar(&r.x2, r.x1, dx, client.x2, true);
            moveFar(&r.y2, r.y1, dy, client.y2, false);
        },
        .left => moveNear(&r.x1, r.x2, dx, client.x1, false),
        .top => moveNear(&r.y1, r.y2, dy, client.y1, true),
        .right => moveFar(&r.x2, r.x1, dx, client.x2, true),
        .bottom => moveFar(&r.y2, r.y1, dy, client.y2, false),
    }
    return r;
}

// --- Gestures on a selection --------------------------------------------------

pub const Align = enum { left, top, right, bottom };
pub const Equal = enum { width, height, size };

/// Every GuiRect a selection edit produced; the caller frees `items`.
pub const Edits = struct {
    items: []GuiRect,

    pub fn deinit(self: Edits, allocator: std.mem.Allocator) void {
        allocator.free(self.items);
    }
};

fn shifted(rc: Rect, dx: f32, dy: f32) Rect {
    return .{ .x1 = rc.x1 + dx, .y1 = rc.y1 + dy, .x2 = rc.x2 + dx, .y2 = rc.y2 + dy };
}

/// The rects that move every window of `ids` by (`dx`, `dy`) on the canvas.
/// A window that is not in the list, or the root, is skipped.
pub fn moveBy(allocator: std.mem.Allocator, windows: []const GuiWindow, ids: []const i32, dx: i32, dy: i32) std.mem.Allocator.Error!Edits {
    var out: std.ArrayListUnmanaged(GuiRect) = .empty;
    errdefer out.deinit(allocator);
    for (ids) |id| {
        const rc = canvasRect(windows, id) orelse continue;
        const edit = rectFor(windows, id, shifted(rc, @floatFromInt(dx), @floatFromInt(dy))) orelse continue;
        try out.append(allocator, edit);
    }
    return .{ .items = try out.toOwnedSlice(allocator) };
}

/// Aligns every window of `ids` to the primary, the first id, on one canvas
/// edge, size kept. The primary itself is not edited.
pub fn align_(allocator: std.mem.Allocator, windows: []const GuiWindow, ids: []const i32, mode: Align) std.mem.Allocator.Error!Edits {
    var out: std.ArrayListUnmanaged(GuiRect) = .empty;
    errdefer out.deinit(allocator);
    if (ids.len < 2) return .{ .items = try out.toOwnedSlice(allocator) };
    const primary = canvasRect(windows, ids[0]) orelse return .{ .items = try out.toOwnedSlice(allocator) };
    for (ids[1..]) |id| {
        const rc = canvasRect(windows, id) orelse continue;
        const target = switch (mode) {
            .left => shifted(rc, primary.x1 - rc.x1, 0),
            .right => shifted(rc, primary.x2 - rc.x2, 0),
            .top => shifted(rc, 0, primary.y1 - rc.y1),
            .bottom => shifted(rc, 0, primary.y2 - rc.y2),
        };
        const edit = rectFor(windows, id, target) orelse continue;
        try out.append(allocator, edit);
    }
    return .{ .items = try out.toOwnedSlice(allocator) };
}

/// Gives every window of `ids` the primary's width, height or both, the top
/// left corner kept. The primary itself is not edited.
pub fn equalize(allocator: std.mem.Allocator, windows: []const GuiWindow, ids: []const i32, mode: Equal) std.mem.Allocator.Error!Edits {
    var out: std.ArrayListUnmanaged(GuiRect) = .empty;
    errdefer out.deinit(allocator);
    if (ids.len < 2) return .{ .items = try out.toOwnedSlice(allocator) };
    const primary = canvasRect(windows, ids[0]) orelse return .{ .items = try out.toOwnedSlice(allocator) };
    for (ids[1..]) |id| {
        const rc = canvasRect(windows, id) orelse continue;
        var target = rc;
        if (mode == .width or mode == .size) target.x2 = rc.x1 + primary.width();
        if (mode == .height or mode == .size) target.y2 = rc.y1 + primary.height();
        const edit = rectFor(windows, id, target) orelse continue;
        try out.append(allocator, edit);
    }
    return .{ .items = try out.toOwnedSlice(allocator) };
}

// --- Tests ------------------------------------------------------------------

const testing = std.testing;
const parent_rect: Rect = .{ .x1 = 100, .y1 = 50, .x2 = 1124, .y2 = 818 }; // 1024 x 768 at (100, 50)

fn expectRect(want: Rect, got: Rect, flag: i32) !void {
    if (!want.eql(got)) {
        std.debug.print("flag 0x{x}: want ({d}, {d}, {d}, {d}) got ({d}, {d}, {d}, {d})\n", .{ flag, want.x1, want.y1, want.x2, want.y2, got.x1, got.y1, got.x2, got.y2 });
        return error.TestExpectedEqual;
    }
}

test "resolve and inverse on all nine anchor combinations against hand derived rects" {
    // Window ints (30, 20) size (200, 60) under the parent (100, 50)-(1124, 818),
    // 1024 wide and 768 high. UIBasic.cpp:643-675 (CSimpleWindow::Reposition).
    //   LEFT   x1 = 100 + 30 = 130, x2 = 330
    //   HMID   centre 100 + 512 + 30 = 642, x1 = 542, x2 = 742
    //   RIGHT  x2 = 1124 - 30 = 1094, x1 = 894
    //   TOP    y1 = 50 + 20 = 70, y2 = 130
    //   VMID   centre 50 + 384 + 20 = 454, y1 = 424, y2 = 484
    //   BOTTOM y2 = 818 - 20 = 798, y1 = 738
    const local: Local = .{ .x = 30, .y = 20, .w = 200, .h = 60 };
    const horizontal = [_]struct { flag: i32, x1: f32, x2: f32 }{
        .{ .flag = place.left, .x1 = 130, .x2 = 330 },
        .{ .flag = place.hmid, .x1 = 542, .x2 = 742 },
        .{ .flag = place.right, .x1 = 894, .x2 = 1094 },
    };
    const vertical = [_]struct { flag: i32, y1: f32, y2: f32 }{
        .{ .flag = place.top, .y1 = 70, .y2 = 130 },
        .{ .flag = place.vmid, .y1 = 424, .y2 = 484 },
        .{ .flag = place.bottom, .y1 = 738, .y2 = 798 },
    };
    for (horizontal) |h| for (vertical) |v| {
        const flag = h.flag | v.flag;
        const want: Rect = .{ .x1 = h.x1, .y1 = v.y1, .x2 = h.x2, .y2 = v.y2 };
        const got = resolve(parent_rect, flag, local);
        try expectRect(want, got, flag);
        const back = inverse(parent_rect, flag, want, local);
        try testing.expectEqual(local, back);
    };
}

test "inverse(resolve(x)) is x for every anchor, with odd parents and sizes" {
    const parents = [_]Rect{
        parent_rect,
        .{ .x1 = 0, .y1 = 0, .x2 = 1025, .y2 = 767 },
        .{ .x1 = 7, .y1 = 3, .x2 = 400, .y2 = 331 },
    };
    const anchors_h = [_]i32{ place.left, place.hmid, place.right };
    const anchors_v = [_]i32{ place.top, place.vmid, place.bottom };
    for (parents) |parent| for (anchors_h) |ah| for (anchors_v) |av| {
        var x: i32 = -9;
        while (x <= 9) : (x += 3) {
            var w: i32 = 0;
            while (w <= 7) : (w += 1) {
                const local: Local = .{ .x = x, .y = x + 1, .w = w + 40, .h = w + 21 };
                const flag = ah | av;
                const rc = resolve(parent, flag, local);
                try testing.expectEqual(local, inverse(parent, flag, rc, local));
            }
        }
    };
}

test "three MainMenu.xml windows resolve to their known canvas places" {
    // Data/UI/MainMenu.xml under the 1024 x 768 root.
    const root: Rect = .{ .x1 = 0, .y1 = 0, .x2 = 1024, .y2 = 768 };
    // The header text, PositionFlag 0x0011, WindowPos (610, 95),
    // WindowSize (400, 172) -> (610, 95)-(1010, 267).
    try expectRect(.{ .x1 = 610, .y1 = 95, .x2 = 1010, .y2 = 267 }, resolve(root, 0x0011, .{ .x = 610, .y = 95, .w = 400, .h = 172 }), 0x11);
    // The COPYRIGHT WARNING text, PositionFlag 0x0033 (right, bottom),
    // WindowPos (10, 54), WindowSize (1004, 50) -> x2 = 1024 - 10 = 1014,
    // y2 = 768 - 54 = 714.
    try expectRect(.{ .x1 = 10, .y1 = 664, .x2 = 1014, .y2 = 714 }, resolve(root, 0x0033, .{ .x = 10, .y = 54, .w = 1004, .h = 50 }), 0x33);
    // Element 10006 (PositionFlag 0x0012: middle, top, WindowPos (0, 260),
    // WindowSize (320, 50)) inside the main menu panel 2000 (0x0011,
    // (610, 267), 400 x 328 -> (610, 267)-(1010, 595)): centre x
    // 610 + 200 + 0 = 810, so x 650..970; y 267 + 260 = 527..577.
    const panel = resolve(root, 0x0011, .{ .x = 610, .y = 267, .w = 400, .h = 328 });
    try expectRect(.{ .x1 = 610, .y1 = 267, .x2 = 1010, .y2 = 595 }, panel, 0x11);
    try expectRect(.{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 }, resolve(panel, 0x0012, .{ .x = 0, .y = 260, .w = 320, .h = 50 }), 0x12);
}

fn win(id: i32, parent: i32, flag: i32, x: i32, y: i32, w: i32, h: i32) GuiWindow {
    return .{ .id = id, .parent = parent, .class_type = 0, .element_id = 0, .flag = flag, .x = x, .y = y, .w = w, .h = h, .visible = 1 };
}

test "canvasRect walks the parent chain and rectFor inverts per anchor" {
    const windows = [_]GuiWindow{
        win(0, -1, 0, 0, 0, 1024, 768),
        win(1, 0, 0x11, 610, 267, 400, 328),
        win(2, 1, 0x12, 0, 260, 320, 50),
        win(3, 1, 0x33, 10, 20, 100, 40),
    };
    try expectRect(.{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 }, canvasRect(&windows, 2).?, 0x12);
    // Window 3 sits on the panel's right, bottom: x2 = 1010 - 10, y2 = 595 - 20.
    try expectRect(.{ .x1 = 900, .y1 = 535, .x2 = 1000, .y2 = 575 }, canvasRect(&windows, 3).?, 0x33);
    // Moving window 3 to the canvas (700, 300)-(760, 330) stores its offset
    // from the panel's far corner: 1010 - 760 = 250, 595 - 330 = 265.
    const edit = rectFor(&windows, 3, .{ .x1 = 700, .y1 = 300, .x2 = 760, .y2 = 330 }).?;
    try testing.expectEqual(GuiRect{ .id = 3, .flag = 0x33, .x = 250, .y = 265, .w = 60, .h = 30 }, edit);
    try testing.expect(canvasRect(&windows, 9) == null);
    try testing.expect(rectFor(&windows, 0, .{ .x1 = 0, .y1 = 0, .x2 = 1, .y2 = 1 }) == null);
}

test "hit test finds the eight handles and misses the inside" {
    const rc: Rect = .{ .x1 = 100, .y1 = 100, .x2 = 200, .y2 = 160 };
    try testing.expectEqual(Handle.left_top, hitTest(rc, 98, 103));
    try testing.expectEqual(Handle.right_top, hitTest(rc, 203, 100));
    try testing.expectEqual(Handle.left_bottom, hitTest(rc, 100, 163));
    try testing.expectEqual(Handle.right_bottom, hitTest(rc, 200, 160));
    try testing.expectEqual(Handle.top, hitTest(rc, 150, 98));
    try testing.expectEqual(Handle.bottom, hitTest(rc, 152, 160));
    try testing.expectEqual(Handle.left, hitTest(rc, 100, 130));
    try testing.expectEqual(Handle.right, hitTest(rc, 201, 128));
    try testing.expectEqual(Handle.none, hitTest(rc, 150, 130));
    try testing.expectEqual(Handle.none, hitTest(rc, 104, 104));
}

test "resize clamps at MINIMAL and at the client rect" {
    const client: Rect = .{ .x1 = 0, .y1 = 0, .x2 = 1024, .y2 = 768 };
    const rc: Rect = .{ .x1 = 100, .y1 = 100, .x2 = 200, .y2 = 160 };
    // Right edge dragged far left: 100 + 5 is the least, so it stays.
    try expectRect(rc, resize(rc, .right, -95, 0, client), 0);
    try expectRect(.{ .x1 = 100, .y1 = 100, .x2 = 106, .y2 = 160 }, resize(rc, .right, -94, 0, client), 0);
    // Left edge: `<=` for the plain handle, so a width of exactly 5 is allowed.
    try expectRect(.{ .x1 = 195, .y1 = 100, .x2 = 200, .y2 = 160 }, resize(rc, .left, 95, 0, client), 0);
    // Top edge: `<` refuses a height of exactly 5.
    try expectRect(rc, resize(rc, .top, 55, 55, client), 0);
    // Bottom edge `>=`: a height of exactly 5 is allowed.
    try expectRect(.{ .x1 = 100, .y1 = 100, .x2 = 200, .y2 = 105 }, resize(rc, .bottom, 0, -55, client), 0);
    // Past the client rect the edge stops on it.
    try expectRect(.{ .x1 = 0, .y1 = 100, .x2 = 200, .y2 = 160 }, resize(rc, .left, -500, 0, client), 0);
    try expectRect(.{ .x1 = 100, .y1 = 100, .x2 = 1024, .y2 = 768 }, resize(rc, .right_bottom, 5000, 5000, client), 0);
    // A corner moves both axes.
    try expectRect(.{ .x1 = 110, .y1 = 120, .x2 = 200, .y2 = 160 }, resize(rc, .left_top, 10, 20, client), 0);
    try expectRect(rc, resize(rc, .none, 10, 20, client), 0);
}

test "align and equal size work in canvas space on mixed anchors" {
    const allocator = testing.allocator;
    const windows = [_]GuiWindow{
        win(0, -1, 0, 0, 0, 1024, 768),
        // Primary: left, top, canvas (100, 200)-(300, 260).
        win(1, 0, 0x11, 100, 200, 200, 60),
        // Right, bottom: canvas (574, 478)-(974, 718), x2 = 1024 - 50, y2 = 768 - 50, size 400 x 240.
        win(2, 0, 0x33, 50, 50, 400, 240),
        // Middle, middle: canvas centre (512 + 10, 384 - 5), size 100 x 40, so (472, 359)-(572, 399).
        win(3, 0, 0x22, 10, -5, 100, 40),
    };
    const ids = [_]i32{ 1, 2, 3 };

    const left = try align_(allocator, &windows, &ids, .left);
    defer left.deinit(allocator);
    try testing.expectEqual(@as(usize, 2), left.items.len);
    // Window 2 to x1 = 100 keeps width 400: x2 = 500, so offset 1024 - 500 = 524.
    try testing.expectEqual(GuiRect{ .id = 2, .flag = 0x33, .x = 524, .y = 50, .w = 400, .h = 240 }, left.items[0]);
    // Window 3 to x1 = 100, width 100: centre 150, offset 150 - 512 = -362.
    try testing.expectEqual(GuiRect{ .id = 3, .flag = 0x22, .x = -362, .y = -5, .w = 100, .h = 40 }, left.items[1]);

    const bottom = try align_(allocator, &windows, &ids, .bottom);
    defer bottom.deinit(allocator);
    // Bottom edge 260: window 2 offset 768 - 260 = 508; window 3 centre 240, offset 240 - 384 = -144.
    try testing.expectEqual(GuiRect{ .id = 2, .flag = 0x33, .x = 50, .y = 508, .w = 400, .h = 240 }, bottom.items[0]);
    try testing.expectEqual(GuiRect{ .id = 3, .flag = 0x22, .x = 10, .y = -144, .w = 100, .h = 40 }, bottom.items[1]);

    const right = try align_(allocator, &windows, &ids, .right);
    defer right.deinit(allocator);
    // Right edge 300: window 2 offset 1024 - 300 = 724; window 3 x1 = 200, centre 250, offset -262.
    try testing.expectEqual(@as(i32, 724), right.items[0].x);
    try testing.expectEqual(@as(i32, -262), right.items[1].x);

    const top = try align_(allocator, &windows, &ids, .top);
    defer top.deinit(allocator);
    // Top edge 200: window 2 y1 = 200, y2 = 440, offset 768 - 440 = 328; window 3 centre 220, offset -164.
    try testing.expectEqual(@as(i32, 328), top.items[0].y);
    try testing.expectEqual(@as(i32, -164), top.items[1].y);

    const same = try equalize(allocator, &windows, &ids, .size);
    defer same.deinit(allocator);
    // Top left kept, size 200 x 60. Window 2 top left (574, 478): x2 = 774, offset 250; y2 = 538, offset 230.
    try testing.expectEqual(GuiRect{ .id = 2, .flag = 0x33, .x = 250, .y = 230, .w = 200, .h = 60 }, same.items[0]);
    // Window 3 top left (472, 359): centre x 472 + 100 = 572 (offset 60), centre y 359 + 30 = 389 (offset 5).
    try testing.expectEqual(GuiRect{ .id = 3, .flag = 0x22, .x = 60, .y = 5, .w = 200, .h = 60 }, same.items[1]);

    const wide = try equalize(allocator, &windows, &ids, .width);
    defer wide.deinit(allocator);
    // Width only: window 2 keeps its height 240 and its y; x2 = 774, offset 250.
    try testing.expectEqual(GuiRect{ .id = 2, .flag = 0x33, .x = 250, .y = 50, .w = 200, .h = 240 }, wide.items[0]);
    const tall = try equalize(allocator, &windows, &ids, .height);
    defer tall.deinit(allocator);
    try testing.expectEqual(@as(i32, 60), tall.items[0].h);
    try testing.expectEqual(@as(i32, 400), tall.items[0].w);

    // One window or none: nothing to align to.
    const alone = try align_(allocator, &windows, ids[0..1], .left);
    defer alone.deinit(allocator);
    try testing.expectEqual(@as(usize, 0), alone.items.len);
}

test "moveBy shifts the canvas by the same amount on every anchor" {
    const allocator = testing.allocator;
    const windows = [_]GuiWindow{
        win(0, -1, 0, 0, 0, 1024, 768),
        win(1, 0, 0x11, 100, 200, 200, 60),
        win(2, 0, 0x33, 50, 50, 400, 240),
    };
    const ids = [_]i32{ 1, 2, 0 };
    const moved = try moveBy(allocator, &windows, &ids, 7, -3);
    defer moved.deinit(allocator);
    // The root is skipped. A far-edge window moves the other way in its ints.
    try testing.expectEqual(@as(usize, 2), moved.items.len);
    try testing.expectEqual(GuiRect{ .id = 1, .flag = 0x11, .x = 107, .y = 197, .w = 200, .h = 60 }, moved.items[0]);
    try testing.expectEqual(GuiRect{ .id = 2, .flag = 0x33, .x = 43, .y = 53, .w = 400, .h = 240 }, moved.items[1]);
}

test "a move keeps an odd stored size on a middle anchor" {
    // 41 wide in a 1024 parent renders 40 wide (both edges truncate), so the
    // rect alone would store 40; the stored size is what a move keeps.
    const allocator = testing.allocator;
    const windows = [_]GuiWindow{
        win(0, -1, 0, 0, 0, 1024, 768),
        win(1, 0, 0x22, 0, 0, 41, 31),
    };
    const ids = [_]i32{1};
    const moved = try moveBy(allocator, &windows, &ids, 3, 2);
    defer moved.deinit(allocator);
    try testing.expectEqual(GuiRect{ .id = 1, .flag = 0x22, .x = 3, .y = 2, .w = 41, .h = 31 }, moved.items[0]);
}
