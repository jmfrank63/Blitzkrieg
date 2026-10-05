//! The resource core's view of the engine bridge
//! (Sources/src/EditorBridge/resource_bridge.h). One method per BkRes* entry
//! point relevant to the headless core, same arguments, same statuses, so the
//! fake (fake_bridge.zig) and the real adapter (S05+) can be read side by
//! side. The core never sees a C type: this file is plain Zig so the module
//! cross-compiles on every target, including x86_64-windows-gnu where the
//! engine C++ does not. The map core's `Sources/editor/core/bridge.zig`
//! establishes the `pub const Bridge = struct { ptr, vtable }` shape used
//! here; the resource editor's own vtable lives below as `ResBridge`.
const std = @import("std");

/// BkEditorStatus from bridge.h (shared between map and resource ABIs). The
/// values follow the C enum exactly.
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
/// message for the status bar; anything else is a failure. `BadArgument`
/// stays separate so the fake and real adapters can report a validation
/// miss distinctly from a refusal (the T03 fake uses this for duplicate ids
/// and for out-of-range inserts, as the task plan asks).
pub const EditError = error{ Refused, BadArgument, Failed, OutOfMemory };

pub fn check(status: Status) EditError!void {
    return switch (status) {
        .ok => {},
        .refused => error.Refused,
        .bad_argument => error.BadArgument,
        else => error.Failed,
    };
}

/// NResourceModel::EResourceKind as the C ABI's BkResKind integer: 21 project
/// kinds, one per MFC CParentFrame subclass (MEM005). The integer values are
/// the enum values the engine uses, so a value coming back through the C
/// bridge round-trips. Order follows the memory record.
pub const Kind = enum(c_int) {
    weapon = 0,
    mine = 1,
    trench = 2,
    squad = 3,
    sprite = 4,
    animation_infantry = 5,
    mesh_unit = 6,
    object = 7,
    fence = 8,
    build = 9,
    bridge = 10,
    particle = 11,
    effect = 12,
    tile_set = 13,
    road_3d = 14,
    river_3d = 15,
    mission = 16,
    chapter = 17,
    campaign = 18,
    medal = 19,
    gui_frame = 20,

    pub fn fromCInt(value: c_int) ?Kind {
        if (value < 0 or value > 20) return null;
        return @enumFromInt(value);
    }

    /// The project file extension (no dot), as the C bridge names a kind in
    /// its messages.
    pub fn extension(self: Kind) []const u8 {
        const table = [_][]const u8{ "wpn", "mcp", "trc", "scp", "spt", "unt", "msh", "obt", "fnc", "bld", "bdg", "pcp", "eff", "til", "3rd", "3rv", "mip", "chc", "cgc", "mdc", "gui" };
        return table[@intCast(@intFromEnum(self))];
    }
};

/// Fixed buffer sizes mirror the C ABI's BkRes* records. A write that cannot
/// fit (with its NUL terminator) is refused rather than truncated.
pub const name_capacity: usize = 64;
pub const value_text_capacity: usize = 128;
pub const warning_text_capacity: usize = 256;
pub const owner_capacity: usize = 128;
pub const lock_message_capacity: usize = 256;

/// One tree node as BkResNodeRecord carries it. `class_name` holds the
/// `CTreeItemFactory::Create` class the item was built from; `display_name`
/// what the renderer shows. The core keeps both because an insert needs the
/// class and the renderer needs the display name - the C ABI's int
/// `class_type` is covered by `class_name` here (names are stable across
/// ports, integers can renumber).
pub const NodeRecord = struct {
    id: i32 = 0,
    parent: i32 = -1,
    class_name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    display_name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    expand: bool = true,
    child_count: i32 = 0,

    pub fn classSlice(self: *const NodeRecord) []const u8 {
        return std.mem.sliceTo(&self.class_name, 0);
    }

    pub fn displaySlice(self: *const NodeRecord) []const u8 {
        return std.mem.sliceTo(&self.display_name, 0);
    }

    pub fn setClass(self: *NodeRecord, text: []const u8) bool {
        return putName(&self.class_name, text);
    }

    pub fn setDisplay(self: *NodeRecord, text: []const u8) bool {
        return putName(&self.display_name, text);
    }
};

