//! D-14's measured look on the real Game: a font bk-font generated is the
//! font the game draws, and it draws legibly. The staged `Tools/bk-font`
//! writes ProggyClean (the tracked fixture, D-15) at 16 pixels into
//! `mods/bkfonttest/data/fonts/medium` of the installed game, the font every
//! main-menu label but the MAIN MENU caption uses (FontSize 1 in
//! MainMenu.xml). The Game runs three times on that mod to its own
//! `-reference-scene` capture of the main menu at 1024x768:
//!
//! - control: the mod without fonts, so the shipped `fonts\medium` draws;
//! - font: the generated font;
//! - blank: the generated `1.tfd` with a fully transparent `1_h.dds`, the
//!   same layout with no glyph ink;
//! - font-ru: the generated font with bk_text's language Russian
//!   (BK_TEXT_LANG=ru). ProggyClean has no Cyrillic, so the mod's font would
//!   draw every Russian letter as an empty box: the Game must read its `1.tfd`,
//!   leave its atlas alone and draw the medium size on bk_text, as the control
//!   does (the Achtung Panzer 2 mod under lang-ru drew its menu as boxes).
//!
//! Control against font must differ, and only inside the eight text windows
//! that use the medium font: the generated font replaced the shipped one
//! there and nowhere else. Font against blank is then exactly the generated
//! font's ink (glyphs and their shadows); for the five buttons, whose texts
//! are known, it is compared with a reference drawn from the font's own
//! `1.tfd` and `1.tga` the way GfxGpuObjectFactory lays text out: every
//! glyph has ink, glyph quads do not overlap, nearly all of the reference ink
//! is drawn and no ink lies outside the reference (one pixel of slack for the
//! linear filter). That last rule caught the atlas rows bleeding into each
//! other (FontAtlas's row gap).
//!
//! The user's data is never touched: the Game and bk-font run with
//! XDG_DATA_HOME and BK_USER_ROOT in a scratch folder, the Game from that
//! folder, its profiles there too (BK_PROFILES_ROOT), in a
//! dedicated profile. The mod is removed from the installation afterwards.
//! BK_FONT_GAME names the installed Game executable and BK_FONT_TOOL the
//! staged bk-font; `zig build bk-font-game-reads-it` sets both.
const std = @import("std");
const timed_run = @import("timed_run.zig");
const l10n_measure = @import("l10n_measure.zig");
const testing = std.testing;

/// The value of an environment variable the build step sets, or null.
/// testing.environ.getPosix compiles only for POSIX targets (on Windows its
/// block has no view(), run 39086936588); the map works everywhere.
fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

const scratch = "zig-out/local-test/bk-font/game-reads-it";
const fixture = "tools/zig/fixtures/fonts/ProggyClean.ttf";
/// Lower case: CICChangeMOD lower-cases the mod's name before it opens the
/// folder.
const mod_name = "bkfonttest";
const font_height = 16;
const width = 1024;
const height = 768;
/// ShadowShift of every medium-font window in MainMenu.xml: the label is
/// drawn a second time, dark, this far right and down.
const shadow = Point{ .x = 1, .y = 2 };

const Point = struct { x: i32, y: i32 };

const Rect = struct {
    x1: usize,
    y1: usize,
    x2: usize,
    y2: usize,

    fn has(self: Rect, x: usize, y: usize) bool {
        return x >= self.x1 and x < self.x2 and y >= self.y1 and y < self.y2;
    }
};

