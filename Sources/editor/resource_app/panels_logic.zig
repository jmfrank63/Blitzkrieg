//! The resource app's parts that need no window, no ImGui and no SDL: the
//! Editors menu in MFC's order, which menu items are enabled, the document
//! lifecycle (New, Open, Import, Save, Close, switching sub-editors) and the
//! unsaved-changes prompt guarding it. Kept apart from the panels so these run
//! under `zig build test-resource-app-logic` against resource_core's fake
//! bridge, without the engine's libraries or a GPU, on every target.
const std = @import("std");
const core = @import("resource_core");

const bridge = core.bridge;
const Kind = bridge.Kind;
const ResBridge = bridge.ResBridge;
const EditError = bridge.EditError;
const Document = core.document.Document;
const History = core.history.History;

pub const kind_count = @typeInfo(Kind).@"enum".fields.len;

// The File menu's lifecycle session and resourceeditor.cfg are their own
// files; test-resource-app-logic is rooted here, so their tests run with it.
test {
    _ = @import("lifecycle.zig");
    _ = @import("settings.zig");
    _ = @import("edit_logic.zig");
    _ = @import("tools_logic.zig");
    _ = @import("docks_logic.zig");
    _ = @import("squad_logic.zig");
    _ = @import("mesh_logic.zig");
    _ = @import("grid_logic.zig");
    _ = @import("keyframe_logic.zig");
    _ = @import("terrain_logic.zig");
}

// --- Editors menu ---------------------------------------------------------

/// One entry of the Editors menu: the kind it switches to, MFC's label, and
/// whether MFC drew a separator above it.
pub const EditorEntry = struct {
    kind: Kind,
    label: [:0]const u8,
    separator_before: bool = false,
};

/// The Editors menu exactly as MFC's IDR_MAINFRAME menu lists it
/// (Sources/src/editor/editor.rc, POPUP "Editors"), labels, order and
/// separators included. Twenty entries for 21 kinds: MFC's CMainFrame::OnCreate
/// has CreateGUIFrame commented out, so the GUI frame (gui) has no menu entry.
/// A .gui project is still reached by New and by opening the file.
pub const editors_menu = [_]EditorEntry{
    .{ .kind = .mesh_unit, .label = "Unit Editor" },
    .{ .kind = .animation_infantry, .label = "Infantry Editor" },
    .{ .kind = .squad, .label = "Squad Editor" },
    .{ .kind = .weapon, .label = "Weapon Editor" },
    .{ .kind = .mine, .label = "Mine Editor" },
    .{ .kind = .particle, .label = "Particle Editor", .separator_before = true },
    .{ .kind = .sprite, .label = "Sprite Editor" },
    .{ .kind = .effect, .label = "Effect Editor" },
    .{ .kind = .build, .label = "Building Editor", .separator_before = true },
    .{ .kind = .object, .label = "Object Editor" },
    .{ .kind = .fence, .label = "Fence Editor" },
    .{ .kind = .bridge, .label = "Bridge Editor" },
    .{ .kind = .trench, .label = "Trench Editor" },
    .{ .kind = .mission, .label = "Mission Editor", .separator_before = true },
    .{ .kind = .chapter, .label = "Chapter Editor" },
    .{ .kind = .campaign, .label = "Campaign Editor" },
    .{ .kind = .medal, .label = "Medal Editor" },
    .{ .kind = .tile_set, .label = "Terrain Editor", .separator_before = true },
    .{ .kind = .road_3d, .label = "Road Editor" },
    .{ .kind = .river_3d, .label = "River Editor" },
};

/// The sub-editor shown before any was chosen: CEditorApp::LoadLastActiveModuleID
/// falls back to E_ANIMATION_FRAME, the Infantry Editor.
pub const default_editor: Kind = .animation_infantry;

pub fn menuEntry(kind: Kind) ?EditorEntry {
    for (editors_menu) |entry| if (entry.kind == kind) return entry;
    return null;
}

