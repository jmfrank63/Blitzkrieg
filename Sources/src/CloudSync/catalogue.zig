//! rclone's provider catalogue: fetch it, cache it, and answer questions
//! about it.
//!
//! `config/providers` is rclone's own machine-readable description of every
//! backend it can configure — each option carrying its name, type, help,
//! flags, default, examples and the vendor expression that decides whether it
//! applies at all. That document is the reason no provider name, field name,
//! vendor list or default is written anywhere in this project: the form is
//! rendered from the data, so a newer rclone brings new providers with no
//! game change. Anything hardcoded here would be a defect the next rclone
//! release exposes.
//!
//! The eighteen option fields modelled below are the **union across all
//! backends**, not the shape of any one record. `Provider` in particular is
//! absent from s3's first option and present on 35 options overall, which is
//! precisely how it gets missed by sampling one entry. `Groups`, which
//! appears in rclone's general option-block documentation, is not present on
//! any backend option record and is therefore not modelled — it is tolerated
//! as an unknown field if a later version adds it.
//!
//! Two tolerances are load-bearing, because this file parses documents
//! written by a binary that will be upgraded underneath it:
//!
//! - **Unknown fields are ignored**, at every level. A newer rclone adding a
//!   key must not make the catalogue unreadable.
//! - **Unknown types fall back to text.** `Kind` classifies only what changes
//!   the widget; everything else — including type names that do not exist
//!   yet — renders as a text field and never fails the parse.
//!
//! `matchProvider` lives here as its single owner. Three consumers need the
//! rule (the vendor-change cleanup, the form filter, and required-field
//! validation), and three implementations of one matching rule would drift.
//!
//! The cache is `<gamedir>/cloudsync/providers.json`: rclone's reply with our
//! version stamp added, written temp-file-then-rename so a crash mid-write
//! leaves either the old document or the new one. **A missing or unreadable
//! cache is an empty list, never an error** — no cache is a state the dialog
//! already handles, and it must never block the settings screen.
//!
//! Nothing here spawns a daemon or waits on a socket by itself: `fetch` takes
//! an `rc.Client` somebody else made ready. Fetching is a worker job
//! (`worker.zig`), because starting a daemon and making an rc call on the
//! calling thread is exactly the block the worker exists to prevent.

const std = @import("std");
const daemon = @import("daemon.zig");
const rc = @import("rc.zig");

const Allocator = std.mem.Allocator;
const Io = std.Io;
const path = Io.Dir.path;

/// Where the cache lives, relative to the game directory. A sibling of
/// `rclone.conf`, in the directory the daemon already owns.
pub const cache_rel_path = "cloudsync/providers.json";

/// The stamp key our cache adds to rclone's reply. Chosen to be a key rclone
/// itself will never emit at the document root.
pub const version_key = "rclone_version";

/// Upper bound on a catalogue document. v1.75.0's full 69-backend reply is
/// about 776 KiB; eight mebibytes leaves a decade of growth and still refuses
/// to read something that is no longer a catalogue. Exceeding it is a miss,
/// which is an empty list — never a hang and never a partial parse.
pub const max_document_bytes: usize = 8 << 20;

/// rclone's option visibility constants. `Hide` is a bitmask, not a boolean:
/// treating any non-zero value as hidden wrongly drops options that are
/// merely hidden from the command line.
pub const hide_command_line: u32 = 1;
pub const hide_configurator: u32 = 2;

/// True when the configurator bit is set, which is the only bit that decides
/// whether a form shows the option.
pub fn hiddenFromConfiguratorMask(hide: u32) bool {
    return hide & hide_configurator != 0;
}

/// How an option's `Type` renders. Only the distinctions that change the
/// widget are named; everything else, including type names a later rclone
/// invents, is `.text`. That fallback is what keeps an unknown type from
/// failing a document.
pub const Kind = enum { text, boolean, integer, number };

/// Classify a raw `Type` string. Unrecognised means text, always.
pub fn classify(type_name: []const u8) Kind {
    if (std.mem.eql(u8, type_name, "bool")) return .boolean;
    if (std.mem.eql(u8, type_name, "int")) return .integer;
    if (std.mem.eql(u8, type_name, "float64")) return .number;
    return .text;
}

