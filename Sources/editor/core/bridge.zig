//! The core's view of the engine bridge (Sources/src/EditorBridge/bridge.h).
//! One call per C entry point, same arguments, same statuses, so the fake and
//! the real adapter (plan 5) can be read side by side. The core never sees a
//! C type: this file is plain Zig so the core builds on every target,
//! including x86_64-windows-gnu, where the engine C++ does not.
const std = @import("std");
const records = @import("records.zig");
const core_filters = @import("filters.zig");

pub const Status = enum(c_int) {
    ok = 0,
    bad_argument = 1,
    no_session = 2,
    no_device = 3,
    data_missing = 4,
    refused = 5,
    failed = 6,
};

/// What a command sees of a status: a refusal is an ordinary answer with a
/// message for the status bar, anything else is a failure.
pub const EditError = error{ Refused, Failed, OutOfMemory };

pub fn check(status: Status) EditError!void {
    return switch (status) {
        .ok => {},
        .refused => error.Refused,
        else => error.Failed,
    };
}

pub const MapInfo = struct {
    width_tiles: i32 = 0,
    height_tiles: i32 = 0,
    season: i32 = 0,
    player_count: i32 = 0,
    map_type: i32 = 0,
    attacking_side: i32 = 0,
};

pub const name_capacity = 64;

/// BkEditorObjectRecord.
pub const ObjectRecord = struct {
    link_id: i32 = -1,
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    x: f32 = 0,
    y: f32 = 0,
    dir: i32 = 0,
    player: i32 = 0,
    scenario: bool = false,
    known: bool = true,
    /// The map's nScriptID: -1 none, else 0..32000 (D-15).
    script_id: i32 = -1,
    /// The map's fHP, the record's own 0..1 (M3, D-26).
    hp: f32 = 1.0,
    /// The map's nFrameIndex: a squad's formation, a terraobj's segment (M3).
    frame_index: i32 = 0,
    /// The map's link.nLinkWith: the host's link ID, 0 none (M3, D-27).
    link_with: i32 = 0,

    pub fn nameSlice(self: *const ObjectRecord) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *ObjectRecord, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

/// BkEditorObjectFieldsEdit (M3, D-26), layout included: the masked fields of
/// one object's record. Mask bits: 1 player, 2 hp (the record's own 0..1),
/// 4 angle in DEGREES (the MFC properties' unit), 8 formation (a squad's).
pub const ObjectFieldsEdit = extern struct {
    pub const player_bit: c_int = 1;
    pub const hp_bit: c_int = 2;
    pub const angle_bit: c_int = 4;
    pub const formation_bit: c_int = 8;
    mask: c_int = 0,
    player: c_int = 0,
    hp: f32 = 1.0,
    angle: f32 = 0,
    formation: c_int = 0,
};

/// What BkEditorReserveRole answers (04-11, D-18).
pub const ReserveRole = enum(i32) { none = 0, self_propelled = 1, towed = 2, truck = 3 };

/// One action type of Data/Editor/actions.ini as BkEditorActionCommands lists
/// it (04-11, D-17): its name and the id a start command stores.
pub const ActionCommand = struct {
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    id: i32 = 0,

    pub fn nameSlice(self: *const ActionCommand) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *ActionCommand, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

/// BkEditorPaintCell, layout included: the adapter hands a slice of these
/// straight to the C call.
pub const PaintCell = extern struct { x: c_int, y: c_int, tile: u8 };

/// BkEditorObjectFilter's word lists (M3, D-31), layout included.
pub const filter_max_lists = 8;
pub const filter_max_words = 8;
pub const filter_word_capacity = 32;
pub const ObjectFilterWords = extern struct {
    word_count: c_int = 0,
    words: [filter_max_words][filter_word_capacity]u8 = [_][filter_word_capacity]u8{[_]u8{0} ** filter_word_capacity} ** filter_max_words,
};

/// BkEditorObjectFilter (M3, D-31), layout included: one named object filter
/// as the shipped Data/Editor/filter.xml and the user
/// <UserRoot>mapeditor/filter.xml carry it, merged. `user` is 1 when the
/// entry came from (or is overridden by) the user file and so belongs in the
/// next `saveObjectFilters` - a shipped name the user has not touched is 0.
pub const ObjectFilter = extern struct {
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    list_count: c_int = 0,
    user: c_int = 0,
    lists: [filter_max_lists]ObjectFilterWords = [_]ObjectFilterWords{.{}} ** filter_max_lists,

    pub fn nameSlice(self: *const ObjectFilter) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *ObjectFilter, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }

    /// Borrows this record as a `filters.Filter` for `matches`: every word
    /// slice points into the record's own buffers, and the scratch holds the
    /// slice arrays - both must outlive the returned Filter, which the
    /// caller's cache (refreshed when `filters_generation` moves) provides.
    pub fn view(self: *const ObjectFilter, scratch: *FilterView) core_filters.Filter {
        const n_lists: usize = @min(@as(usize, @intCast(@max(self.list_count, 0))), filter_max_lists);
        for (0..n_lists) |l| {
            const word_count: usize = @min(@as(usize, @intCast(@max(self.lists[l].word_count, 0))), filter_max_words);
            for (0..word_count) |w| {
                scratch.words[l][w] = std.mem.sliceTo(&self.lists[l].words[w], 0);
            }
            scratch.lists[l] = scratch.words[l][0..word_count];
        }
        return .{ .name = self.nameSlice(), .lists = scratch.lists[0..n_lists] };
    }
};

/// The per-`ObjectFilter.view` scratch: the word and word-list slices must
/// outlive the returned Filter, so they live here (one per live view - the
/// palette's is refreshed when `filters_generation` moves).
pub const FilterView = struct {
    words: [filter_max_lists][filter_max_words][]const u8 = undefined,
    lists: [filter_max_lists]core_filters.WordList = undefined,
};

/// BkEditorVec3 (the fields polygon's point), layout included. WORLD (Vis)
/// units; the fill reads the terrain, so z is ignored.
pub const FieldVec3 = extern struct { x: f32 = 0, y: f32 = 0, z: f32 = 0 };

/// BkEditorFieldApplyParams (M3, D-21), layout included: one fields
/// application. The rules are the C ABI's own; `points` borrows the
/// caller's array for the duration of the call.
pub const field_set_name_capacity = 192;
pub const FieldApplyParams = extern struct {
    field_set: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    point_count: c_int = 0,
    points: ?[*]const FieldVec3 = null,
    randomize: c_int = 0,
    min_length: f32 = 2.0,
    width: f32 = 0,
    disturbance: f32 = 0,
    fill_terrain: c_int = 1,
    place_objects: c_int = 1,
    modify_heights: c_int = 1,
    update_map_after: c_int = 0,
    check_passability_only: c_int = 0,
    can_add_object_filter: c_int = 0,
    object_filter: [name_capacity]u8 = [_]u8{0} ** name_capacity,

    pub fn fieldSetSlice(self: *const FieldApplyParams) []const u8 {
        return std.mem.sliceTo(&self.field_set, 0);
    }

    pub fn setFieldSet(self: *FieldApplyParams, text: []const u8) void {
        const len = @min(text.len, field_set_name_capacity - 1);
        @memset(&self.field_set, 0);
        @memcpy(self.field_set[0..len], text[0..len]);
    }

    pub fn setFilter(self: *FieldApplyParams, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.object_filter, 0);
        @memcpy(self.object_filter[0..len], text[0..len]);
    }
};

/// BkEditorFieldObjectReport: one object the object shells produced and
/// whether it was placed. x, y are AI (map) units.
pub const FieldObjectReport = extern struct {
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    x: f32 = 0,
    y: f32 = 0,
    placed: c_int = 0,

    pub fn nameSlice(self: *const FieldObjectReport) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }
};

