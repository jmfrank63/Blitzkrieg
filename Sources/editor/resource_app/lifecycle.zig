//! The File menu's document lifecycle as one state machine with no window:
//! New (all 21 kinds), Open, Open Recent, Close, Save, Save As, the
//! unsaved-changes prompt guarding them and Quit, the `locked_<user>` prompt
//! with take-over, the session's one `.bak` per file, autosave and the
//! recovery copies in the user-data root (D002). The panels only draw what
//! `Session` asks for (a dialog, a modal) and hand the answers back; all the
//! decisions are here, so `test-resource-app-logic` runs them through the
//! fake resource bridge and the kit's fake files on every target.
//!
//! Paths are OS paths throughout: BkResOpen and BkResSave take them as they
//! are (std::filesystem), so unlike the map editor there is no engine form.
const std = @import("std");
const kit = @import("editor_kit");
const core = @import("resource_core");
const logic = @import("panels_logic.zig");
const app_settings = @import("settings.zig");

const bridge = core.bridge;
const Kind = bridge.Kind;
const ResBridge = bridge.ResBridge;
const Files = kit.files.Files;
const PathText = logic.PathText;
const Pending = logic.Pending;

// --- Names ----------------------------------------------------------------

/// File > New's label for a kind: its Editors menu label, and "GUI Editor"
/// for the one kind the Editors menu leaves out.
pub fn kindLabel(kind: Kind) [:0]const u8 {
    if (logic.menuEntry(kind)) |entry| return entry.label;
    return "GUI Editor";
}

/// `path` with the kind's extension appended when it has another one or
/// none: the bridge picks the format from the extension, and a project
/// saved as `gun` or `gun.txt` would not open as a weapon again.
pub fn withExtension(buffer: []u8, path: []const u8, kind: Kind) ?[]const u8 {
    if (logic.kindFromPath(path) == kind) {
        if (path.len > buffer.len) return null;
        @memcpy(buffer[0..path.len], path);
        return buffer[0..path.len];
    }
    return std.fmt.bufPrint(buffer, "{s}.{s}", .{ path, kind.extension() }) catch null;
}

// --- Recovery copies (D-20..D-22 of the map editor, D002) -----------------

/// `<user_root>resourceeditor/recovery`: never beside shipped data, never in
/// the project's own folder.
pub fn recoveryFolder(buffer: []u8, user_root: []const u8) ?[]const u8 {
    if (user_root.len == 0) return null;
    return std.fmt.bufPrint(buffer, "{s}{s}{c}recovery", .{ user_root, app_settings.user_folder, std.fs.path.sep }) catch null;
}

/// The recovery copy of a project: the kit's `recoveryName` (the stem of
/// `doc_path`, made safe, "untitled" when there is none) with the kind's own
/// extension, in `folder`.
pub fn recoveryPath(buffer: []u8, folder: []const u8, doc_path: ?[]const u8, kind: Kind) ?[]const u8 {
    var extension_buffer: [8]u8 = undefined;
    const extension = std.fmt.bufPrint(&extension_buffer, ".{s}", .{kind.extension()}) catch return null;
    var name_buffer: [256]u8 = undefined;
    const name = kit.autosave.recoveryName(&name_buffer, doc_path orelse "", extension) orelse return null;
    return std.fmt.bufPrint(buffer, "{s}{c}{s}", .{ folder, std.fs.path.sep, name }) catch null;
}

/// Whether `path` lies directly or deeper inside `folder`, either separator,
/// case-insensitively on Windows (`kit.settings.sameOsPath`).
pub fn isInFolder(path: []const u8, folder: []const u8) bool {
    if (folder.len == 0 or path.len <= folder.len + 1) return false;
    if (!kit.settings.sameOsPath(path[0..folder.len], folder)) return false;
    return path[folder.len] == '/' or path[folder.len] == '\\';
}

/// A recovery copy's sidecar (`<copy>.txt`): the project's own path (empty
/// for one never saved) and the Unix time of the copy, one per line.
pub fn formatSidecar(buffer: []u8, original: []const u8, unix_seconds: i64) ?[]const u8 {
    return std.fmt.bufPrint(buffer, "{s}\n{d}\n", .{ original, unix_seconds }) catch null;
}

pub const Sidecar = struct { original: []const u8, unix_seconds: i64 };

/// Lenient: a missing or malformed time is 0, CRLF is tolerated.
pub fn parseSidecar(text: []const u8) Sidecar {
    var lines = std.mem.splitScalar(u8, text, '\n');
    const original = std.mem.trimEnd(u8, lines.next() orelse "", "\r");
    const time_text = std.mem.trim(u8, lines.next() orelse "", " \r\t");
    return .{ .original = original, .unix_seconds = std.fmt.parseInt(i64, time_text, 10) catch 0 };
}

/// One recovery copy found at start-up, offered back once.
pub const RecoveryOffer = struct {
    path: PathText,
    original: PathText,
    unix_seconds: i64,
    kind: Kind,
};

/// A directory entry of the recovery folder as an offer: a file with one of
/// the 21 kinds' extensions (so not its `.txt` sidecar nor the bridge's
/// `.bak`) whose sidecar read. Null for anything else.
pub fn offerFromEntry(folder: []const u8, name: []const u8, sidecar_text: ?[]const u8) ?RecoveryOffer {
    const kind = logic.kindFromPath(name) orelse return null;
    const text = sidecar_text orelse return null;
    var path_buffer: [logic.path_capacity]u8 = undefined;
    const path = std.fmt.bufPrint(&path_buffer, "{s}{c}{s}", .{ folder, std.fs.path.sep, name }) catch return null;
    const sidecar = parseSidecar(text);
    return .{
        .path = PathText.fromSlice(path) orelse return null,
        .original = PathText.fromSlice(sidecar.original) orelse return null,
        .unix_seconds = sidecar.unix_seconds,
        .kind = kind,
    };
}

