//! Offline tests for the short Path1 link and the session-name budget.
//!
//! Nothing here may touch the player's real cache directory, so every case
//! injects `Roots.link_root` — the same trick `daemon_test.zig` plays with
//! `Search`, and for the same reason: a test whose result depends on the
//! machine it runs on is not evidence. The one case that reads the platform
//! layout uses `linkRootPath`, which computes and does not create.
//!
//! The profile directories here are deliberately past 190 bytes and carry the
//! marker `Panzerkommandant`, because the two things worth proving are that
//! the link path is short *and* that the profile name never appears in it.
//! That is the input rclone mangles into a bisync session name, and the
//! measured behaviour — rclone does not dereference a symlink or junction
//! root — is recorded in `docs/superpowers/evidence/cloud-sync/
//! junction-session-name.md`.
//!
//! A skip is written to stderr and the Zig 0.16 build runner fails a test step
//! whose binary writes anything at all to stderr, so the POSIX-only cases
//! return early instead of skipping.

const std = @import("std");
const builtin = @import("builtin");
const plan = @import("plan.zig");

const io = std.testing.io;
const path = std.Io.Dir.path;

/// The profile name whose length is the whole reason this packet exists: 16
/// bytes that must not reach the session name.
const marker = "Panzerkommandant";

/// A temp directory holding both an injected link root and the deep profile
/// directories the links point at.
const Fixture = struct {
    tmp: std.testing.TmpDir,
    gpa: std.mem.Allocator,
    /// Absolute, because a link target must be absolute and a link root is
    /// absolute in the field.
    root: []u8,
    link_root: []u8,

    fn init(gpa: std.mem.Allocator) !Fixture {
        var tmp = std.testing.tmpDir(.{});
        errdefer tmp.cleanup();

        var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const len = try tmp.dir.realPath(io, &buffer);
        const root = try gpa.dupe(u8, buffer[0..len]);
        errdefer gpa.free(root);

        const link_root = try path.join(gpa, &.{ root, "links" });
        return .{ .tmp = tmp, .gpa = gpa, .root = root, .link_root = link_root };
    }

    fn deinit(self: *Fixture) void {
        self.gpa.free(self.link_root);
        self.gpa.free(self.root);
        self.tmp.cleanup();
        self.* = undefined;
    }

    fn roots(self: *Fixture) plan.Roots {
        return .{ .link_root = self.link_root };
    }

    /// A profile directory whose absolute path is past 190 bytes, named after
    /// `name` so the marker can be looked for in the link path afterwards.
    /// Returns the absolute path.
    fn deepProfile(self: *Fixture, name: []const u8) ![]u8 {
        var relative: std.ArrayList(u8) = .empty;
        defer relative.deinit(self.gpa);

        try relative.appendSlice(self.gpa, "profiles");
        try relative.append(self.gpa, path.sep);
        try relative.appendSlice(self.gpa, name);
        // The game is installed wherever the player put it; this stands in for
        // a deep install path rather than for anything the game creates.
        while (self.root.len + 1 + relative.items.len <= 190) {
            try relative.append(self.gpa, path.sep);
            try relative.appendSlice(self.gpa, "deeply-nested-install-directory");
        }

        try self.tmp.dir.createDirPath(io, relative.items);
        return path.join(self.gpa, &.{ self.root, relative.items });
    }

    /// A directory inside the fixture that is not a profile, for the cases
    /// that only need somewhere to point.
    fn shallowDir(self: *Fixture, name: []const u8) ![]u8 {
        try self.tmp.dir.createDirPath(io, name);
        return path.join(self.gpa, &.{ self.root, name });
    }
};

