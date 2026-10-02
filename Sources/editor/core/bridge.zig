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

/// BkEditorRmgGraph (05-08, D-13): one graph of a template with its weight.
pub const RmgGraph = extern struct {
    name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    weight: c_int = 0,

    pub fn nameSlice(self: *const RmgGraph) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }
};

/// BkEditorRmgScripts (05-09): a record's script ID and area lists as the
/// caller sizes them. `*_count` is always the total; an array shorter than its
/// count is filled as far as it fits and the call is `.refused` (the sizing
/// pass). A write ignores the capacities and reads each array by its count.
pub const RmgScripts = extern struct {
    ids: ?[*]c_int = null,
    id_capacity: c_int = 0,
    id_count: c_int = 0,
    areas: ?[*]RmgName = null,
    area_capacity: c_int = 0,
    area_count: c_int = 0,
};

/// BkEditorRmgPatch: one patch of a container (a storage name without
/// extension, its size in patches, its setting - empty is any).
pub const RmgPatch = extern struct {
    name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    size_x: c_int = 0,
    size_y: c_int = 0,
    place: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
};

/// BkEditorRmgContainerRecord, layout included: SRMContainer through the
/// engine's own serialiser. The four direction lists (North, East, South,
/// West) are ONE flat `indices` array of `index_counts` entries each, in that
/// order; an index is a position in `patches`.
pub const RmgContainerRecord = extern struct {
    size_x: c_int = 0,
    size_y: c_int = 0,
    season: c_int = 0,
    season_folder: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    patches: ?[*]RmgPatch = null,
    patch_capacity: c_int = 0,
    patch_count: c_int = 0,
    indices: ?[*]c_int = null,
    index_capacity: c_int = 0,
    index_counts: [4]c_int = .{ 0, 0, 0, 0 },
    scripts: RmgScripts = .{},
};

/// BkEditorRmgNode / BkEditorRmgLink / BkEditorRmgGraphRecord: SRMGraph. A
/// node's rectangle is in VIS tiles (16 per patch, y up, max exclusive); a
/// link's radius and min length are WORLD units (the MFC dialog shows them
/// divided by 32), `kind` 0 road, 1 river.
pub const RmgNode = extern struct {
    x1: c_int = 0,
    y1: c_int = 0,
    x2: c_int = 0,
    y2: c_int = 0,
    container: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
};

pub const RmgLink = extern struct {
    a: c_int = 0,
    b: c_int = 0,
    kind: c_int = 0,
    desc: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    radius: f32 = 0,
    parts: c_int = 0,
    min_length: f32 = 0,
    distance: f32 = 0,
    disturbance: f32 = 0,
};

pub const RmgGraphRecord = extern struct {
    size_x: c_int = 0,
    size_y: c_int = 0,
    season: c_int = 0,
    season_folder: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    nodes: ?[*]RmgNode = null,
    node_capacity: c_int = 0,
    node_count: c_int = 0,
    links: ?[*]RmgLink = null,
    link_capacity: c_int = 0,
    link_count: c_int = 0,
    scripts: RmgScripts = .{},
};

/// BkEditorRmgPatchInfo: what a patch map says about itself.
pub const RmgPatchInfo = extern struct {
    size_x: c_int = 0,
    size_y: c_int = 0,
    season: c_int = 0,
    season_folder: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    scripts: RmgScripts = .{},
};