// --- The session's .bak ---------------------------------------------------

/// D-19's "one `.bak` per file per session": BkResSave copies the file it
/// replaces to `<path>.bak` on every save, so on the second and later saves of
/// a session the `.bak` that held the version from before the session is
/// set aside (`<path>.bak.~keep`) around the save and put back after it, and
/// a file this session created keeps no `.bak` at all. The first save of a
/// path is the bridge's own: what was there becomes the `.bak`.
pub const SessionBackups = struct {
    saved: std.ArrayListUnmanaged([]u8) = .empty,

    pub const Held = enum { none, kept, drop_new };

    pub fn deinit(self: *SessionBackups, allocator: std.mem.Allocator) void {
        for (self.saved.items) |p| allocator.free(p);
        self.saved.deinit(allocator);
    }

    fn contains(self: *const SessionBackups, path: []const u8) bool {
        for (self.saved.items) |p| if (kit.settings.sameOsPath(p, path)) return true;
        return false;
    }

    /// Before the bridge writes `path`.
    pub fn before(self: *const SessionBackups, files: Files, path: []const u8) Held {
        if (!self.contains(path)) return .none;
        var bak_buffer: [kit.shipped.max_path]u8 = undefined;
        var keep_buffer: [kit.shipped.max_path]u8 = undefined;
        const bak = kit.files.backupPathFor(&bak_buffer, path) orelse return .none;
        const keep = std.fmt.bufPrint(&keep_buffer, "{s}.~keep", .{bak}) catch return .none;
        if (!files.exists(bak)) return .drop_new;
        files.rename(bak, keep) catch return .none;
        return .kept;
    }

    /// After the bridge's save, whether it landed or not.
    pub fn after(self: *SessionBackups, allocator: std.mem.Allocator, files: Files, path: []const u8, held: Held, ok: bool) void {
        var bak_buffer: [kit.shipped.max_path]u8 = undefined;
        var keep_buffer: [kit.shipped.max_path]u8 = undefined;
        if (kit.files.backupPathFor(&bak_buffer, path)) |bak| switch (held) {
            .none => {},
            .kept => if (std.fmt.bufPrint(&keep_buffer, "{s}.~keep", .{bak})) |keep| {
                files.rename(keep, bak) catch {};
            } else |_| {},
            .drop_new => if (ok) files.delete(bak),
        };
        if (ok and !self.contains(path)) {
            const owned = allocator.dupe(u8, path) catch return;
            self.saved.append(allocator, owned) catch allocator.free(owned);
        }
    }
};

// --- The lock prompt ------------------------------------------------------

/// Another user's `locked_*` was in the project's folder: the project is
/// open read-only and the host asks whether to stay that way or take the
/// lock over (BkResLockTakeOver, MFC's own choice after its warning).
pub const LockPrompt = struct {
    asking: bool = false,
    owner_buffer: [256]u8 = undefined,
    owner_len: usize = 0,

    pub const Choice = enum { read_only, take_over };

    pub fn ask(self: *LockPrompt, who: []const u8) void {
        self.owner_len = @min(who.len, self.owner_buffer.len);
        @memcpy(self.owner_buffer[0..self.owner_len], who[0..self.owner_len]);
        self.asking = true;
    }

    pub fn owner(self: *const LockPrompt) []const u8 {
        return self.owner_buffer[0..self.owner_len];
    }
};

/// The lock user name the engine uses for `locked_<user>`: the
/// BK_RESOURCE_EDITOR_USER seam, else the login (`USER` / `USERNAME`), else
/// "unknown" - resource_bridge.cpp's LockUserName, so the mirror and the file
/// agree.
pub fn lockUserName(seam: ?[]const u8, user: ?[]const u8, username: ?[]const u8) []const u8 {
    for ([_]?[]const u8{ seam, user, username }) |candidate| {
        if (candidate) |name| if (name.len != 0) return name;
    }
    return "unknown";
}

// --- The session ----------------------------------------------------------

/// What the host lends a `Session` call: the bridge, the disk (null: no
/// disk checks, as some tests do), the engine's roots, the lock user name,
/// the settings the recent list lives in, and the bridge's lock take-over,
/// which is not on the core's ResBridge (an adapter-only call).
pub const Ctx = struct {
    allocator: std.mem.Allocator,
    bridge: ResBridge,
    files: ?Files = null,
    base_root: []const u8 = "",
    user_root: []const u8 = "",
    owner: []const u8,
    settings: *app_settings.Settings,
    take_over: TakeOver,
};

pub const TakeOver = struct {
    ptr: *anyopaque,
    call: *const fn (ptr: *anyopaque) bridge.Status,
};

/// The file dialog the host shows next, if any.
pub const Dialog = enum {
    open,
    save_as,
    /// Save As asked by the unsaved-changes prompt: its answer resumes (or
    /// drops) the guarded action.
    save_as_for_prompt,
};

pub const AutosaveResult = union(enum) {
    none,
    /// Written into the project's own file, a real save.
    project_file,
    /// Written as a recovery copy; the host writes its sidecar.
    recovery: PathText,
    failed,
};

