//! Root module and C ABI for cloud profile sync.
//!
//! Everything C++ ever sees of this module is below: five `callconv(.c)`
//! functions, no zig error union, no slice, no allocator, no pointer into any
//! structure this module owns. That is deliberate. The game's C++ half is
//! built by a different compiler with a different runtime, and a zig type
//! crossing that boundary is an ABI it cannot describe.
//!
//! **Conventions every later export follows** (they exist so the surface stays
//! predictable as packets add to it):
//!
//! - A handle is a small non-negative index into a fixed-size table, never a
//!   pointer. `-1` is the only failure value any function returns; a caller
//!   distinguishes failures by calling `bk_cloudsync_last_error`, never by
//!   reading a second error code.
//! - A returned string is owned by this module and valid until the next call
//!   on the same handle (for the module-wide strings, until the next call at
//!   all). The caller copies it if it wants to keep it.
//! - Anything returning bytes writes them into a caller-supplied buffer and
//!   returns the length, so that ownership never crosses.
//! - No export blocks on a socket. `bk_cloudsync_discovery_status` and
//!   `bk_cloudsync_refresh_discovery` can spawn `rclone version` and so may
//!   take a moment; from P02 onward the sync itself lives on a worker thread
//!   and the main loop only polls.
//!
//! Exports arrive with the packet that implements them, never before — see the
//! amendment rule in the plan's `EXECUTION.md`. There are no stubs here.
//!
//! ## The discovery cache
//!
//! `bk_cloudsync_available` and `bk_cloudsync_discovery_status` answer two
//! questions about one fact, so they read one cached `daemon.Availability`
//! rather than each running their own search: two searches can disagree — a
//! binary deleted between them is enough — and a settings screen that says
//! "unavailable: not found" next to an enabled sync button is worse than
//! either answer alone. Discovery runs on the first call and then only when
//! `bk_cloudsync_refresh_discovery` asks for it again.
//!
//! **Until `P03-M01` exists there is no `rclone_path` to consult**, so the
//! search covers the game directory and `PATH` only. That packet adds the
//! explicit path from `cloud.credentials` as the first source, and owns
//! invalidating this cache when the player changes it — the refresh entry
//! point below is what it calls. The gap is recorded on both sides so neither
//! packet assumes the other closed it.
//!
//! ## Thread safety
//!
//! The refresh above is triggered from the UI thread while the sync worker
//! (P02-M02) may be reading the discovered path in order to spawn a daemon. A
//! refresh that frees the old strings underneath that reader is a
//! use-after-free whose crash lands nowhere near its cause, so:
//!
//! - the cache is guarded by a mutex;
//! - **no caller ever holds a pointer into it.** `available` returns a copied
//!   bool and `discovery_status` serialises into the caller's buffer, both
//!   under the lock. Anything that needs the path itself copies it into its
//!   own allocation before releasing. With no borrows outstanding, a refresh
//!   can free the previous value immediately instead of reference-counting it;
//! - the probe runs **outside** the lock. `daemon.probeVersion` spawns a
//!   subprocess, and holding a mutex across a process launch would stall every
//!   reader for its duration;
//! - and because the probe runs outside the lock, each refresh carries a
//!   generation. Two refreshes can overlap — a credentials save while a retry
//!   is already in flight — and can finish in either order. The mutex makes
//!   that safe; it says nothing about ordering. A refresh publishes only if no
//!   refresh has started since it did, so the one that started last is the one
//!   whose answer survives, whenever it happens to finish. See `refresh`.

const std = @import("std");
const builtin = @import("builtin");
const backup = @import("backup.zig");
const catalogue = @import("catalogue.zig");
const creds = @import("creds.zig");
const daemon = @import("daemon.zig");
const form = @import("form.zig");
const worker = @import("worker.zig");

const Allocator = std.mem.Allocator;

/// Zig 0.16 moved the mutex out of `std.Thread` and into `std.Io`, where
/// locking takes an `Io` because a contended wait is a cancellation point.
/// A C ABI entry point has no `Io` to pass and should not have to build one:
/// the only thing a lock needs here is a futex, and `Io.Threaded`'s futex
/// vtable entries ignore the instance they are given entirely. A statically
/// initialised instance is therefore sufficient, costs nothing at startup, and
/// needs no teardown — unlike a real `Io.Threaded`, which would have to be
/// created before the first lock and so would need a lock of its own.
var lock_io_impl: std.Io.Threaded = .init_single_threaded;

fn lockIo() std.Io {
    return lock_io_impl.io();
}

