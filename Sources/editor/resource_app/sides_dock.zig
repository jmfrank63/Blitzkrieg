//! The Building editor's Sides window (M005 S08): the exported variant and
//! its three turns side by side, as the packs on disk hold them (a state, its
//! winter and its shadow selectable), Generate sides (`bk-views side`) and
//! Import side picture (`bk-sprite compose-pack` with the turn's footprint).
//! Both tools run beside the editor as child processes the frame polls, so
//! the window keeps drawing; every path and command line is sides_logic's.
//!
//! A project may hold a 3D model (Attach model, in `<project's folder>/model`):
//! Generate sides then draws every state from it at the four camera angles, turn
//! 0 included, and Replace atlas exchanges the art of the state shown. The
//! pictures are always the packs on disk, as bk-views renders them.
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const kit = @import("editor_kit");
const views = @import("bk_views_pack");
const c_bridge = @import("c_bridge.zig");
const logic = @import("panels_logic.zig");
const sl = @import("sides_logic.zig");
const compare_logic = @import("compare_logic.zig");

const ig = imgui.c;
const l10n = kit.l10n;
const c = c_bridge.c;
const Running = kit.testlaunch.Running;

/// A rectangle in screen pixels.
pub const Rect = struct { x: f32, y: f32, w: f32, h: f32 };

/// Each cell's ground, a colour no building sprite is made of, so the auto
/// tier tells a side's pixels from it (0xAABBGGRR: #2e3a6e).
pub const ground_colour: u32 = 0xff6e3a2e;
pub const ground_rgb = [3]u8{ 0x2e, 0x3a, 0x6e };

const exe_suffix = if (builtin.os.tag == .windows) ".exe" else "";

const Texture = struct { texture: *sdl3.c.SDL_GPUTexture, width: f32, height: f32 };

const Job = struct {
    running: Running,
    what: []const u8,
    /// The folder the tool's output and temporary pictures went to, deleted after.
    work: []u8,
    /// Import: the pack written, and whether the turn had a shadow pack for it.
    pack_path: ?[]u8 = null,
    had_shadow: bool = true,
    /// Generate sides drew from the project's model.
    from_model: bool = false,
};

