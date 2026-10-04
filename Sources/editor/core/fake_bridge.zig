//! An in-memory map behind the Bridge interface, for the core tier. It keeps
//! the real bridge's rules the core can see: the refusals (a bridge span, an
//! unknown object, a shared link ID, a cell or a placement off the map, a
//! value out of range), how link IDs are handed out, the order paints undo
//! and redo in, what a reopen forgets, and the object list's shape. It is
//! knowingly kinder or simpler in these ways, which a core test must not
//! lean on:
//!  - there is no file: a reopen forgets deleted objects and paint history,
//!    as the real one does, but keeps objects, tiles, diplomacy and map
//!    fields as they are, as if every edit had been saved;
//!  - there is no terrain function: a paint sets exactly the cells it names;
//!  - there is no engine: a known object is placed, moved, turned and
//!    re-owned anywhere on the map, where the real engine refuses some (a
//!    tree takes no direction and no owner);
//!  - any name is accepted on add, for an object or a sound; the real bridge
//!    refuses one the object database does not know (a sound additionally
//!    checked as game type 100);
//!  - edits are accepted before any map is open and after a failed open;
//!    the real bridge refuses them with "no map is open";
//!  - screen and world coordinates are the same thing;
//!  - references are modelled only as far as a delete reads them: a bridge
//!    span (`bridge_spans`, link ID to bridge index) and a trench piece
//!    (`trench_pieces`, link ID to entrenchment index) refuse, and start
//!    commands (`start_commands`, at most `max_command_units` units each,
//!    every field of records.StartCommand, converted at the record calls) and
//!    reserve positions (`reserve_positions`) are edited by the cascade in the
//!    same order as the real bridge, undone in reverse. A start command's
//!    units are any known object (`markNonUnitFixture` makes one a building);
//!    the action types are a built-in list unless a test replaces it or turns
//!    it off (`no_action_list`); a reserve position's roles come from a table a
//!    test fills (`setRoleFixture`: name, role, and the weight or towing force
//!    the MFC towing check reads; a name it does not list is role 0) and
//!    `markSquadFixture` makes an object a squad; Reinforcement groups are
//!    held (`groups`, 04-09) for the Group Manager's records only; a delete
//!    never edits them (they name script IDs); the AI general's sides are held
//!    whole (`ai_sides`, 04-12: the list's length is the side count, a put
//!    resizes it as the real one does) and the fake does not exempt the parcels
//!    the file held when it was opened, only the ones the side holds now. The fake words the delete's summary one change at a time where the
//!    real one groups ("start commands 2 and 5");
//!  - players and the unit creation (05-05, D-30): a player add or delete keeps the
//!    table (17 entries at most, the neutral last), the unit-creation vector, the
//!    camera anchors and the owners of the objects as ONE logged edit; a unit-creation
//!    put is checked against fixture lists (`uc_parties`, `uc_aircraft`, `uc_squads`)
//!    and a name the entry or the opened file held is always taken back. The fake
//!    renames no flag when an owner moves (the real bridge does), and an unknown-type
//!    object can be deleted (Check Map's explicit fix) as in the real one;
//!  - the ground is flat: `groundHeight` answers 0 on the map, where the real
//!    one reads the terrain's altitudes;
//!  - roads and rivers are not sampled: a record's key points are its control
//!    points (after the real builder's rule that drops a point within 2 units
//!    of the one before), each with the drawn width and opacity and a normal
//!    across its neighbours; "too short" is fewer than two such points (the
//!    real one also refuses a line shorter than one 30-unit sampling step);
//!    a record holds at most `max_vso_points` points; there is no engine to
//!    redraw and no AI to lock river tiles in;
//!  - `saveMap` never touches a real file on its own: the real
//!    `BkEditorSaveMap`'s read-back verification (session.cpp,
//!    `SaveSessionMap`) lives entirely inside the engine, invisible to this
//!    fake, which only writes to `files` (a shared `FakeFiles`) when a test
//!    sets it, so `Editor.save`'s temp+backup+swap has something to see land.
const std = @import("std");
const bridge_mod = @import("bridge.zig");
const kit = @import("editor_kit");
const files_mod = kit.files;
const records = @import("records.zig");
const script_file_mod = kit.script_file;
const layers_mod = @import("layers.zig");
const rmg_mod = @import("rmg.zig");
const Status = bridge_mod.Status;
const MapInfo = bridge_mod.MapInfo;
const ObjectRecord = bridge_mod.ObjectRecord;
const SoundRecord = bridge_mod.SoundRecord;
const PaintCell = bridge_mod.PaintCell;
const AltitudeRegion = bridge_mod.AltitudeRegion;
const HeightsStrokeParams = bridge_mod.HeightsStrokeParams;
const NewMapParams = bridge_mod.NewMapParams;
const Bridge = bridge_mod.Bridge;
const VsoKind = bridge_mod.VsoKind;
const VsoDescriptor = bridge_mod.VsoDescriptor;
const VsoKeyPoint = bridge_mod.VsoKeyPoint;
const VsoView = bridge_mod.VsoView;
const BridgeDescriptor = bridge_mod.BridgeDescriptor;
const PlannedPiece = bridge_mod.PlannedPiece;
const BridgeInfo = bridge_mod.BridgeInfo;
const FenceDescriptor = bridge_mod.FenceDescriptor;
const EntrenchmentInfo = bridge_mod.EntrenchmentInfo;

/// World units per tile, standing in for the engine's own conversion.
pub const tile_size: f32 = 32.0;
/// CMapInfo::MOST_COMMON_TILES (MapInfo_Consts.cpp): the terrain type a new
/// map of the season is filled with (M3, D-23). The fake has no variant
/// draw (GetMapsIndex's rand), so the type's index is the tile its cells
/// hold.
pub const most_common_tiles = [4]u8{ 3, 9, 12, 2 };
/// World units per AI tile (half a tile), standing in for the engine's
/// GetAITileIndex.
pub const ai_tile_size: f32 = tile_size / 2.0;
/// How far from an object's centre a point still picks it.
pub const pick_radius: f32 = 16.0;

/// The fake's own heights-validity ceiling (M3, D-18): a stroke whose result
/// leaves +-this many WORLD z units is "invalid height" and refused unless
/// Ctrl is held - the engine's own rule is a slope predicate
/// (CVertexAltitudeInfo::IsValidHeight) the fake's flat ground cannot model,
/// so it uses a bound a test can compute. 16 tiles of height.
pub const fake_height_limit: f32 = tile_size * 16.0;

pub const CallKind = enum { open, save, add, place, delete, restore, diplomacy, map_type, attacking_side, paint, undo_paint, redo_paint, sound_add, sound_edit, sound_delete, record_put, script_id, vso_edit, undo_edit, redo_edit, bridge_edit, altitudes_edit, new_map, heights_stroke, heights_generate, heights_zero, update_map, fill_map, terrain_modes, layer_show, fire_range };
pub const Call = struct { kind: CallKind, id: i32 = 0 };

/// The most units a fake start command holds (the real one holds as many as the
/// ABI's 4096): a fixed capacity keeps the command a plain value, so a test can
/// snapshot the list by copy.
pub const max_command_units = 16;

/// One start command: every field of records.StartCommand, the units in a fixed
/// array (`target` is the record's `link_id`, 0 meaning none).
pub const FakeStartCommand = struct {
    units: [max_command_units]i32 = @splat(0),
    unit_count: usize = 0,
    target: i32 = 0,
    cmd_type: i32 = records.action_stop,
    x: f32 = 0,
    y: f32 = 0,
    from_explosion: bool = false,
    number: f32 = 0,

    fn holds(self: *const FakeStartCommand, link_id: i32) bool {
        for (self.units[0..self.unit_count]) |unit| {
            if (unit == link_id) return true;
        }
        return false;
    }

    fn remove(self: *FakeStartCommand, link_id: i32) void {
        var kept: usize = 0;
        for (self.units[0..self.unit_count]) |unit| {
            if (unit == link_id) continue;
            self.units[kept] = unit;
            kept += 1;
        }
        self.unit_count = kept;
    }

    pub fn unitSlice(self: *const FakeStartCommand) []const i32 {
        return self.units[0..self.unit_count];
    }

    /// The record, its units allocated with `allocator` (the value owns them).
    pub fn toRecord(self: *const FakeStartCommand, allocator: std.mem.Allocator) std.mem.Allocator.Error!records.StartCommand {
        return .{
            .cmd_type = self.cmd_type,
            .link_id = self.target,
            .x = self.x,
            .y = self.y,
            .from_explosion = self.from_explosion,
            .number = self.number,
            .units = try allocator.dupe(i32, self.unitSlice()),
        };
    }

    /// Null when the record holds more units than the fake does.
    pub fn fromRecord(value: records.StartCommand) ?FakeStartCommand {
        if (value.units.len > max_command_units) return null;
        var command: FakeStartCommand = .{
            .target = value.link_id,
            .cmd_type = value.cmd_type,
            .x = value.x,
            .y = value.y,
            .from_explosion = value.from_explosion,
            .number = value.number,
            .unit_count = value.units.len,
        };
        @memcpy(command.units[0..value.units.len], value.units);
        return command;
    }

    pub fn eql(a: *const FakeStartCommand, b: *const FakeStartCommand) bool {
        return a.cmd_type == b.cmd_type and a.target == b.target and a.x == b.x and a.y == b.y and a.from_explosion == b.from_explosion and
            a.number == b.number and std.mem.eql(i32, a.unitSlice(), b.unitSlice());
    }
};
/// A reserve position: its artillery's and its truck's link IDs (0 meaning none)
/// and its place - the record itself.
pub const FakeReservePosition = records.ReservePosition;

/// What the fake's object database says of one object type for a reserve position
/// (04-11): the role BkEditorReserveRole answers and the number the MFC towing check
/// reads (a gun's weight, a truck's towing force).
pub const FakeRole = struct {
    name: [bridge_mod.name_capacity]u8 = [_]u8{0} ** bridge_mod.name_capacity,
    role: bridge_mod.ReserveRole = .none,
    number: f32 = 0,

    fn nameSlice(self: *const FakeRole) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }
};
/// What a delete did to one start command: its position when it was changed
/// (an earlier erase had already shifted the later ones), the record as it
/// was, whether the command went and whether its target was cleared. Undone
/// in reverse.
/// The composer fixtures (05-09): a container, a graph and a patch map the
/// fake's storages hold. A shipped file is read-only, as the real bridge keeps
/// it (a write to its name is refused with the Save-As message).
const RmgContainerFile = struct { name: []u8, shipped: bool, value: rmg_mod.Container };
const RmgGraphFile = struct { name: []u8, shipped: bool, value: rmg_mod.Graph };
const RmgFieldSetFile = struct { name: []u8, shipped: bool, value: rmg_mod.FieldSet };
const RmgTemplateFile = struct { name: []u8, shipped: bool, value: rmg_mod.Template };
const RmgPatchFile = struct { name: []u8, size_x: i32, size_y: i32, season: i32, folder: []u8, ids: []i32, areas: [][]u8 };

const StartChange = struct { position: usize, before: FakeStartCommand, unit_removed: bool, erased: bool, target_cleared: bool };
const ReserveChange = struct { position: usize, before: FakeReservePosition };
/// One soldier of a squad (M3, D-25): the member's link ID and the squad's.
const FakeSquadMember = struct { member: i32, squad: i32 };
/// One link rule the fake's canLink answers from (M3, D-27): source name,
/// target name, the type it links as.
const FakeLinkRule = struct { source: [64]u8 = [_]u8{0} ** 64, target: [64]u8 = [_]u8{0} ** 64, link_type: i32 = 0 };
/// One flag swap (M3, D-26): re-owning a flag to `player` renames it.
const FakeFlagSwap = struct { player: i32, to_name: [64]u8 = [_]u8{0} ** 64 };
const Tombstone = struct {
    record: ObjectRecord,
    index: usize,
    changes: std.ArrayListUnmanaged(StartChange) = .empty,
    reserves: std.ArrayListUnmanaged(ReserveChange) = .empty,
    /// The host's passengers (M3, D-27), each with its own tombstone, in the
    /// order they were deleted: the real session deletes them with the host and
    /// restores them, last deleted first, after it.
    passengers: std.ArrayListUnmanaged(Tombstone) = .empty,
};
/// The most control points a fake road or river holds: as many as the Roads &
/// Rivers tool draws (`tools_vso.max_pending`), so the tools' long-line paths
/// run against the fake too (WR-B06; the real ABI takes 1024).
pub const max_vso_points = 256;
/// The season folder the fake's saved road and river names carry, as the
/// real ones carry the map's (`readVso`'s full name).
pub const fake_season_folder = "fake_season\\";
/// CreateVSO's UniquePolygon distance (RMGC_MINIMAL_VIS_POINT_DISTANCE): a
/// point this close to the one before it is dropped.
pub const vso_min_point_distance: f32 = 2.0;

/// A road or river as the fake keeps it: its type's bare name, the bridge's
/// saved ID, and one key point per control point (see the header).
pub const FakeVso = struct {
    desc: VsoDescriptor = .{},
    saved_id: i32 = 0,
    controls: [max_vso_points]records.Vec3 = @splat(.{}),
    keys: [max_vso_points]VsoKeyPoint = @splat(.{}),
    count: usize = 0,

    pub fn controlSlice(self: *const FakeVso) []const records.Vec3 {
        return self.controls[0..self.count];
    }

    pub fn keySlice(self: *const FakeVso) []const VsoKeyPoint {
        return self.keys[0..self.count];
    }

    /// Every key point's normal, from the control points around it: a unit
    /// vector across the line, as the real sampler's normals are.
    fn renormal(self: *FakeVso) void {
        for (0..self.count) |index| {
            const previous = self.controls[if (index == 0) 0 else index - 1];
            const next = self.controls[@min(index + 1, self.count - 1)];
            const dx = next.x - previous.x;
            const dy = next.y - previous.y;
            const length = @sqrt(dx * dx + dy * dy);
            const key = &self.keys[index];
            key.x = self.controls[index].x;
            key.y = self.controls[index].y;
            key.z = 0;
            key.nx = if (length > 0) -dy / length else 0;
            key.ny = if (length > 0) dx / length else 1;
            key.nz = 0;
        }
    }
};
/// One logged road or river edit: the records before and after (either
/// absent for an add or a delete) at a list position.
const FakeVsoEdit = struct { kind: VsoKind, index: usize, before: ?FakeVso, after: ?FakeVso };

/// The most spans a fake bridge (and pieces a fence run or a trench) holds:
/// the real bridge's own most spans (nMaxBridgeSpans) and the Entrenchment
/// tool's most points (WR-B06).
pub const max_bridge_spans = 256;
/// A bridge type the fake offers: what BkEditorBridgeDescriptors lists, and
/// its line span's length in WORLD units (the real one reads the stats).
pub const FakeBridgeType = struct { descriptor: BridgeDescriptor, span_length: f32 };
/// One bridges entry: the link IDs of its spans, in order, and whether it is
/// built during play (the real one keeps that as the spans' negative HP).
pub const FakeBridgeEntry = struct {
    links: [max_bridge_spans]i32 = @splat(0),
    count: usize = 0,
    built: bool = false,

    pub fn linkSlice(self: *const FakeBridgeEntry) []const i32 {
        return self.links[0..self.count];
    }

    fn holds(self: *const FakeBridgeEntry, link_id: i32) bool {
        return std.mem.indexOfScalar(i32, self.linkSlice(), link_id) != null;
    }
};
/// A bridge as the fake's edit log keeps it: its entry and where it sits,
/// and its span objects with their places in the object list, in the order
/// they go back (ascending place), as the real SBridgeGroup does.
const FakeBridgeGroup = struct {
    /// A fence run (04-07) has no bridges entry: only its objects go out and
    /// come back, and `entry` just holds their link IDs.
    entryless: bool = false,
    /// An entrenchment (04-08): the entry is in `trench_entries`, not
    /// `bridge_entries`; otherwise the same group.
    trench: bool = false,
    entry_index: usize = 0,
    entry: FakeBridgeEntry = .{},
    spans: [max_bridge_spans]ObjectRecord = @splat(.{}),
    places: [max_bridge_spans]usize = @splat(0),
    count: usize = 0,
};
/// One logged bridge edit: the group it takes out and the group it puts in
/// (both for a rotate, at the same entry index).
const FakeBridgeEdit = struct { before: ?FakeBridgeGroup, after: ?FakeBridgeGroup };
/// The fake's edit log holds road and river edits and bridge edits alike,
/// one token space, as the real session's does.
/// A simulated fields apply: the object its fill added, when the test asked
/// for one (`field_adds_object`) - undo takes it out, redo puts it back.
const FakeFieldsEdit = struct { added: ?ObjectRecord = null };
const FakeEdit = union(enum) { vso: FakeVsoEdit, bridge: FakeBridgeEdit, build: FakeBuildEdit, altitudes: FakeAltitudeEdit, fields: FakeFieldsEdit, move_objects: FakeMoveObjectsEdit, object_fields: FakeObjectFieldsEdit, players: FakePlayersEdit };
/// A toggle of built during play: the entry and its flag before and after.
const FakeBuildEdit = struct { index: usize, before: bool, after: bool };
/// One altitude region edit (M3, D-19): the region and its heights before
/// and after, both owned here. Undo writes `before` back, redo `after` - the
/// fake keeps heights only, its ground being flat; the shades the real
/// bridge recomputes are nothing the core can see.
const FakeAltitudeEdit = struct { x0: i32, y0: i32, x1: i32, y1: i32, before: []f32, after: []f32 };

/// One member of the batch move's edit (M3, D-25): its position before and
/// after, map units.
const FakeMovedMember = struct { link_id: i32, before_x: f32, before_y: f32, after_x: f32, after_y: f32 };

/// The batch move's edit (M3, D-25): every member's position before and
/// after, put back raw by undo and forward again by redo - the same record
/// the real bridge's SMoveObjectsEdit keeps.
const FakeMoveObjectsEdit = struct { moves: std.ArrayListUnmanaged(FakeMovedMember) = .empty };

/// One object's whole record before and after a fields edit (M3, D-26/D-27):
/// the same record the real bridge's SObjectFieldsEdit keeps; a flag's swap
/// of name undoes exactly through it.
const FakeObjectFieldsEdit = struct { link_id: i32, before: ObjectRecord, after: ObjectRecord };

/// The table, the unit-creation vector and the camera anchors of a player edit
/// (05-05, D-30), held whole like the real SPlayersEdit's SPlayersState.
pub const max_player_entries = 17;
const FakePlayersState = struct {
    table: [max_player_entries]i32 = @splat(0),
    table_len: usize = 0,
    units: [records.max_uc_slots]records.UnitCreation = @splat(.{}),
    units_len: usize = 0,
    anchors: records.CameraAnchors = .{},
};
/// One object a player edit re-owned, with the name it had before and after
/// (the fake keeps names: the real bridge renames a flag, the fake does not).
const FakeOwnerChange = struct { link_id: i32, before_player: i32, after_player: i32 };
/// A player add or delete as the fake's edit log keeps it.
const FakePlayersEdit = struct { before: FakePlayersState, after: FakePlayersState, owners: std.ArrayListUnmanaged(FakeOwnerChange) = .empty };

/// A mutable empty slice to start from; Allocator.free ignores a zero length.
var no_tiles: [0]u8 = .{};
var no_altitudes: [0]f32 = .{};
const PaintRecord = struct { cells: []PaintCell, before: []u8 };

