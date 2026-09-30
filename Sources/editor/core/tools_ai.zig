//! The AI-side tools (04-10 on). The Script Areas tool (D-21), the MFC editor's
//! area tab (Sources/src/MapEditor/MapToolState.cpp, TabToolsDialog.cpp): a drag
//! draws a rectangle (the drag's two corners) or a circle (its centre and a point on
//! its edge) and names it; the map's areas are listed in a panel, which also renames
//! and deletes; and - beyond the MFC editor, which only drew, added and deleted - the
//! selected area moves by its centre handle and resizes by its corner or edge handle.
//!
//! Drags are world (Vis) units, the pointer's `world_x`/`world_y`; an area is stored
//! in map (AI) units. The bridge converts, once, with the MFC truncation
//! (`Editor.scriptAreaFromVis`, `scriptAreaMoved`, `scriptAreaResized`); the tool
//! never converts a length itself. A handle's place is compared with the pointer in
//! map units (`Pointer.map_x/y`), the units the area is stored in.
//!
//! One undo step each: an area added, an area deleted, a rename, and a whole move or
//! resize drag (its edits merge by gesture; a position the bridge refuses mid-drag is
//! skipped and the drag goes on, as the Selector does).
const std = @import("std");
const editor_mod = @import("editor.zig");
const fake_mod = @import("fake_bridge.zig");
const bridge_mod = @import("bridge.zig");
const records = @import("records.zig");
const tools = @import("tools.zig");
const Editor = editor_mod.Editor;
const EditError = bridge_mod.EditError;
const Event = tools.Event;
const Pointer = tools.Pointer;

/// How far (screen pixels) a press may move and still be a click.
pub const click_pixels: f32 = 4.0;

/// How near (map units) a press must be to a handle to grab it: about a third of a
/// tile, which at the game's default zoom is a few pixels beyond the handle's drawn
/// square.
pub const handle_radius: f32 = 24.0;

/// The selected area's centre handle, map units: its centre.
pub fn centreHandle(area: records.ScriptArea) [2]f32 {
    return .{ area.cx, area.cy };
}

/// The edge handle, map units: a rectangle's (+, +) corner, a circle's rightmost
/// point. Dragging it anywhere sets the size from its distance to the centre
/// (`Editor.scriptAreaResized`).
pub fn edgeHandle(area: records.ScriptArea) [2]f32 {
    return switch (area.shape) {
        .rectangle => .{ area.cx + area.hx, area.cy + area.hy },
        .circle => .{ area.cx + area.r, area.cy },
    };
}

fn near(handle: [2]f32, pointer: Pointer) bool {
    return std.math.hypot(handle[0] - pointer.map_x, handle[1] - pointer.map_y) <= handle_radius;
}

/// Whether the map point is inside the area.
pub fn contains(area: records.ScriptArea, map_x: f32, map_y: f32) bool {
    return switch (area.shape) {
        .rectangle => @abs(map_x - area.cx) <= area.hx and @abs(map_y - area.cy) <= area.hy,
        .circle => std.math.hypot(map_x - area.cx, map_y - area.cy) <= area.r,
    };
}

pub const ScriptAreas = struct {
    /// The shape the next drag draws; set by the panel.
    shape: records.AreaShape = .rectangle,
    /// The name for the next area, from the panel's field. Empty: the first free
    /// "area_<n>". Cleared once an area takes it.
    name_buffer: [records.area_name_capacity]u8 = [_]u8{0} ** records.area_name_capacity,
    name_len: usize = 0,
    /// The area the panel's list and the handles work on: an index into the map's
    /// list, the last area drawn or clicked. Follows an undo that removes it.
    selected: ?usize = null,
    /// WR-B02: the selected area just before an undo or redo.
    selected_key: ?records.ScriptArea = null,
    selected_len: usize = 0,
    /// A drawing drag: where it began and where it is now, world units; null
    /// between drags. The app draws the ghost from these two.
    start: ?[2]f32 = null,
    current: ?[2]f32 = null,
    dragging: bool = false,
    press_screen: [2]f32 = .{ 0, 0 },
    /// A handle drag: which, the gesture its edits merge under, the area as it was
    /// when the drag began (size and name kept from it) and, for a move, the pointer's
    /// offset from the centre in world units.
    handle_drag: HandleDrag = .none,
    gesture: u32 = 0,
    grab_area: records.ScriptArea = .{},
    grab_world: [2]f32 = .{ 0, 0 },

    pub const HandleDrag = enum { none, move, resize };

    pub fn setName(self: *ScriptAreas, text: []const u8) void {
        const len = @min(text.len, self.name_buffer.len - 1);
        @memset(&self.name_buffer, 0);
        @memcpy(self.name_buffer[0..len], text[0..len]);
        self.name_len = len;
    }

    pub fn name(self: *const ScriptAreas) []const u8 {
        return self.name_buffer[0..self.name_len];
    }

    pub fn captureSelection(self: *ScriptAreas, editor: *Editor) void {
        self.selected_key = null;
        const index = self.selected orelse return;
        const list = editor.scriptAreas(editor.allocator) catch return;
        defer editor.allocator.free(list);
        self.selected_len = list.len;
        if (index < list.len) self.selected_key = list[index];
    }

    pub fn resolveSelection(self: *ScriptAreas, editor: *Editor) void {
        const key = self.selected_key orelse {
            self.selected = null;
            return;
        };
        self.selected_key = null;
        const list = editor.scriptAreas(editor.allocator) catch {
            self.selected = null;
            return;
        };
        defer editor.allocator.free(list);
        self.selected = tools.refind(records.ScriptArea, list, self.selected, key, self.selected_len, records.ScriptArea.eql);
    }

    /// Forgets the drag and the selection, as a map change must; shape and name stay.
    pub fn reset(self: *ScriptAreas) void {
        self.start = null;
        self.current = null;
        self.dragging = false;
        self.handle_drag = .none;
        self.gesture = 0;
        self.selected = null;
    }

    /// The name the next area takes: the panel's field, or the first "area_<n>"
    /// (n from 1) no area of the map has, in `buffer`.
    pub fn nameFor(self: *const ScriptAreas, editor: *Editor, buffer: *[records.area_name_capacity]u8) EditError![]const u8 {
        if (self.name_len != 0) return self.name();
        const areas = try editor.scriptAreas(editor.allocator);
        defer editor.allocator.free(areas);
        var number: usize = 1;
        while (true) : (number += 1) {
            const candidate = std.fmt.bufPrint(buffer, "area_{d}", .{number}) catch return error.Failed;
            var taken = false;
            for (areas) |*area| {
                if (std.mem.eql(u8, area.nameSlice(), candidate)) taken = true;
            }
            if (!taken) return candidate;
        }
    }

    /// The selected area, or null when none is selected or the index no longer names
    /// one (an undo took it away).
    pub fn selectedArea(self: *ScriptAreas, editor: *Editor) ?records.ScriptArea {
        const index = self.selected orelse return null;
        var value: records.Value = undefined;
        if (editor.bridge.readRecord(.script_area, @intCast(index), editor.allocator, &value) != .ok) {
            self.selected = null;
            return null;
        }
        return value.script_area;
    }

    /// AI units per world unit as the bridge answers them, for the grab offset of a move.
    fn mapPerWorld(editor: *Editor) f32 {
        var map_x: f32 = 0;
        var map_y: f32 = 0;
        if (editor.bridge.worldToMap(1000, 0, &map_x, &map_y) != .ok or map_x <= 0) return 1.4142135;
        return map_x / 1000;
    }

    pub fn handle(self: *ScriptAreas, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| try self.press(editor, pointer),
            .drag => |pointer| try self.drag(editor, pointer),
            .release => |pointer| try self.release(editor, pointer),
            .key => |key| switch (key) {
                .delete => {
                    const index = self.selected orelse {
                        editor.note("select a script area first");
                        return;
                    };
                    // IN-B06: a refused delete keeps the selection.
                    editor.deleteScriptArea(index) catch |err| {
                        if (err != error.Refused) self.selected = null;
                        return err;
                    };
                    self.selected = null;
                },
                .escape => {
                    self.start = null;
                    self.current = null;
                    self.dragging = false;
                    self.handle_drag = .none;
                    self.gesture = 0;
                },
                else => {},
            },
            else => {},
        }
    }

    fn press(self: *ScriptAreas, editor: *Editor, pointer: Pointer) EditError!void {
        self.press_screen = .{ pointer.screen_x, pointer.screen_y };
        if (self.selectedArea(editor)) |area| {
            // The centre handle wins over the edge handle of a small area.
            const grabbed: HandleDrag = if (near(centreHandle(area), pointer)) .move else if (near(edgeHandle(area), pointer)) .resize else .none;
            if (grabbed != .none) {
                self.handle_drag = grabbed;
                self.gesture = editor.beginGesture();
                self.grab_area = area;
                const ratio = mapPerWorld(editor);
                self.grab_world = .{ area.cx / ratio - pointer.world_x, area.cy / ratio - pointer.world_y };
                return;
            }
        }
        self.start = .{ pointer.world_x, pointer.world_y };
        self.current = self.start;
        self.dragging = true;
    }

    fn drag(self: *ScriptAreas, editor: *Editor, pointer: Pointer) EditError!void {
        if (self.handle_drag != .none) {
            const index = self.selected orelse return;
            const changed = switch (self.handle_drag) {
                .move => editor.scriptAreaMoved(self.grab_area, pointer.world_x + self.grab_world[0], pointer.world_y + self.grab_world[1]),
                .resize => editor.scriptAreaResized(self.grab_area, pointer.world_x, pointer.world_y),
                .none => unreachable,
            } catch |err| {
                if (err == error.Refused) return;
                return err;
            };
            // A position the bridge will not take is skipped: the area stays at the
            // last one it took and the drag goes on; the status line says why.
            editor.editScriptArea(index, changed, self.gesture) catch |err| if (err != error.Refused) return err;
            return;
        }
        if (self.dragging) self.current = .{ pointer.world_x, pointer.world_y };
    }

    fn release(self: *ScriptAreas, editor: *Editor, pointer: Pointer) EditError!void {
        if (self.handle_drag != .none) {
            self.handle_drag = .none;
            self.gesture = 0;
            return;
        }
        if (!self.dragging) return;
        const begin = self.start.?;
        const end: [2]f32 = .{ pointer.world_x, pointer.world_y };
        self.start = null;
        self.current = null;
        self.dragging = false;
        const dx = pointer.screen_x - self.press_screen[0];
        const dy = pointer.screen_y - self.press_screen[1];
        if (dx * dx + dy * dy <= click_pixels * click_pixels) {
            // A click: the last area under it (the one drawn on top), or none.
            const areas = try editor.scriptAreas(editor.allocator);
            defer editor.allocator.free(areas);
            var picked: ?usize = null;
            for (areas, 0..) |area, index| {
                if (contains(area, pointer.map_x, pointer.map_y)) picked = index;
            }
            self.selected = picked;
            if (picked == null) editor.note("drag to draw a script area");
            return;
        }
        var name_buffer: [records.area_name_capacity]u8 = undefined;
        const area_name = try self.nameFor(editor, &name_buffer);
        const area = try editor.scriptAreaFromVis(self.shape, begin[0], begin[1], end[0], end[1], area_name);
        // A drag that converts to nothing - both half sizes or the radius 0 - is not
        // an area: a note, not a command.
        const empty = switch (area.shape) {
            .rectangle => area.hx == 0 and area.hy == 0,
            .circle => area.r == 0,
        };
        if (empty) {
            editor.note("that area is too small; drag further");
            return;
        }
        self.selected = try editor.addScriptArea(area);
        self.setName("");
    }
};

