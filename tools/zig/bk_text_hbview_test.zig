//! test-bk-text-hbview (M004 S01 T06): bk_text's lines against hb-view's, by code (D069). hb-view
//! shapes with HarfBuzz and rasters with FreeType and cairo, a stack that shares nothing with
//! bk_text's stb_truetype rasteriser and R8 atlas, so agreement is evidence about bk_text. A
//! missing hb-view is a failure, never a skip. On a failure both rasters, a diff image and the
//! measures are left in zig-out/local-test/bk-text-hbview/.
const std = @import("std");
const bk_text = @import("bk_text");
const measure = @import("l10n_measure");

const testing = std.testing;
const faces = bk_text.faces;

const scratch_dir = "zig-out/local-test/bk-text-hbview";
const pixel_size: u32 = 32;
const ink_threshold: u8 = 64;

/// Ink boxes may differ by this many pixels in width and in height (two rasterisers round the
/// outline's edges differently).
const max_box_difference: usize = 1;
/// The total advance may differ by this many pixels from hb-shape's.
const max_advance_difference: f64 = 1.0;
/// Both rasters are the same face at the same size, so they must agree far more than the Game's
/// frames do against hb-view (l10n's 0.40 floor, whose bitmap fonts are hinted at 11 px). Measured
/// on this machine the lines reach 0.9 and above; this leaves room for FreeType against stb hinting.
const min_correlation: f64 = 0.80;

const Line = struct {
    key: []const u8,
    text: []const u8,
    language: []const u8,
    /// The language code bk_text's shaper gets (its chain, "zh" for Simplified Chinese).
    code: []const u8,
    face: faces.Face,
    script: ?[]const u8 = null,
};

const lines = [_]Line{
    .{ .key = "latin", .text = "Ärger über Échec, señor", .language = "en", .code = "en", .face = .liberation_serif },
    .{ .key = "cyrillic", .text = "Жёлтый танк", .language = "ru", .code = "ru", .face = .liberation_serif },
    .{ .key = "han-sc", .text = "坦克骨", .language = "zh-Hans", .code = "zh", .face = .noto_sans_sc },
    .{ .key = "han-jp", .text = "戦車骨", .language = "ja", .code = "ja", .face = .noto_sans_jp },
    .{ .key = "devanagari", .text = "क्षत्रिय हिन्दी", .language = "hi", .code = "hi", .face = .noto_sans_devanagari, .script = "Deva" },
};

fn fontPath(allocator: std.mem.Allocator, face: faces.Face) ![]u8 {
    return std.fs.path.join(allocator, &.{ faces.testFontsDir(), face.fileName() });
}

/// bk_text's drawing of a line: shaped, every glyph from the atlas at an integer pen position
/// (the pen rounded), added onto an R8 canvas with `left` empty columns before the pen start.
const Drawn = struct {
    image: measure.Gray,
    advance: f64,
    /// Row of the baseline in `image`.
    baseline: usize,
};

const left_margin: usize = 8;

