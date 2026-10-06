//! The GUI sub-editor's canvas, with no window and no ImGui: the 1024 x 768
//! screen scaled to fit its dock, the selection (a primary and others: click,
//! shift-click, rubber band), and the press-move-release gestures of
//! GUIFrame2's modes (free, select, drag, resize, draw). A gesture only
//! previews while the button is down; the release builds ONE command through
//! resource_core's gui_tools and commits it, so a gesture is one undo step and
//! Escape leaves no history entry and nothing to roll back. The palette,
//! align and equal size, arrow nudge, delete, and copy, cut and paste through
//! an app clipboard are decided here for the same reason, as is the template
//! "Create new template" writes (the user's own folder only).
//! Runs under `zig build test-resource-app-logic` against the fake bridge.
const std = @import("std");
const core = @import("resource_core");

const tools = core.sub_editor_tools;
const bridge_mod = core.bridge;
const geometry = core.gui_geometry;
const gui_tools = core.gui_tools;
const ResBridge = bridge_mod.ResBridge;
const GuiWindow = bridge_mod.GuiWindow;
const GuiRect = bridge_mod.GuiRect;
const Point2 = bridge_mod.Point2;
const Rect = geometry.Rect;
const Document = core.document.Document;
const History = core.history.History;

pub const Error = bridge_mod.EditError;

/// The canvas the UI screens are laid out on (the game's 1024 x 768).
pub const canvas_width = geometry.canvas_width;
pub const canvas_height = geometry.canvas_height;

/// GUIFrame2's modes: nothing in progress, a rubber band, moving the
/// selection, resizing the primary, drawing a new control from the palette.
pub const Mode = enum { free, select, drag, resize, draw };

/// MFC's OnEditPaste puts the copied group's top left this far inside its
/// parent (GUIFrame.cpp:441-537).
pub const paste_inset: f32 = 50;

/// A press that moves less than this many canvas pixels is a click.
const click_slop: f32 = 2;

/// The screen drawn at `scale` with its top left at `origin` (screen pixels).
pub const View = struct {
    origin: Point2 = .{ .x = 0, .y = 0 },
    scale: f32 = 1,

    /// The largest scale at which the canvas fits `width` x `height`, centred.
    pub fn fit(left: f32, top: f32, width: f32, height: f32) View {
        const scale = @max(0.05, @min(width / canvas_width, height / canvas_height));
        return .{
            .origin = .{ .x = left + (width - canvas_width * scale) / 2, .y = top + (height - canvas_height * scale) / 2 },
            .scale = scale,
        };
    }

    pub fn toScreen(self: View, canvas: Point2) Point2 {
        return .{ .x = self.origin.x + canvas.x * self.scale, .y = self.origin.y + canvas.y * self.scale };
    }

    pub fn toCanvas(self: View, screen: Point2) Point2 {
        return .{ .x = (screen.x - self.origin.x) / self.scale, .y = (screen.y - self.origin.y) / self.scale };
    }

    pub fn rectToScreen(self: View, rc: Rect) Rect {
        const a = self.toScreen(.{ .x = rc.x1, .y = rc.y1 });
        const b = self.toScreen(.{ .x = rc.x2, .y = rc.y2 });
        return .{ .x1 = a.x, .y1 = a.y, .x2 = b.x, .y2 = b.y };
    }
};

/// A window's class as the inspector and the canvas label it, from the low
/// bits of UI.h's class type (UI_BASE_VALUE + 0x1100 + n).
pub fn className(class_type: i32) []const u8 {
    return switch (class_type & 0xff) {
        3 => "Button",
        5 => "Static",
        6 => "Status bar",
        7 => "Dialog",
        8 => "Slider",
        9 => "Scrollbar",
        10 => "List",
        13 => "Scroll text",
        else => "Window",
    };
}

/// The folder a new template of this class goes in: the names of the shipped
/// Data/Editor/UI folders, as GetDirectoryFromWindowType picks them.
pub fn templateFolder(class_type: i32) ?[]const u8 {
    return switch (class_type & 0xff) {
        3 => "Buttons",
        5 => "statics",
        6 => "statusbars",
        7 => "dialogs",
        8 => "sliders",
        9 => "scrollbars",
        10 => "lists",
        else => null,
    };
}

fn isDialog(class_type: i32) bool {
    return (class_type & 0xff) == 7;
}

// --- Selection ----------------------------------------------------------------

/// The selected windows, the primary (the one align and equal size follow)
/// first. Ids are the bridge's; `prune` drops the ones a replay removed.
pub const Selection = struct {
    ids: std.ArrayListUnmanaged(i32) = .empty,

    pub fn deinit(self: *Selection, allocator: std.mem.Allocator) void {
        self.ids.deinit(allocator);
    }

    pub fn clear(self: *Selection) void {
        self.ids.clearRetainingCapacity();
    }

    pub fn primary(self: *const Selection) ?i32 {
        return if (self.ids.items.len == 0) null else self.ids.items[0];
    }

    pub fn contains(self: *const Selection, id: i32) bool {
        return std.mem.indexOfScalar(i32, self.ids.items, id) != null;
    }

    pub fn only(self: *Selection, allocator: std.mem.Allocator, id: i32) !void {
        self.ids.clearRetainingCapacity();
        try self.ids.append(allocator, id);
    }

    /// Shift-click: a selected window leaves the selection, any other joins
    /// it as the last, so the primary stays the first.
    pub fn toggle(self: *Selection, allocator: std.mem.Allocator, id: i32) !void {
        if (std.mem.indexOfScalar(i32, self.ids.items, id)) |at| {
            _ = self.ids.orderedRemove(at);
        } else try self.ids.append(allocator, id);
    }

    pub fn prune(self: *Selection, windows: []const GuiWindow) void {
        var keep: usize = 0;
        for (self.ids.items) |id| {
            if (geometry.find(windows, id) != null and id != 0) {
                self.ids.items[keep] = id;
                keep += 1;
            }
        }
        self.ids.shrinkRetainingCapacity(keep);
    }
};

/// The topmost window under a canvas point: the last in document order is
/// drawn last. The root is not a target.
pub fn hit(windows: []const GuiWindow, at: Point2) ?i32 {
    var best: ?i32 = null;
    for (windows) |w| {
        if (w.parent < 0) continue;
        const rc = geometry.canvasRect(windows, w.id) orelse continue;
        if (at.x >= rc.x1 and at.x < rc.x2 and at.y >= rc.y1 and at.y < rc.y2) best = w.id;
    }
    return best;
}