pub const Session = struct {
    life: logic.Lifecycle = .{},
    prompt: logic.UnsavedPrompt = .{},
    lock_prompt: LockPrompt = .{},
    backups: SessionBackups = .{},
    autosave: kit.autosave.Autosave = .{},
    /// This project's recovery copy, while it has one: autosave keeps
    /// writing there, and a real save or a discard deletes it.
    recovery_active: ?PathText = null,
    dialog: ?Dialog = null,
    /// The recent list or another setting changed: the host writes the
    /// settings file and clears this.
    settings_changed: bool = false,
    quit: bool = false,
    message_buffer: [512]u8 = undefined,
    message_len: usize = 0,

    pub fn deinit(self: *Session, allocator: std.mem.Allocator) void {
        self.life.deinit(allocator);
        self.backups.deinit(allocator);
    }

    pub fn message(self: *const Session) []const u8 {
        return self.message_buffer[0..self.message_len];
    }

    fn say(self: *Session, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.message_buffer, format, args) catch blk: {
            break :blk self.message_buffer[0..];
        };
        self.message_len = text.len;
    }

    /// Whether a modal or dialog of the lifecycle is up: nothing else may
    /// start, and autosave waits.
    pub fn busy(self: *const Session) bool {
        return self.dialog != null or self.prompt.phase != .idle or self.lock_prompt.asking;
    }

    /// Whether `path` is shipped data (the kit's rule, Save becomes Save As).
    pub fn isShipped(ctx: *const Ctx, path: []const u8) bool {
        return kit.shipped.isShipped(path, ctx.base_root, ctx.files);
    }

    fn inRecoveryFolder(ctx: *const Ctx, path: []const u8) bool {
        var buffer: [logic.path_capacity]u8 = undefined;
        const folder = recoveryFolder(&buffer, ctx.user_root) orelse return false;
        return isInFolder(path, folder);
    }

    /// Save or Save As for the open project: Save As without a path, for a
    /// read-only (locked) project, shipped data and a recovery copy reopened
    /// from the recovery folder (it leaves that folder only by Save As).
    pub fn route(self: *const Session, ctx: *const Ctx) logic.SaveRoute {
        const path = self.life.doc.pathSlice() orelse return .save_as;
        return self.life.saveRouteFor(isShipped(ctx, path) or inRecoveryFolder(ctx, path));
    }

    /// A menu action: runs at once when nothing is unsaved, otherwise the
    /// unsaved-changes prompt asks first. Ignored while another modal is up.
    pub fn request(self: *Session, ctx: *const Ctx, action: Pending) void {
        if (self.busy()) return;
        switch (self.prompt.guard(self.life.dirty(), action)) {
            .proceed => |go| self.perform(ctx, go),
            .asked => {},
        }
    }

    /// The prompt's answer.
    pub fn answerUnsaved(self: *Session, ctx: *const Ctx, choice: logic.UnsavedPrompt.Choice) void {
        switch (self.prompt.answer(choice, self.route(ctx))) {
            .save => {
                const ok = self.saveTo(ctx, self.life.doc.pathSlice().?);
                if (self.prompt.saveFinished(ok)) |go| self.perform(ctx, go);
            },
            .save_as => self.dialog = .save_as_for_prompt,
            .proceed => |go| self.perform(ctx, go),
            .dropped => {},
        }
    }

    /// File > Save.
    pub fn save(self: *Session, ctx: *const Ctx) void {
        if (!self.life.is_open or self.busy()) return;
        switch (self.route(ctx)) {
            .save => _ = self.saveTo(ctx, self.life.doc.pathSlice().?),
            .save_as => self.dialog = .save_as,
        }
    }

    /// File > Save As.
    pub fn saveAs(self: *Session) void {
        if (!self.life.is_open or self.busy()) return;
        self.dialog = .save_as;
    }

    /// The file dialog the host showed answered: `path`, or null for a
    /// cancel.
    pub fn dialogAnswered(self: *Session, ctx: *const Ctx, path: ?[]const u8) void {
        const which = self.dialog orelse return;
        self.dialog = null;
        switch (which) {
            .open => if (path) |p| self.openPath(ctx, p),
            .save_as => if (path) |p| {
                _ = self.saveAsTo(ctx, p);
            },
            .save_as_for_prompt => {
                const ok = if (path) |p| self.saveAsTo(ctx, p) else false;
                if (self.prompt.saveFinished(ok)) |go| self.perform(ctx, go);
            },
        }
    }

    /// Whether the lock prompt's answer took the lock over.
    pub fn answerLock(self: *Session, ctx: *const Ctx, choice: LockPrompt.Choice) bool {
        if (!self.lock_prompt.asking) return false;
        self.lock_prompt.asking = false;
        if (choice == .read_only) {
            self.say("opened read-only: locked by {s}", .{self.lock_prompt.owner()});
            return false;
        }
        if (ctx.take_over.call(ctx.take_over.ptr) != .ok) {
            self.say("the lock was not taken over: {s}", .{ctx.bridge.lastMessage()});
            return false;
        }
        self.life.read_only = false;
        if (self.life.doc.lock_owner) |*current| current.deinit(ctx.allocator);
        self.life.doc.lock_owner = core.document.LockOwner.fromSlice(ctx.allocator, ctx.owner) catch null;
        self.say("took the lock over from {s}", .{self.lock_prompt.owner()});
        return true;
    }

    fn perform(self: *Session, ctx: *const Ctx, action: Pending) void {
        switch (action) {
            .new_project => |kind| {
                self.life.newProject(ctx.allocator, ctx.bridge, kind) catch {
                    self.say("a new {s} project was refused: {s}", .{ kind.extension(), ctx.bridge.lastMessage() });
                    return;
                };
                self.dropRecovery(ctx);
                self.say("new {s} project", .{kind.extension()});
            },
            .open_dialog => self.dialog = .open,
            .open_path => |path| self.openPath(ctx, path.slice()),
            .import_from_game => |import| {
                const folder = import.folder.slice();
                // A refusal (a kind whose import is not ported) keeps the
                // open project; the bridge's message names the kind.
                self.life.importFromGame(ctx.allocator, ctx.bridge, import.kind, folder) catch {
                    self.say("import of {s} from {s} refused: {s}", .{ import.kind.extension(), folder, ctx.bridge.lastMessage() });
                    return;
                };
                self.dropRecovery(ctx);
                self.say("imported a new, unsaved {s} project from {s}", .{ import.kind.extension(), folder });
            },
            .close => {
                self.life.closeProject(ctx.allocator, ctx.bridge) catch {
                    self.say("close failed: {s}", .{ctx.bridge.lastMessage()});
                    return;
                };
                self.dropRecovery(ctx);
                self.say("closed", .{});
            },
            .quit => {
                self.dropRecovery(ctx);
                self.quit = true;
            },
            .switch_editor => |kind| {
                const outcome = self.life.switchEditor(ctx.allocator, ctx.bridge, kind, ctx.owner) catch {
                    self.say("switching to {s} failed: {s}", .{ kindLabel(kind), ctx.bridge.lastMessage() });
                    return;
                };
                if (outcome != .unchanged) self.dropRecovery(ctx);
                ctx.settings.last_editor = @intFromEnum(kind);
                self.settings_changed = true;
                if (self.life.read_only) self.askLock();
            },
        }
    }

    fn askLock(self: *Session) void {
        const owner = if (self.life.doc.lock_owner) |o| o.name else "another user";
        self.lock_prompt.ask(owner);
    }

    fn openPath(self: *Session, ctx: *const Ctx, path: []const u8) void {
        const outcome = self.life.openProject(ctx.allocator, ctx.bridge, path, ctx.owner) catch |err| {
            if (err == error.Refused and logic.kindFromPath(path) == null) {
                self.say("{s} is not a resource project", .{path});
            } else {
                self.say("could not open {s}: {s}", .{ path, ctx.bridge.lastMessage() });
            }
            ctx.settings.forgetRecent(path);
            self.settings_changed = true;
            return;
        };
        self.dropRecovery(ctx);
        if (inRecoveryFolder(ctx, path)) {
            // A recovery copy opened back: autosave keeps writing it until
            // its own Save As takes it out of the recovery folder.
            self.recovery_active = PathText.fromSlice(path);
        } else {
            ctx.settings.pushRecent(path);
            if (std.fs.path.dirname(path)) |folder| ctx.settings.projects_folder.set(folder);
            self.settings_changed = true;
        }
        switch (outcome) {
            .opened => self.say("opened {s}", .{path}),
            .read_only => self.askLock(),
        }
    }

    /// Save As to a path a dialog chose: the kind's extension is added when
    /// it is missing.
    fn saveAsTo(self: *Session, ctx: *const Ctx, chosen: []const u8) bool {
        var buffer: [logic.path_capacity]u8 = undefined;
        const path = withExtension(&buffer, chosen, self.life.doc.kind) orelse {
            self.say("the path is too long", .{});
            return false;
        };
        return self.saveTo(ctx, path);
    }

    /// The one write path for Save, Save As, the prompt's Save and autosave
    /// into the project's own file. Never into shipped data (Save As there is
    /// refused, D-18's rule), the session's `.bak` kept, and on success the
    /// recent list, the projects folder and the recovery copy follow.
    fn saveTo(self: *Session, ctx: *const Ctx, path: []const u8) bool {
        if (isShipped(ctx, path)) {
            self.say("{s} is shipped game data: save the project in a folder of your own", .{path});
            return false;
        }
        const held: SessionBackups.Held = if (ctx.files) |files| self.backups.before(files, path) else .none;
        var ok = true;
        self.life.saveProject(ctx.allocator, ctx.bridge, path) catch {
            ok = false;
        };
        if (ctx.files) |files| self.backups.after(ctx.allocator, files, path, held, ok);
        if (!ok) {
            self.say("save failed: {s}", .{if (self.life.read_only and !isShipped(ctx, path)) "the project is locked by another user; use Save As" else ctx.bridge.lastMessage()});
            return false;
        }
        if (self.recovery_active) |active| {
            if (!kit.settings.sameOsPath(active.slice(), path)) self.dropRecovery(ctx);
        }
        if (!inRecoveryFolder(ctx, path)) {
            ctx.settings.pushRecent(path);
            if (std.fs.path.dirname(path)) |folder| ctx.settings.projects_folder.set(folder);
            self.settings_changed = true;
        }
        self.say("saved {s}", .{path});
        return true;
    }

    /// Deletes this project's recovery copy, its sidecar and the `.bak` the
    /// bridge left beside it.
    fn dropRecovery(self: *Session, ctx: *const Ctx) void {
        const active = self.recovery_active orelse return;
        self.recovery_active = null;
        const files = ctx.files orelse return;
        files.delete(active.slice());
        var buffer: [logic.path_capacity + 8]u8 = undefined;
        if (std.fmt.bufPrint(&buffer, "{s}.txt", .{active.slice()})) |sidecar| files.delete(sidecar) else |_| {}
        if (std.fmt.bufPrint(&buffer, "{s}.bak", .{active.slice()})) |bak| files.delete(bak) else |_| {}
    }

    /// Once per frame in the interactive mode. Due (the kit's schedule, the
    /// settings' interval) and nothing modal up: a project with its own
    /// writable file is saved into it; any other (untitled, locked, shipped,
    /// a reopened recovery copy) is written as a recovery copy under the
    /// user root, which BkResSave writes like any save. A failure waits a
    /// full interval before the next try.
    pub fn tickAutosave(self: *Session, ctx: *const Ctx, now_ms: u64) AutosaveResult {
        self.autosave.enabled = ctx.settings.autosave;
        self.autosave.interval_ms = @as(u64, ctx.settings.autosave_minutes) * std.time.ms_per_min;
        const dirty = self.life.dirty();
        self.autosave.note(now_ms, dirty);
        if (!self.life.is_open or self.busy()) return .none;
        if (!self.autosave.due(now_ms, dirty)) return .none;
        self.autosave.wrote(now_ms);
        const needs_save_as = self.route(ctx) == .save_as;
        switch (kit.autosave.target(needs_save_as)) {
            .map_file => return if (self.saveTo(ctx, self.life.doc.pathSlice().?)) .project_file else .failed,
            .recovery_copy => {
                var path_buffer: [logic.path_capacity]u8 = undefined;
                var folder_buffer: [logic.path_capacity]u8 = undefined;
                const path: []const u8 = if (self.recovery_active) |*active| active.slice() else blk: {
                    const folder = recoveryFolder(&folder_buffer, ctx.user_root) orelse {
                        self.say("autosave failed: no user folder for the recovery copy", .{});
                        return .failed;
                    };
                    break :blk recoveryPath(&path_buffer, folder, self.life.doc.pathSlice(), self.life.doc.kind) orelse {
                        self.say("autosave failed: the recovery path is too long", .{});
                        return .failed;
                    };
                };
                const text = PathText.fromSlice(path) orelse return .failed;
                if (ctx.bridge.save(text.slice()) != .ok) {
                    self.say("autosave failed: {s}", .{ctx.bridge.lastMessage()});
                    return .failed;
                }
                self.recovery_active = text;
                self.say("recovery copy written", .{});
                return .{ .recovery = text };
            },
        }
    }

    /// The recovery offer's Open: the copy opens from the recovery folder and
    /// stays a recovery copy until its Save As.
    pub fn openRecovery(self: *Session, ctx: *const Ctx, offer: *const RecoveryOffer) void {
        self.request(ctx, .{ .open_path = offer.path });
    }
};

