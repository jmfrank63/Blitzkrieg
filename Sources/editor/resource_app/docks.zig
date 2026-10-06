//! The docks around the preview and the Help menu (06-PARITY A-14, A-15,
//! A-25, A-26, A-35, with the Import UI of A-36 and the preview background
//! of A-37): MFC's thumbnail list (CThumbList), direction button
//! (CDirectionButton), function window frame (CKeyFrameDockWnd; its editing
//! is the Particle and Effect editors'), the Import window, Help and About.
//! The preview scene is the window's background: the engine draws it under
//! every ImGui window, so nothing here covers the screen's middle.
//!
//! Every decision is docks_logic.zig's (tested in test-resource-app-logic);
//! this file draws, forwards input and does the I/O: the folder listing, the
//! thumbnails' decode through the engine, the folder dialogs.
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const kit = @import("editor_kit");
const core = @import("resource_core");
const c_bridge = @import("c_bridge.zig");
const logic = @import("panels_logic.zig");
const dl = @import("docks_logic.zig");
const tl = @import("terrain_logic.zig");
const edit = @import("edit_logic.zig");
const mesh_logic = @import("mesh_logic.zig");
const keyframe_logic = @import("keyframe_logic.zig");
const lifecycle = @import("lifecycle.zig");
const il = @import("image_logic.zig");
const lifecycle_ui = @import("lifecycle_ui.zig");

const ig = imgui.c;
const c = c_bridge.c;
const Kind = core.bridge.Kind;
const ResBridge = core.bridge.ResBridge;
const Cache = kit.pictures_cache.Cache;
const macos = builtin.os.tag == .macos;
const mod_label = if (macos) "Cmd" else "Ctrl";

/// How many thumbnails decode per frame: a folder of a sprite's frames is
/// tens of pictures, so it fills within a second without stalling a frame.
const thumbnail_pump_budget = 4;

/// Where a folder dialog's callback writes its answer: SDL may call back on
/// another thread or after the window is gone, so it is a global.
var folder_slot: FolderSlot = .{};

const FolderSlot = struct {
    state: std.atomic.Value(u8) = .init(@intFromEnum(State.idle)),
    target: Target = .thumbnails,
    buffer: [logic.path_capacity]u8 = undefined,
    len: usize = 0,

    const State = enum(u8) { idle, waiting, arrived, cancelled };
    const Target = enum { thumbnails, import, import_terrains, import_crossets };

    fn request(self: *FolderSlot, target: Target) bool {
        if (self.state.cmpxchgStrong(@intFromEnum(State.idle), @intFromEnum(State.waiting), .acquire, .monotonic) != null) return false;
        self.target = target;
        return true;
    }

    fn deliver(self: *FolderSlot, text: ?[]const u8) void {
        if (self.state.load(.acquire) != @intFromEnum(State.waiting)) return;
        const chosen = text orelse {
            self.state.store(@intFromEnum(State.cancelled), .release);
            return;
        };
        if (chosen.len > self.buffer.len) {
            self.state.store(@intFromEnum(State.cancelled), .release);
            return;
        }
        self.len = chosen.len;
        @memcpy(self.buffer[0..self.len], chosen);
        self.state.store(@intFromEnum(State.arrived), .release);
    }

    /// The chosen folder and whose it is, once it arrived; "" is a cancel.
    fn take(self: *FolderSlot) ?struct { target: Target, path: []const u8 } {
        const state: State = @enumFromInt(self.state.load(.acquire));
        const path: []const u8 = switch (state) {
            .idle, .waiting => return null,
            .arrived => self.buffer[0..self.len],
            .cancelled => "",
        };
        self.state.store(@intFromEnum(State.idle), .release);
        return .{ .target = self.target, .path = path };
    }
};

fn folderCallback(userdata: ?*anyopaque, filelist: [*c]const [*c]const u8, filter: c_int) callconv(.c) void {
    _ = filter;
    const slot: *FolderSlot = @ptrCast(@alignCast(userdata orelse return));
    if (filelist == null) return slot.deliver(null);
    const first = filelist[0];
    slot.deliver(if (first != null) std.mem.span(first) else null);
}

/// A rectangle in screen pixels.
pub const Rect = struct { x: f32, y: f32, w: f32, h: f32 };

/// The places the host check pins the docks to, clear of its probe, of its
/// tree window at the top right and of the screen's middle it measures.
pub const fixed_layout = struct {
    pub const thumbnails = Rect{ .x = 40, .y = 150, .w = 240, .h = 200 };
    pub const direction = Rect{ .x = 40, .y = 370, .w = 160, .h = 170 };
    pub const function = Rect{ .x = 40, .y = 560, .w = 360, .h = 150 };
};

/// The image frame's one picture and gesture state.
const ImageFrame = struct {
    overlay: ?il.Overlay = null,
    texture: ?*sdl3.c.SDL_GPUTexture = null,
    device: ?*anyopaque = null,
    width: i32 = 0,
    height: i32 = 0,
    /// The path the picture (or its failure) belongs to.
    key: [logic.path_capacity + 64]u8 = undefined,
    key_len: usize = 0,
    has_key: bool = false,
    note: [320]u8 = undefined,
    note_len: usize = 0,
    /// Where the picture was drawn in the last frame, for the auto tier.
    shown: ?Rect = null,
    /// The Show crosses checkbox as drawn in the last frame, for the auto tier.
    crosses_toggle: ?Rect = null,

    fn overlayFor(self: *ImageFrame, gpa: std.mem.Allocator) *il.Overlay {
        if (self.overlay == null) self.overlay = il.Overlay.init(gpa);
        return &self.overlay.?;
    }

    fn loadedFor(self: *const ImageFrame, path: []const u8) bool {
        return self.has_key and std.mem.eql(u8, self.key[0..self.key_len], path);
    }

    fn remember(self: *ImageFrame, path: []const u8) void {
        const len = @min(path.len, self.key.len);
        @memcpy(self.key[0..len], path[0..len]);
        self.key_len = len;
        self.has_key = true;
    }

    /// The next frame decodes again.
    fn forget(self: *ImageFrame) void {
        self.release();
        self.has_key = false;
        self.note_len = 0;
    }

    fn release(self: *ImageFrame) void {
        if (self.texture) |texture| if (self.device) |device| kit.pictures_cache.releaseTexture(device, texture);
        self.texture = null;
        self.width = 0;
        self.height = 0;
        self.shown = null;
    }

    /// The project is not an image kind any more: nothing of the picture stays.
    fn close(self: *ImageFrame, b: ResBridge) void {
        if (self.overlay) |*overlay| overlay.cancel(b);
        self.overlay = null;
        self.forget();
    }

    fn say(self: *ImageFrame, comptime fmt: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.note, fmt, args) catch self.note[0..];
        self.note_len = text.len;
    }

    fn report(self: *ImageFrame, b: ResBridge, what: []const u8, err: core.bridge.EditError) void {
        const why = b.lastMessage();
        if (why.len != 0) self.say("{s}: {s}", .{ what, why }) else self.say("{s}: {s}", .{ what, @errorName(err) });
    }
};

const Thumbnails = struct {
    /// The folder typed or chosen; empty follows the open project's folder.
    folder: [logic.path_capacity]u8 = [_]u8{0} ** logic.path_capacity,
    /// The folder the names were read from (decodes resolve against it).
    scanned: [logic.path_capacity]u8 = undefined,
    scanned_len: usize = 0,
    scanned_once: bool = false,
    names: std.ArrayListUnmanaged([]u8) = .empty,
    selected: ?usize = null,
    /// The picture MFC's double-click handed to the frame
    /// (WM_THUMB_LIST_DBLCLK); the sub-editors that take one come later.
    activated: ?usize = null,
    cache: Cache,
    note: [256]u8 = undefined,
    note_len: usize = 0,

    fn scannedFolder(self: *const Thumbnails) []const u8 {
        return self.scanned[0..self.scanned_len];
    }

    fn freeNames(self: *Thumbnails, gpa: std.mem.Allocator) void {
        for (self.names.items) |name| gpa.free(name);
        self.names.clearRetainingCapacity();
    }

    fn say(self: *Thumbnails, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.note, format, args) catch self.note[0..];
        self.note_len = text.len;
    }
};

