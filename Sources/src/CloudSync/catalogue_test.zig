//! Tests for rclone's provider catalogue.
//!
//! Everything here runs offline against the committed fixture, which is a
//! snapshot of one rclone version's `config/providers` reply — evidence about
//! v1.75.0, never the truth about rclone. That distinction is why the operator
//! tests below assert on `matchProvider`'s *rules* rather than on the vendor
//! lists that happen to be in the snapshot: the lists change with every
//! release, the rules do not.
//!
//! The parser is held to two things the next release will exercise: an
//! unrecognised `Type` renders as text rather than failing the document, and
//! an unknown field anywhere is ignored rather than rejected. The fixture
//! already carries one such field (`_fixture`, its own provenance record), so
//! the tolerance is exercised by the real data as well as by the synthetic
//! case.
//!
//! Nothing writes to stderr: a build test step fails on any output at all, so
//! every finding here is an assertion.

const std = @import("std");
const catalogue = @import("catalogue.zig");
const daemon = @import("daemon.zig");

const io = std.testing.io;
const path = std.Io.Dir.path;

/// The committed snapshot, wired in by `build.zig` as an anonymous import.
const fixture_json = @embedFile("config_providers_fixture");

fn findOption(backend: *const catalogue.Backend, name: []const u8) ?*const catalogue.Option {
    for (backend.options) |*option| {
        if (std.mem.eql(u8, option.name, name)) return option;
    }
    return null;
}

// -- The fixture -------------------------------------------------------------

test "the fixture parses into backends carrying every catalogued option field" {
    const gpa = std.testing.allocator;

    var parsed = try catalogue.parse(gpa, fixture_json);
    defer parsed.deinit();

    // Five backends, kept whole: the trim removed backends, never options.
    try std.testing.expectEqual(@as(usize, 5), parsed.backends.len);

    const s3 = parsed.backend("s3") orelse return error.MissingBackend;
    const webdav = parsed.backend("webdav") orelse return error.MissingBackend;
    const sftp = parsed.backend("sftp") orelse return error.MissingBackend;
    const drive = parsed.backend("drive") orelse return error.MissingBackend;
    const dropbox = parsed.backend("dropbox") orelse return error.MissingBackend;

    // The shapes the design measured, so a parser that silently drops options
    // cannot pass.
    try std.testing.expectEqual(@as(usize, 78), s3.options.len);
    try std.testing.expectEqual(@as(usize, 15), webdav.options.len);
    try std.testing.expectEqual(@as(usize, 48), sftp.options.len);
    try std.testing.expectEqual(@as(usize, 52), drive.options.len);
    try std.testing.expectEqual(@as(usize, 24), dropbox.options.len);

    try std.testing.expect(s3.description.len != 0);
    try std.testing.expectEqualStrings("s3", s3.prefix);

    // A password-typed field, a sensitive one, an advanced one and a required
    // one all exist and are distinguishable — the four flags the widget
    // mapping turns into behaviour.
    const secret = findOption(s3, "secret_access_key") orelse return error.MissingOption;
    try std.testing.expect(secret.sensitive);
    try std.testing.expectEqualStrings("string", secret.type_name);
    try std.testing.expectEqual(catalogue.Kind.text, secret.kind);

    const pass = findOption(webdav, "pass") orelse return error.MissingOption;
    try std.testing.expect(pass.is_password);

    const url = findOption(webdav, "url") orelse return error.MissingOption;
    try std.testing.expect(url.required);
    try std.testing.expect(!url.advanced);

    // Non-string types survive as themselves, with the raw name preserved
    // beside the classification.
    const chunk = findOption(s3, "chunk_size") orelse return error.MissingOption;
    try std.testing.expectEqualStrings("SizeSuffix", chunk.type_name);
    try std.testing.expectEqual(catalogue.Kind.text, chunk.kind);
    try std.testing.expect(chunk.advanced);
    try std.testing.expect(chunk.default_str.len != 0);

    const v2_auth = findOption(s3, "v2_auth") orelse return error.MissingOption;
    try std.testing.expectEqualStrings("bool", v2_auth.type_name);
    try std.testing.expectEqual(catalogue.Kind.boolean, v2_auth.kind);
    try std.testing.expect(v2_auth.default == .bool and v2_auth.default.bool == false);

    const concurrency = findOption(s3, "upload_concurrency") orelse return error.MissingOption;
    try std.testing.expectEqualStrings("int", concurrency.type_name);
    try std.testing.expectEqual(catalogue.Kind.integer, concurrency.kind);
    try std.testing.expect(concurrency.default == .integer);
}

