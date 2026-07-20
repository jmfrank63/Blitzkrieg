const std = @import("std");
const xml = @import("xml.zig");
const options = @import("options.zig");
const console = @import("console.zig");
const zip = @import("zip.zig");

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
const FileAttributes = extern struct {
    attributes: u32,
    creation_low: u32,
    creation_high: u32,
    access_low: u32,
    access_high: u32,
    write_low: u32,
    write_high: u32,
    size_high: u32,
    size_low: u32,
};
const FileTime = extern struct { low: u32, high: u32 };
extern fn GetFileAttributesExA(path: [*:0]const u8, info_level: u32, attributes: *FileAttributes) callconv(.winapi) bool;
extern fn FileTimeToDosDateTime(file_time: *const FileTime, date: *u16, time: *u16) callconv(.winapi) bool;
const FindData = extern struct {
    attributes: u32,
    creation_low: u32,
    creation_high: u32,
    access_low: u32,
    access_high: u32,
    write_low: u32,
    write_high: u32,
    size_high: u32,
    size_low: u32,
    reserved0: u32,
    reserved1: u32,
    file_name: [260]u8,
    alternate_name: [14]u8,
};
extern fn FindFirstFileA(pattern: [*:0]const u8, data: *FindData) callconv(.winapi) ?*anyopaque;
extern fn FindNextFileA(handle: *anyopaque, data: *FindData) callconv(.winapi) bool;
extern fn FindClose(handle: *anyopaque) callconv(.winapi) bool;

var arena_state = std.heap.ArenaAllocator.init(std.heap.c_allocator);
const allocator = arena_state.allocator();

const LoadedArchive = struct {
    bytes: []u8,
    path: [:0]u8,
    archive: zip.Archive,
    modified: u32,
};
const ArchiveMatch = struct { archive: *const LoadedArchive, entry: *const zip.Entry };
const StorageOverlay = struct { name: [:0]u8, storage: *Storage };

const Storage = struct {
    base: []u8,
    access: u32,
    archives: std.ArrayListUnmanaged(LoadedArchive) = .empty,
    overlays: std.ArrayListUnmanaged(StorageOverlay) = .empty,
};

const Stream = struct {
    bytes: []u8,
    name: [:0]u8,
    path: [:0]u8,
    position: usize = 0,
    begin: usize = 0,
    access: u32,
};

const StorageStats = extern struct {
    name: ?[*:0]const u8,
    element_type: c_int,
    size: c_int,
    creation_time: u32,
    modification_time: u32,
    access_time: u32,
};

const Tree = struct {
    document: xml.Document,
    arena: std.heap.ArenaAllocator,
    current: *xml.Node,
    stack: std.ArrayListUnmanaged(*xml.Node) = .empty,
    containers: std.ArrayListUnmanaged(*xml.Node) = .empty,
    mode: c_int,
};

const StructureLevel = struct {
    start: usize,
    len: usize,
    counter: usize = 1,
};

const StructureSaver = struct {
    stream: *Stream,
    levels: std.ArrayListUnmanaged(StructureLevel) = .empty,
};

const Enumerator = struct {
    names: std.ArrayListUnmanaged([:0]u8) = .empty,
    index: usize = 0,
};

const GlobalEntry = struct { key: []u8, value: [:0]u8 };
var globals: std.ArrayListUnmanaged(GlobalEntry) = .empty;
var random_state: u32 = 0x9e3779b9;

fn globalIndex(key: []const u8) ?usize {
    for (globals.items, 0..) |entry, index| {
        if (std.ascii.eqlIgnoreCase(entry.key, key)) return index;
    }
    return null;
}

fn fromHandle(comptime T: type, handle: ?*anyopaque) ?*T {
    return if (handle) |value| @ptrCast(@alignCast(value)) else null;
}

fn shortChunkAt(bytes: []const u8, level: StructureLevel, wanted_id: u8, wanted_number: usize) ?StructureLevel {
    var position = level.start;
    const end = level.start + level.len;
    var match_number: usize = 0;
    while (position + 2 <= end) {
        const id = bytes[position];
        position += 1;
        var encoded: u32 = bytes[position];
        position += 1;
        if ((encoded & 1) != 0) {
            if (position + 3 > end) return null;
            encoded |= @as(u32, bytes[position]) << 8;
            encoded |= @as(u32, bytes[position + 1]) << 16;
            encoded |= @as(u32, bytes[position + 2]) << 24;
            position += 3;
        }
        const length: usize = @intCast(encoded >> 1);
        if (position + length > end) return null;
        if (id == wanted_id) {
            match_number += 1;
            if (match_number == wanted_number) return .{ .start = position, .len = length };
        }
        position += length;
    }
    return null;
}

pub export fn bk_structure_create(stream_handle: ?*anyopaque, mode: c_int) callconv(.c) ?*anyopaque {
    if (mode != 1 and mode != 2) return null;
    const stream = fromHandle(Stream, stream_handle) orelse return null;
    const file_level = StructureLevel{ .start = 0, .len = stream.bytes.len };
    const data_level = shortChunkAt(stream.bytes, file_level, 1, 1) orelse return null;
    const saver = allocator.create(StructureSaver) catch return null;
    saver.* = .{ .stream = stream };
    saver.levels.append(allocator, data_level) catch {
        allocator.destroy(saver);
        return null;
    };
    return saver;
}

pub export fn bk_structure_destroy(handle: ?*anyopaque) callconv(.c) void {
    const saver = fromHandle(StructureSaver, handle) orelse return;
    saver.levels.deinit(allocator);
    allocator.destroy(saver);
}

pub export fn bk_structure_start(handle: ?*anyopaque, id: u8) callconv(.c) bool {
    const saver = fromHandle(StructureSaver, handle) orelse return false;
    const current = saver.levels.getLastOrNull() orelse return false;
    const child_level = shortChunkAt(saver.stream.bytes, current, id, current.counter) orelse return false;
    saver.levels.append(allocator, child_level) catch return false;
    return true;
}

pub export fn bk_structure_finish(handle: ?*anyopaque) callconv(.c) void {
    const saver = fromHandle(StructureSaver, handle) orelse return;
    if (saver.levels.items.len > 1) _ = saver.levels.pop();
}

pub export fn bk_structure_data(handle: ?*anyopaque, id: u8, output: ?*anyopaque, size: c_int) callconv(.c) void {
    if (size <= 0 or output == null) return;
    const destination = @as([*]u8, @ptrCast(output.?))[0..@intCast(size)];
    const saver = fromHandle(StructureSaver, handle) orelse {
        @memset(destination, 0);
        return;
    };
    const current = saver.levels.getLastOrNull() orelse {
        @memset(destination, 0);
        return;
    };
    const chunk = shortChunkAt(saver.stream.bytes, current, id, current.counter) orelse {
        @memset(destination, 0);
        return;
    };
    if (chunk.len != destination.len) {
        @memset(destination, 0);
        return;
    }
    @memcpy(destination, saver.stream.bytes[chunk.start .. chunk.start + chunk.len]);
}

