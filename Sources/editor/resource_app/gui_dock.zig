//! The GUI sub-editor's windows (kind gui): the canvas with its outlined
//! controls, class labels, a highlighted primary selection and eight resize
//! handles; the template palette (Data/Editor/UI folders plus the user's
//! own); and the inspector of the selected control. Every decision is
//! gui_logic.zig's, tested in test-resource-app-logic; this file draws,
//! forwards the mouse and keys, and writes the one file "Create new template"
//! makes, into the user's template folder only.
const std = @import("std");
const imgui = @import("editor_imgui");
const core = @import("resource_core");
const gl = @import("gui_logic.zig");
const logic = @import("panels_logic.zig");

const ig = imgui.c;
const ResBridge = core.bridge.ResBridge;
const GuiWindow = core.bridge.GuiWindow;
const geometry = core.gui_geometry;
const gui_tools = core.gui_tools;

/// The drag and drop payload type a palette entry carries: the template's
/// path as text, null terminated.
const payload_type = "GUI_TEMPLATE";

pub const Rect = struct { x: f32, y: f32, w: f32, h: f32 };

const ink_outline: u32 = 0xff909090;
const ink_primary: u32 = 0xff00ffff;
const ink_selected: u32 = 0xffffff00;
const ink_preview: u32 = 0xffffffff;
const ink_text: u32 = 0xffd0d0d0;

/// The canvas, palette and inspector windows as the auto tier places them.
pub const Layout = struct { canvas: Rect, palette: Rect, inspector: Rect };

