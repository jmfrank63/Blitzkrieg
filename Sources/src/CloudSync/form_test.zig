//! Tests for the form model.
//!
//! Everything here runs over the committed catalogue fixture or a synthetic
//! document — no UI and no daemon. Counts and widget kinds are asserted
//! rather than exact labels, so an rclone update changes numbers in one
//! place instead of failing on wording; the counts themselves were measured
//! from the fixture (v1.75.0) and are recorded beside each assertion.

const std = @import("std");
const catalogue = @import("catalogue.zig");
const form = @import("form.zig");

/// The committed snapshot, wired in by `build.zig` as an anonymous import.
const fixture_json = @embedFile("config_providers_fixture");

fn findField(fields: []const form.Field, name: []const u8) ?*const form.Field {
    for (fields) |*field| {
        if (field.role == .option and std.mem.eql(u8, field.name, name)) return field;
    }
    return null;
}

test "the four fixture backends split into basic and advanced with the root appended" {
    const gpa = std.testing.allocator;
    var cat = try catalogue.parse(gpa, fixture_json);
    defer cat.deinit();

    // Measured from the fixture: options / basic / configurator-hidden
    // among basic and advanced. The +1 on basic is the remote-root field.
    const cases = [_]struct {
        backend: []const u8,
        basic: usize,
        advanced: usize,
    }{
        .{ .backend = "s3", .basic = 14 + 1, .advanced = 64 - 3 }, // 78 options
        .{ .backend = "webdav", .basic = 5 + 1, .advanced = 10 }, // 15
        .{ .backend = "sftp", .basic = 13 + 1, .advanced = 35 - 1 }, // 48
        .{ .backend = "drive", .basic = (5 - 1) + 1, .advanced = 47 - 6 }, // 52
    };

    for (cases) |case| {
        var built = try form.buildForm(gpa, &cat, case.backend, "");
        defer built.deinit();

        try std.testing.expectEqual(case.basic, built.basic.len);
        try std.testing.expectEqual(case.advanced, built.advanced.len);
        for (built.basic) |field| try std.testing.expect(!field.advanced);
        for (built.advanced) |field| try std.testing.expect(field.advanced);

        // The remote root is ours: always present, last of the basic
        // fields, generically labelled, and optional — which backends need
        // one is knowledge the catalogue does not carry, so the writability
        // test discovers it with a real error instead of a guess from us.
        const root = built.basic[built.basic.len - 1];
        try std.testing.expect(root.role == .remote_root);
        try std.testing.expect(!root.required);
        try std.testing.expect(root.widget == .text);
        try std.testing.expect(root.label.len != 0);
        try std.testing.expect(root.help.len != 0);
    }
}

test "the secret flags make a masked field" {
    const gpa = std.testing.allocator;
    var cat = try catalogue.parse(gpa, fixture_json);
    defer cat.deinit();

    var s3 = try form.buildForm(gpa, &cat, "s3", "");
    defer s3.deinit();
    const secret_key = findField(s3.basic, "secret_access_key").?;
    try std.testing.expect(secret_key.widget == .masked);
    try std.testing.expect(secret_key.secret and !secret_key.is_password);
    const access_key = findField(s3.basic, "access_key_id").?;
    try std.testing.expect(access_key.widget == .masked);

    var dav = try form.buildForm(gpa, &cat, "webdav", "");
    defer dav.deinit();
    const pass = findField(dav.basic, "pass").?;
    try std.testing.expect(pass.widget == .masked);
    try std.testing.expect(pass.secret and pass.is_password);
    // `user` is Sensitive in v1.75.0 — masked by flag, not by name.
    try std.testing.expect(findField(dav.basic, "user").?.widget == .masked);
}