pub export fn bk_structure_count(handle: ?*anyopaque, id: u8) callconv(.c) c_int {
    const saver = fromHandle(StructureSaver, handle) orelse return 0;
    const current = saver.levels.getLastOrNull() orelse return 0;
    var count: usize = 0;
    while (shortChunkAt(saver.stream.bytes, current, id, count + 1) != null) count += 1;
    return @intCast(count);
}

pub export fn bk_structure_set_counter(handle: ?*anyopaque, counter: c_int) callconv(.c) void {
    const saver = fromHandle(StructureSaver, handle) orelse return;
    if (counter <= 0 or saver.levels.items.len == 0) return;
    saver.levels.items[saver.levels.items.len - 1].counter = @intCast(counter);
}

test "structure saver decodes nested compact chunks" {
    const fixture = [_]u8{ 1, 12, 2, 8, 0x78, 0x56, 0x34, 0x12 };
    const file = StructureLevel{ .start = 0, .len = fixture.len };
    const root = shortChunkAt(&fixture, file, 1, 1).?;
    try std.testing.expectEqual(@as(usize, 2), root.start);
    try std.testing.expectEqual(@as(usize, 6), root.len);
    const value = shortChunkAt(&fixture, root, 2, 1).?;
    try std.testing.expectEqualSlices(u8, &.{ 0x78, 0x56, 0x34, 0x12 }, fixture[value.start .. value.start + value.len]);
}

pub export fn bk_options_create() callconv(.c) ?*anyopaque {
    const system = allocator.create(options.System) catch return null;
    system.* = options.System.init(allocator);
    return system;
}

pub export fn bk_console_create() callconv(.c) ?*anyopaque {
    const value = allocator.create(console.Console) catch return null;
    value.* = console.Console.init(allocator);
    return value;
}

pub export fn bk_console_destroy(handle: ?*anyopaque) callconv(.c) void {
    const value = fromHandle(console.Console, handle) orelse return;
    value.deinit();
    allocator.destroy(value);
}

pub export fn bk_console_configure(handle: ?*anyopaque, config: [*:0]const u8) callconv(.c) bool {
    const value = fromHandle(console.Console, handle) orelse return false;
    return value.configure(std.mem.span(config));
}

pub export fn bk_console_write(handle: ?*anyopaque, channel: c_int, text: [*:0]const u16, color: u32, backup: bool) callconv(.c) void {
    const value = fromHandle(console.Console, handle) orelse return;
    value.writeWide(channel, text, color, backup);
}

pub export fn bk_console_write_ascii(handle: ?*anyopaque, channel: c_int, value_text: [*:0]const u8, color: u32, backup: bool) callconv(.c) void {
    const value = fromHandle(console.Console, handle) orelse return;
    value.writeAscii(channel, value_text, color, backup);
}

pub export fn bk_console_read(handle: ?*anyopaque, channel: c_int, color: ?*u32) callconv(.c) ?[*:0]const u16 {
    const value = fromHandle(console.Console, handle) orelse return null;
    return value.readWide(channel, color);
}

pub export fn bk_console_read_ascii(handle: ?*anyopaque, channel: c_int, color: ?*u32) callconv(.c) ?[*:0]const u8 {
    const value = fromHandle(console.Console, handle) orelse return null;
    return value.readAscii(channel, color);
}

pub export fn bk_options_destroy(handle: ?*anyopaque) callconv(.c) void {
    const system = fromHandle(options.System, handle) orelse return;
    system.deinit();
    allocator.destroy(system);
}

pub export fn bk_options_load_tree(options_handle: ?*anyopaque, tree_handle: ?*anyopaque, only_missing: bool) callconv(.c) c_int {
    const system = fromHandle(options.System, options_handle) orelse return 0;
    const tree = fromHandle(Tree, tree_handle) orelse return 0;
    return @intCast(system.loadXml(tree.current, only_missing) catch 0);
}

pub export fn bk_options_count(handle: ?*anyopaque) callconv(.c) c_int {
    const system = fromHandle(options.System, handle) orelse return 0;
    return @intCast(system.entries.items.len);
}

pub export fn bk_options_name_at(handle: ?*anyopaque, index: c_int) callconv(.c) ?[*:0]const u8 {
    const system = fromHandle(options.System, handle) orelse return null;
    if (index < 0 or index >= system.entries.items.len) return null;
    return system.entries.items[@intCast(index)].name.ptr;
}

pub export fn bk_options_value(handle: ?*anyopaque, name: [*:0]const u8, value_type: ?*u16) callconv(.c) ?[*:0]const u8 {
    const system = fromHandle(options.System, handle) orelse return null;
    const entry = system.get(std.mem.span(name)) orelse return null;
    if (value_type) |result| result.* = entry.value_type;
    return entry.value.ptr;
}

pub export fn bk_options_set(handle: ?*anyopaque, name: [*:0]const u8, value: [*:0]const u8, value_type: u16) callconv(.c) bool {
    const system = fromHandle(options.System, handle) orelse return false;
    system.set(std.mem.span(name), std.mem.span(value), value_type) catch return false;
    return true;
}

pub export fn bk_options_remove(handle: ?*anyopaque, name: [*:0]const u8) callconv(.c) bool {
    const system = fromHandle(options.System, handle) orelse return false;
    return system.remove(std.mem.span(name));
}

pub export fn bk_options_remove_prefix(handle: ?*anyopaque, prefix: [*:0]const u8) callconv(.c) void {
    const system = fromHandle(options.System, handle) orelse return;
    system.removePrefix(std.mem.span(prefix));
}

pub export fn bk_options_changed(handle: ?*anyopaque) callconv(.c) bool {
    const system = fromHandle(options.System, handle) orelse return false;
    return system.changed;
}