/// How a refresh obtains an `Availability`. A function pointer only so that
/// the ordering tests can substitute a probe whose duration they control;
/// production always uses `resolveWithOwnIo`.
pub const Resolver = *const fn (gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability;

/// `daemon.resolveIn` wants an `Io` and this module has no long-lived one yet
/// — the daemon supervisor that will own one arrives with P02. Building a
/// short-lived `Io.Threaded` per refresh is what `daemon.resolve` itself does,
/// and a refresh is rare enough that the setup cost is invisible next to the
/// process launch inside it.
fn resolveWithOwnIo(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    var threaded: std.Io.Threaded = .init(gpa, .{});
    defer threaded.deinit();
    return daemon.resolveIn(gpa, threaded.io(), search);
}

pub const StatusError = error{
    /// The caller's buffer cannot hold the document and its terminator.
    NoSpaceLeft,
    /// Discovery has not produced a value, which can only mean it failed.
    NotDiscovered,
};

pub const Discovery = struct {
    gpa: Allocator,
    resolver: Resolver = resolveWithOwnIo,
    search: daemon.Search = .{},

    mutex: std.Io.Mutex = .init,
    /// The ticket counter. Every refresh takes the next value under the lock,
    /// and publishes only if the counter has not moved since — see `refresh`.
    next_gen: u64 = 0,
    /// Null before the first successful discovery, and after `clear`.
    value: ?daemon.Availability = null,

    fn lock(self: *Discovery) void {
        self.mutex.lockUncancelable(lockIo());
    }

    fn unlock(self: *Discovery) void {
        self.mutex.unlock(lockIo());
    }

    /// Run discovery and replace the cache with the result.
    ///
    /// The publish predicate is `my_gen == self.next_gen`: publish only if no
    /// refresh has started since this one did. The weaker `my_gen >
    /// published_gen` is not enough and fails in exactly the case that
    /// matters — with generations 1 and 2 both in flight and nothing published
    /// yet, generation 1 satisfies `1 > 0` and installs its result while 2 is
    /// still probing. Generation 2 is typically the credentials save that just
    /// changed the path, so the cache would then serve the superseded path to
    /// the worker for as long as the newer probe takes.
    ///
    /// A superseded refresh still succeeds from its caller's point of view; it
    /// simply does not publish. And a refresh that fails leaves the cache
    /// alone rather than reverting it to an older probe's answer:
    /// stale-but-known beats confidently wrong.
    pub fn refresh(self: *Discovery) Allocator.Error!void {
        self.lock();
        self.next_gen += 1;
        const my_gen = self.next_gen;
        // Copied under the lock: `setExplicit` replaces this struct from the
        // UI thread, and a torn read of a slice is a crash. The slice
        // contents themselves are never freed — see `setExplicit`.
        const search = self.search;
        self.unlock();

        // Outside the lock: this launches a process and waits for it.
        var probed = try self.resolver(self.gpa, search);

        self.lock();
        defer self.unlock();
        if (my_gen != self.next_gen) {
            probed.deinit(self.gpa);
            return;
        }
        if (self.value) |*previous| previous.deinit(self.gpa);
        self.value = probed;
    }

    /// Discovery runs once. Concurrent first callers may each start one; the
    /// generation rule then discards all but the newest, which is the same
    /// outcome as an explicit refresh race and needs no separate handling.
    fn ensure(self: *Discovery) Allocator.Error!void {
        self.lock();
        const discovered = self.value != null;
        self.unlock();
        if (discovered) return;
        return self.refresh();
    }

    /// A copy of the verdict, never a borrow of it.
    pub fn available(self: *Discovery) bool {
        self.lock();
        defer self.unlock();
        const current = self.value orelse return false;
        return current == .ready;
    }

    /// Serialise the cached verdict into `out` as `{ found, path, version,
    /// reason }`, under the lock, and return its length excluding the NUL.
    ///
    /// `reason` is the typed `daemon.Reason` rather than free text: the
    /// settings screen has to tell a player *why* the feature is unavailable
    /// and *which* binary was chosen, and neither a boolean nor a generic
    /// error string can answer either question.
    pub fn writeStatus(self: *Discovery, out: []u8) StatusError!usize {
        if (out.len == 0) return error.NoSpaceLeft;
        self.lock();
        defer self.unlock();
        const current = self.value orelse return error.NotDiscovered;

        // One byte held back for the terminator, which is not part of the
        // returned length but lets a C caller treat the buffer as a string.
        var writer: std.Io.Writer = .fixed(out[0 .. out.len - 1]);
        var json: std.json.Stringify = .{ .writer = &writer };
        writeAvailability(&json, current) catch return error.NoSpaceLeft;
        const length = writer.buffered().len;
        out[length] = 0;
        return length;
    }

    /// Drop the cache and make sure no probe already in flight can install
    /// itself afterwards — bumping the counter supersedes every outstanding
    /// ticket at once.
    pub fn clear(self: *Discovery) void {
        self.lock();
        defer self.unlock();
        self.next_gen += 1;
        if (self.value) |*previous| previous.deinit(self.gpa);
        self.value = null;
    }

    /// Install the player's explicit rclone override as the first search
    /// source. The *previous* override allocation is deliberately never
    /// freed: a refresh already in flight captured the old slice outside the
    /// lock and may read it for the length of a probe, and a player changes
    /// this path a handful of times per session — a bounded, tiny leak buys
    /// the absence of a use-after-free with no reference counting.
    pub fn setExplicit(self: *Discovery, path: ?[]const u8) Allocator.Error!void {
        const copy: ?[]u8 = if (path) |p| try self.gpa.dupe(u8, p) else null;
        self.lock();
        defer self.unlock();
        self.search.explicit = copy;
    }
};

fn writeAvailability(json: *std.json.Stringify, current: daemon.Availability) std.json.Stringify.Error!void {
    const found = current == .ready;
    const chosen: ?[]const u8 = switch (current) {
        .ready => |ready| ready.path,
        .unavailable => |rejected| rejected.path,
    };
    const version: ?daemon.Version = switch (current) {
        .ready => |ready| ready.version,
        .unavailable => |rejected| rejected.version,
    };

    try json.beginObject();
    try json.objectField("found");
    try json.write(found);
    try json.objectField("path");
    if (chosen) |value| try json.write(value) else try json.write(null);
    try json.objectField("version");
    if (version) |value| {
        var buffer: [48]u8 = undefined;
        // A three-part version never approaches 48 bytes.
        const text = std.fmt.bufPrint(&buffer, "{d}.{d}.{d}", .{ value.major, value.minor, value.patch }) catch unreachable;
        try json.write(text);
    } else try json.write(null);
    try json.objectField("reason");
    if (current.reason()) |reason| try json.write(@tagName(reason)) else try json.write(null);
    try json.endObject();
}

// ---------------------------------------------------------------------------
// Module state behind the C ABI
// ---------------------------------------------------------------------------

/// `smp_allocator` rather than a page allocator because every export can be
/// called from any thread, and rather than a debug allocator because the
/// shipping module must not pay for leak tracking. The tests below build their
/// own `Discovery` values over `std.testing.allocator`, so this instance is
/// only ever exercised through the C++ smoke consumer.
var module: Discovery = .{ .gpa = std.heap.smp_allocator };

var error_mutex: std.Io.Mutex = .init;
/// Module-owned, valid until the next call, as the conventions above promise.
/// Fixed storage so the pointer itself never dangles; only the contents change.
var error_text: [512]u8 = @splat(0);

fn setError(comptime message: []const u8) void {
    comptime std.debug.assert(message.len < error_text.len);
    error_mutex.lockUncancelable(lockIo());
    defer error_mutex.unlock(lockIo());
    @memcpy(error_text[0..message.len], message);
    error_text[message.len] = 0;
}

fn clearError() void {
    error_mutex.lockUncancelable(lockIo());
    defer error_mutex.unlock(lockIo());
    error_text[0] = 0;
}

/// 1 when a usable rclone was found, 0 otherwise. Reads the same cached
/// discovery `bk_cloudsync_discovery_status` reports, so the two can never
/// disagree.
pub export fn bk_cloudsync_available() callconv(.c) u32 {
    moduleEnsure() catch {
        setError("cloud sync: out of memory while searching for rclone");
        return 0;
    };
    clearError();
    return if (module.available()) 1 else 0;
}

/// Write `{ found, path, version, reason }` into `json_out` as NUL-terminated
/// JSON and return its length excluding the NUL, or -1 on failure. On failure
/// the contents of `json_out` are unspecified.
pub export fn bk_cloudsync_discovery_status(json_out: [*]u8, cap: u32) callconv(.c) i32 {
    moduleEnsure() catch {
        setError("cloud sync: out of memory while searching for rclone");
        return -1;
    };
    const length = module.writeStatus(json_out[0..cap]) catch |err| {
        switch (err) {
            error.NoSpaceLeft => setError("cloud sync: buffer too small for the discovery status document"),
            error.NotDiscovered => setError("cloud sync: rclone discovery has not run"),
        }
        return -1;
    };
    clearError();
    return @intCast(length);
}

/// Re-run discovery and replace the cache. 0 on success, -1 on failure; a
/// refresh that a newer one superseded reports success without publishing, and
/// a refresh that fails leaves the previous value in place.
pub export fn bk_cloudsync_refresh_discovery() callconv(.c) i32 {
    bootstrapExplicitFromCredentials();
    module.refresh() catch {
        setError("cloud sync: out of memory while searching for rclone");
        return -1;
    };
    clearError();
    return 0;
}

/// Release everything this module owns. Idempotent, safe to call twice, and
/// safe with a sync in flight: the worker's destroy cancels the run and is
/// bounded by one POST deadline, and the daemon is shut down with it. A later
/// call re-runs discovery and can start a fresh worker.
pub export fn bk_cloudsync_shutdown() callconv(.c) void {
    // Detach under the lock, destroy outside it: `Worker.destroy` blocks for
    // up to a deadline, and nothing else may wait on the jobs lock that long.
    jobs_mutex.lockUncancelable(lockIo());
    const w = sync_worker;
    sync_worker = null;
    const io_impl = sync_io_impl;
    sync_io_impl = null;
    if (sync_game_dir) |dir| module_gpa.free(dir);
    sync_game_dir = null;
    job_slots = @splat(.{});
    jobs_mutex.unlock(lockIo());

    if (w) |live| live.destroy();
    if (io_impl) |impl| {
        impl.deinit();
        module_gpa.destroy(impl);
    }

    module.clear();
    clearError();
}

/// The most recent failure, or an empty string when the most recent call
/// succeeded. Module-owned; copy it if you need to keep it.
pub export fn bk_cloudsync_last_error() callconv(.c) [*:0]const u8 {
    return @ptrCast(&error_text);
}

// ---------------------------------------------------------------------------
// Credentials
//
// Four exports over `profiles/cloud.credentials`, resolved against the
// game's working directory — the engine's own convention for profile paths.
// The rules live in `creds.zig`; what the ABI adds is the discovery contract:
// a successful save installs the file's `rclone_path` as the explicit search
// source and re-runs discovery through the P00-M04 locking contract, so
// `available` and `discovery_status` reflect the new path on the very next
// call, with no restart. The secret never crosses this boundary outward:
// `creds_load` serialises the redacted form with `has_secret`, and a save
// without a secret merges the stored one rather than destroying it.
// ---------------------------------------------------------------------------

/// One io for the credential exports' file work. Blocking is acceptable here
/// for the same reason it is in `refresh_discovery`: these are settings-screen
/// operations, documented as possibly taking a moment.
fn credsIo() std.Io {
    return lock_io_impl.io();
}

/// Discovery's cold-start seed, run once per process before the first probe:
/// the credentials file's `rclone_path` is the player's explicit override,
/// and it must win on a fresh launch too — not only after the next
/// `creds_save` installs it live. Found the hard way: a game restarted with
/// valid credentials on disk reported rclone missing.
var explicit_bootstrap_done: std.atomic.Value(bool) = .init(false);

fn bootstrapExplicitFromCredentials() void {
    if (explicit_bootstrap_done.swap(true, .acq_rel)) return;
    var loaded = (creds.load(module_gpa, credsIo(), creds.default_path) catch null) orelse return;
    defer loaded.deinit();
    const override = loaded.creds.rclone_path orelse return;
    module.setExplicit(override) catch {};
}

/// `module.ensure` with the cold-start seed applied first.
fn moduleEnsure() Allocator.Error!void {
    bootstrapExplicitFromCredentials();
    return module.ensure();
}

/// Write the redacted credentials document into `json_out` and return its
/// length excluding the NUL, or -1 when none are saved or the buffer is too
/// small. The secret itself never leaves; `has_secret` says whether one is
/// stored.
pub export fn bk_cloudsync_creds_load(json_out: [*]u8, cap: u32) callconv(.c) i32 {
    var loaded = (creds.load(module_gpa, credsIo(), creds.default_path) catch null) orelse {
        setError("cloud sync: no credentials are saved");
        return -1;
    };
    defer loaded.deinit();

    const document = creds.redacted(module_gpa, loaded.creds) catch {
        setError("cloud sync: out of memory serialising credentials");
        return -1;
    };
    defer module_gpa.free(document);

    if (cap == 0 or document.len >= cap) {
        setError("cloud sync: buffer too small for the credentials document");
        return -1;
    }
    @memcpy(json_out[0..document.len], document);
    json_out[document.len] = 0;
    clearError();
    return @intCast(document.len);
}

/// Parse and persist a credentials document, merging the stored secret when
/// the incoming one is absent — the dialog cannot send back what `creds_load`
/// withheld, and editing the endpoint must not destroy the credential. On
/// success the file's `rclone_path` becomes the explicit discovery source and
/// discovery re-runs before this returns.
pub export fn bk_cloudsync_creds_save(json: [*:0]const u8) callconv(.c) i32 {
    var incoming = (creds.parse(module_gpa, std.mem.span(json)) catch {
        setError("cloud sync: out of memory parsing credentials");
        return -1;
    }) orelse {
        setError("cloud sync: the credentials document is malformed or names an unknown protocol");
        return -1;
    };
    defer incoming.deinit();

    var stored = creds.load(module_gpa, credsIo(), creds.default_path) catch null;
    defer if (stored) |*loaded| loaded.deinit();
    if (stored) |loaded| {
        creds.mergeOmittedSecret(&incoming.creds, loaded.creds);
        cleanupVendorChange(&incoming.creds, loaded.creds);
    }

    creds.save(module_gpa, credsIo(), creds.default_path, incoming.creds) catch {
        setError("cloud sync: the credentials file could not be written");
        return -1;
    };

    // The saved path becomes the first discovery source, and the cache is
    // replaced through the locking contract — never reached into — so the
    // worker can be mid-read on the old value without a tear.
    module.setExplicit(incoming.creds.rclone_path) catch {
        setError("cloud sync: out of memory installing the rclone override");
        return -1;
    };
    module.refresh() catch {
        setError("cloud sync: out of memory while searching for rclone");
        return -1;
    };
    clearError();
    return 0;
}

/// Remove the stored secret — the deliberate act, distinct from saving.
pub export fn bk_cloudsync_creds_clear_secret() callconv(.c) i32 {
    var loaded = (creds.load(module_gpa, credsIo(), creds.default_path) catch null) orelse {
        setError("cloud sync: no credentials are saved");
        return -1;
    };
    defer loaded.deinit();

    creds.clearSecret(&loaded.creds);
    creds.save(module_gpa, credsIo(), creds.default_path, loaded.creds) catch {
        setError("cloud sync: the credentials file could not be written");
        return -1;
    };
    clearError();
    return 0;
}

/// 1 when a parseable credentials file exists, 0 otherwise.
pub export fn bk_cloudsync_creds_present() callconv(.c) u32 {
    var loaded = (creds.load(module_gpa, credsIo(), creds.default_path) catch null) orelse return 0;
    loaded.deinit();
    return 1;
}

/// A vendor change is not a backend change: switching S3 from one vendor to
/// another leaves the backend as `s3`, so backend-scoped preservation keeps
/// every merged option — including ones the new vendor never declares, which
/// would keep being sent to rclone. When the `provider` value changes
/// (rclone's own convention for the vendor option, a structural name like
/// `type`, referenced by every `Provider` expression), drop options whose
/// expression no longer matches and clear a closed (`Exclusive`) field's
/// value that is absent from its newly filtered examples; editable fields
/// keep arbitrary values legitimately.
///
/// This runs on the **final merged submission**, not the stored map alone:
/// the dialog resubmits values it preserved across a rebuild, so a value
/// cleaned out of storage can arrive again in the same save. Judgement needs
/// the catalogue; with no cache (or a backend it does not name) nothing can
/// be judged and the submission passes through — the next save with a
/// catalogue filters it.
fn cleanupVendorChange(merged: *creds.Credentials, stored: creds.Credentials) void {
    if (!std.mem.eql(u8, merged.backend, stored.backend)) return;
    const chosen = optionValueOf(merged.*, "provider");
    if (std.mem.eql(u8, chosen, optionValueOf(stored, "provider"))) return;

    // The creds exports resolve everything against the working directory —
    // the game root by the engine's convention — so the cache lives at
    // `./cloudsync/providers.json` here, exactly as the facade passes "."
    // for every game_dir.
    var cat = catalogue.loadCached(module_gpa, credsIo(), ".") catch return;
    defer cat.deinit();
    const backend = cat.backend(merged.backend) orelse return;

    var keep: usize = 0;
    for (merged.options) |opt| {
        // An option the catalogue does not name cannot be judged; keep it —
        // the unknown-field tolerance from the other direction.
        const record = backend.option(opt.name) orelse {
            merged.options[keep] = opt;
            keep += 1;
            continue;
        };
        if (!record.appliesTo(chosen)) continue;
        var kept = opt;
        if (record.exclusive and kept.value.len != 0) {
            const offered = for (record.examples) |example| {
                if (example.appliesTo(chosen) and std.mem.eql(u8, example.value, kept.value))
                    break true;
            } else false;
            // Cleared, not dropped: the field still applies, its value does
            // not. The empty value never reaches disk.
            if (!offered) kept.value = "";
        }
        merged.options[keep] = kept;
        keep += 1;
    }
    merged.options = merged.options[0..keep];
}

fn optionValueOf(credentials: creds.Credentials, name: []const u8) []const u8 {
    const opt = credentials.option(name) orelse return "";
    return opt.value;
}

/// The buffer contract the catalogue-era exports share, replacing the older
/// truncate-to--1: the return value is always the document's length
/// excluding the NUL, and the document was written only when that length is
/// smaller than `cap` — otherwise the buffer is untouched and the caller
/// retries with `cap = length + 1`. Option sets vary by orders of magnitude
/// between backends, so a caller must be able to size a second call.
fn writeSized(out: [*]u8, cap: u32, text: []const u8) i32 {
    if (text.len >= cap) return @intCast(text.len);
    @memcpy(out[0..text.len], text);
    out[text.len] = 0;
    return @intCast(text.len);
}

/// Write the persisted pairing fingerprint — the connection identity, no
/// secret material — under the `writeSized` contract, or -1 when no
/// credentials are saved. This is what the facade compares against the
/// pairing record; it replaced a scraper that derived the identity from
/// legacy JSON fields and broke against the generic schema.
pub export fn bk_cloudsync_creds_fingerprint(out: [*]u8, cap: u32) callconv(.c) i32 {
    var loaded = (creds.load(module_gpa, credsIo(), creds.default_path) catch null) orelse {
        setError("cloud sync: no credentials are saved");
        return -1;
    };
    defer loaded.deinit();

    const print = creds.fingerprint(module_gpa, loaded.creds) catch {
        setError("cloud sync: out of memory deriving the fingerprint");
        return -1;
    };
    defer module_gpa.free(print);
    clearError();
    return writeSized(out, cap, print);
}

/// Clear one named field — the per-field deliberate act the generic schema
/// needs, since a backend can hold several secrets and the argument-free
/// `bk_cloudsync_creds_clear_secret` (kept for its existing caller, the
/// legacy dialog) cannot say which; that one clears every withheld field at
/// once. Clearing a field with no stored value succeeds: the caller asked
/// for a state. Clearing through a save is impossible by design — omitted
/// or empty withheld fields always preserve.
pub export fn bk_cloudsync_creds_clear_option(name: [*:0]const u8) callconv(.c) i32 {
    var loaded = (creds.load(module_gpa, credsIo(), creds.default_path) catch null) orelse {
        setError("cloud sync: no credentials are saved");
        return -1;
    };
    defer loaded.deinit();

    creds.clearOption(&loaded.creds, std.mem.span(name));
    creds.save(module_gpa, credsIo(), creds.default_path, loaded.creds) catch {
        setError("cloud sync: the credentials file could not be written");
        return -1;
    };
    clearError();
    return 0;
}

// ---------------------------------------------------------------------------
// Catalogue exports
//
// The provider catalogue lives in Zig (P01-M01) and the form dialog in C++;
// this is the chain between them, which no packet owned until it was nearly
// unbuildable. Reading is local — the cache document, never a daemon — and
// refreshing is a worker job like everything that touches a socket.
// ---------------------------------------------------------------------------

/// `bk_cloudsync_catalogue_ensure`'s "read the cache now" answer. A success,
/// not a failure: -1 stays the only failure value.
const catalogue_cached: i32 = -2;

/// Make sure the catalogue cache describes the discovered rclone. Returns
/// `catalogue_cached` (-2) when it already does — a local stamp read, no job
/// and no daemon, asserted by the worker staying idle — or a pollable job
/// handle whose outcome is `catalogue_ready` (8) once a fetch replaced the
/// cache, or -1 with a readable error. Cancel and release work like every
/// other job: a player who opens the dialog and closes it strands nothing.
pub export fn bk_cloudsync_catalogue_ensure(game_dir: [*:0]const u8) callconv(.c) i32 {
    const dir = std.mem.span(game_dir);
    moduleEnsure() catch {
        setError("cloud sync: out of memory while searching for rclone");
        return -1;
    };
    // The version to compare against is the discovered binary's: the cache
    // must describe the rclone that will serve the forms, and a version
    // change is exactly what invalidates it.
    const running: daemon.Version = version: {
        module.lock();
        defer module.unlock();
        const current = module.value orelse {
            setError("cloud sync: rclone discovery has not run");
            return -1;
        };
        switch (current) {
            .ready => |ready| break :version ready.version,
            .unavailable => {
                setError("cloud sync: no usable rclone is available");
                return -1;
            },
        }
    };

    const stamp = catalogue.cachedVersion(module_gpa, credsIo(), dir) catch {
        setError("cloud sync: out of memory reading the catalogue stamp");
        return -1;
    };
    if (catalogue.matchesVersion(stamp, running)) {
        clearError();
        return catalogue_cached;
    }

    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());
    return enqueueLocked(dir, .{ .kind = .fetch_catalogue });
}

