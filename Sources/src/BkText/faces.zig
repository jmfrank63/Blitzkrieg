//! The bundled faces (vendor/fonts) and the per-language fallback chain.
//! Faces are read whole, checked against SHA256SUMS and kept alive for the
//! HarfBuzz font and the stb_truetype fontinfo that point into the bytes.
const std = @import("std");
const c = @import("c.zig");

pub const Face = enum {
    liberation_serif,
    noto_sans,
    noto_sans_sc,
    noto_sans_jp,
    noto_sans_devanagari,

    pub fn fileName(face: Face) []const u8 {
        return switch (face) {
            .liberation_serif => "LiberationSerif-Regular.ttf",
            .noto_sans => "NotoSans-Regular.ttf",
            .noto_sans_sc => "NotoSansSC-Regular.otf",
            .noto_sans_jp => "NotoSansJP-Regular.otf",
            .noto_sans_devanagari => "NotoSansDevanagari-Regular.ttf",
        };
    }
};

pub const face_count = std.enums.values(Face).len;

pub const Error = error{ FontMissing, FontRead, FontMismatch, FontInvalid, OutOfMemory };

/// The faces in fallback order for a language (BCP-47 primary code such as
/// `en`, `ru`, `ja`, `zh`, `hi`). The first face with a glyph wins. Liberation
/// Serif leads for Latin and Cyrillic, Noto Sans follows, and Han characters
/// differ between the two CJK faces, so Japanese puts its own face before the
/// Chinese one and every other language the other way round (the order of
/// Sources/editor/kit/fonts.zig mergeOrder, which cannot be imported here: the
/// editors link ImGui). Devanagari is last for every language including `hi`:
/// no other face has those code points, so its place only matters for Latin
/// digits and punctuation, which Hindi text takes from the faces before it.
pub fn chainFor(lang_code: []const u8) [face_count]Face {
    if (std.mem.eql(u8, lang_code, "ja")) return .{ .liberation_serif, .noto_sans, .noto_sans_jp, .noto_sans_sc, .noto_sans_devanagari };
    return .{ .liberation_serif, .noto_sans, .noto_sans_sc, .noto_sans_jp, .noto_sans_devanagari };
}

pub const Loaded = struct {
    bytes: []u8,
    blob: *c.hb.hb_blob_t,
    hb_face: *c.hb.hb_face_t,
    hb_font: *c.hb.hb_font_t,
    /// stb_truetype's fontinfo, opaque; size reported by the C side.
    stb: []align(16) u8,
};