pub export fn bk_options_metadata(handle: ?*anyopaque, index: c_int, editor: ?*i32, flags: ?*u32, order: ?*i32, instant: ?*bool, action: ?*?[*:0]const u8, action_fill: ?*?[*:0]const u8, default_value: ?*?[*:0]const u8, value_type: ?*u16) callconv(.c) bool {
    const system = fromHandle(options.System, handle) orelse return false;
    if (index < 0 or index >= system.entries.items.len) return false;
    const entry = &system.entries.items[@intCast(index)];
    if (editor) |result| result.* = entry.editor_type;
    if (flags) |result| result.* = entry.flags;
    if (order) |result| result.* = entry.order;
    if (instant) |result| result.* = entry.instant_apply;
    if (action) |result| result.* = entry.action.ptr;
    if (action_fill) |result| result.* = entry.action_fill.ptr;
    if (default_value) |result| result.* = entry.default_value.ptr;
    if (value_type) |result| result.* = entry.value_type;
    return true;
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

fn readFileOwned(path: [*:0]const u8) ?[]u8 {
    const file = fopen(path, "rb") orelse return null;
    defer _ = fclose(file);
    if (fseek(file, 0, 2) != 0) return null;
    const end = ftell(file);
    if (end < 0 or fseek(file, 0, 0) != 0) return null;
    const bytes = allocator.alloc(u8, @intCast(end)) catch return null;
    if (bytes.len != 0 and fread(bytes.ptr, 1, bytes.len, file) != bytes.len) {
        allocator.free(bytes);
        return null;
    }
    return bytes;
}

fn dosModified(data: *const FindData) u32 {
    const write_time = FileTime{ .low = data.write_low, .high = data.write_high };
    var date: u16 = 0;
    var time: u16 = 0;
    return if (FileTimeToDosDateTime(&write_time, &date, &time))
        @as(u32, time) | (@as(u32, date) << 16)
    else
        0;
}

fn loadArchives(storage: *Storage) void {
    const pattern_bytes = std.fmt.allocPrint(allocator, "{s}*.pak", .{storage.base}) catch return;
    defer allocator.free(pattern_bytes);
    const pattern = allocator.dupeZ(u8, pattern_bytes) catch return;
    defer allocator.free(pattern);
    var find_data: FindData = undefined;
    const find = FindFirstFileA(pattern.ptr, &find_data) orelse return;
    defer _ = FindClose(find);
    while (true) {
        if ((find_data.attributes & 0x10) == 0) {
            const file_name = std.mem.sliceTo(&find_data.file_name, 0);
            const path_bytes = std.fmt.allocPrint(allocator, "{s}{s}", .{ storage.base, file_name }) catch null;
            if (path_bytes) |owned_path_bytes| {
                defer allocator.free(owned_path_bytes);
                const path = allocator.dupeZ(u8, owned_path_bytes) catch null;
                if (path) |owned_path| {
                    if (readFileOwned(owned_path.ptr)) |bytes| {
                        if (zip.Archive.parse(allocator, bytes)) |archive| {
                            storage.archives.append(allocator, .{
                                .bytes = bytes,
                                .path = owned_path,
                                .archive = archive,
                                .modified = dosModified(&find_data),
                            }) catch {
                                var owned_archive = archive;
                                owned_archive.deinit();
                                allocator.free(bytes);
                                allocator.free(owned_path);
                            };
                        } else |_| {
                            allocator.free(bytes);
                            allocator.free(owned_path);
                        }
                    } else allocator.free(owned_path);
                }
            }
        }
        if (!FindNextFileA(find, &find_data)) break;
    }
}

fn archiveEntry(storage: *const Storage, name: []const u8) ?ArchiveMatch {
    var best: ?ArchiveMatch = null;
    for (storage.archives.items) |*loaded| {
        const entry = loaded.archive.find(name) orelse continue;
        if (best == null or loaded.modified >= best.?.archive.modified) best = .{ .archive = loaded, .entry = entry };
    }
    return best;
}

fn overlayStream(storage: *Storage, name: []const u8, access: u32) ?*Stream {
    var index = storage.overlays.items.len;
    while (index > 0) {
        index -= 1;
        const child = storage.overlays.items[index].storage;
        if (openStream(child, name, access, false) orelse archiveStream(child, name, access)) |stream| return stream;
    }
    return null;
}

fn overlayExists(storage: *const Storage, name: []const u8) bool {
    var index = storage.overlays.items.len;
    while (index > 0) {
        index -= 1;
        const child = storage.overlays.items[index].storage;
        const path = makePath(child, name) orelse continue;
        defer allocator.free(path);
        if (fopen(@ptrCast(path.ptr), "rb")) |file| {
            _ = fclose(file);
            return true;
        }
        if (archiveEntry(child, name) != null or overlayExists(child, name)) return true;
    }
    return false;
}

fn isStructureCache(name: []const u8) bool {
    return name.len >= 4 and std.ascii.eqlIgnoreCase(name[name.len - 4 ..], ".gdb");
}

fn normalizedStorageName(name: []const u8) ?[:0]u8 {
    const normalized = allocator.allocSentinel(u8, name.len, 0) catch return null;
    for (name, 0..) |byte, index| {
        normalized[index] = if (byte == '/') '\\' else std.ascii.toLower(byte);
    }
    return normalized;
}

fn collectFiles(storage: *const Storage, enumerator: *Enumerator, relative: []const u8) void {
    const pattern_bytes = std.fmt.allocPrint(allocator, "{s}{s}*", .{ storage.base, relative }) catch return;
    defer allocator.free(pattern_bytes);
    const pattern = allocator.dupeZ(u8, pattern_bytes) catch return;
    defer allocator.free(pattern);
    var data: FindData = undefined;
    const handle = FindFirstFileA(pattern.ptr, &data) orelse return;
    defer _ = FindClose(handle);
    while (true) {
        const name = std.mem.sliceTo(&data.file_name, 0);
        if (!std.mem.eql(u8, name, ".") and !std.mem.eql(u8, name, "..")) {
            if ((data.attributes & 0x10) != 0) {
                const child = std.fmt.allocPrint(allocator, "{s}{s}\\", .{ relative, name }) catch "";
                defer if (child.len != 0) allocator.free(child);
                if (child.len != 0) collectFiles(storage, enumerator, child);
            } else {
                const full_name_bytes = std.fmt.allocPrint(allocator, "{s}{s}", .{ relative, name }) catch continue;
                defer allocator.free(full_name_bytes);
                const full_name = normalizedStorageName(full_name_bytes) orelse continue;
                enumerator.names.append(allocator, full_name) catch allocator.free(full_name);
            }
        }
        if (!FindNextFileA(handle, &data)) break;
    }
}

fn openStream(storage: *Storage, name: []const u8, access: u32, create: bool) ?*Stream {
    const raw_path = makePath(storage, name) orelse return null;
    defer allocator.free(raw_path);
    const path = allocator.dupeZ(u8, raw_path[0 .. raw_path.len - 1]) catch return null;
    _ = create; // Legacy file storage uses access flags to decide create/truncate behavior.
    const can_read = (access & 0x1) != 0;
    const can_write = (access & 0x2) != 0;
    const append_only = (access & 0x4) != 0 and !can_read;
    var file: ?*File = null;

    if (can_write and !can_read and !append_only) {
        // STREAM_ACCESS_WRITE maps to CREATE_ALWAYS in the original implementation.
        file = fopen(path.ptr, "wb+");
    } else {
        const mode: [*:0]const u8 = if (can_read and !can_write) "rb" else "rb+";
        file = fopen(path.ptr, mode);
        if (file == null and can_write) {
            // OPEN_ALWAYS behavior for RW/RWA/WA combinations.
            file = fopen(path.ptr, "wb+");
        }
    }
    if (file == null) {
        allocator.free(path);
        return null;
    }
    const opened = file.?;
    defer _ = fclose(opened);
    if (fseek(opened, 0, 2) != 0) return null;
    const end = ftell(opened);
    if (end < 0 or fseek(opened, 0, 0) != 0) return null;
    const bytes = allocator.alloc(u8, @intCast(end)) catch return null;
    if (bytes.len != 0 and fread(bytes.ptr, 1, bytes.len, opened) != bytes.len) {
        allocator.free(bytes);
        allocator.free(path);
        return null;
    }
    const name_copy = allocator.dupeZ(u8, name) catch {
        allocator.free(bytes);
        allocator.free(path);
        return null;
    };
    const stream = allocator.create(Stream) catch {
        allocator.free(name_copy);
        allocator.free(path);
        allocator.free(bytes);
        return null;
    };
    stream.* = .{ .bytes = bytes, .name = name_copy, .path = path, .access = access };
    if ((access & 0x4) != 0) stream.position = bytes.len;
    return stream;
}

fn archiveStream(storage: *Storage, name: []const u8, access: u32) ?*Stream {
    if ((access & 0x2) != 0) return null;
    const match = archiveEntry(storage, name) orelse return null;
    const bytes = match.archive.archive.extract(allocator, match.entry) catch return null;
    const name_copy = allocator.dupeZ(u8, name) catch {
        allocator.free(bytes);
        return null;
    };
    const path = allocator.dupeZ(u8, match.archive.path) catch {
        allocator.free(name_copy);
        allocator.free(bytes);
        return null;
    };
    const stream = allocator.create(Stream) catch {
        allocator.free(path);
        allocator.free(name_copy);
        allocator.free(bytes);
        return null;
    };
    stream.* = .{ .bytes = bytes, .name = name_copy, .path = path, .access = access };
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
    loadArchives(storage);
    return storage;
}

pub export fn bk_storage_destroy(handle: ?*anyopaque) callconv(.c) void {
    const storage = fromHandle(Storage, handle) orelse return;
    for (storage.archives.items) |*loaded| {
        loaded.archive.deinit();
        allocator.free(loaded.bytes);
        allocator.free(loaded.path);
    }
    storage.archives.deinit(allocator);
    for (storage.overlays.items) |overlay| allocator.free(overlay.name);
    storage.overlays.deinit(allocator);
    allocator.free(storage.base);
    allocator.destroy(storage);
}

pub export fn bk_global_get(key: [*:0]const u8) callconv(.c) ?[*:0]const u8 {
    const index = globalIndex(std.mem.span(key)) orelse return null;
    return globals.items[index].value.ptr;
}

pub export fn bk_global_set(key: [*:0]const u8, value: [*:0]const u8) callconv(.c) void {
    const copied_value = allocator.dupeZ(u8, std.mem.span(value)) catch {
        return;
    };
    if (globalIndex(std.mem.span(key))) |index| {
        allocator.free(globals.items[index].value);
        globals.items[index].value = copied_value;
        return;
    }
    const copied_key = allocator.dupe(u8, std.mem.span(key)) catch {
        allocator.free(copied_value);
        return;
    };
    globals.append(allocator, .{ .key = copied_key, .value = copied_value }) catch {
        allocator.free(copied_key);
        allocator.free(copied_value);
    };
}

pub export fn bk_global_remove(key: [*:0]const u8) callconv(.c) void {
    const index = globalIndex(std.mem.span(key)) orelse return;
    const entry = globals.swapRemove(index);
    allocator.free(entry.key);
    allocator.free(entry.value);
}

test "global store grows beyond the legacy startup working set" {
    var key_buffer: [64]u8 = undefined;
    var value_buffer: [64]u8 = undefined;
    for (0..700) |index| {
        const key = try std.fmt.bufPrintZ(&key_buffer, "test.global.{d}", .{index});
        const value = try std.fmt.bufPrintZ(&value_buffer, "value-{d}", .{index});
        bk_global_set(key, value);
    }
    bk_global_set("SharedResource.Text.Dialog.Ext", ".txt");
    try std.testing.expectEqualStrings(".txt", std.mem.span(bk_global_get("SharedResource.Text.Dialog.Ext").?));
    for (0..700) |index| {
        const key = try std.fmt.bufPrintZ(&key_buffer, "test.global.{d}", .{index});
        bk_global_remove(key);
    }
    bk_global_remove("SharedResource.Text.Dialog.Ext");
}

pub export fn bk_random_init() callconv(.c) void {
    random_state = 0x9e3779b9;
}
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
    const file = fopen(@ptrCast(path.ptr), "rb") orelse return archiveEntry(storage, std.mem.span(name)) != null or overlayExists(storage, std.mem.span(name));
    _ = fclose(file);
    return true;
}

