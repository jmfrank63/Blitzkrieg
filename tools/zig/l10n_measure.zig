//! The measures of the `l10n-game-reads-it` tier (M002 R056, D066): pure
//! functions over 8-bit gray images, and the hb-view runs that draw the
//! independent references. hb-view shapes with HarfBuzz and rasters with
//! cairo, a stack that shares nothing with the game's bk-font atlas and the
//! shaper that wrote the Private Use Area clusters, so agreement between a
//! captured frame and its hb-view raster is evidence about the game and not
//! about itself.
//!
//! D069: a missing hb-view is a failure, never a skip (`HbViewMissing`).
const std = @import("std");
/// The timed child run every l10n tier uses; see tools/zig/timed_run.zig.
pub const timed_run = @import("timed_run.zig");
const testing = std.testing;

pub const Rect = struct {
    x1: usize,
    y1: usize,
    x2: usize,
    y2: usize,

    pub fn width(self: Rect) usize {
        return self.x2 - self.x1;
    }
    pub fn height(self: Rect) usize {
        return self.y2 - self.y1;
    }
};

/// An 8-bit gray image, top row first; `pixels` belongs to the allocator that
/// made it.
pub const Gray = struct {
    width: usize,
    height: usize,
    pixels: []u8,

    pub fn at(self: Gray, x: usize, y: usize) u8 {
        return self.pixels[y * self.width + x];
    }
};

/// A PNG of 8 bits per channel, gray, RGB or RGBA, not interlaced (what
/// cairo writes), as its luminance.
pub fn decodePng(allocator: std.mem.Allocator, bytes: []const u8) !Gray {
    if (bytes.len < 8 or !std.mem.eql(u8, bytes[0..8], "\x89PNG\r\n\x1a\n")) return error.NotPng;
    var at: usize = 8;
    var width: usize = 0;
    var height: usize = 0;
    var channels: usize = 0;
    var idat: std.ArrayList(u8) = .empty;
    defer idat.deinit(allocator);
    while (at + 12 <= bytes.len) {
        const length = std.mem.readInt(u32, bytes[at..][0..4], .big);
        const kind = bytes[at + 4 ..][0..4];
        const body = bytes[at + 8 ..][0..length];
        at += 12 + length;
        if (std.mem.eql(u8, kind, "IHDR")) {
            width = std.mem.readInt(u32, body[0..4], .big);
            height = std.mem.readInt(u32, body[4..8], .big);
            if (body[8] != 8 or body[12] != 0) return error.UnsupportedPng;
            channels = switch (body[9]) {
                0 => 1,
                2 => 3,
                6 => 4,
                else => return error.UnsupportedPng,
            };
        } else if (std.mem.eql(u8, kind, "IDAT")) {
            try idat.appendSlice(allocator, body);
        } else if (std.mem.eql(u8, kind, "IEND")) break;
    }
    if (width == 0 or height == 0) return error.NotPng;

    var input: std.Io.Reader = .fixed(idat.items);
    var history: [std.compress.flate.max_window_len]u8 = undefined;
    var inflate = std.compress.flate.Decompress.init(&input, .zlib, &history);
    const stride = width * channels;
    const raw = try inflate.reader.allocRemaining(allocator, .limited((stride + 1) * height));
    defer allocator.free(raw);
    if (raw.len != (stride + 1) * height) return error.TruncatedPng;

    const pixels = try allocator.alloc(u8, width * height);
    errdefer allocator.free(pixels);
    const rows = try allocator.alloc(u8, stride * 2);
    defer allocator.free(rows);
    @memset(rows, 0);
    var previous = rows[0..stride];
    var current = rows[stride..];
    for (0..height) |y| {
        const filter = raw[y * (stride + 1)];
        const line = raw[y * (stride + 1) + 1 ..][0..stride];
        for (0..stride) |x| {
            const left: u8 = if (x >= channels) current[x - channels] else 0;
            const up = previous[x];
            const up_left: u8 = if (x >= channels) previous[x - channels] else 0;
            current[x] = line[x] +% switch (filter) {
                0 => 0,
                1 => left,
                2 => up,
                3 => @as(u8, @intCast((@as(u16, left) + up) / 2)),
                4 => paeth(left, up, up_left),
                else => return error.UnsupportedPng,
            };
        }
        for (0..width) |x| {
            const p = current[x * channels ..][0..channels];
            pixels[y * width + x] = if (channels == 1) p[0] else @intCast((@as(u32, p[0]) * 299 + @as(u32, p[1]) * 587 + @as(u32, p[2]) * 114) / 1000);
        }
        std.mem.swap([]u8, &previous, &current);
    }
    return .{ .width = width, .height = height, .pixels = pixels };
}

/// A 32-bit uncompressed true-colour TGA (what `kit.capture.encodeTga` writes) as its luminance,
/// top row first whichever origin the descriptor says.
pub fn decodeTga(allocator: std.mem.Allocator, bytes: []const u8) !Gray {
    if (bytes.len < 18) return error.NotTga;
    const id_length: usize = bytes[0];
    if (bytes[1] != 0 or bytes[2] != 2 or bytes[16] != 32) return error.UnsupportedTga;
    const width: usize = std.mem.readInt(u16, bytes[12..14], .little);
    const height: usize = std.mem.readInt(u16, bytes[14..16], .little);
    if (width == 0 or height == 0) return error.NotTga;
    const start = 18 + id_length;
    if (bytes.len < start + width * height * 4) return error.TruncatedTga;
    const top_first = bytes[17] & 0x20 != 0;
    const pixels = try allocator.alloc(u8, width * height);
    for (0..height) |y| {
        const source_row = if (top_first) y else height - 1 - y;
        for (0..width) |x| {
            const p = bytes[start + (source_row * width + x) * 4 ..][0..4]; // B, G, R, A
            pixels[y * width + x] = @intCast((@as(u32, p[2]) * 299 + @as(u32, p[1]) * 587 + @as(u32, p[0]) * 114) / 1000);
        }
    }
    return .{ .width = width, .height = height, .pixels = pixels };
}

fn paeth(a: u8, b: u8, c: u8) u8 {
    const p = @as(i32, a) + b - c;
    const pa = @abs(p - a);
    const pb = @abs(p - b);
    const pc = @abs(p - c);
    if (pa <= pb and pa <= pc) return a;
    return if (pb <= pc) b else c;
}

