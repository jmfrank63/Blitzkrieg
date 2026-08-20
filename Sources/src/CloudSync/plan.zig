//! Sync planning primitives: the short link that stands in for the profile
//! directory as bisync's Path1, the session-name budget that refuses a run
//! bisync would abort on filename length, the filter set and the
//! machine-local state paths that stay out of it, the remote layout, and the
//! `.bkprofile` sentinel.
//!
//! The sentinel is one file backing two of bisync's guards, which is why it
//! is written once and never rewritten. Unchanged, it keeps `foundSame` true,
//! so the `all files were changed` abort cannot fire on a small profile where
//! overwriting the only autosave changes 100% of the side. Counted, it adds
//! one to `oldCount`, so deleting the only save is 1-of-2 — exactly the 50%
//! `maxDelete` ratio, which compares with `<=` and passes. Rewriting it on a
//! later run would forfeit the first guard; seeding it on both sides would
//! give the copies different modification times and abort the resync.
//!
//! Why a link at all. bisync mangles both canonical paths into one state
//! filename — `<path1>..<path2>.path1.lst` — and dies once that name passes
//! the filesystem's 255-byte limit. The profile directory is wherever the
//! player installed the game plus `profiles/<name>/`, which is routinely past
//! 190 bytes on its own, so handing it to bisync directly makes the session
//! name a property of the install path. A short link the game manages makes it
//! a constant instead.
//!
//! The property this rests on is that **rclone does not dereference the link**
//! when it builds the session name. It canonicalises to the `\\?\` long-path
//! form for its own fs cache, but `bilib.FsPath` strips that prefix again and
//! mangles the path as given. Measured on both platforms: a 199-byte macOS
//! directory reached through an 8-byte symlink produced a 21-byte session
//! name, and a Windows junction at `C:\bk\p0` pointing at a deep target
//! produced `C__bk_p0..C__bk_remote` with the target's marker absent. See
//! `docs/superpowers/evidence/cloud-sync/junction-session-name.md`.
//!
//! Why a slot number and not the profile name. `p0` is two bytes; a profile
//! called `Panzerkommandant` is sixteen, and putting it in the link path puts
//! the length straight back into the session name it was removed from.
//!
//! Why a junction on Windows and not a symlink. A directory symlink needs
//! `SeCreateSymbolicLinkPrivilege` — administrator rights, or Developer Mode.
//! A junction needs neither, confirmed unelevated on a real Windows machine.
//! The game cannot ask a player to run it as administrator to sync a save.
//!
//! Why the target is canonicalised first. The game's profile paths are
//! relative by construction — `NProfile::Segment` returns `profiles\<name>\` —
//! and all three ways of making a junction reject or mishandle a relative
//! target: `New-Item -ItemType Junction` refuses one outright, the
//! `IO_REPARSE_TAG_MOUNT_POINT` buffer wants the `\??\C:\...` NT form, and
//! `mklink /J` silently resolves it against cmd's own current directory, which
//! is the worst of the three because it succeeds.
//!
//! **Everything here blocks the calling thread**: it stats, creates
//! directories, and on the Windows fallback path spawns a process. Like
//! `rc.zig` and `daemon.zig`, it belongs on the worker thread.
//!
//! The `*In` variants exist so that a caller holding an `Io` does not build a
//! second one, and so that tests can inject a link root instead of writing
//! into the player's real cache directory — the same reason `daemon.zig` has
//! `Search`.

const std = @import("std");
const builtin = @import("builtin");

const Allocator = std.mem.Allocator;
const Io = std.Io;
const path = Io.Dir.path;

/// The link root's own directory name. Short on Windows because the whole
/// path is spelled into the session name there, three characters of which the
/// drive letter has already taken (`C:\` becomes `C__`).
pub const root_dir_name = if (builtin.os.tag == .windows) "bk" else "blitzkrieg";

/// Slots are a small integer index, and the index is bounded so that a bug
/// which never reuses a slot cannot fill the cache directory. Sixty-four is
/// far past any plausible number of profiles on one machine.
pub const max_slots: u8 = 64;

/// How the link was made. Recorded rather than discarded: a machine that had
/// to fall back to `mklink` is a machine whose support report should say so,
/// and a Windows run that comes back `.symlink` is a bug.
pub const LinkMethod = enum {
    /// POSIX `symlink(2)`, through `Io.Dir.symLink`.
    symlink,
    /// Windows junction written directly with `DeviceIoControl` and
    /// `FSCTL_SET_REPARSE_POINT`.
    junction_reparse,
    /// Windows junction made by `cmd /c mklink /J`, after the reparse ioctl
    /// was refused.
    junction_mklink,
};