/// The Start Target tool (04-11, D-17), the MFC editor's start-command target click
/// (ObjectPlacerState.cpp, OnLButtonUp with the start-command dialog open): entered
/// from the Start Commands window's "Set target" button with the command's `index`,
/// it takes ONE click and is left. A click on an object makes that object the
/// target (`link_id`; a soldier already answers his squad) and keeps the point; a
/// click on the ground sets the point - the map position with the MFC truncation -
/// and clears `link_id` to 0. One undo step each. A click the bridge refuses leaves
/// the tool active (the status says why; Escape leaves it); the app returns to the
/// tool it came from once `done` is set.
pub const StartTarget = struct {
    /// The start command the click sets the target of.
    index: ?usize = null,
    /// Set once the tool has done its one click (or was cancelled): the app leaves it.
    done: bool = false,

    pub fn reset(self: *StartTarget) void {
        self.index = null;
        self.done = false;
    }

    pub fn handle(self: *StartTarget, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .release => |pointer| try self.click(editor, pointer),
            .key => |key| if (key == .escape) {
                self.done = true;
            },
            else => {},
        }
    }

    fn click(self: *StartTarget, editor: *Editor, pointer: Pointer) EditError!void {
        const index = self.index orelse {
            editor.note("choose a start command and press Set target first");
            self.done = true;
            return;
        };
        try setTarget(editor, index, pointer);
        self.done = true;
    }

    /// The command `index` with its target at the pointer: the object under it, or
    /// else the ground point. One editStartCommand; Refused when the command is gone
    /// or the bridge will not take it, and then nothing has changed.
    pub fn setTarget(editor: *Editor, index: usize, pointer: Pointer) EditError!void {
        const commands = try editor.startCommands(editor.allocator);
        defer Editor.freeStartCommands(editor.allocator, commands);
        if (index >= commands.len) {
            editor.note("that start command is gone");
            return error.Refused;
        }
        var command = commands[index];
        if (pointer.object) |object| {
            command.link_id = object;
        } else {
            command.link_id = 0;
            command.x = records.truncateToAi(pointer.map_x);
            command.y = records.truncateToAi(pointer.map_y);
        }
        try editor.editStartCommand(index, command, 0);
    }
};

/// The Reserve Positions tool (04-11, D-18), the MFC editor's artillery positions mode
/// (ObjectPlacerState.cpp lines 525-641, TemplateEditorFrame1.cpp SaveReservePosition),
/// entered from Unit -> Artillery positions mode. The click order is the MFC
/// editor's: a click on a gun - a self-propelled or a towed one - records it; a click
/// on a truck able to tow (for a towed gun, optional until Enter) records that; a
/// click on the ground records the place, the map position with the MFC
/// truncation. Enter commits the position through `Editor.addReservePosition`: the
/// bridge's refusal (a towed gun with no truck, a truck too weak for the gun, a place
/// off the map) is the editor's status and the pending choice stays for another try.
/// Escape clears the pending choice; Delete deletes the position selected in the
/// panel's list. Every click is the release, as in the MFC editor; one undo step for
/// a position added and one for a position deleted.
///
/// The pieces of a click are read through the bridge: which role a clicked object has
/// (`Editor.reserveRole`), never guessed from its name.
pub const ReservePositions = struct {
    /// The pending choice: the gun and its role, the truck, and the place.
    gun: ?i32 = null,
    gun_role: bridge_mod.ReserveRole = .none,
    truck: ?i32 = null,
    has_place: bool = false,
    x: f32 = 0,
    y: f32 = 0,
    /// The position the panel's list and Delete work on: an index into the map's list,
    /// the last one committed or clicked in the list.
    selected: ?usize = null,

    /// WR-B02: the selected position just before an undo or redo.
    selected_key: ?records.ReservePosition = null,
    selected_len: usize = 0,

    pub fn captureSelection(self: *ReservePositions, editor: *Editor) void {
        self.selected_key = null;
        const index = self.selected orelse return;
        const list = editor.reservePositions(editor.allocator) catch return;
        defer editor.allocator.free(list);
        self.selected_len = list.len;
        if (index < list.len) self.selected_key = list[index];
    }

    pub fn resolveSelection(self: *ReservePositions, editor: *Editor) void {
        const key = self.selected_key orelse {
            self.selected = null;
            return;
        };
        self.selected_key = null;
        const list = editor.reservePositions(editor.allocator) catch {
            self.selected = null;
            return;
        };
        defer editor.allocator.free(list);
        self.selected = tools.refind(records.ReservePosition, list, self.selected, key, self.selected_len, records.ReservePosition.eql);
    }

    /// Forgets the pending choice and the selection, as a map change must.
    pub fn reset(self: *ReservePositions) void {
        self.clearPending();
        self.selected = null;
    }

    pub fn clearPending(self: *ReservePositions) void {
        self.gun = null;
        self.gun_role = .none;
        self.truck = null;
        self.has_place = false;
        self.x = 0;
        self.y = 0;
    }

    /// The pending choice as the record a commit would add, or null while the gun or
    /// the place is missing.
    pub fn pendingRecord(self: *const ReservePositions) ?records.ReservePosition {
        const gun = self.gun orelse return null;
        if (!self.has_place) return null;
        return .{ .artillery = gun, .truck = self.truck orelse 0, .x = self.x, .y = self.y };
    }

    pub fn handle(self: *ReservePositions, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .release => |pointer| try self.pick(editor, pointer),
            .key => |key| switch (key) {
                .enter => try self.commit(editor),
                .escape => self.clearPending(),
                .delete => {
                    const index = self.selected orelse {
                        editor.note("select a reserve position in the list first");
                        return;
                    };
                    // IN-B06: a refused delete keeps the selection.
                    editor.deleteReservePosition(index) catch |err| {
                        if (err != error.Refused) self.selected = null;
                        return err;
                    };
                    self.selected = null;
                },
                else => {},
            },
            else => {},
        }
    }

    /// One click: the object under it if the pointer has one, else the ground.
    pub fn pick(self: *ReservePositions, editor: *Editor, pointer: Pointer) EditError!void {
        const link = pointer.object orelse {
            if (self.gun == null) {
                editor.note("click a gun first, then its truck if it is towed, then the place");
                return;
            }
            self.x = records.truncateToAi(pointer.map_x);
            self.y = records.truncateToAi(pointer.map_y);
            self.has_place = true;
            return;
        };
        const object = editor.document.find(link) orelse {
            editor.note("that object is not on the map");
            return;
        };
        const role = try editor.reserveRole(object.nameSlice());
        switch (role) {
            .self_propelled, .towed => {
                // A gun: the choice starts over with it, the place kept.
                self.gun = link;
                self.gun_role = role;
                self.truck = null;
            },
            .truck => {
                if (self.gun == null) {
                    editor.note("click the gun first: a truck follows a towed gun");
                    return;
                }
                if (self.gun_role != .towed) {
                    editor.note("a self-propelled gun takes no truck");
                    return;
                }
                self.truck = link;
            },
            .none => editor.note("that is not a gun or a truck: click a self-propelled or a towed gun, or a truck"),
        }
    }

    /// Enter: the pending choice is added. Refused, with the pending choice kept, when
    /// the bridge says no; a note when the gun or the place is still missing.
    pub fn commit(self: *ReservePositions, editor: *Editor) EditError!void {
        const record = self.pendingRecord() orelse {
            editor.note("pick a gun and a place first");
            return;
        };
        self.selected = try editor.addReservePosition(record);
        self.clearPending();
    }
};

/// The MFC editor's AI general constants (StateAIGeneral.cpp), in MAP (AI) units: its
/// world-unit handle sizes divided by fAITileXCoeff. A world cell, fWorldCellSize, is
/// 64 AI units, so the handle radii come out as round numbers.
pub const ai_cell: f32 = 64.0;
/// Within this of a parcel's centre handle the press grabs it (PARCEL_CENTER_POINT_RADIUS).
pub const parcel_centre_grab: f32 = ai_cell;
/// Within this of a parcel's arrow handle (PARCEL_ARROW_POINT_RADIUS).
pub const parcel_arrow_grab: f32 = ai_cell / 2.0;
/// Within this of a reinforce point (POSITION_CENTER_POINT_RADIUS).
pub const point_centre_grab: f32 = ai_cell / 1.5;
/// Within this of a point's arrow handle (POSITION_ARROW_POINT_RADIUS).
pub const point_arrow_grab: f32 = ai_cell / 2.0;
/// How far a point's arrow handle is from the point (POSITION_POINT_RADIUS).
pub const point_arrow_length: f32 = ai_cell * 1.5;
/// How far the drawn arrow runs past a parcel's arrow handle (PARCEL_POINT_RADIUS_ARROW).
pub const arrow_tail_length: f32 = ai_cell;

