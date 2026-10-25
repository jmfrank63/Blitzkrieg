//! The R8 glyph atlas: one CPU pixel page filled by a shelf packer, keyed by (face, glyph, pixel
//! size). S02 uploads the dirty rects to a GPU texture, so the atlas reports what changed instead
//! of owning any GPU state.
//!
//! Eviction is all or nothing: when a glyph does not fit and some entry was not used this frame,
//! the whole page is cleared, `generation` increments and the page is one dirty rect. Glyphs come
//! back on demand. Entries handed out earlier in the same frame are stale after that; a caller
//! that holds them compares `generation` before and after. If every entry was used this frame
//! (one frame alone overflows) the atlas returns error.AtlasFull and changes nothing, so the
//! caller can flush the frame or grow the page.
const std = @import("std");
const faces = @import("faces.zig");
const raster = @import("raster.zig");

pub const Rect = struct { x: u32, y: u32, w: u32, h: u32 };

pub const Entry = struct {
    /// The glyph's pixels inside the page; w = h = 0 for a glyph without ink (no pixels, no slot).
    x: u32,
    y: u32,
    w: u32,
    h: u32,
    bearing_x: i32,
    bearing_y: i32,
    last_used_frame: u64,
};

pub const Key = struct {
    face: faces.Face,
    glyph_id: u32,
    /// The pixel size's f32 bits, so 16 and 16.5 are different sizes and no float compare is needed.
    size_bits: u32,
};

pub const Stats = struct {
    generation: u32,
    glyph_count: usize,
    /// Pixels covered by glyph bitmaps (padding not counted).
    used_area: u64,
    eviction_count: u32,
};

pub const Error = error{AtlasFull} || std.mem.Allocator.Error;

/// A glyph is packed with this much empty border on every side so sampling never bleeds.
const padding: u32 = 1;

const Pos = struct { x: u32, y: u32 };
const Shelf = struct { y: u32, h: u32, x: u32 };