pub export fn bk_storage_open(handle: ?*anyopaque, name: [*:0]const u8, access: c_ulong) callconv(.c) ?*anyopaque {
    const storage = fromHandle(Storage, handle) orelse return null;
    const stream_name = std.mem.span(name);
    return overlayStream(storage, stream_name, @truncate(access)) orelse openStream(storage, stream_name, @truncate(access), false) orelse archiveStream(storage, stream_name, @truncate(access));
}

pub export fn bk_storage_add(handle: ?*anyopaque, child_handle: ?*anyopaque, name: [*:0]const u8) callconv(.c) bool {
    const storage = fromHandle(Storage, handle) orelse return false;
    const child = fromHandle(Storage, child_handle) orelse return false;
    if (storage == child) return false;
    const owned_name = allocator.dupeZ(u8, std.mem.span(name)) catch return false;
    storage.overlays.append(allocator, .{ .name = owned_name, .storage = child }) catch {
        allocator.free(owned_name);
        return false;
    };
    return true;
}

pub export fn bk_storage_remove(handle: ?*anyopaque, name: [*:0]const u8) callconv(.c) ?*anyopaque {
    const storage = fromHandle(Storage, handle) orelse return null;
    for (storage.overlays.items, 0..) |overlay, index| {
        if (!std.ascii.eqlIgnoreCase(overlay.name, std.mem.span(name))) continue;
        const removed = storage.overlays.orderedRemove(index);
        allocator.free(removed.name);
        return removed.storage;
    }
    return null;
}

pub export fn bk_storage_create_stream(handle: ?*anyopaque, name: [*:0]const u8, access: c_ulong) callconv(.c) ?*anyopaque {
    const storage = fromHandle(Storage, handle) orelse return null;
    const stream_name = std.mem.span(name);
    // .gdb is a derived binary cache.  Do not create a cache that this Zig
    // runtime cannot yet serialize; callers continue with the XML source.
    if (isStructureCache(stream_name)) return null;
    return openStream(storage, stream_name, @truncate(access), true);
}

pub export fn bk_storage_stats(handle: ?*anyopaque, name: [*:0]const u8, output: ?*StorageStats) callconv(.c) bool {
    const storage = fromHandle(Storage, handle) orelse return false;
    const stats = output orelse return false;
    const path = makePath(storage, std.mem.span(name)) orelse return false;
    defer allocator.free(path);
    const file = fopen(@ptrCast(path.ptr), "rb") orelse return false;
    defer _ = fclose(file);
    if (fseek(file, 0, 2) != 0) return false;
    const end = ftell(file);
    if (end < 0 or end > std.math.maxInt(c_int)) return false;
    var attributes: FileAttributes = undefined;
    const have_attributes = GetFileAttributesExA(@ptrCast(path.ptr), 0, &attributes);
    var date: u16 = 0;
    var time: u16 = 0;
    var modification_time: u32 = 0;
    if (have_attributes) {
        const write_time = FileTime{ .low = attributes.write_low, .high = attributes.write_high };
        if (FileTimeToDosDateTime(&write_time, &date, &time)) modification_time = @as(u32, time) | (@as(u32, date) << 16);
    }
    stats.* = .{
        .name = name,
        .element_type = 2,
        .size = @intCast(end),
        .creation_time = 0,
        .modification_time = modification_time,
        .access_time = 0,
    };
    return true;
}