/// The main menu's buttons on the 1024x768 canvas, as CSimpleWindow::Reposition
/// places them: their panel (element 2000) at (610,267), each button 320x50 at
/// x 200 - 160 in it and y 0, 65, 130, 195 or 260. Exclusive right and bottom.
const Button = struct { text: []const u8, rect: Rect };
const buttons = [_]Button{
    .{ .text = "New Game", .rect = .{ .x1 = 650, .y1 = 267, .x2 = 970, .y2 = 317 } },
    .{ .text = "Multiplayer", .rect = .{ .x1 = 650, .y1 = 332, .x2 = 970, .y2 = 382 } },
    .{ .text = "Load Game", .rect = .{ .x1 = 650, .y1 = 397, .x2 = 970, .y2 = 447 } },
    .{ .text = "Options", .rect = .{ .x1 = 650, .y1 = 462, .x2 = 970, .y2 = 512 } },
    .{ .text = "Exit Game", .rect = .{ .x1 = 650, .y1 = 527, .x2 = 970, .y2 = 577 } },
};
/// The other medium-font windows, placed from the bottom (PositionFlag 0x31
/// and 0x33): the profile name (21000, 500x50 at 10,88), the version (666,
/// 1004x50 at 10,54) and the mod's name and version (667, 1004x50 at 10,10).
const labels = [_]Rect{
    .{ .x1 = 10, .y1 = 630, .x2 = 510, .y2 = 680 },
    .{ .x1 = 10, .y1 = 664, .x2 = 1014, .y2 = 714 },
    .{ .x1 = 10, .y1 = 708, .x2 = 1014, .y2 = 758 },
};

test "the Game draws a font bk-font generated, legibly (D-14)" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

    const game = (try envValue(allocator, "BK_FONT_GAME")) orelse {
        std.debug.print("BK_FONT_GAME is not set: run zig build bk-font-game-reads-it\n", .{});
        return error.MissingGame;
    };
    const tool = (try envValue(allocator, "BK_FONT_TOOL")) orelse {
        std.debug.print("BK_FONT_TOOL is not set: run zig build bk-font-game-reads-it\n", .{});
        return error.MissingTool;
    };
    const game_root = std.fs.path.dirname(game).?;
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ try std.process.currentPathAlloc(io, allocator), scratch });
    cwd.deleteTree(io, scratch) catch {};
    const user = try std.fs.path.join(allocator, &.{ root, "user" });
    try cwd.createDirPath(io, user);

    const mod_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", mod_name });
    cwd.deleteTree(io, mod_folder) catch {};
    defer cwd.deleteTree(io, mod_folder) catch {};
    const mod_data = try std.fs.path.join(allocator, &.{ mod_folder, "data" });
    try cwd.createDirPath(io, mod_data);
    try cwd.writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ mod_data, "mod.xml" }),
        .data = "<?xml version=\"1.0\"?>\r\n<base>\r\n\t<MODName>bk-font test</MODName>\r\n\t<MODVersion>1.0</MODVersion>\r\n</base>\r\n",
    });

    const control = try runGame(io, allocator, game, user, root, "control", .control);

    const font_folder = try std.fs.path.join(allocator, &.{ mod_data, "fonts", "medium" });
    try runTool(io, allocator, tool, user, font_folder);
    const tfd = try cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ font_folder, "1.tfd" }), allocator, .limited(4 << 20));
    const tga = try cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ font_folder, "1.tga" }), allocator, .limited(64 << 20));
    const font = try Font.read(allocator, tfd, tga);
    try testing.expectEqual(@as(i32, font_height), font.height);
    const drawn = try runGame(io, allocator, game, user, root, "font", .mod_font);
    const russian = try runGame(io, allocator, game, user, root, "font-ru", .mod_font_lacks_language);

    // The same description over a texture with no ink.
    const dds_path = try std.fs.path.join(allocator, &.{ font_folder, "1_h.dds" });
    const dds = try cwd.readFileAlloc(io, dds_path, allocator, .limited(64 << 20));
    try testing.expect(std.mem.eql(u8, dds[0..4], "DDS "));
    try testing.expectEqual(128 + font.atlas_width * font.atlas_height * 4, dds.len);
    @memset(dds[128..], 0);
    try cwd.writeFile(io, .{ .sub_path = dds_path, .data = dds });
    const blank = try runGame(io, allocator, game, user, root, "blank", .mod_font);

    for ([_][]const u8{ control, drawn, russian, blank }) |frame| try testing.expectEqual(@as(usize, width * height * 4), frame.len);

    // Russian over the mod's font: the five buttons draw as the control does, on bk_text. Their
    // English labels are in the face bk_text gives Russian too (Liberation Serif leads for Latin
    // and Cyrillic), so a pixel that differs is the mod's bitmap font drawn after all.
    for (buttons) |button| {
        var differs_from_control: usize = 0;
        var differs_from_font: usize = 0;
        for (button.rect.y1..button.rect.y2) |y| for (button.rect.x1..button.rect.x2) |x| {
            const at = (y * width + x) * 4;
            if (maxChannelDelta(control[at..][0..3], russian[at..][0..3]) > 2) differs_from_control += 1;
            if (maxChannelDelta(drawn[at..][0..3], russian[at..][0..3]) > 2) differs_from_font += 1;
        };
        errdefer std.debug.print("D-14: \"{s}\" under Russian differs from the control in {d} pixels and from the mod's font in {d}\n", .{ button.text, differs_from_control, differs_from_font });
        try testing.expectEqual(@as(usize, 0), differs_from_control);
        try testing.expect(differs_from_font >= 50);
    }

    // Control against font: changed pixels only in the medium-font windows,
    // and in every one of them.
    var changed: usize = 0;
    var outside: usize = 0;
    var per_window: [buttons.len + labels.len]usize = @splat(0);
    for (0..height) |y| for (0..width) |x| {
        const at = (y * width + x) * 4;
        if (std.mem.eql(u8, control[at..][0..3], drawn[at..][0..3])) continue;
        changed += 1;
        var inside = false;
        for (buttons, 0..) |button, i| if (button.rect.has(x, y)) {
            per_window[i] += 1;
            inside = true;
        };
        for (labels, 0..) |label, i| if (label.has(x, y)) {
            per_window[buttons.len + i] += 1;
            inside = true;
        };
        // A difference of two levels of 255 or less is the linear filter's rounding next to a window
        // (bk_text's control run puts six such pixels beside the mod-name label), not text.
        if (!inside and maxChannelDelta(control[at..][0..3], drawn[at..][0..3]) > 2) outside += 1;
    };
    {
        errdefer std.debug.print("D-14: control and generated font differ in {d} pixels, {d} outside the eight medium-font windows; per window {any}\n", .{ changed, outside, per_window });
        try testing.expectEqual(@as(usize, 0), outside);
        for (per_window) |count| try testing.expect(count >= 50);
    }

    // Font against blank, per button: the generated font's ink against the
    // reference.
    for (buttons) |button| {
        const result = try measure(allocator, font, button, drawn, blank);
        errdefer std.debug.print("D-14: \"{s}\": {d} glyphs, all with ink: {any}, quads overlap: {any}; {d} ink pixels, reference {d}, drawn {d} ({d:.1}%), outside the reference {d}, origin ({d},{d})\n", .{
            button.text,    result.glyphs,    result.glyphs_with_ink == result.glyphs, result.overlap,
            result.ink,     result.reference, result.hit,                              100.0 * @as(f64, @floatFromInt(result.hit)) / @as(f64, @floatFromInt(result.reference)),
            result.foreign, result.origin.x,  result.origin.y,
        });
        try testing.expectEqual(result.glyphs, result.glyphs_with_ink);
        try testing.expect(!result.overlap);
        try testing.expectEqual(@as(usize, 0), result.foreign);
        try testing.expect(result.hit * 10 >= result.reference * 9);
    }
}