test "Provider is a union across backends, not a property of the first option" {
    const gpa = std.testing.allocator;

    var parsed = try catalogue.parse(gpa, fixture_json);
    defer parsed.deinit();

    const s3 = parsed.backend("s3") orelse return error.MissingBackend;

    // s3's own `provider` option carries no Provider expression — sampling it
    // is exactly how the field gets missed.
    const provider_option = findOption(s3, "provider") orelse return error.MissingOption;
    try std.testing.expectEqualStrings("", provider_option.provider);

    // Yet the backend is full of provider-conditioned options.
    var conditioned: usize = 0;
    for (parsed.backends) |backend| {
        for (backend.options) |option| {
            if (option.provider.len != 0) conditioned += 1;
        }
    }
    try std.testing.expectEqual(@as(usize, 20), conditioned);

    const region = findOption(s3, "region") orelse return error.MissingOption;
    try std.testing.expect(region.provider.len != 0);
}

test "Provider reaches examples, not only options" {
    const gpa = std.testing.allocator;

    var parsed = try catalogue.parse(gpa, fixture_json);
    defer parsed.deinit();

    const s3 = parsed.backend("s3") orelse return error.MissingBackend;

    // The vendor list inside a backend is data too: `provider` is a plain
    // string option whose examples enumerate the vendors.
    const provider_option = findOption(s3, "provider") orelse return error.MissingOption;
    try std.testing.expect(provider_option.examples.len > 1);

    const region = findOption(s3, "region") orelse return error.MissingOption;
    var tagged: usize = 0;
    for (region.examples) |example| {
        if (example.provider.len != 0) tagged += 1;
    }
    // Region examples are vendor-specific to the last one; rendering them all
    // for one vendor offers regions that do not exist there.
    try std.testing.expect(tagged != 0);
    try std.testing.expectEqual(region.examples.len, tagged);

    var fixture_example_providers: usize = 0;
    for (parsed.backends) |backend| {
        for (backend.options) |option| {
            for (option.examples) |example| {
                if (example.provider.len != 0) fixture_example_providers += 1;
            }
        }
    }
    try std.testing.expectEqual(@as(usize, 664), fixture_example_providers);
}

test "Hide is a bitmask, so command-line-only hiding is not configurator hiding" {
    const gpa = std.testing.allocator;

    var parsed = try catalogue.parse(gpa, fixture_json);
    defer parsed.deinit();

    var visible: usize = 0;
    var from_configurator: usize = 0;
    var any_command_line_only = false;
    for (parsed.backends) |backend| {
        for (backend.options) |option| {
            if (option.hide == 0) visible += 1;
            if (option.hiddenFromConfigurator()) from_configurator += 1;
            if (option.hide == catalogue.hide_command_line) any_command_line_only = true;
        }
    }
    // The snapshot has no command-line-only option among these five backends,
    // but the predicate must still be a bit test rather than `hide != 0`.
    try std.testing.expect(!any_command_line_only);
    try std.testing.expect(visible != 0);
    try std.testing.expect(from_configurator != 0);
    try std.testing.expect(from_configurator < visible);

    // The bit test itself, independent of what the snapshot happens to hold.
    try std.testing.expect(!catalogue.hiddenFromConfiguratorMask(0));
    try std.testing.expect(!catalogue.hiddenFromConfiguratorMask(catalogue.hide_command_line));
    try std.testing.expect(catalogue.hiddenFromConfiguratorMask(catalogue.hide_configurator));
    try std.testing.expect(catalogue.hiddenFromConfiguratorMask(
        catalogue.hide_command_line | catalogue.hide_configurator,
    ));
}

