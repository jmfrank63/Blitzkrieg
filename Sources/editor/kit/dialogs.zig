//! The OS file dialogs' answer, handed from SDL's callback to the frame loop.
//! SDL may call the callback on another thread (SDL_dialog.h), even before
//! SDL_Show*FileDialog returns or after the window is gone, so the callback
//! only writes into a `PathSlot` (a global or a long-lived field) and the
//! main thread acts on it in its next frame. Each editor shows its own
//! dialogs (filters, start folder) and passes `PathSlot(...).callback` with
//! the slot as userdata.
const std = @import("std");
const sdl3 = @import("sdl3");

/// The longest path the editors' slots hold.
pub const max_path = 4096;

/// One dialog at a time: `request` refuses while one is up, so the callback
/// never writes a buffer the main thread is reading. The state is the only
/// thing both threads touch; the buffer is written before it is published
/// (release) and read after it is seen (acquire). `Kind` says what the
/// dialog was opened for (`void` when only one thing opens it); `kind`
/// keeps the last request's until the next one, so after `take` a cancel
/// or failure still knows whose it was.
pub fn PathSlot(comptime Kind: type, comptime capacity: usize) type {
    return struct {
        state: std.atomic.Value(u8) = .init(@intFromEnum(State.idle)),
        kind: Kind = undefined,
        buffer: [capacity]u8 = undefined,
        len: usize = 0,

        const Self = @This();
        pub const max_path = capacity;
        const State = enum(u8) { idle, waiting, arrived, failed, cancelled };

        pub const Result = union(enum) {
            path: struct { kind: Kind, path: []const u8 },
            failed: []const u8,
            cancelled,
        };

        /// Main thread, before showing a dialog. False while one is already up.
        pub fn request(self: *Self, kind: Kind) bool {
            if (self.state.cmpxchgStrong(@intFromEnum(State.idle), @intFromEnum(State.waiting), .acquire, .monotonic) != null) return false;
            self.kind = kind;
            return true;
        }

        pub fn waiting(self: *const Self) bool {
            return self.state.load(.acquire) == @intFromEnum(State.waiting);
        }

        /// The dialog's callback, on whatever thread SDL calls it: the chosen
        /// path, or null for a cancel - a result now, not silence, so an
        /// unsaved-changes prompt can tell a cancelled Save As from one that
        /// never happened. Ignored unless a dialog was requested; the scripted
        /// runs call it themselves in place of the OS dialog.
        pub fn deliver(self: *Self, path: ?[]const u8) void {
            if (self.state.load(.acquire) != @intFromEnum(State.waiting)) return;
            const chosen = path orelse {
                self.state.store(@intFromEnum(State.cancelled), .release);
                return;
            };
            if (chosen.len > self.buffer.len) return self.deliverFailure("the chosen path is too long");
            @memcpy(self.buffer[0..chosen.len], chosen);
            self.len = chosen.len;
            self.state.store(@intFromEnum(State.arrived), .release);
        }

        /// The dialog's callback, when SDL reports an error instead of a choice.
        pub fn deliverFailure(self: *Self, message: []const u8) void {
            if (self.state.load(.acquire) != @intFromEnum(State.waiting)) return;
            const len = @min(message.len, self.buffer.len);
            @memcpy(self.buffer[0..len], message[0..len]);
            self.len = len;
            self.state.store(@intFromEnum(State.failed), .release);
        }

        /// Main thread, once a frame: what arrived since the last call, once.
        /// The slice stays valid until the next `request`, which cannot succeed
        /// before this has emptied the slot.
        pub fn take(self: *Self) ?Result {
            const state: State = @enumFromInt(self.state.load(.acquire));
            const result: Result = switch (state) {
                .idle, .waiting => return null,
                .arrived => .{ .path = .{ .kind = self.kind, .path = self.buffer[0..self.len] } },
                .failed => .{ .failed = self.buffer[0..self.len] },
                .cancelled => .cancelled,
            };
            self.state.store(@intFromEnum(State.idle), .release);
            return result;
        }

        /// SDL_DialogFileCallback, with the slot as userdata: SDL calls it
        /// when the user chose, cancelled or the dialog failed. It only hands
        /// the answer to the slot.
        pub fn callback(userdata: ?*anyopaque, filelist: [*c]const [*c]const u8, filter: c_int) callconv(.c) void {
            _ = filter;
            const slot: *Self = @ptrCast(@alignCast(userdata orelse return));
            if (filelist == null) {
                const reason = sdl3.c.SDL_GetError();
                slot.deliverFailure(if (reason != null) std.mem.span(reason) else "no reason given");
                return;
            }
            const first = filelist[0];
            slot.deliver(if (first != null) std.mem.span(first) else null);
        }
    };
}

test "the dialog slot: requested, path arrived, taken once; one dialog at a time; a cancel leaves nothing" {
    const Kind = enum { open, save_as };
    var slot: PathSlot(Kind, max_path) = .{};
    try std.testing.expect(slot.take() == null);
    slot.deliver("/ignored.bzm"); // no dialog was up
    try std.testing.expect(slot.take() == null);

    try std.testing.expect(slot.request(.open));
    try std.testing.expect(!slot.request(.save_as));
    try std.testing.expect(slot.take() == null); // still waiting
    slot.deliver("/maps/a.bzm");
    const got = slot.take().?;
    try std.testing.expectEqual(Kind.open, got.path.kind);
    try std.testing.expectEqualStrings("/maps/a.bzm", got.path.path);
    try std.testing.expect(slot.take() == null);

    try std.testing.expect(slot.request(.save_as));
    slot.deliver(null);
    try std.testing.expect(!slot.waiting());
    switch (slot.take().?) {
        .cancelled => {},
        else => return error.TestUnexpectedResult,
    }
    try std.testing.expectEqual(Kind.save_as, slot.kind);
    try std.testing.expect(slot.take() == null);

    try std.testing.expect(slot.request(.open));
    slot.deliverFailure("no portal");
    try std.testing.expectEqualStrings("no portal", slot.take().?.failed);
}

test "a path longer than the slot is a failure, not a cut path" {
    var slot: PathSlot(void, 8) = .{};
    try std.testing.expect(slot.request({}));
    slot.deliver("/a/much/longer/path");
    // The reason is cut to the slot's buffer.
    try std.testing.expectEqualStrings("the chos", slot.take().?.failed);
}
