//! The editor's own view of a filesystem (std-only, like the rest of the
//! core): what `Editor.save`'s safe save needs - existence, an overwriting
//! copy, a replacing rename, a best-effort delete - behind an interface so
//! `StdFiles` can back it with `std.Io.Dir` and `FakeFiles` can back it with
//! an in-memory map for the core's tests. See
//! docs/superpowers/specs/2026-09-19-portable-map-editor-design.md, "Errors ->
//! Save" (D-19): a temporary file next to the map, read back and compared by
//! the bridge (Editor.save, EditorBridge/session.cpp), then swapped in; one
//! `.bak` per file per session, taken before the first swap.
const std = @import("std");
const builtin = @import("builtin");

/// Enough for any path this editor writes: a user's map path plus the
/// `.~save`/`.bak` suffixes this file adds to it.
pub const max_path = std.Io.Dir.max_path_bytes;

/// `Editor.save`'s view of a filesystem. Every path here is an OS path -
/// native separators, as `osPathFromEngine` produces - never the engine's
/// backslash-always form.
pub const Files = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    pub const Error = error{Failed};

    pub const VTable = struct {
        exists: *const fn (ptr: *anyopaque, os_path: []const u8) bool,
        /// Overwrites `to` if it exists.
        copy: *const fn (ptr: *anyopaque, from: []const u8, to: []const u8) Error!void,
        /// Replaces `to` if it exists (std.Io.Dir.rename's own contract).
        rename: *const fn (ptr: *anyopaque, from: []const u8, to: []const u8) Error!void,
        /// A missing file is not an error: this cleans up a stale temp file,
        /// which not being there is the common case, not a failure.
        delete: *const fn (ptr: *anyopaque, os_path: []const u8) void,
        lastError: *const fn (ptr: *anyopaque) []const u8,
    };

    pub fn exists(self: Files, os_path: []const u8) bool {
        return self.vtable.exists(self.ptr, os_path);
    }
    pub fn copy(self: Files, from: []const u8, to: []const u8) Error!void {
        return self.vtable.copy(self.ptr, from, to);
    }
    pub fn rename(self: Files, from: []const u8, to: []const u8) Error!void {
        return self.vtable.rename(self.ptr, from, to);
    }
    pub fn delete(self: Files, os_path: []const u8) void {
        self.vtable.delete(self.ptr, os_path);
    }
    pub fn lastError(self: Files) []const u8 {
        return self.vtable.lastError(self.ptr);
    }
};

/// `Files` over a real `std.Io.Dir` - `dir` defaults to the process's current
/// directory, matching where every shipped map path in this app is relative
/// to (or an absolute path, which `std.Io.Dir` accepts from any `dir`).
pub const StdFiles = struct {
    io: std.Io,
    dir: std.Io.Dir = .cwd(),
    message_buffer: [256]u8 = undefined,
    message_len: usize = 0,

    pub fn files(self: *StdFiles) Files {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ptr: *anyopaque) *StdFiles {
        return @ptrCast(@alignCast(ptr));
    }

    fn say(self: *StdFiles, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.message_buffer, format, args) catch self.message_buffer[0..];
        self.message_len = text.len;
    }

    const vtable: Files.VTable = .{
        .exists = existsImpl,
        .copy = copyImpl,
        .rename = renameImpl,
        .delete = deleteImpl,
        .lastError = lastErrorImpl,
    };

    fn existsImpl(ptr: *anyopaque, os_path: []const u8) bool {
        const self = from(ptr);
        _ = self.dir.statFile(self.io, os_path, .{}) catch return false;
        return true;
    }

    fn copyImpl(ptr: *anyopaque, from_path: []const u8, to_path: []const u8) Files.Error!void {
        const self = from(ptr);
        std.Io.Dir.copyFile(self.dir, from_path, self.dir, to_path, self.io, .{}) catch |err| {
            self.say("{s}", .{@errorName(err)});
            return error.Failed;
        };
    }

    fn renameImpl(ptr: *anyopaque, from_path: []const u8, to_path: []const u8) Files.Error!void {
        const self = from(ptr);
        std.Io.Dir.rename(self.dir, from_path, self.dir, to_path, self.io) catch |err| {
            self.say("{s}", .{@errorName(err)});
            return error.Failed;
        };
    }

    fn deleteImpl(ptr: *anyopaque, os_path: []const u8) void {
        const self = from(ptr);
        self.dir.deleteFile(self.io, os_path) catch {};
    }

    fn lastErrorImpl(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        return self.message_buffer[0..self.message_len];
    }
};