pub const Docks = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    real: *c_bridge.RealResBridge,
    preview: dl.PreviewSync = .{},
    /// The Function window's curve editor for the selected key-frame node, and
    /// the node and history revision it was loaded at (a changed revision, as
    /// by undo, reloads it).
    curve: ?keyframe_logic.Editor = null,
    curve_revision: u32 = 0,
    curve_note: [160]u8 = undefined,
    curve_note_len: usize = 0,
    /// Where the curve widget was drawn in the last frame (screen pixels), for
    /// the auto tier to aim its pointer at; null while the Function window is
    /// closed or shows no curve.
    curve_rect: ?Rect = null,
    /// The auto tier's own placement of the Function window (a tall one, so
    /// every key is on screen); null leaves the layout to `place`.
    function_override: ?Rect = null,
    /// The same for the Image window, which the default layout puts over the tree.
    image_override: ?Rect = null,
    /// CParticleFrame::bHorizontalCamera, shared by the Particle and Effect previews.
    horizontal_camera: bool = false,
    /// The Get particle info button's four numbers, shown in the status bar.
    particle_status: dl.ParticleStatus = .{},
    /// The Particle source button (ID_PARTICLE_SOURCE) and the name it asks
    /// for the first time a project goes complex in this session.
    particle_source: dl.SourceToggle = .{},
    show_source_name: bool = false,
    source_name: [core.bridge.value_text_capacity]u8 = [_]u8{0} ** core.bridge.value_text_capacity,
    /// The Terrain editor's lists: the mode the tree selection chose, and
    /// whether the lists were opened for the current .til yet (they open
    /// themselves once, as MFC's frame showed them in place of its scene).
    terrain: tl.Lists = .{},
    terrain_shown: bool = false,
    /// ID_SWITCH_WIREFRAME: the road and river preview's wire frame.
    wireframe: bool = false,
    show_thumbnails: bool = false,
    show_direction: bool = false,
    show_function: bool = false,
    show_import: bool = false,
    show_help: bool = false,
    show_about: bool = false,
    /// The host check pins the docks at `fixed_layout`; the interactive mode
    /// places them once and then leaves them to the user (and layout.ini).
    fixed: bool = false,
    /// CDirectionButton's fAngle, -pi..pi, 0 pointing right.
    direction_angle: f32 = 0,
    thumbs: Thumbnails,
    import_form: dl.ImportForm = .{},
    import_note: [256]u8 = undefined,
    import_note_len: usize = 0,
    /// Where the first thumbnail's picture was drawn in the last frame, for
    /// the host check's measurement; null while it is not decoded yet.
    first_thumbnail: ?Rect = null,
    /// ImageFrm's view of the Mission, Chapter, Campaign and Medal picture.
    image: ImageFrame = .{},

    pub fn init(gpa: std.mem.Allocator, io: std.Io, real: *c_bridge.RealResBridge) Docks {
        return .{ .gpa = gpa, .io = io, .real = real, .thumbs = .{ .cache = Cache.init(gpa) } };
    }

    /// Before the host stops: the thumbnails' textures and the preview scene
    /// belong to the engine's device and modules.
    pub fn deinit(self: *Docks) void {
        if (self.curve) |*curve| curve.deinit();
        self.preview.stop(self.real.bridge());
        self.thumbs.freeNames(self.gpa);
        self.thumbs.names.deinit(self.gpa);
        self.thumbs.cache.deinit();
        self.image.release();
    }

    /// A mod switch: the pictures may differ under the new mod.
    pub fn modChanged(self: *Docks) void {
        self.thumbs.cache.clear();
    }

    pub fn setThumbnailFolder(self: *Docks, folder: []const u8) void {
        if (folder.len >= self.thumbs.folder.len) return;
        @memset(&self.thumbs.folder, 0);
        @memcpy(self.thumbs.folder[0..folder.len], folder);
    }

    /// Begins or stops the preview scene for the open project; once a frame.
    pub fn syncPreview(self: *Docks, life: *const logic.Lifecycle) void {
        const change = self.preview.sync(self.real.bridge(), life.is_open, life.doc.kind);
        // A new scene starts with a solid terrain.
        if (change != .none) self.wireframe = false;
        const is_til = life.is_open and life.doc.kind == .tile_set;
        if (is_til and !self.terrain_shown) self.show_thumbnails = true;
        self.terrain_shown = is_til;
    }

    /// The Wireframe check (ID_SWITCH_WIREFRAME); the flag only moves when
    /// the engine took the change.
    pub fn toggleWireframe(self: *Docks) void {
        const b = self.real.bridge();
        if (b.previewWireframe(!self.wireframe) == .ok) {
            self.wireframe = !self.wireframe;
        } else self.thumbs.say("wireframe: {s}", .{b.lastMessage()});
    }

    /// Import terrains / Import crossets (OnImportTerrains, OnImportCrossets):
    /// asks for the tileset editor's xml; takeFolder runs the import.
    fn requestTileImport(self: *Docks, mode: tl.Mode) void {
        _ = self;
        if (!folder_slot.request(if (mode == .crossets) .import_crossets else .import_terrains)) return;
        sdl3.c.SDL_ShowOpenFileDialog(folderCallback, &folder_slot, null, null, 0, null, false);
    }

    fn importTiles(self: *Docks, life: *logic.Lifecycle, path: []const u8, mode: tl.Mode) void {
        const b = self.real.bridge();
        const count = tl.importFile(self.gpa, b, life, path, mode) catch |err| {
            self.thumbs.say("import {s}: {s}", .{ @tagName(mode), if (b.lastMessage().len != 0) b.lastMessage() else @errorName(err) });
            return;
        };
        self.thumbs.say("imported {d} {s} tiles from {s}", .{ count, @tagName(mode), std.fs.path.basename(path) });
        // The tiles on disk changed: read the list again.
        self.thumbs.scanned_once = false;
    }

    /// Run (F5): MFC's Run button exported the project and played it.
    pub fn runPreview(self: *Docks) void {
        _ = self.preview.run(self.real.bridge());
    }

    pub fn stopPreview(self: *Docks) void {
        self.preview.halt(self.real.bridge());
    }

    /// The Camera button (OnButtonCamera): flips between MFC's horizontal and
    /// default camera. The flag only moves when the engine took the change.
    pub fn toggleCamera(self: *Docks) void {
        self.horizontal_camera = dl.toggledCamera(self.real.bridge(), self.horizontal_camera);
    }

    /// The Get particle info button (OnGetParticleInfo), enabled for a
    /// Particle project with its preview begun.
    pub fn getParticleInfo(self: *Docks) void {
        _ = self.particle_status.press(self.real.bridge());
    }

    /// The Particle source button (OnSwitchParticleSourceType). Going complex
    /// with no name known opens the name window instead.
    pub fn toggleParticleSource(self: *Docks, life: *logic.Lifecycle, name: ?[]const u8) dl.SourceToggle.Outcome {
        const target: edit.Target = .{ .allocator = self.gpa, .bridge = self.real.bridge(), .doc = &life.doc, .history = &life.history, .read_only = life.read_only };
        const outcome = self.particle_source.toggle(target, name);
        if (outcome == .need_name) self.show_source_name = true;
        return outcome;
    }

    // --- Menus and keys -----------------------------------------------------

    /// Import at the top of the File menu (MFC's Ctrl+I, ID_IMPORT_XML_FILE).
    pub fn drawFileMenuItems(self: *Docks) void {
        if (ig.igMenuItemEx("Import from game data...", mod_label ++ "+I", false, true)) self.show_import = true;
        ig.igSeparator();
    }

    pub fn drawViewMenuItems(self: *Docks) void {
        if (ig.igMenuItemEx("Thumbnails", null, self.show_thumbnails, true)) self.show_thumbnails = !self.show_thumbnails;
        if (ig.igMenuItemEx("Direction Button", mod_label ++ "+D", self.show_direction, true)) self.show_direction = !self.show_direction;
        if (ig.igMenuItemEx("Function Window", mod_label ++ "+F", self.show_function, true)) self.show_function = !self.show_function;
        const terrain_preview = self.preview.begun == .road_3d or self.preview.begun == .river_3d;
        if (ig.igMenuItemEx("Wireframe", null, self.wireframe, terrain_preview)) self.toggleWireframe();
    }

    pub fn drawPreviewMenuItems(self: *Docks, life: *logic.Lifecycle) void {
        if (ig.igMenuItemEx("Run", "F5", false, self.preview.begun != null)) self.runPreview();
        if (ig.igMenuItemEx("Stop", null, false, self.preview.running)) self.stopPreview();
        if (ig.igMenuItemEx("Wireframe", null, self.wireframe, self.preview.begun == .road_3d or self.preview.begun == .river_3d)) self.toggleWireframe();
        if (ig.igMenuItemEx("Horizontal camera", null, self.horizontal_camera, self.preview.begun != null)) self.toggleCamera();
        const til = life.is_open and life.doc.kind == .tile_set and !life.read_only;
        if (ig.igMenuItemEx("Import terrains...", null, false, til)) self.requestTileImport(.terrains);
        if (ig.igMenuItemEx("Import crossets...", null, false, til)) self.requestTileImport(.crossets);
        if (ig.igMenuItemEx("Get particle info", null, false, self.preview.begun == .particle)) self.getParticleInfo();
        const source_mode = if (life.is_open) dl.SourceToggle.mode(self.real.bridge()) else null;
        if (ig.igMenuItemEx("Particle source: complex", null, source_mode orelse false, source_mode != null)) _ = self.toggleParticleSource(life, null);
        ig.igSeparator();
        const text = self.preview.message();
        ig.igTextDisabled("%.*s", @as(c_int, @intCast(text.len)), text.ptr);
    }

    pub fn drawHelpMenuItems(self: *Docks) void {
        if (ig.igMenuItemEx("About...", null, false, true)) self.show_about = true;
        if (ig.igMenuItemEx("Help...", "F1", false, true)) self.show_help = true;
    }

    /// Ctrl/Cmd+D, +F, +I, F1 and F5, unless a text field has the keyboard.
    pub fn handleShortcuts(self: *Docks) void {
        const io = ig.igGetIO();
        if (io.*.WantTextInput) return;
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_F1, false)) self.show_help = true;
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_F5, false)) self.runPreview();
        const command = if (macos) io.*.KeySuper else io.*.KeyCtrl;
        if (!command or io.*.KeyShift) return;
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_D, false)) self.show_direction = !self.show_direction;
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_F, false)) self.show_function = !self.show_function;
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_I, false)) self.show_import = true;
    }

    // --- Windows ------------------------------------------------------------

    /// The docks and the preview's line; `project_folder` is what an empty
    /// thumbnail folder follows.
    pub fn drawDocks(self: *Docks, project_folder: ?[]const u8, life: *logic.Lifecycle, selected: ?i32) void {
        self.takeFolder(life);
        self.first_thumbnail = null;
        self.curve_rect = null;
        if (self.show_thumbnails) self.drawThumbnails(project_folder, life, selected);
        if (self.show_direction) self.drawDirection(life.is_open and life.active == .mesh_unit, life.is_open and life.active == .effect);
        if (self.show_function) self.drawFunction(life, selected);
        self.drawImageFrame(project_folder, life, selected);
        self.drawPreviewLine();
        self.drawParticleStatus();
    }

    // --- Image frame ----------------------------------------------------------

    /// ImageFrm's window: the sub-editor's picture at its real size in a
    /// scrolling child, the positions drawn as crosses, and the clicks and
    /// drags fed to image_logic's Overlay. The picture is decoded once per
    /// path (a Mission without its map_h.dds has the engine make it first).
    fn drawImageFrame(self: *Docks, project_folder: ?[]const u8, life: *logic.Lifecycle, selected: ?i32) void {
        const frame = &self.image;
        const kind = if (life.is_open) il.Kind.of(life.doc.kind) else null;
        const picked = kind orelse {
            frame.close(self.real.bridge());
            return;
        };
        const b = self.real.bridge();
        frame.crosses_toggle = null;
        const display = ig.igGetIO().*.DisplaySize;
        if (self.image_override) |r| {
            ig.igSetNextWindowPos(.{ .x = r.x, .y = r.y }, ig.ImGuiCond_Always);
            ig.igSetNextWindowSize(.{ .x = r.w, .y = r.h }, ig.ImGuiCond_Always);
        } else {
            ig.igSetNextWindowPos(.{ .x = display.x - 520, .y = 28 }, ig.ImGuiCond_FirstUseEver);
            ig.igSetNextWindowSize(.{ .x = 500, .y = 420 }, ig.ImGuiCond_FirstUseEver);
        }
        defer ig.igEnd();
        if (!ig.igBegin("Image###image_frame", null, 0)) return;

        var path_buffer: [logic.path_capacity + 64]u8 = undefined;
        const path = if (project_folder) |folder| il.sourcePath(&path_buffer, &life.doc, picked, folder) else null;
        self.loadImage(life, picked, path);

        if (picked.hasShowCrosses()) {
            var show = frame.overlayFor(self.gpa).mode == .drag_crosses;
            if (ig.igCheckbox("Show crosses", &show)) frame.overlayFor(self.gpa).setMode(b, if (show) .drag_crosses else .place);
            const low = ig.igGetItemRectMin();
            const high = ig.igGetItemRectMax();
            frame.crosses_toggle = .{ .x = low.x, .y = low.y, .w = high.x - low.x, .h = high.y - low.y };
            ig.igSameLine();
        }
        if (ig.igButton("Reload picture")) frame.forget();
        if (frame.note_len != 0) ig.igTextDisabled("%.*s", @as(c_int, @intCast(frame.note_len)), &frame.note);
        const texture = frame.texture orelse {
            if (project_folder == null) ig.igTextDisabled("Save the project to show its picture.") else if (path == null) ig.igTextDisabled("This project names no picture yet.");
            return;
        };

        if (!ig.igBeginChild("##image_canvas", .{ .x = 0, .y = 0 }, 0, ig.ImGuiWindowFlags_HorizontalScrollbar)) {
            ig.igEndChild();
            return;
        }
        defer ig.igEndChild();
        const overlay = frame.overlayFor(self.gpa);
        const size = ig.ImVec2{ .x = @floatFromInt(frame.width), .y = @floatFromInt(frame.height) };
        const top_left = ig.igGetCursorScreenPos();
        _ = ig.igInvisibleButton("picture", size, ig.ImGuiButtonFlags_MouseButtonLeft);
        // The cursor position already has the scroll taken off.
        overlay.view = .{ .origin = .{ .x = top_left.x, .y = top_left.y }, .size = .{ .x = size.x, .y = size.y } };
        const active = il.activeOf(&life.doc, picked, selected);
        if (!life.read_only and picked.places()) {
            const mouse = ig.igGetMousePos();
            const at: core.bridge.Point2 = .{ .x = mouse.x, .y = mouse.y };
            if (ig.igIsItemActivated()) overlay.press(b, &life.doc, picked, active, at) catch |err| frame.report(b, "place", err);
            if (ig.igIsItemActive()) overlay.move(b, at) catch |err| frame.report(b, "place", err);
            if (ig.igIsItemDeactivated()) overlay.release(b, &life.doc, &life.history, at) catch |err| frame.report(b, "place", err);
            if (ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) overlay.cancel(b);
        }
        const draw_list = ig.igGetWindowDrawList();
        ig.ImDrawList_AddImage(draw_list, textureRef(texture), top_left, .{ .x = top_left.x + size.x, .y = top_left.y + size.y });
        frame.shown = .{ .x = top_left.x, .y = top_left.y, .w = size.x, .h = size.y };
        if (!picked.places()) return;
        var lists: [2]il.List = undefined;
        for (il.crossLists(&life.doc, picked, &lists)) |list| {
            var points = core.sub_editor_tools.readGeometry(b, list.node, list.channel) catch continue;
            defer points.deinit(self.gpa);
            for (points.points2, 0..) |point, index| {
                const here = overlay.view.toScreen(point);
                const is_active = if (active) |a| a.list.node == list.node and a.index == index else false;
                const ink: u32 = if (is_active) 0xff00ffff else 0xff0000ff;
                const arm = il.cross_size / 2;
                ig.ImDrawList_AddLineEx(draw_list, .{ .x = here.x - arm, .y = here.y }, .{ .x = here.x + arm, .y = here.y }, ink, 2);
                ig.ImDrawList_AddLineEx(draw_list, .{ .x = here.x, .y = here.y - arm }, .{ .x = here.x, .y = here.y + arm }, ink, 2);
            }
        }
    }

    /// Decodes the picture at `path` once; a Mission whose map_h.dds is
    /// missing has the engine create it first. Failures stay in the note.
    fn loadImage(self: *Docks, life: *const logic.Lifecycle, kind: il.Kind, path: ?[:0]const u8) void {
        const frame = &self.image;
        const wanted = path orelse {
            frame.forget();
            return;
        };
        if (frame.loadedFor(wanted)) return;
        frame.release();
        frame.remember(wanted);
        frame.say("", .{});
        var device: ?*anyopaque = null;
        var format: c_uint = 0;
        if (c.BkEditorGpuDevice(self.real.session, &device, &format) != c.BK_EDITOR_OK or device == null) {
            frame.say("no graphics device to show the picture", .{});
            return;
        }
        const pixels = self.gpa.alloc(u8, @as(usize, @intCast(il.max_side)) * @as(usize, @intCast(il.max_side)) * 4) catch {
            frame.say("out of memory for the picture", .{});
            return;
        };
        defer self.gpa.free(pixels);
        var width: c_int = 0;
        var height: c_int = 0;
        var status = c.BkEditorMinimapImage(self.real.session, wanted.ptr, pixels.ptr, @intCast(pixels.len), il.max_side, &width, &height);
        if (status != c.BK_EDITOR_OK and kind == .mission and !life.read_only) {
            // MinimapCreation: the Mission's pictures are made on first use.
            const made = self.real.bridge().missionMinimap();
            if (made == .ok) {
                status = c.BkEditorMinimapImage(self.real.session, wanted.ptr, pixels.ptr, @intCast(pixels.len), il.max_side, &width, &height);
            } else {
                frame.say("minimap: {s}", .{self.real.bridge().lastMessage()});
                return;
            }
        }
        if (status != c.BK_EDITOR_OK) {
            const why = std.mem.span(c.BkEditorLastMessage(self.real.session));
            frame.say("{s}: {s}", .{ wanted, why });
            return;
        }
        if (!il.realSize(width, height)) {
            frame.say("{s}: {s}", .{ wanted, il.too_large_message });
            return;
        }
        const size: usize = @as(usize, @intCast(width)) * @as(usize, @intCast(height)) * 4;
        const texture = kit.pictures_cache.upload(device.?, .{ .width = width, .height = height, .bytes = pixels[0..size] }) orelse {
            frame.say("{s}: the picture could not be uploaded", .{wanted});
            return;
        };
        frame.texture = texture;
        frame.device = device;
        frame.width = width;
        frame.height = height;
    }

    /// Import, Help and About: the windows that act on the session.
    pub fn drawDialogs(self: *Docks, ui: *lifecycle_ui.Ui, window: ?*sdl3.c.SDL_Window) void {
        if (self.show_import) self.drawImport(ui, window);
        if (self.show_help) self.drawHelp();
        if (self.show_about) self.drawAbout();
        if (self.show_source_name) self.drawSourceName(&ui.session.life);
    }

    /// The name a complex source scatters, asked once per session (MFC's
    /// complex source item held it as a property; here the switch needs it).
    fn drawSourceName(self: *Docks, life: *logic.Lifecycle) void {
        ig.igSetNextWindowSize(.{ .x = 460, .y = 0 }, ig.ImGuiCond_Appearing);
        if (!ig.igBegin("Particle source###source_name", &self.show_source_name, ig.ImGuiWindowFlags_NoSavedSettings)) {
            ig.igEnd();
            return;
        }
        defer ig.igEnd();
        ig.igTextWrapped("A complex source scatters another particle effect. Name it (for example effects\\particles\\flame).");
        _ = ig.igInputText("Particle", &self.source_name, self.source_name.len - 1, 0);
        const name = std.mem.sliceTo(&self.source_name, 0);
        if (ig.igButton("Switch to complex")) {
            if (self.toggleParticleSource(life, name) == .switched) self.show_source_name = false;
        }
        const note = self.particle_source.note();
        if (note.len != 0) ig.igTextDisabled("%.*s", @as(c_int, @intCast(note.len)), note.ptr);
    }

    fn place(self: *const Docks, fixed: Rect, x: f32, y: f32, w: f32, h: f32) void {
        if (self.fixed) {
            ig.igSetNextWindowPos(.{ .x = fixed.x, .y = fixed.y }, ig.ImGuiCond_Always);
            ig.igSetNextWindowSize(.{ .x = fixed.w, .y = fixed.h }, ig.ImGuiCond_Always);
        } else {
            ig.igSetNextWindowPos(.{ .x = x, .y = y }, ig.ImGuiCond_FirstUseEver);
            ig.igSetNextWindowSize(.{ .x = w, .y = h }, ig.ImGuiCond_FirstUseEver);
        }
    }

    fn windowFlags(self: *const Docks) ig.ImGuiWindowFlags {
        return if (self.fixed) ig.ImGuiWindowFlags_NoSavedSettings else 0;
    }

    fn drawThumbnails(self: *Docks, project_folder: ?[]const u8, life: *logic.Lifecycle, selected: ?i32) void {
        const display = ig.igGetIO().*.DisplaySize;
        self.place(fixed_layout.thumbnails, display.x - 268, 28, 260, @max(200, display.y * 0.5));
        if (!ig.igBegin("Thumbnails###thumbnails", &self.show_thumbnails, self.windowFlags())) {
            ig.igEnd();
            return;
        }
        defer ig.igEnd();
        // The Terrain editor's list is the terrains or crossets folder of the
        // project, whichever the tree selection chose (TileTreeItem.cpp).
        const terrain = life.is_open and life.doc.kind == .tile_set;
        var terrain_buffer: [logic.path_capacity + 16]u8 = undefined;
        if (terrain) _ = self.terrain.follow(&life.doc, selected);
        const typed = std.mem.sliceTo(&self.thumbs.folder, 0);
        const folder = if (terrain) (tl.listFolder(&terrain_buffer, project_folder, self.terrain.mode) orelse "") else if (typed.len != 0) typed else project_folder orelse "";
        if (!self.thumbs.scanned_once or !std.mem.eql(u8, folder, self.thumbs.scannedFolder())) self.scan(folder);

        if (terrain) {
            ig.igText("%s", if (self.terrain.mode == .crossets) "Crossets" else "Terrains");
        } else {
            _ = ig.igInputText("##folder", &self.thumbs.folder, self.thumbs.folder.len, 0);
            ig.igSameLine();
            if (ig.igButton("Folder...")) {
                if (folder_slot.request(.thumbnails)) {
                    var start: [logic.path_capacity + 1]u8 = undefined;
                    const location = std.fmt.bufPrintZ(&start, "{s}", .{folder}) catch null;
                    sdl3.c.SDL_ShowOpenFolderDialog(folderCallback, &folder_slot, null, if (location) |l| l.ptr else null, false);
                }
            }
        }
        ig.igSameLine();
        if (ig.igButton("Rescan")) self.scan(folder);
        // WM_THUMB_LIST_DELETE: Delete removes the frame selected in the tree.
        if (ig.igIsWindowFocused(ig.ImGuiFocusedFlags_RootAndChildWindows) and !ig.igGetIO().*.WantTextInput and ig.igIsKeyPressedEx(ig.ImGuiKey_Delete, false)) {
            dl.deleteSelectedFrame(self.gpa, self.real.bridge(), life, selected) catch |err| {
                if (err != error.Refused) self.thumbs.say("delete frame: {s}", .{@errorName(err)});
            };
        }
        if (self.thumbs.note_len != 0) ig.igTextDisabled("%.*s", @as(c_int, @intCast(self.thumbs.note_len)), &self.thumbs.note);

        var device: ?*anyopaque = null;
        var format: c_uint = 0;
        if (c.BkEditorGpuDevice(self.real.session, &device, &format) == c.BK_EDITOR_OK) {
            if (device) |d| self.thumbs.cache.pump(&decodeThumbnail, @ptrCast(self), d, thumbnail_pump_budget);
        }

        const side = dl.thumbnail_side;
        const line = ig.igGetTextLineHeightWithSpacing();
        const spacing: f32 = 8;
        const avail = ig.igGetContentRegionAvail().x;
        const columns: usize = @max(1, @as(usize, @intFromFloat(@max(0, (avail + spacing) / (side + spacing)))));
        const draw_list = ig.igGetWindowDrawList();
        for (self.thumbs.names.items, 0..) |name, i| {
            if (i % columns != 0) ig.igSameLineEx(0, spacing);
            ig.igPushIDInt(@intCast(i));
            defer ig.igPopID();
            const top_left = ig.igGetCursorScreenPos();
            if (ig.igInvisibleButton("cell", .{ .x = side, .y = side + line }, 0)) self.thumbs.selected = i;
            if (ig.igIsItemHovered(0) and ig.igIsMouseDoubleClicked(0)) {
                self.thumbs.activated = i;
                if (terrain) self.tileFromPicture(life, selected, name) else self.frameFromPicture(life, selected, name);
            }
            // LoadImageToImageList's black cell, the picture fitted in it.
            ig.ImDrawList_AddRectFilled(draw_list, top_left, .{ .x = top_left.x + side, .y = top_left.y + side }, 0xff000000);
            self.thumbs.cache.request(name);
            switch (self.thumbs.cache.lookup(name)) {
                .ready => |ready| {
                    const fit = dl.fitThumbnail(@floatFromInt(ready.width), @floatFromInt(ready.height), side);
                    const min = ig.ImVec2{ .x = top_left.x + fit.x, .y = top_left.y + fit.y };
                    const max = ig.ImVec2{ .x = min.x + fit.w, .y = min.y + fit.h };
                    ig.ImDrawList_AddImage(draw_list, textureRef(ready.texture), min, max);
                    if (i == 0) self.first_thumbnail = .{ .x = min.x, .y = min.y, .w = fit.w, .h = fit.h };
                },
                .pending => {},
                .missing => ig.ImDrawList_AddTextEx(draw_list, .{ .x = top_left.x + 4, .y = top_left.y + 4 }, 0xff8080ff, "?", null),
            }
            const border = if (self.thumbs.selected == i) ig.igGetColorU32(ig.ImGuiCol_ButtonActive) else ig.igGetColorU32(ig.ImGuiCol_Border);
            ig.ImDrawList_AddRect(draw_list, top_left, .{ .x = top_left.x + side, .y = top_left.y + side }, border);
            ig.ImDrawList_PushClipRect(draw_list, .{ .x = top_left.x, .y = top_left.y + side }, .{ .x = top_left.x + side, .y = top_left.y + side + line }, true);
            ig.ImDrawList_AddTextEx(draw_list, .{ .x = top_left.x, .y = top_left.y + side + 1 }, ig.igGetColorU32(ig.ImGuiCol_Text), name.ptr, name.ptr + name.len);
            ig.ImDrawList_PopClipRect(draw_list);
        }
        if (terrain) self.drawSelectedTiles(life, selected);
    }

    /// The second list: the tiles of the active terrain or crosset.
    fn drawSelectedTiles(self: *Docks, life: *const logic.Lifecycle, selected: ?i32) void {
        _ = self;
        ig.igSeparator();
        const target = tl.addTarget(&life.doc, selected) orelse {
            ig.igTextDisabled("Select a terrain or crosset to see its tiles");
            return;
        };
        ig.igText("Tiles of the selected %s", if (target.mode == .crossets) "crosset" else "terrain");
        for (life.doc.tree.nodes.items) |node| {
            if (node.parent != target.id) continue;
            const name = node.displaySlice();
            ig.igText("%.*s", @as(c_int, @intCast(name.len)), name.ptr);
        }
    }

    /// A double-click on a picture of the Terrain editor's list: the tile goes
    /// under the active terrain or crosset (DoubleClickOnThumbList).
    fn tileFromPicture(self: *Docks, life: *logic.Lifecycle, selected: ?i32, name: []const u8) void {
        const target = tl.addTarget(&life.doc, selected) orelse {
            self.thumbs.say("{s}: select a terrain's Tiles or a crosset first", .{name});
            return;
        };
        if (target.mode != self.terrain.mode) {
            self.thumbs.say("{s}: the selected item takes {s} pictures", .{ name, if (target.mode == .crossets) "crosset" else "terrain" });
            return;
        }
        const b = self.real.bridge();
        tl.addTile(self.gpa, b, life, selected, self.terrain.mode, name) catch |err| {
            self.thumbs.say("{s}: the tile was not added ({s})", .{ name, if (b.lastMessage().len != 0) b.lastMessage() else @errorName(err) });
            return;
        };
        self.thumbs.say("{s} added", .{name});
    }

    /// A double-click on a picture: a sprite or an infantry project takes it
    /// as a frame (SpriteFrm and AnimationFrm DoubleClickOnThumbList).
    fn frameFromPicture(self: *Docks, life: *logic.Lifecycle, selected: ?i32, name: []const u8) void {
        dl.addFrameFromPicture(self.gpa, self.real.bridge(), life, selected, name) catch |err| switch (err) {
            error.Refused => self.thumbs.say("{s}: only a sprite, infantry or fence project takes a picture (not read-only, and a fence not twice)", .{name}),
            else => self.thumbs.say("{s}: the frame was not added ({s})", .{ name, @errorName(err) }),
        };
    }

    /// Reads `folder`'s pictures (LoadAllImagesFromDir), sorted by name.
    fn scan(self: *Docks, folder: []const u8) void {
        self.thumbs.freeNames(self.gpa);
        self.thumbs.cache.clear();
        self.thumbs.selected = null;
        self.thumbs.activated = null;
        self.thumbs.scanned_once = true;
        self.thumbs.note_len = 0;
        const len = @min(folder.len, self.thumbs.scanned.len);
        @memcpy(self.thumbs.scanned[0..len], folder[0..len]);
        self.thumbs.scanned_len = len;
        if (folder.len == 0) {
            self.thumbs.say("no folder: choose one, or save the project", .{});
            return;
        }
        var dir = std.Io.Dir.cwd().openDir(self.io, folder, .{ .iterate = true }) catch |err| {
            self.thumbs.say("{s} cannot be read: {s}", .{ folder, @errorName(err) });
            return;
        };
        defer dir.close(self.io);
        var it = dir.iterate();
        while (true) {
            const entry = (it.next(self.io) catch break) orelse break;
            if (entry.kind != .file or !dl.isThumbnailPicture(entry.name)) continue;
            const owned = self.gpa.dupe(u8, entry.name) catch break;
            self.thumbs.names.append(self.gpa, owned) catch {
                self.gpa.free(owned);
                break;
            };
        }
        dl.sortThumbnailNames(@ptrCast(self.thumbs.names.items));
        if (self.thumbs.names.items.len == 0) self.thumbs.say("no .tga pictures in this folder", .{});
    }

    /// `turns_unit`: the open project is a unit, whose preview follows the needle
    /// (MFC's direction button turned the combat object); `turns_effect`: the
    /// open project is an effect, whose running preview takes the needle's
    /// angle (CEffectFrame::UpdateEffectAngle) and whose dock starts at 45
    /// degrees, so the needle shows the bridge's stored angle.
    fn drawDirection(self: *Docks, turns_unit: bool, turns_effect: bool) void {
        const display = ig.igGetIO().*.DisplaySize;
        self.place(fixed_layout.direction, display.x - 188, display.y - 230, 180, 190);
        if (!ig.igBegin("Direction###direction", &self.show_direction, self.windowFlags())) {
            ig.igEnd();
            return;
        }
        defer ig.igEnd();
        const avail = ig.igGetContentRegionAvail();
        const side = @max(40, @min(avail.x, avail.y - ig.igGetTextLineHeightWithSpacing()));
        const top_left = ig.igGetCursorScreenPos();
        _ = ig.igInvisibleButton("button", .{ .x = side, .y = side }, 0);
        if (turns_effect) _ = dl.syncEffectAngle(self.real.bridge(), &self.direction_angle);
        // OnLButtonDown and OnMouseMove with the button held: the angle
        // follows the mouse while it drags.
        if (ig.igIsItemActive()) {
            const mouse = ig.igGetMousePos();
            self.direction_angle = dl.directionAngleAt(mouse.x - top_left.x, mouse.y - top_left.y, side, side);
            if (turns_unit) mesh_logic.turnPreview(self.real.bridge(), self.direction_angle) catch {};
            if (turns_effect) dl.turnEffect(self.real.bridge(), self.direction_angle) catch {};
        }
        const draw_list = ig.igGetWindowDrawList();
        ig.ImDrawList_AddRectFilled(draw_list, top_left, .{ .x = top_left.x + side, .y = top_left.y + side }, ig.igGetColorU32(ig.ImGuiCol_FrameBg));
        const end = dl.directionNeedleEnd(self.direction_angle, side, side);
        ig.ImDrawList_AddLineEx(draw_list, .{ .x = top_left.x + @trunc(side / 2), .y = top_left.y + @trunc(side / 2) }, .{ .x = top_left.x + end.x, .y = top_left.y + end.y }, ig.igGetColorU32(ig.ImGuiCol_Text), 2);
        var text: [32]u8 = undefined;
        const degrees = std.fmt.bufPrintZ(&text, "{d:.2} ", .{dl.directionDegrees(self.direction_angle)}) catch "";
        ig.ImDrawList_AddTextEx(draw_list, .{ .x = top_left.x + 2, .y = top_left.y + 3 }, ig.igGetColorU32(ig.ImGuiCol_Text), degrees.ptr, degrees.ptr + degrees.len);
        ig.igText("Quadrant %d", @as(c_int, dl.directionQuadrant(self.direction_angle)));
    }

    /// (Re)loads the curve editor when the selection or the document moved;
    /// null when the selected node is no curve (the bridge refuses its knobs).
    fn syncCurve(self: *Docks, life: *logic.Lifecycle, selected: ?i32) ?*keyframe_logic.Editor {
        const node = selected orelse return self.dropCurve();
        if (!life.is_open) return self.dropCurve();
        if (self.curve) |*curve| {
            if (curve.node == node and self.curve_revision == life.history.revision) return curve;
            if (curve.mode == .drag and curve.node == node) return curve;
            curve.deinit();
            self.curve = null;
        }
        var editor = keyframe_logic.Editor.init(self.gpa, node);
        editor.load(self.real.bridge()) catch {
            editor.deinit();
            return null;
        };
        self.curve = editor;
        self.curve_revision = life.history.revision;
        return &self.curve.?;
    }

    fn dropCurve(self: *Docks) ?*keyframe_logic.Editor {
        if (self.curve) |*curve| curve.deinit();
        self.curve = null;
        return null;
    }

    fn curveFailed(self: *Docks, err: anyerror) void {
        const text = std.fmt.bufPrint(&self.curve_note, "the curve was not changed: {s} ({s})", .{ @errorName(err), self.real.bridge().lastMessage() }) catch self.curve_note[0..];
        self.curve_note_len = text.len;
    }

    fn drawFunction(self: *Docks, life: *logic.Lifecycle, selected: ?i32) void {
        const display = ig.igGetIO().*.DisplaySize;
        if (self.function_override) |r| {
            ig.igSetNextWindowPos(.{ .x = r.x, .y = r.y }, ig.ImGuiCond_Always);
            ig.igSetNextWindowSize(.{ .x = r.w, .y = r.h }, ig.ImGuiCond_Always);
        } else self.place(fixed_layout.function, 316, display.y - 200, @max(300, display.x - 520), 170);
        if (!ig.igBegin("Function###function", &self.show_function, self.windowFlags())) {
            ig.igEnd();
            return;
        }
        defer ig.igEnd();
        const editor = self.syncCurve(life, selected) orelse {
            ig.igTextDisabled(dl.function_window_note);
            return;
        };
        if (self.curve_note_len != 0) ig.igTextColored(.{ .x = 1, .y = 0.5, .z = 0.5, .w = 1 }, "%.*s", @as(c_int, @intCast(self.curve_note_len)), &self.curve_note);
        const avail = ig.igGetContentRegionAvail();
        const top_left = ig.igGetCursorScreenPos();
        const w = @max(80, avail.x);
        const h = @max(60, avail.y);
        _ = ig.igInvisibleButton("curve", .{ .x = w, .y = h }, ig.ImGuiButtonFlags_MouseButtonLeft | ig.ImGuiButtonFlags_MouseButtonRight);
        const hovered = ig.igIsItemHovered(0);
        editor.setSize(@intFromFloat(w), @intFromFloat(h));
        const mouse = ig.igGetMousePos();
        // ImGui reports the pointer as -FLT_MAX until the first motion event arrives.
        const px: i32 = if (mouse.x > -1.0e6) @intFromFloat(@trunc(mouse.x - top_left.x)) else -1;
        const py: i32 = if (mouse.y > -1.0e6) @intFromFloat(@trunc(mouse.y - top_left.y)) else -1;
        self.curveInput(editor, life, hovered, px, py);
        self.drawCurve(editor, top_left, w, h);
        self.curve_rect = .{ .x = top_left.x, .y = top_left.y, .w = w, .h = h };
    }

    /// The screen pixel (in the window's coordinates) of a curve value as the
    /// widget draws it, so a pointer can be aimed at a key's handle; null
    /// while no curve was drawn.
    pub fn curveScreen(self: *const Docks, x: f32, y: f32) ?struct { x: f32, y: f32 } {
        const rect = self.curve_rect orelse return null;
        const editor = self.curve orelse return null;
        const s = editor.screenByValue(x, y);
        return .{ .x = rect.x + s.x, .y = rect.y + s.y };
    }

    /// The mouse and keys of CKeyFrameEditor: left press adds or grabs a key,
    /// the drag moves it, the release commits one command; Delete removes the
    /// key last touched; the right-click menu is IDR_KEYFRAME_ZOOM_MENU plus
    /// the dock's Reset all.
    fn curveInput(self: *Docks, editor: *keyframe_logic.Editor, life: *logic.Lifecycle, hovered: bool, px: i32, py: i32) void {
        const b = self.real.bridge();
        if (editor.mode == .drag) {
            editor.move(px, py);
            if (ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) {
                editor.cancel();
            } else if (!ig.igIsMouseDown(ig.ImGuiMouseButton_Left)) {
                editor.release(b, &life.doc, &life.history) catch |err| self.curveFailed(err);
                self.curve_revision = life.history.revision;
            }
            return;
        }
        if (hovered) {
            editor.hover(px);
            if (ig.igIsMouseClicked(ig.ImGuiMouseButton_Left)) {
                self.curve_note_len = 0;
                editor.press(px, py) catch |err| self.curveFailed(err);
            }
            if (ig.igIsKeyPressedEx(ig.ImGuiKey_Delete, false) and !ig.igGetIO().*.WantTextInput) {
                _ = editor.deleteActive(b, &life.doc, &life.history) catch |err| self.curveFailed(err);
                self.curve_revision = life.history.revision;
            }
            if (ig.igIsMouseClicked(ig.ImGuiMouseButton_Right)) {
                _ = ig.igOpenPopup("curve_menu", 0);
            }
        }
        if (ig.igBeginPopup("curve_menu", 0)) {
            if (ig.igMenuItem("Zoom in X")) _ = editor.zoomX(.in);
            if (ig.igMenuItem("Zoom out X")) _ = editor.zoomX(.out);
            if (ig.igMenuItem("Zoom in Y")) _ = editor.zoomY(.in);
            if (ig.igMenuItem("Zoom out Y")) _ = editor.zoomY(.out);
            ig.igSeparator();
            if (ig.igMenuItem("Reset all")) {
                _ = editor.resetAll(b, &life.doc, &life.history) catch |err| self.curveFailed(err);
                self.curve_revision = life.history.revision;
            }
            ig.igEndPopup();
        }
    }

    /// The grid, the polyline and the keys, through the editor's own mapping.
    fn drawCurve(self: *Docks, editor: *keyframe_logic.Editor, top_left: ig.ImVec2, w: f32, h: f32) void {
        _ = self;
        const draw_list = ig.igGetWindowDrawList();
        const bottom_right: ig.ImVec2 = .{ .x = top_left.x + w, .y = top_left.y + h };
        ig.ImDrawList_AddRectFilled(draw_list, top_left, bottom_right, ig.igGetColorU32(ig.ImGuiCol_FrameBg));
        ig.ImDrawList_PushClipRect(draw_list, top_left, bottom_right, true);
        defer ig.ImDrawList_PopClipRect(draw_list);
        const grid = ig.igGetColorU32(ig.ImGuiCol_Border);
        const k = editor.knobs;
        // One grid line per step on each axis, as the MFC editor's ruler.
        var x = k.min_x;
        while (x <= k.max_x and k.step_x > 0) : (x += k.step_x) {
            const s = editor.screenByValue(x, k.min_y);
            ig.ImDrawList_AddLineEx(draw_list, .{ .x = top_left.x + s.x, .y = top_left.y }, .{ .x = top_left.x + s.x, .y = bottom_right.y }, grid, 1);
        }
        var y = k.min_y;
        while (y <= k.max_y and k.step_y > 0) : (y += k.step_y) {
            const s = editor.screenByValue(k.min_x, y);
            ig.ImDrawList_AddLineEx(draw_list, .{ .x = top_left.x, .y = top_left.y + s.y }, .{ .x = bottom_right.x, .y = top_left.y + s.y }, grid, 1);
        }
        const line = ig.igGetColorU32(ig.ImGuiCol_PlotLines);
        const hot = ig.igGetColorU32(ig.ImGuiCol_PlotLinesHovered);
        var previous: ?ig.ImVec2 = null;
        for (editor.keys.items, 0..) |key, i| {
            const s = editor.screenByValue(key.x, key.y);
            const at: ig.ImVec2 = .{ .x = top_left.x + s.x, .y = top_left.y + s.y };
            if (previous) |p| ig.ImDrawList_AddLineEx(draw_list, p, at, line, 2);
            previous = at;
            const active = editor.high_index == i or (editor.mode == .drag and editor.drag_index == i);
            ig.ImDrawList_AddRectFilled(draw_list, .{ .x = at.x - 3, .y = at.y - 3 }, .{ .x = at.x + 3, .y = at.y + 3 }, if (active) hot else line);
        }
    }

    /// The preview's state in one line above the status line, with no
    /// background: the scene behind it stays the scene.
    fn drawPreviewLine(self: *Docks) void {
        const text = self.preview.message();
        if (text.len == 0) return;
        const display = ig.igGetIO().*.DisplaySize;
        ig.igSetNextWindowPosEx(.{ .x = display.x / 2, .y = display.y - 50 }, ig.ImGuiCond_Always, .{ .x = 0.5, .y = 0 });
        const flags = ig.ImGuiWindowFlags_NoDecoration | ig.ImGuiWindowFlags_NoBackground | ig.ImGuiWindowFlags_NoInputs |
            ig.ImGuiWindowFlags_NoSavedSettings | ig.ImGuiWindowFlags_AlwaysAutoResize | ig.ImGuiWindowFlags_NoFocusOnAppearing | ig.ImGuiWindowFlags_NoNav;
        if (ig.igBegin("##preview_line", null, flags)) {
            ig.igTextDisabled("Preview: %.*s", @as(c_int, @intCast(text.len)), text.ptr);
        }
        ig.igEnd();
    }

    /// CParticleFrame::OnUpdateStatusBar: four panes along the window's
    /// bottom edge, "Max particles %g", "Size %g", "Average size %g" and
    /// "Average count %g". They belong to the Particle frame, so another
    /// project drops them.
    fn drawParticleStatus(self: *Docks) void {
        if (self.preview.begun != .particle) self.particle_status.clear();
        const note = self.particle_status.note();
        if (self.particle_status.info == null and note.len == 0) return;
        const display = ig.igGetIO().*.DisplaySize;
        ig.igSetNextWindowPosEx(.{ .x = 0, .y = display.y }, ig.ImGuiCond_Always, .{ .x = 0, .y = 1 });
        const flags = ig.ImGuiWindowFlags_NoDecoration | ig.ImGuiWindowFlags_NoSavedSettings | ig.ImGuiWindowFlags_AlwaysAutoResize |
            ig.ImGuiWindowFlags_NoFocusOnAppearing | ig.ImGuiWindowFlags_NoNav | ig.ImGuiWindowFlags_NoInputs;
        if (ig.igBegin("##particle_status", null, flags)) {
            if (self.particle_status.info) |info| {
                var buffers: [4][48]u8 = undefined;
                const panes = dl.infoPanes(&buffers, info);
                for (panes, 0..) |pane, i| {
                    if (i != 0) ig.igSameLineEx(0, 24);
                    ig.igText("%.*s", @as(c_int, @intCast(pane.len)), pane.ptr);
                }
            }
            if (note.len != 0) ig.igTextDisabled("%.*s", @as(c_int, @intCast(note.len)), note.ptr);
        }
        ig.igEnd();
    }

    fn drawImport(self: *Docks, ui: *lifecycle_ui.Ui, window: ?*sdl3.c.SDL_Window) void {
        ig.igSetNextWindowSize(.{ .x = 520, .y = 0 }, ig.ImGuiCond_Appearing);
        if (!ig.igBegin("Import from game data###import", &self.show_import, ig.ImGuiWindowFlags_NoSavedSettings)) {
            ig.igEnd();
            return;
        }
        defer ig.igEnd();
        if (self.import_form.folderSlice().len == 0) {
            var start: [logic.path_capacity]u8 = undefined;
            _ = self.import_form.setFolder(dl.importStartFolder(&start, std.mem.sliceTo(&ui.paths.base_root, 0)));
        }
        ig.igTextWrapped("Builds a new, unsaved project from a game resource folder (the one holding its 1.xml). Infantry imports today; the other kinds come with their sub-editors.");
        if (ig.igBeginCombo("Kind", lifecycle.kindLabel(self.import_form.kind).ptr, 0)) {
            for (std.enums.values(Kind)) |kind| {
                var label: [64]u8 = undefined;
                const text = std.fmt.bufPrintZ(&label, "{s} (.{s})", .{ lifecycle.kindLabel(kind), kind.extension() }) catch continue;
                if (ig.igSelectableEx(text.ptr, kind == self.import_form.kind, 0, .{ .x = 0, .y = 0 })) self.import_form.kind = kind;
            }
            ig.igEndCombo();
        }
        _ = ig.igInputText("Folder", &self.import_form.folder, self.import_form.folder.len - 1, 0);
        ig.igSameLine();
        if (ig.igButton("Browse...")) {
            if (folder_slot.request(.import)) {
                var start: [logic.path_capacity + 1]u8 = undefined;
                const location = std.fmt.bufPrintZ(&start, "{s}", .{self.import_form.folderSlice()}) catch null;
                sdl3.c.SDL_ShowOpenFolderDialog(folderCallback, &folder_slot, window, if (location) |l| l.ptr else null, false);
            }
        }
        if (ig.igButton("Import")) {
            switch (self.import_form.request()) {
                .go => |pending| {
                    var context = ui.ctx();
                    ui.session.request(&context, pending);
                    self.sayImport("{s}", .{ui.session.message()});
                },
                .why_not => |why| self.sayImport("{s}", .{why}),
            }
        }
        ig.igSameLine();
        if (ig.igButton("Close")) self.show_import = false;
        if (self.import_note_len != 0) ig.igTextWrapped("%.*s", @as(c_int, @intCast(self.import_note_len)), &self.import_note);
    }

    fn sayImport(self: *Docks, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.import_note, format, args) catch self.import_note[0..];
        self.import_note_len = text.len;
    }

    fn drawHelp(self: *Docks) void {
        ig.igSetNextWindowSize(.{ .x = 520, .y = 0 }, ig.ImGuiCond_Appearing);
        if (!ig.igBegin("Help###help", &self.show_help, ig.ImGuiWindowFlags_NoSavedSettings)) {
            ig.igEnd();
            return;
        }
        defer ig.igEnd();
        ig.igSeparatorText("Shortcuts");
        if (ig.igBeginTable("shortcuts", 2, ig.ImGuiTableFlags_RowBg)) {
            for (dl.shortcuts) |shortcut| {
                ig.igTableNextRow();
                _ = ig.igTableSetColumnIndex(0);
                var buffer: [64]u8 = undefined;
                const keys = dl.shortcutKeys(&buffer, shortcut.keys, macos);
                ig.igTextUnformattedEx(keys.ptr, keys.ptr + keys.len);
                _ = ig.igTableSetColumnIndex(1);
                ig.igTextUnformattedEx(shortcut.action.ptr, shortcut.action.ptr + shortcut.action.len);
            }
            ig.igEndTable();
        }
        ig.igSeparatorText("Manual");
        ig.igTextWrapped("The MFC editor's reshelp.chm is not in the repository. The design and every feature are written in the spec:");
        ig.igTextUnformatted(dl.spec_path);
        if (ig.igButton("Open in the browser")) _ = sdl3.c.SDL_OpenURL(dl.spec_url);
        ig.igSameLine();
        if (ig.igButton("Copy the link")) ig.igSetClipboardText(dl.spec_url);
        ig.igSameLine();
        if (ig.igButton("Close")) self.show_help = false;
    }

    fn drawAbout(self: *Docks) void {
        if (!ig.igBegin("About Resource Editor###about", &self.show_about, ig.ImGuiWindowFlags_AlwaysAutoResize | ig.ImGuiWindowFlags_NoSavedSettings)) {
            ig.igEnd();
            return;
        }
        defer ig.igEnd();
        ig.igTextUnformatted(dl.about_title);
        for (dl.about_mfc_lines) |line| ig.igTextUnformattedEx(line.ptr, line.ptr + line.len);
        ig.igSeparator();
        ig.igTextUnformatted(dl.about_port);
        ig.igTextUnformatted(dl.about_spec);
        ig.igTextUnformatted(dl.about_source);
        ig.igPushTextWrapPos(480);
        ig.igTextUnformatted(dl.about_license);
        ig.igPopTextWrapPos();
        if (ig.igButton("OK")) self.show_about = false;
    }

    fn takeFolder(self: *Docks, life: *logic.Lifecycle) void {
        const taken = folder_slot.take() orelse return;
        if (taken.path.len == 0) return;
        switch (taken.target) {
            .thumbnails => self.setThumbnailFolder(taken.path),
            .import => _ = self.import_form.setFolder(taken.path),
            .import_terrains => self.importTiles(life, taken.path, .terrains),
            .import_crossets => self.importTiles(life, taken.path, .crossets),
        }
    }
};

