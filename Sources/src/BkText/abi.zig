//! The C ABI of bk_text (bk_text.h): export functions over the Zig core. Every Zig error is caught
//! into a result code and every pointer argument is checked, so a bad call from C++ is a code and
//! never a crash. Memory comes from BkMemory like the rest of the engine, so the C++ side's leak
//! report covers this library too.
const std = @import("std");
const bk_memory = @import("bk_memory_allocator");
const text = @import("root.zig");

const faces = text.faces;
const shape = text.shape;
const atlas_mod = text.atlas;
const sizes = text.sizes;

const Result = enum(c_int) {
    ok = 0,
    invalid_argument = 1,
    not_found = 2,
    out_of_memory = 3,
    buffer_too_small = 4,
    atlas_full = 5,
    font_error = 6,
};

const MissingFn = *const fn (user: ?*anyopaque, codepoint: u32, utf16_index: u32, trace_line: [*:0]const u8) callconv(.c) void;

const Glyph = extern struct { glyph_key: u32, cluster: u32, x_advance: f32, y_advance: f32, x_offset: f32, y_offset: f32 };
const Metrics = extern struct { advance: f32, width: f32, ascent: f32, descent: f32, line_space: f32 };
const Line = extern struct { start: u32, end: u32 };
const Rect = extern struct { x: u32, y: u32, w: u32, h: u32 };
const AtlasGlyph = extern struct { rect: Rect, bearing_x: i32, bearing_y: i32, generation: u32 };
const Stats = extern struct {
    atlas_generation: u32,
    atlas_glyph_count: u32,
    atlas_used_area: u64,
    atlas_eviction_count: u32,
    shape_cache_hits: u64,
    shape_cache_misses: u64,
    shape_cache_entries: u32,
    missing_glyph_count: u32,
};

const shape_cache_entries = 512;
const size_class_count = 4;
const size_class_names = [size_class_count][]const u8{ "tiny", "small", "medium", "large" };

const Context = struct {
    allocator: std.mem.Allocator,
    threaded: std.Io.Threaded,
    set: faces.FaceSet,
    atlas: atlas_mod.Atlas,
    cache: shape.Cache,
    lang_buf: [16]u8,
    lang_len: usize,
    callback: ?MissingFn = null,
    callback_user: ?*anyopaque = null,
    missing_count: u32 = 0,
};

const Font = struct {
    ctx: *Context,
    shaper: shape.Shaper,
    size_class: u32,
};

fn code(result: Result) c_int {
    return @intFromEnum(result);
}

fn fail(err: anyerror) c_int {
    return code(switch (err) {
        error.OutOfMemory => .out_of_memory,
        error.FontMissing => .not_found,
        error.AtlasFull => .atlas_full,
        else => .font_error,
    });
}

fn slice(ptr: ?[*]const u16, len: u32) ?[]const u16 {
    if (len == 0) return &.{};
    return (ptr orelse return null)[0..len];
}

/// The glyph key the atlas lookup takes: face in the top byte, glyph id below.
fn glyphKey(face: faces.Face, glyph_id: u32) u32 {
    return (@as(u32, @intFromEnum(face)) << 24) | (glyph_id & 0xFFFFFF);
}

export fn bk_text_context_create(fonts_dir: ?[*:0]const u8, lang_code: ?[*:0]const u8, out: ?*?*Context) callconv(.c) c_int {
    return createContext(fonts_dir, lang_code, out, null);
}

export fn bk_text_context_create_reporting(fonts_dir: ?[*:0]const u8, lang_code: ?[*:0]const u8, out: ?*?*Context, message: ?[*]u8, capacity: u32) callconv(.c) c_int {
    const buffer = (message orelse return code(.invalid_argument))[0..capacity];
    if (buffer.len == 0) return code(.invalid_argument);
    // One byte stays free for the NUL; a longer reason is cut there.
    var writer: std.Io.Writer = .fixed(buffer[0 .. buffer.len - 1]);
    const result = createContext(fonts_dir, lang_code, out, &writer);
    buffer[writer.end] = 0;
    return result;
}

