//! The CI workflow runs the l10n tiers (M002 S09 T03): reads .github/workflows/cross-platform.yml as
//! text, splits it into jobs by the two-space `  <job-id>:` lines and asserts that each expected
//! `zig build` step is named in the expected job. A step counts only on a `zig build` line, so a
//! comment or a step name that merely mentions it does not. Every Linux job that runs a measured tier
//! must also install libharfbuzz-bin, because the measured tiers fail without hb-view (D069).
//! Rows for macOS and Windows are added to `expected` by the task that wires those jobs.
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
};

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

test "the job splitter finds the last job and rejects a missing one" {
    const sample = "jobs:\n  a:\n    x: 1\n  b:\n    run: zig build one two\n";
    try testing.expect(jobText(sample, "c") == null);
    try testing.expect(runsStep(jobText(sample, "b").?, "two"));
    try testing.expect(!runsStep(jobText(sample, "a").?, "one"));
}
