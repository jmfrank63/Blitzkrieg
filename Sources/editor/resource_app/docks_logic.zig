//! What the docks and the Help menu decide (06-PARITY A-14, A-15, A-25,
//! A-26, A-35, with the Import UI of A-36 and the preview background of
//! A-37): MFC's CDirectionButton geometry and its angle text, CThumbList's
//! picture filter and thumbnail fit, when the preview scene is begun and
//! stopped, the Import form, the Help shortcut list and the About lines.
//! Pure, tested in test-resource-app-logic through FakeResBridge; docks.zig
//! only draws and forwards input.
const std = @import("std");
const core = @import("resource_core");
const logic = @import("panels_logic.zig");
const grid = @import("grid_logic.zig");

const tools = core.sub_editor_tools;
const bridge = core.bridge;
const Kind = bridge.Kind;
const ResBridge = bridge.ResBridge;
const PathText = logic.PathText;
const testing = std.testing;

// --- Direction button (CDirectionButton, DirectionButton.cpp) -------------

/// The angle a click at (x, y) inside a w x h button sets: OnLButtonDown and
/// OnMouseMove measure from the client centre with y up, atan2(cy, cx), so
/// the angle runs -pi..pi, 0 pointing right.
pub fn directionAngleAt(x: f32, y: f32, w: f32, h: f32) f32 {
    const cx = x - @trunc(w / 2);
    const cy = @trunc(h / 2) - y;
    return std.math.atan2(cy, cx);
}

/// Where OnPaint's needle ends, relative to the button's top left: the
/// shorter half-side times the angle's cosine across and half its sine up,
/// a circle squashed to the game's 2:1 ground, with MFC's int truncation.
pub fn directionNeedleEnd(angle: f32, w: f32, h: f32) struct { x: f32, y: f32 } {
    const half_w = @trunc(w / 2);
    const half_h = @trunc(h / 2);
    const radius = @min(half_w, half_h);
    const dx = @trunc(radius * @cos(angle));
    const dy = @trunc(radius * @sin(angle) / 2);
    return .{ .x = dx + half_w, .y = half_h - dy };
}

/// The degrees OnPaint writes in the button's corner ("%.2f"): the angle
/// taken into 0..2pi, then turned a quarter-pi back, so the game's 2:1
/// view's "up-right" diagonal reads as 0.
pub fn directionDegrees(angle: f32) f32 {
    const pi = std.math.pi;
    var a = angle;
    a = if (a < 0) 2 * pi - @abs(a) else a;
    a = if (a > pi / 4.0) a - pi / 4.0 else 2 * pi - (pi / 4.0 - a);
    return a * 180.0 / pi;
}

/// GetQuadrant, line for line, for the sub-editors that ask it (06-06,
/// 06-08, 06-12). Its 3 and 4 branches cannot match an atan2 angle; they
/// are kept as MFC wrote them.
pub fn directionQuadrant(angle: f32) u3 {
    const pi = std.math.pi;
    if (angle >= 0 and angle < pi / 4.0) return 0;
    if (angle >= pi / 4.0 and angle < pi / 2.0) return 1;
    if (angle >= pi / 2.0 and angle < 1.5 * pi) return 2;
    if (angle >= 1.5 * pi and angle <= pi) return 3;
    if (angle <= 0 and angle > -pi / 4.0) return 7;
    if (angle <= pi / 4.0 and angle > -pi / 2.0) return 6;
    if (angle <= pi / 2.0 and angle > -1.5 * pi) return 5;
    if (angle <= 1.5 * pi and angle > pi) return 4;
    return 0;
}

// --- Thumbnail list (CThumbList, ThumbList.cpp) ----------------------------

/// MFC's thumbnails were 100 x 100; the kit's picture cache decodes at most
/// 64 on a side, so a cell here is 64 and the picture is fitted into it the
/// way LoadImageToImageList did (scaled by the smaller of the two rates, up
/// or down, and centred on black).
pub const thumbnail_side: f32 = 64;

/// LoadAllImagesFromDir listed `*.tga`; Windows matched that without regard
/// to case, and so does this.
pub fn isThumbnailPicture(name: []const u8) bool {
    const ext = std.fs.path.extension(name);
    return std.ascii.eqlIgnoreCase(ext, ".tga") and name.len > ext.len;
}

/// The path BkEditorMinimapImage decodes `<folder>/<name>` through: it takes
/// a "<base>.xml" path and reads "<base>.tga" with the engine's own image
/// decoders (the ones CThumbList used, IImageProcessor::LoadImage). Null
/// for a name whose extension is not exactly ".tga" (the engine opens the
/// lower-case name, which a case-sensitive file system would not find) or a
/// path that does not fit.
pub fn thumbnailDecodePath(buffer: []u8, folder: []const u8, name: []const u8) ?[:0]const u8 {
    if (!std.mem.endsWith(u8, name, ".tga") or name.len == ".tga".len) return null;
    const stem = name[0 .. name.len - ".tga".len];
    const sep: []const u8 = if (folder.len == 0 or folder[folder.len - 1] == '/' or folder[folder.len - 1] == '\\') "" else "/";
    return std.fmt.bufPrintZ(buffer, "{s}{s}{s}.xml", .{ folder, sep, stem }) catch null;
}