/// The project kind of a file extension, with or without its dot, in any
/// case - MFC's CFrameManager::ActivateFrameByExtension, which picks the
/// sub-editor an opened file belongs to. All 21 kinds, gui included.
pub fn kindFromExtension(extension: []const u8) ?Kind {
    const bare = if (extension.len > 0 and extension[0] == '.') extension[1..] else extension;
    if (bare.len == 0) return null;
    inline for (@typeInfo(Kind).@"enum".fields) |field| {
        const kind: Kind = @enumFromInt(field.value);
        if (std.ascii.eqlIgnoreCase(bare, kind.extension())) return kind;
    }
    return null;
}

pub fn kindFromPath(path: []const u8) ?Kind {
    return kindFromExtension(std.fs.path.extension(path));
}

/// The remembered sub-editor (MFC's "Active Frame" profile value, here the
/// settings file's last sub-editor) back to a kind. Anything that is not an
/// Editors menu entry - unset, out of range, the GUI frame - is the default.
pub fn restoreEditor(stored: ?i32) Kind {
    const value = stored orelse return default_editor;
    const kind = Kind.fromCInt(value) orelse return default_editor;
    return if (menuEntry(kind) != null) kind else default_editor;
}

// --- Menu enablement ------------------------------------------------------

pub const MenuItem = enum {
    new_project,
    open_project,
    close_project,
    save,
    save_as,
    export_result,
    export_stats_only,
    mod_settings,
    pack_mod,
    set_directories,
    picture_options,
    batch_mode,
    run_game,
    import_from_game,
    undo,
    redo,
    exit,
};

/// What the menu needs to know about the document this frame.
pub const MenuState = struct {
    open: bool = false,
    read_only: bool = false,
    can_undo: bool = false,
    can_redo: bool = false,
};

/// Whether a menu item can be chosen. MFC's OnUpdateFileSave,
/// OnUpdateSaveProjectAs, OnUpdateCloseFile and OnUpdateFileExportFiles all
/// enable exactly when a project tree exists; everything else in File, Tools
/// and Editors is always enabled. Undo and Redo are new (MFC had none) and
/// also need a project the user may edit: a read-only project (another user
/// holds its lock) takes no edits, so nothing on its history can move. Save
/// stays enabled on a read-only project: it becomes Save As (`saveRoute`).
pub fn isEnabled(item: MenuItem, state: MenuState) bool {
    return switch (item) {
        .close_project, .save, .save_as, .export_result, .export_stats_only => state.open,
        .undo => state.open and !state.read_only and state.can_undo,
        .redo => state.open and !state.read_only and state.can_redo,
        .new_project, .open_project, .mod_settings, .pack_mod, .set_directories, .picture_options, .batch_mode, .run_game, .import_from_game, .exit => true,
    };
}

pub const SaveRoute = enum { save, save_as };

/// File > Save: MFC's OnFileSave asks for a name when the project has none.
/// A project another user has locked, or one under the shipped Data/ folder
/// (the kit's `shipped.isShipped`, decided by the caller), is never written in
/// place either: Save becomes Save As.
pub fn saveRoute(path: ?[]const u8, read_only: bool, shipped: bool) SaveRoute {
    const p = path orelse return .save_as;
    if (p.len == 0 or read_only or shipped) return .save_as;
    return .save;
}

// --- Unsaved-changes prompt -----------------------------------------------

pub const path_capacity: usize = 4096;

/// A path held by value, so a guarded action can outlive the dialog or menu
/// that asked for it.
pub const PathText = struct {
    bytes: [path_capacity]u8 = undefined,
    len: usize = 0,

    /// Null when the path does not fit: it is refused, never truncated.
    pub fn fromSlice(text: []const u8) ?PathText {
        if (text.len > path_capacity) return null;
        var out: PathText = .{ .len = text.len };
        @memcpy(out.bytes[0..text.len], text);
        return out;
    }

    pub fn slice(self: *const PathText) []const u8 {
        return self.bytes[0..self.len];
    }
};

/// Import from game data (D-13): the kind to build and the runtime folder
/// holding its 1.xml, held by value like `open_path`.
pub const ImportRequest = struct {
    kind: Kind,
    folder: PathText,
};