/// One property as BkResPropRecord carries it. `value_text` is the text form
/// the properties pane edits (the C ABI parses it against the domain type).
pub const PropRecord = struct {
    id: i32 = 0,
    domain_type: i32 = 0,
    value_kind: i32 = 0,
    combo_count: i32 = 0,
    default_name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    display_name: [name_capacity]u8 = [_]u8{0} ** name_capacity,
    value_text: [value_text_capacity]u8 = [_]u8{0} ** value_text_capacity,

    pub fn defaultSlice(self: *const PropRecord) []const u8 {
        return std.mem.sliceTo(&self.default_name, 0);
    }

    pub fn displaySlice(self: *const PropRecord) []const u8 {
        return std.mem.sliceTo(&self.display_name, 0);
    }

    pub fn valueSlice(self: *const PropRecord) []const u8 {
        return std.mem.sliceTo(&self.value_text, 0);
    }

    pub fn setDefault(self: *PropRecord, text: []const u8) bool {
        return putName(&self.default_name, text);
    }

    pub fn setDisplay(self: *PropRecord, text: []const u8) bool {
        return putName(&self.display_name, text);
    }

    pub fn setValue(self: *PropRecord, text: []const u8) bool {
        return putValueText(&self.value_text, text);
    }
};

/// One reference entry (BkResReferenceEntry): a stable token the project
/// writes and the choice offered.
pub const reference_name_capacity: usize = 128;
pub const ReferenceEntry = struct {
    token: i32 = 0,
    name: [reference_name_capacity]u8 = [_]u8{0} ** reference_name_capacity,

    pub fn nameSlice(self: *const ReferenceEntry) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    pub fn setName(self: *ReferenceEntry, text: []const u8) bool {
        return putGeneric(&self.name, text);
    }
};

/// The export flags (BK_RES_EXPORT_*): `force` is MFC's batch -f (export
/// even when up to date); `open_save` is -os (a batch only re-saves).
pub const ExportFlags = struct {
    force: bool = false,
    open_save: bool = false,

    pub fn toCInt(self: ExportFlags) c_int {
        return (if (self.force) @as(c_int, 1) else 0) | (if (self.open_save) @as(c_int, 2) else 0);
    }
};

/// One warning line of an export (BkResWarning).
pub const Warning = struct {
    text: [warning_text_capacity]u8 = [_]u8{0} ** warning_text_capacity,

    pub fn textSlice(self: *const Warning) []const u8 {
        return std.mem.sliceTo(&self.text, 0);
    }

    /// Unlike the names, a warning is truncated rather than refused: the
    /// export has already happened when its report is written.
    pub fn setText(self: *Warning, text: []const u8) void {
        @memset(&self.text, 0);
        const n = @min(text.len, warning_text_capacity - 1);
        @memcpy(self.text[0..n], text[0..n]);
    }
};

/// BkResExportReport without its buffer: the caller hands the warnings
/// slice separately; `warning_total` is the full count even when that
/// slice was shorter.
pub const ExportReport = struct {
    written: i32 = 0,
    skipped: i32 = 0,
    warning_total: usize = 0,
};

/// BkResModSettings: MFC's MOD Settings dialog. The export dir is the mod's
/// own folder; exports and mod.xml go into its data/ folder.
pub const ModSettings = struct {
    export_dir: [260]u8 = [_]u8{0} ** 260,
    name: [64]u8 = [_]u8{0} ** 64,
    version: [32]u8 = [_]u8{0} ** 32,
    desc: [256]u8 = [_]u8{0} ** 256,

    pub fn exportDirSlice(self: *const ModSettings) []const u8 {
        return std.mem.sliceTo(&self.export_dir, 0);
    }
    pub fn nameSlice(self: *const ModSettings) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }
    pub fn versionSlice(self: *const ModSettings) []const u8 {
        return std.mem.sliceTo(&self.version, 0);
    }
    pub fn descSlice(self: *const ModSettings) []const u8 {
        return std.mem.sliceTo(&self.desc, 0);
    }
    /// Each setter refuses (false, nothing written) a value that does not
    /// fit, like `putName`.
    pub fn setExportDir(self: *ModSettings, text: []const u8) bool {
        return putGeneric(&self.export_dir, text);
    }
    pub fn setName(self: *ModSettings, text: []const u8) bool {
        return putGeneric(&self.name, text);
    }
    pub fn setVersion(self: *ModSettings, text: []const u8) bool {
        return putGeneric(&self.version, text);
    }
    pub fn setDesc(self: *ModSettings, text: []const u8) bool {
        return putGeneric(&self.desc, text);
    }
};