/// The smallest box holding every pixel above `threshold`, or null.
pub fn inkBox(image: Gray, threshold: u8) ?Rect {
    var box = Rect{ .x1 = image.width, .y1 = image.height, .x2 = 0, .y2 = 0 };
    for (0..image.height) |y| for (0..image.width) |x| {
        if (image.at(x, y) <= threshold) continue;
        box = .{ .x1 = @min(box.x1, x), .y1 = @min(box.y1, y), .x2 = @max(box.x2, x + 1), .y2 = @max(box.y2, y + 1) };
    };
    return if (box.x2 == 0) null else box;
}

/// Pixels of `rect` (in a frame `stride` pixels wide) whose RGB differs between
/// two RGBA frames by more than `tolerance` in any channel.
pub fn changedPixels(a: []const u8, b: []const u8, stride: usize, rect: Rect, tolerance: u8) usize {
    var changed: usize = 0;
    for (rect.y1..rect.y2) |y| for (rect.x1..rect.x2) |x| {
        const at = (y * stride + x) * 4;
        for (0..3) |c| {
            if (@abs(@as(i16, a[at + c]) - @as(i16, b[at + c])) > tolerance) {
                changed += 1;
                break;
            }
        }
    };
    return changed;
}

/// Whether the image holds a `w` by `h` window in which at least `min_fill`
/// (0..1) of the pixels are above `threshold`: the solid box a bitmap font
/// draws for a code point it has no glyph for. Letter strokes at the sizes of
/// the game's fonts leave such a window mostly empty.
pub fn hasSolidBox(image: Gray, threshold: u8, w: usize, h: usize, min_fill: f64) bool {
    if (image.width < w or image.height < h) return false;
    const need: usize = @intFromFloat(@ceil(min_fill * @as(f64, @floatFromInt(w * h))));
    for (0..image.height - h + 1) |y| for (0..image.width - w + 1) |x| {
        var lit: usize = 0;
        for (0..h) |dy| for (0..w) |dx| {
            if (image.at(x + dx, y + dy) > threshold) lit += 1;
        };
        if (lit >= need) return true;
    };
    return false;
}

pub fn crop(allocator: std.mem.Allocator, image: Gray, rect: Rect) !Gray {
    const pixels = try allocator.alloc(u8, rect.width() * rect.height());
    for (0..rect.height()) |y| @memcpy(pixels[y * rect.width() ..][0..rect.width()], image.pixels[(rect.y1 + y) * image.width + rect.x1 ..][0..rect.width()]);
    return .{ .width = rect.width(), .height = rect.height(), .pixels = pixels };
}

/// The image on a grid of `width` by `height`, every new pixel the mean of the
/// area it covers (fractional edges weighted), the filter that keeps a
/// downscaled hb-view raster comparable to a pixel the Game's linear filter
/// drew.
pub fn resample(allocator: std.mem.Allocator, image: Gray, width: usize, height: usize) !Gray {
    const pixels = try allocator.alloc(u8, width * height);
    const sx = @as(f64, @floatFromInt(image.width)) / @as(f64, @floatFromInt(width));
    const sy = @as(f64, @floatFromInt(image.height)) / @as(f64, @floatFromInt(height));
    for (0..height) |y| for (0..width) |x| {
        const x0 = @as(f64, @floatFromInt(x)) * sx;
        const x1 = x0 + sx;
        const y0 = @as(f64, @floatFromInt(y)) * sy;
        const y1 = y0 + sy;
        var sum: f64 = 0;
        var weight: f64 = 0;
        var iy: usize = @intFromFloat(@floor(y0));
        while (@as(f64, @floatFromInt(iy)) < y1 and iy < image.height) : (iy += 1) {
            const wy = @min(y1, @as(f64, @floatFromInt(iy + 1))) - @max(y0, @as(f64, @floatFromInt(iy)));
            var ix: usize = @intFromFloat(@floor(x0));
            while (@as(f64, @floatFromInt(ix)) < x1 and ix < image.width) : (ix += 1) {
                const wx = @min(x1, @as(f64, @floatFromInt(ix + 1))) - @max(x0, @as(f64, @floatFromInt(ix)));
                sum += wx * wy * @as(f64, @floatFromInt(image.at(ix, iy)));
                weight += wx * wy;
            }
        }
        pixels[y * width + x] = @intFromFloat(@round(sum / weight));
    };
    return .{ .width = width, .height = height, .pixels = pixels };
}

/// Normalised correlation (Pearson) of two images of one size: 1 for the same
/// ink pattern whatever its brightness, 0 for unrelated, negative for
/// inverted. A constant image has no pattern and gives 0.
pub fn correlation(a: Gray, b: Gray) f64 {
    std.debug.assert(a.width == b.width and a.height == b.height);
    const n: f64 = @floatFromInt(a.pixels.len);
    var mean_a: f64 = 0;
    var mean_b: f64 = 0;
    for (a.pixels, b.pixels) |x, y| {
        mean_a += @floatFromInt(x);
        mean_b += @floatFromInt(y);
    }
    mean_a /= n;
    mean_b /= n;
    var cov: f64 = 0;
    var var_a: f64 = 0;
    var var_b: f64 = 0;
    for (a.pixels, b.pixels) |x, y| {
        const dx = @as(f64, @floatFromInt(x)) - mean_a;
        const dy = @as(f64, @floatFromInt(y)) - mean_b;
        cov += dx * dy;
        var_a += dx * dx;
        var_b += dy * dy;
    }
    if (var_a == 0 or var_b == 0) return 0;
    return cov / @sqrt(var_a * var_b);
}

/// The ink of a frame box against a reference raster: the reference cut to its
/// ink box and resampled onto the frame box's grid, then correlated.
/// `aspect` is the reference's width to height over the frame's, 1 when the
/// two boxes have the same proportions.
pub const Match = struct { correlation: f64, aspect: f64 };

pub fn matchReference(allocator: std.mem.Allocator, frame_box: Gray, reference: Gray, reference_threshold: u8) !Match {
    const box = inkBox(reference, reference_threshold) orelse return error.NoInk;
    const cut = try crop(allocator, reference, box);
    defer allocator.free(cut.pixels);
    const scaled = try resample(allocator, cut, frame_box.width, frame_box.height);
    defer allocator.free(scaled.pixels);
    const reference_aspect = @as(f64, @floatFromInt(box.width())) / @as(f64, @floatFromInt(box.height()));
    const frame_aspect = @as(f64, @floatFromInt(frame_box.width)) / @as(f64, @floatFromInt(frame_box.height));
    return .{ .correlation = correlation(frame_box, scaled), .aspect = frame_aspect / reference_aspect };
}