/// The action an unsaved-changes prompt is guarding (MFC asked before New,
/// Open, Close and Exit; switching sub-editors asks too, because one bridge
/// session holds one project - see `Lifecycle.switchEditor`).
pub const Pending = union(enum) {
    new_project: Kind,
    open_dialog,
    open_path: PathText,
    import_from_game: ImportRequest,
    close,
    quit,
    switch_editor: Kind,
};

/// Idle until a guarded action finds the project dirty, then asking until the
/// user answers Save, Don't save or Cancel - the map app's prompt (D-23) with
/// the resource editor's actions. Save does no I/O itself: the caller saves
/// and reports through `saveFinished`, which continues with the guarded action
/// only once that save landed.
pub const UnsavedPrompt = struct {
    pending: ?Pending = null,
    phase: Phase = .idle,

    const Phase = enum { idle, asking, saving };

    pub const Choice = enum { save, dont_save, cancel };

    pub const GuardResult = union(enum) {
        proceed: Pending,
        asked,
    };

    pub fn guard(self: *UnsavedPrompt, dirty: bool, action: Pending) GuardResult {
        if (!dirty) return .{ .proceed = action };
        self.pending = action;
        self.phase = .asking;
        return .asked;
    }

    pub fn isAsking(self: *const UnsavedPrompt) bool {
        return self.phase == .asking;
    }

    pub const AnswerResult = union(enum) {
        /// Save to the project's own path, then report through `saveFinished`.
        save,
        /// Show Save As (`saveRoute` said so), then report through `saveFinished`.
        save_as,
        /// Don't save: go ahead with the action at once.
        proceed: Pending,
        /// Cancel, or nothing was being asked: the action never happens.
        dropped,
    };

    pub fn answer(self: *UnsavedPrompt, choice: Choice, route: SaveRoute) AnswerResult {
        if (self.phase != .asking) return .dropped;
        switch (choice) {
            .cancel => {
                self.pending = null;
                self.phase = .idle;
                return .dropped;
            },
            .dont_save => {
                const pending = self.pending;
                self.pending = null;
                self.phase = .idle;
                return if (pending) |action| .{ .proceed = action } else .dropped;
            },
            .save => {
                self.phase = .saving;
                return switch (route) {
                    .save => .save,
                    .save_as => .save_as,
                };
            },
        }
    }

    /// Whether the save `answer` asked for landed; a cancelled Save As is
    /// `false` too. Either way a failure drops the guarded action and leaves
    /// the project as it was.
    pub fn saveFinished(self: *UnsavedPrompt, ok: bool) ?Pending {
        if (self.phase != .saving) return null;
        self.phase = .idle;
        const pending = self.pending;
        self.pending = null;
        return if (ok) pending else null;
    }
};

// --- Document lifecycle ---------------------------------------------------

