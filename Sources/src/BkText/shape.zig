//! Shaping: segment.zig's runs through HarfBuzz into positioned glyphs with
//! absolute UTF-16 clusters, and an LRU cache of shaped runs.
const std = @import("std");
const c = @import("c.zig");
const faces = @import("faces.zig");
const segment_mod = @import("segment.zig");
const utf16 = @import("utf16.zig");

const hb = c.hb;

pub const Glyph = struct {
    face: faces.Face,
    glyph_id: u32,
    /// UTF-16 index in the shaped text where the glyph's cluster starts.
    cluster: u32,
    /// Pixels. y follows HarfBuzz: positive is up.
    x_advance: f32,
    y_advance: f32,
    x_offset: f32,
    y_offset: f32,
};

/// UTF-16 units [start, end) drawn by glyphs [glyph_start, glyph_start +
/// glyph_count). The end is the next larger distinct start of the same run
/// (the run's end for the last), whatever the direction, as in Shaper.cpp.
pub const Cluster = struct {
    start: u32,
    end: u32,
    glyph_start: u32,
    glyph_count: u32,
};

/// Glyphs of all runs in text order, each run in visual order. Hard-break
/// characters have no glyphs and no cluster.
pub const GlyphRun = struct {
    glyphs: []Glyph,
    clusters: []Cluster,

    pub fn deinit(self: GlyphRun, allocator: std.mem.Allocator) void {
        allocator.free(self.glyphs);
        allocator.free(self.clusters);
    }
};

/// Called once per code point and shaper for a glyph the chain cannot draw.
pub const MissingSink = struct {
    context: ?*anyopaque = null,
    report: *const fn (context: ?*anyopaque, cp: u21, index: u32, language: []const u8) void,
};

/// The BCP-47 tag HarfBuzz gets for a context code: Han needs the script.
pub fn languageTag(code: []const u8) []const u8 {
    if (std.mem.eql(u8, code, "zh")) return "zh-Hans";
    return code;
}

