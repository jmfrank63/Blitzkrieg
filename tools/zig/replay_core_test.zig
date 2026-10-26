//! The replay core (Sources/src/Main/ReplayCore.h) through its C ABI
//! (tools/zig/replay_core_abi.cpp): what a replay and a save's history keep so
//! that playback starts from the recording's own random state and can prove
//! it stays on the recording (docs/superpowers/specs/2026-10-26-mission-replay-design.md,
//! slice 1). `zig build test-replay-core` runs it.
const std = @import("std");
const testing = std.testing;

extern fn bk_replay_state_hash_period() c_int;
extern fn bk_replay_is_state_hash_segment(segment: c_int) bool;
extern fn bk_replay_format_time(segment: c_int, buffer: [*]u8, capacity: c_int) bool;
extern fn bk_replay_format_random_state(state: c_uint, buffer: [*]u8, capacity: c_int) bool;
extern fn bk_replay_parse_random_state(text: [*:0]const u8, state: *c_uint) bool;
extern fn bk_replay_hashes_create() ?*anyopaque;
extern fn bk_replay_hashes_destroy(handle: ?*anyopaque) void;
extern fn bk_replay_hashes_record(handle: ?*anyopaque, segment: c_int, hash: c_uint) void;
extern fn bk_replay_hashes_count(handle: ?*anyopaque) c_int;
extern fn bk_replay_hashes_segment(handle: ?*anyopaque, index: c_int) c_int;
extern fn bk_replay_hashes_hash(handle: ?*anyopaque, index: c_int) c_uint;
extern fn bk_replay_hashes_encode(handle: ?*anyopaque, buffer: [*]u8, capacity: c_int) bool;
extern fn bk_replay_hashes_decode(handle: ?*anyopaque, text: [*:0]const u8) bool;
extern fn bk_replay_check_arm(handle: ?*anyopaque) void;
extern fn bk_replay_check(handle: ?*anyopaque, segment: c_int, hash: c_uint) c_int;
extern fn bk_replay_check_first_mismatch(handle: ?*anyopaque) c_int;
extern fn bk_replay_check_matched(handle: ?*anyopaque) c_int;

/// NReplay::CStateHashCheck::EResult.
const not_recorded = 0;
const match = 1;
const first_mismatch = 2;
const mismatch = 3;

const Hashes = struct {
    handle: ?*anyopaque,

    fn init() Hashes {
        return .{ .handle = bk_replay_hashes_create() };
    }
    fn deinit(self: Hashes) void {
        bk_replay_hashes_destroy(self.handle);
    }
    fn record(self: Hashes, segment: c_int, hash: c_uint) void {
        bk_replay_hashes_record(self.handle, segment, hash);
    }
    fn count(self: Hashes) usize {
        return @intCast(bk_replay_hashes_count(self.handle));
    }
    fn expectEntries(self: Hashes, expected: []const [2]u32) !void {
        try testing.expectEqual(expected.len, self.count());
        for (expected, 0..) |entry, index| {
            try testing.expectEqual(@as(c_int, @intCast(entry[0])), bk_replay_hashes_segment(self.handle, @intCast(index)));
            try testing.expectEqual(@as(c_uint, entry[1]), bk_replay_hashes_hash(self.handle, @intCast(index)));
        }
    }
    fn encode(self: Hashes, buffer: []u8) ![]const u8 {
        try testing.expect(bk_replay_hashes_encode(self.handle, buffer.ptr, @intCast(buffer.len)));
        return std.mem.sliceTo(buffer, 0);
    }
    fn decode(self: Hashes, text: [:0]const u8) bool {
        return bk_replay_hashes_decode(self.handle, text.ptr);
    }
};