pub export fn bk_stream_stats(handle: ?*anyopaque, output: ?*StorageStats) callconv(.c) bool {
    const stream = fromHandle(Stream, handle) orelse return false;
    const stats = output orelse return false;
    if (stream.bytes.len > std.math.maxInt(c_int)) return false;
    stats.* = .{ .name = stream.name.ptr, .element_type = 2, .size = @intCast(stream.bytes.len), .creation_time = 0, .modification_time = 0, .access_time = 0 };
    return true;
}

fn collectArchiveFiles(storage: *const Storage, enumerator: *Enumerator) void {
    for (storage.archives.items) |*loaded| {
        for (loaded.archive.entries) |*entry| {
            const name_copy = normalizedStorageName(entry.name) orelse continue;
            enumerator.names.append(allocator, name_copy) catch allocator.free(name_copy);
        }
    }
    var index = storage.overlays.items.len;
    while (index > 0) {
        index -= 1;
        collectArchiveFiles(storage.overlays.items[index].storage, enumerator);
    }
}

pub export fn bk_storage_enumerator_create(handle: ?*anyopaque) callconv(.c) ?*anyopaque {
    const storage = fromHandle(Storage, handle) orelse return null;
    const enumerator = allocator.create(Enumerator) catch return null;
    enumerator.* = .{};
    collectFiles(storage, enumerator, "");
    collectArchiveFiles(storage, enumerator);
    return enumerator;
}

pub export fn bk_enumerator_destroy(handle: ?*anyopaque) callconv(.c) void {
    const enumerator = fromHandle(Enumerator, handle) orelse return;
    for (enumerator.names.items) |name| allocator.free(name);
    enumerator.names.deinit(allocator);
    allocator.destroy(enumerator);
}

pub export fn bk_enumerator_reset(handle: ?*anyopaque) callconv(.c) void {
    const enumerator = fromHandle(Enumerator, handle) orelse return;
    enumerator.index = 0;
}

pub export fn bk_enumerator_next(handle: ?*anyopaque) callconv(.c) bool {
    const enumerator = fromHandle(Enumerator, handle) orelse return false;
    if (enumerator.index >= enumerator.names.items.len) return false;
    enumerator.index += 1;
    return true;
}

pub export fn bk_enumerator_stats(handle: ?*anyopaque, output: ?*StorageStats) callconv(.c) bool {
    const enumerator = fromHandle(Enumerator, handle) orelse return false;
    const stats = output orelse return false;
    if (enumerator.index == 0 or enumerator.index > enumerator.names.items.len) return false;
    stats.* = .{ .name = enumerator.names.items[enumerator.index - 1].ptr, .element_type = 2, .size = 0, .creation_time = 0, .modification_time = 0, .access_time = 0 };
    return true;
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
    const base: i64 = switch (origin) {
        0 => @intCast(stream.begin),
        1 => @intCast(stream.position),
        2 => @intCast(stream.bytes.len),
        else => return @intCast(stream.position - stream.begin),
    };
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

pub export fn bk_stream_set_size(handle: ?*anyopaque, size: c_int) callconv(.c) bool {
    const stream = fromHandle(Stream, handle) orelse return false;
    if (size < 0 or (stream.access & 0x2) == 0) return false;
    const target: usize = @intCast(size);
    if (target != stream.bytes.len) {
        const previous = stream.bytes.len;
        const resized = allocator.realloc(stream.bytes, target) catch return false;
        if (target > previous) @memset(resized[previous..], 0);
        stream.bytes = resized;
    }
    if (stream.position > target) stream.position = target;
    if (stream.begin > target) stream.begin = target;
    return true;
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
    const file = fopen(stream.path.ptr, "wb") orelse return false;
    defer _ = fclose(file);
    if (stream.bytes.len != 0 and fwrite(stream.bytes.ptr, 1, stream.bytes.len, file) != stream.bytes.len) return false;
    return fflush(file) == 0;
}

pub export fn bk_stream_destroy(handle: ?*anyopaque) callconv(.c) void {
    const stream = fromHandle(Stream, handle) orelse return;
    allocator.free(stream.bytes);
    allocator.free(stream.name);
    allocator.free(stream.path);
    allocator.destroy(stream);
}

fn lastPathSegment(path: []const u8) []const u8 {
    var start: usize = 0;
    for (path, 0..) |byte, index| {
        if (byte == '/' or byte == '\\') start = index + 1;
    }
    return path[start..];
}

fn treeNode(tree: *Tree, path: []const u8) ?*xml.Node {
    var node = tree.current;
    var parts = std.mem.splitScalar(u8, path, '/');
    while (parts.next()) |part| {
        if (part.len == 0 or std.mem.eql(u8, part, ".")) continue;
        node = xml.child(node, part) orelse return null;
    }
    return node;
}

pub export fn bk_tree_create(stream_handle: ?*anyopaque, mode: c_int, base: [*:0]const u8) callconv(.c) ?*anyopaque {
    const stream = fromHandle(Stream, stream_handle) orelse return null;
    if (mode != 2) return null; // Write support is added only after a complete XML writer exists.
    var arena = std.heap.ArenaAllocator.init(allocator);
    const document = xml.parse(arena.allocator(), stream.bytes) catch {
        arena.deinit();
        return null;
    };
    var current = document.root;
    const base_name = std.mem.span(base);
    if (!std.mem.eql(u8, current.name, base_name)) {
        if (xml.child(current, base_name)) |matched| {
            current = matched;
        }
    }
    const tree = allocator.create(Tree) catch {
        arena.deinit();
        return null;
    };
    tree.* = .{ .document = document, .arena = arena, .current = current, .mode = mode };
    return tree;
}

pub export fn bk_tree_destroy(handle: ?*anyopaque) callconv(.c) void {
    const tree = fromHandle(Tree, handle) orelse return;
    tree.containers.deinit(allocator);
    tree.stack.deinit(allocator);
    tree.arena.deinit();
    allocator.destroy(tree);
}

pub export fn bk_tree_start(handle: ?*anyopaque, name: [*:0]const u8) callconv(.c) c_int {
    const tree = fromHandle(Tree, handle) orelse return 0;
    const path = std.mem.span(name);
    if (path.len == 0) return -1;
    const next = treeNode(tree, path) orelse return 0;
    tree.stack.append(allocator, tree.current) catch return 0;
    tree.current = next;
    return 1;
}

pub export fn bk_tree_finish(handle: ?*anyopaque) callconv(.c) void {
    const tree = fromHandle(Tree, handle) orelse return;
    if (tree.stack.pop()) |previous| tree.current = previous;
}

pub export fn bk_tree_size(handle: ?*anyopaque) callconv(.c) c_int {
    const tree = fromHandle(Tree, handle) orelse return 0;
    return @intCast(tree.current.text.len);
}

pub export fn bk_tree_string(handle: ?*anyopaque, destination: ?*anyopaque) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const output = destination orelse return false;
    const bytes = @as([*]u8, @ptrCast(output));
    @memcpy(bytes[0..tree.current.text.len], tree.current.text);
    bytes[tree.current.text.len] = 0;
    return true;
}

