//! Generates the per-extension source art fixtures the ResourceEditor
//! milestone (M001) measures against. Deterministic and idempotent: a second
//! `zig build make-resource-fixtures` run produces byte-identical files and
//! an unchanged EXTENSIONS.md hash column.
//!
//! For every one of the 21 project extensions registered by the MFC editor
//! (`Sources/src/editor/*Frm.cpp` sets szComposerName / szComposerSaveName /
//! szExtension on a CParentFrame subclass) this tool writes:
//!
//!   <ext>/project.<ext>   -- a minimal CRLF-terminated XML with the real
//!                            `*_Composer_Project` root the MFC loader
//!                            selects on (see ParentFrame::LoadComposerFile
//!                            calling CreateDataTreeSaver with
//!                            szComposerSaveName as the base node). The body
//!                            is a single `<fixture>` child item with a
//!                            `name` chunk, which is as thin as the shape
//!                            allows while still parsing. This file is only
//!                            seeded: once a sub-editor's port replaces it
//!                            with a project in MFC's item-tree shape (S03),
//!                            the committed file is kept and only measured.
//!
//! plus one tiny source-art placeholder per editor kind:
//!   picture   : art-16x16.tga -- 16 x 16 24-bit solid-colour targa (654 B).
//!   mesh      : mesh-2x2x2.obj -- 2 x 2 x 2 cube OBJ (8 verts, 12 tris).
//!   sprite    : sprite-1frame.tga -- 16 x 16 solid-colour targa for a one-
//!                                     frame sprite.
//!   particle  : particle-2key.txt -- two-keyframe CRLF text (t,value pairs).
//!
//! The msh fixture also gets the source art its project names, so the unit
//! exporter has something to convert: 1.tga, 1w.tga, 1a.tga, 2.tga, 2w.tga,
//! 2a.tga and icon.tga, 16 x 16 solid-colour targas coloured by their own
//! names (each channel exact in RGB565), plus the name.txt, desc.txt and
//! stats.txt its Localization item names. Its three models 1.mod, 2.mod and 3.mod are not
//! generated: they are copied from the shipped
//! Data/Units/Technics/German/Artillery/8_8_cm_FlaK18 (see EXTENSIONS.md).
//!
//! Every generated file is below 4 KB. The art is not meant to be a real asset; it is
//! the smallest shape downstream fixtures (XML round-trip, DXT tolerance,
//! preview-scene capture) can read without eyeballing. Replacing the
//! placeholders with richer, still-generated art later is a drop-in change
//! as long as the fixtures remain under 4 KB and under this file's control.
//!
//! usage: resource-editor-fixtures --out <dir> [--log <file>]
//!
//! When invoked by `zig build make-resource-fixtures` the step calls
//! `--out tools/zig/fixtures/resource_editor --log zig-out/local-test/\
//! resource_editor/make-fixtures.log` so the committed goldens live under
//! source control and the stderr-mirroring log lives beside other local
//! build artefacts.

const std = @import("std");
const sha256 = std.crypto.hash.sha2.Sha256;

/// Which placeholder art shape to drop in beside a project file.
pub const ArtKind = enum {
    /// A 16 x 16 solid-colour TGA. The MFC Mission / Chapter / Campaign /
    /// Medal / Weapon / Mine / Fence / Trench / Build / Bridge / Tileset /
    /// 3D Road / 3D River / GUI composers all treat their nodes as pictures
    /// at some stage (toolbar icons, UI tiles, overlay stamps). The TGA is
    /// a conservative drop-in that any image loader will accept.
    picture,
    /// A 2 x 2 x 2 cube in Wavefront OBJ for the Mesh / Object / Unit
    /// composers. OBJ is text; CRLF-terminated to match .gitattributes.
    mesh,
    /// A single-frame TGA for Sprite and Squad composers.
    sprite,
    /// A two-keyframe plain-text keyframe table for Particle and Effect
    /// composers. A real engine source would be XML; the goal here is a
    /// fingerprintable 2-keyframe trace downstream slices can diff.
    particle,
};

/// One MFC frame's registration tuple, exactly as it appears in that
/// frame's constructor. Reordering this list rewrites EXTENSIONS.md.
pub const Fixture = struct {
    ext: []const u8,
    frame_cpp: []const u8,
    composer_name: []const u8,
    composer_save_name: []const u8,
    art: ArtKind,
};