pub const Shaper = struct {
    allocator: std.mem.Allocator,
    set: *const faces.FaceSet,
    chain: [faces.face_count]faces.Face,
    lang_buf: [16]u8 = undefined,
    lang_len: usize,
    language: hb.hb_language_t,
    pixel_size: f32,
    /// One hb_font per face at this shaper's scale, so the shared faces in the
    /// FaceSet are never rescaled.
    fonts: [faces.face_count]*hb.hb_font_t,
    buffer: *hb.hb_buffer_t,
    sink: ?MissingSink = null,
    reported: std.AutoHashMapUnmanaged(u21, void) = .empty,

    pub fn init(allocator: std.mem.Allocator, set: *const faces.FaceSet, lang_code: []const u8, pixel_size: f32) !Shaper {
        var self: Shaper = .{
            .allocator = allocator,
            .set = set,
            .chain = faces.chainFor(lang_code),
            .lang_len = @min(lang_code.len, 16),
            .language = undefined,
            .pixel_size = pixel_size,
            .fonts = undefined,
            .buffer = undefined,
        };
        @memcpy(self.lang_buf[0..self.lang_len], lang_code[0..self.lang_len]);
        const tag = languageTag(self.lang_buf[0..self.lang_len]);
        self.language = hb.hb_language_from_string(tag.ptr, @intCast(tag.len));
        self.buffer = hb.hb_buffer_create() orelse return error.OutOfMemory;
        errdefer hb.hb_buffer_destroy(self.buffer);
        const scale: c_int = @intFromFloat(@round(pixel_size * 64));
        var made: usize = 0;
        errdefer for (self.fonts[0..made]) |f| hb.hb_font_destroy(f);
        for (std.enums.values(faces.Face)) |face| {
            const font = hb.hb_font_create(set.get(face).hb_face) orelse return error.OutOfMemory;
            hb.hb_font_set_scale(font, scale, scale);
            self.fonts[@intFromEnum(face)] = font;
            made += 1;
        }
        return self;
    }

    pub fn deinit(self: *Shaper) void {
        for (self.fonts) |f| hb.hb_font_destroy(f);
        hb.hb_buffer_destroy(self.buffer);
        self.reported.deinit(self.allocator);
    }

    pub fn languageCode(self: *const Shaper) []const u8 {
        return self.lang_buf[0..self.lang_len];
    }

    /// Identifies the fallback chain for the cache key.
    pub fn chainId(self: *const Shaper) u32 {
        var id: u32 = 0;
        for (self.chain) |f| id = id * 8 + @intFromEnum(f);
        return id;
    }

    pub fn shape(self: *Shaper, text: []const u16) !GlyphRun {
        const runs = try segment_mod.segment(self.allocator, self.set, &self.chain, text);
        defer self.allocator.free(runs);

        var glyphs: std.ArrayList(Glyph) = .empty;
        errdefer glyphs.deinit(self.allocator);
        var clusters: std.ArrayList(Cluster) = .empty;
        errdefer clusters.deinit(self.allocator);
        var starts: std.ArrayList(u32) = .empty;
        defer starts.deinit(self.allocator);

        for (runs) |run| {
            if (run.hard_break) continue;
            hb.hb_buffer_clear_contents(self.buffer);
            hb.hb_buffer_add_utf16(self.buffer, text.ptr, @intCast(text.len), run.start, @intCast(run.len));
            hb.hb_buffer_set_script(self.buffer, run.script);
            hb.hb_buffer_set_direction(self.buffer, hb.hb_script_get_horizontal_direction(run.script));
            hb.hb_buffer_set_language(self.buffer, self.language);
            hb.hb_shape(self.fonts[@intFromEnum(run.face)], self.buffer, null, 0);

            var n: c_uint = 0;
            const infos = hb.hb_buffer_get_glyph_infos(self.buffer, &n)[0..n];
            const positions = hb.hb_buffer_get_glyph_positions(self.buffer, &n)[0..n];
            const first_glyph: u32 = @intCast(glyphs.items.len);
            const first_cluster = clusters.items.len;
            starts.clearRetainingCapacity();
            for (infos, positions, 0..) |info, pos, i| {
                try glyphs.append(self.allocator, .{
                    .face = run.face,
                    .glyph_id = info.codepoint,
                    .cluster = info.cluster,
                    .x_advance = @as(f32, @floatFromInt(pos.x_advance)) / 64.0,
                    .y_advance = @as(f32, @floatFromInt(pos.y_advance)) / 64.0,
                    .x_offset = @as(f32, @floatFromInt(pos.x_offset)) / 64.0,
                    .y_offset = @as(f32, @floatFromInt(pos.y_offset)) / 64.0,
                });
                if (info.codepoint == 0) self.reportMissing(text, info.cluster);
                if (i == 0 or info.cluster != infos[i - 1].cluster) {
                    try clusters.append(self.allocator, .{ .start = info.cluster, .end = run.start + run.len, .glyph_start = first_glyph + @as(u32, @intCast(i)), .glyph_count = 0 });
                    try starts.append(self.allocator, info.cluster);
                }
                clusters.items[clusters.items.len - 1].glyph_count += 1;
            }
            // A cluster ends where the next larger start of the run begins.
            std.mem.sort(u32, starts.items, {}, std.sort.asc(u32));
            for (clusters.items[first_cluster..]) |*cl| {
                const at = std.sort.upperBound(u32, starts.items, cl.start, orderU32);
                if (at < starts.items.len) cl.end = starts.items[at];
            }
        }
        const glyph_slice = try glyphs.toOwnedSlice(self.allocator);
        errdefer self.allocator.free(glyph_slice);
        return .{ .glyphs = glyph_slice, .clusters = try clusters.toOwnedSlice(self.allocator) };
    }

    fn reportMissing(self: *Shaper, text: []const u16, index: u32) void {
        var it = utf16.iterate(text[index..]);
        const cp = (it.next() orelse return).cp;
        const sink = self.sink orelse return;
        const gop = self.reported.getOrPut(self.allocator, cp) catch return;
        if (gop.found_existing) return;
        sink.report(sink.context, cp, index, self.languageCode());
    }

    /// Shapes through `cache`; the run is borrowed until the next call on the
    /// cache that can evict.
    pub fn shapeCached(self: *Shaper, cache: *Cache, text: []const u16) !*const GlyphRun {
        const key: Key = .{
            .chain = self.chainId(),
            .size_bits = @bitCast(self.pixel_size),
            .language = self.languageCode(),
            .text = text,
            .hash = hashText(text),
        };
        if (cache.map.get(key)) |node| {
            cache.hits += 1;
            cache.touch(node);
            return &node.run;
        }
        cache.misses += 1;
        const run = try self.shape(text);
        errdefer run.deinit(self.allocator);
        return cache.insert(key, run);
    }
};