const two_pi: f32 = 6.28318531; // FP_2PI, as the MFC editor's constant
const quarter_turn: f32 = 1.57079633; // FP_PI2

/// The MFC angle of a WORD direction: `dir * 2 pi / 0xFFFF`.
pub fn directionAngle(dir: u16) f32 {
    return @as(f32, @floatFromInt(dir)) * two_pi / 65535.0;
}

/// RotatePoint: the vector turned by `angle`.
pub fn rotate(v: [2]f32, angle: f32) [2]f32 {
    return .{ v[0] * @cos(angle) - v[1] * @sin(angle), v[0] * @sin(angle) + v[1] * @cos(angle) };
}

/// GetPolarAngle: 0..2 pi, -1 for no length.
fn polarAngle(v: [2]f32) f32 {
    if (v[0] == 0 and v[1] == 0) return -1;
    if (v[0] == 0) return if (v[1] > 0) quarter_turn else 3.14159265 + quarter_turn;
    const angle = std.math.atan(v[1] / v[0]);
    if (v[0] > 0) return if (v[1] >= 0) angle else two_pi + angle;
    return 3.14159265 + angle;
}

/// NMapGeometry::DirectionFromArrow: the direction an arrow handle at `arrow` gives
/// about `centre` (AI units): the polar angle of the difference less a quarter turn,
/// wrapped into 0..2 pi, as a WORD.
pub fn directionFromArrow(centre: [2]f32, arrow: [2]f32) u16 {
    var alpha = polarAngle(.{ arrow[0] - centre[0], arrow[1] - centre[1] }) - quarter_turn;
    if (alpha < 0) alpha += two_pi;
    const turned = alpha * 65535.0 / two_pi;
    if (!(turned > 0)) return 0;
    return if (turned >= 65535.0) 65535 else @intFromFloat(turned);
}

/// NMapGeometry::RadiusFromArrow: the distance from `centre` to `arrow`, never below
/// the smallest parcel.
pub fn radiusFromArrow(centre: [2]f32, arrow: [2]f32) f32 {
    return @max(records.parcel_min_radius, std.math.hypot(arrow[0] - centre[0], arrow[1] - centre[1]));
}

/// NMapGeometry::ParcelPointFromVis, from a map point (AI units, not yet cut): the
/// click cut as Vis2AI cuts it, less the parcel's centre, turned by minus the
/// parcel's angle - what a reinforce point stores.
pub fn pointFromMap(click: [2]f32, centre: [2]f32, dir: u16) [2]f32 {
    const cut: [2]f32 = .{ records.truncateToAi(click[0]), records.truncateToAi(click[1]) };
    return rotate(.{ cut[0] - centre[0], cut[1] - centre[1] }, -directionAngle(dir));
}

/// The inverse, for hit tests and drawing (NMapGeometry::ParcelPointToVis in AI
/// units): a stored point turned by the parcel's angle plus its centre.
pub fn pointToMap(point: [2]f32, centre: [2]f32, dir: u16) [2]f32 {
    const turned = rotate(point, directionAngle(dir));
    return .{ turned[0] + centre[0], turned[1] + centre[1] };
}

/// A parcel's centre handle, map units.
pub fn parcelCentreHandle(parcel: records.Parcel) [2]f32 {
    return .{ parcel.cx, parcel.cy };
}

/// A parcel's arrow handle: `radius` from its centre along its direction plus a
/// quarter turn (the drawn line runs from the centre to it).
pub fn parcelArrowHandle(parcel: records.Parcel) [2]f32 {
    const angle = directionAngle(parcel.defence_dir) + quarter_turn;
    return .{ parcel.cx + parcel.radius * @cos(angle), parcel.cy + parcel.radius * @sin(angle) };
}

/// A reinforce point's place on the map.
pub fn pointCentreHandle(parcel: records.Parcel, point: records.ParcelPoint) [2]f32 {
    return pointToMap(.{ point.x, point.y }, .{ parcel.cx, parcel.cy }, parcel.defence_dir);
}

/// A reinforce point's arrow handle: `point_arrow_length` from the point along its own
/// direction plus a quarter turn, all turned with the parcel.
pub fn pointArrowHandle(parcel: records.Parcel, point: records.ParcelPoint) [2]f32 {
    const angle = directionAngle(point.dir) + quarter_turn;
    const end: [2]f32 = .{ point.x + point_arrow_length * @cos(angle), point.y + point_arrow_length * @sin(angle) };
    return pointToMap(end, .{ parcel.cx, parcel.cy }, parcel.defence_dir);
}

fn distance(a: [2]f32, b: [2]f32) f32 {
    return std.math.hypot(a[0] - b[0], a[1] - b[1]);
}