pub export fn bk_tree_int(handle: ?*anyopaque, name: [*:0]const u8, value: ?*c_int) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const result = value orelse return false;
    // Navigate to the named child element; fall back to an attribute on the
    // current node so both storage styles work.
    const node = treeNode(tree, std.mem.span(name)) orelse {
        const attr = xml.attribute(tree.current, std.mem.span(name)) orelse return false;
        result.* = parseTreeInt(attr) catch return false;
        return true;
    };
    result.* = parseTreeInt(node.text) catch return false;
    return true;
}

/// Parse a legacy data-tree integer value.
/// The XML serialiser historically stored ints as 8 lower-case hex digits in
/// little-endian byte order ("01000000" = LE bytes 01 00 00 00 = int 1).
/// Plain decimal strings are also accepted for forward compatibility.
fn parseTreeInt(text: []const u8) !c_int {
    const trimmed = std.mem.trim(u8, text, " \t\r\n");
    if (trimmed.len == 8) {
        // Check whether all eight characters are hex digits.
        var all_hex = true;
        for (trimmed) |c| {
            if (!std.ascii.isHex(c)) { all_hex = false; break; }
        }
        if (all_hex) {
            // Decode as four little-endian bytes.
            var bytes: [4]u8 = undefined;
            bytes[0] = try std.fmt.parseInt(u8, trimmed[0..2], 16);
            bytes[1] = try std.fmt.parseInt(u8, trimmed[2..4], 16);
            bytes[2] = try std.fmt.parseInt(u8, trimmed[4..6], 16);
            bytes[3] = try std.fmt.parseInt(u8, trimmed[6..8], 16);
            return @bitCast(std.mem.readInt(u32, &bytes, .little));
        }
    }
    // Try unsigned 32-bit first so hex values like 0xffffbe34 (which exceed
    // i32 max) parse correctly.  Fall back to signed parsing for negative
    // decimal strings such as "-1".
    if (std.fmt.parseInt(u32, trimmed, 0)) |unsigned| {
        return @bitCast(unsigned);
    } else |_| {}
    return std.fmt.parseInt(c_int, trimmed, 0);
}

pub export fn bk_tree_double(handle: ?*anyopaque, name: [*:0]const u8, value: ?*f64) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const result = value orelse return false;
    const text = xml.attribute(tree.current, std.mem.span(name)) orelse return false;
    result.* = std.fmt.parseFloat(f64, text) catch return false;
    return true;
}

pub export fn bk_tree_start_container(handle: ?*anyopaque, name: [*:0]const u8) callconv(.c) c_int {
    const tree = fromHandle(Tree, handle) orelse return 0;
    const chunk = std.mem.span(name);
    const container = treeNode(tree, if (chunk.len == 0) "data" else chunk) orelse return 0;
    tree.stack.append(allocator, tree.current) catch return 0;
    tree.containers.append(allocator, container) catch {
        _ = tree.stack.pop();
        return 0;
    };
    tree.current = container;
    return 1;
}

pub export fn bk_tree_count(handle: ?*anyopaque, _: [*:0]const u8) callconv(.c) c_int {
    const tree = fromHandle(Tree, handle) orelse return 0;
    const container = tree.containers.getLastOrNull() orelse return 0;
    var count: c_int = 0;
    for (container.children.items) |item| {
        if (std.mem.eql(u8, item.name, "item")) count += 1;
    }
    return count;
}

pub export fn bk_tree_set_counter(handle: ?*anyopaque, index: c_int) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const container = tree.containers.getLastOrNull() orelse return false;
    if (index < 0) return false;
    var found: c_int = 0;
    for (container.children.items) |item| {
        if (!std.mem.eql(u8, item.name, "item")) continue;
        if (found == index) {
            tree.current = item;
            return true;
        }
        found += 1;
    }
    return false;
}

pub export fn bk_tree_finish_container(handle: ?*anyopaque) callconv(.c) void {
    const tree = fromHandle(Tree, handle) orelse return;
    _ = tree.containers.pop();
    bk_tree_finish(handle);
}

test "parseTreeInt handles 10-character hex color values (0x prefix)" {
    // UI colors from XML are stored as 0x-prefixed hex like 0xffffbe34 (yellow).
    // These exceed i32 max, so they must parse via u32 -> bitCast.
    try std.testing.expectEqual(@as(c_int, @bitCast(@as(u32, 0xffffbe34))), parseTreeInt("0xffffbe34") catch unreachable);
    try std.testing.expectEqual(@as(c_int, @bitCast(@as(u32, 0xff000000))), parseTreeInt("0xff000000") catch unreachable);
    // Negative decimal strings must still work.
    try std.testing.expectEqual(@as(c_int, -1), parseTreeInt("-1") catch unreachable);
    // Legacy 8-hex-digit LE format must still work.
    try std.testing.expectEqual(@as(c_int, 1), parseTreeInt("01000000") catch unreachable);
}

test "nested data-tree containers restore the outer item enumerator" {
    const source = "<base><Children><item id=\"1\"><States><item value=\"10\"/></States></item><item id=\"2\"/></Children></base>";
    const bytes = try allocator.dupe(u8, source);
    const name = try allocator.dupeZ(u8, "fixture.xml");
    const path = try allocator.dupeZ(u8, "fixture.xml");
    var stream = Stream{ .bytes = bytes, .name = name, .path = path, .access = 1 };
    defer {
        allocator.free(stream.bytes);
        allocator.free(stream.name);
        allocator.free(stream.path);
    }

    const handle = bk_tree_create(&stream, 2, "base") orelse return error.TestUnexpectedResult;
    defer bk_tree_destroy(handle);
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start_container(handle, "Children"));
    try std.testing.expectEqual(@as(c_int, 2), bk_tree_count(handle, "Children"));
    try std.testing.expect(bk_tree_set_counter(handle, 0));
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start_container(handle, "States"));
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_count(handle, "States"));
    bk_tree_finish_container(handle);
    try std.testing.expect(bk_tree_set_counter(handle, 1));
    var id: c_int = 0;
    try std.testing.expect(bk_tree_int(handle, "id", &id));
    try std.testing.expectEqual(@as(c_int, 2), id);
    bk_tree_finish_container(handle);
}

fn isXmlNameByte(byte: u8) bool {
    return std.ascii.isAlphanumeric(byte) or byte == '_' or byte == '-' or byte == ':';
}

