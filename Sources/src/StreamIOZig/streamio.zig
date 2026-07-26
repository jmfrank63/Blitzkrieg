const std = @import("std");
const xml = @import("xml.zig");
const options = @import("options.zig");
const console = @import("console.zig");
const zip = @import("zip.zig");

// Callers store pointer and struct arrays in these scratch buffers
// (Singleton::GetAllObjects, updater notify batches...), so the memory must be
// strongly aligned — a plain u8 list may come back byte-aligned and UBSan
// traps the misaligned stores. 16 matches what the original got from malloc.
const TempBuffer = std.ArrayListAlignedUnmanaged(u8, .@"16");
var buffers: [10]TempBuffer = [_]TempBuffer{.empty} ** 10;

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
    // MSXML CountChunks/SetChunkCounter evaluate the XPath "<name>/item" from
    // the container's PARENT, spanning ALL same-named sibling nodes (reaction
    // data relies on repeated <second> blocks). Track (parent, name) so the
    // item enumeration can concatenate the siblings in document order.
    containers: std.ArrayListUnmanaged(TreeContainer) = .empty,
    mode: c_int,
};

const TreeContainer = struct {
    parent: *xml.Node,
    name: []const u8,
};

const StructureLevel = struct {
    start: usize,
    len: usize,
    // SetChunkCounter value; 0 means "sequential mode" where repeated lookups
    // of the same id walk forward through successive occurrences, mirroring
    // CStructureSaver2::GetShortChunk in the original StreamIO.
    number: usize = 0,
    // Cache of the last successful lookup (relative position past the found
    // chunk, its id, and the number it was found as).
    last_id: u16 = 0xffff,
    last_pos: usize = 0,
    last_number: usize = 0,

    fn clearCache(level: *StructureLevel) void {
        level.last_id = 0xffff;
        level.last_pos = 0;
    }
};

const StructureSaver = struct {
    stream: *Stream,
    levels: std.ArrayListUnmanaged(StructureLevel) = .empty,
    // Sequential cache for bk_structure_enter_object: the bridge visits
    // objects 0..N-1 in order, and rescanning the whole content chunk for
    // each index is O(N^2) over multi-megabyte saves (the "load takes 20
    // seconds" symptom). Remembers where the last found object ended.
    object_cache_valid: bool = false,
    object_cache_index: usize = 0,
    object_cache_pos: usize = 0,
};

const Enumerator = struct {
    names: std.ArrayListUnmanaged([:0]u8) = .empty,
    index: usize = 0,
};

// Case-insensitive global-variable store. The original CGlobalVars used an
// std::unordered_map (O(1)); GetGlobalVar/SetGlobalVar are called hundreds of
// times per frame across the game loop, so the previous linear scan made the
// whole main loop — and thus video frame advancement — run slower than the
// realtime audio track. Keys are stored lowercased for case-insensitive O(1)
// lookup (matches the prior eqlIgnoreCase behavior).
var globals: std.StringHashMapUnmanaged([:0]u8) = .empty;
var random_state: u32 = 0x9e3779b9;

fn lowerKey(buf: *[256]u8, key: []const u8) []const u8 {
    const len = @min(key.len, buf.len);
    for (key[0..len], 0..) |c, i| buf[i] = std.ascii.toLower(c);
    return buf[0..len];
}

fn fromHandle(comptime T: type, handle: ?*anyopaque) ?*T {
    return if (handle) |value| @ptrCast(@alignCast(value)) else null;
}

const ShortChunk = struct { id: u8, start: usize, len: usize };

// Decode one [id][len][payload] chunk at *relative* position `position` inside
// the level; advances position past the chunk.
fn readShortChunk(bytes: []const u8, level: StructureLevel, position: *usize) ?ShortChunk {
    var pos = level.start + position.*;
    const end = level.start + level.len;
    if (pos + 2 > end) return null;
    const id = bytes[pos];
    pos += 1;
    var encoded: u32 = bytes[pos];
    pos += 1;
    if ((encoded & 1) != 0) {
        if (pos + 3 > end) return null;
        encoded |= @as(u32, bytes[pos]) << 8;
        encoded |= @as(u32, bytes[pos + 1]) << 16;
        encoded |= @as(u32, bytes[pos + 2]) << 24;
        pos += 3;
    }
    const length: usize = @intCast(encoded >> 1);
    if (pos + length > end) return null;
    position.* = pos + length - level.start;
    return .{ .id = id, .start = pos, .len = length };
}

// Stateless occurrence lookup (used for counting and level bootstrap).
fn shortChunkAt(bytes: []const u8, level: StructureLevel, wanted_id: u8, wanted_number: usize) ?StructureLevel {
    var position: usize = 0;
    var match_number: usize = 0;
    while (readShortChunk(bytes, level, &position)) |chunk| {
        if (chunk.id == wanted_id) {
            match_number += 1;
            if (match_number == wanted_number) return .{ .start = chunk.start, .len = chunk.len };
        }
    }
    return null;
}