/// The image smoothed by a box of `radius` pixels around each pixel (clamped at
/// the edges). Text of about 11 pixels' height is hinted by the Game's own font
/// raster, so its strokes sit a pixel off a shaper's; the smoothed images are
/// compared instead.
pub fn blur(allocator: std.mem.Allocator, image: Gray, radius: usize) !Gray {
    const pixels = try allocator.alloc(u8, image.pixels.len);
    for (0..image.height) |y| for (0..image.width) |x| {
        var sum: usize = 0;
        var count: usize = 0;
        for (y -| radius..@min(y + radius + 1, image.height)) |yy| for (x -| radius..@min(x + radius + 1, image.width)) |xx| {
            sum += image.at(xx, yy);
            count += 1;
        };
        pixels[y * image.width + x] = @intCast(sum / count);
    };
    return .{ .width = image.width, .height = image.height, .pixels = pixels };
}

/// `matchReference` with both images smoothed by `blur` of `radius` first.
pub fn matchReferenceBlurred(allocator: std.mem.Allocator, frame_box: Gray, reference: Gray, reference_threshold: u8, radius: usize) !Match {
    const box = inkBox(reference, reference_threshold) orelse return error.NoInk;
    const cut = try crop(allocator, reference, box);
    defer allocator.free(cut.pixels);
    const scaled = try resample(allocator, cut, frame_box.width, frame_box.height);
    defer allocator.free(scaled.pixels);
    const soft_reference = try blur(allocator, scaled, radius);
    defer allocator.free(soft_reference.pixels);
    const soft_frame = try blur(allocator, frame_box, radius);
    defer allocator.free(soft_frame.pixels);
    const reference_aspect = @as(f64, @floatFromInt(box.width())) / @as(f64, @floatFromInt(box.height()));
    const frame_aspect = @as(f64, @floatFromInt(frame_box.width)) / @as(f64, @floatFromInt(frame_box.height));
    return .{ .correlation = correlation(soft_frame, soft_reference), .aspect = frame_aspect / reference_aspect };
}

/// The words of a one-line box: runs of columns with ink, split where at
/// least `min_gap` empty columns separate them. Boxes are relative to the image.
pub fn words(allocator: std.mem.Allocator, image: Gray, threshold: u8, min_gap: usize) ![]Rect {
    var list: std.ArrayList(Rect) = .empty;
    var start: ?usize = null;
    var empty: usize = 0;
    for (0..image.width + 1) |x| {
        var inked = false;
        if (x < image.width) for (0..image.height) |y| {
            if (image.at(x, y) > threshold) {
                inked = true;
                break;
            }
        };
        if (inked) {
            if (start == null) start = x;
            empty = 0;
        } else if (start != null) {
            empty += 1;
            if (empty >= min_gap or x == image.width) {
                try list.append(allocator, .{ .x1 = start.?, .y1 = 0, .x2 = x + 1 - empty, .y2 = image.height });
                start = null;
                empty = 0;
            }
        }
    }
    return list.toOwnedSlice(allocator);
}

/// `image` cut into exactly `n` words at its `n - 1` widest gaps between inked
/// columns, for a text whose word count is known. A space is wider than any gap
/// inside a word, but how wide a gap inside a word is depends on the font's and
/// the rasteriser's version (HarfBuzz 14.6's hb-view opens one inside मित्रों that
/// Ubuntu's does not), so a fixed minimum gap does not count words reliably.
/// Null when the image has fewer than `n - 1` gaps.
pub fn wordsN(allocator: std.mem.Allocator, image: Gray, threshold: u8, n: usize) !?[]Rect {
    const Gap = struct { start: usize, end: usize };
    var gaps: std.ArrayList(Gap) = .empty;
    var first: ?usize = null;
    var last: usize = 0;
    var gap_start: ?usize = null;
    for (0..image.width) |x| {
        var inked = false;
        for (0..image.height) |y| {
            if (image.at(x, y) > threshold) {
                inked = true;
                break;
            }
        }
        if (inked) {
            if (first == null) first = x;
            if (gap_start) |g| try gaps.append(allocator, .{ .start = g, .end = x });
            gap_start = null;
            last = x + 1;
        } else if (first != null and gap_start == null) gap_start = x;
    }
    const begin = first orelse return null;
    if (n == 0 or gaps.items.len + 1 < n) return null;
    // The n - 1 widest gaps, then back in column order.
    std.mem.sort(Gap, gaps.items, {}, struct {
        fn wider(_: void, a: Gap, b: Gap) bool {
            return a.end - a.start > b.end - b.start;
        }
    }.wider);
    const cuts = gaps.items[0 .. n - 1];
    std.mem.sort(Gap, cuts, {}, struct {
        fn before(_: void, a: Gap, b: Gap) bool {
            return a.start < b.start;
        }
    }.before);
    const out = try allocator.alloc(Rect, n);
    var x1 = begin;
    for (cuts, 0..) |cut, i| {
        out[i] = .{ .x1 = x1, .y1 = 0, .x2 = cut.start, .y2 = image.height };
        x1 = cut.end;
    }
    out[n - 1] = .{ .x1 = x1, .y1 = 0, .x2 = last, .y2 = image.height };
    return out;
}

/// The headline (shirorekha) of a Devanagari word: the row with the most ink
/// in the word's box, and along it the longest run of columns whose ink is at
/// most `threshold` - the gap D058 forbids between joined letters.
pub const Headline = struct { row: usize, longest_gap: usize };