pub const FakeBridge = struct {
    allocator: std.mem.Allocator,
    info: MapInfo,
    objects_list: std.ArrayListUnmanaged(ObjectRecord) = .empty,
    /// The map's own sound list (CMapInfo::sounds.sounds - see bridge.h's
    /// own comment on BkEditorSounds). Kept across a fake reopen, the same
    /// simplification `objects_list`/`tiles`/`diplomacy_table` make: there is
    /// no file, so nothing here is actually lost.
    sounds_list: std.ArrayListUnmanaged(SoundRecord) = .empty,
    /// The map's camera anchors (BkEditorCameraAnchors): world units, kept
    /// across a fake reopen like the sounds. `setCameraAnchorsFixture` seeds it.
    camera_anchors: records.CameraAnchors = .{},
    /// The map's unit-creation vector (05-05, D-30): one entry per slot, the
    /// vector's length being the slot count a record reads and a put sets. Kept
    /// across a fake reopen like the anchors. `addUnitCreationFixture` seeds it.
    unit_creation_list: std.ArrayListUnmanaged(records.UnitCreation) = .empty,
    /// The vector as it was at the last open: a name the file held is always accepted
    /// back by a put, however odd (the real bridge's openedUC* sets).
    unit_creation_at_open: std.ArrayListUnmanaged(records.UnitCreation) = .empty,
    /// The lists a unit-creation put is checked against and the combos offer
    /// (partys.xml and the aviation / squads folders of the real database).
    uc_parties: []const []const u8 = &.{ "USSR", "Germany", "Allies" },
    uc_aircraft: []const []const u8 = &.{ "Po_2", "Yak-7", "Tb-3", "IL_2", "Ju-87" },
    uc_squads: []const []const u8 = &.{ "USSR_rpd_43", "German_rpd_43" },
    /// The anchors as they were at the last open: a slot put back to its own
    /// value there is exempt from the on-the-map rule, as the real bridge's
    /// (WR-B03).
    camera_anchors_at_open: records.CameraAnchors = .{},
    /// The map's reinforcement groups (04-09, D-16), by group ID; each value's
    /// slice is owned here. Kept across a fake reopen like the sounds.
    /// `addGroupFixture` seeds it; `recordKeys` answers it sorted.
    groups: std.AutoHashMapUnmanaged(i32, []i32) = .empty,
    /// The groups as they were at the last open (owned copies): a put may keep
    /// any script ID - as many times - as the group held then, as the real
    /// GroupPutAllowed's openedGroups exemption (WR-B06).
    groups_at_open: std.AutoHashMapUnmanaged(i32, []i32) = .empty,
    /// The map's script file (04-10, D-20) and the value it held when the map
    /// was opened, which a put may always bring back (an undo). Kept across a
    /// fake reopen like the groups. `setScriptFileFixture` seeds both.
    script_file: records.ScriptFile = .{},
    script_file_at_open: records.ScriptFile = .{},
    /// The map's script areas (04-10, D-21), in list order, AI units as stored,
    /// and the list as the map was opened (a name the file held twice may be put
    /// back as often as it held it). Kept across a fake reopen. The fake takes
    /// AI units per world unit from `map_per_world`, truncating with Vis2AI's
    /// +0.3 as the real conversion does.
    script_areas: std.ArrayListUnmanaged(records.ScriptArea) = .empty,
    script_areas_at_open: std.ArrayListUnmanaged(records.ScriptArea) = .empty,
    /// The script IDs "Hide checked" holds back (04-09): objects of the
    /// objects list (not scenario objects) carrying one are skipped by
    /// `objectAt`. A view setting: forgotten by an open, as the real one.
    hidden_script_ids: std.ArrayListUnmanaged(i32) = .empty,
    /// The Place tool's ghost (PARITY O7): a scene object only, so it is in none
    /// of the lists, none of the history and none of the saved bytes - it is
    /// here only so a test can read what the tool asked the engine to show.
    /// `ghost_sets` counts the calls that asked (a redundant call is visible).
    ghost: ?Ghost = null,
    ghost_sets: u32 = 0,
    /// Entries `setPlacementGhost` refuses, as the engine refuses an entry the
    /// palette cannot place (a test fixture; none by default).
    ghost_refused_name: [bridge_mod.name_capacity]u8 = undefined,
    ghost_refused_len: usize = 0,
    /// Bridge spans: link ID to the bridge that holds it. A span cannot be
    /// deleted singly (the game's loaders assert every link of a bridge).
    bridge_spans: std.AutoHashMapUnmanaged(i32, i32) = .empty,
    /// Trench pieces: link ID to the entrenchment that holds it. A piece cannot
    /// be deleted singly (LoadEntrenchments asserts every link).
    trench_pieces: std.AutoHashMapUnmanaged(i32, i32) = .empty,
    /// The map's start commands, in file order. `addStartCommandFixture`.
    start_commands: std.ArrayListUnmanaged(FakeStartCommand) = .empty,
    /// The start commands as the map was opened: a command of the file is always
    /// accepted back (an undo of a delete). Kept across a fake reopen.
    start_commands_at_open: std.ArrayListUnmanaged(FakeStartCommand) = .empty,
    /// The action types the fake lists (04-11): empty means the built-in list
    /// `default_actions`; `no_action_list` is the file missing.
    action_list: std.ArrayListUnmanaged(bridge_mod.ActionCommand) = .empty,
    no_action_list: bool = false,
    /// Link IDs that are not units or squads (a building, a tree): a start
    /// command may not name them. Every other known object is a unit.
    non_units: std.ArrayListUnmanaged(i32) = .empty,
    /// The map's reserve positions, in file order. `addReservePositionFixture`.
    reserve_positions: std.ArrayListUnmanaged(FakeReservePosition) = .empty,
    /// The positions as the map was opened (always accepted back), the role table
    /// and the squads (04-11). Kept across a fake reopen.
    reserve_positions_at_open: std.ArrayListUnmanaged(FakeReservePosition) = .empty,
    roles: std.ArrayListUnmanaged(FakeRole) = .empty,
    squads: std.ArrayListUnmanaged(i32) = .empty,
    /// The soldiers of the map's squads (M3, D-25): member link -> squad
    /// link. The real bridge answers a soldier's pick with his squad's link
    /// ID; this list is what the fake answers from (addSquadMemberFixture).
    squad_members: std.ArrayListUnmanaged(FakeSquadMember) = .empty,
    /// The link rules and flag swaps the fake's link answers answer from
    /// (M3, D-26/D-27). The real bridge reads the stats and partys.xml; the
    /// fake has neither, so a test seeds what it needs.
    link_rules: std.ArrayListUnmanaged(FakeLinkRule) = .empty,
    flag_swaps: std.ArrayListUnmanaged(FakeFlagSwap) = .empty,
    /// The AI general's sides (04-12, D-19): the list's length is the side count. A
    /// side owns its script IDs, parcels and points (`AiSide.deinit`); its `side_count`
    /// field is unused here (the list's length is the count), its `side` is its index.
    ai_sides: std.ArrayListUnmanaged(records.AiSide) = .empty,
    tombstones: std.AutoHashMapUnmanaged(i32, Tombstone) = .empty,
    /// The season's road (0) and river (1) types. `addVsoDescriptorFixture`.
    vso_descriptors: [2]std.ArrayListUnmanaged(VsoDescriptor) = .{ .empty, .empty },
    /// The map's roads (0) and rivers (1), in list order. Kept across a reopen.
    vso_lists: [2]std.ArrayListUnmanaged(FakeVso) = .{ .empty, .empty },
    /// The edit log (BkEditorUndoEdit/RedoEdit): by token, and the two stacks.
    edits: std.ArrayListUnmanaged(FakeEdit) = .empty,
    /// The bridge types (`addBridgeTypeFixture`) and the map's bridges entries
    /// (`addBridgeEntryFixture`, and every drawn bridge), in list order.
    bridge_types: std.ArrayListUnmanaged(FakeBridgeType) = .empty,
    bridge_entries: std.ArrayListUnmanaged(FakeBridgeEntry) = .empty,
    /// The fence types (`addFenceTypeFixture`).
    fence_types: std.ArrayListUnmanaged(FenceDescriptor) = .empty,
    /// The map's entrenchments drawn with the Entrenchment tool (04-08), in
    /// list order: the link IDs of each one's pieces (the fake keeps one
    /// section per trench). A fixture's `trench_pieces` stay apart.
    trench_entries: std.ArrayListUnmanaged(FakeBridgeEntry) = .empty,
    applied_edits: std.ArrayListUnmanaged(i32) = .empty,
    undone_edits: std.ArrayListUnmanaged(i32) = .empty,
    diplomacy_table: std.ArrayListUnmanaged(i32) = .empty,
    tiles: []u8 = &no_tiles,
    /// The fire-range areas the fake's AI shows (05-07): what a test puts here
    /// `minimapAreas` answers; empty by default, as before any group shows them.
    minimap_areas: std.ArrayListUnmanaged(bridge_mod.MinimapArea) = .empty,
    /// What `createMinimapImages` was last asked: how many times, and the path
    /// (cut to the buffer). The fake writes no file.
    images_created: u32 = 0,
    images_path: [256]u8 = undefined,
    images_path_len: usize = 0,
    /// The map's vertex heights (M3, D-19): (tiles + 1) per axis, row-major -
    /// the fake keeps heights only, its ground being flat, so there is no
    /// shade to recompute. Allocated at the first open, like `tiles`.
    altitudes_grid: []f32 = &no_altitudes,
    /// The heights stroke-start cache (M3, D-18): whether a stroke is open,
    /// the click modes' frozen targets, and the last Generate type asked of
    /// the fake (its tests read it back).
    heights_stroke_open: bool = false,
    click_tile_valid: bool = false,
    click_tile_height: f32 = 0,
    click_average: f32 = 0,
    last_generate_type: bridge_mod.HeightsGenerateType = .hills,
    /// The terrain-mode toggles (M3, D-20): set through setTerrainModes, the
    /// defaults the MFC's own. The fake reads them back in its tests; the
    /// per-stroke objects-Z pass and the fit snap they name are the real
    /// bridge's business (the fake holds no engine, no object database and
    /// no object z), so no fake behaviour keys off them yet.
    instant_update: bool = false,
    fit_to_grid: bool = true,
    /// The Layers menu (M3, D-32): the renderer's bits as the fake engine
    /// holds them, and what it can draw. The fixture renderer cannot draw the
    /// depth complexity (the real GPU renderer cannot either - 05-06's probe),
    /// so a test sees the mask respected. An open or a new map brings the
    /// renderer up at `layers_mod.default_bits` and drops every fire-range
    /// group - what the engine's own memory does to the MFC editor's menu -
    /// so a test proves the editor re-applies what it remembers rather than
    /// relying on the bridge to.
    layer_bits: u32 = layers_mod.default_bits,
    layer_mask: u32 = layers_mod.all_bits & ~layers_mod.bit(.depth_complexity),
    fire_mode: u32 = 0,
    fire_filter_buffer: [layers_mod.max_filter_len]u8 = undefined,
    fire_filter_len: usize = 0,
    fire_link_ids: std.ArrayListUnmanaged(i32) = .empty,
    /// The fields fixtures (M3, D-21): the season a fieldSetSeason ask
    /// answers (the fixture set is a summer one) and the names listRmg
    /// answers for the field sets folder.
    field_set_season: i32 = 0,
    field_set_names: []const []const u8 = &.{"scenarios/fieldsets/summer_basic"},
    /// Create Random Map's fixtures (05-08): the names listRmg answers for the
    /// templates, settings and chapters folders, how many graphs every
    /// template has, what the last generation asked for, the names already
    /// generated (a repeat is refused without overwrite - the real bridge's
    /// rule over the user's maps folder) and the seed a blank seed draws.
    template_names: []const []const u8 = &.{"scenarios\\templates\\summer\\small"},
    setting_names: []const []const u8 = &.{"scenarios\\settings\\summer_france"},
    chapter_names: []const []const u8 = &.{"scenarios\\chapters\\allies\\france\\context"},
    template_graph_count: i32 = 3,
    /// Every file the fake storages hold under the folders the Export lists
    /// walk (05-08, D-13), as the real enumeration names them: lower case,
    /// backslashes, the extension kept - listStorageFiles filters this by
    /// folder and extension exactly as the real one does.
    storage_files: []const []const u8 = &.{
        "scenarios\\templates\\summer\\template00.xml",
        "scenarios\\templates\\summer\\template01.xml",
        "scenarios\\chapters\\allies\\france\\chapter.xml",
        "scenarios\\chapters\\allies\\france\\context.xml",
        "scenarios\\chapters\\german\\poland\\context.xml",
        "scenarios\\patches\\ridge.bzm",
        "scenarios\\patches\\ridge.xml",
        "maps\\arena.bzm",
        "maps\\river3d.xml",
        "maps\\road3d.xml",
        "maps\\duel.xml",
    },
    /// What rmgTemplateGraphs answers for every template: names and weights.
    template_graph_names: []const []const u8 = &.{ "scenarios\\graphs\\summer\\graph00", "scenarios\\graphs\\summer\\graph01", "scenarios\\graphs\\summer\\graph02" },
    template_graph_weights: []const i32 = &.{ 1, 2, 3 },
    /// Settings rmgCheckSetting (and createRandomMap) refuse for every template.
    unfit_settings: []const []const u8 = &.{},
    last_generate: ?bridge_mod.RmgGenerateParams = null,
    generated_names: std.ArrayListUnmanaged([bridge_mod.rmg_map_name_capacity]u8) = .empty,
    drawn_seed: u32 = 0x5eed0001,
    last_field_apply: ?bridge_mod.FieldApplyParams = null,
    field_apply_refused: bool = false,
    /// Set by a test: a fields apply adds one object (the fill's own), at
    /// the polygon's first point - so the document's object list can be
    /// checked across the composite's undo and redo.
    field_adds_object: bool = false,
    /// The object filters (M3, D-31): the user file's contents, saved whole
    /// by `saveObjectFilters`; `objectFilters` merges them over the shipped
    /// `default_filters` (user wins by name) the way the real bridge reads
    /// its two files. Seeded empty - the two fixtures are always "shipped".
    object_filters: std.ArrayListUnmanaged(bridge_mod.ObjectFilter) = .empty,
    paints: std.ArrayListUnmanaged(PaintRecord) = .empty,
    applied: std.ArrayListUnmanaged(i32) = .empty,
    undone: std.ArrayListUnmanaged(i32) = .empty,
    link_floor: i32 = 0,
    calls: std.ArrayListUnmanaged(Call) = .empty,
    message_buffer: [384]u8 = undefined,
    message_len: usize = 0,
    /// Set by a test to make the next `objects()` call fail, as a listing
    /// might after a successful open (a corrupt scenario, say).
    fail_objects: bool = false,
    /// Set by a test to make `openMap` answer `failed`, as the real bridge
    /// does when the engine throws while it builds the new map.
    fail_build: bool = false,
    /// Set by a test to make `undoEdit` of this token fail (as an engine that
    /// will not take a span back does), for the core's replay-unwinding tests.
    fail_undo_token: ?i32 = null,
    /// The same for `redoEdit`.
    fail_redo_token: ?i32 = null,
    /// Set by a test to make `paint` refuse cells that are on the map, as
    /// the real bridge does when the map will not take a paint (no tileset).
    refuse_paints: bool = false,
    /// Set by a test so `saveMap` writes its result into a shared FakeFiles,
    /// at the OS form of whatever path it is given - the same fake
    /// filesystem `Editor.files`'s temp+backup+swap operates on, so a
    /// plan-6 save test can see the whole dance land in one place. Not
    /// owned: the test that sets this owns the FakeFiles.
    files: ?*files_mod.FakeFiles = null,
    /// Set by a test to make `saveMap` answer `failed` before writing
    /// anything, as the real bridge does when the disk write itself fails.
    fail_save: bool = false,
    /// Map units per world unit. 1 keeps screen, world and map the same
    /// point, which most tests read most easily; the real engine's is
    /// sqrt 2 (BkEditorWorldToMap), and a test sets another to catch a
    /// world point used as a map position.
    map_per_world: f32 = 1,
    /// Set by a test: `damageObject` refuses naming the missing stats, as
    /// the real bridge refuses an object whose stats did not read (the MFC
    /// bug is not copied).
    no_stats_fixture: bool = false,
    /// The RMG composers' fixtures (05-09): the containers, graphs and patch
    /// maps the storages hold, the user root the bridge reports, how many
    /// patches were copied in, and whether the next write is refused as the
    /// real bridge refuses a name under no folder.
    rmg_containers: std.ArrayListUnmanaged(RmgContainerFile) = .empty,
    rmg_graphs: std.ArrayListUnmanaged(RmgGraphFile) = .empty,
    rmg_patches: std.ArrayListUnmanaged(RmgPatchFile) = .empty,
    rmg_root_text: []const u8 = "/fake/user/rmg",
    rmg_imports: usize = 0,
    /// The Fields Composer's fixtures (05-10): the field sets the storages
    /// hold, how many terrain types each season's tileset has (a season set to
    /// 0 is a tileset that will not load) and the files `rmgFileExists` knows
    /// (lower case, backslashes, the extension kept).
    rmg_fieldsets: std.ArrayListUnmanaged(RmgFieldSetFile) = .empty,
    rmg_templates: std.ArrayListUnmanaged(RmgTemplateFile) = .empty,
    tileset_counts: [4]usize = .{ 12, 12, 12, 12 },
    known_files: []const []const u8 = &.{"scenarios\\profiles\\profile.tga"},

    pub fn init(allocator: std.mem.Allocator, width_tiles: i32, height_tiles: i32, players: i32) FakeBridge {
        return .{
            .allocator = allocator,
            .info = .{ .width_tiles = width_tiles, .height_tiles = height_tiles, .player_count = players },
        };
    }

    /// The fixture filters: the shipped file's own names and word lists
    /// (one condition of one word each), user 0.
    const default_filters = blk: {
        var buildings = bridge_mod.ObjectFilter{};
        buildings.setName("Buildings");
        buildings.lists[0].word_count = 1;
        @memcpy(buildings.lists[0].words[0][0.."buildings".len], "buildings");
        var squads = bridge_mod.ObjectFilter{};
        squads.setName("Squads");
        squads.lists[0].word_count = 1;
        @memcpy(squads.lists[0].words[0][0.."squads".len], "squads");
        break :blk [_]bridge_mod.ObjectFilter{ buildings, squads };
    };

    pub fn deinit(self: *FakeBridge) void {
        self.fire_link_ids.deinit(self.allocator);
        self.generated_names.deinit(self.allocator);
        for (self.rmg_containers.items) |*file| {
            self.allocator.free(file.name);
            file.value.deinit(self.allocator);
        }
        self.rmg_containers.deinit(self.allocator);
        for (self.rmg_graphs.items) |*file| {
            self.allocator.free(file.name);
            file.value.deinit(self.allocator);
        }
        self.rmg_graphs.deinit(self.allocator);
        for (self.rmg_fieldsets.items) |*file| {
            self.allocator.free(file.name);
            file.value.deinit(self.allocator);
        }
        self.rmg_fieldsets.deinit(self.allocator);
        for (self.rmg_templates.items) |*file| {
            self.allocator.free(file.name);
            file.value.deinit(self.allocator);
        }
        self.rmg_templates.deinit(self.allocator);
        for (self.rmg_patches.items) |file| {
            self.allocator.free(file.name);
            self.allocator.free(file.folder);
            self.allocator.free(file.ids);
            for (file.areas) |area| self.allocator.free(area);
            self.allocator.free(file.areas);
        }
        self.rmg_patches.deinit(self.allocator);
        for (self.paints.items) |paint_record| {
            self.allocator.free(paint_record.cells);
            self.allocator.free(paint_record.before);
        }
        self.paints.deinit(self.allocator);
        self.applied.deinit(self.allocator);
        self.undone.deinit(self.allocator);
        self.objects_list.deinit(self.allocator);
        self.minimap_areas.deinit(self.allocator);
        self.sounds_list.deinit(self.allocator);
        self.script_areas.deinit(self.allocator);
        self.script_areas_at_open.deinit(self.allocator);
        var group_values = self.groups.valueIterator();
        while (group_values.next()) |ids| self.allocator.free(ids.*);
        self.groups.deinit(self.allocator);
        self.forgetGroupsAtOpen();
        self.groups_at_open.deinit(self.allocator);
        self.hidden_script_ids.deinit(self.allocator);
        self.bridge_spans.deinit(self.allocator);
        self.trench_pieces.deinit(self.allocator);
        self.start_commands.deinit(self.allocator);
        self.start_commands_at_open.deinit(self.allocator);
        self.action_list.deinit(self.allocator);
        self.non_units.deinit(self.allocator);
        self.reserve_positions.deinit(self.allocator);
        self.reserve_positions_at_open.deinit(self.allocator);
        self.roles.deinit(self.allocator);
        self.squads.deinit(self.allocator);
        self.squad_members.deinit(self.allocator);
        self.link_rules.deinit(self.allocator);
        self.flag_swaps.deinit(self.allocator);
        for (self.ai_sides.items) |*side| side.deinit(self.allocator);
        self.ai_sides.deinit(self.allocator);
        self.freeTombstones();
        self.tombstones.deinit(self.allocator);
        for (&self.vso_descriptors) |*list| list.deinit(self.allocator);
        for (&self.vso_lists) |*list| list.deinit(self.allocator);
        for (self.edits.items) |*logged| {
            switch (logged.*) {
                .altitudes => |*edit| {
                    self.allocator.free(edit.before);
                    self.allocator.free(edit.after);
                },
                .move_objects => |*edit| edit.moves.deinit(self.allocator),
                .players => |*edit| edit.owners.deinit(self.allocator),
                else => {},
            }
        }
        self.edits.deinit(self.allocator);
        self.bridge_types.deinit(self.allocator);
        self.bridge_entries.deinit(self.allocator);
        self.fence_types.deinit(self.allocator);
        self.trench_entries.deinit(self.allocator);
        self.applied_edits.deinit(self.allocator);
        self.undone_edits.deinit(self.allocator);
        self.object_filters.deinit(self.allocator);
        self.diplomacy_table.deinit(self.allocator);
        self.unit_creation_list.deinit(self.allocator);
        self.unit_creation_at_open.deinit(self.allocator);
        self.calls.deinit(self.allocator);
        self.allocator.free(self.tiles);
        self.allocator.free(self.altitudes_grid);
        self.* = undefined;
    }

    /// An object in the map before it opens; `bridge_span` makes it a span of
    /// bridge 0, so its delete is refused as the real bridge refuses one.
    pub fn addFixture(self: *FakeBridge, new_record: ObjectRecord, bridge_span: bool) !void {
        try self.objects_list.append(self.allocator, new_record);
        if (bridge_span) try self.bridge_spans.put(self.allocator, new_record.link_id, 0);
    }

    /// A start command in the map before it opens: the unit link IDs it
    /// commands (at most eight) and its target's link ID (0: none).
    pub fn addStartCommandFixture(self: *FakeBridge, units: []const i32, target: i32) !void {
        var command: FakeStartCommand = .{ .target = target };
        std.debug.assert(units.len <= command.units.len);
        @memcpy(command.units[0..units.len], units);
        command.unit_count = units.len;
        try self.start_commands.append(self.allocator, command);
    }

    /// A start command in the map before it opens, with every field.
    pub fn addStartCommandFixtureFull(self: *FakeBridge, command: records.StartCommand) !void {
        try self.start_commands.append(self.allocator, FakeStartCommand.fromRecord(command) orelse return error.TooManyUnits);
    }

    /// Makes an object of the map a building or a tree for the start commands: its
    /// link ID is refused as a unit.
    pub fn markNonUnitFixture(self: *FakeBridge, link_id: i32) !void {
        try self.non_units.append(self.allocator, link_id);
    }

    /// A reserve position in the map before it opens, with every field.
    pub fn addReservePositionFixtureFull(self: *FakeBridge, position: FakeReservePosition) !void {
        try self.reserve_positions.append(self.allocator, position);
    }

    /// What BkEditorReserveRole answers for an object type name, and the number the
    /// towing check reads: a gun's weight or a truck's towing force.
    pub fn setRoleFixture(self: *FakeBridge, name: []const u8, role: bridge_mod.ReserveRole, number: f32) !void {
        var entry: FakeRole = .{ .role = role, .number = number };
        const len = @min(name.len, entry.name.len - 1);
        @memcpy(entry.name[0..len], name[0..len]);
        try self.roles.append(self.allocator, entry);
    }

    /// Makes an object of the map a squad: a reserve position refuses it in either role.
    pub fn markSquadFixture(self: *FakeBridge, link_id: i32) !void {
        try self.squads.append(self.allocator, link_id);
    }

    /// One soldier of squad `squad_link` (M3, D-25): a pick that meets the
    /// member answers the squad's link ID, as the real bridge answers it.
    pub fn addSquadMemberFixture(self: *FakeBridge, member_link: i32, squad_link: i32) !void {
        try self.squad_members.append(self.allocator, .{ .member = member_link, .squad = squad_link });
    }

    /// A link rule the fake's `canLink` answers from (M3, D-27): source NAME
    /// to target NAME with the type it links as (0 garrison, 1 train, 2 tow).
    /// The real bridge reads the stats; the fake has no database, so a test
    /// seeds the pairs it needs - the core behavior under test is the token,
    /// the refusal path and the beside-move, not the stats.
    pub fn addLinkRuleFixture(self: *FakeBridge, source_name: []const u8, target_name: []const u8, link_type: i32) !void {
        var rule: FakeLinkRule = .{ .link_type = link_type };
        const src_len = @min(source_name.len, rule.source.len - 1);
        @memcpy(rule.source[0..src_len], source_name[0..src_len]);
        const dst_len = @min(target_name.len, rule.target.len - 1);
        @memcpy(rule.target[0..dst_len], target_name[0..dst_len]);
        try self.link_rules.append(self.allocator, rule);
    }

    /// The flag swap the fake answers (M3, D-26): re-owning a Flag_ object
    /// to `player` renames it to `to_name`, as the real bridge renames it to
    /// Flag_<the party's general side>.
    pub fn addFlagSwapFixture(self: *FakeBridge, player: i32, to_name: []const u8) !void {
        var swap: FakeFlagSwap = .{ .player = player };
        const len = @min(to_name.len, swap.to_name.len - 1);
        @memcpy(swap.to_name[0..len], to_name[0..len]);
        try self.flag_swaps.append(self.allocator, swap);
    }

    fn linkRuleFor(self: *const FakeBridge, source_name: []const u8, target_name: []const u8) ?i32 {
        for (self.link_rules.items) |*rule| {
            if (std.mem.eql(u8, std.mem.sliceTo(&rule.source, 0), source_name) and
                std.mem.eql(u8, std.mem.sliceTo(&rule.target, 0), target_name))
                return rule.link_type;
        }
        return null;
    }

    fn flagSwapFor(self: *const FakeBridge, player: i32) ?[]const u8 {
        for (self.flag_swaps.items) |*swap| {
            if (swap.player == player) return std.mem.sliceTo(&swap.to_name, 0);
        }
        return null;
    }

    /// The link a pick answers for `link_id`: the squad's when the object is
    /// a seeded member, its own otherwise.
    fn pickLinkFor(self: *const FakeBridge, link_id: i32) i32 {
        for (self.squad_members.items) |entry| {
            if (entry.member == link_id) return entry.squad;
        }
        return link_id;
    }

    /// One side of the AI general in the map before it opens (04-12): appended to the
    /// list, so the side count grows by one. The fake keeps a copy.
    pub fn addAiSideFixture(self: *FakeBridge, side: records.AiSide) !void {
        var stored = try side.clone(self.allocator);
        errdefer stored.deinit(self.allocator);
        stored.side = @intCast(self.ai_sides.items.len);
        stored.side_count = 0;
        try self.ai_sides.append(self.allocator, stored);
    }

    /// A sound in the map before it opens, for a test to seed the list
    /// `sounds()`/`editSound`/`deleteSound` then see.
    pub fn addSoundFixture(self: *FakeBridge, new_record: SoundRecord) !void {
        try self.sounds_list.append(self.allocator, new_record);
    }

    /// A reserve position in the map before it opens: the artillery's and the
    /// truck's link IDs (0: none).
    pub fn addReservePositionFixture(self: *FakeBridge, artillery: i32, truck: i32) !void {
        try self.reserve_positions.append(self.allocator, .{ .artillery = artillery, .truck = truck });
    }

    /// Makes an object of the map a piece of entrenchment `entrenchment`, so
    /// its delete is refused as the real bridge refuses one.
    pub fn addTrenchPieceFixture(self: *FakeBridge, link_id: i32, entrenchment: i32) !void {
        try self.trench_pieces.put(self.allocator, link_id, entrenchment);
    }

    /// A road (`.road`) or river (`.river`) type the season offers.
    pub fn addVsoDescriptorFixture(self: *FakeBridge, kind: VsoKind, name: []const u8) !void {
        var descriptor: VsoDescriptor = .{};
        descriptor.setName(name);
        try self.vso_descriptors[@intFromEnum(kind)].append(self.allocator, descriptor);
    }

    /// A road or river in the map before it opens, built as `addVso` builds
    /// one; returns false when the fake's own rules refuse it.
    pub fn addVsoFixture(self: *FakeBridge, kind: VsoKind, desc: []const u8, points: []const records.Vec3, width: f32, opacity: f32) !bool {
        var built: FakeVso = .{};
        if (!self.buildVso(desc, points, width, opacity, &built)) return false;
        try self.vso_lists[@intFromEnum(kind)].append(self.allocator, built);
        return true;
    }

    /// A bridge type the object database offers, with its span length (world
    /// units).
    pub fn addBridgeTypeFixture(self: *FakeBridge, name: []const u8, direction: bridge_mod.BridgeDirection, has_partner: bool, span_length: f32) !void {
        var descriptor: BridgeDescriptor = .{ .direction = direction, .has_partner = has_partner };
        descriptor.setName(name);
        descriptor.build_during_play_allowed = std.mem.indexOf(u8, name, "WoodenBig_Heavy_") != null;
        try self.bridge_types.append(self.allocator, .{ .descriptor = descriptor, .span_length = span_length });
    }

    /// A fence type the object database offers.
    pub fn addFenceTypeFixture(self: *FakeBridge, name: []const u8) !void {
        var descriptor: FenceDescriptor = .{};
        descriptor.setName(name);
        try self.fence_types.append(self.allocator, descriptor);
    }

    /// A bridges entry naming objects already in the map (`addFixture`).
    pub fn addBridgeEntryFixture(self: *FakeBridge, links: []const i32) !void {
        var entry: FakeBridgeEntry = .{ .count = links.len };
        std.debug.assert(links.len <= max_bridge_spans);
        @memcpy(entry.links[0..links.len], links);
        try self.bridge_entries.append(self.allocator, entry);
    }

    pub fn bridgeCount(self: *const FakeBridge) usize {
        return self.bridge_entries.items.len;
    }

    pub fn bridgeEntry(self: *const FakeBridge, index: usize) *const FakeBridgeEntry {
        return &self.bridge_entries.items[index];
    }

    /// The road or river at `index`, for a test to read.
    pub fn vso(self: *const FakeBridge, kind: VsoKind, index: usize) *const FakeVso {
        return &self.vso_lists[@intFromEnum(kind)].items[index];
    }

    pub fn vsoLen(self: *const FakeBridge, kind: VsoKind) usize {
        return self.vso_lists[@intFromEnum(kind)].items.len;
    }

    /// A reinforcement group in the map before it opens (04-09).
    pub fn addGroupFixture(self: *FakeBridge, id: i32, ids: []const i32) !void {
        const owned = try self.allocator.dupe(i32, ids);
        errdefer self.allocator.free(owned);
        const previous = try self.groups.fetchPut(self.allocator, id, owned);
        if (previous) |entry| self.allocator.free(entry.value);
    }

    /// The script file the map names before it opens (04-10), as the file
    /// holds it - a test may give it a path, which reads verbatim.
    pub fn setScriptFileFixture(self: *FakeBridge, name: []const u8) void {
        self.script_file.setName(name);
        self.script_file_at_open = self.script_file;
    }

    /// A script area the map holds before it opens (04-10), in AI units.
    pub fn addScriptAreaFixture(self: *FakeBridge, area: records.ScriptArea) !void {
        try self.script_areas.append(self.allocator, area);
    }

    /// The script IDs of group `id`, for a test to read; null when there is none.
    pub fn groupIDs(self: *const FakeBridge, id: i32) ?[]const i32 {
        return self.groups.get(id);
    }

    /// The camera anchors the map holds before it opens.
    pub fn setCameraAnchorsFixture(self: *FakeBridge, anchors: records.CameraAnchors) void {
        self.camera_anchors = anchors;
    }

    pub fn tile(self: *const FakeBridge, x: i32, y: i32) u8 {
        return self.tiles[@intCast(y * self.info.width_tiles + x)];
    }

    /// One vertex's height, for a test to read (M3, D-19); the sheet is
    /// built flat if nothing has touched it yet.
    pub fn altitude(self: *const FakeBridge, x: i32, y: i32) f32 {
        if (self.altitudes_grid.len == 0) return 0;
        return self.altitudes_grid[self.altitudeIndex(x, y)];
    }

    /// One vertex's height before the map opens (M3, D-19), so a test knows
    /// the before-value an undo must restore.
    pub fn setAltitudeFixture(self: *FakeBridge, x: i32, y: i32, height: f32) !void {
        if (self.ensureAltitudes() != .ok) return error.OutOfMemory;
        self.altitudes_grid[self.altitudeIndex(x, y)] = height;
    }

    pub fn bridge(self: *FakeBridge) Bridge {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ptr: *anyopaque) *FakeBridge {
        return @ptrCast(@alignCast(ptr));
    }

    fn say(self: *FakeBridge, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.message_buffer, format, args) catch self.message_buffer[0..];
        self.message_len = text.len;
    }

    fn record(self: *FakeBridge, kind: CallKind, id: i32) void {
        self.calls.append(self.allocator, .{ .kind = kind, .id = id }) catch {};
    }

    fn onMap(self: *const FakeBridge, x: f32, y: f32) bool {
        return x >= 0 and y >= 0 and
            x < @as(f32, @floatFromInt(self.info.width_tiles)) * tile_size and
            y < @as(f32, @floatFromInt(self.info.height_tiles)) * tile_size;
    }

    /// onMap for a map position rather than a world point.
    fn onMapAt(self: *const FakeBridge, x: f32, y: f32) bool {
        return self.onMap(x / self.map_per_world, y / self.map_per_world);
    }

    fn indexOf(self: *const FakeBridge, link_id: i32) ?usize {
        for (self.objects_list.items, 0..) |object, index| {
            if (object.link_id == link_id) return index;
        }
        return null;
    }

    fn forgetHistory(self: *FakeBridge) void {
        for (self.paints.items) |paint_record| {
            self.allocator.free(paint_record.cells);
            self.allocator.free(paint_record.before);
        }
        self.paints.clearRetainingCapacity();
        self.applied.clearRetainingCapacity();
        self.undone.clearRetainingCapacity();
        self.freeTombstones();
        self.tombstones.clearRetainingCapacity();
        for (self.edits.items) |*logged| {
            switch (logged.*) {
                .altitudes => |*edit| {
                    self.allocator.free(edit.before);
                    self.allocator.free(edit.after);
                },
                .move_objects => |*edit| edit.moves.deinit(self.allocator),
                .players => |*edit| edit.owners.deinit(self.allocator),
                else => {},
            }
        }
        self.edits.clearRetainingCapacity();
        self.applied_edits.clearRetainingCapacity();
        self.undone_edits.clearRetainingCapacity();
    }

    fn freeTombstones(self: *FakeBridge) void {
        var tombstones = self.tombstones.valueIterator();
        while (tombstones.next()) |tombstone| self.freeTombstone(tombstone);
    }

    fn freeTombstone(self: *FakeBridge, tombstone: *Tombstone) void {
        tombstone.changes.deinit(self.allocator);
        tombstone.reserves.deinit(self.allocator);
        for (tombstone.passengers.items) |*passenger| self.freeTombstone(passenger);
        tombstone.passengers.deinit(self.allocator);
    }

    /// The real bridge's summary of what a delete changed besides the object,
    /// in the message buffer: "also removed from start command 0; start
    /// command 3 erased; reserve position 1 erased". Positions are the ones
    /// before the delete.
    fn describeCascade(self: *FakeBridge, changes: []const StartChange, reserves: []const ReserveChange) void {
        self.message_len = 0;
        var erased_before: usize = 0;
        for (changes) |change| {
            const original = change.position + erased_before;
            if (change.erased) erased_before += 1;
            if (change.erased) {
                self.appendPart("start command {d} erased", .{original});
            } else if (change.unit_removed) {
                self.appendPart("removed from start command {d}", .{original});
            }
            if (change.target_cleared) self.appendPart("target of start command {d} cleared", .{original});
        }
        erased_before = 0;
        for (reserves) |reserve| {
            self.appendPart("reserve position {d} erased", .{reserve.position + erased_before});
            erased_before += 1;
        }
    }

    fn appendPart(self: *FakeBridge, comptime format: []const u8, args: anytype) void {
        const prefix: []const u8 = if (self.message_len == 0) "also " else "; ";
        var buffer: [96]u8 = undefined;
        const part = std.fmt.bufPrint(&buffer, format, args) catch return;
        const room = self.message_buffer.len - self.message_len;
        if (prefix.len + part.len > room) return;
        @memcpy(self.message_buffer[self.message_len..][0..prefix.len], prefix);
        self.message_len += prefix.len;
        @memcpy(self.message_buffer[self.message_len..][0..part.len], part);
        self.message_len += part.len;
    }

    /// More than one object carrying the link ID: the real bridge cannot
    /// tell which one an edit means, and refuses it.
    fn shared(self: *FakeBridge, link_id: i32) bool {
        var count: usize = 0;
        for (self.objects_list.items) |object| {
            if (object.link_id == link_id) count += 1;
        }
        if (count <= 1) return false;
        self.say("{d} objects of the map share link ID {d}, so the editor cannot tell which of them it would change; they are kept as they are", .{ count, link_id });
        return true;
    }

    fn nextLinkId(self: *const FakeBridge) i32 {
        var next: i32 = self.link_floor;
        for (self.objects_list.items) |object| next = @max(next, object.link_id + 1);
        return next;
    }

    /// The tiles the fake's tileset has colours for (05-07).
    pub const minimap_tile_count: usize = 64;

    /// The colour the fake gives a tile index: distinct for every index below
    /// `minimap_tile_count`, so a test can tell which tile a pixel came from.
    pub fn minimapColorOf(tile_index: usize) u32 {
        return (@as(u32, @intCast(tile_index * 4 + 3)) << 16) | (@as(u32, @intCast(255 - tile_index * 3)) << 8) | @as(u32, @intCast(tile_index * 2));
    }

    /// BkEditorTiles: a region of `tiles`, row-major, row 0 first.
    fn tiles_read(ptr: *anyopaque, region: bridge_mod.TileRegion, out: []u8, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        total.* = 0;
        if (region.x1 <= region.x0 or region.y1 <= region.y0) return .bad_argument;
        if (self.tiles.len == 0) {
            self.say("no map is open", .{});
            return .refused;
        }
        if (region.x0 < 0 or region.y0 < 0 or region.x1 > self.info.width_tiles or region.y1 > self.info.height_tiles) {
            self.say("the region {d},{d}-{d},{d} is not on the map", .{ region.x0, region.y0, region.x1, region.y1 });
            return .refused;
        }
        const width: usize = @intCast(region.x1 - region.x0);
        const area = width * @as(usize, @intCast(region.y1 - region.y0));
        total.* = area;
        if (out.len < area) {
            self.say("the region holds {d} tiles, the buffer has room for {d}", .{ area, out.len });
            return .refused;
        }
        var at: usize = 0;
        var y = region.y0;
        while (y < region.y1) : (y += 1) {
            var x = region.x0;
            while (x < region.x1) : (x += 1) {
                out[at] = self.tile(x, y);
                at += 1;
            }
        }
        return .ok;
    }

    fn minimapTileColors(ptr: *anyopaque, out: []u32, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        total.* = 0;
        if (self.tiles.len == 0) {
            self.say("no map is open", .{});
            return .refused;
        }
        total.* = minimap_tile_count;
        const fit = @min(out.len, minimap_tile_count);
        for (0..fit) |tile_index| out[tile_index] = minimapColorOf(tile_index);
        return if (out.len >= minimap_tile_count) .ok else .refused;
    }

    /// One marker per object: five AI tiles square around its position, clamped
    /// to the map, the player's colour of the 17.
    fn minimapUnits(ptr: *anyopaque, out: []bridge_mod.MinimapUnit, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        total.* = 0;
        if (self.tiles.len == 0) {
            self.say("no map is open", .{});
            return .refused;
        }
        const limit_x = self.info.width_tiles * 2;
        const limit_y = self.info.height_tiles * 2;
        var count: usize = 0;
        for (self.objects_list.items) |object| {
            if (!object.known) continue;
            const cx: i32 = @intFromFloat(object.x / 32.0);
            const cy: i32 = @intFromFloat(object.y / 32.0);
            const unit: bridge_mod.MinimapUnit = .{
                .link_id = object.link_id,
                .x0 = @max(cx - 2, 0),
                .y0 = @max(cy - 2, 0),
                .x1 = @min(cx + 3, limit_x + 1),
                .y1 = @min(cy + 3, limit_y + 1),
                .color_index = if (object.player >= 0 and object.player < 17) object.player else 16,
                .squad = 0,
            };
            if (count < out.len) out[count] = unit;
            count += 1;
        }
        total.* = count;
        return if (out.len >= count) .ok else .refused;
    }

    fn minimapAreas(ptr: *anyopaque, out: []bridge_mod.MinimapArea, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        const areas = self.minimap_areas.items;
        total.* = areas.len;
        const fit = @min(out.len, areas.len);
        @memcpy(out[0..fit], areas[0..fit]);
        return if (out.len >= areas.len) .ok else .refused;
    }

    /// Records the request and writes nothing; a path that is no .bzm or .xml
    /// is a bad argument, one without a map open or a full path is refused.
    fn createMinimapImages(ptr: *anyopaque, map_path: []const u8) Status {
        const self = from(ptr);
        self.message_len = 0;
        const is_map = std.ascii.endsWithIgnoreCase(map_path, ".bzm") or std.ascii.endsWithIgnoreCase(map_path, ".xml");
        if (map_path.len == 0 or !is_map) return .bad_argument;
        if (self.tiles.len == 0) {
            self.say("no map is open", .{});
            return .refused;
        }
        if (map_path[0] != '/' and map_path[0] != '\\' and !(map_path.len > 1 and map_path[1] == ':')) {
            self.say("minimap images are written beside a saved map: give the map's full path", .{});
            return .refused;
        }
        self.images_created += 1;
        const len = @min(map_path.len, self.images_path.len);
        @memcpy(self.images_path[0..len], map_path[0..len]);
        self.images_path_len = len;
        return .ok;
    }

    const vtable: Bridge.VTable = .{
        .lastMessage = lastMessage,
        .openMap = openMap,
        .saveMap = saveMap,
        .objects = objects,
        .diplomacy = diplomacy,
        .addObject = addObject,
        .placeObject = placeObject,
        .deleteObject = deleteObject,
        .restoreObject = restoreObject,
        .setDiplomacy = setDiplomacy,
        .setMapType = setMapType,
        .setAttackingSide = setAttackingSide,
        .paint = paint,
        .undoPaint = undoPaint,
        .redoPaint = redoPaint,
        .screenToWorld = screenToWorld,
        .worldToTile = worldToTile,
        .worldToMap = worldToMap,
        .objectAt = objectAt,
        .setPlacementGhost = setPlacementGhost,
        .clearPlacementGhost = clearPlacementGhost,
        .placementGhost = placementGhost,
        .pickObjects = pickObjects,
        .pickObjectsInTiles = pickObjectsInTiles,
        .moveObjects = moveObjects,
        .setObjectFields = setObjectFields,
        .canLink = canLink,
        .setLink = setLink,
        .unlink = unlink,
        .damageObject = damageObject,
        .sounds = sounds,
        .addSound = addSound,
        .setSound = setSound,
        .deleteSound = deleteSound,
        .readRecord = readRecord,
        .putRecord = putRecord,
        .recordKeys = recordKeys,
        .insertRecord = insertRecord,
        .removeRecord = removeRecord,
        .firstFreeGroupID = firstFreeGroupID,
        .setHiddenScriptIDs = setHiddenScriptIDs,
        .groundHeight = groundHeight,
        .setObjectScriptID = setObjectScriptID,
        .scriptAreaFromVis = scriptAreaFromVis,
        .scriptAreaMoved = scriptAreaMoved,
        .scriptAreaResized = scriptAreaResized,
        .actionCommands = actionCommands,
        .reserveRole = reserveRole,
        .undoEdit = undoEdit,
        .redoEdit = redoEdit,
        .altitudes = altitudes,
        .setAltitudes = setAltitudes,
        .newMap = newMap,
        .heightsStroke = heightsStroke,
        .generateHeights = generateHeights,
        .setZeroHeights = setZeroHeights,
        .updateMap = updateMap,
        .fillEntireMap = fillEntireMap,
        .setTerrainModes = setTerrainModes,
        .layers = layers,
        .setLayerShow = setLayerShow,
        .setFireRangeMode = setFireRangeMode,
        .snapToGrid = snapToGrid,
        .vsoDescriptors = vsoDescriptors,
        .vsoCount = vsoCount,
        .readVso = readVso,
        .addVso = addVso,
        .deleteVso = deleteVso,
        .moveVsoPoints = moveVsoPoints,
        .setVsoWidth = setVsoWidth,
        .setVsoOpacity = setVsoOpacity,
        .insertVsoPoint = insertVsoPoint,
        .deleteVsoPoint = deleteVsoPoint,
        .pickVso = pickVso,
        .bridgeDescriptors = bridgeDescriptors,
        .planBridge = planBridge,
        .drawBridge = drawBridge,
        .bridges = bridges,
        .pickGroup = pickGroup,
        .deleteBridge = deleteBridge,
        .rotateBridge = rotateBridge,
        .toggleBridgeBuild = toggleBridgeBuild,
        .fenceDescriptors = fenceDescriptors,
        .planFences = planFences,
        .drawFences = drawFences,
        .planEntrenchment = planEntrenchment,
        .drawEntrenchment = drawEntrenchment,
        .entrenchments = entrenchments,
        .deleteEntrenchment = deleteEntrenchment,
        .objectFilters = objectFilters,
        .saveObjectFilters = saveObjectFilters,
        .applyField = applyField,
        .fieldSetSeason = fieldSetSeason,
        .listRmg = listRmg,
        .createRandomMap = createRandomMap,
        .listStorageFiles = listStorageFiles,
        .rmgTemplateGraphs = rmgTemplateGraphs,
        .rmgCheckSetting = rmgCheckSetting,
        .rmgReadContainer = rmgReadContainer,
        .rmgWriteContainer = rmgWriteContainer,
        .rmgReadGraph = rmgReadGraph,
        .rmgWriteGraph = rmgWriteGraph,
        .rmgPatchInfo = rmgPatchInfo,
        .rmgImportPatch = rmgImportPatch,
        .rmgRoot = rmgRoot,
        .rmgReadFieldSet = rmgReadFieldSet,
        .rmgReadTemplate = rmgReadTemplate,
        .rmgWriteTemplate = rmgWriteTemplate,
        .rmgWriteFieldSet = rmgWriteFieldSet,
        .rmgTileset = rmgTileset,
        .rmgFileExists = rmgFileExists,
        .addPlayer = addPlayer,
        .deletePlayer = deletePlayer,
        .unitCreationChoices = unitCreationChoices,
        .tiles = tiles_read,
        .minimapTileColors = minimapTileColors,
        .minimapUnits = minimapUnits,
        .minimapAreas = minimapAreas,
        .createMinimapImages = createMinimapImages,
    };

    /// The real builder's rules the core sees, without the sampling: drops a
    /// point within `vso_min_point_distance` of the one kept before it; "too
    /// short" below two points; a width in world units at every key point.
    fn buildVso(self: *FakeBridge, desc: []const u8, points: []const records.Vec3, width: f32, opacity: f32, out: *FakeVso) bool {
        var vso_record: FakeVso = .{};
        vso_record.desc.setName(desc);
        for (points) |point| {
            if (vso_record.count > 0) {
                const last = vso_record.controls[vso_record.count - 1];
                const dx = point.x - last.x;
                const dy = point.y - last.y;
                if (@sqrt(dx * dx + dy * dy) <= vso_min_point_distance) continue;
            }
            if (vso_record.count == max_vso_points) {
                self.say("the fake holds at most {d} points per road", .{max_vso_points});
                return false;
            }
            vso_record.controls[vso_record.count] = .{ .x = point.x, .y = point.y, .z = 0 };
            vso_record.count += 1;
        }
        if (vso_record.count < 2) {
            self.say("that road is too short: it needs two points at least 2 units apart", .{});
            return false;
        }
        vso_record.renormal();
        for (vso_record.keys[0..vso_record.count]) |*key| {
            key.width = width;
            key.opacity = opacity;
        }
        vso_record.saved_id = self.nextVsoId();
        out.* = vso_record;
        return true;
    }

    /// One above every saved ID of both lists, at least 1 (NextVsoID).
    fn nextVsoId(self: *const FakeBridge) i32 {
        var next: i32 = 1;
        for (&self.vso_lists) |*list| {
            for (list.items) |item| next = @max(next, item.saved_id + 1);
        }
        return next;
    }

    /// The one put an edit, its undo and its redo share (PutVso).
    fn putVso(self: *FakeBridge, kind: VsoKind, index: usize, before: ?*const FakeVso, after: ?*const FakeVso) Status {
        const list = &self.vso_lists[@intFromEnum(kind)];
        if (before != null and after != null) {
            if (index >= list.items.len) return .failed;
            list.items[index] = after.?.*;
        } else if (after) |record_after| {
            if (index > list.items.len) return .failed;
            list.insert(self.allocator, index, record_after.*) catch return .failed;
        } else if (before != null) {
            if (index >= list.items.len) return .failed;
            _ = list.orderedRemove(index);
        }
        return .ok;
    }

    /// Puts an edit through and logs it, handing out its token.
    fn logVsoEdit(self: *FakeBridge, edit: FakeVsoEdit, token: *i32) Status {
        self.edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        self.applied_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        const put = self.putVso(edit.kind, edit.index, if (edit.before) |*b| b else null, if (edit.after) |*a| a else null);
        if (put != .ok) return put;
        self.edits.appendAssumeCapacity(.{ .vso = edit });
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.appendAssumeCapacity(token.*);
        self.undone_edits.clearRetainingCapacity();
        self.record(.vso_edit, token.*);
        return .ok;
    }

    fn undoEdit(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (self.fail_undo_token == token) {
            self.say("the engine would not take edit {d} back", .{token});
            return .failed;
        }
        if (self.applied_edits.items.len == 0 or self.applied_edits.items[self.applied_edits.items.len - 1] != token) {
            self.say("edits are undone newest first", .{});
            return .refused;
        }
        self.undone_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        const put = switch (self.edits.items[@intCast(token)]) {
            .fields => |*edit| blk: {
                if (edit.added) |added| {
                    if (self.indexOf(added.link_id)) |index| _ = self.objects_list.orderedRemove(index);
                }
                break :blk Status.ok;
            },
            .vso => |*edit| self.putVso(edit.kind, edit.index, if (edit.after) |*a| a else null, if (edit.before) |*b| b else null),
            .bridge => |*edit| self.putBridge(if (edit.after) |*a| a else null, if (edit.before) |*b| b else null),
            .build => |edit| self.putBuild(edit.index, edit.before),
            .altitudes => |*edit| self.putAltitudesGrid(.{ .x0 = edit.x0, .y0 = edit.y0, .x1 = edit.x1, .y1 = edit.y1 }, edit.before),
            .move_objects => |*edit| blk: {
                self.putMoves(edit.moves.items, true);
                break :blk Status.ok;
            },
            .object_fields => |*edit| blk: {
                const put = self.putObjectRecordBack(edit.link_id, &edit.before);
                break :blk put;
            },
            .players => |*edit| self.putPlayersState(&edit.before, edit.owners.items, false),
        };
        if (put != .ok) return put;
        _ = self.applied_edits.pop();
        self.undone_edits.appendAssumeCapacity(token);
        self.record(.undo_edit, token);
        return .ok;
    }

    fn redoEdit(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (self.fail_redo_token == token) {
            self.say("the engine would not redo edit {d}", .{token});
            return .failed;
        }
        if (self.undone_edits.items.len == 0 or self.undone_edits.items[self.undone_edits.items.len - 1] != token) {
            self.say("edits are redone in the order they were undone", .{});
            return .refused;
        }
        self.applied_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        const put = switch (self.edits.items[@intCast(token)]) {
            .fields => |*edit| blk: {
                if (edit.added) |added| self.objects_list.append(self.allocator, added) catch break :blk Status.failed;
                break :blk Status.ok;
            },
            .vso => |*edit| self.putVso(edit.kind, edit.index, if (edit.before) |*b| b else null, if (edit.after) |*a| a else null),
            .bridge => |*edit| self.putBridge(if (edit.before) |*b| b else null, if (edit.after) |*a| a else null),
            .build => |edit| self.putBuild(edit.index, edit.after),
            .altitudes => |*edit| self.putAltitudesGrid(.{ .x0 = edit.x0, .y0 = edit.y0, .x1 = edit.x1, .y1 = edit.y1 }, edit.after),
            .move_objects => |*edit| blk: {
                self.putMoves(edit.moves.items, false);
                break :blk Status.ok;
            },
            .object_fields => |*edit| blk: {
                const put = self.putObjectRecordBack(edit.link_id, &edit.after);
                break :blk put;
            },
            .players => |*edit| self.putPlayersState(&edit.after, edit.owners.items, true),
        };
        if (put != .ok) return put;
        _ = self.undone_edits.pop();
        self.applied_edits.appendAssumeCapacity(token);
        self.record(.redo_edit, token);
        return .ok;
    }

    // Altitudes (M3, D-19): the real rules the core can see - an empty or
    // inverted region and a count mismatch are caller bugs, a region off the
    // map is an ordinary refusal, and a refusal changes nothing. The fake
    // keeps heights only (its ground is flat), one more vertex than tiles per
    // axis like the real sheet.
    fn vertexCountX(self: *const FakeBridge) usize {
        return @as(usize, @intCast(self.info.width_tiles)) + 1;
    }

    pub fn altitudeIndex(self: *const FakeBridge, x: i32, y: i32) usize {
        return @as(usize, @intCast(y)) * self.vertexCountX() + @as(usize, @intCast(x));
    }

    fn ensureAltitudes(self: *FakeBridge) Status {
        if (self.altitudes_grid.len != 0) return .ok;
        const vertices_x = self.vertexCountX();
        const vertices_y = @as(usize, @intCast(self.info.height_tiles)) + 1;
        self.altitudes_grid = self.allocator.alloc(f32, vertices_x * vertices_y) catch return .failed;
        @memset(self.altitudes_grid, 0);
        return .ok;
    }

    /// False for a region that is empty, inverted or off the fake's vertex
    /// bounds; says why, as the real bridge does.
    fn altitudesRegionUsable(self: *FakeBridge, region: AltitudeRegion) bool {
        if (region.x1 <= region.x0 or region.y1 <= region.y0) return false;
        const bounds_x = @as(i32, @intCast(self.vertexCountX()));
        const bounds_y = self.info.height_tiles + 1;
        if (region.x0 < 0 or region.y0 < 0 or region.x1 > bounds_x or region.y1 > bounds_y) {
            self.say("vertices {d},{d}..{d},{d} are not on the map", .{ region.x0, region.y0, region.x1, region.y1 });
            return false;
        }
        return true;
    }

    fn altitudes(ptr: *anyopaque, region: AltitudeRegion, heights: []f32, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (region.x1 <= region.x0 or region.y1 <= region.y0) return .bad_argument;
        const started = self.ensureAltitudes();
        if (started != .ok) return started;
        if (!self.altitudesRegionUsable(region)) return .refused;
        const width = @as(usize, @intCast(region.x1 - region.x0));
        const area = width * @as(usize, @intCast(region.y1 - region.y0));
        total.* = area;
        const fit = @min(heights.len, area);
        var i: usize = 0;
        var y = region.y0;
        while (y < region.y1) : (y += 1) {
            var x = region.x0;
            while (x < region.x1) : (x += 1) {
                if (i < fit) heights[i] = self.altitudes_grid[self.altitudeIndex(x, y)];
                i += 1;
            }
        }
        return if (heights.len >= area) .ok else .refused;
    }

    fn setAltitudes(ptr: *anyopaque, region: AltitudeRegion, heights: []const f32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (region.x1 <= region.x0 or region.y1 <= region.y0) return .bad_argument;
        const width = @as(usize, @intCast(region.x1 - region.x0));
        const area = width * @as(usize, @intCast(region.y1 - region.y0));
        if (heights.len != area) return .bad_argument;
        for (heights) |height| {
            if (!std.math.isFinite(height)) return .bad_argument;
        }
        const started = self.ensureAltitudes();
        if (started != .ok) return started;
        if (!self.altitudesRegionUsable(region)) return .refused;
        const before = self.allocator.alloc(f32, area) catch return .failed;
        var filled: usize = 0;
        var y = region.y0;
        while (y < region.y1) : (y += 1) {
            var x = region.x0;
            while (x < region.x1) : (x += 1) {
                before[filled] = self.altitudes_grid[self.altitudeIndex(x, y)];
                filled += 1;
            }
        }
        const after = self.allocator.dupe(f32, heights) catch {
            self.allocator.free(before);
            return .failed;
        };
        _ = self.putAltitudesGrid(region, heights);
        self.edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        self.applied_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        self.edits.appendAssumeCapacity(.{ .altitudes = .{
            .x0 = region.x0,
            .y0 = region.y0,
            .x1 = region.x1,
            .y1 = region.y1,
            .before = before,
            .after = after,
        } });
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.appendAssumeCapacity(token.*);
        self.undone_edits.clearRetainingCapacity();
        self.record(.altitudes_edit, token.*);
        return .ok;
    }

    /// Writes `values` row-major over the region - the one put an altitude
    /// edit, its undo and its redo share.
    fn putAltitudesGrid(self: *FakeBridge, region: AltitudeRegion, values: []const f32) Status {
        var i: usize = 0;
        var y = region.y0;
        while (y < region.y1) : (y += 1) {
            var x = region.x0;
            while (x < region.x1) : (x += 1) {
                if (i >= values.len) return .failed;
                self.altitudes_grid[self.altitudeIndex(x, y)] = values[i];
                i += 1;
            }
        }
        return .ok;
    }

    /// BkEditorNewMap (M3, D-23): the real rules the core can see - sizes
    /// 1..32 patches per axis, season 0..3, a whole new map (the fake's
    /// objects, paints and edits are forgotten, its tiles and altitudes
    /// resized and refilled; the tile is the season's most common terrain
    /// type's index, the fake having no variant draw). The mod is the
    /// caller's word: the fake holds no mods of its own.
    fn newMap(ptr: *anyopaque, params: NewMapParams, info: *MapInfo) Status {
        const self = from(ptr);
        self.message_len = 0;
        self.record(.new_map, 0);
        if (params.size_x < 1 or params.size_x > 32 or params.size_y < 1 or params.size_y > 32) {
            self.say("a new map is 1..32 patches per axis", .{});
            return .bad_argument;
        }
        if (params.season < 0 or params.season > 3) {
            self.say("the season is Summer, Winter, Africa or Spring", .{});
            return .bad_argument;
        }
        const width = params.size_x * 16;
        const height = params.size_y * 16;
        // The renderer comes up at its own defaults and the AI's groups are
        // gone with the map before (see layer_bits).
        self.layer_bits = layers_mod.default_bits;
        self.ghost = null;
        self.fire_mode = 0;
        self.fire_filter_len = 0;
        self.fire_link_ids.clearRetainingCapacity();
        // A new map replaces everything: the objects, the history, the
        // tombstones and the tiles of the map before.
        self.forgetHistory();
        self.objects_list.clearRetainingCapacity();
        self.freeTombstones();
        self.tombstones.clearRetainingCapacity();
        self.hidden_script_ids.clearRetainingCapacity();
        self.info.width_tiles = width;
        self.info.height_tiles = height;
        // REAL_SEASONS (MapInfo_Consts.cpp): the dialog's Spring maps onto
        // the Summer the map stores.
        const real_seasons = [4]i32{ 0, 1, 2, 0 };
        self.info.season = real_seasons[@intCast(params.season)];
        self.info.player_count = 2;
        if (self.tiles.len != @as(usize, @intCast(width * height))) {
            self.allocator.free(self.tiles);
            self.tiles = self.allocator.alloc(u8, @intCast(width * height)) catch return .failed;
        }
        @memset(self.tiles, most_common_tiles[@intCast(params.season)]);
        if (self.altitudes_grid.len != (@as(usize, @intCast(width)) + 1) * (@as(usize, @intCast(height)) + 1)) {
            self.allocator.free(self.altitudes_grid);
            self.altitudes_grid = self.allocator.alloc(f32, (@as(usize, @intCast(width)) + 1) * (@as(usize, @intCast(height)) + 1)) catch return .failed;
        }
        @memset(self.altitudes_grid, 0);
        self.diplomacy_table.clearRetainingCapacity();
        self.diplomacy_table.appendSlice(self.allocator, &.{ 2, 2 }) catch return .failed;
        self.link_floor = self.nextLinkId();
        self.heights_stroke_open = false;
        info.* = self.info;
        return .ok;
    }

    // Heights (M3, D-18): the real rules the core can see - the brush range,
    // the stroke-start click cache, one edit per step - over a knowingly
    // simpler model, which a core test must not lean on:
    //  - the pattern is a smooth radial dome speed * (1 - distance from the
    //    pattern's centre), the real profile.tga gradient's own shape family
    //    without the image: the fake has no data storage;
    //  - the validity rule is |height| <= fake_height_limit rather than the
    //    engine's slope predicate, so a test triggers the rollback the same
    //    way (a speed past the limit) with a rule it can compute;
    //  - generate is a fixed diagonal ramp into [min_z, max_z] (the fake has
    //    no Perlin noise), set zero is the zeros.
    fn heightsPatternValue(brush: i32, speed: f32, cell_x: i32, cell_y: i32) f32 {
        const n: f32 = @floatFromInt(brush * 2);
        // ApplyVAInRadius's own arithmetic (VA_Types.h:442-451): the cell's
        // offset from the centre over the half-extent, Euclidean.
        const half: f32 = (n - 1.0) / 2.0;
        const dx = (@as(f32, @floatFromInt(cell_x)) - half) / half;
        const dy = (@as(f32, @floatFromInt(cell_y)) - half) / half;
        const dist = @sqrt(dx * dx + dy * dy);
        if (dist > 1.0) return 0;
        return speed * (1.0 - dist);
    }

    fn heightsMaskValue(brush: i32, cell_x: i32, cell_y: i32) f32 {
        return if (heightsPatternValue(brush, 1.0, cell_x, cell_y) > 0) 1.0 else 0.0;
    }

    fn heightsCorner(brush: i32, tile_x: i32, tile_y: i32) [2]i32 {
        const half = brush - 1;
        return .{ tile_x - half, tile_y - half };
    }

    fn heightsMaskAverage(self: *FakeBridge, brush: i32, corner: [2]i32) f32 {
        const bounds_x: i32 = @intCast(self.vertexCountX());
        const bounds_y: i32 = self.info.height_tiles + 1;
        var total: f64 = 0;
        var count: usize = 0;
        var cy: i32 = 0;
        while (cy < brush * 2) : (cy += 1) {
            var cx: i32 = 0;
            while (cx < brush * 2) : (cx += 1) {
                const x = corner[0] + cx;
                const y = corner[1] + cy;
                if (x < 0 or y < 0 or x >= bounds_x or y >= bounds_y) continue;
                if (heightsMaskValue(brush, cx, cy) == 0) continue;
                total += self.altitudes_grid[self.altitudeIndex(x, y)];
                count += 1;
            }
        }
        return if (count != 0) @floatCast(total / @as(f64, @floatFromInt(count))) else 0;
    }

    fn heightsStroke(ptr: *anyopaque, params: HeightsStrokeParams, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (params.action < 0 or params.action > 2 or params.level_mode < 0 or params.level_mode > 3) return .bad_argument;
        if (params.brush < 2 or params.brush > 16) {
            self.say("the heights brush is 2..16", .{});
            return .bad_argument;
        }
        if (!std.math.isFinite(params.height_speed) or !std.math.isFinite(params.level_ratio_percent) or
            !std.math.isFinite(params.pos_x) or !std.math.isFinite(params.pos_y) or
            !std.math.isFinite(params.click_x) or !std.math.isFinite(params.click_y)) return .bad_argument;
        const started = self.ensureAltitudes();
        if (started != .ok) return started;
        const tile_x: i32 = @intFromFloat(@floor(params.pos_x / tile_size));
        const tile_y: i32 = @intFromFloat(@floor(params.pos_y / tile_size));
        if (tile_x < 0 or tile_y < 0 or tile_x >= self.info.width_tiles or tile_y >= self.info.height_tiles) {
            self.say("the cursor is not over the map", .{});
            return .refused;
        }
        const corner = heightsCorner(params.brush, tile_x, tile_y);
        // The stroke-start cache: the click modes' targets are frozen at the
        // stroke's first step, exactly the session's own rule.
        if (params.stroke_start != 0) {
            self.heights_stroke_open = true;
            const cx: i32 = @intFromFloat(@floor(params.click_x / tile_size));
            const cy: i32 = @intFromFloat(@floor(params.click_y / tile_size));
            self.click_tile_valid = cx >= 0 and cy >= 0 and cx < self.info.width_tiles and cy < self.info.height_tiles;
            if (self.click_tile_valid) {
                self.click_tile_height = (self.altitudes_grid[self.altitudeIndex(cx, cy)] +
                    self.altitudes_grid[self.altitudeIndex(cx + 1, cy)] +
                    self.altitudes_grid[self.altitudeIndex(cx, cy + 1)] +
                    self.altitudes_grid[self.altitudeIndex(cx + 1, cy + 1)]) / 4.0;
                self.click_average = self.heightsMaskAverage(params.brush, heightsCorner(params.brush, cx, cy));
            } else {
                self.click_tile_height = 0;
                self.click_average = 0;
            }
        }
        var target: f32 = 0;
        if (params.action == 2) {
            switch (params.level_mode) {
                0 => target = 0,
                1 => {
                    if (!self.click_tile_valid) {
                        self.say("the stroke's tile left the map", .{});
                        return .refused;
                    }
                    target = self.click_tile_height;
                },
                2 => target = self.heightsMaskAverage(params.brush, corner),
                else => target = self.click_average,
            }
        }
        // The would-be values over the clipped pattern rect; the validity
        // rule runs before anything is written, so a refused step changes
        // nothing by construction.
        const bounds_x: i32 = @intCast(self.vertexCountX());
        const bounds_y: i32 = self.info.height_tiles + 1;
        const x0 = @max(corner[0], 0);
        const y0 = @max(corner[1], 0);
        const x1 = @min(corner[0] + params.brush * 2, bounds_x);
        const y1 = @min(corner[1] + params.brush * 2, bounds_y);
        if (x1 <= x0 or y1 <= y0) return .refused;
        const width: usize = @intCast(x1 - x0);
        const area = width * @as(usize, @intCast(y1 - y0));
        const before = self.allocator.alloc(f32, area) catch return .failed;
        const after = self.allocator.alloc(f32, area) catch {
            self.allocator.free(before);
            return .failed;
        };
        const ratio = params.level_ratio_percent / 100.0;
        var i: usize = 0;
        var invalid = false;
        var y = y0;
        while (y < y1) : (y += 1) {
            var x = x0;
            while (x < x1) : (x += 1) {
                const at = self.altitudes_grid[self.altitudeIndex(x, y)];
                const pattern = heightsPatternValue(params.brush, params.height_speed, x - corner[0], y - corner[1]);
                var value = at;
                switch (params.action) {
                    1 => value = at - pattern,
                    2 => if (heightsMaskValue(params.brush, x - corner[0], y - corner[1]) != 0) {
                        value = at + (target - at) * ratio;
                    },
                    else => value = at + pattern,
                }
                before[i] = at;
                after[i] = value;
                if (@abs(value) > fake_height_limit) invalid = true;
                i += 1;
            }
        }
        if (invalid and params.ctrl_held == 0) {
            self.allocator.free(before);
            self.allocator.free(after);
            self.say("invalid height", .{});
            return .refused;
        }
        _ = self.putAltitudesGrid(.{ .x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1 }, after);
        self.logAltitudesEdit(.{ .x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1 }, before, after, token);
        self.record(.heights_stroke, token.*);
        return .ok;
    }

    fn generateHeights(ptr: *anyopaque, gen_type: bridge_mod.HeightsGenerateType, granularity: f32, min_z: f32, max_z: f32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (!std.math.isFinite(granularity) or !std.math.isFinite(min_z) or !std.math.isFinite(max_z) or granularity <= 0) return .bad_argument;
        const started = self.ensureAltitudes();
        if (started != .ok) return started;
        const w: f32 = @floatFromInt(self.vertexCountX() - 1);
        const h: f32 = @floatFromInt(self.info.height_tiles);
        const values = self.allocator.alloc(f32, self.altitudes_grid.len) catch return .failed;
        defer self.allocator.free(values);
        var y: i32 = 0;
        while (y <= self.info.height_tiles) : (y += 1) {
            var x: i32 = 0;
            while (x <= self.info.width_tiles) : (x += 1) {
                // The fake's fixed diagonal ramp: the shape its tests build
                // their expected values from, into the caller's z range the
                // way the real formula scales its noise.
                const t = ( @as(f32, @floatFromInt(x)) / w + @as(f32, @floatFromInt(y)) / h ) / 2.0;
                values[self.altitudeIndex(x, y)] = min_z * tile_size + (max_z - min_z) * tile_size * t;
            }
        }
        return self.applyWholeSheet(values, token, .heights_generate, gen_type);
    }

    fn setZeroHeights(ptr: *anyopaque, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const started = self.ensureAltitudes();
        if (started != .ok) return started;
        const values = self.allocator.alloc(f32, self.altitudes_grid.len) catch return .failed;
        defer self.allocator.free(values);
        @memset(values, 0);
        return self.applyWholeSheet(values, token, .heights_zero, null);
    }

    /// One logged edit over the whole vertex sheet - the shape generate and
    /// set zero share. `gen_type` rides the recorded call for a test to read.
    fn applyWholeSheet(self: *FakeBridge, values: []const f32, token: *i32, kind: CallKind, gen_type: ?bridge_mod.HeightsGenerateType) Status {
        const region = AltitudeRegion{ .x0 = 0, .y0 = 0, .x1 = self.info.width_tiles + 1, .y1 = self.info.height_tiles + 1 };
        const before = self.allocator.dupe(f32, self.altitudes_grid) catch return .failed;
        const after = self.allocator.dupe(f32, values) catch {
            self.allocator.free(before);
            return .failed;
        };
        _ = self.putAltitudesGrid(region, values);
        self.logAltitudesEdit(region, before, after, token);
        if (gen_type) |which| self.last_generate_type = which;
        self.record(kind, token.*);
        return .ok;
    }

    /// Appends one altitude edit record and hands out its token - the tail
    /// `setAltitudes` and the heights calls share. Takes ownership of both
    /// slices on success only.
    fn logAltitudesEdit(self: *FakeBridge, region: AltitudeRegion, before: []f32, after: []f32, token: *i32) void {
        self.edits.ensureUnusedCapacity(self.allocator, 1) catch return;
        self.applied_edits.ensureUnusedCapacity(self.allocator, 1) catch return;
        self.edits.appendAssumeCapacity(.{ .altitudes = .{
            .x0 = region.x0,
            .y0 = region.y0,
            .x1 = region.x1,
            .y1 = region.y1,
            .before = before,
            .after = after,
        } });
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.appendAssumeCapacity(token.*);
        self.undone_edits.clearRetainingCapacity();
    }

    /// Update Map (M3, D-20): the fake's own simplification, listed in the
    /// header - its map is already consistent (flat ground it keeps shaded
    /// by construction, tiles and crosses always in step), so a fake update
    /// changes no altitudes and no tiles; the logged edit is the whole sheet
    /// with before equal to after, exactly as undoable as the real thing and
    /// byte-neutral. The fit pass and the objects-Z refresh it names are the
    /// real bridge's engine work, proven at the engine tier. Progress hears
    /// the MFC's own count, seven fixed steps, once the work is done.
    fn updateMap(ptr: *anyopaque, progress: ?bridge_mod.ProgressFn, user: ?*anyopaque, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const started = self.ensureAltitudes();
        if (started != .ok) return started;
        const before = self.allocator.dupe(f32, self.altitudes_grid) catch return .failed;
        const after = self.allocator.dupe(f32, self.altitudes_grid) catch {
            self.allocator.free(before);
            return .failed;
        };
        const region = AltitudeRegion{ .x0 = 0, .y0 = 0, .x1 = self.info.width_tiles + 1, .y1 = self.info.height_tiles + 1 };
        self.logAltitudesEdit(region, before, after, token);
        self.record(.update_map, token.*);
        if (progress) |report| {
            var step: c_int = 0;
            while (step < 7) {
                step += 1;
                report(step, 7, user);
            }
        }
        return .ok;
    }

    /// Fill Entire Map (M3, D-22): the fake's fill IS a whole-map paint of
    /// the one tile - the real bridge's own shape (FillEntireMapInSession
    /// rides PaintIntoSession too), so the fake gets the same paint token,
    /// the same undoPaint/redoPaint route, and the paint path's own
    /// refusals for free.
    fn fillEntireMap(ptr: *anyopaque, tile_index: u8, token: *i32) Status {
        const self = from(ptr);
        const cells = self.allocator.alloc(bridge_mod.PaintCell, @intCast(self.info.width_tiles * self.info.height_tiles)) catch return .failed;
        defer self.allocator.free(cells);
        var index: usize = 0;
        var y: i32 = 0;
        while (y < self.info.height_tiles) : (y += 1) {
            var x: i32 = 0;
            while (x < self.info.width_tiles) : (x += 1) {
                cells[index] = .{ .x = x, .y = y, .tile = tile_index };
                index += 1;
            }
        }
        return paint(ptr, cells, token);
    }

    /// The terrain-mode toggles (M3, D-20): a view setting on the fake, no
    /// map data, no history - recorded so a test can see the call, changing
    /// nothing else.
    fn setTerrainModes(ptr: *anyopaque, instant_update: bool, fit_to_grid: bool) Status {
        const self = from(ptr);
        self.instant_update = instant_update;
        self.fit_to_grid = fit_to_grid;
        self.record(.terrain_modes, 0);
        return .ok;
    }

    /// BkEditorLayers (M3, D-32): the fake renderer's bits - the fire-range bit
    /// derived from its mode, as the real one's - and mask.
    fn layers(ptr: *anyopaque, bits: *u32, mask: *u32) Status {
        const self = from(ptr);
        self.message_len = 0;
        bits.* = (self.layer_bits & ~layers_mod.bit(.fire_ranges)) | (if (self.fire_mode != 0) layers_mod.bit(.fire_ranges) else 0);
        mask.* = self.layer_mask | layers_mod.bit(.fire_ranges);
        return .ok;
    }

    /// BkEditorSetLayerShow: the real rules - a layer outside 0..13 and the
    /// fire-range mode are bad arguments, no map open and a layer outside the
    /// mask are refused, and in every refusal nothing changes.
    fn setLayerShow(ptr: *anyopaque, layer: u32, shown: bool) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (layer >= layers_mod.layer_count) {
            self.say("no such layer", .{});
            return .bad_argument;
        }
        if (layer == @intFromEnum(layers_mod.Layer.fire_ranges)) {
            self.say("the fire ranges are a mode, not a toggle", .{});
            return .bad_argument;
        }
        if (self.tiles.len == 0) {
            self.say("no map is open", .{});
            return .refused;
        }
        const one = @as(u32, 1) << @intCast(layer);
        if (self.layer_mask & one == 0) {
            self.say("that layer cannot be drawn by this renderer", .{});
            return .refused;
        }
        if (shown) self.layer_bits |= one else self.layer_bits &= ~one;
        self.record(.layer_show, @intCast(layer));
        return .ok;
    }

    fn filterKnown(self: *const FakeBridge, name: []const u8) bool {
        for (default_filters) |one| {
            if (std.mem.eql(u8, one.nameSlice(), name)) return true;
        }
        for (self.object_filters.items) |one| {
            if (std.mem.eql(u8, one.nameSlice(), name)) return true;
        }
        return false;
    }

    /// BkEditorSetFireRangeMode: the real rules, the shown set kept as the
    /// mode, the filter's name and the selection it was asked with (a test
    /// reads them back).
    fn setFireRangeMode(ptr: *anyopaque, mode: u32, filter: []const u8, link_ids: []const i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (mode > @intFromEnum(layers_mod.FireMode.filter)) {
            self.say("no such fire-range mode", .{});
            return .bad_argument;
        }
        if (mode == @intFromEnum(layers_mod.FireMode.filter) and (filter.len == 0 or !self.filterKnown(filter))) {
            self.say("no object filter is named that", .{});
            return .bad_argument;
        }
        if (self.tiles.len == 0) {
            self.say("no map is open", .{});
            return .refused;
        }
        self.fire_mode = mode;
        self.fire_filter_len = if (mode == @intFromEnum(layers_mod.FireMode.filter)) @min(filter.len, self.fire_filter_buffer.len) else 0;
        @memcpy(self.fire_filter_buffer[0..self.fire_filter_len], filter[0..self.fire_filter_len]);
        self.fire_link_ids.clearRetainingCapacity();
        if (mode == @intFromEnum(layers_mod.FireMode.selected)) {
            self.fire_link_ids.appendSlice(self.allocator, link_ids) catch return .failed;
        }
        self.record(.fire_range, @intCast(mode));
        return .ok;
    }

    /// The map's own position for the link ID: the fake's record, read back.
    /// BkEditorSnapToGrid (M3, D-20): the fake's database knows no origins,
    /// so its answer is the input unchanged, whatever the toggles say - the
    /// rule itself is proven at the engine tier, where the stats live.
    fn snapToGrid(ptr: *anyopaque, name: [*:0]const u8, x: f32, y: f32, out_x: *f32, out_y: *f32) Status {
        _ = name;
        const self = from(ptr);
        self.message_len = 0;
        out_x.* = x;
        out_y.* = y;
        return .ok;
    }

    fn vsoDescriptors(ptr: *anyopaque, kind: VsoKind, out: []VsoDescriptor, total: *usize) Status {
        const self = from(ptr);
        const list = self.vso_descriptors[@intFromEnum(kind)].items;
        total.* = list.len;
        const count = @min(out.len, list.len);
        @memcpy(out[0..count], list[0..count]);
        return if (out.len >= list.len) .ok else .refused;
    }

    fn vsoCount(ptr: *anyopaque, kind: VsoKind, count: *usize) Status {
        const self = from(ptr);
        count.* = self.vso_lists[@intFromEnum(kind)].items.len;
        return .ok;
    }

    fn readVso(ptr: *anyopaque, kind: VsoKind, index: i32, allocator: std.mem.Allocator, out: *VsoView) Status {
        const self = from(ptr);
        const list = self.vso_lists[@intFromEnum(kind)].items;
        if (index < 0 or index >= list.len) return .bad_argument;
        const item = &list[@intCast(index)];
        const controls = allocator.dupe(records.Vec3, item.controlSlice()) catch return .failed;
        const keys = allocator.dupe(VsoKeyPoint, item.keySlice()) catch {
            allocator.free(controls);
            return .failed;
        };
        out.* = .{ .saved_id = item.saved_id, .control_points = controls, .key_points = keys };
        // The descriptor's full saved name, as the real bridge answers it: the
        // season folder, Roads3D\ or Rivers\, and the bare name (WR-B06) -
        // `vsoDescriptors` alone answers bare names.
        _ = std.fmt.bufPrint(out.desc[0 .. out.desc.len - 1], "{s}{s}{s}", .{ fake_season_folder, if (kind == .road) "Roads3D\\" else "Rivers\\", item.desc.nameSlice() }) catch {
            allocator.free(controls);
            allocator.free(keys);
            return .failed;
        };
        return .ok;
    }

    fn deleteVso(ptr: *anyopaque, kind: VsoKind, index: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const list = self.vso_lists[@intFromEnum(kind)].items;
        if (index < 0 or index >= list.len) return .bad_argument;
        const at: usize = @intCast(index);
        return self.logVsoEdit(.{ .kind = kind, .index = at, .before = list[at], .after = null }, token);
    }

    /// The record at `index`, or null (a caller bug: bad_argument).
    fn vsoAt(self: *FakeBridge, kind: VsoKind, index: i32) ?FakeVso {
        const list = self.vso_lists[@intFromEnum(kind)].items;
        if (index < 0 or index >= list.len) return null;
        return list[@intCast(index)];
    }

    fn logReplace(self: *FakeBridge, kind: VsoKind, index: i32, before: FakeVso, after: FakeVso, token: *i32) Status {
        return self.logVsoEdit(.{ .kind = kind, .index = @intCast(index), .before = before, .after = after }, token);
    }

    fn moveVsoPoints(ptr: *anyopaque, kind: VsoKind, index: i32, points: []const records.Vec3, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (points.len != before.count) return .bad_argument;
        for (points) |point| {
            if (!finite(point)) return .bad_argument;
        }
        for (points, 0..) |point, at| {
            if (!self.onMap(point.x, point.y)) {
                self.say("point {d} would be off the map", .{at});
                return .refused;
            }
            if (at > 0) {
                const dx = point.x - points[at - 1].x;
                const dy = point.y - points[at - 1].y;
                if (dx * dx + dy * dy <= vso_min_point_distance * vso_min_point_distance) {
                    self.say("two neighbouring points would be closer than 2 units", .{});
                    return .refused;
                }
            }
        }
        var after = before;
        for (points, 0..) |point, at| after.controls[at] = .{ .x = point.x, .y = point.y, .z = 0 };
        after.renormal();
        return self.logReplace(kind, index, before, after, token);
    }

    /// Whether key `at` is changed by an edit at `key` in `mode`.
    fn inMode(mode: bridge_mod.VsoWidthMode, key: usize, at: usize) bool {
        return switch (mode) {
            .single => at == key,
            .multi => at >= key,
            .all => true,
        };
    }

    fn setVsoWidth(ptr: *anyopaque, kind: VsoKind, index: i32, key: i32, width: f32, mode: bridge_mod.VsoWidthMode, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (key < 0 or key >= before.count or !std.math.isFinite(width) or width <= 0) return .bad_argument;
        var after = before;
        for (after.keys[0..after.count], 0..) |*item, at| {
            if (inMode(mode, @intCast(key), at)) item.width = width;
        }
        return self.logReplace(kind, index, before, after, token);
    }

    fn setVsoOpacity(ptr: *anyopaque, kind: VsoKind, index: i32, key: i32, opacity: f32, mode: bridge_mod.VsoWidthMode, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (key < 0 or key >= before.count or !std.math.isFinite(opacity) or opacity < 0 or opacity > 1) return .bad_argument;
        var after = before;
        for (after.keys[0..after.count], 0..) |*item, at| {
            if (inMode(mode, @intCast(key), at)) item.opacity = opacity;
        }
        return self.logReplace(kind, index, before, after, token);
    }

    fn insertVsoPoint(ptr: *anyopaque, kind: VsoKind, index: i32, control: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (control < 0 or control >= before.count) return .bad_argument;
        if (before.count == max_vso_points) {
            self.say("the fake holds at most {d} points per road", .{max_vso_points});
            return .refused;
        }
        const c: usize = @intCast(control);
        const other = if (c + 1 < before.count) c + 1 else c - 1;
        const at = if (c + 1 < before.count) c + 1 else c;
        var after = before;
        var middle = before.controls[c];
        middle.x = (before.controls[c].x + before.controls[other].x) / 2;
        middle.y = (before.controls[c].y + before.controls[other].y) / 2;
        var key = before.keys[c];
        key.width = (before.keys[c].width + before.keys[other].width) / 2;
        key.opacity = (before.keys[c].opacity + before.keys[other].opacity) / 2;
        var move = after.count;
        while (move > at) : (move -= 1) {
            after.controls[move] = after.controls[move - 1];
            after.keys[move] = after.keys[move - 1];
        }
        after.controls[at] = middle;
        after.keys[at] = key;
        after.count += 1;
        after.renormal();
        return self.logReplace(kind, index, before, after, token);
    }

    fn deleteVsoPoint(ptr: *anyopaque, kind: VsoKind, index: i32, control: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const before = self.vsoAt(kind, index) orelse return .bad_argument;
        if (control < 0 or control >= before.count) return .bad_argument;
        if (before.count <= 2) {
            self.say("a road needs at least 2 points", .{});
            return .refused;
        }
        var after = before;
        var at: usize = @intCast(control);
        while (at + 1 < after.count) : (at += 1) {
            after.controls[at] = after.controls[at + 1];
            after.keys[at] = after.keys[at + 1];
        }
        after.count -= 1;
        after.renormal();
        return self.logReplace(kind, index, before, after, token);
    }

    /// The fake's hit test: within the wider of the two key widths of a
    /// segment of the line, roads before rivers (the real one tests the
    /// sampled stripe's bounding polygon).
    fn pickVso(ptr: *anyopaque, wx: f32, wy: f32, cycle: i32, kind: *VsoKind, index: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        index.* = -1;
        if (cycle < 0 or !std.math.isFinite(wx) or !std.math.isFinite(wy)) return .bad_argument;
        var hits: [64]bridge_mod.VsoRef = undefined;
        var count: usize = 0;
        for ([_]VsoKind{ .road, .river }) |candidate_kind| {
            for (self.vso_lists[@intFromEnum(candidate_kind)].items, 0..) |*item, at| {
                if (count == hits.len) break;
                if (!hitsLine(item, wx, wy)) continue;
                hits[count] = .{ .kind = candidate_kind, .index = at };
                count += 1;
            }
        }
        if (count == 0) {
            self.say("no road or river there", .{});
            return .refused;
        }
        const hit = hits[@as(usize, @intCast(cycle)) % count];
        kind.* = hit.kind;
        index.* = @intCast(hit.index);
        return .ok;
    }

    fn hitsLine(item: *const FakeVso, x: f32, y: f32) bool {
        var at: usize = 0;
        while (at + 1 < item.count) : (at += 1) {
            const a = item.controls[at];
            const b = item.controls[at + 1];
            const dx = b.x - a.x;
            const dy = b.y - a.y;
            const length2 = dx * dx + dy * dy;
            const t = if (length2 > 0) std.math.clamp(((x - a.x) * dx + (y - a.y) * dy) / length2, 0, 1) else 0;
            const px = a.x + t * dx - x;
            const py = a.y + t * dy - y;
            const reach = @max(item.keys[at].width, item.keys[at + 1].width);
            if (px * px + py * py <= reach * reach) return true;
        }
        return false;
    }

    fn knownDescriptor(self: *const FakeBridge, kind: VsoKind, desc: []const u8) bool {
        for (self.vso_descriptors[@intFromEnum(kind)].items) |*item| {
            if (std.mem.eql(u8, item.nameSlice(), desc)) return true;
        }
        return false;
    }

    fn addVso(ptr: *anyopaque, kind: VsoKind, desc: []const u8, points: []const records.Vec3, width_tiles: f32, opacity: f32, token: *i32, index: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        index.* = -1;
        if (points.len > 1024) return .bad_argument;
        if (!std.math.isFinite(width_tiles) or !std.math.isFinite(opacity) or width_tiles < 1 or width_tiles > 16 or opacity < 0 or opacity > 1) return .bad_argument;
        for (points) |point| {
            if (!finite(point)) return .bad_argument;
        }
        if (!self.knownDescriptor(kind, desc)) {
            self.say("there is no {s} type \"{s}\" in this map's season", .{ kind.label(), desc });
            return .refused;
        }
        for (points, 0..) |point, at| {
            if (!self.onMap(point.x, point.y)) {
                self.say("point {d} of the {s} is not on the map", .{ at, kind.label() });
                return .refused;
            }
        }
        var built: FakeVso = .{};
        if (!self.buildVso(desc, points, width_tiles * tile_size / 2.0, opacity, &built)) return .refused;
        const at = self.vso_lists[@intFromEnum(kind)].items.len;
        const logged = self.logVsoEdit(.{ .kind = kind, .index = at, .before = null, .after = built }, token);
        if (logged != .ok) return logged;
        index.* = @intCast(at);
        return .ok;
    }

    /// The bridges entry naming the span, else a fixture span's bridge
    /// (`addFixture(.., true)`), else null.
    fn bridgeHolding(self: *const FakeBridge, link_id: i32) ?i32 {
        for (self.bridge_entries.items, 0..) |*entry, index| {
            if (entry.holds(link_id)) return @intCast(index);
        }
        return self.bridge_spans.get(link_id);
    }

    fn bridgeType(self: *const FakeBridge, name: []const u8) ?*const FakeBridgeType {
        for (self.bridge_types.items) |*item| {
            if (std.mem.eql(u8, item.descriptor.nameSlice(), name)) return item;
        }
        return null;
    }

    /// The real PlanBridge's rules the core can see, without the grid fit and
    /// the truncation: the drag's longer axis must be the type's (a drag with
    /// no length matches either), it is locked to that axis and ordered, and
    /// n = int(run / span length) middle spans lie between a begin and an end
    /// span at the drag's lower end + (i + 0.5) L and + n L. Positions are
    /// MAP units; a span off the map is `off_map`.
    const Plan = struct { pieces: [max_bridge_spans]PlannedPiece = undefined, count: usize = 0, off_map: bool = false };

    fn planFor(self: *FakeBridge, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, plan: *Plan) Status {
        if (desc.len == 0 or desc.len >= bridge_mod.name_capacity) return .bad_argument;
        if (!std.math.isFinite(wx0) or !std.math.isFinite(wy0) or !std.math.isFinite(wx1) or !std.math.isFinite(wy1)) return .bad_argument;
        const kind = self.bridgeType(desc) orelse {
            self.say("\"{s}\" is not a bridge type", .{desc});
            return .refused;
        };
        const horizontal = kind.descriptor.direction == .horizontal;
        const dx = @abs(wx1 - wx0);
        const dy = @abs(wy1 - wy0);
        if (if (horizontal) dy > dx else dx > dy) {
            if (horizontal) self.say("this bridge runs horizontally: drag it along the other axis or rotate the type", .{}) else self.say("this bridge runs vertically: drag it along the other axis or rotate the type", .{});
            return .refused;
        }
        const start: [2]f32 = if (horizontal) .{ @min(wx0, wx1), wy0 } else .{ wx0, @min(wy0, wy1) };
        const run = if (horizontal) dx else dy;
        const parts: usize = @intFromFloat(@floor(run / kind.span_length));
        if (parts + 2 > max_bridge_spans) {
            self.say("the fake holds at most {d} spans per bridge", .{max_bridge_spans});
            return .refused;
        }
        plan.* = .{};
        var index: usize = 0;
        while (index < parts + 2) : (index += 1) {
            const along: f32 = if (index == 0) 0 else if (index == parts + 1) @as(f32, @floatFromInt(parts)) * kind.span_length else (@as(f32, @floatFromInt(index - 1)) + 0.5) * kind.span_length;
            const world: [2]f32 = if (horizontal) .{ start[0] + along, start[1] } else .{ start[0], start[1] + along };
            if (!self.onMap(world[0], world[1])) plan.off_map = true;
            plan.pieces[index] = .{
                .x = world[0] * self.map_per_world,
                .y = world[1] * self.map_per_world,
                .type = if (index == 0) 1 else if (index == parts + 1) 4 else 2,
                .dir = 0,
            };
        }
        plan.count = parts + 2;
        return .ok;
    }

    fn bridgeDescriptors(ptr: *anyopaque, out: []BridgeDescriptor, total: *usize) Status {
        const self = from(ptr);
        total.* = self.bridge_types.items.len;
        for (self.bridge_types.items[0..@min(out.len, self.bridge_types.items.len)], 0..) |item, index| out[index] = item.descriptor;
        return if (out.len >= self.bridge_types.items.len) .ok else .refused;
    }

    fn planBridge(ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, out: []PlannedPiece, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        total.* = 0;
        var plan: Plan = .{};
        const planned = self.planFor(desc, wx0, wy0, wx1, wy1, &plan);
        if (planned != .ok) return planned;
        total.* = plan.count;
        const count = @min(out.len, plan.count);
        @memcpy(out[0..count], plan.pieces[0..count]);
        return if (out.len >= plan.count) .ok else .refused;
    }

    fn drawBridge(ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, token: *i32, index: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        index.* = -1;
        var plan: Plan = .{};
        const planned = self.planFor(desc, wx0, wy0, wx1, wy1, &plan);
        if (planned != .ok) return planned;
        if (plan.off_map) {
            self.say("the engine would not place the bridge's spans there (off the map?)", .{});
            return .refused;
        }
        var group: FakeBridgeGroup = .{ .entry_index = self.bridge_entries.items.len, .count = plan.count };
        var link_id = self.nextLinkId();
        for (plan.pieces[0..plan.count], 0..) |piece, at| {
            var span: ObjectRecord = .{ .link_id = link_id, .x = piece.x, .y = piece.y, .dir = 0, .player = 0 };
            span.setName(desc);
            group.spans[at] = span;
            group.places[at] = std.math.maxInt(usize); // appended
            group.entry.links[at] = link_id;
            link_id += 1;
        }
        group.entry.count = plan.count;
        const logged = self.logBridgeEdit(.{ .before = null, .after = group }, token);
        if (logged != .ok) return logged;
        index.* = @intCast(group.entry_index);
        return .ok;
    }

    fn bridges(ptr: *anyopaque, out: []BridgeInfo, total: *usize) Status {
        const self = from(ptr);
        total.* = self.bridge_entries.items.len;
        for (self.bridge_entries.items[0..@min(out.len, self.bridge_entries.items.len)], 0..) |*entry, at| {
            var info: BridgeInfo = .{ .span_count = @intCast(entry.count), .built_during_play = entry.built };
            var any = false;
            for (entry.linkSlice()) |link| {
                const place = self.indexOf(link) orelse continue;
                const span = self.objects_list.items[place];
                if (!any) {
                    info.setDesc(span.nameSlice());
                    info.min_x = span.x;
                    info.max_x = span.x;
                    info.min_y = span.y;
                    info.max_y = span.y;
                    any = true;
                }
                info.min_x = @min(info.min_x, span.x);
                info.min_y = @min(info.min_y, span.y);
                info.max_x = @max(info.max_x, span.x);
                info.max_y = @max(info.max_y, span.y);
            }
            out[at] = info;
        }
        return if (out.len >= self.bridge_entries.items.len) .ok else .refused;
    }

    /// The fake's pick: a bridge span within `pick_radius` of the point (the
    /// screen is the world here), the last bridge listed first. Trench
    /// pieces are the fixture's `trench_pieces`.
    fn pickGroup(ptr: *anyopaque, sx: f32, sy: f32, kind: *bridge_mod.GroupKind, index: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        index.* = -1;
        var at = self.bridge_entries.items.len;
        while (at != 0) {
            at -= 1;
            for (self.bridge_entries.items[at].linkSlice()) |link| {
                const place = self.indexOf(link) orelse continue;
                const span = self.objects_list.items[place];
                if (@abs(span.x / self.map_per_world - sx) <= pick_radius and @abs(span.y / self.map_per_world - sy) <= pick_radius) {
                    kind.* = .bridge;
                    index.* = @intCast(at);
                    return .ok;
                }
            }
        }
        at = self.trench_entries.items.len;
        while (at != 0) {
            at -= 1;
            for (self.trench_entries.items[at].linkSlice()) |link| {
                const place = self.indexOf(link) orelse continue;
                const piece = self.objects_list.items[place];
                if (@abs(piece.x / self.map_per_world - sx) <= pick_radius and @abs(piece.y / self.map_per_world - sy) <= pick_radius) {
                    kind.* = .entrenchment;
                    index.* = @intCast(at);
                    return .ok;
                }
            }
        }
        var pieces = self.trench_pieces.iterator();
        while (pieces.next()) |piece| {
            const place = self.indexOf(piece.key_ptr.*) orelse continue;
            const object = self.objects_list.items[place];
            if (@abs(object.x / self.map_per_world - sx) <= pick_radius and @abs(object.y / self.map_per_world - sy) <= pick_radius) {
                kind.* = .entrenchment;
                index.* = piece.value_ptr.*;
                return .ok;
            }
        }
        self.say("no bridge or entrenchment there", .{});
        return .refused;
    }

    fn deleteBridge(ptr: *anyopaque, index: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (index < 0 or index >= self.bridge_entries.items.len) return .bad_argument;
        const at: usize = @intCast(index);
        for (self.bridge_entries.items[at].linkSlice()) |link| {
            const place = self.indexOf(link) orelse {
                self.say("span link ID {d} of that bridge is not one the engine holds; the bridge is kept as it is", .{link});
                return .refused;
            };
            if (!self.objects_list.items[place].known) {
                self.say("span link ID {d} of that bridge is not one the engine holds; the bridge is kept as it is", .{link});
                return .refused;
            }
        }
        return self.logBridgeEdit(.{ .before = .{ .entry_index = at, .entry = self.bridge_entries.items[at] }, .after = null }, token);
    }

    /// The fake's fence types, sorted as added.
    fn fenceDescriptors(ptr: *anyopaque, out: []FenceDescriptor, total: *usize) Status {
        const self = from(ptr);
        total.* = self.fence_types.items.len;
        const count = @min(out.len, self.fence_types.items.len);
        @memcpy(out[0..count], self.fence_types.items[0..count]);
        return if (out.len >= self.fence_types.items.len) .ok else .refused;
    }

    fn isFenceType(self: *const FakeBridge, name: []const u8) bool {
        for (self.fence_types.items) |*item| {
            if (std.mem.eql(u8, item.nameSlice(), name)) return true;
        }
        return false;
    }

    /// The AI tile of a world point, floored (the real one rounds; the fake's
    /// tests use points that agree).
    fn aiTile(world: f32) i32 {
        return @intFromFloat(@floor(world / ai_tile_size));
    }

    fn cellOnMap(self: *const FakeBridge, at: [2]i32) bool {
        const cx = at[0] >> 1;
        const cy = at[1] >> 1;
        return cx >= 0 and cy >= 0 and cx < self.info.width_tiles and cy < self.info.height_tiles;
    }

    /// The real PlanFences' rules the core can see, without the grid fit: an
    /// end tile whose cell is off the map refuses the whole run; the same tile
    /// is one fence (direction 0, 1 with `ctrl`); otherwise the longer tile
    /// delta is the axis (a tie is horizontal), one fence every second tile,
    /// direction 1 going left else 3 with the tile moved +2 (horizontal) or 0
    /// going up with the tile moved -2 else 2 (vertical); type
    /// (1 << dir) | 0x10000. Positions are MAP units.
    fn planFencesFor(self: *FakeBridge, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, plan: *Plan) Status {
        if (desc.len == 0 or desc.len >= bridge_mod.name_capacity) return .bad_argument;
        if (!std.math.isFinite(wx0) or !std.math.isFinite(wy0) or !std.math.isFinite(wx1) or !std.math.isFinite(wy1)) return .bad_argument;
        if (!self.isFenceType(desc)) {
            self.say("\"{s}\" is not a fence type", .{desc});
            return .refused;
        }
        const first: [2]i32 = .{ aiTile(wx0), aiTile(wy0) };
        const last: [2]i32 = .{ aiTile(wx1), aiTile(wy1) };
        if (!self.cellOnMap(first) or !self.cellOnMap(last)) {
            self.say("the fence run leaves the map", .{});
            return .refused;
        }
        plan.* = .{};
        var dir: u5 = 0;
        var shift: [2]i32 = .{ 0, 0 };
        var along: usize = 0; // tiles from first to last along the axis
        var step: [2]i32 = .{ 0, 0 };
        if (first[0] == last[0] and first[1] == last[1]) {
            dir = if (ctrl) 1 else 0;
        } else {
            const dx: usize = @abs(last[0] - first[0]);
            const dy: usize = @abs(last[1] - first[1]);
            if (!(dx < dy)) {
                along = dx;
                step = .{ if (last[0] > first[0]) 1 else -1, 0 };
                if (first[0] > last[0]) {
                    dir = 1;
                } else {
                    dir = 3;
                    shift[0] = 2;
                }
            } else {
                along = dy;
                step = .{ 0, if (last[1] > first[1]) 1 else -1 };
                if (first[1] > last[1]) {
                    dir = 0;
                    shift[1] = -2;
                } else {
                    dir = 2;
                }
            }
        }
        const count = along / 2 + 1;
        if (count > max_bridge_spans) {
            self.say("the fake holds at most {d} fences per run", .{max_bridge_spans});
            return .refused;
        }
        for (0..count) |index| {
            const offset: i32 = @intCast(index * 2);
            const tx = first[0] + step[0] * offset + shift[0];
            const ty = first[1] + step[1] * offset + shift[1];
            const x = @as(f32, @floatFromInt(tx)) * ai_tile_size;
            const y = @as(f32, @floatFromInt(ty)) * ai_tile_size;
            if (!self.onMap(x, y)) {
                self.say("the fence run leaves the map", .{});
                return .refused;
            }
            plan.pieces[index] = .{ .x = x * self.map_per_world, .y = y * self.map_per_world, .type = (@as(i32, 1) << dir) | 0x00010000, .dir = 0 };
        }
        plan.count = count;
        return .ok;
    }

    fn planFences(ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, out: []PlannedPiece, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        total.* = 0;
        var plan: Plan = .{};
        const planned = self.planFencesFor(desc, wx0, wy0, wx1, wy1, ctrl, &plan);
        if (planned != .ok) return planned;
        total.* = plan.count;
        const count = @min(out.len, plan.count);
        @memcpy(out[0..count], plan.pieces[0..count]);
        return if (out.len >= plan.count) .ok else .refused;
    }

    fn drawFences(ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        var plan: Plan = .{};
        const planned = self.planFencesFor(desc, wx0, wy0, wx1, wy1, ctrl, &plan);
        if (planned != .ok) return planned;
        var group: FakeBridgeGroup = .{ .entryless = true, .count = plan.count };
        var link_id = self.nextLinkId();
        for (plan.pieces[0..plan.count], 0..) |piece, at| {
            var fence: ObjectRecord = .{ .link_id = link_id, .x = piece.x, .y = piece.y, .dir = 0, .player = 0 };
            fence.setName(desc);
            group.spans[at] = fence;
            group.places[at] = std.math.maxInt(usize); // appended
            group.entry.links[at] = link_id;
            link_id += 1;
        }
        group.entry.count = plan.count;
        return self.logBridgeEdit(.{ .before = null, .after = group }, token);
    }

    /// The drawn entrenchment naming the piece, else a fixture piece's
    /// (`addTrenchPieceFixture`), else null.
    fn trenchHolding(self: *const FakeBridge, link_id: i32) ?i32 {
        for (self.trench_entries.items, 0..) |*entry, index| {
            if (entry.holds(link_id)) return @intCast(index);
        }
        return self.trench_pieces.get(link_id);
    }

    /// The real builder's rules the core can see, without its geometry: a
    /// click within 2 world units of the one kept before it is dropped; fewer
    /// than two kept is "shorter than one piece"; a terminator at each end
    /// (the begin one first, then the end one, as the real plan lists them)
    /// and one piece per step at its midpoint, fireplace and line alternating
    /// from a fireplace; a piece off the map refuses the trench. Positions
    /// are MAP units.
    fn planTrenchFor(self: *FakeBridge, points: []const records.Vec3, plan: *Plan) Status {
        if (points.len > 256) return .bad_argument;
        for (points) |point| {
            if (!std.math.isFinite(point.x) or !std.math.isFinite(point.y)) return .bad_argument;
        }
        var kept: [max_bridge_spans][2]f32 = undefined;
        var count: usize = 0;
        for (points) |point| {
            if (count > 0 and std.math.hypot(point.x - kept[count - 1][0], point.y - kept[count - 1][1]) <= 2) continue;
            if (count + 1 >= max_bridge_spans) {
                self.say("the fake holds at most {d} pieces per trench", .{max_bridge_spans});
                return .refused;
            }
            kept[count] = .{ point.x, point.y };
            count += 1;
        }
        if (count < 2) {
            self.say("the trench is shorter than one piece: click farther on, then double-click", .{});
            return .refused;
        }
        plan.* = .{};
        plan.pieces[0] = .{ .x = kept[0][0], .y = kept[0][1], .type = bridge_mod.trench_terminator };
        plan.pieces[1] = .{ .x = kept[count - 1][0], .y = kept[count - 1][1], .type = bridge_mod.trench_terminator };
        for (0..count - 1) |step| {
            plan.pieces[2 + step] = .{
                .x = (kept[step][0] + kept[step + 1][0]) / 2,
                .y = (kept[step][1] + kept[step + 1][1]) / 2,
                .type = if (step % 2 == 0) bridge_mod.trench_fireplace else bridge_mod.trench_line,
            };
        }
        plan.count = count + 1;
        for (plan.pieces[0..plan.count]) |*piece| {
            if (!self.onMap(piece.x, piece.y)) {
                self.say("the trench leaves the map", .{});
                return .refused;
            }
            piece.x *= self.map_per_world;
            piece.y *= self.map_per_world;
        }
        return .ok;
    }

    fn planEntrenchment(ptr: *anyopaque, points: []const records.Vec3, out: []PlannedPiece, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        total.* = 0;
        var plan: Plan = .{};
        const planned = self.planTrenchFor(points, &plan);
        if (planned != .ok) return planned;
        total.* = plan.count;
        const count = @min(out.len, plan.count);
        @memcpy(out[0..count], plan.pieces[0..count]);
        return if (out.len >= plan.count) .ok else .refused;
    }

    fn drawEntrenchment(ptr: *anyopaque, points: []const records.Vec3, player: i32, token: *i32, index: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        index.* = -1;
        if (player < 0 or player >= self.info.player_count) return .bad_argument;
        var plan: Plan = .{};
        const planned = self.planTrenchFor(points, &plan);
        if (planned != .ok) return planned;
        var group: FakeBridgeGroup = .{ .trench = true, .entry_index = self.trench_entries.items.len, .count = plan.count };
        var link_id = self.nextLinkId();
        for (plan.pieces[0..plan.count], 0..) |piece, at| {
            var object: ObjectRecord = .{ .link_id = link_id, .x = piece.x, .y = piece.y, .dir = 0, .player = player };
            object.setName("Entrenchment");
            group.spans[at] = object;
            group.places[at] = std.math.maxInt(usize); // appended
            group.entry.links[at] = link_id;
            link_id += 1;
        }
        group.entry.count = plan.count;
        const logged = self.logBridgeEdit(.{ .before = null, .after = group }, token);
        if (logged != .ok) return logged;
        index.* = @intCast(group.entry_index);
        return .ok;
    }

    fn entrenchments(ptr: *anyopaque, out: []EntrenchmentInfo, total: *usize) Status {
        const self = from(ptr);
        total.* = self.trench_entries.items.len;
        for (self.trench_entries.items[0..@min(out.len, self.trench_entries.items.len)], 0..) |*entry, at| {
            var info: EntrenchmentInfo = .{ .piece_count = @intCast(entry.count), .section_count = 1 };
            var any = false;
            for (entry.linkSlice()) |link| {
                const place = self.indexOf(link) orelse continue;
                const piece = self.objects_list.items[place];
                if (!any) {
                    info.player = piece.player;
                    info.min_x = piece.x;
                    info.max_x = piece.x;
                    info.min_y = piece.y;
                    info.max_y = piece.y;
                    any = true;
                }
                info.min_x = @min(info.min_x, piece.x);
                info.min_y = @min(info.min_y, piece.y);
                info.max_x = @max(info.max_x, piece.x);
                info.max_y = @max(info.max_y, piece.y);
            }
            out[at] = info;
        }
        return if (out.len >= self.trench_entries.items.len) .ok else .refused;
    }

    /// The real delete's rules the core sees: every piece a known object of
    /// the map, none holding a unit (nLinkWith is not modelled here, so that
    /// refusal is the real bridge's alone).
    fn deleteEntrenchment(ptr: *anyopaque, index: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (index < 0 or index >= self.trench_entries.items.len) return .bad_argument;
        const at: usize = @intCast(index);
        for (self.trench_entries.items[at].linkSlice()) |link| {
            const place = self.indexOf(link) orelse {
                self.say("piece link ID {d} of that entrenchment is not one the engine holds; the entrenchment is kept as it is", .{link});
                return .refused;
            };
            if (!self.objects_list.items[place].known) {
                self.say("piece link ID {d} of that entrenchment is not one the engine holds; the entrenchment is kept as it is", .{link});
                return .refused;
            }
        }
        return self.logBridgeEdit(.{ .before = .{ .trench = true, .entry_index = at, .entry = self.trench_entries.items[at] }, .after = null }, token);
    }

    /// The number of drawn entrenchments, for a test to read.
    pub fn trenchCount(self: *const FakeBridge) usize {
        return self.trench_entries.items.len;
    }

    pub fn trenchEntry(self: *const FakeBridge, index: usize) *const FakeBridgeEntry {
        return &self.trench_entries.items[index];
    }

    fn putBuild(self: *FakeBridge, index: usize, built: bool) Status {
        if (index >= self.bridge_entries.items.len) return .failed;
        self.bridge_entries.items[index].built = built;
        return .ok;
    }

    /// The type of an entry: its first span's name.
    fn entryType(self: *const FakeBridge, index: usize) ?[]const u8 {
        const entry = &self.bridge_entries.items[index];
        if (entry.count == 0) return null;
        const place = self.indexOf(entry.links[0]) orelse return null;
        return self.objects_list.items[place].nameSlice();
    }

    /// The real rotate's rules the core can see: the partner (`_01` <-> `_02`)
    /// must be a bridge type; the new spans lie along the partner's axis about
    /// the mean of the old first and last spans, the same count; a span off
    /// the map refuses; built during play carries over.
    fn rotateBridge(ptr: *anyopaque, index: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (index < 0 or index >= self.bridge_entries.items.len) return .bad_argument;
        const at: usize = @intCast(index);
        const entry = self.bridge_entries.items[at];
        const name = self.entryType(at) orelse {
            self.say("that bridge has no spans", .{});
            return .refused;
        };
        var partner_buffer: [bridge_mod.name_capacity]u8 = undefined;
        const partner: ?[]const u8 = partner: {
            if (name.len < 3) break :partner null;
            const tail = name[name.len - 3 ..];
            const swapped = if (std.mem.eql(u8, tail, "_01")) "_02" else if (std.mem.eql(u8, tail, "_02")) "_01" else break :partner null;
            @memcpy(partner_buffer[0 .. name.len - 3], name[0 .. name.len - 3]);
            @memcpy(partner_buffer[name.len - 3 .. name.len], swapped);
            break :partner partner_buffer[0..name.len];
        };
        const kind = if (partner) |p| self.bridgeType(p) else null;
        if (kind == null) {
            self.say("no rotated variant of {s}", .{name});
            return .refused;
        }
        const first = self.objects_list.items[self.indexOf(entry.links[0]).?];
        const last = self.objects_list.items[self.indexOf(entry.links[entry.count - 1]) orelse return .failed];
        const centre: [2]f32 = .{ (first.x + last.x) / 2 / self.map_per_world, (first.y + last.y) / 2 / self.map_per_world };
        const parts: usize = if (entry.count > 2) entry.count - 2 else 0;
        const length = kind.?.span_length;
        const horizontal = kind.?.descriptor.direction == .horizontal;
        const from_centre = -@as(f32, @floatFromInt(parts)) * length / 2;
        var group: FakeBridgeGroup = .{ .entry_index = at, .count = parts + 2 };
        group.entry.count = parts + 2;
        group.entry.built = entry.built;
        var link_id = self.nextLinkId();
        for (0..parts + 2) |piece| {
            const along: f32 = from_centre + (if (piece == 0) 0 else if (piece == parts + 1) @as(f32, @floatFromInt(parts)) * length else (@as(f32, @floatFromInt(piece - 1)) + 0.5) * length);
            const world: [2]f32 = if (horizontal) .{ centre[0] + along, centre[1] } else .{ centre[0], centre[1] + along };
            if (!self.onMap(world[0], world[1])) {
                self.say("the engine would not place the rotated bridge's spans there (off the map?)", .{});
                return .refused;
            }
            var span: ObjectRecord = .{ .link_id = link_id, .x = world[0] * self.map_per_world, .y = world[1] * self.map_per_world };
            span.setName(partner.?);
            group.spans[piece] = span;
            group.places[piece] = std.math.maxInt(usize);
            group.entry.links[piece] = link_id;
            link_id += 1;
        }
        return self.logBridgeEdit(.{ .before = .{ .entry_index = at, .entry = entry }, .after = group }, token);
    }

    fn toggleBridgeBuild(ptr: *anyopaque, index: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (index < 0 or index >= self.bridge_entries.items.len) return .bad_argument;
        const at: usize = @intCast(index);
        const name = self.entryType(at) orelse "";
        if (std.mem.indexOf(u8, name, "WoodenBig_Heavy_") == null) {
            self.say("only WoodenBig_Heavy bridges can be built during play", .{});
            return .refused;
        }
        self.edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        self.applied_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        const built = self.bridge_entries.items[at].built;
        _ = self.putBuild(at, !built);
        self.edits.appendAssumeCapacity(.{ .build = .{ .index = at, .before = built, .after = !built } });
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.appendAssumeCapacity(token.*);
        self.undone_edits.clearRetainingCapacity();
        self.record(.bridge_edit, token.*);
        return .ok;
    }

    /// Puts a bridge edit through and logs it, handing out its token.
    fn logBridgeEdit(self: *FakeBridge, edit: FakeBridgeEdit, token: *i32) Status {
        self.edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        self.applied_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        var logged = edit;
        const put = self.putBridge(if (logged.before) |*b| b else null, if (logged.after) |*a| a else null);
        if (put != .ok) return put;
        self.edits.appendAssumeCapacity(.{ .bridge = logged });
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.appendAssumeCapacity(token.*);
        self.undone_edits.clearRetainingCapacity();
        self.record(.bridge_edit, token.*);
        return .ok;
    }

    /// The one put of a bridge edit, its undo and its redo: `out` taken out
    /// (its span records and places captured into it), then `in` put in.
    fn putBridge(self: *FakeBridge, out: ?*FakeBridgeGroup, in: ?*const FakeBridgeGroup) Status {
        if (out) |group| {
            const taken = self.removeBridgeGroup(group);
            if (taken != .ok) return taken;
        }
        if (in) |group| {
            const put = self.addBridgeGroup(group);
            if (put != .ok) return put;
        }
        return .ok;
    }

    /// The entry first, then the spans in descending place (the real
    /// RemoveGroup's order), their records and places kept for the way back.
    /// The list a group's entry lives in: a trench's or a bridge's.
    fn entryList(self: *FakeBridge, group: *const FakeBridgeGroup) *std.ArrayListUnmanaged(FakeBridgeEntry) {
        return if (group.trench) &self.trench_entries else &self.bridge_entries;
    }

    fn removeBridgeGroup(self: *FakeBridge, group: *FakeBridgeGroup) Status {
        const list = self.entryList(group);
        if (!group.entryless) {
            if (group.entry_index >= list.items.len) return .failed;
            const held = list.items[group.entry_index];
            if (!std.mem.eql(i32, held.linkSlice(), group.entry.linkSlice())) return .failed;
            group.entry = held;
        }
        const entry = group.entry;
        var count: usize = 0;
        for (entry.linkSlice()) |link| {
            const place = self.indexOf(link) orelse continue;
            group.places[count] = place;
            count += 1;
        }
        std.mem.sort(usize, group.places[0..count], {}, std.sort.asc(usize));
        for (group.places[0..count], 0..) |place, at| group.spans[at] = self.objects_list.items[place];
        group.count = count;
        if (!group.entryless) _ = list.orderedRemove(group.entry_index);
        var at = count;
        while (at != 0) {
            at -= 1;
            _ = self.objects_list.orderedRemove(group.places[at]);
        }
        for (group.spans[0..count]) |span| self.link_floor = @max(self.link_floor, span.link_id + 1);
        return .ok;
    }

    /// The spans back at their places, ascending, then the entry at its index.
    fn addBridgeGroup(self: *FakeBridge, group: *const FakeBridgeGroup) Status {
        const list = self.entryList(group);
        if (!group.entryless and group.entry_index > list.items.len) return .failed;
        for (group.spans[0..group.count]) |span| {
            if (self.indexOf(span.link_id) != null) {
                self.say("a span's link ID is in use again", .{});
                return .refused;
            }
        }
        self.objects_list.ensureUnusedCapacity(self.allocator, group.count) catch return .failed;
        if (!group.entryless) list.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        for (group.spans[0..group.count], group.places[0..group.count]) |span, place| {
            self.objects_list.insertAssumeCapacity(@min(place, self.objects_list.items.len), span);
            self.link_floor = @max(self.link_floor, span.link_id + 1);
        }
        if (!group.entryless) list.insertAssumeCapacity(group.entry_index, group.entry);
        return .ok;
    }

    fn lastMessage(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        return self.message_buffer[0..self.message_len];
    }

    fn openMap(ptr: *anyopaque, path: []const u8, info: *MapInfo) Status {
        const self = from(ptr);
        self.message_len = 0;
        self.record(.open, 0);
        if (std.mem.eql(u8, path, "missing.bzm")) {
            self.say("no such map", .{});
            return .data_missing;
        }
        if (self.fail_build) {
            self.say("the engine threw", .{});
            return .failed;
        }
        // The renderer comes up at its own defaults and the AI's groups are
        // gone with the map before (see layer_bits).
        self.layer_bits = layers_mod.default_bits;
        self.ghost = null;
        self.fire_mode = 0;
        self.fire_filter_len = 0;
        self.fire_link_ids.clearRetainingCapacity();
        // What the real bridge forgets on an open: every tombstone and every
        // paint token of the map before.
        self.forgetHistory();
        self.hidden_script_ids.clearRetainingCapacity();
        self.script_file_at_open = self.script_file;
        self.camera_anchors_at_open = self.camera_anchors;
        self.forgetGroupsAtOpen();
        var group_entries = self.groups.iterator();
        while (group_entries.next()) |entry| {
            const owned = self.allocator.dupe(i32, entry.value_ptr.*) catch return .failed;
            self.groups_at_open.put(self.allocator, entry.key_ptr.*, owned) catch {
                self.allocator.free(owned);
                return .failed;
            };
        }
        self.unit_creation_at_open.clearRetainingCapacity();
        self.unit_creation_at_open.appendSlice(self.allocator, self.unit_creation_list.items) catch return .failed;
        self.script_areas_at_open.clearRetainingCapacity();
        self.script_areas_at_open.appendSlice(self.allocator, self.script_areas.items) catch return .failed;
        self.start_commands_at_open.clearRetainingCapacity();
        self.start_commands_at_open.appendSlice(self.allocator, self.start_commands.items) catch return .failed;
        self.reserve_positions_at_open.clearRetainingCapacity();
        self.reserve_positions_at_open.appendSlice(self.allocator, self.reserve_positions.items) catch return .failed;
        if (self.tiles.len == 0) {
            self.tiles = self.allocator.alloc(u8, @intCast(self.info.width_tiles * self.info.height_tiles)) catch return .failed;
            @memset(self.tiles, 0);
            self.diplomacy_table.resize(self.allocator, @intCast(self.info.player_count)) catch return .failed;
            for (self.diplomacy_table.items, 0..) |*side, player| side.* = @intCast(player % 2);
        }
        // One more vertex than tiles per axis, the real sheet's own shape
        // (M3, D-19); flat, until an edit says otherwise.
        if (self.altitudes_grid.len == 0) {
            const vertices_x = @as(usize, @intCast(self.info.width_tiles)) + 1;
            const vertices_y = @as(usize, @intCast(self.info.height_tiles)) + 1;
            self.altitudes_grid = self.allocator.alloc(f32, vertices_x * vertices_y) catch return .failed;
            @memset(self.altitudes_grid, 0);
        }
        self.link_floor = self.nextLinkId();
        info.* = self.info;
        return .ok;
    }

    fn saveMap(ptr: *anyopaque, path: []const u8) Status {
        const self = from(ptr);
        self.message_len = 0;
        self.record(.save, 0);
        if (path.len == 0) return .bad_argument;
        if (self.fail_save) {
            self.say("the disk is full", .{});
            return .failed;
        }
        if (self.files) |fake_files| {
            var os_buffer: [files_mod.max_path]u8 = undefined;
            const os_path = files_mod.osPathFromEngine(&os_buffer, path) orelse return .bad_argument;
            var content_buffer: [64]u8 = undefined;
            const content = std.fmt.bufPrint(&content_buffer, "fake map: {d} objects", .{self.objects_list.items.len}) catch return .failed;
            fake_files.write(os_path, content) catch return .failed;
        }
        return .ok;
    }

    fn objects(ptr: *anyopaque, out: []ObjectRecord, total: *usize) Status {
        const self = from(ptr);
        if (self.fail_objects) {
            self.say("the object listing failed", .{});
            return .failed;
        }
        total.* = self.objects_list.items.len;
        const count = @min(out.len, self.objects_list.items.len);
        @memcpy(out[0..count], self.objects_list.items[0..count]);
        return if (out.len >= self.objects_list.items.len) .ok else .refused;
    }

    fn diplomacy(ptr: *anyopaque, player: i32, value: *i32) Status {
        const self = from(ptr);
        if (player < 0 or @as(usize, @intCast(player)) >= self.diplomacy_table.items.len) return .bad_argument;
        value.* = self.diplomacy_table.items[@intCast(player)];
        return .ok;
    }

    fn addObject(ptr: *anyopaque, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (name.len == 0) return .bad_argument;
        if (!self.onMapAt(x, y)) {
            self.say("the engine would not place the object there", .{});
            return .refused;
        }
        var object: ObjectRecord = .{ .link_id = self.nextLinkId(), .x = x, .y = y, .dir = dir, .player = player };
        object.setName(name);
        self.objects_list.append(self.allocator, object) catch return .failed;
        self.link_floor = object.link_id + 1;
        link_id.* = object.link_id;
        self.record(.add, object.link_id);
        return .ok;
    }

    fn placeObject(ptr: *anyopaque, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const index = self.indexOf(link_id) orelse return .bad_argument;
        const object = &self.objects_list.items[index];
        if (self.shared(link_id)) return .refused;
        if (!object.known) {
            self.say("the object database does not know this object's type; it is kept as it is", .{});
            return .refused;
        }
        if (!self.onMapAt(x, y)) {
            self.say("the engine would not take that placement for the object", .{});
            return .refused;
        }
        object.x = x;
        object.y = y;
        object.dir = dir;
        object.player = player;
        self.record(.place, link_id);
        return .ok;
    }

    /// The real bridge's rules: -1..32000, a known link ID that is not 0 and
    /// not shared. Both refusals name why.
    fn setObjectScriptID(ptr: *anyopaque, link_id: i32, script_id: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (script_id < -1 or script_id > 32000) {
            self.say("a script ID is -1 (none) or 0..32000", .{});
            return .refused;
        }
        if (link_id == 0) {
            self.say("an object with link ID 0 has no link ID to name it by, so its script ID is kept as it is", .{});
            return .refused;
        }
        const index = self.indexOf(link_id) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        if (!self.objects_list.items[index].known) {
            self.say("the object database does not know this object's type; it is kept as it is", .{});
            return .refused;
        }
        if (self.shared(link_id)) return .refused;
        self.objects_list.items[index].script_id = script_id;
        self.record(.script_id, link_id);
        return .ok;
    }

    fn deleteObject(ptr: *anyopaque, link_id: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const status = self.deleteWithPassengers(link_id, 0);
        if (status == .ok) self.record(.delete, link_id);
        return status;
    }

    /// The real session's delete (DeleteObjectInChain): the host's passengers -
    /// the records whose link_with names it - are deleted first, each through
    /// this same path, and kept in the host's tombstone; the host's own refusals
    /// (a bridge span, a trench piece, a shared link ID) come after, and a
    /// refusal anywhere puts the passengers already taken back, so the delete is
    /// all or nothing. A link cycle is refused, as the real one is.
    fn deleteWithPassengers(self: *FakeBridge, link_id: i32, depth: usize) Status {
        _ = self.indexOf(link_id) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        if (depth > self.objects_list.items.len) {
            self.say("objects link to each other in a cycle; unlink one of them first", .{});
            return .refused;
        }
        if (self.shared(link_id)) return .refused;
        var passengers: std.ArrayListUnmanaged(Tombstone) = .empty;
        if (link_id != 0) {
            var ids: std.ArrayListUnmanaged(i32) = .empty;
            defer ids.deinit(self.allocator);
            for (self.objects_list.items) |other| {
                if (other.link_with == link_id and other.link_id != link_id)
                    ids.append(self.allocator, other.link_id) catch return .failed;
            }
            passengers.ensureTotalCapacity(self.allocator, ids.items.len) catch return .failed;
            for (ids.items) |passenger_id| {
                const status = self.deleteWithPassengers(passenger_id, depth + 1);
                if (status != .ok) {
                    self.putPassengersBack(&passengers);
                    return status;
                }
                const taken = self.tombstones.fetchRemove(passenger_id) orelse unreachable;
                passengers.appendAssumeCapacity(taken.value);
            }
        }
        const status = self.deleteRecord(link_id, &passengers);
        if (status != .ok) self.putPassengersBack(&passengers);
        return status;
    }

    /// Restores the passengers a refused host delete had taken, last deleted
    /// first, and empties the list. The message is left as the refusal set it.
    fn putPassengersBack(self: *FakeBridge, passengers: *std.ArrayListUnmanaged(Tombstone)) void {
        while (passengers.pop()) |passenger| {
            var taken = passenger;
            const id = taken.record.link_id;
            self.tombstones.put(self.allocator, id, taken) catch {
                self.freeTombstone(&taken);
                continue;
            };
            _ = self.restoreWithPassengers(id);
        }
        passengers.deinit(self.allocator);
    }

    /// The host's own delete, its passengers already gone: on success the
    /// passengers move into its tombstone, on a refusal the caller puts them back.
    fn deleteRecord(self: *FakeBridge, link_id: i32, passengers: *std.ArrayListUnmanaged(Tombstone)) Status {
        const index = self.indexOf(link_id) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        if (self.bridgeHolding(link_id)) |bridge_index| {
            self.say("still referred to by bridge {d}", .{bridge_index});
            return .refused;
        }
        if (self.trenchHolding(link_id)) |entrenchment_index| {
            self.say("still part of entrenchment {d}", .{entrenchment_index});
            return .refused;
        }
        // Everything that can fail comes first, so a failure leaves the map as it was.
        var changes: std.ArrayListUnmanaged(StartChange) = .empty;
        errdefer changes.deinit(self.allocator);
        var reserves: std.ArrayListUnmanaged(ReserveChange) = .empty;
        errdefer reserves.deinit(self.allocator);
        changes.ensureTotalCapacity(self.allocator, self.start_commands.items.len) catch return .failed;
        reserves.ensureTotalCapacity(self.allocator, self.reserve_positions.items.len) catch return .failed;
        self.tombstones.ensureUnusedCapacity(self.allocator, 1) catch return .failed;

        const removed = self.objects_list.orderedRemove(index);
        // Link ID 0 is "none": a record listing 0 is not naming this object.
        if (link_id != 0) {
            var position: usize = 0;
            while (position < self.start_commands.items.len) {
                const command = &self.start_commands.items[position];
                const holds = command.holds(link_id);
                const targets = command.target == link_id;
                if (!holds and !targets) {
                    position += 1;
                    continue;
                }
                const before = command.*;
                if (holds) command.remove(link_id);
                const erased = holds and command.unit_count == 0;
                var cleared = false;
                if (!erased and targets) {
                    command.target = 0;
                    cleared = true;
                }
                changes.appendAssumeCapacity(.{ .position = position, .before = before, .unit_removed = holds, .erased = erased, .target_cleared = cleared });
                if (erased) _ = self.start_commands.orderedRemove(position) else position += 1;
            }
            position = 0;
            while (position < self.reserve_positions.items.len) {
                const reserve = self.reserve_positions.items[position];
                if (reserve.artillery != link_id and reserve.truck != link_id) {
                    position += 1;
                    continue;
                }
                reserves.appendAssumeCapacity(.{ .position = position, .before = reserve });
                _ = self.reserve_positions.orderedRemove(position);
            }
        }
        self.tombstones.putAssumeCapacity(link_id, .{ .record = removed, .index = index, .changes = changes, .reserves = reserves, .passengers = passengers.* });
        self.link_floor = @max(self.link_floor, link_id + 1);
        self.describeCascade(changes.items, reserves.items);
        return .ok;
    }

    fn restoreObject(ptr: *anyopaque, link_id: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const status = self.restoreWithPassengers(link_id);
        if (status == .ok) self.record(.restore, link_id);
        return status;
    }

    /// The object back, then its passengers from their own tombstones, last
    /// deleted first (their links point at it); the real RestoreObjectInSession.
    fn restoreWithPassengers(self: *FakeBridge, link_id: i32) Status {
        const tombstone = self.tombstones.getPtr(link_id) orelse {
            self.say("no deleted object has that link ID", .{});
            return .refused;
        };
        if (self.indexOf(link_id) != null) {
            self.say("the link ID is in use again", .{});
            return .refused;
        }
        self.objects_list.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        self.start_commands.ensureUnusedCapacity(self.allocator, tombstone.changes.items.len) catch return .failed;
        self.reserve_positions.ensureUnusedCapacity(self.allocator, tombstone.reserves.items.len) catch return .failed;
        // Reverse order of application: a later erase shifted the positions after it.
        var back = tombstone.reserves.items.len;
        while (back != 0) {
            back -= 1;
            const reserve = tombstone.reserves.items[back];
            self.reserve_positions.insertAssumeCapacity(@min(reserve.position, self.reserve_positions.items.len), reserve.before);
        }
        back = tombstone.changes.items.len;
        while (back != 0) {
            back -= 1;
            const change = tombstone.changes.items[back];
            if (change.erased) {
                self.start_commands.insertAssumeCapacity(@min(change.position, self.start_commands.items.len), change.before);
            } else {
                self.start_commands.items[change.position] = change.before;
            }
        }
        const index = @min(tombstone.index, self.objects_list.items.len);
        self.objects_list.insertAssumeCapacity(index, tombstone.record);
        tombstone.changes.deinit(self.allocator);
        tombstone.reserves.deinit(self.allocator);
        var passengers = tombstone.passengers;
        _ = self.tombstones.remove(link_id);
        var all_back = true;
        while (passengers.pop()) |passenger| {
            var taken = passenger;
            const passenger_id = taken.record.link_id;
            self.tombstones.put(self.allocator, passenger_id, taken) catch {
                self.freeTombstone(&taken);
                all_back = false;
                continue;
            };
            if (self.restoreWithPassengers(passenger_id) != .ok) all_back = false;
        }
        passengers.deinit(self.allocator);
        if (!all_back) {
            self.say("a passenger would not come back with the host", .{});
            return .refused;
        }
        return .ok;
    }

    fn setDiplomacy(ptr: *anyopaque, player: i32, value: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (player < 0 or @as(usize, @intCast(player)) >= self.diplomacy_table.items.len) return .bad_argument;
        if (value < 0 or value > 2) {
            self.say("{d} is no diplomacy: 0 and 1 are the two sides, 2 is neutral", .{value});
            return .bad_argument;
        }
        self.diplomacy_table.items[@intCast(player)] = value;
        self.record(.diplomacy, player);
        return .ok;
    }

    fn setMapType(ptr: *anyopaque, value: i32) Status {
        const self = from(ptr);
        self.info.map_type = value;
        self.record(.map_type, value);
        return .ok;
    }

    fn setAttackingSide(ptr: *anyopaque, value: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (value < 0 or value > 1) {
            self.say("{d} is no side: the attacking side is 0 or 1", .{value});
            return .bad_argument;
        }
        self.info.attacking_side = value;
        self.record(.attacking_side, value);
        return .ok;
    }

    fn sounds(ptr: *anyopaque, out: []SoundRecord, total: *usize) Status {
        const self = from(ptr);
        total.* = self.sounds_list.items.len;
        const count = @min(out.len, self.sounds_list.items.len);
        @memcpy(out[0..count], self.sounds_list.items[0..count]);
        return if (out.len >= self.sounds_list.items.len) .ok else .refused;
    }

    /// The real bridge's rules the core can see: a non-empty name (the real
    /// one additionally checks the object database knows it as a sound,
    /// which this fake has no database for - any non-empty name is accepted,
    /// the same simplification `addObject`'s own doc comment above makes for
    /// objects), a position on the map (`tile_size` standing in for
    /// `fWorldCellSize`, `onMap`'s own reasoning), and non-negative times and
    /// radii with min <= max.
    fn validSound(self: *FakeBridge, sound: SoundRecord) Status {
        if (sound.nameSlice().len == 0) return .bad_argument;
        if (!self.onMap(sound.x, sound.y)) {
            self.say("that position is not on the map", .{});
            return .refused;
        }
        if (sound.repeat_ms < 0 or sound.repeat_random_ms < 0 or sound.min_radius < 0 or sound.max_radius < 0 or sound.min_radius > sound.max_radius) {
            self.say("a sound's times and radii must not be negative, and its minimum radius must not be above its maximum", .{});
            return .refused;
        }
        return .ok;
    }

    fn addSound(ptr: *anyopaque, index: i32, new_record: SoundRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        const valid = self.validSound(new_record);
        if (valid != .ok) return valid;
        const count: i32 = @intCast(self.sounds_list.items.len);
        if (index < -1 or index > count) return .bad_argument;
        const at: usize = if (index < 0) self.sounds_list.items.len else @intCast(index);
        self.sounds_list.insert(self.allocator, at, new_record) catch return .failed;
        self.record(.sound_add, @intCast(at));
        return .ok;
    }

    fn setSound(ptr: *anyopaque, index: i32, new_record: SoundRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (index < 0 or index >= @as(i32, @intCast(self.sounds_list.items.len))) return .bad_argument;
        const valid = self.validSound(new_record);
        if (valid != .ok) return valid;
        self.sounds_list.items[@intCast(index)] = new_record;
        self.record(.sound_edit, index);
        return .ok;
    }

    fn deleteSound(ptr: *anyopaque, index: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (index < 0 or index >= @as(i32, @intCast(self.sounds_list.items.len))) return .bad_argument;
        _ = self.sounds_list.orderedRemove(@intCast(index));
        self.record(.sound_delete, index);
        return .ok;
    }

    fn readRecord(ptr: *anyopaque, kind: records.Kind, key: i32, allocator: std.mem.Allocator, out: *records.Value) Status {
        const self = from(ptr);
        self.message_len = 0;
        switch (kind) {
            .camera_anchors => {
                if (key != 0) return .bad_argument;
                out.* = .{ .camera_anchors = self.camera_anchors };
            },
            .group => {
                const ids = self.groups.get(key) orelse {
                    self.say("there is no reinforcement group {d}", .{key});
                    return .refused;
                };
                out.* = .{ .group = .{ .id = key, .ids = allocator.dupe(i32, ids) catch return .failed } };
            },
            .script_file => {
                if (key != 0) return .bad_argument;
                out.* = .{ .script_file = self.script_file };
            },
            .script_area => {
                if (key < 0 or key >= self.script_areas.items.len) return .bad_argument;
                out.* = .{ .script_area = self.script_areas.items[@intCast(key)] };
            },
            .start_command => {
                if (key < 0 or key >= self.start_commands.items.len) return .bad_argument;
                out.* = .{ .start_command = self.start_commands.items[@intCast(key)].toRecord(allocator) catch return .failed };
            },
            .reserve_position => {
                if (key < 0 or key >= self.reserve_positions.items.len) return .bad_argument;
                out.* = .{ .reserve_position = self.reserve_positions.items[@intCast(key)] };
            },
            .unit_creation => {
                if (key < 0 or key >= records.max_uc_slots) return .bad_argument;
                var unit: records.UnitCreation = if (key < self.unit_creation_list.items.len) self.unit_creation_list.items[@intCast(key)] else records.UnitCreation.defaults();
                unit.slot_count = @intCast(self.unit_creation_list.items.len);
                out.* = .{ .unit_creation = unit };
            },
            .ai_side => {
                if (key < 0 or key >= records.max_ai_sides) return .bad_argument;
                const count: u32 = @intCast(self.ai_sides.items.len);
                var side: records.AiSide = if (key < count) self.ai_sides.items[@intCast(key)].clone(allocator) catch return .failed else .{};
                side.side = key;
                side.side_count = count;
                out.* = .{ .ai_side = side };
            },
        }
        return .ok;
    }

    /// The real bridge's ValidateAISide: what a put ADDS is judged - a parcel type 1 or
    /// 2, a radius above 0, a centre on the map, finite numbers and mobile script IDs
    /// 0..32000 that appear once; a parcel or ID the side holds now is exempt. (The real
    /// bridge also exempts what the file held when it was opened; the fake keeps no such
    /// copy.) Says why when it is not.
    fn aiSideAllowed(self: *FakeBridge, current: records.AiSide, wanted: records.AiSide) bool {
        for (wanted.parcels, 0..) |parcel, index| {
            var held = false;
            for (current.parcels) |existing| {
                if (existing.eql(parcel)) held = true;
            }
            if (held) continue;
            if (parcel.kind != .defence and parcel.kind != .reinforce) {
                self.say("parcel {d} is of type {d}: a parcel is a defence (1) or a reinforce (2) parcel", .{ index, @intFromEnum(parcel.kind) });
                return false;
            }
            if (!std.math.isFinite(parcel.radius) or !(parcel.radius > 0)) {
                self.say("parcel {d} needs a radius above 0", .{index});
                return false;
            }
            if (!std.math.isFinite(parcel.cx) or !std.math.isFinite(parcel.cy) or !self.onMapAt(parcel.cx, parcel.cy)) {
                self.say("parcel {d} is not on the map", .{index});
                return false;
            }
            for (parcel.points, 0..) |point, point_index| {
                if (!std.math.isFinite(point.x) or !std.math.isFinite(point.y)) {
                    self.say("point {d} of parcel {d} is not a number", .{ point_index, index });
                    return false;
                }
            }
        }
        for (wanted.mobile_ids) |script_id| {
            const in_wanted = std.mem.count(i32, wanted.mobile_ids, &.{script_id});
            const in_current = std.mem.count(i32, current.mobile_ids, &.{script_id});
            if (in_wanted <= in_current) continue;
            if (script_id < records.min_script_id or script_id > records.max_script_id) {
                self.say("a mobile script ID is 0..32000", .{});
                return false;
            }
            if (in_wanted > 1) {
                self.say("script ID {d} appears twice in the mobile list", .{script_id});
                return false;
            }
        }
        return true;
    }

    /// The real bridge's put of a side with the side count: resize the list (the sides
    /// below a new one come out empty, a smaller count drops the sides above it), then
    /// set the side when it is below the count.
    fn putAiSide(self: *FakeBridge, key: i32, wanted: records.AiSide) Status {
        if (key < 0 or key >= records.max_ai_sides or wanted.side != key) return .bad_argument;
        if (wanted.side_count > records.max_ai_sides) return .bad_argument;
        const count: usize = wanted.side_count;
        if (key >= count and (wanted.parcels.len != 0 or wanted.mobile_ids.len != 0)) {
            self.say("side {d} is not one of the map's {d} sides, so it holds nothing", .{ key, count });
            return .refused;
        }
        // A smaller count drops only empty sides (an undo takes back the sides a
        // put created, which are empty): one above it that holds anything,
        // other than `key`, is refused (WR-A03).
        for (self.ai_sides.items, 0..) |side, index| {
            if (index < count or index == @as(usize, @intCast(key))) continue;
            if (side.parcels.len != 0 or side.mobile_ids.len != 0) {
                self.say("side {d} holds parcels or script IDs, so the side count cannot drop to {d}", .{ index, count });
                return .refused;
            }
        }
        var current: records.AiSide = .{};
        if (key < self.ai_sides.items.len) current = self.ai_sides.items[@intCast(key)];
        if (key < count and !self.aiSideAllowed(current, wanted)) return .refused;
        var stored: records.AiSide = .{};
        if (key < count) stored = wanted.clone(self.allocator) catch return .failed;
        stored.side = key;
        stored.side_count = 0;
        // Room first, so once the list changes nothing can fail.
        self.ai_sides.ensureTotalCapacity(self.allocator, count) catch {
            stored.deinit(self.allocator);
            return .failed;
        };
        while (self.ai_sides.items.len > count) {
            var dropped = self.ai_sides.pop().?;
            dropped.deinit(self.allocator);
        }
        while (self.ai_sides.items.len < count) {
            self.ai_sides.appendAssumeCapacity(.{ .side = @intCast(self.ai_sides.items.len) });
        }
        if (key < count) {
            self.ai_sides.items[@intCast(key)].deinit(self.allocator);
            self.ai_sides.items[@intCast(key)] = stored;
        }
        self.record(.record_put, key);
        return .ok;
    }

    fn forgetGroupsAtOpen(self: *FakeBridge) void {
        var values = self.groups_at_open.valueIterator();
        while (values.next()) |ids| self.allocator.free(ids.*);
        self.groups_at_open.clearRetainingCapacity();
    }

    /// The real bridge's group rules: every ID a put adds is 0..32000 and
    /// appears once; an ID the group holds now, or held when the map was
    /// opened (so an undo can put odd data back after an edit took it out), is
    /// exempt. Says why when it is not.
    fn groupPutAllowed(self: *FakeBridge, id: i32, current: []const i32, wanted: []const i32) bool {
        const opened: []const i32 = self.groups_at_open.get(id) orelse &.{};
        for (wanted) |script_id| {
            const in_wanted = std.mem.count(i32, wanted, &.{script_id});
            const in_current = @max(std.mem.count(i32, current, &.{script_id}), std.mem.count(i32, opened, &.{script_id}));
            if (in_wanted <= in_current) continue;
            if (script_id < records.min_script_id or script_id > records.max_script_id) {
                self.say("a script ID in a group is 0..32000", .{});
                return false;
            }
            if (in_wanted > 1) {
                self.say("script ID {d} appears twice in the group", .{script_id});
                return false;
            }
        }
        return true;
    }

    /// Creates or replaces a group with `wanted` (owned copy made here).
    fn storeGroup(self: *FakeBridge, id: i32, wanted: []const i32) Status {
        const owned = self.allocator.dupe(i32, wanted) catch return .failed;
        const previous = self.groups.fetchPut(self.allocator, id, owned) catch {
            self.allocator.free(owned);
            return .failed;
        };
        if (previous) |entry| self.allocator.free(entry.value);
        return .ok;
    }

    fn recordKeys(ptr: *anyopaque, kind: records.Kind, allocator: std.mem.Allocator, out: *[]i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        switch (kind) {
            .camera_anchors, .script_file => {
                const keys = allocator.alloc(i32, 1) catch return .failed;
                keys[0] = 0;
                out.* = keys;
            },
            .script_area => {
                const keys = allocator.alloc(i32, self.script_areas.items.len) catch return .failed;
                for (keys, 0..) |*key_slot, index| key_slot.* = @intCast(index);
                out.* = keys;
            },
            .start_command => {
                const keys = allocator.alloc(i32, self.start_commands.items.len) catch return .failed;
                for (keys, 0..) |*key_slot, index| key_slot.* = @intCast(index);
                out.* = keys;
            },
            .reserve_position => {
                const keys = allocator.alloc(i32, self.reserve_positions.items.len) catch return .failed;
                for (keys, 0..) |*key_slot, index| key_slot.* = @intCast(index);
                out.* = keys;
            },
            .unit_creation => return .bad_argument,
            .ai_side => {
                const keys = allocator.alloc(i32, self.ai_sides.items.len) catch return .failed;
                for (keys, 0..) |*key_slot, index| key_slot.* = @intCast(index);
                out.* = keys;
            },
            .group => {
                const keys = allocator.alloc(i32, self.groups.count()) catch return .failed;
                var index: usize = 0;
                var it = self.groups.keyIterator();
                while (it.next()) |key| : (index += 1) keys[index] = key.*;
                std.mem.sort(i32, keys, {}, std.sort.asc(i32));
                out.* = keys;
            },
        }
        return .ok;
    }

    fn insertRecord(ptr: *anyopaque, key: i32, value: *const records.Value) Status {
        const self = from(ptr);
        self.message_len = 0;
        switch (value.*) {
            .camera_anchors, .script_file, .ai_side, .unit_creation => return .bad_argument,
            .script_area => |area| {
                if (key < 0 or key > self.script_areas.items.len) return .bad_argument;
                if (!self.areaPutAllowed(area, null)) return .refused;
                self.script_areas.insert(self.allocator, @intCast(key), area) catch return .failed;
                self.record(.record_put, key);
                return .ok;
            },
            .start_command => |command| {
                if (key < 0 or key > self.start_commands.items.len) return .bad_argument;
                const wanted = FakeStartCommand.fromRecord(command) orelse {
                    self.say("the fake holds at most {d} units per start command", .{max_command_units});
                    return .refused;
                };
                if (!self.startCommandAllowed(&wanted, null)) return .refused;
                self.start_commands.insert(self.allocator, @intCast(key), wanted) catch return .failed;
                self.warnHeldUnit(&wanted);
                self.record(.record_put, key);
                return .ok;
            },
            .reserve_position => |position| {
                if (key < 0 or key > self.reserve_positions.items.len) return .bad_argument;
                if (!self.reservePositionAllowed(&position, null)) return .refused;
                self.reserve_positions.insert(self.allocator, @intCast(key), position) catch return .failed;
                self.record(.record_put, key);
                return .ok;
            },
            .group => |group| {
                if (key < 0 or group.id != key) return .bad_argument;
                if (self.groups.contains(key)) {
                    self.say("there is already a reinforcement group {d}", .{key});
                    return .refused;
                }
                if (!self.groupPutAllowed(key, &.{}, group.ids)) return .refused;
                const stored = self.storeGroup(key, group.ids);
                if (stored == .ok) self.record(.record_put, key);
                return stored;
            },
        }
    }

    fn removeRecord(ptr: *anyopaque, kind: records.Kind, key: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        switch (kind) {
            .camera_anchors, .script_file, .ai_side, .unit_creation => return .bad_argument,
            .script_area => {
                if (key < 0 or key >= self.script_areas.items.len) return .bad_argument;
                _ = self.script_areas.orderedRemove(@intCast(key));
                self.record(.record_put, key);
                return .ok;
            },
            .start_command => {
                if (key < 0 or key >= self.start_commands.items.len) return .bad_argument;
                _ = self.start_commands.orderedRemove(@intCast(key));
                self.record(.record_put, key);
                return .ok;
            },
            .reserve_position => {
                if (key < 0 or key >= self.reserve_positions.items.len) return .bad_argument;
                _ = self.reserve_positions.orderedRemove(@intCast(key));
                self.record(.record_put, key);
                return .ok;
            },
            .group => {
                const removed = self.groups.fetchRemove(key) orelse {
                    self.say("there is no reinforcement group {d}", .{key});
                    return .refused;
                };
                self.allocator.free(removed.value);
                self.record(.record_put, key);
                return .ok;
            },
        }
    }

    fn firstFreeGroupID(ptr: *anyopaque, from_id: i32, out: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        var id: i32 = @max(from_id, 0);
        while (self.groups.contains(id)) id += 1;
        out.* = id;
        return .ok;
    }

    /// The real bridge's rules the core can see: the count 0..32 and finite
    /// coordinates are a caller bug; a slot this put changes that is not unset
    /// must be on the map (world units), or the put is refused naming it.
    fn putRecord(ptr: *anyopaque, key: i32, value: *const records.Value) Status {
        const self = from(ptr);
        self.message_len = 0;
        switch (value.*) {
            .camera_anchors => |wanted| {
                if (key != 0) return .bad_argument;
                if (wanted.player_count > records.max_camera_players) return .bad_argument;
                if (!finite(wanted.neutral)) return .bad_argument;
                for (wanted.players[0..wanted.player_count]) |slot| {
                    if (!finite(slot)) return .bad_argument;
                }
                const current = self.camera_anchors;
                const opened = self.camera_anchors_at_open;
                if (!wanted.neutral.eql(current.neutral) and !wanted.neutral.eql(opened.neutral) and !wanted.neutral.isUnset() and !self.onMap(wanted.neutral.x, wanted.neutral.y)) {
                    self.say("the neutral camera anchor is not on the map", .{});
                    return .refused;
                }
                for (wanted.players[0..wanted.player_count], 0..) |slot, index| {
                    if (slot.eql(current.slot(index)) or slot.eql(opened.slot(index)) or slot.isUnset()) continue;
                    if (!self.onMap(slot.x, slot.y)) {
                        self.say("the camera anchor of player {d} is not on the map", .{index});
                        return .refused;
                    }
                }
                // Exact: slots past the new count are dropped, as the real
                // put resizes the vector.
                var exact: records.CameraAnchors = .{};
                exact.neutral = wanted.neutral;
                exact.player_count = wanted.player_count;
                @memcpy(exact.players[0..wanted.player_count], wanted.players[0..wanted.player_count]);
                self.camera_anchors = exact;
                self.record(.record_put, key);
            },
            .group => |group| {
                if (key < 0 or group.id != key) return .bad_argument;
                const current: []const i32 = self.groups.get(key) orelse &.{};
                if (!self.groupPutAllowed(key, current, group.ids)) return .refused;
                const stored = self.storeGroup(key, group.ids);
                if (stored == .ok) self.record(.record_put, key);
                return stored;
            },
            .script_area => |wanted| {
                if (key < 0 or key >= self.script_areas.items.len) return .bad_argument;
                if (!self.areaPutAllowed(wanted, @intCast(key))) return .refused;
                self.script_areas.items[@intCast(key)] = wanted;
                self.record(.record_put, key);
            },
            .start_command => |command| {
                if (key < 0 or key >= self.start_commands.items.len) return .bad_argument;
                var wanted = FakeStartCommand.fromRecord(command) orelse {
                    self.say("the fake holds at most {d} units per start command", .{max_command_units});
                    return .refused;
                };
                const current = self.start_commands.items[@intCast(key)];
                // D-17: the explosion flag is the file's; a set never changes it.
                wanted.from_explosion = current.from_explosion;
                if (!self.startCommandAllowed(&wanted, &current)) return .refused;
                self.start_commands.items[@intCast(key)] = wanted;
                self.warnHeldUnit(&wanted);
                self.record(.record_put, key);
            },
            .reserve_position => |wanted| {
                if (key < 0 or key >= self.reserve_positions.items.len) return .bad_argument;
                const current = self.reserve_positions.items[@intCast(key)];
                if (!self.reservePositionAllowed(&wanted, &current)) return .refused;
                self.reserve_positions.items[@intCast(key)] = wanted;
                self.record(.record_put, key);
            },
            .ai_side => |wanted| return self.putAiSide(key, wanted),
            .unit_creation => |wanted| return self.putUnitCreation(key, wanted),
            .script_file => |wanted| {
                if (key != 0) return .bad_argument;
                // The real bridge's rule: None, a bare name, or the value the
                // file held when the map was opened.
                if (!wanted.eql(self.script_file_at_open) and !script_file_mod.isBareName(wanted.nameSlice())) {
                    self.say("a script is named without folder or .lua", .{});
                    return .refused;
                }
                self.script_file = wanted;
                self.record(.record_put, key);
            },
        }
        return .ok;
    }

    /// AI units from world units, truncating with Vis2AI's +0.3 (the real rule).
    fn visToAi(self: *const FakeBridge, vis: f32) f32 {
        return @floatFromInt(@as(i32, @intFromFloat(vis * self.map_per_world + 0.3)));
    }

    /// World units from AI units (AI2Vis).
    fn aiToVis(self: *const FakeBridge, ai: f32) f32 {
        return ai / self.map_per_world;
    }

    fn scriptAreaFromVis(ptr: *anyopaque, shape: records.AreaShape, wx0: f32, wy0: f32, wx1: f32, wy1: f32, name: []const u8, out: *records.ScriptArea) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (name.len >= records.area_name_capacity) return .bad_argument;
        for ([_]f32{ wx0, wy0, wx1, wy1 }) |value| if (!std.math.isFinite(value)) return .bad_argument;
        var area: records.ScriptArea = .{ .shape = shape };
        area.setName(name);
        switch (shape) {
            .rectangle => {
                area.cx = self.visToAi((wx0 + wx1) / 2);
                area.cy = self.visToAi((wy0 + wy1) / 2);
                area.hx = self.visToAi(@abs(wx0 - wx1) / 2);
                area.hy = self.visToAi(@abs(wy0 - wy1) / 2);
            },
            .circle => {
                area.cx = self.visToAi(wx0);
                area.cy = self.visToAi(wy0);
                area.r = self.visToAi(std.math.hypot(wx0 - wx1, wy0 - wy1));
            },
        }
        out.* = area;
        return .ok;
    }

    fn scriptAreaMoved(ptr: *anyopaque, area: records.ScriptArea, wx: f32, wy: f32, out: *records.ScriptArea) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (!std.math.isFinite(wx) or !std.math.isFinite(wy)) return .bad_argument;
        var moved = area;
        moved.cx = self.visToAi(wx);
        moved.cy = self.visToAi(wy);
        out.* = moved;
        return .ok;
    }

    fn scriptAreaResized(ptr: *anyopaque, area: records.ScriptArea, wx: f32, wy: f32, out: *records.ScriptArea) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (!std.math.isFinite(wx) or !std.math.isFinite(wy)) return .bad_argument;
        var resized = area;
        const centre_x = self.aiToVis(area.cx);
        const centre_y = self.aiToVis(area.cy);
        switch (area.shape) {
            .rectangle => {
                resized.hx = self.visToAi(@abs(wx - centre_x));
                resized.hy = self.visToAi(@abs(wy - centre_y));
            },
            .circle => resized.r = self.visToAi(std.math.hypot(wx - centre_x, wy - centre_y)),
        }
        out.* = resized;
        return .ok;
    }

    /// The real bridge's script-area rules: a name, unique among the areas but
    /// the one being replaced (`replacing`), a size that is not negative, a
    /// centre on the map when the put moves it; a name the map held twice when it
    /// was opened may be put back as often as it held it. Says why when not.
    fn areaPutAllowed(self: *FakeBridge, wanted: records.ScriptArea, replacing: ?usize) bool {
        // An area the map held at open, put back bit for bit (an undo), keeps
        // whatever it held: only the name rule applies to it (WR-A04).
        var opened_exact = false;
        for (self.script_areas_at_open.items) |existing| {
            const same_bits = std.mem.eql(u8, existing.nameSlice(), wanted.nameSlice()) and existing.shape == wanted.shape and
                @as(u32, @bitCast(existing.cx)) == @as(u32, @bitCast(wanted.cx)) and @as(u32, @bitCast(existing.cy)) == @as(u32, @bitCast(wanted.cy)) and
                @as(u32, @bitCast(existing.hx)) == @as(u32, @bitCast(wanted.hx)) and @as(u32, @bitCast(existing.hy)) == @as(u32, @bitCast(wanted.hy)) and
                @as(u32, @bitCast(existing.r)) == @as(u32, @bitCast(wanted.r));
            if (same_bits) opened_exact = true;
        }
        if (wanted.nameSlice().len == 0 and !opened_exact) {
            self.say("an area needs a name", .{});
            return false;
        }
        if (!opened_exact and (wanted.hx < 0 or wanted.hy < 0 or wanted.r < 0)) {
            self.say("an area's size is not negative", .{});
            return false;
        }
        var others: usize = 0;
        for (self.script_areas.items, 0..) |existing, index| {
            if (replacing != null and replacing.? == index) continue;
            if (std.mem.eql(u8, existing.nameSlice(), wanted.nameSlice())) others += 1;
        }
        if (others > 0) {
            var opened: usize = 0;
            for (self.script_areas_at_open.items) |existing| {
                if (std.mem.eql(u8, existing.nameSlice(), wanted.nameSlice())) opened += 1;
            }
            if (others + 1 > opened) {
                self.say("an area named {s} exists", .{wanted.nameSlice()});
                return false;
            }
        }
        const moved = if (replacing) |index| (self.script_areas.items[index].cx != wanted.cx or self.script_areas.items[index].cy != wanted.cy) else true;
        if (moved and !opened_exact and !self.onMapAt(wanted.cx, wanted.cy)) {
            self.say("the area's centre is not on the map", .{});
            return false;
        }
        return true;
    }

    fn roleOf(self: *const FakeBridge, name: []const u8) ?*const FakeRole {
        for (self.roles.items) |*entry| {
            if (std.mem.eql(u8, entry.nameSlice(), name)) return entry;
        }
        return null;
    }

    fn reserveRole(ptr: *anyopaque, name: []const u8, role: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        role.* = if (self.roleOf(name)) |entry| @intFromEnum(entry.role) else 0;
        return .ok;
    }

    fn isSquad(self: *const FakeBridge, link_id: i32) bool {
        return std.mem.indexOfScalar(i32, self.squads.items, link_id) != null;
    }

    /// True, with the reason said, when the object cannot be a gun (`gun`) or a
    /// truck of a reserve position; else `entry` is its role.
    fn refusesReserveObject(self: *FakeBridge, link_id: i32, gun: bool, entry: *?*const FakeRole) bool {
        const who: []const u8 = if (gun) "artillery" else "truck";
        const index = self.indexOf(link_id) orelse {
            self.say("no object has link ID {d}", .{link_id});
            return true;
        };
        const object = &self.objects_list.items[index];
        if (self.isSquad(link_id)) {
            self.say("object {d} ({s}) is a squad, and a squad cannot be {s}: a reserve position names a single unit", .{ link_id, object.nameSlice(), who });
            return true;
        }
        const found = self.roleOf(object.nameSlice());
        if (found == null or found.?.role == .none) {
            self.say("object {d} ({s}) is not a vehicle or a gun, so it cannot be {s}", .{ link_id, object.nameSlice(), who });
            return true;
        }
        entry.* = found;
        const role = found.?.role;
        if (gun and role != .self_propelled and role != .towed) {
            self.say("object {d} ({s}) is not artillery: a reserve position holds a self-propelled or a towed gun", .{ link_id, object.nameSlice() });
            return true;
        }
        if (!gun and role != .truck) {
            self.say("object {d} ({s}) is not a truck that can tow", .{ link_id, object.nameSlice() });
            return true;
        }
        return false;
    }

    /// The real bridge's ValidateReservePosition: what the put CHANGES is judged
    /// against `current` (null for an add), and a position the map held when it was
    /// opened is always accepted. Says why when it is not.
    fn reservePositionAllowed(self: *FakeBridge, wanted: *const FakeReservePosition, current: ?*const FakeReservePosition) bool {
        for (self.reserve_positions_at_open.items) |opened| {
            if (opened.eql(wanted.*)) return true;
        }
        const roles_changed = current == null or current.?.artillery != wanted.artillery or current.?.truck != wanted.truck;
        if (roles_changed) {
            if (wanted.artillery <= 0) {
                self.say("{s}", .{if (wanted.truck == 0) "a reserve position needs a gun" else "the gun's link ID is above 0: link ID 0 names no gun"});
                return false;
            }
            if (wanted.truck < 0) {
                self.say("a truck's link ID is above 0, or 0 for none", .{});
                return false;
            }
            var gun: ?*const FakeRole = null;
            if (self.refusesReserveObject(wanted.artillery, true, &gun)) return false;
            if (wanted.truck == 0) {
                if (gun.?.role == .towed) {
                    self.say("a towed gun needs a truck", .{});
                    return false;
                }
            } else {
                if (gun.?.role != .towed) {
                    self.say("a self-propelled gun takes no truck", .{});
                    return false;
                }
                var truck: ?*const FakeRole = null;
                if (self.refusesReserveObject(wanted.truck, false, &truck)) return false;
                if (!(truck.?.number > gun.?.number)) {
                    self.say("the truck {d} cannot tow the gun {d}: it pulls {d:.0} and the gun weighs {d:.0}", .{ wanted.truck, wanted.artillery, truck.?.number, gun.?.number });
                    return false;
                }
            }
        }
        if (!std.math.isFinite(wanted.x) or !std.math.isFinite(wanted.y)) return false;
        const moved = current == null or current.?.x != wanted.x or current.?.y != wanted.y;
        if (moved and !self.onMapAt(wanted.x, wanted.y)) {
            self.say("the reserve position is not on the map", .{});
            return false;
        }
        return true;
    }

    /// The built-in action types when a test seeds none: the first entries of
    /// the real Data/Editor/actions.ini.
    const default_actions = [_]struct { name: []const u8, id: i32 }{
        .{ .name = "MOVE_TO", .id = 0 },
        .{ .name = "ATTACK_UNIT", .id = 1 },
        .{ .name = "ATTACK_OBJECT", .id = 2 },
        .{ .name = "SWARM_TO", .id = 3 },
        .{ .name = "LOAD", .id = 4 },
        .{ .name = "UNLOAD", .id = 5 },
        .{ .name = "ENTER", .id = 6 },
        .{ .name = "LEAVE", .id = 7 },
        .{ .name = "ROTATE_TO", .id = 8 },
        .{ .name = "STOP", .id = 9 },
        .{ .name = "PARADE", .id = 10 },
    };

    /// Whether the action type is one the list offers; the list missing refuses,
    /// saying so.
    fn actionListed(self: *FakeBridge, id: i32) bool {
        if (self.no_action_list) {
            self.say("the action list Data\\Editor\\actions.ini is not in the data", .{});
            return false;
        }
        if (self.action_list.items.len == 0) {
            for (default_actions) |item| {
                if (item.id == id) return true;
            }
        } else {
            for (self.action_list.items) |item| {
                if (item.id == id) return true;
            }
        }
        self.say("{d} is not an action type Data\\Editor\\actions.ini lists", .{id});
        return false;
    }

    fn actionCommands(ptr: *anyopaque, allocator: std.mem.Allocator, out: *[]bridge_mod.ActionCommand, default_index: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (self.no_action_list) {
            self.say("the action list Data\\Editor\\actions.ini is not in the data", .{});
            return .refused;
        }
        const count = if (self.action_list.items.len == 0) default_actions.len else self.action_list.items.len;
        const list = allocator.alloc(bridge_mod.ActionCommand, count) catch return .failed;
        for (list, 0..) |*item, index| {
            if (self.action_list.items.len == 0) {
                item.* = .{ .id = default_actions[index].id };
                item.setName(default_actions[index].name);
            } else item.* = self.action_list.items[index];
        }
        out.* = list;
        default_index.* = if (count > 9) 9 else count - 1;
        return .ok;
    }

    fn objectFilters(ptr: *anyopaque, allocator: std.mem.Allocator, out: *[]bridge_mod.ObjectFilter) Status {
        const self = from(ptr);
        self.message_len = 0;
        // The merged read the real bridge answers: the shipped fixtures in
        // their order, a user entry of the same name replacing one in place
        // (with its user 1), user-only names appended in their save order.
        var list: std.ArrayListUnmanaged(bridge_mod.ObjectFilter) = .empty;
        for (default_filters) |shipped| {
            const merged = self.findUserFilter(shipped.nameSlice()) orelse shipped;
            list.append(allocator, merged) catch {
                list.deinit(allocator);
                return .failed;
            };
        }
        for (self.object_filters.items) |one| {
            var shipped = false;
            for (default_filters) |d| {
                if (std.mem.eql(u8, d.nameSlice(), one.nameSlice())) shipped = true;
            }
            if (!shipped) {
                list.append(allocator, one) catch {
                    list.deinit(allocator);
                    return .failed;
                };
            }
        }
        out.* = list.toOwnedSlice(allocator) catch return .failed;
        return .ok;
    }

    fn findUserFilter(self: *FakeBridge, name: []const u8) ?bridge_mod.ObjectFilter {
        for (self.object_filters.items) |one| {
            if (std.mem.eql(u8, one.nameSlice(), name)) return one;
        }
        return null;
    }

    fn saveObjectFilters(ptr: *anyopaque, filters: []const bridge_mod.ObjectFilter) Status {
        const self = from(ptr);
        self.message_len = 0;
        self.object_filters.clearRetainingCapacity();
        self.object_filters.appendSlice(self.allocator, filters) catch return .failed;
        return .ok;
    }

    fn applyField(ptr: *anyopaque, params: bridge_mod.FieldApplyParams, report: []bridge_mod.FieldObjectReport, total: *usize, token: *i32) Status {
        _ = report;
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        total.* = 0;
        if (self.field_apply_refused or params.point_count < 3) {
            self.say("the fields application is refused", .{});
            return .refused;
        }
        self.last_field_apply = params;
        // The simulated apply: one undoable edit of the log that changes
        // nothing a caller can see (the fake holds no terrain fills). The
        // documented simplification: the engine's tile, object and height
        // fills are the real bridge's business; the fake proves the command
        // shape - one token, the report path, the refusal path.
        var edit = FakeEdit{ .fields = .{} };
        if (self.field_adds_object) {
            const first = params.points.?[0];
            var object: ObjectRecord = .{ .link_id = self.nextLinkId(), .x = first.x, .y = first.y };
            object.setName("FieldTree");
            self.objects_list.append(self.allocator, object) catch return .failed;
            self.link_floor = object.link_id + 1;
            edit.fields.added = object;
        }
        self.edits.append(self.allocator, edit) catch return .failed;
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.append(self.allocator, token.*) catch return .failed;
        self.undone_edits.clearRetainingCapacity();
        return .ok;
    }

    fn fieldSetSeason(ptr: *anyopaque, name: [*:0]const u8, season: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        const wanted = std.mem.span(name);
        for (self.field_set_names) |known| {
            if (std.mem.eql(u8, known, wanted)) {
                season.* = self.field_set_season;
                return .ok;
            }
        }
        self.say("the field set \"{s}\" is not in the data", .{wanted});
        return .refused;
    }

    // Players and the unit creation (05-05, D-30): the real bridge's rules the core
    // can see - the 17-entry bound, the neutral kept last, the owners that follow,
    // MutableValidate's name and range checks - over fixed-size state.

    fn capturePlayers(self: *const FakeBridge) FakePlayersState {
        var state: FakePlayersState = .{};
        state.table_len = self.diplomacy_table.items.len;
        for (self.diplomacy_table.items, 0..) |side, index| {
            if (index < state.table.len) state.table[index] = side;
        }
        state.units_len = self.unit_creation_list.items.len;
        for (self.unit_creation_list.items, 0..) |unit, index| {
            if (index < state.units.len) state.units[index] = unit;
        }
        state.anchors = self.camera_anchors;
        return state;
    }

    /// The table, vector and anchors of `state` and the owners of the changed
    /// objects (the after-values when `after`).
    fn putPlayersState(self: *FakeBridge, state: *const FakePlayersState, owners: []const FakeOwnerChange, after: bool) Status {
        self.diplomacy_table.resize(self.allocator, state.table_len) catch return .failed;
        for (self.diplomacy_table.items, 0..) |*side, index| side.* = state.table[index];
        self.unit_creation_list.resize(self.allocator, state.units_len) catch return .failed;
        for (self.unit_creation_list.items, 0..) |*unit, index| unit.* = state.units[index];
        self.camera_anchors = state.anchors;
        for (owners) |change| {
            const index = self.indexOf(change.link_id) orelse return .failed;
            self.objects_list.items[index].player = if (after) change.after_player else change.before_player;
        }
        return .ok;
    }

    fn logPlayersEdit(self: *FakeBridge, edit: FakePlayersEdit, token: *i32) Status {
        self.edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        self.applied_edits.ensureUnusedCapacity(self.allocator, 1) catch return .failed;
        const put = self.putPlayersState(&edit.after, edit.owners.items, true);
        if (put != .ok) return put;
        self.edits.appendAssumeCapacity(.{ .players = edit });
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.appendAssumeCapacity(token.*);
        self.undone_edits.clearRetainingCapacity();
        self.record(.vso_edit, token.*);
        return .ok;
    }

    fn addPlayer(ptr: *anyopaque, side: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (side < 0 or side > 1) {
            self.say("{d} is no side for a player: 0 and 1 are the two sides", .{side});
            return .refused;
        }
        const len = self.diplomacy_table.items.len;
        if (len == 0 or len >= max_player_entries) {
            self.say("{s}", .{if (len == 0) "the map has no neutral player to add a player before" else "a map holds 16 players and the neutral"});
            return .refused;
        }
        const new_index = len - 1;
        var edit: FakePlayersEdit = .{ .before = self.capturePlayers(), .after = self.capturePlayers() };
        errdefer edit.owners.deinit(self.allocator);
        var after = &edit.after;
        var i = after.table_len;
        while (i > new_index) : (i -= 1) after.table[i] = after.table[i - 1];
        after.table[new_index] = side;
        after.table_len += 1;
        if (after.units_len >= new_index and after.units_len < records.max_uc_slots) {
            var u = after.units_len;
            while (u > new_index) : (u -= 1) after.units[u] = after.units[u - 1];
            after.units[new_index] = records.UnitCreation.defaults();
            after.units_len += 1;
        }
        if (after.anchors.player_count >= new_index and after.anchors.player_count < records.max_camera_players) {
            var a: usize = after.anchors.player_count;
            while (a > new_index) : (a -= 1) after.anchors.players[a] = after.anchors.players[a - 1];
            after.anchors.players[new_index] = .{};
            after.anchors.player_count += 1;
        }
        for (self.objects_list.items) |object| {
            if (object.player >= @as(i32, @intCast(new_index))) {
                edit.owners.append(self.allocator, .{ .link_id = object.link_id, .before_player = object.player, .after_player = object.player + 1 }) catch return .failed;
            }
        }
        return self.logPlayersEdit(edit, token);
    }

    fn deletePlayer(ptr: *anyopaque, player: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const len = self.diplomacy_table.items.len;
        if (player < 0 or player >= @as(i32, @intCast(len)) - 1) {
            if (len > 0 and player == @as(i32, @intCast(len)) - 1) {
                self.say("the neutral player cannot be deleted", .{});
            } else {
                self.say("{d} is no player: the map holds {d}", .{ player, if (len > 0) len - 1 else 0 });
            }
            return .refused;
        }
        if (len - 1 < 3) {
            self.say("a map keeps at least two players and the neutral", .{});
            return .refused;
        }
        const gone: usize = @intCast(player);
        const neutral: i32 = @as(i32, @intCast(len)) - 2;
        var edit: FakePlayersEdit = .{ .before = self.capturePlayers(), .after = self.capturePlayers() };
        errdefer edit.owners.deinit(self.allocator);
        var after = &edit.after;
        var i = gone;
        while (i + 1 < after.table_len) : (i += 1) after.table[i] = after.table[i + 1];
        after.table_len -= 1;
        if (after.units_len > gone) {
            var u = gone;
            while (u + 1 < after.units_len) : (u += 1) after.units[u] = after.units[u + 1];
            after.units_len -= 1;
        }
        if (after.anchors.player_count > gone) {
            var a = gone;
            while (a + 1 < after.anchors.player_count) : (a += 1) after.anchors.players[a] = after.anchors.players[a + 1];
            after.anchors.player_count -= 1;
            after.anchors.players[after.anchors.player_count] = .{};
        }
        for (self.objects_list.items) |object| {
            const moved: i32 = if (object.player == player) neutral else if (object.player > player) object.player - 1 else object.player;
            if (moved != object.player) {
                edit.owners.append(self.allocator, .{ .link_id = object.link_id, .before_player = object.player, .after_player = moved }) catch return .failed;
            }
        }
        return self.logPlayersEdit(edit, token);
    }

    fn ucNameKnown(name: []const u8, list: []const []const u8) bool {
        for (list) |known| {
            if (std.mem.eql(u8, known, name)) return true;
        }
        return false;
    }

    /// Whether the file held `name` as a party (0), an aircraft (1) or a paratroop
    /// squad (2) in any entry when the map was opened.
    fn ucOpenedHas(self: *const FakeBridge, kind: u8, name: []const u8) bool {
        for (self.unit_creation_at_open.items) |unit| {
            switch (kind) {
                0 => if (std.mem.eql(u8, unit.partySlice(), name)) return true,
                1 => for (unit.aircraft) |slot| {
                    if (std.mem.eql(u8, slot.nameSlice(), name)) return true;
                },
                else => if (std.mem.eql(u8, unit.paratroopSlice(), name)) return true,
            }
        }
        return false;
    }

    /// The real SetSessionUnitCreation's rules, as far as the fake can see them: a
    /// field that differs from the entry's own is held to its list or range; the
    /// refusal names the field. A name the entry holds now or any entry held when
    /// the map was opened is always accepted back (an undo of the file's own data).
    fn putUnitCreation(self: *FakeBridge, key: i32, wanted: records.UnitCreation) Status {
        if (key < 0 or key >= records.max_uc_slots or wanted.slot_count > records.max_uc_slots) return .bad_argument;
        const have = self.unit_creation_list.items.len;
        const players = @max(self.diplomacy_table.items.len -| 1, have);
        if (@as(usize, @intCast(key)) >= players) {
            self.say("{d} is no player: the map holds {d}", .{ key, players });
            return .refused;
        }
        const slots: usize = wanted.slot_count;
        if (slots > have) {
            self.unit_creation_list.ensureTotalCapacity(self.allocator, slots) catch return .failed;
        }
        if (slots > @as(usize, @intCast(key))) {
            const current = if (@as(usize, @intCast(key)) < have) self.unit_creation_list.items[@intCast(key)] else records.UnitCreation.defaults();
            if (!std.mem.eql(u8, wanted.partySlice(), current.partySlice()) and !ucNameKnown(wanted.partySlice(), self.uc_parties) and !self.ucOpenedHas(0, wanted.partySlice())) {
                self.say("the party \"{s}\" is not in partys.xml", .{wanted.partySlice()});
                return .refused;
            }
            for (wanted.aircraft, current.aircraft, 0..) |slot, held, index| {
                if (slot.nameSlice().len == 0 or (!std.mem.eql(u8, slot.nameSlice(), held.nameSlice()) and !ucNameKnown(slot.nameSlice(), self.uc_aircraft) and !self.ucOpenedHas(1, slot.nameSlice()))) {
                    self.say("{s}: \"{s}\" is no aircraft of the object database", .{ records.uc_aircraft_labels[index], slot.nameSlice() });
                    return .refused;
                }
                if (slot.formation_size != held.formation_size and (slot.formation_size < 1 or slot.formation_size > 32)) {
                    self.say("{s}: formation size {d} is outside 1..32", .{ records.uc_aircraft_labels[index], slot.formation_size });
                    return .refused;
                }
                if (slot.count != held.count and (slot.count < 0 or slot.count > 255)) {
                    self.say("{s}: count {d} is outside 0..255", .{ records.uc_aircraft_labels[index], slot.count });
                    return .refused;
                }
            }
            if (!std.mem.eql(u8, wanted.paratroopSlice(), current.paratroopSlice()) and !ucNameKnown(wanted.paratroopSlice(), self.uc_squads) and !self.ucOpenedHas(2, wanted.paratroopSlice())) {
                self.say("the paratroop squad \"{s}\" is no squad of the object database", .{wanted.paratroopSlice()});
                return .refused;
            }
            if (wanted.paratroop_count != current.paratroop_count and (wanted.paratroop_count < 0 or wanted.paratroop_count > 255)) {
                self.say("the paratroop squads count {d} is outside 0..255", .{wanted.paratroop_count});
                return .refused;
            }
            if (wanted.relax_time != current.relax_time and wanted.relax_time < 1) {
                self.say("the relax time {d} is below 1 second", .{wanted.relax_time});
                return .refused;
            }
            for (wanted.appearSlice()) |point| {
                var held = false;
                for (current.appearSlice()) |existing| {
                    if (existing.eql(point)) held = true;
                }
                if (!held and !self.onMapAt(point.x, point.y)) {
                    self.say("the appear point ({d:.1}, {d:.1}) is off the map", .{ point.x, point.y });
                    return .refused;
                }
            }
        }
        var stored = wanted;
        stored.slot_count = 0;
        self.unit_creation_list.resize(self.allocator, slots) catch return .failed;
        if (slots > have) {
            for (self.unit_creation_list.items[have..]) |*padded| padded.* = records.UnitCreation.defaults();
        }
        if (slots > @as(usize, @intCast(key))) self.unit_creation_list.items[@intCast(key)] = stored;
        self.record(.record_put, key);
        return .ok;
    }

    fn unitCreationChoices(ptr: *anyopaque, kind: bridge_mod.UcChoice, out: []bridge_mod.UcName, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        const list = switch (kind) {
            .parties => self.uc_parties,
            .aircraft => self.uc_aircraft,
            .squads => self.uc_squads,
        };
        total.* = list.len;
        if (out.len < list.len) return .refused;
        for (list, 0..) |name, i| {
            var entry = bridge_mod.UcName{};
            const len = @min(name.len, entry.name.len - 1);
            @memcpy(entry.name[0..len], name[0..len]);
            out[i] = entry;
        }
        return .ok;
    }

    /// One player's unit creation before the map opens (05-05): appended to the
    /// vector, so the slot count grows by one.
    pub fn addUnitCreationFixture(self: *FakeBridge, unit: records.UnitCreation) !void {
        var stored = unit;
        stored.slot_count = 0;
        try self.unit_creation_list.append(self.allocator, stored);
    }

    // --- The RMG composers (05-09) -------------------------------------

    /// A shipped container before the session: the fake owns it from here.
    pub fn addContainerFixture(self: *FakeBridge, name: []const u8, shipped: bool, value: rmg_mod.Container) !void {
        const owned = try self.allocator.dupe(u8, name);
        errdefer self.allocator.free(owned);
        try self.rmg_containers.append(self.allocator, .{ .name = owned, .shipped = shipped, .value = value });
    }

    pub fn addGraphFixture(self: *FakeBridge, name: []const u8, shipped: bool, value: rmg_mod.Graph) !void {
        const owned = try self.allocator.dupe(u8, name);
        errdefer self.allocator.free(owned);
        try self.rmg_graphs.append(self.allocator, .{ .name = owned, .shipped = shipped, .value = value });
    }

    pub fn addPatchFixture(self: *FakeBridge, name: []const u8, size: i32, season: i32, folder: []const u8, ids: []const i32, areas: []const []const u8) !void {
        const owned = try self.allocator.dupe(u8, name);
        errdefer self.allocator.free(owned);
        const folder_copy = try self.allocator.dupe(u8, folder);
        errdefer self.allocator.free(folder_copy);
        const ids_copy = try self.allocator.dupe(i32, ids);
        errdefer self.allocator.free(ids_copy);
        const areas_copy = try self.allocator.alloc([]u8, areas.len);
        errdefer self.allocator.free(areas_copy);
        var made: usize = 0;
        errdefer for (areas_copy[0..made]) |area| self.allocator.free(area);
        for (areas, 0..) |area, i| {
            areas_copy[i] = try self.allocator.dupe(u8, area);
            made += 1;
        }
        try self.rmg_patches.append(self.allocator, .{ .name = owned, .size_x = size, .size_y = size, .season = season, .folder = folder_copy, .ids = ids_copy, .areas = areas_copy });
    }

    /// The composer name the real bridge reads: lower case, backslashes, no
    /// ".xml", at most 191 bytes into `buffer`.
    fn rmgName(buffer: []u8, name: []const u8) []const u8 {
        const stripped = if (name.len > 4 and std.ascii.eqlIgnoreCase(name[name.len - 4 ..], ".xml")) name[0 .. name.len - 4] else name;
        const len = @min(stripped.len, buffer.len);
        for (stripped[0..len], 0..) |byte, i| buffer[i] = if (byte == '/') '\\' else std.ascii.toLower(byte);
        return buffer[0..len];
    }

    /// A plain relative name under `folder` with something after it - the
    /// real bridge's T-05-09-01 rule.
    fn rmgNameIsPlain(name: []const u8, folder: []const u8) bool {
        if (name.len >= 192 or name.len <= folder.len or !std.mem.startsWith(u8, name, folder)) return false;
        if (std.mem.indexOfAny(u8, name, ":<>\"|*?") != null) return false;
        var parts = std.mem.splitScalar(u8, name, '\\');
        while (parts.next()) |part| {
            if (part.len == 0 or std.mem.eql(u8, part, ".") or std.mem.eql(u8, part, "..")) return false;
        }
        return true;
    }

    fn putText(field: []u8, text: []const u8) void {
        @memset(field, 0);
        const len = @min(text.len, field.len - 1);
        @memcpy(field[0..len], text[0..len]);
    }

    fn putScripts(scripts: *bridge_mod.RmgScripts, ids: []const i32, areas: []const []const u8) bool {
        scripts.id_count = @intCast(ids.len);
        scripts.area_count = @intCast(areas.len);
        var short = false;
        const id_capacity: usize = @intCast(@max(scripts.id_capacity, 0));
        if (ids.len <= id_capacity) {
            if (scripts.ids) |dst| for (ids, 0..) |id, i| {
                dst[i] = id;
            };
        } else short = true;
        const area_capacity: usize = @intCast(@max(scripts.area_capacity, 0));
        if (areas.len <= area_capacity) {
            if (scripts.areas) |dst| for (areas, 0..) |area, i| {
                dst[i] = .{};
                putText(&dst[i].name, area);
            };
        } else short = true;
        return short;
    }

    fn rmgReadContainer(ptr: *anyopaque, name: [*:0]const u8, rec: *bridge_mod.RmgContainerRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        rec.patch_count = 0;
        rec.index_counts = .{ 0, 0, 0, 0 };
        rec.scripts.id_count = 0;
        rec.scripts.area_count = 0;
        var buffer: [192]u8 = undefined;
        const wanted = rmgName(&buffer, std.mem.span(name));
        if (!rmgNameIsPlain(wanted, "scenarios\\containers\\")) {
            self.say("\"{s}\" is not a name under scenarios\\containers\\", .{std.mem.span(name)});
            return .refused;
        }
        for (self.rmg_containers.items) |file| {
            if (!std.mem.eql(u8, file.name, wanted)) continue;
            const c = &file.value;
            rec.size_x = c.size_x;
            rec.size_y = c.size_y;
            rec.season = c.season;
            putText(&rec.season_folder, c.season_folder);
            var short = false;
            rec.patch_count = @intCast(c.patches.items.len);
            if (c.patches.items.len <= @as(usize, @intCast(@max(rec.patch_capacity, 0)))) {
                if (rec.patches) |dst| for (c.patches.items, 0..) |patch, i| {
                    dst[i] = .{ .size_x = patch.size_x, .size_y = patch.size_y };
                    putText(&dst[i].name, patch.name);
                    putText(&dst[i].place, patch.place);
                };
            } else short = true;
            var total: usize = 0;
            for (c.indices, 0..) |list, d| {
                rec.index_counts[d] = @intCast(list.items.len);
                total += list.items.len;
            }
            if (total <= @as(usize, @intCast(@max(rec.index_capacity, 0)))) {
                if (rec.indices) |dst| {
                    var at: usize = 0;
                    for (c.indices) |list| for (list.items) |entry| {
                        dst[at] = entry;
                        at += 1;
                    };
                }
            } else short = true;
            if (putScripts(&rec.scripts, c.script_ids.items, @as([]const []const u8, @ptrCast(c.script_areas.items)))) short = true;
            if (short) {
                self.say("the record's arrays are short: the counts are the totals", .{});
                return .refused;
            }
            return .ok;
        }
        self.say("container \"{s}\" is not in the data or does not load as a container", .{wanted});
        return .refused;
    }

    fn containerFromRecord(self: *FakeBridge, rec: *const bridge_mod.RmgContainerRecord) !rmg_mod.Container {
        var out: rmg_mod.Container = .{ .size_x = rec.size_x, .size_y = rec.size_y, .season = rec.season };
        errdefer out.deinit(self.allocator);
        out.season_folder = try self.allocator.dupe(u8, std.mem.sliceTo(&rec.season_folder, 0));
        const count: usize = @intCast(@max(rec.patch_count, 0));
        if (rec.patches) |src| for (src[0..count]) |patch| {
            var made: rmg_mod.Patch = .{ .size_x = patch.size_x, .size_y = patch.size_y };
            made.name = try self.allocator.dupe(u8, std.mem.sliceTo(&patch.name, 0));
            errdefer self.allocator.free(made.name);
            made.place = try self.allocator.dupe(u8, std.mem.sliceTo(&patch.place, 0));
            try out.patches.append(self.allocator, made);
        };
        var at: usize = 0;
        for (rec.index_counts, 0..) |n, d| {
            var i: c_int = 0;
            while (i < n) : (i += 1) {
                try out.indices[d].append(self.allocator, rec.indices.?[at]);
                at += 1;
            }
        }
        const id_count: usize = @intCast(@max(rec.scripts.id_count, 0));
        if (rec.scripts.ids) |src| for (src[0..id_count]) |id| try out.script_ids.append(self.allocator, id);
        const area_count: usize = @intCast(@max(rec.scripts.area_count, 0));
        if (rec.scripts.areas) |src| for (src[0..area_count]) |area| {
            try out.script_areas.append(self.allocator, try self.allocator.dupe(u8, std.mem.sliceTo(&area.name, 0)));
        };
        return out;
    }

    fn rmgWriteContainer(ptr: *anyopaque, name: [*:0]const u8, rec: *const bridge_mod.RmgContainerRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        var buffer: [192]u8 = undefined;
        const wanted = rmgName(&buffer, std.mem.span(name));
        if (!rmgNameIsPlain(wanted, "scenarios\\containers\\")) {
            self.say("\"{s}\" is not a name under scenarios\\containers\\", .{std.mem.span(name)});
            return .bad_argument;
        }
        if (rec.patch_count < 0 or rec.patch_count > bridge_mod.rmg_max_patches) {
            self.say("a container holds at most {d} patches", .{bridge_mod.rmg_max_patches});
            return .refused;
        }
        if (rec.season < 0 or rec.season > 3) {
            self.say("season {d} is outside 0..3", .{rec.season});
            return .refused;
        }
        var at: usize = 0;
        for (rec.index_counts, 0..) |n, d| {
            var i: c_int = 0;
            while (i < n) : (i += 1) {
                const entry = rec.indices.?[at];
                at += 1;
                if (entry < 0 or entry >= rec.patch_count) {
                    self.say("direction {d} lists patch {d}, and there are {d} patches", .{ d, entry, rec.patch_count });
                    return .refused;
                }
            }
        }
        for (self.rmg_containers.items) |*file| {
            if (!std.mem.eql(u8, file.name, wanted)) continue;
            if (file.shipped) {
                self.say("\"{s}\" is shipped data and read-only: Save As a new name", .{wanted});
                return .refused;
            }
            const made = self.containerFromRecord(rec) catch return .failed;
            file.value.deinit(self.allocator);
            file.value = made;
            return .ok;
        }
        var made = self.containerFromRecord(rec) catch return .failed;
        self.addContainerFixture(wanted, false, made) catch {
            made.deinit(self.allocator);
            return .failed;
        };
        return .ok;
    }

    fn rmgReadGraph(ptr: *anyopaque, name: [*:0]const u8, rec: *bridge_mod.RmgGraphRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        rec.node_count = 0;
        rec.link_count = 0;
        rec.scripts.id_count = 0;
        rec.scripts.area_count = 0;
        var buffer: [192]u8 = undefined;
        const wanted = rmgName(&buffer, std.mem.span(name));
        if (!rmgNameIsPlain(wanted, "scenarios\\graphs\\")) {
            self.say("\"{s}\" is not a name under scenarios\\graphs\\", .{std.mem.span(name)});
            return .refused;
        }
        for (self.rmg_graphs.items) |file| {
            if (!std.mem.eql(u8, file.name, wanted)) continue;
            const g = &file.value;
            rec.size_x = g.size_x;
            rec.size_y = g.size_y;
            rec.season = g.season;
            putText(&rec.season_folder, g.season_folder);
            var short = false;
            rec.node_count = @intCast(g.nodes.items.len);
            if (g.nodes.items.len <= @as(usize, @intCast(@max(rec.node_capacity, 0)))) {
                if (rec.nodes) |dst| for (g.nodes.items, 0..) |node, i| {
                    dst[i] = .{ .x1 = node.rect.x1, .y1 = node.rect.y1, .x2 = node.rect.x2, .y2 = node.rect.y2 };
                    putText(&dst[i].container, node.container);
                };
            } else short = true;
            rec.link_count = @intCast(g.links.items.len);
            if (g.links.items.len <= @as(usize, @intCast(@max(rec.link_capacity, 0)))) {
                if (rec.links) |dst| for (g.links.items, 0..) |link, i| {
                    dst[i] = .{ .a = link.a, .b = link.b, .kind = link.kind, .radius = link.radius, .parts = link.parts, .min_length = link.min_length, .distance = link.distance, .disturbance = link.disturbance };
                    putText(&dst[i].desc, link.desc);
                };
            } else short = true;
            if (putScripts(&rec.scripts, g.script_ids.items, @as([]const []const u8, @ptrCast(g.script_areas.items)))) short = true;
            if (short) {
                self.say("the record's arrays are short: the counts are the totals", .{});
                return .refused;
            }
            return .ok;
        }
        self.say("graph \"{s}\" is not in the data or does not load as a graph", .{wanted});
        return .refused;
    }

    fn graphFromRecord(self: *FakeBridge, rec: *const bridge_mod.RmgGraphRecord) !rmg_mod.Graph {
        var out: rmg_mod.Graph = .{ .size_x = rec.size_x, .size_y = rec.size_y, .season = rec.season };
        errdefer out.deinit(self.allocator);
        out.season_folder = try self.allocator.dupe(u8, std.mem.sliceTo(&rec.season_folder, 0));
        const nodes: usize = @intCast(@max(rec.node_count, 0));
        if (rec.nodes) |src| for (src[0..nodes]) |node| {
            const container = try self.allocator.dupe(u8, std.mem.sliceTo(&node.container, 0));
            errdefer self.allocator.free(container);
            try out.nodes.append(self.allocator, .{ .rect = .{ .x1 = node.x1, .y1 = node.y1, .x2 = node.x2, .y2 = node.y2 }, .container = container });
        };
        const links: usize = @intCast(@max(rec.link_count, 0));
        if (rec.links) |src| for (src[0..links]) |link| {
            const desc = try self.allocator.dupe(u8, std.mem.sliceTo(&link.desc, 0));
            errdefer self.allocator.free(desc);
            try out.links.append(self.allocator, .{ .a = link.a, .b = link.b, .kind = link.kind, .desc = desc, .radius = link.radius, .parts = link.parts, .min_length = link.min_length, .distance = link.distance, .disturbance = link.disturbance });
        };
        const id_count: usize = @intCast(@max(rec.scripts.id_count, 0));
        if (rec.scripts.ids) |src| for (src[0..id_count]) |id| try out.script_ids.append(self.allocator, id);
        const area_count: usize = @intCast(@max(rec.scripts.area_count, 0));
        if (rec.scripts.areas) |src| for (src[0..area_count]) |area| {
            try out.script_areas.append(self.allocator, try self.allocator.dupe(u8, std.mem.sliceTo(&area.name, 0)));
        };
        return out;
    }

    fn rmgWriteGraph(ptr: *anyopaque, name: [*:0]const u8, rec: *const bridge_mod.RmgGraphRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        var buffer: [192]u8 = undefined;
        const wanted = rmgName(&buffer, std.mem.span(name));
        if (!rmgNameIsPlain(wanted, "scenarios\\graphs\\")) {
            self.say("\"{s}\" is not a name under scenarios\\graphs\\", .{std.mem.span(name)});
            return .bad_argument;
        }
        if (rec.node_count < 0 or rec.node_count > bridge_mod.rmg_max_nodes or rec.link_count < 0 or rec.link_count > bridge_mod.rmg_max_links) {
            self.say("a graph holds at most {d} nodes and {d} links", .{ bridge_mod.rmg_max_nodes, bridge_mod.rmg_max_links });
            return .refused;
        }
        if (rec.season < 0 or rec.season > 3) {
            self.say("season {d} is outside 0..3", .{rec.season});
            return .refused;
        }
        if (rec.nodes) |nodes| for (nodes[0..@intCast(rec.node_count)], 0..) |node, i| {
            if (node.x2 <= node.x1 or node.y2 <= node.y1) {
                self.say("node {d} has no area", .{i});
                return .refused;
            }
        };
        if (rec.links) |links| for (links[0..@intCast(rec.link_count)], 0..) |link, i| {
            if (link.a < 0 or link.a >= rec.node_count or link.b < 0 or link.b >= rec.node_count) {
                self.say("link {d} joins nodes {d} and {d}, and there are {d} nodes", .{ i, link.a, link.b, rec.node_count });
                return .refused;
            }
            if (link.kind < 0 or link.kind > 255 or link.parts < 0) {
                self.say("link {d} has a type outside 0..255 or a negative part count", .{i});
                return .refused;
            }
        };
        for (self.rmg_graphs.items) |*file| {
            if (!std.mem.eql(u8, file.name, wanted)) continue;
            if (file.shipped) {
                self.say("\"{s}\" is shipped data and read-only: Save As a new name", .{wanted});
                return .refused;
            }
            const made = self.graphFromRecord(rec) catch return .failed;
            file.value.deinit(self.allocator);
            file.value = made;
            return .ok;
        }
        var made = self.graphFromRecord(rec) catch return .failed;
        self.addGraphFixture(wanted, false, made) catch {
            made.deinit(self.allocator);
            return .failed;
        };
        return .ok;
    }

    pub fn addFieldSetFixture(self: *FakeBridge, name: []const u8, shipped: bool, value: rmg_mod.FieldSet) !void {
        const owned = try self.allocator.dupe(u8, name);
        errdefer self.allocator.free(owned);
        try self.rmg_fieldsets.append(self.allocator, .{ .name = owned, .shipped = shipped, .value = value });
    }

    fn rmgReadFieldSet(ptr: *anyopaque, name: [*:0]const u8, rec: *bridge_mod.RmgFieldSetRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        rec.tile_shell_count = 0;
        rec.tile_total = 0;
        rec.object_shell_count = 0;
        rec.object_total = 0;
        var buffer: [192]u8 = undefined;
        const wanted = rmgName(&buffer, std.mem.span(name));
        if (!rmgNameIsPlain(wanted, "scenarios\\fieldsets\\")) {
            self.say("\"{s}\" is not a name under scenarios\\fieldsets\\", .{std.mem.span(name)});
            return .refused;
        }
        for (self.rmg_fieldsets.items) |file| {
            if (!std.mem.eql(u8, file.name, wanted)) continue;
            const f = &file.value;
            rec.season = f.season;
            putText(&rec.season_folder, f.season_folder);
            putText(&rec.profile, f.profile);
            rec.height = f.height;
            rec.pattern_min = f.pattern_min;
            rec.pattern_max = f.pattern_max;
            rec.positive_ratio = f.positive_ratio;
            rec.tile_shell_count = @intCast(f.tile_shells.items.len);
            rec.object_shell_count = @intCast(f.object_shells.items.len);
            rec.tile_total = @intCast(f.tileEntryCount());
            rec.object_total = @intCast(f.objectEntryCount());
            var short = false;
            if (f.tile_shells.items.len <= @as(usize, @intCast(@max(rec.tile_shell_capacity, 0)))) {
                if (rec.tile_shells) |dst| for (f.tile_shells.items, 0..) |shell, i| {
                    dst[i] = .{ .width = shell.width, .tile_count = @intCast(shell.tiles.items.len) };
                };
            } else short = true;
            if (f.object_shells.items.len <= @as(usize, @intCast(@max(rec.object_shell_capacity, 0)))) {
                if (rec.object_shells) |dst| for (f.object_shells.items, 0..) |shell, i| {
                    dst[i] = .{ .width = shell.width, .step = shell.step, .ratio = shell.ratio, .object_count = @intCast(shell.objects.items.len) };
                };
            } else short = true;
            if (f.tileEntryCount() <= @as(usize, @intCast(@max(rec.tile_capacity, 0)))) {
                if (rec.tiles) |dst| {
                    var at: usize = 0;
                    for (f.tile_shells.items) |shell| for (shell.tiles.items) |entry| {
                        dst[at] = .{ .tile = entry.tile, .weight = entry.weight };
                        at += 1;
                    };
                }
            } else short = true;
            if (f.objectEntryCount() <= @as(usize, @intCast(@max(rec.object_capacity, 0)))) {
                if (rec.objects) |dst| {
                    var at: usize = 0;
                    for (f.object_shells.items) |shell| for (shell.objects.items) |entry| {
                        dst[at] = .{ .weight = entry.weight };
                        putText(&dst[at].name, entry.name);
                        at += 1;
                    };
                }
            } else short = true;
            if (short) {
                self.say("the record's arrays are short: the counts are the totals", .{});
                return .refused;
            }
            return .ok;
        }
        self.say("field set \"{s}\" is not in the data or does not load as a field set", .{wanted});
        return .refused;
    }

    fn fieldSetFromRecord(self: *FakeBridge, rec: *const bridge_mod.RmgFieldSetRecord) !rmg_mod.FieldSet {
        const a = self.allocator;
        var out: rmg_mod.FieldSet = .{ .season = rec.season, .height = rec.height, .pattern_min = rec.pattern_min, .pattern_max = rec.pattern_max, .positive_ratio = rec.positive_ratio };
        errdefer out.deinit(a);
        out.season_folder = try a.dupe(u8, std.mem.sliceTo(&rec.season_folder, 0));
        out.profile = try a.dupe(u8, std.mem.sliceTo(&rec.profile, 0));
        var tile_at: usize = 0;
        const shells: usize = @intCast(@max(rec.tile_shell_count, 0));
        if (rec.tile_shells) |src| for (src[0..shells]) |shell| {
            var made: rmg_mod.TileShell = .{ .width = shell.width };
            errdefer made.deinit(a);
            const count: usize = @intCast(@max(shell.tile_count, 0));
            if (rec.tiles) |tiles| for (tiles[tile_at .. tile_at + count]) |entry| try made.tiles.append(a, .{ .tile = entry.tile, .weight = entry.weight });
            tile_at += count;
            try out.tile_shells.append(a, made);
        };
        var object_at: usize = 0;
        const object_shells: usize = @intCast(@max(rec.object_shell_count, 0));
        if (rec.object_shells) |src| for (src[0..object_shells]) |shell| {
            var made: rmg_mod.ObjectShell = .{ .width = shell.width, .step = shell.step, .ratio = shell.ratio };
            errdefer made.deinit(a);
            const count: usize = @intCast(@max(shell.object_count, 0));
            if (rec.objects) |object_src| for (object_src[object_at .. object_at + count]) |entry| {
                const copy = try a.dupe(u8, std.mem.sliceTo(&entry.name, 0));
                errdefer a.free(copy);
                try made.objects.append(a, .{ .name = copy, .weight = entry.weight });
            };
            object_at += count;
            try out.object_shells.append(a, made);
        };
        return out;
    }

    fn rmgWriteFieldSet(ptr: *anyopaque, name: [*:0]const u8, rec: *const bridge_mod.RmgFieldSetRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        var buffer: [192]u8 = undefined;
        const wanted = rmgName(&buffer, std.mem.span(name));
        if (!rmgNameIsPlain(wanted, "scenarios\\fieldsets\\")) {
            self.say("\"{s}\" is not a name under scenarios\\fieldsets\\", .{std.mem.span(name)});
            return .bad_argument;
        }
        if (rec.tile_shell_count < 0 or rec.tile_shell_count > bridge_mod.rmg_max_shells or rec.object_shell_count < 0 or rec.object_shell_count > bridge_mod.rmg_max_shells or
            rec.tile_total < 0 or rec.tile_total > bridge_mod.rmg_max_shell_entries or rec.object_total < 0 or rec.object_total > bridge_mod.rmg_max_shell_entries)
        {
            self.say("a field set holds at most {d} shells of each kind and {d} entries of each", .{ bridge_mod.rmg_max_shells, bridge_mod.rmg_max_shell_entries });
            return .refused;
        }
        if (rec.season < 0 or rec.season > 3) {
            self.say("season {d} is outside 0..3", .{rec.season});
            return .refused;
        }
        if (!std.math.isFinite(rec.height) or rec.height < 0 or !std.math.isFinite(rec.positive_ratio) or rec.positive_ratio < 0) {
            self.say("the height and the positive ratio must be finite and not negative", .{});
            return .refused;
        }
        var tiles_seen: usize = 0;
        if (rec.tile_shells) |shells| for (shells[0..@intCast(rec.tile_shell_count)], 0..) |shell, i| {
            if (shell.tile_count < 0 or @as(usize, @intCast(shell.tile_count)) > @as(usize, @intCast(rec.tile_total)) - tiles_seen or !std.math.isFinite(shell.width) or shell.width < 0) {
                self.say("tile shell {d}: its entry count does not fit the tiles given, or its width is negative or not finite", .{i});
                return .refused;
            }
            tiles_seen += @intCast(shell.tile_count);
        };
        var objects_seen: usize = 0;
        if (rec.object_shells) |shells| for (shells[0..@intCast(rec.object_shell_count)], 0..) |shell, i| {
            if (shell.object_count < 0 or @as(usize, @intCast(shell.object_count)) > @as(usize, @intCast(rec.object_total)) - objects_seen or !std.math.isFinite(shell.width) or shell.width < 0 or !std.math.isFinite(shell.ratio) or shell.ratio < 0) {
                self.say("object shell {d}: its entry count does not fit the objects given, or its width or ratio is negative or not finite", .{i});
                return .refused;
            }
            objects_seen += @intCast(shell.object_count);
        };
        if (tiles_seen != @as(usize, @intCast(rec.tile_total)) or objects_seen != @as(usize, @intCast(rec.object_total))) {
            self.say("the shells' entry counts do not add up to the tiles or objects given", .{});
            return .refused;
        }
        if (rec.tiles) |tiles| for (tiles[0..@intCast(rec.tile_total)]) |entry| {
            if (entry.tile < 0 or entry.weight < 0) {
                self.say("a tile or weight below 0", .{});
                return .refused;
            }
        };
        if (rec.objects) |object_src| for (object_src[0..@intCast(rec.object_total)]) |entry| {
            if (entry.weight < 0) {
                self.say("a weight below 0", .{});
                return .refused;
            }
        };
        for (self.rmg_fieldsets.items) |*file| {
            if (!std.mem.eql(u8, file.name, wanted)) continue;
            if (file.shipped) {
                self.say("\"{s}\" is shipped data and read-only: Save As a new name", .{wanted});
                return .refused;
            }
            const made = self.fieldSetFromRecord(rec) catch return .failed;
            file.value.deinit(self.allocator);
            file.value = made;
            return .ok;
        }
        var made = self.fieldSetFromRecord(rec) catch return .failed;
        self.addFieldSetFixture(wanted, false, made) catch {
            made.deinit(self.allocator);
            return .failed;
        };
        return .ok;
    }

    pub fn addTemplateFixture(self: *FakeBridge, name: []const u8, shipped: bool, value: rmg_mod.Template) !void {
        const owned = try self.allocator.dupe(u8, name);
        errdefer self.allocator.free(owned);
        try self.rmg_templates.append(self.allocator, .{ .name = owned, .shipped = shipped, .value = value });
    }

    fn unitToRecord(unit: records.UnitCreation, out: *bridge_mod.RmgUnit) void {
        out.* = .{ .slot_count = @intCast(unit.slot_count), .paratroop_count = unit.paratroop_count, .relax_time = unit.relax_time, .appear_count = @intCast(unit.appear_count) };
        @memcpy(&out.party, &unit.party);
        @memcpy(&out.paratroop_name, &unit.paratroop_name);
        for (unit.aircraft, 0..) |slot, i| {
            @memcpy(&out.aircraft[i].name, &slot.name);
            out.aircraft[i].formation_size = slot.formation_size;
            out.aircraft[i].count = slot.count;
        }
        for (unit.appear, 0..) |point, i| out.appear[i] = .{ .x = point.x, .y = point.y, .z = point.z };
    }

    fn unitFromRecord(rec: bridge_mod.RmgUnit) records.UnitCreation {
        var out: records.UnitCreation = .{ .slot_count = 0, .paratroop_count = rec.paratroop_count, .relax_time = rec.relax_time, .appear_count = @intCast(@max(rec.appear_count, 0)) };
        @memcpy(&out.party, &rec.party);
        @memcpy(&out.paratroop_name, &rec.paratroop_name);
        for (rec.aircraft, 0..) |slot, i| {
            @memcpy(&out.aircraft[i].name, &slot.name);
            out.aircraft[i].formation_size = slot.formation_size;
            out.aircraft[i].count = slot.count;
        }
        for (rec.appear, 0..) |point, i| out.appear[i] = .{ .x = point.x, .y = point.y, .z = point.z };
        return out;
    }

    fn rmgReadTemplate(ptr: *anyopaque, name: [*:0]const u8, rec: *bridge_mod.RmgTemplateRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        rec.field_count = 0;
        rec.graph_count = 0;
        rec.vso_count = 0;
        rec.diplomacy_count = 0;
        rec.unit_count = 0;
        rec.scripts.id_count = 0;
        rec.scripts.area_count = 0;
        var buffer: [192]u8 = undefined;
        const wanted = rmgName(&buffer, std.mem.span(name));
        if (!rmgNameIsPlain(wanted, "scenarios\\templates\\")) {
            self.say("\"{s}\" is not a name under scenarios\\templates\\", .{std.mem.span(name)});
            return .refused;
        }
        for (self.rmg_templates.items) |file| {
            if (!std.mem.eql(u8, file.name, wanted)) continue;
            const t = &file.value;
            rec.size_x = t.size_x;
            rec.size_y = t.size_y;
            rec.season = t.season;
            rec.default_field = t.default_field;
            rec.mission_index = t.mission_index;
            rec.game_type = t.game_type;
            rec.attacking_side = t.attacking_side;
            rec.camera = t.camera;
            putText(&rec.season_folder, t.season_folder);
            putText(&rec.place, t.place);
            putText(&rec.script_file, t.script_file);
            putText(&rec.chapter_name, t.chapter_name);
            putText(&rec.forest_circle_sounds, t.forest_circle_sounds);
            putText(&rec.forest_ambient_sounds, t.forest_ambient_sounds);
            putText(&rec.mod_name, t.mod_name);
            putText(&rec.mod_version, t.mod_version);
            var short = false;
            rec.field_count = @intCast(t.fields.items.len);
            if (t.fields.items.len <= @as(usize, @intCast(@max(rec.field_capacity, 0)))) {
                if (rec.fields) |dst| for (t.fields.items, 0..) |entry, i| {
                    dst[i] = .{ .weight = entry.weight };
                    putText(&dst[i].name, entry.name);
                };
            } else short = true;
            rec.graph_count = @intCast(t.graphs.items.len);
            if (t.graphs.items.len <= @as(usize, @intCast(@max(rec.graph_capacity, 0)))) {
                if (rec.graphs) |dst| for (t.graphs.items, 0..) |entry, i| {
                    dst[i] = .{ .weight = entry.weight };
                    putText(&dst[i].name, entry.name);
                };
            } else short = true;
            rec.vso_count = @intCast(t.vso.items.len);
            if (t.vso.items.len <= @as(usize, @intCast(@max(rec.vso_capacity, 0)))) {
                if (rec.vso) |dst| for (t.vso.items, 0..) |entry, i| {
                    dst[i] = .{ .weight = entry.weight, .width = entry.width, .opacity = entry.opacity };
                    putText(&dst[i].name, entry.name);
                };
            } else short = true;
            rec.diplomacy_count = @intCast(t.diplomacies.items.len);
            if (t.diplomacies.items.len <= @as(usize, @intCast(@max(rec.diplomacy_capacity, 0)))) {
                if (rec.diplomacies) |dst| @memcpy(dst[0..t.diplomacies.items.len], t.diplomacies.items);
            } else short = true;
            rec.unit_count = @intCast(t.units.items.len);
            if (t.units.items.len <= @as(usize, @intCast(@max(rec.unit_capacity, 0)))) {
                if (rec.units) |dst| for (t.units.items, 0..) |unit, i| {
                    unitToRecord(unit, &dst[i]);
                    dst[i].slot_count = rec.unit_count;
                };
            } else short = true;
            if (putScripts(&rec.scripts, t.script_ids.items, @as([]const []const u8, @ptrCast(t.script_areas.items)))) short = true;
            if (short) {
                self.say("the record's arrays are short: the counts are the totals", .{});
                return .refused;
            }
            return .ok;
        }
        self.say("template \"{s}\" is not in the data or does not load as a template", .{wanted});
        return .refused;
    }

    fn templateFromRecord(self: *FakeBridge, rec: *const bridge_mod.RmgTemplateRecord) !rmg_mod.Template {
        const a = self.allocator;
        var out: rmg_mod.Template = .{
            .size_x = rec.size_x,
            .size_y = rec.size_y,
            .season = rec.season,
            .default_field = rec.default_field,
            .mission_index = rec.mission_index,
            .game_type = rec.game_type,
            .attacking_side = rec.attacking_side,
            .camera = rec.camera,
        };
        errdefer out.deinit(a);
        const sources = [8][]const u8{
            std.mem.sliceTo(&rec.season_folder, 0), std.mem.sliceTo(&rec.place, 0),                 std.mem.sliceTo(&rec.script_file, 0),
            std.mem.sliceTo(&rec.chapter_name, 0),  std.mem.sliceTo(&rec.forest_circle_sounds, 0), std.mem.sliceTo(&rec.forest_ambient_sounds, 0),
            std.mem.sliceTo(&rec.mod_name, 0),      std.mem.sliceTo(&rec.mod_version, 0),
        };
        for (sources, 0..) |text, i| try out.setText(a, @enumFromInt(i), text);
        const field_count: usize = @intCast(@max(rec.field_count, 0));
        if (rec.fields) |src| for (src[0..field_count]) |entry| {
            const copy = try a.dupe(u8, std.mem.sliceTo(&entry.name, 0));
            errdefer a.free(copy);
            try out.fields.append(a, .{ .name = copy, .weight = entry.weight });
        };
        const graph_count: usize = @intCast(@max(rec.graph_count, 0));
        if (rec.graphs) |src| for (src[0..graph_count]) |entry| {
            const copy = try a.dupe(u8, std.mem.sliceTo(&entry.name, 0));
            errdefer a.free(copy);
            try out.graphs.append(a, .{ .name = copy, .weight = entry.weight });
        };
        const vso_count: usize = @intCast(@max(rec.vso_count, 0));
        if (rec.vso) |src| for (src[0..vso_count]) |entry| {
            const copy = try a.dupe(u8, std.mem.sliceTo(&entry.name, 0));
            errdefer a.free(copy);
            try out.vso.append(a, .{ .name = copy, .weight = entry.weight, .width = entry.width, .opacity = entry.opacity });
        };
        const diplomacy_count: usize = @intCast(@max(rec.diplomacy_count, 0));
        if (rec.diplomacies) |src| try out.diplomacies.appendSlice(a, src[0..diplomacy_count]);
        const unit_count: usize = @intCast(@max(rec.unit_count, 0));
        if (rec.units) |src| for (src[0..unit_count]) |unit| try out.units.append(a, unitFromRecord(unit));
        const id_count: usize = @intCast(@max(rec.scripts.id_count, 0));
        if (rec.scripts.ids) |src| for (src[0..id_count]) |id| try out.script_ids.append(a, id);
        const area_count: usize = @intCast(@max(rec.scripts.area_count, 0));
        if (rec.scripts.areas) |src| for (src[0..area_count]) |area| {
            try out.script_areas.append(a, try a.dupe(u8, std.mem.sliceTo(&area.name, 0)));
        };
        // What the loader does after reading: no diplomacies is the default pair and
        // the neutral, and the unit creation has a player each (at least two).
        if (out.diplomacies.items.len == 0) try out.diplomacies.appendSlice(a, &.{ 0, 1, 2 });
        while (out.units.items.len < 2) try out.units.append(a, rmg_mod.Template.defaultUnit());
        return out;
    }

    fn rmgWriteTemplate(ptr: *anyopaque, name: [*:0]const u8, rec: *const bridge_mod.RmgTemplateRecord) Status {
        const self = from(ptr);
        self.message_len = 0;
        var buffer: [192]u8 = undefined;
        const wanted = rmgName(&buffer, std.mem.span(name));
        if (!rmgNameIsPlain(wanted, "scenarios\\templates\\")) {
            self.say("\"{s}\" is not a name under scenarios\\templates\\", .{std.mem.span(name)});
            return .bad_argument;
        }
        if (rec.field_count < 0 or rec.field_count > bridge_mod.rmg_max_weighted or rec.graph_count < 0 or rec.graph_count > bridge_mod.rmg_max_weighted or
            rec.vso_count < 0 or rec.vso_count > bridge_mod.rmg_max_weighted or rec.diplomacy_count < 0 or rec.diplomacy_count > bridge_mod.rmg_max_players or
            rec.unit_count < 0 or rec.unit_count > bridge_mod.rmg_max_units)
        {
            self.say("a template holds at most {d} fields, graphs and vso each, {d} diplomacies and {d} units", .{ bridge_mod.rmg_max_weighted, bridge_mod.rmg_max_players, bridge_mod.rmg_max_units });
            return .refused;
        }
        if (rec.season < 0 or rec.season > 3 or rec.size_x < 0 or rec.size_y < 0) {
            self.say("season {d} is outside 0..3, or the size is negative", .{rec.season});
            return .refused;
        }
        if (rec.fields) |src| for (src[0..@intCast(rec.field_count)]) |entry| {
            if (entry.weight < 0) {
                self.say("a field weight below 0", .{});
                return .refused;
            }
        };
        if (rec.graphs) |src| for (src[0..@intCast(rec.graph_count)]) |entry| {
            if (entry.weight < 0) {
                self.say("a graph weight below 0", .{});
                return .refused;
            }
        };
        if (rec.vso) |src| for (src[0..@intCast(rec.vso_count)]) |entry| {
            if (entry.weight < 0 or !std.math.isFinite(entry.width) or !std.math.isFinite(entry.opacity)) {
                self.say("a vso weight below 0 or a width or opacity not finite", .{});
                return .refused;
            }
        };
        if (!std.math.isFinite(rec.camera[0]) or !std.math.isFinite(rec.camera[1]) or !std.math.isFinite(rec.camera[2])) {
            self.say("the camera anchor is not finite", .{});
            return .refused;
        }
        for (self.rmg_templates.items) |*file| {
            if (!std.mem.eql(u8, file.name, wanted)) continue;
            if (file.shipped) {
                self.say("\"{s}\" is shipped data and read-only: Save As a new name", .{wanted});
                return .refused;
            }
            const made = self.templateFromRecord(rec) catch return .failed;
            file.value.deinit(self.allocator);
            file.value = made;
            return .ok;
        }
        var made = self.templateFromRecord(rec) catch return .failed;
        self.addTemplateFixture(wanted, false, made) catch {
            made.deinit(self.allocator);
            return .failed;
        };
        return .ok;
    }

    fn rmgTileset(ptr: *anyopaque, season: i32, out: []bridge_mod.RmgTerrainType, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        total.* = 0;
        if (season < 0 or season > 3) return .bad_argument;
        const count = self.tileset_counts[@intCast(season)];
        if (count == 0) {
            self.say("the tileset of season {d} does not load", .{season});
            return .refused;
        }
        total.* = count;
        if (out.len < count) return .refused;
        for (0..count) |i| {
            out[i] = .{ .variant_count = 4 };
            var label: [32]u8 = undefined;
            const text = std.fmt.bufPrint(&label, "terrain{d}", .{i}) catch "terrain";
            putText(&out[i].name, text);
        }
        return .ok;
    }

    fn rmgFileExists(ptr: *anyopaque, name: [*:0]const u8, extension: [*:0]const u8, exists: *bool) Status {
        const self = from(ptr);
        self.message_len = 0;
        exists.* = false;
        const wanted_name = std.mem.span(name);
        if (wanted_name.len == 0 or std.mem.indexOf(u8, wanted_name, "..") != null or std.mem.indexOfScalar(u8, wanted_name, ':') != null) return .bad_argument;
        var buffer: [256]u8 = undefined;
        const trimmed = std.mem.trimStart(u8, wanted_name, "\\/");
        const joined = std.fmt.bufPrint(&buffer, "{s}{s}", .{ trimmed, std.mem.span(extension) }) catch return .bad_argument;
        for (joined) |*byte| byte.* = if (byte.* == '/') '\\' else std.ascii.toLower(byte.*);
        for (self.known_files) |known| {
            if (std.mem.eql(u8, known, joined)) {
                exists.* = true;
                break;
            }
        }
        return .ok;
    }

    fn rmgPatchInfo(ptr: *anyopaque, name: [*:0]const u8, info: *bridge_mod.RmgPatchInfo) Status {
        const self = from(ptr);
        self.message_len = 0;
        info.scripts.id_count = 0;
        info.scripts.area_count = 0;
        var buffer: [192]u8 = undefined;
        const wanted = rmgName(&buffer, std.mem.span(name));
        for (self.rmg_patches.items) |file| {
            if (!std.mem.eql(u8, file.name, wanted)) continue;
            info.size_x = file.size_x;
            info.size_y = file.size_y;
            info.season = file.season;
            putText(&info.season_folder, file.folder);
            if (putScripts(&info.scripts, file.ids, @as([]const []const u8, @ptrCast(file.areas)))) {
                self.say("the record's arrays are short: the counts are the totals", .{});
                return .refused;
            }
            return .ok;
        }
        self.say("patch \"{s}\" is not in the data or does not load as a map", .{wanted});
        return .refused;
    }

    fn rmgImportPatch(ptr: *anyopaque, source: [*:0]const u8, apply: bool, out: *bridge_mod.RmgName) Status {
        const self = from(ptr);
        self.message_len = 0;
        out.* = .{};
        const path = std.mem.span(source);
        const base = std.fs.path.basename(path);
        const dot = std.mem.lastIndexOfScalar(u8, base, '.') orelse base.len;
        const extension = base[dot..];
        if (path.len == 0 or path[0] != '/' or !(std.ascii.eqlIgnoreCase(extension, ".bzm") or std.ascii.eqlIgnoreCase(extension, ".xml"))) {
            self.say("\"{s}\" is not the full path of an existing .bzm or .xml map file", .{path});
            return .refused;
        }
        if (std.mem.indexOf(u8, base, "notamap") != null) {
            self.say("the file does not read as a map: fixture", .{});
            return .refused;
        }
        var stem_buffer: [64]u8 = undefined;
        const stem = rmgName(&stem_buffer, base[0..dot]);
        var name_buffer: [192]u8 = undefined;
        const name = std.fmt.bufPrint(&name_buffer, "scenarios\\patches\\summer\\{s}", .{stem}) catch return .failed;
        if (apply) {
            for (self.rmg_patches.items) |file| {
                if (std.mem.eql(u8, file.name, name)) {
                    // Only a patch of ours (one this fake copied in) is replaced.
                    if (self.rmg_imports == 0) {
                        self.say("\"{s}\" is shipped data and read-only: rename the map file first", .{name});
                        return .refused;
                    }
                    putText(&out.name, name);
                    return .ok;
                }
            }
            self.addPatchFixture(name, 1, 0, "terrain\\sets\\1\\", &.{}, &.{}) catch return .failed;
            self.rmg_imports += 1;
        }
        putText(&out.name, name);
        return .ok;
    }

    fn rmgRoot(ptr: *anyopaque, out: []u8) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (out.len <= self.rmg_root_text.len) {
            self.say("the user RMG root does not fit the caller's buffer", .{});
            return .refused;
        }
        @memcpy(out[0..self.rmg_root_text.len], self.rmg_root_text);
        out[self.rmg_root_text.len] = 0;
        return .ok;
    }

    fn rmgNames(self: *const FakeBridge, kind: bridge_mod.RmgKind) []const []const u8 {
        return switch (kind) {
            .field_sets => self.field_set_names,
            .templates => self.template_names,
            .settings => self.setting_names,
            .chapters => self.chapter_names,
            else => &.{},
        };
    }

    fn listRmg(ptr: *anyopaque, kind: bridge_mod.RmgKind, out: []bridge_mod.RmgName, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (kind == .containers or kind == .graphs) {
            // The stored composer files, shipped and user alike, as the scan lists them.
            const count = if (kind == .containers) self.rmg_containers.items.len else self.rmg_graphs.items.len;
            total.* = count;
            if (out.len < count) return .refused;
            for (0..count) |i| {
                const name = if (kind == .containers) self.rmg_containers.items[i].name else self.rmg_graphs.items[i].name;
                var entry = bridge_mod.RmgName{};
                const len = @min(name.len, entry.name.len - 1);
                @memcpy(entry.name[0..len], name[0..len]);
                out[i] = entry;
            }
            return .ok;
        }
        if (kind == .field_sets and self.rmg_fieldsets.items.len != 0) {
            // The Fields Composer's stored files, shipped and user alike.
            total.* = self.rmg_fieldsets.items.len;
            if (out.len < total.*) return .refused;
            for (self.rmg_fieldsets.items, 0..) |file, i| {
                var entry = bridge_mod.RmgName{};
                const len = @min(file.name.len, entry.name.len - 1);
                @memcpy(entry.name[0..len], file.name[0..len]);
                out[i] = entry;
            }
            return .ok;
        }
        if (kind == .templates and self.rmg_templates.items.len != 0) {
            // The Templates Composer's stored files, shipped and user alike.
            total.* = self.rmg_templates.items.len;
            if (out.len < total.*) return .refused;
            for (self.rmg_templates.items, 0..) |file, i| {
                var entry = bridge_mod.RmgName{};
                const len = @min(file.name.len, entry.name.len - 1);
                @memcpy(entry.name[0..len], file.name[0..len]);
                out[i] = entry;
            }
            return .ok;
        }
        const names = self.rmgNames(kind);
        total.* = names.len;
        if (out.len < total.*) return .refused;
        for (names, 0..) |name, i| {
            var entry = bridge_mod.RmgName{};
            const len = @min(name.len, entry.name.len - 1);
            @memcpy(entry.name[0..len], name[0..len]);
            out[i] = entry;
        }
        return .ok;
    }

    fn listStorageFiles(ptr: *anyopaque, folder: [*:0]const u8, extension: [*:0]const u8, out: []bridge_mod.RmgName, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        total.* = 0;
        const wanted_folder = std.mem.span(folder);
        const wanted_extension = std.mem.span(extension);
        if (wanted_folder.len < 2 or wanted_folder[wanted_folder.len - 1] != '\\' or wanted_extension.len == 0) {
            self.say("the folder or the extension of the storage listing is not plain", .{});
            return .refused;
        }
        // The real listing is sorted and deduped; the fixture's order is the
        // sorted one, so the filter keeps it.
        var count: usize = 0;
        for (self.storage_files) |name| {
            if (name.len <= wanted_folder.len or !std.mem.startsWith(u8, name, wanted_folder)) continue;
            if (!std.mem.endsWith(u8, name, wanted_extension)) continue;
            if (count < out.len) {
                var entry = bridge_mod.RmgName{};
                const len = @min(name.len, entry.name.len - 1);
                @memcpy(entry.name[0..len], name[0..len]);
                out[count] = entry;
            }
            count += 1;
        }
        total.* = count;
        return if (count > out.len) .refused else .ok;
    }

    fn rmgCheckSetting(ptr: *anyopaque, template: [*:0]const u8, graph: i32, angle: i32, setting: [*:0]const u8) Status {
        _ = graph;
        _ = angle;
        const self = from(ptr);
        self.message_len = 0;
        if (!self.rmgHas(.templates, std.mem.span(template))) {
            self.say("template \"{s}\" is not in the data", .{std.mem.span(template)});
            return .refused;
        }
        if (self.settingUnfit(std.mem.span(setting))) {
            self.say("template \"{s}\" cannot be built in setting \"{s}\"", .{ std.mem.span(template), std.mem.span(setting) });
            return .refused;
        }
        return .ok;
    }

    fn settingUnfit(self: *const FakeBridge, setting: []const u8) bool {
        for (self.unfit_settings) |unfit| {
            if (std.ascii.eqlIgnoreCase(unfit, setting)) return true;
        }
        return false;
    }

    fn rmgTemplateGraphs(ptr: *anyopaque, template: [*:0]const u8, out: []bridge_mod.RmgGraph, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        total.* = 0;
        if (!self.rmgHas(.templates, std.mem.span(template))) {
            self.say("template \"{s}\" is not in the data", .{std.mem.span(template)});
            return .refused;
        }
        total.* = self.template_graph_names.len;
        if (out.len < total.*) return .refused;
        for (self.template_graph_names, 0..) |name, i| {
            var entry = bridge_mod.RmgGraph{};
            const len = @min(name.len, entry.name.len - 1);
            @memcpy(entry.name[0..len], name[0..len]);
            entry.weight = self.template_graph_weights[i];
            out[i] = entry;
        }
        return .ok;
    }

    fn rmgHas(self: *const FakeBridge, kind: bridge_mod.RmgKind, name: []const u8) bool {
        for (self.rmgNames(kind)) |known| {
            if (std.ascii.eqlIgnoreCase(known, name)) return true;
        }
        return false;
    }

    /// Create Random Map's scripted generation: the real bridge's refusals
    /// (a name the data does not hold, a range, a map name that is not
    /// plain, a repeat without overwrite) naming their field, then a
    /// fixture result - the seed as given or the drawn one, the graph and
    /// angle as given or 0 - with the 19 progress steps reported.
    fn createRandomMap(ptr: *anyopaque, params: bridge_mod.RmgGenerateParams, result: *bridge_mod.RmgGenerateResult) Status {
        const self = from(ptr);
        self.message_len = 0;
        result.* = .{};
        if (!self.rmgHas(.templates, params.templateSlice())) {
            self.say("template \"{s}\" is not in the data", .{params.templateSlice()});
            return .refused;
        }
        if (!self.rmgHas(.chapters, params.contextSlice())) {
            self.say("context \"{s}\" is not in the data", .{params.contextSlice()});
            return .refused;
        }
        const setting = params.settingSlice();
        if (setting.len != 0 and !std.mem.eql(u8, setting, bridge_mod.rmg_any_setting) and !self.rmgHas(.settings, setting)) {
            self.say("setting \"{s}\" is not in the data", .{setting});
            return .refused;
        }
        if (params.level < 0 or params.level > 2) {
            self.say("level {d} is outside 0..2", .{params.level});
            return .refused;
        }
        if (params.graph < -1 or params.graph >= self.template_graph_count) {
            self.say("graph {d} is outside -1..{d} for template \"{s}\"", .{ params.graph, self.template_graph_count - 1, params.templateSlice() });
            return .refused;
        }
        if (params.angle < -1 or params.angle > 3) {
            self.say("angle {d} is outside -1..3", .{params.angle});
            return .refused;
        }
        if (self.settingUnfit(setting)) {
            self.say("template \"{s}\" cannot be built in setting \"{s}\"", .{ params.templateSlice(), setting });
            return .refused;
        }
        const name = params.mapNameSlice();
        if (name.len == 0 or std.mem.indexOfAny(u8, name, "\\/:") != null or std.mem.eql(u8, name, ".") or std.mem.eql(u8, name, "..")) {
            self.say("map name \"{s}\" is not a plain name (no folder, drive or dots)", .{name});
            return .refused;
        }
        var repeated = false;
        for (self.generated_names.items) |known| {
            if (std.mem.eql(u8, std.mem.sliceTo(&known, 0), name)) repeated = true;
        }
        if (repeated and params.overwrite == 0) {
            self.say("a map named \"{s}\" already exists in the maps folder", .{name});
            return .refused;
        }
        if (!repeated) self.generated_names.append(self.allocator, params.map_name) catch return .failed;
        self.last_generate = params;
        var step: c_int = 1;
        while (step <= 19) : (step += 1) {
            if (params.progress) |report| report(step, 19, params.user);
        }
        result.seed = if (params.has_seed != 0) params.seed else self.drawn_seed;
        result.graph = if (params.graph >= 0) params.graph else 0;
        result.angle = if (params.angle >= 0) params.angle else 0;
        const graph_name = std.fmt.bufPrint(&result.graph_name, "graphs\\fake{d}", .{result.graph}) catch "";
        _ = graph_name;
        const extension: []const u8 = if (params.save_as_bzm != 0) "bzm" else "xml";
        _ = std.fmt.bufPrint(&result.map_path, "/fake/user/maps/{s}.{s}", .{ name, extension }) catch {};
        return .ok;
    }

    fn isNonUnit(self: *const FakeBridge, link_id: i32) bool {
        return std.mem.indexOfScalar(i32, self.non_units.items, link_id) != null;
    }

    /// True, with the reason said, when a link ID cannot be a unit of a start command.
    fn refusesStartUnit(self: *FakeBridge, link_id: i32) bool {
        if (link_id <= 0) {
            self.say("a start command's unit is a link ID above 0", .{});
            return true;
        }
        const index = self.indexOf(link_id) orelse {
            self.say("no object has link ID {d}", .{link_id});
            return true;
        };
        const object = &self.objects_list.items[index];
        if (!object.known) {
            self.say("the object database does not know the type of object {d} ({s})", .{ link_id, object.nameSlice() });
            return true;
        }
        if (self.isNonUnit(link_id)) {
            self.say("object {d} ({s}) is not a unit or a squad", .{ link_id, object.nameSlice() });
            return true;
        }
        return false;
    }

    /// The real bridge's start-command rules: what the put CHANGES is judged
    /// against `current` (null for an add), and a command the map held when it was
    /// opened is always accepted. Says why when it is not.
    fn startCommandAllowed(self: *FakeBridge, wanted: *const FakeStartCommand, current: ?*const FakeStartCommand) bool {
        for (self.start_commands_at_open.items) |*opened| {
            if (opened.eql(wanted)) return true;
        }
        if (wanted.unit_count == 0) {
            self.say("a start command needs at least one unit", .{});
            return false;
        }
        for (wanted.unitSlice()) |unit| {
            const in_wanted = std.mem.count(i32, wanted.unitSlice(), &.{unit});
            const held = if (current) |command| std.mem.count(i32, command.unitSlice(), &.{unit}) else 0;
            if (held != 0 and in_wanted <= held) continue;
            if (self.refusesStartUnit(unit)) return false;
            if (in_wanted > 1) {
                self.say("unit {d} is in the start command twice", .{unit});
                return false;
            }
        }
        const target_changed = current == null or current.?.target != wanted.target;
        if (wanted.target < 0 and target_changed) {
            self.say("a start command's target is a link ID above 0, or 0 for none", .{});
            return false;
        }
        if (wanted.target > 0 and target_changed and self.indexOf(wanted.target) == null) {
            self.say("the target object {d} is not on the map", .{wanted.target});
            return false;
        }
        if ((current == null or current.?.cmd_type != wanted.cmd_type) and !self.actionListed(wanted.cmd_type)) return false;
        if (!std.math.isFinite(wanted.x) or !std.math.isFinite(wanted.y) or !std.math.isFinite(wanted.number)) return false;
        const moved = current == null or current.?.x != wanted.x or current.?.y != wanted.y;
        if (moved and !self.onMapAt(wanted.x, wanted.y)) {
            self.say("the start command's target point is not on the map", .{});
            return false;
        }
        return true;
    }

    /// Assumption A3: a unit a reinforcement group holds back is no refusal, the
    /// answer just says so.
    fn warnHeldUnit(self: *FakeBridge, command: *const FakeStartCommand) void {
        for (command.unitSlice()) |unit| {
            const index = self.indexOf(unit) orelse continue;
            const object = &self.objects_list.items[index];
            if (object.scenario or object.script_id < 0) continue;
            var groups = self.groups.iterator();
            while (groups.next()) |entry| {
                if (std.mem.indexOfScalar(i32, entry.value_ptr.*, object.script_id) != null) {
                    self.say("unit {d} is held back by reinforcement group {d} until a script brings it in; its start command may not find it", .{ unit, entry.key_ptr.* });
                    return;
                }
            }
        }
    }

    fn finite(point: records.Vec3) bool {
        return std.math.isFinite(point.x) and std.math.isFinite(point.y) and std.math.isFinite(point.z);
    }

    fn setHiddenScriptIDs(ptr: *anyopaque, script_ids: []const i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        self.hidden_script_ids.clearRetainingCapacity();
        self.hidden_script_ids.appendSlice(self.allocator, script_ids) catch return .failed;
        return .ok;
    }

    /// Flat ground: 0 on the map. A simplification listed in the header.
    fn groundHeight(ptr: *anyopaque, wx: f32, wy: f32, z: *f32) Status {
        const self = from(ptr);
        self.message_len = 0;
        z.* = 0;
        if (!self.onMap(wx, wy)) {
            self.say("that point is not on the map", .{});
            return .refused;
        }
        return .ok;
    }

    fn paint(ptr: *anyopaque, cells: []const PaintCell, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        for (cells) |cell| {
            if (cell.x < 0 or cell.y < 0 or cell.x >= self.info.width_tiles or cell.y >= self.info.height_tiles) {
                self.say("cell {d},{d} is not on the map", .{ cell.x, cell.y });
                return .refused;
            }
        }
        if (self.refuse_paints) {
            self.say("the map would not take that paint", .{});
            return .refused;
        }
        const copy = self.allocator.dupe(PaintCell, cells) catch return .failed;
        const before = self.allocator.alloc(u8, cells.len) catch {
            self.allocator.free(copy);
            return .failed;
        };
        for (cells, 0..) |cell, index| {
            before[index] = self.tile(cell.x, cell.y);
            self.tiles[@intCast(cell.y * self.info.width_tiles + cell.x)] = cell.tile;
        }
        self.paints.append(self.allocator, .{ .cells = copy, .before = before }) catch return .failed;
        token.* = @intCast(self.paints.items.len - 1);
        self.applied.append(self.allocator, token.*) catch return .failed;
        self.undone.clearRetainingCapacity();
        self.record(.paint, token.*);
        return .ok;
    }

    fn undoPaint(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        if (self.applied.items.len == 0 or self.applied.items[self.applied.items.len - 1] != token) {
            self.say("paints are undone newest first", .{});
            return .refused;
        }
        const entry = self.paints.items[@intCast(token)];
        // Backwards, so a cell named twice in one paint ends at its first value.
        var index = entry.cells.len;
        while (index != 0) {
            index -= 1;
            const cell = entry.cells[index];
            self.tiles[@intCast(cell.y * self.info.width_tiles + cell.x)] = entry.before[index];
        }
        _ = self.applied.pop();
        self.undone.append(self.allocator, token) catch return .failed;
        self.record(.undo_paint, token);
        return .ok;
    }

    fn redoPaint(ptr: *anyopaque, token: i32) Status {
        const self = from(ptr);
        if (self.undone.items.len == 0 or self.undone.items[self.undone.items.len - 1] != token) {
            self.say("paints are redone in the order they were undone", .{});
            return .refused;
        }
        for (self.paints.items[@intCast(token)].cells) |cell| {
            self.tiles[@intCast(cell.y * self.info.width_tiles + cell.x)] = cell.tile;
        }
        _ = self.undone.pop();
        self.applied.append(self.allocator, token) catch return .failed;
        self.record(.redo_paint, token);
        return .ok;
    }

    fn screenToWorld(ptr: *anyopaque, sx: f32, sy: f32, wx: *f32, wy: *f32) Status {
        const self = from(ptr);
        if (!self.onMap(sx, sy)) return .refused;
        wx.* = sx;
        wy.* = sy;
        return .ok;
    }

    fn worldToTile(ptr: *anyopaque, wx: f32, wy: f32, tx: *i32, ty: *i32) Status {
        const self = from(ptr);
        if (!self.onMap(wx, wy)) return .refused;
        tx.* = @intFromFloat(@floor(wx / tile_size));
        ty.* = @intFromFloat(@floor(wy / tile_size));
        return .ok;
    }

    fn worldToMap(ptr: *anyopaque, wx: f32, wy: f32, mx: *f32, my: *f32) Status {
        const self = from(ptr);
        mx.* = wx * self.map_per_world;
        my.* = wy * self.map_per_world;
        return .ok;
    }

    /// What the engine's ghost visual holds: the entry, the world point, the turn.
    pub const Ghost = struct {
        name: [bridge_mod.name_capacity]u8 = undefined,
        name_len: usize = 0,
        x: f32 = 0,
        y: f32 = 0,
        dir: i32 = 0,

        pub fn nameSlice(self: *const Ghost) []const u8 {
            return self.name[0..self.name_len];
        }
    };

    /// Makes `setPlacementGhost` refuse `name`, the engine's answer for an entry
    /// the palette cannot place.
    pub fn refuseGhostFor(self: *FakeBridge, name: []const u8) void {
        self.ghost_refused_len = @min(name.len, self.ghost_refused_name.len);
        @memcpy(self.ghost_refused_name[0..self.ghost_refused_len], name[0..self.ghost_refused_len]);
    }

    fn setPlacementGhost(ptr: *anyopaque, name: []const u8, wx: f32, wy: f32, dir: i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        if (name.len == 0 or name.len > bridge_mod.name_capacity or !std.math.isFinite(wx) or !std.math.isFinite(wy)) return .bad_argument;
        self.ghost_sets += 1;
        if (!self.onMap(wx, wy) or (self.ghost_refused_len != 0 and std.mem.eql(u8, name, self.ghost_refused_name[0..self.ghost_refused_len]))) {
            self.ghost = null;
            self.say("the engine would not show the object there", .{});
            return .refused;
        }
        var ghost: Ghost = .{ .name_len = name.len, .x = wx, .y = wy, .dir = dir };
        @memcpy(ghost.name[0..name.len], name);
        self.ghost = ghost;
        return .ok;
    }

    fn clearPlacementGhost(ptr: *anyopaque) Status {
        from(ptr).ghost = null;
        return .ok;
    }

    fn placementGhost(ptr: *anyopaque, shown: *bool, wx: *f32, wy: *f32, dir: *i32) Status {
        const self = from(ptr);
        shown.* = self.ghost != null;
        wx.* = if (self.ghost) |ghost| ghost.x else 0;
        wy.* = if (self.ghost) |ghost| ghost.y else 0;
        dir.* = if (self.ghost) |ghost| ghost.dir else 0;
        return .ok;
    }

    fn objectAt(ptr: *anyopaque, sx: f32, sy: f32, link_id: *i32) Status {
        const self = from(ptr);
        // The last one listed wins, as the topmost drawn would.
        var index = self.objects_list.items.len;
        while (index != 0) {
            index -= 1;
            const object = self.objects_list.items[index];
            if (!object.known) continue; // never placed, so never drawn
            if (!object.scenario and std.mem.indexOfScalar(i32, self.hidden_script_ids.items, object.script_id) != null) continue; // held back
            // The screen is the world here; the object is drawn at its map
            // position's world point.
            const x = object.x / self.map_per_world;
            const y = object.y / self.map_per_world;
            if (@abs(x - sx) <= pick_radius and @abs(y - sy) <= pick_radius) {
                // A soldier answers his squad's link ID (M3, D-25), as the
                // real bridge answers it.
                link_id.* = self.pickLinkFor(object.link_id);
                return .ok;
            }
        }
        return .refused;
    }

    /// The drawn world point of an object, the same point `objectAt` picks
    /// by (the screen is the world here).
    fn drawnAt(self: *const FakeBridge, object: ObjectRecord) [2]f32 {
        return .{ object.x / self.map_per_world, object.y / self.map_per_world };
    }

    /// Whether a pick may name the record at all: the same rules `objectAt`
    /// picks by, and - for the tile pick - not a bridge span or a trench
    /// piece, which their groups edit (D-25), and no shared link ID, which
    /// no edit may name.
    fn pickable(self: *FakeBridge, object: ObjectRecord, tile_pick: bool) bool {
        if (!object.known) return false;
        if (!object.scenario and std.mem.indexOfScalar(i32, self.hidden_script_ids.items, object.script_id) != null) return false;
        if (!tile_pick) return true;
        if (self.bridgeHolding(object.link_id) != null) return false;
        if (self.trench_pieces.contains(object.link_id)) return false;
        return !self.shared(object.link_id);
    }

    fn pickObjects(ptr: *anyopaque, sx0: f32, sy0: f32, sx1: f32, sy1: f32, out: []i32, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        const left = @min(sx0, sx1);
        const right = @max(sx0, sx1);
        const top = @min(sy0, sy1);
        const bottom = @max(sy0, sy1);
        var links: std.ArrayListUnmanaged(i32) = .empty;
        defer links.deinit(self.allocator);
        // Topmost first, the drawn order `objectAt` picks in.
        var index = self.objects_list.items.len;
        while (index != 0) {
            index -= 1;
            const object = self.objects_list.items[index];
            if (!self.pickable(object, false)) continue;
            const at = self.drawnAt(object);
            if (at[0] < left or at[0] > right or at[1] < top or at[1] > bottom) continue;
            const link = self.pickLinkFor(object.link_id);
            if (std.mem.indexOfScalar(i32, links.items, link) == null)
                links.append(self.allocator, link) catch return .failed;
        }
        total.* = links.items.len;
        if (out.len < total.*) return .refused;
        @memcpy(out[0..total.*], links.items);
        return .ok;
    }

    fn pickObjectsInTiles(ptr: *anyopaque, tx0: i32, ty0: i32, tx1: i32, ty1: i32, out: []i32, total: *usize) Status {
        const self = from(ptr);
        self.message_len = 0;
        const left = @min(tx0, tx1);
        const right = @max(tx0, tx1);
        const top = @min(ty0, ty1);
        const bottom = @max(ty0, ty1);
        var links: std.ArrayListUnmanaged(i32) = .empty;
        defer links.deinit(self.allocator);
        for (self.objects_list.items) |object| {
            if (!self.pickable(object, true)) continue;
            // One tile is `tile_size` map units here, as `worldToTile` cuts
            // the map (32); a record is inside when its map position is.
            const tile_x = @divFloor(object.x, tile_size);
            const tile_y = @divFloor(object.y, tile_size);
            if (tile_x < @as(f32, @floatFromInt(left)) or tile_x > @as(f32, @floatFromInt(right))) continue;
            if (tile_y < @as(f32, @floatFromInt(top)) or tile_y > @as(f32, @floatFromInt(bottom))) continue;
            const link = self.pickLinkFor(object.link_id);
            if (std.mem.indexOfScalar(i32, links.items, link) == null)
                links.append(self.allocator, link) catch return .failed;
        }
        total.* = links.items.len;
        if (out.len < total.*) return .refused;
        @memcpy(out[0..total.*], links.items);
        return .ok;
    }

    fn moveObjects(ptr: *anyopaque, link_ids: []const i32, dx: f32, dy: f32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (link_ids.len == 0) {
            self.say("no objects to move", .{});
            return .refused;
        }
        if (!std.math.isFinite(dx) or !std.math.isFinite(dy)) return .bad_argument;
        // Everything checked first: one bad member refuses the whole move,
        // and nothing is touched (the real bridge's rule).
        var index_of: std.ArrayListUnmanaged(usize) = .empty;
        defer index_of.deinit(self.allocator);
        for (link_ids, 0..) |link_id, position| {
            if (std.mem.indexOfScalar(i32, link_ids[0..position], link_id) != null) {
                self.say("link ID {d} is named twice", .{link_id});
                return .refused;
            }
            const index = self.indexOf(link_id) orelse {
                self.say("no object with that link ID", .{});
                return .refused;
            };
            if (self.shared(link_id)) return .refused;
            const object = &self.objects_list.items[index];
            if (!object.known) {
                self.say("the object database does not know this object's type; it is kept as it is", .{});
                return .refused;
            }
            if (!self.onMapAt(object.x + dx, object.y + dy)) {
                self.say("object {d} would leave the map", .{link_id});
                return .refused;
            }
            index_of.append(self.allocator, index) catch return .failed;
        }
        var edit: FakeMoveObjectsEdit = .{};
        for (link_ids, index_of.items) |link_id, index| {
            const object = &self.objects_list.items[index];
            edit.moves.append(self.allocator, .{
                .link_id = link_id,
                .before_x = object.x,
                .before_y = object.y,
                .after_x = object.x + dx,
                .after_y = object.y + dy,
            }) catch {
                edit.moves.deinit(self.allocator);
                return .failed;
            };
            object.x += dx;
            object.y += dy;
        }
        self.edits.append(self.allocator, .{ .move_objects = edit }) catch return .failed;
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.append(self.allocator, token.*) catch return .failed;
        self.undone_edits.clearRetainingCapacity();
        self.record(.vso_edit, token.*);
        return .ok;
    }

    /// The batch move's undo and redo half: every member's position put back
    /// raw, reverse order on the way back.
    fn putMoves(self: *FakeBridge, moves: []const FakeMovedMember, back: bool) void {
        var index: usize = moves.len;
        while (index != 0) {
            index -= 1;
            const move = moves[index];
            if (self.indexOf(move.link_id)) |object_index| {
                const object = &self.objects_list.items[object_index];
                object.x = if (back) move.before_x else move.after_x;
                object.y = if (back) move.before_y else move.after_y;
            }
        }
    }

    /// Writes `wanted` over the object's record - the fields edit's undo and
    /// redo half, the whole record raw (a flag's swapped name included).
    fn putObjectRecordBack(self: *FakeBridge, link_id: i32, wanted: *const ObjectRecord) Status {
        const index = self.indexOf(link_id) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        self.objects_list.items[index] = wanted.*;
        return .ok;
    }

    fn setObjectFields(ptr: *anyopaque, link_id: i32, edit: *const bridge_mod.ObjectFieldsEdit, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (edit.mask == 0) return .bad_argument;
        const index = self.indexOf(link_id) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        if (self.shared(link_id)) return .refused;
        const object = &self.objects_list.items[index];
        if (!object.known) {
            self.say("the object database does not know this object's type; it is kept as it is", .{});
            return .refused;
        }
        var after = object.*;
        if (edit.mask & bridge_mod.ObjectFieldsEdit.player_bit != 0) {
            if (edit.player < 0 or @as(usize, @intCast(edit.player)) >= self.diplomacy_table.items.len) {
                self.say("{d} is no player: the map holds {d}", .{ edit.player, self.diplomacy_table.items.len });
                return .refused;
            }
            after.player = edit.player;
            // The flag swap: a re-owned Flag_ object takes the seeded name.
            if (std.mem.startsWith(u8, object.nameSlice(), "Flag_")) {
                if (self.flagSwapFor(edit.player)) |to_name| after.setName(to_name);
            }
        }
        if (edit.mask & bridge_mod.ObjectFieldsEdit.hp_bit != 0) {
            if (!std.math.isFinite(edit.hp)) return .bad_argument;
            after.hp = edit.hp;
        }
        if (edit.mask & bridge_mod.ObjectFieldsEdit.angle_bit != 0) {
            if (!std.math.isFinite(edit.angle)) return .bad_argument;
            after.dir = @intFromFloat((edit.angle * 65536.0) / 360.0 + 0.5);
        }
        if (edit.mask & bridge_mod.ObjectFieldsEdit.formation_bit != 0) {
            if (edit.formation < 0) {
                self.say("a formation index is 0 or greater", .{});
                return .refused;
            }
            if (!self.isSquad(link_id)) {
                self.say("only a squad carries a formation", .{});
                return .refused;
            }
            after.frame_index = edit.formation;
        }
        if (std.meta.eql(after, object.*)) return .ok; // nothing changed: no token
        const logged = FakeObjectFieldsEdit{ .link_id = link_id, .before = object.*, .after = after };
        self.edits.append(self.allocator, .{ .object_fields = logged }) catch return .failed;
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.append(self.allocator, token.*) catch return .failed;
        self.undone_edits.clearRetainingCapacity();
        self.objects_list.items[index] = after;
        self.record(.vso_edit, token.*);
        return .ok;
    }

    fn canLink(ptr: *anyopaque, source: i32, target: i32, link_type: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        link_type.* = 0;
        const source_index = self.indexOf(source) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        const target_index = self.indexOf(target) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        if (source == target) {
            self.say("an object does not link to itself", .{});
            return .refused;
        }
        const source_name = self.objects_list.items[source_index].nameSlice();
        const target_name = self.objects_list.items[target_index].nameSlice();
        const rule = self.linkRuleFor(source_name, target_name) orelse {
            self.say("{s} does not link to {s}", .{ source_name, target_name });
            return .refused;
        };
        link_type.* = rule;
        return .ok;
    }

    fn setLink(ptr: *anyopaque, source: i32, target: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        var link_type: i32 = 0;
        const can = canLink(ptr, source, target, &link_type);
        if (can != .ok) return can;
        const index = self.indexOf(source) orelse return .refused;
        if (self.shared(source) or self.objects_list.items[index].known == false) return .refused;
        const object = &self.objects_list.items[index];
        var after = object.*;
        after.link_with = target;
        if (link_type == 0) {
            // A garrison stands beside its host, the MFC's own offset.
            const host = &self.objects_list.items[self.indexOf(target).?];
            after.x = host.x - 30;
            after.y = host.y + 30;
        }
        if (std.meta.eql(after, object.*)) return .ok;
        const logged = FakeObjectFieldsEdit{ .link_id = source, .before = object.*, .after = after };
        self.edits.append(self.allocator, .{ .object_fields = logged }) catch return .failed;
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.append(self.allocator, token.*) catch return .failed;
        self.undone_edits.clearRetainingCapacity();
        self.objects_list.items[index] = after;
        self.record(.vso_edit, token.*);
        return .ok;
    }

    fn damageObject(ptr: *anyopaque, link_id: i32, delta: f32, mode: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        if (mode < 0 or mode > 2 or !std.math.isFinite(delta) or delta < 0 or delta > 1) return .bad_argument;
        const index = self.indexOf(link_id) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        if (self.shared(link_id)) return .refused;
        const object = &self.objects_list.items[index];
        if (!object.known or self.no_stats_fixture) {
            self.say("the object has no stats to damage", .{});
            return .refused;
        }
        // The MFC's clamps: a unit (the fake names units "T34"-style; every
        // known object here is a unit or a building - the floor is 1% for
        // both) keeps 1% under damage.
        const floor: f32 = 0.01;
        const before_hp = object.hp;
        var new_hp = before_hp;
        if (mode == 2) {
            new_hp = 1.0;
        } else {
            const move: f32 = if (mode == 1) -delta else delta;
            if (before_hp - move > 1.0) {
                new_hp = 1.0;
            } else if (before_hp - move < floor) {
                new_hp = floor;
            } else {
                new_hp = before_hp - move;
            }
        }
        if (new_hp == before_hp) return .ok; // nothing to change: no token
        const logged = FakeObjectFieldsEdit{ .link_id = link_id, .before = object.*, .after = blk: {
            var after = object.*;
            after.hp = new_hp;
            break :blk after;
        } };
        self.edits.append(self.allocator, .{ .object_fields = logged }) catch return .failed;
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.append(self.allocator, token.*) catch return .failed;
        self.undone_edits.clearRetainingCapacity();
        self.objects_list.items[index] = logged.after;
        self.record(.vso_edit, token.*);
        return .ok;
    }

    fn unlink(ptr: *anyopaque, link_id: i32, token: *i32) Status {
        const self = from(ptr);
        self.message_len = 0;
        token.* = -1;
        const index = self.indexOf(link_id) orelse {
            self.say("no object with that link ID", .{});
            return .refused;
        };
        if (self.shared(link_id)) return .refused;
        const object = &self.objects_list.items[index];
        if (object.link_with == 0) return .ok; // nothing linked: no token
        const logged = FakeObjectFieldsEdit{ .link_id = link_id, .before = object.*, .after = blk: {
            var after = object.*;
            after.link_with = 0;
            break :blk after;
        } };
        self.edits.append(self.allocator, .{ .object_fields = logged }) catch return .failed;
        token.* = @intCast(self.edits.items.len - 1);
        self.applied_edits.append(self.allocator, token.*) catch return .failed;
        self.undone_edits.clearRetainingCapacity();
        self.objects_list.items[index] = logged.after;
        self.record(.vso_edit, token.*);
        return .ok;
    }
};