// --- Tests ----------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = core.fake_bridge.FakeResBridge;
const FakeFiles = kit.files.FakeFiles;
const sep = [_]u8{std.fs.path.sep};

/// A test's Ctx over the fake bridge: the take-over hands the fake's lock to
/// the test's user, as BkResLockTakeOver removes every other `locked_*`.
const Harness = struct {
    fake: FakeResBridge,
    files: FakeFiles,
    settings: app_settings.Settings = .{},
    session: Session = .{},

    fn init(self: *Harness) void {
        self.* = .{ .fake = FakeResBridge.init(testing.allocator), .files = FakeFiles.init(testing.allocator) };
    }

    fn deinit(self: *Harness) void {
        self.session.deinit(testing.allocator);
        self.fake.deinit();
        self.files.deinit();
    }

    fn ctx(self: *Harness) Ctx {
        return .{
            .allocator = testing.allocator,
            .bridge = self.fake.bridge(),
            .files = self.files.files(),
            .base_root = "/game/",
            .user_root = "/home/me/.local/share/blitzkrieg/",
            .owner = "me",
            .settings = &self.settings,
            .take_over = .{ .ptr = self, .call = takeOver },
        };
    }

    fn takeOver(ptr: *anyopaque) bridge.Status {
        const self: *Harness = @ptrCast(@alignCast(ptr));
        if (self.fake.lock_owner) |old| testing.allocator.free(old);
        self.fake.lock_owner = testing.allocator.dupe(u8, "me") catch return .failed;
        return .ok;
    }

    /// The fake keeps one lock across a close; a real close releases ours.
    fn releaseLock(self: *Harness) void {
        if (self.fake.lock_owner) |old| testing.allocator.free(old);
        self.fake.lock_owner = null;
    }

    fn edit(self: *Harness) !void {
        const allocator = testing.allocator;
        const life = &self.session.life;
        const root_id = self.fake.nodes.items[0].id;
        var prop: bridge.PropRecord = .{ .id = 1 };
        _ = prop.setDefault("damage");
        _ = prop.setValue("10");
        try self.fake.nodes.items[0].props.append(allocator, prop);
        try life.doc.reload(allocator, self.fake.bridge());
        try life.history.reserve(allocator);
        var cmd: core.history.ResourceCommand = .{ .set_prop = .{
            .node = root_id,
            .prop_id = 1,
            .before = try core.history.OwnedBytes.fromSlice(allocator, "10"),
            .after = try core.history.OwnedBytes.fromSlice(allocator, "42"),
        } };
        try life.doc.apply(allocator, self.fake.bridge(), &cmd);
        life.history.recordAssumeCapacity(allocator, cmd, 0);
    }
};