// Faithful port of CStructureSaver2::GetShortChunk: cached, and sequential
// when wanted_number is 0 (SetChunkCounter never called on this level).
fn getShortChunk(bytes: []const u8, level: *StructureLevel, wanted_id: u8, wanted_number: usize) ?StructureLevel {
    var position = level.last_pos;
    var counter = wanted_number;
    if (level.last_id != 0xffff and level.last_id == wanted_id) {
        if (wanted_number == level.last_number + 1) {
            counter = 1;
        } else {
            level.clearCache();
            return getShortChunk(bytes, level, wanted_id, wanted_number);
        }
    } else {
        if (wanted_number != 0) {
            if (level.last_pos != 0) {
                level.clearCache();
                return getShortChunk(bytes, level, wanted_id, wanted_number);
            }
        } else {
            counter = 1;
        }
    }
    while (readShortChunk(bytes, level.*, &position)) |chunk| {
        if (chunk.id == wanted_id) {
            if (counter == 1) {
                level.last_id = wanted_id;
                level.last_pos = position;
                level.last_number = wanted_number;
                return .{ .start = chunk.start, .len = chunk.len };
            }
            counter -= 1;
        }
    }
    if (level.last_pos == 0) return null;
    level.clearCache();
    return getShortChunk(bytes, level, wanted_id, wanted_number);
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
    if (saver.levels.items.len == 0) return false;
    const current = &saver.levels.items[saver.levels.items.len - 1];
    const child_level = getShortChunk(saver.stream.bytes, current, id, current.number) orelse return false;
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
    if (saver.levels.items.len == 0) {
        @memset(destination, 0);
        return;
    }
    const current = &saver.levels.items[saver.levels.items.len - 1];
    const chunk = getShortChunk(saver.stream.bytes, current, id, current.number) orelse {
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
    saver.levels.items[saver.levels.items.len - 1].number = @intCast(counter);
}

// Object-graph support. A save stream may hold, at the file root, a directory
// (chunk id 0) and per-object content (chunk id 2) alongside the main data
// (chunk id 1). These helpers read that graph without disturbing the main-data
// level stack that StartChunk/DataChunk use. The C++ bridge orchestrates the
// object lifecycle (factory creation, ptrID map); this layer only locates the
// chunks. Format mirrors CStructureSaver2: directory = N records of
// [typeID:u32 LE][ptrID:u32 LE][valid:u8]; content = N chunk-id-1 children,
// each [ptrID:u32][operator& output].
fn fileLevel(saver: *StructureSaver) StructureLevel {
    return .{ .start = 0, .len = saver.stream.bytes.len };
}

const DIR_ENTRY_SIZE: usize = 9;

pub export fn bk_structure_has_directory(handle: ?*anyopaque) callconv(.c) bool {
    const saver = fromHandle(StructureSaver, handle) orelse return false;
    return shortChunkAt(saver.stream.bytes, fileLevel(saver), 0, 1) != null;
}

pub export fn bk_structure_directory_entry(handle: ?*anyopaque, index: c_int, out_type: ?*c_int, out_ptr: ?*c_int, out_valid: ?*u8) callconv(.c) bool {
    const saver = fromHandle(StructureSaver, handle) orelse return false;
    const dir = shortChunkAt(saver.stream.bytes, fileLevel(saver), 0, 1) orelse return false;
    const idx: usize = @intCast(@max(@as(c_int, 0), index));
    const start = dir.start + idx * DIR_ENTRY_SIZE;
    if (start + DIR_ENTRY_SIZE > dir.start + dir.len) return false;
    const bytes = saver.stream.bytes;
    const type_id = std.mem.readInt(u32, bytes[start..][0..4], .little);
    const ptr_id = std.mem.readInt(u32, bytes[start + 4 ..][0..4], .little);
    if (out_type) |p| p.* = @bitCast(type_id);
    if (out_ptr) |p| p.* = @bitCast(ptr_id);
    if (out_valid) |p| p.* = bytes[start + 8];
    return true;
}

pub export fn bk_structure_object_count(handle: ?*anyopaque) callconv(.c) c_int {
    const saver = fromHandle(StructureSaver, handle) orelse return 0;
    const content = shortChunkAt(saver.stream.bytes, fileLevel(saver), 2, 1) orelse return 0;
    var count: usize = 0;
    while (shortChunkAt(saver.stream.bytes, content, 1, count + 1) != null) count += 1;
    return @intCast(count);
}

// Push object `index`'s content chunk (chunk-id-1 child of file-level chunk 2)
// onto the level stack so the bridge can read its ptrID (sub-chunk 0) and run
// the object's operator& (sub-chunk 1). Pair with bk_structure_finish.
pub export fn bk_structure_enter_object(handle: ?*anyopaque, index: c_int) callconv(.c) bool {
    const saver = fromHandle(StructureSaver, handle) orelse return false;
    const content = shortChunkAt(saver.stream.bytes, fileLevel(saver), 2, 1) orelse return false;
    const idx: usize = @intCast(@max(@as(c_int, 0), index));

    // Resume the scan just past the previously found object when the caller
    // walks sequentially; fall back to a full scan otherwise.
    var position: usize = 0;
    var remaining: usize = idx + 1;
    if (saver.object_cache_valid and idx == saver.object_cache_index + 1) {
        position = saver.object_cache_pos;
        remaining = 1;
    }
    var found: ?StructureLevel = null;
    while (readShortChunk(saver.stream.bytes, content, &position)) |chunk| {
        if (chunk.id == 1) {
            remaining -= 1;
            if (remaining == 0) {
                found = .{ .start = chunk.start, .len = chunk.len };
                break;
            }
        }
    }
    const obj = found orelse return false;
    saver.object_cache_valid = true;
    saver.object_cache_index = idx;
    saver.object_cache_pos = position;
    saver.levels.append(allocator, obj) catch return false;
    return true;
}

// Read `size` raw bytes from the CURRENT level's payload. Object references
// (StoreObject/LoadObject) are wrapped by AddInternal as a leaf sub-chunk whose
// payload is exactly the 4-byte ptrID; LoadObject reads it via this primitive
// rather than as a sub-chunk lookup.
pub export fn bk_structure_read_raw(handle: ?*anyopaque, output: ?*anyopaque, size: c_int) callconv(.c) bool {
    if (size <= 0 or output == null) return false;
    const saver = fromHandle(StructureSaver, handle) orelse return false;
    const current = saver.levels.getLastOrNull() orelse return false;
    const want: usize = @intCast(size);
    if (current.len < want) return false;
    const dst = @as([*]u8, @ptrCast(output.?))[0..want];
    @memcpy(dst, saver.stream.bytes[current.start .. current.start + want]);
    return true;
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

pub export fn bk_options_save_tree(options_handle: ?*anyopaque, tree_handle: ?*anyopaque) callconv(.c) c_int {
    const system = fromHandle(options.System, options_handle) orelse return 0;
    const tree = fromHandle(Tree, tree_handle) orelse return 0;
    if (tree.mode != 1) return 0;
    return @intCast(saveOptionsTree(system, tree) catch 0);
}

// COptionSystem::SerializeConfig write direction: an <Options><Vars> block with
// one <item> per option, sorted by key name (the original copies the map into a
// vector and std::sorts by szKeyName). CDataTreeXML stores numeric leaves as
// ATTRIBUTES and string leaves as CHILD ELEMENTS, which is exactly the shape
// options.System.loadXml reads back.
fn saveOptionsTree(system: *const options.System, tree: *Tree) !usize {
    const arena = tree.arena.allocator();
    const options_node = createTreeChild(tree, tree.current, "Options") orelse return error.OutOfMemory;
    const vars_node = createTreeChild(tree, options_node, "Vars") orelse return error.OutOfMemory;

    const sorted = try arena.alloc(*const options.Option, system.entries.items.len);
    for (system.entries.items, 0..) |*entry, index| sorted[index] = entry;
    std.mem.sort(*const options.Option, sorted, {}, optionNameLess);

    var buffer: [16]u8 = undefined;
    for (sorted) |entry| {
        const item = createTreeChild(tree, vars_node, "item") orelse return error.OutOfMemory;
        if (!setTreeAttribute(tree, item, "EditorType", try std.fmt.bufPrint(&buffer, "{d}", .{entry.editor_type}))) return error.OutOfMemory;
        // Flags go through the int DataChunk ("%d"), so big flag masks print
        // negative; parseU32Compat on the read side bit-casts them back.
        if (!setTreeAttribute(tree, item, "Flags", try std.fmt.bufPrint(&buffer, "{d}", .{@as(i32, @bitCast(entry.flags))}))) return error.OutOfMemory;
        if (!setTreeAttribute(tree, item, "Order", try std.fmt.bufPrint(&buffer, "{d}", .{entry.order}))) return error.OutOfMemory;
        try writeOptionVariant(tree, item, entry.value_type, entry.value);
        try setChildText(tree, item, "Action", entry.action);
        try setChildText(tree, item, "ActionFill", entry.action_fill);
        const default_node = createTreeChild(tree, item, "Default") orelse return error.OutOfMemory;
        try writeOptionVariant(tree, default_node, entry.value_type, entry.default_value);
        if (!setTreeAttribute(tree, item, "InstantApply", if (entry.instant_apply) "1" else "0")) return error.OutOfMemory;
        try setChildText(tree, item, "KeyName", entry.name);
    }
    return sorted.len;
}

fn optionNameLess(_: void, a: *const options.Option, b: *const options.Option) bool {
    return std.mem.order(u8, a.name, b.name) == .lt;
}

// SSerialVariantT::operator&(IDataTree): a Type attribute plus a "Var" leaf —
// an attribute for numeric types, a child element's text for VT_BSTR (8),
// nothing for VT_EMPTY (0).
fn writeOptionVariant(tree: *Tree, node: *xml.Node, value_type: u16, value: []const u8) !void {
    var buffer: [8]u8 = undefined;
    if (!setTreeAttribute(tree, node, "Type", try std.fmt.bufPrint(&buffer, "{d}", .{value_type}))) return error.OutOfMemory;
    switch (value_type) {
        0 => {},
        8 => try setChildText(tree, node, "Var", value),
        else => if (!setTreeAttribute(tree, node, "Var", value)) return error.OutOfMemory,
    }
}

fn setChildText(tree: *Tree, parent: *xml.Node, name: []const u8, text: []const u8) !void {
    const node = createTreeChild(tree, parent, name) orelse return error.OutOfMemory;
    node.text = try tree.arena.allocator().dupe(u8, text);
}

pub export fn bk_options_count(handle: ?*anyopaque) callconv(.c) c_int {
    const system = fromHandle(options.System, handle) orelse return 0;
    return @intCast(system.entries.items.len);
}

pub export fn bk_options_name_at(handle: ?*anyopaque, index: c_int) callconv(.c) ?[*:0]const u8 {
    const system = fromHandle(options.System, handle) orelse return null;
    const entry_index = optionIndex(system, index) orelse return null;
    return system.entries.items[entry_index].name.ptr;
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
    const entry_index = optionIndex(system, index) orelse return false;
    const entry = &system.entries.items[entry_index];
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

fn optionIndex(system: *const options.System, index: c_int) ?usize {
    if (index < 0) return null;
    const entry_index: usize = @intCast(index);
    if (entry_index >= system.entries.items.len) return null;
    return entry_index;
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
    var buf: [256]u8 = undefined;
    const lk = lowerKey(&buf, std.mem.span(key));
    if (globals.get(lk)) |value| return value.ptr;
    return null;
}

pub export fn bk_global_set(key: [*:0]const u8, value: [*:0]const u8) callconv(.c) void {
    const copied_value = allocator.dupeZ(u8, std.mem.span(value)) catch return;
    var buf: [256]u8 = undefined;
    const lk = lowerKey(&buf, std.mem.span(key));
    if (globals.getEntry(lk)) |entry| {
        allocator.free(entry.value_ptr.*);
        entry.value_ptr.* = copied_value;
        return;
    }
    const owned_key = allocator.dupe(u8, lk) catch {
        allocator.free(copied_value);
        return;
    };
    globals.put(allocator, owned_key, copied_value) catch {
        allocator.free(owned_key);
        allocator.free(copied_value);
    };
}

pub export fn bk_global_remove(key: [*:0]const u8) callconv(.c) void {
    var buf: [256]u8 = undefined;
    const lk = lowerKey(&buf, std.mem.span(key));
    const removed = globals.fetchRemove(lk) orelse return;
    allocator.free(removed.key);
    allocator.free(removed.value);
}

pub export fn bk_global_count() callconv(.c) c_int {
    return @intCast(globals.count());
}

// Copies the index-th key (already lowercased) into buffer as a C string and
// returns its length, or -1 if index/capacity is out of range. Iteration order
// is stable as long as the map is not mutated between calls.
pub export fn bk_global_key_at(index: c_int, buffer: [*]u8, capacity: c_int) callconv(.c) c_int {
    if (index < 0 or capacity <= 0) return -1;
    var it = globals.iterator();
    var i: c_int = 0;
    while (it.next()) |entry| {
        if (i == index) {
            const key = entry.key_ptr.*;
            if (key.len + 1 > @as(usize, @intCast(capacity))) return -1;
            @memcpy(buffer[0..key.len], key);
            buffer[key.len] = 0;
            return @intCast(key.len);
        }
        i += 1;
    }
    return -1;
}

pub export fn bk_global_clear() callconv(.c) void {
    var it = globals.iterator();
    while (it.next()) |entry| {
        allocator.free(entry.key_ptr.*);
        allocator.free(entry.value_ptr.*);
    }
    globals.clearRetainingCapacity();
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
    if (stream.path.len == 0) return true; // memory-only stream, nothing to persist
    const file = fopen(stream.path.ptr, "wb") orelse return false;
    defer _ = fclose(file);
    if (stream.bytes.len != 0 and fwrite(stream.bytes.ptr, 1, stream.bytes.len, file) != stream.bytes.len) return false;
    return fflush(file) == 0;
}

pub export fn bk_stream_destroy(handle: ?*anyopaque) callconv(.c) void {
    const stream = fromHandle(Stream, handle) orelse return;
    // The original CFileStream wrote through to the file during Write; this
    // implementation buffers in memory, so persist writable streams on close.
    if ((stream.access & 0x2) != 0 and stream.path.len != 0) _ = bk_stream_flush(handle);
    allocator.free(stream.bytes);
    allocator.free(stream.name);
    allocator.free(stream.path);
    allocator.destroy(stream);
}

// Create an in-memory Stream owning a copy of the given bytes. Used when a
// structure saver must be built over an arbitrary IDataStream whose bytes are
// already in C++ memory (e.g. a CStreamRangeAdaptor wrapping a save file) —
// bk_structure_create needs a Stream handle, so we snapshot the bytes here.
pub export fn bk_stream_create_memory(src: ?*const anyopaque, len: c_int) callconv(.c) ?*anyopaque {
    const n: usize = if (len > 0) @intCast(len) else 0;
    const bytes = allocator.alloc(u8, n) catch return null;
    if (n > 0 and src != null) {
        const src_bytes = @as([*]const u8, @ptrCast(src.?))[0..n];
        @memcpy(bytes, src_bytes);
    }
    const name = allocator.dupeZ(u8, "memory") catch { allocator.free(bytes); return null; };
    const path = allocator.dupeZ(u8, "") catch { allocator.free(bytes); allocator.free(name); return null; };
    const stream = allocator.create(Stream) catch { allocator.free(bytes); allocator.free(name); allocator.free(path); return null; };
    stream.* = .{ .bytes = bytes, .name = name, .path = path, .access = 1 };
    return stream;
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

// Write-mode node construction. All memory comes from the tree's arena, so
// bk_tree_destroy releases everything at once.
fn createTreeChild(tree: *Tree, parent: *xml.Node, name: []const u8) ?*xml.Node {
    const arena = tree.arena.allocator();
    const node = arena.create(xml.Node) catch return null;
    node.* = .{ .name = arena.dupe(u8, name) catch return null };
    parent.children.append(arena, node) catch return null;
    return node;
}

fn setTreeAttribute(tree: *Tree, node: *xml.Node, name: []const u8, value: []const u8) bool {
    const arena = tree.arena.allocator();
    const owned = arena.dupe(u8, value) catch return false;
    for (node.attributes.items) |*existing| {
        if (std.mem.eql(u8, existing.name, name)) {
            existing.value = owned;
            return true;
        }
    }
    const owned_name = arena.dupe(u8, name) catch return false;
    node.attributes.append(arena, .{ .name = owned_name, .value = owned }) catch return false;
    return true;
}

pub export fn bk_tree_create(stream_handle: ?*anyopaque, mode: c_int, base: [*:0]const u8) callconv(.c) ?*anyopaque {
    const stream = fromHandle(Stream, stream_handle) orelse return null;
    if (mode == 1) {
        // Write mode, mirroring CDataTreeXML::Open(WRITE): an empty document
        // whose root element is the base chunk; bk_tree_flush serializes it.
        var arena = std.heap.ArenaAllocator.init(allocator);
        const tree = allocator.create(Tree) catch {
            arena.deinit();
            return null;
        };
        tree.* = .{ .document = undefined, .arena = arena, .current = undefined, .mode = mode };
        const arena_allocator = tree.arena.allocator();
        const root = arena_allocator.create(xml.Node) catch {
            tree.arena.deinit();
            allocator.destroy(tree);
            return null;
        };
        const base_span = std.mem.span(base);
        root.* = .{ .name = arena_allocator.dupe(u8, if (base_span.len == 0) "base" else base_span) catch {
            tree.arena.deinit();
            allocator.destroy(tree);
            return null;
        } };
        tree.document = .{ .root = root, .allocator = arena_allocator };
        tree.current = root;
        return tree;
    }
    if (mode != 2) {
        std.debug.print("bk_tree_create: unsupported mode {d} for \"{s}\"\n", .{ mode, stream.name });
        return null;
    }
    var arena = std.heap.ArenaAllocator.init(allocator);
    const base_name = std.mem.span(base);
    const trimmed = std.mem.trim(u8, stream.bytes, " \t\r\n\x00");
    const document = blk: {
        if (trimmed.len == 0) {
            // The not-yet-implemented XML write path truncates state files to
            // 0 bytes (config.cfg, profile chapter states); reading them back
            // must behave like an empty document — a null tree crashes callers
            // that never check (e.g. GetGameStatsLocal).
            const synthetic = std.fmt.allocPrint(arena.allocator(), "<{s}/>", .{base_name}) catch {
                arena.deinit();
                return null;
            };
            break :blk xml.parse(arena.allocator(), synthetic) catch {
                arena.deinit();
                return null;
            };
        }
        break :blk xml.parse(arena.allocator(), stream.bytes) catch |err| {
            // Always leave a trace naming the offending file.
            std.debug.print("bk_tree_create: XML parse failed for \"{s}\" ({s}, {d} bytes)\n", .{ stream.name, @errorName(err), stream.bytes.len });
            arena.deinit();
            return null;
        };
    };
    var current = document.root;
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
    if (tree.mode == 1) {
        // CDataTreeXML write mode: StartChunk always CREATES a child element.
        const next = createTreeChild(tree, tree.current, path) orelse return 0;
        tree.stack.append(allocator, tree.current) catch return 0;
        tree.current = next;
        return 1;
    }
    const next = treeNode(tree, path) orelse return 0;
    tree.stack.append(allocator, tree.current) catch return 0;
    tree.current = next;
    return 1;
}

pub export fn bk_tree_write_int(handle: ?*anyopaque, name: [*:0]const u8, value: c_int) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    var buffer: [16]u8 = undefined;
    const text = std.fmt.bufPrint(&buffer, "{d}", .{value}) catch return false;
    return setTreeAttribute(tree, tree.current, std.mem.span(name), text);
}

pub export fn bk_tree_write_double(handle: ?*anyopaque, name: [*:0]const u8, value: f64) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    var buffer: [48]u8 = undefined;
    const text = std.fmt.bufPrint(&buffer, "{d}", .{value}) catch return false;
    return setTreeAttribute(tree, tree.current, std.mem.span(name), text);
}

pub export fn bk_tree_write_string(handle: ?*anyopaque, text: [*:0]const u8) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const arena = tree.arena.allocator();
    const addition = std.mem.span(text);
    // CDataTreeXML appends a text node; repeated writes concatenate.
    tree.current.text = std.mem.concat(arena, u8, &.{ tree.current.text, addition }) catch return false;
    return true;
}

pub export fn bk_tree_write_wstring(handle: ?*anyopaque, text: [*:0]const u16) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const arena = tree.arena.allocator();
    const wide = std.mem.span(text);
    // The read bridge widens stored bytes one-to-one (StringData(WORD*)), so
    // store the low bytes for a faithful round-trip of the legacy code pages.
    const bytes = arena.alloc(u8, wide.len) catch return false;
    for (wide, 0..) |unit, index| bytes[index] = @truncate(unit);
    tree.current.text = std.mem.concat(arena, u8, &.{ tree.current.text, bytes }) catch return false;
    return true;
}