/// The window a control dropped at `at` goes into: the topmost dialog under
/// the point, else the root.
pub fn containerAt(windows: []const GuiWindow, at: Point2) i32 {
    var best: i32 = 0;
    for (windows) |w| {
        if (w.parent < 0 or !isDialog(w.class_type)) continue;
        const rc = geometry.canvasRect(windows, w.id) orelse continue;
        if (at.x >= rc.x1 and at.x < rc.x2 and at.y >= rc.y1 and at.y < rc.y2) best = w.id;
    }
    return best;
}

/// The windows whose canvas rect meets `band`, for a rubber band select.
pub fn inBand(allocator: std.mem.Allocator, windows: []const GuiWindow, band: Rect) std.mem.Allocator.Error![]i32 {
    var out: std.ArrayListUnmanaged(i32) = .empty;
    errdefer out.deinit(allocator);
    for (windows) |w| {
        if (w.parent < 0) continue;
        const rc = geometry.canvasRect(windows, w.id) orelse continue;
        if (rc.x1 < band.x2 and rc.x2 > band.x1 and rc.y1 < band.y2 and rc.y2 > band.y1) try out.append(allocator, w.id);
    }
    return out.toOwnedSlice(allocator);
}

fn normalized(a: Point2, b: Point2) Rect {
    return .{ .x1 = @min(a.x, b.x), .y1 = @min(a.y, b.y), .x2 = @max(a.x, b.x), .y2 = @max(a.y, b.y) };
}

// --- The palette ----------------------------------------------------------------

pub const PaletteEntry = struct {
    folder: []const u8,
    file: []const u8,
    path: []const u8,
};

/// BkResGuiTemplates' text as entries, in the bridge's order (folders by
/// name, the user's last). The entries point into `text`.
pub fn parsePalette(allocator: std.mem.Allocator, text: []const u8) std.mem.Allocator.Error![]PaletteEntry {
    var out: std.ArrayListUnmanaged(PaletteEntry) = .empty;
    errdefer out.deinit(allocator);
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trimEnd(u8, raw, "\r");
        if (line.len == 0) continue;
        var fields = std.mem.splitScalar(u8, line, '\t');
        const folder = fields.next() orelse continue;
        const file = fields.next() orelse continue;
        const path = fields.next() orelse continue;
        try out.append(allocator, .{ .folder = folder, .file = file, .path = path });
    }
    return out.toOwnedSlice(allocator);
}

/// The palette read from the bridge; `text` owns what the entries point at.
pub const Palette = struct {
    text: []u8 = &.{},
    entries: []PaletteEntry = &.{},

    pub fn deinit(self: *Palette, allocator: std.mem.Allocator) void {
        allocator.free(self.entries);
        allocator.free(self.text);
        self.* = .{};
    }

    pub fn load(allocator: std.mem.Allocator, bridge: ResBridge, user_folder: []const u8) Error!Palette {
        var size: usize = 0;
        var none: [0]u8 = .{};
        const sizing = bridge.guiTemplates(user_folder, &none, &size);
        if (sizing != .ok and (sizing != .refused or size == 0)) try bridge_mod.check(sizing);
        // The bridge writes a closing NUL after the text, so the buffer holds one byte more than the size.
        const buffer = try allocator.alloc(u8, size + 1);
        errdefer allocator.free(buffer);
        try bridge_mod.check(bridge.guiTemplates(user_folder, buffer, &size));
        const text = buffer[0..@min(size, buffer.len)];
        const entries = try parsePalette(allocator, text);
        return .{ .text = buffer, .entries = entries };
    }
};

// --- Create new template ------------------------------------------------------

/// A template file made from one window's clipboard text: the clip root is
/// dropped and the window's own element becomes the `<base>` root, as
/// OnCreatenewtemplate's file reads (a ClassTypeID and the window's tree).
/// Null when the text is not exactly one element inside a clip root.
pub fn templateFromClipboard(allocator: std.mem.Allocator, clipboard: []const u8) std.mem.Allocator.Error!?[]u8 {
    const open_end = std.mem.indexOfScalar(u8, clipboard, '>') orelse return null;
    const close_start = std.mem.lastIndexOfScalar(u8, clipboard, '<') orelse return null;
    if (close_start <= open_end or !std.mem.startsWith(u8, clipboard[close_start..], "</")) return null;
    const inner = std.mem.trim(u8, clipboard[open_end + 1 .. close_start], " \t\r\n");
    if (!std.mem.startsWith(u8, inner, "<item") or inner.len < 6) return null;
    const name_end = std.mem.indexOfAny(u8, inner[1..], " \t\r\n/>") orelse return null;
    if (!std.mem.eql(u8, inner[1 .. 1 + name_end], "item")) return null;
    if (!std.mem.endsWith(u8, inner, "</item>") and !std.mem.endsWith(u8, inner, "/>")) return null;
    var out: std.ArrayListUnmanaged(u8) = .empty;
    errdefer out.deinit(allocator);
    try out.appendSlice(allocator, "<?xml version=\"1.0\"?>\r\n<base");
    if (std.mem.endsWith(u8, inner, "</item>")) {
        try out.appendSlice(allocator, inner["<item".len .. inner.len - "</item>".len]);
        try out.appendSlice(allocator, "</base>\r\n");
    } else {
        try out.appendSlice(allocator, inner["<item".len..]);
        try out.appendSlice(allocator, "\r\n");
    }
    return try out.toOwnedSlice(allocator);
}

/// The next free `<Folder><NN>.xml` name in `taken` (the names already in the
/// user's folder), NN from 00 to 99 as MFC probes.
pub fn nextTemplateName(buffer: []u8, stem: []const u8, taken: []const []const u8) ?[]const u8 {
    var i: u32 = 0;
    while (i < 100) : (i += 1) {
        const name = std.fmt.bufPrint(buffer, "{s}{d:0>2}.xml", .{ stem, i }) catch return null;
        var used = false;
        for (taken) |t| used = used or std.ascii.eqlIgnoreCase(t, name);
        if (!used) return name;
    }
    return null;
}

// --- The overlay --------------------------------------------------------------

