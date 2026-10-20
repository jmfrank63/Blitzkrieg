//! The CI workflow runs the l10n tiers (M002 S09 T03): reads .github/workflows/cross-platform.yml as
//! text, splits it into jobs by the two-space `  <job-id>:` lines and asserts that each expected
//! `zig build` step is named in the expected job. A step counts only on a `zig build` line, so a
//! comment or a step name that merely mentions it does not. Every Linux job that runs a measured tier
//! must also install libharfbuzz-bin, because the measured tiers fail without hb-view (D069).
//! macOS jobs install hb-view with `brew install harfbuzz` and run the measured tiers; Windows jobs run
//! only the contract tiers and compile the measured ones (D088), and the last test keeps it so.
const std = @import("std");
const testing = std.testing;

const workflow_path = ".github/workflows/cross-platform.yml";

const Row = struct { job: []const u8, step: []const u8 };

const game_codes = [_][]const u8{ "en", "ru", "zh-hans", "ja", "hi", "es", "fr", "de", "pt-br", "id" };

/// One row per step a job must run. The Linux rows first; the game shards are added by `gameRows`.
const expected = [_]Row{
    .{ .job = "linux-game", .step = "install-language-mods" },
    .{ .job = "linux-elk", .step = "elk-l10n-auto" },
    .{ .job = "linux-map-editor", .step = "map-editor-l10n-auto" },
    .{ .job = "linux-resource-editor", .step = "resource-editor-l10n-auto" },
    .{ .job = "linux-tools", .step = "test-bk-l10n" },
    .{ .job = "linux-tools", .step = "test-bk-tools-l10n" },
    .{ .job = "linux-tools", .step = "test-l10n-measure" },
    // The compile job builds every tier once for the others, so each new step is in its list.
    .{ .job = "linux-build", .step = "test-editor-kit" },
    .{ .job = "linux-build", .step = "install-language-mods" },
    .{ .job = "linux-build", .step = "elk-l10n-auto" },
    .{ .job = "linux-build", .step = "map-editor-l10n-auto" },
    .{ .job = "linux-build", .step = "resource-editor-l10n-auto" },
    .{ .job = "linux-build", .step = "test-bk-l10n" },
    .{ .job = "linux-build", .step = "test-bk-tools-l10n" },
    .{ .job = "linux-build", .step = "test-l10n-measure" },
    // macOS runs everything Linux does that needs hb-view, through Homebrew's harfbuzz (D088).
    .{ .job = "macos-game", .step = "install-language-mods" },
    .{ .job = "macos-elk", .step = "elk-l10n-auto" },
    .{ .job = "macos-map-editor", .step = "map-editor-l10n-auto" },
    .{ .job = "macos-resource-editor", .step = "resource-editor-l10n-auto" },
    .{ .job = "macos-tools", .step = "test-bk-l10n" },
    .{ .job = "macos-tools", .step = "test-bk-tools-l10n" },
    .{ .job = "macos-tools", .step = "test-l10n-measure" },
    .{ .job = "macos-build", .step = "install-language-mods" },
    .{ .job = "macos-build", .step = "elk-l10n-auto" },
    .{ .job = "macos-build", .step = "map-editor-l10n-auto" },
    .{ .job = "macos-build", .step = "resource-editor-l10n-auto" },
    .{ .job = "macos-build", .step = "test-bk-l10n" },
    .{ .job = "macos-build", .step = "test-bk-tools-l10n" },
    .{ .job = "macos-build", .step = "test-l10n-measure" },
    // Windows runs the contract tiers; the measured ones are only compiled there (D088).
    .{ .job = "windows-game", .step = "language-mods" },
    .{ .job = "windows-tools", .step = "test-bk-l10n" },
    .{ .job = "windows-tools", .step = "test-bk-tools-l10n" },
    .{ .job = "windows-build", .step = "install-language-mods" },
    .{ .job = "windows-build", .step = "elk-l10n-auto" },
    .{ .job = "windows-build", .step = "map-editor-l10n-auto" },
    .{ .job = "windows-build", .step = "resource-editor-l10n-auto" },
    .{ .job = "windows-build", .step = "test-bk-l10n" },
    .{ .job = "windows-build", .step = "test-bk-tools-l10n" },
    .{ .job = "windows-build", .step = "test-l10n-measure" },
};

/// The macOS jobs that run a measured tier, and so need hb-view from Homebrew.
const macos_measured_jobs = [_][]const u8{ "macos-game", "macos-elk", "macos-map-editor", "macos-resource-editor", "macos-tools" };

