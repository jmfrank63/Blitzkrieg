//! Autosave's schedule and target (D-20..D-22): when a periodic write is
//! due, and whether it goes into the map file itself or a recovery copy for
//! a map that has never been saved. Std-only, like the rest of the core: the
//! write itself (`Editor.save`'s safe save, or `RealBridge.saveCopy` for a
//! recovery copy) is the app's job (main.zig), which also owns the wall-clock
//! reading `now_ms` comes from.
const std = @import("std");

/// D-21's own default.
pub const default_interval_minutes: u32 = 2;

/// D-20, D-21: on by default, every 2 minutes, writing only when there are
/// unsaved changes. `interval_ms` is a setting (`Settings.autosave_minutes *
/// std.time.ms_per_min`); `enabled` mirrors `Settings.autosave`.
pub const Autosave = struct {
    enabled: bool = true,
    interval_ms: u64 = @as(u64, default_interval_minutes) * std.time.ms_per_min,
    /// The tick the document most recently became dirty, or null while it is
    /// clean (a fresh map, just after opening, or just after any save -
    /// autosave's own writes included). `due`'s interval starts here.
    dirty_since_ms: ?u64 = null,
    /// The tick of the last successful (or last attempted - see `wrote`'s own
    /// doc comment) autosave write this session, or null before the first
    /// one.
    last_write_ms: ?u64 = null,

    /// Every frame, before `due`: tracks when the document became dirty, and
    /// forgets it once clean again (any save landing, autosave's own
    /// included, or an undo back past the saved mark going the other way
    /// would make it dirty again through this same call) - a later edit
    /// starts `due`'s interval fresh, exactly like a first edit would.
    pub fn note(self: *Autosave, now_ms: u64, dirty: bool) void {
        if (dirty) {
            if (self.dirty_since_ms == null) self.dirty_since_ms = now_ms;
        } else {
            self.dirty_since_ms = null;
        }
    }

    /// Whether a write is due now: enabled, dirty, and `interval_ms` since
    /// the LATER of becoming dirty and the last write (D-21: never writes a
    /// clean map, and a write just made does not fire again a moment later
    /// merely because the map became dirty a while before that write).
    pub fn due(self: *const Autosave, now_ms: u64, dirty: bool) bool {
        if (!self.enabled or !dirty) return false;
        const since = self.dirty_since_ms orelse return false;
        const baseline = if (self.last_write_ms) |written| @max(since, written) else since;
        return now_ms >= baseline + self.interval_ms;
    }

    /// The caller made a write, or tried to and is choosing to wait a full
    /// interval before trying again either way (main.zig's own retry rule on
    /// a failure) - `due` will not fire again for `interval_ms` from `now_ms`.
    pub fn wrote(self: *Autosave, now_ms: u64) void {
        self.last_write_ms = now_ms;
    }
};

pub const Target = enum { map_file, recovery_copy };

/// D-20, D-22: a map with a real, writable path (Save/Save As already
/// happened) autosaves into itself; one that has never been saved
/// (`needs_save_as` - a brand new map, or a shipped one not yet Saved As)
/// autosaves to a recovery copy instead.
pub fn target(needs_save_as: bool) Target {
    return if (needs_save_as) .recovery_copy else .map_file;
}