pub const FaceSet = struct {
    allocator: std.mem.Allocator,
    faces: [face_count]Loaded,
    loaded: usize = 0,

    /// Reads and verifies every face in `dir`. A failure names the file and
    /// whether it was missing, unreadable, or a SHA256 mismatch, on stderr.
    pub fn load(allocator: std.mem.Allocator, io: std.Io, dir: []const u8) Error!FaceSet {
        return loadReporting(allocator, io, dir, null);
    }

    /// load, with the failure's line (no trailing newline) written to
    /// `diagnostics` instead of stderr when it is set: a caller that turns the
    /// line into its own message, or a test that fails a load on purpose and
    /// asserts what it says.
    pub fn loadReporting(allocator: std.mem.Allocator, io: std.Io, dir: []const u8, diagnostics: ?*std.Io.Writer) Error!FaceSet {
        var set: FaceSet = .{ .allocator = allocator, .faces = undefined };
        errdefer set.deinit();

        const sums_path = try std.fs.path.join(allocator, &.{ dir, "SHA256SUMS" });
        defer allocator.free(sums_path);
        const sums = std.Io.Dir.cwd().readFileAlloc(io, sums_path, allocator, .limited(1 << 16)) catch |err| {
            report(diagnostics, "bk_text: {s}: {s}", .{ sums_path, if (err == error.FileNotFound) "missing" else "cannot be read" });
            return if (err == error.FileNotFound) error.FontMissing else error.FontRead;
        };
        defer allocator.free(sums);

        for (std.enums.values(Face)) |face| {
            const path = try std.fs.path.join(allocator, &.{ dir, face.fileName() });
            defer allocator.free(path);
            const bytes = std.Io.Dir.cwd().readFileAlloc(io, path, allocator, .limited(64 << 20)) catch |err| {
                if (err == error.FileNotFound) {
                    report(diagnostics, "bk_text: font file {s} is missing", .{path});
                    return error.FontMissing;
                }
                report(diagnostics, "bk_text: font file {s} cannot be read: {s}", .{ path, @errorName(err) });
                return error.FontRead;
            };
            errdefer allocator.free(bytes);
            if (!verify(sums, face.fileName(), bytes)) {
                report(diagnostics, "bk_text: font file {s} fails its SHA256SUMS check", .{path});
                return error.FontMismatch;
            }
            const slot = &set.faces[set.loaded];
            slot.* = try open(allocator, bytes, path, diagnostics);
            set.loaded += 1;
        }
        return set;
    }

    pub fn deinit(self: *FaceSet) void {
        for (self.faces[0..self.loaded]) |f| {
            c.hb.hb_font_destroy(f.hb_font);
            c.hb.hb_face_destroy(f.hb_face);
            c.hb.hb_blob_destroy(f.blob);
            self.allocator.free(f.stb);
            self.allocator.free(f.bytes);
        }
        self.loaded = 0;
    }

    pub fn get(self: *const FaceSet, face: Face) *const Loaded {
        return &self.faces[@intFromEnum(face)];
    }

    /// Whether `face` has a glyph for the code point.
    pub fn covers(self: *const FaceSet, face: Face, cp: u21) bool {
        var glyph: c.hb.hb_codepoint_t = 0;
        return c.hb.hb_font_get_nominal_glyph(self.get(face).hb_font, cp, &glyph) != 0;
    }
};

/// One line of a failed load: to `diagnostics` when the caller gave one, else stderr.
fn report(diagnostics: ?*std.Io.Writer, comptime format: []const u8, args: anytype) void {
    if (diagnostics) |writer| {
        writer.print(format, args) catch {};
        return;
    }
    std.debug.print(format ++ "\n", args);
}

fn open(allocator: std.mem.Allocator, bytes: []u8, path: []const u8, diagnostics: ?*std.Io.Writer) Error!Loaded {
    // The blob is read-only and does not own the bytes: FaceSet frees them.
    const blob = c.hb.hb_blob_create(bytes.ptr, @intCast(bytes.len), c.hb.HB_MEMORY_MODE_READONLY, null, null) orelse return error.OutOfMemory;
    errdefer c.hb.hb_blob_destroy(blob);
    const hb_face = c.hb.hb_face_create(blob, 0) orelse return error.OutOfMemory;
    errdefer c.hb.hb_face_destroy(hb_face);
    const hb_font = c.hb.hb_font_create(hb_face) orelse return error.OutOfMemory;
    errdefer c.hb.hb_font_destroy(hb_font);
    const stb = try allocator.alignedAlloc(u8, .@"16", c.bk_stbtt_info_size());
    errdefer allocator.free(stb);
    if (c.bk_stbtt_init_font(stb.ptr, bytes.ptr, 0) == 0) {
        report(diagnostics, "bk_text: font file {s} is not a usable TrueType or CFF font", .{path});
        return error.FontInvalid;
    }
    return .{ .bytes = bytes, .blob = blob, .hb_face = hb_face, .hb_font = hb_font, .stb = stb };
}

/// True when the `<hex>  <name>` line for `name` in `sums` matches the bytes.
fn verify(sums: []const u8, name: []const u8, bytes: []const u8) bool {
    var digest: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(bytes, &digest, .{});
    const hex = std.fmt.bytesToHex(digest, .lower);
    var lines = std.mem.tokenizeAny(u8, sums, "\r\n");
    while (lines.next()) |line| {
        var parts = std.mem.tokenizeScalar(u8, line, ' ');
        const sum = parts.next() orelse continue;
        const file = parts.next() orelse continue;
        if (std.mem.eql(u8, file, name)) return std.mem.eql(u8, sum, &hex);
    }
    return false;
}

