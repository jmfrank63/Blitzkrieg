//! The Minimap panel (05-07, D-14..D-17): the MFC editor's minimap bar as one
//! floating window. Editor mode is the live map - the terrain drawn CPU-side
//! into a texture (one pixel per tile, the engine-averaged colour of the
//! tile's terrain type, or a grey height gradient while the Heights tool is
//! active) and rebuilt when the document's revision moves; over it, drawn
//! in the panel's own space so they stay sharp at any size, the fire-range
//! areas, the objects' markers in the 17 player colours, the camera's frame
//! and the patch grid. Game mode is the map's own pre-built picture
//! (`<map>.tga`, else `<map>_h.dds`). A click or a drag moves the camera the
//! way the MFC's did (`panels_logic.minimapCameraTarget`).
//!
//! The maths and the rasterisers are in panels_logic.zig, tested without a
//! window; the buttons and BK_EDITOR_AUTO both run the named commands
//! (`minimap_toggle`, `minimap_mode`, `minimap_click`, `minimap_create`,
//! commands.zig), which land on the functions here.
//!
//! Deviation from D-14's "drawn CPU-side into a texture", recorded in the
//! 05-07 summary: only the terrain is rasterised into the texture; the
//! overlays are vector draws over it. Refreshing is by dirty flag (the
//! document's revision, the Heights tool, the map's size) at most once a
//! frame, not by dirty rectangle.
const std = @import("std");
const imgui = @import("editor_imgui");
const sdl3 = @import("sdl3");
const core = @import("editor_core");
const kit = @import("editor_kit");
const panels = @import("panels.zig");
const commands = @import("commands.zig");
const logic = @import("panels_logic.zig");

const ig = imgui.c;
const State = panels.State;

pub const Mode = logic.MinimapMode;

/// What is known of the map's own minimap picture.
pub const GameState = enum { unknown, present, missing };

/// The largest side the Game picture is decoded to: the pictures the engine
/// writes are 512 and 256.
const game_max_side: i32 = 1024;

/// One GPU texture the minimap owns and replaces whole when its size changes.
const GpuImage = struct {
    texture: ?*sdl3.c.SDL_GPUTexture = null,
    width: u32 = 0,
    height: u32 = 0,

    fn release(self: *GpuImage, device: ?*anyopaque) void {
        if (self.texture) |texture| {
            if (device) |dev| {
                const gpu: *sdl3.c.SDL_GPUDevice = @ptrCast(@alignCast(dev));
                sdl3.c.SDL_ReleaseGPUTexture(gpu, texture);
            }
        }
        self.* = .{};
    }

    /// Puts RGBA8 `pixels` (`width` x `height`) into the texture, making it
    /// first when there is none or it is another size. False when the GPU
    /// would not (the panel then draws grey).
    fn set(self: *GpuImage, device: *anyopaque, pixels: []const u8, width: u32, height: u32) bool {
        if (width == 0 or height == 0 or pixels.len != @as(usize, width) * height * 4) return false;
        const gpu: *sdl3.c.SDL_GPUDevice = @ptrCast(@alignCast(device));
        if (self.texture != null and (self.width != width or self.height != height)) self.release(device);
        if (self.texture == null) {
            self.texture = sdl3.c.SDL_CreateGPUTexture(gpu, &.{
                .type = sdl3.c.SDL_GPU_TEXTURETYPE_2D,
                .format = sdl3.c.SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                .usage = sdl3.c.SDL_GPU_TEXTUREUSAGE_SAMPLER,
                .width = width,
                .height = height,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .sample_count = sdl3.c.SDL_GPU_SAMPLECOUNT_1,
                .props = 0,
            });
            self.width = width;
            self.height = height;
        }
        const texture = self.texture orelse return false;

        const transfer = sdl3.c.SDL_CreateGPUTransferBuffer(gpu, &.{
            .usage = sdl3.c.SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = @intCast(pixels.len),
            .props = 0,
        }) orelse return false;
        defer sdl3.c.SDL_ReleaseGPUTransferBuffer(gpu, transfer);
        const mapped = sdl3.c.SDL_MapGPUTransferBuffer(gpu, transfer, false) orelse return false;
        const dest: [*]u8 = @ptrCast(mapped);
        @memcpy(dest[0..pixels.len], pixels);
        sdl3.c.SDL_UnmapGPUTransferBuffer(gpu, transfer);

        const command_buffer = sdl3.c.SDL_AcquireGPUCommandBuffer(gpu) orelse return false;
        const copy_pass = sdl3.c.SDL_BeginGPUCopyPass(command_buffer) orelse {
            _ = sdl3.c.SDL_SubmitGPUCommandBuffer(command_buffer);
            return false;
        };
        // cycle = true: the texture may still be sampled by the previous frame.
        sdl3.c.SDL_UploadToGPUTexture(copy_pass, &.{
            .transfer_buffer = transfer,
            .offset = 0,
            .pixels_per_row = width,
            .rows_per_layer = height,
        }, &.{
            .texture = texture,
            .mip_level = 0,
            .layer = 0,
            .x = 0,
            .y = 0,
            .z = 0,
            .w = width,
            .h = height,
            .d = 1,
        }, true);
        sdl3.c.SDL_EndGPUCopyPass(copy_pass);
        _ = sdl3.c.SDL_SubmitGPUCommandBuffer(command_buffer);
        return true;
    }
};

