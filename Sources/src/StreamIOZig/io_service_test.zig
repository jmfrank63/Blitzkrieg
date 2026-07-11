const std = @import("std");
const service = @import("io_service.zig");

test "submitted read completes and synchronous wait returns its byte count" {
    var io_service = try service.IoService.init(std.testing.allocator, .threaded);
    defer io_service.deinit();

    var bytes = [_]u8{ 1, 2, 3 };
    var request = try io_service.submitRead(.{ .buffer = &bytes, .complete_bytes = 3 });
    try std.testing.expectEqual(@as(usize, 3), try request.await(io_service.io()));
}

test "canceled request reaches the canceled terminal state" {
    var io_service = try service.IoService.init(std.testing.allocator, .threaded);
    defer io_service.deinit();

    var empty = [_]u8{};
    var request = try io_service.submitRead(.{ .buffer = &empty, .complete_bytes = 0 });
    request.cancel();
    try std.testing.expectError(error.Canceled, request.await(io_service.io()));
}