pub const GuiDock = struct {
    gpa: std.mem.Allocator,
    io: std.Io,
    overlay: ?gl.Overlay = null,
    palette: gl.Palette = .{},
    palette_loaded: bool = false,
    /// The user's template folder; empty when there is none (an automated run).
    user_folder: [logic.path_capacity]u8 = undefined,
    user_folder_len: usize = 0,
    note: [256]u8 = undefined,
    note_len: usize = 0,
    /// The inspector's attribute being typed in, and its text.
    editing: ?usize = null,
    edit_buffer: [256]u8 = @splat(0),
    /// Where the canvas was drawn in the last frame (screen pixels), for the
    /// auto tier to aim its pointer at; null while the window is closed.
    shown: ?Rect = null,
    /// The same for the screen's 1024 x 768 area and its scale.
    view: gl.View = .{},
    /// Where the auto tier puts the three windows, so its pointer aims at known places and no other
    /// window covers them; null leaves them to the user's layout.
    layout: ?Layout = null,
    /// Where each palette entry was drawn in the last frame (parallel to `palette.entries`; a zero
    /// width marks one that was not drawn, a folded folder), for the auto tier to start a drag on.
    entry_rects: std.ArrayListUnmanaged(Rect) = .empty,

    pub fn init(gpa: std.mem.Allocator, io: std.Io) GuiDock {
        return .{ .gpa = gpa, .io = io };
    }

    pub fn deinit(self: *GuiDock) void {
        if (self.overlay) |*overlay| overlay.deinit();
        self.overlay = null;
        self.palette.deinit(self.gpa);
        self.entry_rects.deinit(self.gpa);
    }

    /// The screen rect a palette entry (`folder`, then the file's name without its extension) was drawn
    /// at in the last frame, or null when it was not drawn.
    pub fn entryRect(self: *const GuiDock, folder: []const u8, stem: []const u8) ?Rect {
        for (self.palette.entries, 0..) |entry, index| {
            if (!std.mem.eql(u8, entry.folder, folder)) continue;
            if (!std.mem.eql(u8, std.fs.path.stem(entry.file), stem)) continue;
            if (index >= self.entry_rects.items.len or self.entry_rects.items[index].w <= 0) return null;
            return self.entry_rects.items[index];
        }
        return null;
    }

    /// The project is not a GUI screen any more: nothing of it stays.
    pub fn close(self: *GuiDock) void {
        if (self.overlay) |*overlay| overlay.deinit();
        self.overlay = null;
        self.palette.deinit(self.gpa);
        self.palette_loaded = false;
        self.editing = null;
        self.shown = null;
    }

    pub fn setUserFolder(self: *GuiDock, folder: []const u8) void {
        const len = @min(folder.len, self.user_folder.len);
        @memcpy(self.user_folder[0..len], folder[0..len]);
        self.user_folder_len = len;
        self.palette_loaded = false;
    }

    fn userFolder(self: *const GuiDock) []const u8 {
        return self.user_folder[0..self.user_folder_len];
    }

    pub fn overlayFor(self: *GuiDock) *gl.Overlay {
        if (self.overlay == null) self.overlay = gl.Overlay.init(self.gpa);
        return &self.overlay.?;
    }

    fn say(self: *GuiDock, comptime fmt: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.note, fmt, args) catch self.note[0..];
        self.note_len = text.len;
    }

    fn report(self: *GuiDock, b: ResBridge, what: []const u8, err: core.bridge.EditError) void {
        const why = b.lastMessage();
        if (why.len != 0) self.say("{s}: {s}", .{ what, why }) else self.say("{s}: {s}", .{ what, @errorName(err) });
    }

    fn loadPalette(self: *GuiDock, b: ResBridge) void {
        if (self.palette_loaded) return;
        self.palette_loaded = true;
        self.palette.deinit(self.gpa);
        self.palette = gl.Palette.load(self.gpa, b, self.userFolder()) catch |err| {
            self.report(b, "templates", err);
            return;
        };
    }

    // --- The windows -----------------------------------------------------------

    /// The canvas, the palette and the inspector while a GUI screen is open.
    pub fn draw(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle) void {
        if (!life.is_open or life.doc.kind != .gui_frame) {
            self.close();
            return;
        }
        const windows = gui_tools.readWindows(self.gpa, b) catch |err| {
            self.report(b, "windows", err);
            return;
        };
        defer self.gpa.free(windows);
        const overlay = self.overlayFor();
        overlay.selection.prune(windows);
        self.loadPalette(b);
        self.drawCanvas(b, life, overlay, windows);
        self.drawPalette(overlay);
        self.drawInspector(b, life, overlay, windows);
    }

    fn drawCanvas(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay, windows: []const GuiWindow) void {
        const display = ig.igGetIO().*.DisplaySize;
        if (self.layout) |layout| {
            place(layout.canvas);
        } else {
            ig.igSetNextWindowPos(.{ .x = 320, .y = 28 }, ig.ImGuiCond_FirstUseEver);
            ig.igSetNextWindowSize(.{ .x = @max(400, display.x - 640), .y = @max(300, display.y - 80) }, ig.ImGuiCond_FirstUseEver);
        }
        defer ig.igEnd();
        self.shown = null;
        if (!ig.igBegin("GUI canvas###gui_canvas", null, 0)) return;
        const read_only = life.read_only;

        // The Align menu's actions as a toolbar; the context menu has them too.
        ig.igBeginDisabled(read_only or overlay.selection.ids.items.len < 2);
        self.drawAlignButtons(b, life, overlay);
        ig.igEndDisabled();
        if (self.note_len != 0) ig.igTextDisabled("%.*s", @as(c_int, @intCast(self.note_len)), &self.note);

        var avail = ig.igGetContentRegionAvail();
        avail.y = @max(32, avail.y - ig.igGetTextLineHeightWithSpacing());
        avail.x = @max(32, avail.x);
        const top_left = ig.igGetCursorScreenPos();
        _ = ig.igInvisibleButton("canvas", avail, ig.ImGuiButtonFlags_MouseButtonLeft);
        overlay.view = gl.View.fit(top_left.x, top_left.y, avail.x, avail.y);
        self.view = overlay.view;
        self.shown = .{ .x = top_left.x, .y = top_left.y, .w = avail.x, .h = avail.y };

        if (!read_only) self.feedMouse(b, life, overlay);
        self.drawContextMenu(b, life, overlay);
        if (!read_only) self.acceptDrop(b, life, overlay);
        if (!read_only) self.handleKeys(b, life, overlay);
        self.drawScreen(b, overlay, windows);

        var status_buffer: [320]u8 = undefined;
        const status = overlay.status(&status_buffer);
        ig.igTextDisabled("%.*s", @as(c_int, @intCast(status.len)), status.ptr);
    }

    fn place(rect: Rect) void {
        ig.igSetNextWindowPos(.{ .x = rect.x, .y = rect.y }, ig.ImGuiCond_Always);
        ig.igSetNextWindowSize(.{ .x = rect.w, .y = rect.h }, ig.ImGuiCond_Always);
    }

    fn drawAlignButtons(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay) void {
        const aligns = [_]struct { label: [:0]const u8, mode: geometry.Align }{
            .{ .label = "Left", .mode = .left },
            .{ .label = "Top", .mode = .top },
            .{ .label = "Right", .mode = .right },
            .{ .label = "Bottom", .mode = .bottom },
        };
        ig.igTextUnformatted("Align:");
        for (aligns) |entry| {
            ig.igSameLine();
            if (ig.igSmallButton(entry.label.ptr)) self.alignSelection(b, life, overlay, entry.mode);
        }
        const equals = [_]struct { label: [:0]const u8, mode: geometry.Equal }{
            .{ .label = "Equal width", .mode = .width },
            .{ .label = "Equal height", .mode = .height },
            .{ .label = "Equal size", .mode = .size },
        };
        for (equals) |entry| {
            ig.igSameLine();
            if (ig.igSmallButton(entry.label.ptr)) self.equalSelection(b, life, overlay, entry.mode);
        }
    }

    /// The Edit menu's Align submenu and the clipboard items, for the screen
    /// open now; call inside the menu. Nothing is drawn for other kinds.
    pub fn drawEditMenuItems(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle) void {
        if (!life.is_open or life.doc.kind != .gui_frame) return;
        const overlay = self.overlayFor();
        ig.igSeparator();
        const can_edit = !life.read_only;
        const selected = overlay.selection.ids.items.len;
        if (ig.igMenuItemEx("Copy windows", "Ctrl+C", false, selected != 0)) overlay.copySelection(b) catch |err| self.report(b, "copy", err);
        if (ig.igMenuItemEx("Cut windows", "Ctrl+X", false, can_edit and selected != 0)) overlay.cutSelection(b, &life.doc, &life.history) catch |err| self.report(b, "cut", err);
        if (ig.igMenuItemEx("Paste windows", "Ctrl+V", false, can_edit and overlay.clip != null)) overlay.pasteClipboard(b, &life.doc, &life.history) catch |err| self.report(b, "paste", err);
        if (ig.igBeginMenuEx("Align", can_edit and selected >= 2)) {
            self.drawAlignItems(b, life, overlay);
            ig.igEndMenu();
        }
    }

    fn drawAlignItems(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay) void {
        if (ig.igMenuItemEx("Left", null, false, true)) self.alignSelection(b, life, overlay, .left);
        if (ig.igMenuItemEx("Top", null, false, true)) self.alignSelection(b, life, overlay, .top);
        if (ig.igMenuItemEx("Right", null, false, true)) self.alignSelection(b, life, overlay, .right);
        if (ig.igMenuItemEx("Bottom", null, false, true)) self.alignSelection(b, life, overlay, .bottom);
        ig.igSeparator();
        if (ig.igMenuItemEx("Equal width", null, false, true)) self.equalSelection(b, life, overlay, .width);
        if (ig.igMenuItemEx("Equal height", null, false, true)) self.equalSelection(b, life, overlay, .height);
        if (ig.igMenuItemEx("Equal size", null, false, true)) self.equalSelection(b, life, overlay, .size);
    }

    fn alignSelection(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay, mode: geometry.Align) void {
        overlay.alignSelection(b, &life.doc, &life.history, mode) catch |err| self.report(b, "align", err);
    }

    fn equalSelection(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay, mode: geometry.Equal) void {
        overlay.equalize(b, &life.doc, &life.history, mode) catch |err| self.report(b, "equal size", err);
    }

    fn feedMouse(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay) void {
        const mouse = ig.igGetMousePos();
        const at: core.bridge.Point2 = .{ .x = mouse.x, .y = mouse.y };
        if (ig.igIsItemActivated()) overlay.press(b, at, ig.igGetIO().*.KeyShift) catch |err| self.report(b, "select", err);
        if (ig.igIsItemActive()) overlay.move(at);
        if (ig.igIsItemDeactivated()) overlay.release(b, &life.doc, &life.history, at) catch |err| self.report(b, "gesture", err);
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) overlay.cancel();
    }

    fn acceptDrop(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay) void {
        if (!ig.igBeginDragDropTarget()) return;
        defer ig.igEndDragDropTarget();
        const payload = ig.igAcceptDragDropPayload(payload_type, 0) orelse return;
        const size: usize = @intCast(payload.*.DataSize);
        if (size == 0) return;
        const bytes = @as([*]const u8, @ptrCast(payload.*.Data))[0..size];
        const path = std.mem.sliceTo(bytes, 0);
        const mouse = ig.igGetMousePos();
        overlay.drop(b, &life.doc, &life.history, path, .{ .x = mouse.x, .y = mouse.y }) catch |err| self.report(b, "place", err);
    }

    fn handleKeys(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay) void {
        const io = ig.igGetIO();
        if (io.*.WantTextInput or !ig.igIsWindowFocused(ig.ImGuiFocusedFlags_RootAndChildWindows)) return;
        const step: i32 = if (io.*.KeyShift) 10 else 1;
        const doc = &life.doc;
        const history = &life.history;
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_LeftArrow, true)) overlay.nudge(b, doc, history, -step, 0) catch |err| self.report(b, "nudge", err);
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_RightArrow, true)) overlay.nudge(b, doc, history, step, 0) catch |err| self.report(b, "nudge", err);
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_UpArrow, true)) overlay.nudge(b, doc, history, 0, -step) catch |err| self.report(b, "nudge", err);
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_DownArrow, true)) overlay.nudge(b, doc, history, 0, step) catch |err| self.report(b, "nudge", err);
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_Delete, false)) overlay.deleteSelection(b, doc, history) catch |err| self.report(b, "delete", err);
        const command = io.*.KeyCtrl or io.*.KeySuper;
        if (!command) return;
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_C, false)) overlay.copySelection(b) catch |err| self.report(b, "copy", err);
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_X, false)) overlay.cutSelection(b, doc, history) catch |err| self.report(b, "cut", err);
        if (ig.igIsKeyPressedEx(ig.ImGuiKey_V, false)) overlay.pasteClipboard(b, doc, history) catch |err| self.report(b, "paste", err);
    }

    fn drawContextMenu(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay) void {
        if (!ig.igBeginPopupContextItemEx("##gui_context", ig.ImGuiPopupFlags_MouseButtonRight)) return;
        defer ig.igEndPopup();
        const can_edit = !life.read_only;
        const selected = overlay.selection.ids.items.len;
        if (ig.igMenuItemEx("Copy", "Ctrl+C", false, selected != 0)) overlay.copySelection(b) catch |err| self.report(b, "copy", err);
        if (ig.igMenuItemEx("Cut", "Ctrl+X", false, can_edit and selected != 0)) overlay.cutSelection(b, &life.doc, &life.history) catch |err| self.report(b, "cut", err);
        if (ig.igMenuItemEx("Paste", "Ctrl+V", false, can_edit and overlay.clip != null)) overlay.pasteClipboard(b, &life.doc, &life.history) catch |err| self.report(b, "paste", err);
        if (ig.igMenuItemEx("Delete", "Delete", false, can_edit and selected != 0)) overlay.deleteSelection(b, &life.doc, &life.history) catch |err| self.report(b, "delete", err);
        ig.igSeparator();
        if (ig.igBeginMenuEx("Align", can_edit and selected >= 2)) {
            self.drawAlignItems(b, life, overlay);
            ig.igEndMenu();
        }
        ig.igSeparator();
        if (ig.igMenuItemEx("Create new template", null, false, selected == 1 and self.user_folder_len != 0)) self.createTemplate(b, overlay);
    }

    /// OnCreatenewtemplate: the one selected control as a template file in the
    /// user's folder (never Data), named for its class and the next free number.
    fn createTemplate(self: *GuiDock, b: ResBridge, overlay: *gl.Overlay) void {
        const id = overlay.selection.primary() orelse return;
        const windows = gui_tools.readWindows(self.gpa, b) catch |err| return self.report(b, "new template", err);
        defer self.gpa.free(windows);
        const class_type = if (geometry.find(windows, id)) |w| w.class_type else return;
        const stem = gl.templateFolder(class_type) orelse return self.say("new template: a {s} has no template folder", .{gl.className(class_type)});
        const text = (overlay.templateText(b) catch |err| return self.report(b, "new template", err)) orelse return self.say("new template: the control cannot be written as a template", .{});
        defer self.gpa.free(text);
        const folder = self.userFolder();
        var taken: std.ArrayListUnmanaged([]const u8) = .empty;
        defer taken.deinit(self.gpa);
        for (self.palette.entries) |entry| taken.append(self.gpa, entry.file) catch return;
        var name_buffer: [64]u8 = undefined;
        const name = gl.nextTemplateName(&name_buffer, stem, taken.items) orelse return self.say("new template: {s}00 to 99 are all taken", .{stem});
        var path_buffer: [logic.path_capacity + 64]u8 = undefined;
        const path = std.fmt.bufPrint(&path_buffer, "{s}{c}{s}", .{ folder, std.fs.path.sep, name }) catch return self.say("new template: the path is too long", .{});
        const cwd = std.Io.Dir.cwd();
        cwd.createDirPath(self.io, folder) catch |err| return self.say("new template: {s}: {s}", .{ folder, @errorName(err) });
        cwd.writeFile(self.io, .{ .sub_path = path, .data = text }) catch |err| return self.say("new template: {s}: {s}", .{ path, @errorName(err) });
        self.palette_loaded = false;
        self.say("new template written: {s}", .{path});
    }

    fn drawScreen(self: *GuiDock, b: ResBridge, overlay: *gl.Overlay, windows: []const GuiWindow) void {
        const list = ig.igGetWindowDrawList();
        const view = overlay.view;
        const whole = view.rectToScreen(.{ .x1 = 0, .y1 = 0, .x2 = gl.canvas_width, .y2 = gl.canvas_height });
        ig.ImDrawList_AddRectFilled(list, .{ .x = whole.x1, .y = whole.y1 }, .{ .x = whole.x2, .y = whole.y2 }, 0xff202020);
        ig.ImDrawList_AddRectEx(list, .{ .x = whole.x1, .y = whole.y1 }, .{ .x = whole.x2, .y = whole.y2 }, 0xff606060, 0, 1, 0);
        for (windows) |w| {
            if (w.parent < 0) continue;
            const rc = geometry.canvasRect(windows, w.id) orelse continue;
            const at = view.rectToScreen(rc);
            const primary = overlay.selection.primary() == w.id;
            const selected = overlay.selection.contains(w.id);
            const ink = if (primary) ink_primary else if (selected) ink_selected else ink_outline;
            ig.ImDrawList_AddRectEx(list, .{ .x = at.x1, .y = at.y1 }, .{ .x = at.x2, .y = at.y2 }, ink, 0, if (selected) 2 else 1, 0);
            var label: [64]u8 = undefined;
            const text = std.fmt.bufPrint(&label, "{s} {d}", .{ gl.className(w.class_type), w.element_id }) catch "";
            if (text.len != 0 and at.x2 - at.x1 > 24) ig.ImDrawList_AddTextEx(list, .{ .x = at.x1 + 3, .y = at.y1 + 2 }, ink_text, text.ptr, text.ptr + text.len);
        }
        if (overlay.selection.primary()) |primary| {
            if (geometry.canvasRect(windows, primary)) |rc| drawHandles(list, view.rectToScreen(rc));
        }
        const previews = overlay.preview(b) catch return;
        defer self.gpa.free(previews);
        for (previews) |rc| {
            const at = view.rectToScreen(rc);
            ig.ImDrawList_AddRectEx(list, .{ .x = at.x1, .y = at.y1 }, .{ .x = at.x2, .y = at.y2 }, ink_preview, 0, 1, 0);
        }
    }

    // --- The palette -----------------------------------------------------------

    fn drawPalette(self: *GuiDock, overlay: *gl.Overlay) void {
        if (self.layout) |layout| {
            place(layout.palette);
        } else {
            ig.igSetNextWindowPos(.{ .x = 8, .y = 340 }, ig.ImGuiCond_FirstUseEver);
            ig.igSetNextWindowSize(.{ .x = 300, .y = 300 }, ig.ImGuiCond_FirstUseEver);
        }
        defer ig.igEnd();
        self.entry_rects.clearRetainingCapacity();
        self.entry_rects.appendNTimes(self.gpa, .{ .x = 0, .y = 0, .w = 0, .h = 0 }, self.palette.entries.len) catch {};
        if (!ig.igBegin("Templates###gui_palette", null, 0)) return;
        if (self.palette.entries.len == 0) {
            ig.igTextDisabled("No templates found.");
            return;
        }
        ig.igTextDisabled("Drag a template onto the canvas, or pick one and click it there.");
        if (ig.igButton("Disarm")) overlay.arm(null) catch {};
        var folder: []const u8 = "";
        var open = false;
        for (self.palette.entries, 0..) |entry, index| {
            if (!std.mem.eql(u8, entry.folder, folder)) {
                if (open) ig.igTreePop();
                folder = entry.folder;
                var folder_buffer: [96]u8 = undefined;
                const label = std.mem.printSentinel(&folder_buffer, "{s}##folder", .{entry.folder}, 0) catch "?";
                open = ig.igTreeNodeEx(label.ptr, ig.ImGuiTreeNodeFlags_DefaultOpen);
            }
            if (!open) continue;
            var name_buffer: [128]u8 = undefined;
            const label = std.mem.printSentinel(&name_buffer, "{s}##t{d}", .{ entry.file, index }, 0) catch continue;
            const armed = if (overlay.armed) |path| std.mem.eql(u8, path, entry.path) else false;
            if (ig.igSelectableEx(label.ptr, armed, 0, .{ .x = 0, .y = 0 })) overlay.arm(entry.path) catch {};
            if (index < self.entry_rects.items.len) {
                const low = ig.igGetItemRectMin();
                const high = ig.igGetItemRectMax();
                self.entry_rects.items[index] = .{ .x = low.x, .y = low.y, .w = high.x - low.x, .h = high.y - low.y };
            }
            if (ig.igBeginDragDropSource(0)) {
                var path_buffer: [logic.path_capacity]u8 = undefined;
                const len = @min(entry.path.len, path_buffer.len - 1);
                @memcpy(path_buffer[0..len], entry.path[0..len]);
                path_buffer[len] = 0;
                _ = ig.igSetDragDropPayload(payload_type, &path_buffer, len + 1, 0);
                ig.igTextUnformatted(label.ptr);
                ig.igEndDragDropSource();
            }
        }
        if (open) ig.igTreePop();
    }

    // --- The inspector -----------------------------------------------------------

    fn drawInspector(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay, windows: []const GuiWindow) void {
        const display = ig.igGetIO().*.DisplaySize;
        if (self.layout) |layout| {
            place(layout.inspector);
        } else {
            ig.igSetNextWindowPos(.{ .x = display.x - 300, .y = 28 }, ig.ImGuiCond_FirstUseEver);
            ig.igSetNextWindowSize(.{ .x = 290, .y = 420 }, ig.ImGuiCond_FirstUseEver);
        }
        defer ig.igEnd();
        if (!ig.igBegin("GUI properties###gui_inspector", null, 0)) return;
        const id = overlay.selection.primary() orelse {
            self.editing = null;
            ig.igTextDisabled("Select a control on the canvas.");
            return;
        };
        const window = geometry.find(windows, id) orelse return;
        ig.igText("%s, window %d", gl.className(window.class_type).ptr, id);
        ig.igBeginDisabled(life.read_only);
        defer ig.igEndDisabled();

        // Pos, Size and PositionFlag are one rect edit.
        var edit: core.bridge.GuiRect = .{ .id = id, .flag = window.flag, .x = window.x, .y = window.y, .w = window.w, .h = window.h };
        var changed = false;
        changed = intField("Pos x", &edit.x) or changed;
        changed = intField("Pos y", &edit.y) or changed;
        changed = intField("Size w", &edit.w) or changed;
        changed = intField("Size h", &edit.h) or changed;
        changed = anchorField("Horizontal anchor", &edit.flag, 0xf, &horizontal_anchors) or changed;
        changed = anchorField("Vertical anchor", &edit.flag, 0xf0, &vertical_anchors) or changed;
        if (changed) overlay.setLocal(b, &life.doc, &life.history, edit) catch |err| self.report(b, "set rect", err);

        ig.igSeparator();
        for (gl.inspector_attrs, 0..) |name, index| self.drawAttr(b, life, overlay, id, name, index);
    }

    fn drawAttr(self: *GuiDock, b: ResBridge, life: *logic.Lifecycle, overlay: *gl.Overlay, id: i32, name: []const u8, index: usize) void {
        var label_buffer: [64]u8 = undefined;
        const label = std.mem.printSentinel(&label_buffer, "{s}##attr{d}", .{ name, index }, 0) catch return;
        var shown: [256]u8 = @splat(0);
        const typing = self.editing == index;
        if (typing) {
            shown = self.edit_buffer;
        } else {
            const value = gl.readAttr(b, id, name, shown[0 .. shown.len - 1]);
            shown[value.len] = 0;
        }
        const entered = ig.igInputText(label.ptr, &shown, shown.len, ig.ImGuiInputTextFlags_EnterReturnsTrue);
        if (ig.igIsItemActive() and !typing) self.editing = index;
        if (self.editing == index) self.edit_buffer = shown;
        if (entered or (ig.igIsItemDeactivatedAfterEdit() and self.editing == index)) {
            const value = std.mem.sliceTo(&shown, 0);
            overlay.setAttribute(b, &life.doc, &life.history, id, name, value) catch |err| self.report(b, "set attribute", err);
            self.editing = null;
        } else if (self.editing == index and !ig.igIsItemActive()) self.editing = null;
    }
};

