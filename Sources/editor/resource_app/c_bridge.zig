//! The resource core's ResBridge over the real C ABI
//! (Sources/src/EditorBridge/resource_bridge.h). The core is std-only and runs
//! on every target; this file is where it meets the engine, so it lives in the
//! app, like the map app's c_bridge.zig. Statuses map one to one, strings are
//! copied into NUL-terminated buffers on the way in, and the bridge's last
//! message is copied out, because the bridge's pointer is valid only until its
//! next call.
//!
//! Three places where the Zig interface and the C ABI differ, and how this
//! adapter bridges them:
//! - A node's class is an integer in C (`class_type`, the item's
//!   `CTreeItemFactory` type) and a name in the core (names survive a port,
//!   integers can renumber). The adapter writes the integer as decimal text
//!   into `class_name` and parses it back for an insert, so whatever the core
//!   read from `nodes` it can hand to `insertNode` unchanged.
//! - The C two-pass reads answer OK to a sizing call (null buffer, capacity
//!   0); the core's contract, which the fake keeps, is that any buffer shorter
//!   than the total is refused. An empty slice is therefore sent as a sizing
//!   call and turned into REFUSED when the total is not 0.
//! - `lock` takes an owner in the core, but the engine names the lock file
//!   after the login (or the BK_RESOURCE_EDITOR_USER test seam). The owner
//!   argument is not sent; the host passes that same login name so the
//!   document's mirror and the file agree.
const std = @import("std");
const core = @import("resource_core");

/// resource_bridge.h (which includes bridge.h). This @cImport is separate from
/// `editor_kit.host.c`, so its BkEditorSession is a different opaque type from
/// the kit's: `RealResBridge.init` takes the host's session as `*anyopaque` and
/// casts it, which is sound because both name the same C struct.
pub const c = @cImport(@cInclude("resource_bridge.h"));

const rb = core.bridge;
const Status = rb.Status;
const ResBridge = rb.ResBridge;
const Kind = rb.Kind;
const NodeRecord = rb.NodeRecord;
const PropRecord = rb.PropRecord;
const ReferenceEntry = rb.ReferenceEntry;
const MeshLocator = rb.MeshLocator;
const ExportFlags = rb.ExportFlags;
const ExportReport = rb.ExportReport;
const Warning = rb.Warning;
const ModSettings = rb.ModSettings;
const KeyframeKnobs = rb.KeyframeKnobs;
const ParticleInfo = rb.ParticleInfo;
const Point2 = rb.Point2;
const Vec3 = rb.Vec3;
const AimedPoint = rb.AimedPoint;
const GeometryChannel = rb.GeometryChannel;
const GeometryValue = rb.GeometryValue;

comptime {
    // The geometry records are handed to the C entry points as they are: the
    // core's extern structs and the header's structs must stay one layout.
    std.debug.assert(@sizeOf(Point2) == @sizeOf(c.BkResPoint2));
    std.debug.assert(@offsetOf(Point2, "y") == @offsetOf(c.BkResPoint2, "y"));
    std.debug.assert(@sizeOf(Vec3) == @sizeOf(c.BkResVec3));
    std.debug.assert(@offsetOf(Vec3, "z") == @offsetOf(c.BkResVec3, "z"));
    std.debug.assert(@sizeOf(AimedPoint) == @sizeOf(c.BkResAimedPoint));
    std.debug.assert(@offsetOf(AimedPoint, "angle") == @offsetOf(c.BkResAimedPoint, "angle"));
    std.debug.assert(@offsetOf(AimedPoint, "cone") == @offsetOf(c.BkResAimedPoint, "cone"));
    // The records copied field by field: the buffers must be the core's
    // capacities, so a copy never truncates.
    std.debug.assert(@sizeOf(@FieldType(c.BkResNodeRecord, "display_name")) == rb.name_capacity);
    std.debug.assert(@sizeOf(@FieldType(c.BkResPropRecord, "default_name")) == rb.name_capacity);
    std.debug.assert(@sizeOf(@FieldType(c.BkResPropRecord, "display_name")) == rb.name_capacity);
    std.debug.assert(@sizeOf(@FieldType(c.BkResPropRecord, "value_text")) == rb.value_text_capacity);
    std.debug.assert(@sizeOf(@FieldType(c.BkResReferenceEntry, "name")) == rb.reference_name_capacity);
    std.debug.assert(@sizeOf(@FieldType(c.BkResLocator, "name")) == rb.name_capacity);
    std.debug.assert(@sizeOf(c.BkResWarning) == rb.warning_text_capacity);
    std.debug.assert(@sizeOf(@FieldType(c.BkResModSettings, "export_dir")) == @sizeOf(@FieldType(ModSettings, "export_dir")));
    std.debug.assert(@sizeOf(@FieldType(c.BkResModSettings, "name")) == @sizeOf(@FieldType(ModSettings, "name")));
    std.debug.assert(@sizeOf(@FieldType(c.BkResModSettings, "version")) == @sizeOf(@FieldType(ModSettings, "version")));
    std.debug.assert(@sizeOf(@FieldType(c.BkResModSettings, "desc")) == @sizeOf(@FieldType(ModSettings, "desc")));
}