pub fn headline(image: Gray, word: Rect, threshold: u8) Headline {
    var best_row: usize = word.y1;
    var best_sum: u64 = 0;
    for (word.y1..word.y2) |y| {
        var sum: u64 = 0;
        for (word.x1..word.x2) |x| sum += image.at(x, y);
        if (sum > best_sum) {
            best_sum = sum;
            best_row = y;
        }
    }
    // The bar may sit on either side of the brightest row once the raster is
    // filtered; the longer-lit of the row and its two neighbours counts. Only
    // the stretch between the row's first and last ink is a headline: a vowel
    // sign hanging below can make the word wider than its bar.
    var gap: usize = word.width();
    var row = if (best_row > word.y1) best_row - 1 else best_row;
    while (row <= @min(best_row + 1, word.y2 - 1)) : (row += 1) {
        var first: ?usize = null;
        var last: usize = 0;
        for (word.x1..word.x2) |x| if (image.at(x, row) > threshold) {
            if (first == null) first = x;
            last = x;
        };
        var longest: usize = word.width();
        if (first) |start| {
            longest = 0;
            var run: usize = 0;
            for (start..last + 1) |x| {
                if (image.at(x, row) > threshold) {
                    run = 0;
                } else {
                    run += 1;
                    longest = @max(longest, run);
                }
            }
        }
        gap = @min(gap, longest);
    }
    return .{ .row = best_row, .longest_gap = gap };
}

/// The box of `word` from `from` to `to` of its width (fractions), the full height.
pub fn window(word: Rect, from: f64, to: f64) Rect {
    const w: f64 = @floatFromInt(word.width());
    return .{
        .x1 = word.x1 + @as(usize, @intFromFloat(@round(w * from))),
        .y1 = word.y1,
        .x2 = word.x1 + @max(1, @as(usize, @intFromFloat(@round(w * to)))),
        .y2 = word.y2,
    };
}

/// The correlation of a region of one image with a region of another; the
/// second is resampled onto the first's grid when the two differ by a pixel
/// or so (the same word is a pixel wider in one raster than in the other).
pub fn correlationIn(allocator: std.mem.Allocator, a: Gray, b: Gray, a_rect: Rect, b_rect: Rect) !f64 {
    const left = try crop(allocator, a, a_rect);
    defer allocator.free(left.pixels);
    const right = try crop(allocator, b, b_rect);
    defer allocator.free(right.pixels);
    if (left.width == right.width and left.height == right.height) return correlation(left, right);
    const fitted = try resample(allocator, right, left.width, left.height);
    defer allocator.free(fitted.pixels);
    return correlation(left, fitted);
}

/// How near a word's conjunct columns are to the shaped reference and to the
/// halant spelling. The two references share everything before the conjunct, so
/// a whole-label correlation hardly tells them apart (0.666 against 0.683 on a
/// frame that was drawn right); only the columns from where they first differ
/// to the end of the word are compared.
pub const Conjunct = struct { shaped: f64, spelled: f64 };

/// The shaped reference must win in the conjunct columns by this much. A frame
/// drawn right wins by 0.14 and a frame drawn as the halant spelling loses by
/// 0.33 (मुख्य, hb-view's Noto Sans Devanagari), so the margin sits between
/// the two with room on both sides.
pub const conjunct_margin: f64 = 0.08;

/// `frame` and both references are cut to their ink boxes, `frame_word`,
/// `shaped_word` and `spelled_word` are the same word's boxes in them. The
/// conjunct starts at the first column where the two references' words differ
/// (by more than `tolerance` summed over the rows, on the references' own
/// grid, which is the same size for both). Each spelling is a hypothesis about
/// where the frame's conjunct starts: the same share of the frame's word as of
/// that reference's word. The frame's columns from there to the word's end are
/// correlated with the reference's, resampled onto them.
pub fn conjunctMatch(allocator: std.mem.Allocator, frame: Gray, frame_word: Rect, shaped: Gray, shaped_word: Rect, spelled: Gray, spelled_word: Rect, tolerance: usize) !Conjunct {
    var prefix: usize = 0;
    const common = @min(shaped_word.width(), spelled_word.width());
    while (prefix < common) : (prefix += 1) {
        var apart: usize = 0;
        for (0..@min(shaped.height, spelled.height)) |y| {
            const a: i32 = shaped.at(shaped_word.x1 + prefix, y);
            const b: i32 = spelled.at(spelled_word.x1 + prefix, y);
            apart += @abs(a - b);
        }
        if (apart > tolerance) break;
    }
    return .{
        .shaped = try conjunctCorrelation(allocator, frame, frame_word, shaped, shaped_word, prefix),
        .spelled = try conjunctCorrelation(allocator, frame, frame_word, spelled, spelled_word, prefix),
    };
}

fn conjunctCorrelation(allocator: std.mem.Allocator, frame: Gray, frame_word: Rect, reference: Gray, reference_word: Rect, prefix: usize) !f64 {
    const share = @as(f64, @floatFromInt(prefix)) / @as(f64, @floatFromInt(reference_word.width()));
    const from = @as(usize, @intFromFloat(@round(share * @as(f64, @floatFromInt(frame_word.width())))));
    if (frame_word.x1 + from >= frame_word.x2) return error.NoConjunct;
    const mine = Rect{ .x1 = frame_word.x1 + from, .y1 = 0, .x2 = frame_word.x2, .y2 = frame.height };
    const theirs = Rect{ .x1 = reference_word.x1 + prefix, .y1 = 0, .x2 = reference_word.x2, .y2 = reference.height };
    return correlationIn(allocator, frame, reference, mine, theirs);
}

pub const HbOptions = struct {
    /// The face file.
    font: []const u8,
    /// em size in pixels; references are drawn large and scaled down.
    size: u32 = 96,
    language: []const u8,
    script: ?[]const u8 = null,
    /// A comma list in hb-view's `--features` syntax.
    features: ?[]const u8 = null,
};

/// The OpenType features that make a conjunct or a half form, off: the
/// halant spelling of a Devanagari cluster, reordering (the short i) still on.
pub const halant_spelling = "-ccmp,-akhn,-rphf,-rkrf,-pref,-blwf,-abvf,-half,-pstf,-vatu,-cjct,-pres,-abvs,-blws,-psts,-haln";