/// The name a frame item takes from a listed picture (CThumbList's item
/// text is the file name without its extension; the exporters add ".tga"
/// back when they look for the frame).
pub fn frameNameOf(picture: []const u8) []const u8 {
    const ext = std.fs.path.extension(picture);
    return picture[0 .. picture.len - ext.len];
}

/// The animation a double-clicked picture joins (AnimationFrm's
/// m_pActiveAnimation): the selected animation, or the animation of the
/// selected frame, else the first animation of the tree.
pub fn activeAnimation(doc: *const core.document.Document, selected: ?i32) ?i32 {
    if (selected) |id| if (tools.findNode(doc, id)) |node| {
        if (tools.isClass(node, tools.item_type.unit_animation_props)) return id;
        if (tools.isClass(node, tools.item_type.unit_frame_props)) return node.parent;
    };
    return tools.firstOfClass(doc, tools.item_type.unit_animation_props);
}

/// Whether the fence insert type already has a segment of this name.
fn fenceSegmentNamed(doc: *const core.document.Document, insert: i32, name: []const u8) bool {
    for (doc.tree.nodes.items) |*node| {
        if (node.parent == insert and tools.isClass(node, tools.item_type.fence_props) and std.mem.eql(u8, node.displaySlice(), name)) return true;
    }
    return false;
}

/// A double-click on a listed picture (CThumbList's WM_THUMB_LIST_DBLCLK):
/// a sprite takes a frame under its Sprites item, an infantry project one
/// under the active animation, a fence a segment under the active insert type. One undo step; a kind without frames, a
/// read-only project and a picture that does not fit are refused.
pub fn addFrameFromPicture(gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, selected: ?i32, picture: []const u8) bridge.EditError!void {
    if (!life.is_open or life.read_only) return error.Refused;
    const name = frameNameOf(picture);
    const command = switch (life.doc.kind) {
        .sprite => try tools.spriteAddFrame(gpa, &life.doc, tools.firstOfClass(&life.doc, tools.item_type.sprites) orelse return error.Refused, name),
        .animation_infantry => try tools.infantryAddFrame(gpa, &life.doc, activeAnimation(&life.doc, selected) orelse return error.Refused, name),
        .fence => blk: {
            const insert = grid.activeInsert(&life.doc, selected) orelse return error.Refused;
            // FenceFrm skips a picture the insert type already lists.
            if (fenceSegmentNamed(&life.doc, insert, name)) return error.Refused;
            break :blk try tools.fenceAddSegment(gpa, &life.doc, insert, name);
        },
        else => return error.Refused,
    };
    try tools.commit(gpa, b, &life.doc, &life.history, command, 0);
}

/// The thumbnail list's Delete (WM_THUMB_LIST_DELETE, DeleteFrameInTree):
/// the selected frame goes, as one undo step.
pub fn deleteSelectedFrame(gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, selected: ?i32) bridge.EditError!void {
    if (!life.is_open or life.read_only) return error.Refused;
    const command = try tools.deleteFrame(&life.doc, selected orelse return error.Refused);
    try tools.commit(gpa, b, &life.doc, &life.history, command, 0);
}

/// The rectangle a w x h picture takes inside a `side` square cell.
pub const Fit = struct { x: f32, y: f32, w: f32, h: f32 };

pub fn fitThumbnail(w: f32, h: f32, side: f32) Fit {
    if (w <= 0 or h <= 0) return .{ .x = 0, .y = 0, .w = 0, .h = 0 };
    const rate = @min(side / w, side / h);
    const fw = w * rate;
    const fh = h * rate;
    return .{ .x = (side - fw) / 2, .y = (side - fh) / 2, .w = fw, .h = fh };
}

/// Names in the order the list shows them: case-insensitively by name,
/// the order NTFS gave FindFirstFile, whatever the host file system does.
pub fn sortThumbnailNames(names: [][]const u8) void {
    std.mem.sort([]const u8, names, {}, struct {
        fn lessThan(_: void, a: []const u8, b: []const u8) bool {
            return std.ascii.orderIgnoreCase(a, b) == .lt;
        }
    }.lessThan);
}

// --- Preview background (D-16, BkResPreview*) ------------------------------