pub export fn bk_tree_write_raw(handle: ?*anyopaque, data: ?*const anyopaque, size: c_int) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const source = data orelse return false;
    if (size < 0) return false;
    const arena = tree.arena.allocator();
    const bytes = @as([*]const u8, @ptrCast(source))[0..@intCast(size)];
    const encoded = arena.alloc(u8, bytes.len * 2) catch return false;
    const digits = "0123456789abcdef";
    for (bytes, 0..) |byte, index| {
        encoded[index * 2] = digits[byte >> 4];
        encoded[index * 2 + 1] = digits[byte & 0xf];
    }
    tree.current.text = std.mem.concat(arena, u8, &.{ tree.current.text, encoded }) catch return false;
    return true;
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

pub export fn bk_tree_raw(handle: ?*anyopaque, destination: ?*anyopaque, length: c_int) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const output = destination orelse return false;
    if (length < 0) return false;
    const bytes = std.mem.trim(u8, tree.current.text, " \t\r\n");
    const output_length: usize = @intCast(length);
    if (bytes.len != output_length * 2) return false;
    const result = @as([*]u8, @ptrCast(output))[0..output_length];
    for (result, 0..) |*byte, index| {
        byte.* = std.fmt.parseInt(u8, bytes[index * 2 .. index * 2 + 2], 16) catch return false;
    }
    return true;
}