/// The text drawn by hb-view, white on black without margins, decoded. `scratch`
/// is a folder that exists; the PNG is kept in it as `name`.png.
pub fn hbView(allocator: std.mem.Allocator, io: std.Io, scratch: []const u8, name: []const u8, text: []const u8, options: HbOptions) !Gray {
    var arena = std.heap.ArenaAllocator.init(allocator);
    defer arena.deinit();
    const scoped = arena.allocator();
    const out = try std.fmt.allocPrint(scoped, "{s}/{s}.png", .{ scratch, name });
    var argv: std.ArrayList([]const u8) = .empty;
    try argv.appendSlice(scoped, &.{
        "hb-view",
        try std.fmt.allocPrint(scoped, "--font-file={s}", .{options.font}),
        try std.fmt.allocPrint(scoped, "--font-size={d}", .{options.size}),
        try std.fmt.allocPrint(scoped, "--language={s}", .{options.language}),
        "--output-format=png",
        "--foreground=ffffff",
        "--background=000000",
        "--margin=0",
        try std.fmt.allocPrint(scoped, "--output-file={s}", .{out}),
    });
    if (options.script) |script| try argv.append(scoped, try std.fmt.allocPrint(scoped, "--script={s}", .{script}));
    if (options.features) |features| try argv.append(scoped, try std.fmt.allocPrint(scoped, "--features={s}", .{features}));
    try argv.appendSlice(scoped, &.{ "--", text });
    const result = timed_run.run(allocator, io, .{
        .argv = argv.items,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(60), .clock = .awake } },
    }) catch |err| switch (err) {
        error.FileNotFound => {
            std.debug.print("hb-view is not installed (apt install libharfbuzz-bin): the tier fails without it (D069)\n", .{});
            return error.HbViewMissing;
        },
        else => return err,
    };
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);
    if (!result.term.success()) {
        std.debug.print("hb-view ended {any}: {s}{s}\n", .{ result.term, result.stdout, result.stderr });
        return error.HbViewFailed;
    }
    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, out, allocator, .limited(16 << 20));
    defer allocator.free(bytes);
    return decodePng(allocator, bytes);
}

/// A run of rows of a text box that hold ink: one line of text.
pub const Band = struct { y1: usize, y2: usize };

/// The line bands of a box of wrapped text: rows with ink above `threshold`, where fewer than
/// `min_gap` empty rows between two runs join them (a line whose glyphs leave one row empty is
/// still one line).
pub fn rowBands(allocator: std.mem.Allocator, image: Gray, threshold: u8, min_gap: usize) ![]Band {
    var list: std.ArrayList(Band) = .empty;
    var start: ?usize = null;
    var empty: usize = 0;
    for (0..image.height + 1) |y| {
        var inked = false;
        if (y < image.height) for (0..image.width) |x| {
            if (image.at(x, y) > threshold) {
                inked = true;
                break;
            }
        };
        if (inked) {
            if (start == null) start = y;
            empty = 0;
        } else if (start != null) {
            empty += 1;
            if (empty >= min_gap or y == image.height) {
                try list.append(allocator, .{ .y1 = start.?, .y2 = y + 1 - empty });
                start = null;
                empty = 0;
            }
        }
    }
    return list.toOwnedSlice(allocator);
}

/// What a frame's wrapped text holds against the lines the game's wrap predicts for it: how many
/// line bands the ink has, and how well each band matches the hb-view raster of its predicted line.
pub const LineCheck = struct {
    predicted: usize,
    measured: usize,
    /// One per band when the counts agree, empty when they do not (the bands then pair with no line).
    correlations: []f64,

    pub const Fault = enum { count_differs, weak_line };

    pub fn minCorrelation(self: LineCheck) f64 {
        var least: f64 = 1;
        for (self.correlations) |one| least = @min(least, one);
        return least;
    }

    /// The first thing wrong with the text, or null: the band count differs from the prediction
    /// (a line broke elsewhere, or one more or fewer was drawn), or a band's ink is not its
    /// predicted line's (below `floor`).
    pub fn fault(self: LineCheck, floor: f64) ?Fault {
        if (self.predicted != self.measured) return .count_differs;
        for (self.correlations) |one| if (one < floor) return .weak_line;
        return null;
    }
};

/// A text box's ink (`region`, ink above `ink_threshold`) against `references`, the hb-view
/// rasters of the lines the game's wrap predicts, in order. The ink's row bands are the lines; each
/// band's ink box is correlated with the same-index reference's (smoothed by `blur_radius` for
/// small text, 0 for none) when the counts agree.
pub fn lineBands(allocator: std.mem.Allocator, region: Gray, references: []const Gray, ink_threshold: u8, reference_threshold: u8, min_gap: usize, blur_radius: usize) !LineCheck {
    const bands = try rowBands(allocator, region, ink_threshold, min_gap);
    defer allocator.free(bands);
    var found: std.ArrayList(f64) = .empty;
    errdefer found.deinit(allocator);
    if (bands.len == references.len) for (bands, references) |band, reference| {
        const strip = try crop(allocator, region, .{ .x1 = 0, .y1 = band.y1, .x2 = region.width, .y2 = band.y2 });
        defer allocator.free(strip.pixels);
        const box = inkBox(strip, ink_threshold) orelse return error.NoInk;
        const cut = try crop(allocator, strip, box);
        defer allocator.free(cut.pixels);
        const match = if (blur_radius == 0) try matchReference(allocator, cut, reference, reference_threshold) else try matchReferenceBlurred(allocator, cut, reference, reference_threshold, blur_radius);
        try found.append(allocator, match.correlation);
    };
    return .{ .predicted = references.len, .measured = bands.len, .correlations = try found.toOwnedSlice(allocator) };
}

/// A predicted line that breaks the kinsoku rules.
pub const Kinsoku = struct {
    line: usize,
    reason: []const u8,
};

/// The first line of `lines` (UTF-16) that starts with a character in the no-start set or follows a
/// line that ends in a no-end one, or null. The sets are the game's (l10n_wrap.zig); they are
/// passed in so this file stays free of the wrap's module. Leading and trailing spaces of a line
/// do not count as its first or last character.
pub fn kinsokuViolation(lines: []const []const u16, no_start: *const fn (u16) bool, no_end: *const fn (u16) bool) ?Kinsoku {
    var previous_last: ?u16 = null;
    for (lines, 0..) |line, at| {
        const text = std.mem.trim(u16, line, &[_]u16{ ' ', '\r', '\n', 0 });
        if (text.len == 0) continue;
        if (at != 0 and no_start(text[0])) return .{ .line = at, .reason = "starts with a character that must not start a line" };
        if (previous_last) |last| if (no_end(last)) return .{ .line = at - 1, .reason = "ends with a character that must not end a line" };
        previous_last = text[text.len - 1];
    }
    return null;
}

// ----------------------------------------------------------------------------

const scratch_dir = "zig-out/local-test/l10n-measure";

