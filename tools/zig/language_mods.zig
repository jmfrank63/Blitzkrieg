//! The language mods of M002 (D060, D068): `lang-ru`, `lang-zh-hans`,
//! `lang-ja` and `lang-hi` of the installed game, built by ELK's own pak
//! export from the tracked `Data/ELK` and the overlays of `Localization/`.
//! Per language: the database is copied to a scratch folder and the overlay
//! laid over it; a first export (for Hindi through the shaper, which rewrites
//! every text into Private Use Area characters) tells which characters the
//! texts use (`chars.txt`, and the cluster table for Hindi); bk-font renders
//! the language's Noto face at the game's four font sizes from them; and the
//! final export carries texts and fonts into `mods/lang-<code>/data/texts.pak`
//! with a `mod.xml`. Data/ELK is hashed before and after: nothing writes it.
//! BK_LM_GAME names the installed Game executable, BK_LM_FONT_TOOL the
//! installed bk-font and BK_LM_MODULES the folder of the engine libraries it
//! loads; `zig build language-mods` sets them.
const std = @import("std");
const elk = @import("elk");
const zip = @import("zip");
const database = elk.database;
const packages = elk.packages;
const testing = std.testing;

const LmShaper = opaque {};
extern fn LmShaperCreate(font: [*]const u8, size: usize, script: [*:0]const u8, language: [*:0]const u8) *LmShaper;
extern fn LmShaperDestroy(shaper: *LmShaper) void;
extern fn LmShaperEncode(shaper: *LmShaper, text: [*]const u16, len: usize, out: [*]u16, err: [*]u8, err_size: usize) c_long;
extern fn LmShaperClusters(shaper: *const LmShaper) usize;
extern fn LmShaperSerialize(shaper: *const LmShaper, out: [*]u8, capacity: usize) usize;

const scratch = "zig-out/local-test/language-mods";

const Language = struct {
    /// The overlay folder, `Localization/game/<code>`.
    code: []const u8,
    /// The mod folder below `mods/`.
    mod: []const u8,
    /// The name the language option lists (`MODName`).
    name: []const u8,
    /// The face below `vendor/fonts`.
    face: []const u8,
    /// Hindi only: the script the clusters are shaped with.
    shaped: bool = false,
};

const languages = [_]Language{
    .{ .code = "ru", .mod = "lang-ru", .name = "Русский", .face = "NotoSans-Regular.ttf" },
    .{ .code = "zh-Hans", .mod = "lang-zh-hans", .name = "简体中文", .face = "NotoSansSC-Regular.otf" },
    .{ .code = "ja", .mod = "lang-ja", .name = "日本語", .face = "NotoSansJP-Regular.otf" },
    .{ .code = "hi", .mod = "lang-hi", .name = "हिन्दी", .face = "NotoSansDevanagari-Regular.ttf", .shaped = true },
};

/// The game's four font folders and their cell heights (Data/Fonts/*/1.tfd).
const Size = struct { name: []const u8, height: []const u8 };
const sizes = [_]Size{
    .{ .name = "tiny", .height = "10" },
    .{ .name = "small", .height = "16" },
    .{ .name = "medium", .height = "20" },
    .{ .name = "large", .height = "40" },
};

/// The first overlay key that must come out of the pak as the overlay says
/// (and, for Hindi, shaped).
const probe_key = "textes/strings/imheader-gbutton-cutscenes";

fn envValue(allocator: std.mem.Allocator, key: []const u8) !?[:0]const u8 {
    var map = try testing.environ.createMap(allocator);
    const value = map.get(key) orelse return null;
    return try allocator.dupeSentinel(u8, value, 0);
}

fn required(allocator: std.mem.Allocator, key: []const u8) ![:0]const u8 {
    return (try envValue(allocator, key)) orelse {
        std.debug.print("{s} is not set: run zig build language-mods\n", .{key});
        return error.MissingEnvironment;
    };
}

/// The stage a failure happened in, named on the failing line.
var stage: []const u8 = "assemble";