/// The preview scene behind the windows: begun for the open project's kind,
/// stopped when no project is open, and run on request (MFC's Run button,
/// F5). The bridge decides which kinds have a preview, so a kind without one
/// is told by its refusal, and its message is what the window shows.
pub const PreviewSync = struct {
    /// The kind the scene was begun for, or null when there is none.
    begun: ?Kind = null,
    /// The kind the last Begin was asked for, refused or not: a refused
    /// kind is not asked again every frame.
    asked: ?Kind = null,
    running: bool = false,
    message_buffer: [256]u8 = undefined,
    message_len: usize = 0,

    pub const Change = enum { none, begun, refused, stopped };

    /// Call once per frame with the lifecycle's state.
    pub fn sync(self: *PreviewSync, b: ResBridge, open: bool, kind: Kind) Change {
        if (!open) {
            if (self.asked == null) return .none;
            self.stop(b);
            self.say("no project is open", .{});
            return .stopped;
        }
        if (self.asked == kind) return .none;
        self.asked = kind;
        self.running = false;
        switch (b.previewBegin(kind)) {
            .ok => {
                self.begun = kind;
                self.say("preview of .{s} ready: Run (F5) shows the project", .{kind.extension()});
                return .begun;
            },
            else => {
                // A refused Begin leaves no scene (BeginPreview stops the
                // old one only once it accepts the kind), so stop it here.
                if (self.begun != null) _ = b.previewStop();
                self.begun = null;
                self.say("no preview: {s}", .{b.lastMessage()});
                return .refused;
            },
        }
    }

    /// Run (F5): export the project into the preview, build it and start
    /// its animation through the bridge, as MFC's Run button did.
    pub fn run(self: *PreviewSync, b: ResBridge) bool {
        if (self.begun == null) {
            if (self.message_len == 0) self.say("no preview for this project", .{});
            return false;
        }
        if (b.previewShow() != .ok) {
            self.running = false;
            self.say("the preview was not shown: {s}", .{b.lastMessage()});
            return false;
        }
        self.running = true;
        self.say("showing the project: {s}", .{b.lastMessage()});
        if (b.previewPlayback(true) != .ok) {
            self.running = false;
            self.say("the animation did not start: {s}", .{b.lastMessage()});
            return false;
        }
        return true;
    }

    /// Stop: the animation stops, the scene stays begun.
    pub fn halt(self: *PreviewSync, b: ResBridge) void {
        if (!self.running) return;
        _ = b.previewPlayback(false);
        self.running = false;
    }

    /// Tears the scene down; at exit too, before the engine's modules unload.
    pub fn stop(self: *PreviewSync, b: ResBridge) void {
        if (self.begun != null) _ = b.previewStop();
        self.begun = null;
        self.asked = null;
        self.running = false;
    }

    pub fn message(self: *const PreviewSync) []const u8 {
        return self.message_buffer[0..self.message_len];
    }

    fn say(self: *PreviewSync, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.message_buffer, format, args) catch self.message_buffer[0..];
        self.message_len = text.len;
    }
};

/// The Camera button (CParticleFrame::OnButtonCamera): the flag flips only
/// when the engine took the change, so a closed preview keeps the old camera.
pub fn toggledCamera(b: ResBridge, horizontal: bool) bool {
    return if (b.previewCameraMode(!horizontal) == .ok) !horizontal else horizontal;
}

// --- Particle info (CParticleFrame::GetParticleInfo, OnUpdateStatusBar) ----

pub const ParticleInfo = bridge.ParticleInfo;

/// printf's "%g" (six significant digits, trailing zeros dropped, scientific
/// notation below 1e-4 and from 1e6 up), which MFC's status bar panes used.
pub fn formatG(buffer: []u8, value: f64) []const u8 {
    if (std.math.isNan(value)) return std.fmt.bufPrint(buffer, "nan", .{}) catch buffer[0..0];
    if (std.math.isInf(value)) return std.fmt.bufPrint(buffer, "{s}inf", .{if (value < 0) "-" else ""}) catch buffer[0..0];
    if (value == 0) return std.fmt.bufPrint(buffer, "0", .{}) catch buffer[0..0];
    // The exponent after rounding to six digits decides the style, as in C.
    var probe: [48]u8 = undefined;
    const scientific = std.fmt.bufPrint(&probe, "{e:.5}", .{value}) catch return buffer[0..0];
    const e_at = std.mem.indexOfScalar(u8, scientific, 'e') orelse return buffer[0..0];
    const exponent = std.fmt.parseInt(i32, scientific[e_at + 1 ..], 10) catch return buffer[0..0];
    var text: [64]u8 = undefined;
    var len: usize = 0;
    if (exponent < -4 or exponent >= 6) {
        const mantissa = trimZeros(scientific[0..e_at]);
        const sign: u8 = if (exponent < 0) '-' else '+';
        const out = std.fmt.bufPrint(&text, "{s}e{c}{d:0>2}", .{ mantissa, sign, @abs(exponent) }) catch return buffer[0..0];
        len = out.len;
    } else {
        const decimals: usize = @intCast(5 - exponent);
        const out = std.fmt.bufPrint(&text, "{d:.[1]}", .{ value, decimals }) catch return buffer[0..0];
        len = trimZeros(out).len;
    }
    const n = @min(len, buffer.len);
    @memcpy(buffer[0..n], text[0..n]);
    return buffer[0..n];
}