/// One suggested value for an option. `provider` carries the same expression
/// grammar as `Option.provider`: filtering reaches values, not only fields,
/// because an option can stay applicable while its examples change.
pub const Example = struct {
    value: []const u8 = "",
    help: []const u8 = "",
    provider: []const u8 = "",

    pub fn appliesTo(self: Example, selected: []const u8) bool {
        return matchProvider(self.provider, selected);
    }
};

/// One configurable option, carrying every field the backend catalogue
/// provides. Absent fields take their neutral value rather than making the
/// record unreadable.
pub const Option = struct {
    name: []const u8 = "",
    /// rclone's override for the environment-variable/config field name;
    /// empty means "derive it from `name`", which is rclone's own default.
    field_name: []const u8 = "",
    help: []const u8 = "",
    /// The raw `Type` string, preserved beside `kind` so nothing is lost to
    /// a classification this build does not know how to make.
    type_name: []const u8 = "",
    kind: Kind = .text,
    required: bool = false,
    advanced: bool = false,
    is_password: bool = false,
    sensitive: bool = false,
    no_prefix: bool = false,
    /// True when the examples are the only permitted values — a closed
    /// droplist rather than an editable one.
    exclusive: bool = false,
    /// A bitmask; see `hide_command_line` and `hide_configurator`.
    hide: u32 = 0,
    short_opt: []const u8 = "",
    /// The vendor expression deciding whether this option applies at all.
    provider: []const u8 = "",
    /// rclone's default, in whatever JSON type it has — string, bool, int,
    /// list or object. Never stored as a player value: a default that changes
    /// upstream must follow upstream.
    default: std.json.Value = .null,
    /// The human-readable rendering of `default`, which is what a placeholder
    /// shows.
    default_str: []const u8 = "",
    /// The option's current value and its rendering. Both are empty in a
    /// catalogue reply — the catalogue describes a backend, not a configured
    /// remote — but they are part of the record and are carried faithfully.
    value: std.json.Value = .null,
    value_str: []const u8 = "",
    examples: []const Example = &.{},

    /// True when a form for `selected` should show this option.
    pub fn appliesTo(self: Option, selected: []const u8) bool {
        return matchProvider(self.provider, selected);
    }

    pub fn hiddenFromConfigurator(self: Option) bool {
        return hiddenFromConfiguratorMask(self.hide);
    }

    /// Both flags that make a field a secret. `sensitive` and `is_password`
    /// are separate questions downstream — one decides what a load withholds,
    /// the other what a token read-back must not overwrite — so this is only
    /// the "withhold it" test.
    pub fn isSecret(self: Option) bool {
        return self.is_password or self.sensitive;
    }
};

/// One backend. Unknown backend-level keys (`Aliases`, `CommandHelp`,
/// `MetadataInfo`, `Overview`) are ignored: they describe rclone's CLI
/// surface, not a configuration form.
pub const Backend = struct {
    name: []const u8 = "",
    description: []const u8 = "",
    /// The config-file prefix rclone uses for this backend's options.
    prefix: []const u8 = "",
    /// rclone's own "do not offer this" flag. Distinct from the candidate
    /// filter P01-M04 owns, which is about wrappers and non-destinations.
    hidden: bool = false,
    options: []const Option = &.{},

    pub fn option(self: *const Backend, name: []const u8) ?*const Option {
        for (self.options) |*candidate| {
            if (std.mem.eql(u8, candidate.name, name)) return candidate;
        }
        return null;
    }
};

/// A parsed catalogue and the arena its strings live in. Every slice inside
/// points into that arena, including the JSON values, so `deinit` frees the
/// whole document at once.
pub const Catalogue = struct {
    arena: std.heap.ArenaAllocator,
    /// The rclone that produced this document, when it is one of ours. A
    /// reply straight off the wire has no stamp yet, and neither does an
    /// empty catalogue standing in for a missing cache.
    rclone_version: ?daemon.Version = null,
    backends: []const Backend = &.{},

    pub fn deinit(self: *Catalogue) void {
        self.arena.deinit();
        self.* = undefined;
    }

    pub fn backend(self: *const Catalogue, name: []const u8) ?*const Backend {
        for (self.backends) |*candidate| {
            if (std.mem.eql(u8, candidate.name, name)) return candidate;
        }
        return null;
    }

    pub fn isEmpty(self: *const Catalogue) bool {
        return self.backends.len == 0;
    }
};