pub export fn bk_tree_int(handle: ?*anyopaque, name: [*:0]const u8, value: ?*c_int) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const result = value orelse return false;
    // MSXML GetTextNode order: attribute on the current node FIRST, then the
    // named child element. Matching it matters when a node carries both an
    // attribute and a child with the same name (attribute names never contain
    // '/', so path lookups fall through naturally).
    if (xml.attribute(tree.current, std.mem.span(name))) |attr| {
        result.* = parseTreeInt(attr) catch return false;
        return true;
    }
    const node = treeNode(tree, std.mem.span(name)) orelse return false;
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
            if (!std.ascii.isHex(c)) {
                all_hex = false;
                break;
            }
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
    // Same MSXML order as bk_tree_int: attribute first, then child element
    // text (previously the child fallback was missing entirely, so floats
    // stored as <Name>1.5</Name> silently kept their defaults).
    const text = xml.attribute(tree.current, std.mem.span(name)) orelse blk: {
        const node = treeNode(tree, std.mem.span(name)) orelse return false;
        break :blk node.text;
    };
    result.* = std.fmt.parseFloat(f64, std.mem.trim(u8, text, " \t\r\n")) catch return false;
    return true;
}

pub export fn bk_tree_start_container(handle: ?*anyopaque, name: [*:0]const u8) callconv(.c) c_int {
    const tree = fromHandle(Tree, handle) orelse return 0;
    const chunk = std.mem.span(name);
    const path = if (chunk.len == 0) "data" else chunk;

    if (tree.mode == 1) {
        // CDataTreeXML write mode: create the container element but leave the
        // current element untouched — SetChunkCounter descends into new items,
        // and FinishContainerChunk pops back to the pre-container element.
        const container = createTreeChild(tree, tree.current, path) orelse return 0;
        tree.stack.append(allocator, tree.current) catch return 0;
        tree.containers.append(allocator, .{ .parent = container, .name = "item" }) catch {
            _ = tree.stack.pop();
            return 0;
        };
        return 1;
    }

    // Split an optional "a/b/name" path: siblings are enumerated among the
    // children of the node the PREFIX resolves to.
    var parent = tree.current;
    var container_name = path;
    if (std.mem.lastIndexOfScalar(u8, path, '/')) |slash| {
        parent = treeNode(tree, path[0..slash]) orelse return 0;
        container_name = path[slash + 1 ..];
    }
    const first = xml.child(parent, container_name) orelse return 0;

    const stored_name = tree.arena.allocator().dupe(u8, container_name) catch return 0;
    tree.stack.append(allocator, tree.current) catch return 0;
    tree.containers.append(allocator, .{ .parent = parent, .name = stored_name }) catch {
        _ = tree.stack.pop();
        return 0;
    };
    tree.current = first;
    return 1;
}