// -- Tolerance ---------------------------------------------------------------

test "an unrecognised type renders as text and unknown fields never fail the parse" {
    const gpa = std.testing.allocator;

    // A future rclone: a type nobody here has heard of, unknown keys on the
    // document, on the backend, on the option and on the example, and an
    // option record missing most of the eighteen fields entirely.
    const future =
        \\{
        \\  "cache_generation": 9,
        \\  "providers": [
        \\    {
        \\      "Name": "futurestore",
        \\      "Description": "A backend from a later release",
        \\      "Prefix": "futurestore",
        \\      "Quantum": true,
        \\      "Options": [
        \\        {
        \\          "Name": "entanglement",
        \\          "Help": "How entangled.",
        \\          "Type": "QuantumState",
        \\          "Groups": "Advanced",
        \\          "Examples": [
        \\            { "Value": "up", "Help": "Spin up", "Flavour": "strange" }
        \\          ]
        \\        },
        \\        { "Name": "bare" }
        \\      ]
        \\    }
        \\  ]
        \\}
    ;

    var parsed = try catalogue.parse(gpa, future);
    defer parsed.deinit();

    const backend = parsed.backend("futurestore") orelse return error.MissingBackend;
    try std.testing.expectEqual(@as(usize, 2), backend.options.len);

    const entanglement = findOption(backend, "entanglement") orelse return error.MissingOption;
    try std.testing.expectEqualStrings("QuantumState", entanglement.type_name);
    // The whole point: unknown means text, never a rejected document.
    try std.testing.expectEqual(catalogue.Kind.text, entanglement.kind);
    try std.testing.expectEqual(@as(usize, 1), entanglement.examples.len);
    try std.testing.expectEqualStrings("up", entanglement.examples[0].value);

    // An option record with nothing but a name still parses, with every
    // absent field at its neutral value.
    const bare = findOption(backend, "bare") orelse return error.MissingOption;
    try std.testing.expectEqualStrings("", bare.type_name);
    try std.testing.expectEqual(catalogue.Kind.text, bare.kind);
    try std.testing.expect(!bare.required);
    try std.testing.expect(!bare.advanced);
    try std.testing.expect(!bare.is_password);
    try std.testing.expect(!bare.sensitive);
    try std.testing.expect(!bare.exclusive);
    try std.testing.expect(!bare.no_prefix);
    try std.testing.expectEqual(@as(u32, 0), bare.hide);
    try std.testing.expectEqual(@as(usize, 0), bare.examples.len);
    try std.testing.expect(bare.default == .null);
}

test "a document that is not a catalogue is rejected rather than half-parsed" {
    const gpa = std.testing.allocator;

    try std.testing.expectError(error.BadJson, catalogue.parse(gpa, "not json at all"));
    try std.testing.expectError(error.BadJson, catalogue.parse(gpa, "[1,2,3]"));
    try std.testing.expectError(error.BadJson, catalogue.parse(gpa, "{\"providers\":7}"));

    // An empty catalogue is a valid catalogue: no backends is the same state
    // as no cache, which every caller already handles.
    var none = try catalogue.parse(gpa, "{\"providers\":[]}");
    defer none.deinit();
    try std.testing.expectEqual(@as(usize, 0), none.backends.len);
}

// -- matchProvider -----------------------------------------------------------

test "matchProvider: both empty cases match everything" {
    // This pair is what makes a backend with no vendor chosen show all of its
    // conditional fields rather than none of them.
    try std.testing.expect(catalogue.matchProvider("", ""));
    try std.testing.expect(catalogue.matchProvider("", "Wasabi"));
    try std.testing.expect(catalogue.matchProvider("Wasabi", ""));
    try std.testing.expect(catalogue.matchProvider("!Wasabi", ""));
}