/// One screen's canvas: the selection, the mode, the gesture in progress and
/// the app clipboard. Windows are read from the bridge at each press and
/// each commit, so a replay never leaves a stale rect behind.
pub const Overlay = struct {
    allocator: std.mem.Allocator,
    mode: Mode = .free,
    view: View = .{},
    selection: Selection = .{},
    /// The press, and the latest pointer, in canvas pixels.
    start: Point2 = .{ .x = 0, .y = 0 },
    at: Point2 = .{ .x = 0, .y = 0 },
    handle: geometry.Handle = .none,
    /// The primary's canvas rect at the press, for a resize.
    origin_rect: Rect = .{ .x1 = 0, .y1 = 0, .x2 = 0, .y2 = 0 },
    /// Whether a drag press landed on an already selected window, so a click
    /// without movement can narrow the selection.
    narrow_to: ?i32 = null,
    /// The palette file the next press on the canvas draws, when armed.
    armed: ?[]u8 = null,
    /// The app clipboard: MFC's copied list, as bridge text, and the top
    /// left of the copied windows on the canvas for the paste offset.
    clip: ?[]u8 = null,
    clip_origin: Point2 = .{ .x = 0, .y = 0 },
    /// What the canvas status line says was done last.
    last: [128]u8 = undefined,
    last_len: usize = 0,

    pub fn init(allocator: std.mem.Allocator) Overlay {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *Overlay) void {
        self.selection.deinit(self.allocator);
        if (self.armed) |armed| self.allocator.free(armed);
        if (self.clip) |clip| self.allocator.free(clip);
        self.* = undefined;
    }

    pub fn busy(self: *const Overlay) bool {
        return self.mode == .select or self.mode == .drag or self.mode == .resize or self.mode == .draw;
    }

    fn say(self: *Overlay, comptime fmt: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.last, fmt, args) catch self.last[0..];
        self.last_len = text.len;
    }

    pub fn lastSaid(self: *const Overlay) []const u8 {
        return self.last[0..self.last_len];
    }

    /// The canvas status line: the mode, the selection and the last command.
    pub fn status(self: *const Overlay, buffer: []u8) []const u8 {
        const mode = @tagName(self.mode);
        const armed: []const u8 = if (self.armed) |path| std.fs.path.basename(path) else "";
        return std.fmt.bufPrint(buffer, "mode {s}{s}{s} | {d} selected{s} | {s}", .{
            mode,
            if (armed.len != 0) ", drawing " else "",
            armed,
            self.selection.ids.items.len,
            if (self.selection.primary() != null) " (first is primary)" else "",
            if (self.last_len != 0) self.lastSaid() else "nothing done yet",
        }) catch buffer[0..0];
    }

    /// Picking a palette entry arms drawing: the next press on the canvas
    /// places it. Null disarms.
    pub fn arm(self: *Overlay, template: ?[]const u8) std.mem.Allocator.Error!void {
        if (self.armed) |old| self.allocator.free(old);
        self.armed = if (template) |path| try self.allocator.dupe(u8, path) else null;
    }

    fn windowsOf(self: *Overlay, bridge: ResBridge) Error![]GuiWindow {
        return gui_tools.readWindows(self.allocator, bridge);
    }

    /// Reads the screen, dropping selected windows an undo took away.
    pub fn refresh(self: *Overlay, bridge: ResBridge) Error!void {
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        self.selection.prune(windows);
    }

    // --- The mouse -------------------------------------------------------------

    /// A press at `screen`. A press on the primary's handle resizes it; on a
    /// window selects it (shift toggles) and starts a drag of the selection;
    /// on nothing starts a rubber band, or draws the armed template.
    pub fn press(self: *Overlay, bridge: ResBridge, screen: Point2, shift: bool) Error!void {
        if (self.busy()) return;
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        self.selection.prune(windows);
        const canvas = self.view.toCanvas(screen);
        self.start = canvas;
        self.at = canvas;
        self.narrow_to = null;
        if (self.armed != null) {
            self.mode = .draw;
            return;
        }
        if (self.selection.primary()) |primary| {
            if (geometry.canvasRect(windows, primary)) |rc| {
                const handle = geometry.hitTest(rc, canvas.x, canvas.y);
                if (handle != .none) {
                    self.handle = handle;
                    self.origin_rect = rc;
                    self.mode = .resize;
                    return;
                }
            }
        }
        const target = hit(windows, canvas) orelse {
            self.mode = .select;
            return;
        };
        if (shift) {
            try self.selection.toggle(self.allocator, target);
            if (!self.selection.contains(target)) return;
        } else if (!self.selection.contains(target)) {
            try self.selection.only(self.allocator, target);
        } else if (self.selection.ids.items.len > 1) {
            self.narrow_to = target;
        }
        self.mode = .drag;
    }

    /// The pointer moving with the button down; only the preview follows.
    pub fn move(self: *Overlay, screen: Point2) void {
        if (self.busy()) self.at = self.view.toCanvas(screen);
    }

    fn delta(self: *const Overlay) struct { x: i32, y: i32 } {
        return .{ .x = @intFromFloat(@round(self.at.x - self.start.x)), .y = @intFromFloat(@round(self.at.y - self.start.y)) };
    }

    /// The canvas rects the gesture would leave the windows at, for the
    /// canvas to draw while the button is down. Owned by the caller.
    pub fn preview(self: *Overlay, bridge: ResBridge) Error![]Rect {
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        var out: std.ArrayListUnmanaged(Rect) = .empty;
        errdefer out.deinit(self.allocator);
        switch (self.mode) {
            .drag => {
                const d = self.delta();
                for (self.selection.ids.items) |id| {
                    const rc = geometry.canvasRect(windows, id) orelse continue;
                    try out.append(self.allocator, .{ .x1 = rc.x1 + @as(f32, @floatFromInt(d.x)), .y1 = rc.y1 + @as(f32, @floatFromInt(d.y)), .x2 = rc.x2 + @as(f32, @floatFromInt(d.x)), .y2 = rc.y2 + @as(f32, @floatFromInt(d.y)) });
                }
            },
            .resize => try out.append(self.allocator, self.resized(windows)),
            .select => try out.append(self.allocator, normalized(self.start, self.at)),
            else => {},
        }
        return out.toOwnedSlice(self.allocator);
    }

    fn resized(self: *const Overlay, windows: []const GuiWindow) Rect {
        const primary = self.selection.primary() orelse return self.origin_rect;
        const parent = if (geometry.find(windows, primary)) |w| w.parent else -1;
        const client = if (parent >= 0) geometry.canvasRect(windows, parent) orelse self.fullCanvas() else self.fullCanvas();
        return geometry.resize(self.origin_rect, self.handle, self.at.x - self.start.x, self.at.y - self.start.y, client);
    }

    fn fullCanvas(self: *const Overlay) Rect {
        _ = self;
        return .{ .x1 = 0, .y1 = 0, .x2 = canvas_width, .y2 = canvas_height };
    }

    /// The release at `screen`: commits the gesture as one undo step, or
    /// nothing when it changed nothing.
    pub fn release(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, screen: Point2) Error!void {
        if (!self.busy()) return;
        self.at = self.view.toCanvas(screen);
        const mode = self.mode;
        self.mode = .free;
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        switch (mode) {
            .drag => {
                const d = self.delta();
                const moved = @abs(self.at.x - self.start.x) >= click_slop or @abs(self.at.y - self.start.y) >= click_slop;
                if (!moved) {
                    // A plain click on one of several selected windows narrows to it.
                    if (self.narrow_to) |id| try self.selection.only(self.allocator, id);
                    return;
                }
                var edits = try geometry.moveBy(self.allocator, windows, self.selection.ids.items, d.x, d.y);
                defer edits.deinit(self.allocator);
                try self.commitRects(bridge, doc, history, edits.items, "moved");
            },
            .resize => {
                const primary = self.selection.primary() orelse return;
                const rc = self.resized(windows);
                if (rc.eql(self.origin_rect)) return;
                const edit = geometry.rectFor(windows, primary, rc) orelse return;
                try self.commitRects(bridge, doc, history, &.{edit}, "resized");
            },
            .select => {
                if (@abs(self.at.x - self.start.x) < click_slop and @abs(self.at.y - self.start.y) < click_slop) {
                    self.selection.clear();
                    return;
                }
                const found = try inBand(self.allocator, windows, normalized(self.start, self.at));
                defer self.allocator.free(found);
                self.selection.clear();
                try self.selection.ids.appendSlice(self.allocator, found);
                self.say("selected {d}", .{found.len});
            },
            .draw => {
                const path = self.armed orelse return;
                try self.insert(bridge, doc, history, windows, path, self.start);
            },
            .free => {},
        }
    }

    /// Escape: the preview goes away and nothing is recorded.
    pub fn cancel(self: *Overlay) void {
        self.mode = .free;
        self.narrow_to = null;
    }

    fn commitRects(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, edits: []const GuiRect, what: []const u8) Error!void {
        const command = (try gui_tools.setRects(self.allocator, bridge, &doc.gui_ids, edits)) orelse return;
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
        self.say("{s} {d}", .{ what, edits.len });
    }

    // --- Palette ----------------------------------------------------------------

    /// Inserts the template at `canvas`, under the dialog there or the root.
    /// The new window becomes the selection.
    fn insert(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, windows: []const GuiWindow, path: []const u8, canvas: Point2) Error!void {
        const parent = containerAt(windows, canvas);
        const parent_rc = geometry.canvasRect(windows, parent) orelse self.fullCanvas();
        const x: i32 = @intFromFloat(@round(canvas.x - parent_rc.x1));
        const y: i32 = @intFromFloat(@round(canvas.y - parent_rc.y1));
        const command = try gui_tools.insertTemplate(self.allocator, &doc.gui_ids, parent, path, x, y);
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
        try self.selectLastCreated(history);
        self.say("placed {s}", .{std.fs.path.basename(path)});
    }

    /// A template dropped from the palette at a screen point: one insert, as
    /// a press and release with the template armed would do.
    pub fn drop(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, template: []const u8, screen: Point2) Error!void {
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        try self.insert(bridge, doc, history, windows, template, self.view.toCanvas(screen));
    }

    fn selectLastCreated(self: *Overlay, history: *const History) Error!void {
        if (history.undo_stack.items.len == 0) return;
        switch (history.undo_stack.items[history.undo_stack.items.len - 1].command) {
            .gui => |g| switch (g) {
                .create => |c| {
                    self.selection.clear();
                    try self.selection.ids.appendSlice(self.allocator, c.tops);
                },
                else => {},
            },
            else => {},
        }
    }

    // --- Keys and menus ----------------------------------------------------------

    /// An arrow key: the selection moves one canvas pixel (ten with shift).
    pub fn nudge(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, dx: i32, dy: i32) Error!void {
        if (self.busy() or self.selection.ids.items.len == 0) return;
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        var edits = try geometry.moveBy(self.allocator, windows, self.selection.ids.items, dx, dy);
        defer edits.deinit(self.allocator);
        try self.commitRects(bridge, doc, history, edits.items, "nudged");
    }

    /// The Align menu: the other selected windows to the primary's edge.
    pub fn alignSelection(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, mode: geometry.Align) Error!void {
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        var edits = try geometry.align_(self.allocator, windows, self.selection.ids.items, mode);
        defer edits.deinit(self.allocator);
        try self.commitRects(bridge, doc, history, edits.items, @tagName(mode));
    }

    /// Equal width, height or size to the primary.
    pub fn equalize(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, mode: geometry.Equal) Error!void {
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        var edits = try geometry.equalize(self.allocator, windows, self.selection.ids.items, mode);
        defer edits.deinit(self.allocator);
        try self.commitRects(bridge, doc, history, edits.items, "equal");
    }

    /// Delete: the selection and its subtrees, one step.
    pub fn deleteSelection(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History) Error!void {
        if (self.selection.ids.items.len == 0) return;
        const command = try gui_tools.delete(self.allocator, bridge, &doc.gui_ids, self.selection.ids.items);
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
        self.say("deleted {d}", .{self.selection.ids.items.len});
        self.selection.clear();
    }

    fn setClip(self: *Overlay, bridge: ResBridge, windows: []const GuiWindow) Error!void {
        var text = try gui_tools.copy(self.allocator, bridge, self.selection.ids.items);
        errdefer text.deinit(self.allocator);
        var origin: ?Point2 = null;
        for (self.selection.ids.items) |id| {
            const rc = geometry.canvasRect(windows, id) orelse continue;
            if (origin) |*o| {
                o.x = @min(o.x, rc.x1);
                o.y = @min(o.y, rc.y1);
            } else origin = .{ .x = rc.x1, .y = rc.y1 };
        }
        if (self.clip) |old| self.allocator.free(old);
        self.clip = text.bytes;
        self.clip_origin = origin orelse .{ .x = 0, .y = 0 };
    }

    /// Ctrl+C.
    pub fn copySelection(self: *Overlay, bridge: ResBridge) Error!void {
        if (self.selection.ids.items.len == 0) return;
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        try self.setClip(bridge, windows);
        self.say("copied {d}", .{self.selection.ids.items.len});
    }

    /// Ctrl+X: the clipboard, then one delete step.
    pub fn cutSelection(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History) Error!void {
        if (self.selection.ids.items.len == 0) return;
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        try self.setClip(bridge, windows);
        try self.deleteSelection(bridge, doc, history);
        self.say("cut", .{});
    }

    /// Ctrl+V: the copied group's top left lands `paste_inset` inside the
    /// root, as OnEditPaste places it; the pasted windows become the selection.
    pub fn pasteClipboard(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History) Error!void {
        const text = self.clip orelse return;
        const windows = try self.windowsOf(bridge);
        defer self.allocator.free(windows);
        const parent_rc = geometry.canvasRect(windows, 0) orelse self.fullCanvas();
        const dx: i32 = @intFromFloat(@round(parent_rc.x1 + paste_inset - self.clip_origin.x));
        const dy: i32 = @intFromFloat(@round(parent_rc.y1 + paste_inset - self.clip_origin.y));
        const command = try gui_tools.paste(self.allocator, &doc.gui_ids, 0, text, dx, dy);
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
        try self.selectLastCreated(history);
        self.say("pasted", .{});
    }

    /// Edits a window's own ints (the inspector's Pos, Size and
    /// PositionFlag) as one step.
    pub fn setLocal(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, edit: GuiRect) Error!void {
        try self.commitRects(bridge, doc, history, &.{edit}, "edited");
    }

    /// Edits one attribute of a window as one undoable step.
    pub fn setAttribute(self: *Overlay, bridge: ResBridge, doc: *Document, history: *History, id: i32, name: []const u8, value: []const u8) Error!void {
        const command = try gui_tools.setAttr(self.allocator, bridge, &doc.gui_ids, id, name, value);
        try tools.commit(self.allocator, bridge, doc, history, command, 0);
        self.say("{s} set", .{name});
    }

    /// The clipboard text of the one selected window as a template file, for
    /// "Create new template"; null unless exactly one window is selected.
    pub fn templateText(self: *Overlay, bridge: ResBridge) Error!?[]u8 {
        if (self.selection.ids.items.len != 1) return null;
        var text = try gui_tools.copy(self.allocator, bridge, self.selection.ids.items);
        defer text.deinit(self.allocator);
        return try templateFromClipboard(self.allocator, text.bytes);
    }
};