/// The open project, its history and the active sub-editor, driven through a
/// ResBridge (the fake in tests, c_bridge.RealResBridge in the app).
///
/// The spec keeps one project per sub-editor kind, as MFC's frames did. The
/// bridge session holds one project at a time, so switching sub-editors parks
/// the project by its path and reopens it on the way back; the unsaved-changes
/// prompt guards the switch, and a parked project returns with a fresh
/// history. An untitled project has no path to park and is closed.
pub const Lifecycle = struct {
    doc: Document = .{},
    history: History = .{},
    is_open: bool = false,
    /// Another user's `locked_*` is in the project's folder: shown, not
    /// edited, saved only through Save As.
    read_only: bool = false,
    /// True from an import until the first save: an imported project exists
    /// only in the bridge, so it counts as unsaved before any edit.
    unsaved_import: bool = false,
    active: Kind = default_editor,
    /// Per kind, the path of the project to reopen when that sub-editor is
    /// switched back to. Owned.
    parked: [kind_count]?[]u8 = @splat(null),

    pub fn deinit(self: *Lifecycle, allocator: std.mem.Allocator) void {
        self.doc.deinit(allocator);
        self.history.deinit(allocator);
        for (&self.parked) |*slot| {
            if (slot.*) |p| allocator.free(p);
            slot.* = null;
        }
    }

    pub fn dirty(self: *const Lifecycle) bool {
        return self.is_open and (self.unsaved_import or self.doc.isDirty(&self.history));
    }

    pub fn menuState(self: *const Lifecycle) MenuState {
        return .{
            .open = self.is_open,
            .read_only = self.read_only,
            .can_undo = self.history.canUndo(),
            .can_redo = self.history.canRedo(),
        };
    }

    pub fn saveRouteFor(self: *const Lifecycle, shipped: bool) SaveRoute {
        return saveRoute(self.doc.pathSlice(), self.read_only, shipped);
    }

    /// The mirror and history after the bridge has a new project: everything
    /// of the previous one goes, the new one starts clean.
    fn adopt(self: *Lifecycle, allocator: std.mem.Allocator, b: ResBridge, path: ?[]const u8) EditError!void {
        self.history.clear(allocator);
        if (self.doc.lock_owner) |*owner| owner.deinit(allocator);
        self.doc.lock_owner = null;
        try self.doc.reload(allocator, b);
        try self.doc.refreshKind(b);
        try self.doc.setPath(allocator, path);
        self.is_open = true;
        self.read_only = false;
        self.unsaved_import = false;
        self.active = self.doc.kind;
    }

    fn forget(self: *Lifecycle, allocator: std.mem.Allocator) void {
        self.history.clear(allocator);
        self.doc.deinit(allocator);
        self.doc = .{};
        self.is_open = false;
        self.read_only = false;
        self.unsaved_import = false;
    }

    /// File > New: an empty project of `kind`, untitled until its first save.
    /// The caller has passed the unsaved-changes prompt.
    pub fn newProject(self: *Lifecycle, allocator: std.mem.Allocator, b: ResBridge, kind: Kind) EditError!void {
        try bridge.check(b.new(kind));
        try self.adopt(allocator, b, null);
    }

    pub const OpenOutcome = enum { opened, read_only };

    /// File > Open: reads the project and takes MFC's cooperative lock as
    /// `owner`. When another user holds it the project opens read-only and
    /// `doc.lock_owner` names who (BkResLockOwner); the host then warns and
    /// may take over. An extension that is not one of the 21 kinds is refused
    /// before the bridge is asked; the sub-editor follows the opened kind.
    pub fn openProject(self: *Lifecycle, allocator: std.mem.Allocator, b: ResBridge, path: []const u8, owner: []const u8) EditError!OpenOutcome {
        if (kindFromPath(path) == null) return error.Refused;
        try bridge.check(b.open(path));
        try self.adopt(allocator, b, path);
        self.doc.takeLock(allocator, b, owner) catch |err| switch (err) {
            error.Refused => {
                self.read_only = true;
                var buffer: [1024]u8 = undefined;
                var len: usize = 0;
                if (b.lockOwner(&buffer, &len) == .ok) {
                    self.doc.lock_owner = try core.document.LockOwner.fromSlice(allocator, buffer[0..len]);
                }
                return .read_only;
            },
            else => return err,
        };
        return .opened;
    }

    /// Import from game data (D-13): a new project the bridge built from a
    /// runtime folder. Untitled and unsaved until its first save. A kind whose
    /// import is not ported is refused by the bridge and the open project,
    /// if any, stays as it was.
    pub fn importFromGame(self: *Lifecycle, allocator: std.mem.Allocator, b: ResBridge, kind: Kind, folder: []const u8) EditError!void {
        try bridge.check(b.importFromGame(kind, folder));
        try self.adopt(allocator, b, null);
        self.unsaved_import = true;
    }

    /// File > Save / Save As to `path` (the caller chose it by `saveRoute`).
    /// Refused for a read-only project's own path, so a locked project is
    /// never written in place.
    pub fn saveProject(self: *Lifecycle, allocator: std.mem.Allocator, b: ResBridge, path: []const u8) EditError!void {
        if (!self.is_open) return error.Refused;
        if (self.read_only) {
            if (self.doc.pathSlice()) |own| if (std.mem.eql(u8, own, path)) return error.Refused;
        }
        try bridge.check(b.save(path));
        try self.doc.setPath(allocator, path);
        self.history.markClean();
        self.unsaved_import = false;
    }

    /// File > Close: drops the project (BkResClose releases this session's
    /// lock). The caller has passed the unsaved-changes prompt.
    pub fn closeProject(self: *Lifecycle, allocator: std.mem.Allocator, b: ResBridge) EditError!void {
        if (!self.is_open) return;
        try bridge.check(b.close());
        self.forget(allocator);
    }

    fn park(self: *Lifecycle, allocator: std.mem.Allocator, kind: Kind, path: ?[]const u8) !void {
        const slot = &self.parked[@intCast(@intFromEnum(kind))];
        const fresh = if (path) |p| try allocator.dupe(u8, p) else null;
        if (slot.*) |old| allocator.free(old);
        slot.* = fresh;
    }

    pub const SwitchOutcome = enum { unchanged, empty, reopened, reopen_failed };

    /// Editors menu: makes `kind` the active sub-editor. The project open now
    /// is parked under its own kind by path (closed, not saved: the caller has
    /// passed the unsaved-changes prompt), and the project parked for `kind`,
    /// if any, is reopened. A parked project that no longer opens leaves the
    /// sub-editor empty and is forgotten.
    pub fn switchEditor(self: *Lifecycle, allocator: std.mem.Allocator, b: ResBridge, kind: Kind, owner: []const u8) EditError!SwitchOutcome {
        if (kind == self.active) return .unchanged;
        if (self.is_open) {
            try self.park(allocator, self.doc.kind, self.doc.pathSlice());
            try self.closeProject(allocator, b);
        }
        self.active = kind;
        const slot = &self.parked[@intCast(@intFromEnum(kind))];
        const path = slot.* orelse return .empty;
        slot.* = null;
        defer allocator.free(path);
        _ = self.openProject(allocator, b, path, owner) catch {
            if (self.is_open) self.forget(allocator);
            self.active = kind;
            return .reopen_failed;
        };
        // A project parked under a kind reopens as that kind; keep the menu
        // on the entry the user chose even if the bridge says otherwise.
        self.active = kind;
        return .reopened;
    }
};