/// The AI General tool (04-12, D-19), the MFC editor's AI general tab
/// (StateAIGeneral.cpp, TabAIGeneralDialog.cpp): it edits ONE side at a time, `side`,
/// set by the panel.
///
///  - A press on a handle grabs it: a parcel's centre (within one world cell), its
///    arrow (half a cell), a reinforce point (two thirds of a cell) or its arrow (half
///    a cell) - points first, in list order, then the parcel, as the MFC editor tests.
///    Dragging moves the centre (the points, stored relative to it, move with it), sets
///    the radius (never below 256) and the defence direction from the arrow, moves the
///    point, or sets its direction. The handle keeps the offset at which it was grabbed.
///  - A press elsewhere inside a parcel adds a reinforce point there: stored relative to
///    the parcel's centre and turned by minus its direction, direction 0. A press outside
///    every parcel (at or beyond the radius) makes a defence parcel of radius 256 AI
///    units (four map tiles, the MFC PARCEL_POINT_RADIUS) and direction 0. Either
///    thing is grabbed at once with no offset, so the press-and-drag places it. A side
///    the map does not have is created, with every side below it empty, and undo takes
///    them away again (the whole side is the record, with the side count).
///  - Enter, Insert and Space switch the selected parcel between defence and
///    reinforce; Delete removes the selected point, else the selected parcel; Escape lets
///    go of the selection. The selection is the last thing a press grabbed or made (or
///    the panel's choice), kept after the release.
///  - One undo step per click, drag or key.
///
/// Parcels and points are MAP (AI) units, like every record the AI side keeps; the
/// pointer's `map_x/map_y` is what the tool compares and stores. The formulas are
/// NMapGeometry's (MapGeometry.h), repeated here, and tested against the same literals.
pub const AIGeneral = struct {
    /// The side the tool edits, from the panel's radios.
    side: usize = 0,
    /// The selection: a parcel of the side, and one of its points or none.
    selected_parcel: ?usize = null,
    selected_point: ?usize = null,
    /// WR-B02: the selected parcel (its own fields, not its points) and point
    /// just before an undo or redo, so `resolveSelection` finds them again.
    selected_key: ?ParcelKey = null,
    selected_point_key: ?records.ParcelPoint = null,
    selected_len: usize = 0,
    selected_points_len: usize = 0,
    /// The handle being dragged, and the gesture its edits merge under.
    grab: ?Grab = null,
    gesture: u32 = 0,

    pub const Grab = struct {
        parcel: usize,
        point: ?usize,
        arrow: bool,
        /// The handle's place less the press's, map units.
        offset: [2]f32,
    };

    pub const Hit = struct { parcel: usize, point: ?usize, arrow: bool, handle: [2]f32 };

    pub const ParcelKey = struct {
        kind: records.ParcelKind,
        cx: f32,
        cy: f32,
        radius: f32,
        defence_dir: u16,

        fn of(parcel: records.Parcel) ParcelKey {
            return .{ .kind = parcel.kind, .cx = parcel.cx, .cy = parcel.cy, .radius = parcel.radius, .defence_dir = parcel.defence_dir };
        }
    };

    pub fn captureSelection(self: *AIGeneral, editor: *Editor) void {
        self.selected_key = null;
        self.selected_point_key = null;
        const index = self.selected_parcel orelse return;
        var side = editor.aiSide(editor.allocator, self.side) catch return;
        defer side.deinit(editor.allocator);
        if (index >= side.parcels.len) return;
        self.selected_len = side.parcels.len;
        self.selected_points_len = side.parcels[index].points.len;
        self.selected_key = ParcelKey.of(side.parcels[index]);
        if (self.selected_point) |point| {
            if (point < side.parcels[index].points.len) self.selected_point_key = side.parcels[index].points[point];
        }
    }

    /// The drag is dropped; the parcel is found again by its own fields and the
    /// point within it by its own (a replay that took the point away leaves the
    /// parcel selected alone).
    pub fn resolveSelection(self: *AIGeneral, editor: *Editor) void {
        self.grab = null;
        self.gesture = 0;
        const key = self.selected_key orelse {
            self.selected_parcel = null;
            self.selected_point = null;
            return;
        };
        const point_key = self.selected_point_key;
        self.selected_key = null;
        self.selected_point_key = null;
        var side = editor.aiSide(editor.allocator, self.side) catch {
            self.selected_parcel = null;
            self.selected_point = null;
            return;
        };
        defer side.deinit(editor.allocator);
        const keys = editor.allocator.alloc(ParcelKey, side.parcels.len) catch {
            self.selected_parcel = null;
            self.selected_point = null;
            return;
        };
        defer editor.allocator.free(keys);
        for (side.parcels, keys) |parcel, *slot| slot.* = ParcelKey.of(parcel);
        self.selected_parcel = tools.refind(ParcelKey, keys, self.selected_parcel, key, self.selected_len, tools.metaEql(ParcelKey));
        const parcel = self.selected_parcel orelse {
            self.selected_point = null;
            return;
        };
        const wanted = point_key orelse {
            self.selected_point = null;
            return;
        };
        self.selected_point = tools.refind(records.ParcelPoint, side.parcels[parcel].points, self.selected_point, wanted, self.selected_points_len, records.ParcelPoint.eql);
    }

    /// Forgets the selection and any drag, as a map change must; the side stays.
    pub fn reset(self: *AIGeneral) void {
        self.selected_parcel = null;
        self.selected_point = null;
        self.grab = null;
        self.gesture = 0;
    }

    /// The panel's list chose a parcel (or one of its points).
    pub fn select(self: *AIGeneral, parcel: ?usize, point: ?usize) void {
        self.selected_parcel = parcel;
        self.selected_point = if (parcel == null) null else point;
    }

    pub fn handle(self: *AIGeneral, editor: *Editor, event: Event) EditError!void {
        switch (event) {
            .press => |pointer| try self.press(editor, pointer),
            .drag => |pointer| try self.drag(editor, pointer),
            .release => {
                self.grab = null;
                self.gesture = 0;
            },
            .key => |key| switch (key) {
                .enter, .insert, .space => try self.switchType(editor),
                .delete => try self.deleteSelected(editor),
                .escape => self.reset(),
                else => {},
            },
            else => {},
        }
    }

    /// The handle under the click, in the MFC editor's order: the first parcel that has
    /// one, its points' arrows and centres in list order, then its own arrow and centre.
    pub fn hitHandle(side: records.AiSide, click: [2]f32) ?Hit {
        for (side.parcels, 0..) |parcel, parcel_index| {
            for (parcel.points, 0..) |point, point_index| {
                const arrow = pointArrowHandle(parcel, point);
                if (distance(arrow, click) < point_arrow_grab) return .{ .parcel = parcel_index, .point = point_index, .arrow = true, .handle = arrow };
                const centre = pointCentreHandle(parcel, point);
                if (distance(centre, click) < point_centre_grab) return .{ .parcel = parcel_index, .point = point_index, .arrow = false, .handle = centre };
            }
            const arrow = parcelArrowHandle(parcel);
            if (distance(arrow, click) < parcel_arrow_grab) return .{ .parcel = parcel_index, .point = null, .arrow = true, .handle = arrow };
            const centre = parcelCentreHandle(parcel);
            if (distance(centre, click) < parcel_centre_grab) return .{ .parcel = parcel_index, .point = null, .arrow = false, .handle = centre };
        }
        return null;
    }

    /// The first parcel the click is inside: nearer its centre than its radius (at the
    /// radius is outside), the MFC editor's test.
    pub fn parcelContaining(side: records.AiSide, click: [2]f32) ?usize {
        for (side.parcels, 0..) |parcel, index| {
            if (distance(.{ parcel.cx, parcel.cy }, click) < parcel.radius) return index;
        }
        return null;
    }

    fn press(self: *AIGeneral, editor: *Editor, pointer: Pointer) EditError!void {
        const allocator = editor.allocator;
        var side = try editor.aiSide(allocator, self.side);
        defer side.deinit(allocator);
        const click: [2]f32 = .{ pointer.map_x, pointer.map_y };
        self.grab = null;
        self.gesture = 0;
        if (hitHandle(side, click)) |hit| {
            self.selected_parcel = hit.parcel;
            self.selected_point = hit.point;
            self.grab = .{ .parcel = hit.parcel, .point = hit.point, .arrow = hit.arrow, .offset = .{ hit.handle[0] - click[0], hit.handle[1] - click[1] } };
            self.gesture = editor.beginGesture();
            return;
        }
        const gesture = editor.beginGesture();
        if (parcelContaining(side, click)) |parcel_index| {
            const parcel = side.parcels[parcel_index];
            const at = pointFromMap(click, .{ parcel.cx, parcel.cy }, parcel.defence_dir);
            try side.appendPoint(allocator, parcel_index, .{ .x = at[0], .y = at[1], .dir = 0 });
            try editor.editAiSide(self.side, side, gesture);
            self.selected_parcel = parcel_index;
            self.selected_point = side.parcels[parcel_index].points.len - 1;
            self.grab = .{ .parcel = parcel_index, .point = self.selected_point, .arrow = false, .offset = .{ 0, 0 } };
            self.gesture = gesture;
            return;
        }
        const index = try editor.addDefenceParcelIn(self.side, click[0], click[1], gesture);
        self.selected_parcel = index;
        self.selected_point = null;
        self.grab = .{ .parcel = index, .point = null, .arrow = false, .offset = .{ 0, 0 } };
        self.gesture = gesture;
    }

    fn drag(self: *AIGeneral, editor: *Editor, pointer: Pointer) EditError!void {
        const grab = self.grab orelse return;
        const allocator = editor.allocator;
        var side = try editor.aiSide(allocator, self.side);
        defer side.deinit(allocator);
        if (grab.parcel >= side.parcels.len) {
            self.grab = null;
            return;
        }
        const parcel = &side.parcelsMut()[grab.parcel];
        const now: [2]f32 = .{
            records.truncateToAi(pointer.map_x + grab.offset[0]),
            records.truncateToAi(pointer.map_y + grab.offset[1]),
        };
        const centre: [2]f32 = .{ parcel.cx, parcel.cy };
        if (grab.point) |point_index| {
            if (point_index >= parcel.points.len) {
                self.grab = null;
                return;
            }
            const point = &side.pointsMut(grab.parcel)[point_index];
            const relative = rotate(.{ now[0] - centre[0], now[1] - centre[1] }, -directionAngle(parcel.defence_dir));
            if (grab.arrow) {
                point.dir = directionFromArrow(.{ point.x, point.y }, relative);
            } else {
                point.x = relative[0];
                point.y = relative[1];
            }
        } else if (grab.arrow) {
            parcel.radius = radiusFromArrow(centre, now);
            parcel.defence_dir = directionFromArrow(centre, now);
        } else {
            parcel.cx = now[0];
            parcel.cy = now[1];
        }
        // A place the bridge will not take (a centre off the map) is skipped: the parcel
        // stays at the last one it took and the drag goes on; the status line says why.
        editor.editAiSide(self.side, side, self.gesture) catch |err| if (err != error.Refused) return err;
    }

    /// Enter, Insert, Space: the selected parcel is a defence parcel if it was anything
    /// but one, else a reinforce parcel. One undo step.
    pub fn switchType(self: *AIGeneral, editor: *Editor) EditError!void {
        const allocator = editor.allocator;
        var side = try editor.aiSide(allocator, self.side);
        defer side.deinit(allocator);
        const index = self.selected_parcel orelse {
            editor.note("select a parcel first: click its centre or its arrow");
            return;
        };
        if (index >= side.parcels.len) {
            self.reset();
            editor.note("that parcel is gone");
            return;
        }
        const parcel = &side.parcelsMut()[index];
        parcel.kind = if (parcel.kind == .defence) .reinforce else .defence;
        try editor.editAiSide(self.side, side, 0);
    }

    /// Delete: the selected point, else the selected parcel with its points. One undo
    /// step; the selection is let go of.
    pub fn deleteSelected(self: *AIGeneral, editor: *Editor) EditError!void {
        const allocator = editor.allocator;
        var side = try editor.aiSide(allocator, self.side);
        defer side.deinit(allocator);
        const index = self.selected_parcel orelse {
            editor.note("select a parcel or a point first");
            return;
        };
        if (index >= side.parcels.len) {
            self.reset();
            editor.note("that parcel is gone");
            return;
        }
        if (self.selected_point) |point_index| {
            // A point that is gone (an undo took it) is not the parcel: the
            // Delete meant for one point never takes the whole parcel (WR-B08).
            if (point_index >= side.parcels[index].points.len) {
                self.selected_point = null;
                editor.note("that point is gone");
                return;
            }
            try side.removePoint(allocator, index, point_index);
            self.selected_point = null;
            try editor.editAiSide(self.side, side, 0);
            return;
        }
        try side.removeParcel(allocator, index);
        self.selected_parcel = null;
        try editor.editAiSide(self.side, side, 0);
    }
};

const testing = std.testing;
const testFixture = editor_mod.testFixture;

fn opened(fake: *fake_mod.FakeBridge) !Editor {
    var editor = Editor.init(testing.allocator, fake.bridge());
    errdefer editor.deinit();
    try editor.open("fixture.bzm");
    return editor;
}

/// A pointer at a world point, with the map point the fake's ratio gives and a
/// screen point that is the world one (so a drag longer than a click is one).
fn pointerAt(fake: *const fake_mod.FakeBridge, x: f32, y: f32) Pointer {
    return .{ .world_x = x, .world_y = y, .map_x = x * fake.map_per_world, .map_y = y * fake.map_per_world, .screen_x = x, .screen_y = y };
}

fn drag(tool: *ScriptAreas, editor: *Editor, fake: *const fake_mod.FakeBridge, from: [2]f32, to: [2]f32) !void {
    try tool.handle(editor, .{ .press = pointerAt(fake, from[0], from[1]) });
    try tool.handle(editor, .{ .drag = pointerAt(fake, (from[0] + to[0]) / 2, (from[1] + to[1]) / 2) });
    try tool.handle(editor, .{ .drag = pointerAt(fake, to[0], to[1]) });
    try tool.handle(editor, .{ .release = pointerAt(fake, to[0], to[1]) });
}

test "a rectangle drag adds a named area in AI units as one undo step, and is selected" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{};
    tool.setName("m2_area");
    try drag(&tool, &editor, &fake, .{ 100, 200 }, .{ 300, 260 }); // 8x8 tiles of 40: inside the fixture map? tile_size decides
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    const area = fake.script_areas.items[0];
    try testing.expectEqualStrings("m2_area", area.nameSlice());
    try testing.expectEqual(records.AreaShape.rectangle, area.shape);
    try testing.expectEqual(@as(f32, 283), area.cx);
    try testing.expectEqual(@as(f32, 325), area.cy);
    try testing.expectEqual(@as(f32, 141), area.hx);
    try testing.expectEqual(@as(f32, 42), area.hy);
    try testing.expectEqual(@as(?usize, 0), tool.selected);
    try testing.expectEqual(@as(usize, 0), tool.name().len); // the name was taken
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 0), fake.script_areas.items.len);
}