fn synthetic(allocator: std.mem.Allocator, width: usize, height: usize, comptime fill: fn (usize, usize) u8) !Gray {
    const pixels = try allocator.alloc(u8, width * height);
    for (0..height) |y| for (0..width) |x| {
        pixels[y * width + x] = fill(x, y);
    };
    return .{ .width = width, .height = height, .pixels = pixels };
}

fn diagonal(x: usize, y: usize) u8 {
    return @intCast((x * 7 + y * 3) % 256);
}
fn inverse(x: usize, y: usize) u8 {
    return 255 - diagonal(x, y);
}
fn brighter(x: usize, y: usize) u8 {
    return diagonal(x, y) / 2 + 20;
}
fn noise(x: usize, y: usize) u8 {
    return @intCast((x * 131 + y * 71 + x * y * 17) % 251);
}

test {
    _ = timed_run;
}

test "correlation is 1 for the same pattern at any brightness, -1 inverted and near 0 for another" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const a = try synthetic(arena.allocator(), 40, 20, diagonal);
    try testing.expectApproxEqAbs(@as(f64, 1), correlation(a, a), 1e-9);
    try testing.expectApproxEqAbs(@as(f64, -1), correlation(a, try synthetic(arena.allocator(), 40, 20, inverse)), 1e-9);
    try testing.expect(correlation(a, try synthetic(arena.allocator(), 40, 20, brighter)) > 0.99);
    try testing.expect(@abs(correlation(a, try synthetic(arena.allocator(), 40, 20, noise))) < 0.3);
}

fn bars(x: usize, y: usize) u8 {
    _ = y;
    return if (x % 8 < 4) 200 else 0;
}

fn shiftedBars(x: usize, y: usize) u8 {
    return bars(x + 1, y);
}

test "resample keeps the mean and the pattern, crop and inkBox find the ink" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const a = try synthetic(allocator, 64, 16, bars);
    // Eight source columns (four bright, four dark) per new pixel: their mean.
    const mean = try resample(allocator, a, 8, 4);
    try testing.expectEqual(@as(u8, 100), mean.at(0, 0));
    try testing.expectEqual(@as(u8, 100), mean.at(7, 3));
    const same = try resample(allocator, a, 64, 16);
    try testing.expectEqualSlices(u8, a.pixels, same.pixels);
    var padded = try synthetic(allocator, 30, 12, zero);
    for (3..7) |y| for (10..25) |x| {
        padded.pixels[y * 30 + x] = 255;
    };
    const box = inkBox(padded, 0).?;
    try testing.expectEqual(Rect{ .x1 = 10, .y1 = 3, .x2 = 25, .y2 = 7 }, box);
    const cut = try crop(allocator, padded, box);
    try testing.expectEqual(@as(usize, 15 * 4), cut.pixels.len);
    try testing.expectEqual(@as(?Rect, null), inkBox(try synthetic(allocator, 4, 4, zero), 0));
}

fn zero(_: usize, _: usize) u8 {
    return 0;
}

test "a solid box is found and thin strokes are not; changed pixels are counted per rect" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var strokes = try synthetic(allocator, 40, 20, bars);
    // Bars of width 4 are lit 4 of every 8 columns: a 6 by 8 window is at most 4/6 lit.
    try testing.expect(!hasSolidBox(strokes, 24, 6, 8, 0.9));
    for (4..14) |y| for (10..20) |x| {
        strokes.pixels[y * 40 + x] = 255;
    };
    try testing.expect(hasSolidBox(strokes, 24, 6, 8, 0.9));
    try testing.expect(!hasSolidBox(try synthetic(allocator, 4, 4, zero), 24, 6, 8, 0.9));
    const a = try allocator.alloc(u8, 4 * 4 * 4);
    @memset(a, 10);
    const b = try allocator.dupe(u8, a);
    b[(1 * 4 + 2) * 4 + 1] = 40;
    b[(3 * 4 + 3) * 4] = 12;
    try testing.expectEqual(@as(usize, 1), changedPixels(a, b, 4, .{ .x1 = 0, .y1 = 0, .x2 = 4, .y2 = 4 }, 8));
    try testing.expectEqual(@as(usize, 2), changedPixels(a, b, 4, .{ .x1 = 0, .y1 = 0, .x2 = 4, .y2 = 4 }, 1));
    try testing.expectEqual(@as(usize, 0), changedPixels(a, b, 4, .{ .x1 = 0, .y1 = 0, .x2 = 2, .y2 = 4 }, 1));
}

test "smoothing makes a stroke pattern shifted by a pixel correlate" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const a = try synthetic(allocator, 40, 12, bars);
    const b = try synthetic(allocator, 40, 12, shiftedBars);
    const sharp = correlation(a, b);
    const soft = correlation(try blur(allocator, a, 1), try blur(allocator, b, 1));
    try testing.expect(soft > sharp);
    try testing.expectEqual(@as(u8, 0), (try blur(allocator, try synthetic(allocator, 4, 4, zero), 1)).pixels[5]);
}

test "words split at wide gaps and the headline's gap is found" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    // Two words of a headline at row 2; the first has a vowel sign hanging
    // below past the end of its bar, the second's headline has a hole of two columns.
    var image = try synthetic(allocator, 40, 12, zero);
    for (0..14) |x| image.pixels[2 * 40 + x] = 255;
    for (3..10) |y| image.pixels[y * 40 + 1] = 255;
    for (3..6) |y| for (14..17) |x| {
        image.pixels[y * 40 + x] = 255;
    };
    for (22..38) |x| {
        if (x == 29 or x == 30) continue;
        image.pixels[2 * 40 + x] = 255;
    }
    const split = try words(allocator, image, 0, 4);
    try testing.expectEqual(@as(usize, 2), split.len);
    try testing.expectEqual(@as(usize, 0), split[0].x1);
    try testing.expectEqual(@as(usize, 17), split[0].x2);
    const first = headline(image, split[0], 0);
    try testing.expectEqual(@as(usize, 2), first.row);
    try testing.expectEqual(@as(usize, 0), first.longest_gap);
    try testing.expectEqual(@as(usize, 2), headline(image, split[1], 0).longest_gap);
    try testing.expectEqual(Rect{ .x1 = 22, .y1 = 0, .x2 = 22 + 4, .y2 = 12 }, window(split[1], 0, 0.25));
}