fn status(value: c.BkEditorStatus) Status {
    return std.enums.fromInt(Status, value) orelse .failed;
}

/// A slice length as the C ABI's int capacity; a slice longer than an int can
/// count is offered as the largest int, which is still "enough" for any total
/// the bridge can report.
fn capacityOf(len: usize) c_int {
    return @intCast(@min(len, @as(usize, std.math.maxInt(c_int))));
}

fn countOf(value: c_int) usize {
    return if (value > 0) @intCast(value) else 0;
}

/// Text into a NUL-terminated buffer; null when it does not fit or holds a
/// NUL itself (the C side would read a shorter string than was meant).
fn terminated(buffer: []u8, text: []const u8) ?[*:0]const u8 {
    if (text.len >= buffer.len) return null;
    if (std.mem.indexOfScalar(u8, text, 0) != null) return null;
    @memcpy(buffer[0..text.len], text);
    buffer[text.len] = 0;
    return @ptrCast(buffer.ptr);
}

/// A C fixed buffer into the core's same-sized buffer, always terminated.
fn copyFixed(dst: anytype, src: anytype) void {
    const n = @min(dst.len, src.len);
    @memset(dst, 0);
    const text = std.mem.sliceTo(src[0..n], 0);
    const len = @min(text.len, dst.len - 1);
    @memcpy(dst[0..len], text[0..len]);
}

/// The class integer as the core's class name (decimal text).
pub fn classNameOf(class_type: c_int, out: *[rb.name_capacity]u8) void {
    @memset(out, 0);
    _ = std.fmt.bufPrint(out[0 .. out.len - 1], "{d}", .{class_type}) catch unreachable;
}

/// The core's class name back to the C class integer; null when it is not
/// one this adapter wrote.
pub fn classTypeOf(class_name: []const u8) ?c_int {
    return std.fmt.parseInt(c_int, class_name, 10) catch null;
}