/// The attributes the inspector offers besides the rect (PositionFlag, Pos
/// and Size go through `setLocal`): GUIFrame's element properties.
pub const inspector_attrs = [_][]const u8{
    "ElementID",
    "VisibleFlag",
    "ActiveFlag",
    "TextKey",
    "CurrentState",
    "Background",
    "text_type",
    "text_align",
    "text_color",
};

/// The value of attribute `name` of window `id`, written into `buffer`;
/// empty when the window has none.
pub fn readAttr(bridge: ResBridge, id: i32, name: []const u8, buffer: []u8) []const u8 {
    var size: usize = 0;
    const status = bridge.guiGetAttr(id, name, buffer, &size);
    if (status != .ok) return buffer[0..0];
    return buffer[0..@min(size, buffer.len)];
}

// --- The Game's frame ---------------------------------------------------------------

/// A pixel box of a captured Game frame, `x2` and `y2` one past the last pixel.
pub const PixelRect = struct { x1: u32, y1: u32, x2: u32, y2: u32 };

/// Where a canvas rect lands in a Game shot of `width` x `height`: the Game
/// draws its 1024 x 768 screen at the largest uniform scale that fits the
/// window, centred (measured on a 1920 x 1000 shot: 1333 pixels wide, 293
/// from the left). The box is widened by `margin` pixels and clamped.
pub fn gameRect(rc: Rect, width: u32, height: u32, margin: u32) PixelRect {
    const w: f32 = @floatFromInt(width);
    const h: f32 = @floatFromInt(height);
    const scale = @min(w / canvas_width, h / canvas_height);
    const left = (w - canvas_width * scale) / 2;
    const top = (h - canvas_height * scale) / 2;
    const m: f32 = @floatFromInt(margin);
    return .{
        .x1 = clampPixel(@floor(left + rc.x1 * scale - m), width),
        .y1 = clampPixel(@floor(top + rc.y1 * scale - m), height),
        .x2 = clampPixel(@ceil(left + rc.x2 * scale + m), width),
        .y2 = clampPixel(@ceil(top + rc.y2 * scale + m), height),
    };
}