pub export fn bk_tree_count(handle: ?*anyopaque, _: [*:0]const u8) callconv(.c) c_int {
    const tree = fromHandle(Tree, handle) orelse return 0;
    if (tree.mode == 1) return 0; // matches CDataTreeXML::CountChunks in write mode
    const container = tree.containers.getLastOrNull() orelse return 0;
    var count: c_int = 0;
    for (container.parent.children.items) |sibling| {
        if (!std.mem.eql(u8, sibling.name, container.name)) continue;
        for (sibling.children.items) |item| {
            if (std.mem.eql(u8, item.name, "item")) count += 1;
        }
    }
    return count;
}

pub export fn bk_tree_set_counter(handle: ?*anyopaque, index: c_int) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const container = tree.containers.getLastOrNull() orelse return false;
    if (tree.mode == 1) {
        // Write mode appends a fresh <item> and makes it current.
        const item = createTreeChild(tree, container.parent, "item") orelse return false;
        tree.current = item;
        return true;
    }
    if (index < 0) return false;
    var found: c_int = 0;
    for (container.parent.children.items) |sibling| {
        if (!std.mem.eql(u8, sibling.name, container.name)) continue;
        for (sibling.children.items) |item| {
            if (!std.mem.eql(u8, item.name, "item")) continue;
            if (found == index) {
                tree.current = item;
                return true;
            }
            found += 1;
        }
    }
    return false;
}