/// The 21 extensions, in a stable source-code order chosen for a readable
/// EXTENSIONS.md table (basic data -> infantry/mesh -> sprite/particle ->
/// terrain -> mission/campaign -> UI). The art column is this file's
/// author-chosen taxonomy; it is recorded in EXTENSIONS.md so downstream
/// slices do not have to re-derive it. See the header comment above for the
/// reasoning. If this list changes, EXTENSIONS.md regenerates and the file's
/// hash column updates on the next make-resource-fixtures run.
pub const fixtures = [_]Fixture{
    .{ .ext = "wpn", .frame_cpp = "WeaponFrm.cpp", .composer_name = "Weapon Editor", .composer_save_name = "Weapon_Composer_Project", .art = .picture },
    .{ .ext = "mcp", .frame_cpp = "MineFrm.cpp", .composer_name = "Mine Editor", .composer_save_name = "Mine_Composer_Project", .art = .picture },
    .{ .ext = "trc", .frame_cpp = "TrenchFrm.cpp", .composer_name = "Trench Editor", .composer_save_name = "Trench_Composer_Project", .art = .picture },
    .{ .ext = "scp", .frame_cpp = "SquadFrm.cpp", .composer_name = "Squad Editor", .composer_save_name = "Squad_Composer_Project", .art = .sprite },
    .{ .ext = "spt", .frame_cpp = "SpriteFrm.cpp", .composer_name = "Sprite Editor", .composer_save_name = "Sprite_Composer_Project", .art = .sprite },
    .{ .ext = "unt", .frame_cpp = "AnimationFrm.cpp", .composer_name = "Infantry Editor", .composer_save_name = "Unit_Composer_Project", .art = .mesh },
    .{ .ext = "msh", .frame_cpp = "MeshFrm.cpp", .composer_name = "Unit Editor", .composer_save_name = "Mesh_Composer_Project", .art = .mesh },
    .{ .ext = "obt", .frame_cpp = "ObjectFrm.cpp", .composer_name = "Object Editor", .composer_save_name = "Object_Composer_Project", .art = .mesh },
    .{ .ext = "fnc", .frame_cpp = "FenceFrm.cpp", .composer_name = "Fence Editor", .composer_save_name = "Fence_Composer_Project", .art = .picture },
    .{ .ext = "bld", .frame_cpp = "BuildFrm.cpp", .composer_name = "Building Editor", .composer_save_name = "Building_Composer_Project", .art = .picture },
    .{ .ext = "bdg", .frame_cpp = "BridgeFrm.cpp", .composer_name = "Bridge Editor", .composer_save_name = "Bridge_Composer_Project", .art = .picture },
    .{ .ext = "pcp", .frame_cpp = "ParticleFrm.cpp", .composer_name = "Particle Editor", .composer_save_name = "Particle_Composer_Project", .art = .particle },
    .{ .ext = "eff", .frame_cpp = "EffectFrm.cpp", .composer_name = "Effect Editor", .composer_save_name = "Effect_Composer_Project", .art = .particle },
    .{ .ext = "til", .frame_cpp = "TileSetFrm.cpp", .composer_name = "Terrain Editor", .composer_save_name = "TileSet_Composer_Project", .art = .picture },
    .{ .ext = "3rd", .frame_cpp = "3dRoadFrm.cpp", .composer_name = "Road Editor", .composer_save_name = "Road3D_Composer_Project", .art = .picture },
    .{ .ext = "3rv", .frame_cpp = "3dRiverFrm.cpp", .composer_name = "River Editor", .composer_save_name = "River3D_Composer_Project", .art = .picture },
    .{ .ext = "mip", .frame_cpp = "MissionFrm.cpp", .composer_name = "Mission Editor", .composer_save_name = "Mission_Composer_Project", .art = .picture },
    .{ .ext = "chc", .frame_cpp = "ChapterFrm.cpp", .composer_name = "Chapter Editor", .composer_save_name = "Chapter_Composer_Project", .art = .picture },
    .{ .ext = "cgc", .frame_cpp = "CampaignFrm.cpp", .composer_name = "Campaign Editor", .composer_save_name = "Campaign_Composer_Project", .art = .picture },
    .{ .ext = "mdc", .frame_cpp = "MedalFrm.cpp", .composer_name = "Medal Editor", .composer_save_name = "Medal_Composer_Project", .art = .picture },
    .{ .ext = "gui", .frame_cpp = "GUIFrame.cpp", .composer_name = "GUI Editor", .composer_save_name = "GUI_Composer_Project", .art = .picture },
};