fn orderU32(context: u32, item: u32) std.math.Order {
    return std.math.order(context, item);
}

fn hashText(text: []const u16) u64 {
    return std.hash.Wyhash.hash(0, std.mem.sliceAsBytes(text));
}

const Key = struct {
    chain: u32,
    size_bits: u32,
    language: []const u8,
    text: []const u16,
    hash: u64,
};

const KeyContext = struct {
    pub fn hash(_: KeyContext, k: Key) u64 {
        return k.hash ^ (@as(u64, k.chain) << 32 | k.size_bits) ^ std.hash.Wyhash.hash(1, k.language) ^ k.text.len;
    }
    pub fn eql(_: KeyContext, a: Key, b: Key) bool {
        return a.chain == b.chain and a.size_bits == b.size_bits and a.hash == b.hash and
            std.mem.eql(u8, a.language, b.language) and std.mem.eql(u16, a.text, b.text);
    }
};

const Node = struct {
    key: Key,
    run: GlyphRun,
    prev: ?*Node = null,
    next: ?*Node = null,
};

/// LRU cache of shaped runs keyed by (chain, pixel size, language, text); the
/// text is compared on a hash hit, so a collision is only a miss.
pub const Cache = struct {
    allocator: std.mem.Allocator,
    max_entries: usize,
    map: std.HashMapUnmanaged(Key, *Node, KeyContext, 80) = .empty,
    head: ?*Node = null,
    tail: ?*Node = null,
    hits: u64 = 0,
    misses: u64 = 0,
    evictions: u64 = 0,

    pub fn init(allocator: std.mem.Allocator, max_entries: usize) Cache {
        return .{ .allocator = allocator, .max_entries = @max(max_entries, 1) };
    }

    pub fn deinit(self: *Cache) void {
        var node = self.head;
        while (node) |n| {
            node = n.next;
            self.free(n);
        }
        self.map.deinit(self.allocator);
        self.head = null;
        self.tail = null;
    }

    pub fn count(self: *const Cache) usize {
        return self.map.count();
    }

    fn free(self: *Cache, node: *Node) void {
        node.run.deinit(self.allocator);
        self.allocator.free(node.key.language);
        self.allocator.free(node.key.text);
        self.allocator.destroy(node);
    }

    fn unlink(self: *Cache, node: *Node) void {
        if (node.prev) |p| p.next = node.next else self.head = node.next;
        if (node.next) |n| n.prev = node.prev else self.tail = node.prev;
        node.prev = null;
        node.next = null;
    }

    fn pushFront(self: *Cache, node: *Node) void {
        node.next = self.head;
        if (self.head) |h| h.prev = node;
        self.head = node;
        if (self.tail == null) self.tail = node;
    }

    fn touch(self: *Cache, node: *Node) void {
        if (self.head == node) return;
        self.unlink(node);
        self.pushFront(node);
    }

    fn insert(self: *Cache, key: Key, run: GlyphRun) !*const GlyphRun {
        while (self.map.count() >= self.max_entries) {
            const victim = self.tail.?;
            self.unlink(victim);
            _ = self.map.remove(victim.key);
            self.free(victim);
            self.evictions += 1;
        }
        const node = try self.allocator.create(Node);
        errdefer self.allocator.destroy(node);
        const text = try self.allocator.dupe(u16, key.text);
        errdefer self.allocator.free(text);
        const language = try self.allocator.dupe(u8, key.language);
        errdefer self.allocator.free(language);
        node.* = .{ .key = .{ .chain = key.chain, .size_bits = key.size_bits, .language = language, .text = text, .hash = key.hash }, .run = run };
        try self.map.put(self.allocator, node.key, node);
        self.pushFront(node);
        return &node.run;
    }
};