pub const ParseError = Allocator.Error || error{BadJson};

/// An empty catalogue: what a missing cache is, and what every caller must be
/// able to render.
pub fn empty(gpa: Allocator) Catalogue {
    return .{ .arena = .init(gpa) };
}

/// Parse a catalogue document — rclone's `config/providers` reply, or one of
/// our stamped cache files, which differ only by the stamp.
///
/// The text is copied into the arena before parsing, so every slice the
/// result hands out outlives the caller's buffer regardless of how the JSON
/// parser chose to allocate.
pub fn parse(gpa: Allocator, text: []const u8) ParseError!Catalogue {
    var arena: std.heap.ArenaAllocator = .init(gpa);
    errdefer arena.deinit();
    const alloc = arena.allocator();

    const owned = try alloc.dupe(u8, text);
    const document = std.json.parseFromSliceLeaky(std.json.Value, alloc, owned, .{}) catch
        return error.BadJson;

    const root = switch (document) {
        .object => |object| object,
        else => return error.BadJson,
    };

    const providers = switch (root.get("providers") orelse return error.BadJson) {
        .array => |array| array,
        else => return error.BadJson,
    };

    const backends = try alloc.alloc(Backend, providers.items.len);
    for (providers.items, 0..) |entry, index| {
        backends[index] = try parseBackend(alloc, entry);
    }

    return .{
        .arena = arena,
        .rclone_version = readStamp(root),
        .backends = backends,
    };
}

fn readStamp(root: std.json.ObjectMap) ?daemon.Version {
    const stamped = root.get(version_key) orelse return null;
    const text = switch (stamped) {
        .string => |s| s,
        else => return null,
    };
    // `parseVersion` wants rclone's own `vMAJOR.MINOR.PATCH` shape, which is
    // what `stampDocument` writes.
    return daemon.parseVersion(text);
}

fn parseBackend(alloc: Allocator, value: std.json.Value) Allocator.Error!Backend {
    const object = switch (value) {
        .object => |o| o,
        // A malformed entry is skipped rather than fatal: one bad backend in
        // a newer reply must not cost the player every other one.
        else => return .{},
    };

    var backend: Backend = .{
        .name = stringField(object, "Name"),
        .description = stringField(object, "Description"),
        .prefix = stringField(object, "Prefix"),
        .hidden = boolField(object, "Hide"),
    };

    if (object.get("Options")) |raw| {
        if (raw == .array) {
            const items = raw.array.items;
            const options = try alloc.alloc(Option, items.len);
            for (items, 0..) |item, index| {
                options[index] = try parseOption(alloc, item);
            }
            backend.options = options;
        }
    }
    return backend;
}

fn parseOption(alloc: Allocator, value: std.json.Value) Allocator.Error!Option {
    const object = switch (value) {
        .object => |o| o,
        else => return .{},
    };

    const type_name = stringField(object, "Type");
    var option: Option = .{
        .name = stringField(object, "Name"),
        .field_name = stringField(object, "FieldName"),
        .help = stringField(object, "Help"),
        .type_name = type_name,
        .kind = classify(type_name),
        .required = boolField(object, "Required"),
        .advanced = boolField(object, "Advanced"),
        .is_password = boolField(object, "IsPassword"),
        .sensitive = boolField(object, "Sensitive"),
        .no_prefix = boolField(object, "NoPrefix"),
        .exclusive = boolField(object, "Exclusive"),
        .hide = maskField(object, "Hide"),
        .short_opt = stringField(object, "ShortOpt"),
        .provider = stringField(object, "Provider"),
        .default = object.get("Default") orelse .null,
        .default_str = stringField(object, "DefaultStr"),
        .value = object.get("Value") orelse .null,
        .value_str = stringField(object, "ValueStr"),
    };

    if (object.get("Examples")) |raw| {
        if (raw == .array) {
            const items = raw.array.items;
            const examples = try alloc.alloc(Example, items.len);
            for (items, 0..) |item, index| {
                examples[index] = parseExample(item);
            }
            option.examples = examples;
        }
    }
    return option;
}