/// Scan the opening tag starting at `pos` (just past the tag name) for an
/// attribute named `name`.  Returns the attribute value without modifying `pos`.
/// `candidate_end` is the position of the `>` (or the `>` in `/>`) that closes
/// the opening tag.
fn scanAttr(bytes: []const u8, pos: usize, name: []const u8) ?[]const u8 {
    // Find the `>` that closes this opening tag.
    const end = std.mem.indexOfPos(u8, bytes, pos, ">") orelse return null;
    var p = pos;
    while (p < end) {
        while (p < end and (bytes[p] == ' ' or bytes[p] == '\t' or bytes[p] == '\r' or bytes[p] == '\n')) : (p += 1) {}
        const a_start = p;
        while (p < end and isXmlNameByte(bytes[p])) : (p += 1) {}
        if (a_start == p) { p += 1; continue; }
        const a_name = bytes[a_start..p];
        while (p < end and bytes[p] != '=') : (p += 1) {}
        if (p >= end) break;
        p += 1; // '='
        while (p < end and (bytes[p] == ' ' or bytes[p] == '\t')) : (p += 1) {}
        if (p >= end or (bytes[p] != '\'' and bytes[p] != '"')) continue;
        const quote = bytes[p];
        p += 1;
        const v_start = p;
        while (p < end and bytes[p] != quote) : (p += 1) {}
        if (p >= end) return null;
        if (std.mem.eql(u8, a_name, name)) return bytes[v_start..p];
        p += 1;
    }
    return null;
}

/// Given `pos` which is just past the tag name of an element's opening tag,
/// find and return the position just past the `>` that closes that opening tag.
/// Works for both `<el ...>` and `<el ... />`.
fn skipTag(bytes: []const u8, pos: usize) ?usize {
    const end = std.mem.indexOfPos(u8, bytes, pos, ">") orelse return null;
    return end + 1;
}

/// Navigate from `start` to find a child opening-tag named `tag`.
/// Returns the position just past the tag name on success, null on failure.
/// The caller can then use `scanAttr` at this position to read attributes,
/// or `skipTag` to skip past the `>` to the element content.
fn findChild(bytes: []const u8, start: usize, tag: []const u8) ?usize {
    var pos = start;
    var depth: usize = 0;
    const len = bytes.len;
    while (pos < len) {
        const open = std.mem.indexOfPos(u8, bytes, pos, "<") orelse return null;
        pos = open + 1;
        if (pos >= len) return null;
        if (bytes[pos] == '/') {
            // Closing tag – skip its name
            pos += 1;
            while (pos < len and isXmlNameByte(bytes[pos])) : (pos += 1) {}
            if (depth > 0) depth -= 1;
            continue;
        }
        if (bytes[pos] == '!' or bytes[pos] == '?') { pos += 1; continue; }
        const n_start = pos;
        while (pos < len and isXmlNameByte(bytes[pos])) : (pos += 1) {}
        const el = bytes[n_start..pos];
        if (depth == 0 and std.mem.eql(u8, el, tag)) {
            // Found it – return position just past the tag name.
            return pos;
        }
        depth += 1;
        // Skip past this element's opening tag
        const tag_end = std.mem.indexOfPos(u8, bytes, pos, ">") orelse return null;
        const self_close = tag_end > 0 and bytes[tag_end - 1] == '/';
        if (self_close) depth -= 1;
        pos = tag_end + 1;
    }
    return null;
}

fn xmlAttribute(bytes: []const u8, row: []const u8, entry: []const u8) ?[]const u8 {
    const tag = lastPathSegment(row);
    var cursor: usize = 0;
    while (std.mem.indexOfPos(u8, bytes, cursor, "<")) |open| {
        cursor = open + 1;
        if (cursor >= bytes.len or bytes[cursor] == '/' or bytes[cursor] == '!' or bytes[cursor] == '?') continue;
        const name_start = cursor;
        while (cursor < bytes.len and isXmlNameByte(bytes[cursor])) : (cursor += 1) {}
        if (!std.mem.eql(u8, bytes[name_start..cursor], tag)) continue;

        // Check whether entry is a simple attribute name or a dot-separated
        // path through child elements (e.g. "Colors.Summer.Text.Default.A").
        var has_dot = false;
        for (entry) |c| if (c == '.') { has_dot = true; break; };
        if (!has_dot) {
            // Flat attribute lookup on the row element itself.
            return scanAttr(bytes, cursor, entry);
        }

        // Hierarchical path: split entry on '.' and navigate child elements.
        var path_seg: [16][]const u8 = undefined;
        var path_idx: usize = 0;
        {
            var start: usize = 0;
            for (entry, 0..) |c, i| {
                if (c == '.') {
                    path_seg[path_idx] = entry[start..i];
                    path_idx += 1;
                    start = i + 1;
                }
            }
            path_seg[path_idx] = entry[start..];
            path_idx += 1;
        }

        // cursor is just past the tag name of the row element.
        // Navigate child-element segments: for the (path_idx - 2) middle
        // segments we both find the element AND skip to its content; for
        // the final element (path_idx - 1) we only find it, because the
        // attribute lives on its own opening tag.
        var pos = cursor;

        for (path_seg[0 .. path_idx - 1]) |seg| {
            pos = findChild(bytes, pos, seg) orelse return null;
        }

        // The last path segment (path_idx - 1) is the attribute name on the
        // final element we just found.  pos is just past the tag name.
        return scanAttr(bytes, pos, path_seg[path_idx - 1]);
    }
    return null;
}

pub export fn bk_table_get_int(stream_handle: ?*anyopaque, row: [*:0]const u8, entry: [*:0]const u8, fallback: c_int) callconv(.c) c_int {
    const stream = fromHandle(Stream, stream_handle) orelse return fallback;
    const value = xmlAttribute(stream.bytes, std.mem.span(row), std.mem.span(entry)) orelse return fallback;
    return std.fmt.parseInt(c_int, value, 10) catch fallback;
}

pub export fn bk_table_get_double(stream_handle: ?*anyopaque, row: [*:0]const u8, entry: [*:0]const u8, fallback: f64) callconv(.c) f64 {
    const stream = fromHandle(Stream, stream_handle) orelse return fallback;
    const value = xmlAttribute(stream.bytes, std.mem.span(row), std.mem.span(entry)) orelse return fallback;
    return std.fmt.parseFloat(f64, value) catch fallback;
}

test "storage base keeps the directory portion of game archive masks" {
    try std.testing.expectEqualStrings(".\\data\\", pathBase(".\\data\\*.pak"));
    try std.testing.expectEqualStrings("C:\\Blitzkrieg\\Data\\", pathBase("C:\\Blitzkrieg\\Data\\*.pak"));
    try std.testing.expectEqualStrings(".\\", pathBase("*.pak"));
}

test "storage enumeration normalizes the six tutorial mission paths" {
    const handle = bk_storage_create("Data\\*.pak", 1, 0) orelse return error.TestUnexpectedResult;
    defer bk_storage_destroy(handle);
    const enumerator_handle = bk_storage_enumerator_create(handle) orelse return error.TestUnexpectedResult;
    defer bk_enumerator_destroy(enumerator_handle);

    var tutorial_count: usize = 0;
    while (bk_enumerator_next(enumerator_handle)) {
        var stats: StorageStats = undefined;
        try std.testing.expect(bk_enumerator_stats(enumerator_handle, &stats));
        const name = std.mem.span(stats.name orelse return error.TestUnexpectedResult);
        if (std.mem.startsWith(u8, name, "scenarios\\tutorials\\") and
            std.mem.endsWith(u8, name, "\\1.xml"))
        {
            tutorial_count += 1;
        }
    }

    try std.testing.expectEqual(@as(usize, 6), tutorial_count);
}