test "region is an editable droplist whose examples follow the vendor" {
    const gpa = std.testing.allocator;
    var cat = try catalogue.parse(gpa, fixture_json);
    defer cat.deinit();

    // `region` itself survives the vendor switch — its expression names 39
    // vendors including Wasabi — but its 153 examples are provider-tagged:
    // 26 apply to AWS and 2 to Wasabi. A Wasabi user shown AWS regions is
    // being offered values that do not exist.
    var aws = try form.buildForm(gpa, &cat, "s3", "AWS");
    defer aws.deinit();
    const aws_region = findField(aws.basic, "region").?;
    try std.testing.expect(aws_region.widget == .droplist_editable);
    try std.testing.expectEqual(@as(usize, 26), aws_region.examples.len);

    var wasabi = try form.buildForm(gpa, &cat, "s3", "Wasabi");
    defer wasabi.deinit();
    const wasabi_region = findField(wasabi.basic, "region").?;
    try std.testing.expect(wasabi_region.widget == .droplist_editable);
    try std.testing.expectEqual(@as(usize, 2), wasabi_region.examples.len);
}

test "provider-conditioned options appear only for their vendor" {
    const gpa = std.testing.allocator;
    var cat = try catalogue.parse(gpa, fixture_json);
    defer cat.deinit();

    // Three genuinely AWS-only options, all advanced, whose `Provider` is
    // exactly `AWS`.
    var aws = try form.buildForm(gpa, &cat, "s3", "AWS");
    defer aws.deinit();
    try std.testing.expect(findField(aws.advanced, "requester_pays") != null);
    try std.testing.expect(findField(aws.advanced, "use_accelerate_endpoint") != null);
    try std.testing.expect(findField(aws.advanced, "directory_bucket") != null);

    var wasabi = try form.buildForm(gpa, &cat, "s3", "Wasabi");
    defer wasabi.deinit();
    try std.testing.expect(findField(wasabi.advanced, "requester_pays") == null);
    try std.testing.expect(findField(wasabi.advanced, "use_accelerate_endpoint") == null);
    try std.testing.expect(findField(wasabi.advanced, "directory_bucket") == null);

    // No provider selected yet: every conditional field shows, rclone's own
    // rule — a backend with no vendor chosen offers all of them, not none.
    var open = try form.buildForm(gpa, &cat, "s3", "");
    defer open.deinit();
    try std.testing.expect(findField(open.advanced, "requester_pays") != null);
}

test "Hide is a bitmask: only the configurator bit removes a field" {
    const gpa = std.testing.allocator;

    var cat = try catalogue.parse(gpa,
        \\{"providers":[{"Name":"synthetic","Options":[
        \\  {"Name":"plain","Type":"string","Hide":0},
        \\  {"Name":"cli_only","Type":"string","Hide":1},
        \\  {"Name":"cfg_hidden","Type":"string","Hide":2},
        \\  {"Name":"both_hidden","Type":"string","Hide":3}
        \\]}]}
    );
    defer cat.deinit();

    var built = try form.buildForm(gpa, &cat, "synthetic", "");
    defer built.deinit();

    // plain, cli_only, and the remote root — dropping everything non-zero
    // would wrongly hide the Hide=1 field.
    try std.testing.expectEqual(@as(usize, 3), built.basic.len);
    try std.testing.expect(findField(built.basic, "plain") != null);
    try std.testing.expect(findField(built.basic, "cli_only") != null);
    try std.testing.expect(findField(built.basic, "cfg_hidden") == null);
    try std.testing.expect(findField(built.basic, "both_hidden") == null);
}