comptime {
    if (fixtures.len != 21) @compileError("expected exactly 21 fixtures");
}

/// Thin wrapper over std.ArrayList(u8).empty + an allocator so render
/// helpers can push bytes without plumbing both. ArrayList in 0.16 has no
/// std.Io.Writer facade; appendSlice is the equivalent primitive.
const Buf = struct {
    data: std.ArrayList(u8) = .empty,
    a: std.mem.Allocator,

    fn deinit(self: *Buf) void {
        self.data.deinit(self.a);
    }

    fn push(self: *Buf, bytes: []const u8) !void {
        try self.data.appendSlice(self.a, bytes);
    }

    fn pushFmt(self: *Buf, comptime fmt: []const u8, args: anytype) !void {
        var scratch: [320]u8 = undefined;
        const slice = try std.fmt.bufPrint(&scratch, fmt, args);
        try self.data.appendSlice(self.a, slice);
    }

    fn items(self: *const Buf) []const u8 {
        return self.data.items;
    }

    fn toOwnedSlice(self: *Buf) ![]u8 {
        return self.data.toOwnedSlice(self.a);
    }
};

/// Shape the project XML the MFC loader will accept for a given composer
/// save name. The outer `<?xml version="1.0"?>` + root element shape is
/// what Sources/src/StreamIOLib/DataTreeXML.cpp's CDataTreeXML::Open writes
/// in WRITE mode; the first chunk inserted after Open is the base node
/// (szComposerSaveName) and nested chunks become child elements.
fn writeProjectXml(buf: *Buf, composer_save_name: []const u8) !void {
    try buf.push("<?xml version=\"1.0\"?>\r\n");
    try buf.push("<");
    try buf.push(composer_save_name);
    try buf.push(">\r\n");
    try buf.push("\t<fixture>\r\n");
    try buf.push("\t\t<name>minimal</name>\r\n");
    try buf.push("\t</fixture>\r\n");
    try buf.push("</");
    try buf.push(composer_save_name);
    try buf.push(">\r\n");
}

/// 16 x 16, 24-bit, uncompressed, bottom-up Targa with a solid colour
/// derived from the ext string so each fixture gets a distinguishable
/// placeholder without any random source. The 18-byte TGA header is the
/// minimum valid shape (no colour map, image type 2, no footer).
fn writeSolidTga(buf: *Buf, seed: []const u8) !void {
    const w: u16 = 16;
    const h: u16 = 16;
    var header = [_]u8{0} ** 18;
    header[2] = 2;
    std.mem.writeInt(u16, header[12..14], w, .little);
    std.mem.writeInt(u16, header[14..16], h, .little);
    header[16] = 24;
    header[17] = 0;
    try buf.push(&header);
    var hash: [sha256.digest_length]u8 = undefined;
    sha256.hash(seed, &hash, .{});
    const b = hash[0];
    const g = hash[1];
    const r = hash[2];
    var row: [16 * 3]u8 = undefined;
    var x: usize = 0;
    while (x < 16) : (x += 1) {
        row[x * 3 + 0] = b;
        row[x * 3 + 1] = g;
        row[x * 3 + 2] = r;
    }
    var y: usize = 0;
    while (y < 16) : (y += 1) try buf.push(&row);
}

/// Like writeSolidTga, but each channel is snapped to a value that RGB565
/// holds exactly, so the DXT encoder has no quantisation error to add and the
/// exporter's DDS can be held to the gate of shipped textures (whose p99 is
/// 2) rather than to the worst case of an arbitrary colour.
fn writeExactSolidTga(buf: *Buf, seed: []const u8) !void {
    var header = [_]u8{0} ** 18;
    header[2] = 2;
    std.mem.writeInt(u16, header[12..14], 16, .little);
    std.mem.writeInt(u16, header[14..16], 16, .little);
    header[16] = 24;
    try buf.push(&header);
    var hash: [sha256.digest_length]u8 = undefined;
    sha256.hash(seed, &hash, .{});
    const b5: u8 = hash[0] >> 3;
    const g6: u8 = hash[1] >> 2;
    const r5: u8 = hash[2] >> 3;
    const b = (b5 << 3) | (b5 >> 2);
    const g = (g6 << 2) | (g6 >> 4);
    const r = (r5 << 3) | (r5 >> 2);
    var row: [16 * 3]u8 = undefined;
    var x: usize = 0;
    while (x < 16) : (x += 1) {
        row[x * 3 + 0] = b;
        row[x * 3 + 1] = g;
        row[x * 3 + 2] = r;
    }
    var y: usize = 0;
    while (y < 16) : (y += 1) try buf.push(&row);
}