test "a circle drag adds a circle, and a click or a zero-size drag is a note, not a command" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{ .shape = .circle };
    try drag(&tool, &editor, &fake, .{ 100, 100 }, .{ 130, 140 });
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    const ring = fake.script_areas.items[0];
    try testing.expectEqual(records.AreaShape.circle, ring.shape);
    try testing.expectEqual(@as(f32, 141), ring.cx);
    try testing.expectEqual(@as(f32, 71), ring.r); // distance 50 -> 70.7 + 0.3
    try testing.expectEqualStrings("area_1", ring.nameSlice()); // the first free default
    // A press and release in one place (a click on nothing): a note.
    const depth = editor.history.undo_stack.items.len;
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 200, 40) });
    try tool.handle(&editor, .{ .release = pointerAt(&fake, 200, 40) });
    try testing.expectEqual(@as(?usize, null), tool.selected);
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    // A drag far enough to leave a click but whose circle converts to radius 0 is refused by a note.
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 200, 40) });
    var tiny = pointerAt(&fake, 200, 40);
    tiny.screen_x = 260; // the screen says a drag, the world says nothing moved
    try tool.handle(&editor, .{ .release = tiny });
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "too small") != null);
    // The next default name skips the taken one.
    try drag(&tool, &editor, &fake, .{ 50, 50 }, .{ 80, 50 });
    try testing.expectEqualStrings("area_2", fake.script_areas.items[1].nameSlice());
}

test "a taken name is Refused with the history unchanged, and the name stays for another try" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var existing: records.ScriptArea = .{ .shape = .circle, .cx = 40, .cy = 40, .r = 8 };
    existing.setName("zone");
    try fake.addScriptAreaFixture(existing);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{};
    tool.setName("zone");
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 100, 100) });
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .release = pointerAt(&fake, 160, 160) }));
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expectEqualStrings("zone", tool.name());
    tool.setName("zone_b");
    try drag(&tool, &editor, &fake, .{ 100, 100 }, .{ 160, 160 });
    try testing.expectEqual(@as(usize, 2), fake.script_areas.items.len);
}

test "the centre handle moves the selected area and the edge handle resizes it, one undo step per drag" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var box: records.ScriptArea = .{ .shape = .rectangle, .cx = 100, .cy = 100, .hx = 30, .hy = 20 };
    box.setName("box");
    try fake.addScriptAreaFixture(box);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{ .selected = 0 };
    // The centre (100, 100) map units is world (70.7, 70.7). Grab it a little off-centre
    // and drag by 20 world units each way: the centre moves by the same, size kept.
    const grab_x = 100 / fake.map_per_world + 3;
    const grab_y = 100 / fake.map_per_world + 3;
    try tool.handle(&editor, .{ .press = pointerAt(&fake, grab_x, grab_y) });
    try testing.expectEqual(ScriptAreas.HandleDrag.move, tool.handle_drag);
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, grab_x + 10, grab_y + 10) });
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, grab_x + 20, grab_y + 20) });
    try tool.handle(&editor, .{ .release = pointerAt(&fake, grab_x + 20, grab_y + 20) });
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(f32, 128), fake.script_areas.items[0].cx); // (70.7 + 20) * sqrt 2 + 0.3, cut
    try testing.expectEqual(@as(f32, 30), fake.script_areas.items[0].hx);
    try testing.expectEqualStrings("box", fake.script_areas.items[0].nameSlice());
    // A press nowhere near a handle does not grab one; it starts a new area's drag and, released
    // as a click inside the area, selects it.
    try testing.expect(try editor.undo());
    try testing.expect(fake.script_areas.items[0].eql(box));
    // The resize: the (+, +) corner is (130, 120) map units.
    const corner = edgeHandle(box);
    try tool.handle(&editor, .{ .press = pointerAt(&fake, corner[0] / fake.map_per_world, corner[1] / fake.map_per_world) });
    try testing.expectEqual(ScriptAreas.HandleDrag.resize, tool.handle_drag);
    const centre_world = 100 / fake.map_per_world;
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, centre_world + 40, centre_world + 12) });
    try tool.handle(&editor, .{ .release = pointerAt(&fake, centre_world + 40, centre_world + 12) });
    try testing.expectEqual(@as(f32, 56), fake.script_areas.items[0].hx);
    try testing.expectEqual(@as(f32, 17), fake.script_areas.items[0].hy);
    try testing.expectEqual(@as(f32, 100), fake.script_areas.items[0].cx);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expect(fake.script_areas.items[0].eql(box));
    try testing.expect(try editor.redo());
    try testing.expectEqual(@as(f32, 56), fake.script_areas.items[0].hx);
}

test "a position the bridge refuses mid-drag is skipped and the drag goes on" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var ring: records.ScriptArea = .{ .shape = .circle, .cx = 100, .cy = 100, .r = 10 };
    ring.setName("ring");
    try fake.addScriptAreaFixture(ring);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{ .selected = 0 };
    const centre_world = 100 / fake.map_per_world;
    try tool.handle(&editor, .{ .press = pointerAt(&fake, centre_world, centre_world) });
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, centre_world + 10, centre_world) });
    const at_ten = fake.script_areas.items[0].cx;
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, -500, centre_world) }); // off the map: refused, skipped
    try testing.expectEqual(at_ten, fake.script_areas.items[0].cx);
    try tool.handle(&editor, .{ .drag = pointerAt(&fake, centre_world + 20, centre_world) });
    try testing.expect(fake.script_areas.items[0].cx > at_ten);
    try tool.handle(&editor, .{ .release = pointerAt(&fake, centre_world + 20, centre_world) });
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
}

test "an undo that shifts the areas list keeps the tool on the area it had selected (WR-B02)" {
    var fake = try editor_mod.testFixture(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var a: records.ScriptArea = .{ .shape = .circle, .cx = 40, .cy = 40, .r = 8 };
    a.setName("a");
    var b: records.ScriptArea = .{ .shape = .circle, .cx = 80, .cy = 80, .r = 8 };
    b.setName("b");
    _ = try editor.addScriptArea(a);
    _ = try editor.addScriptArea(b);
    try editor.deleteScriptArea(0);
    var tool: ScriptAreas = .{};
    tool.selected = 0; // "b", now first
    tool.captureSelection(&editor);
    try testing.expect(try editor.undo()); // "a" is back in front of it
    tool.resolveSelection(&editor);
    try testing.expectEqual(@as(?usize, 1), tool.selected);
    try testing.expectEqualStrings("b", tool.selectedArea(&editor).?.nameSlice());
}

test "a click inside an area selects it, the last one drawn when they overlap, and Delete removes the selected one" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var first: records.ScriptArea = .{ .shape = .circle, .cx = 100, .cy = 100, .r = 60 };
    first.setName("big");
    var second: records.ScriptArea = .{ .shape = .rectangle, .cx = 110, .cy = 100, .hx = 10, .hy = 10 };
    second.setName("small");
    try fake.addScriptAreaFixture(first);
    try fake.addScriptAreaFixture(second);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{};
    const click = pointerAt(&fake, 105 / fake.map_per_world, 100 / fake.map_per_world);
    try tool.handle(&editor, .{ .press = click });
    try tool.handle(&editor, .{ .release = click });
    try testing.expectEqual(@as(?usize, 1), tool.selected);
    const outer = pointerAt(&fake, 60 / fake.map_per_world, 100 / fake.map_per_world);
    try tool.handle(&editor, .{ .press = outer });
    try tool.handle(&editor, .{ .release = outer });
    try testing.expectEqual(@as(?usize, 0), tool.selected);
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try testing.expectEqualStrings("small", fake.script_areas.items[0].nameSlice());
    try testing.expectEqual(@as(?usize, null), tool.selected);
    try tool.handle(&editor, .{ .key = .delete }); // nothing selected: a note
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqualStrings("big", fake.script_areas.items[0].nameSlice());
    try testing.expect(try editor.redo());
    try testing.expectEqual(@as(usize, 1), fake.script_areas.items.len);
}

test "Escape abandons a drag and the tool ignores the events it has no use for" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: ScriptAreas = .{};
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 100, 100) });
    try tool.handle(&editor, .{ .key = .escape });
    try testing.expect(tool.start == null and !tool.dragging);
    try tool.handle(&editor, .{ .release = pointerAt(&fake, 200, 200) });
    try testing.expectEqual(@as(usize, 0), fake.script_areas.items.len);
    const pointer = pointerAt(&fake, 100, 100);
    for ([_]Event{ .{ .right_press = pointer }, .{ .right_drag = pointer }, .{ .right_release = pointer }, .{ .double_click = pointer }, .{ .key = .enter }, .{ .key = .insert }, .{ .key = .space }, .{ .key = .rotate_left } }) |event| {
        try tool.handle(&editor, event);
    }
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

fn targetPointer(fake: *const fake_mod.FakeBridge, x: f32, y: f32, object: ?i32) Pointer {
    var pointer = pointerAt(fake, x, y);
    pointer.object = object;
    return pointer;
}

fn commandFake(allocator: std.mem.Allocator) !fake_mod.FakeBridge {
    var fake = try testFixture(allocator);
    errdefer fake.deinit();
    fake.map_per_world = 1.4142135;
    try fake.addStartCommandFixtureFull(.{ .cmd_type = 0, .link_id = 1, .x = 10, .y = 10, .units = &.{1} });
    return fake;
}

test "Start Target: a click on the ground sets the point with the MFC cut and clears the target, one step, and the tool is done" {
    var fake = try commandFake(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: StartTarget = .{ .index = 0 };
    try tool.handle(&editor, .{ .press = targetPointer(&fake, 100, 60, null) });
    try testing.expect(!tool.done); // the click is the release, as in the MFC editor
    try tool.handle(&editor, .{ .release = targetPointer(&fake, 100, 60, null) });
    try testing.expect(tool.done);
    const command = fake.start_commands.items[0];
    try testing.expectEqual(@as(i32, 0), command.target);
    try testing.expectEqual(@as(f32, 141), command.x); // 100 * 1.4142 + 0.3, cut
    try testing.expectEqual(@as(f32, 85), command.y); // 84.85 + 0.3
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(i32, 1), fake.start_commands.items[0].target);
    try testing.expectEqual(@as(f32, 10), fake.start_commands.items[0].x);
}