/// Both creates: `diagnostics` takes the reason a face did not load, stderr when null.
fn createContext(fonts_dir: ?[*:0]const u8, lang_code: ?[*:0]const u8, out: ?*?*Context, diagnostics: ?*std.Io.Writer) c_int {
    const out_ptr = out orelse return code(.invalid_argument);
    out_ptr.* = null;
    const dir = std.mem.span(fonts_dir orelse return code(.invalid_argument));
    const lang = std.mem.span(lang_code orelse return code(.invalid_argument));
    if (lang.len == 0 or lang.len > 16) return code(.invalid_argument);

    const allocator = bk_memory.allocator;
    const ctx = allocator.create(Context) catch return code(.out_of_memory);
    ctx.allocator = allocator;
    ctx.callback = null;
    ctx.callback_user = null;
    ctx.missing_count = 0;
    ctx.threaded = std.Io.Threaded.init(allocator, .{});
    ctx.lang_len = lang.len;
    @memcpy(ctx.lang_buf[0..lang.len], lang);
    ctx.set = faces.FaceSet.loadReporting(allocator, ctx.threaded.io(), dir, diagnostics) catch |err| {
        ctx.threaded.deinit();
        allocator.destroy(ctx);
        return fail(err);
    };
    ctx.atlas = atlas_mod.Atlas.init(allocator, &ctx.set, atlas_mod.Atlas.default_size, atlas_mod.Atlas.default_size) catch |err| {
        ctx.set.deinit();
        ctx.threaded.deinit();
        allocator.destroy(ctx);
        return fail(err);
    };
    ctx.cache = shape.Cache.init(allocator, shape_cache_entries);
    out_ptr.* = ctx;
    return code(.ok);
}

export fn bk_text_context_destroy(ctx: ?*Context) callconv(.c) void {
    const c = ctx orelse return;
    c.cache.deinit();
    c.atlas.deinit();
    c.set.deinit();
    c.threaded.deinit();
    c.allocator.destroy(c);
}

export fn bk_text_context_set_missing_glyph_callback(ctx: ?*Context, callback: ?MissingFn, user: ?*anyopaque) callconv(.c) c_int {
    const c = ctx orelse return code(.invalid_argument);
    c.callback = callback;
    c.callback_user = user;
    return code(.ok);
}

/// The shaper's sink: builds the Game's BK_GLYPH_TRACE line and hands it to the callback, or to
/// stderr when the environment asks for it and nobody installed a callback.
fn reportMissing(context: ?*anyopaque, cp: u21, index: u32, _: []const u8) void {
    const font: *Font = @ptrCast(@alignCast(context.?));
    const ctx = font.ctx;
    ctx.missing_count +%= 1;
    var buf: [160]u8 = undefined;
    const text_line = std.fmt.bufPrint(buf[0 .. buf.len - 1], "BK_GLYPH_TRACE: no glyph U+{X:0>4} in font \"bk_text/{s}/{s}\"", .{
        cp,
        ctx.lang_buf[0..ctx.lang_len],
        size_class_names[font.size_class],
    }) catch return;
    buf[text_line.len] = 0;
    const line: [:0]const u8 = buf[0..text_line.len :0];
    if (ctx.callback) |callback| {
        callback(ctx.callback_user, cp, index, line.ptr);
    } else if (text.c.getenv("BK_GLYPH_TRACE") != null) {
        std.debug.print("{s}\n", .{line});
    }
}

/// The Game's pixel size for a size class; an unknown class is medium, as the Game treats it.
export fn bk_text_size_class_pixel_size(size_class: u32, ui_scale: f32) callconv(.c) f32 {
    const class: sizes.SizeClass = if (size_class < size_class_count) @enumFromInt(size_class) else .medium;
    return sizes.pixelSize(class, ui_scale);
}