/// The engine's own image decoders through BkEditorMinimapImage, which reads
/// "<base>.tga" for a "<base>.xml" path (docks_logic.thumbnailDecodePath):
/// the IImageProcessor::LoadImage CThumbList used, scaled down to the
/// cache's side when the picture is larger.
fn decodeThumbnail(ctx: *anyopaque, name: []const u8, pixel_buffer: []u8, max_side: i32) ?kit.pictures_cache.Decoded {
    const self: *Docks = @ptrCast(@alignCast(ctx));
    var path_buffer: [logic.path_capacity + 8]u8 = undefined;
    const path = dl.thumbnailDecodePath(&path_buffer, self.thumbs.scannedFolder(), name) orelse return null;
    var width: c_int = 0;
    var height: c_int = 0;
    if (c.BkEditorMinimapImage(self.real.session, path.ptr, pixel_buffer.ptr, @intCast(pixel_buffer.len), max_side, &width, &height) != c.BK_EDITOR_OK) return null;
    if (width <= 0 or height <= 0) return null;
    const size: usize = @as(usize, @intCast(width)) * @as(usize, @intCast(height)) * 4;
    if (size > pixel_buffer.len) return null;
    return .{ .width = width, .height = height, .bytes = pixel_buffer[0..size] };
}

fn textureRef(texture: *sdl3.c.SDL_GPUTexture) ig.ImTextureRef {
    return .{ ._TexData = null, ._TexID = @intCast(@intFromPtr(texture)) };
}