test "File > New offers all 21 kinds with a label each" {
    var seen: usize = 0;
    inline for (std.enums.values(Kind)) |kind| {
        try testing.expect(kindLabel(kind).len != 0);
        seen += 1;
    }
    try testing.expectEqual(@as(usize, 21), seen);
    try testing.expectEqualStrings("GUI Editor", kindLabel(.gui_frame));
    try testing.expectEqualStrings("Weapon Editor", kindLabel(.weapon));
}

test "New for every kind opens an empty, clean, untitled project through the session" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    inline for (std.enums.values(Kind)) |kind| {
        h.session.request(&ctx, .{ .new_project = kind });
        try testing.expect(h.session.life.is_open);
        try testing.expectEqual(kind, h.session.life.doc.kind);
        try testing.expect(!h.session.life.dirty());
        try testing.expect(h.session.life.doc.pathSlice() == null);
        try testing.expectEqual(logic.SaveRoute.save_as, h.session.route(&ctx));
    }
}

test "withExtension appends the kind's extension unless it is already there" {
    var buffer: [64]u8 = undefined;
    try testing.expectEqualStrings("a/gun.wpn", withExtension(&buffer, "a/gun.wpn", .weapon).?);
    try testing.expectEqualStrings("a/gun.WPN", withExtension(&buffer, "a/gun.WPN", .weapon).?);
    try testing.expectEqualStrings("a/gun.wpn", withExtension(&buffer, "a/gun", .weapon).?);
    try testing.expectEqualStrings("a/gun.scp.wpn", withExtension(&buffer, "a/gun.scp", .weapon).?);
    var tiny: [4]u8 = undefined;
    try testing.expect(withExtension(&tiny, "a/gun", .weapon) == null);
}