pub const Atlas = struct {
    allocator: std.mem.Allocator,
    set: *const faces.FaceSet,
    width: u32,
    height: u32,
    pixels: []u8,
    entries: std.AutoHashMapUnmanaged(Key, Entry) = .empty,
    shelves: std.ArrayList(Shelf) = .empty,
    dirty: std.ArrayList(Rect) = .empty,
    frame: u64 = 0,
    generation: u32 = 0,
    eviction_count: u32 = 0,
    used_area: u64 = 0,

    pub const default_size: u32 = 1024;

    pub fn init(allocator: std.mem.Allocator, set: *const faces.FaceSet, width: u32, height: u32) !Atlas {
        const pixels = try allocator.alloc(u8, @as(usize, width) * height);
        @memset(pixels, 0);
        return .{ .allocator = allocator, .set = set, .width = width, .height = height, .pixels = pixels };
    }

    pub fn deinit(self: *Atlas) void {
        self.allocator.free(self.pixels);
        self.entries.deinit(self.allocator);
        self.shelves.deinit(self.allocator);
        self.dirty.deinit(self.allocator);
    }

    /// Marks the frame glyph lookups belong to; entries used in it are not evicted.
    pub fn beginFrame(self: *Atlas, n: u64) void {
        self.frame = n;
    }

    pub fn stats(self: *const Atlas) Stats {
        return .{
            .generation = self.generation,
            .glyph_count = self.entries.count(),
            .used_area = self.used_area,
            .eviction_count = self.eviction_count,
        };
    }

    /// The cached entry without rasterising or touching its frame.
    pub fn find(self: *const Atlas, key: Key) ?Entry {
        return self.entries.get(key);
    }

    /// The entry for a glyph, rasterising and packing it on a miss.
    pub fn lookupOrInsert(self: *Atlas, face: faces.Face, glyph_id: u32, pixel_size: f32) Error!Entry {
        const key: Key = .{ .face = face, .glyph_id = glyph_id, .size_bits = @bitCast(pixel_size) };
        if (self.entries.getPtr(key)) |hit| {
            hit.last_used_frame = self.frame;
            return hit.*;
        }
        const bitmap = try raster.rasterise(self.allocator, self.set, face, glyph_id, pixel_size);
        defer bitmap.deinit(self.allocator);

        var entry: Entry = .{
            .x = 0,
            .y = 0,
            .w = bitmap.width,
            .h = bitmap.height,
            .bearing_x = bitmap.bearing_x,
            .bearing_y = bitmap.bearing_y,
            .last_used_frame = self.frame,
        };
        if (bitmap.width != 0) {
            const slot_w = bitmap.width + 2 * padding;
            const slot_h = bitmap.height + 2 * padding;
            if (slot_w > self.width or slot_h > self.height) return error.AtlasFull;
            var pos = try self.place(slot_w, slot_h);
            if (pos == null) {
                if (!self.hasStale()) return error.AtlasFull;
                try self.evict();
                pos = try self.place(slot_w, slot_h);
            }
            const at = pos orelse return error.AtlasFull;
            entry.x = at.x + padding;
            entry.y = at.y + padding;
            for (0..bitmap.height) |row| {
                const dst = (@as(usize, entry.y) + row) * self.width + entry.x;
                const src = row * bitmap.width;
                @memcpy(self.pixels[dst..][0..bitmap.width], bitmap.pixels[src..][0..bitmap.width]);
            }
            try self.dirty.append(self.allocator, .{ .x = entry.x, .y = entry.y, .w = entry.w, .h = entry.h });
            self.used_area += @as(u64, entry.w) * entry.h;
        }
        try self.entries.put(self.allocator, key, entry);
        return entry;
    }

    /// The rects changed since the last call, owned by the caller; clears the list. After an
    /// eviction that is the whole page as one rect.
    pub fn takeDirty(self: *Atlas, allocator: std.mem.Allocator) ![]Rect {
        const out = try allocator.dupe(Rect, self.dirty.items);
        self.dirty.clearRetainingCapacity();
        return out;
    }

    fn place(self: *Atlas, w: u32, h: u32) !?Pos {
        for (self.shelves.items) |*shelf| {
            if (shelf.h >= h and shelf.x + w <= self.width) {
                const at: Pos = .{ .x = shelf.x, .y = shelf.y };
                shelf.x += w;
                return at;
            }
        }
        var next_y: u32 = 0;
        if (self.shelves.items.len > 0) {
            const last = self.shelves.items[self.shelves.items.len - 1];
            next_y = last.y + last.h;
        }
        if (next_y + h > self.height) return null;
        try self.shelves.append(self.allocator, .{ .y = next_y, .h = h, .x = w });
        return .{ .x = 0, .y = next_y };
    }

    /// Whether clearing would free anything the current frame is not drawing.
    fn hasStale(self: *const Atlas) bool {
        var it = self.entries.valueIterator();
        while (it.next()) |e| {
            if (e.w != 0 and e.last_used_frame != self.frame) return true;
        }
        return false;
    }

    fn evict(self: *Atlas) !void {
        @memset(self.pixels, 0);
        self.entries.clearRetainingCapacity();
        self.shelves.clearRetainingCapacity();
        self.dirty.clearRetainingCapacity();
        self.used_area = 0;
        self.generation += 1;
        self.eviction_count += 1;
        // The page content changed everywhere, so the upload is one full rect.
        try self.dirty.append(self.allocator, .{ .x = 0, .y = 0, .w = self.width, .h = self.height });
    }
};

const shape = @import("shape.zig");
const c = @import("c.zig");

fn utf16Literal(comptime s: []const u8) [std.unicode.calcUtf16LeLen(s) catch unreachable]u16 {
    var out: [std.unicode.calcUtf16LeLen(s) catch unreachable]u16 = undefined;
    _ = std.unicode.utf8ToUtf16Le(&out, s) catch unreachable;
    return out;
}

fn nominal(set: *const faces.FaceSet, face: faces.Face, cp: u21) u32 {
    var glyph: c.hb.hb_codepoint_t = 0;
    _ = c.hb.hb_font_get_nominal_glyph(set.get(face).hb_font, cp, &glyph);
    return glyph;
}

const Line = struct { lang: []const u8, text: []const u16 };

fn overlaps(a: Entry, b: Entry) bool {
    return a.x < b.x + b.w and b.x < a.x + a.w and a.y < b.y + b.h and b.y < a.y + a.h;
}

