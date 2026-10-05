//! The batch mode's command line and its self-check, run on an engine session
//! main.zig has started with a hidden window:
//!
//!   ResourceEditor [-mod=...] --batch <kind|all> <src> <dst> [-f] [-os]
//!   ResourceEditor [-mod=...] --batch-check <fixtures> <scratch>
//!
//! --batch is MFC's `reseditor.exe <*.ext> <src> <dst> [-f] [-os]`
//! (CEditorApp::RunBatchMode): every project of the kind under <src>, at any
//! depth, exported into <dst>'s data/ folder (-f: even when up to date), or
//! with -os only opened and saved again. Nothing is drawn and nothing of the
//! user's settings is read or written. The report (tools_logic.BatchSummary)
//! goes to stderr like every other automated mode's output and lists the
//! projects exported, skipped and failed, and those without a gamma.cfg; the
//! exit is 0 only when no project failed (2 for a bad command line).
//!
//! The engine session needs a window and a GPU device (BkEditorStart), so the
//! batch opens a hidden, never focused window; a host without a device cannot
//! run it. --batch-check, the resource-editor-batch tier, copies the 21
//! tracked project fixtures into <scratch> and proves the command line's own
//! path end to end: an -os batch over all of them re-saves each byte for byte
//! and the engine's reader (BkResOpen) opens every result as its kind; an
//! export batch reports its projects and the missing gamma.cfg, which a
//! gamma.cfg in the source root then satisfies; the shipped Data folder is
//! refused as a destination.
const std = @import("std");
const kit = @import("editor_kit");
const core = @import("resource_core");
const c_bridge = @import("c_bridge.zig");
const tools = @import("tools_logic.zig");

const Kind = core.bridge.Kind;
const ResBridge = core.bridge.ResBridge;

pub const usage_line = "ResourceEditor [-mod=<Folder>|-mod=None] --batch <kind|all> <source folder> <destination folder> [-f] [-os]";

/// The command line's own check, before the engine starts: null when it is
/// a batch, else the exit code (2) after the reason and the usage.
pub fn refuseArgs(args: []const []const u8) ?u8 {
    switch (tools.parseBatchArgs(args)) {
        .ok => return null,
        .bad => |reason| {
            std.debug.print("resource-editor: batch: {s}\nusage: {s}\n", .{ reason, usage_line });
            return 2;
        },
    }
}

/// --batch: the exit code.
pub fn run(gpa: std.mem.Allocator, io: std.Io, session: *anyopaque, args: []const []const u8) u8 {
    const request = switch (tools.parseBatchArgs(args)) {
        .ok => |r| r,
        .bad => |reason| {
            std.debug.print("resource-editor: batch: {s}\nusage: {s}\n", .{ reason, usage_line });
            return 2;
        },
    };
    var real = c_bridge.RealResBridge.init(gpa, session);
    var summary = execute(gpa, io, real.bridge(), request) catch |err| {
        std.debug.print("resource-editor: batch failed: {s}\n", .{@errorName(err)});
        return 1;
    };
    defer summary.deinit(gpa);
    printReport(gpa, &summary);
    return summary.exitCode();
}

/// The projects under the source folder, then BkResBatch over them. A source
/// folder that will not list still goes to the bridge, whose refusal names it.
pub fn execute(gpa: std.mem.Allocator, io: std.Io, b: ResBridge, request: tools.BatchRequest) !tools.BatchSummary {
    const projects = tools.listProjects(io, gpa, request.src, request.mask) catch |err| switch (err) {
        error.OutOfMemory => return error.OutOfMemory,
        else => try gpa.alloc([]u8, 0),
    };
    defer tools.freeProjects(gpa, projects);
    var std_files: kit.files.StdFiles = .{ .io = io, .dir = std.Io.Dir.cwd() };
    return tools.runBatch(gpa, b, std_files.files(), request, projects);
}

pub fn printReport(gpa: std.mem.Allocator, summary: *const tools.BatchSummary) void {
    var out: std.Io.Writer.Allocating = .init(gpa);
    defer out.deinit();
    tools.formatBatchReport(summary, &out.writer) catch {
        std.debug.print("resource-editor: the batch report did not fit in memory\n", .{});
        return;
    };
    std.debug.print("{s}", .{out.written()});
}