/// Write `{"providers":[{name, description, hidden}...]}` from the cached
/// catalogue under the `writeSized` contract. A missing cache is an empty
/// list, never an error — the settings screen must render regardless, and
/// `bk_cloudsync_catalogue_ensure` is how the list gets filled. `hidden` is
/// rclone's own "do not offer" flag, carried so the destination filter
/// (P01-M04) can honour it without a second read.
pub export fn bk_cloudsync_catalogue_providers(
    game_dir: [*:0]const u8,
    json_out: [*]u8,
    cap: u32,
) callconv(.c) i32 {
    var cat = catalogue.loadCached(module_gpa, credsIo(), std.mem.span(game_dir)) catch {
        setError("cloud sync: out of memory reading the catalogue");
        return -1;
    };
    defer cat.deinit();

    const text = providersJson(module_gpa, &cat) catch {
        setError("cloud sync: out of memory serialising the provider list");
        return -1;
    };
    defer module_gpa.free(text);
    clearError();
    return writeSized(json_out, cap, text);
}

/// Write one backend's option list — everything a form needs: name, help,
/// type and widget kind, the required/advanced/secret/is_password/exclusive
/// flags, the configurator-hidden bit, the vendor expression, the default's
/// rendering, and the examples with their own vendor expressions. Unknown
/// backend is -1 with a readable error; the caller enumerated providers
/// first.
pub export fn bk_cloudsync_catalogue_options(
    game_dir: [*:0]const u8,
    backend_name: [*:0]const u8,
    json_out: [*]u8,
    cap: u32,
) callconv(.c) i32 {
    var cat = catalogue.loadCached(module_gpa, credsIo(), std.mem.span(game_dir)) catch {
        setError("cloud sync: out of memory reading the catalogue");
        return -1;
    };
    defer cat.deinit();

    const backend = cat.backend(std.mem.span(backend_name)) orelse {
        setError("cloud sync: the catalogue has no such backend");
        return -1;
    };
    const text = optionsJson(module_gpa, backend) catch {
        setError("cloud sync: out of memory serialising the option list");
        return -1;
    };
    defer module_gpa.free(text);
    clearError();
    return writeSized(json_out, cap, text);
}