pub export fn bk_tree_finish_container(handle: ?*anyopaque) callconv(.c) void {
    const tree = fromHandle(Tree, handle) orelse return;
    _ = tree.containers.pop();
    bk_tree_finish(handle);
}

fn writeXmlEscaped(output: *std.ArrayListUnmanaged(u8), arena: std.mem.Allocator, text: []const u8, escape_quotes: bool) !void {
    for (text) |byte| {
        switch (byte) {
            '&' => try output.appendSlice(arena, "&amp;"),
            '<' => try output.appendSlice(arena, "&lt;"),
            '>' => try output.appendSlice(arena, "&gt;"),
            '"' => if (escape_quotes) try output.appendSlice(arena, "&quot;") else try output.append(arena, byte),
            else => try output.append(arena, byte),
        }
    }
}

fn serializeNode(output: *std.ArrayListUnmanaged(u8), arena: std.mem.Allocator, node: *const xml.Node) !void {
    try output.append(arena, '<');
    try output.appendSlice(arena, node.name);
    for (node.attributes.items) |attr| {
        try output.append(arena, ' ');
        try output.appendSlice(arena, attr.name);
        try output.appendSlice(arena, "=\"");
        try writeXmlEscaped(output, arena, attr.value, true);
        try output.append(arena, '"');
    }
    if (node.children.items.len == 0 and node.text.len == 0) {
        try output.appendSlice(arena, "/>");
        return;
    }
    try output.append(arena, '>');
    try writeXmlEscaped(output, arena, node.text, false);
    for (node.children.items) |child_node| try serializeNode(output, arena, child_node);
    try output.appendSlice(arena, "</");
    try output.appendSlice(arena, node.name);
    try output.append(arena, '>');
}

pub export fn bk_tree_flush(handle: ?*anyopaque, stream_handle: ?*anyopaque) callconv(.c) bool {
    const tree = fromHandle(Tree, handle) orelse return false;
    const stream = fromHandle(Stream, stream_handle) orelse return false;
    if (tree.mode != 1) return false;
    const arena = tree.arena.allocator();
    var output: std.ArrayListUnmanaged(u8) = .empty;
    output.appendSlice(arena, "<?xml version=\"1.0\"?>\r\n") catch return false;
    serializeNode(&output, arena, tree.document.root) catch return false;
    output.appendSlice(arena, "\r\n") catch return false;

    // Replace the stream contents wholesale, like MSXML's save into the
    // freshly truncated write stream.
    const grown = allocator.realloc(stream.bytes, output.items.len) catch return false;
    stream.bytes = grown;
    @memcpy(stream.bytes, output.items);
    stream.position = stream.bytes.len;
    return bk_stream_flush(stream_handle);
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

test "write-mode tree round-trips through the reader" {
    var stream = Stream{
        .bytes = try allocator.alloc(u8, 0),
        .name = try allocator.dupeZ(u8, "state.xml"),
        .path = try allocator.dupeZ(u8, ""),
        .access = 2,
    };
    defer {
        allocator.free(stream.bytes);
        allocator.free(stream.name);
        allocator.free(stream.path);
    }

    const writer = bk_tree_create(&stream, 1, "base") orelse return error.TestUnexpectedResult;
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start(writer, "Options"));
    try std.testing.expect(bk_tree_write_int(writer, "State", 3));
    try std.testing.expect(bk_tree_write_string(writer, "hello & <world>"));
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start_container(writer, "Items"));
    try std.testing.expect(bk_tree_set_counter(writer, 0));
    try std.testing.expect(bk_tree_write_int(writer, "id", 7));
    try std.testing.expect(bk_tree_set_counter(writer, 1));
    try std.testing.expect(bk_tree_write_int(writer, "id", 9));
    bk_tree_finish_container(writer);
    const raw_bytes = [_]u8{ 0x01, 0x00, 0xff };
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start(writer, "Blob"));
    try std.testing.expect(bk_tree_write_raw(writer, &raw_bytes, raw_bytes.len));
    bk_tree_finish(writer);
    bk_tree_finish(writer);
    try std.testing.expect(bk_tree_flush(writer, &stream));
    bk_tree_destroy(writer);

    const reader = bk_tree_create(&stream, 2, "base") orelse return error.TestUnexpectedResult;
    defer bk_tree_destroy(reader);
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start(reader, "Options"));
    var state: c_int = 0;
    try std.testing.expect(bk_tree_int(reader, "State", &state));
    try std.testing.expectEqual(@as(c_int, 3), state);
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start_container(reader, "Items"));
    try std.testing.expectEqual(@as(c_int, 2), bk_tree_count(reader, "Items"));
    try std.testing.expect(bk_tree_set_counter(reader, 1));
    var id: c_int = 0;
    try std.testing.expect(bk_tree_int(reader, "id", &id));
    try std.testing.expectEqual(@as(c_int, 9), id);
    bk_tree_finish_container(reader);
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start(reader, "Blob"));
    var decoded = [_]u8{0} ** 3;
    try std.testing.expect(bk_tree_raw(reader, &decoded, decoded.len));
    try std.testing.expectEqualSlices(u8, &raw_bytes, &decoded);
}