// --- Tests ----------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = core.fake_bridge.FakeResBridge;
const OwnedBytes = core.history.OwnedBytes;
const ResourceCommand = core.history.ResourceCommand;

/// One recorded property edit on the root, so the history is dirty.
fn editRoot(allocator: std.mem.Allocator, fake: *FakeResBridge, life: *Lifecycle) !void {
    const root_id = fake.nodes.items[0].id;
    var prop: bridge.PropRecord = .{ .id = 1 };
    _ = prop.setDefault("damage");
    _ = prop.setValue("10");
    try fake.nodes.items[0].props.append(allocator, prop);
    try life.doc.reload(allocator, fake.bridge());
    try life.history.reserve(allocator);
    var cmd: ResourceCommand = .{ .set_prop = .{
        .node = root_id,
        .prop_id = 1,
        .before = try OwnedBytes.fromSlice(allocator, "10"),
        .after = try OwnedBytes.fromSlice(allocator, "42"),
    } };
    try life.doc.apply(allocator, fake.bridge(), &cmd);
    life.history.recordAssumeCapacity(allocator, cmd, 0);
}

test "the Editors menu is MFC's: 20 entries, its order, labels and separators" {
    try testing.expectEqual(@as(usize, 20), editors_menu.len);
    const expected = [_]Kind{
        .mesh_unit,  .animation_infantry, .squad,   .weapon, .mine,
        .particle,   .sprite,             .effect,  .build,  .object,
        .fence,      .bridge,             .trench,  .mission, .chapter,
        .campaign,   .medal,              .tile_set, .road_3d, .river_3d,
    };
    for (editors_menu, expected) |entry, kind| try testing.expectEqual(kind, entry.kind);
    try testing.expectEqualStrings("Unit Editor", editors_menu[0].label);
    try testing.expectEqualStrings("Terrain Editor", editors_menu[17].label);
    var separators: [5]usize = undefined;
    var n: usize = 0;
    for (editors_menu, 0..) |entry, i| if (entry.separator_before) {
        separators[n] = i;
        n += 1;
    };
    try testing.expectEqualSlices(usize, &.{ 5, 8, 13, 17 }, separators[0..n]);
    // Every kind but the GUI frame appears exactly once.
    inline for (@typeInfo(Kind).@"enum".fields) |field| {
        const kind: Kind = @enumFromInt(field.value);
        var count: usize = 0;
        for (editors_menu) |entry| if (entry.kind == kind) {
            count += 1;
        };
        try testing.expectEqual(@as(usize, if (kind == .gui_frame) 0 else 1), count);
    }
}