/// Where the link root comes from. A `null` field means "ask the operating
/// system"; an empty string means "the operating system has nothing to say",
/// which is how a test excludes the machine it runs on.
pub const Roots = struct {
    /// The whole link root, bypassing the platform layout. Tests set this to
    /// a temp directory; the game never does.
    link_root: ?[]const u8 = null,
    /// `%LOCALAPPDATA%`. Windows only.
    local_app_data: ?[]const u8 = null,
    /// `$HOME`. Everywhere else.
    home: ?[]const u8 = null,
};

pub const RootError = error{
    /// The platform could not name a home or local-app-data directory. There
    /// is deliberately no fallback: a relative link root would land wherever
    /// the game happened to be launched from.
    RootUnknown,
} || Allocator.Error;

pub const LinkRootError = RootError || error{
    /// The root is named but could not be created.
    RootUnwritable,
};

pub const ShortLinkError = LinkRootError || error{
    /// The profile directory is not there.
    TargetNotFound,
    /// Something is there, but it is not a directory. Linking it would give
    /// bisync a Path1 it cannot list.
    TargetNotDirectory,
    /// Every slot is taken by a link to a different, still-existing profile.
    NoFreeSlot,
    /// A slot number past `max_slots`.
    SlotOutOfRange,
    /// The operating system refused to create the link. On Windows this means
    /// both the reparse ioctl and `mklink /J` failed.
    LinkFailed,
};

/// A short Path1 and the profile it stands for.
pub const ShortLink = struct {
    /// `<linkRoot>/p<slot>`. This is the path handed to bisync, and the exact
    /// bytes it mangles into the session name. Owned.
    path: []u8,
    /// The canonical absolute directory the link points at. Owned.
    target: []u8,
    slot: u8,
    /// How the link was made, or `null` when an existing link already pointed
    /// at `target` and nothing was created.
    method: ?LinkMethod,

    pub fn deinit(self: *ShortLink, gpa: Allocator) void {
        gpa.free(self.path);
        gpa.free(self.target);
        self.* = undefined;
    }
};

/// The link root's path, computed and not created.
///
/// `%LOCALAPPDATA%\bk` on Windows, `~/Library/Caches/blitzkrieg` on macOS,
/// `~/.cache/blitzkrieg` elsewhere. No trailing separator: callers join onto
/// it.
pub fn linkRootPath(gpa: Allocator, roots: Roots) RootError![]u8 {
    if (roots.link_root) |explicit| {
        if (explicit.len == 0) return error.RootUnknown;
        return gpa.dupe(u8, explicit);
    }

    if (builtin.os.tag == .windows) {
        const base = try baseDir(gpa, roots.local_app_data, "LOCALAPPDATA");
        defer gpa.free(base);
        return path.join(gpa, &.{ base, root_dir_name });
    }

    const home = try baseDir(gpa, roots.home, "HOME");
    defer gpa.free(home);
    return switch (builtin.os.tag) {
        .macos => path.join(gpa, &.{ home, "Library", "Caches", root_dir_name }),
        else => path.join(gpa, &.{ home, ".cache", root_dir_name }),
    };
}

/// The link root, created when absent. Owned by the caller.
pub fn linkRoot(gpa: Allocator) LinkRootError![]u8 {
    var threaded: Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    return linkRootIn(gpa, threaded.io(), .{});
}

pub fn linkRootIn(gpa: Allocator, io: Io, roots: Roots) LinkRootError![]u8 {
    const root = try linkRootPath(gpa, roots);
    errdefer gpa.free(root);

    Io.Dir.cwd().createDirPath(io, root) catch return error.RootUnwritable;
    return root;
}

/// The short Path1 for `profile_dir`, creating the link if there is not one
/// already. `profile_dir` may be relative — it is resolved against the current
/// directory before anything is linked.
///
/// A slot already pointing at this profile is reused, so the common case of a
/// second launch creates nothing.
pub fn ensureShortLink(gpa: Allocator, profile_dir: []const u8) ShortLinkError!ShortLink {
    var threaded: Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    return ensureShortLinkIn(gpa, threaded.io(), .{}, profile_dir);
}

pub fn ensureShortLinkIn(
    gpa: Allocator,
    io: Io,
    roots: Roots,
    profile_dir: []const u8,
) ShortLinkError!ShortLink {
    const target = try canonicalDir(gpa, io, profile_dir);
    errdefer gpa.free(target);

    const root = try linkRootIn(gpa, io, roots);
    defer gpa.free(root);

    // Two things are wanted from one scan: the slot already pointing here, if
    // there is one, and otherwise the lowest slot that is free. Reuse wins,
    // which is why the scan cannot stop at the first vacancy.
    var vacant: ?u8 = null;
    var slot: u8 = 0;
    while (slot < max_slots) : (slot += 1) {
        const candidate = try slotPath(gpa, root, slot);
        switch (inspect(io, candidate, target)) {
            .matches => return .{
                .path = candidate,
                .target = target,
                .slot = slot,
                .method = null,
            },
            .vacant => {
                if (vacant == null) vacant = slot;
                gpa.free(candidate);
            },
            .occupied => gpa.free(candidate),
        }
    }

    const chosen = vacant orelse return error.NoFreeSlot;
    return linkAt(gpa, io, root, chosen, target);
}