test "recovery paths live in the user root's resourceeditor folder with the kind's extension" {
    var folder_buffer: [128]u8 = undefined;
    const folder = recoveryFolder(&folder_buffer, "/u/").?;
    try testing.expectEqualStrings("/u/resourceeditor" ++ sep ++ "recovery", folder);
    try testing.expect(recoveryFolder(&folder_buffer, "") == null);
    var buffer: [256]u8 = undefined;
    try testing.expectEqualStrings("/u/resourceeditor" ++ sep ++ "recovery" ++ sep ++ "untitled.scp", recoveryPath(&buffer, folder, null, .squad).?);
    try testing.expectEqualStrings("/u/resourceeditor" ++ sep ++ "recovery" ++ sep ++ "my_gun.wpn", recoveryPath(&buffer, folder, "/game/Data/my gun.wpn", .weapon).?);
    try testing.expect(isInFolder("/u/resourceeditor" ++ sep ++ "recovery" ++ sep ++ "a.wpn", folder));
    try testing.expect(!isInFolder("/u/resourceeditor" ++ sep ++ "recoveryX" ++ sep ++ "a.wpn", folder));
    try testing.expect(!isInFolder(folder, folder));
}

test "sidecars round-trip; offers take only project files with a sidecar" {
    var buffer: [128]u8 = undefined;
    const text = formatSidecar(&buffer, "/p/gun.wpn", 1700000000).?;
    const back = parseSidecar(text);
    try testing.expectEqualStrings("/p/gun.wpn", back.original);
    try testing.expectEqual(@as(i64, 1700000000), back.unix_seconds);
    const crlf = parseSidecar("\r\nnot-a-number\r\n");
    try testing.expectEqualStrings("", crlf.original);
    try testing.expectEqual(@as(i64, 0), crlf.unix_seconds);

    const offer = offerFromEntry("/r", "gun.wpn", text).?;
    try testing.expectEqualStrings("/r" ++ sep ++ "gun.wpn", offer.path.slice());
    try testing.expectEqualStrings("/p/gun.wpn", offer.original.slice());
    try testing.expectEqual(Kind.weapon, offer.kind);
    try testing.expect(offerFromEntry("/r", "gun.wpn.txt", text) == null);
    try testing.expect(offerFromEntry("/r", "gun.wpn.bak", text) == null);
    try testing.expect(offerFromEntry("/r", "gun.wpn", null) == null);
}

test "the session's .bak holds the version from before the session, and a new file gets none" {
    const allocator = testing.allocator;
    var fake_files = FakeFiles.init(allocator);
    defer fake_files.deinit();
    const files = fake_files.files();
    var backups: SessionBackups = .{};
    defer backups.deinit(allocator);

    // BkResSave's own steps: the file it replaces to .bak, then the new bytes.
    const bridgeSave = struct {
        fn run(ff: *FakeFiles, path: []const u8, bytes: []const u8) !void {
            var bak: [64]u8 = undefined;
            if (ff.contents(path) != null) try ff.files().copy(path, kit.files.backupPathFor(&bak, path).?);
            try ff.write(path, bytes);
        }
    }.run;

    try fake_files.write("p/gun.wpn", "opened");
    var held = backups.before(files, "p/gun.wpn");
    try bridgeSave(&fake_files, "p/gun.wpn", "first");
    backups.after(allocator, files, "p/gun.wpn", held, true);
    try testing.expectEqualStrings("opened", fake_files.contents("p/gun.wpn.bak").?);

    held = backups.before(files, "p/gun.wpn");
    try testing.expectEqual(SessionBackups.Held.kept, held);
    try bridgeSave(&fake_files, "p/gun.wpn", "second");
    backups.after(allocator, files, "p/gun.wpn", held, true);
    try testing.expectEqualStrings("second", fake_files.contents("p/gun.wpn").?);
    try testing.expectEqualStrings("opened", fake_files.contents("p/gun.wpn.bak").?);
    try testing.expect(fake_files.contents("p/gun.wpn.bak.~keep") == null);

    held = backups.before(files, "p/new.wpn");
    try bridgeSave(&fake_files, "p/new.wpn", "one");
    backups.after(allocator, files, "p/new.wpn", held, true);
    held = backups.before(files, "p/new.wpn");
    try testing.expectEqual(SessionBackups.Held.drop_new, held);
    try bridgeSave(&fake_files, "p/new.wpn", "two");
    backups.after(allocator, files, "p/new.wpn", held, true);
    try testing.expect(fake_files.contents("p/new.wpn.bak") == null);
}