/// The Windows jobs that run tests. None of them may run a measured tier: there is no hb-view there.
const windows_run_jobs = [_][]const u8{ "windows-game", "windows-resource-editor", "windows-map-editor", "windows-elk", "windows-tools" };

/// The steps whose reference rasters come from hb-view.
const measured_steps = [_][]const u8{ "elk-l10n-auto", "map-editor-l10n-auto", "resource-editor-l10n-auto", "test-l10n-measure", "l10n-game-reads-it" };

/// The Linux jobs that run a measured tier, and so need hb-view.
const measured_jobs = [_][]const u8{ "linux-game", "linux-elk", "linux-map-editor", "linux-resource-editor", "linux-tools" };

/// The text of one job: from its `  <id>:` line to the next such line.
fn jobText(workflow: []const u8, id: []const u8) ?[]const u8 {
    var start: ?usize = null;
    var offset: usize = 0;
    var lines = std.mem.splitScalar(u8, workflow, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trimEnd(u8, raw, "\r");
        const is_job_line = line.len > 3 and std.mem.startsWith(u8, line, "  ") and line[2] != ' ' and line[2] != '#' and line[line.len - 1] == ':';
        if (is_job_line) {
            if (start) |s| return workflow[s..offset];
            if (std.mem.eql(u8, line[2 .. line.len - 1], id)) start = offset;
        }
        offset += raw.len + 1;
    }
    if (start) |s| return workflow[s..];
    return null;
}

/// Whether a `zig build` line of the job lists the step as a whole word.
fn runsStep(job: []const u8, step: []const u8) bool {
    var lines = std.mem.splitScalar(u8, job, '\n');
    while (lines.next()) |raw| {
        if (std.mem.indexOf(u8, raw, "zig build") == null) continue;
        var words = std.mem.tokenizeAny(u8, raw, " \t\r\"'");
        while (words.next()) |word| if (std.mem.eql(u8, word, step)) return true;
    }
    return false;
}

fn readWorkflow(allocator: std.mem.Allocator) ![]u8 {
    const io = testing.io;
    return std.Io.Dir.cwd().readFileAlloc(io, workflow_path, allocator, .limited(4 << 20));
}

fn expectRow(workflow: []const u8, job: []const u8, step: []const u8) !void {
    const text = jobText(workflow, job) orelse {
        std.debug.print("ci-l10n-wiring: job {s} not found in {s}\n", .{ job, workflow_path });
        return error.JobMissing;
    };
    if (!runsStep(text, step)) {
        std.debug.print("ci-l10n-wiring: {s} does not run `zig build {s}`\n", .{ job, step });
        return error.StepMissing;
    }
}

test "each Linux job runs its l10n steps" {
    const workflow = try readWorkflow(testing.allocator);
    defer testing.allocator.free(workflow);
    for (expected) |row| try expectRow(workflow, row.job, row.step);
    var buffer: [48]u8 = undefined;
    for (game_codes) |code| {
        const step = try std.fmt.bufPrint(&buffer, "l10n-game-reads-it-{s}", .{code});
        try expectRow(workflow, "linux-game", step);
    }
}

test "every Linux job with a measured tier installs hb-view" {
    const workflow = try readWorkflow(testing.allocator);
    defer testing.allocator.free(workflow);
    for (measured_jobs) |job| {
        const text = jobText(workflow, job) orelse return error.JobMissing;
        if (std.mem.indexOf(u8, text, "libharfbuzz-bin") == null) {
            std.debug.print("ci-l10n-wiring: {s} runs a measured tier but does not install libharfbuzz-bin\n", .{job});
            return error.HbViewMissing;
        }
    }
}

/// Whether a non-comment line of the job installs harfbuzz.
fn installsHarfbuzz(job: []const u8) bool {
    var lines = std.mem.splitScalar(u8, job, '\n');
    while (lines.next()) |raw| {
        const line = std.mem.trim(u8, raw, " \t\r");
        if (line.len == 0 or line[0] == '#') continue;
        if (std.mem.indexOf(u8, line, "install") != null and std.mem.indexOf(u8, line, "harfbuzz") != null) return true;
    }
    return false;
}

/// Whether a `zig build` line of the job names a step that starts with the prefix.
fn runsStepPrefix(job: []const u8, prefix: []const u8) bool {
    var lines = std.mem.splitScalar(u8, job, '\n');
    while (lines.next()) |raw| {
        if (std.mem.indexOf(u8, raw, "zig build") == null) continue;
        var words = std.mem.tokenizeAny(u8, raw, " \t\r\"'");
        while (words.next()) |word| if (std.mem.startsWith(u8, word, prefix)) return true;
    }
    return false;
}