export fn bk_text_font_open(ctx: ?*Context, size_class: u32, pixel_size: f32, out: ?*?*Font) callconv(.c) c_int {
    const out_ptr = out orelse return code(.invalid_argument);
    out_ptr.* = null;
    const c = ctx orelse return code(.invalid_argument);
    if (size_class >= size_class_count) return code(.invalid_argument);
    if (!(pixel_size >= 1.0 and pixel_size <= 512.0)) return code(.invalid_argument);

    const font = c.allocator.create(Font) catch return code(.out_of_memory);
    font.ctx = c;
    font.size_class = size_class;
    // pixel_size is the cell height of the 2003 fonts; the shaper works in em pixels.
    font.shaper = shape.Shaper.init(c.allocator, &c.set, c.lang_buf[0..c.lang_len], sizes.emForCell(&c.set, pixel_size)) catch |err| {
        c.allocator.destroy(font);
        return fail(err);
    };
    font.shaper.sink = .{ .context = font, .report = reportMissing };
    out_ptr.* = font;
    return code(.ok);
}

export fn bk_text_font_close(font: ?*Font) callconv(.c) void {
    const f = font orelse return;
    f.shaper.deinit();
    f.ctx.allocator.destroy(f);
}

export fn bk_text_shape_utf16(font: ?*Font, ptr: ?[*]const u16, len: u32, out: ?[*]Glyph, cap: u32, out_count: ?*u32) callconv(.c) c_int {
    const count_ptr = out_count orelse return code(.invalid_argument);
    const f = font orelse return code(.invalid_argument);
    const units = slice(ptr, len) orelse return code(.invalid_argument);
    if (cap != 0 and out == null) return code(.invalid_argument);
    const run = f.shaper.shapeCached(&f.ctx.cache, units) catch |err| return fail(err);
    count_ptr.* = @intCast(run.glyphs.len);
    if (run.glyphs.len > cap) return code(.buffer_too_small);
    for (run.glyphs, 0..) |g, i| out.?[i] = .{
        .glyph_key = glyphKey(g.face, g.glyph_id),
        .cluster = g.cluster,
        .x_advance = g.x_advance,
        .y_advance = g.y_advance,
        .x_offset = g.x_offset,
        .y_offset = g.y_offset,
    };
    return code(.ok);
}

export fn bk_text_measure_utf16(font: ?*Font, ptr: ?[*]const u16, len: u32, out: ?*Metrics) callconv(.c) c_int {
    const out_ptr = out orelse return code(.invalid_argument);
    const f = font orelse return code(.invalid_argument);
    const units = slice(ptr, len) orelse return code(.invalid_argument);
    const run = f.shaper.shapeCached(&f.ctx.cache, units) catch |err| return fail(err);
    const ext = text.measure.measureRun(&f.shaper, run.*);
    const face = text.measure.faceMetrics(&f.shaper, f.shaper.chain[0]);
    out_ptr.* = .{
        .advance = ext.advance,
        .width = ext.width,
        .ascent = face.ascent,
        .descent = face.descent,
        .line_space = text.measure.lineSpace(&f.shaper),
    };
    return code(.ok);
}

export fn bk_text_wrap_utf16(font: ?*Font, ptr: ?[*]const u16, len: u32, width: f32, out: ?[*]Line, cap: u32, out_count: ?*u32) callconv(.c) c_int {
    const count_ptr = out_count orelse return code(.invalid_argument);
    const f = font orelse return code(.invalid_argument);
    const units = slice(ptr, len) orelse return code(.invalid_argument);
    if (cap != 0 and out == null) return code(.invalid_argument);
    if (std.math.isNan(width)) return code(.invalid_argument);
    var lines: std.ArrayList(text.wrap.Line) = .empty;
    defer lines.deinit(f.ctx.allocator);
    text.wrap.wrapText(f.ctx.allocator, &f.shaper, units, width, &lines) catch |err| return fail(err);
    count_ptr.* = @intCast(lines.items.len);
    if (lines.items.len > cap) return code(.buffer_too_small);
    for (lines.items, 0..) |l, i| out.?[i] = .{ .start = l.start, .end = l.end };
    return code(.ok);
}