pub const Sides = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    /// The variant folder shown, NUL-terminated for the text field.
    variant: [logic.path_capacity]u8 = @splat(0),
    /// The project the variant folder was last filled in for.
    project: [logic.path_capacity]u8 = @splat(0),
    look: sl.Look = .{},
    /// The cell Import side picture replaces a picture of.
    selected: usize = 1,
    textures: [sl.cells]?Texture = @splat(null),
    /// Each cell's anchor cross in its picture's image coordinates, from the
    /// same decode as the texture.
    anchors: [sl.cells]?[2]i32 = @splat(null),
    device: ?*anyopaque = null,
    /// The variant and pack the textures show; empty after a change on disk.
    key: [logic.path_capacity + 8]u8 = undefined,
    key_len: usize = 0,
    job: ?Job = null,
    note: [768]u8 = undefined,
    note_len: usize = 0,
    /// The Compare shipped 2003 toggle and everything it shows: the shipped
    /// building folder (editable), its picture's texture and anchor, and the
    /// selected cell's display transform. Nothing here writes; the compare
    /// never disturbs the generated turns.
    compare: bool = false,
    compare_folder: [logic.path_capacity]u8 = @splat(0),
    compare_texture: ?Texture = null,
    compare_anchor: ?[2]i32 = null,
    mode: compare_logic.Mode = .upright,
    /// The compare cell's shown rect, for the scenario's checks beside the
    /// regular cells' `shown`.
    shown_compare: ?Rect = null,
    /// The installation's `Data` folder, beside this editor's own
    /// executable: the shipped-2003 folder guesses root here, so the compare
    /// works no matter which directory the editor was started from.
    data_root: [logic.path_capacity]u8 = @splat(0),
    /// The last Generate or Import failed.
    failed: bool = false,
    /// Where each cell's picture was drawn in the last frame (null: missing), for the auto tier.
    shown: [sl.cells]?Rect = @splat(null),
    /// The auto tier's placement.
    override: ?Rect = null,
    /// Where the open project's model lives (`sl.modelStem`), and whether it has one.
    model_stem: [logic.path_capacity]u8 = @splat(0),
    has_model: bool = false,
    /// The auto tier's fewer turns: every picture but those it never looks at.
    model_turns: []const u8 = sl.model_turns,

    pub fn init(gpa: std.mem.Allocator, io: std.Io) Sides {
        var self: Sides = .{ .gpa = gpa, .io = io };
        if (std.process.executableDirPath(self.io, &self.data_root)) |len| {
            // The Data folder sits beside this editor's own executable; trim
            // to the filled length and keep it NUL-terminated.
            if (len + 1 < self.data_root.len) {
                self.data_root[len] = '/';
                self.data_root[len + 1] = 'D';
                self.data_root[len + 2] = 'a';
                self.data_root[len + 3] = 't';
                self.data_root[len + 4] = 'a';
                self.data_root[len + 5] = 0;
            }
        } else |_| {}
        return self;
    }

    pub fn deinit(self: *Sides) void {
        if (self.job) |*job| {
            job.running.terminate(self.io);
            _ = job.running.waitBlocking(self.io, 5000);
            self.endJob();
        }
        self.release();
    }

    fn release(self: *Sides) void {
        for (&self.textures) |*slot| {
            if (slot.*) |t| if (self.device) |device| kit.pictures_cache.releaseTexture(device, t.texture);
            slot.* = null;
        }
        if (self.compare_texture) |t| if (self.device) |device| kit.pictures_cache.releaseTexture(device, t.texture);
        self.compare_texture = null;
        self.compare_anchor = null;
        self.shown_compare = null;
        self.key_len = 0;
    }

    pub fn variantPath(self: *const Sides) []const u8 {
        return std.mem.sliceTo(&self.variant, 0);
    }

    pub fn setVariant(self: *Sides, path: []const u8) void {
        if (path.len >= self.variant.len) return;
        @memset(&self.variant, 0);
        @memcpy(self.variant[0..path.len], path);
    }

    fn say(self: *Sides, comptime key: []const u8, args: anytype) void {
        self.note_len = l10n.say(&self.note, key, args).len;
    }

    /// The note of a refused or failed tool, remembered for the auto tier.
    fn fail(self: *Sides, comptime key: []const u8, args: anytype) void {
        self.failed = true;
        self.say(key, args);
    }

    /// The cells drawn as missing in the last frame.
    pub fn missing(self: *const Sides) usize {
        var count: usize = 0;
        for (self.shown) |rect| count += @intFromBool(rect == null);
        return count;
    }

    /// Fills the variant folder for a newly opened project: where its export goes.
    fn follow(self: *Sides, b: core_bridge.ResBridge, project: ?[]const u8) void {
        const path = project orelse return;
        if (std.mem.eql(u8, std.mem.sliceTo(&self.project, 0), path) or path.len >= self.project.len) return;
        @memset(&self.project, 0);
        @memcpy(self.project[0..path.len], path);
        self.findModel(path);
        var settings: core_bridge.ModSettings = .{};
        if (b.modSettingsGet(&settings) != .ok) return;
        var buffer: [logic.path_capacity]u8 = undefined;
        if (sl.defaultVariant(&buffer, settings.exportDirSlice(), path)) |variant| {
            self.setVariant(variant);
            // The compare cell's first guess: the same building in the
            // installation's shipped data, absolute so it reads no matter
            // which directory the editor was started from.
            if (compare_logic.spineAfterBuildings(variant)) |spine| {
                const data_root = std.mem.sliceTo(&self.data_root, 0);
                if (data_root.len != 0) {
                    if (std.fmt.bufPrint(&self.compare_folder, "{s}/{s}", .{ data_root, spine })) |_| {
                        self.key_len = 0;
                    } else |_| {}
                }
            }
        }
    }

    /// The window, for an open Building project.
    pub fn draw(self: *Sides, real: *c_bridge.RealResBridge, project: ?[]const u8) void {
        self.follow(real.bridge(), project);
        self.poll();
        if (self.override) |r| {
            ig.igSetNextWindowPos(.{ .x = r.x, .y = r.y }, ig.ImGuiCond_Always);
            ig.igSetNextWindowSize(.{ .x = r.w, .y = r.h }, ig.ImGuiCond_Always);
        } else {
            ig.igSetNextWindowPos(.{ .x = 300, .y = 420 }, ig.ImGuiCond_FirstUseEver);
            ig.igSetNextWindowSize(.{ .x = 900, .y = 380 }, ig.ImGuiCond_FirstUseEver);
        }
        defer ig.igEnd();
        self.shown = @splat(null);
        self.shown_compare = null;
        if (!ig.igBegin(l10n.id("resource_app.window.sides"), null, 0)) return;
        if (ig.igInputText(l10n.id("resource_app.sides.variant"), &self.variant, self.variant.len - 1, 0)) self.key_len = 0;
        ig.igTextDisabled("%s", l10n.t("resource_app.sides.state"));
        var state: u8 = 1;
        while (state <= 3) : (state += 1) {
            ig.igSameLine();
            var label: [8:0]u8 = @splat(0);
            _ = std.fmt.bufPrint(&label, "{d}##state", .{state}) catch unreachable;
            if (ig.igRadioButton(&label, self.look.state == state)) self.look.state = state;
        }
        ig.igSameLine();
        _ = ig.igCheckbox(l10n.id("resource_app.sides.winter"), &self.look.winter);
        ig.igSameLine();
        _ = ig.igCheckbox(l10n.id("resource_app.sides.shadow"), &self.look.shadow);
        ig.igSameLine();
        if (ig.igCheckbox(l10n.id("resource_app.sides.compare"), &self.compare)) self.key_len = 0;
        if (self.compare) {
            if (ig.igInputText(l10n.id("resource_app.sides.compare_folder"), &self.compare_folder, self.compare_folder.len - 1, 0)) self.key_len = 0;
            ig.igSameLine();
            if (ig.igButton(l10n.id("resource_app.sides.mirror"))) {
                self.mode = if (self.mode == .mirrored) .upright else .mirrored;
                self.key_len = 0;
            }
            ig.igSameLine();
            if (ig.igButton(l10n.id("resource_app.sides.rotate"))) {
                self.mode = if (self.mode == .rotated_180) .upright else .rotated_180;
                self.key_len = 0;
            }
        } else self.mode = .upright;
        const idle = self.job == null;
        ig.igBeginDisabled(!idle);
        if (ig.igButton(l10n.id("resource_app.sides.generate"))) self.generate();
        ig.igSameLine();
        if (ig.igButton(l10n.id("resource_app.sides.import"))) requestImport();
        ig.igSameLine();
        if (ig.igButton(l10n.id("resource_app.sides.reload"))) self.key_len = 0;
        ig.igSameLine();
        if (ig.igButton(l10n.id("resource_app.sides.attach_model"))) requestPick(.model);
        if (self.has_model) {
            ig.igSameLine();
            if (ig.igButton(l10n.id("resource_app.sides.replace_atlas"))) requestPick(.atlas);
        }
        ig.igEndDisabled();
        if (self.note_len != 0) ig.igTextWrapped("%.*s", @as(c_int, @intCast(self.note_len)), &self.note);
        ig.igPushTextWrapPos(0);
        ig.igTextDisabled("%s", l10n.t("resource_app.sides.hint"));
        ig.igPopTextWrapPos();
        self.load(real);
        self.drawCells();
    }

    /// The four cells — a turn's label (a click selects it for Import), its
    /// picture on the cell's ground at one scale, or "missing" — and, with
    /// the compare on, the shipped 2003 picture in a leading cell. With the
    /// compare on every cell also shows its anchor cross, so the two sides'
    /// crosses can be judged against each other.
    fn drawCells(self: *Sides) void {
        const compare_cell = self.compare and self.compare_texture != null;
        const total: usize = sl.cells + if (compare_cell) @as(usize, 1) else 0;
        const avail = ig.igGetContentRegionAvail();
        const spacing: f32 = 8;
        const cell_w = @max(40, (avail.x - spacing * @as(f32, @floatFromInt(total - 1))) / @as(f32, @floatFromInt(total)));
        const cell_h = @max(40, avail.y - ig.igGetFrameHeightWithSpacing());
        var widest: f32 = 1;
        var tallest: f32 = 1;
        for (self.textures) |slot| if (slot) |t| {
            widest = @max(widest, t.width);
            tallest = @max(tallest, t.height);
        };
        if (self.compare_texture) |t| {
            widest = @max(widest, t.width);
            tallest = @max(tallest, t.height);
        }
        const scale = @min(1, @min(cell_w / widest, cell_h / tallest));
        const draw_list = ig.igGetWindowDrawList();
        var index: usize = 0;
        while (index < total) : (index += 1) {
            if (index > 0) ig.igSameLine();
            ig.igBeginGroup();
            const compare_here = compare_cell and index == 0;
            var top = ig.igGetCursorScreenPos();
            if (compare_here) {
                var label: [64:0]u8 = @splat(0);
                const text = l10n.say(label[0 .. label.len - 1], "resource_app.sides.original", .{});
                ig.igTextDisabled("%s", text.ptr);
                top = ig.igGetCursorScreenPos();
                ig.ImDrawList_AddRectFilled(draw_list, top, .{ .x = top.x + cell_w, .y = top.y + cell_h }, ground_colour);
                if (self.compare_texture) |t| {
                    const size = ig.ImVec2{ .x = t.width * scale, .y = t.height * scale };
                    ig.ImDrawList_AddImage(draw_list, .{ ._TexData = null, ._TexID = @intCast(@intFromPtr(t.texture)) }, top, .{ .x = top.x + size.x, .y = top.y + size.y });
                    self.shown_compare = .{ .x = top.x, .y = top.y, .w = size.x, .h = size.y };
                    if (self.compare_anchor) |anchor| {
                        const cx = top.x + @as(f32, @floatFromInt(anchor[0])) * scale;
                        const cy = top.y + @as(f32, @floatFromInt(anchor[1])) * scale;
                        const cross: u32 = 0xffffffff;
                        ig.ImDrawList_AddLine(draw_list, .{ .x = cx - 5, .y = cy }, .{ .x = cx + 5, .y = cy }, cross);
                        ig.ImDrawList_AddLine(draw_list, .{ .x = cx, .y = cy - 5 }, .{ .x = cx, .y = cy + 5 }, cross);
                    }
                } else {
                    ig.igSetCursorScreenPos(.{ .x = top.x + 6, .y = top.y + 6 });
                    ig.igTextDisabled("%s", l10n.t("resource_app.sides.missing"));
                    ig.igSetCursorScreenPos(top);
                }
                ig.igDummy(.{ .x = cell_w, .y = cell_h });
                ig.igEndGroup();
                continue;
            }
            const k: usize = if (compare_cell) index - 1 else index;
            var label: [64:0]u8 = @splat(0);
            const text = l10n.say(label[0 .. label.len - 1], "resource_app.sides.turn", .{k});
            var id: [80:0]u8 = @splat(0);
            _ = std.fmt.bufPrint(&id, "{s}##turn{d}", .{ text, k }) catch {};
            if (ig.igRadioButton(&id, self.selected == k)) {
                self.selected = k;
                // The transform follows the selection: reload so the
                // freshly selected cell shows it.
                if (self.mode != .upright) self.key_len = 0;
            }
            top = ig.igGetCursorScreenPos();
            ig.ImDrawList_AddRectFilled(draw_list, top, .{ .x = top.x + cell_w, .y = top.y + cell_h }, ground_colour);
            const cell_texture = self.textures[k];
            const cell_anchor = self.anchors[k];
            if (cell_texture) |t| {
                const size = ig.ImVec2{ .x = t.width * scale, .y = t.height * scale };
                ig.ImDrawList_AddImage(draw_list, .{ ._TexData = null, ._TexID = @intCast(@intFromPtr(t.texture)) }, top, .{ .x = top.x + size.x, .y = top.y + size.y });
                self.shown[k] = .{ .x = top.x, .y = top.y, .w = size.x, .h = size.y };
                // The anchor cross, only while comparing: the two sides'
                // crosses sit at the same image coordinates when the
                // generated turn fits the original.
                if (compare_cell) {
                    if (cell_anchor) |anchor| {
                        const cx = top.x + @as(f32, @floatFromInt(anchor[0])) * scale;
                        const cy = top.y + @as(f32, @floatFromInt(anchor[1])) * scale;
                        const cross: u32 = 0xffffffff;
                        ig.ImDrawList_AddLine(draw_list, .{ .x = cx - 5, .y = cy }, .{ .x = cx + 5, .y = cy }, cross);
                        ig.ImDrawList_AddLine(draw_list, .{ .x = cx, .y = cy - 5 }, .{ .x = cx, .y = cy + 5 }, cross);
                    }
                }
            } else {
                ig.igSetCursorScreenPos(.{ .x = top.x + 6, .y = top.y + 6 });
                ig.igTextDisabled("%s", l10n.t("resource_app.sides.missing"));
                ig.igSetCursorScreenPos(top);
            }
            ig.igDummy(.{ .x = cell_w, .y = cell_h });
            ig.igEndGroup();
        }
    }

    /// Decodes the four pictures of the current look once per variant, pack,
    /// compare folder and display mode.
    fn load(self: *Sides, real: *c_bridge.RealResBridge) void {
        var pack_buffer: [4]u8 = undefined;
        const pack = self.look.pack(&pack_buffer);
        var key_buffer: [logic.path_capacity * 2 + 16]u8 = undefined;
        const key = std.fmt.bufPrint(&key_buffer, "{s}|{s}|{s}|{d}", .{ self.variantPath(), pack, std.mem.sliceTo(&self.compare_folder, 0), @intFromEnum(self.mode) }) catch return;
        if (self.key_len == key.len and std.mem.eql(u8, self.key[0..self.key_len], key)) return;
        self.release();
        @memcpy(self.key[0..key.len], key);
        self.key_len = key.len;
        var device: ?*anyopaque = null;
        var format: c_uint = 0;
        if (c.BkEditorGpuDevice(real.session, &device, &format) != c.BK_EDITOR_OK or device == null) {
            self.say("resource_app.image.no_device", .{});
            return;
        }
        self.device = device;
        for (&self.textures, 0..) |*slot, k| {
            var arena_state = std.heap.ArenaAllocator.init(self.gpa);
            defer arena_state.deinit();
            var folder_buffer: [logic.path_capacity + 8]u8 = undefined;
            // A model's cell shows what the Game draws at that camera step:
            // the turn under the sun fixed in the world, where it was made.
            var lit: ?sl.Picture = null;
            if (self.has_model) {
                const lit_folder = sl.litFolder(&folder_buffer, self.variantPath(), k) catch continue;
                lit = sl.readPicture(arena_state.allocator(), self.io, lit_folder, pack) catch null;
            }
            const folder = sl.turnFolder(&folder_buffer, self.variantPath(), k) catch continue;
            const picture = lit orelse (sl.readPicture(arena_state.allocator(), self.io, folder, pack) catch null) orelse continue;
            // The selected cell carries the display transform: what the
            // maintainer judges here is exactly what a mirrored pack would
            // ship. The other cells stay upright.
            const shown = if (self.mode != .upright and k == self.selected)
                (compare_logic.transformPicture(arena_state.allocator(), picture, self.mode) catch picture)
            else
                picture;
            self.anchors[k] = shown.anchor;
            const image = shown.image;
            const texture = kit.pictures_cache.upload(device.?, .{ .width = @intCast(image.width), .height = @intCast(image.height), .bytes = std.mem.sliceAsBytes(image.pixels) }) orelse continue;
            slot.* = .{ .texture = texture, .width = @floatFromInt(image.width), .height = @floatFromInt(image.height) };
        }
        // The compare cell: the shipped 2003 building's own pack for the same
        // look, upright whatever the selected cell's transform is. A folder
        // typed relative reads where the editor was started from first, then
        // beside its own Data — the prefill is absolute, but a hand-typed
        // one need not be.
        self.compare_texture = null;
        self.compare_anchor = null;
        const compare_folder = std.mem.sliceTo(&self.compare_folder, 0);
        if (self.compare and compare_folder.len != 0) {
            var arena_state = std.heap.ArenaAllocator.init(self.gpa);
            defer arena_state.deinit();
            const data_root = std.mem.sliceTo(&self.data_root, 0);
            var picture = (compare_logic.originalPicture(arena_state.allocator(), self.io, compare_folder, self.look) catch null);
            if (picture == null and !std.fs.path.isAbsolute(compare_folder) and data_root.len != 0) {
                const rooted = std.fmt.allocPrint(arena_state.allocator(), "{s}/{s}", .{ data_root, compare_folder }) catch null;
                if (rooted) |r| picture = (compare_logic.originalPicture(arena_state.allocator(), self.io, r, self.look) catch null);
            }
            if (picture) |p| {
                self.compare_anchor = p.anchor;
                const image = p.image;
                const texture = kit.pictures_cache.upload(device.?, .{ .width = @intCast(image.width), .height = @intCast(image.height), .bytes = std.mem.sliceAsBytes(image.pixels) });
                if (texture) |t| self.compare_texture = .{ .texture = t, .width = @floatFromInt(image.width), .height = @floatFromInt(image.height) };
            }
        }
    }

    // --- The tools -----------------------------------------------------------

    /// A tool installed beside the editor.
    fn toolPath(self: *Sides, arena: std.mem.Allocator, name: []const u8) ![]const u8 {
        var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const len = try std.process.executableDirPath(self.io, &buffer);
        return std.fmt.allocPrint(arena, "{s}{c}{s}{s}", .{ buffer[0..len], std.fs.path.sep, name, exe_suffix });
    }

    /// A work folder beside the variant, `.<name>.views-work`, made empty.
    fn workFolder(self: *Sides, variant: []const u8) ![]u8 {
        const work = try std.fmt.allocPrint(self.gpa, "{s}{c}.{s}.views-work", .{ std.fs.path.dirname(variant) orelse ".", std.fs.path.sep, std.fs.path.basename(variant) });
        errdefer self.gpa.free(work);
        std.Io.Dir.cwd().deleteTree(self.io, work) catch {};
        try std.Io.Dir.cwd().createDirPath(self.io, work);
        return work;
    }

    /// Runs `argv` with its output in `work`/tool.log; the job owns `work`
    /// and `pack_path` from here, and frees them on failure too.
    fn start(self: *Sides, what: []const u8, argv: []const []const u8, work: []u8, pack_path: ?[]u8, had_shadow: bool) void {
        self.spawn(argv, work) catch |err| {
            std.Io.Dir.cwd().deleteTree(self.io, work) catch {};
            self.gpa.free(work);
            if (pack_path) |p| self.gpa.free(p);
            return self.fail("resource_app.sides.failed", .{ what, @errorName(err) });
        };
        self.job.?.what = what;
        self.job.?.pack_path = pack_path;
        self.job.?.had_shadow = had_shadow;
        self.failed = false;
        self.say("resource_app.sides.running", .{what});
    }

    fn spawn(self: *Sides, argv: []const []const u8, work: []u8) !void {
        var buffer: [logic.path_capacity + 16]u8 = undefined;
        var log = try std.Io.Dir.cwd().createFile(self.io, try std.fmt.bufPrint(&buffer, "{s}{c}tool.log", .{ work, std.fs.path.sep }), .{ .truncate = true });
        defer log.close(self.io);
        const child = try std.process.spawn(self.io, .{ .argv = argv, .stdin = .ignore, .stdout = .{ .file = log }, .stderr = .{ .file = log }, .create_no_window = true });
        self.job = .{ .running = .{ .child = child, .started = std.Io.Clock.Timestamp.now(self.io, .awake) }, .what = "", .work = work };
    }

    fn modelStemPath(self: *const Sides) []const u8 {
        return std.mem.sliceTo(&self.model_stem, 0);
    }

    /// Whether the project's folder holds a model.
    fn findModel(self: *Sides, project: []const u8) void {
        @memset(&self.model_stem, 0);
        self.has_model = false;
        const stem = sl.modelStem(&self.model_stem, project) orelse return;
        var buffer: [logic.path_capacity + 8]u8 = undefined;
        const json = std.fmt.bufPrint(&buffer, "{s}.json", .{stem}) catch return;
        std.Io.Dir.cwd().access(self.io, json, .{}) catch return;
        self.has_model = true;
    }

    /// A PNG as bk-views reads it, written over `path`.
    fn writeAtlas(self: *Sides, arena: std.mem.Allocator, path: []const u8, image: views.Image) !void {
        try std.Io.Dir.cwd().writeFile(self.io, .{ .sub_path = path, .data = try views.writePngDeflated(arena, image) });
    }

    /// Attach model: `path`, `<name>.json`, and its atlases beside it become the
    /// project's model. Nothing changes unless all of them read.
    pub fn attachModel(self: *Sides, path: []const u8) void {
        var arena_state = std.heap.ArenaAllocator.init(self.gpa);
        defer arena_state.deinit();
        self.attachIn(arena_state.allocator(), path) catch |err| self.fail("resource_app.sides.model_bad", .{ path, @errorName(err) });
    }

    fn attachIn(self: *Sides, arena: std.mem.Allocator, path: []const u8) !void {
        const cwd = std.Io.Dir.cwd();
        if (!std.mem.endsWith(u8, path, ".json")) return error.NotAModel;
        const from = path[0 .. path.len - ".json".len];
        if (self.modelStemPath().len == 0) return error.NoProject;
        var parts: [sl.model_suffixes.len]?[]const u8 = @splat(null);
        for (sl.model_suffixes, &parts) |suffix, *part| {
            const bytes = cwd.readFileAlloc(self.io, try std.fmt.allocPrint(arena, "{s}{s}", .{ from, suffix }), arena, .limited(64 << 20)) catch |err| switch (err) {
                error.FileNotFound => continue,
                else => return err,
            };
            part.* = bytes;
        }
        const intact = parts[0] orelse return error.NotAModel;
        if (parts[1] == null) return error.NoAtlas;
        // Each atlas read, against the layout it lies on.
        const sizes = [_]?[2]u32{ try sl.checkModel(arena, intact), if (parts[3]) |j| try sl.checkModel(arena, j) else null, if (parts[6]) |j| try sl.checkModel(arena, j) else null };
        var images: [sl.model_suffixes.len]?views.Image = @splat(null);
        for (sl.model_suffixes, parts, &images) |suffix, part, *image| {
            const bytes = part orelse continue;
            if (!std.mem.endsWith(u8, suffix, ".png")) continue;
            const atlas = sl.readPng(arena, bytes) catch return error.NotAPng;
            const size = (if (suffix[1] == '2') sizes[1] else if (suffix[1] == '3') sizes[2] else null) orelse sizes[0].?;
            if (atlas.width != size[0] or atlas.height != size[1]) return error.AtlasSize;
            image.* = atlas;
        }
        const stem = try arena.dupe(u8, self.modelStemPath());
        try cwd.createDirPath(self.io, std.fs.path.dirname(stem).?);
        for (sl.model_suffixes, parts, images) |suffix, part, image| {
            const to = try std.fmt.allocPrint(arena, "{s}{s}", .{ stem, suffix });
            if (image) |i| {
                try self.writeAtlas(arena, to, i);
            } else if (part) |bytes| {
                try cwd.writeFile(self.io, .{ .sub_path = to, .data = bytes });
            } else cwd.deleteFile(self.io, to) catch {};
        }
        self.has_model = true;
        self.failed = false;
        self.say("resource_app.sides.model_attached", .{std.fs.path.basename(path)});
    }

    /// Replace atlas: `png` becomes the art of the state shown, drawn at once.
    pub fn replaceAtlas(self: *Sides, png: []const u8) void {
        var arena_state = std.heap.ArenaAllocator.init(self.gpa);
        defer arena_state.deinit();
        self.replaceIn(arena_state.allocator(), png) catch |err| self.fail("resource_app.sides.model_bad", .{ png, @errorName(err) });
    }

    fn replaceIn(self: *Sides, arena: std.mem.Allocator, png: []const u8) !void {
        const cwd = std.Io.Dir.cwd();
        const stem = self.modelStemPath();
        const new = sl.readPng(arena, try cwd.readFileAlloc(self.io, png, arena, .limited(64 << 20))) catch return self.fail("resource_app.sides.not_png", .{png});
        var layout_buffer: [16]u8 = undefined;
        const own = cwd.readFileAlloc(self.io, try std.fmt.allocPrint(arena, "{s}{s}", .{ stem, sl.layoutSuffix(&layout_buffer, self.look.state) }), arena, .limited(64 << 20)) catch |err| switch (err) {
            error.FileNotFound => try cwd.readFileAlloc(self.io, try std.fmt.allocPrint(arena, "{s}.json", .{stem}), arena, .limited(64 << 20)),
            else => return err,
        };
        const size = try sl.checkModel(arena, own);
        if (new.width != size[0] or new.height != size[1]) return self.fail("resource_app.sides.atlas_size", .{ png, new.width, new.height, size[0], size[1] });
        var suffix_buffer: [16]u8 = undefined;
        try self.writeAtlas(arena, try std.fmt.allocPrint(arena, "{s}{s}", .{ stem, sl.atlasSuffix(&suffix_buffer, self.look) }), new);
        self.generate();
    }

    /// A file of `from` to `to`, made empty first; subfolders are not a variant's.
    fn copyFolder(self: *Sides, arena: std.mem.Allocator, from: []const u8, to: []const u8) !void {
        const cwd = std.Io.Dir.cwd();
        try cwd.createDirPath(self.io, to);
        var dir = try cwd.openDir(self.io, from, .{ .iterate = true });
        defer dir.close(self.io);
        var walker = dir.iterate();
        while (try walker.next(self.io)) |entry| {
            if (entry.kind != .file) continue;
            const bytes = try cwd.readFileAlloc(self.io, try std.fs.path.join(arena, &.{ from, entry.name }), arena, .limited(256 << 20));
            try cwd.writeFile(self.io, .{ .sub_path = try std.fs.path.join(arena, &.{ to, entry.name }), .data = bytes });
        }
    }

    /// Generate sides of a variant with a model: from the variant as first exported.
    fn generateFromModel(self: *Sides, arena: std.mem.Allocator) !void {
        const cwd = std.Io.Dir.cwd();
        const what = "bk-views side";
        const variant = self.variantPath();
        const stem = self.modelStemPath();
        var source_buffer: [logic.path_capacity]u8 = undefined;
        const source = sl.modelSource(&source_buffer, stem, variant) orelse return error.NameTooLong;
        cwd.access(self.io, try std.fmt.allocPrint(arena, "{s}{c}1.san", .{ source, std.fs.path.sep }), .{}) catch {
            cwd.access(self.io, try std.fmt.allocPrint(arena, "{s}{c}1.san", .{ variant, std.fs.path.sep }), .{}) catch return self.fail("resource_app.sides.no_variant", .{variant});
            try self.copyFolder(arena, variant, source);
        };
        const tool = try self.toolPath(arena, "bk-views");
        const work = try self.workFolder(variant);
        errdefer self.gpa.free(work);
        const argv = try sl.modelSideArgv(arena, tool, source, variant, work, stem, self.model_turns);
        self.start(what, argv, work, null, true);
        if (self.job) |*job| job.from_model = true;
    }

    /// Generate sides: the variant's three turns, beside it.
    pub fn generate(self: *Sides) void {
        var arena_state = std.heap.ArenaAllocator.init(self.gpa);
        defer arena_state.deinit();
        const arena = arena_state.allocator();
        if (self.has_model) {
            self.generateFromModel(arena) catch |err| self.fail("resource_app.sides.failed", .{ "bk-views side", @errorName(err) });
            return;
        }
        const variant = self.variantPath();
        std.Io.Dir.cwd().access(self.io, std.fmt.allocPrint(arena, "{s}{c}1.san", .{ variant, std.fs.path.sep }) catch return, .{}) catch {
            self.fail("resource_app.sides.no_variant", .{variant});
            return;
        };
        const what = "bk-views side";
        const tool = self.toolPath(arena, "bk-views") catch |err| return self.fail("resource_app.sides.failed", .{ what, @errorName(err) });
        const work = self.workFolder(variant) catch |err| return self.fail("resource_app.sides.failed", .{ what, @errorName(err) });
        const argv = sl.sideArgv(arena, tool, variant, work) catch |err| {
            self.gpa.free(work);
            return self.fail("resource_app.sides.failed", .{ what, @errorName(err) });
        };
        self.start(what, argv, work, null, true);
    }

    /// Import side picture: `png` becomes the selected cell's picture of the
    /// current state (its sprite, the shadow kept), composed with that turn's
    /// footprint, its zero cross where the old picture had it.
    pub fn import(self: *Sides, png: []const u8) void {
        var arena_state = std.heap.ArenaAllocator.init(self.gpa);
        defer arena_state.deinit();
        const arena = arena_state.allocator();
        self.importIn(arena, png) catch |err| self.fail("resource_app.sides.failed", .{ "bk-sprite compose-pack", @errorName(err) });
    }

    fn importIn(self: *Sides, arena: std.mem.Allocator, png: []const u8) !void {
        const cwd = std.Io.Dir.cwd();
        var folder_buffer: [logic.path_capacity + 8]u8 = undefined;
        const folder = try arena.dupe(u8, try sl.turnFolder(&folder_buffer, self.variantPath(), self.selected));
        var name_buffer: [4]u8 = undefined;
        const name = try arena.dupe(u8, self.look.sprite().pack(&name_buffer));
        const old = try sl.readPicture(arena, self.io, folder, name) orelse return self.fail("resource_app.sides.no_picture", .{ folder, name });
        const new = sl.readPng(arena, try cwd.readFileAlloc(self.io, png, arena, .limited(64 << 20))) catch return self.fail("resource_app.sides.not_png", .{png});
        const print = try sl.footprint(arena, try cwd.readFileAlloc(self.io, try std.fs.path.join(arena, &.{ folder, "1.xml" }), arena, .limited(16 << 20)));
        const tool = try self.toolPath(arena, "bk-sprite");
        const out_name = try std.fs.path.join(arena, &.{ folder, name });
        // One canvas for the new picture, the shadow and the ground picture as
        // they are, from the zero cross's corner, and a row below to spare: a
        // picture over all its shadow keeps one shadow pixel there.
        const shadow = try sl.readPicture(arena, self.io, folder, try std.fmt.allocPrint(arena, "{s}s", .{name}));
        const ground = try sl.readPicture(arena, self.io, folder, try std.fmt.allocPrint(arena, "{s}g", .{name}));
        var size = [2]u32{ @max(new.width, old.image.width), @max(new.height, old.image.height) };
        for ([_]?sl.Picture{ shadow, ground }) |picture| if (picture) |p| {
            size = .{ @max(size[0], p.image.width), @max(size[1], p.image.height) };
        };
        size[1] += 1;
        const empty: sl.Picture = .{ .image = .{ .width = 0, .height = 0, .pixels = &.{} }, .anchor = old.anchor };
        const sprite = try sl.placed(arena, .{ .image = new, .anchor = old.anchor }, size, old.anchor);
        const shade = try sl.placed(arena, shadow orelse empty, size, old.anchor);
        views.keepShadow(sprite, shade);
        const work = try self.workFolder(folder);
        errdefer self.gpa.free(work);
        var paths: [3][]const u8 = undefined;
        for ([_]?views.Image{ sprite, shade, if (ground) |g| try sl.placed(arena, g, size, old.anchor) else null }, [_][]const u8{ "sprite", "shadow", "ground" }, &paths) |image, file, *path| {
            path.* = try std.fmt.allocPrint(arena, "{s}{c}{s}.png", .{ work, std.fs.path.sep, file });
            if (image) |i| try cwd.writeFile(self.io, .{ .sub_path = path.*, .data = try views.writePng(arena, i) });
        }
        const argv = try sl.composeArgv(arena, .{
            .tool = tool,
            .picture = paths[0],
            .shadow = paths[1],
            .out_name = out_name,
            .anchor = old.anchor,
            .footprint = print,
            .ground = if (ground != null) paths[2] else null,
            .icon = if (self.look.state == 1 and !self.look.winter) try std.fs.path.join(arena, &.{ folder, "icon.tga" }) else null,
        });
        const pack_path = try self.gpa.dupe(u8, out_name);
        self.start("bk-sprite compose-pack", argv, work, pack_path, shadow != null);
    }

    /// The running tool's end, once a frame: the note, the cleanup, the pictures read again.
    fn poll(self: *Sides) void {
        if (self.job) |*job| if (job.running.poll(self.io)) |exit| self.finish(exit);
    }

    fn finish(self: *Sides, exit: kit.testlaunch.Exit) void {
        const job = &self.job.?;
        if (exit.code != null and exit.code.? == 0) {
            if (job.pack_path) |pack| {
                // compose-pack always packs a shadow; a picture that had none keeps none.
                if (!job.had_shadow) for ([_][]const u8{ ".san", "_c.dds", "_h.dds", "_l.dds" }) |suffix| {
                    var buffer: [logic.path_capacity + 16]u8 = undefined;
                    const path = std.fmt.bufPrint(&buffer, "{s}s{s}", .{ pack, suffix }) catch continue;
                    std.Io.Dir.cwd().deleteFile(self.io, path) catch {};
                };
                self.say("resource_app.sides.imported", .{ std.fs.path.basename(pack), std.fs.path.dirname(pack) orelse "" });
            } else if (job.from_model) {
                self.say("resource_app.sides.model_generated", .{self.variantPath()});
            } else self.say("resource_app.sides.generated", .{self.variantPath()});
        } else {
            // The tool's last line says why.
            var buffer: [logic.path_capacity + 16]u8 = undefined;
            const log_path = std.fmt.bufPrint(&buffer, "{s}{c}tool.log", .{ job.work, std.fs.path.sep }) catch "";
            const log = std.Io.Dir.cwd().readFileAlloc(self.io, log_path, self.gpa, .limited(1 << 20)) catch null;
            defer if (log) |l| self.gpa.free(l);
            const text = std.mem.trim(u8, log orelse "", " \r\n");
            const last = text[if (std.mem.lastIndexOfScalar(u8, text, '\n')) |at| at + 1 else 0..];
            self.fail("resource_app.sides.failed", .{ job.what, if (last.len > 0) last else "exit" });
        }
        self.endJob();
        self.key_len = 0;
    }

    fn endJob(self: *Sides) void {
        const job = &self.job.?;
        std.Io.Dir.cwd().deleteTree(self.io, job.work) catch {};
        self.gpa.free(job.work);
        if (job.pack_path) |p| self.gpa.free(p);
        self.job = null;
    }

    /// The auto tier's wait: the running tool to its end, or false after `seconds`.
    pub fn waitJob(self: *Sides, seconds: u32) bool {
        if (self.job) |*job| self.finish(job.running.waitBlocking(self.io, seconds * 1000) orelse return false);
        return true;
    }

    /// What a file dialog chose, once.
    pub fn takePicked(self: *Sides) void {
        switch (pick_slot.take() orelse return) {
            .path => |answer| if (answer.path.len != 0) switch (answer.kind) {
                .side_picture => self.import(answer.path),
                .model => self.attachModel(answer.path),
                .atlas => self.replaceAtlas(answer.path),
            },
            .failed, .cancelled => {},
        }
    }

    pub fn noteText(self: *const Sides) []const u8 {
        return self.note[0..self.note_len];
    }
};

const core_bridge = @import("resource_core").bridge;

/// The window's file dialogs: the answer arrives through `Sides.takePicked`.
const Pick = enum { side_picture, model, atlas };
var pick_slot: kit.dialogs.PathSlot(Pick, logic.path_capacity) = .{};
const png_filters = [_]sdl3.c.SDL_DialogFileFilter{.{ .name = "PNG", .pattern = "png" }};
const json_filters = [_]sdl3.c.SDL_DialogFileFilter{.{ .name = "JSON", .pattern = "json" }};

fn requestImport() void {
    requestPick(.side_picture);
}

fn requestPick(kind: Pick) void {
    if (!pick_slot.request(kind)) return;
    const filters: []const sdl3.c.SDL_DialogFileFilter = if (kind == .model) &json_filters else &png_filters;
    sdl3.c.SDL_ShowOpenFileDialog(@TypeOf(pick_slot).callback, &pick_slot, null, filters.ptr, @intCast(filters.len), null, false);
}