test "extensions pick the sub-editor for all 21 kinds, in any case" {
    try testing.expectEqual(Kind.squad, kindFromExtension("scp").?);
    try testing.expectEqual(Kind.squad, kindFromExtension(".SCP").?);
    try testing.expectEqual(Kind.river_3d, kindFromPath("mods/x/River.3RV").?);
    try testing.expectEqual(Kind.gui_frame, kindFromPath("a/b/menu.gui").?);
    try testing.expect(kindFromPath("a/b/map.bzm") == null);
    try testing.expect(kindFromPath("noextension") == null);
    try testing.expect(kindFromExtension(".") == null);
    inline for (@typeInfo(Kind).@"enum".fields) |field| {
        const kind: Kind = @enumFromInt(field.value);
        try testing.expectEqual(kind, kindFromExtension(kind.extension()).?);
    }
}

test "the remembered sub-editor falls back to the Infantry Editor" {
    try testing.expectEqual(Kind.animation_infantry, restoreEditor(null));
    try testing.expectEqual(Kind.animation_infantry, restoreEditor(99));
    try testing.expectEqual(Kind.animation_infantry, restoreEditor(-1));
    try testing.expectEqual(Kind.animation_infantry, restoreEditor(@intFromEnum(Kind.gui_frame)));
    try testing.expectEqual(Kind.bridge, restoreEditor(@intFromEnum(Kind.bridge)));
}

test "menu enablement follows MFC's update handlers" {
    const closed: MenuState = .{};
    for ([_]MenuItem{ .close_project, .save, .save_as, .export_result, .export_stats_only, .undo, .redo }) |item|
        try testing.expect(!isEnabled(item, closed));
    for ([_]MenuItem{ .new_project, .open_project, .mod_settings, .pack_mod, .set_directories, .picture_options, .batch_mode, .run_game, .import_from_game, .exit }) |item|
        try testing.expect(isEnabled(item, closed));
    const open: MenuState = .{ .open = true, .can_undo = true };
    try testing.expect(isEnabled(.save, open));
    try testing.expect(isEnabled(.undo, open));
    try testing.expect(!isEnabled(.redo, open));
    const locked: MenuState = .{ .open = true, .read_only = true, .can_undo = true, .can_redo = true };
    try testing.expect(isEnabled(.save, locked));
    try testing.expect(!isEnabled(.undo, locked));
    try testing.expect(!isEnabled(.redo, locked));
}

test "Save becomes Save As without a path, on a locked project and under shipped data" {
    try testing.expectEqual(SaveRoute.save_as, saveRoute(null, false, false));
    try testing.expectEqual(SaveRoute.save_as, saveRoute("", false, false));
    try testing.expectEqual(SaveRoute.save, saveRoute("p.wpn", false, false));
    try testing.expectEqual(SaveRoute.save_as, saveRoute("p.wpn", true, false));
    try testing.expectEqual(SaveRoute.save_as, saveRoute("p.wpn", false, true));
}