test "matchProvider: membership in the comma-separated list" {
    try std.testing.expect(catalogue.matchProvider("Alpha", "Alpha"));
    try std.testing.expect(!catalogue.matchProvider("Alpha", "Beta"));

    try std.testing.expect(catalogue.matchProvider("Alpha,Beta,Gamma", "Alpha"));
    try std.testing.expect(catalogue.matchProvider("Alpha,Beta,Gamma", "Beta"));
    try std.testing.expect(catalogue.matchProvider("Alpha,Beta,Gamma", "Gamma"));
    try std.testing.expect(!catalogue.matchProvider("Alpha,Beta,Gamma", "Delta"));

    // Membership is whole-element equality, not a substring test: a vendor
    // whose name is a prefix of another must not borrow its fields.
    try std.testing.expect(!catalogue.matchProvider("Alphabet", "Alpha"));
    try std.testing.expect(!catalogue.matchProvider("Alpha,Beta", "Alph"));
}

test "matchProvider: a leading bang negates, both ways" {
    try std.testing.expect(!catalogue.matchProvider("!Alpha", "Alpha"));
    try std.testing.expect(catalogue.matchProvider("!Alpha", "Beta"));

    try std.testing.expect(!catalogue.matchProvider("!Alpha,Beta", "Alpha"));
    try std.testing.expect(!catalogue.matchProvider("!Alpha,Beta", "Beta"));
    try std.testing.expect(catalogue.matchProvider("!Alpha,Beta", "Gamma"));

    // A bare `!` excludes nothing, so everything still matches.
    try std.testing.expect(catalogue.matchProvider("!", "Alpha"));
}

test "an option knows whether it applies to the selected vendor" {
    const gpa = std.testing.allocator;

    var parsed = try catalogue.parse(gpa, fixture_json);
    defer parsed.deinit();

    const s3 = parsed.backend("s3") orelse return error.MissingBackend;
    const region = findOption(s3, "region") orelse return error.MissingOption;

    // Whatever the snapshot's list holds, the option must apply when no
    // vendor is chosen, and the filtered example count must never exceed the
    // unfiltered one.
    try std.testing.expect(region.appliesTo(""));

    var everything: usize = 0;
    for (region.examples) |example| {
        if (catalogue.matchProvider(example.provider, "")) everything += 1;
    }
    try std.testing.expectEqual(region.examples.len, everything);

    const first_tagged = blk: {
        for (region.examples) |example| {
            if (example.provider.len != 0) break :blk example.provider;
        }
        break :blk "";
    };
    try std.testing.expect(first_tagged.len != 0);

    var narrowed: usize = 0;
    for (region.examples) |example| {
        if (catalogue.matchProvider(example.provider, first_tagged)) narrowed += 1;
    }
    try std.testing.expect(narrowed != 0);
    try std.testing.expect(narrowed < region.examples.len);
}

// -- Cache -------------------------------------------------------------------

const Fixture = struct {
    tmp: std.testing.TmpDir,
    gpa: std.mem.Allocator,
    root: []u8,

    fn init(gpa: std.mem.Allocator) !Fixture {
        var tmp = std.testing.tmpDir(.{});
        errdefer tmp.cleanup();

        var buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const len = try tmp.dir.realPath(io, &buffer);
        const root = try gpa.dupe(u8, buffer[0..len]);
        return .{ .tmp = tmp, .gpa = gpa, .root = root };
    }

    fn deinit(self: *Fixture) void {
        self.gpa.free(self.root);
        self.tmp.cleanup();
        self.* = undefined;
    }
};

test "a missing cache is an empty list, never an error" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    var loaded = try catalogue.loadCached(gpa, io, fixture.root);
    defer loaded.deinit();

    try std.testing.expectEqual(@as(usize, 0), loaded.backends.len);
    try std.testing.expect(loaded.rclone_version == null);
    try std.testing.expect(loaded.isEmpty());
}

