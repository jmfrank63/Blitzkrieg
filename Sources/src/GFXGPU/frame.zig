const std = @import("std");

pub const State = enum { idle, recording, pass_active, ready_to_submit };
pub const FrameError = error{ InvalidState, SkippedFrame };

pub const Frame = struct {
    state: State = .idle,
    skipped: bool = false,
    submitted: bool = false,
    command_buffer: ?*anyopaque = null,
    swapchain_texture: ?*anyopaque = null,
    render_pass: ?*anyopaque = null,

    pub fn begin(self: *Frame, acquired_texture: bool) FrameError!void {
        if (self.state != .idle) return FrameError.InvalidState;
        if (!acquired_texture) {
            self.skipped = true;
            return FrameError.SkippedFrame;
        }
        self.skipped = false;
        self.submitted = false;
        self.command_buffer = null;
        self.swapchain_texture = null;
        self.render_pass = null;
        self.state = .recording;
    }
    pub fn beginPass(self: *Frame) FrameError!void {
        if (self.state != .recording) return FrameError.InvalidState;
        self.state = .pass_active;
    }
    pub fn endPass(self: *Frame) FrameError!void {
        if (self.state != .pass_active) return FrameError.InvalidState;
        self.state = .recording;
    }
    pub fn end(self: *Frame) FrameError!void {
        if (self.state != .recording) return FrameError.InvalidState;
        self.state = .ready_to_submit;
    }
    pub fn present(self: *Frame) FrameError!void {
        if (self.state != .ready_to_submit) return FrameError.InvalidState;
        self.submitted = true;
        self.command_buffer = null;
        self.swapchain_texture = null;
        self.render_pass = null;
        self.state = .idle;
    }
    pub fn cancel(self: *Frame) void {
        self.state = .idle;
        self.skipped = false;
        self.submitted = false;
        self.command_buffer = null;
        self.swapchain_texture = null;
        self.render_pass = null;
    }
};

test "frame state machine accepts valid transitions and rejects invalid operations" {
    var frame = Frame{};
    try std.testing.expectError(FrameError.InvalidState, frame.present());
    try std.testing.expectError(FrameError.SkippedFrame, frame.begin(false));
    try std.testing.expectEqual(State.idle, frame.state);
    try frame.begin(true);
    try frame.beginPass();
    try frame.endPass();
    try frame.end();
    try frame.present();
    try std.testing.expect(frame.submitted);
    try frame.begin(true);
    try frame.beginPass();
    frame.cancel();
    try std.testing.expectEqual(State.idle, frame.state);
}