// ---- tests ----

fn loadSet() !faces.FaceSet {
    return faces.FaceSet.load(std.testing.allocator, std.testing.io, faces.testFontsDir());
}

fn expectTiles(run: GlyphRun, len: u32) !void {
    var covered = try std.testing.allocator.alloc(u8, len);
    defer std.testing.allocator.free(covered);
    @memset(covered, 0);
    for (run.clusters) |cl| {
        try std.testing.expect(cl.start < cl.end and cl.end <= len);
        for (cl.start..cl.end) |u| covered[u] += 1;
        try std.testing.expect(cl.glyph_count > 0);
    }
    for (covered) |n| try std.testing.expectEqual(@as(u8, 1), n);
}

fn width(run: GlyphRun) f32 {
    var w: f32 = 0;
    for (run.glyphs) |g| w += g.x_advance;
    return w;
}

test "Latin: kerning narrows AV and precomposed accents are one glyph each" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try Shaper.init(std.testing.allocator, &set, "en", 40);
    defer shaper.deinit();

    const av = try shaper.shape(&utf16.literal("AV"));
    defer av.deinit(std.testing.allocator);
    const a = try shaper.shape(&utf16.literal("A"));
    defer a.deinit(std.testing.allocator);
    const v = try shaper.shape(&utf16.literal("V"));
    defer v.deinit(std.testing.allocator);
    try std.testing.expect(width(av) < width(a) + width(v) - 0.5);

    const accents = try shaper.shape(&utf16.literal("ÄÉé"));
    defer accents.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(usize, 3), accents.glyphs.len);
    try std.testing.expectEqual(@as(usize, 3), accents.clusters.len);

    const line_text = utf16.literal("Äv Évé fiancé AV To");
    const line = try shaper.shape(&line_text);
    defer line.deinit(std.testing.allocator);
    try expectTiles(line, line_text.len);
    for (line.glyphs) |g| {
        try std.testing.expectEqual(faces.Face.liberation_serif, g.face);
        try std.testing.expect(g.glyph_id != 0);
    }
}

test "Cyrillic comes entirely from Liberation Serif" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try Shaper.init(std.testing.allocator, &set, "ru", 24);
    defer shaper.deinit();
    const run = try shaper.shape(&utf16.literal("Жёлтый"));
    defer run.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(usize, 6), run.glyphs.len);
    for (run.glyphs) |g| {
        try std.testing.expectEqual(faces.Face.liberation_serif, g.face);
        try std.testing.expect(g.glyph_id != 0);
    }
}

test "mixed Latin, Han and Devanagari give serif, a CJK face, Devanagari in order" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try Shaper.init(std.testing.allocator, &set, "en", 24);
    defer shaper.deinit();
    const text = utf16.literal("Tank 戦車 टैंक");
    const run = try shaper.shape(&text);
    defer run.deinit(std.testing.allocator);
    try expectTiles(run, text.len);
    var seen: [3]?faces.Face = .{ null, null, null };
    var n: usize = 0;
    for (run.glyphs) |g| {
        const u = text[g.cluster];
        if (u == ' ') continue;
        if (n > 0 and seen[n - 1].? == g.face) continue;
        try std.testing.expect(n < 3);
        seen[n] = g.face;
        n += 1;
    }
    try std.testing.expectEqual(@as(usize, 3), n);
    try std.testing.expectEqual(faces.Face.liberation_serif, seen[0].?);
    try std.testing.expect(seen[1].? == .noto_sans_sc or seen[1].? == .noto_sans_jp);
    try std.testing.expectEqual(faces.Face.noto_sans_devanagari, seen[2].?);
}