test "a corrupt cache is an empty list too, not a blocked settings screen" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const cache_path = try catalogue.cachePath(gpa, fixture.root);
    defer gpa.free(cache_path);
    if (path.dirname(cache_path)) |dir| try std.Io.Dir.cwd().createDirPath(io, dir);
    try std.Io.Dir.cwd().writeFile(io, .{ .sub_path = cache_path, .data = "{ truncated" });

    var loaded = try catalogue.loadCached(gpa, io, fixture.root);
    defer loaded.deinit();
    try std.testing.expect(loaded.isEmpty());
}

test "cache and loadCached round-trip the catalogue under its version stamp" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const version: daemon.Version = .{ .major = 1, .minor = 75, .patch = 0 };
    const document = try catalogue.stampDocument(gpa, fixture_json, version);
    defer gpa.free(document);

    try catalogue.cache(gpa, io, fixture.root, document);

    var loaded = try catalogue.loadCached(gpa, io, fixture.root);
    defer loaded.deinit();

    try std.testing.expectEqual(@as(usize, 5), loaded.backends.len);
    try std.testing.expect(loaded.rclone_version != null);
    try std.testing.expectEqual(std.math.Order.eq, loaded.rclone_version.?.order(version));

    const s3 = loaded.backend("s3") orelse return error.MissingBackend;
    try std.testing.expectEqual(@as(usize, 78), s3.options.len);

    // Published by rename: the temp file must not survive a successful write,
    // and a reader must never be able to see a partial document under the
    // real name.
    const cache_path = try catalogue.cachePath(gpa, fixture.root);
    defer gpa.free(cache_path);
    const tmp_path = try std.fmt.allocPrint(gpa, "{s}.tmp", .{cache_path});
    defer gpa.free(tmp_path);
    try std.testing.expectError(
        error.FileNotFound,
        std.Io.Dir.cwd().statFile(io, tmp_path, .{}),
    );
}

test "a cache written by a different rclone is still readable, and reports its own version" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const older: daemon.Version = .{ .major = 1, .minor = 74, .patch = 2 };
    const document = try catalogue.stampDocument(gpa, "{\"providers\":[]}", older);
    defer gpa.free(document);
    try catalogue.cache(gpa, io, fixture.root, document);

    var loaded = try catalogue.loadCached(gpa, io, fixture.root);
    defer loaded.deinit();

    try std.testing.expect(loaded.rclone_version != null);
    try std.testing.expectEqual(std.math.Order.eq, loaded.rclone_version.?.order(older));
    // Which is how the caller decides to refetch: the stamp differs from what
    // the running binary reports.
    const running: daemon.Version = .{ .major = 1, .minor = 75, .patch = 0 };
    try std.testing.expect(!catalogue.matchesVersion(loaded.rclone_version, running));
    try std.testing.expect(catalogue.matchesVersion(loaded.rclone_version, older));
    try std.testing.expect(!catalogue.matchesVersion(null, older));
}

test "cache overwrites the previous document rather than appending to it" {
    const gpa = std.testing.allocator;

    var fixture = try Fixture.init(gpa);
    defer fixture.deinit();

    const version: daemon.Version = .{ .major = 1, .minor = 75, .patch = 0 };

    const big = try catalogue.stampDocument(gpa, fixture_json, version);
    defer gpa.free(big);
    try catalogue.cache(gpa, io, fixture.root, big);

    const small = try catalogue.stampDocument(gpa, "{\"providers\":[]}", version);
    defer gpa.free(small);
    try catalogue.cache(gpa, io, fixture.root, small);

    var loaded = try catalogue.loadCached(gpa, io, fixture.root);
    defer loaded.deinit();
    try std.testing.expectEqual(@as(usize, 0), loaded.backends.len);
    try std.testing.expect(loaded.rclone_version != null);
}