/// Build the form for one backend under one selected provider and write it
/// under the `writeSized` contract as `{backend, provider, basic, advanced}`
/// — the P02-M01 model, serialised. The provider argument is the whole
/// point of the export existing separately from the option enumeration: a
/// build-by-backend-name call could not express provider filtering, and the
/// dialog would render an unfiltered form. The current option map stays on
/// the C++ side by design — the model does not need it, and shipping it
/// would serialise freshly typed secrets across the boundary on every
/// rebuild; preserving typed values is the dialog's job, by field name.
pub export fn bk_cloudsync_catalogue_form(
    game_dir: [*:0]const u8,
    backend_name: [*:0]const u8,
    selected_provider: [*:0]const u8,
    json_out: [*]u8,
    cap: u32,
) callconv(.c) i32 {
    var cat = catalogue.loadCached(module_gpa, credsIo(), std.mem.span(game_dir)) catch {
        setError("cloud sync: out of memory reading the catalogue");
        return -1;
    };
    defer cat.deinit();

    var built = form.buildForm(
        module_gpa,
        &cat,
        std.mem.span(backend_name),
        std.mem.span(selected_provider),
    ) catch |err| switch (err) {
        error.OutOfMemory => {
            setError("cloud sync: out of memory building the form");
            return -1;
        },
        // An empty catalogue also has no backends, but "fetch it first" is
        // the actionable half of that truth.
        error.UnknownBackend => {
            if (cat.isEmpty()) {
                setError("cloud sync: no provider catalogue is cached; fetch it first");
            } else {
                setError("cloud sync: the catalogue has no such backend");
            }
            return -1;
        },
    };
    defer built.deinit();

    const text = formJson(
        module_gpa,
        &built,
        std.mem.span(backend_name),
        std.mem.span(selected_provider),
    ) catch {
        setError("cloud sync: out of memory serialising the form");
        return -1;
    };
    defer module_gpa.free(text);
    clearError();
    return writeSized(json_out, cap, text);
}

fn formJson(
    gpa: Allocator,
    built: *const form.Form,
    backend_name: []const u8,
    selected_provider: []const u8,
) Allocator.Error![]u8 {
    var out: std.Io.Writer.Allocating = .init(gpa);
    errdefer out.deinit();
    var json: std.json.Stringify = .{ .writer = &out.writer };

    writeForm(&json, built, backend_name, selected_provider) catch {
        out.deinit();
        return error.OutOfMemory;
    };
    return out.toOwnedSlice();
}

fn writeForm(
    json: *std.json.Stringify,
    built: *const form.Form,
    backend_name: []const u8,
    selected_provider: []const u8,
) !void {
    try json.beginObject();
    try json.objectField("backend");
    try json.write(backend_name);
    try json.objectField("provider");
    try json.write(selected_provider);
    try json.objectField("basic");
    try writeFields(json, built.basic);
    try json.objectField("advanced");
    try writeFields(json, built.advanced);
    try json.endObject();
}

fn writeFields(json: *std.json.Stringify, fields: []const form.Field) !void {
    try json.beginArray();
    for (fields) |field| {
        try json.beginObject();
        try json.objectField("role");
        try json.write(@tagName(field.role));
        try json.objectField("name");
        try json.write(field.name);
        try json.objectField("label");
        try json.write(field.label);
        try json.objectField("help");
        try json.write(field.help);
        try json.objectField("widget");
        try json.write(@tagName(field.widget));
        try json.objectField("kind");
        try json.write(@tagName(field.kind));
        try json.objectField("required");
        try json.write(field.required);
        try json.objectField("secret");
        try json.write(field.secret);
        try json.objectField("is_password");
        try json.write(field.is_password);
        try json.objectField("placeholder");
        try json.write(field.placeholder);
        try json.objectField("examples");
        try json.beginArray();
        for (field.examples) |example| {
            try json.beginObject();
            try json.objectField("value");
            try json.write(example.value);
            try json.objectField("help");
            try json.write(example.help);
            try json.endObject();
        }
        try json.endArray();
        try json.endObject();
    }
    try json.endArray();
}

fn providersJson(gpa: Allocator, cat: *const catalogue.Catalogue) Allocator.Error![]u8 {
    var out: std.Io.Writer.Allocating = .init(gpa);
    errdefer out.deinit();
    var json: std.json.Stringify = .{ .writer = &out.writer };

    write: {
        json.beginObject() catch break :write;
        json.objectField("providers") catch break :write;
        json.beginArray() catch break :write;
        for (cat.backends) |backend| {
            json.beginObject() catch break :write;
            json.objectField("name") catch break :write;
            json.write(backend.name) catch break :write;
            json.objectField("description") catch break :write;
            json.write(backend.description) catch break :write;
            json.objectField("hidden") catch break :write;
            json.write(backend.hidden) catch break :write;
            json.endObject() catch break :write;
        }
        json.endArray() catch break :write;
        json.endObject() catch break :write;
        return out.toOwnedSlice();
    }
    out.deinit();
    return error.OutOfMemory;
}

fn optionsJson(gpa: Allocator, backend: *const catalogue.Backend) Allocator.Error![]u8 {
    var out: std.Io.Writer.Allocating = .init(gpa);
    errdefer out.deinit();
    var json: std.json.Stringify = .{ .writer = &out.writer };

    writeOptions(&json, backend) catch {
        out.deinit();
        return error.OutOfMemory;
    };
    return out.toOwnedSlice();
}