fn parseExample(value: std.json.Value) Example {
    const object = switch (value) {
        .object => |o| o,
        else => return .{},
    };
    return .{
        .value = stringField(object, "Value"),
        .help = stringField(object, "Help"),
        .provider = stringField(object, "Provider"),
    };
}

/// A missing or wrongly-typed field reads as empty rather than as a failure —
/// the unknown-field rule from the other direction.
fn stringField(object: std.json.ObjectMap, name: []const u8) []const u8 {
    const value = object.get(name) orelse return "";
    return switch (value) {
        .string => |s| s,
        else => "",
    };
}

fn boolField(object: std.json.ObjectMap, name: []const u8) bool {
    const value = object.get(name) orelse return false;
    return switch (value) {
        .bool => |b| b,
        else => false,
    };
}

fn maskField(object: std.json.ObjectMap, name: []const u8) u32 {
    const value = object.get(name) orelse return 0;
    return switch (value) {
        .integer => |i| if (i > 0 and i < std.math.maxInt(u32)) @intCast(i) else 0,
        // A later rclone that turns `Hide` into a boolean would mean "hidden
        // everywhere", which is both bits.
        .bool => |b| if (b) hide_command_line | hide_configurator else 0,
        else => 0,
    };
}

// -- Provider matching -------------------------------------------------------

/// rclone's own `MatchProvider`, and the single implementation of it here.
///
/// An empty expression matches everything, and so does an empty selection:
/// that second case is what makes a backend with no vendor chosen yet show
/// all of its conditional fields rather than none of them. Otherwise a
/// leading `!` negates, and the rest is membership in a comma-separated list
/// compared element by element — never as a substring, or a vendor whose name
/// is a prefix of another would borrow its fields.
pub fn matchProvider(expression: []const u8, selected: []const u8) bool {
    if (expression.len == 0 or selected.len == 0) return true;

    var list = expression;
    var negate = false;
    if (list[0] == '!') {
        negate = true;
        list = list[1..];
    }

    var members = std.mem.splitScalar(u8, list, ',');
    while (members.next()) |member| {
        if (std.mem.eql(u8, member, selected)) return !negate;
    }
    return negate;
}

// -- Candidates and the offered list -----------------------------------------

/// The backends that are not cloud destinations, and therefore never
/// offered: each either wraps another remote (`alias`, `cache`, `chunker`,
/// `combine`, `compress`, `crypt`, `hasher`, `union`), writes archives over
/// one (`archive`), or is not remote storage at all (`local`, `memory`).
/// The catalogue has no field that states this, so the list is one of the
/// plan's declared exceptions to "no provider names in source" — kept here,
/// in exactly one place, verified against `config/providers` on v1.75.0.
/// `overview` is deliberately absent: it appears in lists scraped from
/// rclone's documentation but is not a backend, and excluding it would be
/// encoding the scrape's mistake.
const non_candidate_backends = [_][]const u8{
    "alias",  "archive",  "cache",  "chunker", "combine", "compress",
    "crypt",  "hasher",   "local",  "memory",  "union",
};

/// True when a backend may be offered as a sync destination. This is a
/// *candidate* filter, not a verification: nothing in the catalogue says
/// whether a backend supports writable bisync, so the writable connection
/// test is what turns a candidate into a usable destination — a backend
/// that lists but cannot write must fail there with a clear reason, not at
/// the first sync.
pub fn isCandidate(name: []const u8) bool {
    for (non_candidate_backends) |excluded| {
        if (std.mem.eql(u8, name, excluded)) return false;
    }
    return true;
}