fn clampPixel(v: f32, limit: u32) u32 {
    if (v <= 0) return 0;
    const top: f32 = @floatFromInt(limit);
    return @intFromFloat(@min(v, top));
}

/// How many pixels of two RGBA frames of one size differ by more than
/// `tolerance` in some channel: those inside `inside` (the whole frame when
/// null) and outside every box of `holes`.
pub fn differing(a: []const u8, b: []const u8, width: u32, height: u32, tolerance: u8, inside: ?PixelRect, holes: []const PixelRect) usize {
    var count: usize = 0;
    var y: u32 = 0;
    while (y < height) : (y += 1) {
        var x: u32 = 0;
        while (x < width) : (x += 1) {
            if (inside) |box| {
                if (x < box.x1 or x >= box.x2 or y < box.y1 or y >= box.y2) continue;
            }
            var held = false;
            for (holes) |hole| {
                if (x >= hole.x1 and x < hole.x2 and y >= hole.y1 and y < hole.y2) held = true;
            }
            if (held) continue;
            const at = (@as(usize, y) * width + x) * 4;
            var changed = false;
            for (0..3) |channel| {
                const d = @as(i32, a[at + channel]) - @as(i32, b[at + channel]);
                if (@abs(d) > tolerance) changed = true;
            }
            if (changed) count += 1;
        }
    }
    return count;
}

/// The size in an `autoshot_<frame>_<w>x<h>.rgba` file name, the Game's BK_AUTO_UI shot.
pub fn autoshotSize(name: []const u8) ?struct { width: u32, height: u32 } {
    const prefix = "autoshot_";
    const suffix = ".rgba";
    if (!std.mem.startsWith(u8, name, prefix) or !std.mem.endsWith(u8, name, suffix)) return null;
    const middle = name[prefix.len .. name.len - suffix.len];
    const underscore = std.mem.indexOfScalar(u8, middle, '_') orelse return null;
    const size = middle[underscore + 1 ..];
    const x = std.mem.indexOfScalar(u8, size, 'x') orelse return null;
    const width = std.fmt.parseInt(u32, size[0..x], 10) catch return null;
    const height = std.fmt.parseInt(u32, size[x + 1 ..], 10) catch return null;
    if (width == 0 or height == 0) return null;
    return .{ .width = width, .height = height };
}

// --- Tests ------------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = core.fake_bridge.FakeResBridge;

const button_template = "data/editor/ui/Buttons/Button.xml";