fn writeOptions(json: *std.json.Stringify, backend: *const catalogue.Backend) !void {
    try json.beginObject();
    try json.objectField("backend");
    try json.write(backend.name);
    try json.objectField("options");
    try json.beginArray();
    for (backend.options) |option| {
        try json.beginObject();
        try json.objectField("name");
        try json.write(option.name);
        try json.objectField("help");
        try json.write(option.help);
        try json.objectField("type");
        try json.write(option.type_name);
        try json.objectField("kind");
        try json.write(@tagName(option.kind));
        try json.objectField("required");
        try json.write(option.required);
        try json.objectField("advanced");
        try json.write(option.advanced);
        try json.objectField("secret");
        try json.write(option.isSecret());
        try json.objectField("is_password");
        try json.write(option.is_password);
        try json.objectField("exclusive");
        try json.write(option.exclusive);
        try json.objectField("hidden");
        try json.write(option.hiddenFromConfigurator());
        try json.objectField("provider");
        try json.write(option.provider);
        try json.objectField("default_str");
        try json.write(option.default_str);
        try json.objectField("examples");
        try json.beginArray();
        for (option.examples) |example| {
            try json.beginObject();
            try json.objectField("value");
            try json.write(example.value);
            try json.objectField("help");
            try json.write(example.help);
            try json.objectField("provider");
            try json.write(example.provider);
            try json.endObject();
        }
        try json.endArray();
        try json.endObject();
    }
    try json.endArray();
    try json.endObject();
}

// ---------------------------------------------------------------------------
// Sync jobs
//
// The engine is reachable from C++ through six exports and a handle table.
// A handle is an index into `job_slots`, never a pointer; each slot holds a
// value copy of the worker's snapshot, refreshed on `poll` while its job is
// the active one and frozen once a newer job begins. `release` invalidates
// the handle only — the worker, daemon and rc client are shared state owned
// by the module and torn down solely by `bk_cloudsync_shutdown`.
//
// Stable numeric values, which C++ switches on — reordering the Zig enums
// would silently change behaviour, so the mapping is pinned by comptime
// asserts right below and must never be edited in one place only:
//
//   state:   0 idle, 1 starting, 2 pairing, 3 syncing, 4 done, 5 failed
//   outcome: 0 none, 1 paired, 2 synced, 3 failed
//
// `bk_cloudsync_begin` takes one NUL-terminated JSON document rather than the
// bare profile name a caller cannot act on alone:
//
//   { "kind": "pair" | "sync", "path1": ..., "remote": ..., "profile": ...,
//     "game_dir": ..., "profile_id": ..., "remote_fingerprint": ... }
//
// The credentials packet (P03) and the facade (P06) own producing it; until
// then the C++ smoke consumer builds it by hand. Windows paths must be
// JSON-escaped like any other string.
// ---------------------------------------------------------------------------

comptime {
    // The ABI contract above, enforced at build time.
    std.debug.assert(@intFromEnum(worker.State.idle) == 0);
    std.debug.assert(@intFromEnum(worker.State.starting) == 1);
    std.debug.assert(@intFromEnum(worker.State.pairing) == 2);
    std.debug.assert(@intFromEnum(worker.State.syncing) == 3);
    std.debug.assert(@intFromEnum(worker.State.done) == 4);
    std.debug.assert(@intFromEnum(worker.State.failed) == 5);
    std.debug.assert(@intFromEnum(worker.State.testing) == 6);
    std.debug.assert(@intFromEnum(worker.Outcome.none) == 0);
    std.debug.assert(@intFromEnum(worker.Outcome.paired) == 1);
    std.debug.assert(@intFromEnum(worker.Outcome.synced) == 2);
    std.debug.assert(@intFromEnum(worker.Outcome.failed) == 3);
    std.debug.assert(@intFromEnum(worker.Outcome.connection_ok) == 4);
    std.debug.assert(@intFromEnum(worker.Outcome.backups_listed) == 5);
    std.debug.assert(@intFromEnum(worker.Outcome.restore_staged) == 6);
    std.debug.assert(@intFromEnum(worker.Outcome.undo_done) == 7);
    std.debug.assert(@intFromEnum(worker.Outcome.catalogue_ready) == 8);
}

const module_gpa = std.heap.smp_allocator;
const max_job_slots = 8;

const JobSlot = struct {
    in_use: bool = false,
    /// Still the worker's current job. Exactly one slot is active at a time.
    active: bool = false,
    snapshot: worker.Snapshot = .{},
    /// The NUL-terminated copy `bk_cloudsync_error` hands out; per-slot fixed
    /// storage, so the pointer stays valid until the next call on the same
    /// handle, exactly as the module conventions promise.
    error_z: [worker.error_text_max + 1]u8 = @splat(0),
};

var jobs_mutex: std.Io.Mutex = .init;
var job_slots: [max_job_slots]JobSlot = @splat(.{});
var sync_worker: ?*worker.Worker = null;
var sync_io_impl: ?*std.Io.Threaded = null;
/// The game directory the worker was created for. One worker, one game dir;
/// a job naming another is a caller bug reported as a failure, not honoured.
var sync_game_dir: ?[]u8 = null;

/// What `begin` parses. Unknown fields are ignored so later packets can
/// extend the document without breaking older callers.
const JobDoc = struct {
    kind: []const u8,
    path1: []const u8,
    remote: []const u8,
    profile: []const u8,
    game_dir: []const u8,
    profile_id: []const u8,
    remote_fingerprint: []const u8,
    /// Snapshot config.cfg after a clean sync (the `Cloud.Config.Backup`
    /// option). Defaults keep older documents valid.
    backup_config: bool = false,
    host: []const u8 = "",
};

/// The production `BinarySource`: an owned copy out of the discovery cache,
/// taken under that cache's lock — the P00-M04 contract. Runs on the worker
/// thread at spawn time, so the path is as current as the last refresh.
fn resolveFromDiscovery(context: ?*anyopaque, gpa: Allocator) ?[]u8 {
    _ = context;
    moduleEnsure() catch return null;
    module.lock();
    defer module.unlock();
    const current = module.value orelse return null;
    return switch (current) {
        .ready => |ready| gpa.dupe(u8, ready.path) catch null,
        .unavailable => null,
    };
}

/// Enqueue one job described by `job_json` and return its handle, or -1 with
/// a readable `bk_cloudsync_last_error`. Never blocks on a socket: daemon
/// spawn, readiness and the run itself happen on the worker and are observed
/// through `bk_cloudsync_poll`.
pub export fn bk_cloudsync_begin(job_json: [*:0]const u8) callconv(.c) i32 {
    const text = std.mem.span(job_json);
    const parsed = std.json.parseFromSlice(JobDoc, module_gpa, text, .{
        .ignore_unknown_fields = true,
    }) catch {
        setError("cloud sync: the job document is not valid JSON or misses a field");
        return -1;
    };
    defer parsed.deinit();
    const doc = parsed.value;

    const kind: worker.JobKind = if (std.mem.eql(u8, doc.kind, "pair"))
        .pair
    else if (std.mem.eql(u8, doc.kind, "sync"))
        .sync
    else {
        setError("cloud sync: job kind must be \"pair\" or \"sync\"");
        return -1;
    };

    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    return enqueueLocked(doc.game_dir, .{
        .kind = kind,
        .path1 = doc.path1,
        .remote = doc.remote,
        .profile = doc.profile,
        .profile_id = doc.profile_id,
        .remote_fingerprint = doc.remote_fingerprint,
        .backup_config = doc.backup_config,
        .host = doc.host,
    });
}

/// The worker for `game_dir`, created on first use. Call under `jobs_mutex`.
/// Null after setting the module error.
fn ensureWorkerLocked(game_dir: []const u8) ?*worker.Worker {
    if (sync_game_dir) |existing| {
        if (!std.mem.eql(u8, existing, game_dir)) {
            setError("cloud sync: the game directory cannot change between jobs");
            return null;
        }
    }
    if (sync_worker == null) {
        const io_impl = module_gpa.create(std.Io.Threaded) catch {
            setError("cloud sync: out of memory starting the sync worker");
            return null;
        };
        io_impl.* = .init(module_gpa, .{});
        const w = worker.Worker.create(module_gpa, io_impl.io(), .{
            .game_dir = game_dir,
            .binary_source = .{ .resolve = resolveFromDiscovery },
        }) catch {
            io_impl.deinit();
            module_gpa.destroy(io_impl);
            setError("cloud sync: the sync worker could not be started");
            return null;
        };
        sync_game_dir = module_gpa.dupe(u8, game_dir) catch null;
        sync_io_impl = io_impl;
        sync_worker = w;
    }
    return sync_worker.?;
}