test "Save on an untitled project asks for Save As; the dialog's path gets the extension and joins the recent list" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    h.session.request(&ctx, .{ .new_project = .weapon });
    try h.edit();
    h.session.save(&ctx);
    try testing.expectEqual(Dialog.save_as, h.session.dialog.?);
    h.session.dialogAnswered(&ctx, "/work/gun");
    try testing.expect(h.session.dialog == null);
    try testing.expectEqualStrings("/work/gun.wpn", h.session.life.doc.pathSlice().?);
    try testing.expect(!h.session.life.dirty());
    try testing.expect(h.fake.files.get("/work/gun.wpn") != null);
    try testing.expectEqualStrings("/work/gun.wpn", h.settings.recentAt(0));
    try testing.expectEqualStrings("/work", h.settings.projectsFolder());
    try testing.expect(h.session.settings_changed);

    // Now it has a path: Save writes in place with no dialog.
    try h.edit();
    h.session.save(&ctx);
    try testing.expect(h.session.dialog == null);
    try testing.expect(!h.session.life.dirty());
}

test "a shipped project's Save becomes Save As, and Save As into shipped data is refused" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    h.session.request(&ctx, .{ .new_project = .squad });
    _ = h.session.saveTo(&ctx, "/work/a.scp");
    h.releaseLock();
    // Stand a shipped copy in: the fake bridge opens it from its own table.
    try h.fake.files.put(testing.allocator, try testing.allocator.dupe(u8, "/game/Data/units/a.scp"), try testing.allocator.dupe(u8, h.fake.files.get("/work/a.scp").?));
    h.session.request(&ctx, .{ .open_path = PathText.fromSlice("/game/Data/units/a.scp").? });
    try testing.expect(h.session.life.is_open);
    try testing.expectEqual(logic.SaveRoute.save_as, h.session.route(&ctx));
    h.session.save(&ctx);
    try testing.expectEqual(Dialog.save_as, h.session.dialog.?);
    h.session.dialogAnswered(&ctx, "/game/Data/units/b.scp");
    try testing.expect(h.fake.files.get("/game/Data/units/b.scp") == null);
    try testing.expect(std.mem.indexOf(u8, h.session.message(), "shipped") != null);
    h.session.saveAs();
    h.session.dialogAnswered(&ctx, "/work/b.scp");
    try testing.expectEqualStrings("/work/b.scp", h.session.life.doc.pathSlice().?);
}

test "close and quit ask when dirty: Cancel keeps it, Don't save goes on, Save via Save As then goes on" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    h.session.request(&ctx, .{ .new_project = .mine });
    try h.edit();

    h.session.request(&ctx, .close);
    try testing.expect(h.session.prompt.isAsking());
    try testing.expect(h.session.busy());
    h.session.answerUnsaved(&ctx, .cancel);
    try testing.expect(h.session.life.is_open);

    h.session.request(&ctx, .quit);
    h.session.answerUnsaved(&ctx, .save);
    try testing.expectEqual(Dialog.save_as_for_prompt, h.session.dialog.?);
    h.session.dialogAnswered(&ctx, null); // a cancelled Save As drops the quit
    try testing.expect(!h.session.quit);
    try testing.expect(!h.session.busy());

    h.session.request(&ctx, .quit);
    h.session.answerUnsaved(&ctx, .save);
    h.session.dialogAnswered(&ctx, "/work/m.mcp");
    try testing.expect(h.session.quit);
    try testing.expect(h.fake.files.get("/work/m.mcp") != null);

    h.session.quit = false;
    try h.edit();
    h.session.request(&ctx, .close);
    h.session.answerUnsaved(&ctx, .dont_save);
    try testing.expect(!h.session.life.is_open);
}

test "another user's lock opens read-only and asks; take-over makes it writable" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    h.session.request(&ctx, .{ .new_project = .weapon });
    _ = h.session.saveTo(&ctx, "/work/gun.wpn");
    h.session.request(&ctx, .close);
    h.releaseLock();
    h.fake.lock_owner = try testing.allocator.dupe(u8, "alice");

    h.session.request(&ctx, .{ .open_path = PathText.fromSlice("/work/gun.wpn").? });
    try testing.expect(h.session.life.read_only);
    try testing.expect(h.session.lock_prompt.asking);
    try testing.expectEqualStrings("alice", h.session.lock_prompt.owner());
    try testing.expectEqual(logic.SaveRoute.save_as, h.session.route(&ctx));
    try testing.expect(!h.session.answerLock(&ctx, .read_only));
    try testing.expect(h.session.life.read_only);

    h.session.request(&ctx, .close);
    h.session.request(&ctx, .{ .open_path = PathText.fromSlice("/work/gun.wpn").? });
    try testing.expect(h.session.answerLock(&ctx, .take_over));
    try testing.expect(!h.session.life.read_only);
    try testing.expectEqualStrings("me", h.session.life.doc.lock_owner.?.name);
    try testing.expectEqual(logic.SaveRoute.save, h.session.route(&ctx));
}

test "a recent entry that no longer opens is dropped from the list" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    h.settings.pushRecent("/work/gone.wpn");
    h.session.request(&ctx, .{ .open_path = PathText.fromSlice("/work/gone.wpn").? });
    try testing.expectEqual(@as(usize, 0), h.settings.recentCount());
    try testing.expect(std.mem.indexOf(u8, h.session.message(), "could not open") != null);
}