/// What bk-font wrote, as the game reads it: the metrics, each character's
/// texture rect and A/B/C spacing from `1.tfd`, and the atlas coverage from
/// `1.tga`.
const Font = struct {
    height: i32,
    chars: std.AutoHashMapUnmanaged(u16, Char),
    atlas_width: usize,
    atlas_height: usize,
    /// Alpha, top row first.
    coverage: []const u8,

    const Char = struct { x1: f32, y1: f32, x2: f32, y2: f32, a: f32, b: f32, c: f32, width: f32 };

    /// `1.tfd` is the engine's structure saver format (StructureSaver.cpp):
    /// chunks of a one-byte id and a length, doubled, in one byte or, when
    /// odd, four. The file's chunk 1 holds the object saved as chunk 1,
    /// SFontFormat: chunk 2 the metrics (1 nHeight), chunk 3 the characters,
    /// the n-th key chunk 1 paired with the n-th value chunk 2.
    fn read(allocator: std.mem.Allocator, tfd: []const u8, tga: []const u8) !Font {
        const format = try child(try child(tfd, 1, 0), 1, 0);
        const metrics = try child(format, 2, 0);
        const chars_chunk = try child(format, 3, 0);
        var font: Font = .{ .height = try int(i32, try child(metrics, 1, 0)), .chars = .empty, .atlas_width = 0, .atlas_height = 0, .coverage = &.{} };
        var index: usize = 0;
        while (child(chars_chunk, 1, index)) |key| : (index += 1) {
            const desc = try child(chars_chunk, 2, index);
            var fields: [8]f32 = undefined;
            for (&fields, 1..) |*field, id| field.* = @bitCast(try int(u32, try child(desc, @intCast(id), 0)));
            try font.chars.put(allocator, try int(u16, key), .{ .x1 = fields[0], .y1 = fields[1], .x2 = fields[2], .y2 = fields[3], .a = fields[4], .b = fields[5], .c = fields[6], .width = fields[7] });
        } else |err| if (err != error.ChunkMissing) return err;
        if (font.chars.count() == 0) return error.NoCharacters;

        // An uncompressed 32-bit TGA, as bk-font writes it.
        if (tga.len < 18 or tga[2] != 2 or tga[16] != 32) return error.UnexpectedTga;
        font.atlas_width = std.mem.readInt(u16, tga[12..14], .little);
        font.atlas_height = std.mem.readInt(u16, tga[14..16], .little);
        const pixels = tga[18 + @as(usize, tga[0]) ..];
        if (pixels.len < font.atlas_width * font.atlas_height * 4) return error.UnexpectedTga;
        const top_first = tga[17] & 0x20 != 0;
        const coverage = try allocator.alloc(u8, font.atlas_width * font.atlas_height);
        for (0..font.atlas_height) |y| {
            const row = if (top_first) y else font.atlas_height - 1 - y;
            for (0..font.atlas_width) |x| coverage[y * font.atlas_width + x] = pixels[(row * font.atlas_width + x) * 4 + 3];
        }
        font.coverage = coverage;
        return font;
    }

    fn get(self: Font, c: u8) !Char {
        return self.chars.get(c) orelse error.CharacterMissing;
    }
};