fn trimZeros(text: []const u8) []const u8 {
    if (std.mem.indexOfScalar(u8, text, '.') == null) return text;
    var end = text.len;
    while (end > 0 and text[end - 1] == '0') end -= 1;
    if (end > 0 and text[end - 1] == '.') end -= 1;
    return text[0..end];
}

/// The four status bar panes in MFC's order and wording.
pub fn infoPanes(buffers: *[4][48]u8, info: ParticleInfo) [4][]const u8 {
    const labels = [4][]const u8{ "Max particles", "Size", "Average size", "Average count" };
    const values = [4]f32{ info.max_count, info.max_size, info.average_size, info.average_count };
    var panes: [4][]const u8 = undefined;
    for (0..4) |i| {
        var number: [32]u8 = undefined;
        panes[i] = std.fmt.bufPrint(&buffers[i], "{s} {s}", .{ labels[i], formatG(&number, values[i]) }) catch buffers[i][0..0];
    }
    return panes;
}

/// The one-line status bar text: the four panes side by side.
pub fn infoLine(buffer: []u8, info: ParticleInfo) []const u8 {
    var panes_buffers: [4][48]u8 = undefined;
    const panes = infoPanes(&panes_buffers, info);
    return std.fmt.bufPrint(buffer, "{s}  |  {s}  |  {s}  |  {s}", .{ panes[0], panes[1], panes[2], panes[3] }) catch buffer[0..0];
}

/// The Get particle info button's state (m_fNumberOfParticles and its
/// siblings): the numbers of the last successful press, kept until the next
/// press or until the project is no longer a Particle one.
pub const ParticleStatus = struct {
    info: ?ParticleInfo = null,
    note_buffer: [256]u8 = undefined,
    note_len: usize = 0,

    /// The button (OnGetParticleInfo). A refusal leaves the old numbers and
    /// puts the bridge's reason in the note.
    pub fn press(self: *ParticleStatus, b: ResBridge) bool {
        var info: ParticleInfo = .{};
        const status = b.particleInfo(&info);
        if (status != .ok) {
            const text = std.fmt.bufPrint(&self.note_buffer, "particle info: {s}", .{b.lastMessage()}) catch self.note_buffer[0..];
            self.note_len = text.len;
            return false;
        }
        self.info = info;
        self.note_len = 0;
        return true;
    }

    pub fn clear(self: *ParticleStatus) void {
        self.info = null;
        self.note_len = 0;
    }

    pub fn note(self: *const ParticleStatus) []const u8 {
        return self.note_buffer[0..self.note_len];
    }
};

// --- Import (A-36: Ctrl+I, ID_IMPORT_XML_FILE had no handler in MFC) -------

/// The Import window's choices: the kind to build and the runtime folder
/// holding its 1.xml. Only infantry imports today; the other kinds go to the
/// bridge too, so its refusal (naming the kind) is what the user reads.
pub const ImportForm = struct {
    kind: Kind = .animation_infantry,
    folder: [logic.path_capacity]u8 = [_]u8{0} ** logic.path_capacity,

    pub fn folderSlice(self: *const ImportForm) []const u8 {
        return std.mem.sliceTo(&self.folder, 0);
    }

    pub fn setFolder(self: *ImportForm, text: []const u8) bool {
        if (text.len >= self.folder.len) return false;
        @memset(&self.folder, 0);
        @memcpy(self.folder[0..text.len], text);
        return true;
    }

    /// The guarded action Import asks the lifecycle for, or why not.
    pub fn request(self: *const ImportForm) union(enum) { go: logic.Pending, why_not: []const u8 } {
        const folder = std.mem.trim(u8, self.folderSlice(), " \t");
        if (folder.len == 0) return .{ .why_not = "choose the game folder that holds the resource's 1.xml" };
        const text = PathText.fromSlice(folder) orelse return .{ .why_not = "the folder's path is too long" };
        return .{ .go = .{ .import_from_game = .{ .kind = self.kind, .folder = text } } };
    }
};

/// The folder the Import window starts in: the installation's Data folder,
/// where the runtime resources it reads sit.
pub fn importStartFolder(buffer: []u8, base_root: []const u8) []const u8 {
    const sep: []const u8 = if (base_root.len == 0 or base_root[base_root.len - 1] == '/' or base_root[base_root.len - 1] == '\\') "" else "/";
    return std.fmt.bufPrint(buffer, "{s}{s}Data", .{ base_root, sep }) catch "";
}

// --- Help and About (A-25, A-26) ------------------------------------------