/// The backend names a selection UI should offer: candidates that rclone
/// itself does not hide, sorted alphabetically — catalogue order is not a
/// menu — plus `configured`, the backend already saved in the profile's
/// credentials, whatever the filter or a newer rclone thinks of it: a
/// working configuration must not vanish because of our list. Pass an
/// empty string when nothing is configured.
///
/// The returned slice is the caller's to free; the names inside borrow
/// from the catalogue (and from `configured`), so both must outlive the
/// list.
pub fn offeredBackends(
    gpa: Allocator,
    cat: *const Catalogue,
    configured: []const u8,
) Allocator.Error![][]const u8 {
    var names = try std.ArrayList([]const u8).initCapacity(gpa, cat.backends.len + 1);
    errdefer names.deinit(gpa);

    for (cat.backends) |backend| {
        if (backend.name.len == 0) continue;
        if (backend.hidden) continue;
        if (!isCandidate(backend.name)) continue;
        if (std.mem.eql(u8, backend.name, configured)) continue; // added below, once
        try names.append(gpa, backend.name);
    }
    if (configured.len != 0) try names.append(gpa, configured);

    std.mem.sort([]const u8, names.items, {}, struct {
        fn lessThan(_: void, lhs: []const u8, rhs: []const u8) bool {
            return std.mem.lessThan(u8, lhs, rhs);
        }
    }.lessThan);
    return names.toOwnedSlice(gpa);
}

// -- Fetch -------------------------------------------------------------------

pub const FetchError = Allocator.Error || error{ RcCallFailed, BadCatalogue, VersionUnreadable };

/// A fetched catalogue and the exact bytes `cache` would write for it.
/// Keeping the document beside the parse avoids serialising the whole
/// catalogue back out again, and guarantees what is cached is what was read.
pub const Fetched = struct {
    catalogue: Catalogue,
    /// Owned by the allocator passed to `fetch`.
    document: []u8,

    pub fn deinit(self: *Fetched, gpa: Allocator) void {
        self.catalogue.deinit();
        gpa.free(self.document);
        self.* = undefined;
    }
};

/// Ask the running daemon what version it is. The stamp comes from the daemon
/// rather than from discovery so that the cached document records the binary
/// that actually produced it.
pub fn runningVersion(client: *rc.Client) FetchError!daemon.Version {
    var reply = client.call("core/version", .null) catch return error.RcCallFailed;
    defer reply.deinit();

    const object = switch (reply.value) {
        .object => |o| o,
        else => return error.VersionUnreadable,
    };
    const raw = object.get("version") orelse return error.VersionUnreadable;
    const text = switch (raw) {
        .string => |s| s,
        else => return error.VersionUnreadable,
    };
    return daemon.parseVersion(text) orelse return error.VersionUnreadable;
}

/// `config/providers` plus `core/version`, parsed and stamped. Blocking, like
/// every other rc call: only the worker thread may call this.
pub fn fetch(gpa: Allocator, client: *rc.Client) FetchError!Fetched {
    return fetchAt(gpa, client, try runningVersion(client));
}

/// `fetch` for a caller that has already asked the daemon its version, so the
/// refresh path makes two rc calls rather than three.
fn fetchAt(gpa: Allocator, client: *rc.Client, version: daemon.Version) FetchError!Fetched {
    var reply = client.call("config/providers", .{ .object = .empty }) catch
        return error.RcCallFailed;
    defer reply.deinit();

    const body = std.json.Stringify.valueAlloc(gpa, reply.value, .{}) catch
        return error.OutOfMemory;
    defer gpa.free(body);

    const document = try stampDocument(gpa, body, version);
    errdefer gpa.free(document);

    var parsed = parse(gpa, document) catch |err| switch (err) {
        error.OutOfMemory => return error.OutOfMemory,
        error.BadJson => return error.BadCatalogue,
    };
    errdefer parsed.deinit();

    return .{ .catalogue = parsed, .document = document };
}

/// Rewrite a catalogue document with our version stamp at its root, textually
/// — the reply's `providers` array is copied through untouched, so nothing
/// rclone said is reinterpreted on the way to disk.
///
/// `body` is rclone's own reply, which never carries a stamp. Stamping an
/// already-stamped document would leave two of the key, and the one already
/// there would win the read.
pub fn stampDocument(
    gpa: Allocator,
    body: []const u8,
    version: daemon.Version,
) Allocator.Error![]u8 {
    const trimmed = std.mem.trim(u8, body, " \t\r\n");
    // Every catalogue document is a JSON object; anything else would have
    // failed `parse` already, and the stamp has nowhere to go.
    if (trimmed.len < 2 or trimmed[0] != '{' or trimmed[trimmed.len - 1] != '}') {
        return gpa.dupe(u8, body);
    }

    const inner = std.mem.trim(u8, trimmed[1 .. trimmed.len - 1], " \t\r\n");
    const separator: []const u8 = if (inner.len == 0) "" else ",";
    return std.fmt.allocPrint(
        gpa,
        "{{\"{s}\":\"v{d}.{d}.{d}\"{s}{s}}}",
        .{ version_key, version.major, version.minor, version.patch, separator, inner },
    );
}