/// The panel's whole state; `State.minimap`.
pub const Minimap = struct {
    visible: bool = false,
    mode: Mode = .editor,
    show_grid: bool = true,

    // The map as last drawn into the texture.
    revision_seen: ?u32 = null,
    heights_seen: bool = false,
    tiles_w: i32 = 0,
    tiles_h: i32 = 0,
    colors: [256]u32 = @splat(0),
    color_count: usize = 0,
    colors_valid: bool = false,
    tiles: []u8 = &.{},
    heights: []f32 = &.{},
    pixels: []u8 = &.{},
    terrain: GpuImage = .{},
    read_failed: bool = false,
    units: std.ArrayListUnmanaged(core.bridge.MinimapUnit) = .empty,
    areas: std.ArrayListUnmanaged(core.bridge.MinimapArea) = .empty,

    // The map's own picture.
    game: GameState = .unknown,
    game_path: logic.PathText = .{},
    game_image: GpuImage = .{},

    // The picture's size as last drawn (pixels), what a scripted click is
    // relative to.
    rect_w: f32 = 0,
    rect_h: f32 = 0,
    rect_valid: bool = false,
    /// Whether the last click moved the camera (`minimap_moved`).
    last_click_moved: bool = false,
    /// Set by Create Minimap Images when Save/Save As has to come first: the
    /// pictures are made once the save lands (`panels.act`).
    create_pending: bool = false,
    device: ?*anyopaque = null,

    pub fn deinit(self: *Minimap, allocator: std.mem.Allocator) void {
        self.terrain.release(self.device);
        self.game_image.release(self.device);
        allocator.free(self.tiles);
        allocator.free(self.heights);
        allocator.free(self.pixels);
        self.units.deinit(allocator);
        self.areas.deinit(allocator);
        self.* = .{};
    }

    /// A map was opened, made or closed: everything read is stale.
    pub fn mapOpened(self: *Minimap) void {
        self.revision_seen = null;
        self.colors_valid = false;
        self.read_failed = false;
        self.game = .unknown;
        self.create_pending = false;
        self.rect_valid = false;
        self.last_click_moved = false;
    }
};

fn textureRef(texture: *sdl3.c.SDL_GPUTexture) ig.ImTextureRef {
    return .{ ._TexData = null, ._TexID = @intCast(@intFromPtr(texture)) };
}

/// ImGui's ABGR for an 0xRRGGBB colour and an alpha.
fn abgr(rgb: u32, alpha: u8) u32 {
    return (@as(u32, alpha) << 24) | ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb >> 16) & 0xFF);
}

// ---------------------------------------------------------------------------
// Reads and the texture.
// ---------------------------------------------------------------------------