/// BkResPoint2: a 2D point in map/scene units.
pub const Point2 = extern struct { x: f32 = 0, y: f32 = 0 };

/// BkResVec3: a 3D vector (particle and effect keyframes carry z).
pub const Vec3 = extern struct { x: f32 = 0, y: f32 = 0, z: f32 = 0 };

/// BkResAimedPoint: a point with an angle and a cone (shoot/fire/smoke/
/// directed-explosion points).
pub const AimedPoint = extern struct {
    at: Point2 = .{},
    angle: i32 = 0,
    cone: i32 = 0,
};

/// The geometry channels the core edits through one unified entry point so
/// `ResourceCommand.geometry` (reserved for T04) can carry any of them.
/// Matches the BkRes[Get|Set]* families in the C header.
pub const GeometryChannel = enum(c_int) {
    passability_cells = 0,
    locked_tiles = 1,
    transparency_lines = 2,
    zero_point = 3,
    entrance = 4,
    shoot_points = 5,
    fire_points = 6,
    smoke_points = 7,
    directed_explosion_points = 8,
    formation_positions = 9,
    bridge_span_marks = 10,
    mission_objectives = 11,
    chapter_crosses = 12,
    campaign_crosses = 13,
    particle_keyframes = 14,
    effect_keyframes = 15,

    /// The payload family the channel carries, as the `GeometryValue` tag.
    /// Formation positions and bridge span marks are flat Point2 lists in
    /// MFC's AI world units with z dropped (SquadFrm and BridgeFrm keep it 0),
    /// so they share the transparency lines' family. Mission objectives and
    /// chapter/campaign crosses are map positions MFC stores as one CVec2 per
    /// child (ChapterFrm, CampaignFrm, MissionFrm), so they join it too. The
    /// keyframe lists keep a third component and are the vec3 family.
    pub fn family(self: GeometryChannel) std.meta.Tag(GeometryValue) {
        return switch (self) {
            .passability_cells, .locked_tiles => .bytes_grid,
            .transparency_lines, .formation_positions, .bridge_span_marks, .mission_objectives, .chapter_crosses, .campaign_crosses => .points2,
            .zero_point, .entrance => .point2,
            .shoot_points, .fire_points, .smoke_points, .directed_explosion_points => .aimed,
            .particle_keyframes, .effect_keyframes => .vec3,
        };
    }
};

/// A geometry channel's payload - the three value families BkRes[Get|Set]*
/// use: a grid of bytes, a flat point2 list, a flat aimed-point list, a flat
/// vec3 list, or a single Point2 (zero point, entrance). The variant tag
/// doubles as the channel's family; the channel enum tells the bridge which
/// specific list to read or write.
pub const GeometryValue = union(enum) {
    bytes_grid: struct { bytes: []u8, width: i32, height: i32 },
    points2: []Point2,
    aimed: []AimedPoint,
    vec3: []Vec3,
    point2: Point2,

    pub fn deinit(self: *GeometryValue, allocator: std.mem.Allocator) void {
        switch (self.*) {
            .bytes_grid => |*g| allocator.free(g.bytes),
            .points2 => |p| allocator.free(p),
            .aimed => |p| allocator.free(p),
            .vec3 => |p| allocator.free(p),
            .point2 => {},
        }
        self.* = .{ .point2 = .{} };
    }

    pub fn dupe(self: GeometryValue, allocator: std.mem.Allocator) !GeometryValue {
        return switch (self) {
            .bytes_grid => |g| .{ .bytes_grid = .{
                .bytes = try allocator.dupe(u8, g.bytes),
                .width = g.width,
                .height = g.height,
            } },
            .points2 => |p| .{ .points2 = try allocator.dupe(Point2, p) },
            .aimed => |p| .{ .aimed = try allocator.dupe(AimedPoint, p) },
            .vec3 => |p| .{ .vec3 = try allocator.dupe(Vec3, p) },
            .point2 => |p| .{ .point2 = p },
        };
    }
};