/// A link root short enough to show the real figure. POSIX only: there is no
/// comparably short writable temp root on Windows, where the shipped root is
/// `%LOCALAPPDATA%\bk` and the measured figure came from `C:\bk\p0` on a real
/// machine — see the evidence file.
const ShortRoot = struct {
    gpa: std.mem.Allocator,
    dir: []u8,

    fn init(gpa: std.mem.Allocator) !ShortRoot {
        var suffix: u32 = undefined;
        io.random(std.mem.asBytes(&suffix));
        const dir = try std.fmt.allocPrint(gpa, "/tmp/bk{x:0>8}", .{suffix});
        return .{ .gpa = gpa, .dir = dir };
    }

    fn deinit(self: *ShortRoot) void {
        std.Io.Dir.cwd().deleteTree(io, self.dir) catch {};
        self.gpa.free(self.dir);
        self.* = undefined;
    }
};

fn joinPath(gpa: std.mem.Allocator, parts: []const []const u8) ![]u8 {
    return path.join(gpa, parts);
}

fn readThrough(gpa: std.mem.Allocator, dir: []const u8, name: []const u8) ![]u8 {
    const file_path = try joinPath(gpa, &.{ dir, name });
    defer gpa.free(file_path);
    return std.Io.Dir.cwd().readFileAlloc(io, file_path, gpa, .limited(4096));
}

fn writeThrough(gpa: std.mem.Allocator, dir: []const u8, name: []const u8, data: []const u8) !void {
    const file_path = try joinPath(gpa, &.{ dir, name });
    defer gpa.free(file_path);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = file_path, .data = data });
}

test "the link root is the platform cache directory" {
    const gpa = std.testing.allocator;

    // Computed, never created: this is the only case that names the real
    // layout, and creating it would put a directory in the player's home.
    const roots: plan.Roots = switch (builtin.os.tag) {
        .windows => .{ .local_app_data = "C:\\Users\\player\\AppData\\Local" },
        else => .{ .home = "/home/player" },
    };
    const root = try plan.linkRootPath(gpa, roots);
    defer gpa.free(root);

    const expected = switch (builtin.os.tag) {
        .windows => "C:\\Users\\player\\AppData\\Local\\bk",
        .macos => "/home/player/Library/Caches/blitzkrieg",
        else => "/home/player/.cache/blitzkrieg",
    };
    try std.testing.expectEqualStrings(expected, root);
}

test "a missing base directory is a typed failure, not a relative path" {
    const gpa = std.testing.allocator;

    // Empty means "the platform could not tell us". Falling back to a relative
    // path here would produce a link root under the current directory, which
    // is wherever the game was launched from.
    const roots: plan.Roots = switch (builtin.os.tag) {
        .windows => .{ .local_app_data = "" },
        else => .{ .home = "" },
    };
    try std.testing.expectError(error.RootUnknown, plan.linkRootPath(gpa, roots));
}

test "the link root is created when absent and accepted when present" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const first = try plan.linkRootIn(gpa, io, fixture.roots());
    defer gpa.free(first);
    try std.testing.expectEqualStrings(fixture.link_root, first);

    const stat = try std.Io.Dir.cwd().statFile(io, first, .{});
    try std.testing.expectEqual(std.Io.File.Kind.directory, stat.kind);

    // Twice, because the second launch of the game is the common case.
    const second = try plan.linkRootIn(gpa, io, fixture.roots());
    defer gpa.free(second);
    try std.testing.expectEqualStrings(first, second);
}

test "a deep profile directory is reachable through a short link" {
    // POSIX only for the byte figure; see `ShortRoot`.
    if (builtin.os.tag == .windows) return;
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();
    var short = try ShortRoot.init(gpa);
    defer short.deinit();

    const profile = try fixture.deepProfile(marker);
    defer gpa.free(profile);
    try std.testing.expect(profile.len > 190);

    // Something to list, so that "the same contents" means something.
    try writeThrough(gpa, profile, "campaign.sav", "panzer");

    var link = try plan.ensureShortLinkIn(gpa, io, .{ .link_root = short.dir }, profile);
    defer link.deinit(gpa);

    try std.testing.expect(link.path.len < 40);
    try std.testing.expectEqual(@as(u8, 0), link.slot);

    const through_link = try readThrough(gpa, link.path, "campaign.sav");
    defer gpa.free(through_link);
    try std.testing.expectEqualStrings("panzer", through_link);
}

