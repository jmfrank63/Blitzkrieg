//! The Fields tool (M3, D-21): the MFC editor's CFieldsState polygon
//! machine (StateTerrainFields.cpp) as a tool. Three states - Add draws the
//! pending polygon, Select picks a vertex, Edit drags one - with the MFC's
//! own keys: click adds a vertex, right-click removes the last, double-click
//! or Enter closes (the UniquePolygon+area rule), Esc clears but keeps the
//! last point, Insert bisects the dragged vertex's next edge, Delete removes
//! the dragged vertex. The points are WORLD (Vis) units, z on the ground is
//! the view's business (the tools' Pointer carries the world position the
//! ground answered).
const std = @import("std");
const bridge_mod = @import("bridge.zig");

pub const max_points = 64;

/// The tool's three states, the MFC editor's own names.
pub const State = enum { add, select, edit };

pub const Fields = struct {
    state: State = .add,
    /// The pending polygon, WORLD units, in draw order. In `add` the last
    /// point is the cursor's rubber point (the MFC's mouse-move point).
    pending: [max_points]bridge_mod.FieldVec3 = undefined,
    pending_len: usize = 0,
    /// The vertex Select/Edit is working on; -1 none (the MFC's own
    /// INVALID_INDEX).
    current: i32 = -1,
    /// The drag's grab offset, so the vertex follows the cursor exactly.
    grab_dx: f32 = 0,
    grab_dy: f32 = 0,

    pub fn points(self: *const Fields) []const bridge_mod.FieldVec3 {
        return self.pending[0..self.pending_len];
    }

    pub fn clear(self: *Fields) void {
        self.pending_len = 0;
        self.current = -1;
        self.state = .add;
        self.grab_dx = 0;
        self.grab_dy = 0;
    }

    /// Add: a click adds a vertex at the world point.
    pub fn vertexAdd(self: *Fields, wx: f32, wy: f32) bool {
        if (self.pending_len >= max_points) return false;
        self.pending[self.pending_len] = .{ .x = wx, .y = wy, .z = 0 };
        self.pending_len += 1;
        return true;
    }

    /// Add: a right-click takes the last vertex back and leaves the cursor
    /// as the rubber point (the MFC's double-pop+push).
    pub fn vertexRelease(self: *Fields, wx: f32, wy: f32) void {
        if (self.pending_len > 0) self.pending_len -= 1;
        if (self.pending_len > 0) self.pending_len -= 1;
        _ = self.vertexAdd(wx, wy);
    }

    /// The MFC's closing rule (StateTerrainFields.cpp:176-185): the polygon
    /// closes when, after deduping within a quarter cell, more than two
    /// points remain and the area is real. The dedupe itself is the bridge's
    /// (UniquePolygon); the tool only needs the count.
    pub fn closes(self: *const Fields) bool {
        return self.pending_len > 2;
    }

    /// Double-click or Enter: close into Select, when the polygon is real.
    pub fn tryClose(self: *Fields) bool {
        if (!self.closes()) return false;
        self.current = -1;
        self.state = .select;
        return true;
    }

    /// Esc: clear but keep the last point, as the MFC's Add state kept it.
    pub fn escape(self: *Fields) void {
        if (self.pending_len == 0) return;
        const last = self.pending[self.pending_len - 1];
        self.pending_len = 0;
        self.pending[0] = last;
        self.pending_len = 1;
        self.state = .add;
        self.current = -1;
    }

    /// Select: a click picks the vertex within a quarter cell; false keeps
    /// the state.
    pub fn pick(self: *Fields, wx: f32, wy: f32, radius: f32) bool {
        var i: i32 = @intCast(self.pending_len);
        while (i > 0) {
            i -= 1;
            const p = self.pending[@intCast(i)];
            const dx = p.x - wx;
            const dy = p.y - wy;
            if (dx * dx + dy * dy <= radius * radius) {
                self.current = i;
                self.grab_dx = p.x - wx;
                self.grab_dy = p.y - wy;
                self.state = .edit;
                return true;
            }
        }
        return false;
    }

    /// Edit: the drag moves the grabbed vertex (the view keeps z on the
    /// ground - CVSOBuilder::UpdateZ's rule).
    pub fn dragTo(self: *Fields, wx: f32, wy: f32) bool {
        if (self.current < 0 or self.current >= @as(i32, @intCast(self.pending_len))) return false;
        self.pending[@intCast(self.current)] = .{ .x = wx + self.grab_dx, .y = wy + self.grab_dy, .z = 0 };
        return true;
    }

    /// Edit: the drag ends - back to Select.
    pub fn dragEnd(self: *Fields) void {
        self.current = -1;
        self.grab_dx = 0;
        self.grab_dy = 0;
        self.state = .select;
    }

    /// Edit, Insert: bisect the dragged vertex's next edge at the midpoint
    /// (the MFC's own midpoint; the z is the ground's business again).
    pub fn insertVertex(self: *Fields) bool {
        if (self.current < 0 or self.current >= @as(i32, @intCast(self.pending_len))) return false;
        if (self.pending_len >= max_points) return false;
        const a = self.pending[@intCast(self.current)];
        const b = self.pending[@intCast(@mod(self.current + 1, @as(i32, @intCast(self.pending_len))))];
        const mid = bridge_mod.FieldVec3{ .x = (a.x + b.x) / 2, .y = (a.y + b.y) / 2, .z = 0 };
        const at: usize = @intCast(self.current + 1);
        std.mem.copyBackwards(bridge_mod.FieldVec3, self.pending[at + 1 .. self.pending_len + 1], self.pending[at..self.pending_len]);
        self.pending[at] = mid;
        self.pending_len += 1;
        return true;
    }

    /// Edit, Delete: remove the dragged vertex, back to Select.
    pub fn deleteVertex(self: *Fields) bool {
        if (self.current < 0 or self.current >= @as(i32, @intCast(self.pending_len))) return false;
        const at: usize = @intCast(self.current);
        std.mem.copyForwards(bridge_mod.FieldVec3, self.pending[at .. self.pending_len - 1], self.pending[at + 1 .. self.pending_len]);
        self.pending_len -= 1;
        self.current = -1;
        self.grab_dx = 0;
        self.grab_dy = 0;
        self.state = .select;
        return true;
    }

    /// The polygon as the apply's points (WORLD xy, z ignored), when the
    /// pending one closes.
    pub fn applyPoints(self: *const Fields, out: *[max_points]bridge_mod.FieldVec3) ?usize {
        if (!self.closes()) return null;
        @memcpy(out[0..self.pending_len], self.pending[0..self.pending_len]);
        return self.pending_len;
    }
};