test "script lines at 16 and 32 px pack without overlap and match a fresh raster" {
    const gpa = std.testing.allocator;
    var set = try faces.FaceSet.load(gpa, std.testing.io, faces.testFontsDir());
    defer set.deinit();
    var atlas = try Atlas.init(gpa, &set, Atlas.default_size, Atlas.default_size);
    defer atlas.deinit();

    const latin = utf16Literal("Příliš žluťoučký kůň");
    const cyrillic = utf16Literal("Съешь же ещё этих булок");
    const han = utf16Literal("骨直中文 日本語");
    const deva = utf16Literal("क्षत्रिय हिन्दी");
    const lines = [_]Line{
        .{ .lang = "en", .text = &latin },
        .{ .lang = "ru", .text = &cyrillic },
        .{ .lang = "zh", .text = &han },
        .{ .lang = "ja", .text = &han },
        .{ .lang = "hi", .text = &deva },
    };

    var inserted: std.ArrayList(struct { key: Key, size: f32, entry: Entry }) = .empty;
    defer inserted.deinit(gpa);
    for ([_]f32{ 16, 32 }) |size| {
        for (lines) |line| {
            var shaper = try shape.Shaper.init(gpa, &set, line.lang, size);
            defer shaper.deinit();
            const run = try shaper.shape(line.text);
            defer run.deinit(gpa);
            for (run.glyphs) |g| {
                const entry = try atlas.lookupOrInsert(g.face, g.glyph_id, size);
                const key: Key = .{ .face = g.face, .glyph_id = g.glyph_id, .size_bits = @bitCast(size) };
                try inserted.append(gpa, .{ .key = key, .size = size, .entry = entry });
            }
        }
    }
    const st = atlas.stats();
    try std.testing.expect(st.glyph_count > 40);
    try std.testing.expectEqual(@as(u32, 0), st.generation);
    try std.testing.expectEqual(@as(u32, 0), st.eviction_count);

    // Entries are unique per key; collect them and check bounds and overlap.
    var it = atlas.entries.iterator();
    var all: std.ArrayList(Entry) = .empty;
    defer all.deinit(gpa);
    var area: u64 = 0;
    while (it.next()) |kv| {
        const e = kv.value_ptr.*;
        try std.testing.expect(e.x + e.w <= atlas.width and e.y + e.h <= atlas.height);
        area += @as(u64, e.w) * e.h;
        try all.append(gpa, e);
    }
    try std.testing.expectEqual(area, st.used_area);
    for (all.items, 0..) |a, i| {
        for (all.items[i + 1 ..]) |b| try std.testing.expect(!overlaps(a, b));
    }

    // Pixels equal a fresh rasterisation, and the dirty rects cover every inserted entry.
    const dirty = try atlas.takeDirty(gpa);
    defer gpa.free(dirty);
    try std.testing.expectEqual(@as(usize, 0), (try atlas.takeDirty(gpa)).len);
    for (inserted.items) |ins| {
        const bm = try raster.rasterise(gpa, &set, ins.key.face, ins.key.glyph_id, ins.size);
        defer bm.deinit(gpa);
        try std.testing.expectEqual(bm.width, ins.entry.w);
        try std.testing.expectEqual(bm.height, ins.entry.h);
        try std.testing.expectEqual(bm.bearing_x, ins.entry.bearing_x);
        try std.testing.expectEqual(bm.bearing_y, ins.entry.bearing_y);
        for (0..bm.height) |row| {
            const at = (@as(usize, ins.entry.y) + row) * atlas.width + ins.entry.x;
            try std.testing.expectEqualSlices(u8, bm.pixels[row * bm.width ..][0..bm.width], atlas.pixels[at..][0..bm.width]);
        }
        if (ins.entry.w == 0) continue;
        var covered = false;
        for (dirty) |r| {
            if (r.x <= ins.entry.x and r.y <= ins.entry.y and r.x + r.w >= ins.entry.x + ins.entry.w and r.y + r.h >= ins.entry.y + ins.entry.h) covered = true;
        }
        try std.testing.expect(covered);
    }
}

test "a repeated lookup is a hit with no new dirty rect" {
    const gpa = std.testing.allocator;
    var set = try faces.FaceSet.load(gpa, std.testing.io, faces.testFontsDir());
    defer set.deinit();
    var atlas = try Atlas.init(gpa, &set, 128, 128);
    defer atlas.deinit();
    const g = nominal(&set, .liberation_serif, 'A');
    const first = try atlas.lookupOrInsert(.liberation_serif, g, 16);
    gpa.free(try atlas.takeDirty(gpa));
    atlas.beginFrame(5);
    const again = try atlas.lookupOrInsert(.liberation_serif, g, 16);
    try std.testing.expectEqual(first.x, again.x);
    try std.testing.expectEqual(first.y, again.y);
    try std.testing.expectEqual(@as(u64, 5), again.last_used_frame);
    const dirty = try atlas.takeDirty(gpa);
    defer gpa.free(dirty);
    try std.testing.expectEqual(@as(usize, 0), dirty.len);
    try std.testing.expectEqual(@as(usize, 1), atlas.stats().glyph_count);
    // Another size of the same glyph is another entry.
    _ = try atlas.lookupOrInsert(.liberation_serif, g, 17);
    try std.testing.expectEqual(@as(usize, 2), atlas.stats().glyph_count);
}

