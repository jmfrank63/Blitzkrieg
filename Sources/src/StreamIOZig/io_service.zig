const std = @import("std");

pub const Backend = enum { threaded, evented };
pub const Error = error{ UnsupportedBackend, Canceled };

pub const ReadRequest = struct {
    buffer: []u8,
    complete_bytes: usize,
};

pub const Request = struct {
    state: enum { completed, canceled } = .completed,
    complete_bytes: usize,

    pub fn cancel(self: *Request) void {
        self.state = .canceled;
    }

    pub fn await(self: *Request, io: std.Io) Error!usize {
        _ = io;
        return switch (self.state) {
            .completed => self.complete_bytes,
            .canceled => error.Canceled,
        };
    }
};

pub const IoService = struct {
    threaded: std.Io.Threaded,

    pub fn init(allocator: std.mem.Allocator, backend: Backend) Error!IoService {
        if (backend != .threaded) return error.UnsupportedBackend;
        return .{ .threaded = std.Io.Threaded.init(allocator, .{}) };
    }

    pub fn deinit(self: *IoService) void {
        self.threaded.deinit();
    }

    pub fn io(self: *IoService) std.Io {
        return self.threaded.io();
    }

    pub fn submitRead(_: *IoService, read: ReadRequest) Error!Request {
        if (read.complete_bytes > read.buffer.len) return error.UnsupportedBackend;
        return .{ .complete_bytes = read.complete_bytes };
    }
};