fn intField(label: [:0]const u8, value: *i32) bool {
    var v: c_int = value.*;
    if (!ig.igInputIntEx(label.ptr, &v, 1, 10, ig.ImGuiInputTextFlags_EnterReturnsTrue)) return false;
    value.* = v;
    return true;
}

const Anchor = struct { label: [:0]const u8, bits: i32 };
const horizontal_anchors = [_]Anchor{ .{ .label = "Left", .bits = 0x1 }, .{ .label = "Middle", .bits = 0x2 }, .{ .label = "Right", .bits = 0x3 } };
const vertical_anchors = [_]Anchor{ .{ .label = "Top", .bits = 0x10 }, .{ .label = "Middle", .bits = 0x20 }, .{ .label = "Bottom", .bits = 0x30 } };

fn anchorField(label: [:0]const u8, flag: *i32, mask: i32, anchors: []const Anchor) bool {
    var current: [:0]const u8 = "none";
    for (anchors) |a| {
        if ((flag.* & mask) == a.bits) current = a.label;
    }
    var changed = false;
    if (ig.igBeginCombo(label.ptr, current.ptr, 0)) {
        for (anchors) |a| {
            if (ig.igSelectableEx(a.label.ptr, (flag.* & mask) == a.bits, 0, .{ .x = 0, .y = 0 })) {
                flag.* = (flag.* & ~mask) | a.bits;
                changed = true;
            }
        }
        ig.igEndCombo();
    }
    return changed;
}

/// MFC's eight handles around the primary: small squares on the corners and
/// the edge middles.
fn drawHandles(list: *ig.ImDrawList, rc: geometry.Rect) void {
    const cx = (rc.x1 + rc.x2) / 2;
    const cy = (rc.y1 + rc.y2) / 2;
    const points = [_][2]f32{
        .{ rc.x1, rc.y1 }, .{ cx, rc.y1 }, .{ rc.x2, rc.y1 }, .{ rc.x1, cy },
        .{ rc.x2, cy },    .{ rc.x1, rc.y2 }, .{ cx, rc.y2 }, .{ rc.x2, rc.y2 },
    };
    for (points) |p| {
        ig.ImDrawList_AddRectFilled(list, .{ .x = p[0] - 3, .y = p[1] - 3 }, .{ .x = p[0] + 3, .y = p[1] + 3 }, ink_primary);
    }
}