test "the unsaved prompt: clean proceeds, Cancel drops, Don't save proceeds, Save waits for the save" {
    var prompt: UnsavedPrompt = .{};
    switch (prompt.guard(false, .close)) {
        .proceed => |action| try testing.expect(action == .close),
        .asked => return error.TestUnexpectedResult,
    }
    try testing.expect(prompt.guard(true, .quit) == .asked);
    try testing.expect(prompt.isAsking());
    try testing.expect(prompt.answer(.cancel, .save) == .dropped);
    try testing.expect(!prompt.isAsking());

    _ = prompt.guard(true, .{ .switch_editor = .medal });
    switch (prompt.answer(.dont_save, .save)) {
        .proceed => |action| try testing.expectEqual(Kind.medal, action.switch_editor),
        else => return error.TestUnexpectedResult,
    }

    _ = prompt.guard(true, .{ .open_path = PathText.fromSlice("a/b.scp").? });
    try testing.expect(prompt.answer(.save, .save_as) == .save_as);
    try testing.expect(prompt.saveFinished(false) == null);
    try testing.expect(prompt.saveFinished(true) == null); // nothing left to finish

    _ = prompt.guard(true, .{ .new_project = .weapon });
    try testing.expect(prompt.answer(.save, .save) == .save);
    const resumed = prompt.saveFinished(true).?;
    try testing.expectEqual(Kind.weapon, resumed.new_project);
    try testing.expect(prompt.answer(.save, .save) == .dropped);
}

test "PathText refuses rather than truncates" {
    var long: [path_capacity + 1]u8 = undefined;
    @memset(&long, 'p');
    try testing.expect(PathText.fromSlice(&long) == null);
    try testing.expectEqualStrings("x.wpn", PathText.fromSlice("x.wpn").?.slice());
}

test "New, edit, Save, Close through the fake bridge: dirty, routes and menu" {
    const allocator = testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    var life: Lifecycle = .{};
    defer life.deinit(allocator);

    try testing.expect(!isEnabled(.save, life.menuState()));
    try life.newProject(allocator, fake.bridge(), .squad);
    try testing.expect(life.is_open);
    try testing.expectEqual(Kind.squad, life.active);
    try testing.expect(!life.dirty());
    try testing.expectEqual(SaveRoute.save_as, life.saveRouteFor(false));
    try testing.expect(isEnabled(.save, life.menuState()));

    try editRoot(allocator, &fake, &life);
    try testing.expect(life.dirty());
    try testing.expect(isEnabled(.undo, life.menuState()));

    try life.saveProject(allocator, fake.bridge(), "work/one.scp");
    try testing.expect(!life.dirty());
    try testing.expectEqualStrings("work/one.scp", life.doc.pathSlice().?);
    try testing.expectEqual(SaveRoute.save, life.saveRouteFor(false));
    try testing.expect(fake.files.get("work/one.scp") != null);

    try life.closeProject(allocator, fake.bridge());
    try testing.expect(!life.is_open);
    try testing.expect(!life.dirty());
    try testing.expect(fake.kind == null);
    try testing.expect(!isEnabled(.close_project, life.menuState()));
    try testing.expectError(error.Refused, life.saveProject(allocator, fake.bridge(), "work/two.scp"));
}

test "Open takes the lock; another user's lock opens read-only and names them" {
    const allocator = testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    var life: Lifecycle = .{};
    defer life.deinit(allocator);

    try life.newProject(allocator, fake.bridge(), .weapon);
    try life.saveProject(allocator, fake.bridge(), "p/gun.wpn");
    try life.closeProject(allocator, fake.bridge());

    try testing.expectEqual(Lifecycle.OpenOutcome.opened, try life.openProject(allocator, fake.bridge(), "p/gun.wpn", "me"));
    try testing.expect(!life.read_only);
    try testing.expectEqualStrings("me", life.doc.lock_owner.?.name);
    try testing.expectEqualStrings("p/gun.wpn", life.doc.pathSlice().?);
    try life.closeProject(allocator, fake.bridge());

    // The fake keeps its lock across a close, standing in for another
    // user's `locked_*` left in the folder.
    allocator.free(fake.lock_owner.?);
    fake.lock_owner = try allocator.dupe(u8, "alice");
    try testing.expectEqual(Lifecycle.OpenOutcome.read_only, try life.openProject(allocator, fake.bridge(), "p/gun.wpn", "me"));
    try testing.expect(life.read_only);
    try testing.expectEqualStrings("alice", life.doc.lock_owner.?.name);
    try testing.expectEqual(SaveRoute.save_as, life.saveRouteFor(false));
    try testing.expectError(error.Refused, life.saveProject(allocator, fake.bridge(), "p/gun.wpn"));
    try life.saveProject(allocator, fake.bridge(), "p/copy.wpn");
    try testing.expect(fake.files.get("p/copy.wpn") != null);
}