test "every macOS job with a measured tier installs harfbuzz" {
    const workflow = try readWorkflow(testing.allocator);
    defer testing.allocator.free(workflow);
    for (macos_measured_jobs) |job| {
        const text = jobText(workflow, job) orelse return error.JobMissing;
        if (!installsHarfbuzz(text)) {
            std.debug.print("ci-l10n-wiring: {s} runs a measured tier but has no `brew install harfbuzz`\n", .{job});
            return error.HbViewMissing;
        }
    }
    const game = jobText(workflow, "macos-game") orelse return error.JobMissing;
    try testing.expect(runsStepPrefix(game, "l10n-game-reads-it"));
}

test "no Windows job runs a measured tier without hb-view" {
    const workflow = try readWorkflow(testing.allocator);
    defer testing.allocator.free(workflow);
    for (windows_run_jobs) |job| {
        const text = jobText(workflow, job) orelse return error.JobMissing;
        if (installsHarfbuzz(text)) continue;
        for (measured_steps) |step| {
            if (runsStepPrefix(text, step)) {
                std.debug.print("ci-l10n-wiring: {s} runs `{s}` (measured) but installs no hb-view; Windows only compiles it (D088)\n", .{ job, step });
                return error.MeasuredTierOnWindows;
            }
        }
    }
}

/// What the l10n tiers open in the checkout, each with the sparse-checkout lines that provide it:
/// Localization/ (the overlays, the glossaries), vendor/ (fonts, harfbuzz), tools/ (the fixtures,
/// the scripts) and the shipped Data the tiers stage or read (the ELK database, the Fonts).
const Input = struct { what: []const u8, lines: []const []const u8 };
const l10n_inputs = [_]Input{
    .{ .what = "the overlays and glossaries", .lines = &.{"/Localization/"} },
    .{ .what = "vendor/fonts and vendor/harfbuzz", .lines = &.{"/vendor/"} },
    .{ .what = "tools/zig and its fixtures", .lines = &.{"/tools/"} },
    .{ .what = "the ELK database", .lines = &.{"/Data/ELK/"} },
    .{ .what = "the game's Fonts", .lines = &.{ "/Data/Fonts/", "/Data/" } },
};

/// Whether the job's text holds one of the lines on a line of its own (a sparse-checkout entry).
fn hasSparseLine(job: []const u8, lines: []const []const u8) bool {
    var it = std.mem.splitScalar(u8, job, '\n');
    while (it.next()) |raw| {
        const trimmed = std.mem.trim(u8, raw, " \t\r");
        for (lines) |want| if (std.mem.eql(u8, trimmed, want)) return true;
    }
    return false;
}

test "every job with an l10n step checks out the inputs of the l10n tiers" {
    // The language mods and the glossary read Localization/ from the checkout, and the jobs are
    // sparse: without the folder language-mods had nothing to build (run 39446226101).
    const workflow = try readWorkflow(testing.allocator);
    defer testing.allocator.free(workflow);
    for (expected) |row| {
        const text = jobText(workflow, row.job) orelse return error.JobMissing;
        for (l10n_inputs) |input| {
            if (hasSparseLine(text, input.lines)) continue;
            std.debug.print("ci-l10n-wiring: {s} runs `{s}` but its sparse checkout lacks {s} (needs {s} for {s})\n", .{ row.job, row.step, input.lines[0], input.lines[0], input.what });
            return error.SparseInputMissing;
        }
    }
}

test "the l10n-check job checks out the S08 document sets" {
    const workflow = try readWorkflow(testing.allocator);
    defer testing.allocator.free(workflow);
    const text = jobText(workflow, "l10n-check") orelse return error.JobMissing;
    for ([_][]const u8{ "/Localization/", "/Data/ELK/", "/Data/Editor/TestProjects/", "/vendor/", "/tools/" }) |path| {
        if (!hasSparseLine(text, &.{path})) {
            std.debug.print("ci-l10n-wiring: l10n-check lacks {s} in its sparse checkout\n", .{path});
            return error.SparseInputMissing;
        }
    }
}

test "the sparse-input check notices a dropped line" {
    const job = "          sparse-checkout: |\r\n            /*\r\n            /vendor/\r\n";
    try testing.expect(hasSparseLine(job, &.{"/vendor/"}));
    try testing.expect(!hasSparseLine(job, &.{"/Localization/"}));
}

test "the job splitter finds the last job and rejects a missing one" {
    const sample = "jobs:\n  a:\n    x: 1\n  b:\n    run: zig build one two\n";
    try testing.expect(jobText(sample, "c") == null);
    try testing.expect(runsStep(jobText(sample, "b").?, "two"));
    try testing.expect(!runsStep(jobText(sample, "a").?, "one"));
}