/// Enqueue on the (possibly new) worker and hand out a slot. Call under
/// `jobs_mutex`.
fn enqueueLocked(game_dir: []const u8, spec: worker.JobSpec) i32 {
    const w = ensureWorkerLocked(game_dir) orelse return -1;

    // Find a free slot before enqueueing, so a full table cannot strand a
    // job nothing can observe.
    const handle: i32 = for (&job_slots, 0..) |*slot, index| {
        if (!slot.in_use) break @intCast(index);
    } else {
        setError("cloud sync: all job handles are in use; release one first");
        return -1;
    };

    w.begin(spec) catch |err| {
        switch (err) {
            error.Busy => setError("cloud sync: a job is already running; poll it to completion first"),
            error.OutOfMemory => setError("cloud sync: out of memory enqueueing the job"),
        }
        return -1;
    };

    // The previous job, if any, keeps its final snapshot but stops tracking
    // the worker: from here the worker's state describes the new job.
    for (&job_slots) |*slot| {
        if (slot.active) slot.active = false;
    }
    job_slots[@intCast(handle)] = .{
        .in_use = true,
        .active = true,
        .snapshot = w.poll(),
    };
    clearError();
    return handle;
}

/// Probe the configured remote — `operations/list` of its root on the
/// worker, never blocking this thread — and report through the same handle
/// machinery as a sync. On failure the handle's error text begins with the
/// classified outcome's name (`auth_failed: …`, `remote_unreachable: …`,
/// `remote_missing: …`), so the dialog can branch while the human reads
/// rclone's (already redacted) words.
pub export fn bk_cloudsync_test_connection(game_dir: [*:0]const u8) callconv(.c) i32 {
    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    return enqueueLocked(std.mem.span(game_dir), .{
        .kind = .test_connection,
        .path1 = "",
        .remote = creds.sync_remote_name,
        .profile = "",
        .profile_id = "",
        .remote_fingerprint = "",
    });
}

fn slotAt(handle: i32) ?*JobSlot {
    if (handle < 0 or handle >= max_job_slots) return null;
    const slot = &job_slots[@intCast(handle)];
    return if (slot.in_use) slot else null;
}

/// The job's state as the stable numeric mapping above. An invalid or
/// released handle answers `failed` with a readable last error — a state a
/// caller already handles — rather than a second error channel.
pub export fn bk_cloudsync_poll(handle: i32) callconv(.c) u32 {
    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    const slot = slotAt(handle) orelse {
        setError("cloud sync: unknown job handle");
        return @intFromEnum(worker.State.failed);
    };
    if (slot.active) {
        if (sync_worker) |w| slot.snapshot = w.poll();
    }
    return @intFromEnum(slot.snapshot.state);
}

/// How the job ended, meaningful once `poll` reports done or failed.
pub export fn bk_cloudsync_outcome(handle: i32) callconv(.c) u32 {
    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    const slot = slotAt(handle) orelse {
        setError("cloud sync: unknown job handle");
        return @intFromEnum(worker.Outcome.failed);
    };
    if (slot.active) {
        if (sync_worker) |w| slot.snapshot = w.poll();
    }
    return @intFromEnum(slot.snapshot.outcome);
}

/// The job's error text — empty while it runs or when it succeeded. The
/// pointer is valid until the next call on the same handle.
pub export fn bk_cloudsync_error(handle: i32) callconv(.c) [*:0]const u8 {
    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    const slot = slotAt(handle) orelse {
        setError("cloud sync: unknown job handle");
        return "cloud sync: unknown job handle";
    };
    if (slot.active) {
        if (sync_worker) |w| slot.snapshot = w.poll();
    }
    const text = slot.snapshot.errorText();
    @memcpy(slot.error_z[0..text.len], text);
    slot.error_z[text.len] = 0;
    return @ptrCast(&slot.error_z);
}

/// Abandon the wait on a running job. The rclone job keeps running
/// server-side; the handle will report failed with a cancellation text.
pub export fn bk_cloudsync_cancel(handle: i32) callconv(.c) void {
    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    const slot = slotAt(handle) orelse return;
    if (slot.active) {
        if (sync_worker) |w| w.cancel();
    }
}

/// Start fetching the backup listing for `profile` — networked, therefore a
/// pollable job like every other; outcome `backups_listed` (5) on done. The
/// entries are then read one at a time with `bk_cloudsync_backup_entry`.
pub export fn bk_cloudsync_backup_list(
    game_dir: [*:0]const u8,
    profile: [*:0]const u8,
) callconv(.c) i32 {
    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    return enqueueLocked(std.mem.span(game_dir), .{
        .kind = .list_backups,
        .path1 = "",
        .remote = creds.sync_remote_name,
        .profile = std.mem.span(profile),
        .profile_id = "",
        .remote_fingerprint = "",
    });
}

/// Write entry `index` of the most recent completed listing into `json_out`
/// as NUL-terminated `{id, host, timestamp, size, remote_path}` and return
/// its length, or -1 past the end — which is also how a caller counts.
pub export fn bk_cloudsync_backup_entry(
    handle: i32,
    index: u32,
    json_out: [*]u8,
    cap: u32,
) callconv(.c) i32 {
    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    _ = slotAt(handle) orelse {
        setError("cloud sync: unknown job handle");
        return -1;
    };
    const w = sync_worker orelse {
        setError("cloud sync: no worker is running");
        return -1;
    };
    const length = w.backupEntryJson(index, json_out[0..cap]) orelse {
        setError("cloud sync: no such backup entry");
        return -1;
    };
    clearError();
    return @intCast(length);
}

/// Download one backup into a staged restore — pollable, outcome
/// `restore_staged` (6) on done. Nothing is applied here: the stage waits
/// for `bk_cloudsync_apply_pending_restore` at the next startup. `mode` is
/// 0 for the GFX-preserving merge, 1 for a full restore (warn first).
pub export fn bk_cloudsync_backup_restore(
    game_dir: [*:0]const u8,
    profile: [*:0]const u8,
    entry_id: [*:0]const u8,
    mode: u32,
) callconv(.c) i32 {
    var dir_buffer: [1024]u8 = undefined;
    const profile_dir = std.fmt.bufPrint(&dir_buffer, "{s}/profiles/{s}", .{
        std.mem.span(game_dir),
        std.mem.span(profile),
    }) catch {
        setError("cloud sync: the profile path does not fit");
        return -1;
    };

    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    return enqueueLocked(std.mem.span(game_dir), .{
        .kind = .restore_stage,
        .path1 = profile_dir,
        .remote = creds.sync_remote_name,
        .profile = std.mem.span(profile),
        .profile_id = "",
        .remote_fingerprint = "",
        .entry_id = std.mem.span(entry_id),
        .restore_mode = if (mode == 1) .full else .merge_keep_local_gfx,
    });
}

/// Apply the published stage for `profile`, resolved as `profiles/<name>`
/// against the working directory — the game's own convention. Purely local:
/// no daemon, no network, no credentials, so a restore already downloaded
/// finishes even with rclone gone and cloud sync disabled. Returns 1 when a
/// stage was applied, 0 when nothing is staged (cheap; call it
/// unconditionally at startup), -1 on a hard error with the reason in
/// `bk_cloudsync_last_error`.
pub export fn bk_cloudsync_apply_pending_restore(profile: [*:0]const u8) callconv(.c) i32 {
    var dir_buffer: [1024]u8 = undefined;
    const profile_dir = std.fmt.bufPrint(&dir_buffer, "profiles/{s}", .{std.mem.span(profile)}) catch {
        setError("cloud sync: the profile path does not fit");
        return -1;
    };

    const outcome = backup.applyPendingRestore(module_gpa, credsIo(), profile_dir) catch |err| {
        switch (err) {
            error.StageCorrupt => setError("cloud sync: the staged restore is corrupt and was not applied"),
            error.ConfigUnwritable => setError("cloud sync: the restored config could not be written"),
            error.OutOfMemory => setError("cloud sync: out of memory applying the staged restore"),
        }
        return -1;
    };
    clearError();
    return switch (outcome) {
        .applied => 1,
        .nothing_staged => 0,
    };
}

/// Undo the restore state for `profile`: cancel a staged-but-unapplied
/// restore, or stage the `LATEST_UNDO` snapshot back as a full restore for
/// the next startup. Runs as a job — the worker's one-at-a-time discipline
/// is the operation slot, so this returns -1 busy while a restore download
/// is in flight rather than racing it over `ACTIVE`. Outcome `undo_done`
/// (7) on done.
pub export fn bk_cloudsync_restore_undo(
    game_dir: [*:0]const u8,
    profile: [*:0]const u8,
) callconv(.c) i32 {
    var dir_buffer: [1024]u8 = undefined;
    const profile_dir = std.fmt.bufPrint(&dir_buffer, "{s}/profiles/{s}", .{
        std.mem.span(game_dir),
        std.mem.span(profile),
    }) catch {
        setError("cloud sync: the profile path does not fit");
        return -1;
    };

    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    return enqueueLocked(std.mem.span(game_dir), .{
        .kind = .restore_undo,
        .path1 = profile_dir,
        .remote = creds.sync_remote_name,
        .profile = std.mem.span(profile),
        .profile_id = "",
        .remote_fingerprint = "",
    });
}