// -- Cache -------------------------------------------------------------------

pub const CacheError = Allocator.Error || error{CatalogueUnwritable};

/// `<game_dir>/cloudsync/providers.json`.
pub fn cachePath(gpa: Allocator, game_dir: []const u8) Allocator.Error![]u8 {
    return path.join(gpa, &.{ game_dir, cache_rel_path });
}

/// Write `document` to the cache atomically: a temp file first, then a rename
/// over the target, so a crash mid-write leaves either the old catalogue or
/// the new one and never a truncated hybrid a reader would have to guess at.
pub fn cache(
    gpa: Allocator,
    io: Io,
    game_dir: []const u8,
    document: []const u8,
) CacheError!void {
    const final = try cachePath(gpa, game_dir);
    defer gpa.free(final);

    if (path.dirname(final)) |dir| {
        Io.Dir.cwd().createDirPath(io, dir) catch return error.CatalogueUnwritable;
    }

    const tmp = std.fmt.allocPrint(gpa, "{s}.tmp", .{final}) catch return error.OutOfMemory;
    defer gpa.free(tmp);

    Io.Dir.cwd().writeFile(io, .{
        .sub_path = tmp,
        .data = document,
        .flags = .{ .truncate = true },
    }) catch return error.CatalogueUnwritable;

    Io.Dir.rename(.cwd(), tmp, .cwd(), final, io) catch
        return error.CatalogueUnwritable;
}

/// Read the cached catalogue. **A missing, oversized, unreadable or malformed
/// cache is an empty list**, not an error: no catalogue yet is an ordinary
/// state on a fresh install, and the only thing that must never happen is a
/// blocked settings screen. Only allocation failure can fail here.
pub fn loadCached(gpa: Allocator, io: Io, game_dir: []const u8) Allocator.Error!Catalogue {
    const final = try cachePath(gpa, game_dir);
    defer gpa.free(final);

    const text = Io.Dir.cwd().readFileAlloc(io, final, gpa, .limited(max_document_bytes)) catch
        return empty(gpa);
    defer gpa.free(text);

    return parse(gpa, text) catch |err| switch (err) {
        error.OutOfMemory => error.OutOfMemory,
        error.BadJson => empty(gpa),
    };
}

/// The cached document's version stamp, without keeping the catalogue. The
/// only work a caller needs to decide whether a fetch is required.
pub fn cachedVersion(gpa: Allocator, io: Io, game_dir: []const u8) Allocator.Error!?daemon.Version {
    var cached = try loadCached(gpa, io, game_dir);
    defer cached.deinit();
    // An empty document with a stamp is still a stamped cache, but there is
    // nothing to render from it, so it is treated as a miss.
    if (cached.isEmpty()) return null;
    return cached.rclone_version;
}

/// True when a stamp matches the running binary. A missing stamp never
/// matches — an unstamped document cannot be attributed to any version.
pub fn matchesVersion(stamp: ?daemon.Version, running: daemon.Version) bool {
    const known = stamp orelse return false;
    return known.order(running) == .eq;
}

pub const Refresh = enum { unchanged, refreshed };

pub const RefreshError = FetchError || CacheError;

/// Bring the cache up to date against the daemon on the other end of
/// `client`, fetching only when there is something to fetch.
///
/// The cheap half runs first: one `core/version` round trip decides whether
/// the cached stamp still describes the running binary. That is what makes
/// the opportunistic refresh after a sync nearly free, and it is also the
/// mechanism by which a rclone upgrade invalidates the catalogue.
pub fn refreshCache(
    gpa: Allocator,
    io: Io,
    client: *rc.Client,
    game_dir: []const u8,
) RefreshError!Refresh {
    const running = try runningVersion(client);
    const stamp = try cachedVersion(gpa, io, game_dir);
    if (matchesVersion(stamp, running)) return .unchanged;

    var fetched = try fetchAt(gpa, client, running);
    defer fetched.deinit(gpa);

    try cache(gpa, io, game_dir, fetched.document);
    return .refreshed;
}