export fn bk_text_atlas_info(ctx: ?*Context, w: ?*u32, h: ?*u32, generation: ?*u32) callconv(.c) c_int {
    const c = ctx orelse return code(.invalid_argument);
    if (w) |p| p.* = c.atlas.width;
    if (h) |p| p.* = c.atlas.height;
    if (generation) |p| p.* = c.atlas.generation;
    return code(.ok);
}

export fn bk_text_atlas_glyph(font: ?*Font, glyph_key: u32, out: ?*AtlasGlyph) callconv(.c) c_int {
    const out_ptr = out orelse return code(.invalid_argument);
    const f = font orelse return code(.invalid_argument);
    const face_index = glyph_key >> 24;
    if (face_index >= faces.face_count) return code(.invalid_argument);
    const face: faces.Face = @enumFromInt(face_index);
    const entry = f.ctx.atlas.lookupOrInsert(face, glyph_key & 0xFFFFFF, f.shaper.pixel_size) catch |err| return fail(err);
    out_ptr.* = .{
        .rect = .{ .x = entry.x, .y = entry.y, .w = entry.w, .h = entry.h },
        .bearing_x = entry.bearing_x,
        .bearing_y = entry.bearing_y,
        .generation = f.ctx.atlas.generation,
    };
    return code(.ok);
}

export fn bk_text_atlas_begin_frame(ctx: ?*Context, frame: u64) callconv(.c) c_int {
    const c = ctx orelse return code(.invalid_argument);
    c.atlas.beginFrame(frame);
    return code(.ok);
}

export fn bk_text_atlas_take_dirty(ctx: ?*Context, out: ?[*]Rect, cap: u32, count: ?*u32) callconv(.c) c_int {
    const c = ctx orelse return code(.invalid_argument);
    const count_ptr = count orelse return code(.invalid_argument);
    if (cap != 0 and out == null) return code(.invalid_argument);
    const dirty = &c.atlas.dirty;
    count_ptr.* = @intCast(dirty.items.len);
    if (dirty.items.len > cap) return code(.buffer_too_small);
    for (dirty.items, 0..) |r, i| out.?[i] = .{ .x = r.x, .y = r.y, .w = r.w, .h = r.h };
    dirty.clearRetainingCapacity();
    return code(.ok);
}

export fn bk_text_atlas_pixels(ctx: ?*Context, pixels: ?*?[*]const u8, pitch: ?*u32) callconv(.c) c_int {
    const c = ctx orelse return code(.invalid_argument);
    const pixels_ptr = pixels orelse return code(.invalid_argument);
    const pitch_ptr = pitch orelse return code(.invalid_argument);
    pixels_ptr.* = c.atlas.pixels.ptr;
    pitch_ptr.* = c.atlas.width;
    return code(.ok);
}

export fn bk_text_stats(ctx: ?*Context, out: ?*Stats) callconv(.c) c_int {
    const c = ctx orelse return code(.invalid_argument);
    const out_ptr = out orelse return code(.invalid_argument);
    const a = c.atlas.stats();
    out_ptr.* = .{
        .atlas_generation = a.generation,
        .atlas_glyph_count = @intCast(a.glyph_count),
        .atlas_used_area = a.used_area,
        .atlas_eviction_count = a.eviction_count,
        .shape_cache_hits = c.cache.hits,
        .shape_cache_misses = c.cache.misses,
        .shape_cache_entries = @intCast(c.cache.count()),
        .missing_glyph_count = c.missing_count,
    };
    return code(.ok);
}