test "a space is an entry without pixels or a dirty rect" {
    const gpa = std.testing.allocator;
    var set = try faces.FaceSet.load(gpa, std.testing.io, faces.testFontsDir());
    defer set.deinit();
    var atlas = try Atlas.init(gpa, &set, 64, 64);
    defer atlas.deinit();
    const e = try atlas.lookupOrInsert(.liberation_serif, nominal(&set, .liberation_serif, ' '), 16);
    try std.testing.expectEqual(@as(u32, 0), e.w);
    try std.testing.expectEqual(@as(u32, 0), e.h);
    const dirty = try atlas.takeDirty(gpa);
    defer gpa.free(dirty);
    try std.testing.expectEqual(@as(usize, 0), dirty.len);
    try std.testing.expectEqual(@as(u64, 0), atlas.stats().used_area);
    try std.testing.expectEqual(@as(usize, 1), atlas.stats().glyph_count);
}

test "a full atlas evicts everything unused, bumps the generation and re-inserts" {
    const gpa = std.testing.allocator;
    var set = try faces.FaceSet.load(gpa, std.testing.io, faces.testFontsDir());
    defer set.deinit();
    var atlas = try Atlas.init(gpa, &set, 64, 64);
    defer atlas.deinit();

    const letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const first_key: Key = .{ .face = .liberation_serif, .glyph_id = nominal(&set, .liberation_serif, 'A'), .size_bits = @bitCast(@as(f32, 16)) };
    var next: usize = 0;
    var frame: u64 = 0;
    while (atlas.stats().eviction_count == 0) : (frame += 1) {
        try std.testing.expect(frame < 20);
        atlas.beginFrame(frame);
        for (0..6) |_| {
            _ = try atlas.lookupOrInsert(.liberation_serif, nominal(&set, .liberation_serif, letters[next % letters.len]), 16);
            next += 1;
        }
    }
    const st = atlas.stats();
    try std.testing.expectEqual(@as(u32, 1), st.generation);
    try std.testing.expectEqual(@as(u32, 1), st.eviction_count);
    try std.testing.expect(atlas.find(first_key) == null);

    // The whole page is one dirty rect (plus the glyph that triggered the eviction).
    const dirty = try atlas.takeDirty(gpa);
    defer gpa.free(dirty);
    try std.testing.expectEqual(Rect{ .x = 0, .y = 0, .w = 64, .h = 64 }, dirty[0]);

    // Re-insert works and the page holds only what came after the eviction.
    atlas.beginFrame(frame);
    const again = try atlas.lookupOrInsert(.liberation_serif, first_key.glyph_id, 16);
    try std.testing.expect(again.w > 0);
    try std.testing.expect(atlas.find(first_key) != null);
    var it = atlas.entries.valueIterator();
    var area: u64 = 0;
    while (it.next()) |e| area += @as(u64, e.w) * e.h;
    try std.testing.expectEqual(area, atlas.stats().used_area);
}

test "one frame that overflows the atlas returns AtlasFull and changes nothing" {
    const gpa = std.testing.allocator;
    var set = try faces.FaceSet.load(gpa, std.testing.io, faces.testFontsDir());
    defer set.deinit();
    var atlas = try Atlas.init(gpa, &set, 64, 64);
    defer atlas.deinit();
    atlas.beginFrame(1);
    const letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    var failed = false;
    for (letters) |ch| {
        _ = atlas.lookupOrInsert(.liberation_serif, nominal(&set, .liberation_serif, ch), 32) catch |err| {
            try std.testing.expectEqual(error.AtlasFull, err);
            failed = true;
            break;
        };
    }
    try std.testing.expect(failed);
    try std.testing.expectEqual(@as(u32, 0), atlas.stats().generation);
    try std.testing.expectEqual(@as(u32, 0), atlas.stats().eviction_count);
    try std.testing.expect(atlas.stats().glyph_count > 0);

    // A glyph larger than the whole page can never fit, even in an empty atlas.
    var tiny = try Atlas.init(gpa, &set, 8, 8);
    defer tiny.deinit();
    try std.testing.expectError(error.AtlasFull, tiny.lookupOrInsert(.liberation_serif, nominal(&set, .liberation_serif, 'W'), 32));
}