fn drawBkText(allocator: std.mem.Allocator, set: *const faces.FaceSet, line: Line) !Drawn {
    const utf16_text = try std.unicode.utf8ToUtf16LeAlloc(allocator, line.text);
    var shaper = try bk_text.shape.Shaper.init(allocator, set, line.code, @floatFromInt(pixel_size));
    defer shaper.deinit();
    const run = try shaper.shape(utf16_text);
    defer run.deinit(allocator);
    const extents = bk_text.measure.measureRun(&shaper, run);
    var atlas = try bk_text.atlas.Atlas.init(allocator, set, 512, 512);
    defer atlas.deinit();

    const metrics = bk_text.measure.faceMetrics(&shaper, line.face);
    const ascent: usize = @intFromFloat(@ceil(metrics.ascent));
    const descent: usize = @intFromFloat(@ceil(metrics.descent));
    const width = left_margin + @as(usize, @intFromFloat(extents.width)) + left_margin;
    const height = ascent + descent + 8;
    const baseline = ascent + 4;
    const pixels = try allocator.alloc(u8, width * height);
    @memset(pixels, 0);

    var pen: f32 = 0;
    for (run.glyphs) |glyph| {
        const entry = try atlas.lookupOrInsert(glyph.face, glyph.glyph_id, @floatFromInt(pixel_size));
        const pen_x: i32 = @intFromFloat(@round(pen + glyph.x_offset));
        const pen_y: i32 = -@as(i32, @intFromFloat(@round(glyph.y_offset)));
        for (0..entry.h) |row| for (0..entry.w) |col| {
            const x = @as(i32, @intCast(left_margin)) + pen_x + entry.bearing_x + @as(i32, @intCast(col));
            const y = @as(i32, @intCast(baseline)) + pen_y - entry.bearing_y + @as(i32, @intCast(row));
            if (x < 0 or y < 0 or x >= width or y >= height) continue;
            const at = @as(usize, @intCast(y)) * width + @as(usize, @intCast(x));
            const coverage = atlas.pixels[(@as(usize, entry.y) + row) * atlas.width + entry.x + col];
            pixels[at] = @intCast(@min(255, @as(u16, pixels[at]) + coverage));
        };
        pen += glyph.x_advance;
    }
    return .{ .image = .{ .width = width, .height = height, .pixels = pixels }, .advance = extents.advance, .baseline = baseline };
}

/// hb-shape's advance sum of the line in its face, from advances in 26.6 (font size 64 times the
/// pixel size, so no rounding to whole pixels).
fn hbShapeAdvance(allocator: std.mem.Allocator, line: Line) !f64 {
    const font = try fontPath(allocator, line.face);
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(allocator, &.{
        "hb-shape",
        try std.fmt.allocPrint(allocator, "--font-file={s}", .{font}),
        try std.fmt.allocPrint(allocator, "--font-size={d}", .{pixel_size * 64}),
        try std.fmt.allocPrint(allocator, "--language={s}", .{line.language}),
        "--no-glyph-names",
        "--no-clusters",
    });
    if (line.script) |script| try argv.append(allocator, try std.fmt.allocPrint(allocator, "--script={s}", .{script}));
    try argv.appendSlice(allocator, &.{ "--", line.text });
    const result = measure.timed_run.run(allocator, testing.io, .{
        .argv = argv.items,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(60), .clock = .awake } },
    }) catch |err| switch (err) {
        error.FileNotFound => {
            std.debug.print("hb-shape is not installed (apt install libharfbuzz-bin): the tier fails without it (D069)\n", .{});
            return error.HbShapeMissing;
        },
        else => return err,
    };
    if (!result.term.success()) {
        std.debug.print("hb-shape ended {any}: {s}{s}\n", .{ result.term, result.stdout, result.stderr });
        return error.HbShapeFailed;
    }
    // "[gid+advance|gid+advance|...]"; a glyph with an offset reads gid@x,y+advance.
    var sum: f64 = 0;
    var it = std.mem.tokenizeAny(u8, result.stdout, "[]|\r\n");
    while (it.next()) |glyph| {
        const plus = std.mem.lastIndexOfScalar(u8, glyph, '+') orelse return error.HbShapeOutput;
        sum += @floatFromInt(try std.fmt.parseInt(i64, glyph[plus + 1 ..], 10));
    }
    return sum / 64.0;
}

// --- failure artifacts ---------------------------------------------------------------------