/// One line of Help's shortcut list. `keys` names Ctrl, which macOS shows
/// as Cmd (`shortcutKeys`).
pub const Shortcut = struct { keys: []const u8, action: []const u8 };

/// The keys the editor answers today, MFC's accelerators (editor.rc,
/// IDR_EDITORTYPE) where it had them. reshelp.chm is not in the repository,
/// so this list and the spec are the help.
pub const shortcuts = [_]Shortcut{
    .{ .keys = "Ctrl+O", .action = "Open a project" },
    .{ .keys = "Ctrl+S", .action = "Save the project" },
    .{ .keys = "Ctrl+Shift+S", .action = "Save the project as" },
    .{ .keys = "Ctrl+I", .action = "Import from game data" },
    .{ .keys = "Ctrl+Z", .action = "Undo" },
    .{ .keys = "Ctrl+Y, Ctrl+Shift+Z", .action = "Redo" },
    .{ .keys = "Insert", .action = "Insert an item (project tree)" },
    .{ .keys = "Delete", .action = "Delete the selected items (project tree)" },
    .{ .keys = "F2", .action = "Rename the item (project tree)" },
    .{ .keys = "Ctrl+D", .action = "Show or hide the direction button" },
    .{ .keys = "Ctrl+F", .action = "Show or hide the function window" },
    .{ .keys = "F5", .action = "Run the preview" },
    .{ .keys = "F1", .action = "This help" },
};

/// `keys` as the platform names the modifier.
pub fn shortcutKeys(buffer: []u8, keys: []const u8, macos: bool) []const u8 {
    if (!macos) return keys;
    const size = std.mem.replacementSize(u8, keys, "Ctrl", "Cmd");
    if (size > buffer.len) return keys;
    _ = std.mem.replace(u8, keys, "Ctrl", "Cmd", buffer[0..size]);
    return buffer[0..size];
}

pub const spec_path = "docs/superpowers/specs/2026-09-30-portable-resource-editor-design.md";
pub const spec_url = "https://github.com/jmfrank63/Blitzkrieg/blob/main/" ++ spec_path;

/// IDD_ABOUTBOX's lines, then the port's own (as MapEditor's About has).
pub const about_title = "Blitzkrieg Resource Editor";
pub const about_mfc_lines = [_][]const u8{
    "Version 1.0",
    "\xc2\xa9 2003 Nival Interactive. All rights reserved.",
    "Blitzkrieg is a trademark of Nival Interactive.",
    "Published by CDV Software Entertainment AG.",
};
pub const about_port = "Portable editor (Zig, Dear ImGui, the engine through resource_bridge.h), milestone M001";
pub const about_spec = "Design: " ++ spec_path;
pub const about_source = "Source: github.com/jmfrank63/Blitzkrieg";
pub const about_license = "Blitzkrieg and its data belong to Nival International Ltd.; use is licensed for noncommercial purposes only (LICENSE.md).";

/// The Function window's frame (A-15): MFC's CKeyFrameDockWnd edits a
/// particle or effect track's keys; the editing is the curve widget in docks.zig over keyframe_logic.zig.
pub const function_window_note = "Select a key-frame curve of a Particle or Effect project to edit it here.";

// --- Tests -------------------------------------------------------------------

const FakeResBridge = core.fake_bridge.FakeResBridge;

test "direction button: a click sets atan2 from the centre with y up, as OnLButtonDown" {
    try testing.expectApproxEqAbs(@as(f32, 0), directionAngleAt(90, 40, 80, 80), 1e-6);
    try testing.expectApproxEqAbs(@as(f32, std.math.pi / 2.0), directionAngleAt(40, 0, 80, 80), 1e-6);
    try testing.expectApproxEqAbs(@as(f32, std.math.pi), directionAngleAt(0, 40, 80, 80), 1e-6);
    try testing.expectApproxEqAbs(@as(f32, -std.math.pi / 2.0), directionAngleAt(40, 80, 80, 80), 1e-6);
}

test "direction button: the needle is the shorter half-side across and half of it up, truncated" {
    const right = directionNeedleEnd(0, 100, 80);
    try testing.expectEqual(@as(f32, 90), right.x);
    try testing.expectEqual(@as(f32, 40), right.y);
    const up = directionNeedleEnd(std.math.pi / 2.0, 100, 80);
    try testing.expectEqual(@as(f32, 50), up.x);
    try testing.expectEqual(@as(f32, 20), up.y);
    const down_wide = directionNeedleEnd(-std.math.pi / 2.0, 60, 100);
    try testing.expectEqual(@as(f32, 30), down_wide.x);
    try testing.expectEqual(@as(f32, 65), down_wide.y);
}