/// The n-th chunk `id` directly inside `data`.
fn child(data: []const u8, id: u8, n: usize) ![]const u8 {
    var at: usize = 0;
    var seen: usize = 0;
    while (at + 2 <= data.len) {
        const chunk_id = data[at];
        var length: usize = data[at + 1];
        at += 2;
        if (length & 1 != 0) {
            if (at + 3 > data.len) return error.BadChunk;
            length |= @as(usize, std.mem.readInt(u24, data[at..][0..3], .little)) << 8;
            at += 3;
        }
        length >>= 1;
        if (at + length > data.len) return error.BadChunk;
        if (chunk_id == id) {
            if (seen == n) return data[at .. at + length];
            seen += 1;
        }
        at += length;
    }
    return error.ChunkMissing;
}

fn int(comptime T: type, data: []const u8) !T {
    if (data.len != @sizeOf(T)) return error.BadChunk;
    return std.mem.readInt(T, data[0..@sizeOf(T)], .little);
}

const Measurement = struct {
    glyphs: usize = 0,
    glyphs_with_ink: usize = 0,
    overlap: bool = false,
    ink: usize = 0,
    reference: usize = 0,
    hit: usize = 0,
    foreign: usize = 0,
    origin: Point = .{ .x = 0, .y = 0 },
};