/// BkEditorRmgWeightedName / WeightedTile / TileShell / ObjectShell /
/// FieldSetRecord (05-10): SRMFieldSet through the engine's own serialiser.
/// The shells' entries are two flat arrays in shell order (a shell's
/// `tile_count` / `object_count` says how many are its own); every count is the
/// TOTAL and a short array is filled as far as it fits (two-pass, like the
/// container record).
pub const RmgWeightedName = extern struct {
    name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    weight: c_int = 0,
};
pub const RmgWeightedTile = extern struct { tile: c_int = 0, weight: c_int = 0 };
pub const RmgTileShell = extern struct { width: f32 = 0, tile_count: c_int = 0 };
pub const RmgObjectShell = extern struct { width: f32 = 0, step: c_int = 0, ratio: f32 = 0, object_count: c_int = 0 };
pub const RmgFieldSetRecord = extern struct {
    season: c_int = 0,
    season_folder: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    profile: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    height: f32 = 0,
    pattern_min: c_int = 0,
    pattern_max: c_int = 0,
    positive_ratio: f32 = 0,
    tile_shells: ?[*]RmgTileShell = null,
    tile_shell_capacity: c_int = 0,
    tile_shell_count: c_int = 0,
    tiles: ?[*]RmgWeightedTile = null,
    tile_capacity: c_int = 0,
    tile_total: c_int = 0,
    object_shells: ?[*]RmgObjectShell = null,
    object_shell_capacity: c_int = 0,
    object_shell_count: c_int = 0,
    objects: ?[*]RmgWeightedName = null,
    object_capacity: c_int = 0,
    object_total: c_int = 0,
};

/// BkEditorUnitCreationRecord's layout (05-05): one player's unit creation as the
/// C side hands it over - the template record carries one per player.
pub const RmgUnitAircraft = extern struct {
    name: [64]u8 = [_]u8{0} ** 64,
    formation_size: c_int = 0,
    count: c_int = 0,
};
pub const RmgVec3 = extern struct { x: f32 = 0, y: f32 = 0, z: f32 = 0 };
pub const RmgUnit = extern struct {
    slot_count: c_int = 0,
    party: [64]u8 = [_]u8{0} ** 64,
    aircraft: [5]RmgUnitAircraft = [_]RmgUnitAircraft{.{}} ** 5,
    paratroop_name: [64]u8 = [_]u8{0} ** 64,
    paratroop_count: c_int = 0,
    relax_time: c_int = 0,
    appear_count: c_int = 0,
    appear: [32]RmgVec3 = [_]RmgVec3{.{}} ** 32,
};

/// BkEditorRmgVso: a template's road or river descriptor with its weight; the
/// width is in WORLD units (the dialog shows it divided by the 32-unit cell),
/// the opacity 0..1.
pub const RmgVso = extern struct {
    name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    weight: c_int = 0,
    width: f32 = 0,
    opacity: f32 = 0,
};

/// BkEditorRmgTemplateRecord (05-10): SRMTemplate through the engine's own
/// serialiser, every list caller-sized and two-pass like the containers'.
pub const RmgTemplateRecord = extern struct {
    size_x: c_int = 0,
    size_y: c_int = 0,
    season: c_int = 0,
    season_folder: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    place: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    default_field: c_int = -1,
    mission_index: c_int = 0,
    game_type: c_int = 0,
    attacking_side: c_int = 0,
    camera: [3]f32 = .{ 0, 0, 0 },
    script_file: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    chapter_name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    forest_circle_sounds: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    forest_ambient_sounds: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    mod_name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    mod_version: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    fields: ?[*]RmgWeightedName = null,
    field_capacity: c_int = 0,
    field_count: c_int = 0,
    graphs: ?[*]RmgWeightedName = null,
    graph_capacity: c_int = 0,
    graph_count: c_int = 0,
    vso: ?[*]RmgVso = null,
    vso_capacity: c_int = 0,
    vso_count: c_int = 0,
    diplomacies: ?[*]u8 = null,
    diplomacy_capacity: c_int = 0,
    diplomacy_count: c_int = 0,
    units: ?[*]RmgUnit = null,
    unit_capacity: c_int = 0,
    unit_count: c_int = 0,
    scripts: RmgScripts = .{},
};

/// BkEditorRmgTerrainType: one terrain type of a season's tileset.
pub const RmgTerrainType = extern struct {
    name: [64]u8 = [_]u8{0} ** 64,
    variant_count: c_int = 0,

    pub fn nameSlice(self: *const RmgTerrainType) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }
};

/// Text into a NUL-terminated C field, zero-filled; false (nothing written)
/// when it does not fit with its terminator - never truncated.
pub fn putName(field: []u8, text: []const u8) bool {
    if (text.len >= field.len) return false;
    @memset(field, 0);
    @memcpy(field[0..text.len], text);
    return true;
}