test "Start Target: a click on an object makes it the target and keeps the point" {
    var fake = try commandFake(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const second = try editor.addObject("T34", 60, 60, 0, 0);
    var tool: StartTarget = .{ .index = 0 };
    try tool.handle(&editor, .{ .release = targetPointer(&fake, 100, 60, second) });
    const command = fake.start_commands.items[0];
    try testing.expectEqual(second, command.target);
    try testing.expectEqual(@as(f32, 10), command.x);
    try testing.expectEqual(@as(f32, 10), command.y);
    try testing.expect(tool.done);
}

test "Start Target: no command chosen, or one gone, is a note or a refusal and leaves the map alone" {
    var fake = try commandFake(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var none: StartTarget = .{};
    try none.handle(&editor, .{ .release = targetPointer(&fake, 100, 60, null) });
    try testing.expect(none.done);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "Set target") != null);
    var gone: StartTarget = .{ .index = 5 };
    try testing.expectError(error.Refused, gone.handle(&editor, .{ .release = targetPointer(&fake, 100, 60, null) }));
    try testing.expect(!gone.done);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(f32, 10), fake.start_commands.items[0].x);
}

test "Start Target: a point the bridge refuses leaves the tool active and the command as it was; Escape leaves it" {
    var fake = try commandFake(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: StartTarget = .{ .index = 0 };
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .release = targetPointer(&fake, -500, 60, null) }));
    try testing.expect(!tool.done);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "not on the map") != null);
    try tool.handle(&editor, .{ .key = .escape });
    try testing.expect(tool.done);
    tool.reset();
    try testing.expect(!tool.done and tool.index == null);
    // Other events are nobody's business.
    const pointer = pointerAt(&fake, 100, 60);
    for ([_]Event{ .{ .press = pointer }, .{ .drag = pointer }, .{ .right_press = pointer }, .{ .double_click = pointer }, .{ .key = .delete }, .{ .key = .enter } }) |event| {
        try tool.handle(&editor, event);
    }
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

fn reserveFake(allocator: std.mem.Allocator) !fake_mod.FakeBridge {
    var fake = try testFixture(allocator);
    errdefer fake.deinit();
    fake.map_per_world = 1.4142135;
    try fake.setRoleFixture("Gun", .towed, 1000);
    try fake.setRoleFixture("Truck", .truck, 2000);
    try fake.setRoleFixture("Weak_Truck", .truck, 500);
    try fake.setRoleFixture("Panzer", .self_propelled, 30000);
    return fake;
}

fn clickAt(tool: *ReservePositions, editor: *Editor, fake: *const fake_mod.FakeBridge, x: f32, y: f32, object: ?i32) !void {
    const pointer = targetPointer(fake, x, y, object);
    try tool.handle(editor, .{ .press = pointer });
    try tool.handle(editor, .{ .release = pointer });
}

test "Reserve Positions: a towed gun, its truck, the ground and Enter make one entry, one undo step" {
    var fake = try reserveFake(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const gun = try editor.addObject("Gun", 60, 100, 0, 0);
    const truck = try editor.addObject("Truck", 90, 100, 0, 0);
    const depth = editor.history.undo_stack.items.len;
    var tool: ReservePositions = .{};
    try clickAt(&tool, &editor, &fake, 60, 100, gun);
    try testing.expectEqual(@as(?i32, gun), tool.gun);
    try testing.expectEqual(bridge_mod.ReserveRole.towed, tool.gun_role);
    try clickAt(&tool, &editor, &fake, 90, 100, truck);
    try testing.expectEqual(@as(?i32, truck), tool.truck);
    try testing.expect(tool.pendingRecord() == null); // no place yet
    try clickAt(&tool, &editor, &fake, 100, 60, null);
    try testing.expect(tool.has_place);
    try testing.expectEqual(@as(f32, 141), tool.x); // 100 * 1.4142 + 0.3, cut
    try testing.expectEqual(@as(f32, 85), tool.y);
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expectEqual(@as(usize, 1), fake.reserve_positions.items.len);
    const position = fake.reserve_positions.items[0];
    try testing.expectEqual(gun, position.artillery);
    try testing.expectEqual(truck, position.truck);
    try testing.expectEqual(@as(f32, 141), position.x);
    try testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(?usize, 0), tool.selected);
    try testing.expect(tool.gun == null and !tool.has_place); // the pending choice was taken
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 0), fake.reserve_positions.items.len);
}

test "Reserve Positions: a towed gun without a truck is Refused with the history unchanged and the choice kept" {
    var fake = try reserveFake(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const gun = try editor.addObject("Gun", 60, 100, 0, 0);
    const weak = try editor.addObject("Weak_Truck", 90, 100, 0, 0);
    var tool: ReservePositions = .{};
    try clickAt(&tool, &editor, &fake, 60, 100, gun);
    try clickAt(&tool, &editor, &fake, 100, 60, null);
    const depth = editor.history.undo_stack.items.len;
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .key = .enter }));
    try testing.expect(std.mem.indexOf(u8, editor.status(), "towed gun needs a truck") != null);
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(?i32, gun), tool.gun);
    try testing.expect(tool.has_place);
    // Another try: a truck too weak for the gun is refused too, then the choice is cleared.
    try clickAt(&tool, &editor, &fake, 90, 100, weak);
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .key = .enter }));
    try testing.expect(std.mem.indexOf(u8, editor.status(), "cannot tow") != null);
    try testing.expectEqual(@as(usize, 0), fake.reserve_positions.items.len);
    try tool.handle(&editor, .{ .key = .escape });
    try testing.expect(tool.gun == null and tool.truck == null and !tool.has_place);
}

test "Reserve Positions: a self-propelled gun needs no truck, and a truck click after it is a note" {
    var fake = try reserveFake(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const panzer = try editor.addObject("Panzer", 120, 100, 0, 0);
    const truck = try editor.addObject("Truck", 90, 100, 0, 0);
    var tool: ReservePositions = .{};
    try clickAt(&tool, &editor, &fake, 120, 100, panzer);
    try clickAt(&tool, &editor, &fake, 90, 100, truck);
    try testing.expect(tool.truck == null);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "takes no truck") != null);
    try clickAt(&tool, &editor, &fake, 100, 60, null);
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expectEqual(@as(usize, 1), fake.reserve_positions.items.len);
    try testing.expectEqual(@as(i32, 0), fake.reserve_positions.items[0].truck);
}

test "Reserve Positions: a click out of order or on the wrong thing is a note, and a second gun starts the choice over" {
    var fake = try reserveFake(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const gun = try editor.addObject("Gun", 60, 100, 0, 0);
    const other = try editor.addObject("Gun", 70, 100, 0, 0);
    const truck = try editor.addObject("Truck", 90, 100, 0, 0);
    var tool: ReservePositions = .{};
    // The ground and a truck before any gun.
    try clickAt(&tool, &editor, &fake, 100, 60, null);
    try testing.expect(!tool.has_place);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "click a gun first") != null);
    try clickAt(&tool, &editor, &fake, 90, 100, truck);
    try testing.expect(tool.truck == null);
    // A tank (role none).
    try clickAt(&tool, &editor, &fake, 40, 40, 1);
    try testing.expect(tool.gun == null);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "not a gun or a truck") != null);
    // A second gun replaces the first and drops its truck, keeping the place.
    try clickAt(&tool, &editor, &fake, 60, 100, gun);
    try clickAt(&tool, &editor, &fake, 90, 100, truck);
    try clickAt(&tool, &editor, &fake, 100, 60, null);
    try clickAt(&tool, &editor, &fake, 70, 100, other);
    try testing.expectEqual(@as(?i32, other), tool.gun);
    try testing.expect(tool.truck == null and tool.has_place);
    // Enter with nothing to add is a note, never an error.
    var empty: ReservePositions = .{};
    try empty.handle(&editor, .{ .key = .enter });
    try testing.expect(std.mem.indexOf(u8, editor.status(), "pick a gun and a place") != null);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len - 3); // only the three objects added
}

test "Reserve Positions: Delete removes the selected position as one step and undo brings it back" {
    var fake = try reserveFake(testing.allocator);
    defer fake.deinit();
    var editor = try opened(&fake);
    defer editor.deinit();
    const panzer = try editor.addObject("Panzer", 120, 100, 0, 0);
    _ = try editor.addReservePosition(.{ .artillery = panzer, .x = 70, .y = 80 });
    var tool: ReservePositions = .{};
    try tool.handle(&editor, .{ .key = .delete }); // nothing selected: a note
    try testing.expect(std.mem.indexOf(u8, editor.status(), "select a reserve position") != null);
    try testing.expectEqual(@as(usize, 1), fake.reserve_positions.items.len);
    tool.selected = 0;
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 0), fake.reserve_positions.items.len);
    try testing.expectEqual(@as(?usize, null), tool.selected);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 1), fake.reserve_positions.items.len);
    // Other events are nobody's business.
    const pointer = pointerAt(&fake, 100, 60);
    for ([_]Event{ .{ .press = pointer }, .{ .drag = pointer }, .{ .right_press = pointer }, .{ .double_click = pointer }, .{ .key = .space } }) |event| {
        try tool.handle(&editor, event);
    }
}

test "AI General: a click outside every parcel makes a defence parcel of radius 256, in one undo step" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 100, 200) });
    try tool.handle(&editor, .{ .release = pointerAt(&fake, 100, 200) });
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items.len);
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[0].parcels.len);
    const parcel = fake.ai_sides.items[0].parcels[0];
    try testing.expectEqual(records.ParcelKind.defence, parcel.kind);
    try testing.expectEqual(@as(f32, 141), parcel.cx); // 141.42 + 0.3, cut
    try testing.expectEqual(@as(f32, 283), parcel.cy); // 282.84 + 0.3, cut
    try testing.expectEqual(@as(f32, 256), parcel.radius);
    try testing.expectEqual(@as(u16, 0), parcel.defence_dir);
    try testing.expectEqual(@as(usize, 0), parcel.points.len);
    try testing.expectEqual(@as(?usize, 0), tool.selected_parcel);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 0), fake.ai_sides.items.len);
    try testing.expect(try editor.redo());
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[0].parcels.len);
}