/// Reads the tile colours (once per map), the tiles and, with the Heights tool
/// active, the heights; rasterises them and puts them in the texture. The units
/// are read with them.
fn rebuild(state: *State, device: *anyopaque, heights_mode: bool) void {
    const mm = &state.minimap;
    const allocator = state.allocator;
    const info = state.editor.document.info;
    const width: usize = @intCast(info.width_tiles);
    const height: usize = @intCast(info.height_tiles);
    mm.read_failed = true;
    defer mm.revision_seen = state.editor.mapRevision();
    mm.tiles_w = info.width_tiles;
    mm.tiles_h = info.height_tiles;
    mm.heights_seen = heights_mode;
    const bridge = state.editor.bridge;

    if (!mm.colors_valid) {
        var total: usize = 0;
        if (bridge.minimapTileColors(&mm.colors, &total) != .ok or total > mm.colors.len) return;
        mm.color_count = total;
        mm.colors_valid = true;
    }

    if (mm.tiles.len != width * height) {
        allocator.free(mm.tiles);
        mm.tiles = &.{};
        mm.tiles = allocator.alloc(u8, width * height) catch return;
    }
    if (mm.pixels.len != width * height * 4) {
        allocator.free(mm.pixels);
        mm.pixels = &.{};
        mm.pixels = allocator.alloc(u8, width * height * 4) catch return;
    }
    var total: usize = 0;
    const region: core.bridge.TileRegion = .{ .x0 = 0, .y0 = 0, .x1 = info.width_tiles, .y1 = info.height_tiles };
    if (bridge.tiles(region, mm.tiles, &total) != .ok or total != mm.tiles.len) return;
    logic.rasterizeMinimapTerrain(mm.pixels, mm.tiles, mm.colors[0..mm.color_count]);

    if (heights_mode) heights: {
        const vertex_w = width + 1;
        const vertex_h = height + 1;
        if (mm.heights.len != vertex_w * vertex_h) {
            allocator.free(mm.heights);
            mm.heights = &.{};
            mm.heights = allocator.alloc(f32, vertex_w * vertex_h) catch break :heights;
        }
        const vertices: core.bridge.AltitudeRegion = .{ .x0 = 0, .y0 = 0, .x1 = @intCast(vertex_w), .y1 = @intCast(vertex_h) };
        var got: usize = 0;
        if (bridge.altitudes(vertices, mm.heights, &got) != .ok or got != mm.heights.len) break :heights;
        logic.rasterizeMinimapHeights(mm.pixels, mm.heights, vertex_w, width, height);
    }

    if (!mm.terrain.set(device, mm.pixels, @intCast(width), @intCast(height))) return;
    readUnits(state);
    mm.read_failed = false;
}

/// The markers, two-pass: the count first, then exactly that many.
fn readUnits(state: *State) void {
    const mm = &state.minimap;
    mm.units.clearRetainingCapacity();
    var none: [0]core.bridge.MinimapUnit = .{};
    var total: usize = 0;
    const sizing = state.editor.bridge.minimapUnits(&none, &total);
    if (sizing != .ok and sizing != .refused) return;
    if (total == 0) return;
    mm.units.resize(state.allocator, total) catch return;
    var got: usize = 0;
    if (state.editor.bridge.minimapUnits(mm.units.items, &got) != .ok or got != total) mm.units.clearRetainingCapacity();
}

/// The fire-range areas the AI shows now, read every frame the panel is up: a
/// stack buffer first, the exact count when that was too small.
fn readAreas(state: *State) void {
    const mm = &state.minimap;
    mm.areas.clearRetainingCapacity();
    var stack: [32]core.bridge.MinimapArea = undefined;
    var total: usize = 0;
    const first = state.editor.bridge.minimapAreas(&stack, &total);
    if (first == .ok) {
        mm.areas.appendSlice(state.allocator, stack[0..@min(total, stack.len)]) catch {};
        return;
    }
    if (first != .refused or total == 0) return;
    mm.areas.resize(state.allocator, total) catch return;
    var got: usize = 0;
    if (state.editor.bridge.minimapAreas(mm.areas.items, &got) != .ok or got != total) mm.areas.clearRetainingCapacity();
}

/// Decodes the map's own picture into the Game texture; sets `game`.
/// Cheap to ask twice: it only decodes while `game` is unknown or the
/// document's path moved.
pub fn probeGame(state: *State) GameState {
    const mm = &state.minimap;
    const path = state.editor.document.path.items;
    if (mm.game != .unknown and std.mem.eql(u8, mm.game_path.slice(), path)) return mm.game;
    mm.game = .missing;
    mm.game_path.set(path);
    if (path.len == 0) return mm.game;
    const device = state.real.gpuDevice() orelse return mm.game;
    mm.device = device;
    const buffer = state.allocator.alloc(u8, @as(usize, game_max_side) * game_max_side * 4) catch return mm.game;
    defer state.allocator.free(buffer);
    const picture = state.real.minimapImage(path, buffer, game_max_side) orelse {
        mm.game_image.release(device);
        return mm.game;
    };
    if (mm.game_image.set(device, picture.bytes, @intCast(picture.width), @intCast(picture.height))) mm.game = .present;
    return mm.game;
}