/// The bounds a record write refuses above (bridge.h BK_EDITOR_RMG_MAX_*).
pub const rmg_max_patches = 512;
pub const rmg_max_nodes = 256;
pub const rmg_max_links = 1024;
pub const rmg_max_script_ids = 4096;
pub const rmg_max_script_areas = 512;
/// The field set write's bounds (bridge.h BK_EDITOR_RMG_MAX_SHELLS and kin).
pub const rmg_max_shells = 256;
pub const rmg_max_shell_entries = 16384;
pub const rmg_max_weighted = 1024;
pub const rmg_max_players = 17;
pub const rmg_max_units = 16;

/// BkEditorRmgGenerateParams (05-08, D-01), layout included: one Create
/// Random Map. The names are storage-relative as `listRmg` answers them
/// (templates, chapters for the context, settings; `setting_name` empty or
/// "<any setting>" is any setting); `level` 0..2, `graph` -1 or an index,
/// `angle` -1 or 0..3; `map_name` is one path component. `has_seed` 0 draws
/// a fresh seed. `progress` is called once per generator step on the calling
/// thread and must not call back into the bridge.
pub const rmg_map_name_capacity = 96;
pub const rmg_any_setting = "<any setting>";
pub const RmgGenerateParams = extern struct {
    template_name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    context_name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    setting_name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    map_name: [rmg_map_name_capacity]u8 = [_]u8{0} ** rmg_map_name_capacity,
    level: c_int = 0,
    graph: c_int = -1,
    angle: c_int = -1,
    save_as_bzm: c_int = 1,
    write_dds: c_int = 0,
    overwrite: c_int = 0,
    has_seed: c_int = 0,
    seed: c_uint = 0,
    progress: ?ProgressFn = null,
    user: ?*anyopaque = null,

    fn put(field: []u8, text: []const u8) void {
        const len = @min(text.len, field.len - 1);
        @memset(field, 0);
        @memcpy(field[0..len], text[0..len]);
    }
    pub fn setTemplate(self: *RmgGenerateParams, text: []const u8) void {
        put(&self.template_name, text);
    }
    pub fn setContext(self: *RmgGenerateParams, text: []const u8) void {
        put(&self.context_name, text);
    }
    pub fn setSetting(self: *RmgGenerateParams, text: []const u8) void {
        put(&self.setting_name, text);
    }
    pub fn setMapName(self: *RmgGenerateParams, text: []const u8) void {
        put(&self.map_name, text);
    }
    pub fn templateSlice(self: *const RmgGenerateParams) []const u8 {
        return std.mem.sliceTo(&self.template_name, 0);
    }
    pub fn contextSlice(self: *const RmgGenerateParams) []const u8 {
        return std.mem.sliceTo(&self.context_name, 0);
    }
    pub fn settingSlice(self: *const RmgGenerateParams) []const u8 {
        return std.mem.sliceTo(&self.setting_name, 0);
    }
    pub fn mapNameSlice(self: *const RmgGenerateParams) []const u8 {
        return std.mem.sliceTo(&self.map_name, 0);
    }
};

/// BkEditorRmgGenerateResult (05-08), layout included: the seed the
/// generation ran from (read back from the .seed file beside the map), the
/// graph and angle it used and the map file's OS path.
pub const RmgGenerateResult = extern struct {
    seed: c_uint = 0,
    graph: c_int = -1,
    angle: c_int = -1,
    graph_name: [field_set_name_capacity]u8 = [_]u8{0} ** field_set_name_capacity,
    map_path: [1024]u8 = [_]u8{0} ** 1024,

    pub fn graphNameSlice(self: *const RmgGenerateResult) []const u8 {
        return std.mem.sliceTo(&self.graph_name, 0);
    }
    pub fn mapPathSlice(self: *const RmgGenerateResult) []const u8 {
        return std.mem.sliceTo(&self.map_path, 0);
    }
};

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

/// BkEditorTileRegion (05-07, D-14), layout included: TILE coordinates,
/// half-open [x0, x1) x [y0, y1), row 0 at the top of the map.
pub const TileRegion = extern struct { x0: c_int, y0: c_int, x1: c_int, y1: c_int };