/// `Files` over an in-memory path -> bytes map, for the core tier's tests: no
/// real disk, and `fail_copy`/`fail_rename` let a test make either step
/// refuse without touching the other. `fake_bridge.zig`'s `FakeBridge.files`,
/// when set, writes its "saved" bytes here too (under the same keys this
/// interface uses), so a test can see the whole temp-then-swap dance land in
/// one place.
pub const FakeFiles = struct {
    allocator: std.mem.Allocator,
    entries: std.StringHashMapUnmanaged([]u8) = .empty,
    op_log: std.ArrayListUnmanaged(Op) = .empty,
    fail_copy: bool = false,
    fail_rename: bool = false,
    message_buffer: [64]u8 = undefined,
    message_len: usize = 0,

    pub const OpKind = enum { write, copy, rename, delete };
    pub const Op = struct { kind: OpKind, from: []const u8, to: []const u8 = "" };

    pub fn init(allocator: std.mem.Allocator) FakeFiles {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *FakeFiles) void {
        var it = self.entries.iterator();
        while (it.next()) |entry| {
            self.allocator.free(entry.key_ptr.*);
            self.allocator.free(entry.value_ptr.*);
        }
        self.entries.deinit(self.allocator);
        for (self.op_log.items) |op| {
            self.allocator.free(op.from);
            if (op.to.len != 0) self.allocator.free(op.to);
        }
        self.op_log.deinit(self.allocator);
        self.* = undefined;
    }

    /// The bytes at `os_path`, or null if nothing has written there.
    pub fn contents(self: *const FakeFiles, os_path: []const u8) ?[]const u8 {
        return self.entries.get(os_path);
    }

    /// What `FakeBridge.saveMap` calls: the fake "engine" writing its result
    /// straight into this fake filesystem, at the same key `Files.exists` /
    /// `copy` / `rename` use for the same path.
    pub fn write(self: *FakeFiles, os_path: []const u8, contents_: []const u8) !void {
        try self.record(.write, os_path, "");
        try self.put(os_path, try self.allocator.dupe(u8, contents_));
    }

    fn put(self: *FakeFiles, os_path: []const u8, owned_contents: []u8) !void {
        if (self.entries.fetchRemove(os_path)) |old| {
            self.allocator.free(old.key);
            self.allocator.free(old.value);
        }
        const owned_key = try self.allocator.dupe(u8, os_path);
        errdefer self.allocator.free(owned_key);
        try self.entries.put(self.allocator, owned_key, owned_contents);
    }

    fn record(self: *FakeFiles, kind: OpKind, from_path: []const u8, to_path: []const u8) !void {
        const owned_from = try self.allocator.dupe(u8, from_path);
        errdefer self.allocator.free(owned_from);
        const owned_to = if (to_path.len != 0) try self.allocator.dupe(u8, to_path) else "";
        try self.op_log.append(self.allocator, .{ .kind = kind, .from = owned_from, .to = owned_to });
    }

    fn say(self: *FakeFiles, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.message_buffer, format, args) catch self.message_buffer[0..];
        self.message_len = text.len;
    }

    pub fn files(self: *FakeFiles) Files {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ptr: *anyopaque) *FakeFiles {
        return @ptrCast(@alignCast(ptr));
    }

    const vtable: Files.VTable = .{
        .exists = existsImpl,
        .copy = copyImpl,
        .rename = renameImpl,
        .delete = deleteImpl,
        .lastError = lastErrorImpl,
    };

    fn existsImpl(ptr: *anyopaque, os_path: []const u8) bool {
        return from(ptr).entries.contains(os_path);
    }

    fn copyImpl(ptr: *anyopaque, from_path: []const u8, to_path: []const u8) Files.Error!void {
        const self = from(ptr);
        self.record(.copy, from_path, to_path) catch return error.Failed;
        if (self.fail_copy) {
            self.say("the disk is full", .{});
            return error.Failed;
        }
        const data = self.entries.get(from_path) orelse {
            self.say("no such file: {s}", .{from_path});
            return error.Failed;
        };
        const owned_contents = self.allocator.dupe(u8, data) catch return error.Failed;
        self.put(to_path, owned_contents) catch {
            self.allocator.free(owned_contents);
            return error.Failed;
        };
    }

    fn renameImpl(ptr: *anyopaque, from_path: []const u8, to_path: []const u8) Files.Error!void {
        const self = from(ptr);
        self.record(.rename, from_path, to_path) catch return error.Failed;
        if (self.fail_rename) {
            self.say("could not replace the file", .{});
            return error.Failed;
        }
        const removed = self.entries.fetchRemove(from_path) orelse {
            self.say("no such file: {s}", .{from_path});
            return error.Failed;
        };
        self.allocator.free(removed.key);
        self.put(to_path, removed.value) catch {
            self.allocator.free(removed.value);
            return error.Failed;
        };
    }

    fn deleteImpl(ptr: *anyopaque, os_path: []const u8) void {
        const self = from(ptr);
        self.record(.delete, os_path, "") catch {};
        if (self.entries.fetchRemove(os_path)) |old| {
            self.allocator.free(old.key);
            self.allocator.free(old.value);
        }
    }

    fn lastErrorImpl(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        return self.message_buffer[0..self.message_len];
    }
};

