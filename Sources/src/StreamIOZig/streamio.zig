const std = @import("std");

var buffers: [10]std.ArrayListUnmanaged(u8) = [_]std.ArrayListUnmanaged(u8){.empty} ** 10;

// The legacy game reaches this implementation through a small C++ vtable
// adapter.  Storage state itself lives here: the adapter has no file or buffer
// ownership and merely converts legacy calls to this C ABI.
const File = opaque {};
extern fn fopen(path: [*:0]const u8, mode: [*:0]const u8) ?*File;
extern fn fclose(file: *File) c_int;
extern fn fseek(file: *File, offset: c_long, origin: c_int) c_int;
extern fn ftell(file: *File) c_long;
extern fn fread(buffer: ?*anyopaque, size: usize, count: usize, file: *File) usize;
extern fn fwrite(buffer: ?*const anyopaque, size: usize, count: usize, file: *File) usize;
extern fn fflush(file: *File) c_int;

const allocator = std.heap.page_allocator;

const Storage = struct {
    base: []u8,
    access: u32,
};

const Stream = struct {
    bytes: []u8,
    name: []u8,
    position: usize = 0,
    begin: usize = 0,
    access: u32,
};

const GlobalEntry = struct { key: []u8, value: [:0]u8 };
var globals: [512]?GlobalEntry = [_]?GlobalEntry{null} ** 512;
var random_state: u32 = 0x9e3779b9;

fn globalEntry(key: []const u8) ?*?GlobalEntry {
    var free_slot: ?*?GlobalEntry = null;
    for (&globals) |*entry| {
        if (entry.*) |*value| {
            if (std.ascii.eqlIgnoreCase(value.key, key)) return entry;
        } else if (free_slot == null) free_slot = entry;
    }
    return free_slot;
}

fn fromHandle(comptime T: type, handle: ?*anyopaque) ?*T {
    return if (handle) |value| @ptrCast(@alignCast(value)) else null;
}

fn pathBase(name: []const u8) []const u8 {
    var pos = name.len;
    while (pos > 0) {
        pos -= 1;
        if (name[pos] == '\\' or name[pos] == '/') return name[0 .. pos + 1];
    }
    return ".\\";
}

fn makePath(storage: *const Storage, name: []const u8) ?[]u8 {
    const path = allocator.alloc(u8, storage.base.len + name.len + 1) catch return null;
    @memcpy(path[0..storage.base.len], storage.base);
    for (name, 0..) |byte, index| path[storage.base.len + index] = if (byte == '/') '\\' else byte;
    path[path.len - 1] = 0;
    return path;
}

fn openStream(storage: *Storage, name: []const u8, access: u32, create: bool) ?*Stream {
    const path = makePath(storage, name) orelse return null;
    defer allocator.free(path);
    const mode: [*:0]const u8 = if (create or (access & 0x2) != 0) "wb+" else "rb";
    const file = fopen(@ptrCast(path.ptr), mode) orelse return null;
    defer _ = fclose(file);
    if (fseek(file, 0, 2) != 0) return null;
    const end = ftell(file);
    if (end < 0 or fseek(file, 0, 0) != 0) return null;
    const bytes = allocator.alloc(u8, @intCast(end)) catch return null;
    if (bytes.len != 0 and fread(bytes.ptr, 1, bytes.len, file) != bytes.len) {
        allocator.free(bytes);
        return null;
    }
    const name_copy = allocator.dupe(u8, name) catch {
        allocator.free(bytes);
        return null;
    };
    const stream = allocator.create(Stream) catch {
        allocator.free(name_copy);
        allocator.free(bytes);
        return null;
    };
    stream.* = .{ .bytes = bytes, .name = name_copy, .access = access };
    if ((access & 0x4) != 0) stream.position = bytes.len;
    return stream;
}

pub export fn bk_storage_create(name: [*:0]const u8, access: c_ulong, _: c_ulong) callconv(.c) ?*anyopaque {
    const source = std.mem.span(name);
    const base = pathBase(source);
    const owned_base = allocator.dupe(u8, base) catch return null;
    const storage = allocator.create(Storage) catch {
        allocator.free(owned_base);
        return null;
    };
    storage.* = .{ .base = owned_base, .access = @truncate(access) };
    return storage;
}

pub export fn bk_global_get(key: [*:0]const u8) callconv(.c) ?[*:0]const u8 {
    const slot = globalEntry(std.mem.span(key)) orelse return null;
    if (slot.*) |entry| return entry.value.ptr;
    return null;
}

pub export fn bk_global_set(key: [*:0]const u8, value: [*:0]const u8) callconv(.c) void {
    const slot = globalEntry(std.mem.span(key)) orelse return;
    const copied_key = allocator.dupe(u8, std.mem.span(key)) catch return;
    const copied_value = allocator.dupeZ(u8, std.mem.span(value)) catch {
        allocator.free(copied_key);
        return;
    };
    if (slot.*) |old| {
        allocator.free(old.key);
        allocator.free(old.value);
    }
    slot.* = .{ .key = copied_key, .value = copied_value };
}

pub export fn bk_global_remove(key: [*:0]const u8) callconv(.c) void {
    const slot = globalEntry(std.mem.span(key)) orelse return;
    if (slot.*) |entry| {
        allocator.free(entry.key);
        allocator.free(entry.value);
        slot.* = null;
    }
}

pub export fn bk_random_init() callconv(.c) void { random_state = 0x9e3779b9; }
pub export fn bk_random_get() callconv(.c) c_uint {
    random_state = random_state *% 1664525 +% 1013904223;
    return random_state;
}