pub const RealResBridge = struct {
    /// Scratch for the C records and owner of every geometry list a read
    /// returns: the caller frees those with this same allocator
    /// (`GeometryValue.deinit`), as it does with the fake's.
    allocator: std.mem.Allocator,
    session: *c.BkEditorSession,
    message: [1024]u8 = undefined,
    /// A message the adapter itself produced (an allocation failure, a
    /// family mismatch), shown once instead of the bridge's.
    own_message: ?[]const u8 = null,

    /// `session` is the host's BkEditorSession (`editor_kit.host.Host.session`).
    pub fn init(allocator: std.mem.Allocator, session: *anyopaque) RealResBridge {
        return .{ .allocator = allocator, .session = @ptrCast(@alignCast(session)) };
    }

    pub fn bridge(self: *RealResBridge) ResBridge {
        return .{ .ptr = self, .vtable = &vtable };
    }

    fn from(ptr: *anyopaque) *RealResBridge {
        return @ptrCast(@alignCast(ptr));
    }

    fn fail(self: *RealResBridge, result: Status, text: []const u8) Status {
        self.own_message = text;
        return result;
    }

    /// BkResLockTakeOver: removes every other user's `locked_*` and takes the
    /// lock. Not in the core's vtable (the fake has no other users); the
    /// lock prompt's Take over button calls it on the real bridge.
    pub fn lockTakeOver(self: *RealResBridge) Status {
        return status(c.BkResLockTakeOver(self.session));
    }

    /// BkResPreviewCamera: the preview camera's anchor and zoom step.
    pub fn previewCamera(self: *RealResBridge, wx: f32, wy: f32, zoom: i32) Status {
        return status(c.BkResPreviewCamera(self.session, wx, wy, zoom));
    }

    const vtable: ResBridge.VTable = .{
        .lastMessage = lastMessage,
        .new = new,
        .open = open,
        .save = save,
        .close = close,
        .kindOf = kindOf,
        .lock = lock,
        .lockOwner = lockOwner,
        .nodes = nodes,
        .props = props,
        .setProp = setProp,
        .insertNode = insertNode,
        .deleteNode = deleteNode,
        .restoreNode = restoreNode,
        .moveNode = moveNode,
        .setNodeName = setNodeName,
        .setNodeExpand = setNodeExpand,
        .propStrings = propStrings,
        .refList = refList,
        .geometryRead = geometryRead,
        .geometryWrite = geometryWrite,
        .exportProject = exportProject,
        .batch = batch,
        .modSettingsGet = modSettingsGet,
        .modSettingsSet = modSettingsSet,
        .packMod = packMod,
        .importFromGame = importFromGame,
        .previewBegin = previewBegin,
        .previewShow = previewShow,
        .previewStop = previewStop,
        .previewPlayback = previewPlayback,
        .previewMeshVariant = previewMeshVariant,
        .previewDirection = previewDirection,
        .effectSetDirection = effectSetDirection,
        .effectGetDirection = effectGetDirection,
        .previewShowLocators = previewShowLocators,
        .meshLocators = meshLocators,
        .keyframeKnobs = keyframeKnobs,
        .particleInfo = particleInfo,
        .particleSourceMode = particleSourceMode,
        .particleSetSourceMode = particleSetSourceMode,
        .previewCameraMode = previewCameraMode,
    };

    fn lastMessage(ptr: *anyopaque) []const u8 {
        const self = from(ptr);
        const text = if (self.own_message) |own| own else std.mem.span(c.BkEditorLastMessage(self.session));
        self.own_message = null;
        const len = @min(text.len, self.message.len);
        @memcpy(self.message[0..len], text[0..len]);
        return self.message[0..len];
    }

    fn new(ptr: *anyopaque, kind: Kind) Status {
        const self = from(ptr);
        return status(c.BkResNew(self.session, @intFromEnum(kind)));
    }

    fn open(ptr: *anyopaque, path: []const u8) Status {
        const self = from(ptr);
        var buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, path) orelse return self.fail(.bad_argument, "the path is too long or holds a NUL");
        return status(c.BkResOpen(self.session, z));
    }

    fn save(ptr: *anyopaque, path: []const u8) Status {
        const self = from(ptr);
        var buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, path) orelse return self.fail(.bad_argument, "the path is too long or holds a NUL");
        return status(c.BkResSave(self.session, z));
    }

    fn close(ptr: *anyopaque) Status {
        const self = from(ptr);
        return status(c.BkResClose(self.session));
    }

    fn kindOf(ptr: *anyopaque, out: *Kind) Status {
        const self = from(ptr);
        var raw: c.BkResKind = 0;
        const result = status(c.BkResKindOf(self.session, &raw));
        if (result != .ok) return result;
        out.* = Kind.fromCInt(raw) orelse return self.fail(.failed, "the bridge reported an unknown project kind");
        return .ok;
    }

    fn lock(ptr: *anyopaque, owner: []const u8) Status {
        const self = from(ptr);
        _ = owner; // the engine names the lock after the login (file doc comment)
        return status(c.BkResLock(self.session));
    }

    fn lockOwner(ptr: *anyopaque, out: []u8, out_len: *usize) Status {
        const self = from(ptr);
        // The C call truncates; read into a buffer big enough for any
        // folder's owners and apply the core's refuse-when-short contract.
        var owners: [4096]u8 = undefined;
        const result = status(c.BkResLockOwner(self.session, &owners, owners.len));
        if (result != .ok) {
            out_len.* = 0;
            return result;
        }
        const text = std.mem.sliceTo(&owners, 0);
        out_len.* = text.len;
        if (out.len < text.len) return .refused;
        @memcpy(out[0..text.len], text);
        return .ok;
    }

    fn nodes(ptr: *anyopaque, out: []NodeRecord, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        if (out.len == 0) {
            const sized = status(c.BkResNodes(self.session, null, 0, &count));
            total.* = countOf(count);
            if (sized == .ok and total.* > 0) return .refused;
            return sized;
        }
        const scratch = self.allocator.alloc(c.BkResNodeRecord, out.len) catch return self.fail(.failed, "out of memory reading the tree");
        defer self.allocator.free(scratch);
        const result = status(c.BkResNodes(self.session, scratch.ptr, capacityOf(scratch.len), &count));
        total.* = countOf(count);
        if (result != .ok) return result;
        for (scratch[0..total.*], out[0..total.*]) |record, *dst| {
            dst.* = .{
                .id = record.id,
                .parent = record.parent,
                .expand = record.expand != 0,
                .child_count = record.child_count,
            };
            classNameOf(record.class_type, &dst.class_name);
            copyFixed(&dst.display_name, &record.display_name);
        }
        return .ok;
    }

    fn props(ptr: *anyopaque, node: i32, out: []PropRecord, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        if (out.len == 0) {
            const sized = status(c.BkResProps(self.session, node, null, 0, &count));
            total.* = countOf(count);
            if (sized == .ok and total.* > 0) return .refused;
            return sized;
        }
        const scratch = self.allocator.alloc(c.BkResPropRecord, out.len) catch return self.fail(.failed, "out of memory reading the properties");
        defer self.allocator.free(scratch);
        const result = status(c.BkResProps(self.session, node, scratch.ptr, capacityOf(scratch.len), &count));
        total.* = countOf(count);
        if (result != .ok) return result;
        for (scratch[0..total.*], out[0..total.*]) |record, *dst| {
            dst.* = .{
                .id = record.id,
                .domain_type = record.domain_type,
                .value_kind = record.value_kind,
                .combo_count = record.combo_count,
            };
            copyFixed(&dst.default_name, &record.default_name);
            copyFixed(&dst.display_name, &record.display_name);
            copyFixed(&dst.value_text, &record.value_text);
        }
        return .ok;
    }

    fn setProp(ptr: *anyopaque, node: i32, prop_id: i32, value_text: []const u8) Status {
        const self = from(ptr);
        var buffer: [rb.value_text_capacity]u8 = undefined;
        const z = terminated(&buffer, value_text) orelse return self.fail(.bad_argument, "the value is too long or holds a NUL");
        return status(c.BkResSetProp(self.session, node, prop_id, z));
    }

    fn insertNode(ptr: *anyopaque, parent: i32, class_name: []const u8, index: i32, out_id: *i32) Status {
        const self = from(ptr);
        const class_type = classTypeOf(class_name) orelse return self.fail(.bad_argument, "the class name is not a tree item type");
        var id: c_int = 0;
        const result = status(c.BkResInsertNode(self.session, parent, class_type, index, &id));
        if (result == .ok) out_id.* = id;
        return result;
    }

    fn deleteNode(ptr: *anyopaque, node: i32, out_blob: []u8, out_size: *usize) Status {
        const self = from(ptr);
        var size: c_int = 0;
        const result = status(c.BkResDeleteNode(self.session, node, if (out_blob.len == 0) null else out_blob.ptr, capacityOf(out_blob.len), &size));
        out_size.* = countOf(size);
        return result;
    }

    fn restoreNode(ptr: *anyopaque, blob: []const u8, parent: i32, index: i32, out_id: *i32) Status {
        const self = from(ptr);
        if (blob.len > std.math.maxInt(c_int)) return self.fail(.bad_argument, "the subtree is too large");
        var id: c_int = 0;
        const result = status(c.BkResRestoreNode(self.session, blob.ptr, @intCast(blob.len), parent, index, &id));
        if (result == .ok) out_id.* = id;
        return result;
    }

    fn moveNode(ptr: *anyopaque, node: i32, new_parent: i32, new_index: i32) Status {
        const self = from(ptr);
        return status(c.BkResMoveNode(self.session, node, new_parent, new_index));
    }

    fn setNodeName(ptr: *anyopaque, node: i32, name: []const u8) Status {
        const self = from(ptr);
        var buffer: [rb.name_capacity + 1]u8 = undefined;
        const z = terminated(&buffer, name) orelse return self.fail(.bad_argument, "the name is too long or holds a NUL");
        return status(c.BkResSetNodeName(self.session, node, z));
    }

    fn setNodeExpand(ptr: *anyopaque, node: i32, expand: bool) Status {
        const self = from(ptr);
        return status(c.BkResSetNodeExpand(self.session, node, @intFromBool(expand)));
    }

    fn propStrings(ptr: *anyopaque, node: i32, prop_id: i32, out: []ReferenceEntry, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        if (out.len == 0) {
            const sized = status(c.BkResPropStrings(self.session, node, prop_id, null, 0, &count));
            total.* = countOf(count);
            if (sized == .ok and total.* > 0) return .refused;
            return sized;
        }
        const scratch = self.allocator.alloc(c.BkResReferenceEntry, out.len) catch return self.fail(.failed, "out of memory reading the property's strings");
        defer self.allocator.free(scratch);
        const result = status(c.BkResPropStrings(self.session, node, prop_id, scratch.ptr, capacityOf(scratch.len), &count));
        total.* = countOf(count);
        if (result != .ok) return result;
        for (scratch[0..total.*], out[0..total.*]) |entry, *dst| {
            dst.* = .{ .token = entry.token };
            copyFixed(&dst.name, &entry.name);
        }
        return .ok;
    }

    fn refList(ptr: *anyopaque, ref_type: i32, out: []ReferenceEntry, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        if (out.len == 0) {
            const sized = status(c.BkResRefList(self.session, ref_type, null, 0, &count));
            total.* = countOf(count);
            if (sized == .ok and total.* > 0) return .refused;
            return sized;
        }
        const scratch = self.allocator.alloc(c.BkResReferenceEntry, out.len) catch return self.fail(.failed, "out of memory reading the reference list");
        defer self.allocator.free(scratch);
        const result = status(c.BkResRefList(self.session, ref_type, scratch.ptr, capacityOf(scratch.len), &count));
        total.* = countOf(count);
        if (result != .ok) return result;
        for (scratch[0..total.*], out[0..total.*]) |entry, *dst| {
            dst.* = .{ .token = entry.token };
            copyFixed(&dst.name, &entry.name);
        }
        return .ok;
    }

    // --- geometry ---------------------------------------------------------

    const PointsGet = *const fn (?*c.BkEditorSession, c_int, [*c]c.BkResPoint2, c_int, [*c]c_int) callconv(.c) c.BkEditorStatus;
    const PointsSet = *const fn (?*c.BkEditorSession, c_int, [*c]const c.BkResPoint2, c_int) callconv(.c) c.BkEditorStatus;
    const AimedGet = *const fn (?*c.BkEditorSession, c_int, [*c]c.BkResAimedPoint, c_int, [*c]c_int) callconv(.c) c.BkEditorStatus;
    const AimedSet = *const fn (?*c.BkEditorSession, c_int, [*c]const c.BkResAimedPoint, c_int) callconv(.c) c.BkEditorStatus;
    const Vec3Get = *const fn (?*c.BkEditorSession, c_int, [*c]c.BkResVec3, c_int, [*c]c_int) callconv(.c) c.BkEditorStatus;
    const Vec3Set = *const fn (?*c.BkEditorSession, c_int, [*c]const c.BkResVec3, c_int) callconv(.c) c.BkEditorStatus;
    const GridGet = *const fn (?*c.BkEditorSession, c_int, [*c]u8, c_int, [*c]c_int, [*c]c_int) callconv(.c) c.BkEditorStatus;
    const GridSet = *const fn (?*c.BkEditorSession, c_int, [*c]const u8, c_int, c_int) callconv(.c) c.BkEditorStatus;
    const PointGet = *const fn (?*c.BkEditorSession, c_int, [*c]c.BkResPoint2) callconv(.c) c.BkEditorStatus;
    const PointSet = *const fn (?*c.BkEditorSession, c_int, [*c]const c.BkResPoint2) callconv(.c) c.BkEditorStatus;

    fn gridPair(channel: GeometryChannel) struct { GridGet, GridSet } {
        return switch (channel) {
            .passability_cells => .{ &c.BkResGetPassabilityCells, &c.BkResSetPassabilityCells },
            .locked_tiles => .{ &c.BkResGetLockedTiles, &c.BkResSetLockedTiles },
            .transparency_cells => .{ &c.BkResGetTransparencyCells, &c.BkResSetTransparencyCells },
            .fence_transparences => .{ &c.BkResGetFenceTransparences, &c.BkResSetFenceTransparences },
            else => unreachable,
        };
    }

    fn pointsPair(channel: GeometryChannel) struct { PointsGet, PointsSet } {
        return switch (channel) {
            .transparency_lines => .{ &c.BkResGetTransparencyLines, &c.BkResSetTransparencyLines },
            .formation_positions => .{ &c.BkResGetFormationPositions, &c.BkResSetFormationPositions },
            .bridge_span_marks => .{ &c.BkResGetBridgeSpanMarks, &c.BkResSetBridgeSpanMarks },
            .mission_objectives => .{ &c.BkResGetMissionObjectives, &c.BkResSetMissionObjectives },
            .chapter_crosses => .{ &c.BkResGetChapterCrosses, &c.BkResSetChapterCrosses },
            .campaign_crosses => .{ &c.BkResGetCampaignCrosses, &c.BkResSetCampaignCrosses },
            else => unreachable,
        };
    }

    fn pointPair(channel: GeometryChannel) struct { PointGet, PointSet } {
        return switch (channel) {
            .zero_point => .{ &c.BkResGetZeroPoint, &c.BkResSetZeroPoint },
            .entrance => .{ &c.BkResGetEntrance, &c.BkResSetEntrance },
            .formation_direction => .{ &c.BkResGetFormationDirection, &c.BkResSetFormationDirection },
            .sprite_pos => .{ &c.BkResGetSpritePos, &c.BkResSetSpritePos },
            else => unreachable,
        };
    }

    fn aimedPair(channel: GeometryChannel) struct { AimedGet, AimedSet } {
        return switch (channel) {
            .shoot_points => .{ &c.BkResGetShootPoints, &c.BkResSetShootPoints },
            .fire_points => .{ &c.BkResGetFirePoints, &c.BkResSetFirePoints },
            .smoke_points => .{ &c.BkResGetSmokePoints, &c.BkResSetSmokePoints },
            .directed_explosion_points => .{ &c.BkResGetDirectedExplosionPoints, &c.BkResSetDirectedExplosionPoints },
            else => unreachable,
        };
    }

    fn vec3Pair(channel: GeometryChannel) struct { Vec3Get, Vec3Set } {
        return switch (channel) {
            .particle_keyframes => .{ &c.BkResGetParticleKeyframes, &c.BkResSetParticleKeyframes },
            .effect_keyframes => .{ &c.BkResGetEffectKeyframes, &c.BkResSetEffectKeyframes },
            else => unreachable,
        };
    }

    /// A list read: sizing call, then a buffer of exactly the total. The
    /// core's records share the C layout (the comptime block above), so the
    /// list is read straight into the slice the caller will own.
    fn readList(self: *RealResBridge, comptime T: type, comptime C: type, get: anytype, node: i32) union(enum) { ok: []T, err: Status } {
        var count: c_int = 0;
        const sized = status(get(self.session, node, null, 0, &count));
        if (sized != .ok) return .{ .err = sized };
        const list = self.allocator.alloc(T, countOf(count)) catch return .{ .err = self.fail(.failed, "out of memory reading geometry") };
        if (list.len == 0) return .{ .ok = list };
        const filled = status(get(self.session, node, @as([*c]C, @ptrCast(list.ptr)), capacityOf(list.len), &count));
        if (filled != .ok) {
            self.allocator.free(list);
            return .{ .err = filled };
        }
        return .{ .ok = list };
    }

    fn geometryRead(ptr: *anyopaque, node: i32, channel: GeometryChannel, out: *GeometryValue) Status {
        const self = from(ptr);
        switch (channel.family()) {
            .bytes_grid => {
                const get = gridPair(channel)[0];
                var w: c_int = 0;
                var h: c_int = 0;
                const sized = status(get(self.session, node, null, 0, &w, &h));
                if (sized != .ok) return sized;
                const bytes = self.allocator.alloc(u8, countOf(w) * countOf(h)) catch return self.fail(.failed, "out of memory reading geometry");
                if (bytes.len > 0) {
                    const filled = status(get(self.session, node, bytes.ptr, capacityOf(bytes.len), &w, &h));
                    if (filled != .ok) {
                        self.allocator.free(bytes);
                        return filled;
                    }
                }
                out.* = .{ .bytes_grid = .{ .bytes = bytes, .width = w, .height = h } };
            },
            .points2 => switch (self.readList(Point2, c.BkResPoint2, pointsPair(channel)[0], node)) {
                .ok => |list| out.* = .{ .points2 = list },
                .err => |s| return s,
            },
            .aimed => switch (self.readList(AimedPoint, c.BkResAimedPoint, aimedPair(channel)[0], node)) {
                .ok => |list| out.* = .{ .aimed = list },
                .err => |s| return s,
            },
            .vec3 => switch (self.readList(Vec3, c.BkResVec3, vec3Pair(channel)[0], node)) {
                .ok => |list| out.* = .{ .vec3 = list },
                .err => |s| return s,
            },
            .point2 => {
                var point: c.BkResPoint2 = .{ .x = 0, .y = 0 };
                const result = status(pointPair(channel)[0](self.session, node, &point));
                if (result != .ok) return result;
                out.* = .{ .point2 = .{ .x = point.x, .y = point.y } };
            },
        }
        return .ok;
    }

    fn geometryWrite(ptr: *anyopaque, node: i32, channel: GeometryChannel, value: *const GeometryValue) Status {
        const self = from(ptr);
        if (std.meta.activeTag(value.*) != channel.family())
            return self.fail(.bad_argument, "the geometry payload does not match the channel");
        return switch (value.*) {
            .bytes_grid => |g| blk: {
                if (g.width < 0 or g.height < 0 or g.bytes.len != countOf(g.width) * countOf(g.height))
                    break :blk self.fail(.bad_argument, "the grid's size does not match its bytes");
                break :blk status(gridPair(channel)[1](self.session, node, if (g.bytes.len == 0) null else g.bytes.ptr, g.width, g.height));
            },
            .points2 => |list| self.writeList(c.BkResPoint2, pointsPair(channel)[1], node, list),
            .aimed => |list| self.writeList(c.BkResAimedPoint, aimedPair(channel)[1], node, list),
            .vec3 => |list| self.writeList(c.BkResVec3, vec3Pair(channel)[1], node, list),
            .point2 => |p| blk: {
                const point: c.BkResPoint2 = .{ .x = p.x, .y = p.y };
                break :blk status(pointPair(channel)[1](self.session, node, &point));
            },
        };
    }

    fn writeList(self: *RealResBridge, comptime C: type, set: anytype, node: i32, list: anytype) Status {
        if (list.len > std.math.maxInt(c_int)) return self.fail(.bad_argument, "the geometry list is too long");
        const items: [*c]const C = if (list.len == 0) null else @ptrCast(list.ptr);
        return status(set(self.session, node, items, @intCast(list.len)));
    }

    // --- export, batch, MOD -----------------------------------------------

    /// Runs one report-filling call with a C warnings buffer the size of the
    /// caller's, then copies the counts and lines back.
    fn withReport(self: *RealResBridge, report: *ExportReport, warnings: []Warning, call: anytype, args: anytype) Status {
        const scratch = self.allocator.alloc(c.BkResWarning, warnings.len) catch return self.fail(.failed, "out of memory for the export report");
        defer self.allocator.free(scratch);
        var c_report: c.BkResExportReport = std.mem.zeroes(c.BkResExportReport);
        c_report.warnings = if (scratch.len == 0) null else scratch.ptr;
        c_report.warnings_capacity = capacityOf(scratch.len);
        const result = status(@call(.auto, call, args ++ .{&c_report}));
        report.* = .{
            .written = c_report.written,
            .skipped = c_report.skipped,
            .warning_total = countOf(c_report.warning_count),
        };
        const copied = @min(report.warning_total, warnings.len);
        for (scratch[0..copied], warnings[0..copied]) |line, *dst| dst.setText(std.mem.sliceTo(&line.text, 0));
        return result;
    }

    fn exportProject(ptr: *anyopaque, flags: ExportFlags, stats_only: bool, report: *ExportReport, warnings: []Warning) Status {
        const self = from(ptr);
        return if (stats_only)
            self.withReport(report, warnings, c.BkResExportStatsOnly, .{ self.session, flags.toCInt() })
        else
            self.withReport(report, warnings, c.BkResExport, .{ self.session, flags.toCInt() });
    }

    fn batch(ptr: *anyopaque, kind: ?Kind, src: []const u8, dst: []const u8, flags: ExportFlags, report: *ExportReport, warnings: []Warning) Status {
        const self = from(ptr);
        var src_buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        var dst_buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const src_z = terminated(&src_buffer, src) orelse return self.fail(.bad_argument, "the source folder is too long or holds a NUL");
        const dst_z = terminated(&dst_buffer, dst) orelse return self.fail(.bad_argument, "the destination folder is too long or holds a NUL");
        const raw_kind: c_int = if (kind) |k| @intFromEnum(k) else -1;
        return self.withReport(report, warnings, c.BkResBatch, .{ self.session, raw_kind, src_z, dst_z, flags.toCInt() });
    }

    fn modSettingsGet(ptr: *anyopaque, out: *ModSettings) Status {
        const self = from(ptr);
        var settings: c.BkResModSettings = std.mem.zeroes(c.BkResModSettings);
        const result = status(c.BkResModSettingsGet(self.session, &settings));
        if (result != .ok) return result;
        copyFixed(&out.export_dir, &settings.export_dir);
        copyFixed(&out.name, &settings.name);
        copyFixed(&out.version, &settings.version);
        copyFixed(&out.desc, &settings.desc);
        return .ok;
    }

    fn modSettingsSet(ptr: *anyopaque, in: *const ModSettings) Status {
        const self = from(ptr);
        var settings: c.BkResModSettings = std.mem.zeroes(c.BkResModSettings);
        copyFixed(&settings.export_dir, &in.export_dir);
        copyFixed(&settings.name, &in.name);
        copyFixed(&settings.version, &in.version);
        copyFixed(&settings.desc, &in.desc);
        return status(c.BkResModSettingsSet(self.session, &settings));
    }

    fn packMod(ptr: *anyopaque, out_path: []const u8) Status {
        const self = from(ptr);
        var buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, out_path) orelse return self.fail(.bad_argument, "the path is too long or holds a NUL");
        return status(c.BkResPackMod(self.session, z));
    }

    // --- import, preview --------------------------------------------------

    fn importFromGame(ptr: *anyopaque, kind: Kind, path: []const u8) Status {
        const self = from(ptr);
        var buffer: [std.Io.Dir.max_path_bytes + 1]u8 = undefined;
        const z = terminated(&buffer, path) orelse return self.fail(.bad_argument, "the path is too long or holds a NUL");
        return status(c.BkResImportFromGame(self.session, @intFromEnum(kind), z));
    }

    fn previewBegin(ptr: *anyopaque, kind: Kind) Status {
        const self = from(ptr);
        return status(c.BkResPreviewBegin(self.session, @intFromEnum(kind)));
    }

    fn previewShow(ptr: *anyopaque) Status {
        const self = from(ptr);
        return status(c.BkResPreviewShow(self.session));
    }

    fn previewStop(ptr: *anyopaque) Status {
        const self = from(ptr);
        return status(c.BkResPreviewStop(self.session));
    }

    /// BkResPreviewPlayback: run (true) or stop the preview's animation.
    fn previewPlayback(ptr: *anyopaque, run: bool) Status {
        const self = from(ptr);
        return status(c.BkResPreviewPlayback(self.session, @intFromBool(run)));
    }

    /// BkResPreviewMeshVariant: combat (0), install (1) or transportable (2).
    fn previewMeshVariant(ptr: *anyopaque, variant: u8) Status {
        const self = from(ptr);
        return status(c.BkResPreviewMeshVariant(self.session, variant));
    }

    /// BkResPreviewDirection: the unit's turn in degrees.
    fn previewDirection(ptr: *anyopaque, angle: i32) Status {
        const self = from(ptr);
        return status(c.BkResPreviewDirection(self.session, angle));
    }

    /// BkResEffectSetDirection: the direction dock's angle in radians.
    fn effectSetDirection(ptr: *anyopaque, angle: f32) Status {
        const self = from(ptr);
        return status(c.BkResEffectSetDirection(self.session, angle));
    }

    /// BkResEffectGetDirection: the stored dock angle.
    fn effectGetDirection(ptr: *anyopaque, angle: *f32) Status {
        const self = from(ptr);
        return status(c.BkResEffectGetDirection(self.session, angle));
    }

    /// BkResPreviewShowLocators: locator sprites and bounding boxes.
    fn previewShowLocators(ptr: *anyopaque, locators: bool, bounding_boxes: bool) Status {
        const self = from(ptr);
        return status(c.BkResPreviewShowLocators(self.session, @intFromBool(locators), @intFromBool(bounding_boxes)));
    }

    /// BkResGetKeyframeKnobs: a curve node's range, step and resize mode.
    fn keyframeKnobs(ptr: *anyopaque, node: i32, out: *KeyframeKnobs) Status {
        const self = from(ptr);
        var knobs: c.BkResKeyframeKnobs = std.mem.zeroes(c.BkResKeyframeKnobs);
        const result = status(c.BkResGetKeyframeKnobs(self.session, node, &knobs));
        if (result != .ok) return result;
        out.* = .{
            .min_x = knobs.min_x,
            .max_x = knobs.max_x,
            .min_y = knobs.min_y,
            .max_y = knobs.max_y,
            .step_x = knobs.step_x,
            .step_y = knobs.step_y,
            .resize_mode = knobs.resize_mode != 0,
        };
        return .ok;
    }

    /// BkResGetParticleInfo: the four numbers of the built particle source.
    fn particleInfo(ptr: *anyopaque, out: *ParticleInfo) Status {
        const self = from(ptr);
        var info: c.BkResParticleInfo = std.mem.zeroes(c.BkResParticleInfo);
        const result = status(c.BkResGetParticleInfo(self.session, &info));
        if (result != .ok) return result;
        out.* = .{
            .max_count = info.max_count,
            .max_size = info.max_size,
            .average_size = info.average_size,
            .average_count = info.average_count,
        };
        return .ok;
    }

    /// BkResParticleSourceMode: whether the open .pcp is a complex source.
    fn particleSourceMode(ptr: *anyopaque, complex: *bool) Status {
        const self = from(ptr);
        var value: c_int = 0;
        const result = status(c.BkResParticleSourceMode(self.session, &value));
        complex.* = value != 0;
        return result;
    }

    /// BkResParticleSetSourceMode: complex fills the reference with `name`, simple clears it.
    fn particleSetSourceMode(ptr: *anyopaque, complex: bool, name: []const u8) Status {
        const self = from(ptr);
        var buffer: [rb.value_text_capacity]u8 = undefined;
        const z = terminated(&buffer, name) orelse return self.fail(.bad_argument, "the particle name is too long or holds a NUL");
        return status(c.BkResParticleSetSourceMode(self.session, @intFromBool(complex), z));
    }

    /// BkResPreviewCameraMode: MFC's Camera button.
    fn previewCameraMode(ptr: *anyopaque, horizontal: bool) Status {
        const self = from(ptr);
        return status(c.BkResPreviewCameraMode(self.session, @intFromBool(horizontal)));
    }

    /// BkResMeshLocators: the two-pass read of the shown model's nodes.
    fn meshLocators(ptr: *anyopaque, out: []MeshLocator, total: *usize) Status {
        const self = from(ptr);
        var count: c_int = 0;
        if (out.len == 0) {
            const sized = status(c.BkResMeshLocators(self.session, null, 0, &count));
            total.* = countOf(count);
            if (sized == .ok and total.* > 0) return .refused;
            return sized;
        }
        const scratch = self.allocator.alloc(c.BkResLocator, out.len) catch return self.fail(.failed, "out of memory reading the locators");
        defer self.allocator.free(scratch);
        const result = status(c.BkResMeshLocators(self.session, scratch.ptr, capacityOf(scratch.len), &count));
        total.* = countOf(count);
        if (result != .ok) return result;
        for (scratch[0..@min(total.*, out.len)], out[0..@min(total.*, out.len)]) |record, *dst| {
            dst.* = .{ .node_id = record.node_id, .wx = record.wx, .wy = record.wy, .wz = record.wz, .sx = record.sx, .sy = record.sy };
            copyFixed(&dst.name, &record.name);
        }
        return .ok;
    }
};