/// One button's text drawn by the generated font against the reference. The
/// ink is every pixel the font and blank frames disagree on inside the button.
/// The reference is the text laid out as GfxGpuObjectFactory::AppendGeometry
/// does (pen += A, quad of fWidth from the cell's texels, pen += B + C), each
/// inked texel of a cell becoming a pixel, plus the same shifted by the
/// shadow. Where the button puts the text's origin is found, not assumed: the
/// origin, within two pixels of the one that lines the boxes up, with the
/// least ink outside the reference. Outside means farther than one pixel from
/// every reference pixel, the slack of the linear filter.
fn measure(allocator: std.mem.Allocator, font: Font, button: Button, drawn: []const u8, blank: []const u8) !Measurement {
    var ink: std.AutoHashMapUnmanaged(Point, void) = .empty;
    var ink_box = Rect{ .x1 = width, .y1 = height, .x2 = 0, .y2 = 0 };
    for (button.rect.y1..button.rect.y2) |y| for (button.rect.x1..button.rect.x2) |x| {
        const at = (y * width + x) * 4;
        if (std.mem.eql(u8, drawn[at..][0..3], blank[at..][0..3])) continue;
        try ink.put(allocator, .{ .x = @intCast(x), .y = @intCast(y) }, {});
        ink_box = .{ .x1 = @min(ink_box.x1, x), .y1 = @min(ink_box.y1, y), .x2 = @max(ink_box.x2, x + 1), .y2 = @max(ink_box.y2, y + 1) };
    };
    if (ink.count() == 0) return error.NoInk;

    // The reference at origin (0,0): glyph pixels with the glyph each belongs to.
    const Pixel = struct { at: Point, glyph: usize };
    var glyph_pixels: std.ArrayList(Pixel) = .empty;
    var result = Measurement{};
    var pen: f32 = 0;
    var previous_right: i32 = std.math.minInt(i32);
    for (button.text) |c| {
        const char = try font.get(c);
        pen += char.a;
        const left: i32 = @intFromFloat(@floor(pen));
        const cell_x: usize = @intFromFloat(@round(char.x1 * @as(f32, @floatFromInt(font.atlas_width)) - 0.5));
        const cell_y: usize = @intFromFloat(@round(char.y1 * @as(f32, @floatFromInt(font.atlas_height)) - 0.5));
        const cell_width: usize = @intFromFloat(char.width);
        if (c != ' ') {
            result.glyphs += 1;
            if (left < previous_right) result.overlap = true;
            previous_right = left + @as(i32, @intCast(cell_width));
            for (0..@intCast(font.height)) |y| for (0..cell_width) |x| {
                if (font.coverage[(cell_y + y) * font.atlas_width + cell_x + x] == 0) continue;
                try glyph_pixels.append(allocator, .{ .at = .{ .x = left + @as(i32, @intCast(x)), .y = @intCast(y) }, .glyph = result.glyphs - 1 });
            };
        }
        pen += char.b + char.c;
    }
    var box = Rect{ .x1 = std.math.maxInt(usize), .y1 = std.math.maxInt(usize), .x2 = 0, .y2 = 0 };
    for (glyph_pixels.items) |pixel| {
        box.x1 = @min(box.x1, @as(usize, @intCast(pixel.at.x)));
        box.y1 = @min(box.y1, @as(usize, @intCast(pixel.at.y)));
    }

    var best: ?Measurement = null;
    var dy: i32 = -2;
    while (dy <= 2) : (dy += 1) {
        var dx: i32 = -2;
        while (dx <= 2) : (dx += 1) {
            const origin = Point{ .x = @as(i32, @intCast(ink_box.x1)) - @as(i32, @intCast(box.x1)) + dx, .y = @as(i32, @intCast(ink_box.y1)) - @as(i32, @intCast(box.y1)) + dy };
            var near: std.AutoHashMapUnmanaged(Point, void) = .empty;
            var try_result = Measurement{ .glyphs = result.glyphs, .overlap = result.overlap, .ink = ink.count(), .reference = glyph_pixels.items.len, .origin = origin };
            const has_ink = try allocator.alloc(bool, result.glyphs);
            @memset(has_ink, false);
            for (glyph_pixels.items) |pixel| {
                const at = Point{ .x = origin.x + pixel.at.x, .y = origin.y + pixel.at.y };
                if (ink.contains(at)) {
                    try_result.hit += 1;
                    has_ink[pixel.glyph] = true;
                }
                for ([_]Point{ at, .{ .x = at.x + shadow.x, .y = at.y + shadow.y } }) |drawn_at| {
                    var ny: i32 = -1;
                    while (ny <= 1) : (ny += 1) {
                        var nx: i32 = -1;
                        while (nx <= 1) : (nx += 1) try near.put(allocator, .{ .x = drawn_at.x + nx, .y = drawn_at.y + ny }, {});
                    }
                }
            }
            for (has_ink) |any| try_result.glyphs_with_ink += @intFromBool(any);
            var keys = ink.keyIterator();
            while (keys.next()) |at| try_result.foreign += @intFromBool(!near.contains(at.*));
            if (best == null or try_result.foreign < best.?.foreign or (try_result.foreign == best.?.foreign and try_result.hit > best.?.hit)) best = try_result;
        }
    }
    return best.?;
}