test "AI General: on a side beyond the count the count grows with empty sides below, and undo shrinks it back" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try fake.addAiSideFixture(.{});
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 3 };
    try tool.handle(&editor, .{ .press = pointerAt(&fake, 100, 100) });
    try testing.expectEqual(@as(usize, 4), fake.ai_sides.items.len);
    try testing.expectEqual(@as(usize, 0), fake.ai_sides.items[1].parcels.len);
    try testing.expectEqual(@as(usize, 0), fake.ai_sides.items[2].parcels.len);
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[3].parcels.len);
    try testing.expectEqual(@as(usize, 4), try editor.aiSideCount());
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items.len); // exactly the count it had
    try testing.expectEqual(@as(usize, 1), try editor.aiSideCount());
    try testing.expect(try editor.redo());
    try testing.expectEqual(@as(usize, 4), fake.ai_sides.items.len);
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[3].parcels.len);
}

test "AI General: a click off the map is Refused with nothing changed" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{};
    try testing.expectError(error.Refused, tool.handle(&editor, .{ .press = pointerAt(&fake, -50, 100) }));
    try testing.expectEqual(@as(usize, 0), fake.ai_sides.items.len);
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
}

/// A pointer at a MAP (AI) point, for the AI General tool, whose handles are in AI units.
fn pointerAtMap(fake: *const fake_mod.FakeBridge, x: f32, y: f32) Pointer {
    return .{ .world_x = x / fake.map_per_world, .world_y = y / fake.map_per_world, .map_x = x, .map_y = y, .screen_x = x, .screen_y = y };
}

/// A fake holding one parcel on side 0 before it opens, with the points given.
fn seedParcel(fake: *fake_mod.FakeBridge, parcel: records.Parcel) !void {
    var side: records.AiSide = .{};
    defer side.deinit(testing.allocator);
    try side.appendParcel(testing.allocator, parcel);
    try fake.addAiSideFixture(side);
}

fn expectNear(expected: f32, actual: f32, tolerance: f32) !void {
    if (@abs(expected - actual) > tolerance) {
        std.debug.print("expected {d}, found {d} (tolerance {d})\n", .{ expected, actual, tolerance });
        return error.TestExpectedApproxEqAbs;
    }
}

test "AI General: a click inside a parcel adds a reinforce point stored relative to the centre, turned by minus the direction" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try seedParcel(&fake, .{ .cx = 150, .cy = 150, .radius = 256, .defence_dir = 16384 });
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    const depth = editor.history.undo_stack.items.len;
    // The click at (250, 200) is (100, 50) from the centre; the parcel is turned a quarter
    // turn (16384 * 2 pi / 65535 = 1.5708203), so the store is the offset turned back:
    // x' = x cos a + y sin a = 49.9976, y' = -x sin a + y cos a = -100.0012.
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 250, 200) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 250, 200) });
    const points = fake.ai_sides.items[0].parcels[0].points;
    try testing.expectEqual(@as(usize, 1), points.len);
    try expectNear(49.9976, points[0].x, 0.01);
    try expectNear(-100.0012, points[0].y, 0.01);
    try testing.expectEqual(@as(u16, 0), points[0].dir);
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[0].parcels.len); // no new parcel
    try testing.expectEqual(@as(?usize, 0), tool.selected_parcel);
    try testing.expectEqual(@as(?usize, 0), tool.selected_point);
    try testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    // The point sits where it was clicked: stored and drawn are inverses.
    const at = pointCentreHandle(fake.ai_sides.items[0].parcels[0], points[0]);
    try expectNear(250, at[0], 0.01);
    try expectNear(200, at[1], 0.01);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 0), fake.ai_sides.items[0].parcels[0].points.len);
}

test "AI General: Delete with a stale point selected deletes nothing, never the whole parcel (WR-B08)" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try seedParcel(&fake, .{ .cx = 150, .cy = 150, .radius = 256 });
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 250, 200) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 250, 200) });
    try testing.expectEqual(@as(?usize, 0), tool.selected_point);
    // An undo the tool did not see takes the point away; the index is stale.
    try testing.expect(try editor.undo());
    const depth = editor.history.undo_stack.items.len;
    try tool.deleteSelected(&editor);
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[0].parcels.len);
    try testing.expectEqual(depth, editor.history.undo_stack.items.len);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "gone") != null);
    try testing.expectEqual(@as(?usize, null), tool.selected_point);
    try testing.expectEqual(@as(?usize, 0), tool.selected_parcel);
}

test "AI General: a point in a parcel of direction 0 is the click less the centre, and the click is cut first" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try seedParcel(&fake, .{ .cx = 150, .cy = 150, .radius = 256 });
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 250.4, 200.4) }); // cut as int(v + 0.3): 250, 200
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 250.4, 200.4) });
    const points = fake.ai_sides.items[0].parcels[0].points;
    try testing.expectEqual(@as(usize, 1), points.len);
    try testing.expectEqual(@as(f32, 100), points[0].x);
    try testing.expectEqual(@as(f32, 50), points[0].y);
}

test "AI General: the press that adds a point goes on dragging it, in the same undo step" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try seedParcel(&fake, .{ .cx = 150, .cy = 150, .radius = 256, .defence_dir = 16384 });
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    const depth = editor.history.undo_stack.items.len;
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 250, 200) });
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 260, 215) });
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 270, 230) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 270, 230) });
    // The new point followed the pointer to (270, 230): (120, 80) from the centre, turned back.
    const points = fake.ai_sides.items[0].parcels[0].points;
    try testing.expectEqual(@as(usize, 1), points.len);
    try expectNear(79.9971, points[0].x, 0.01);
    try expectNear(-120.0019, points[0].y, 0.01);
    try testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 0), fake.ai_sides.items[0].parcels[0].points.len);
}

test "AI General: dragging the centre handle moves the parcel and its points move with it" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var side: records.AiSide = .{};
    defer side.deinit(testing.allocator);
    try side.appendParcel(testing.allocator, .{ .cx = 150, .cy = 150, .radius = 256, .defence_dir = 8192 });
    try side.appendPoint(testing.allocator, 0, .{ .x = 40, .y = 10, .dir = 7 });
    try fake.addAiSideFixture(side);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    const depth = editor.history.undo_stack.items.len;
    const before = pointCentreHandle(fake.ai_sides.items[0].parcels[0], fake.ai_sides.items[0].parcels[0].points[0]);
    // The grab is 30 short of the centre handle (within a world cell, 64): the offset is kept.
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 130, 140) });
    try testing.expectEqual(@as(?usize, 0), tool.selected_parcel);
    try testing.expectEqual(@as(?usize, null), tool.selected_point);
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 160, 150) });
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 180, 170) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 180, 170) });
    const parcel = fake.ai_sides.items[0].parcels[0];
    try testing.expectEqual(@as(f32, 200), parcel.cx); // 180 + 20
    try testing.expectEqual(@as(f32, 180), parcel.cy); // 170 + 10
    try testing.expectEqual(@as(f32, 256), parcel.radius);
    try testing.expectEqual(@as(u16, 8192), parcel.defence_dir);
    // The point is stored relative, so it did not change, and on the map it moved by the same 50, 30.
    try testing.expectEqual(@as(f32, 40), parcel.points[0].x);
    try testing.expectEqual(@as(f32, 10), parcel.points[0].y);
    const after = pointCentreHandle(parcel, parcel.points[0]);
    try expectNear(before[0] + 50, after[0], 0.01);
    try expectNear(before[1] + 30, after[1], 0.01);
    try testing.expectEqual(depth + 1, editor.history.undo_stack.items.len); // the whole drag is one step
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(f32, 150), fake.ai_sides.items[0].parcels[0].cx);
}

test "AI General: a press on a handle and no movement is no edit" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try seedParcel(&fake, .{ .cx = 150, .cy = 150, .radius = 256 });
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 150, 150) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 150, 150) });
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expectEqual(@as(?usize, 0), tool.selected_parcel);
}

test "AI General: dragging the arrow handle sets the radius, never below 256, and the direction" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try seedParcel(&fake, .{ .cx = 150, .cy = 150, .radius = 256 });
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    const depth = editor.history.undo_stack.items.len;
    // The arrow handle of a parcel of direction 0 is 256 along +y from its centre.
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 150, 400) });
    try testing.expect(tool.grab != null and tool.grab.?.arrow);
    // To (450, 550): 300 x 400 from the centre, radius 500; polar angle atan(4/3) = 0.9273,
    // less a quarter turn is negative, plus 2 pi: 5.6397 -> 58823.
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 450, 544) }); // grabbed 6 short: offset +6
    var parcel = fake.ai_sides.items[0].parcels[0];
    try testing.expectEqual(@as(f32, 500), parcel.radius);
    try testing.expectEqual(@as(u16, 58823), parcel.defence_dir);
    try testing.expectEqual(@as(f32, 150), parcel.cx); // the centre did not move
    // Nearer than 256: the radius stays 256, the direction follows the arrow (straight up +y: 0).
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 150, 244) });
    parcel = fake.ai_sides.items[0].parcels[0];
    try testing.expectEqual(@as(f32, 256), parcel.radius);
    try testing.expectEqual(@as(u16, 0), parcel.defence_dir);
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 450, 544) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 450, 544) });
    try testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    parcel = fake.ai_sides.items[0].parcels[0];
    try testing.expectEqual(@as(f32, 256), parcel.radius);
    try testing.expectEqual(@as(u16, 0), parcel.defence_dir);
}