/// Forces semantic analysis of every vtable entry and C call even where no
/// executable links this file (the test-resource-app-logic tier compiles it
/// as an object: the BkRes* symbols stay unresolved there, and are resolved
/// where the ResourceEditor executable links the engine).
pub fn analyse() void {
    _ = &RealResBridge.vtable;
    _ = &RealResBridge.lockTakeOver;
    _ = &RealResBridge.previewCamera;
}

comptime {
    _ = &analyse;
}

test "class names round-trip through the C class integer" {
    var name: [rb.name_capacity]u8 = undefined;
    classNameOf(1234, &name);
    try std.testing.expectEqualStrings("1234", std.mem.sliceTo(&name, 0));
    try std.testing.expectEqual(@as(?c_int, 1234), classTypeOf(std.mem.sliceTo(&name, 0)));
    classNameOf(-7, &name);
    try std.testing.expectEqual(@as(?c_int, -7), classTypeOf(std.mem.sliceTo(&name, 0)));
    try std.testing.expectEqual(@as(?c_int, null), classTypeOf("CWeaponTreeRootItem"));
    try std.testing.expectEqual(@as(?c_int, null), classTypeOf(""));
}

test "terminated refuses text that does not fit or holds a NUL" {
    var buffer: [4]u8 = undefined;
    try std.testing.expect(terminated(&buffer, "abc") != null);
    try std.testing.expect(terminated(&buffer, "abcd") == null);
    try std.testing.expect(terminated(&buffer, "a\x00b") == null);
}

test "copyFixed stops at the C terminator and always terminates" {
    var dst: [4]u8 = undefined;
    const src = [_]u8{ 'a', 'b', 0, 'z' };
    copyFixed(&dst, &src);
    try std.testing.expectEqualStrings("ab", std.mem.sliceTo(&dst, 0));
    const full = [_]u8{ 'w', 'x', 'y', 'z' };
    copyFixed(&dst, &full);
    try std.testing.expectEqualStrings("wxy", std.mem.sliceTo(&dst, 0));
}
