const std = @import("std");
const bridge_mod = @import("bridge.zig");
const fake_mod = @import("fake_bridge.zig");
const document_mod = @import("document.zig");
const Bridge = bridge_mod.Bridge;
const EditError = bridge_mod.EditError;
const ObjectRecord = bridge_mod.ObjectRecord;
const FakeBridge = fake_mod.FakeBridge;
const Document = document_mod.Document;

/// The core's one entry point for the app: every edit goes through here, so
/// the bridge, the document and the history never disagree.
pub const Editor = struct {
    allocator: std.mem.Allocator,
    bridge: Bridge,
    document: Document = .{},
    status_buffer: [256]u8 = undefined,
    status_len: usize = 0,

    pub fn init(allocator: std.mem.Allocator, b: Bridge) Editor {
        return .{ .allocator = allocator, .bridge = b };
    }

    pub fn deinit(self: *Editor) void {
        self.document.deinit(self.allocator);
        self.* = undefined;
    }

    /// The status bar's line: the bridge's reason for the last refusal or
    /// failure, empty after a success.
    pub fn status(self: *const Editor) []const u8 {
        return self.status_buffer[0..self.status_len];
    }

    fn noteOutcome(self: *Editor, status_code: bridge_mod.Status) EditError!void {
        if (status_code == .ok) {
            self.status_len = 0;
            return;
        }
        const message = self.bridge.lastMessage();
        const len = @min(message.len, self.status_buffer.len);
        @memcpy(self.status_buffer[0..len], message[0..len]);
        self.status_len = len;
        return bridge_mod.check(status_code);
    }

    /// On failure the map that was open stays open, as the bridge keeps it.
    pub fn open(self: *Editor, path: []const u8) EditError!void {
        var info: bridge_mod.MapInfo = .{};
        try self.noteOutcome(self.bridge.openMap(path, &info));
        try self.document.reload(self.allocator, self.bridge, path, info);
    }

    pub fn save(self: *Editor, path: []const u8) EditError!void {
        try self.noteOutcome(self.bridge.saveMap(path));
        self.document.path.clearRetainingCapacity();
        try self.document.path.appendSlice(self.allocator, path);
    }
};

/// The same fixture fake_bridge.zig builds for its own tests, exposed here
/// under the name later tasks call it by.
pub const testFixture = fake_mod.fixture;

test "open fills the document from the bridge" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expectEqual(@as(i32, 8), editor.document.info.width_tiles);
    try std.testing.expectEqual(@as(usize, 3), editor.document.objects.items.len);
    try std.testing.expectEqualStrings("T34", editor.document.find(1).?.nameSlice());
    try std.testing.expect(!editor.document.find(3).?.known);
    try std.testing.expectEqualSlices(i32, &.{ 0, 1 }, editor.document.diplomacy.items);
}

test "a failed open keeps the map that was open" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try std.testing.expectError(error.Failed, editor.open("missing.bzm"));
    try std.testing.expectEqualStrings("fixture.bzm", editor.document.path.items);
    try std.testing.expectEqualStrings("no such map", editor.status());
}

test "save moves the document to the saved path" {
    var fake = try testFixture(std.testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(std.testing.allocator, fake.bridge());
    defer editor.deinit();
    try editor.open("fixture.bzm");
    try editor.save("renamed.bzm");
    try std.testing.expectEqualStrings("renamed.bzm", editor.document.path.items);
}