/// Everything the panel shows, brought up to the document: the terrain texture
/// when the revision, the Heights tool or the map's size moved, the markers
/// with it, the areas every time, the Game picture when asked for.
fn refresh(state: *State) void {
    const mm = &state.minimap;
    const info = state.editor.document.info;
    if (info.width_tiles <= 0 or info.height_tiles <= 0) return;
    const device = state.real.gpuDevice() orelse return;
    mm.device = device;
    const heights_mode = state.view.tool == .heights;
    const same_size = mm.tiles_w == info.width_tiles and mm.tiles_h == info.height_tiles;
    if (mm.revision_seen == null or mm.revision_seen.? != state.editor.mapRevision() or mm.heights_seen != heights_mode or !same_size)
        rebuild(state, device, heights_mode);
    readAreas(state);
    if (mm.mode == .game and probeGame(state) != .present) mm.mode = .editor;
}

// ---------------------------------------------------------------------------
// The click and the commands' bodies.
// ---------------------------------------------------------------------------

/// A click or a drag at (`px`, `py`) pixels from the picture's top-left, in the
/// picture as last drawn (`mm.rect_w` x `rect_h`): the camera moves by the MFC's
/// rule. False when there is nothing to move (no picture, no camera, no map).
pub fn clickPixels(state: *State, px: f32, py: f32) bool {
    const mm = &state.minimap;
    if (!mm.rect_valid or !panels.documentLoaded(state.editor)) return false;
    const info = state.editor.document.info;
    const world = logic.minimapToWorld(px, py, mm.rect_w, mm.rect_h, info.width_tiles, info.height_tiles);
    const screen = state.real.screenSize() orelse return false;
    const view = state.real.viewState() orelse return false;
    const anchor: [2]f32 = .{ view.anchor_x, view.anchor_y };
    // What is under the screen's centre now; the anchor itself when the
    // centre is off the terrain (a zoomed-out view of a small map), which
    // leaves the click's point at the anchor.
    var centre = anchor;
    if (state.editor.resolve(@as(f32, @floatFromInt(screen[0])) / 2.0, @as(f32, @floatFromInt(screen[1])) / 2.0)) |pointer| {
        centre = .{ pointer.world_x, pointer.world_y };
    } else |_| {}
    const target = logic.minimapCameraTarget(world, anchor, centre, state.view.map);
    state.view.centreOn(state.real, target[0], target[1]);
    const after = state.real.viewState() orelse return true;
    mm.last_click_moved = after.anchor_x != view.anchor_x or after.anchor_y != view.anchor_y;
    return true;
}

/// A scripted click: percents of the picture's width and height.
pub fn clickPercent(state: *State, x_percent: f32, y_percent: f32) bool {
    const mm = &state.minimap;
    if (!mm.rect_valid) return false;
    return clickPixels(state, x_percent / 100.0 * (mm.rect_w - 1), y_percent / 100.0 * (mm.rect_h - 1));
}

/// Chooses the picture. Game needs the map's own: refused, saying how to get
/// one, when there is none.
pub fn setMode(state: *State, mode: Mode) bool {
    const mm = &state.minimap;
    if (mode == .game and probeGame(state) != .present) {
        state.view.setStatus("minimap: ", "this map has no minimap picture of its own - Map > Create Minimap Images makes one");
        return false;
    }
    mm.mode = mode;
    return true;
}

/// Makes the four pictures beside the saved map, then shows them. The caller has
/// made sure the document is a saved user map; the bridge refuses anything else
/// and says why.
pub fn createNow(state: *State) bool {
    const mm = &state.minimap;
    // The bridge takes a full path (a relative one would name the game's data);
    // the document's may be relative to where the editor runs, so it is resolved
    // here, as the file system sees it.
    var os_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    var real_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const document_path = state.editor.document.path.items;
    const os_path = kit.files.osPathFromEngine(&os_buffer, document_path) orelse document_path;
    const path = if (state.editor.files) |files| (files.realPath(os_path, &real_buffer) orelse os_path) else os_path;
    const result = state.editor.bridge.createMinimapImages(path);
    if (result != .ok) {
        state.view.setStatus("minimap pictures: ", state.editor.bridge.lastMessage());
        return false;
    }
    mm.game = .unknown;
    mm.mode = .game;
    if (probeGame(state) != .present) mm.mode = .editor;
    var note: [200]u8 = undefined;
    const base = logic.minimapImageBase(document_path) orelse document_path;
    state.view.setStatus("minimap: ", std.fmt.bufPrint(&note, "wrote {s}_large and {s} as .tga and _c/_l/_h.dds", .{ logic.baseName(base), logic.baseName(base) }) catch "wrote the minimap pictures");
    return true;
}