/// Point an existing slot at a different profile, for profile switching.
///
/// Removal and recreation, never an in-place retarget: POSIX has no such
/// operation for a symlink, and rewriting a junction's reparse point in place
/// leaves a window in which the link is a plain directory that a concurrent
/// write could fill.
pub fn repointShortLink(gpa: Allocator, slot: u8, new_target: []const u8) ShortLinkError!ShortLink {
    var threaded: Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    return repointShortLinkIn(gpa, threaded.io(), .{}, slot, new_target);
}

pub fn repointShortLinkIn(
    gpa: Allocator,
    io: Io,
    roots: Roots,
    slot: u8,
    new_target: []const u8,
) ShortLinkError!ShortLink {
    if (slot >= max_slots) return error.SlotOutOfRange;

    const target = try canonicalDir(gpa, io, new_target);
    errdefer gpa.free(target);

    const root = try linkRootIn(gpa, io, roots);
    defer gpa.free(root);

    return linkAt(gpa, io, root, slot, target);
}

/// `<root>/p<slot>`.
pub fn slotPath(gpa: Allocator, root: []const u8, slot: u8) Allocator.Error![]u8 {
    return std.fmt.allocPrint(gpa, "{s}{c}p{d}", .{ root, path.sep, slot });
}

// -- Session-name budget -----------------------------------------------------
//
// bisync names every state file after the session: `<canon1>..<canon2>` plus
// a suffix — `.path1.lst-new`, `.lck`, and friends. That name is a filename
// in rclone's cache directory, and filesystems refuse names past 255 bytes,
// so a pair whose mangled paths are long enough kills every run with an error
// about a listing file rather than anything a player could act on.
//
// The arithmetic below reproduces rclone's — `CanonicalPath`, `FsPath` and
// `SessionName` from `cmd/bisync/bilib`, measured against v1.75.0 — so the
// refusal happens here, with a number, before a daemon is ever asked. The
// short link holds Path1 constant, but the profile name is part of Path2 and
// changes under a rename, which is why the check runs before every sync and
// not once at pairing.

/// Which side of the pair a path is, because rclone derives the two
/// differently.
pub const EndpointKind = enum {
    /// A local directory. Contributes its entire absolute path — the reason
    /// Path1 must be the short link and never the profile directory itself.
    local,
    /// A named remote, passed as `name:root`. Contributes only that string,
    /// which is why Path2's length is bounded by the profile name and not by
    /// anything about the player's machine.
    remote,
};

/// One side of a bisync run: the path as it will be handed to rclone.
pub const Endpoint = struct {
    path: []const u8,
    kind: EndpointKind,
};

/// Mirror of `bilib.FsPath`: the argument string rclone derives for one side.
/// A local path gains its platform separator when not already suffixed — on
/// Windows after every `/` has become `\` — and a remote gains a `/`.
pub fn fsPath(gpa: Allocator, raw: []const u8, kind: EndpointKind) Allocator.Error![]u8 {
    const sep: u8 = if (kind == .local and builtin.os.tag == .windows) '\\' else '/';

    const owned = try gpa.dupe(u8, raw);
    if (kind == .local and builtin.os.tag == .windows) {
        for (owned) |*byte| {
            if (byte.* == '/') byte.* = '\\';
        }
    }

    if (owned.len != 0 and owned[owned.len - 1] == sep) return owned;
    defer gpa.free(owned);
    return std.fmt.allocPrint(gpa, "{s}{c}", .{ owned, sep });
}

/// Mirror of `bilib.CanonicalPath`: leading and trailing separators trimmed,
/// then every byte of the class `[\s\\/:?*]` replaced with `_`. `\s` is Go's:
/// space, tab, newline, form feed and carriage return.
pub fn canonicalPath(gpa: Allocator, remote: []const u8) Allocator.Error![]u8 {
    const trimmed = std.mem.trim(u8, remote, "\\/");
    const out = try gpa.dupe(u8, trimmed);
    for (out) |*byte| switch (byte.*) {
        ' ', '\t', '\n', 0x0c, '\r', '\\', '/', ':', '?', '*' => byte.* = '_',
        else => {},
    };
    return out;
}

/// Mirror of `bilib.SessionName`: the base name of every bisync state file
/// for this pair, and the exact bytes the budget is spent on.
pub fn sessionName(gpa: Allocator, p1: Endpoint, p2: Endpoint) Allocator.Error![]u8 {
    const c1 = try canonicalEndpoint(gpa, p1);
    defer gpa.free(c1);
    const c2 = try canonicalEndpoint(gpa, p2);
    defer gpa.free(c2);
    return std.mem.concat(gpa, u8, &.{ c1, "..", c2 });
}