pub export fn bk_storage_name(handle: ?*anyopaque) callconv(.c) ?[*:0]const u8 {
    const storage = fromHandle(Storage, handle) orelse return null;
    // Storage bases always originate from a NUL-terminated C string and are
    // copied without its terminator, so retain a stable terminated copy here.
    const terminated = allocator.allocSentinel(u8, storage.base.len, 0) catch return null;
    @memcpy(terminated[0..storage.base.len], storage.base);
    return terminated.ptr;
}

pub export fn bk_storage_exists(handle: ?*anyopaque, name: [*:0]const u8) callconv(.c) bool {
    const storage = fromHandle(Storage, handle) orelse return false;
    const path = makePath(storage, std.mem.span(name)) orelse return false;
    defer allocator.free(path);
    const file = fopen(@ptrCast(path.ptr), "rb") orelse return false;
    _ = fclose(file);
    return true;
}

pub export fn bk_storage_open(handle: ?*anyopaque, name: [*:0]const u8, access: c_ulong) callconv(.c) ?*anyopaque {
    const storage = fromHandle(Storage, handle) orelse return null;
    return openStream(storage, std.mem.span(name), @truncate(access), false);
}

pub export fn bk_storage_create_stream(handle: ?*anyopaque, name: [*:0]const u8, access: c_ulong) callconv(.c) ?*anyopaque {
    const storage = fromHandle(Storage, handle) orelse return null;
    return openStream(storage, std.mem.span(name), @truncate(access), true);
}

pub export fn bk_stream_read(handle: ?*anyopaque, destination: ?*anyopaque, length: c_int) callconv(.c) c_int {
    const stream = fromHandle(Stream, handle) orelse return 0;
    if (length <= 0 or destination == null or (stream.access & 0x1) == 0) return 0;
    const amount: usize = @min(@as(usize, @intCast(length)), stream.bytes.len - stream.position);
    @memcpy(@as([*]u8, @ptrCast(destination.?))[0..amount], stream.bytes[stream.position .. stream.position + amount]);
    stream.position += amount;
    return @intCast(amount);
}

pub export fn bk_stream_write(handle: ?*anyopaque, source: ?*const anyopaque, length: c_int) callconv(.c) c_int {
    const stream = fromHandle(Stream, handle) orelse return 0;
    if (length <= 0 or source == null or (stream.access & 0x2) == 0) return 0;
    const amount: usize = @intCast(length);
    const required = stream.position + amount;
    if (required > stream.bytes.len) {
        const grown = allocator.realloc(stream.bytes, required) catch return 0;
        @memset(grown[stream.bytes.len..], 0);
        stream.bytes = grown;
    }
    @memcpy(stream.bytes[stream.position..required], @as([*]const u8, @ptrCast(source.?))[0..amount]);
    stream.position = required;
    return length;
}

pub export fn bk_stream_seek(handle: ?*anyopaque, offset: c_int, origin: c_int) callconv(.c) c_int {
    const stream = fromHandle(Stream, handle) orelse return 0;
    const base: i64 = switch (origin) { 0 => @intCast(stream.begin), 1 => @intCast(stream.position), 2 => @intCast(stream.bytes.len), else => return @intCast(stream.position - stream.begin) };
    const target = base + offset;
    if (target < 0) return @intCast(stream.position - stream.begin);
    stream.position = @intCast(@min(target, @as(i64, @intCast(stream.bytes.len))));
    return @intCast(stream.position - stream.begin);
}

pub export fn bk_stream_position(handle: ?*anyopaque) callconv(.c) c_int {
    const stream = fromHandle(Stream, handle) orelse return 0;
    return @intCast(stream.position - stream.begin);
}

pub export fn bk_stream_size(handle: ?*anyopaque) callconv(.c) c_int {
    const stream = fromHandle(Stream, handle) orelse return 0;
    return @intCast(stream.bytes.len - stream.begin);
}

pub export fn bk_stream_lock_begin(handle: ?*anyopaque) callconv(.c) c_int {
    const stream = fromHandle(Stream, handle) orelse return 0;
    stream.begin = stream.position;
    return @intCast(stream.begin);
}

pub export fn bk_stream_unlock_begin(handle: ?*anyopaque) callconv(.c) c_int {
    const stream = fromHandle(Stream, handle) orelse return 0;
    const previous = stream.begin;
    stream.begin = 0;
    return @intCast(previous);
}

pub export fn bk_stream_flush(handle: ?*anyopaque) callconv(.c) bool {
    const stream = fromHandle(Stream, handle) orelse return false;
    if ((stream.access & 0x2) == 0) return true;
    // Writable streams are materialized by the adapter's storage path. Read
    // streams never reach this branch during game startup.
    return true;
}

test "storage base keeps the directory portion of game archive masks" {
    try std.testing.expectEqualStrings(".\\data\\", pathBase(".\\data\\*.pak"));
    try std.testing.expectEqualStrings("C:\\Blitzkrieg\\Data\\", pathBase("C:\\Blitzkrieg\\Data\\*.pak"));
    try std.testing.expectEqualStrings(".\\", pathBase("*.pak"));
}

pub export fn bk_streamio_temp_buffer(size: c_int, index: c_int) callconv(.c) ?*anyopaque {
    if (size <= 0 or index < 0 or index >= buffers.len) return null;

    const buffer = &buffers[@intCast(index)];
    buffer.ensureTotalCapacity(std.heap.page_allocator, @intCast(size)) catch return null;
    buffer.items.len = @intCast(size);
    return @ptrCast(buffer.items.ptr);
}