/// The same three fixture objects every fake-bridge test starts from: a known
/// tank, a known object another holds a reference to, and an object of a type
/// the object database does not know. Public so later tasks' editor tests can
/// reuse it instead of copying it.
pub fn fixture(allocator: std.mem.Allocator) !FakeBridge {
    var fake = FakeBridge.init(allocator, 8, 8, 2);
    errdefer fake.deinit();
    var tank: ObjectRecord = .{ .link_id = 1, .x = 40, .y = 40, .dir = 0, .player = 0 };
    tank.setName("T34");
    try fake.addFixture(tank, false);
    var bridge_span: ObjectRecord = .{ .link_id = 2, .x = 100, .y = 40, .dir = 0, .player = 0 };
    bridge_span.setName("Bridge_Span");
    try fake.addFixture(bridge_span, true); // referred to, like a span named by a bridge
    var mystery: ObjectRecord = .{ .link_id = 3, .x = 150, .y = 150, .dir = 0, .player = 1, .known = false };
    mystery.setName("No_Such_Object");
    try fake.addFixture(mystery, false);
    try fake.addVsoDescriptorFixture(.road, "rail_road_grass");
    try fake.addVsoDescriptorFixture(.road, "road_track");
    try fake.addVsoDescriptorFixture(.river, "defaultriver");
    // The span above is bridge 0's only span, as a shipped one-span bridge.
    try fake.addBridgeEntryFixture(&.{2});
    // Bridge types: a horizontal/vertical pair, a WoodenBig_Heavy_ pair (the
    // only kind built during play), and one without a rotated variant. A span
    // is one tile long.
    try fake.addBridgeTypeFixture("W_Fake_Bridge_01", .horizontal, true, tile_size);
    try fake.addBridgeTypeFixture("W_Fake_Bridge_02", .vertical, true, tile_size);
    try fake.addBridgeTypeFixture("W_WoodenBig_Heavy_01", .horizontal, true, tile_size);
    try fake.addBridgeTypeFixture("W_WoodenBig_Heavy_02", .vertical, true, tile_size);
    try fake.addBridgeTypeFixture("Lonely_Bridge", .horizontal, false, tile_size);
    try fake.addFenceTypeFixture("W_Fake_Fence");
    try fake.addFenceTypeFixture("W_Fake_Fence_Wire");
    try addRmgFixtures(&fake);
    return fake;
}