test "the language mods are built from the overlays through ELK's export" {
    const io = testing.io;
    var arena = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    const game = try required(allocator, "BK_LM_GAME");
    const font_tool = try required(allocator, "BK_LM_FONT_TOOL");
    const modules = try required(allocator, "BK_LM_MODULES");
    const game_root = std.fs.path.dirname(game).?;
    const cwd_path = try std.process.currentPathAlloc(io, allocator);

    const elk_before = try treeHash("Data/ELK");
    for (languages) |language| {
        build(io, allocator, language, game_root, font_tool, modules, cwd_path) catch |err| {
            std.debug.print("language-mods: {s}: {s} failed: {s} (scratch: {s}/{s})\n", .{ language.code, stage, @errorName(err), scratch, language.code });
            return err;
        };
    }
    // Nothing above writes the database it was built from.
    try testing.expectEqual(elk_before, try treeHash("Data/ELK"));
}

fn build(io: std.Io, allocator: std.mem.Allocator, language: Language, game_root: []const u8, font_tool: []const u8, modules: []const u8, cwd_path: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    const root = try std.fs.path.join(allocator, &.{ scratch, language.code });
    cwd.deleteTree(io, root) catch {};
    const abs_root = try std.fs.path.join(allocator, &.{ cwd_path, root });

    // assemble: Data/ELK copied, the overlay over it.
    const db_folder = try std.fs.path.join(allocator, &.{ root, "db" });
    const elk_xml = try std.fs.path.join(allocator, &.{ db_folder, "elk.xml" });
    {
        stage = "assemble";
        try database.copyTree(io, "Data/ELK/game_data_base", try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
        try cwd.writeFile(io, .{ .sub_path = elk_xml, .data = try cwd.readFileAlloc(io, "Data/ELK/elk.xml", allocator, .limited(1 << 20)) });
        const overlay = try std.fs.path.join(allocator, &.{ "Localization/game", language.code, "game_data_base" });
        try database.copyTree(io, overlay, try std.fs.path.join(allocator, &.{ db_folder, "game_data_base" }));
    }
    var db = try database.Database.open(testing.allocator, io, elk_xml);
    defer db.deinit();

    const face_path = try std.fs.path.join(allocator, &.{ "vendor/fonts", language.face });
    const face = try cwd.readFileAlloc(io, face_path, allocator, .limited(64 << 20));
    var shaping = Shaper{};
    defer if (shaping.handle) |handle| LmShaperDestroy(handle);
    if (language.shaped) shaping.handle = LmShaperCreate(face.ptr, face.len, "Deva", "hi");
    const shape: ?packages.Shaping = if (language.shaped) .{ .context = &shaping, .rewrite = Shaper.rewrite } else null;

    // First export: which characters the texts use.
    var chars = @as([65536]bool, @splat(false));
    var texts: usize = 0;
    {
        stage = "export";
        const probe_root = try std.fs.path.join(allocator, &.{ root, "probe" });
        const written = try packages.exportTo(testing.allocator, io, &db, 0, probe_root, .{ .mod = language.mod }, .{ .shape = shape });
        defer testing.allocator.free(written.path);
        var package = try packages.readArchive(testing.allocator, io, written.path);
        defer package.deinit();
        for (package.entries) |entry| {
            if (!std.mem.endsWith(u8, entry.name, ".txt")) continue;
            texts += 1;
            var at: usize = 2;
            while (at + 1 < entry.bytes.len) : (at += 2) chars[std.mem.readInt(u16, entry.bytes[at..][0..2], .little)] = true;
        }
    }

    // bk-font: the character list (the Private Use Area is the cluster table's)
    // and one font per size.
    var distinct: usize = 0;
    var list: std.ArrayListUnmanaged(u8) = .empty;
    for (chars, 0..) |used, unit| {
        if (!used or unit < 32 or (unit >= 0xD800 and unit <= 0xF8FF)) continue;
        var buffer: [4]u8 = undefined;
        const len = try std.unicode.utf8Encode(@intCast(unit), &buffer);
        try list.appendSlice(allocator, buffer[0..len]);
        distinct += 1;
    }
    const chars_path = try std.fs.path.join(allocator, &.{ root, "chars.txt" });
    try cwd.writeFile(io, .{ .sub_path = chars_path, .data = list.items });
    var clusters: usize = 0;
    const clusters_path = try std.fs.path.join(allocator, &.{ root, "clusters.txt" });
    if (language.shaped) {
        clusters = LmShaperClusters(shaping.handle.?);
        var none: [1]u8 = undefined;
        const size = LmShaperSerialize(shaping.handle.?, &none, 0);
        const table = try allocator.alloc(u8, size);
        _ = LmShaperSerialize(shaping.handle.?, table.ptr, size);
        try cwd.writeFile(io, .{ .sub_path = clusters_path, .data = table });
    }
    var environ = try testing.environ.createMap(allocator);
    const user = try std.fs.path.join(allocator, &.{ abs_root, "user" });
    try cwd.createDirPath(io, user);
    try environ.put("XDG_DATA_HOME", user);
    try environ.put("BK_USER_ROOT", user);

    var fonts: std.ArrayListUnmanaged(packages.Entry) = .empty;
    var atlas_report: std.ArrayListUnmanaged(u8) = .empty;
    for (sizes) |size| {
        stage = "bk-font";
        const out = try std.fs.path.join(allocator, &.{ abs_root, "fonts", size.name });
        var argv: std.ArrayListUnmanaged([]const u8) = .empty;
        try argv.appendSlice(allocator, &.{ font_tool, "--ttf", face_path, "--height", size.height, "--chars", chars_path, "--out", out, "--force", "--modules", modules });
        if (language.shaped) try argv.appendSlice(allocator, &.{ "--clusters", clusters_path, "--script", "Deva", "--language", "hi" });
        const result = try std.process.run(allocator, io, .{
            .argv = argv.items,
            .cwd = .{ .path = cwd_path },
            .environ_map = &environ,
            .timeout = .{ .duration = .{ .raw = .fromSeconds(180), .clock = .awake } },
        });
        if (!result.term.success()) {
            std.debug.print("language-mods: {s}: bk-font ({s}) ended {any}: {s}{s}\n", .{ language.code, size.name, result.term, result.stdout, result.stderr });
            return error.FontToolFailed;
        }
        // "... , 512x256 atlas": the size is the last word but one.
        const line = std.mem.trimEnd(u8, result.stdout, "\r\n");
        var words = std.mem.splitBackwardsScalar(u8, line, ' ');
        _ = words.next();
        try atlas_report.print(allocator, " {s}={s}", .{ size.name, words.next().? });
        for ([_][]const u8{ "1.tfd", "1_h.dds" }) |file| {
            const bytes = try cwd.readFileAlloc(io, try std.fs.path.join(allocator, &.{ out, file }), allocator, .limited(64 << 20));
            try fonts.append(allocator, .{ .name = try std.mem.concat(allocator, u8, &.{ size.name, "/", file }), .bytes = bytes });
        }
    }

    // export and install: the same texts again (the table is complete, so the
    // shaper answers as before) with the fonts, into the mod of the install.
    const mod_folder = try std.fs.path.join(allocator, &.{ game_root, "mods", language.mod });
    cwd.deleteTree(io, mod_folder) catch {};
    var pak_path: []u8 = undefined;
    {
        stage = "install";
        const written = try packages.exportTo(testing.allocator, io, &db, 0, game_root, .{ .mod = language.mod }, .{ .shape = shape, .fonts = fonts.items });
        defer testing.allocator.free(written.path);
        pak_path = try allocator.dupe(u8, written.path);
        const mod_xml = try std.fmt.allocPrint(allocator, "<?xml version=\"1.0\"?>\r\n<base><MODName>{s}</MODName><MODVersion>1.0</MODVersion><MODDesc></MODDesc></base>\r\n", .{language.name});
        try database.writeFileAtomic(io, testing.allocator, try std.fs.path.join(allocator, &.{ std.fs.path.dirname(written.path).?, "mod.xml" }), mod_xml);
    }

    // The pak read back with the engine's own reader.
    var package = try packages.readArchive(testing.allocator, io, pak_path);
    defer package.deinit();
    // The reader of importable texts skips the fonts, so the archive itself
    // is opened for them.
    const pak_bytes = try cwd.readFileAlloc(io, pak_path, allocator, .limited(256 << 20));
    var archive = try zip.Archive.parse(testing.allocator, pak_bytes);
    defer archive.deinit();
    for (sizes) |size| for ([_][]const u8{ "1.tfd", "1_h.dds" }) |file| {
        const name = try std.mem.concat(allocator, u8, &.{ "fonts/", size.name, "/", file });
        var found = false;
        for (archive.entries) |*entry| {
            if (std.mem.eql(u8, entry.name, name)) found = (try archive.extract(allocator, entry)).len > 0;
        }
        if (!found) {
            std.debug.print("language-mods: {s}: {s} is missing from {s}\n", .{ language.code, name, pak_path });
            return error.FontMissing;
        }
    };
    const overlay_text = try cwd.readFileAlloc(io, try std.mem.concat(allocator, u8, &.{ "Localization/game/", language.code, "/game_data_base/", probe_key, ".txt" }), allocator, .limited(1 << 20));
    const packed_text = for (package.entries) |entry| {
        if (std.mem.eql(u8, entry.name, probe_key ++ ".txt")) break entry.bytes;
    } else return error.OverlayTextMissing;
    if (language.shaped) {
        try testing.expect(!std.mem.eql(u8, overlay_text, packed_text));
        var pua = false;
        var at: usize = 2;
        while (at + 1 < packed_text.len) : (at += 2) {
            const unit = std.mem.readInt(u16, packed_text[at..][0..2], .little);
            pua = pua or (unit >= 0xE000 and unit <= 0xF8FF);
        }
        try testing.expect(pua);
    } else {
        try testing.expectEqualSlices(u8, overlay_text, packed_text);
    }
    std.debug.print("language-mods: {s}: {d} texts, {d} characters, {d} clusters, atlas{s}, installed {s}\n", .{ language.code, texts, distinct, clusters, atlas_report.items, pak_path });
}

/// The export's rewrite for a shaped language: a text with Devanagari in it
/// goes through the shaper, any other is passed on as it is.
const Shaper = struct {
    handle: ?*LmShaper = null,

    fn rewrite(context: ?*anyopaque, gpa: std.mem.Allocator, bytes: []const u8) anyerror![]u8 {
        const self: *Shaper = @ptrCast(@alignCast(context.?));
        var devanagari = false;
        var at: usize = 2;
        while (at + 1 < bytes.len) : (at += 2) {
            const unit = std.mem.readInt(u16, bytes[at..][0..2], .little);
            devanagari = devanagari or (unit >= 0x0900 and unit <= 0x097f);
        }
        if (!devanagari) return gpa.dupe(u8, bytes);
        const count = (bytes.len - 2) / 2;
        const text = try gpa.alloc(u16, count);
        defer gpa.free(text);
        for (text, 0..) |*unit, index| unit.* = std.mem.readInt(u16, bytes[2 + index * 2 ..][0..2], .little);
        const encoded = try gpa.alloc(u16, count);
        defer gpa.free(encoded);
        var message: [256]u8 = undefined;
        const length = LmShaperEncode(self.handle.?, text.ptr, count, encoded.ptr, &message, message.len);
        if (length < 0) {
            std.debug.print("language-mods: shaping failed: {s}\n", .{std.mem.sliceTo(&message, 0)});
            return error.ShapingFailed;
        }
        const out = try gpa.alloc(u8, 2 + @as(usize, @intCast(length)) * 2);
        out[0] = 0xff;
        out[1] = 0xfe;
        for (encoded[0..@intCast(length)], 0..) |unit, index| std.mem.writeInt(u16, out[2 + index * 2 ..][0..2], unit, .little);
        return out;
    }
};

/// A hash of every file below `path` that does not depend on the walk order.
fn treeHash(path: []const u8) !u64 {
    const io = testing.io;
    var dir = try std.Io.Dir.cwd().openDir(io, path, .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(testing.allocator);
    defer walker.deinit();
    var sum: u64 = 0;
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        const bytes = try dir.readFileAlloc(io, entry.path, testing.allocator, .limited(64 << 20));
        defer testing.allocator.free(bytes);
        sum +%= std.hash.Wyhash.hash(std.hash.Wyhash.hash(0, entry.path), bytes);
    }
    return sum;
}