fn canonicalEndpoint(gpa: Allocator, endpoint: Endpoint) Allocator.Error![]u8 {
    const fs_path = try fsPath(gpa, endpoint.path, endpoint.kind);
    defer gpa.free(fs_path);
    return canonicalPath(gpa, fs_path);
}

/// The longest suffix bisync appends to a session name: `.path1.lst-new`,
/// with `-old` and `-err` the same length and `.lck` shorter.
pub const session_suffix_max: usize = ".path1.lst-new".len;

/// What remains of a 255-byte filename once the worst suffix has taken its
/// share.
pub const session_budget: usize = 255 - session_suffix_max;

pub const SessionBudgetError = error{
    /// The projected session name is past the budget. Its length is in the
    /// caller's out-parameter, so the player is told a number against the
    /// budget rather than "sync failed".
    SessionNameTooLong,
} || Allocator.Error;

/// Refuse a pair whose session name bisync could not create a state file
/// for. Called before every run, not only at setup: a profile rename changes
/// Path2.
///
/// `projected` is written on success and failure both — a Zig error carries
/// no payload, and the number is the entire point of checking here instead of
/// letting the run die on a filename.
pub fn checkSessionBudget(
    gpa: Allocator,
    p1: Endpoint,
    p2: Endpoint,
    projected: *usize,
) SessionBudgetError!void {
    const name = try sessionName(gpa, p1, p2);
    defer gpa.free(name);
    projected.* = name.len;
    if (name.len > session_budget) return error.SessionNameTooLong;
}

// -- Machine-local state paths -----------------------------------------------
//
// Pairing state, bisync's workdir, the daemon record and the filters file all
// describe one machine: which rclone was found, what this host has paired,
// where its listings live. Inside Path1 they would travel to every other
// machine and corrupt the same state they record, so they live under
// `<gamedir>/cloudsync/` — the directory `daemon.zig` already owns for
// `rclone.conf` and `daemon.json`. Nothing under `profiles/<name>/` may hold
// machine-local state except the trash and the sentinel.

/// The state directory's name under the game root. Kept textually identical
/// to `daemon.state_dir_name`; not imported, because the planning module
/// should not pull the daemon's process machinery into its compile.
pub const state_dir_name = "cloudsync";

/// `<gamedir>/cloudsync` — the root of everything machine-local. The game
/// directory is a parameter, not discovered here, for the same reason
/// `daemon.zig` takes `Options.game_dir`: the ABI layer owns discovery, and
/// tests inject a fixture.
pub fn stateRoot(gpa: Allocator, game_dir: []const u8) Allocator.Error![]u8 {
    return path.join(gpa, &.{ game_dir, state_dir_name });
}

/// `<stateRoot>/state/<profile>.json` — what this machine knows about one
/// profile's pairing: whether a resync has succeeded, when, against what.
pub fn pairingStatePath(gpa: Allocator, game_dir: []const u8, profile: []const u8) Allocator.Error![]u8 {
    return std.fmt.allocPrint(
        gpa,
        "{s}{c}{s}{c}state{c}{s}.json",
        .{ game_dir, path.sep, state_dir_name, path.sep, path.sep, profile },
    );
}

/// `<stateRoot>/workdir` — bisync's listing files, one pair per session.
/// Machine-local by nature: another machine's listings describe another
/// machine's last sync.
pub fn workdirPath(gpa: Allocator, game_dir: []const u8) Allocator.Error![]u8 {
    return path.join(gpa, &.{ game_dir, state_dir_name, "workdir" });
}

/// `<stateRoot>/filters.txt` — where `writeFiltersFile` puts the rule set
/// handed to bisync as `filtersFile`.
pub fn filtersFilePath(gpa: Allocator, game_dir: []const u8) Allocator.Error![]u8 {
    return path.join(gpa, &.{ game_dir, state_dir_name, "filters.txt" });
}

// -- Filters -----------------------------------------------------------------