test "Open refuses an unknown extension and a missing file without touching the open project" {
    const allocator = testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    var life: Lifecycle = .{};
    defer life.deinit(allocator);

    try life.newProject(allocator, fake.bridge(), .mine);
    try testing.expectError(error.Refused, life.openProject(allocator, fake.bridge(), "map.bzm", "me"));
    try testing.expectError(error.Failed, life.openProject(allocator, fake.bridge(), "absent.mcp", "me"));
    try testing.expect(life.is_open);
    try testing.expectEqual(Kind.mine, life.active);
}

test "an imported project is unsaved until its first save; an unported kind is refused" {
    const allocator = testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    var life: Lifecycle = .{};
    defer life.deinit(allocator);

    try fake.addGameFolder("units/humans/rifleman", "Rifleman");
    try life.importFromGame(allocator, fake.bridge(), .animation_infantry, "units/humans/rifleman");
    try testing.expect(life.dirty());
    try testing.expectEqual(SaveRoute.save_as, life.saveRouteFor(false));
    var prompt: UnsavedPrompt = .{};
    try testing.expect(prompt.guard(life.dirty(), .quit) == .asked);
    try life.saveProject(allocator, fake.bridge(), "src/rifleman.unt");
    try testing.expect(!life.dirty());

    try testing.expectError(error.Refused, life.importFromGame(allocator, fake.bridge(), .sprite, "units/humans/rifleman"));
    try testing.expect(life.is_open);
    try testing.expectEqualStrings("src/rifleman.unt", life.doc.pathSlice().?);
}

test "switching sub-editors parks a saved project and reopens it on the way back" {
    const allocator = testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    var life: Lifecycle = .{};
    defer life.deinit(allocator);

    try testing.expectEqual(Lifecycle.SwitchOutcome.unchanged, try life.switchEditor(allocator, fake.bridge(), default_editor, "me"));
    try life.newProject(allocator, fake.bridge(), .weapon);
    try life.saveProject(allocator, fake.bridge(), "p/gun.wpn");
    try testing.expectEqual(Lifecycle.SwitchOutcome.empty, try life.switchEditor(allocator, fake.bridge(), .medal, "me"));
    try testing.expect(!life.is_open);
    try testing.expectEqual(Kind.medal, life.active);

    try testing.expectEqual(Lifecycle.SwitchOutcome.reopened, try life.switchEditor(allocator, fake.bridge(), .weapon, "me"));
    try testing.expect(life.is_open);
    try testing.expectEqual(Kind.weapon, life.active);
    try testing.expectEqualStrings("p/gun.wpn", life.doc.pathSlice().?);
    try testing.expect(!life.dirty());
}

test "switching away from an untitled project closes it; a parked file that is gone leaves the editor empty" {
    const allocator = testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    var life: Lifecycle = .{};
    defer life.deinit(allocator);

    try life.newProject(allocator, fake.bridge(), .fence);
    try testing.expectEqual(Lifecycle.SwitchOutcome.empty, try life.switchEditor(allocator, fake.bridge(), .trench, "me"));
    try testing.expectEqual(Lifecycle.SwitchOutcome.empty, try life.switchEditor(allocator, fake.bridge(), .fence, "me"));
    try testing.expect(!life.is_open);

    try life.newProject(allocator, fake.bridge(), .fence);
    try life.saveProject(allocator, fake.bridge(), "p/wall.fnc");
    _ = try life.switchEditor(allocator, fake.bridge(), .trench, "me");
    const removed = fake.files.fetchRemove("p/wall.fnc").?;
    allocator.free(removed.key);
    allocator.free(removed.value);
    try testing.expectEqual(Lifecycle.SwitchOutcome.reopen_failed, try life.switchEditor(allocator, fake.bridge(), .fence, "me"));
    try testing.expect(!life.is_open);
    try testing.expectEqual(Kind.fence, life.active);
    try testing.expect(life.parked[@intCast(@intFromEnum(Kind.fence))] == null);
}