/// Writes the fixture font into `out` with the staged bk-font, the modder's
/// command; it must exit 0.
fn runTool(io: std.Io, allocator: std.mem.Allocator, tool: []const u8, user: []const u8, out: []const u8) !void {
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MEM_REPORT", "count");
    // bk-font's messages are English whatever the machine's language is.
    try environ.put("BK_LANG", "en");
    try environ.put("BK_OS_LOCALES", "");
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ tool, "--ttf", fixture, "--height", std.fmt.comptimePrint("{d}", .{font_height}), "--out", out, "--force" },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(60), .clock = .awake } },
    });
    if (!result.term.success()) {
        std.debug.print("bk-font ended {any}:\n{s}{s}\n", .{ result.term, result.stdout, result.stderr });
        return error.ToolFailed;
    }
}

fn maxChannelDelta(a: []const u8, b: []const u8) u8 {
    var most: u8 = 0;
    for (a, b) |x, y| most = @max(most, if (x > y) x - y else y - x);
    return most;
}

/// Runs the Game on the mod to its main-menu capture `<root>/<name>.rgba` and
/// returns the frame (RGBA, alpha zero). It must exit 0, report the capture,
/// lay the main menu out at scale 1 and open the medium font from the mod as
/// `run` says. Its output is kept beside the capture as `<name>.log`.
fn runGame(io: std.Io, allocator: std.mem.Allocator, game: []const u8, user: []const u8, root: []const u8, name: []const u8, run: Run) ![]u8 {
    const capture = try std.fmt.allocPrint(allocator, "{s}/{s}.rgba", .{ root, name });
    const log_path = try std.fmt.allocPrint(allocator, "{s}/{s}.log", .{ root, name });
    var environ = try testing.environ.createMap(allocator);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);
    try environ.put("BK_PROFILES_ROOT", try std.fs.path.join(allocator, &.{ user, "profiles" }));
    try environ.put("BK_MEM_REPORT", "count");
    try environ.put("BK_DEBUG_LOG", "1");
    try environ.put("BK_MOD_TRACE", "1");
    try environ.put("BK_UI_TRACE", "1");
    // The Game's text engine is bk_text (M004 S05): the control draws every size on it, and the
    // mod's own medium font must still draw with its bitmap while the other sizes stay on bk_text.
    try environ.put("BK_TEXT_ENGINE", "ttf");
    // bk_text's language, which the mod's font is checked against; the menu's texts stay English.
    if (run == .mod_font_lacks_language) try environ.put("BK_TEXT_LANG", "ru") else _ = environ.swapRemove("BK_TEXT_LANG");
    const result = try timed_run.run(allocator, io, .{
        .argv = &.{ game, "-reference-scene", capture, "-reference-resolution", "1024", "768", "-windowed", "-mod=" ++ mod_name, "-profile=BkFontTest" },
        .cwd = .{ .path = user },
        .environ_map = &environ,
        .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
    });
    const log = try std.mem.concat(allocator, u8, &.{ result.stdout, result.stderr });
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = log_path, .data = log });
    if (!result.term.success()) {
        std.debug.print("Game ({s}) ended {any}; its output is in {s}\n", .{ name, result.term, log_path });
        return error.GameFailed;
    }
    try l10n_measure.expectNoLeak(log, log_path);
    try expectLine(log, log_path, "BK_REFERENCE_SCENE: capture complete");
    try expectLine(log, log_path, "BK_UI_TRACE: reposition \"ui\\mainmenu\" screen 1024x768 -> parent 1024x768 at (0,0) scale 1.000,1.000");
    // The font's description is read from the mod whenever the mod ships one, its atlas only when
    // the font draws.
    for ([_][]const u8{ "fonts/medium/1.tfd", "fonts/medium/1_h.dds" }, [_]bool{ run != .control, run == .mod_font }) |file, from_mod| {
        if (opensFromMod(log, file) == from_mod) continue;
        std.debug.print("Game ({s}) {s} {s} from the mod; its output is in {s}\n", .{ name, if (from_mod) "did not open" else "opened", file, log_path });
        return error.FontSource;
    }
    // LogFontMetrics names the path each size took: the mod's medium is a bitmap font (reason "mod
    // font"), every other size that loaded is a bk_text font; without the mod's font medium is bk_text too.
    for ([_][]const u8{ "tiny", "small", "medium", "large" }) |size| {
        const bitmap = run == .mod_font and std.mem.eql(u8, size, "medium");
        const line_bitmap = try std.fmt.allocPrint(allocator, "font bitmap fonts\\{s}:", .{size});
        const line_ttf = try std.fmt.allocPrint(allocator, "font ttf fonts\\{s}:", .{size});
        // The Game opens every font before the mod is added and again when it is (the font manager
        // reloads), so the last line of a size is the path it draws with.
        const last_bitmap = std.mem.lastIndexOf(u8, log, line_bitmap);
        const last_ttf = std.mem.lastIndexOf(u8, log, line_ttf);
        const is_bitmap = last_bitmap != null and (last_ttf == null or last_bitmap.? > last_ttf.?);
        const is_ttf = last_ttf != null and !is_bitmap;
        const loaded = is_bitmap or is_ttf;
        if (std.mem.eql(u8, size, "medium") and !loaded or loaded and (is_bitmap != bitmap or is_ttf == bitmap)) {
            std.debug.print("Game ({s}) drew fonts\\{s} on {s} (bitmap {any}, bk_text {any}), expected {s}; its output is in {s}\n", .{ name, size, if (is_bitmap) "bitmap" else "bk_text", is_bitmap, is_ttf, if (bitmap) "bitmap" else "bk_text", log_path });
            return error.FontEngine;
        }
    }
    switch (run) {
        .control => {},
        .mod_font => try expectLine(log, log_path, "reason mod font"),
        .mod_font_lacks_language => try expectLine(log, log_path, "reason mod font lacks the language's letters"),
    }
    return std.Io.Dir.cwd().readFileAlloc(io, capture, allocator, .limited(16 << 20));
}