/// The composers' shipped fixtures (05-09): three summer patches (one with a
/// script ID), a winter one, a container of the first two patches, a graph of
/// two nodes holding it and one link.
fn addRmgFixtures(fake: *FakeBridge) !void {
    const a = fake.allocator;
    try fake.addPatchFixture("scenarios\\patches\\summer\\road_a1", 1, 0, "terrain\\sets\\1\\", &.{3}, &.{"Ambush"});
    try fake.addPatchFixture("scenarios\\patches\\summer\\road_a2", 2, 0, "terrain\\sets\\1\\", &.{3}, &.{"Ambush"});
    try fake.addPatchFixture("scenarios\\patches\\summer\\road_b1", 1, 0, "terrain\\sets\\1\\", &.{9}, &.{});
    try fake.addPatchFixture("scenarios\\patches\\winter\\snow_a1", 1, 1, "terrain\\sets\\2\\", &.{3}, &.{"Ambush"});
    var container: rmg_mod.Container = .{};
    errdefer container.deinit(a);
    try container.addPatch(a, "scenarios\\patches\\summer\\road_a1", 1, 1);
    try container.addPatch(a, "scenarios\\patches\\summer\\road_a2", 2, 2);
    container.season_folder = try a.dupe(u8, "terrain\\sets\\1\\");
    try container.script_ids.append(a, 3);
    try container.script_areas.append(a, try a.dupe(u8, "Ambush"));
    try fake.addContainerFixture("scenarios\\containers\\summer\\road_a", true, container);
    var graph: rmg_mod.Graph = .{};
    errdefer graph.deinit(a);
    _ = try graph.addNode(a, .{ .x1 = 0, .y1 = 0, .x2 = 32, .y2 = 32 });
    _ = try graph.addNode(a, .{ .x1 = 48, .y1 = 0, .x2 = 80, .y2 = 32 });
    graph.nodes.items[0].container = try a.dupe(u8, "scenarios\\containers\\summer\\road_a");
    _ = try graph.addLink(a, 0, 1);
    graph.links.items[0].desc = try a.dupe(u8, "terrain\\sets\\1\\roads3d\\road_grunt");
    graph.season_folder = try a.dupe(u8, "terrain\\sets\\1\\");
    try graph.script_ids.append(a, 3);
    try graph.script_areas.append(a, try a.dupe(u8, "Ambush"));
    try fake.addGraphFixture("scenarios\\graphs\\summer\\graph_a", true, graph);
}