test "decodeTga reads a top-left TGA, a bottom-left one and refuses others" {
    const allocator = testing.allocator;
    // Two pixels wide, two high: top row white, black; bottom row black, white.
    var bytes: [18 + 16]u8 = @splat(0);
    bytes[2] = 2;
    bytes[12] = 2;
    bytes[14] = 2;
    bytes[16] = 32;
    bytes[17] = 0x28;
    const white = [4]u8{ 255, 255, 255, 255 };
    const black = [4]u8{ 0, 0, 0, 255 };
    bytes[18..][0..4].* = white;
    bytes[22..][0..4].* = black;
    bytes[26..][0..4].* = black;
    bytes[30..][0..4].* = white;
    const top = try decodeTga(allocator, &bytes);
    defer allocator.free(top.pixels);
    try testing.expectEqualSlices(u8, &.{ 255, 0, 0, 255 }, top.pixels);
    bytes[17] = 0x08; // bottom-left origin: the rows are stored bottom first
    const flipped = try decodeTga(allocator, &bytes);
    defer allocator.free(flipped.pixels);
    try testing.expectEqualSlices(u8, &.{ 0, 255, 255, 0 }, flipped.pixels);
    try testing.expectError(error.TruncatedTga, decodeTga(allocator, bytes[0..30]));
    bytes[16] = 24;
    try testing.expectError(error.UnsupportedTga, decodeTga(allocator, &bytes));
    try testing.expectError(error.NotTga, decodeTga(allocator, bytes[0..10]));
}

test "decodePng reads what hb-view writes" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    try std.Io.Dir.cwd().createDirPath(testing.io, scratch_dir);
    const latin = try hbView(allocator, testing.io, scratch_dir, "latin", "Hello", .{ .font = "vendor/fonts/NotoSans-Regular.ttf", .language = "en" });
    try testing.expect(latin.width > 100 and latin.height > 50);
    const box = inkBox(latin, 64).?;
    try testing.expect(box.height() > 50 and box.height() <= latin.height);
}

test "hb-view draws the Simplified and Japanese forms of 直骨角化 differently" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    try std.Io.Dir.cwd().createDirPath(testing.io, scratch_dir);
    const text = "直骨角化";
    const sc = try hbView(allocator, testing.io, scratch_dir, "han-sc", text, .{ .font = "vendor/fonts/NotoSansSC-Regular.otf", .language = "zh-Hans" });
    const jp = try hbView(allocator, testing.io, scratch_dir, "han-jp", text, .{ .font = "vendor/fonts/NotoSansJP-Regular.otf", .language = "ja" });
    const sc_box = try crop(allocator, sc, inkBox(sc, 64).?);
    const same = try matchReference(allocator, sc_box, sc, 64);
    const other = try matchReference(allocator, sc_box, jp, 64);
    std.debug.print("Han 直骨角化: SC against itself {d:.4}, against JP {d:.4}\n", .{ same.correlation, other.correlation });
    try testing.expect(same.correlation > 0.999);
    try testing.expect(other.correlation < same.correlation - 0.02);
}

test "the halant spelling of त्र is wider than the conjunct and the matra of कि is left of its consonant" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    try std.Io.Dir.cwd().createDirPath(testing.io, scratch_dir);
    const face = "vendor/fonts/NotoSansDevanagari-Regular.ttf";
    const joined = try hbView(allocator, testing.io, scratch_dir, "hi-tra", "मित्रों", .{ .font = face, .language = "hi", .script = "Deva" });
    const apart = try hbView(allocator, testing.io, scratch_dir, "hi-tra-halant", "मित्रों", .{ .font = face, .language = "hi", .script = "Deva", .features = halant_spelling });
    const joined_box = inkBox(joined, 64).?;
    const apart_box = inkBox(apart, 64).?;
    std.debug.print("मित्रों: conjunct {d} px wide, halant spelling {d}\n", .{ joined_box.width(), apart_box.width() });
    try testing.expect(apart_box.width() > joined_box.width());
    // कि: the matra's bar is in the first third of the cluster, so that third of
    // the raster is like itself and not like the last third.
    const ki = try hbView(allocator, testing.io, scratch_dir, "hi-ki", "कि", .{ .font = face, .language = "hi", .script = "Deva" });
    const cut = try crop(allocator, ki, inkBox(ki, 64).?);
    const whole = Rect{ .x1 = 0, .y1 = 0, .x2 = cut.width, .y2 = cut.height };
    const first = window(whole, 0, 0.3);
    const last = window(whole, 0.7, 1);
    const same = try correlationIn(allocator, cut, cut, first, first);
    const swapped = try correlationIn(allocator, cut, cut, first, last);
    std.debug.print("कि: first third against itself {d:.2}, against the last third {d:.2}\n", .{ same, swapped });
    try testing.expect(same > 0.999 and swapped < 0.8);
}