/// The resource core's view of the C bridge, same shape as
/// `Sources/editor/core/bridge.zig`'s `pub const Bridge = struct { ptr,
/// vtable }`. Every method mirrors one BkRes* entry point from
/// resource_bridge.h; names are the C name without the BkRes prefix,
/// lower-camelCased. Buffer-shaped arguments (name buffers) are fixed-size
/// arrays by reference; variable-length lists are slices.
pub const ResBridge = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    pub const VTable = struct {
        /// BkEditorLastMessage: owned by the session, valid until the next
        /// call on it. Never null, even on a null session.
        lastMessage: *const fn (ptr: *anyopaque) []const u8,
        /// BkResNew: fresh project of the given kind.
        new: *const fn (ptr: *anyopaque, kind: Kind) Status,
        /// BkResOpen: parse an existing project from path.
        open: *const fn (ptr: *anyopaque, path: []const u8) Status,
        /// BkResSave: safe-save (temp+read-back+rename). The path's
        /// extension decides the format.
        save: *const fn (ptr: *anyopaque, path: []const u8) Status,
        /// BkResClose: drop the open project (no save).
        close: *const fn (ptr: *anyopaque) Status,
        /// BkResKindOf: the open project's kind, written through *out.
        kindOf: *const fn (ptr: *anyopaque, out: *Kind) Status,
        /// BkResLock: cooperative advisory lock in the project file's
        /// folder.
        lock: *const fn (ptr: *anyopaque, owner: []const u8) Status,
        /// BkResLockOwner: read the lock owner. `out_len` is the full
        /// length; a short buffer is refused.
        lockOwner: *const fn (ptr: *anyopaque, out: []u8, out_len: *usize) Status,
        /// BkResNodes: two-pass read of every tree node in treeItemList
        /// order; `total` is always the full count.
        nodes: *const fn (ptr: *anyopaque, out: []NodeRecord, total: *usize) Status,
        /// BkResProps: two-pass read of every property of a node in SProp
        /// order.
        props: *const fn (ptr: *anyopaque, node: i32, out: []PropRecord, total: *usize) Status,
        /// BkResSetProp: writes a property's value_text, parsed against the
        /// domain type.
        setProp: *const fn (ptr: *anyopaque, node: i32, prop_id: i32, value_text: []const u8) Status,
        /// BkResInsertNode: inserts a new node of `class_name` at
        /// parent/index. Writes the new node's id through *out_id.
        insertNode: *const fn (ptr: *anyopaque, parent: i32, class_name: []const u8, index: i32, out_id: *i32) Status,
        /// BkResDeleteNode: deletes a node and every descendant; two-pass
        /// read of the serialised subtree through *out_blob.
        deleteNode: *const fn (ptr: *anyopaque, node: i32, out_blob: []u8, out_size: *usize) Status,
        /// BkResRestoreNode: puts a deleted subtree back at parent/index.
        restoreNode: *const fn (ptr: *anyopaque, blob: []const u8, parent: i32, index: i32, out_id: *i32) Status,
        /// BkResMoveNode: moves a node to a new parent/index. Refused for
        /// a cycle.
        moveNode: *const fn (ptr: *anyopaque, node: i32, new_parent: i32, new_index: i32) Status,
        /// BkResRefList: two-pass read of a reference list by type.
        refList: *const fn (ptr: *anyopaque, ref_type: i32, out: []ReferenceEntry, total: *usize) Status,
        /// Generic geometry read. Two-pass like `nodes`: `total` is always
        /// the full count. The payload family comes from the channel.
        geometryRead: *const fn (ptr: *anyopaque, node: i32, channel: GeometryChannel, out: *GeometryValue) Status,
        /// Generic geometry write. Replaces the whole list / grid for the
        /// channel.
        geometryWrite: *const fn (ptr: *anyopaque, node: i32, channel: GeometryChannel, value: *const GeometryValue) Status,
        /// BkResExport (stats_only false) / BkResExportStatsOnly: the open
        /// project into the export root's data/ folder. Refused while the
        /// kind's exporter is not ported yet.
        exportProject: *const fn (ptr: *anyopaque, flags: ExportFlags, stats_only: bool, report: *ExportReport, warnings: []Warning) Status,
        /// BkResBatch: every project of `kind` (null: all) under src into
        /// dst's data/ folder; a failing project is a warning, not a stop.
        batch: *const fn (ptr: *anyopaque, kind: ?Kind, src: []const u8, dst: []const u8, flags: ExportFlags, report: *ExportReport, warnings: []Warning) Status,
        /// BkResModSettingsGet / Set: the export dir and mod.xml's fields.
        modSettingsGet: *const fn (ptr: *anyopaque, out: *ModSettings) Status,
        modSettingsSet: *const fn (ptr: *anyopaque, in: *const ModSettings) Status,
        /// BkResPackMod: the export root's data/ folder zipped into a .pak.
        packMod: *const fn (ptr: *anyopaque, out_path: []const u8) Status,
        /// BkResImportFromGame: a new, unsaved project of `kind` from a
        /// runtime resource folder. Refused for a kind not ported yet.
        importFromGame: *const fn (ptr: *anyopaque, kind: Kind, path: []const u8) Status,
        /// BkResPreviewBegin: empty preview scene for the kind.
        previewBegin: *const fn (ptr: *anyopaque, kind: Kind) Status,
        /// BkResPreviewShow: export the open project into the preview and
        /// build it.
        previewShow: *const fn (ptr: *anyopaque) Status,
        /// BkResPreviewStop: tear the preview scene down.
        previewStop: *const fn (ptr: *anyopaque) Status,
    };

    pub fn lastMessage(self: ResBridge) []const u8 {
        return self.vtable.lastMessage(self.ptr);
    }
    pub fn new(self: ResBridge, kind: Kind) Status {
        return self.vtable.new(self.ptr, kind);
    }
    pub fn open(self: ResBridge, path: []const u8) Status {
        return self.vtable.open(self.ptr, path);
    }
    pub fn save(self: ResBridge, path: []const u8) Status {
        return self.vtable.save(self.ptr, path);
    }
    pub fn close(self: ResBridge) Status {
        return self.vtable.close(self.ptr);
    }
    pub fn kindOf(self: ResBridge, out: *Kind) Status {
        return self.vtable.kindOf(self.ptr, out);
    }
    pub fn lock(self: ResBridge, owner: []const u8) Status {
        return self.vtable.lock(self.ptr, owner);
    }
    pub fn lockOwner(self: ResBridge, out: []u8, out_len: *usize) Status {
        return self.vtable.lockOwner(self.ptr, out, out_len);
    }
    pub fn nodes(self: ResBridge, out: []NodeRecord, total: *usize) Status {
        return self.vtable.nodes(self.ptr, out, total);
    }
    pub fn props(self: ResBridge, node: i32, out: []PropRecord, total: *usize) Status {
        return self.vtable.props(self.ptr, node, out, total);
    }
    pub fn setProp(self: ResBridge, node: i32, prop_id: i32, value_text: []const u8) Status {
        return self.vtable.setProp(self.ptr, node, prop_id, value_text);
    }
    pub fn insertNode(self: ResBridge, parent: i32, class_name: []const u8, index: i32, out_id: *i32) Status {
        return self.vtable.insertNode(self.ptr, parent, class_name, index, out_id);
    }
    pub fn deleteNode(self: ResBridge, node: i32, out_blob: []u8, out_size: *usize) Status {
        return self.vtable.deleteNode(self.ptr, node, out_blob, out_size);
    }
    pub fn restoreNode(self: ResBridge, blob: []const u8, parent: i32, index: i32, out_id: *i32) Status {
        return self.vtable.restoreNode(self.ptr, blob, parent, index, out_id);
    }
    pub fn moveNode(self: ResBridge, node: i32, new_parent: i32, new_index: i32) Status {
        return self.vtable.moveNode(self.ptr, node, new_parent, new_index);
    }
    pub fn refList(self: ResBridge, ref_type: i32, out: []ReferenceEntry, total: *usize) Status {
        return self.vtable.refList(self.ptr, ref_type, out, total);
    }
    pub fn geometryRead(self: ResBridge, node: i32, channel: GeometryChannel, out: *GeometryValue) Status {
        return self.vtable.geometryRead(self.ptr, node, channel, out);
    }
    pub fn geometryWrite(self: ResBridge, node: i32, channel: GeometryChannel, value: *const GeometryValue) Status {
        return self.vtable.geometryWrite(self.ptr, node, channel, value);
    }
    pub fn exportProject(self: ResBridge, flags: ExportFlags, stats_only: bool, report: *ExportReport, warnings: []Warning) Status {
        return self.vtable.exportProject(self.ptr, flags, stats_only, report, warnings);
    }
    pub fn batch(self: ResBridge, kind: ?Kind, src: []const u8, dst: []const u8, flags: ExportFlags, report: *ExportReport, warnings: []Warning) Status {
        return self.vtable.batch(self.ptr, kind, src, dst, flags, report, warnings);
    }
    pub fn modSettingsGet(self: ResBridge, out: *ModSettings) Status {
        return self.vtable.modSettingsGet(self.ptr, out);
    }
    pub fn modSettingsSet(self: ResBridge, in: *const ModSettings) Status {
        return self.vtable.modSettingsSet(self.ptr, in);
    }
    pub fn packMod(self: ResBridge, out_path: []const u8) Status {
        return self.vtable.packMod(self.ptr, out_path);
    }
    pub fn importFromGame(self: ResBridge, kind: Kind, path: []const u8) Status {
        return self.vtable.importFromGame(self.ptr, kind, path);
    }
    pub fn previewBegin(self: ResBridge, kind: Kind) Status {
        return self.vtable.previewBegin(self.ptr, kind);
    }
    pub fn previewShow(self: ResBridge) Status {
        return self.vtable.previewShow(self.ptr);
    }
    pub fn previewStop(self: ResBridge) Status {
        return self.vtable.previewStop(self.ptr);
    }
};