const Rig = struct {
    fake: FakeResBridge,
    doc: Document = .{},
    hist: History = .{},
    overlay: Overlay,

    fn init(self: *Rig, allocator: std.mem.Allocator) !void {
        self.* = .{ .fake = FakeResBridge.init(allocator), .overlay = Overlay.init(allocator) };
        try bridge_mod.check(self.fake.bridge().new(.gui_frame));
        try self.fake.addGuiTemplate(button_template, 0x10001103, 120, 30);
        // The fake's root is 800 x 600 at the origin; the view is 1:1 at (0, 0).
        // 1: left top (10, 20) 100 x 50. 2: right bottom (30, 40) 60 x 30.
        // 3: left top (200, 200) 80 x 40.
        try self.fake.gui_windows.append(allocator, .{ .id = 1, .parent = 0, .class_type = 0x10001103, .element_id = 11, .flag = 0x11, .x = 10, .y = 20, .w = 100, .h = 50, .visible = 1 });
        try self.fake.gui_windows.append(allocator, .{ .id = 2, .parent = 0, .class_type = 0x10001105, .element_id = 12, .flag = 0x33, .x = 30, .y = 40, .w = 60, .h = 30, .visible = 1 });
        try self.fake.gui_windows.append(allocator, .{ .id = 3, .parent = 0, .class_type = 0x10001103, .element_id = 13, .flag = 0x11, .x = 200, .y = 200, .w = 80, .h = 40, .visible = 1 });
        self.fake.gui_next_id = 4;
    }

    fn deinit(self: *Rig, allocator: std.mem.Allocator) void {
        self.overlay.deinit();
        self.hist.deinit(allocator);
        self.doc.deinit(allocator);
        self.fake.deinit();
    }

    fn b(self: *Rig) ResBridge {
        return self.fake.bridge();
    }

    fn window(self: *Rig, id: i32) GuiWindow {
        return geometry.find(self.fake.gui_windows.items, id).?;
    }

    fn press(self: *Rig, x: f32, y: f32, shift: bool) !void {
        try self.overlay.press(self.b(), .{ .x = x, .y = y }, shift);
    }

    fn release(self: *Rig, x: f32, y: f32) !void {
        try self.overlay.release(self.b(), &self.doc, &self.hist, .{ .x = x, .y = y });
    }

    fn undo(self: *Rig, allocator: std.mem.Allocator) !void {
        const entry = self.hist.undo_stack.items.len - 1;
        try self.doc.undoOne(allocator, self.b(), &self.hist.undo_stack.items[entry].command);
    }

    fn redo(self: *Rig, allocator: std.mem.Allocator) !void {
        const entry = self.hist.undo_stack.items.len - 1;
        try self.doc.redoOne(allocator, self.b(), &self.hist.undo_stack.items[entry].command);
    }
};

test "the view fits the canvas and maps points both ways" {
    const view = View.fit(100, 50, 512, 600);
    // 512 / 1024 = 0.5 is the tighter side: the canvas is 512 x 384, centred vertically.
    try testing.expectEqual(@as(f32, 0.5), view.scale);
    try testing.expectEqual(@as(f32, 100), view.origin.x);
    try testing.expectEqual(@as(f32, 50 + (600 - 384) / 2), view.origin.y);
    const screen = view.toScreen(.{ .x = 200, .y = 100 });
    try testing.expectEqual(@as(f32, 200), screen.x);
    const back = view.toCanvas(screen);
    try testing.expectEqual(@as(f32, 200), back.x);
    try testing.expectEqual(@as(f32, 100), back.y);
}

test "drag moves the selection by the dragged canvas delta, anchors kept, one undo step" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);

    // Select 1 then shift-click 2 (right, bottom anchored; canvas (710, 530)-(770, 560)).
    try rig.press(50, 40, false);
    try rig.release(50, 40);
    try testing.expectEqual(@as(?i32, 1), rig.overlay.selection.primary());
    try rig.press(740, 540, true);
    try testing.expectEqual(Mode.drag, rig.overlay.mode);
    try testing.expectEqual(@as(usize, 2), rig.overlay.selection.ids.items.len);
    // Drag by (+15, -6): window 1's ints follow, window 2's far-edge ints go the other way.
    rig.overlay.move(.{ .x = 755, .y = 534 });
    const shown = try rig.overlay.preview(rig.b());
    defer allocator.free(shown);
    try testing.expectEqual(@as(f32, 25), shown[0].x1);
    try rig.release(755, 534);
    try testing.expectEqual(@as(i32, 25), rig.window(1).x);
    try testing.expectEqual(@as(i32, 14), rig.window(1).y);
    try testing.expectEqual(@as(i32, 15), rig.window(2).x);
    try testing.expectEqual(@as(i32, 46), rig.window(2).y);
    try testing.expectEqual(@as(i32, 0x33), rig.window(2).flag);
    try testing.expectEqual(@as(usize, 1), rig.hist.undo_stack.items.len);

    try rig.undo(allocator);
    try testing.expectEqual(@as(i32, 10), rig.window(1).x);
    try testing.expectEqual(@as(i32, 30), rig.window(2).x);
    try rig.redo(allocator);
    try testing.expectEqual(@as(i32, 25), rig.window(1).x);
}

test "resize drags a handle of the primary, clamped, as one step" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);

    try rig.press(50, 40, false);
    try rig.release(50, 40);
    // Window 1 is (10, 20)-(110, 70); its right-bottom handle is at (110, 70).
    try rig.press(110, 70, false);
    try testing.expectEqual(Mode.resize, rig.overlay.mode);
    try testing.expectEqual(geometry.Handle.right_bottom, rig.overlay.handle);
    rig.overlay.move(.{ .x = 140, .y = 95 });
    try rig.release(140, 95);
    try testing.expectEqual(@as(i32, 130), rig.window(1).w);
    try testing.expectEqual(@as(i32, 75), rig.window(1).h);
    try testing.expectEqual(@as(i32, 10), rig.window(1).x);
    try testing.expectEqual(@as(usize, 1), rig.hist.undo_stack.items.len);
    try rig.undo(allocator);
    try testing.expectEqual(@as(i32, 100), rig.window(1).w);
    try testing.expectEqual(@as(i32, 50), rig.window(1).h);

    // A resize past MINIMAL stays: dragging the right handle far left changes nothing.
    try rig.press(110, 45, false);
    rig.overlay.move(.{ .x = 0, .y = 45 });
    try rig.release(0, 45);
    try testing.expectEqual(@as(i32, 100), rig.window(1).w);
}

test "rubber band selects the windows it touches, a click on nothing clears" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);

    try rig.press(0, 0, false);
    try testing.expectEqual(Mode.select, rig.overlay.mode);
    rig.overlay.move(.{ .x = 300, .y = 300 });
    const band = try rig.overlay.preview(rig.b());
    defer allocator.free(band);
    try testing.expectEqual(@as(f32, 300), band[0].x2);
    try rig.release(300, 300);
    // Windows 1 and 3 are inside, window 2 is at the far corner.
    try testing.expectEqualSlices(i32, &.{ 1, 3 }, rig.overlay.selection.ids.items);
    try testing.expectEqual(@as(usize, 0), rig.hist.undo_stack.items.len);

    try rig.press(500, 400, false);
    try rig.release(500, 400);
    try testing.expectEqual(@as(usize, 0), rig.overlay.selection.ids.items.len);
}