/// The recovery copy's file name (D-22): `doc_path`'s stem (its last path
/// component with any extension removed - the directory is never part of a
/// recovery name, so a crafted "..\maps\a.bzm" cannot walk the recovery copy
/// anywhere outside the recovery folder), every character outside
/// `[A-Za-z0-9_.-]` replaced with `_`, `"untitled"` when the stem is empty (a
/// brand new map has no path at all yet), plus `extension`. Null when the
/// result would not fit `buffer`. `extension` is caller-chosen so this
/// primitive is kit-generic (MapEditor threads ".bzm", other editors their
/// own project extension); it is written verbatim and is not sanitised.
pub fn recoveryName(buffer: []u8, doc_path: []const u8, extension: []const u8) ?[]const u8 {
    const cut = std.mem.lastIndexOfAny(u8, doc_path, "/\\");
    const file_name = if (cut) |c| doc_path[c + 1 ..] else doc_path;
    const dot = std.mem.lastIndexOfScalar(u8, file_name, '.');
    const stem = if (dot) |d| file_name[0..d] else file_name;
    const safe_stem = if (stem.len == 0) "untitled" else stem;
    if (safe_stem.len + extension.len > buffer.len) return null;
    for (safe_stem, buffer[0..safe_stem.len]) |char, *out| out.* = if (isSafeChar(char)) char else '_';
    @memcpy(buffer[safe_stem.len..][0..extension.len], extension);
    return buffer[0 .. safe_stem.len + extension.len];
}

fn isSafeChar(c: u8) bool {
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or c == '_' or c == '.' or c == '-';
}

test "due: waits interval_ms from becoming dirty, not from process start" {
    var autosave: Autosave = .{ .interval_ms = 1000 };
    autosave.note(0, true);
    try std.testing.expect(!autosave.due(500, true));
    try std.testing.expect(autosave.due(1000, true));
}

test "due: disabled never fires, and neither does a clean map" {
    var disabled: Autosave = .{ .interval_ms = 1000, .enabled = false };
    disabled.note(0, true);
    try std.testing.expect(!disabled.due(2000, true));

    var clean: Autosave = .{ .interval_ms = 1000 };
    clean.note(0, false);
    try std.testing.expect(!clean.due(2000, false));
}

test "due: measures from the later of becoming dirty and the last write" {
    var autosave: Autosave = .{ .interval_ms = 1000 };
    autosave.note(0, true);
    try std.testing.expect(autosave.due(1000, true));
    autosave.wrote(1000);
    try std.testing.expect(!autosave.due(1500, true));
    try std.testing.expect(autosave.due(2000, true));
}

test "due: a save clearing dirty, then a fresh edit, starts the interval over" {
    var autosave: Autosave = .{ .interval_ms = 1000 };
    autosave.note(0, true);
    autosave.note(500, false); // a manual save landed before autosave fired
    try std.testing.expect(!autosave.due(600, false));
    autosave.note(600, true); // dirty again
    try std.testing.expect(!autosave.due(1000, true));
    try std.testing.expect(autosave.due(1600, true));
}

test "target: no path or a shipped map autosaves to a recovery copy, otherwise the file itself" {
    try std.testing.expectEqual(Target.recovery_copy, target(true));
    try std.testing.expectEqual(Target.map_file, target(false));
}

test "recoveryName: unsafe characters become '_', an empty stem is 'untitled', no directory survives" {
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("a_b_c.bzm", recoveryName(&buffer, "..\\maps\\a:b*c.bzm", ".bzm").?);
    try std.testing.expectEqualStrings("untitled.bzm", recoveryName(&buffer, "", ".bzm").?);
    try std.testing.expectEqualStrings("untitled.bzm", recoveryName(&buffer, "maps\\.bzm", ".bzm").?);
    try std.testing.expectEqualStrings("coldwinter.bzm", recoveryName(&buffer, "Data\\Maps\\Multiplayer\\coldwinter.bzm", ".bzm").?);
    try std.testing.expectEqualStrings("passwd.bzm", recoveryName(&buffer, "../../etc/passwd", ".bzm").?);
    var tiny: [4]u8 = undefined;
    try std.testing.expect(recoveryName(&tiny, "a.bzm", ".bzm") == null);
}

test "recoveryName: a caller-chosen extension is honoured, so other editors can thread their own project extension" {
    var buffer: [64]u8 = undefined;
    try std.testing.expectEqualStrings("mine.wpn", recoveryName(&buffer, "weapons\\mine.wpn", ".wpn").?);
    try std.testing.expectEqualStrings("untitled.obt", recoveryName(&buffer, "", ".obt").?);
}