fn writeGrayPng(allocator: std.mem.Allocator, path: []const u8, image: measure.Gray) !void {
    var raw: std.ArrayList(u8) = .empty;
    for (0..image.height) |y| {
        try raw.append(allocator, 0);
        try raw.appendSlice(allocator, image.pixels[y * image.width ..][0..image.width]);
    }
    // zlib with stored blocks: no compression is needed for a diagnostic image.
    var z: std.ArrayList(u8) = .empty;
    try z.appendSlice(allocator, &.{ 0x78, 0x01 });
    var at: usize = 0;
    while (at < raw.items.len or at == 0) {
        const n = @min(65535, raw.items.len - at);
        const last: u8 = if (at + n >= raw.items.len) 1 else 0;
        try z.append(allocator, last);
        try z.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToLittle(u16, @intCast(n))));
        try z.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToLittle(u16, @intCast(~@as(u16, @intCast(n))))));
        try z.appendSlice(allocator, raw.items[at..][0..n]);
        at += n;
        if (last == 1) break;
    }
    try z.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToBig(u32, std.hash.Adler32.hash(raw.items))));

    var png: std.ArrayList(u8) = .empty;
    try png.appendSlice(allocator, "\x89PNG\r\n\x1a\n");
    var header: [13]u8 = undefined;
    std.mem.writeInt(u32, header[0..4], @intCast(image.width), .big);
    std.mem.writeInt(u32, header[4..8], @intCast(image.height), .big);
    header[8] = 8; // bit depth
    header[9] = 0; // gray
    header[10] = 0;
    header[11] = 0;
    header[12] = 0;
    try pngChunk(allocator, &png, "IHDR", &header);
    try pngChunk(allocator, &png, "IDAT", z.items);
    try pngChunk(allocator, &png, "IEND", "");
    try std.Io.Dir.cwd().writeFile(testing.io, .{ .sub_path = path, .data = png.items });
}

fn pngChunk(allocator: std.mem.Allocator, png: *std.ArrayList(u8), kind: *const [4]u8, body: []const u8) !void {
    try png.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToBig(u32, @as(u32, @intCast(body.len)))));
    const start = png.items.len;
    try png.appendSlice(allocator, kind);
    try png.appendSlice(allocator, body);
    const crc = std.hash.Crc32.hash(png.items[start..]);
    try png.appendSlice(allocator, &std.mem.toBytes(std.mem.nativeToBig(u32, crc)));
}

/// Both ink crops side by side with their absolute difference below: a 3-panel diagnostic.
fn writeDiff(allocator: std.mem.Allocator, path: []const u8, a: measure.Gray, b: measure.Gray) !void {
    const width = @max(a.width, b.width);
    const height = a.height + b.height + @max(a.height, b.height);
    const pixels = try allocator.alloc(u8, width * height);
    @memset(pixels, 0);
    for (0..a.height) |y| @memcpy(pixels[y * width ..][0..a.width], a.pixels[y * a.width ..][0..a.width]);
    for (0..b.height) |y| @memcpy(pixels[(a.height + y) * width ..][0..b.width], b.pixels[y * b.width ..][0..b.width]);
    const base = a.height + b.height;
    for (0..@min(a.height, b.height)) |y| for (0..@min(a.width, b.width)) |x| {
        const p = a.at(x, y);
        const q = b.at(x, y);
        pixels[(base + y) * width + x] = if (p > q) p - q else q - p;
    };
    try writeGrayPng(allocator, path, .{ .width = width, .height = height, .pixels = pixels });
}

// --- the tier ---------------------------------------------------------------------------

const Result = struct {
    mine_box: measure.Rect,
    reference_box: measure.Rect,
    mine_advance: f64,
    shape_advance: f64,
    correlation: f64,
};

