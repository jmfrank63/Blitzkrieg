//! The project tree and the object inspector windows (06-PARITY A-27..A-34,
//! A-38): MFC's CTreeDockWnd/CETreeCtrl and CPropView/CCtrlObjectInspector,
//! with its reference, multi-select and browse dialogs. Everything that
//! decides something - the selection, the widget a property gets, the text a
//! pick writes, every edit and its undo step - is in edit_logic.zig and
//! tested there; this file only draws and forwards the user's input.
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const core = @import("resource_core");
const logic = @import("panels_logic.zig");
const edit = @import("edit_logic.zig");
const squad = @import("squad_logic.zig");
const mesh = @import("mesh_logic.zig");

const ig = imgui.c;
const bridge = core.bridge;
const ResBridge = bridge.ResBridge;
const ReferenceEntry = bridge.ReferenceEntry;
const PropRecord = bridge.PropRecord;
const NodeRecord = bridge.NodeRecord;
const Point2 = bridge.Point2;

const drag_payload = "BK_RES_NODE";
const rename_popup = "Rename item";
const picker_popup = "Choose reference";
const mod_label = if (builtin.os.tag == .macos) "Cmd" else "Ctrl";

/// Where a browse dialog's callback writes its answer: SDL may call back on
/// another thread or after the window is gone, so it is a global.
var browse_slot: BrowseSlot = .{};