/// BkEditorMinimapUnit (05-07, D-14), layout included: one marker of the MFC
/// minimap - an AI-tile rectangle, half-open, two AI tiles per terrain tile,
/// y up from the south edge - in a colour of the 17-colour player table
/// (`color_index` 0..16); `squad` is 1 for a squad's own marker.
pub const MinimapUnit = extern struct { link_id: c_int, x0: c_int, y0: c_int, x1: c_int, y1: c_int, color_index: c_int, squad: c_int };

/// BkEditorMinimapArea (05-07, D-14), layout included: one fire-range area the
/// AI shows. Centre and radii are AI units (64 per terrain tile, y up from the
/// south edge); the angles are 0..65535 turns, equal for a full circle; `rgb`
/// is 0x00RRGGBB.
pub const MinimapArea = extern struct { kind: c_int, cx: f32, cy: f32, radius: f32, min_radius: f32, start_angle: c_int, finish_angle: c_int, rgb: c_uint };

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
        /// BkEditorLayers (M3, D-32): the renderer's layer state - `bits` one
        /// bit per `layers.Layer` (set = shown), `mask` the layers the
        /// renderer can draw at all (a refused layer is greyed in the menu).
        /// Fixed-size, no map need be open.
        layers: *const fn (ptr: *anyopaque, bits: *u32, mask: *u32) Status,
        /// BkEditorSetLayerShow (M3, D-32): one toggle layer to a state.
        /// Renderer state only: no map data, never dirty, no history. Refused
        /// with no map open or for a layer outside the mask.
        setLayerShow: *const fn (ptr: *anyopaque, layer: u32, shown: bool) Status,
        /// BkEditorSetFireRangeMode (M3, D-32): `mode` is a `layers.FireMode`
        /// value; `filter` names an object filter (`.filter` only) and
        /// `link_ids` the selection (`.selected` only). The previous group is
        /// dropped first.
        setFireRangeMode: *const fn (ptr: *anyopaque, mode: u32, filter: []const u8, link_ids: []const i32) Status,
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
        /// BkEditorCreateRandomMap (05-08, D-01..D-05): one generation through
        /// the engine's CreateRandomMap, into the user's (or the mod's) maps
        /// folder. Not an edit of the open map: no token, the map is left as it
        /// was. REFUSED names the field in `lastMessage` and wrote nothing.
        createRandomMap: *const fn (ptr: *anyopaque, params: RmgGenerateParams, result: *RmgGenerateResult) Status,
        /// BkEditorListStorageFiles (05-08, D-13): the storage files under a
        /// folder ending in an extension, extension kept (the Export lists'
        /// enumeration). Two-pass like `listRmg`.
        listStorageFiles: *const fn (ptr: *anyopaque, folder: [*:0]const u8, extension: [*:0]const u8, out: []RmgName, total: *usize) Status,
        /// BkEditorRmgTemplateGraphs (05-08, D-13): a template's graphs with
        /// their weights, in its own order. Two-pass like `listRmg`.
        rmgTemplateGraphs: *const fn (ptr: *anyopaque, template: [*:0]const u8, out: []RmgGraph, total: *usize) Status,
        /// BkEditorRmgReadContainer / BkEditorRmgWriteContainer (05-09, D-06/D-07):
        /// a container through the engine's own serialiser. Reads are two-pass
        /// (see `RmgScripts`); a write under the user RMG root refuses a shipped
        /// name with the Save-As message.
        rmgReadContainer: *const fn (ptr: *anyopaque, name: [*:0]const u8, record: *RmgContainerRecord) Status,
        rmgWriteContainer: *const fn (ptr: *anyopaque, name: [*:0]const u8, record: *const RmgContainerRecord) Status,
        /// The same for a graph.
        rmgReadGraph: *const fn (ptr: *anyopaque, name: [*:0]const u8, record: *RmgGraphRecord) Status,
        rmgWriteGraph: *const fn (ptr: *anyopaque, name: [*:0]const u8, record: *const RmgGraphRecord) Status,
        /// BkEditorRmgPatchInfoRead: a patch map's size, season, folder and
        /// script lists, two-pass.
        rmgPatchInfo: *const fn (ptr: *anyopaque, name: [*:0]const u8, info: *RmgPatchInfo) Status,
        /// BkEditorRmgImportPatch (D-10): a map outside the storages copied into
        /// the user RMG root's Scenarios/Patches/<season>/. `apply` false only
        /// validates and names the destination.
        rmgImportPatch: *const fn (ptr: *anyopaque, source: [*:0]const u8, apply: bool, out: *RmgName) Status,
        /// BkEditorRmgRoot: the user RMG root as the host spells it, written
        /// into `out` (NUL-terminated).
        rmgRoot: *const fn (ptr: *anyopaque, out: []u8) Status,
        /// BkEditorRmgReadFieldSet / BkEditorRmgWriteFieldSet (05-10, D-06/D-07):
        /// SRMFieldSet through the engine's serialiser, like the containers'.
        rmgReadFieldSet: *const fn (ptr: *anyopaque, name: [*:0]const u8, record: *RmgFieldSetRecord) Status,
        rmgWriteFieldSet: *const fn (ptr: *anyopaque, name: [*:0]const u8, record: *const RmgFieldSetRecord) Status,
        /// BkEditorRmgTileset: the terrain types of a season's tileset (0 summer
        /// .. 3 spring), no map needed. Two-pass like `listRmg`.
        rmgReadTemplate: *const fn (ptr: *anyopaque, name: [*:0]const u8, record: *RmgTemplateRecord) Status,
        rmgWriteTemplate: *const fn (ptr: *anyopaque, name: [*:0]const u8, record: *const RmgTemplateRecord) Status,
        rmgTileset: *const fn (ptr: *anyopaque, season: i32, out: []RmgTerrainType, total: *usize) Status,
        /// BkEditorRmgFileExists: whether `name` + `extension` is in the storage
        /// stack (a profile's .tga, a script's .lua, a descriptor's .xml).
        rmgFileExists: *const fn (ptr: *anyopaque, name: [*:0]const u8, extension: [*:0]const u8, exists: *bool) Status,
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
        /// BkEditorTiles (05-07, D-14): the tile indices of `region`, row-major,
        /// row 0 at the top. Two-pass like `altitudes`: `total` is always the
        /// region's area and a buffer too short for it is refused.
        tiles: *const fn (ptr: *anyopaque, region: TileRegion, out: []u8, total: *usize) Status,
        /// BkEditorMinimapTileColors (05-07, D-14): one 0x00RRGGBB per tile index
        /// of the open map's tileset (`total` is the tileset's tile count, at most
        /// 256). Two-pass.
        minimapTileColors: *const fn (ptr: *anyopaque, out: []u32, total: *usize) Status,
        /// BkEditorMinimapUnits (05-07, D-14): the MFC minimap's object markers.
        /// Two-pass.
        minimapUnits: *const fn (ptr: *anyopaque, out: []MinimapUnit, total: *usize) Status,
        /// BkEditorMinimapAreas (05-07, D-14): the fire-range areas the AI shows
        /// now (none until a group shows them). Two-pass.
        minimapAreas: *const fn (ptr: *anyopaque, out: []MinimapArea, total: *usize) Status,
        /// BkEditorCreateMiniMapImage (05-07, D-17): the four minimap pictures
        /// (DDS and TGA, 512 `_large` and 256) beside the SAVED map at `map_path`,
        /// verified by size. Refused, naming why, for a path that is not a saved
        /// user map; the document is never touched.
        createMinimapImages: *const fn (ptr: *anyopaque, map_path: []const u8) Status,
    };

    pub fn addPlayer(self: Bridge, side: i32, token: *i32) Status { return self.vtable.addPlayer(self.ptr, side, token); }
    pub fn tiles(self: Bridge, region: TileRegion, out: []u8, total: *usize) Status { return self.vtable.tiles(self.ptr, region, out, total); }
    pub fn minimapTileColors(self: Bridge, out: []u32, total: *usize) Status { return self.vtable.minimapTileColors(self.ptr, out, total); }
    pub fn minimapUnits(self: Bridge, out: []MinimapUnit, total: *usize) Status { return self.vtable.minimapUnits(self.ptr, out, total); }
    pub fn minimapAreas(self: Bridge, out: []MinimapArea, total: *usize) Status { return self.vtable.minimapAreas(self.ptr, out, total); }
    pub fn createMinimapImages(self: Bridge, map_path: []const u8) Status { return self.vtable.createMinimapImages(self.ptr, map_path); }
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
    pub fn layers(self: Bridge, bits: *u32, mask: *u32) Status { return self.vtable.layers(self.ptr, bits, mask); }
    pub fn setLayerShow(self: Bridge, layer: u32, shown: bool) Status { return self.vtable.setLayerShow(self.ptr, layer, shown); }
    pub fn setFireRangeMode(self: Bridge, mode: u32, filter: []const u8, link_ids: []const i32) Status { return self.vtable.setFireRangeMode(self.ptr, mode, filter, link_ids); }
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
    pub fn createRandomMap(self: Bridge, params: RmgGenerateParams, result: *RmgGenerateResult) Status { return self.vtable.createRandomMap(self.ptr, params, result); }
    pub fn listStorageFiles(self: Bridge, folder: [*:0]const u8, extension: [*:0]const u8, out: []RmgName, total: *usize) Status { return self.vtable.listStorageFiles(self.ptr, folder, extension, out, total); }
    pub fn rmgTemplateGraphs(self: Bridge, template: [*:0]const u8, out: []RmgGraph, total: *usize) Status { return self.vtable.rmgTemplateGraphs(self.ptr, template, out, total); }
    pub fn rmgReadContainer(self: Bridge, name: [*:0]const u8, record: *RmgContainerRecord) Status { return self.vtable.rmgReadContainer(self.ptr, name, record); }
    pub fn rmgWriteContainer(self: Bridge, name: [*:0]const u8, record: *const RmgContainerRecord) Status { return self.vtable.rmgWriteContainer(self.ptr, name, record); }
    pub fn rmgReadGraph(self: Bridge, name: [*:0]const u8, record: *RmgGraphRecord) Status { return self.vtable.rmgReadGraph(self.ptr, name, record); }
    pub fn rmgWriteGraph(self: Bridge, name: [*:0]const u8, record: *const RmgGraphRecord) Status { return self.vtable.rmgWriteGraph(self.ptr, name, record); }
    pub fn rmgPatchInfo(self: Bridge, name: [*:0]const u8, info: *RmgPatchInfo) Status { return self.vtable.rmgPatchInfo(self.ptr, name, info); }
    pub fn rmgImportPatch(self: Bridge, source: [*:0]const u8, apply: bool, out: *RmgName) Status { return self.vtable.rmgImportPatch(self.ptr, source, apply, out); }
    pub fn rmgRoot(self: Bridge, out: []u8) Status { return self.vtable.rmgRoot(self.ptr, out); }
    pub fn rmgReadFieldSet(self: Bridge, name: [*:0]const u8, record: *RmgFieldSetRecord) Status { return self.vtable.rmgReadFieldSet(self.ptr, name, record); }
    pub fn rmgWriteFieldSet(self: Bridge, name: [*:0]const u8, record: *const RmgFieldSetRecord) Status { return self.vtable.rmgWriteFieldSet(self.ptr, name, record); }
    pub fn rmgReadTemplate(self: Bridge, name: [*:0]const u8, record: *RmgTemplateRecord) Status { return self.vtable.rmgReadTemplate(self.ptr, name, record); }
    pub fn rmgWriteTemplate(self: Bridge, name: [*:0]const u8, record: *const RmgTemplateRecord) Status { return self.vtable.rmgWriteTemplate(self.ptr, name, record); }
    pub fn rmgTileset(self: Bridge, season: i32, out: []RmgTerrainType, total: *usize) Status { return self.vtable.rmgTileset(self.ptr, season, out, total); }
    pub fn rmgFileExists(self: Bridge, name: [*:0]const u8, extension: [*:0]const u8, exists: *bool) Status { return self.vtable.rmgFileExists(self.ptr, name, extension, exists); }
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