test "a state hash every second of game time, after the start" {
    try testing.expectEqual(@as(c_int, 20), bk_replay_state_hash_period());
    try testing.expect(!bk_replay_is_state_hash_segment(0));
    try testing.expect(!bk_replay_is_state_hash_segment(19));
    try testing.expect(bk_replay_is_state_hash_segment(20));
    try testing.expect(!bk_replay_is_state_hash_segment(21));
    try testing.expect(bk_replay_is_state_hash_segment(72000));
    try testing.expect(!bk_replay_is_state_hash_segment(-20));
}

test "the out-of-sync report names the game time" {
    var buffer: [32]u8 = undefined;
    try testing.expect(bk_replay_format_time(20 * 192, &buffer, buffer.len));
    try testing.expectEqualStrings("03:12", std.mem.sliceTo(&buffer, 0));
    try testing.expect(bk_replay_format_time(39, &buffer, buffer.len));
    try testing.expectEqualStrings("00:01", std.mem.sliceTo(&buffer, 0));
    try testing.expect(!bk_replay_format_time(20, &buffer, 3));
}

test "StartRandomState round-trips every 32-bit state and tells absent from zero" {
    var buffer: [16]u8 = undefined;
    for ([_]c_uint{ 0, 1, 0x9e3779b9, 0xffffffff, 0x12345678 }) |state| {
        try testing.expect(bk_replay_format_random_state(state, &buffer, buffer.len));
        const text = std.mem.sliceTo(&buffer, 0);
        try testing.expectEqual(@as(usize, 8), text.len);
        var parsed: c_uint = 0xdeadbeef;
        try testing.expect(bk_replay_parse_random_state(@ptrCast(text.ptr), &parsed));
        try testing.expectEqual(state, parsed);
    }
    try testing.expect(bk_replay_format_random_state(0, &buffer, buffer.len));
    try testing.expectEqualStrings("00000000", std.mem.sliceTo(&buffer, 0));
    var state: c_uint = 7;
    try testing.expect(bk_replay_parse_random_state("9E3779B9", &state));
    try testing.expectEqual(@as(c_uint, 0x9e3779b9), state);
    // An old replay has no key, so the text stays empty: not a state, the
    // zero seed applies; and nothing malformed is taken for one.
    for ([_][:0]const u8{ "", "1234567", "123456789", "1234567g", " 1234567", "-1234567", "0x123456" }) |bad| {
        state = 7;
        try testing.expect(!bk_replay_parse_random_state(bad.ptr, &state));
        try testing.expectEqual(@as(c_uint, 7), state);
    }
}

test "a hash at or after a load's segment belongs to the timeline the load left" {
    const hashes = Hashes.init();
    defer hashes.deinit();
    hashes.record(20, 0xa);
    hashes.record(40, 0xb);
    hashes.record(60, 0xc);
    hashes.record(80, 0xd);
    // A save written at segment 50 is loaded: the mission goes on from there,
    // and its next hash is at 60.
    hashes.record(60, 0xe);
    try hashes.expectEntries(&.{ .{ 20, 0xa }, .{ 40, 0xb }, .{ 60, 0xe } });
    // A save written at segment 30, then a hash at 40 again.
    hashes.record(40, 0xf);
    try hashes.expectEntries(&.{ .{ 20, 0xa }, .{ 40, 0xf } });
    // Going on as usual appends.
    hashes.record(60, 0x10);
    try hashes.expectEntries(&.{ .{ 20, 0xa }, .{ 40, 0xf }, .{ 60, 0x10 } });
}