test "the fake refuses what the bridge refuses" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    try std.testing.expectEqual(Status.ok, b.openMap("fixture.bzm", &info));
    try std.testing.expectEqual(Status.refused, b.deleteObject(2)); // referenced
    try std.testing.expectEqual(Status.ok, b.deleteObject(3)); // unknown type: its removal is explicit (D-33); every other edit refuses
    try std.testing.expectEqual(Status.ok, b.restoreObject(3));
    try std.testing.expectEqual(Status.refused, b.placeObject(3, 10, 10, 0, 1)); // unknown type
    try std.testing.expectEqual(Status.refused, b.placeObject(1, -5, 10, 0, 0)); // off the map
    var link: i32 = -1;
    try std.testing.expectEqual(Status.refused, b.addObject("T34", 9999, 10, 0, 0, &link));
    try std.testing.expectEqual(Status.bad_argument, b.setDiplomacy(7, 0));
    try std.testing.expectEqual(Status.bad_argument, b.setDiplomacy(0, 3));
    try std.testing.expectEqual(Status.bad_argument, b.setDiplomacy(0, -1));
    try std.testing.expectEqual(Status.bad_argument, b.setAttackingSide(2));
}

test "the fake never reuses a deleted object's link ID" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    try std.testing.expectEqual(Status.ok, b.deleteObject(1));
    var added: i32 = -1;
    try std.testing.expectEqual(Status.ok, b.addObject("T34", 60, 60, 0, 0, &added));
    try std.testing.expect(added > 3);
    try std.testing.expectEqual(Status.ok, b.restoreObject(1));
    try std.testing.expectEqual(Status.refused, b.restoreObject(1));
}