fn compare(allocator: std.mem.Allocator, set: *const faces.FaceSet, line: Line) !Result {
    const font = try fontPath(allocator, line.face);
    const mine = try drawBkText(allocator, set, line);
    const reference = try measure.hbView(allocator, testing.io, scratch_dir, line.key, line.text, .{
        .font = font,
        .size = pixel_size,
        .language = line.language,
        .script = line.script,
    });
    const mine_box = measure.inkBox(mine.image, ink_threshold) orelse return error.NoInk;
    const reference_box = measure.inkBox(reference, ink_threshold) orelse return error.NoInk;
    const shape_advance = try hbShapeAdvance(allocator, line);
    const mine_crop = try measure.crop(allocator, mine.image, mine_box);
    const reference_crop = try measure.crop(allocator, reference, reference_box);
    const correlation = (try measure.matchReference(allocator, mine_crop, reference, ink_threshold)).correlation;
    const result: Result = .{ .mine_box = mine_box, .reference_box = reference_box, .mine_advance = mine.advance, .shape_advance = shape_advance, .correlation = correlation };

    const width_difference = @max(mine_box.width(), reference_box.width()) - @min(mine_box.width(), reference_box.width());
    const height_difference = @max(mine_box.height(), reference_box.height()) - @min(mine_box.height(), reference_box.height());
    const ok = width_difference <= max_box_difference and height_difference <= max_box_difference and
        @abs(result.mine_advance - result.shape_advance) <= max_advance_difference and correlation >= min_correlation;
    if (!ok) {
        std.debug.print("{s} \"{s}\": ink {d}x{d} against hb-view {d}x{d}, advance {d:.2} against hb-shape {d:.2}, correlation {d:.3}\n", .{
            line.key, line.text, mine_box.width(), mine_box.height(), reference_box.width(), reference_box.height(), result.mine_advance, result.shape_advance, correlation,
        });
        const mine_path = try std.fmt.allocPrint(allocator, "{s}/{s}-bk-text.png", .{ scratch_dir, line.key });
        const diff_path = try std.fmt.allocPrint(allocator, "{s}/{s}-diff.png", .{ scratch_dir, line.key });
        try writeGrayPng(allocator, mine_path, mine.image);
        try writeDiff(allocator, diff_path, mine_crop, reference_crop);
        std.debug.print("FAIL {s}: bk_text raster {s}, hb-view raster {s}/{s}.png, diff {s}\n", .{ line.key, mine_path, scratch_dir, line.key, diff_path });
        return error.LineDisagrees;
    }
    return result;
}

test "bk_text lines agree with hb-view and hb-shape" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    try std.Io.Dir.cwd().createDirPath(testing.io, scratch_dir);
    var set = try faces.FaceSet.load(testing.allocator, testing.io, faces.testFontsDir());
    defer set.deinit();
    var failed = false;
    for (lines) |line| _ = compare(allocator, &set, line) catch |err| switch (err) {
        error.LineDisagrees => failed = true,
        else => return err,
    };
    try testing.expect(!failed);
}

test "the Japanese and Simplified renders of 骨 differ in bk_text as in hb-view" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    try std.Io.Dir.cwd().createDirPath(testing.io, scratch_dir);
    var set = try faces.FaceSet.load(testing.allocator, testing.io, faces.testFontsDir());
    defer set.deinit();
    const sc_line: Line = .{ .key = "bone-sc", .text = "骨", .language = "zh-Hans", .code = "zh", .face = .noto_sans_sc };
    const jp_line: Line = .{ .key = "bone-jp", .text = "骨", .language = "ja", .code = "ja", .face = .noto_sans_jp };
    const sc = try drawBkText(allocator, &set, sc_line);
    const jp = try drawBkText(allocator, &set, jp_line);
    const sc_ref = try measure.hbView(allocator, testing.io, scratch_dir, "bone-sc", sc_line.text, .{ .font = try fontPath(allocator, .noto_sans_sc), .size = pixel_size, .language = "zh-Hans" });
    const jp_ref = try measure.hbView(allocator, testing.io, scratch_dir, "bone-jp", jp_line.text, .{ .font = try fontPath(allocator, .noto_sans_jp), .size = pixel_size, .language = "ja" });
    const sc_box = try measure.crop(allocator, sc.image, measure.inkBox(sc.image, ink_threshold).?);
    const same = (try measure.matchReference(allocator, sc_box, sc_ref, ink_threshold)).correlation;
    const other = (try measure.matchReference(allocator, sc_box, jp_ref, ink_threshold)).correlation;
    const jp_box = try measure.crop(allocator, jp.image, measure.inkBox(jp.image, ink_threshold).?);
    const jp_same = (try measure.matchReference(allocator, jp_box, jp_ref, ink_threshold)).correlation;
    const jp_other = (try measure.matchReference(allocator, jp_box, sc_ref, ink_threshold)).correlation;
    errdefer std.debug.print("骨: bk_text SC against hb-view SC {d:.3}, against JP {d:.3}; bk_text JP against hb-view JP {d:.3}, against SC {d:.3}\n", .{ same, other, jp_same, jp_other });
    // hb-view's own forms differ (l10n_measure's test); bk_text's must follow the same split.
    try testing.expect(same > other and jp_same > jp_other);
    try testing.expect(same - other > 0.02 and jp_same - jp_other > 0.02);
}