test "fields: the MFC polygon keys" {
    var fields = Fields{};
    // Add: clicks, the rubber point's right-click, Esc keeps the last.
    _ = fields.vertexAdd(1, 1);
    _ = fields.vertexAdd(5, 1);
    _ = fields.vertexAdd(5, 5);
    try std.testing.expectEqual(@as(usize, 3), fields.points().len);
    fields.vertexRelease(4, 4);
    // Two real vertices went in, the third was the rubber: the right-click
    // takes the rubber and the last real, and the cursor becomes the rubber.
    try std.testing.expectEqual(@as(usize, 2), fields.points().len);
    try std.testing.expectEqual(@as(f32, 4), fields.points()[1].x);
    fields.escape();
    try std.testing.expectEqual(@as(usize, 1), fields.points().len);
    try std.testing.expectEqual(@as(f32, 4), fields.points()[0].x);
    // A real polygon closes; a degenerate one does not.
    _ = fields.vertexAdd(1, 1);
    _ = fields.vertexAdd(5, 5);
    fields.pending_len = 2;
    try std.testing.expect(!fields.tryClose());
    _ = fields.vertexAdd(5, 5);
    try std.testing.expect(fields.tryClose());
    try std.testing.expectEqual(State.select, fields.state);
    // Select picks the vertex within the radius (the pending polygon here is
    // (4,4), (1,1), (5,5)); Edit drags it by the grab offset, Insert bisects
    // the next edge, Delete removes it.
    try std.testing.expect(fields.pick(1.1, 1.1, 0.25));
    try std.testing.expectEqual(State.edit, fields.state);
    try std.testing.expect(fields.dragTo(2, 2));
    try std.testing.expectEqual(@as(f32, 1.9), fields.points()[1].x);
    try std.testing.expectEqual(@as(f32, 1.9), fields.points()[1].y);
    try std.testing.expect(fields.insertVertex());
    try std.testing.expectEqual(@as(usize, 4), fields.points().len);
    try std.testing.expectEqual(@as(f32, 3.45), fields.points()[2].x);
    try std.testing.expect(fields.deleteVertex());
    try std.testing.expectEqual(@as(usize, 3), fields.points().len);
    try std.testing.expectEqual(State.select, fields.state);
}