/// The fonts directory the tests read; build.zig passes an absolute path.
pub fn testFontsDir() []const u8 {
    return std.mem.span(c.getenv("BK_TEXT_FONTS_DIR") orelse @panic("BK_TEXT_FONTS_DIR is not set"));
}

test "chain order per language" {
    const F = Face;
    const latin = [_]F{ .liberation_serif, .noto_sans, .noto_sans_sc, .noto_sans_jp, .noto_sans_devanagari };
    const ja = [_]F{ .liberation_serif, .noto_sans, .noto_sans_jp, .noto_sans_sc, .noto_sans_devanagari };
    inline for (.{ "en", "ru", "zh", "hi" }) |code| try std.testing.expectEqualSlices(F, &latin, &chainFor(code));
    try std.testing.expectEqualSlices(F, &ja, &chainFor("ja"));
}

test "every face loads, verifies and covers its script" {
    var set = try FaceSet.load(std.testing.allocator, std.testing.io, testFontsDir());
    defer set.deinit();
    try std.testing.expect(set.covers(.liberation_serif, 'A'));
    try std.testing.expect(set.covers(.liberation_serif, 0x0416));
    try std.testing.expect(set.covers(.noto_sans, 0x0159));
    try std.testing.expect(set.covers(.noto_sans_sc, 0x4E2D));
    try std.testing.expect(set.covers(.noto_sans_jp, 0x3042));
    try std.testing.expect(set.covers(.noto_sans_devanagari, 0x0939));
    try std.testing.expect(!set.covers(.liberation_serif, 0x0939));
}

test "a flipped byte is a SHA256 mismatch" {
    const gpa = std.testing.allocator;
    const io = std.testing.io;
    const dst = "zig-out/local-test/bk-text/mismatch";
    const cwd = std.Io.Dir.cwd();
    try cwd.createDirPath(io, dst);
    for (std.enums.values(Face)) |face| {
        const from = try std.fs.path.join(gpa, &.{ testFontsDir(), face.fileName() });
        defer gpa.free(from);
        const bytes = try cwd.readFileAlloc(io, from, gpa, .limited(64 << 20));
        defer gpa.free(bytes);
        if (face == .noto_sans) bytes[bytes.len / 2] ^= 0x01;
        const to = try std.fs.path.join(gpa, &.{ dst, face.fileName() });
        defer gpa.free(to);
        try cwd.writeFile(io, .{ .sub_path = to, .data = bytes });
    }
    const sums_from = try std.fs.path.join(gpa, &.{ testFontsDir(), "SHA256SUMS" });
    defer gpa.free(sums_from);
    const sums = try cwd.readFileAlloc(io, sums_from, gpa, .limited(1 << 16));
    defer gpa.free(sums);
    try cwd.writeFile(io, .{ .sub_path = dst ++ "/SHA256SUMS", .data = sums });
    var messages: std.Io.Writer.Allocating = .init(gpa);
    defer messages.deinit();
    try std.testing.expectError(error.FontMismatch, FaceSet.loadReporting(gpa, io, dst, &messages.writer));
    try std.testing.expect(std.mem.endsWith(u8, messages.written(), "NotoSans-Regular.ttf fails its SHA256SUMS check"));
}

test "a missing directory names the missing file" {
    var messages: std.Io.Writer.Allocating = .init(std.testing.allocator);
    defer messages.deinit();
    try std.testing.expectError(error.FontMissing, FaceSet.loadReporting(std.testing.allocator, std.testing.io, "zig-out/local-test/bk-text/none", &messages.writer));
    try std.testing.expect(std.mem.endsWith(u8, messages.written(), "SHA256SUMS: missing"));
}