test "the conjunct columns of मुख्य tell the captured shaped label from the halant spelling" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    try std.Io.Dir.cwd().createDirPath(testing.io, scratch_dir);
    const face = "vendor/fonts/NotoSansDevanagari-Regular.ttf";
    const text = "मुख्य मेनू";
    const joined = try hbView(allocator, testing.io, scratch_dir, "hi-header", text, .{ .font = face, .language = "hi", .script = "Deva" });
    const apart = try hbView(allocator, testing.io, scratch_dir, "hi-header-halant", text, .{ .font = face, .language = "hi", .script = "Deva", .features = halant_spelling });
    const joined_cut = try crop(allocator, joined, inkBox(joined, 64).?);
    const apart_cut = try crop(allocator, apart, inkBox(apart, 64).?);
    const joined_words = try words(allocator, joined_cut, 64, 3);
    const apart_words = try words(allocator, apart_cut, 64, 3);
    try testing.expectEqual(@as(usize, 2), joined_words.len);
    try testing.expectEqual(@as(usize, 2), apart_words.len);

    // The Game's main menu header, in its own ink as the tier measures it, from
    // the Linux run and from the macOS arm64 run of CI 39477983663 (whose
    // whole-label check was 0.666 for the shaped reference against 0.683).
    for ([_][]const u8{ "linux", "macos" }) |name| {
        const path = try std.fmt.allocPrint(allocator, "tools/zig/fixtures/l10n/hi-header-ink-{s}.png", .{name});
        const bytes = try std.Io.Dir.cwd().readFileAlloc(testing.io, path, allocator, .limited(1 << 20));
        const full = try decodePng(allocator, bytes);
        const ink = try crop(allocator, full, inkBox(full, 24).?);
        const mine = try words(allocator, ink, 24, 3);
        try testing.expectEqual(@as(usize, 2), mine.len);
        const whole = try matchReference(allocator, ink, joined, 64);
        const whole_apart = try matchReference(allocator, ink, apart, 64);
        const c = try conjunctMatch(allocator, ink, mine[0], joined_cut, joined_words[0], apart_cut, apart_words[0], 2 * 255);
        std.debug.print("{s}: whole label {d:.3} against {d:.3}; conjunct columns {d:.3} against {d:.3}\n", .{ name, whole.correlation, whole_apart.correlation, c.shaped, c.spelled });
        try testing.expect(c.shaped - c.spelled >= conjunct_margin);
    }

    // The halant spelling drawn as the frame (the references' own raster, on the
    // frame's height and width) loses, and the shaped reference drawn as the
    // frame wins.
    const drawn_apart = try resample(allocator, apart_cut, 102, 34);
    const lost = try conjunctMatch(allocator, drawn_apart, (try words(allocator, drawn_apart, 64, 3))[0], joined_cut, joined_words[0], apart_cut, apart_words[0], 2 * 255);
    std.debug.print("halant spelling as the frame: conjunct columns {d:.3} against {d:.3}\n", .{ lost.shaped, lost.spelled });
    try testing.expect(lost.shaped - lost.spelled < conjunct_margin);
    try testing.expect(lost.spelled > lost.shaped);
    const drawn_joined = try resample(allocator, joined_cut, 102, 34);
    const won = try conjunctMatch(allocator, drawn_joined, (try words(allocator, drawn_joined, 64, 3))[0], joined_cut, joined_words[0], apart_cut, apart_words[0], 2 * 255);
    std.debug.print("shaped reference as the frame: conjunct columns {d:.3} against {d:.3}\n", .{ won.shaped, won.spelled });
    try testing.expect(won.shaped - won.spelled >= conjunct_margin);
}

fn fineBars(x: usize, y: usize) u8 {
    _ = y;
    return if (x % 4 < 2) 200 else 0;
}

/// A 64 x 30 box with two lines of ink: `bars` on rows 2 to 10 and `fineBars` on rows 14 to 22 (plus,
/// when `extra`, a third one on rows 25 to 28).
fn twoLines(allocator: std.mem.Allocator, extra: bool) !Gray {
    var image = try synthetic(allocator, 64, 30, zero);
    for (0..64) |x| {
        for (2..10) |y| image.pixels[y * 64 + x] = bars(x, y);
        for (14..22) |y| image.pixels[y * 64 + x] = fineBars(x, y);
        if (extra) for (25..28) |y| {
            image.pixels[y * 64 + x] = bars(x, y);
        };
    }
    return image;
}

fn never(_: u16) bool {
    return false;
}
fn comma(c: u16) bool {
    return c == 0x3001 or c == 0x30FC;
}
fn bracket(c: u16) bool {
    return c == 0x300C;
}

test "two ink lines that match their predicted lines pass; a wrong or an extra line fails with a named reason" {
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const first = try synthetic(allocator, 64, 8, bars);
    const second = try synthetic(allocator, 64, 8, fineBars);
    const good = try lineBands(allocator, try twoLines(allocator, false), &.{ first, second }, 24, 64, 2, 0);
    try testing.expectEqual(@as(usize, 2), good.measured);
    try testing.expect(good.minCorrelation() > 0.95);
    try testing.expectEqual(@as(?LineCheck.Fault, null), good.fault(0.5));
    // Planted: an extra ink row where the prediction has no third line.
    const extra = try lineBands(allocator, try twoLines(allocator, true), &.{ first, second }, 24, 64, 2, 0);
    try testing.expectEqual(@as(usize, 3), extra.measured);
    try testing.expectEqual(@as(?LineCheck.Fault, .count_differs), extra.fault(0.5));
    // Planted: the lines swapped, so each band meets the other's raster.
    const swapped = try lineBands(allocator, try twoLines(allocator, false), &.{ second, first }, 24, 64, 2, 0);
    try testing.expectEqual(@as(?LineCheck.Fault, .weak_line), swapped.fault(0.5));
    // A one-row gap inside a line does not split it; a wider one does.
    var split = try twoLines(allocator, false);
    for (0..64) |x| split.pixels[5 * 64 + x] = 0;
    try testing.expectEqual(@as(usize, 2), (try rowBands(allocator, split, 24, 2)).len);
    try testing.expectEqual(@as(usize, 3), (try rowBands(allocator, split, 24, 1)).len);
}

test "a line broken before 、 or ー, or after 「, is a kinsoku violation; a clean break is not" {
    const clean = [_][]const u16{ std.unicode.utf8ToUtf16LeStringLiteral("我が軍は"), std.unicode.utf8ToUtf16LeStringLiteral("ドイツ軍から"), std.unicode.utf8ToUtf16LeStringLiteral("") };
    try testing.expectEqual(@as(?Kinsoku, null), kinsokuViolation(&clean, comma, bracket));
    const before_comma = [_][]const u16{ std.unicode.utf8ToUtf16LeStringLiteral("我が軍は"), std.unicode.utf8ToUtf16LeStringLiteral("、ドイツ軍") };
    const found = kinsokuViolation(&before_comma, comma, bracket).?;
    try testing.expectEqual(@as(usize, 1), found.line);
    try testing.expect(std.mem.indexOf(u8, found.reason, "must not start") != null);
    const before_dash = [_][]const u16{ std.unicode.utf8ToUtf16LeStringLiteral("ナルヴィク "), std.unicode.utf8ToUtf16LeStringLiteral("ーの港") };
    try testing.expectEqual(@as(usize, 1), kinsokuViolation(&before_dash, comma, bracket).?.line);
    const after_bracket = [_][]const u16{ std.unicode.utf8ToUtf16LeStringLiteral("我が軍は「"), std.unicode.utf8ToUtf16LeStringLiteral("ドイツ軍」") };
    const opened = kinsokuViolation(&after_bracket, comma, bracket).?;
    try testing.expectEqual(@as(usize, 0), opened.line);
    try testing.expect(std.mem.indexOf(u8, opened.reason, "must not end") != null);
    try testing.expectEqual(@as(?Kinsoku, null), kinsokuViolation(&before_comma, never, never));
}