/// An OS path (native separators) from an engine path (always backslash-
/// separated - `OpenFileStream` splits on backslash only, wherever it runs).
/// Null when `engine_path` does not fit `buffer`.
pub fn osPathFromEngine(buffer: []u8, engine_path: []const u8) ?[]const u8 {
    if (engine_path.len > buffer.len) return null;
    if (builtin.os.tag == .windows) {
        @memcpy(buffer[0..engine_path.len], engine_path);
    } else {
        for (engine_path, buffer[0..engine_path.len]) |char, *out| out.* = if (char == '\\') '/' else char;
    }
    return buffer[0..engine_path.len];
}

/// `<dir>\<stem>.~save<ext>`, in the engine's own form (so it can be handed
/// straight to `Bridge.saveMap`): the extension stays whatever `engine_path`
/// had, because the bridge picks the map format from it. Null when it does
/// not fit `buffer`.
pub fn tempPathFor(buffer: []u8, engine_path: []const u8) ?[]const u8 {
    const cut = std.mem.lastIndexOfScalar(u8, engine_path, '\\');
    const dir = if (cut) |c| engine_path[0 .. c + 1] else "";
    const name = if (cut) |c| engine_path[c + 1 ..] else engine_path;
    const dot = std.mem.lastIndexOfScalar(u8, name, '.');
    const stem = if (dot) |d| name[0..d] else name;
    const ext = if (dot) |d| name[d..] else "";
    return std.fmt.bufPrint(buffer, "{s}{s}.~save{s}", .{ dir, stem, ext }) catch null;
}

/// `os_path` + `.bak` (D-19's `name.bzm.bak`): takes and returns an OS path,
/// unlike `tempPathFor`, since it is only ever used on a path already
/// converted for the `Files` interface. Null when it does not fit `buffer`.
pub fn backupPathFor(buffer: []u8, os_path: []const u8) ?[]const u8 {
    return std.fmt.bufPrint(buffer, "{s}.bak", .{os_path}) catch null;
}