/// Text into a NUL-terminated fixed-size name buffer, zero-filled; false
/// (nothing written) when it does not fit with its terminator - never
/// truncated. Matches the map core's `putName` contract.
pub fn putName(field: *[name_capacity]u8, text: []const u8) bool {
    return putGeneric(field, text);
}

pub fn putValueText(field: *[value_text_capacity]u8, text: []const u8) bool {
    return putGeneric(field, text);
}

fn putGeneric(field: anytype, text: []const u8) bool {
    const info = @typeInfo(@TypeOf(field)).pointer;
    const Child = info.child;
    const child_info = @typeInfo(Child);
    const field_len = child_info.array.len;
    if (text.len >= field_len) return false;
    @memset(field, 0);
    @memcpy(field[0..text.len], text);
    return true;
}

test "Kind.fromCInt rejects out-of-range values and accepts the 21 ids" {
    try std.testing.expect(Kind.fromCInt(-1) == null);
    try std.testing.expect(Kind.fromCInt(21) == null);
    try std.testing.expectEqual(Kind.weapon, Kind.fromCInt(0).?);
    try std.testing.expectEqual(Kind.gui_frame, Kind.fromCInt(20).?);
}

test "check maps the C statuses to EditError" {
    try check(.ok);
    try std.testing.expectError(error.Refused, check(.refused));
    try std.testing.expectError(error.BadArgument, check(.bad_argument));
    try std.testing.expectError(error.Failed, check(.failed));
    try std.testing.expectError(error.Failed, check(.no_device));
}

test "ExportFlags maps to the BK_RES_EXPORT_* bits" {
    try std.testing.expectEqual(@as(c_int, 0), (ExportFlags{}).toCInt());
    try std.testing.expectEqual(@as(c_int, 1), (ExportFlags{ .force = true }).toCInt());
    try std.testing.expectEqual(@as(c_int, 3), (ExportFlags{ .force = true, .open_save = true }).toCInt());
}

test "Warning.setText truncates and stays terminated" {
    var warning: Warning = .{};
    var long: [warning_text_capacity + 10]u8 = undefined;
    @memset(&long, 'w');
    warning.setText(&long);
    try std.testing.expectEqual(warning_text_capacity - 1, warning.textSlice().len);
}

test "putName refuses to truncate" {
    var buffer: [name_capacity]u8 = [_]u8{0xAA} ** name_capacity;
    try std.testing.expect(putName(&buffer, "ok"));
    try std.testing.expectEqualStrings("ok", std.mem.sliceTo(&buffer, 0));
    var too_long: [name_capacity]u8 = undefined;
    @memset(&too_long, 'x');
    try std.testing.expect(!putName(&buffer, &too_long));
}