test "align and equal size on two selected controls, each undone and redone" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);

    try rig.overlay.selection.ids.appendSlice(allocator, &.{ 1, 3 });
    try rig.overlay.alignSelection(rig.b(), &rig.doc, &rig.hist, .left);
    try testing.expectEqual(@as(i32, 10), rig.window(3).x);
    try testing.expectEqual(@as(i32, 200), rig.window(3).y);
    try rig.undo(allocator);
    try testing.expectEqual(@as(i32, 200), rig.window(3).x);
    try rig.redo(allocator);
    try testing.expectEqual(@as(i32, 10), rig.window(3).x);

    try rig.overlay.equalize(rig.b(), &rig.doc, &rig.hist, .size);
    try testing.expectEqual(@as(i32, 100), rig.window(3).w);
    try testing.expectEqual(@as(i32, 50), rig.window(3).h);
    try testing.expectEqual(@as(usize, 2), rig.hist.undo_stack.items.len);
    try rig.undo(allocator);
    try testing.expectEqual(@as(i32, 80), rig.window(3).w);
    try rig.redo(allocator);
    try testing.expectEqual(@as(i32, 100), rig.window(3).w);

    // One window selected: nothing to align to, nothing recorded.
    rig.overlay.selection.clear();
    try rig.overlay.selection.ids.append(allocator, 1);
    try rig.overlay.alignSelection(rig.b(), &rig.doc, &rig.hist, .top);
    try testing.expectEqual(@as(usize, 2), rig.hist.undo_stack.items.len);
}

test "paste puts the copied group 50 inside the root, as one undo step, and selects it" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);

    try rig.overlay.selection.ids.appendSlice(allocator, &.{ 1, 3 });
    try rig.overlay.copySelection(rig.b());
    try testing.expect(rig.overlay.clip != null);
    // The group's top left is (10, 20); it lands at (50, 50): offset (+40, +30).
    try rig.overlay.pasteClipboard(rig.b(), &rig.doc, &rig.hist);
    try testing.expectEqual(@as(usize, 6), rig.fake.gui_windows.items.len);
    const first = rig.fake.gui_windows.items[rig.fake.gui_windows.items.len - 2];
    const second = rig.fake.gui_windows.items[rig.fake.gui_windows.items.len - 1];
    try testing.expectEqual(@as(i32, 50), first.x);
    try testing.expectEqual(@as(i32, 50), first.y);
    try testing.expectEqual(@as(i32, 240), second.x);
    try testing.expectEqual(@as(i32, 230), second.y);
    try testing.expectEqual(@as(usize, 2), rig.overlay.selection.ids.items.len);
    try testing.expectEqual(first.id, rig.overlay.selection.ids.items[0]);

    try rig.undo(allocator);
    try testing.expectEqual(@as(usize, 4), rig.fake.gui_windows.items.len);
    try rig.redo(allocator);
    try testing.expectEqual(@as(usize, 6), rig.fake.gui_windows.items.len);
}

test "cut leaves the clipboard and one delete step, delete refuses nothing selected" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);

    try rig.overlay.deleteSelection(rig.b(), &rig.doc, &rig.hist);
    try testing.expectEqual(@as(usize, 0), rig.hist.undo_stack.items.len);

    try rig.overlay.selection.ids.append(allocator, 3);
    try rig.overlay.cutSelection(rig.b(), &rig.doc, &rig.hist);
    try testing.expectEqual(@as(usize, 3), rig.fake.gui_windows.items.len);
    try testing.expect(rig.overlay.clip != null);
    try testing.expectEqual(@as(usize, 0), rig.overlay.selection.ids.items.len);
    try rig.undo(allocator);
    try testing.expectEqual(@as(usize, 4), rig.fake.gui_windows.items.len);
    try rig.redo(allocator);
    try rig.overlay.pasteClipboard(rig.b(), &rig.doc, &rig.hist);
    try testing.expectEqual(@as(usize, 4), rig.fake.gui_windows.items.len);
}

test "Escape cancels a gesture with no history entry" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);

    try rig.press(50, 40, false);
    rig.overlay.move(.{ .x = 200, .y = 200 });
    rig.overlay.cancel();
    try testing.expectEqual(Mode.free, rig.overlay.mode);
    try rig.release(200, 200);
    try testing.expectEqual(@as(i32, 10), rig.window(1).x);
    try testing.expectEqual(@as(usize, 0), rig.hist.undo_stack.items.len);
}

test "a template dropped on the canvas is one insert, selected, undone and redone" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);

    try rig.overlay.drop(rig.b(), &rig.doc, &rig.hist, button_template, .{ .x = 300, .y = 220 });
    try testing.expectEqual(@as(usize, 5), rig.fake.gui_windows.items.len);
    const placed = rig.fake.gui_windows.items[4];
    try testing.expectEqual(@as(i32, 300), placed.x);
    try testing.expectEqual(@as(i32, 220), placed.y);
    try testing.expectEqual(@as(?i32, placed.id), rig.overlay.selection.primary());
    try rig.undo(allocator);
    try testing.expectEqual(@as(usize, 4), rig.fake.gui_windows.items.len);
    try rig.redo(allocator);
    try testing.expectEqual(@as(usize, 5), rig.fake.gui_windows.items.len);

    // Armed: a press then a release places it at the press point.
    try rig.overlay.arm(button_template);
    try rig.press(400, 100, false);
    try testing.expectEqual(Mode.draw, rig.overlay.mode);
    try rig.release(400, 100);
    try testing.expectEqual(@as(usize, 6), rig.fake.gui_windows.items.len);
    try testing.expectEqual(@as(i32, 400), rig.fake.gui_windows.items[5].x);

    // A path the bridge cannot read records nothing.
    try testing.expectError(error.Failed, rig.overlay.drop(rig.b(), &rig.doc, &rig.hist, "data/editor/ui/Nope.xml", .{ .x = 1, .y = 1 }));
    try testing.expectEqual(@as(usize, 2), rig.hist.undo_stack.items.len);
}

test "arrow nudge and the inspector's edits are undoable steps" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);

    try rig.overlay.selection.ids.append(allocator, 1);
    try rig.overlay.nudge(rig.b(), &rig.doc, &rig.hist, 1, 0);
    try testing.expectEqual(@as(i32, 11), rig.window(1).x);
    try rig.undo(allocator);
    try testing.expectEqual(@as(i32, 10), rig.window(1).x);

    try rig.overlay.setLocal(rig.b(), &rig.doc, &rig.hist, .{ .id = 1, .flag = 0x11, .x = 10, .y = 20, .w = 150, .h = 50 });
    try testing.expectEqual(@as(i32, 150), rig.window(1).w);
    try rig.undo(allocator);
    try testing.expectEqual(@as(i32, 100), rig.window(1).w);

    try bridge_mod.check(rig.b().guiSetAttr(1, "TextKey", "old"));
    try rig.overlay.setAttribute(rig.b(), &rig.doc, &rig.hist, 1, "TextKey", "new");
    var buffer: [32]u8 = undefined;
    try testing.expectEqualStrings("new", readAttr(rig.b(), 1, "TextKey", &buffer));
    try rig.undo(allocator);
    try testing.expectEqualStrings("old", readAttr(rig.b(), 1, "TextKey", &buffer));
    try testing.expectEqualStrings("", readAttr(rig.b(), 2, "TextKey", &buffer));
}