/// What undo would do for `profile` (resolved as `profiles/<name>` against
/// the working directory): 0 nothing, 1 a staged restore can be cancelled,
/// 2 an applied restore can be reinstated, 3 busy — a job holds the
/// operation slot, and answering "available" now is what would let a stale
/// undo race the finishing download.
pub export fn bk_cloudsync_restore_undo_available(profile: [*:0]const u8) callconv(.c) u32 {
    {
        jobs_mutex.lockUncancelable(lockIo());
        defer jobs_mutex.unlock(lockIo());
        if (sync_worker) |w| {
            switch (w.poll().state) {
                .starting, .pairing, .syncing, .testing => return 3,
                .idle, .done, .failed => {},
            }
        }
    }

    var dir_buffer: [1024]u8 = undefined;
    const profile_dir = std.fmt.bufPrint(&dir_buffer, "profiles/{s}", .{std.mem.span(profile)}) catch
        return 0;
    const availability = backup.restoreUndoAvailability(module_gpa, credsIo(), profile_dir) catch
        return 0;
    return switch (availability) {
        .none => 0,
        .cancellable => 1,
        .reinstatable => 2,
    };
}

/// Invalidate the handle. Shared state — the worker, the daemon, the rc
/// client — is untouched; `bk_cloudsync_shutdown` owns that.
pub export fn bk_cloudsync_release(handle: i32) callconv(.c) void {
    jobs_mutex.lockUncancelable(lockIo());
    defer jobs_mutex.unlock(lockIo());

    const slot = slotAt(handle) orelse return;
    slot.* = .{};
}

// ---------------------------------------------------------------------------
// Tests
//
// Nothing here touches `module` or the real resolver: a test that shelled out
// to whatever rclone the machine happens to have would pass or fail on the
// machine rather than on the code. The C++ smoke consumer covers the real
// path, because proving the exports work from C++ is the one thing these
// cannot do.
//
// Nothing here prints, either. The Zig 0.16 build runner fails a test step
// whose binary writes anything at all to stderr, even with every test passing.
// ---------------------------------------------------------------------------

const testing = std.testing;

/// A probe the test opens and closes by hand. `entered` rises when the probe
/// starts, which is also the proof that its refresh has taken its ticket;
/// `release` lets it finish. No sleeps and no timing assumptions: every
/// ordering below is imposed, not hoped for.
const Gate = struct {
    entered: std.atomic.Value(bool) = .init(false),
    release: std.atomic.Value(bool) = .init(false),
    /// What this probe answers with, duplicated into the caller's allocator.
    path: []const u8 = "",
    /// When set, the probe fails instead of answering.
    fails: bool = false,

    fn open(self: *Gate) void {
        self.release.store(true, .release);
    }

    fn awaitEntry(self: *Gate) void {
        while (!self.entered.load(.acquire)) std.Thread.yield() catch {};
    }
};

var gates: [8]Gate = @splat(.{});
var gate_cursor: std.atomic.Value(u32) = .init(0);
var probe_count: std.atomic.Value(u32) = .init(0);

fn resetGates() void {
    gates = @splat(.{});
    gate_cursor.store(0, .release);
    probe_count.store(0, .release);
}

/// Each call takes the next gate in order, so a test that starts one refresh,
/// waits for it to enter, and only then starts the next knows exactly which
/// gate belongs to which generation.
fn gatedResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = search;
    const index = gate_cursor.fetchAdd(1, .monotonic);
    const gate = &gates[index];
    _ = probe_count.fetchAdd(1, .monotonic);
    gate.entered.store(true, .release);
    while (!gate.release.load(.acquire)) std.Thread.yield() catch {};
    if (gate.fails) return error.OutOfMemory;
    return .{ .ready = .{
        .path = try gpa.dupe(u8, gate.path),
        .version = .{ .major = 1, .minor = 75, .patch = 0 },
    } };
}

/// A gate that is open before anyone reaches it, for the cases that care about
/// the result rather than the ordering.
fn openGate(index: usize, path: []const u8) void {
    gates[index].path = path;
    gates[index].open();
}

fn cachedPath(discovery: *Discovery) ?[]const u8 {
    discovery.lock();
    defer discovery.unlock();
    const current = discovery.value orelse return null;
    return switch (current) {
        .ready => |ready| ready.path,
        .unavailable => |rejected| rejected.path,
    };
}

fn expectCachedPath(discovery: *Discovery, expected: []const u8) !void {
    const actual = cachedPath(discovery) orelse return error.TestExpectedCachedValue;
    try testing.expectEqualStrings(expected, actual);
}

fn refreshThread(discovery: *Discovery, result: *anyerror!void) void {
    result.* = discovery.refresh();
}

fn fixedResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = search;
    _ = probe_count.fetchAdd(1, .monotonic);
    return .{ .ready = .{
        .path = try gpa.dupe(u8, "/fixed/rclone"),
        .version = .{ .major = 1, .minor = 75, .patch = 0 },
    } };
}

fn rejectingResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = search;
    return .{ .unavailable = .{
        .reason = .too_old,
        .path = try gpa.dupe(u8, "/old/rclone"),
        .version = .{ .major = 1, .minor = 60, .patch = 2 },
    } };
}

fn missingResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = gpa;
    _ = search;
    return .{ .unavailable = .{ .reason = .not_found, .path = null, .version = null } };
}

fn unreadableResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    _ = search;
    return .{ .unavailable = .{
        .reason = .not_executable,
        .path = try gpa.dupe(u8, "/bin/rclone"),
        .version = null,
    } };
}

fn statusOf(discovery: *Discovery, buffer: []u8) ![]const u8 {
    const length = try discovery.writeStatus(buffer);
    try testing.expectEqual(@as(u8, 0), buffer[length]);
    return buffer[0..length];
}

test "a ready discovery serialises every field the settings screen needs" {
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = fixedResolve };
    defer discovery.clear();
    try testing.expect(discovery.available() == false); // nothing cached yet
    try discovery.ensure();

    var buffer: [512]u8 = undefined;
    try testing.expectEqualStrings(
        \\{"found":true,"path":"/fixed/rclone","version":"1.75.0","reason":null}
    , try statusOf(&discovery, &buffer));
    try testing.expect(discovery.available());
}

test "each rejection carries its typed reason rather than free text" {
    var buffer: [512]u8 = undefined;

    var too_old: Discovery = .{ .gpa = testing.allocator, .resolver = rejectingResolve };
    defer too_old.clear();
    try too_old.ensure();
    try testing.expectEqualStrings(
        \\{"found":false,"path":"/old/rclone","version":"1.60.2","reason":"too_old"}
    , try statusOf(&too_old, &buffer));
    try testing.expect(!too_old.available());

    var missing: Discovery = .{ .gpa = testing.allocator, .resolver = missingResolve };
    defer missing.clear();
    try missing.ensure();
    try testing.expectEqualStrings(
        \\{"found":false,"path":null,"version":null,"reason":"not_found"}
    , try statusOf(&missing, &buffer));

    var unreadable: Discovery = .{ .gpa = testing.allocator, .resolver = unreadableResolve };
    defer unreadable.clear();
    try unreadable.ensure();
    try testing.expectEqualStrings(
        \\{"found":false,"path":"/bin/rclone","version":null,"reason":"not_executable"}
    , try statusOf(&unreadable, &buffer));
}

test "a buffer that cannot hold the document fails instead of truncating" {
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = fixedResolve };
    defer discovery.clear();
    try discovery.ensure();

    var tiny: [16]u8 = undefined;
    try testing.expectError(error.NoSpaceLeft, discovery.writeStatus(&tiny));
    var empty: [0]u8 = undefined;
    try testing.expectError(error.NoSpaceLeft, discovery.writeStatus(&empty));

    // The document plus its terminator is the exact requirement, not one byte
    // more: a buffer of that size must succeed.
    var measured: [512]u8 = undefined;
    const length = try discovery.writeStatus(&measured);
    const exact = try testing.allocator.alloc(u8, length + 1);
    defer testing.allocator.free(exact);
    try testing.expectEqual(length, try discovery.writeStatus(exact));
    const one_short = try testing.allocator.alloc(u8, length);
    defer testing.allocator.free(one_short);
    try testing.expectError(error.NoSpaceLeft, discovery.writeStatus(one_short));
}