// ---------------------------------------------------------------------------
// The window.
// ---------------------------------------------------------------------------

/// The panel's window, drawn from panels.zig's `draw`.
pub fn draw(state: *State, pos: ig.ImVec2, size: ig.ImVec2) void {
    const mm = &state.minimap;
    mm.rect_valid = false;
    if (!mm.visible) return;
    ig.igSetNextWindowPos(pos, ig.ImGuiCond_FirstUseEver);
    ig.igSetNextWindowSize(size, ig.ImGuiCond_FirstUseEver);
    const open = ig.igBegin("Minimap", &mm.visible, ig.ImGuiWindowFlags_NoCollapse);
    defer ig.igEnd();
    if (!open) return;
    if (!panels.documentLoaded(state.editor)) {
        panels.text("no map open");
        return;
    }
    refresh(state);

    // The MFC bar's own three buttons, and the grid.
    if (ig.igRadioButton("Editor", mm.mode == .editor)) _ = commands.run(state, "minimap_mode", "editor");
    ig.igSameLine();
    ig.igBeginDisabled(mm.game == .missing);
    if (ig.igRadioButton("Game", mm.mode == .game)) _ = commands.run(state, "minimap_mode", "game");
    ig.igEndDisabled();
    ig.igSameLine();
    if (ig.igButton("Create")) _ = commands.run(state, "minimap_create", "");
    if (ig.igIsItemHovered(0) and ig.igBeginTooltip()) {
        panels.text("Writes the map's minimap pictures beside the saved map (Map > Create Minimap Images)");
        ig.igEndTooltip();
    }
    ig.igSameLine();
    _ = ig.igCheckbox("Grid", &mm.show_grid);

    const info = state.editor.document.info;
    const avail = ig.igGetContentRegionAvail();
    const fit = logic.minimapFit(avail.x, avail.y, info.width_tiles, info.height_tiles);
    if (fit[0] < 8 or fit[1] < 8) return;
    const origin = ig.igGetCursorScreenPos();
    _ = ig.igInvisibleButton("##minimap", .{ .x = fit[0], .y = fit[1] }, 0);
    const clicking = ig.igIsItemActive();
    const draw_list = ig.igGetWindowDrawList();
    const corner: ig.ImVec2 = .{ .x = origin.x + fit[0], .y = origin.y + fit[1] };
    mm.rect_w = fit[0];
    mm.rect_h = fit[1];
    mm.rect_valid = true;

    ig.ImDrawList_PushClipRect(draw_list, origin, corner, true);
    defer ig.ImDrawList_PopClipRect(draw_list);
    const texture = if (mm.mode == .game) mm.game_image.texture else mm.terrain.texture;
    if (texture) |handle| {
        ig.ImDrawList_AddImage(draw_list, textureRef(handle), origin, corner);
    } else {
        ig.ImDrawList_AddRectFilled(draw_list, origin, corner, abgr(0x404040, 0xFF));
    }
    drawOverlays(state, draw_list, origin, fit[0], fit[1]);

    // A click or a drag inside the picture. The MFC's own stops at its edge.
    if (clicking) {
        const mouse = ig.igGetMousePos();
        const px = mouse.x - origin.x;
        const py = mouse.y - origin.y;
        if (px >= 0 and py >= 0 and px < fit[0] and py < fit[1]) _ = clickPixels(state, px, py);
    }
}