/// A 2 x 2 x 2 cube in Wavefront OBJ, text with CRLF line endings. 8
/// vertices, 12 triangle faces. The vertex layout is deterministic (unit
/// cube centred at the origin).
fn writeMeshObj(buf: *Buf) !void {
    try buf.push("# resource-editor fixture: 2x2x2 cube\r\n");
    const verts = [_][3]i8{
        .{ -1, -1, -1 }, .{ 1, -1, -1 }, .{ 1, 1, -1 }, .{ -1, 1, -1 },
        .{ -1, -1, 1 },  .{ 1, -1, 1 },  .{ 1, 1, 1 },  .{ -1, 1, 1 },
    };
    for (verts) |v| try buf.pushFmt("v {d} {d} {d}\r\n", .{ v[0], v[1], v[2] });
    const faces = [_][3]u8{
        .{ 1, 3, 2 }, .{ 1, 4, 3 },
        .{ 5, 6, 7 }, .{ 5, 7, 8 },
        .{ 1, 2, 6 }, .{ 1, 6, 5 },
        .{ 4, 7, 3 }, .{ 4, 8, 7 },
        .{ 1, 5, 8 }, .{ 1, 8, 4 },
        .{ 2, 3, 7 }, .{ 2, 7, 6 },
    };
    for (faces) |f| try buf.pushFmt("f {d} {d} {d}\r\n", .{ f[0], f[1], f[2] });
}

/// A plain-text two-keyframe source (CRLF-terminated). The columns are
/// time (seconds) and a scalar value; the shape is small on purpose so a
/// downstream particle slice can round-trip through a richer schema.
fn writeParticleKeys(buf: *Buf) !void {
    try buf.push("# resource-editor fixture: 2-keyframe source\r\n");
    try buf.push("# time\tvalue\r\n");
    try buf.push("0.000\t0.0\r\n");
    try buf.push("1.000\t1.0\r\n");
}

/// The pictures the msh project names beside its models (Graphics Info: the
/// alive and dead textures of three seasons) plus the unit icon the exporter
/// looks for in the project folder.
const msh_pictures = [_][]const u8{ "1.tga", "1w.tga", "1a.tga", "2.tga", "2w.tga", "2a.tga", "icon.tga" };

/// The art filename (without directory) a given ArtKind writes.
/// The pictures the obt project names: a sprite and its shadow per season.
/// 32-bit so the object exporter has alpha to pack, build the shadow from and
/// crop the icon to: an opaque 8 x 8 square in a transparent 16 x 16, coloured
/// by the file name; a shadow is a black half-alpha 6 x 6 square shifted down
/// and right, so the sprite does not hide all of it.
const obt_pictures = [_][]const u8{ "1.tga", "1s.tga", "1w.tga", "1ws.tga", "1a.tga", "1as.tga" };

fn writeAlphaTga(buf: *Buf, seed: []const u8, shadow: bool) !void {
    var header = [_]u8{0} ** 18;
    header[2] = 2;
    std.mem.writeInt(u16, header[12..14], 16, .little);
    std.mem.writeInt(u16, header[14..16], 16, .little);
    header[16] = 32;
    header[17] = 8;
    try buf.push(&header);
    var hash: [sha256.digest_length]u8 = undefined;
    sha256.hash(seed, &hash, .{});
    const b5: u8 = hash[0] >> 3;
    const g6: u8 = hash[1] >> 2;
    const r5: u8 = hash[2] >> 3;
    var px = [4]u8{ (b5 << 3) | (b5 >> 2), (g6 << 2) | (g6 >> 4), (r5 << 3) | (r5 >> 2), 255 };
    if (shadow) px = .{ 0, 0, 0, 128 };
    var y: usize = 0;
    while (y < 16) : (y += 1) {
        var x: usize = 0;
        while (x < 16) : (x += 1) {
            const inside = if (shadow) x >= 8 and x < 14 and y >= 8 and y < 14 else x >= 4 and x < 12 and y >= 4 and y < 12;
            if (inside) try buf.push(&px) else try buf.push(&[4]u8{ 0, 0, 0, 0 });
        }
    }
}