test "direction button: the degrees text turns the angle a quarter-pi back into 0..360" {
    try testing.expectApproxEqAbs(@as(f32, 0), directionDegrees(std.math.pi / 4.0 + 1e-6), 1e-3);
    try testing.expectApproxEqAbs(@as(f32, 315), directionDegrees(0), 1e-3);
    try testing.expectApproxEqAbs(@as(f32, 45), directionDegrees(std.math.pi / 2.0), 1e-3);
    try testing.expectApproxEqAbs(@as(f32, 225), directionDegrees(-std.math.pi / 2.0), 1e-3);
}

test "direction button: GetQuadrant as MFC wrote it" {
    try testing.expectEqual(@as(u3, 0), directionQuadrant(0.1));
    try testing.expectEqual(@as(u3, 1), directionQuadrant(1.0));
    try testing.expectEqual(@as(u3, 2), directionQuadrant(3.0));
    try testing.expectEqual(@as(u3, 2), directionQuadrant(std.math.pi));
    try testing.expectEqual(@as(u3, 7), directionQuadrant(-0.5));
    try testing.expectEqual(@as(u3, 6), directionQuadrant(-1.0));
    try testing.expectEqual(@as(u3, 5), directionQuadrant(-2.0));
}

test "thumbnails: *.tga in any case, a decode path only for the engine's lower-case name" {
    try testing.expect(isThumbnailPicture("frame1.tga"));
    try testing.expect(isThumbnailPicture("FRAME1.TGA"));
    try testing.expect(!isThumbnailPicture("frame1.dds"));
    try testing.expect(!isThumbnailPicture(".tga"));
    var buffer: [64]u8 = undefined;
    try testing.expectEqualStrings("/a/b/frame1.xml", thumbnailDecodePath(&buffer, "/a/b", "frame1.tga").?);
    try testing.expectEqualStrings("/a/b/frame1.xml", thumbnailDecodePath(&buffer, "/a/b/", "frame1.tga").?);
    try testing.expect(thumbnailDecodePath(&buffer, "/a/b", "FRAME1.TGA") == null);
    var tiny: [8]u8 = undefined;
    try testing.expect(thumbnailDecodePath(&tiny, "/a/b", "frame1.tga") == null);
}

test "thumbnails: a picture is fitted by the smaller rate and centred, up or down" {
    const wide = fitThumbnail(32, 16, 64);
    try testing.expectEqual(Fit{ .x = 0, .y = 16, .w = 64, .h = 32 }, wide);
    const tall = fitThumbnail(100, 200, 64);
    try testing.expectEqual(Fit{ .x = 16, .y = 0, .w = 32, .h = 64 }, tall);
    try testing.expectEqual(Fit{ .x = 0, .y = 0, .w = 64, .h = 64 }, fitThumbnail(16, 16, 64));
    try testing.expectEqual(Fit{ .x = 0, .y = 0, .w = 0, .h = 0 }, fitThumbnail(0, 16, 64));
}

test "thumbnails: a frame is named after its picture without the extension" {
    try testing.expectEqualStrings("walk_01", frameNameOf("walk_01.tga"));
    try testing.expectEqualStrings("a.b", frameNameOf("a.b.TGA"));
    try testing.expectEqualStrings("plain", frameNameOf("plain"));
}

/// A fake project of `kind` with `classes` inserted under the root, each
/// under the one before it (parent chain), and the lifecycle adopting it.
fn framesRig(fake: *FakeResBridge, life: *logic.Lifecycle, kind: Kind, classes: []const i32) ![4]i32 {
    const b = fake.bridge();
    try life.newProject(testing.allocator, b, kind);
    var ids = [_]i32{ fake.nodes.items[0].id, 0, 0, 0 };
    var parent = ids[0];
    for (classes, 1..) |class_type, i| {
        var name: [16]u8 = undefined;
        try bridge.check(b.insertNode(parent, try std.fmt.bufPrint(&name, "{d}", .{class_type}), 0, &ids[i]));
        parent = ids[i];
    }
    try life.doc.reload(testing.allocator, b);
    return ids;
}

test "thumbnails: a double-click adds a named frame, Delete removes it, both undoable" {
    const item = tools.item_type;
    var fake = FakeResBridge.init(testing.allocator);
    defer fake.deinit();
    var life: logic.Lifecycle = .{};
    defer life.deinit(testing.allocator);
    const ids = try framesRig(&fake, &life, .sprite, &.{item.sprites});
    const b = fake.bridge();

    try addFrameFromPicture(testing.allocator, b, &life, null, "walk_03.tga");
    const frame = tools.childOfClass(&life.doc, ids[1], item.sprite_props, 0).?;
    try testing.expectEqualStrings("walk_03", tools.findNode(&life.doc, frame).?.displaySlice());
    try testing.expect(life.dirty());
    try deleteSelectedFrame(testing.allocator, b, &life, frame);
    try testing.expectEqual(@as(i32, 0), tools.childCount(&life.doc, ids[1]));
    try testing.expectError(error.Refused, deleteSelectedFrame(testing.allocator, b, &life, ids[1]));
    try testing.expectError(error.Refused, deleteSelectedFrame(testing.allocator, b, &life, null));
    try testing.expectEqual(@as(usize, 2), life.history.undo_stack.items.len);
    try life.doc.undoOne(testing.allocator, b, &life.history.undo_stack.items[1].command);
    try testing.expectEqual(@as(i32, 1), tools.childCount(&life.doc, ids[1]));
}