test "discovery runs once and both readers see the same result" {
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = fixedResolve };
    defer discovery.clear();
    probe_count.store(0, .release);

    var buffer: [512]u8 = undefined;
    try testing.expect(bkAvailableOf(&discovery));
    _ = try statusOf(&discovery, &buffer);
    try testing.expect(bkAvailableOf(&discovery));
    try testing.expectEqual(@as(u32, 1), probe_count.load(.acquire));

    try discovery.refresh();
    try testing.expectEqual(@as(u32, 2), probe_count.load(.acquire));
}

/// `available` with the lazy discovery the export performs, so the test above
/// measures the same thing `bk_cloudsync_available` does.
fn bkAvailableOf(discovery: *Discovery) bool {
    discovery.ensure() catch return false;
    return discovery.available();
}

test "shutdown drops the cache and the next call rediscovers" {
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = fixedResolve };
    defer discovery.clear();
    try discovery.ensure();
    try expectCachedPath(&discovery, "/fixed/rclone");

    discovery.clear();
    try testing.expect(cachedPath(&discovery) == null);
    var buffer: [512]u8 = undefined;
    try testing.expectError(error.NotDiscovered, discovery.writeStatus(&buffer));
    try testing.expect(!discovery.available());

    try testing.expect(bkAvailableOf(&discovery));
    try expectCachedPath(&discovery, "/fixed/rclone");
}

test "a failing refresh keeps the previous value rather than clearing it" {
    resetGates();
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = gatedResolve };
    defer discovery.clear();

    openGate(0, "/first/rclone");
    try discovery.ensure();
    try expectCachedPath(&discovery, "/first/rclone");

    gates[1].fails = true;
    gates[1].open();
    try testing.expectError(error.OutOfMemory, discovery.refresh());
    try expectCachedPath(&discovery, "/first/rclone");
}

test "an older refresh that finishes last never reaches the cache" {
    resetGates();
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = gatedResolve };
    defer discovery.clear();

    openGate(0, "/base/rclone");
    try discovery.ensure();
    try expectCachedPath(&discovery, "/base/rclone");

    // Generation 2 starts first and is still probing.
    gates[1].path = "/older/rclone";
    var older_result: anyerror!void = {};
    const older = try std.Thread.spawn(.{}, refreshThread, .{ &discovery, &older_result });
    gates[1].awaitEntry();

    // Generation 3 starts second, finishes first, and publishes.
    gates[2].path = "/newer/rclone";
    var newer_result: anyerror!void = {};
    const newer = try std.Thread.spawn(.{}, refreshThread, .{ &discovery, &newer_result });
    gates[2].awaitEntry();
    gates[2].open();
    newer.join();
    try newer_result;

    // Inspected while the older probe is still in flight — it is blocked on
    // its gate and cannot have finished — the cache must already hold the
    // newer answer and must never hold the older one.
    try testing.expect(!gates[1].release.load(.acquire));
    try expectCachedPath(&discovery, "/newer/rclone");

    gates[1].open();
    older.join();
    // Superseded, so it reports success to its own caller without publishing.
    try older_result;
    try expectCachedPath(&discovery, "/newer/rclone");
}

test "an older refresh that finishes first is discarded, not published" {
    // This is the case `my_gen > published_gen` gets wrong: with generations 2
    // and 3 both in flight and nothing published since, generation 2 would
    // satisfy that test and install its result while 3 is still probing.
    resetGates();
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = gatedResolve };
    defer discovery.clear();

    openGate(0, "/base/rclone");
    try discovery.ensure();

    gates[1].path = "/older/rclone";
    var older_result: anyerror!void = {};
    const older = try std.Thread.spawn(.{}, refreshThread, .{ &discovery, &older_result });
    gates[1].awaitEntry();

    gates[2].path = "/newer/rclone";
    var newer_result: anyerror!void = {};
    const newer = try std.Thread.spawn(.{}, refreshThread, .{ &discovery, &newer_result });
    gates[2].awaitEntry();

    gates[1].open();
    older.join();
    try older_result;

    // The newer probe is demonstrably still running — its gate is shut — and
    // the older one has completed. The cache must still hold the baseline: an
    // answer that has been superseded is discarded, not written.
    try testing.expect(!gates[2].release.load(.acquire));
    try expectCachedPath(&discovery, "/base/rclone");

    gates[2].open();
    newer.join();
    try newer_result;
    try expectCachedPath(&discovery, "/newer/rclone");
}

/// A resolver that honours the explicit override the way the real search
/// does — precedence itself is pinned by `explicit_path_wins_over_path_entry`
/// in the daemon suite; what this proves is the *plumbing*: a saved override
/// reaches the search, the refresh publishes, and both readers flip.
fn searchSensitiveResolve(gpa: Allocator, search: daemon.Search) Allocator.Error!daemon.Availability {
    if (search.explicit) |explicit| {
        if (explicit.len != 0) return .{ .ready = .{
            .path = try gpa.dupe(u8, explicit),
            .version = .{ .major = 1, .minor = 75, .patch = 0 },
        } };
    }
    return .{ .unavailable = .{ .reason = .not_found, .path = null, .version = null } };
}

test "a saved rclone override flips availability without a restart" {
    var discovery: Discovery = .{ .gpa = testing.allocator, .resolver = searchSensitiveResolve };
    defer discovery.clear();

    // Before: nothing on the machine. Both readers agree on "not found".
    try discovery.ensure();
    try testing.expect(!discovery.available());
    var buffer: [512]u8 = undefined;
    var length = try discovery.writeStatus(&buffer);
    try testing.expect(std.mem.indexOf(u8, buffer[0..length], "\"reason\":\"not_found\"") != null);

    // The dialog saves credentials carrying a valid rclone_path; the save
    // path installs the override and refreshes — no restart anywhere.
    try discovery.setExplicit("/opt/rclone/rclone");
    try discovery.refresh();

    try testing.expect(discovery.available());
    length = try discovery.writeStatus(&buffer);
    try testing.expect(std.mem.indexOf(u8, buffer[0..length], "\"path\":\"/opt/rclone/rclone\"") != null);
    try testing.expect(std.mem.indexOf(u8, buffer[0..length], "\"found\":true") != null);

    // And back: clearing the override re-finds nothing. `setExplicit`
    // deliberately never frees the previous override (see its doc comment),
    // so this test frees the superseded copy by hand to keep the testing
    // allocator's leak check meaningful.
    const superseded = discovery.search.explicit;
    try discovery.setExplicit(null);
    try discovery.refresh();
    try testing.expect(!discovery.available());
    if (superseded) |allocation| testing.allocator.free(@constCast(allocation));
}

const StressState = struct {
    discovery: *Discovery,
    stop: std.atomic.Value(bool) = .init(false),
    failures: std.atomic.Value(u32) = .init(0),
};

fn stressReader(state: *StressState) void {
    var buffer: [512]u8 = undefined;
    while (!state.stop.load(.acquire)) {
        if (!state.discovery.available()) {
            _ = state.failures.fetchAdd(1, .monotonic);
            continue;
        }
        const length = state.discovery.writeStatus(&buffer) catch {
            _ = state.failures.fetchAdd(1, .monotonic);
            continue;
        };
        // The bytes were copied out under the lock, so they stay readable no
        // matter how many times the cache has been replaced since. Every
        // refresh here answers identically, so anything but the expected
        // document means a read saw memory a refresh had already freed.
        const document = buffer[0..length];
        if (std.mem.indexOf(u8, document, "\"path\":\"/fixed/rclone\"") == null) {
            _ = state.failures.fetchAdd(1, .monotonic);
        }
    }
}

fn stressRefresher(state: *StressState) void {
    var remaining: u32 = 200;
    while (remaining > 0) : (remaining -= 1) {
        state.discovery.refresh() catch {
            _ = state.failures.fetchAdd(1, .monotonic);
            return;
        };
    }
    state.stop.store(true, .release);
}

test "concurrent readers survive a refresher freeing the value underneath them" {
    if (builtin.single_threaded) return;

    // The debug allocator turns a freed-while-read into a failure rather than
    // into a flake: it poisons freed memory and reports a leak or a double
    // free outright, where a general-purpose allocator would usually hand the
    // same bytes back and let the bug pass.
    var debug: std.heap.DebugAllocator(.{ .safety = true, .thread_safe = true }) = .init;
    defer testing.expect(debug.deinit() == .ok) catch @panic("cloudsync discovery cache leaked");

    var discovery: Discovery = .{ .gpa = debug.allocator(), .resolver = fixedResolve };
    defer discovery.clear();
    try discovery.ensure();

    var state: StressState = .{ .discovery = &discovery };
    var readers: [4]std.Thread = undefined;
    for (&readers) |*reader| reader.* = try std.Thread.spawn(.{}, stressReader, .{&state});
    const refresher = try std.Thread.spawn(.{}, stressRefresher, .{&state});

    refresher.join();
    state.stop.store(true, .release);
    for (readers) |reader| reader.join();
    try testing.expectEqual(@as(u32, 0), state.failures.load(.acquire));
}