fn artFileName(kind: ArtKind) []const u8 {
    return switch (kind) {
        .picture => "art-16x16.tga",
        .mesh => "mesh-2x2x2.obj",
        .sprite => "sprite-1frame.tga",
        .particle => "particle-2key.txt",
    };
}

/// Build the raw bytes an ArtKind places next to the project file.
fn renderArt(allocator: std.mem.Allocator, kind: ArtKind, seed: []const u8) ![]u8 {
    var buf: Buf = .{ .a = allocator };
    errdefer buf.deinit();
    switch (kind) {
        .picture, .sprite => try writeSolidTga(&buf, seed),
        .mesh => try writeMeshObj(&buf),
        .particle => try writeParticleKeys(&buf),
    }
    return buf.toOwnedSlice();
}

/// Build the raw bytes a project.<ext> file contains.
fn renderProject(allocator: std.mem.Allocator, composer_save_name: []const u8) ![]u8 {
    var buf: Buf = .{ .a = allocator };
    errdefer buf.deinit();
    try writeProjectXml(&buf, composer_save_name);
    return buf.toOwnedSlice();
}

/// Lowercase the first 7 hex nybbles of a sha256 digest for compact
/// identification in the EXTENSIONS.md table. 28 bits is plenty of
/// collision room for 42 files.
fn hashTag(bytes: []const u8, out: *[7]u8) void {
    var hash: [sha256.digest_length]u8 = undefined;
    sha256.hash(bytes, &hash, .{});
    const chars = "0123456789abcdef";
    out[0] = chars[hash[0] >> 4];
    out[1] = chars[hash[0] & 0xf];
    out[2] = chars[hash[1] >> 4];
    out[3] = chars[hash[1] & 0xf];
    out[4] = chars[hash[2] >> 4];
    out[5] = chars[hash[2] & 0xf];
    out[6] = chars[hash[3] >> 4];
}

const ExtensionsRow = struct {
    ext: []const u8,
    frame_cpp: []const u8,
    composer_name: []const u8,
    composer_save_name: []const u8,
    art: ArtKind,
    project_bytes: usize,
    project_tag: [7]u8,
    art_file: []const u8,
    art_bytes: usize,
    art_tag: [7]u8,
};

fn writeExtensionsMd(buf: *Buf, rows: []const ExtensionsRow) !void {
    try buf.push("# ResourceEditor fixtures\r\n");
    try buf.push("\r\n");
    try buf.push("Generated by `zig build make-resource-fixtures`\r\n");
    try buf.push("(`tools/zig/resource_editor_fixtures.zig`).\r\n");
    try buf.push("Do not edit by hand -- re-run the step after changing the\r\n");
    try buf.push("generator and commit the updated files together.\r\n");
    try buf.push("\r\n");
    try buf.push("One row per project extension registered by a `CParentFrame`\r\n");
    try buf.push("subclass in `Sources/src/editor/*Frm.cpp` (via `szExtension`\r\n");
    try buf.push("plus `szComposerSaveName`). The art column names this\r\n");
    try buf.push("fixture's placeholder source art and its kind; the hash\r\n");
    try buf.push("columns are the first 28 bits of each file's SHA-256 so an\r\n");
    try buf.push("agent can diff a run against this table without rereading\r\n");
    try buf.push("the file bytes.\r\n");
    try buf.push("\r\n");
    try buf.push("| # | ext | frame | composer | root element | project bytes | project hash | art | art bytes | art hash |\r\n");
    try buf.push("|---|-----|-------|----------|--------------|---------------|--------------|-----|-----------|----------|\r\n");
    for (rows, 0..) |row, i| {
        try buf.pushFmt(
            "| {d} | `{s}` | `{s}` | {s} | `{s}` | {d} | `{s}` | `{s}` ({s}) | {d} | `{s}` |\r\n",
            .{
                i + 1,
                row.ext,
                row.frame_cpp,
                row.composer_name,
                row.composer_save_name,
                row.project_bytes,
                row.project_tag[0..],
                row.art_file,
                @tagName(row.art),
                row.art_bytes,
                row.art_tag[0..],
            },
        );
    }
    try buf.push("\r\n");
    try buf.push("## Unit (msh) source art\r\n");
    try buf.push("\r\n");
    try buf.push("`msh/project.msh` names `1.mod`, `2.mod`, `3.mod` and the textures `1.tga`,\r\n");
    try buf.push("`1w.tga`, `1a.tga`, `2.tga`, `2w.tga`, `2a.tga`; the exporter also looks for\r\n");
    try buf.push("`icon.tga` in the project folder. The three models are copied unchanged from\r\n");
    try buf.push("the shipped `Data/Units/Technics/German/Artillery/8_8_cm_FlaK18` (the unit\r\n");
    try buf.push("ships all three, so the install and transportable variants are real\r\n");
    try buf.push("models); none of the project's locator references match that unit's nodes,\r\n");
    try buf.push("which the exporter reports as warnings. The seven targas are generated by\r\n");
    try buf.push("this tool, 16 x 16 solid colour, each coloured by its own file name.\r\n");
    try buf.push("\r\n");
    try buf.push("## Object (obt) source art\r\n");
    try buf.push("\r\n");
    try buf.push("`obt/project.obt` names `1.tga`, `1s.tga`, `1w.tga`, `1ws.tga`, `1a.tga` and\r\n");
    try buf.push("`1as.tga` (sprite and shadow per season), generated by this tool as 16 x 16\r\n");
    try buf.push("32-bit targas: an opaque 8 x 8 square, coloured by file name, in a transparent\r\n");
    try buf.push("frame (shadows black at half alpha). Its own_data and desc hold a passability\r\n");
    try buf.push("grid with origin (2, 1), transparency tiles 1..7, two trans-lines, a zero point\r\n");
    try buf.push("and a sprite position; the project file is hand-edited, not regenerated.\r\n");
}