test "thumbnails: an infantry frame goes under the selected animation, else the first; other kinds and read-only refuse" {
    const item = tools.item_type;
    var fake = FakeResBridge.init(testing.allocator);
    defer fake.deinit();
    var life: logic.Lifecycle = .{};
    defer life.deinit(testing.allocator);
    const ids = try framesRig(&fake, &life, .animation_infantry, &.{ item.unit_animations, item.unit_animation_props });
    const b = fake.bridge();
    const second = blk: {
        var id: i32 = 0;
        try bridge.check(b.insertNode(ids[1], "285212683", 1, &id));
        break :blk id;
    };
    try life.doc.reload(testing.allocator, b);

    try addFrameFromPicture(testing.allocator, b, &life, null, "a.tga");
    try testing.expectEqual(@as(i32, 1), tools.childCount(&life.doc, ids[2]));
    try addFrameFromPicture(testing.allocator, b, &life, second, "b.tga");
    try testing.expectEqual(@as(i32, 1), tools.childCount(&life.doc, second));
    // A selected frame stands for its animation.
    const frame = tools.childOfClass(&life.doc, ids[2], item.unit_frame_props, 0).?;
    try addFrameFromPicture(testing.allocator, b, &life, frame, "c.tga");
    try testing.expectEqual(@as(i32, 2), tools.childCount(&life.doc, ids[2]));

    life.read_only = true;
    try testing.expectError(error.Refused, addFrameFromPicture(testing.allocator, b, &life, null, "d.tga"));
    life.read_only = false;
    life.doc.kind = .weapon;
    try testing.expectError(error.Refused, addFrameFromPicture(testing.allocator, b, &life, null, "d.tga"));
}

test "thumbnails: names sort without regard to case" {
    var names = [_][]const u8{ "b.tga", "A.tga", "c.TGA", "a2.tga" };
    sortThumbnailNames(&names);
    try testing.expectEqualStrings("A.tga", names[0]);
    try testing.expectEqualStrings("a2.tga", names[1]);
    try testing.expectEqualStrings("b.tga", names[2]);
    try testing.expectEqualStrings("c.TGA", names[3]);
}