// --- --batch-check --------------------------------------------------------

fn checkFail(comptime format: []const u8, args: anytype) u8 {
    std.debug.print("resource-editor: batch check FAIL: " ++ format ++ "\n", args);
    return 1;
}

/// --batch-check <fixtures> <scratch>: the exit code.
pub fn check(gpa: std.mem.Allocator, io: std.Io, session: *anyopaque, base_root: []const u8, args: []const []const u8) u8 {
    if (args.len != 2) {
        std.debug.print("usage: ResourceEditor --batch-check <fixtures folder> <scratch folder>\n", .{});
        return 2;
    }
    const fixtures = args[0];
    const scratch = args[1];
    const cwd = std.Io.Dir.cwd();
    var real = c_bridge.RealResBridge.init(gpa, session);
    const b = real.bridge();

    // A fresh scratch folder, so a file of an earlier run cannot pass.
    cwd.deleteTree(io, scratch) catch |err| return checkFail("{s} could not be cleared: {s}", .{ scratch, @errorName(err) });
    var src_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const src = std.fmt.bufPrint(&src_buffer, "{s}{c}src", .{ scratch, std.fs.path.sep }) catch return checkFail("the scratch path is too long", .{});
    var dst_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const dst = std.fmt.bufPrint(&dst_buffer, "{s}{c}out", .{ scratch, std.fs.path.sep }) catch return checkFail("the scratch path is too long", .{});

    const kinds = std.enums.values(Kind);
    var originals: [kinds.len][]u8 = undefined;
    var loaded: usize = 0;
    defer for (originals[0..loaded]) |bytes| gpa.free(bytes);
    for (kinds) |kind| {
        var from_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const from = std.fmt.bufPrint(&from_buffer, "{s}{c}{s}{c}project.{s}", .{ fixtures, std.fs.path.sep, kind.extension(), std.fs.path.sep, kind.extension() }) catch return checkFail("the fixture path is too long", .{});
        const bytes = cwd.readFileAlloc(io, from, gpa, .limited(16 << 20)) catch |err| return checkFail("{s}: {s}", .{ from, @errorName(err) });
        originals[loaded] = bytes;
        loaded += 1;
        var to_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const to = projectPath(&to_buffer, src, kind) orelse return checkFail("the scratch path is too long", .{});
        cwd.createDirPath(io, std.fs.path.dirname(to).?) catch |err| return checkFail("{s}: {s}", .{ to, @errorName(err) });
        cwd.writeFile(io, .{ .sub_path = to, .data = bytes }) catch |err| return checkFail("{s}: {s}", .{ to, @errorName(err) });
    }

    // 1. -os over every kind: all re-saved, byte for byte, and each opens
    // again through the engine's reader as its own kind.
    {
        var summary = execute(gpa, io, b, .{ .mask = .all, .src = src, .dst = dst, .flags = .{ .open_save = true } }) catch |err| return checkFail("the -os batch: {s}", .{@errorName(err)});
        defer summary.deinit(gpa);
        printReport(gpa, &summary);
        if (summary.status != .ok) return checkFail("the -os batch was refused: {s}", .{summary.message()});
        if (summary.project_count != kinds.len) return checkFail("the -os batch found {d} projects, not {d}", .{ summary.project_count, kinds.len });
        if (summary.failed_projects != 0 or summary.report.written != kinds.len)
            return checkFail("the -os batch re-saved {d} of {d}, {d} failed", .{ summary.report.written, kinds.len, summary.failed_projects });
        if (summary.missing_gamma.items.len != 0) return checkFail("an -os batch must not look for gamma.cfg", .{});
        if (summary.exitCode() != 0) return checkFail("the -os batch's exit is {d}", .{summary.exitCode()});
    }
    for (kinds, 0..) |kind, i| {
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const path = projectPath(&path_buffer, src, kind).?;
        const bytes = cwd.readFileAlloc(io, path, gpa, .limited(16 << 20)) catch |err| return checkFail("{s}: {s}", .{ path, @errorName(err) });
        defer gpa.free(bytes);
        if (!std.mem.eql(u8, bytes, originals[i])) return checkFail("{s} changed when re-saved ({d} bytes, the fixture {d})", .{ path, bytes.len, originals[i].len });
        if (b.open(path) != .ok) return checkFail("the engine's reader refused the re-saved {s}: {s}", .{ path, b.lastMessage() });
        var kind_back: Kind = .weapon;
        const kind_status = b.kindOf(&kind_back);
        var total: usize = 0;
        var none: [0]core.bridge.NodeRecord = .{};
        _ = b.nodes(&none, &total);
        _ = b.close();
        if (kind_status != .ok or kind_back != kind) return checkFail("{s} reopened as {s}, not {s}", .{ path, kind_back.extension(), kind.extension() });
        if (total == 0) return checkFail("{s} reopened with no root node", .{path});
    }

    // 2. An export batch of one kind: its project is reported, exported or
    // (while the kind's exporter is not ported) failed with that reason, and
    // listed as having no gamma.cfg.
    const export_request: tools.BatchRequest = .{ .mask = .{ .one = .weapon }, .src = src, .dst = dst };
    {
        var summary = execute(gpa, io, b, export_request) catch |err| return checkFail("the export batch: {s}", .{@errorName(err)});
        defer summary.deinit(gpa);
        printReport(gpa, &summary);
        if (summary.status != .ok) return checkFail("the export batch was refused: {s}", .{summary.message()});
        if (summary.project_count != 1) return checkFail("the wpn export batch found {d} projects", .{summary.project_count});
        if (summary.failed_projects == 1) {
            if (std.mem.indexOf(u8, summary.failed.items[0].reason, "not ported") == null)
                return checkFail("the wpn export failed for another reason: {s}", .{summary.failed.items[0].reason});
            if (summary.exitCode() == 0) return checkFail("a failed project must fail the batch's exit", .{});
        } else if (summary.report.written == 0) return checkFail("the wpn export neither failed nor wrote a file", .{});
        if (summary.missing_gamma.items.len != 1) return checkFail("{d} projects reported without gamma.cfg, not 1", .{summary.missing_gamma.items.len});
    }

    // 3. A gamma.cfg in the source root is found from the project's folder.
    {
        var gamma_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const gamma_path = std.fmt.bufPrint(&gamma_buffer, "{s}{c}{s}", .{ src, std.fs.path.sep, tools.gamma_file_name }) catch return checkFail("the scratch path is too long", .{});
        var text_buffer: [256]u8 = undefined;
        cwd.writeFile(io, .{ .sub_path = gamma_path, .data = tools.formatGammaCfg(&text_buffer, .{ .gamma = 0.5 }).? }) catch |err| return checkFail("{s}: {s}", .{ gamma_path, @errorName(err) });
        var summary = execute(gpa, io, b, export_request) catch |err| return checkFail("the export batch with gamma.cfg: {s}", .{@errorName(err)});
        defer summary.deinit(gpa);
        if (summary.missing_gamma.items.len != 0) return checkFail("gamma.cfg in {s} was not found from {s}", .{ src, summary.missing_gamma.items[0] });
    }

    // 4. The installation's own folder as destination: its data/ is the
    // shipped Data, refused before any project is touched.
    {
        var summary = execute(gpa, io, b, .{ .mask = .{ .one = .weapon }, .src = src, .dst = base_root }) catch |err| return checkFail("the shipped batch: {s}", .{@errorName(err)});
        defer summary.deinit(gpa);
        if (summary.status != .refused) return checkFail("a batch into the shipped Data folder answered {s}", .{@tagName(summary.status)});
    }

    std.debug.print("resource-editor: batch check PASS ({d} projects re-saved byte for byte and reopened, export batch reported, gamma.cfg missing then found, shipped Data refused)\n", .{kinds.len});
    return 0;
}

fn projectPath(buffer: []u8, src: []const u8, kind: Kind) ?[]const u8 {
    return std.fmt.bufPrint(buffer, "{s}{c}{s}{c}project.{s}", .{ src, std.fs.path.sep, kind.extension(), std.fs.path.sep, kind.extension() }) catch null;
}