/// BkEditorRmgName: one storage-relative RMG name.
pub const RmgName = extern struct {
    name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,

    pub fn nameSlice(self: *const RmgName) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }
};

/// One name of a unit-creation combo (BkEditorUcName, 05-05, D-30).
pub const UcName = extern struct {
    name: [64]u8 = [_]u8{0} ** 64,

    pub fn nameSlice(self: *const UcName) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }
};

/// The lists BkEditorUnitCreationChoices answers (D-30): the parties of
/// partys.xml, the aircraft of the aviation folders and the paratroop squads.
pub const UcChoice = enum(c_int) { parties = 0, aircraft = 1, squads = 2 };

/// The RMG folder kinds BkEditorListRmg walks (D-08).
pub const RmgKind = enum(c_int) { field_sets = 0, templates = 1, graphs = 2, containers = 3, settings = 4, chapters = 5 };

/// BkEditorAltitudeRegion (M3, D-19), layout included: terrain-VERTEX
/// indices, half-open [x0, x1) x [y0, y1) - altitudes are indexed by terrain
/// vertex, one more per axis than the map's tiles.
pub const AltitudeRegion = extern struct { x0: c_int, y0: c_int, x1: c_int, y1: c_int };

/// BkEditorHeightsStrokeParams (M3, D-18), layout included: one stroke step
/// of the Heights tool. `action` 0 raise, 1 lower, 2 level; `level_mode` 0
/// zero, 1 click tile, 2 instant average (the MFC's own default), 3 click
/// average; `brush` the MFC slider's own 2..16 (the pattern spans brush*2
/// vertices per axis); `height_speed` the profile gradient's ceiling and
/// `level_ratio_percent` the level step, world z units / percent; pos the
/// cursor now and click the stroke's start, both WORLD (Vis) units;
/// `stroke_start` marks the first step (where the click modes' frozen
/// targets are taken); `ctrl_held` keeps a height the engine's validity
/// predicate refuses.
pub const HeightsStrokeParams = extern struct {
    action: c_int = 0,
    level_mode: c_int = 2,
    brush: c_int = 3,
    height_speed: f32 = 1.0,
    level_ratio_percent: f32 = 3.0,
    pos_x: f32 = 0,
    pos_y: f32 = 0,
    click_x: f32 = 0,
    click_y: f32 = 0,
    stroke_start: c_int = 0,
    ctrl_held: c_int = 0,
};

/// The Heights tool's Generate types (M3, D-18): the MFC dialog's three
/// names; the hidden MULTI/HETERO radios are not features. The values are
/// the engine's own ETerGenAlgs (TerrainGenerator.h:14-23).
pub const HeightsGenerateType = enum(c_int) { hills = 0, rocks = 3, dunes = 4 };

/// What a heights stroke does (the buttons decide, the MFC's own precedence).
pub const HeightsAction = enum(c_int) { raise = 0, lower = 1, level = 2 };

/// Update Map's progress callback (BkEditorProgressFn): called once per step
/// with the step number (from 1), the MFC's own total (7 + the snapped
/// objects) and the caller's own pointer. Must not call back into the bridge.
/// C ABI (`callconv(.c)`), so the pointer passes straight through
/// c_bridge.zig to BkEditorUpdateMap.
pub const ProgressFn = *const fn (step: c_int, total: c_int, user: ?*anyopaque) callconv(.c) void;

/// What a level stroke moves the terrain toward (the MFC's LEVEL_TO_0..3,
/// default LEVEL_TO_2 - instant average).
pub const HeightsLevelMode = enum(c_int) { zero = 0, click_tile = 1, instant_average = 2, click_average = 3 };

/// BkEditorNewMapParams (M3, D-23), layout included. Sizes are in PATCHES
/// per axis (1..32), season 0..3 (Summer/Winter/Africa/Spring), and the mod
/// folder is "" (keep the current mod), "none", or a bare folder name the
/// bridge's mod list must hold.
pub const NewMapParams = extern struct {
    size_x: c_int = 8,
    size_y: c_int = 8,
    season: c_int = 0,
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    mod_folder: [name_capacity]u8 = [_]u8{0} ** name_capacity,

    pub fn nameSlice(self: *const NewMapParams) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *NewMapParams, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }

    pub fn modSlice(self: *const NewMapParams) []const u8 {
        return std.mem.sliceTo(&self.mod_folder, 0);
    }

    pub fn setMod(self: *NewMapParams, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.mod_folder, 0);
        @memcpy(self.mod_folder[0..len], text[0..len]);
    }
};