test "the fake's paints undo newest first and redo in undo order" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    var first: i32 = -1;
    var second: i32 = -1;
    try std.testing.expectEqual(Status.ok, b.paint(&.{.{ .x = 1, .y = 1, .tile = 5 }}, &first));
    try std.testing.expectEqual(Status.ok, b.paint(&.{.{ .x = 1, .y = 1, .tile = 6 }}, &second));
    try std.testing.expectEqual(Status.refused, b.undoPaint(first));
    try std.testing.expectEqual(Status.ok, b.undoPaint(second));
    try std.testing.expectEqual(@as(u8, 5), fake.tile(1, 1));
    try std.testing.expectEqual(Status.ok, b.undoPaint(first));
    try std.testing.expectEqual(@as(u8, 0), fake.tile(1, 1));
    try std.testing.expectEqual(Status.refused, b.redoPaint(second));
    try std.testing.expectEqual(Status.ok, b.redoPaint(first));
    try std.testing.expectEqual(Status.refused, b.paint(&.{.{ .x = 8, .y = 0, .tile = 1 }}, &first)); // off the map
}

test "a reopen forgets deleted objects and paint tokens, as the bridge's does" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    var token: i32 = -1;
    try std.testing.expectEqual(Status.ok, b.paint(&.{.{ .x = 1, .y = 1, .tile = 5 }}, &token));
    try std.testing.expectEqual(Status.ok, b.deleteObject(1));
    try std.testing.expectEqual(Status.ok, b.openMap("fixture.bzm", &info));
    try std.testing.expectEqual(Status.refused, b.undoPaint(token));
    try std.testing.expectEqual(Status.refused, b.restoreObject(1));
}