const RunStats = struct {
    files_written: usize = 0,
    files_unchanged: usize = 0,
    bytes_written: usize = 0,

    fn note(self: *RunStats, changed: bool, bytes: usize) void {
        if (changed) self.files_written += 1 else self.files_unchanged += 1;
        self.bytes_written += bytes;
    }
};

const WriteResult = struct {
    changed: bool,
    bytes: usize,
    tag: [7]u8,
};

fn writeIfChanged(
    io: std.Io,
    out_dir: std.Io.Dir,
    sub_path: []const u8,
    data: []const u8,
) !WriteResult {
    var existing_matches = false;
    if (out_dir.readFileAlloc(io, sub_path, std.heap.page_allocator, .limited(16 * 1024))) |existing| {
        defer std.heap.page_allocator.free(existing);
        existing_matches = std.mem.eql(u8, existing, data);
    } else |err| switch (err) {
        error.FileNotFound => {},
        else => return err,
    }
    if (!existing_matches) try out_dir.writeFile(io, .{ .sub_path = sub_path, .data = data });
    var result: WriteResult = .{ .changed = !existing_matches, .bytes = data.len, .tag = undefined };
    hashTag(data, &result.tag);
    return result;
}

/// Writes `data` only when `sub_path` does not exist yet; an existing file is
/// kept and reported with its own size and hash. The project files start as
/// stubs and are then replaced by projects the ResourceModel tests need in
/// MFC's shape, which this generator must not overwrite.
fn seedIfMissing(
    io: std.Io,
    out_dir: std.Io.Dir,
    sub_path: []const u8,
    data: []const u8,
) !WriteResult {
    if (out_dir.readFileAlloc(io, sub_path, std.heap.page_allocator, .limited(1024 * 1024))) |existing| {
        defer std.heap.page_allocator.free(existing);
        var kept: WriteResult = .{ .changed = false, .bytes = existing.len, .tag = undefined };
        hashTag(existing, &kept.tag);
        return kept;
    } else |err| switch (err) {
        error.FileNotFound => {},
        else => return err,
    }
    return writeIfChanged(io, out_dir, sub_path, data);
}

fn usage() noreturn {
    std.debug.print("usage: resource-editor-fixtures --out <dir> [--log <file>]\n", .{});
    std.process.exit(2);
}