const BrowseSlot = struct {
    state: std.atomic.Value(u8) = .init(@intFromEnum(State.idle)),
    buffer: [logic.path_capacity]u8 = undefined,
    len: usize = 0,

    const State = enum(u8) { idle, waiting, arrived, cancelled };

    fn request(self: *BrowseSlot) bool {
        return self.state.cmpxchgStrong(@intFromEnum(State.idle), @intFromEnum(State.waiting), .acquire, .monotonic) == null;
    }

    fn deliver(self: *BrowseSlot, text: ?[]const u8) void {
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

    /// The chosen path once it arrived (null while waiting); "" is a cancel.
    fn take(self: *BrowseSlot) ?[]const u8 {
        const state: State = @enumFromInt(self.state.load(.acquire));
        const result: []const u8 = switch (state) {
            .idle, .waiting => return null,
            .arrived => self.buffer[0..self.len],
            .cancelled => "",
        };
        self.state.store(@intFromEnum(State.idle), .release);
        return result;
    }
};

fn browseCallback(userdata: ?*anyopaque, filelist: [*c]const [*c]const u8, filter: c_int) callconv(.c) void {
    _ = filter;
    const slot: *BrowseSlot = @ptrCast(@alignCast(userdata orelse return));
    if (filelist == null) return slot.deliver(null);
    const first = filelist[0];
    slot.deliver(if (first != null) std.mem.span(first) else null);
}

/// A property of the selection the user is working on.
const PropKey = struct { node: i32, prop_id: i32 };

pub const Panels = struct {
    selection: edit.Selection = .{},
    gestures: edit.Gestures = .{},
    refs: edit.RefLists = .{},
    /// The tree's display order of the last frame (Shift-click takes runs of it).
    order: std.ArrayListUnmanaged(i32) = .empty,
    order_next: std.ArrayListUnmanaged(i32) = .empty,
    /// The strings of the props shown, read once per selection and history
    /// revision (combo choices, browse folders).
    strings: std.ArrayListUnmanaged(StringsEntry) = .empty,
    strings_revision: u32 = std.math.maxInt(u32),
    strings_primary: ?i32 = null,
    /// The edit box being typed in; its text lives here until it is committed.
    active: ?PropKey = null,
    edit_buffer: [edit.value_capacity]u8 = [_]u8{0} ** edit.value_capacity,
    /// The colour drag's gesture: its frames are one undo step.
    color_gesture: u32 = 0,
    rename_node: ?i32 = null,
    rename_buffer: [bridge.name_capacity]u8 = [_]u8{0} ** bridge.name_capacity,
    rename_open: bool = false,
    picker: ?Picker = null,
    picker_open: bool = false,
    browse_for: ?PropKey = null,
    tree_focused: bool = false,
    /// SquadFrm's formation view: the gesture in progress and the mode.
    overlay: ?squad.Overlay = null,
    /// The unit preview's toolbar and the markers it last read.
    mesh_toolbar: mesh.Toolbar = .{},
    mesh_locators: [mesh.locator_capacity]bridge.MeshLocator = undefined,
    status: [256]u8 = undefined,
    status_len: usize = 0,

    const StringsEntry = struct { key: PropKey, entries: []ReferenceEntry };

    const Picker = struct {
        prop_id: i32,
        ref_type: edit.RefType,
        /// CMultySelDialog (actions): a mask of checked actions, else a
        /// single choice (CReferenceDialog).
        multi: bool,
        mask: u64 = 0,
        kind: edit.ValueKind,
        chosen: ?usize = null,
        filter: [64]u8 = [_]u8{0} ** 64,
    };

    pub fn deinit(self: *Panels, gpa: std.mem.Allocator) void {
        self.selection.deinit(gpa);
        self.refs.deinit(gpa);
        self.order.deinit(gpa);
        self.order_next.deinit(gpa);
        self.dropStrings(gpa);
        self.strings.deinit(gpa);
    }

    fn dropStrings(self: *Panels, gpa: std.mem.Allocator) void {
        for (self.strings.items) |entry| gpa.free(entry.entries);
        self.strings.clearRetainingCapacity();
    }

    /// The reference lists follow the active mod: call after a mod switch.
    pub fn modChanged(self: *Panels, gpa: std.mem.Allocator) void {
        self.refs.invalidate(gpa);
    }

    fn say(self: *Panels, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.status, format, args) catch self.status[0..];
        self.status_len = text.len;
    }

    fn report(self: *Panels, b: ResBridge, what: []const u8, err: bridge.EditError) void {
        const why = b.lastMessage();
        if (why.len != 0) self.say("{s}: {s}", .{ what, why }) else self.say("{s}: {s}", .{ what, @errorName(err) });
    }

    fn target(life: *logic.Lifecycle, gpa: std.mem.Allocator, b: ResBridge) edit.Target {
        return .{ .allocator = gpa, .bridge = b, .doc = &life.doc, .history = &life.history, .read_only = life.read_only };
    }

    // --- Edit menu and keys --------------------------------------------------

    /// Edit menu items, inside the main menu bar's Edit menu.
    pub fn drawEditMenuItems(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        const state = life.menuState();
        if (ig.igMenuItemEx("Undo", mod_label ++ "+Z", false, logic.isEnabled(.undo, state))) self.undo(gpa, b, life);
        if (ig.igMenuItemEx("Redo", mod_label ++ "+Y", false, logic.isEnabled(.redo, state))) self.redo(gpa, b, life);
        ig.igSeparator();
        const primary = self.selection.primary;
        const can_edit = life.is_open and !life.read_only;
        const insertable = if (primary) |p| edit.insertClassFor(&life.doc, p) != null else false;
        if (ig.igMenuItemEx("Insert item", "Insert", false, can_edit and insertable)) self.insertUnder(gpa, b, life, primary.?);
        if (ig.igMenuItemEx("Delete item", "Delete", false, can_edit and self.selection.ids.items.len != 0)) self.deleteSelected(gpa, b, life);
        if (ig.igMenuItemEx("Rename item", "F2", false, can_edit and primary != null)) self.beginRename(life, primary.?);
    }

    /// Ctrl/Cmd+Z, +Y and +Shift+Z anywhere; Delete, Insert and F2 while the
    /// tree has the focus. Nothing while a text field has the keyboard.
    pub fn handleShortcuts(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        const io = ig.igGetIO();
        if (io.*.WantTextInput or !life.is_open) return;
        const ctrl = io.*.KeyCtrl or io.*.KeySuper;
        const key: edit.Key = if (ig.igIsKeyPressedEx(ig.ImGuiKey_Z, true)) .z else if (ig.igIsKeyPressedEx(ig.ImGuiKey_Y, true)) .y else if (ig.igIsKeyPressedEx(ig.ImGuiKey_Delete, false)) .delete else if (ig.igIsKeyPressedEx(ig.ImGuiKey_Insert, false)) .insert else if (ig.igIsKeyPressedEx(ig.ImGuiKey_F2, false)) .f2 else .other;
        switch (edit.shortcutFor(key, ctrl, io.*.KeyShift, self.tree_focused)) {
            .none => {},
            .undo => self.undo(gpa, b, life),
            .redo => self.redo(gpa, b, life),
            .delete => self.deleteSelected(gpa, b, life),
            .insert => if (self.selection.primary) |p| self.insertUnder(gpa, b, life, p),
            .rename => if (self.selection.primary) |p| self.beginRename(life, p),
        }
    }

    fn undo(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        self.active = null;
        _ = edit.undo(target(life, gpa, b)) catch |err| self.report(b, "undo", err);
        self.selection.prune(&life.doc);
    }

    fn redo(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        self.active = null;
        _ = edit.redo(target(life, gpa, b)) catch |err| self.report(b, "redo", err);
        self.selection.prune(&life.doc);
    }

    fn insertUnder(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, parent: i32) void {
        if (edit.insertClassFor(&life.doc, parent) == null) {
            self.say("this item takes no inserted children here", .{});
            return;
        }
        const id = edit.insert(target(life, gpa, b), parent) catch |err| return self.report(b, "insert", err);
        _ = edit.setExpand(target(life, gpa, b), parent, true) catch {};
        self.selection.only(gpa, id) catch {};
    }

    fn deleteSelected(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        if (self.selection.ids.items.len == 0) return;
        edit.deleteNodes(target(life, gpa, b), self.selection.ids.items) catch |err| return self.report(b, "delete", err);
        self.selection.prune(&life.doc);
    }

    fn beginRename(self: *Panels, life: *logic.Lifecycle, node: i32) void {
        if (life.read_only) return;
        const record = edit.findNode(&life.doc, node) orelse return;
        @memset(&self.rename_buffer, 0);
        const name = record.displaySlice();
        @memcpy(self.rename_buffer[0..name.len], name);
        self.rename_node = node;
        self.rename_open = true;
    }

    // --- Windows -----------------------------------------------------------

    /// The tree and the inspector, then the dialogs they opened. Call once a
    /// frame while a project may be open.
    pub fn draw(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, window: ?*sdl3.c.SDL_Window) void {
        if (!life.is_open) {
            self.selection.clear();
            self.active = null;
            return;
        }
        self.selection.prune(&life.doc);
        if (self.selection.primary == null) if (edit.rootId(&life.doc)) |root| self.selection.only(gpa, root) catch {};
        self.drawTree(gpa, b, life);
        self.drawInspector(gpa, b, life, window);
        if (life.active == .squad) self.drawFormation(gpa, b, life) else self.overlay = null;
        if (life.active == .mesh_unit) self.drawMeshPreview(gpa, b, life) else self.mesh_toolbar.reset();
        self.drawRename(gpa, b, life);
        self.drawPicker(gpa, b, life);
        self.takeBrowse(gpa, b, life);
    }

    fn drawTree(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        const display = ig.igGetIO().*.DisplaySize;
        ig.igSetNextWindowPos(.{ .x = 8, .y = 28 }, ig.ImGuiCond_FirstUseEver);
        ig.igSetNextWindowSize(.{ .x = 300, .y = @max(200, display.y * 0.55) }, ig.ImGuiCond_FirstUseEver);
        var title_buffer: [96]u8 = undefined;
        const title = std.fmt.bufPrintZ(&title_buffer, "Project{s}###project_tree", .{if (life.read_only) " (read-only)" else ""}) catch "Project###project_tree";
        defer ig.igEnd();
        if (!ig.igBegin(title.ptr, null, 0)) {
            self.tree_focused = false;
            return;
        }
        self.tree_focused = ig.igIsWindowFocused(ig.ImGuiFocusedFlags_RootAndChildWindows);
        self.order_next.clearRetainingCapacity();
        const root = edit.rootId(&life.doc) orelse return;
        self.drawNode(gpa, b, life, root, 0);
        std.mem.swap(std.ArrayListUnmanaged(i32), &self.order, &self.order_next);
    }

    fn drawNode(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, id: i32, depth: usize) void {
        // A malformed tree (a cycle) must not recurse forever.
        if (depth > life.doc.tree.nodes.items.len) return;
        const record = (edit.findNode(&life.doc, id) orelse return).*;
        self.order_next.append(gpa, id) catch {};
        const has_children = edit.childCount(&life.doc, id) != 0;
        var flags: c_int = ig.ImGuiTreeNodeFlags_OpenOnArrow | ig.ImGuiTreeNodeFlags_OpenOnDoubleClick | ig.ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!has_children) flags |= ig.ImGuiTreeNodeFlags_Leaf;
        if (self.selection.contains(id)) flags |= ig.ImGuiTreeNodeFlags_Selected;
        var label_buffer: [bridge.name_capacity + 32]u8 = undefined;
        const name = record.displaySlice();
        const label = std.fmt.bufPrintZ(&label_buffer, "{s}##node{d}", .{ if (name.len != 0) name else record.classSlice(), id }) catch return;
        if (has_children) ig.igSetNextItemOpen(record.expand, ig.ImGuiCond_Always);
        const open = ig.igTreeNodeEx(label.ptr, flags);
        if (ig.igIsItemToggledOpen() and has_children) {
            _ = edit.setExpand(target(life, gpa, b), id, !record.expand) catch |err| self.report(b, "expand", err);
        } else if (ig.igIsItemClicked()) {
            const io = ig.igGetIO();
            self.selection.click(gpa, id, io.*.KeyCtrl or io.*.KeySuper, io.*.KeyShift, self.order.items) catch {};
            self.active = null;
        }
        self.dragAndDrop(gpa, b, life, id);
        self.contextMenu(gpa, b, life, id);
        if (open) {
            if (has_children) {
                var i: i32 = 0;
                while (edit.childAt(&life.doc, id, i)) |child| : (i += 1) self.drawNode(gpa, b, life, child, depth + 1);
            }
            ig.igTreePop();
        }
    }

    fn dragAndDrop(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, id: i32) void {
        if (life.read_only) return;
        if (ig.igBeginDragDropSource(0)) {
            _ = ig.igSetDragDropPayload(drag_payload, &id, @sizeOf(i32), 0);
            const record = edit.findNode(&life.doc, id);
            const name = if (record) |r| r.displaySlice() else "";
            var buffer: [bridge.name_capacity + 1]u8 = undefined;
            const text = std.fmt.bufPrintZ(&buffer, "{s}", .{name}) catch "";
            ig.igTextUnformatted(text.ptr);
            ig.igEndDragDropSource();
        }
        if (ig.igBeginDragDropTarget()) {
            if (ig.igAcceptDragDropPayload(drag_payload, 0)) |payload| {
                if (payload.*.DataSize == @sizeOf(i32)) {
                    const dragged = @as(*const i32, @ptrCast(@alignCast(payload.*.Data))).*;
                    if (edit.dropPlace(&life.doc, dragged, id)) |place| {
                        edit.move(target(life, gpa, b), dragged, place) catch |err| self.report(b, "move", err);
                    } else self.say("the item cannot go there", .{});
                }
            }
            ig.igEndDragDropTarget();
        }
    }

    /// MFC's right-click menus (IDR_INSERT_TREE_ITEM_MENU,
    /// IDR_DELETE_TREE_ITEM), with Rename and Move Up / Down beside them.
    fn contextMenu(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, id: i32) void {
        if (!ig.igBeginPopupContextItemEx(null, ig.ImGuiPopupFlags_MouseButtonRight)) return;
        defer ig.igEndPopup();
        if (!self.selection.contains(id)) self.selection.only(gpa, id) catch {};
        const can_edit = !life.read_only;
        const is_root = edit.rootId(&life.doc) == id;
        const place = edit.placeOf(&life.doc, id);
        if (ig.igMenuItemEx("Insert item", "Insert", false, can_edit and edit.insertClassFor(&life.doc, id) != null)) self.insertUnder(gpa, b, life, id);
        if (ig.igMenuItemEx("Delete item", "Delete", false, can_edit and !is_root)) self.deleteSelected(gpa, b, life);
        const actions = squad.treeActionsFor(&life.doc, id);
        for (actions.constSlice()) |action| {
            if (ig.igMenuItemEx(action.label().ptr, null, false, can_edit)) {
                squad.runTreeAction(gpa, b, &life.doc, &life.history, action, id) catch |err| return self.report(b, "tree action", err);
                self.selection.prune(&life.doc);
            }
        }
        if (ig.igMenuItemEx("Rename item", "F2", false, can_edit)) self.beginRename(life, id);
        ig.igSeparator();
        const up_ok = can_edit and !is_root and place != null and place.?.index > 0;
        const down_ok = can_edit and !is_root and place != null and place.?.index + 1 < edit.childCount(&life.doc, place.?.parent);
        if (ig.igMenuItemEx("Move up", null, false, up_ok)) edit.moveBy(target(life, gpa, b), id, -1) catch |err| self.report(b, "move", err);
        if (ig.igMenuItemEx("Move down", null, false, down_ok)) edit.moveBy(target(life, gpa, b), id, 1) catch |err| self.report(b, "move", err);
    }

    /// SquadFrm's view of the formation: a marker per member, the zero point
    /// as a cross and the formation's direction as an arrow. The three
    /// modes (move a member, set the zero point, turn the arrow) are
    /// squad_logic's Overlay; this only draws it and feeds it the mouse.
    fn drawFormation(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        const tools = core.sub_editor_tools;
        const display = ig.igGetIO().*.DisplaySize;
        ig.igSetNextWindowPos(.{ .x = display.x - 408, .y = 28 }, ig.ImGuiCond_FirstUseEver);
        ig.igSetNextWindowSize(.{ .x = 400, .y = 360 }, ig.ImGuiCond_FirstUseEver);
        defer ig.igEnd();
        if (!ig.igBegin("Formation###formation", null, 0)) return;
        // The selected formation, else the first one.
        var formation: ?i32 = null;
        if (self.selection.primary) |node| if (edit.findNode(&life.doc, node)) |record| {
            if (tools.isClass(record, tools.item_type.squad_formation_props)) formation = node;
        };
        if (formation == null) formation = tools.firstOfClass(&life.doc, tools.item_type.squad_formation_props);
        const node = formation orelse {
            ig.igTextDisabled("This squad has no formation.");
            return;
        };
        if (self.overlay == null or self.overlay.?.formation != node) {
            if (self.overlay) |*old| old.cancel(b);
            self.overlay = squad.Overlay.init(gpa, node);
        }
        const overlay = &self.overlay.?;
        const can_edit = !life.read_only;
        const modes = [_]struct { mode: squad.Mode, label: [:0]const u8 }{
            .{ .mode = .drag, .label = "Move members" },
            .{ .mode = .set_zero, .label = "Set zero point" },
            .{ .mode = .direction, .label = "Direction arrow" },
        };
        for (modes, 0..) |entry, i| {
            if (i != 0) ig.igSameLine();
            if (ig.igRadioButton(entry.label.ptr, overlay.mode == entry.mode)) overlay.setMode(b, entry.mode);
        }
        var slots = tools.readGeometry(b, node, .formation_positions) catch return;
        defer slots.deinit(gpa);
        const zero = (tools.readGeometry(b, node, .zero_point) catch return).point2;
        const direction = (tools.readGeometry(b, node, .formation_direction) catch return).point2.x;

        const avail = ig.igGetContentRegionAvail();
        const size: ig.ImVec2 = .{ .x = @max(60, avail.x), .y = @max(60, avail.y) };
        const top_left = ig.igGetCursorScreenPos();
        _ = ig.igInvisibleButton("canvas", size, ig.ImGuiButtonFlags_MouseButtonLeft);
        // The zero point stays at the canvas centre while no gesture runs.
        if (!overlay.busy()) overlay.view = .{
            .origin = .{ .x = top_left.x + size.x / 2 - zero.x * overlay.view.scale, .y = top_left.y + size.y / 2 + zero.y * overlay.view.scale },
            .scale = overlay.view.scale,
        };
        if (can_edit) {
            const mouse = ig.igGetMousePos();
            const at: Point2 = .{ .x = mouse.x, .y = mouse.y };
            if (ig.igIsItemActivated()) overlay.press(b, at) catch |err| self.report(b, "formation", err);
            if (ig.igIsItemActive()) overlay.move(b, at) catch |err| self.report(b, "formation", err);
            if (ig.igIsItemDeactivated()) overlay.release(b, &life.doc, &life.history, at) catch |err| self.report(b, "formation", err);
            if (ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) overlay.cancel(b);
        }

        const draw_list = ig.igGetWindowDrawList();
        ig.ImDrawList_PushClipRect(draw_list, top_left, .{ .x = top_left.x + size.x, .y = top_left.y + size.y }, true);
        defer ig.ImDrawList_PopClipRect(draw_list);
        ig.ImDrawList_AddRectFilled(draw_list, top_left, .{ .x = top_left.x + size.x, .y = top_left.y + size.y }, ig.igGetColorU32(ig.ImGuiCol_FrameBg));
        const ink = ig.igGetColorU32(ig.ImGuiCol_Text);
        const marker = ig.igGetColorU32(ig.ImGuiCol_PlotHistogram);
        const centre = overlay.view.toScreen(zero);
        ig.ImDrawList_AddLineEx(draw_list, .{ .x = centre.x - 8, .y = centre.y }, .{ .x = centre.x + 8, .y = centre.y }, ink, 1);
        ig.ImDrawList_AddLineEx(draw_list, .{ .x = centre.x, .y = centre.y - 8 }, .{ .x = centre.x, .y = centre.y + 8 }, ink, 1);
        for (slots.points2, 0..) |slot, i| {
            const at = overlay.view.toScreen(slot);
            ig.ImDrawList_AddCircleFilled(draw_list, .{ .x = at.x, .y = at.y }, 6, marker, 0);
            var number: [8]u8 = undefined;
            const text = std.fmt.bufPrintZ(&number, "{d}", .{i + 1}) catch continue;
            ig.ImDrawList_AddTextEx(draw_list, .{ .x = at.x + 8, .y = at.y - 6 }, ink, text.ptr, text.ptr + text.len);
        }
        const angle = if (overlay.arrowing) overlay.arrow_angle else direction;
        // MFC's world vector from the zero point, mapped by the view like any other world point.
        const toward = squad.arrowDirection(angle);
        const reach = 40 / overlay.view.scale;
        const tip_at = overlay.view.toScreen(.{ .x = zero.x + toward.x * reach, .y = zero.y + toward.y * reach });
        const tip: ig.ImVec2 = .{ .x = tip_at.x, .y = tip_at.y };
        ig.ImDrawList_AddLineEx(draw_list, .{ .x = centre.x, .y = centre.y }, tip, ink, 2);
        ig.ImDrawList_AddCircleFilled(draw_list, tip, 3, ink, 0);
    }

    /// The unit preview's toolbar (MFC's combat, install and transportable
    /// buttons and the two display toggles) and the locator markers drawn
    /// over the scene from the screen positions the engine reports. A
    /// right-click on the scene, not on a window, picks the nearest marker
    /// and selects its Locators child; nothing is recorded in the history.
    fn drawMeshPreview(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        const display = ig.igGetIO().*.DisplaySize;
        ig.igSetNextWindowPosEx(.{ .x = display.x / 2, .y = 28 }, ig.ImGuiCond_FirstUseEver, .{ .x = 0.5, .y = 0 });
        ig.igSetNextWindowSize(.{ .x = 0, .y = 0 }, ig.ImGuiCond_FirstUseEver);
        const toolbar = &self.mesh_toolbar;
        if (ig.igBegin("Unit preview###mesh_preview", null, ig.ImGuiWindowFlags_AlwaysAutoResize)) {
            const variants = [_]mesh.Variant{ .combat, .install, .transportable };
            for (variants, 0..) |variant, i| {
                if (i != 0) ig.igSameLine();
                if (ig.igRadioButton(variant.label().ptr, toolbar.variant == variant)) toolbar.setVariant(b, variant) catch |err| self.report(b, "model variant", err);
            }
            var locators = toolbar.show_locators;
            var boxes = toolbar.show_bounding_boxes;
            const changed_locators = ig.igCheckbox("Show locators", &locators);
            ig.igSameLine();
            const changed_boxes = ig.igCheckbox("Bounding boxes", &boxes);
            if (changed_locators or changed_boxes) toolbar.setShow(b, locators, boxes) catch |err| self.report(b, "show locators", err);
        }
        ig.igEnd();
        if (!toolbar.show_locators) return;

        const markers = mesh.readLocators(b, &self.mesh_locators) catch |err| return self.report(b, "locators", err);
        const draw_list = ig.igGetForegroundDrawList();
        const ink = ig.igGetColorU32(ig.ImGuiCol_PlotHistogram);
        const active = ig.igGetColorU32(ig.ImGuiCol_Text);
        var selected: ?i32 = null;
        if (self.selection.primary) |node| selected = node;
        for (markers) |marker| {
            const at: ig.ImVec2 = .{ .x = marker.sx, .y = marker.sy };
            const is_active = if (selected) |node| (mesh.nodeForLocator(&life.doc, marker) orelse -1) == node else false;
            ig.ImDrawList_AddCircleFilled(draw_list, at, if (is_active) 5 else 3, if (is_active) active else ink, 0);
            // The active locator's line runs to the scene's origin marker above it.
            if (is_active) ig.ImDrawList_AddLineEx(draw_list, at, .{ .x = at.x, .y = at.y - 24 }, active, 2);
        }
        const io = ig.igGetIO();
        if (ig.igIsMouseClickedEx(ig.ImGuiMouseButton_Right, false) and !io.*.WantCaptureMouse) {
            const mouse = ig.igGetMousePos();
            const at: Point2 = .{ .x = mouse.x, .y = mouse.y };
            const pick = mesh.pickAndSelect(gpa, &life.doc, &self.selection, markers, at) catch return;
            switch (pick) {
                .node => {},
                .miss => |nearest| {
                    var text: [160]u8 = undefined;
                    self.say("{s}", .{mesh.missText(&text, at, nearest)});
                },
            }
        }
    }

    fn drawRename(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        if (self.rename_open) {
            _ = ig.igOpenPopup(rename_popup, 0);
            self.rename_open = false;
        }
        if (!ig.igBeginPopupModal(rename_popup, null, ig.ImGuiWindowFlags_AlwaysAutoResize)) return;
        defer ig.igEndPopup();
        if (ig.igIsWindowAppearing()) ig.igSetKeyboardFocusHere();
        const entered = ig.igInputText("Name", &self.rename_buffer, self.rename_buffer.len, ig.ImGuiInputTextFlags_EnterReturnsTrue);
        if (entered or ig.igButton("OK")) {
            if (self.rename_node) |node| {
                const name = std.mem.sliceTo(&self.rename_buffer, 0);
                edit.rename(target(life, gpa, b), node, name) catch |err| self.report(b, "rename", err);
            }
            self.rename_node = null;
            ig.igCloseCurrentPopup();
        }
        ig.igSameLine();
        if (ig.igButton("Cancel") or ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) {
            self.rename_node = null;
            ig.igCloseCurrentPopup();
        }
    }

    // --- Inspector -----------------------------------------------------------

    fn stringsOf(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, key: PropKey) []const ReferenceEntry {
        for (self.strings.items) |entry| if (entry.key.node == key.node and entry.key.prop_id == key.prop_id) return entry.entries;
        const entries = edit.readEntries(gpa, b, .{ .prop = .{ .node = key.node, .prop_id = key.prop_id } }) catch return &.{};
        self.strings.append(gpa, .{ .key = key, .entries = entries }) catch {
            gpa.free(entries);
            return &.{};
        };
        return entries;
    }

    fn drawInspector(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, window: ?*sdl3.c.SDL_Window) void {
        const display = ig.igGetIO().*.DisplaySize;
        ig.igSetNextWindowPos(.{ .x = 8, .y = 36 + @max(200, display.y * 0.55) }, ig.ImGuiCond_FirstUseEver);
        ig.igSetNextWindowSize(.{ .x = 300, .y = @max(160, display.y * 0.4 - 44) }, ig.ImGuiCond_FirstUseEver);
        defer ig.igEnd();
        if (!ig.igBegin("Properties###inspector", null, 0)) return;
        if (self.strings_revision != life.history.revision or self.strings_primary != self.selection.primary) {
            self.dropStrings(gpa);
            self.strings_revision = life.history.revision;
            self.strings_primary = self.selection.primary;
        }
        const primary = self.selection.primary orelse return;
        const node = edit.findNode(&life.doc, primary) orelse return;
        var heading: [bridge.name_capacity + 48]u8 = undefined;
        const others = self.selection.ids.items.len -| 1;
        const head = if (others == 0)
            std.fmt.bufPrintZ(&heading, "{s}", .{node.displaySlice()}) catch ""
        else
            std.fmt.bufPrintZ(&heading, "{s} (+{d} selected)", .{ node.displaySlice(), others }) catch "";
        ig.igSeparatorText(head.ptr);
        if (life.read_only) ig.igBeginDisabled(true);
        defer if (life.read_only) ig.igEndDisabled();
        if (ig.igBeginTable("props", 2, ig.ImGuiTableFlags_RowBg | ig.ImGuiTableFlags_Resizable)) {
            // By index, and the record copied: an edit reloads the mirror's
            // lists under this loop.
            var i: usize = 0;
            while (i < life.doc.tree.props.items.len) : (i += 1) {
                const entry = life.doc.tree.props.items[i];
                if (entry.node != primary) continue;
                const prop = entry.record;
                _ = ig.igTableNextColumn();
                const label = prop.displaySlice();
                ig.igTextUnformattedEx(label.ptr, label.ptr + label.len);
                _ = ig.igTableNextColumn();
                ig.igPushIDInt(prop.id);
                self.drawProp(gpa, b, life, primary, &prop, window);
                ig.igPopID();
                // The props list may have been reloaded by the edit.
                if (edit.findNode(&life.doc, primary) == null) break;
            }
            ig.igEndTable();
        }
        if (self.status_len != 0) {
            ig.igSeparator();
            ig.igPushTextWrapPos(0);
            ig.igTextUnformattedEx(&self.status, @as([*]const u8, &self.status) + self.status_len);
            ig.igPopTextWrapPos();
        }
    }

    fn write(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, prop_id: i32, text: []const u8, gesture: u32) void {
        var targets: std.ArrayListUnmanaged(i32) = .empty;
        defer targets.deinit(gpa);
        edit.editTargets(gpa, &life.doc, &self.selection, prop_id, &targets) catch return;
        edit.setProp(target(life, gpa, b), targets.items, prop_id, text, gesture) catch |err| return self.report(b, "edit", err);
        self.status_len = 0;
    }

    fn drawProp(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, node: i32, prop: *const PropRecord, window: ?*sdl3.c.SDL_Window) void {
        const domain = edit.domainOf(prop);
        const kind = edit.valueKindOf(prop);
        const widget = edit.widgetFor(domain);
        const key: PropKey = .{ .node = node, .prop_id = prop.id };
        ig.igSetNextItemWidth(-std.math.floatMin(f32));
        switch (widget) {
            .read_only => {
                const value = prop.valueSlice();
                ig.igTextUnformattedEx(value.ptr, value.ptr + value.len);
            },
            .check => {
                const options = self.stringsOf(gpa, b, key);
                var on = edit.boolValue(kind, options, prop.valueSlice());
                if (ig.igCheckbox("##v", &on)) if (edit.boolText(kind, options, on)) |text| self.write(gpa, b, life, prop.id, text.slice(), 0);
            },
            .combo => {
                const options = self.stringsOf(gpa, b, key);
                const current = edit.comboCurrent(kind, options, prop.valueSlice());
                var preview_buffer: [bridge.reference_name_capacity + 1]u8 = undefined;
                const preview = std.fmt.bufPrintZ(&preview_buffer, "{s}", .{if (current) |i| options[i].nameSlice() else prop.valueSlice()}) catch "";
                if (ig.igBeginCombo("##v", preview.ptr, 0)) {
                    for (options, 0..) |*option, i| {
                        var item_buffer: [bridge.reference_name_capacity + 16]u8 = undefined;
                        const item = std.fmt.bufPrintZ(&item_buffer, "{s}##{d}", .{ option.nameSlice(), i }) catch continue;
                        if (ig.igSelectableEx(item.ptr, current == i, 0, .{ .x = 0, .y = 0 })) {
                            if (edit.comboText(kind, options, i)) |text| self.write(gpa, b, life, prop.id, text.slice(), 0);
                        }
                    }
                    ig.igEndCombo();
                }
            },
            .color => {
                var rgb = edit.colorRgb(kind, prop.valueSlice()) orelse [3]f32{ 0, 0, 0 };
                const changed = ig.igColorEdit3("##v", &rgb, 0);
                if (ig.igIsItemActivated()) self.color_gesture = self.gestures.begin();
                if (changed) if (edit.colorText(kind, prop.valueSlice(), rgb)) |text| self.write(gpa, b, life, prop.id, text.slice(), self.color_gesture);
                if (ig.igIsItemDeactivated()) self.color_gesture = 0;
            },
            .text, .int, .hex, .float => self.editBox(gpa, b, life, key, prop, widget, kind, 0),
            .browse_file, .browse_dir => {
                self.editBox(gpa, b, life, key, prop, widget, kind, 32);
                ig.igSameLine();
                if (ig.igSmallButton("...")) self.browse(life, key, widget == .browse_dir, self.stringsOf(gpa, b, key), window);
            },
            .reference, .action_mask => {
                self.editBox(gpa, b, life, key, prop, widget, kind, 32);
                ig.igSameLine();
                if (ig.igSmallButton("...")) {
                    const ref_type = edit.refTypeFor(domain).?;
                    self.picker = .{
                        .prop_id = prop.id,
                        .ref_type = ref_type,
                        .multi = widget == .action_mask,
                        .mask = edit.actionMask(kind, prop.valueSlice()),
                        .kind = kind,
                    };
                    self.picker_open = true;
                }
            },
        }
    }

    /// An edit box committed when it loses the keyboard after an edit (MFC's
    /// WM_USER_LOST_FOCUS), or on Enter: one undo step per commit.
    fn editBox(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle, key: PropKey, prop: *const PropRecord, widget: edit.Widget, kind: edit.ValueKind, room_right: f32) void {
        const is_active = if (self.active) |a| a.node == key.node and a.prop_id == key.prop_id else false;
        var scratch: [edit.value_capacity]u8 = [_]u8{0} ** edit.value_capacity;
        const buffer: *[edit.value_capacity]u8 = if (is_active) &self.edit_buffer else &scratch;
        if (!is_active) {
            const shown = if (widget == .hex) (edit.hexDisplay(kind, prop.valueSlice()) orelse edit.ValueText{}) else edit.ValueText{};
            const text = if (widget == .hex and shown.len != 0) shown.slice() else prop.valueSlice();
            @memcpy(scratch[0..text.len], text);
        }
        if (room_right > 0) ig.igSetNextItemWidth(-room_right);
        _ = ig.igInputText("##v", buffer, buffer.len, 0);
        if (ig.igIsItemActivated()) {
            self.active = key;
            self.edit_buffer = buffer.*;
        }
        if (ig.igIsItemDeactivatedAfterEdit()) {
            const typed = std.mem.sliceTo(if (is_active) &self.edit_buffer else buffer, 0);
            self.active = null;
            if (edit.parseTyped(widget, kind, typed)) |text| {
                self.write(gpa, b, life, prop.id, text.slice(), 0);
            } else self.say("\"{s}\" is not a valid {s}", .{ typed, @tagName(widget) });
        } else if (is_active and !ig.igIsItemActive()) self.active = null;
    }

    // --- Browse (A-32) -----------------------------------------------------------

    fn browse(self: *Panels, life: *logic.Lifecycle, key: PropKey, folder: bool, strings: []const ReferenceEntry, window: ?*sdl3.c.SDL_Window) void {
        if (!browse_slot.request()) return;
        self.browse_for = key;
        var location_buffer: [logic.path_capacity + 1]u8 = undefined;
        const source: []const u8 = if (strings.len > 0) strings[0].nameSlice() else "";
        const project_dir = if (life.doc.pathSlice()) |p| std.fs.path.dirname(p) orelse "" else "";
        const location: ?[*:0]const u8 = blk: {
            const dir = if (source.len != 0 and std.fs.path.isAbsolute(source)) source else project_dir;
            if (dir.len == 0) break :blk null;
            break :blk (std.fmt.bufPrintZ(&location_buffer, "{s}", .{dir}) catch break :blk null).ptr;
        };
        if (folder) {
            sdl3.c.SDL_ShowOpenFolderDialog(browseCallback, &browse_slot, window, location, false);
        } else {
            sdl3.c.SDL_ShowOpenFileDialog(browseCallback, &browse_slot, window, null, 0, location, false);
        }
    }

    fn takeBrowse(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        const chosen = browse_slot.take() orelse return;
        const key = self.browse_for orelse return;
        self.browse_for = null;
        if (chosen.len == 0) return;
        if (self.selection.primary != key.node) return;
        const strings = self.stringsOf(gpa, b, key);
        const source: ?[]const u8 = if (strings.len > 0) strings[0].nameSlice() else null;
        const value = edit.browseValue(chosen, source, life.doc.pathSlice()) orelse return self.say("the chosen path is too long for the property", .{});
        self.write(gpa, b, life, key.prop_id, value.slice(), 0);
    }

    // --- Reference pickers (A-30, A-31) --------------------------------------

    /// MFC's CReferenceDialog (one entry of the property's list, filtered)
    /// and CMultySelDialog (a check per action of actions.ini, written as the
    /// mask of their ids).
    fn drawPicker(self: *Panels, gpa: std.mem.Allocator, b: ResBridge, life: *logic.Lifecycle) void {
        if (self.picker_open) {
            _ = ig.igOpenPopup(picker_popup, 0);
            self.picker_open = false;
        }
        ig.igSetNextWindowSize(.{ .x = 420, .y = 460 }, ig.ImGuiCond_Appearing);
        if (!ig.igBeginPopupModal(picker_popup, null, 0)) return;
        defer ig.igEndPopup();
        if (self.picker == null) {
            ig.igCloseCurrentPopup();
            return;
        }
        const picker = &self.picker.?;
        var title_buffer: [64]u8 = undefined;
        const title = std.fmt.bufPrintZ(&title_buffer, "{s}", .{picker.ref_type.label()}) catch "";
        ig.igSeparatorText(title.ptr);
        const entries = self.refs.get(gpa, b, picker.ref_type) catch |err| {
            self.report(b, "reference list", err);
            ig.igCloseCurrentPopup();
            self.picker = null;
            return;
        };
        ig.igSetNextItemWidth(-std.math.floatMin(f32));
        _ = ig.igInputTextWithHint("##filter", "Filter", &picker.filter, picker.filter.len, 0);
        var hits: std.ArrayListUnmanaged(usize) = .empty;
        defer hits.deinit(gpa);
        edit.filterEntries(gpa, entries, std.mem.sliceTo(&picker.filter, 0), &hits) catch {};
        var accept = false;
        if (ig.igBeginChild("entries", .{ .x = 0, .y = -ig.igGetFrameHeightWithSpacing() }, ig.ImGuiChildFlags_Borders, 0)) {
            for (hits.items) |i| {
                const entry = &entries[i];
                var item_buffer: [bridge.reference_name_capacity + 16]u8 = undefined;
                const item = std.fmt.bufPrintZ(&item_buffer, "{s}##{d}", .{ entry.nameSlice(), i }) catch continue;
                if (picker.multi) {
                    var on = picker.mask & (if (entry.token >= 0 and entry.token < 64) @as(u64, 1) << @intCast(entry.token) else 0) != 0;
                    if (ig.igCheckbox(item.ptr, &on)) picker.mask = edit.toggleAction(picker.mask, entry.token, on);
                } else {
                    if (ig.igSelectableEx(item.ptr, picker.chosen == i, ig.ImGuiSelectableFlags_AllowDoubleClick | ig.ImGuiSelectableFlags_NoAutoClosePopups, .{ .x = 0, .y = 0 })) {
                        picker.chosen = i;
                        if (ig.igIsMouseDoubleClicked(0)) accept = true;
                    }
                }
            }
        }
        ig.igEndChild();
        if (ig.igButton("OK")) accept = true;
        ig.igSameLine();
        const cancel = ig.igButton("Cancel") or ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false);
        if (accept) {
            if (picker.multi) {
                if (edit.actionMaskText(picker.kind, picker.mask)) |text| self.write(gpa, b, life, picker.prop_id, text.slice(), 0);
            } else if (picker.chosen) |i| if (i < entries.len) {
                self.write(gpa, b, life, picker.prop_id, entries[i].nameSlice(), 0);
            };
        }
        if (accept or cancel) {
            self.picker = null;
            ig.igCloseCurrentPopup();
        }
    }
};