test "Han goes to the face of the language" {
    var set = try loadSet();
    defer set.deinit();
    const expected = [_]struct { code: []const u8, face: faces.Face }{
        .{ .code = "ja", .face = .noto_sans_jp },
        .{ .code = "zh", .face = .noto_sans_sc },
        .{ .code = "en", .face = .noto_sans_sc },
    };
    for (expected) |e| {
        var shaper = try Shaper.init(std.testing.allocator, &set, e.code, 32);
        defer shaper.deinit();
        const run = try shaper.shape(&utf16.literal("骨"));
        defer run.deinit(std.testing.allocator);
        try std.testing.expectEqual(@as(usize, 1), run.glyphs.len);
        try std.testing.expectEqual(e.face, run.glyphs[0].face);
        try std.testing.expect(run.glyphs[0].glyph_id != 0);
    }
}

test "Devanagari cluster ranges match Shaper.cpp" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try Shaper.init(std.testing.allocator, &set, "hi", 32);
    defer shaper.deinit();

    // Values of bk_shaper_test.cpp (CheckDevanagari): a conjunct and a matra
    // form one cluster; "क्षत्रिय हिन्दी" has starts 0 3 7 8 9 11 13 (spike).
    const text = utf16.literal("क्षत्रिय हिन्दी");
    const run = try shaper.shape(&text);
    defer run.deinit(std.testing.allocator);
    const want = [_][2]u32{ .{ 0, 3 }, .{ 3, 7 }, .{ 7, 8 }, .{ 8, 9 }, .{ 9, 11 }, .{ 11, 13 }, .{ 13, 15 } };
    try std.testing.expectEqual(want.len, run.clusters.len);
    for (want, run.clusters) |w, cl| {
        try std.testing.expectEqual(w[0], cl.start);
        try std.testing.expectEqual(w[1], cl.end);
    }

    const ki = try shaper.shape(&utf16.literal("कि"));
    defer ki.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(usize, 1), ki.clusters.len);
    try std.testing.expectEqual(@as(u32, 0), ki.clusters[0].start);
    try std.testing.expectEqual(@as(u32, 2), ki.clusters[0].end);
    try std.testing.expect(ki.glyphs.len >= 2);

    inline for (.{ "क्ष", "त्र", "ज्ञ", "श्र" }) |conjunct| {
        const cj = utf16.literal(conjunct);
        const r = try shaper.shape(&cj);
        defer r.deinit(std.testing.allocator);
        try std.testing.expectEqual(@as(usize, 1), r.clusters.len);
        try std.testing.expectEqual(@as(u32, cj.len), r.clusters[0].end);
    }

    const triple = utf16.literal("कि खि गि");
    const t = try shaper.shape(&triple);
    defer t.deinit(std.testing.allocator);
    const want3 = [_][2]u32{ .{ 0, 2 }, .{ 2, 3 }, .{ 3, 5 }, .{ 5, 6 }, .{ 6, 8 } };
    try std.testing.expectEqual(want3.len, t.clusters.len);
    for (want3, t.clusters) |w, cl| {
        try std.testing.expectEqual(w[0], cl.start);
        try std.testing.expectEqual(w[1], cl.end);
    }

    const game = utf16.literal("नया खेल शुरू करें क्षत्र विक्रम");
    const g = try shaper.shape(&game);
    defer g.deinit(std.testing.allocator);
    try expectTiles(g, game.len);
}

test "a combining mark shapes in its base's face" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try Shaper.init(std.testing.allocator, &set, "en", 32);
    defer shaper.deinit();
    const run = try shaper.shape(&utf16.literal("e\u{0301}x"));
    defer run.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(u32, 0), run.clusters[0].start);
    try std.testing.expectEqual(@as(u32, 2), run.clusters[0].end);
    for (run.glyphs) |g| try std.testing.expectEqual(faces.Face.liberation_serif, g.face);
}