pub fn main(init: std.process.Init) !void {
    var arena_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = init.io;

    var iterator = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    defer iterator.deinit();
    _ = iterator.skip();
    var out_path: ?[]const u8 = null;
    var log_path: ?[]const u8 = null;
    while (iterator.next()) |arg| {
        if (std.mem.eql(u8, arg, "--out")) {
            out_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--log")) {
            log_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else {
            usage();
        }
    }
    const out = out_path orelse usage();

    try std.Io.Dir.cwd().createDirPath(io, out);
    var out_dir = try std.Io.Dir.cwd().openDir(io, out, .{});
    defer out_dir.close(io);

    var log: Buf = .{ .a = arena };

    var rows = try arena.alloc(ExtensionsRow, fixtures.len);
    var stats: RunStats = .{};

    for (fixtures, 0..) |fx, i| {
        try out_dir.createDirPath(io, fx.ext);
        const project_bytes = try renderProject(arena, fx.composer_save_name);
        const project_sub = try std.fmt.allocPrint(arena, "{s}/project.{s}", .{ fx.ext, fx.ext });
        const project_result = try seedIfMissing(io, out_dir, project_sub, project_bytes);
        stats.note(project_result.changed, project_result.bytes);

        const art_name = artFileName(fx.art);
        const art_bytes = try renderArt(arena, fx.art, fx.ext);
        const art_sub = try std.fmt.allocPrint(arena, "{s}/{s}", .{ fx.ext, art_name });
        const art_result = try writeIfChanged(io, out_dir, art_sub, art_bytes);
        stats.note(art_result.changed, art_result.bytes);

        if (std.mem.eql(u8, fx.ext, "msh")) {
            // The localisation files the project names, one line each.
            for ([_][]const u8{ "name.txt", "desc.txt", "stats.txt" }) |text_name| {
                const text_sub = try std.fmt.allocPrint(arena, "msh/{s}", .{text_name});
                const text = try std.fmt.allocPrint(arena, "fixture unit {s}\r\n", .{text_name});
                const text_result = try writeIfChanged(io, out_dir, text_sub, text);
                stats.note(text_result.changed, text_result.bytes);
            }
            for (msh_pictures) |picture| {
                var tga: Buf = .{ .a = arena };
                try writeExactSolidTga(&tga, picture);
                const picture_sub = try std.fmt.allocPrint(arena, "msh/{s}", .{picture});
                const picture_result = try writeIfChanged(io, out_dir, picture_sub, tga.items());
                stats.note(picture_result.changed, picture_result.bytes);
                try log.pushFmt("fixture ext=msh art={s} bytes={d} hash={s}\n", .{ picture_sub, picture_result.bytes, picture_result.tag[0..] });
            }
        }

        if (std.mem.eql(u8, fx.ext, "obt")) {
            for (obt_pictures) |picture| {
                var tga: Buf = .{ .a = arena };
                try writeAlphaTga(&tga, picture, picture.len > 1 and picture[picture.len - 5] == 's');
                const picture_sub = try std.fmt.allocPrint(arena, "obt/{s}", .{picture});
                const picture_result = try writeIfChanged(io, out_dir, picture_sub, tga.items());
                stats.note(picture_result.changed, picture_result.bytes);
                try log.pushFmt("fixture ext=obt art={s} bytes={d} hash={s}\n", .{ picture_sub, picture_result.bytes, picture_result.tag[0..] });
            }
        }

        try log.pushFmt(
            "fixture ext={s} project={s} bytes={d} hash={s} art={s} bytes={d} hash={s}\n",
            .{ fx.ext, project_sub, project_result.bytes, project_result.tag[0..], art_sub, art_result.bytes, art_result.tag[0..] },
        );

        rows[i] = .{
            .ext = fx.ext,
            .frame_cpp = fx.frame_cpp,
            .composer_name = fx.composer_name,
            .composer_save_name = fx.composer_save_name,
            .art = fx.art,
            .project_bytes = project_result.bytes,
            .project_tag = project_result.tag,
            .art_file = art_name,
            .art_bytes = art_result.bytes,
            .art_tag = art_result.tag,
        };
    }

    var md: Buf = .{ .a = arena };
    try writeExtensionsMd(&md, rows);
    const md_result = try writeIfChanged(io, out_dir, "EXTENSIONS.md", md.items());
    stats.note(md_result.changed, md_result.bytes);
    try log.pushFmt(
        "EXTENSIONS.md bytes={d} hash={s}\n",
        .{ md_result.bytes, md_result.tag[0..] },
    );
    try log.pushFmt(
        "total files={d} written={d} unchanged={d} bytes={d}\n",
        .{ fixtures.len * 2 + 1, stats.files_written, stats.files_unchanged, stats.bytes_written },
    );

    var stderr_buffer: [4096]u8 = undefined;
    var stderr = std.Io.File.stderr().writer(io, &stderr_buffer);
    const err = &stderr.interface;
    try err.writeAll(log.items());
    try err.flush();

    if (log_path) |path| {
        if (std.fs.path.dirname(path)) |dir| try std.Io.Dir.cwd().createDirPath(io, dir);
        try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = path, .data = log.items() });
    }
}

