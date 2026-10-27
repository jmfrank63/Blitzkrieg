//! The Building editor's Sides window (M005 S08): the exported variant and
//! its three turns side by side, as the packs on disk hold them (a state, its
//! winter and its shadow selectable), Generate sides (`bk-views side`) and
//! Import side picture (`bk-sprite compose-pack` with the turn's footprint).
//! Both tools run beside the editor as child processes the frame polls, so
//! the window keeps drawing; every path and command line is sides_logic's.
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const kit = @import("editor_kit");
const views = @import("bk_views_pack");
const c_bridge = @import("c_bridge.zig");
const logic = @import("panels_logic.zig");
const sl = @import("sides_logic.zig");

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
    device: ?*anyopaque = null,
    /// The variant and pack the textures show; empty after a change on disk.
    key: [logic.path_capacity + 8]u8 = undefined,
    key_len: usize = 0,
    job: ?Job = null,
    note: [768]u8 = undefined,
    note_len: usize = 0,
    /// The last Generate or Import failed.
    failed: bool = false,
    /// Where each cell's picture was drawn in the last frame (null: missing), for the auto tier.
    shown: [sl.cells]?Rect = @splat(null),
    /// The auto tier's placement.
    override: ?Rect = null,

    pub fn init(gpa: std.mem.Allocator, io: std.Io) Sides {
        return .{ .gpa = gpa, .io = io };
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
        var settings: core_bridge.ModSettings = .{};
        if (b.modSettingsGet(&settings) != .ok) return;
        var buffer: [logic.path_capacity]u8 = undefined;
        if (sl.defaultVariant(&buffer, settings.exportDirSlice(), path)) |variant| self.setVariant(variant);
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
        const idle = self.job == null;
        ig.igBeginDisabled(!idle);
        if (ig.igButton(l10n.id("resource_app.sides.generate"))) self.generate();
        ig.igSameLine();
        if (ig.igButton(l10n.id("resource_app.sides.import"))) requestImport();
        ig.igSameLine();
        if (ig.igButton(l10n.id("resource_app.sides.reload"))) self.key_len = 0;
        ig.igEndDisabled();
        if (self.note_len != 0) ig.igTextWrapped("%.*s", @as(c_int, @intCast(self.note_len)), &self.note);
        ig.igPushTextWrapPos(0);
        ig.igTextDisabled("%s", l10n.t("resource_app.sides.hint"));
        ig.igPopTextWrapPos();
        self.load(real);
        self.drawCells();
    }

    /// The four cells: a turn's label (a click selects it for Import), its
    /// picture on the cell's ground at one scale for all four, or "missing".
    fn drawCells(self: *Sides) void {
        const avail = ig.igGetContentRegionAvail();
        const spacing: f32 = 8;
        const cell_w = @max(40, (avail.x - spacing * 3) / sl.cells);
        const cell_h = @max(40, avail.y - ig.igGetFrameHeightWithSpacing());
        var widest: f32 = 1;
        var tallest: f32 = 1;
        for (self.textures) |slot| if (slot) |t| {
            widest = @max(widest, t.width);
            tallest = @max(tallest, t.height);
        };
        const scale = @min(1, @min(cell_w / widest, cell_h / tallest));
        const draw_list = ig.igGetWindowDrawList();
        for (self.textures, 0..) |slot, k| {
            if (k > 0) ig.igSameLine();
            ig.igBeginGroup();
            var label: [64:0]u8 = @splat(0);
            const text = l10n.say(label[0 .. label.len - 1], "resource_app.sides.turn", .{k});
            var id: [80:0]u8 = @splat(0);
            _ = std.fmt.bufPrint(&id, "{s}##turn{d}", .{ text, k }) catch {};
            if (ig.igRadioButton(&id, self.selected == k)) self.selected = k;
            const top = ig.igGetCursorScreenPos();
            ig.ImDrawList_AddRectFilled(draw_list, top, .{ .x = top.x + cell_w, .y = top.y + cell_h }, ground_colour);
            if (slot) |t| {
                const size = ig.ImVec2{ .x = t.width * scale, .y = t.height * scale };
                ig.ImDrawList_AddImage(draw_list, .{ ._TexData = null, ._TexID = @intCast(@intFromPtr(t.texture)) }, top, .{ .x = top.x + size.x, .y = top.y + size.y });
                self.shown[k] = .{ .x = top.x, .y = top.y, .w = size.x, .h = size.y };
            } else {
                ig.igSetCursorScreenPos(.{ .x = top.x + 6, .y = top.y + 6 });
                ig.igTextDisabled("%s", l10n.t("resource_app.sides.missing"));
                ig.igSetCursorScreenPos(top);
            }
            ig.igDummy(.{ .x = cell_w, .y = cell_h });
            ig.igEndGroup();
        }
    }

    /// Decodes the four pictures of the current look once per variant and pack.
    fn load(self: *Sides, real: *c_bridge.RealResBridge) void {
        var pack_buffer: [4]u8 = undefined;
        const pack = self.look.pack(&pack_buffer);
        var key_buffer: [logic.path_capacity + 8]u8 = undefined;
        const key = std.fmt.bufPrint(&key_buffer, "{s}|{s}", .{ self.variantPath(), pack }) catch return;
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
            const folder = sl.turnFolder(&folder_buffer, self.variantPath(), k) catch continue;
            const picture = (sl.readPicture(arena_state.allocator(), self.io, folder, pack) catch null) orelse continue;
            const image = picture.image;
            const texture = kit.pictures_cache.upload(device.?, .{ .width = @intCast(image.width), .height = @intCast(image.height), .bytes = std.mem.sliceAsBytes(image.pixels) }) orelse continue;
            slot.* = .{ .texture = texture, .width = @floatFromInt(image.width), .height = @floatFromInt(image.height) };
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

    /// Generate sides: the variant's three turns, beside it.
    pub fn generate(self: *Sides) void {
        var arena_state = std.heap.ArenaAllocator.init(self.gpa);
        defer arena_state.deinit();
        const arena = arena_state.allocator();
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

    pub fn noteText(self: *const Sides) []const u8 {
        return self.note[0..self.note_len];
    }
};

const core_bridge = @import("resource_core").bridge;

/// Import side picture's file dialog: its answer arrives through `picked`.
var png_slot: kit.dialogs.PathSlot(enum { side_picture }, logic.path_capacity) = .{};
const png_filters = [_]sdl3.c.SDL_DialogFileFilter{.{ .name = "PNG", .pattern = "png" }};

fn requestImport() void {
    if (!png_slot.request(.side_picture)) return;
    sdl3.c.SDL_ShowOpenFileDialog(@TypeOf(png_slot).callback, &png_slot, null, &png_filters, png_filters.len, null, false);
}

/// The PNG the dialog chose, once.
pub fn picked() ?[]const u8 {
    return switch (png_slot.take() orelse return null) {
        .path => |answer| if (answer.path.len == 0) null else answer.path,
        .failed, .cancelled => null,
    };
}