test "a real file crosses the link in both directions" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const profile = try fixture.deepProfile(marker);
    defer gpa.free(profile);

    var link = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), profile);
    defer link.deinit(gpa);

    // Down: written at the deep path, read back through the link. This is the
    // direction a pull takes when rclone hands the file to Path1.
    try writeThrough(gpa, profile, "autosave.sav", "written-at-the-target");
    const down = try readThrough(gpa, link.path, "autosave.sav");
    defer gpa.free(down);
    try std.testing.expectEqualStrings("written-at-the-target", down);

    // Up: written through the link, read back at the deep path. The Windows
    // probe that settled session naming ran with both sides empty, so this is
    // the half it left unexercised.
    try writeThrough(gpa, link.path, "uploaded.sav", "written-through-the-link");
    const up = try readThrough(gpa, profile, "uploaded.sav");
    defer gpa.free(up);
    try std.testing.expectEqualStrings("written-through-the-link", up);

    // And a file written through the link is a real file at the target, not a
    // copy that happens to read back.
    const uploaded_at_target = try path.join(gpa, &.{ profile, "uploaded.sav" });
    defer gpa.free(uploaded_at_target);
    const target_stat = try std.Io.Dir.cwd().statFile(io, uploaded_at_target, .{});
    try std.testing.expectEqual(std.Io.File.Kind.file, target_stat.kind);
    try std.testing.expectEqual(@as(u64, "written-through-the-link".len), target_stat.size);
}

test "the profile name never reaches the link path" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const profile = try fixture.deepProfile(marker);
    defer gpa.free(profile);

    var link = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), profile);
    defer link.deinit(gpa);

    // Not vacuous: the marker really is in the target.
    try std.testing.expect(std.mem.indexOf(u8, link.target, marker) != null);
    // rclone mangles the path as given into the bisync session name and never
    // dereferences the link, so a marker absent here is a marker absent from
    // the session name. Measured on macOS (symlink) and Windows (junction) —
    // see docs/superpowers/evidence/cloud-sync/junction-session-name.md.
    try std.testing.expect(std.mem.indexOf(u8, link.path, marker) == null);
    try std.testing.expect(link.path.len < profile.len);
}

test "the same profile keeps its slot and is not relinked" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const profile = try fixture.deepProfile(marker);
    defer gpa.free(profile);

    var first = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), profile);
    defer first.deinit(gpa);
    try std.testing.expect(first.method != null);

    var second = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), profile);
    defer second.deinit(gpa);

    try std.testing.expectEqualStrings(first.path, second.path);
    try std.testing.expectEqual(first.slot, second.slot);
    // Null records "nothing was created": relinking on every launch would
    // churn a directory the operating system caches.
    try std.testing.expect(second.method == null);
}

test "distinct profiles take distinct small slots" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const first_profile = try fixture.deepProfile(marker);
    defer gpa.free(first_profile);
    const second_profile = try fixture.deepProfile("Generalfeldmarschall");
    defer gpa.free(second_profile);

    var first = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), first_profile);
    defer first.deinit(gpa);
    var second = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), second_profile);
    defer second.deinit(gpa);

    try std.testing.expectEqual(@as(u8, 0), first.slot);
    try std.testing.expectEqual(@as(u8, 1), second.slot);

    const expected_first = try path.join(gpa, &.{ fixture.link_root, "p0" });
    defer gpa.free(expected_first);
    try std.testing.expectEqualStrings(expected_first, first.path);
}