// ---------------------------------------------------------------------------
// Tests. Keep every assertion inside this file: these are pure in-memory
// checks against the fixture table and the renderers.

test "fixtures table enumerates 21 distinct extensions" {
    var seen = std.StringHashMap(void).init(std.testing.allocator);
    defer seen.deinit();
    try std.testing.expectEqual(@as(usize, 21), fixtures.len);
    for (fixtures) |fx| {
        const entry = try seen.getOrPut(fx.ext);
        try std.testing.expect(!entry.found_existing);
    }
}

test "project XML round-trips the composer save name as the root tag" {
    const xml = try renderProject(std.testing.allocator, "Weapon_Composer_Project");
    defer std.testing.allocator.free(xml);
    try std.testing.expect(std.mem.startsWith(u8, xml, "<?xml version=\"1.0\"?>\r\n"));
    try std.testing.expect(std.mem.indexOf(u8, xml, "<Weapon_Composer_Project>") != null);
    try std.testing.expect(std.mem.indexOf(u8, xml, "</Weapon_Composer_Project>") != null);
    try std.testing.expect(std.mem.indexOf(u8, xml, "<fixture>") != null);
    try std.testing.expect(std.mem.indexOf(u8, xml, "<name>minimal</name>") != null);
    try std.testing.expect(std.mem.indexOf(u8, xml, "\n") != null);
    try std.testing.expect(std.mem.indexOf(u8, xml, "\r\n") != null);
    var i: usize = 0;
    while (i < xml.len) : (i += 1) {
        if (xml[i] == '\n') try std.testing.expect(i > 0 and xml[i - 1] == '\r');
    }
}

test "every art generator stays under 4 KB and is deterministic" {
    const limit = 4 * 1024;
    for (fixtures) |fx| {
        const project = try renderProject(std.testing.allocator, fx.composer_save_name);
        defer std.testing.allocator.free(project);
        try std.testing.expect(project.len < limit);
        const art_a = try renderArt(std.testing.allocator, fx.art, fx.ext);
        defer std.testing.allocator.free(art_a);
        try std.testing.expect(art_a.len < limit);
        const art_b = try renderArt(std.testing.allocator, fx.art, fx.ext);
        defer std.testing.allocator.free(art_b);
        try std.testing.expectEqualSlices(u8, art_a, art_b);
    }
}

test "solid-colour TGA has a valid 18-byte header and 16x16 24-bit pixel block" {
    const bytes = try renderArt(std.testing.allocator, .picture, "wpn");
    defer std.testing.allocator.free(bytes);
    try std.testing.expectEqual(@as(usize, 18 + 16 * 16 * 3), bytes.len);
    try std.testing.expectEqual(@as(u8, 2), bytes[2]);
    try std.testing.expectEqual(@as(u16, 16), std.mem.readInt(u16, bytes[12..14], .little));
    try std.testing.expectEqual(@as(u16, 16), std.mem.readInt(u16, bytes[14..16], .little));
    try std.testing.expectEqual(@as(u8, 24), bytes[16]);
}

test "mesh OBJ has 8 vertices and 12 triangle faces, CRLF-terminated" {
    const bytes = try renderArt(std.testing.allocator, .mesh, "msh");
    defer std.testing.allocator.free(bytes);
    var v_count: usize = 0;
    var f_count: usize = 0;
    var it = std.mem.splitSequence(u8, bytes, "\r\n");
    while (it.next()) |line| {
        if (std.mem.startsWith(u8, line, "v ")) v_count += 1;
        if (std.mem.startsWith(u8, line, "f ")) f_count += 1;
    }
    try std.testing.expectEqual(@as(usize, 8), v_count);
    try std.testing.expectEqual(@as(usize, 12), f_count);
}

test "particle keyframes have exactly 2 data lines" {
    const bytes = try renderArt(std.testing.allocator, .particle, "pcp");
    defer std.testing.allocator.free(bytes);
    var data_lines: usize = 0;
    var it = std.mem.splitSequence(u8, bytes, "\r\n");
    while (it.next()) |line| {
        if (line.len == 0 or line[0] == '#') continue;
        data_lines += 1;
    }
    try std.testing.expectEqual(@as(usize, 2), data_lines);
}