test "StateHashes are delta-encoded and read back" {
    const hashes = Hashes.init();
    defer hashes.deinit();
    hashes.record(20, 0x1a2b3c4d);
    hashes.record(40, 0);
    hashes.record(60, 0xffffffff);
    hashes.record(100, 0x5);
    var buffer: [256]u8 = undefined;
    const text = try hashes.encode(&buffer);
    try testing.expectEqualStrings("20:1a2b3c4d 20:0 20:ffffffff 40:5", text);

    const read = Hashes.init();
    defer read.deinit();
    var copy: [256:0]u8 = undefined;
    @memcpy(copy[0..text.len], text);
    copy[text.len] = 0;
    try testing.expect(read.decode(copy[0..text.len :0]));
    try read.expectEntries(&.{ .{ 20, 0x1a2b3c4d }, .{ 40, 0 }, .{ 60, 0xffffffff }, .{ 100, 0x5 } });

    // An hour of game time stays a short attribute: 3,600 entries.
    const hour = Hashes.init();
    defer hour.deinit();
    var segment: c_int = 20;
    while (segment <= 72000) : (segment += 20) hour.record(segment, @as(c_uint, @intCast(segment)) *% 2654435761);
    var big: [64 * 1024]u8 = undefined;
    const hour_text = try hour.encode(&big);
    try testing.expect(hour_text.len < 3600 * 12);
    big[hour_text.len] = 0;
    const hour_read = Hashes.init();
    defer hour_read.deinit();
    try testing.expect(hour_read.decode(big[0..hour_text.len :0]));
    try testing.expectEqual(@as(usize, 3600), hour_read.count());
    try testing.expectEqual(@as(c_int, 72000), bk_replay_hashes_segment(hour_read.handle, 3599));
    try testing.expectEqual(@as(c_uint, 72000) *% 2654435761, bk_replay_hashes_hash(hour_read.handle, 3599));
}

test "an old replay without StateHashes, or a damaged list, checks nothing" {
    const hashes = Hashes.init();
    defer hashes.deinit();
    // The key absent: the reader leaves the text empty.
    try testing.expect(hashes.decode(""));
    try testing.expectEqual(@as(usize, 0), hashes.count());
    bk_replay_check_arm(hashes.handle);
    try testing.expectEqual(@as(c_int, not_recorded), bk_replay_check(hashes.handle, 20, 0x1234));
    try testing.expectEqual(@as(c_int, -1), bk_replay_check_first_mismatch(hashes.handle));

    for ([_][:0]const u8{ "0:12", "20:", ":12", "20:123456789", "20:zz", "20:12 x", "20;12", "-20:12", "20:12,20:13", "20:0x1" }) |bad| {
        hashes.record(20, 1);
        try testing.expect(!hashes.decode(bad));
        try testing.expectEqual(@as(usize, 0), hashes.count());
    }
}

test "playback compares each hash with the recording's at the same segment and reports the first difference once" {
    const hashes = Hashes.init();
    defer hashes.deinit();
    try testing.expect(hashes.decode("20:a 20:b 20:c 20:d"));
    bk_replay_check_arm(hashes.handle);
    try testing.expectEqual(@as(c_int, match), bk_replay_check(hashes.handle, 20, 0xa));
    try testing.expectEqual(@as(c_int, match), bk_replay_check(hashes.handle, 40, 0xb));
    // A segment the recording holds no hash for is not a difference.
    try testing.expectEqual(@as(c_int, not_recorded), bk_replay_check(hashes.handle, 50, 0xb));
    try testing.expectEqual(@as(c_int, first_mismatch), bk_replay_check(hashes.handle, 60, 0x99));
    try testing.expectEqual(@as(c_int, mismatch), bk_replay_check(hashes.handle, 80, 0x98));
    // Past the recording's end.
    try testing.expectEqual(@as(c_int, not_recorded), bk_replay_check(hashes.handle, 100, 0xe));
    try testing.expectEqual(@as(c_int, 60), bk_replay_check_first_mismatch(hashes.handle));
    try testing.expectEqual(@as(c_int, 2), bk_replay_check_matched(hashes.handle));
    // Armed again (a new playback), the report is fresh.
    bk_replay_check_arm(hashes.handle);
    try testing.expectEqual(@as(c_int, -1), bk_replay_check_first_mismatch(hashes.handle));
    try testing.expectEqual(@as(c_int, match), bk_replay_check(hashes.handle, 80, 0xd));
}
