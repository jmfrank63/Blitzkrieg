const std = @import("std");
const frame_mod = @import("frame.zig");

pub const LoadOp = enum { load, clear, dont_care };
pub const Event = enum { first_use, clear, draw, target_change, copy, readback, frame_end, cancel };
pub const PassPlan = struct {
    begin: bool = false,
    end: bool = false,
    consume_clear: bool = false,
    color_load: LoadOp = .load,
    depth_load: LoadOp = .load,
    stencil_load: LoadOp = .load,
};

pub fn plan(active_target: ?u64, requested_target: u64, pending_clear: bool, event: Event) PassPlan {
    const active = active_target != null;
    return switch (event) {
        .first_use, .draw => if (!active) .{ .begin = true, .consume_clear = pending_clear, .color_load = if (pending_clear) .clear else .load, .depth_load = if (pending_clear) .clear else .load, .stencil_load = if (pending_clear) .clear else .load } else .{},
        .clear => if (active and active_target.? == requested_target) .{ .end = true, .begin = true, .consume_clear = true, .color_load = .clear, .depth_load = .clear, .stencil_load = .clear } else .{ .begin = true, .consume_clear = true, .color_load = .clear, .depth_load = .clear, .stencil_load = .clear, .end = active },
        .target_change => if (active and active_target.? != requested_target) .{ .end = true, .begin = true, .consume_clear = pending_clear, .color_load = if (pending_clear) .clear else .load, .depth_load = if (pending_clear) .clear else .load, .stencil_load = if (pending_clear) .clear else .load } else .{},
        .copy, .readback, .frame_end, .cancel => .{ .end = active },
    };
}

pub const Api = struct {
    begin: *const fn (*anyopaque, u64, PassPlan) ?*anyopaque,
    end: *const fn (*anyopaque, *anyopaque) bool,
};

pub const Controller = struct {
    allocator: std.mem.Allocator,
    frame: *frame_mod.Frame,
    context: *anyopaque,
    api: Api,
    active_target: ?u64 = null,
    active_pass: ?*anyopaque = null,
    pending_clear: bool = false,
    clear_color: [4]f32 = .{ 0, 0, 0, 1 },
    clear_depth: f32 = 1,
    clear_stencil: u8 = 0,

    pub fn init(allocator: std.mem.Allocator, frame: *frame_mod.Frame, context: *anyopaque, api: Api) Controller { return .{ .allocator = allocator, .frame = frame, .context = context, .api = api }; }

    pub fn requestClear(self: *Controller, color: [4]f32, depth: f32, stencil: u8) void {
        self.pending_clear = true;
        self.clear_color = color;
        self.clear_depth = depth;
        self.clear_stencil = stencil;
    }

    fn endActive(self: *Controller) bool {
        const pass = self.active_pass orelse return true;
        const ended = self.api.end(self.context, pass);
        self.active_pass = null;
        self.active_target = null;
        if (ended) self.frame.endPass() catch self.frame.cancel();
        return ended;
    }

    pub fn ensurePass(self: *Controller, target: u64) !void {
        const event: Event = if (self.active_target == null) if (self.pending_clear) .clear else .first_use else if (self.active_target.? == target) .draw else .target_change;
        const pass_plan = plan(self.active_target, target, self.pending_clear, event);
        if (pass_plan.end and !self.endActive()) { self.frame.cancel(); return error.PassEndFailed; }
        if (!pass_plan.begin) return;
        const pass = self.api.begin(self.context, target, pass_plan) orelse { self.frame.cancel(); return error.PassBeginFailed; };
        self.frame.beginPass() catch { _ = self.api.end(self.context, pass); self.frame.cancel(); return frame_mod.FrameError.InvalidState; };
        self.active_pass = pass;
        self.active_target = target;
        if (pass_plan.consume_clear) self.pending_clear = false;
    }

    pub fn finishFrame(self: *Controller) !void {
        if (!self.endActive()) { self.frame.cancel(); return error.PassEndFailed; }
        self.frame.end() catch |err| { self.frame.cancel(); return err; };
    }

    pub fn cancel(self: *Controller) void { _ = self.endActive(); self.pending_clear = false; self.frame.cancel(); }
};

test "PassPlan matrix covers first use, clear, reuse, target switch, and frame end" {
    try std.testing.expect(plan(null, 1, false, .first_use).begin);
    const clear = plan(null, 1, true, .clear);
    try std.testing.expect(clear.begin and clear.consume_clear and clear.color_load == .clear);
    try std.testing.expect(!plan(1, 1, false, .draw).begin and !plan(1, 1, false, .draw).end);
    const switch_plan = plan(1, 2, false, .target_change);
    try std.testing.expect(switch_plan.end and switch_plan.begin);
    try std.testing.expect(plan(1, 1, false, .frame_end).end);
    try std.testing.expect(!plan(null, 1, false, .frame_end).end);
}

test "controller consumes clear once, reuses passes, and cancels failures" {
    const Context = struct { begins: u32 = 0, ends: u32 = 0, fail_begin: bool = false };
    var context = Context{};
    const Fake = struct {
        var ctx: *Context = undefined;
        fn begin(_: *anyopaque, _: u64, _: PassPlan) ?*anyopaque { ctx.begins += 1; if (ctx.fail_begin) return null; return @ptrCast(&ctx.begins); }
        fn end(_: *anyopaque, _: *anyopaque) bool { ctx.ends += 1; return true; }
    };
    Fake.ctx = &context;
    var frame = frame_mod.Frame{};
    try frame.begin(true);
    var controller = Controller.init(std.testing.allocator, &frame, @ptrCast(&context), .{ .begin = Fake.begin, .end = Fake.end });
    controller.requestClear(.{ 1, 0, 0, 1 }, 1, 0);
    try controller.ensurePass(1);
    try std.testing.expect(!controller.pending_clear);
    try controller.ensurePass(1);
    try std.testing.expectEqual(@as(u32, 1), context.begins);
    try controller.ensurePass(2);
    try std.testing.expectEqual(@as(u32, 1), context.ends);
    controller.cancel();
    try std.testing.expectEqual(frame_mod.State.idle, frame.state);
    try frame.begin(true);
    context.fail_begin = true;
    try std.testing.expectError(error.PassBeginFailed, controller.ensurePass(3));
    try std.testing.expectEqual(frame_mod.State.idle, frame.state);
}