test "osPathFromEngine: backslashes become the OS separator, except on Windows" {
    var buffer: [64]u8 = undefined;
    if (builtin.os.tag == .windows) {
        try std.testing.expectEqualStrings("\\Users\\me\\a.bzm", osPathFromEngine(&buffer, "\\Users\\me\\a.bzm").?);
    } else {
        try std.testing.expectEqualStrings("/Users/me/a.bzm", osPathFromEngine(&buffer, "\\Users\\me\\a.bzm").?);
    }
    try std.testing.expectEqualStrings("fixture.bzm", osPathFromEngine(&buffer, "fixture.bzm").?);
    var tiny: [3]u8 = undefined;
    try std.testing.expect(osPathFromEngine(&tiny, "fixture.bzm") == null);
}

test "tempPathFor: the stem gets .~save before the extension, the directory is kept" {
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("fixture.~save.bzm", tempPathFor(&buffer, "fixture.bzm").?);
    try std.testing.expectEqualStrings("\\Users\\me\\a.~save.xml", tempPathFor(&buffer, "\\Users\\me\\a.xml").?);
    var tiny: [4]u8 = undefined;
    try std.testing.expect(tempPathFor(&tiny, "fixture.bzm") == null);
}

test "backupPathFor: .bak on an OS path" {
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("/Users/me/a.bzm.bak", backupPathFor(&buffer, "/Users/me/a.bzm").?);
    var tiny: [4]u8 = undefined;
    try std.testing.expect(backupPathFor(&tiny, "/Users/me/a.bzm") == null);
}

// The real-disk half of D-19 and research A2: does a plain rename-over-an-
// existing-file actually replace it on this OS with no extra flag? Runs in
// the core tier, on all six CI targets, so both Windows jobs answer it too.
test "StdFiles on a real directory: copy to .bak, rename replaces the target" {
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    var std_files: StdFiles = .{ .io = std.testing.io, .dir = tmp.dir };
    const files = std_files.files();

    try tmp.dir.writeFile(std.testing.io, .{ .sub_path = "a.bzm", .data = "old" });
    try tmp.dir.writeFile(std.testing.io, .{ .sub_path = "a.~save.bzm", .data = "new" });

    try std.testing.expect(files.exists("a.bzm"));
    try std.testing.expect(!files.exists("a.bzm.bak"));
    try files.copy("a.bzm", "a.bzm.bak");
    try files.rename("a.~save.bzm", "a.bzm");

    const a = try tmp.dir.readFileAlloc(std.testing.io, "a.bzm", std.testing.allocator, .limited(64));
    defer std.testing.allocator.free(a);
    try std.testing.expectEqualStrings("new", a);
    const bak = try tmp.dir.readFileAlloc(std.testing.io, "a.bzm.bak", std.testing.allocator, .limited(64));
    defer std.testing.allocator.free(bak);
    try std.testing.expectEqualStrings("old", bak);
    try std.testing.expect(!files.exists("a.~save.bzm"));
}

test "FakeFiles: copy and rename move bytes between keys, fail_copy/fail_rename refuse" {
    var fake = FakeFiles.init(std.testing.allocator);
    defer fake.deinit();
    const files = fake.files();
    try std.testing.expect(!files.exists("a.bzm"));
    try fake.write("a.~save.bzm", "new");
    try fake.write("a.bzm", "old");
    try files.copy("a.bzm", "a.bzm.bak");
    try std.testing.expectEqualStrings("old", fake.contents("a.bzm.bak").?);
    try files.rename("a.~save.bzm", "a.bzm");
    try std.testing.expectEqualStrings("new", fake.contents("a.bzm").?);
    try std.testing.expect(fake.contents("a.~save.bzm") == null);

    fake.fail_copy = true;
    try std.testing.expectError(error.Failed, files.copy("a.bzm", "a2.bzm.bak"));
    try std.testing.expectEqualStrings("the disk is full", files.lastError());
    fake.fail_rename = true;
    try std.testing.expectError(error.Failed, files.rename("a.bzm", "a2.bzm"));

    try std.testing.expectError(error.Failed, files.rename("no-such.bzm", "x.bzm"));
    files.delete("a.bzm");
    try std.testing.expect(!files.exists("a.bzm"));
    files.delete("never-there.bzm"); // a missing file is fine to delete
}