test "AI General: dragging a point handle moves the point, and its arrow sets its direction" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var side: records.AiSide = .{};
    defer side.deinit(testing.allocator);
    try side.appendParcel(testing.allocator, .{ .cx = 150, .cy = 150, .radius = 256 });
    try side.appendPoint(testing.allocator, 0, .{ .x = 100, .y = 50, .dir = 0 });
    try fake.addAiSideFixture(side);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    const depth = editor.history.undo_stack.items.len;
    // The point is at (250, 200); a press on it grabs the point, not the parcel.
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 250, 200) });
    try testing.expectEqual(@as(?usize, 0), tool.selected_point);
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 270, 230) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 270, 230) });
    var parcel = fake.ai_sides.items[0].parcels[0];
    try testing.expectEqual(@as(f32, 120), parcel.points[0].x);
    try testing.expectEqual(@as(f32, 80), parcel.points[0].y);
    try testing.expectEqual(@as(f32, 150), parcel.cx);
    try testing.expectEqual(depth + 1, editor.history.undo_stack.items.len);
    // Its arrow handle is 96 from it along +y: (270, 326). Dragged to (370, 280) the arrow
    // points (100, 50) from the point: polar 0.4636, less a quarter turn plus 2 pi -> 53987.
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 270, 326) });
    try testing.expect(tool.grab != null and tool.grab.?.arrow and tool.grab.?.point != null);
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 370, 280) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 370, 280) });
    parcel = fake.ai_sides.items[0].parcels[0];
    try testing.expectEqual(@as(u16, 53987), parcel.points[0].dir);
    try testing.expectEqual(@as(f32, 120), parcel.points[0].x); // the place did not change
    try testing.expectEqual(depth + 2, editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(u16, 0), fake.ai_sides.items[0].parcels[0].points[0].dir);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(f32, 100), fake.ai_sides.items[0].parcels[0].points[0].x);
}

test "AI General: Enter, Insert and Space switch the type, one undo step each" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try seedParcel(&fake, .{ .cx = 150, .cy = 150, .radius = 256 });
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 150, 150) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 150, 150) });
    const depth = editor.history.undo_stack.items.len;
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expectEqual(records.ParcelKind.reinforce, fake.ai_sides.items[0].parcels[0].kind);
    try tool.handle(&editor, .{ .key = .insert });
    try testing.expectEqual(records.ParcelKind.defence, fake.ai_sides.items[0].parcels[0].kind);
    try tool.handle(&editor, .{ .key = .space });
    try testing.expectEqual(records.ParcelKind.reinforce, fake.ai_sides.items[0].parcels[0].kind);
    try testing.expectEqual(depth + 3, editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(records.ParcelKind.defence, fake.ai_sides.items[0].parcels[0].kind);
    // A file's own odd type becomes a defence parcel.
    fake.ai_sides.items[0].parcelsMut()[0].kind = @enumFromInt(0);
    try tool.handle(&editor, .{ .key = .enter });
    try testing.expectEqual(records.ParcelKind.defence, fake.ai_sides.items[0].parcels[0].kind);
}

test "AI General: Delete removes the selected point, else the selected parcel, one undo step each" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    var side: records.AiSide = .{};
    defer side.deinit(testing.allocator);
    try side.appendParcel(testing.allocator, .{ .cx = 150, .cy = 150, .radius = 256 });
    try side.appendPoint(testing.allocator, 0, .{ .x = 100, .y = 50, .dir = 3 });
    try side.appendPoint(testing.allocator, 0, .{ .x = -100, .y = 60, .dir = 4 });
    try fake.addAiSideFixture(side);
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    // Nothing selected: a note, no command.
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 0), editor.history.undo_stack.items.len);
    try testing.expect(std.mem.indexOf(u8, editor.status(), "select") != null);
    // The first point selected (a press on it), Delete takes it alone.
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 250, 200) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 250, 200) });
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[0].parcels[0].points.len);
    try testing.expectEqual(@as(f32, -100), fake.ai_sides.items[0].parcels[0].points[0].x);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
    // The parcel selected (its centre), Delete takes it and its points.
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 150, 150) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 150, 150) });
    try tool.handle(&editor, .{ .key = .delete });
    try testing.expectEqual(@as(usize, 0), fake.ai_sides.items[0].parcels.len);
    try testing.expectEqual(@as(?usize, null), tool.selected_parcel);
    try testing.expectEqual(@as(usize, 2), editor.history.undo_stack.items.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[0].parcels.len);
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[0].parcels[0].points.len);
    try testing.expect(try editor.undo());
    try testing.expectEqual(@as(usize, 2), fake.ai_sides.items[0].parcels[0].points.len);
    // The side is still there: deleting a parcel never drops the side.
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items.len);
}

test "AI General: a press at the radius is outside the parcel, one inside it adds a point" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try seedParcel(&fake, .{ .cx = 50, .cy = 50, .radius = 256 });
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 305, 50) }); // 255 away: inside
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 305, 50) });
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[0].parcels.len);
    try testing.expectEqual(@as(usize, 1), fake.ai_sides.items[0].parcels[0].points.len);
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 306, 200) }); // hypot(256, 150) = 296.7: outside
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 306, 200) });
    try testing.expectEqual(@as(usize, 2), fake.ai_sides.items[0].parcels.len);
    try testing.expectEqual(@as(f32, 256), fake.ai_sides.items[0].parcels[1].radius);
    // Exactly at the radius is outside too (the test is strictly nearer than the radius).
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 50, 306) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 50, 306) });
    // (50, 306) is 256 from the first parcel's centre and on its arrow handle (direction 0): a grab, no new parcel.
    try testing.expectEqual(@as(usize, 2), fake.ai_sides.items[0].parcels.len);
    try testing.expect(tool.grab == null);
    try testing.expectEqual(@as(?usize, 0), tool.selected_parcel);
}

test "AI General: a centre dragged off the map is skipped and the drag goes on" {
    var fake = try testFixture(testing.allocator);
    defer fake.deinit();
    fake.map_per_world = 1.4142135;
    try seedParcel(&fake, .{ .cx = 150, .cy = 150, .radius = 256 });
    var editor = try opened(&fake);
    defer editor.deinit();
    var tool: AIGeneral = .{ .side = 0 };
    try tool.handle(&editor, .{ .press = pointerAtMap(&fake, 150, 150) });
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, -400, 150) }); // off the map: the bridge refuses, the tool goes on
    try testing.expectEqual(@as(f32, 150), fake.ai_sides.items[0].parcels[0].cx);
    try tool.handle(&editor, .{ .drag = pointerAtMap(&fake, 200, 160) });
    try tool.handle(&editor, .{ .release = pointerAtMap(&fake, 200, 160) });
    try testing.expectEqual(@as(f32, 200), fake.ai_sides.items[0].parcels[0].cx);
    try testing.expectEqual(@as(usize, 1), editor.history.undo_stack.items.len);
}

test "AI General: the formulas give the MFC editor's literal values" {
    // The same table as map_file_test.cpp's TestM2ParcelFormulas.
    const arrows = [_]struct { dx: f32, dy: f32, dir: u16 }{
        .{ .dx = 0, .dy = 100, .dir = 0 },
        .{ .dx = 100, .dy = 0, .dir = 49151 },
        .{ .dx = 0, .dy = -100, .dir = 32767 },
        .{ .dx = -100, .dy = 0, .dir = 16383 },
        .{ .dx = 100, .dy = 100, .dir = 57343 },
        .{ .dx = -100, .dy = 100, .dir = 8191 },
        .{ .dx = -100, .dy = -100, .dir = 24575 },
        .{ .dx = 100, .dy = -100, .dir = 40959 },
        .{ .dx = 0.01, .dy = 1000, .dir = 65534 },
        .{ .dx = -0.01, .dy = 1000, .dir = 0 },
    };
    for (arrows) |arrow| {
        try testing.expectEqual(arrow.dir, directionFromArrow(.{ 1000, 1000 }, .{ 1000 + arrow.dx, 1000 + arrow.dy }));
    }
    const none = directionFromArrow(.{ 1000, 1000 }, .{ 1000, 1000 });
    try testing.expect(none >= 38720 and none <= 38722);
    try testing.expectEqual(@as(f32, 500), radiusFromArrow(.{ 1000, 1000 }, .{ 1300, 1400 }));
    try testing.expectEqual(@as(f32, 256), radiusFromArrow(.{ 1000, 1000 }, .{ 1100, 1000 }));
    try testing.expectEqual(@as(f32, 256), radiusFromArrow(.{ 1000, 1000 }, .{ 1000, 1000 }));
    try testing.expectEqual(@as(f32, 256), radiusFromArrow(.{ 1000, 1000 }, .{ 1256, 1000 }));
    try expectNear(257, radiusFromArrow(.{ 1000, 1000 }, .{ 1257, 1000 }), 0.001);
    // A point in a parcel: the store of a click at AI (400, 199) in a parcel at (300, 150).
    const straight = pointFromMap(.{ 400.0, 199.0 }, .{ 300, 150 }, 0);
    try testing.expectEqual(@as(f32, 100), straight[0]);
    try testing.expectEqual(@as(f32, 49), straight[1]);
    const quarter = pointFromMap(.{ 400.0, 199.0 }, .{ 300, 150 }, 16384);
    try expectNear(48.9976, quarter[0], 0.01);
    try expectNear(-100.0012, quarter[1], 0.01);
    const eighth = pointFromMap(.{ 400.0, 199.0 }, .{ 300, 150 }, 8192);
    try expectNear(105.3585, eighth[0], 0.01);
    try expectNear(-36.0637, eighth[1], 0.01);
    // Drawn: the stored point turned by the angle, plus the centre (map units here; the
    // C++ literals are the same times sqrt 2 / 2 in world units).
    const drawn = pointToMap(.{ 100, 49 }, .{ 300, 150 }, 16384);
    try expectNear(177.4821 * 1.41421356, drawn[0], 0.02);
    try expectNear(176.7759 * 1.41421356, drawn[1], 0.02);
    // The click is cut before the centre is taken away.
    const cut = pointFromMap(.{ 400.4, 199.4 }, .{ 300, 150 }, 0);
    try testing.expectEqual(@as(f32, 100), cut[0]);
    try testing.expectEqual(@as(f32, 49), cut[1]);
}