/// The rule set handed to bisync as `filtersFile`, byte-exact and constant:
/// bisync stores an MD5 of this file and demands a resync when it changes, so
/// the content must never vary between runs or machines.
///
/// `config.cfg` is backed up rather than synced — it carries `GFX.*`, and
/// display choices are per machine by hard-won design. `screenshots/**` is
/// large and low-value. `*.tmp-rename` is `NProfile::Rename`'s intermediate
/// state. `cloud.credentials` never leaves the machine. The trash, restore
/// stages and config backups are excluded belt-and-braces: the remote copies
/// are siblings of `profiles/` and never under Path2 at all, and the local
/// ones are `.cloudsync-*` names inside the profile. The final rule covers
/// any future machine-local file dropped into the profile directory by
/// mistake.
pub const filters_file_content =
    \\- config.cfg
    \\- screenshots/**
    \\- *.tmp-rename
    \\- cloud.credentials
    \\- .cloudsync-trash/**
    \\- .cloudsync-restore/**
    \\- config-backups/**
    \\- .cloudsync-*
    \\
;

/// Write the filter set to `file_path`, replacing whatever is there. The
/// content is comptime-constant, so rewriting is idempotent and cannot
/// trip bisync's filters-changed MD5 check.
pub fn writeFiltersFile(io: Io, file_path: []const u8) !void {
    try Io.Dir.cwd().writeFile(io, .{ .sub_path = file_path, .data = filters_file_content });
}

// -- Remote layout -----------------------------------------------------------
//
// Path2 is `<remote>:profiles/<name>`, and nothing that is not a profile
// lives under it: anything beneath the synced prefix is by definition synced
// back down to every machine, which is how a config-backup history would have
// arrived on every machine at once. The trash and the backups are siblings of
// `profiles/`, so the filter entries naming them are a second fence around a
// path that is already outside the pen.

/// `<remote>:profiles/<profile>` — Path2, the synced prefix.
pub fn remoteProfileRoot(gpa: Allocator, remote: []const u8, profile: []const u8) Allocator.Error![]u8 {
    return std.fmt.allocPrint(gpa, "{s}:profiles/{s}", .{ remote, profile });
}

/// `<remote>:trash/<profile>/<run_id>` — `backupDir2`, fresh per run because
/// rclone overwrites a backup at an existing path and save filenames recur
/// every session.
pub fn remoteTrashRoot(
    gpa: Allocator,
    remote: []const u8,
    profile: []const u8,
    run_id: []const u8,
) Allocator.Error![]u8 {
    return std.fmt.allocPrint(gpa, "{s}:trash/{s}/{s}", .{ remote, profile, run_id });
}

/// `<remote>:config-backups/<profile>/<host>` — one-way config snapshots,
/// keyed by host so machines never overwrite each other's history.
pub fn remoteConfigBackupRoot(
    gpa: Allocator,
    remote: []const u8,
    profile: []const u8,
    host: []const u8,
) Allocator.Error![]u8 {
    return std.fmt.allocPrint(gpa, "{s}:config-backups/{s}/{s}", .{ remote, profile, host });
}

// -- Delete ratio ------------------------------------------------------------

/// bisync's `maxDelete`, sent explicitly on every run: over rc the option
/// defaults to zero — the CLI's 50 is applied in `cmd.go`, which rc never
/// runs — and zero aborts on *any* delete rather than allowing none.
pub const max_delete_percent: usize = 50;

/// The abort predicate, mirrored: a run passes while
/// `deleted / oldCount <= maxDelete/100`. Scaled to integers so the boundary
/// is exact — one delete of two files is precisely 50% and passes, which is
/// the arithmetic the sentinel exists to arrange.
pub fn deleteWithinRatio(old_count: usize, deletes: usize) bool {
    return deletes * 100 <= old_count * max_delete_percent;
}

// -- Sentinel ----------------------------------------------------------------

/// The one file guaranteed present and unchanged in every paired profile.
pub const sentinel_name = ".bkprofile";

/// What `ensureSentinel` did, recorded rather than discarded — the pairing
/// step's report should say whether this machine seeded the profile or is
/// waiting for the resync to deliver the sentinel.
pub const SentinelAction = enum {
    /// A sentinel already exists locally. It was not touched: its unchanged
    /// modification time is the `foundSame` guarantee.
    already_present,
    /// No sentinel anywhere; one was written here, and the resync will carry
    /// it up.
    written,
    /// The remote already carries one, so nothing was written — two
    /// independently created copies differ in modification time and abort
    /// the resync with `Modtime not equal in listing`. The resync delivers
    /// the remote's copy instead.
    deferred_to_remote,
};

pub const SentinelError = Allocator.Error || error{
    /// The sentinel was absent and could not be written — the profile
    /// directory is missing or read-only.
    SentinelUnwritable,
};

/// Guarantee the pairing can rely on the sentinel without ever rewriting one.
/// `remote_has_sentinel` is the caller's answer from asking the remote —
/// P02-M01 owns that question; this function owns never seeding both sides.
pub fn ensureSentinel(
    gpa: Allocator,
    io: Io,
    profile_dir: []const u8,
    profile_id: []const u8,
    remote_has_sentinel: bool,
) SentinelError!SentinelAction {
    const sentinel_path = try path.join(gpa, &.{ profile_dir, sentinel_name });
    defer gpa.free(sentinel_path);

    if (Io.Dir.cwd().statFile(io, sentinel_path, .{})) |_| {
        return .already_present;
    } else |_| {}

    if (remote_has_sentinel) return .deferred_to_remote;

    const content = try std.fmt.allocPrint(gpa, "{s}\n", .{profile_id});
    defer gpa.free(content);
    Io.Dir.cwd().writeFile(io, .{ .sub_path = sentinel_path, .data = content }) catch
        return error.SentinelUnwritable;
    return .written;
}

/// Take `target` on the ownership of the returned `ShortLink`, replacing
/// whatever occupies the slot.
fn linkAt(gpa: Allocator, io: Io, root: []const u8, slot: u8, target: []u8) ShortLinkError!ShortLink {
    const link_path = try slotPath(gpa, root, slot);
    errdefer gpa.free(link_path);

    // Only ever the link itself. `removeLink` is written so that it cannot
    // reach through into the profile, because the profile is the player's
    // saves and this is the one place that deletes anything.
    removeLink(io, link_path) catch {};

    const method = createLink(gpa, io, link_path, target) catch return error.LinkFailed;
    return .{ .path = link_path, .target = target, .slot = slot, .method = method };
}

const SlotState = enum {
    /// A link that resolves to the profile we were asked about.
    matches,
    /// Nothing there, or a link to something that no longer exists. Both are
    /// usable: a slot held forever by a link to a deleted profile would run
    /// the game out of slots.
    vacant,
    /// A link to a different profile that still exists.
    occupied,
};

fn inspect(io: Io, link_path: []const u8, target: []const u8) SlotState {
    var buffer: [Io.Dir.max_path_bytes]u8 = undefined;
    // Resolution rather than reading the link back: it answers on both
    // platforms with one call, and a link that cannot be resolved is exactly
    // the dangling case that should be reclaimed.
    const len = Io.Dir.cwd().realPathFile(io, link_path, &buffer) catch return .vacant;
    return if (std.mem.eql(u8, buffer[0..len], target)) .matches else .occupied;
}

/// The canonical absolute path of a directory. Every caller goes through this
/// before a link is created — see the note on relative targets at the top.
fn canonicalDir(gpa: Allocator, io: Io, dir_path: []const u8) ShortLinkError![]u8 {
    const stat = Io.Dir.cwd().statFile(io, dir_path, .{}) catch return error.TargetNotFound;
    if (stat.kind != .directory) return error.TargetNotDirectory;

    var buffer: [Io.Dir.max_path_bytes]u8 = undefined;
    const len = Io.Dir.cwd().realPathFile(io, dir_path, &buffer) catch return error.TargetNotFound;
    return gpa.dupe(u8, buffer[0..len]);
}

fn createLink(gpa: Allocator, io: Io, link_path: []const u8, target: []const u8) !LinkMethod {
    if (builtin.os.tag == .windows) return createJunction(gpa, io, link_path, target);
    try Io.Dir.cwd().symLink(io, target, link_path, .{ .is_directory = true });
    return .symlink;
}

/// Remove the link, never what is behind it.
fn removeLink(io: Io, link_path: []const u8) !void {
    if (builtin.os.tag == .windows) {
        // A junction is a directory entry, so `RemoveDirectoryW` is the call
        // that unlinks it — and it unlinks the reparse point rather than
        // descending, which is the whole reason it is used here in preference
        // to anything that opens the path first.
        var buffer: [Io.Dir.max_path_bytes]u16 = undefined;
        const wide = try widePath(&buffer, link_path);
        if (!RemoveDirectoryW(wide).toBool()) return error.RemoveFailed;
        return;
    }

    // `unlink` on a symlink removes the link. A real directory sitting in the
    // slot — left by a crash between `CreateDirectory` and the ioctl on
    // Windows, or by a player — comes back as `IsDir` and is removed as one,
    // and it is empty by construction.
    Io.Dir.cwd().deleteFile(io, link_path) catch |err| switch (err) {
        error.IsDir => try Io.Dir.cwd().deleteDir(io, link_path),
        else => |e| return e,
    };
}

fn baseDir(gpa: Allocator, injected: ?[]const u8, comptime name: [:0]const u8) RootError![]u8 {
    if (injected) |value| {
        if (value.len == 0) return error.RootUnknown;
        return gpa.dupe(u8, value);
    }

    const value = environValue(gpa, name) orelse return error.RootUnknown;
    if (value.len == 0) {
        gpa.free(value);
        return error.RootUnknown;
    }
    return value;
}

/// One environment variable, copied out. Zig 0.16 hands the environment to
/// `main`, which this module does not have — it is compiled into a library the
/// game loads — so it is read from the platform instead, exactly as
/// `daemon.zig` reads `PATH`.
fn environValue(gpa: Allocator, comptime name: [:0]const u8) ?[]u8 {
    if (builtin.os.tag == .windows) {
        const environ: std.process.Environ = .{ .block = .global };
        return environ.getAlloc(gpa, name) catch null;
    }
    const raw = std.c.getenv(name.ptr) orelse return null;
    return gpa.dupe(u8, std.mem.span(raw)) catch null;
}

// -- Windows junctions -------------------------------------------------------
//
// A junction is a directory carrying an `IO_REPARSE_TAG_MOUNT_POINT` reparse
// point whose substitute name is the NT form of the target, `\??\C:\...`. The
// sequence is: create an empty directory, open it with
// `FILE_FLAG_OPEN_REPARSE_POINT` so the open does not follow anything, and
// write the reparse buffer with `FSCTL_SET_REPARSE_POINT`.
//
// `mklink /J` is the fallback, not the primary, for three reasons: it is a
// `cmd` builtin so it costs a shell, its output is localised so a failure
// cannot be classified, and it resolves a relative target against cmd's own
// current directory. The target passed to it here is already absolute, which
// removes the third — but the first two are enough to prefer the ioctl.
//
// Paths handed to `CreateFileW` and `RemoveDirectoryW` are *not* given the
// `\\?\` prefix. The prefix demands a fully normalised path and is there to
// pass MAX_PATH, and the link path is short by construction — being short is
// the entire point of this file. The prefix does appear in the reparse
// buffer's substitute name, as `\??\`, where the object manager requires it.

/// Build the junction, recording which of the two ways worked.
fn createJunction(gpa: Allocator, io: Io, link_path: []const u8, target: []const u8) !LinkMethod {
    if (setReparsePoint(gpa, link_path, target)) {
        return .junction_reparse;
    } else |_| {
        // The directory may be there without a reparse point on it. Left
        // behind, it would occupy the slot and read as an empty profile.
        removeLink(io, link_path) catch {};
        try mklinkJunction(gpa, io, link_path, target);
        return .junction_mklink;
    }
}

fn setReparsePoint(gpa: Allocator, link_path: []const u8, target: []const u8) !void {
    const w = std.os.windows;

    var link_buffer: [Io.Dir.max_path_bytes]u16 = undefined;
    const link_wide = try widePath(&link_buffer, link_path);

    if (!CreateDirectoryW(link_wide, null).toBool()) return error.JunctionFailed;
    errdefer _ = RemoveDirectoryW(link_wide);

    const handle = CreateFileW(
        link_wide,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        null,
        OPEN_EXISTING,
        // Backup semantics to open a directory at all; open-reparse-point so
        // that the handle is the link and not whatever it points at.
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
        null,
    );
    if (handle == w.INVALID_HANDLE_VALUE) return error.JunctionFailed;
    defer w.CloseHandle(handle);

    const substitute = try std.fmt.allocPrint(gpa, "\\??\\{s}", .{target});
    defer gpa.free(substitute);
    const substitute_wide = try std.unicode.wtf8ToWtf16LeAlloc(gpa, substitute);
    defer gpa.free(substitute_wide);
    const print_wide = try std.unicode.wtf8ToWtf16LeAlloc(gpa, target);
    defer gpa.free(print_wide);

    // Substitute name, its terminator, print name, its terminator. The
    // terminators are not counted in either length field but must be present.
    const names_bytes = (substitute_wide.len + 1 + print_wide.len + 1) * @sizeOf(u16);
    const total = @sizeOf(MountPointHeader) + names_bytes;
    if (total > w.MAXIMUM_REPARSE_DATA_BUFFER_SIZE) return error.JunctionFailed;

    var buffer: [w.MAXIMUM_REPARSE_DATA_BUFFER_SIZE]u8 align(@alignOf(MountPointHeader)) = undefined;
    const header: *MountPointHeader = @ptrCast(&buffer);
    header.* = .{
        .ReparseTag = w.IO_REPARSE_TAG.MOUNT_POINT,
        // Everything after the first eight bytes: the four offsets and
        // lengths, plus the names.
        .ReparseDataLength = @intCast(@sizeOf(MountPointHeader) - 8 + names_bytes),
        .Reserved = 0,
        .SubstituteNameOffset = 0,
        .SubstituteNameLength = @intCast(substitute_wide.len * @sizeOf(u16)),
        .PrintNameOffset = @intCast((substitute_wide.len + 1) * @sizeOf(u16)),
        .PrintNameLength = @intCast(print_wide.len * @sizeOf(u16)),
    };

    const names: [*]u16 = @ptrCast(@alignCast(buffer[@sizeOf(MountPointHeader)..].ptr));
    @memcpy(names[0..substitute_wide.len], substitute_wide);
    names[substitute_wide.len] = 0;
    @memcpy(names[substitute_wide.len + 1 ..][0..print_wide.len], print_wide);
    names[substitute_wide.len + 1 + print_wide.len] = 0;

    var returned: w.DWORD = 0;
    if (!DeviceIoControl(
        handle,
        FSCTL_SET_REPARSE_POINT,
        &buffer,
        @intCast(total),
        null,
        0,
        &returned,
        null,
    ).toBool()) return error.JunctionFailed;
}

fn mklinkJunction(gpa: Allocator, io: Io, link_path: []const u8, target: []const u8) !void {
    // `mklink` is a `cmd` builtin and cannot be spawned directly. `target` is
    // absolute by the time it gets here, which is what keeps cmd from
    // resolving it against its own current directory.
    const result = std.process.run(gpa, io, .{
        .argv = &.{ "cmd", "/c", "mklink", "/J", link_path, target },
        .stdout_limit = .limited(4096),
        .stderr_limit = .limited(4096),
        .timeout = .{ .duration = .{
            .raw = .fromMilliseconds(mklink_timeout_ms),
            .clock = .awake,
        } },
    }) catch return error.JunctionFailed;
    defer gpa.free(result.stdout);
    defer gpa.free(result.stderr);

    // The message is localised, so the exit code is the only thing worth
    // reading.
    switch (result.term) {
        .exited => |code| if (code != 0) return error.JunctionFailed,
        else => return error.JunctionFailed,
    }
}

/// A NUL-terminated WTF-16 copy of a path, for the `W` entry points.
fn widePath(buffer: []u16, utf8: []const u8) ![*:0]const u16 {
    const len = try std.unicode.wtf8ToWtf16Le(buffer[0 .. buffer.len - 1], utf8);
    buffer[len] = 0;
    return @ptrCast(buffer.ptr);
}

/// `REPARSE_DATA_BUFFER` specialised to the mount-point form. The standard
/// library's `REPARSE_DATA_BUFFER` ends in a one-byte flexible array, which
/// makes the offsets awkward to write; this is the same layout with the four
/// mount-point fields spelled out and the names following.
const MountPointHeader = extern struct {
    ReparseTag: std.os.windows.IO_REPARSE_TAG,
    ReparseDataLength: std.os.windows.USHORT,
    Reserved: std.os.windows.USHORT,
    SubstituteNameOffset: std.os.windows.USHORT,
    SubstituteNameLength: std.os.windows.USHORT,
    PrintNameOffset: std.os.windows.USHORT,
    PrintNameLength: std.os.windows.USHORT,
};

/// How long `cmd /c mklink` may take. It is a builtin doing one syscall; this
/// bound is for a machine where spawning `cmd` itself is pathological.
const mklink_timeout_ms: u32 = 10_000;

const GENERIC_READ: std.os.windows.DWORD = 0x8000_0000;
const GENERIC_WRITE: std.os.windows.DWORD = 0x4000_0000;
const FILE_SHARE_READ: std.os.windows.DWORD = 0x0000_0001;
const FILE_SHARE_WRITE: std.os.windows.DWORD = 0x0000_0002;
const FILE_SHARE_DELETE: std.os.windows.DWORD = 0x0000_0004;
const OPEN_EXISTING: std.os.windows.DWORD = 3;
const FILE_FLAG_BACKUP_SEMANTICS: std.os.windows.DWORD = 0x0200_0000;
const FILE_FLAG_OPEN_REPARSE_POINT: std.os.windows.DWORD = 0x0020_0000;
const FSCTL_SET_REPARSE_POINT: std.os.windows.DWORD = 0x0009_00A4;

extern "kernel32" fn CreateFileW(
    lpFileName: [*:0]const u16,
    dwDesiredAccess: std.os.windows.DWORD,
    dwShareMode: std.os.windows.DWORD,
    lpSecurityAttributes: ?*anyopaque,
    dwCreationDisposition: std.os.windows.DWORD,
    dwFlagsAndAttributes: std.os.windows.DWORD,
    hTemplateFile: ?std.os.windows.HANDLE,
) callconv(.winapi) std.os.windows.HANDLE;

extern "kernel32" fn CreateDirectoryW(
    lpPathName: [*:0]const u16,
    lpSecurityAttributes: ?*anyopaque,
) callconv(.winapi) std.os.windows.BOOL;

extern "kernel32" fn RemoveDirectoryW(
    lpPathName: [*:0]const u16,
) callconv(.winapi) std.os.windows.BOOL;

extern "kernel32" fn DeviceIoControl(
    hDevice: std.os.windows.HANDLE,
    dwIoControlCode: std.os.windows.DWORD,
    lpInBuffer: ?*const anyopaque,
    nInBufferSize: std.os.windows.DWORD,
    lpOutBuffer: ?*anyopaque,
    nOutBufferSize: std.os.windows.DWORD,
    lpBytesReturned: *std.os.windows.DWORD,
    lpOverlapped: ?*anyopaque,
) callconv(.winapi) std.os.windows.BOOL;