test "Exclusive examples close the droplist and unknown types degrade to text" {
    const gpa = std.testing.allocator;

    // Synthetic because exactly one option across all 69 v1.75.0 backends
    // is Exclusive — the rule must hold for the backend that gains one
    // tomorrow.
    var cat = try catalogue.parse(gpa,
        \\{"providers":[{"Name":"synthetic","Options":[
        \\  {"Name":"closed","Type":"string","Exclusive":true,"Examples":[
        \\    {"Value":"one"},{"Value":"two"}]},
        \\  {"Name":"open","Type":"string","Examples":[{"Value":"suggested"}]},
        \\  {"Name":"strange","Type":"quantum-flux"},
        \\  {"Name":"flag","Type":"bool"}
        \\]}]}
    );
    defer cat.deinit();

    var built = try form.buildForm(gpa, &cat, "synthetic", "");
    defer built.deinit();

    try std.testing.expect(findField(built.basic, "closed").?.widget == .droplist_closed);
    try std.testing.expect(findField(built.basic, "open").?.widget == .droplist_editable);
    // A type this build has never heard of renders as a text field and
    // never fails the parse or the form.
    const strange = findField(built.basic, "strange").?;
    try std.testing.expect(strange.widget == .text);
    try std.testing.expect(strange.kind == .text);
    // A known non-text kind still renders text by the widget rule, but the
    // kind rides along so a renderer can refine it.
    const flag = findField(built.basic, "flag").?;
    try std.testing.expect(flag.widget == .text);
    try std.testing.expect(flag.kind == .boolean);
}

test "defaults become placeholders the save path can compare against" {
    const gpa = std.testing.allocator;
    var cat = try catalogue.parse(gpa, fixture_json);
    defer cat.deinit();

    var built = try form.buildForm(gpa, &cat, "s3", "");
    defer built.deinit();

    // `env_auth` carries DefaultStr "false": the placeholder shows it, and
    // a typed value equal to it must never persist — a default that changes
    // upstream must follow upstream.
    const env_auth = findField(built.basic, "env_auth").?;
    try std.testing.expectEqualStrings("false", env_auth.placeholder);
    try std.testing.expect(env_auth.kind == .boolean);

    // Help rides through untouched as the tooltip source.
    try std.testing.expect(env_auth.help.len != 0);
}

test "required means must-fill: a default satisfies it, another vendor's does not apply" {
    const gpa = std.testing.allocator;

    // Real half: webdav's `url` is required with no default — blank must
    // block a save, named before any network call.
    var cat = try catalogue.parse(gpa, fixture_json);
    defer cat.deinit();
    var webdav = try form.buildForm(gpa, &cat, "webdav", "");
    defer webdav.deinit();
    try std.testing.expect(findField(webdav.basic, "url").?.required);

    // Synthetic half: rclone accepts an unset required option whose
    // default is non-empty (pixeldrain.api_url, iclouddrive.service and
    // oracleobjectstorage.provider are the three real ones in v1.75.0),
    // and a requirement scoped to another vendor does not apply at all.
    var syn = try catalogue.parse(gpa,
        \\{"providers":[{"Name":"synthetic","Options":[
        \\  {"Name":"must_fill","Type":"string","Required":true},
        \\  {"Name":"defaulted","Type":"string","Required":true,
        \\   "DefaultStr":"https://api.example"},
        \\  {"Name":"other_vendor","Type":"string","Required":true,
        \\   "Provider":"VendorB"}
        \\]}]}
    );
    defer syn.deinit();

    var vendor_a = try form.buildForm(gpa, &syn, "synthetic", "VendorA");
    defer vendor_a.deinit();
    try std.testing.expect(findField(vendor_a.basic, "must_fill").?.required);
    try std.testing.expect(!findField(vendor_a.basic, "defaulted").?.required);
    // VendorB's requirement is not in VendorA's form at all, so it cannot
    // block validation — validating the unfiltered set would.
    try std.testing.expect(findField(vendor_a.basic, "other_vendor") == null);

    // No vendor chosen shows every conditional field — requirement included.
    var open = try form.buildForm(gpa, &syn, "synthetic", "");
    defer open.deinit();
    try std.testing.expect(findField(open.basic, "other_vendor").?.required);
}

test "an unknown backend is an error, not an empty form" {
    const gpa = std.testing.allocator;
    var cat = try catalogue.parse(gpa, fixture_json);
    defer cat.deinit();

    try std.testing.expectError(
        error.UnknownBackend,
        form.buildForm(gpa, &cat, "no-such-backend", ""),
    );
}