test "the link method is recorded" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const profile = try fixture.deepProfile(marker);
    defer gpa.free(profile);

    var link = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), profile);
    defer link.deinit(gpa);

    // Junction creation needs no administrator rights — confirmed on a real
    // Windows box — so a Windows run must never come back with a symlink.
    switch (builtin.os.tag) {
        .windows => try std.testing.expect(link.method.? != .symlink),
        else => try std.testing.expectEqual(plan.LinkMethod.symlink, link.method.?),
    }
}

test "a relative profile directory is canonicalised before linking" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const profile = try fixture.deepProfile(marker);
    defer gpa.free(profile);

    // `NProfile::Segment` returns `profiles\<name>\`, relative to the game
    // root, so this is the shape the caller actually has. `mklink /J` would
    // resolve it against cmd's own current directory, and the reparse buffer
    // needs the `\??\C:\...` NT form; both need an absolute path first.
    var cwd_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    // Through `.` rather than the `cwd()` handle: that handle is `AT_FDCWD`,
    // which no `fcntl` can name.
    const cwd_len = try std.Io.Dir.cwd().realPathFile(io, ".", &cwd_buffer);
    const cwd = cwd_buffer[0..cwd_len];

    const relative = try path.relative(gpa, cwd, null, cwd, profile);
    defer gpa.free(relative);
    try std.testing.expect(!path.isAbsolute(relative));

    try writeThrough(gpa, profile, "relative.sav", "resolved");

    var link = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), relative);
    defer link.deinit(gpa);

    try std.testing.expect(path.isAbsolute(link.target));
    const through_link = try readThrough(gpa, link.path, "relative.sav");
    defer gpa.free(through_link);
    try std.testing.expectEqualStrings("resolved", through_link);

    // And the relative path names the same profile the absolute one does, so
    // the slot is reused rather than a second link being made.
    var again = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), profile);
    defer again.deinit(gpa);
    try std.testing.expectEqual(link.slot, again.slot);
    try std.testing.expect(again.method == null);
}

test "a link is repointed by removal and recreation" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const first_profile = try fixture.deepProfile(marker);
    defer gpa.free(first_profile);
    const second_profile = try fixture.deepProfile("Generalfeldmarschall");
    defer gpa.free(second_profile);

    try writeThrough(gpa, first_profile, "first.sav", "first");
    try writeThrough(gpa, second_profile, "second.sav", "second");

    var link = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), first_profile);
    defer link.deinit(gpa);

    var repointed = try plan.repointShortLinkIn(gpa, io, fixture.roots(), link.slot, second_profile);
    defer repointed.deinit(gpa);

    try std.testing.expectEqualStrings(link.path, repointed.path);
    try std.testing.expect(repointed.method != null);

    const now_visible = try readThrough(gpa, repointed.path, "second.sav");
    defer gpa.free(now_visible);
    try std.testing.expectEqualStrings("second", now_visible);

    // The old profile is no longer reachable through the slot...
    try std.testing.expectError(
        error.FileNotFound,
        readThrough(gpa, repointed.path, "first.sav"),
    );
    // ...but it is still there. Repointing a link must never delete a save.
    const still_there = try readThrough(gpa, first_profile, "first.sav");
    defer gpa.free(still_there);
    try std.testing.expectEqualStrings("first", still_there);
}

test "a dangling slot is reclaimed rather than leaked" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const gone = try fixture.shallowDir("to-be-deleted");
    defer gpa.free(gone);

    var stale = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), gone);
    defer stale.deinit(gpa);
    try std.testing.expectEqual(@as(u8, 0), stale.slot);

    // The profile is deleted out from under the link, which is what happens
    // when a player removes one. A slot held by a link to nothing forever
    // would run the game out of slots.
    try std.Io.Dir.cwd().deleteTree(io, gone);

    const profile = try fixture.deepProfile(marker);
    defer gpa.free(profile);
    var link = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), profile);
    defer link.deinit(gpa);

    try std.testing.expectEqual(@as(u8, 0), link.slot);
    try std.testing.expectEqualStrings(stale.path, link.path);
}