test "autosave: an untitled project goes to a recovery copy in the user root, a saved one into itself" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    h.settings.autosave_minutes = 1;
    const minute = std.time.ms_per_min;
    h.session.request(&ctx, .{ .new_project = .bridge });
    try testing.expect(h.session.tickAutosave(&ctx, 0) == .none); // clean
    try h.edit();
    try testing.expect(h.session.tickAutosave(&ctx, 10) == .none);
    const result = h.session.tickAutosave(&ctx, 10 + minute);
    const expected = "/home/me/.local/share/blitzkrieg/resourceeditor" ++ sep ++ "recovery" ++ sep ++ "untitled.bdg";
    try testing.expectEqualStrings(expected, result.recovery.slice());
    try testing.expect(h.fake.files.get(expected) != null);
    try testing.expect(h.session.life.dirty()); // a recovery copy is not a save
    try testing.expect(h.session.life.doc.pathSlice() == null);

    // Not again until a full interval after that write; never while a modal is up.
    try testing.expect(h.session.tickAutosave(&ctx, 20 + minute) == .none);
    h.session.request(&ctx, .close);
    try testing.expect(h.session.tickAutosave(&ctx, 10 + 3 * minute) == .none);
    h.session.answerUnsaved(&ctx, .cancel);

    // Saving for real drops the recovery copy; autosave then writes the file itself.
    try h.files.write(expected, "copy");
    h.session.saveAs();
    h.session.dialogAnswered(&ctx, "/work/span.bdg");
    try testing.expect(h.session.recovery_active == null);
    try testing.expect(h.files.contents(expected) == null);
    try testing.expect(h.session.tickAutosave(&ctx, 4 * minute) == .none); // clean again: the schedule resets
    try h.edit();
    try testing.expect(h.session.tickAutosave(&ctx, 4 * minute + 1) == .none);
    try testing.expect(h.session.tickAutosave(&ctx, 5 * minute + 1) == .project_file);
    try testing.expect(!h.session.life.dirty());

    // Autosave off: nothing is ever written.
    h.settings.autosave = false;
    try h.edit();
    try testing.expect(h.session.tickAutosave(&ctx, 60 * minute) == .none);
}

test "a recovery copy reopened stays on Save As and in the recovery folder until saved elsewhere" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    h.session.request(&ctx, .{ .new_project = .medal });
    try h.edit();
    h.settings.autosave_minutes = 1;
    _ = h.session.tickAutosave(&ctx, 0);
    const written = h.session.tickAutosave(&ctx, std.time.ms_per_min);
    const copy = written.recovery;
    // The session ends without a save (a crash): the next one finds the copy.
    h.session.recovery_active = null;
    h.session.request(&ctx, .close);
    h.session.answerUnsaved(&ctx, .dont_save);
    h.releaseLock();

    var sidecar_buffer: [128]u8 = undefined;
    const offer = offerFromEntry(std.fs.path.dirname(copy.slice()).?, std.fs.path.basename(copy.slice()), formatSidecar(&sidecar_buffer, "", 1).?).?;
    h.session.openRecovery(&ctx, &offer);
    try testing.expect(h.session.life.is_open);
    // The offer names the kind by the copy's extension; the fake's own files
    // carry no kind (it reopens them as weapons), the engine's do.
    try testing.expectEqual(Kind.medal, offer.kind);
    try testing.expectEqualStrings(copy.slice(), h.session.recovery_active.?.slice());
    try testing.expectEqual(logic.SaveRoute.save_as, h.session.route(&ctx));
    try testing.expectEqual(@as(usize, 0), h.settings.recentCount()); // a recovery copy is not a recent file
    h.session.saveAs();
    h.session.dialogAnswered(&ctx, "/work/medal.mdc");
    try testing.expect(h.session.recovery_active == null);
    try testing.expectEqual(logic.SaveRoute.save, h.session.route(&ctx));
}

test "switching sub-editors remembers the last one in the settings" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    h.session.request(&ctx, .{ .switch_editor = .chapter });
    try testing.expectEqual(@as(i32, @intFromEnum(Kind.chapter)), h.settings.last_editor.?);
    try testing.expect(h.session.settings_changed);
    try testing.expectEqual(Kind.chapter, logic.restoreEditor(h.settings.last_editor));
}

test "lockUserName follows the engine's order" {
    try testing.expectEqualStrings("seam", lockUserName("seam", "u", "w"));
    try testing.expectEqualStrings("u", lockUserName(null, "u", "w"));
    try testing.expectEqualStrings("w", lockUserName("", null, "w"));
    try testing.expectEqualStrings("unknown", lockUserName(null, null, null));
}

test "Import asks first when dirty, builds an unsaved untitled project, and a refused kind keeps the open one" {
    var h: Harness = undefined;
    h.init();
    defer h.deinit();
    var ctx = h.ctx();
    try h.fake.addGameFolder("/game/Data/Units/Humans/German/Gunner", "Gunner");
    h.session.request(&ctx, .{ .new_project = .weapon });
    try h.edit();

    const gunner = PathText.fromSlice("/game/Data/Units/Humans/German/Gunner").?;
    h.session.request(&ctx, .{ .import_from_game = .{ .kind = .animation_infantry, .folder = gunner } });
    try testing.expect(h.session.prompt.isAsking());
    h.session.answerUnsaved(&ctx, .dont_save);
    try testing.expect(h.session.life.is_open);
    try testing.expectEqual(Kind.animation_infantry, h.session.life.doc.kind);
    try testing.expect(h.session.life.doc.pathSlice() == null);
    try testing.expect(h.session.life.dirty());
    try testing.expect(std.mem.indexOf(u8, h.session.message(), "imported") != null);

    // Another import over the unsaved one asks again; Don't save, then a
    // kind that has no import yet is refused with the bridge's reason and
    // the imported project stays.
    h.session.request(&ctx, .{ .import_from_game = .{ .kind = .mesh_unit, .folder = gunner } });
    try testing.expect(h.session.prompt.isAsking());
    h.session.answerUnsaved(&ctx, .dont_save);
    try testing.expectEqual(Kind.animation_infantry, h.session.life.doc.kind);
    try testing.expect(std.mem.indexOf(u8, h.session.message(), "not ported yet") != null);
}