test "preview: begun once per kind, stopped with no project, Run needs a begun scene" {
    var fake = FakeResBridge.init(testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var preview: PreviewSync = .{};

    try testing.expectEqual(PreviewSync.Change.none, preview.sync(b, false, .weapon));
    try testing.expect(!preview.run(b));
    try testing.expectEqual(PreviewSync.Change.begun, preview.sync(b, true, .sprite));
    try testing.expectEqual(Kind.sprite, preview.begun.?);
    try testing.expectEqual(PreviewSync.Change.none, preview.sync(b, true, .sprite));

    // Run with no project in the fake is the bridge's refusal, shown.
    try testing.expect(!preview.run(b));
    try testing.expect(std.mem.indexOf(u8, preview.message(), "not shown") != null);
    try testing.expectEqual(bridge.Status.ok, b.new(.sprite));
    try testing.expect(preview.run(b));
    try testing.expect(preview.running);
    try testing.expect(fake.preview_playing);
    preview.halt(b);
    try testing.expect(!preview.running);
    try testing.expect(!fake.preview_playing);
    try testing.expect(preview.run(b));

    try testing.expectEqual(PreviewSync.Change.begun, preview.sync(b, true, .effect));
    try testing.expect(!preview.running);
    try testing.expectEqual(PreviewSync.Change.stopped, preview.sync(b, false, .effect));
    try testing.expect(preview.begun == null);
    try testing.expectEqual(FakeResBridge.PreviewState.closed, fake.preview_state);
}

test "preview: a refused Begin is shown once and not asked again until the kind changes" {
    var fake = FakeResBridge.init(testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var preview: PreviewSync = .{};
    fake.no_device = true;
    try testing.expectEqual(PreviewSync.Change.refused, preview.sync(b, true, .mesh_unit));
    try testing.expect(std.mem.indexOf(u8, preview.message(), "no GPU device") != null);
    try testing.expectEqual(PreviewSync.Change.none, preview.sync(b, true, .mesh_unit));
    try testing.expect(!preview.run(b));
    fake.no_device = false;
    try testing.expectEqual(PreviewSync.Change.begun, preview.sync(b, true, .particle));
}

test "camera: the button flips the camera, and a closed preview keeps it" {
    var fake = FakeResBridge.init(testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    try testing.expect(!toggledCamera(b, false));
    try testing.expect(!fake.preview_horizontal);
    var preview: PreviewSync = .{};
    _ = preview.sync(b, true, .particle);
    try testing.expect(toggledCamera(b, false));
    try testing.expect(fake.preview_horizontal);
    try testing.expect(!toggledCamera(b, true));
    try testing.expect(!fake.preview_horizontal);
}

test "import: an empty folder is refused before the bridge, a chosen one is the guarded action" {
    var form: ImportForm = .{};
    switch (form.request()) {
        .why_not => |why| try testing.expect(std.mem.indexOf(u8, why, "1.xml") != null),
        .go => return error.TestUnexpectedResult,
    }
    try testing.expect(form.setFolder("  /game/Data/Units/Humans/German/Gunner "));
    switch (form.request()) {
        .go => |pending| {
            try testing.expectEqual(Kind.animation_infantry, pending.import_from_game.kind);
            try testing.expectEqualStrings("/game/Data/Units/Humans/German/Gunner", pending.import_from_game.folder.slice());
        },
        .why_not => return error.TestUnexpectedResult,
    }
    var long: [logic.path_capacity + 1]u8 = @splat('a');
    try testing.expect(!form.setFolder(&long));
    var buffer: [64]u8 = undefined;
    try testing.expectEqualStrings("/game/Data", importStartFolder(&buffer, "/game/"));
    try testing.expectEqualStrings("/game/Data", importStartFolder(&buffer, "/game"));
}

test "help: every shortcut has keys and an action, Ctrl reads Cmd on macOS, the spec link names the spec" {
    for (shortcuts) |s| {
        try testing.expect(s.keys.len != 0 and s.action.len != 0);
    }
    var buffer: [64]u8 = undefined;
    try testing.expectEqualStrings("Cmd+Y, Cmd+Shift+Z", shortcutKeys(&buffer, "Ctrl+Y, Ctrl+Shift+Z", true));
    try testing.expectEqualStrings("Ctrl+O", shortcutKeys(&buffer, "Ctrl+O", false));
    try testing.expect(std.mem.endsWith(u8, spec_url, spec_path));
    try testing.expectEqualStrings("Version 1.0", about_mfc_lines[0]);
}

test "particle info: %g formatting follows printf" {
    var buffer: [64]u8 = undefined;
    try testing.expectEqualStrings("0", formatG(&buffer, 0));
    try testing.expectEqualStrings("120", formatG(&buffer, 120));
    try testing.expectEqualStrings("0.5", formatG(&buffer, 0.5));
    try testing.expectEqualStrings("123.457", formatG(&buffer, 123.456789));
    try testing.expectEqualStrings("0.0001", formatG(&buffer, 0.0001));
    try testing.expectEqualStrings("1e-05", formatG(&buffer, 0.00001));
    try testing.expectEqualStrings("123457", formatG(&buffer, 123456.7));
    try testing.expectEqualStrings("1e+06", formatG(&buffer, 1000000));
    try testing.expectEqualStrings("1.5e+07", formatG(&buffer, 15000000));
    try testing.expectEqualStrings("-2.25", formatG(&buffer, -2.25));
}

test "particle info: the four panes carry MFC's labels, and a press keeps the numbers or names the refusal" {
    var fake = FakeResBridge.init(testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var status: ParticleStatus = .{};

    // No project: refused, the reason shown, no numbers.
    try testing.expect(!status.press(b));
    try testing.expect(status.info == null);
    try testing.expect(std.mem.indexOf(u8, status.note(), "no project") != null);

    try testing.expectEqual(bridge.Status.ok, b.new(.weapon));
    try testing.expect(!status.press(b));
    try testing.expect(std.mem.indexOf(u8, status.note(), "pcp") != null);

    try testing.expectEqual(bridge.Status.ok, b.close());
    try testing.expectEqual(bridge.Status.ok, b.new(.particle));
    try testing.expect(status.press(b));
    try testing.expectEqual(@as(usize, 0), status.note().len);
    var line: [160]u8 = undefined;
    try testing.expectEqualStrings(
        "Max particles 120  |  Size 0.5  |  Average size 0.25  |  Average count 60",
        infoLine(&line, status.info.?),
    );
    var buffers: [4][48]u8 = undefined;
    const panes = infoPanes(&buffers, status.info.?);
    try testing.expectEqualStrings("Max particles 120", panes[0]);
    try testing.expectEqualStrings("Size 0.5", panes[1]);
    try testing.expectEqualStrings("Average size 0.25", panes[2]);
    try testing.expectEqualStrings("Average count 60", panes[3]);

    // A source that did not build keeps the last numbers and says why.
    fake.particle_info = null;
    try testing.expect(!status.press(b));
    try testing.expect(status.info != null);
    try testing.expect(std.mem.indexOf(u8, status.note(), "no particle source") != null);
    status.clear();
    try testing.expect(status.info == null);
}