test "real GAZ_61 unit XML decodes Type/Passangers like MSXML" {
    // The staged game data: the boarding bug reduces to whether this exact
    // file yields Type="trn_military_auto" (the transport enum name) and
    // Passangers=3 through the tree reader.
    const file = fopen("zig-out/Game/x86/Debug/Data/Units/Technics/USSR/Auto/GAZ_61/1.xml", "rb") orelse return error.SkipZigTest;
    defer _ = fclose(file);
    _ = fseek(file, 0, 2);
    const file_size: usize = @intCast(ftell(file));
    _ = fseek(file, 0, 0);
    const file_bytes = try allocator.alloc(u8, file_size);
    if (fread(file_bytes.ptr, 1, file_size, file) != file_size) {
        allocator.free(file_bytes);
        return error.TestUnexpectedResult;
    }
    var stream = Stream{
        .bytes = file_bytes,
        .name = try allocator.dupeZ(u8, "1.xml"),
        .path = try allocator.dupeZ(u8, "1.xml"),
        .access = 1,
    };
    defer {
        allocator.free(stream.bytes);
        allocator.free(stream.name);
        allocator.free(stream.path);
    }

    const handle = bk_tree_create(&stream, 2, "base") orelse return error.TestUnexpectedResult;
    defer bk_tree_destroy(handle);
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start(handle, "RPG"));

    // CTreeAccessor::Add("Type", &std::string): StartChunk + GetChunkSize +
    // StringData + FinishChunk.
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start(handle, "Type"));
    const type_len = bk_tree_size(handle);
    var buffer: [64]u8 = undefined;
    try std.testing.expect(type_len > 0 and type_len < buffer.len);
    try std.testing.expect(bk_tree_string(handle, &buffer));
    try std.testing.expectEqualStrings("trn_military_auto", buffer[0..@intCast(type_len)]);
    bk_tree_finish(handle);

    var seats: c_int = 0;
    try std.testing.expect(bk_tree_int(handle, "Passangers", &seats));
    try std.testing.expectEqual(@as(c_int, 3), seats);

    // Exposure bit 4 (ACTION_COMMAND_LOAD) via the container shape.
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start(handle, "Exposures"));
    var size: c_int = 0;
    try std.testing.expect(bk_tree_int(handle, "Size", &size));
    try std.testing.expectEqual(@as(c_int, 5), size);
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start_container(handle, "BitArray"));
    try std.testing.expect(bk_tree_set_counter(handle, 0));
    var bits: c_int = 0;
    try std.testing.expect(bk_tree_int(handle, "data", &bits));
    try std.testing.expectEqual(@as(c_int, 16), bits);
    bk_tree_finish_container(handle);
    bk_tree_finish(handle);
}

test "options save/load round-trips through the tree writer" {
    var stream = Stream{
        .bytes = try allocator.alloc(u8, 0),
        .name = try allocator.dupeZ(u8, "config.cfg"),
        .path = try allocator.dupeZ(u8, ""),
        .access = 2,
    };
    defer {
        allocator.free(stream.bytes);
        allocator.free(stream.name);
        allocator.free(stream.path);
    }

    var source = options.System.init(allocator);
    defer source.deinit();
    try source.set("Sound.Volume.Music", "7", 3); // VT_I4 -> Var attribute
    try source.set("GamePlay.PlayerName", "Player & <One>", 8); // VT_BSTR -> Var child
    try source.set("GFX.LandQuality", "1", 11); // VT_BOOL -> Var attribute

    const writer = bk_tree_create(&stream, 1, "base") orelse return error.TestUnexpectedResult;
    try std.testing.expectEqual(@as(c_int, 3), bk_options_save_tree(&source, writer));
    try std.testing.expect(bk_tree_flush(writer, &stream));
    bk_tree_destroy(writer);

    const reader = bk_tree_create(&stream, 2, "base") orelse return error.TestUnexpectedResult;
    defer bk_tree_destroy(reader);
    var loaded = options.System.init(allocator);
    defer loaded.deinit();
    try std.testing.expectEqual(@as(c_int, 3), bk_options_load_tree(&loaded, reader, false));
    try std.testing.expectEqualStrings("7", loaded.get("Sound.Volume.Music").?.value);
    try std.testing.expectEqual(@as(u16, 3), loaded.get("Sound.Volume.Music").?.value_type);
    try std.testing.expectEqualStrings("Player & <One>", loaded.get("GamePlay.PlayerName").?.value);
    try std.testing.expectEqual(@as(u16, 8), loaded.get("GamePlay.PlayerName").?.value_type);
    try std.testing.expectEqualStrings("1", loaded.get("GFX.LandQuality").?.value);
}

test "RPG stats bit arrays read like the transport 1.xml shape" {
    // Mirrors Data\Units\Technics\...\1.xml: two containers both named
    // BitArray under different parents, values in item ATTRIBUTES, Size as a
    // parent attribute. availExposures reading 16 (bit 4 = ACTION_COMMAND_LOAD)
    // is what makes infantry able to board transports.
    const source = "<RPG><Commands Size=\"128\"><BitArray><item data=\"255\"/><item data=\"7\"/></BitArray></Commands><Exposures Size=\"5\"><BitArray><item data=\"16\"/></BitArray></Exposures></RPG>";
    const bytes = try allocator.dupe(u8, source);
    const name = try allocator.dupeZ(u8, "1.xml");
    const path = try allocator.dupeZ(u8, "1.xml");
    var stream = Stream{ .bytes = bytes, .name = name, .path = path, .access = 1 };
    defer {
        allocator.free(stream.bytes);
        allocator.free(stream.name);
        allocator.free(stream.path);
    }
    const handle = bk_tree_create(&stream, 2, "RPG") orelse return error.TestUnexpectedResult;
    defer bk_tree_destroy(handle);

    var value: c_int = 0;
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start(handle, "Commands"));
    try std.testing.expect(bk_tree_int(handle, "Size", &value));
    try std.testing.expectEqual(@as(c_int, 128), value);
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start_container(handle, "BitArray"));
    try std.testing.expectEqual(@as(c_int, 2), bk_tree_count(handle, "BitArray"));
    try std.testing.expect(bk_tree_set_counter(handle, 0));
    try std.testing.expect(bk_tree_int(handle, "data", &value));
    try std.testing.expectEqual(@as(c_int, 255), value);
    try std.testing.expect(bk_tree_set_counter(handle, 1));
    try std.testing.expect(bk_tree_int(handle, "data", &value));
    try std.testing.expectEqual(@as(c_int, 7), value);
    bk_tree_finish_container(handle);
    bk_tree_finish(handle);

    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start(handle, "Exposures"));
    try std.testing.expect(bk_tree_int(handle, "Size", &value));
    try std.testing.expectEqual(@as(c_int, 5), value);
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start_container(handle, "BitArray"));
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_count(handle, "BitArray"));
    try std.testing.expect(bk_tree_set_counter(handle, 0));
    try std.testing.expect(bk_tree_int(handle, "data", &value));
    try std.testing.expectEqual(@as(c_int, 16), value);
    bk_tree_finish_container(handle);
    bk_tree_finish(handle);
}