const Sink = struct {
    calls: usize = 0,
    cp: u21 = 0,
    index: u32 = 0,
    fn report(context: ?*anyopaque, cp: u21, index: u32, _: []const u8) void {
        const self: *Sink = @ptrCast(@alignCast(context.?));
        self.calls += 1;
        self.cp = cp;
        self.index = index;
    }
};

test "a code point no face covers is glyph 0 in the last face, reported once" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try Shaper.init(std.testing.allocator, &set, "en", 32);
    defer shaper.deinit();
    var sink: Sink = .{};
    shaper.sink = .{ .context = &sink, .report = Sink.report };
    const text = utf16.literal("\u{10FFFD}x\u{10FFFD}");
    for (0..2) |_| {
        const run = try shaper.shape(&text);
        defer run.deinit(std.testing.allocator);
        try std.testing.expectEqual(@as(u32, 0), run.glyphs[0].glyph_id);
        try std.testing.expectEqual(faces.Face.noto_sans_devanagari, run.glyphs[0].face);
        try std.testing.expectEqual(@as(usize, 3), run.clusters.len);
        try std.testing.expectEqual(@as(u32, 2), run.clusters[0].end);
    }
    try std.testing.expectEqual(@as(usize, 1), sink.calls);
    try std.testing.expectEqual(@as(u21, 0x10FFFD), sink.cp);
    try std.testing.expectEqual(@as(u32, 0), sink.index);
}

test "a surrogate pair is one cluster of two units" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try Shaper.init(std.testing.allocator, &set, "ja", 32);
    defer shaper.deinit();
    const text = utf16.literal("\u{20B9F}");
    try std.testing.expectEqual(@as(usize, 2), text.len);
    const run = try shaper.shape(&text);
    defer run.deinit(std.testing.allocator);
    try std.testing.expectEqual(@as(usize, 1), run.clusters.len);
    try std.testing.expectEqual(@as(u32, 0), run.clusters[0].start);
    try std.testing.expectEqual(@as(u32, 2), run.clusters[0].end);
    try std.testing.expectEqual(faces.Face.noto_sans_jp, run.glyphs[0].face);
    try std.testing.expect(run.glyphs[0].glyph_id != 0);
}

test "the cache returns identical data on a hit and evicts at its bound" {
    var set = try loadSet();
    defer set.deinit();
    var shaper = try Shaper.init(std.testing.allocator, &set, "en", 20);
    defer shaper.deinit();
    var cache = Cache.init(std.testing.allocator, 2);
    defer cache.deinit();

    const one = utf16.literal("one");
    const two = utf16.literal("two");
    const three = utf16.literal("three");
    const first = try shaper.shapeCached(&cache, &one);
    const copy = try std.testing.allocator.dupe(Glyph, first.glyphs);
    defer std.testing.allocator.free(copy);
    _ = try shaper.shapeCached(&cache, &two);
    const again = try shaper.shapeCached(&cache, &one);
    try std.testing.expectEqualSlices(Glyph, copy, again.glyphs);
    try std.testing.expectEqual(@as(u64, 1), cache.hits);
    try std.testing.expectEqual(@as(u64, 2), cache.misses);

    // "two" is now the least recently used; "three" evicts it, "one" stays.
    _ = try shaper.shapeCached(&cache, &three);
    try std.testing.expectEqual(@as(usize, 2), cache.count());
    try std.testing.expectEqual(@as(u64, 1), cache.evictions);
    _ = try shaper.shapeCached(&cache, &one);
    try std.testing.expectEqual(@as(u64, 2), cache.hits);
    _ = try shaper.shapeCached(&cache, &two);
    try std.testing.expectEqual(@as(u64, 4), cache.misses);

    // Another size is another key for the same text.
    var big = try Shaper.init(std.testing.allocator, &set, "en", 40);
    defer big.deinit();
    const wide = width((try big.shapeCached(&cache, &two)).*);
    const narrow = width((try shaper.shapeCached(&cache, &two)).*);
    try std.testing.expect(wide > narrow);
}