test "a profile directory that is not there is a typed failure" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const missing = try path.join(gpa, &.{ fixture.root, "no-such-profile" });
    defer gpa.free(missing);

    try std.testing.expectError(
        error.TargetNotFound,
        plan.ensureShortLinkIn(gpa, io, fixture.roots(), missing),
    );

    // A file is not a profile directory, and linking one would give bisync a
    // Path1 it cannot list.
    const file_path = try path.join(gpa, &.{ fixture.root, "not-a-directory" });
    defer gpa.free(file_path);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = file_path, .data = "x" });

    try std.testing.expectError(
        error.TargetNotDirectory,
        plan.ensureShortLinkIn(gpa, io, fixture.roots(), file_path),
    );
}

test "the slot space is bounded and the bound is reported" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    var slot: u8 = 0;
    while (slot < plan.max_slots) : (slot += 1) {
        const name = try std.fmt.allocPrint(gpa, "profile-{d}", .{slot});
        defer gpa.free(name);
        const dir = try fixture.shallowDir(name);
        defer gpa.free(dir);

        var link = try plan.ensureShortLinkIn(gpa, io, fixture.roots(), dir);
        defer link.deinit(gpa);
        try std.testing.expectEqual(slot, link.slot);
    }

    const overflow = try fixture.shallowDir("one-too-many");
    defer gpa.free(overflow);
    try std.testing.expectError(
        error.NoFreeSlot,
        plan.ensureShortLinkIn(gpa, io, fixture.roots(), overflow),
    );
}

test "repointing a slot outside the space is refused" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const dir = try fixture.shallowDir("profile");
    defer gpa.free(dir);

    try std.testing.expectError(
        error.SlotOutOfRange,
        plan.repointShortLinkIn(gpa, io, fixture.roots(), plan.max_slots, dir),
    );
}

// -- Session-name budget -----------------------------------------------------
//
// Pure arithmetic from here down: no filesystem, no platform variance. The
// expected strings are identical on every OS because `canonicalPath` folds
// both separators to `_`, which the cross-platform pairs below rely on.

test "fsPath mirrors rclone's separator handling" {
    const gpa = std.testing.allocator;

    // A local path is suffixed with the platform separator — on Windows after
    // its forward slashes are rewritten, which is `bilib.FsPath`'s order.
    const local = try plan.fsPath(gpa, "/tmp/bkp", .local);
    defer gpa.free(local);
    const expected = if (builtin.os.tag == .windows) "\\tmp\\bkp\\" else "/tmp/bkp/";
    try std.testing.expectEqualStrings(expected, local);

    // An already-suffixed path gains nothing.
    const suffixed_input = if (builtin.os.tag == .windows) "C:\\bk\\p0\\" else "/bk/p0/";
    const suffixed = try plan.fsPath(gpa, suffixed_input, .local);
    defer gpa.free(suffixed);
    try std.testing.expectEqualStrings(suffixed_input, suffixed);

    // A remote contributes `name:root` and a `/` regardless of platform.
    const remote = try plan.fsPath(gpa, "bkremote:profiles/Foo", .remote);
    defer gpa.free(remote);
    try std.testing.expectEqualStrings("bkremote:profiles/Foo/", remote);
}

test "canonicalisation covers rclone's character class" {
    const gpa = std.testing.allocator;

    // Every member of `[\s\\/:?*]` in one string, wrapped in the separators
    // that must be trimmed rather than replaced. The trailing `\n` survives
    // the trim — only `\` and `/` are trimmed — and is then replaced, which
    // is why the expectation ends in two underscores.
    const canon = try plan.canonicalPath(gpa, "/a b\tc?d*e:f\\g\r\n/");
    defer gpa.free(canon);
    try std.testing.expectEqualStrings("a_b_c_d_e_f_g__", canon);
}