fn drawOverlays(state: *State, draw_list: *ig.ImDrawList, origin: ig.ImVec2, w: f32, h: f32) void {
    const mm = &state.minimap;
    const info = state.editor.document.info;

    // The patch grid: one dotted-looking line a patch, dark and faint.
    if (mm.show_grid) {
        const grid = abgr(0x000000, 0x70);
        for (0..logic.minimapGridCount(info.width_tiles, logic.minimap_grid_step_tiles)) |i| {
            const x = origin.x + logic.minimapGridFraction(i, info.width_tiles, logic.minimap_grid_step_tiles) * w;
            ig.ImDrawList_AddLine(draw_list, .{ .x = x, .y = origin.y }, .{ .x = x, .y = origin.y + h }, grid);
        }
        for (0..logic.minimapGridCount(info.height_tiles, logic.minimap_grid_step_tiles)) |i| {
            const y = origin.y + logic.minimapGridFraction(i, info.height_tiles, logic.minimap_grid_step_tiles) * h;
            ig.ImDrawList_AddLine(draw_list, .{ .x = origin.x, .y = y }, .{ .x = origin.x + w, .y = y }, grid);
        }
    }

    // The fire-range areas: the circle, and for a sector its two edges.
    for (mm.areas.items) |area| {
        const color = abgr(area.rgb, 0xFF);
        const centre = logic.minimapAiUnitsToPanel(area.cx, area.cy, w, h, info.width_tiles, info.height_tiles);
        const radius = area.radius * w / (@as(f32, @floatFromInt(info.width_tiles)) * logic.minimap_ai_units_per_tile);
        if (radius < 0.5) continue;
        const middle: ig.ImVec2 = .{ .x = origin.x + centre[0], .y = origin.y + centre[1] };
        ig.ImDrawList_AddCircleEx(draw_list, middle, radius, color, 48, 1.0);
        if (area.start_angle != area.finish_angle) {
            for ([2]i32{ area.start_angle, area.finish_angle }) |angle| {
                const end = logic.minimapSectorEnd(area.cx, area.cy, area.radius, angle);
                const edge = logic.minimapAiUnitsToPanel(end[0], end[1], w, h, info.width_tiles, info.height_tiles);
                ig.ImDrawList_AddLine(draw_list, middle, .{ .x = origin.x + edge[0], .y = origin.y + edge[1] }, color);
            }
        }
    }

    // The objects: their rectangles in the player's colour, at least two
    // pixels so a lone soldier shows.
    for (mm.units.items) |unit| {
        const top_left = logic.minimapAiTileToPanel(@floatFromInt(unit.x0), @floatFromInt(unit.y1), w, h, info.width_tiles, info.height_tiles);
        const bottom_right = logic.minimapAiTileToPanel(@floatFromInt(unit.x1), @floatFromInt(unit.y0), w, h, info.width_tiles, info.height_tiles);
        const min_x = origin.x + top_left[0];
        const min_y = origin.y + top_left[1];
        const max_x = @max(origin.x + bottom_right[0], min_x + 2);
        const max_y = @max(origin.y + bottom_right[1], min_y + 2);
        ig.ImDrawList_AddRectFilled(draw_list, .{ .x = min_x, .y = min_y }, .{ .x = max_x, .y = max_y }, abgr(logic.minimapPlayerColor(unit.color_index), 0xFF));
    }

    // The camera's frame: the four screen corners on the ground, drawn twice
    // (dark under light) so it reads on any terrain - the MFC's XOR lines.
    const screen = state.real.screenSize() orelse return;
    const sw: f32 = @floatFromInt(screen[0]);
    const sh: f32 = @floatFromInt(screen[1]);
    const corners = [4][2]f32{ .{ 0, 0 }, .{ 0, sh }, .{ sw, sh }, .{ sw, 0 } };
    var points: [4]ig.ImVec2 = undefined;
    for (corners, 0..) |c, i| {
        var wx: f32 = 0;
        var wy: f32 = 0;
        if (state.editor.bridge.screenToWorld(c[0], c[1], &wx, &wy) != .ok) return;
        const p = logic.minimapWorldToPanel(wx, wy, w, h, info.width_tiles, info.height_tiles);
        points[i] = .{ .x = origin.x + p[0], .y = origin.y + p[1] };
    }
    for (0..4) |i| ig.ImDrawList_AddLineEx(draw_list, points[i], points[(i + 1) % 4], abgr(0x000000, 0xFF), 3.0);
    for (0..4) |i| ig.ImDrawList_AddLineEx(draw_list, points[i], points[(i + 1) % 4], abgr(0xFFFFFF, 0xFF), 1.0);
}