test "storage opens an entry from a repository PAK" {
    const handle = bk_storage_create("Data\\ELK\\*.pak", 1, 0) orelse return error.TestUnexpectedResult;
    defer bk_storage_destroy(handle);
    const storage = fromHandle(Storage, handle).?;
    try std.testing.expect(storage.archives.items.len > 0);
    const first_entry = storage.archives.items[0].archive.entries[0];
    const entry_name = try allocator.dupeZ(u8, first_entry.name);
    defer allocator.free(entry_name);
    const stream_handle = bk_storage_open(handle, entry_name.ptr, 1) orelse return error.TestUnexpectedResult;
    defer bk_stream_destroy(stream_handle);
    try std.testing.expectEqual(@as(c_int, @intCast(first_entry.uncompressed_size)), bk_stream_size(stream_handle));
}

test "storage overlay exposes child archive entries" {
    const base_handle = bk_storage_create("Data\\*.pak", 1, 0) orelse return error.TestUnexpectedResult;
    defer bk_storage_destroy(base_handle);
    const child_handle = bk_storage_create("Data\\ELK\\*.pak", 1, 0) orelse return error.TestUnexpectedResult;
    defer bk_storage_destroy(child_handle);
    const child = fromHandle(Storage, child_handle).?;
    const entry_name: [:0]const u8 = "movies\\intro.txt";
    try std.testing.expect(archiveEntry(child, entry_name) != null);
    try std.testing.expect(!bk_storage_exists(base_handle, entry_name.ptr));
    try std.testing.expect(bk_storage_add(base_handle, child_handle, "elk"));
    try std.testing.expect(bk_storage_exists(base_handle, entry_name.ptr));
    const stream_handle = bk_storage_open(base_handle, entry_name.ptr, 1) orelse return error.TestUnexpectedResult;
    bk_stream_destroy(stream_handle);
    try std.testing.expect(bk_storage_remove(base_handle, "elk") == child_handle);
}

test "XML table lookup reads startup attributes" {
    const fixture = "<base><Net GameVersion=\"7\"/><Sound SFXMasterVolume=\"0.9\"/></base>";
    try std.testing.expectEqualStrings("7", xmlAttribute(fixture, "Net", "GameVersion").?);
    try std.testing.expectEqualStrings("0.9", xmlAttribute(fixture, "Sound", "SFXMasterVolume").?);
}

test "XML table lookup reads hierarchical color attributes" {
    const fixture = "<base><Scene><PlayerColors><Allied4 A=\"255\" R=\"0\" G=\"255\" B=\"255\"/></PlayerColors></Scene></base>";
    const value = xmlAttribute(fixture, "Scene", "PlayerColors.Allied4.A").?;
    try std.testing.expectEqualStrings("255", value);
    const value_r = xmlAttribute(fixture, "Scene", "PlayerColors.Allied4.R").?;
    try std.testing.expectEqualStrings("0", value_r);
    const value_g = xmlAttribute(fixture, "Scene", "PlayerColors.Allied4.G").?;
    try std.testing.expectEqualStrings("255", value_g);
    const value_b = xmlAttribute(fixture, "Scene", "PlayerColors.Allied4.B").?;
    try std.testing.expectEqualStrings("255", value_b);
}

test "XML table lookup reads real consts.xml color paths" {
    // Fixture matching the actual consts.xml structure for colors
    const fixture = 
        "<base>" ++
        "<Scene>" ++
        "<Colors>" ++
        "<Summer>" ++
        "<Text>" ++
        "<Chat A=\"255\" R=\"255\" G=\"255\" B=\"90\"/>" ++
        "<Default A=\"255\" R=\"216\" G=\"189\" B=\"62\"/>" ++
        "</Text>" ++
        "</Summer>" ++
        "</Colors>" ++
        "<PlayerColors>" ++
        "<Allied4 A=\"255\" R=\"0\" G=\"255\" B=\"255\"/>" ++
        "</PlayerColors>" ++
        "</Scene>" ++
        "</base>";
    
    // Test text color: Scene.Colors.Summer.Text.Chat.A
    const chat_a = xmlAttribute(fixture, "Scene", "Colors.Summer.Text.Chat.A").?;
    try std.testing.expectEqualStrings("255", chat_a);
    const chat_r = xmlAttribute(fixture, "Scene", "Colors.Summer.Text.Chat.R").?;
    try std.testing.expectEqualStrings("255", chat_r);
    const chat_g = xmlAttribute(fixture, "Scene", "Colors.Summer.Text.Chat.G").?;
    try std.testing.expectEqualStrings("255", chat_g);
    const chat_b = xmlAttribute(fixture, "Scene", "Colors.Summer.Text.Chat.B").?;
    try std.testing.expectEqualStrings("90", chat_b);
    
    // Test default color: Scene.Colors.Summer.Text.Default.A
    const def_a = xmlAttribute(fixture, "Scene", "Colors.Summer.Text.Default.A").?;
    try std.testing.expectEqualStrings("255", def_a);
    const def_r = xmlAttribute(fixture, "Scene", "Colors.Summer.Text.Default.R").?;
    try std.testing.expectEqualStrings("216", def_r);
    const def_g = xmlAttribute(fixture, "Scene", "Colors.Summer.Text.Default.G").?;
    try std.testing.expectEqualStrings("189", def_g);
    const def_b = xmlAttribute(fixture, "Scene", "Colors.Summer.Text.Default.B").?;
    try std.testing.expectEqualStrings("62", def_b);
    
    // Test player color: Scene.PlayerColors.Allied4.A
    const allied_a = xmlAttribute(fixture, "Scene", "PlayerColors.Allied4.A").?;
    try std.testing.expectEqualStrings("255", allied_a);
    const allied_r = xmlAttribute(fixture, "Scene", "PlayerColors.Allied4.R").?;
    try std.testing.expectEqualStrings("0", allied_r);
    const allied_g = xmlAttribute(fixture, "Scene", "PlayerColors.Allied4.G").?;
    try std.testing.expectEqualStrings("255", allied_g);
    const allied_b = xmlAttribute(fixture, "Scene", "PlayerColors.Allied4.B").?;
    try std.testing.expectEqualStrings("255", allied_b);
}

pub export fn bk_streamio_temp_buffer(size: c_int, index: c_int) callconv(.c) ?*anyopaque {
    if (size <= 0 or index < 0 or index >= buffers.len) return null;

    const buffer = &buffers[@intCast(index)];
    buffer.ensureTotalCapacity(allocator, @intCast(size)) catch return null;
    buffer.items.len = @intCast(size);
    return @ptrCast(buffer.items.ptr);
}