test "duplicate same-named sibling containers concatenate their items" {
    // Reaction data (EscapeMenuReactions.xml) stores pair<enum, vector<string>>
    // as REPEATED <second> blocks; MSXML selectNodes("second/item") spans all
    // of them, so the reader must too.
    const source = "<base><CustomCheck><first>7</first><second><item id=\"1\"/></second><second><item id=\"2\"/><item id=\"3\"/></second></CustomCheck></base>";
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
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start_container(handle, "CustomCheck/second"));
    defer bk_tree_finish_container(handle);
    try std.testing.expectEqual(@as(c_int, 3), bk_tree_count(handle, "second"));
    var id: c_int = 0;
    try std.testing.expect(bk_tree_set_counter(handle, 0));
    try std.testing.expect(bk_tree_int(handle, "id", &id));
    try std.testing.expectEqual(@as(c_int, 1), id);
    try std.testing.expect(bk_tree_set_counter(handle, 2));
    try std.testing.expect(bk_tree_int(handle, "id", &id));
    try std.testing.expectEqual(@as(c_int, 3), id);
    try std.testing.expect(!bk_tree_set_counter(handle, 3));
}

test "data-tree raw rows decode legacy hexadecimal bytes" {
    const source = "<base><Passability><item size_x=\"4\" size_y=\"1\"/><item>010001ff</item></Passability></base>";
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
    try std.testing.expectEqual(@as(c_int, 1), bk_tree_start_container(handle, "Passability"));
    defer bk_tree_finish_container(handle);
    try std.testing.expect(bk_tree_set_counter(handle, 1));

    var output = [_]u8{0xaa} ** 4;
    try std.testing.expect(bk_tree_raw(handle, &output, @intCast(output.len)));
    try std.testing.expectEqualSlices(u8, &.{ 0x01, 0x00, 0x01, 0xff }, &output);
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
        if (a_start == p) {
            p += 1;
            continue;
        }
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
        if (bytes[pos] == '!' or bytes[pos] == '?') {
            pos += 1;
            continue;
        }
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

/// Element TEXT content: given `pos` just past the tag name of an opening tag,
/// return the text between `>` and the next `<` (MSXML .text for leaf values).
/// Self-closed elements yield an empty string.
fn elementText(bytes: []const u8, pos: usize) ?[]const u8 {
    const tag_end = std.mem.indexOfPos(u8, bytes, pos, ">") orelse return null;
    if (tag_end > 0 and bytes[tag_end - 1] == '/') return bytes[tag_end..tag_end];
    const content_start = tag_end + 1;
    const content_end = std.mem.indexOfPos(u8, bytes, content_start, "<") orelse return null;
    return std.mem.trim(u8, bytes[content_start..content_end], " \t\r\n");
}

/// CDataTableXML::GetNode semantics for the final path segment: try an
/// ATTRIBUTE of that name on the element at `pos` first, then fall back to a
/// CHILD ELEMENT of that name and return its text.
fn attrOrChildText(bytes: []const u8, pos: usize, name: []const u8) ?[]const u8 {
    if (scanAttr(bytes, pos, name)) |value| return value;
    const content = skipTag(bytes, pos) orelse return null;
    const child = findChild(bytes, content, name) orelse return null;
    return elementText(bytes, child);
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
        for (entry) |c| if (c == '.') {
            has_dot = true;
            break;
        };
        if (!has_dot) {
            // CDataTableXML::GetNode: attribute on the row element first,
            // then a child element's text.
            return attrOrChildText(bytes, cursor, entry);
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

        // The last path segment (path_idx - 1): attribute on the element we
        // just navigated to, else that element's CHILD of the same name (the
        // original reads pNode->text either way — consts like
        // <Actions><User><Friendly>37, 41, ...</Friendly> are element text).
        return attrOrChildText(bytes, pos, path_seg[path_idx - 1]);
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

/// CDataTableXML::GetString: copy the resolved value into `buffer` (NUL
/// terminated, truncating at size-1). Returns false when the entry does not
/// exist — the caller then applies its own default.
pub export fn bk_table_get_string(stream_handle: ?*anyopaque, row: [*:0]const u8, entry: [*:0]const u8, buffer: ?[*]u8, size: c_int) callconv(.c) bool {
    const stream = fromHandle(Stream, stream_handle) orelse return false;
    const output = buffer orelse return false;
    if (size <= 0) return false;
    const value = xmlAttribute(stream.bytes, std.mem.span(row), std.mem.span(entry)) orelse return false;
    const capacity: usize = @intCast(size - 1);
    const length = @min(value.len, capacity);
    @memcpy(output[0..length], value[0..length]);
    output[length] = 0;
    return true;
}

test "table getters read element-text consts like Actions.User.Friendly" {
    const source = "<base><World Speed=\"5\"><Actions><User><Friendly>37, 41, 27, 26, 25, 28, 4, 14, 15, 0</Friendly><Enemy/></User></Actions><MinRotateRadius>30</MinRotateRadius></World></base>";
    const bytes = try allocator.dupe(u8, source);
    const name = try allocator.dupeZ(u8, "consts.xml");
    const path = try allocator.dupeZ(u8, "consts.xml");
    var stream = Stream{ .bytes = bytes, .name = name, .path = path, .access = 1 };
    defer {
        allocator.free(stream.bytes);
        allocator.free(stream.name);
        allocator.free(stream.path);
    }

    var buffer: [128]u8 = undefined;
    // Dotted path resolving to child-element TEXT (the boarding priority list).
    try std.testing.expect(bk_table_get_string(&stream, "World", "Actions.User.Friendly", &buffer, buffer.len));
    try std.testing.expectEqualStrings("37, 41, 27, 26, 25, 28, 4, 14, 15, 0", std.mem.sliceTo(@as([*:0]u8, @ptrCast(&buffer)), 0));
    // Flat attribute still works.
    try std.testing.expectEqual(@as(c_int, 5), bk_table_get_int(&stream, "World", "Speed", -1));
    // Flat entry falling back to child-element text.
    try std.testing.expectEqual(@as(c_int, 30), bk_table_get_int(&stream, "World", "MinRotateRadius", -1));
    // Missing entry -> false, caller default preserved.
    try std.testing.expect(!bk_table_get_string(&stream, "World", "Actions.User.Nope", &buffer, buffer.len));
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
    if (index < 0 or index >= buffers.len) return null;

    // Contract of the original GetTempRawBuffer_Hook: NEVER null for a valid
    // index (buffers start at 32 bytes and reserve() only grows). Callers pass
    // size 0 and still write into the returned pointer's slack (e.g.
    // CUpdater::UpdateTurretTurn appending the vertical turn set).
    const requested: usize = if (size > 0) @intCast(size) else 0;
    const buffer = &buffers[@intCast(index)];
    buffer.ensureTotalCapacity(allocator, @max(requested, 32)) catch return null;
    buffer.items.len = requested;
    return @ptrCast(buffer.items.ptr);
}