test "the fake refuses an edit of a link ID two objects share" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    var twin: ObjectRecord = .{ .link_id = 1, .x = 60, .y = 60 };
    twin.setName("Flowers");
    try fake.addFixture(twin, false);
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    try std.testing.expectEqual(Status.refused, b.deleteObject(1));
    try std.testing.expectEqual(Status.refused, b.placeObject(1, 50, 50, 0, 0));
    try std.testing.expectEqual(@as(usize, 4), fake.objects_list.items.len);
}

test "the fake lists objects the way BkEditorObjects does" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    _ = b.openMap("fixture.bzm", &info);
    var total: usize = 0;
    var none: [0]ObjectRecord = .{};
    try std.testing.expectEqual(Status.refused, b.objects(&none, &total));
    try std.testing.expectEqual(@as(usize, 3), total);
    var out: [3]ObjectRecord = undefined;
    try std.testing.expectEqual(Status.ok, b.objects(&out, &total));
    try std.testing.expectEqualStrings("T34", out[0].nameSlice());
    try std.testing.expect(!out[2].known);
}

test "the fake's camera anchors refuse what the bridge refuses and put exactly" {
    var fake = try fixture(std.testing.allocator);
    defer fake.deinit();
    const b = fake.bridge();
    var info: MapInfo = .{};
    try std.testing.expectEqual(Status.ok, b.openMap("fixture.bzm", &info));
    var value: records.Value = undefined;
    try std.testing.expectEqual(Status.ok, b.readRecord(.camera_anchors, 0, std.testing.allocator, &value));
    try std.testing.expectEqual(@as(u32, 0), value.camera_anchors.player_count);
    var wanted: records.CameraAnchors = .{};
    wanted.player_count = 3;
    wanted.players[2] = .{ .x = 100, .y = 100, .z = 0 };
    var put: records.Value = .{ .camera_anchors = wanted };
    try std.testing.expectEqual(Status.ok, b.putRecord(0, &put));
    try std.testing.expectEqual(@as(u32, 3), fake.camera_anchors.player_count);
    // Off the map, a count over 32 and a non-finite value.
    put.camera_anchors.players[1] = .{ .x = 9999, .y = 1, .z = 0 };
    try std.testing.expectEqual(Status.refused, b.putRecord(0, &put));
    try std.testing.expect(fake.camera_anchors.players[1].isUnset());
    put.camera_anchors.players[1] = .{};
    put.camera_anchors.player_count = 33;
    try std.testing.expectEqual(Status.bad_argument, b.putRecord(0, &put));
    put.camera_anchors.player_count = 1;
    put.camera_anchors.players[0] = .{ .x = std.math.nan(f32), .y = 0, .z = 0 };
    try std.testing.expectEqual(Status.bad_argument, b.putRecord(0, &put));
    // An exact put shrinks: what undo needs.
    put.camera_anchors = .{};
    try std.testing.expectEqual(Status.ok, b.putRecord(0, &put));
    try std.testing.expectEqual(@as(u32, 0), fake.camera_anchors.player_count);
    var z: f32 = 5;
    try std.testing.expectEqual(Status.ok, b.groundHeight(10, 10, &z));
    try std.testing.expectEqual(@as(f32, 0), z);
    try std.testing.expectEqual(Status.refused, b.groundHeight(-1, 10, &z));
}