test "the measured probe pairs mangle to the recorded session names" {
    const gpa = std.testing.allocator;

    // The Windows junction probe: a junction at C:\bk\p0 against C:\bk\remote
    // produced exactly this — see docs/superpowers/evidence/cloud-sync/
    // junction-session-name.md. This arithmetic must agree with that evidence.
    const windows_pair = try plan.sessionName(
        gpa,
        .{ .path = "C:\\bk\\p0", .kind = .local },
        .{ .path = "C:\\bk\\remote", .kind = .local },
    );
    defer gpa.free(windows_pair);
    try std.testing.expectEqualStrings("C__bk_p0..C__bk_remote", windows_pair);

    // The macOS symlink probe: a 199-byte directory reached through /tmp/bkp
    // came out as these 21 bytes, and the budget check accepts them.
    const p1: plan.Endpoint = .{ .path = "/tmp/bkp", .kind = .local };
    const p2: plan.Endpoint = .{ .path = "/tmp/bkremote", .kind = .local };
    const posix_pair = try plan.sessionName(gpa, p1, p2);
    defer gpa.free(posix_pair);
    try std.testing.expectEqualStrings("tmp_bkp..tmp_bkremote", posix_pair);

    var projected: usize = 0;
    try plan.checkSessionBudget(gpa, p1, p2, &projected);
    try std.testing.expectEqual(@as(usize, 21), projected);
}

test "a named remote contributes its name, colon and root" {
    const gpa = std.testing.allocator;

    // Path2 as the field will see it: remote name plus profile prefix. The
    // space matters — profile names carry them, and `\s` is in the class.
    const name = try plan.sessionName(
        gpa,
        .{ .path = "/tmp/bkp", .kind = .local },
        .{ .path = "bkremote:profiles/My Profile", .kind = .remote },
    );
    defer gpa.free(name);
    try std.testing.expectEqualStrings("tmp_bkp..bkremote_profiles_My_Profile", name);
}

test "the measured overlong pair is refused with the number" {
    const gpa = std.testing.allocator;

    // 120 + ".." + 127 = 249 bytes, the measured failure: bisync would die
    // writing `<249 bytes>.path1.lst-new`, a 263-byte filename.
    const p1: plan.Endpoint = .{ .path = "/" ++ ("a" ** 120), .kind = .local };
    const p2: plan.Endpoint = .{ .path = "/" ++ ("b" ** 127), .kind = .local };

    const name = try plan.sessionName(gpa, p1, p2);
    defer gpa.free(name);
    try std.testing.expectEqual(@as(usize, 249), name.len);

    var projected: usize = 0;
    try std.testing.expectError(
        error.SessionNameTooLong,
        plan.checkSessionBudget(gpa, p1, p2, &projected),
    );
    // The number is written even though the call failed: 249 against a budget
    // of 241 names the eight bytes that have to go, and "sync failed" names
    // nothing.
    try std.testing.expectEqual(@as(usize, 249), projected);
}

test "the budget boundary sits at exactly 241 bytes" {
    const gpa = std.testing.allocator;

    // `.path1.lst-new` is 14 bytes; 255 - 14 = 241. A session name of exactly
    // 241 fills the filename limit to the byte and must pass.
    try std.testing.expectEqual(@as(usize, 14), plan.session_suffix_max);
    try std.testing.expectEqual(@as(usize, 241), plan.session_budget);

    const p1: plan.Endpoint = .{ .path = "/" ++ ("a" ** 120), .kind = .local };
    var projected: usize = 0;

    try plan.checkSessionBudget(
        gpa,
        p1,
        .{ .path = "/" ++ ("b" ** 119), .kind = .local },
        &projected,
    );
    try std.testing.expectEqual(@as(usize, 241), projected);

    // One more byte and the `-new` listing cannot be created.
    try std.testing.expectError(
        error.SessionNameTooLong,
        plan.checkSessionBudget(
            gpa,
            p1,
            .{ .path = "/" ++ ("b" ** 120), .kind = .local },
            &projected,
        ),
    );
    try std.testing.expectEqual(@as(usize, 242), projected);
}