/// BkEditorSoundRecord. Positions are world (scene) units, not map units
/// (bridge.h's own comment on BkEditorSounds); radii are vis tiles; times
/// are milliseconds.
pub const SoundRecord = struct {
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    x: f32 = 0,
    y: f32 = 0,
    z: f32 = 0,
    repeat_ms: i32 = 0,
    repeat_random_ms: i32 = 0,
    mute_in_combat: bool = false,
    min_radius: i32 = 0,
    max_radius: i32 = 0,

    pub fn nameSlice(self: *const SoundRecord) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *SoundRecord, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

/// Roads (the map's roads3) and rivers: the C ABI's kind 0 and 1.
pub const VsoKind = enum(u8) {
    road = 0,
    river = 1,

    pub fn label(self: VsoKind) []const u8 {
        return switch (self) {
            .road => "road",
            .river => "river",
        };
    }
};

/// The MFC width modes (CTabVOVSODialog::CW_SINGLE/MULTI/ALL): an edit of a
/// point changes that point only, that point and every later one, or every
/// point. The C ABI's mode 0, 1, 2.
pub const VsoWidthMode = enum(u8) { single = 0, multi = 1, all = 2 };

/// A road or river of the map: its kind and its index in that kind's list.
pub const VsoRef = struct { kind: VsoKind, index: usize };

/// BkEditorVsoDescriptor's and BkEditorVsoInfo's name capacity.
pub const vso_name_capacity = 128;

/// A road or river type: the bare descriptor name (no folder, no extension)
/// as BkEditorVsoDescriptors lists it.
pub const VsoDescriptor = struct {
    name: [vso_name_capacity]u8 = [_]u8{0} ** vso_name_capacity,

    pub fn nameSlice(self: *const VsoDescriptor) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *VsoDescriptor, text: []const u8) void {
        const len = @min(text.len, vso_name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

/// BkEditorVsoKeyPoint: a key point's sampled position (world units, z the
/// ground's), its normal (a unit vector across the stripe), its width (world
/// units, centre line to edge) and its opacity (0..1). The width handles sit
/// at position +- normal * width.
pub const VsoKeyPoint = struct {
    x: f32 = 0,
    y: f32 = 0,
    z: f32 = 0,
    nx: f32 = 0,
    ny: f32 = 0,
    nz: f32 = 0,
    width: f32 = 0,
    opacity: f32 = 0,
};

/// One road or river as BkEditorVso reads it: the nID the file holds, the
/// descriptor's full saved name, and owned copies of its control points and
/// key points (world units). `deinit` with the allocator `readVso` was given.
pub const VsoView = struct {
    saved_id: i32 = 0,
    desc: [vso_name_capacity]u8 = [_]u8{0} ** vso_name_capacity,
    control_points: []records.Vec3 = &.{},
    key_points: []VsoKeyPoint = &.{},

    pub fn descSlice(self: *const VsoView) []const u8 {
        return std.mem.sliceTo(&self.desc, 0);
    }

    pub fn deinit(self: *VsoView, allocator: std.mem.Allocator) void {
        allocator.free(self.control_points);
        allocator.free(self.key_points);
        self.* = .{};
    }
};

/// Bridges (04-06, D-10..D-12). A bridge type as BkEditorBridgeDescriptors
/// lists it: its name, its direction (the drag must run along it), whether
/// its rotated `_01`/`_02` variant exists, and whether it may be built during
/// play (a WoodenBig_Heavy_ type).
pub const BridgeDirection = enum(u8) { vertical = 0, horizontal = 1 };

pub const BridgeDescriptor = struct {
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    direction: BridgeDirection = .horizontal,
    has_partner: bool = false,
    build_during_play_allowed: bool = false,

    pub fn nameSlice(self: *const BridgeDescriptor) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *BridgeDescriptor, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

/// BkEditorPlannedPiece: one span a drag would place - its position in MAP
/// units, its packed frame type (1 begin, 2 middle, 4 end) and its direction.
pub const PlannedPiece = struct { x: f32 = 0, y: f32 = 0, type: i32 = 0, dir: i32 = 0 };

/// BkEditorBridgeInfo: one bridges entry - its type (the first span's name),
/// how many spans it names, the box of their positions (MAP units) and
/// whether it is built during play.
pub const BridgeInfo = struct {
    desc: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    span_count: i32 = 0,
    min_x: f32 = 0,
    min_y: f32 = 0,
    max_x: f32 = 0,
    max_y: f32 = 0,
    built_during_play: bool = false,

    pub fn descSlice(self: *const BridgeInfo) []const u8 {
        return std.mem.sliceTo(&self.desc, 0);
    }

    pub fn setDesc(self: *BridgeInfo, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.desc, 0);
        @memcpy(self.desc[0..len], text[0..len]);
    }
};

/// A fence type as BkEditorFenceDescriptors lists it (04-07, D-14): its name.
pub const FenceDescriptor = struct {
    name: [name_capacity]u8 = [_]u8{0} ** name_capacity,

    pub fn nameSlice(self: *const FenceDescriptor) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *FenceDescriptor, text: []const u8) void {
        const len = @min(text.len, name_capacity - 1);
        @memset(&self.name, 0);
        @memcpy(self.name[0..len], text[0..len]);
    }
};

/// BkEditorEntrenchmentInfo (04-08): one entrenchments entry - how many
/// pieces and sections it names, its first piece's player and the box of its
/// pieces' positions (MAP units).
pub const EntrenchmentInfo = struct {
    piece_count: i32 = 0,
    section_count: i32 = 0,
    player: i32 = 0,
    min_x: f32 = 0,
    min_y: f32 = 0,
    max_x: f32 = 0,
    max_y: f32 = 0,
};

/// The packed trench piece types a planned piece carries (BkEditorPlannedPiece
/// .type for a trench): 1 line, 2 fireplace, 4 terminator, 8 arc.
pub const trench_line: i32 = 1;
pub const trench_fireplace: i32 = 2;
pub const trench_terminator: i32 = 4;
pub const trench_arc: i32 = 8;

/// BkEditorPickGroup's kinds: a bridge span picks its bridges entry, a trench
/// piece its entrenchment.
pub const GroupKind = enum(u8) { bridge = 1, entrenchment = 2 };
pub const GroupRef = struct { kind: GroupKind, index: usize };

/// The Damage tool's modes (M3, D-29), the C ABI's own numbers.
pub const DamageMode = enum(i32) { damage = 0, heal = 1, repair_full = 2 };

pub const Bridge = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    pub const VTable = struct {
        lastMessage: *const fn (ptr: *anyopaque) []const u8,
        openMap: *const fn (ptr: *anyopaque, path: []const u8, info: *MapInfo) Status,
        saveMap: *const fn (ptr: *anyopaque, path: []const u8) Status,
        objects: *const fn (ptr: *anyopaque, out: []ObjectRecord, total: *usize) Status,
        diplomacy: *const fn (ptr: *anyopaque, player: i32, value: *i32) Status,
        addObject: *const fn (ptr: *anyopaque, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status,
        placeObject: *const fn (ptr: *anyopaque, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status,
        deleteObject: *const fn (ptr: *anyopaque, link_id: i32) Status,
        restoreObject: *const fn (ptr: *anyopaque, link_id: i32) Status,
        setDiplomacy: *const fn (ptr: *anyopaque, player: i32, value: i32) Status,
        setMapType: *const fn (ptr: *anyopaque, value: i32) Status,
        setAttackingSide: *const fn (ptr: *anyopaque, value: i32) Status,
        paint: *const fn (ptr: *anyopaque, cells: []const PaintCell, token: *i32) Status,
        undoPaint: *const fn (ptr: *anyopaque, token: i32) Status,
        redoPaint: *const fn (ptr: *anyopaque, token: i32) Status,
        screenToWorld: *const fn (ptr: *anyopaque, sx: f32, sy: f32, wx: *f32, wy: *f32) Status,
        worldToTile: *const fn (ptr: *anyopaque, wx: f32, wy: f32, tx: *i32, ty: *i32) Status,
        /// World units (the camera's, screenToWorld's) to map units (every
        /// object position's). BkEditorWorldToMap.
        worldToMap: *const fn (ptr: *anyopaque, wx: f32, wy: f32, mx: *f32, my: *f32) Status,
        objectAt: *const fn (ptr: *anyopaque, sx: f32, sy: f32, link_id: *i32) Status,
        /// BkEditorPickObjects (M3, D-25): the rubber band's pick - the link
        /// IDs of every object the screen rectangle (window pixels, corners
        /// in any order) selects, soldiers answered by their squad's link,
        /// bridges and entrenchments passed over. Two-pass like `sounds`
        /// (`total` is always the full count; a short buffer is refused).
        /// The order is the pick's own, which the Selector's cycle walks.
        pickObjects: *const fn (ptr: *anyopaque, sx0: f32, sy0: f32, sx1: f32, sy1: f32, out: []i32, total: *usize) Status,
        /// BkEditorPickObjectsInTiles (M3, D-25): the Ctrl band's pick - the
        /// editable records whose tile position falls inside the rectangle
        /// of tiles (`tile` units, as `worldToTile` answers them), bridges
        /// and entrenchments passed over. Two-pass like `pickObjects`.
        pickObjectsInTiles: *const fn (ptr: *anyopaque, tx0: i32, ty0: i32, tx1: i32, ty1: i32, out: []i32, total: *usize) Status,
        /// BkEditorMoveObjects (M3, D-25): every member of `link_ids` moved
        /// by one (dx, dy) delta in MAP units, ONE edit of the log (`token`
        /// names it for undoEdit/redoEdit). A refusal (a member that cannot
        /// be moved, a destination off the map) changes nothing and the
        /// whole move is refused.
        moveObjects: *const fn (ptr: *anyopaque, link_ids: []const i32, dx: f32, dy: f32, token: *i32) Status,
        /// BkEditorSetObjectFields (M3, D-26): the masked fields of one
        /// object's record, ONE edit of the log (`token`, -1 when nothing
        /// changed). The flag swap rides the player bit; the formation bit
        /// is refused for a kind that carries none.
        setObjectFields: *const fn (ptr: *anyopaque, link_id: i32, edit: *const ObjectFieldsEdit, token: *i32) Status,
        /// BkEditorCanLink (M3, D-27): CheckForInserting's rules as a
        /// question - `link_type` 0 garrison, 1 train coupling, 2 tow;
        /// refused naming the rule when none holds. A read.
        canLink: *const fn (ptr: *anyopaque, source: i32, target: i32, link_type: *i32) Status,
        /// BkEditorSetLink (M3, D-27): the passenger's nLinkWith becomes the
        /// host's link ID, ONE edit of the log; a garrison moves the
        /// passenger beside the host. Refused with canLink's reason.
        setLink: *const fn (ptr: *anyopaque, source: i32, target: i32, token: *i32) Status,
        /// BkEditorUnlink (M3, D-27): the record's nLinkWith back to 0, ONE
        /// edit of the log (`token` -1 when nothing was linked).
        unlink: *const fn (ptr: *anyopaque, link_id: i32, token: *i32) Status,
        /// BkEditorDamageObject (M3, D-29): the record's fHP moved by
        /// `delta` (the tool's percentage/100) with the MFC's clamps, the
        /// engine's live object damaged the same share; ONE edit of the
        /// log. `mode` 0 damage, 1 heal, 2 repair to full. Missing stats
        /// refuse (the MFC's null dereference is not copied); the clamps
        /// leaving nothing to change answer -1.
        damageObject: *const fn (ptr: *anyopaque, link_id: i32, delta: f32, mode: i32, token: *i32) Status,
        /// BkEditorSounds. Like `objects`: `total` is always the full count,
        /// so a caller sizes `out` from a first sizing call the way
        /// `document.reload` does for `objects`.
        sounds: *const fn (ptr: *anyopaque, out: []SoundRecord, total: *usize) Status,
        /// BkEditorAddSound. `index` is 0..count to insert there, -1 to
        /// append - the C call's own sentinel, kept as-is rather than an
        /// optional, since the core passes it straight through.
        addSound: *const fn (ptr: *anyopaque, index: i32, record: SoundRecord) Status,
        /// BkEditorSetSound.
        setSound: *const fn (ptr: *anyopaque, index: i32, record: SoundRecord) Status,
        /// BkEditorDeleteSound.
        deleteSound: *const fn (ptr: *anyopaque, index: i32) Status,
        /// Reads the whole record of `kind` at `key` (an index or ID; 0 for a
        /// singleton) into `out`, allocating any owned memory with `allocator`.
        /// The camera anchors: BkEditorCameraAnchors, world units.
        readRecord: *const fn (ptr: *anyopaque, kind: records.Kind, key: i32, allocator: std.mem.Allocator, out: *records.Value) Status,
        /// A raw put of the whole record at `key`, the kind taken from the
        /// union tag: the same call an edit, an undo and a redo all use. The
        /// camera anchors: BkEditorSetCameraAnchors, an exact put (the vector
        /// becomes exactly `player_count` long), world units.
        putRecord: *const fn (ptr: *anyopaque, key: i32, value: *const records.Value) Status,
        /// The keys of the records of `kind`, ascending, allocated with
        /// `allocator` into `out` (the caller frees it on .ok only): the
        /// group IDs of the map's reinforcement groups (BkEditorGroupIDs), and
        /// for the camera anchors the single key 0.
        recordKeys: *const fn (ptr: *anyopaque, kind: records.Kind, allocator: std.mem.Allocator, out: *[]i32) Status,
        /// A record put in that was not there (the kind from the union tag):
        /// a new group (BkEditorSetGroup, refused when the ID is taken). The
        /// camera anchors are a singleton and take no insert (bad_argument).
        insertRecord: *const fn (ptr: *anyopaque, key: i32, value: *const records.Value) Status,
        /// A record taken out: BkEditorDeleteGroup for a group, refused when
        /// there is none. The camera anchors cannot be removed (bad_argument).
        removeRecord: *const fn (ptr: *anyopaque, kind: records.Kind, key: i32) Status,
        /// BkEditorFirstFreeGroupID: the first group ID at or above `from`
        /// (clamped to 0) that no group uses (C9).
        firstFreeGroupID: *const fn (ptr: *anyopaque, from: i32, out: *i32) Status,
        /// BkEditorSetHiddenScriptIDs (04-09, D-16): the objects of the map's
        /// objects list whose script ID is in `script_ids` are hidden in the
        /// view and skipped by picking; an empty set shows everything. A view
        /// setting, never saved and never in the history.
        setHiddenScriptIDs: *const fn (ptr: *anyopaque, script_ids: []const i32) Status,
        /// BkEditorGroundHeight: the terrain height at a world point, world
        /// units in and out. Refused off the map.
        groundHeight: *const fn (ptr: *anyopaque, wx: f32, wy: f32, z: *f32) Status,
        /// BkEditorSetObjectScriptID (04-09, D-15): -1 none, else 0..32000.
        /// Refused for an unknown or shared link ID, link ID 0 and a value out
        /// of range.
        setObjectScriptID: *const fn (ptr: *anyopaque, link_id: i32, script_id: i32) Status,
        /// BkEditorScriptAreaFromVis / Moved / Resized (04-10, D-21): the MFC
        /// conversion of a drag or a handle, world (Vis) units in, AI units out,
        /// no map touched. `name` may be empty here (the add refuses that).
        scriptAreaFromVis: *const fn (ptr: *anyopaque, shape: records.AreaShape, wx0: f32, wy0: f32, wx1: f32, wy1: f32, name: []const u8, out: *records.ScriptArea) Status,
        scriptAreaMoved: *const fn (ptr: *anyopaque, area: records.ScriptArea, wx: f32, wy: f32, out: *records.ScriptArea) Status,
        scriptAreaResized: *const fn (ptr: *anyopaque, area: records.ScriptArea, wx: f32, wy: f32, out: *records.ScriptArea) Status,
        /// BkEditorActionCommands (04-11, D-17): the action types of
        /// Data/Editor/actions.ini in the file's order, allocated with
        /// `allocator` into `out` (the caller frees it on .ok only), and the
        /// entry a new command starts at. Refused, naming why, when the file is
        /// not in the data or lists nothing.
        actionCommands: *const fn (ptr: *anyopaque, allocator: std.mem.Allocator, out: *[]ActionCommand, default_index: *usize) Status,
        /// BkEditorReserveRole (04-11, D-18): what an object type can be in a
        /// reserve position - 0 nothing, 1 a self-propelled gun, 2 a towed gun,
        /// 3 a truck able to tow - from its stats, as the MFC editor classifies
        /// them. A name the database does not know, a squad and anything else is 0.
        reserveRole: *const fn (ptr: *anyopaque, name: []const u8, role: *i32) Status,
        /// BkEditorUndoEdit / BkEditorRedoEdit: the bridge's edit log, the
        /// paints' order (newest first; redo in the order undone). A token
        /// out of order is refused.
        undoEdit: *const fn (ptr: *anyopaque, token: i32) Status,
        redoEdit: *const fn (ptr: *anyopaque, token: i32) Status,
        /// BkEditorAltitudes (M3, D-19): the terrain vertex heights (WORLD z
        /// units) over the region, row-major, two-pass like `sounds` (`total`
        /// is always the full count; a short buffer is refused).
        altitudes: *const fn (ptr: *anyopaque, region: AltitudeRegion, heights: []f32, total: *usize) Status,
        /// BkEditorSetAltitudes: the heights (WORLD z units) over the region,
        /// row-major, `count == the region's vertex count`; one edit of the
        /// bridge's log (`token` names it for undoEdit/redoEdit). The bridge
        /// sets the heights, recomputes the shades over the region grown by
        /// the shade kernel (one vertex per side) and pushes the covering
        /// patches into the engine; undo restores the recorded region raw.
        /// Refused, changing nothing, for a region off the map; bad
        /// heights (null, non-finite, count mismatch) never reach the map.
        setAltitudes: *const fn (ptr: *anyopaque, region: AltitudeRegion, heights: []const f32, token: *i32) Status,
        /// BkEditorNewMap (M3, D-23): the engine builds a map of the params
        /// (sizes in patches 1..32, season 0..3, the name for the terrain
        /// loader's sidecars, the mod folder - "", "none" or an installed
        /// one, switched first when it differs) and it opens as the
        /// session's map, never-saved. The summary answers what was built.
        newMap: *const fn (ptr: *anyopaque, params: NewMapParams, info: *MapInfo) Status,
        /// BkEditorHeightsStroke (M3, D-18): one stroke step of the Heights
        /// tool - the MFC DrawShadeState machine over the profile pattern,
        /// the click modes' frozen targets taken on the stroke-start step.
        /// One bridge edit per step (the token names it for
        /// undoEdit/redoEdit); a step whose result fails the engine's
        /// height-validity predicate is refused ("invalid height") and
        /// changes nothing unless ctrl_held is set.
        heightsStroke: *const fn (ptr: *anyopaque, params: HeightsStrokeParams, token: *i32) Status,
        /// BkEditorGenerateHeights (M3, D-18): the engine's own noise over
        /// the whole vertex sheet, one edit. The confirmation is the
        /// caller's.
        generateHeights: *const fn (ptr: *anyopaque, gen_type: HeightsGenerateType, granularity: f32, min_z: f32, max_z: f32, token: *i32) Status,
        /// BkEditorSetZeroHeights (M3, D-18): every height to 0, shades
        /// recomputed, one edit. The confirmation is the caller's.
        setZeroHeights: *const fn (ptr: *anyopaque, token: *i32) Status,
        /// BkEditorUpdateMap (M3, D-20): the OnButtonUpdate composite as one
        /// edit - engine height/terrain updates, full crosses and shades,
        /// the VSO z refresh, the fit pass. `progress` (nullable) is called
        /// once per step with the MFC's own total and must not call back.
        updateMap: *const fn (ptr: *anyopaque, progress: ?ProgressFn, user: ?*anyopaque, token: *i32) Status,
        /// BkEditorFillEntireMap (M3, D-22): every tile becomes the type's
        /// own, crosses recomputed over the whole map - one PAINT of the
        /// log (the token names it for undoPaint/redoPaint, exactly a
        /// brush paint's own). The confirmation is the caller's.
        fillEntireMap: *const fn (ptr: *anyopaque, tile: u8, token: *i32) Status,
        /// BkEditorSetTerrainModes (M3, D-20): the Instant Update and Fit To
        /// Grid toggles. A view setting: no map data, no history.
        setTerrainModes: *const fn (ptr: *anyopaque, instant_update: bool, fit_to_grid: bool) Status,
        /// BkEditorSnapToGrid (M3, D-20): the MFC placer's own question -
        /// where would the fit put (x, y) for this object type? The session's
        /// fit flag decides; the kinds the rule does not fit answer the
        /// input. A read: nothing is edited, nothing is recorded.
        snapToGrid: *const fn (ptr: *anyopaque, name: [*:0]const u8, x: f32, y: f32, out_x: *f32, out_y: *f32) Status,
        /// BkEditorVsoDescriptors: the season's road or river types, bare
        /// names, sorted. `total` is always the full count (two-pass, like
        /// `sounds`).
        vsoDescriptors: *const fn (ptr: *anyopaque, kind: VsoKind, out: []VsoDescriptor, total: *usize) Status,
        /// BkEditorVsoCount.
        vsoCount: *const fn (ptr: *anyopaque, kind: VsoKind, count: *usize) Status,
        /// BkEditorVso: the record at `index`, its point arrays allocated with
        /// `allocator` into `out` (the caller deinits it on .ok only).
        readVso: *const fn (ptr: *anyopaque, kind: VsoKind, index: i32, allocator: std.mem.Allocator, out: *VsoView) Status,
        /// BkEditorAddVso: a road or river through `points` (world units, z
        /// ignored), type `desc` (a bare name), `width_tiles` 1..16, `opacity`
        /// 0..1; appended, `index` is where it landed, `token` names the edit
        /// for undoEdit/redoEdit. Refused naming why: an unknown type, a point
        /// off the map, a line too short to be a road.
        addVso: *const fn (ptr: *anyopaque, kind: VsoKind, desc: []const u8, points: []const records.Vec3, width_tiles: f32, opacity: f32, token: *i32, index: *i32) Status,
        /// BkEditorDeleteVso: the whole road or river at `index`, one edit.
        deleteVso: *const fn (ptr: *anyopaque, kind: VsoKind, index: i32, token: *i32) Status,
        /// BkEditorMoveVsoPoints: every control point of the line at `index`
        /// (world units), resampled keeping its key points; one edit.
        moveVsoPoints: *const fn (ptr: *anyopaque, kind: VsoKind, index: i32, points: []const records.Vec3, token: *i32) Status,
        /// BkEditorSetVsoWidth: the width (world units) at key point `key`
        /// in `mode`; one edit.
        setVsoWidth: *const fn (ptr: *anyopaque, kind: VsoKind, index: i32, key: i32, width: f32, mode: VsoWidthMode, token: *i32) Status,
        /// BkEditorSetVsoOpacity: the opacity (0..1) at key point `key` in
        /// `mode`, nothing resampled; one edit.
        setVsoOpacity: *const fn (ptr: *anyopaque, kind: VsoKind, index: i32, key: i32, opacity: f32, mode: VsoWidthMode, token: *i32) Status,
        /// BkEditorInsertVsoPoint: the midpoint after control point `control`
        /// (before it when it is the last); one edit.
        insertVsoPoint: *const fn (ptr: *anyopaque, kind: VsoKind, index: i32, control: i32, token: *i32) Status,
        /// BkEditorDeleteVsoPoint: removes control point `control`; refused
        /// while only 2 remain; one edit.
        deleteVsoPoint: *const fn (ptr: *anyopaque, kind: VsoKind, index: i32, control: i32, token: *i32) Status,
        /// BkEditorPickVso: the road or river under a world point, roads
        /// first; `cycle` skips that many earlier hits. Refused when none.
        pickVso: *const fn (ptr: *anyopaque, wx: f32, wy: f32, cycle: i32, kind: *VsoKind, index: *i32) Status,
        /// BkEditorBridgeDescriptors: every bridge type, sorted; `total` is
        /// always the full count (two-pass).
        bridgeDescriptors: *const fn (ptr: *anyopaque, out: []BridgeDescriptor, total: *usize) Status,
        /// BkEditorPlanBridge: the spans a drag (WORLD units) of type `desc`
        /// would place, changing nothing; `total` is always the planned count.
        /// Refused naming why (a drag along the other axis, a bad type).
        planBridge: *const fn (ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, out: []PlannedPiece, total: *usize) Status,
        /// BkEditorDrawBridge: the planned spans and a new bridges entry, one
        /// edit (`token`); `index` is the entry's. Refused, changing nothing,
        /// for the plan's refusals and a span off the map.
        drawBridge: *const fn (ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, token: *i32, index: *i32) Status,
        /// BkEditorBridges: every bridges entry in list order (two-pass).
        bridges: *const fn (ptr: *anyopaque, out: []BridgeInfo, total: *usize) Status,
        /// BkEditorPickGroup: the bridge or entrenchment under a screen
        /// point. Refused when neither is there.
        pickGroup: *const fn (ptr: *anyopaque, sx: f32, sy: f32, kind: *GroupKind, index: *i32) Status,
        /// BkEditorDeleteBridge: the whole bridge at `index`, one edit.
        deleteBridge: *const fn (ptr: *anyopaque, index: i32, token: *i32) Status,
        /// BkEditorRotateBridge: the `_01`/`_02` partner about the same
        /// centre, same span count, same index; one edit. Refused naming why
        /// (no partner, off the map).
        rotateBridge: *const fn (ptr: *anyopaque, index: i32, token: *i32) Status,
        /// BkEditorToggleBridgeBuild: intact <-> built during play; one edit.
        /// Refused unless a WoodenBig_Heavy_ type.
        toggleBridgeBuild: *const fn (ptr: *anyopaque, index: i32, token: *i32) Status,
        /// BkEditorFenceDescriptors: every fence type, sorted (two-pass).
        fenceDescriptors: *const fn (ptr: *anyopaque, out: []FenceDescriptor, total: *usize) Status,
        /// BkEditorPlanFences: the fences a drag (WORLD units) of type `desc`
        /// would place, changing nothing (`ctrl` flips a single fence);
        /// `total` is always the planned count. Refused naming why (a bad
        /// type, an end off the map).
        planFences: *const fn (ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, out: []PlannedPiece, total: *usize) Status,
        /// BkEditorDrawFences: the planned fences as objects, one edit
        /// (`token`). Refused, changing nothing, for the plan's refusals and
        /// a fence the engine will not place.
        drawFences: *const fn (ptr: *anyopaque, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, token: *i32) Status,
        /// BkEditorPlanEntrenchment (04-08): the pieces the clicks (WORLD
        /// units) would commit, changing nothing; `total` is always the
        /// planned count. Refused naming why (shorter than a piece, off the
        /// map).
        planEntrenchment: *const fn (ptr: *anyopaque, points: []const records.Vec3, out: []PlannedPiece, total: *usize) Status,
        /// BkEditorDrawEntrenchment: the planned pieces and a new
        /// entrenchments entry for `player`, one edit (`token`); `index` is
        /// the entry's. Refused, changing nothing, for the plan's refusals.
        drawEntrenchment: *const fn (ptr: *anyopaque, points: []const records.Vec3, player: i32, token: *i32, index: *i32) Status,
        /// BkEditorEntrenchments: every entrenchments entry in list order
        /// (two-pass).
        entrenchments: *const fn (ptr: *anyopaque, out: []EntrenchmentInfo, total: *usize) Status,
        /// BkEditorDeleteEntrenchment: the whole entrenchment at `index`, one
        /// edit. Refused for one that holds units or has a piece the editor
        /// could not put back.
        deleteEntrenchment: *const fn (ptr: *anyopaque, index: i32, token: *i32) Status,
        /// BkEditorObjectFilters (M3, D-31): the shipped filter.xml merged
        /// with the user file, ordered by name, allocated with `allocator`
        /// into `out` (the caller frees it on .ok only). Not map data: works
        /// with no map open. Refused when a file holds more filters than the
        /// first pass answered (two-pass).
        objectFilters: *const fn (ptr: *anyopaque, allocator: std.mem.Allocator, out: *[]ObjectFilter) Status,
        /// BkEditorSaveObjectFilters (M3, D-31): writes `filters` (the ones
        /// authored or edited - `user` 1) to <UserRoot>mapeditor/filter.xml
        /// in the shipped file's own XML shape. A refusal changes nothing.
        saveObjectFilters: *const fn (ptr: *anyopaque, filters: []const ObjectFilter) Status,
        /// BkEditorApplyField (M3, D-21): the fields application as ONE edit
        /// (`token`, -1 after a refusal). `report` is sized by the caller
        /// from the first pass (`total` is always the full count); null with
        /// `report.len == 0` sizes. `check_passability_only` writes the
        /// report and changes nothing.
        applyField: *const fn (ptr: *anyopaque, params: FieldApplyParams, report: []FieldObjectReport, total: *usize, token: *i32) Status,
        /// BkEditorFieldSetSeason (M3, D-21): the set's season, for the
        /// app's YES/NO confirmation before an apply.
        fieldSetSeason: *const fn (ptr: *anyopaque, name: [*:0]const u8, season: *i32) Status,
        /// BkEditorListRmg (M3, D-08): the storage folder's bare names,
        /// sorted (two-pass, like `vsoDescriptors`).
        listRmg: *const fn (ptr: *anyopaque, kind: RmgKind, out: []RmgName, total: *usize) Status,
        /// BkEditorAddPlayer (05-05, D-30): a player of `side` (0 or 1) before
        /// the neutral entry, ONE bridge-logged edit (`token`, -1 after a
        /// refusal). The diplomacies, unit creation, camera anchors and every
        /// re-owned object undo and redo through `undoEdit`/`redoEdit`.
        addPlayer: *const fn (ptr: *anyopaque, side: i32, token: *i32) Status,
        /// BkEditorDeletePlayer: the same for deleting player `player` (never the
        /// neutral); its objects become the neutral's.
        deletePlayer: *const fn (ptr: *anyopaque, player: i32, token: *i32) Status,
        /// BkEditorUnitCreationChoices: the names a unit-creation combo offers,
        /// two-pass like `listRmg`.
        unitCreationChoices: *const fn (ptr: *anyopaque, kind: UcChoice, out: []UcName, total: *usize) Status,
    };

    pub fn addPlayer(self: Bridge, side: i32, token: *i32) Status { return self.vtable.addPlayer(self.ptr, side, token); }
    pub fn deletePlayer(self: Bridge, player: i32, token: *i32) Status { return self.vtable.deletePlayer(self.ptr, player, token); }
    pub fn unitCreationChoices(self: Bridge, kind: UcChoice, out: []UcName, total: *usize) Status { return self.vtable.unitCreationChoices(self.ptr, kind, out, total); }
    pub fn lastMessage(self: Bridge) []const u8 { return self.vtable.lastMessage(self.ptr); }
    pub fn openMap(self: Bridge, path: []const u8, info: *MapInfo) Status { return self.vtable.openMap(self.ptr, path, info); }
    pub fn saveMap(self: Bridge, path: []const u8) Status { return self.vtable.saveMap(self.ptr, path); }
    pub fn objects(self: Bridge, out: []ObjectRecord, total: *usize) Status { return self.vtable.objects(self.ptr, out, total); }
    pub fn diplomacy(self: Bridge, player: i32, value: *i32) Status { return self.vtable.diplomacy(self.ptr, player, value); }
    pub fn addObject(self: Bridge, name: []const u8, x: f32, y: f32, dir: i32, player: i32, link_id: *i32) Status { return self.vtable.addObject(self.ptr, name, x, y, dir, player, link_id); }
    pub fn placeObject(self: Bridge, link_id: i32, x: f32, y: f32, dir: i32, player: i32) Status { return self.vtable.placeObject(self.ptr, link_id, x, y, dir, player); }
    pub fn deleteObject(self: Bridge, link_id: i32) Status { return self.vtable.deleteObject(self.ptr, link_id); }
    pub fn restoreObject(self: Bridge, link_id: i32) Status { return self.vtable.restoreObject(self.ptr, link_id); }
    pub fn setDiplomacy(self: Bridge, player: i32, value: i32) Status { return self.vtable.setDiplomacy(self.ptr, player, value); }
    pub fn setMapType(self: Bridge, value: i32) Status { return self.vtable.setMapType(self.ptr, value); }
    pub fn setAttackingSide(self: Bridge, value: i32) Status { return self.vtable.setAttackingSide(self.ptr, value); }
    pub fn paint(self: Bridge, cells: []const PaintCell, token: *i32) Status { return self.vtable.paint(self.ptr, cells, token); }
    pub fn undoPaint(self: Bridge, token: i32) Status { return self.vtable.undoPaint(self.ptr, token); }
    pub fn redoPaint(self: Bridge, token: i32) Status { return self.vtable.redoPaint(self.ptr, token); }
    pub fn screenToWorld(self: Bridge, sx: f32, sy: f32, wx: *f32, wy: *f32) Status { return self.vtable.screenToWorld(self.ptr, sx, sy, wx, wy); }
    pub fn worldToTile(self: Bridge, wx: f32, wy: f32, tx: *i32, ty: *i32) Status { return self.vtable.worldToTile(self.ptr, wx, wy, tx, ty); }
    pub fn worldToMap(self: Bridge, wx: f32, wy: f32, mx: *f32, my: *f32) Status { return self.vtable.worldToMap(self.ptr, wx, wy, mx, my); }
    pub fn objectAt(self: Bridge, sx: f32, sy: f32, link_id: *i32) Status { return self.vtable.objectAt(self.ptr, sx, sy, link_id); }
    /// Two-pass like `sounds`: a sizing call with an empty buffer puts the
    /// total in `total`, a second call with room reads it.
    pub fn pickObjects(self: Bridge, sx0: f32, sy0: f32, sx1: f32, sy1: f32, out: []i32, total: *usize) Status { return self.vtable.pickObjects(self.ptr, sx0, sy0, sx1, sy1, out, total); }
    pub fn pickObjectsInTiles(self: Bridge, tx0: i32, ty0: i32, tx1: i32, ty1: i32, out: []i32, total: *usize) Status { return self.vtable.pickObjectsInTiles(self.ptr, tx0, ty0, tx1, ty1, out, total); }
    pub fn moveObjects(self: Bridge, link_ids: []const i32, dx: f32, dy: f32, token: *i32) Status { return self.vtable.moveObjects(self.ptr, link_ids, dx, dy, token); }
    pub fn setObjectFields(self: Bridge, link_id: i32, edit: *const ObjectFieldsEdit, token: *i32) Status { return self.vtable.setObjectFields(self.ptr, link_id, edit, token); }
    pub fn canLink(self: Bridge, source: i32, target: i32, link_type: *i32) Status { return self.vtable.canLink(self.ptr, source, target, link_type); }
    pub fn setLink(self: Bridge, source: i32, target: i32, token: *i32) Status { return self.vtable.setLink(self.ptr, source, target, token); }
    pub fn unlink(self: Bridge, link_id: i32, token: *i32) Status { return self.vtable.unlink(self.ptr, link_id, token); }
    pub fn damageObject(self: Bridge, link_id: i32, delta: f32, mode: i32, token: *i32) Status { return self.vtable.damageObject(self.ptr, link_id, delta, mode, token); }
    pub fn sounds(self: Bridge, out: []SoundRecord, total: *usize) Status { return self.vtable.sounds(self.ptr, out, total); }
    pub fn addSound(self: Bridge, index: i32, rec: SoundRecord) Status { return self.vtable.addSound(self.ptr, index, rec); }
    pub fn setSound(self: Bridge, index: i32, rec: SoundRecord) Status { return self.vtable.setSound(self.ptr, index, rec); }
    pub fn deleteSound(self: Bridge, index: i32) Status { return self.vtable.deleteSound(self.ptr, index); }
    pub fn readRecord(self: Bridge, kind: records.Kind, key: i32, allocator: std.mem.Allocator, out: *records.Value) Status { return self.vtable.readRecord(self.ptr, kind, key, allocator, out); }
    pub fn putRecord(self: Bridge, key: i32, value: *const records.Value) Status { return self.vtable.putRecord(self.ptr, key, value); }
    pub fn recordKeys(self: Bridge, kind: records.Kind, allocator: std.mem.Allocator, out: *[]i32) Status { return self.vtable.recordKeys(self.ptr, kind, allocator, out); }
    pub fn insertRecord(self: Bridge, key: i32, value: *const records.Value) Status { return self.vtable.insertRecord(self.ptr, key, value); }
    pub fn removeRecord(self: Bridge, kind: records.Kind, key: i32) Status { return self.vtable.removeRecord(self.ptr, kind, key); }
    pub fn firstFreeGroupID(self: Bridge, from: i32, out: *i32) Status { return self.vtable.firstFreeGroupID(self.ptr, from, out); }
    pub fn setHiddenScriptIDs(self: Bridge, script_ids: []const i32) Status { return self.vtable.setHiddenScriptIDs(self.ptr, script_ids); }
    pub fn groundHeight(self: Bridge, wx: f32, wy: f32, z: *f32) Status { return self.vtable.groundHeight(self.ptr, wx, wy, z); }
    pub fn setObjectScriptID(self: Bridge, link_id: i32, script_id: i32) Status { return self.vtable.setObjectScriptID(self.ptr, link_id, script_id); }
    pub fn scriptAreaFromVis(self: Bridge, shape: records.AreaShape, wx0: f32, wy0: f32, wx1: f32, wy1: f32, name: []const u8, out: *records.ScriptArea) Status { return self.vtable.scriptAreaFromVis(self.ptr, shape, wx0, wy0, wx1, wy1, name, out); }
    pub fn scriptAreaMoved(self: Bridge, area: records.ScriptArea, wx: f32, wy: f32, out: *records.ScriptArea) Status { return self.vtable.scriptAreaMoved(self.ptr, area, wx, wy, out); }
    pub fn scriptAreaResized(self: Bridge, area: records.ScriptArea, wx: f32, wy: f32, out: *records.ScriptArea) Status { return self.vtable.scriptAreaResized(self.ptr, area, wx, wy, out); }
    pub fn actionCommands(self: Bridge, allocator: std.mem.Allocator, out: *[]ActionCommand, default_index: *usize) Status { return self.vtable.actionCommands(self.ptr, allocator, out, default_index); }
    pub fn reserveRole(self: Bridge, name: []const u8, role: *i32) Status { return self.vtable.reserveRole(self.ptr, name, role); }
    pub fn undoEdit(self: Bridge, token: i32) Status { return self.vtable.undoEdit(self.ptr, token); }
    pub fn redoEdit(self: Bridge, token: i32) Status { return self.vtable.redoEdit(self.ptr, token); }
    pub fn altitudes(self: Bridge, region: AltitudeRegion, heights: []f32, total: *usize) Status { return self.vtable.altitudes(self.ptr, region, heights, total); }
    pub fn setAltitudes(self: Bridge, region: AltitudeRegion, heights: []const f32, token: *i32) Status { return self.vtable.setAltitudes(self.ptr, region, heights, token); }
    pub fn newMap(self: Bridge, params: NewMapParams, info: *MapInfo) Status { return self.vtable.newMap(self.ptr, params, info); }
    pub fn heightsStroke(self: Bridge, params: HeightsStrokeParams, token: *i32) Status { return self.vtable.heightsStroke(self.ptr, params, token); }
    pub fn generateHeights(self: Bridge, gen_type: HeightsGenerateType, granularity: f32, min_z: f32, max_z: f32, token: *i32) Status { return self.vtable.generateHeights(self.ptr, gen_type, granularity, min_z, max_z, token); }
    pub fn setZeroHeights(self: Bridge, token: *i32) Status { return self.vtable.setZeroHeights(self.ptr, token); }
    pub fn updateMap(self: Bridge, progress: ?ProgressFn, user: ?*anyopaque, token: *i32) Status { return self.vtable.updateMap(self.ptr, progress, user, token); }
    pub fn fillEntireMap(self: Bridge, tile: u8, token: *i32) Status { return self.vtable.fillEntireMap(self.ptr, tile, token); }
    pub fn setTerrainModes(self: Bridge, instant_update: bool, fit_to_grid: bool) Status { return self.vtable.setTerrainModes(self.ptr, instant_update, fit_to_grid); }
    pub fn snapToGrid(self: Bridge, name: [*:0]const u8, x: f32, y: f32, out_x: *f32, out_y: *f32) Status { return self.vtable.snapToGrid(self.ptr, name, x, y, out_x, out_y); }
    pub fn vsoDescriptors(self: Bridge, kind: VsoKind, out: []VsoDescriptor, total: *usize) Status { return self.vtable.vsoDescriptors(self.ptr, kind, out, total); }
    pub fn vsoCount(self: Bridge, kind: VsoKind, count: *usize) Status { return self.vtable.vsoCount(self.ptr, kind, count); }
    pub fn readVso(self: Bridge, kind: VsoKind, index: i32, allocator: std.mem.Allocator, out: *VsoView) Status { return self.vtable.readVso(self.ptr, kind, index, allocator, out); }
    pub fn moveVsoPoints(self: Bridge, kind: VsoKind, index: i32, points: []const records.Vec3, token: *i32) Status { return self.vtable.moveVsoPoints(self.ptr, kind, index, points, token); }
    pub fn setVsoWidth(self: Bridge, kind: VsoKind, index: i32, key: i32, width: f32, mode: VsoWidthMode, token: *i32) Status { return self.vtable.setVsoWidth(self.ptr, kind, index, key, width, mode, token); }
    pub fn setVsoOpacity(self: Bridge, kind: VsoKind, index: i32, key: i32, opacity: f32, mode: VsoWidthMode, token: *i32) Status { return self.vtable.setVsoOpacity(self.ptr, kind, index, key, opacity, mode, token); }
    pub fn insertVsoPoint(self: Bridge, kind: VsoKind, index: i32, control: i32, token: *i32) Status { return self.vtable.insertVsoPoint(self.ptr, kind, index, control, token); }
    pub fn deleteVsoPoint(self: Bridge, kind: VsoKind, index: i32, control: i32, token: *i32) Status { return self.vtable.deleteVsoPoint(self.ptr, kind, index, control, token); }
    pub fn pickVso(self: Bridge, wx: f32, wy: f32, cycle: i32, kind: *VsoKind, index: *i32) Status { return self.vtable.pickVso(self.ptr, wx, wy, cycle, kind, index); }
    pub fn deleteVso(self: Bridge, kind: VsoKind, index: i32, token: *i32) Status { return self.vtable.deleteVso(self.ptr, kind, index, token); }
    pub fn bridgeDescriptors(self: Bridge, out: []BridgeDescriptor, total: *usize) Status { return self.vtable.bridgeDescriptors(self.ptr, out, total); }
    pub fn planBridge(self: Bridge, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, out: []PlannedPiece, total: *usize) Status { return self.vtable.planBridge(self.ptr, desc, wx0, wy0, wx1, wy1, out, total); }
    pub fn drawBridge(self: Bridge, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, token: *i32, index: *i32) Status { return self.vtable.drawBridge(self.ptr, desc, wx0, wy0, wx1, wy1, token, index); }
    pub fn bridges(self: Bridge, out: []BridgeInfo, total: *usize) Status { return self.vtable.bridges(self.ptr, out, total); }
    pub fn pickGroup(self: Bridge, sx: f32, sy: f32, kind: *GroupKind, index: *i32) Status { return self.vtable.pickGroup(self.ptr, sx, sy, kind, index); }
    pub fn deleteBridge(self: Bridge, index: i32, token: *i32) Status { return self.vtable.deleteBridge(self.ptr, index, token); }
    pub fn rotateBridge(self: Bridge, index: i32, token: *i32) Status { return self.vtable.rotateBridge(self.ptr, index, token); }
    pub fn toggleBridgeBuild(self: Bridge, index: i32, token: *i32) Status { return self.vtable.toggleBridgeBuild(self.ptr, index, token); }
    pub fn fenceDescriptors(self: Bridge, out: []FenceDescriptor, total: *usize) Status { return self.vtable.fenceDescriptors(self.ptr, out, total); }
    pub fn planFences(self: Bridge, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, out: []PlannedPiece, total: *usize) Status { return self.vtable.planFences(self.ptr, desc, wx0, wy0, wx1, wy1, ctrl, out, total); }
    pub fn drawFences(self: Bridge, desc: []const u8, wx0: f32, wy0: f32, wx1: f32, wy1: f32, ctrl: bool, token: *i32) Status { return self.vtable.drawFences(self.ptr, desc, wx0, wy0, wx1, wy1, ctrl, token); }
    pub fn planEntrenchment(self: Bridge, points: []const records.Vec3, out: []PlannedPiece, total: *usize) Status { return self.vtable.planEntrenchment(self.ptr, points, out, total); }
    pub fn drawEntrenchment(self: Bridge, points: []const records.Vec3, player: i32, token: *i32, index: *i32) Status { return self.vtable.drawEntrenchment(self.ptr, points, player, token, index); }
    pub fn entrenchments(self: Bridge, out: []EntrenchmentInfo, total: *usize) Status { return self.vtable.entrenchments(self.ptr, out, total); }
    pub fn deleteEntrenchment(self: Bridge, index: i32, token: *i32) Status { return self.vtable.deleteEntrenchment(self.ptr, index, token); }
    pub fn objectFilters(self: Bridge, allocator: std.mem.Allocator, out: *[]ObjectFilter) Status { return self.vtable.objectFilters(self.ptr, allocator, out); }
    pub fn saveObjectFilters(self: Bridge, filters: []const ObjectFilter) Status { return self.vtable.saveObjectFilters(self.ptr, filters); }
    pub fn applyField(self: Bridge, params: FieldApplyParams, report: []FieldObjectReport, total: *usize, token: *i32) Status { return self.vtable.applyField(self.ptr, params, report, total, token); }
    pub fn fieldSetSeason(self: Bridge, name: [*:0]const u8, season: *i32) Status { return self.vtable.fieldSetSeason(self.ptr, name, season); }
    pub fn listRmg(self: Bridge, kind: RmgKind, out: []RmgName, total: *usize) Status { return self.vtable.listRmg(self.ptr, kind, out, total); }
    pub fn addVso(self: Bridge, kind: VsoKind, desc: []const u8, points: []const records.Vec3, width_tiles: f32, opacity: f32, token: *i32, index: *i32) Status { return self.vtable.addVso(self.ptr, kind, desc, points, width_tiles, opacity, token, index); }
};

test "check turns a refusal into Refused and everything else into Failed" {
    try check(.ok);
    try std.testing.expectError(error.Refused, check(.refused));
    try std.testing.expectError(error.Failed, check(.failed));
    try std.testing.expectError(error.Failed, check(.no_device));
}

test "a paint cell has the C struct's layout" {
    try std.testing.expectEqual(@as(usize, 12), @sizeOf(PaintCell));
    try std.testing.expectEqual(@as(usize, 8), @offsetOf(PaintCell, "tile"));
}

test "an altitude region has the C struct's layout" {
    try std.testing.expectEqual(@as(usize, 16), @sizeOf(AltitudeRegion));
    try std.testing.expectEqual(@as(usize, 0), @offsetOf(AltitudeRegion, "x0"));
    try std.testing.expectEqual(@as(usize, 4), @offsetOf(AltitudeRegion, "y0"));
    try std.testing.expectEqual(@as(usize, 8), @offsetOf(AltitudeRegion, "x1"));
    try std.testing.expectEqual(@as(usize, 12), @offsetOf(AltitudeRegion, "y1"));
}

test "new map params have the C struct's layout" {
    try std.testing.expectEqual(@as(usize, 12 + 2 * name_capacity), @sizeOf(NewMapParams));
    try std.testing.expectEqual(@as(usize, 12), @offsetOf(NewMapParams, "name"));
    try std.testing.expectEqual(@as(usize, 12 + name_capacity), @offsetOf(NewMapParams, "mod_folder"));
}

test "a heights stroke has the C struct's layout" {
    // 3 ints, 6 floats, 2 ints - 44 bytes, no padding in either run.
    try std.testing.expectEqual(@as(usize, 44), @sizeOf(HeightsStrokeParams));
    try std.testing.expectEqual(@as(usize, 12), @offsetOf(HeightsStrokeParams, "height_speed"));
    try std.testing.expectEqual(@as(usize, 20), @offsetOf(HeightsStrokeParams, "pos_x"));
    try std.testing.expectEqual(@as(usize, 28), @offsetOf(HeightsStrokeParams, "click_x"));
    try std.testing.expectEqual(@as(usize, 36), @offsetOf(HeightsStrokeParams, "stroke_start"));
    try std.testing.expectEqual(@as(usize, 40), @offsetOf(HeightsStrokeParams, "ctrl_held"));
}

test "an object filter has the C struct's layout" {
    try std.testing.expectEqual(@as(usize, 4 + filter_max_words * filter_word_capacity), @sizeOf(ObjectFilterWords));
    try std.testing.expectEqual(@as(usize, name_capacity + 4 + 4 + filter_max_lists * (4 + filter_max_words * filter_word_capacity)), @sizeOf(ObjectFilter));
    try std.testing.expectEqual(@as(usize, name_capacity), @offsetOf(ObjectFilter, "list_count"));
    try std.testing.expectEqual(@as(usize, name_capacity + 4), @offsetOf(ObjectFilter, "user"));
    try std.testing.expectEqual(@as(usize, name_capacity + 8), @offsetOf(ObjectFilter, "lists"));
}