test "the palette text parses into folders, files and paths" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);
    var palette = try Palette.load(allocator, rig.b(), "");
    defer palette.deinit(allocator);
    try testing.expectEqual(@as(usize, 1), palette.entries.len);
    try testing.expectEqualStrings("Buttons", palette.entries[0].folder);
    try testing.expectEqualStrings("Button.xml", palette.entries[0].file);
    try testing.expectEqualStrings(button_template, palette.entries[0].path);

    const parsed = try parsePalette(allocator, "Buttons\tB.xml\t/d/B.xml\r\n\r\nUser\tU.xml\t/u/U.xml\n");
    defer allocator.free(parsed);
    try testing.expectEqual(@as(usize, 2), parsed.len);
    try testing.expectEqualStrings("User", parsed[1].folder);
}

test "a new template is the selected window's element as a base document, named in the user folder" {
    const allocator = testing.allocator;
    const clip = "<clip>\n<item ClassTypeID=\"268439811\" PositionFlag=\"17\"><WindowPos x=\"1\" y=\"2\"/></item>\n</clip>";
    const text = (try templateFromClipboard(allocator, clip)).?;
    defer allocator.free(text);
    try testing.expectEqualStrings("<?xml version=\"1.0\"?>\r\n<base ClassTypeID=\"268439811\" PositionFlag=\"17\"><WindowPos x=\"1\" y=\"2\"/></base>\r\n", text);

    const empty = (try templateFromClipboard(allocator, "<clip>\n<item A=\"1\"/>\n</clip>")).?;
    defer allocator.free(empty);
    try testing.expectEqualStrings("<?xml version=\"1.0\"?>\r\n<base A=\"1\"/>\r\n", empty);

    try testing.expect((try templateFromClipboard(allocator, "<clip>\n</clip>")) == null);
    try testing.expect((try templateFromClipboard(allocator, "<clip><itemx A=\"1\"/></clip>")) == null);
    try testing.expect((try templateFromClipboard(allocator, "garbage")) == null);

    var buffer: [32]u8 = undefined;
    try testing.expectEqualStrings("Buttons00.xml", nextTemplateName(&buffer, "Buttons", &.{}).?);
    try testing.expectEqualStrings("Buttons02.xml", nextTemplateName(&buffer, "Buttons", &.{ "Buttons00.xml", "buttons01.XML" }).?);
    try testing.expectEqualStrings("Buttons", templateFolder(0x10001103).?);
    try testing.expect(templateFolder(0x10001199) == null);

    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);
    try testing.expect((try rig.overlay.templateText(rig.b())) == null);
}

test "the status line names the mode, the selection and the last command" {
    const allocator = testing.allocator;
    var rig: Rig = undefined;
    try rig.init(allocator);
    defer rig.deinit(allocator);
    var buffer: [256]u8 = undefined;
    try testing.expectEqualStrings("mode free | 0 selected | nothing done yet", rig.overlay.status(&buffer));
    try rig.overlay.selection.ids.append(allocator, 1);
    try rig.overlay.nudge(rig.b(), &rig.doc, &rig.hist, 2, 0);
    try testing.expectEqualStrings("mode free | 1 selected (first is primary) | nudged 1", rig.overlay.status(&buffer));
}

test "a canvas rect lands in the Game's centred, uniformly scaled screen" {
    // 1920 x 1000: scale 1000 / 768, the 4:3 area starts at x = (1920 - 1333.33) / 2.
    const whole = gameRect(.{ .x1 = 0, .y1 = 0, .x2 = 1024, .y2 = 768 }, 1920, 1000, 0);
    try testing.expectEqual(@as(u32, 293), whole.x1);
    try testing.expectEqual(@as(u32, 0), whole.y1);
    try testing.expectEqual(@as(u32, 1627), whole.x2);
    try testing.expectEqual(@as(u32, 1000), whole.y2);
    const part = gameRect(.{ .x1 = 100, .y1 = 100, .x2 = 200, .y2 = 150 }, 1024, 768, 2);
    try testing.expectEqual(PixelRect{ .x1 = 98, .y1 = 98, .x2 = 202, .y2 = 152 }, part);
    // A margin never leaves the frame.
    const edge = gameRect(.{ .x1 = 0, .y1 = 0, .x2 = 1024, .y2 = 768 }, 1024, 768, 8);
    try testing.expectEqual(PixelRect{ .x1 = 0, .y1 = 0, .x2 = 1024, .y2 = 768 }, edge);
}

test "differing pixels are counted inside a box and outside the holes" {
    var a = [_]u8{0} ** (4 * 4 * 4);
    var b = a;
    // Pixels (1,1) and (3,3) change, the second by less than the tolerance.
    b[(1 * 4 + 1) * 4] = 100;
    b[(3 * 4 + 3) * 4 + 1] = 3;
    try testing.expectEqual(@as(usize, 1), differing(&a, &b, 4, 4, 8, null, &.{}));
    try testing.expectEqual(@as(usize, 2), differing(&a, &b, 4, 4, 2, null, &.{}));
    try testing.expectEqual(@as(usize, 1), differing(&a, &b, 4, 4, 2, .{ .x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2 }, &.{}));
    try testing.expectEqual(@as(usize, 1), differing(&a, &b, 4, 4, 2, null, &.{.{ .x1 = 0, .y1 = 0, .x2 = 2, .y2 = 2 }}));
    try testing.expectEqual(@as(usize, 0), differing(&a, &b, 4, 4, 2, .{ .x1 = 2, .y1 = 0, .x2 = 4, .y2 = 2 }, &.{}));
}

test "the Game's shot name gives the frame size" {
    const size = autoshotSize("autoshot_400_1920x1000.rgba").?;
    try testing.expectEqual(@as(u32, 1920), size.width);
    try testing.expectEqual(@as(u32, 1000), size.height);
    try testing.expect(autoshotSize("autoshot_400_1920.rgba") == null);
    try testing.expect(autoshotSize("shot_400_2x2.rgba") == null);
    try testing.expect(autoshotSize("autoshot_400_0x5.rgba") == null);
}