/// What a run of the Game draws the medium size with: the shipped size on bk_text (the mod has no
/// font), the mod's bitmap font, or bk_text again because the mod's font lacks the letters of
/// bk_text's language.
const Run = enum { control, mod_font, mod_font_lacks_language };

fn expectLine(log: []const u8, log_path: []const u8, line: []const u8) !void {
    if (std.mem.indexOf(u8, log, line) != null) return;
    std.debug.print("the Game did not print \"{s}\"; its output is in {s}\n", .{ line, log_path });
    return error.GameLineMissing;
}

/// Whether a BK_MOD_TRACE line says the MOD overlay answered `name`, the
/// game's spelling (backslashes, any case) compared without regard to either.
fn opensFromMod(log: []const u8, name: []const u8) bool {
    const prefix = "BK_MOD_TRACE: open \"";
    var lines = std.mem.splitScalar(u8, log, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trimEnd(u8, raw, "\r");
        if (!std.mem.startsWith(u8, line, prefix) or !std.mem.endsWith(u8, line, "\" from MOD")) continue;
        const opened = line[prefix.len .. line.len - "\" from MOD".len];
        if (opened.len != name.len) continue;
        for (opened, name) |a, b| {
            const left = if (a == '\\') '/' else std.ascii.toLower(a);
            if (left != std.ascii.toLower(b)) break;
        } else return true;
    }
    return false;
}

test "a structure-saver chunk is found by id and number, with either length form" {
    // id 1 (2 bytes), id 2 (3 bytes), id 1 (300 bytes, the four-byte length).
    var data: [2 + 2 + 2 + 3 + 5 + 300]u8 = undefined;
    data[0..4].* = .{ 1, 4, 0xaa, 0xbb };
    data[4..9].* = .{ 2, 6, 1, 2, 3 };
    data[9] = 1;
    std.mem.writeInt(u32, data[10..14], 300 << 1 | 1, .little);
    @memset(data[14..], 7);
    try testing.expectEqualSlices(u8, &.{ 0xaa, 0xbb }, try child(&data, 1, 0));
    try testing.expectEqualSlices(u8, &.{ 1, 2, 3 }, try child(&data, 2, 0));
    try testing.expectEqual(@as(usize, 300), (try child(&data, 1, 1)).len);
    try testing.expectError(error.ChunkMissing, child(&data, 1, 2));
    try testing.expectError(error.ChunkMissing, child(&data, 3, 0));
}
