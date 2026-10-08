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
//! export batch of every kind writes what the single export (BkResExport)
//! writes for the same file, byte for byte, with nothing open and with a
//! project of the kind open and edited, which the batch neither reads nor
//! changes; a mixed folder (a corrupt project, one of another kind, files
//! that are no project) names each failure, goes on, and fails the exit; an
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
        // The whole kind folder, as the app would find it: the stats a save caches read the pictures
        // and files beside the project (an image rect is the picture's size).
        var folder_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const from_folder = std.fmt.bufPrint(&folder_buffer, "{s}{c}{s}", .{ fixtures, std.fs.path.sep, kind.extension() }) catch return checkFail("the fixture path is too long", .{});
        copyFolder(gpa, io, from_folder, std.fs.path.dirname(to).?) catch |err| return checkFail("{s}: {s}", .{ from_folder, @errorName(err) });
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

    // 1b. The export batch of every kind (the tier's main check).
    {
        var scratch_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const exports = std.fmt.bufPrint(&scratch_buffer, "{s}{c}exports", .{ scratch, std.fs.path.sep }) catch return checkFail("the scratch path is too long", .{});
        var arena_state = std.heap.ArenaAllocator.init(gpa);
        defer arena_state.deinit();
        if (checkExports(gpa, arena_state.allocator(), io, b, base_root, fixtures, exports)) |why| return checkFail("{s}", .{why});
    }

    // 2. An export batch of one kind: its project is reported, exported or
    // (while the kind's exporter is not ported) failed with that reason, and
    // listed as having no gamma.cfg.
    const export_request: tools.BatchRequest = .{ .mask = .{ .one = .weapon }, .src = src, .dst = dst };
    {
        var summary = execute(gpa, io, b, export_request) catch |err| return checkFail("the export batch: {s}", .{@errorName(err)});
        defer summary.deinit(gpa);
        printReport(gpa, &summary);
        if (!summary.ran()) return checkFail("the export batch was refused: {s}", .{summary.message()});
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

    std.debug.print("resource-editor: batch check PASS ({d} projects re-saved byte for byte and reopened, every kind exported by the batch as by the single export with and without a project open, a mixed folder reported and continued, gamma.cfg missing then found, shipped Data refused)\n", .{kinds.len});
    return 0;
}

fn projectPath(buffer: []u8, src: []const u8, kind: Kind) ?[]const u8 {
    return std.fmt.bufPrint(buffer, "{s}{c}{s}{c}project.{s}", .{ src, std.fs.path.sep, kind.extension(), std.fs.path.sep, kind.extension() }) catch null;
}

// --- the export batch of every kind -----------------------------------------

/// The files under a mod's data/ folder by relative path, bytes owned. The two files
/// BkResModSettingsSet writes are left out: the batch's destination has no settings.
const Tree = struct {
    names: std.ArrayList([]u8) = .empty,
    bytes: std.ArrayList([]u8) = .empty,

    fn deinit(self: *Tree, gpa: std.mem.Allocator) void {
        for (self.names.items) |name| gpa.free(name);
        for (self.bytes.items) |data| gpa.free(data);
        self.names.deinit(gpa);
        self.bytes.deinit(gpa);
    }

    fn find(self: *const Tree, name: []const u8) ?usize {
        for (self.names.items, 0..) |have, i| if (std.mem.eql(u8, have, name)) return i;
        return null;
    }

    fn contains(self: *const Tree, part: []const u8) bool {
        for (self.names.items) |name| if (std.mem.indexOf(u8, name, part) != null) return true;
        return false;
    }

    /// Null when `other` holds exactly this tree's files with the same bytes, else the first difference.
    fn difference(self: *const Tree, other: *const Tree, buffer: []u8) ?[]const u8 {
        for (self.names.items, self.bytes.items) |name, data| {
            const at = other.find(name) orelse return std.fmt.bufPrint(buffer, "{s} is missing", .{name}) catch "a file is missing";
            if (!std.mem.eql(u8, data, other.bytes.items[at]))
                return std.fmt.bufPrint(buffer, "{s} differs ({d} bytes, the single export {d})", .{ name, other.bytes.items[at].len, data.len }) catch "a file differs";
        }
        for (other.names.items) |name| if (self.find(name) == null) return std.fmt.bufPrint(buffer, "{s} is extra", .{name}) catch "a file is extra";
        return null;
    }
};

fn snapshot(gpa: std.mem.Allocator, io: std.Io, data_dir: []const u8) !Tree {
    var tree: Tree = .{};
    errdefer tree.deinit(gpa);
    var dir = std.Io.Dir.cwd().openDir(io, data_dir, .{ .iterate = true }) catch return tree;
    defer dir.close(io);
    var walker = try dir.walk(gpa);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        if (std.mem.eql(u8, entry.basename, "mod.xml") or std.mem.eql(u8, entry.basename, "modobjects.xml")) continue;
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const full = try std.fmt.bufPrint(&path_buffer, "{s}{c}{s}", .{ data_dir, std.fs.path.sep, entry.path });
        const data = try std.Io.Dir.cwd().readFileAlloc(io, full, gpa, .limited(64 << 20));
        errdefer gpa.free(data);
        const name = try gpa.dupe(u8, entry.path);
        errdefer gpa.free(name);
        try tree.names.append(gpa, name);
        errdefer _ = tree.names.pop();
        try tree.bytes.append(gpa, data);
    }
    return tree;
}

/// Every file under `from` copied to the same path under `to`, but the fixtures' goldens and the
/// mission fixture's nested project (it has a final map of its own to find).
fn copyFolder(gpa: std.mem.Allocator, io: std.Io, from: []const u8, to: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    var dir = try cwd.openDir(io, from, .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(gpa);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        if (std.mem.startsWith(u8, entry.path, "golden") or std.mem.startsWith(u8, entry.path, "final-map")) continue;
        var source_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        var out_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const source = try std.fmt.bufPrint(&source_buffer, "{s}{c}{s}", .{ from, std.fs.path.sep, entry.path });
        const out_path = try std.fmt.bufPrint(&out_buffer, "{s}{c}{s}", .{ to, std.fs.path.sep, entry.path });
        const data = try cwd.readFileAlloc(io, source, gpa, .limited(64 << 20));
        defer gpa.free(data);
        if (std.fs.path.dirname(out_path)) |parent| try cwd.createDirPath(io, parent);
        try cwd.writeFile(io, .{ .sub_path = out_path, .data = data });
    }
}

fn writeText(io: std.Io, path: []const u8, text: []const u8) !void {
    const cwd = std.Io.Dir.cwd();
    if (std.fs.path.dirname(path)) |parent| try cwd.createDirPath(io, parent);
    try cwd.writeFile(io, .{ .sub_path = path, .data = text });
}

fn exportDir(b: ResBridge, dir: []const u8) bool {
    var settings: core.bridge.ModSettings = .{};
    return settings.setExportDir(dir) and b.modSettingsSet(&settings) == .ok;
}

/// Where a kind's project sits below the source folder: the particle is named as the effect
/// fixture refers to it (the effect exports only beside that particle), the GUI kind is a screen.
fn kindFolder(buffer: []u8, kind: Kind) ![]const u8 {
    if (kind == .particle) return std.fmt.bufPrint(buffer, "pcp{c}particle-2key", .{std.fs.path.sep});
    return std.fmt.bufPrint(buffer, "{s}", .{kind.extension()});
}

fn kindProject(buffer: []u8, base: []const u8, kind: Kind) ![]const u8 {
    var folder_buffer: [64]u8 = undefined;
    const folder = try kindFolder(&folder_buffer, kind);
    if (kind == .gui_frame) return std.fmt.bufPrint(buffer, "{s}{c}{s}{c}MainMenu.gui", .{ base, std.fs.path.sep, folder, std.fs.path.sep });
    return std.fmt.bufPrint(buffer, "{s}{c}{s}{c}project.{s}", .{ base, std.fs.path.sep, folder, std.fs.path.sep, kind.extension() });
}

/// The batch exports every kind from the files alone: each project exported by the app's own single
/// export (BkResExport), then one batch over the folder of all of them with nothing open, then a batch
/// of each kind with a project of that kind open and edited in the session. All must write the single
/// export's files byte for byte, and leave the session as it was. The engine's reader opens the
/// results (the weapon and the stats the Game loads). One line per kind; the failure text, or null.
/// Failure texts are allocated from `arena`, which the caller keeps until it has printed them.
fn checkExports(gpa: std.mem.Allocator, arena: std.mem.Allocator, io: std.Io, b: ResBridge, base_root: []const u8, fixtures: []const u8, scratch: []const u8) ?[]const u8 {
    const kinds = std.enums.values(Kind);
    const sep = std.fs.path.sep;
    const cwd = std.Io.Dir.cwd();
    const xsrc = std.fmt.allocPrint(arena, "{s}{c}xsrc", .{ scratch, sep }) catch return "out of memory";
    const failure = struct {
        fn text(a: std.mem.Allocator, comptime format: []const u8, args: anytype) []const u8 {
            return std.fmt.allocPrint(a, format, args) catch "a failure text did not fit in memory";
        }
    };

    for (kinds) |kind| {
        var folder_buffer: [64]u8 = undefined;
        const folder = kindFolder(&folder_buffer, kind) catch return "the folder name is too long";
        const from = std.fmt.allocPrint(arena, "{s}{c}{s}", .{ fixtures, sep, kind.extension() }) catch return "out of memory";
        const to = std.fmt.allocPrint(arena, "{s}{c}{s}", .{ xsrc, sep, folder }) catch return "out of memory";
        copyFolder(gpa, io, from, to) catch |err| return failure.text(arena, "{s} could not be copied to {s}: {s}", .{ from, to, @errorName(err) });
    }
    // The palette project is not a screen; the GUI kind is a screen, the Game's own.
    {
        const palette = std.fmt.allocPrint(arena, "{s}{c}gui{c}project.gui", .{ xsrc, sep, sep }) catch return "out of memory";
        cwd.deleteFile(io, palette) catch {};
        const shipped = std.fmt.allocPrint(arena, "{s}{c}Data{c}UI{c}MainMenu.xml", .{ base_root, sep, sep, sep }) catch return "out of memory";
        const screen = std.fmt.allocPrint(arena, "{s}{c}gui{c}MainMenu.gui", .{ xsrc, sep, sep }) catch return "out of memory";
        const text = cwd.readFileAlloc(io, shipped, arena, .limited(16 << 20)) catch |err| return failure.text(arena, "{s}: {s}", .{ shipped, @errorName(err) });
        writeText(io, screen, text) catch |err| return failure.text(arena, "{s}: {s}", .{ screen, @errorName(err) });
    }

    // The single export of each project into a mod of its own (the effect beside its particle).
    var singles: [kinds.len]Tree = undefined;
    var built: usize = 0;
    defer for (singles[0..built]) |*tree| tree.deinit(gpa);
    var all: Tree = .{};
    defer all.deinit(gpa);
    for (kinds, 0..) |kind, i| {
        const mod = std.fmt.allocPrint(arena, "{s}{c}single{c}{s}", .{ scratch, sep, sep, kind.extension() }) catch return "out of memory";
        const data = std.fmt.allocPrint(arena, "{s}{c}data", .{ mod, sep }) catch return "out of memory";
        if (kind == .effect) {
            const particle = std.fmt.allocPrint(arena, "{s}{c}single{c}pcp{c}data", .{ scratch, sep, sep, sep }) catch return "out of memory";
            copyFolder(gpa, io, particle, data) catch |err| return failure.text(arena, "the particle could not be copied beside the effect: {s}", .{@errorName(err)});
        }
        if (!exportDir(b, mod)) return failure.text(arena, "the export folder {s} was refused: {s}", .{ mod, b.lastMessage() });
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const project = kindProject(&path_buffer, xsrc, kind) catch return "the project path is too long";
        if (b.open(project) != .ok) return failure.text(arena, "{s} did not open: {s}", .{ project, b.lastMessage() });
        var report: core.bridge.ExportReport = .{};
        var warnings: [tools.max_report_warnings]core.bridge.Warning = undefined;
        const status = b.exportProject(.{ .force = true }, false, &report, &warnings);
        const message = failure.text(arena, "{s}", .{b.lastMessage()});
        _ = b.close();
        if (status != .ok or report.written < 1) return failure.text(arena, "the single export of {s} answered {s}: {s}", .{ kind.extension(), @tagName(status), message });
        singles[i] = snapshot(gpa, io, data) catch return "a single export's files could not be read";
        built += 1;
        for (singles[i].names.items, singles[i].bytes.items) |name, bytes| {
            if (all.find(name)) |at| {
                gpa.free(all.bytes.items[at]);
                all.bytes.items[at] = gpa.dupe(u8, bytes) catch return "out of memory";
                continue;
            }
            const owned_name = gpa.dupe(u8, name) catch return "out of memory";
            const owned_bytes = gpa.dupe(u8, bytes) catch return "out of memory";
            all.names.append(gpa, owned_name) catch return "out of memory";
            all.bytes.append(gpa, owned_bytes) catch return "out of memory";
        }
    }

    // 1. Nothing open: one batch over the folder of all 21 kinds, its destination a folder of its own.
    const unused = std.fmt.allocPrint(arena, "{s}{c}unused", .{ scratch, sep }) catch return "out of memory";
    _ = b.close();
    if (!exportDir(b, unused)) return failure.text(arena, "the export folder {s} was refused: {s}", .{ unused, b.lastMessage() });
    const all_dst = std.fmt.allocPrint(arena, "{s}{c}batch-all", .{ scratch, sep }) catch return "out of memory";
    {
        var summary = execute(gpa, io, b, .{ .mask = .all, .src = xsrc, .dst = all_dst, .flags = .{ .force = true } }) catch |err| return failure.text(arena, "the export batch of all kinds: {s}", .{@errorName(err)});
        defer summary.deinit(gpa);
        printReport(gpa, &summary);
        if (summary.project_count != kinds.len) return failure.text(arena, "the export batch found {d} projects, not {d}", .{ summary.project_count, kinds.len });
        if (summary.status != .ok or summary.failed_projects != 0 or summary.exitCode() != 0)
            return failure.text(arena, "the export batch of all kinds failed {d} projects ({s}): {s}", .{ summary.failed_projects, @tagName(summary.status), summary.message() });
        const data = std.fmt.allocPrint(arena, "{s}{c}data", .{ all_dst, sep }) catch return "out of memory";
        var batched = snapshot(gpa, io, data) catch return "the batch's files could not be read";
        defer batched.deinit(gpa);
        var buffer: [512]u8 = undefined;
        if (all.difference(&batched, &buffer)) |why| return failure.text(arena, "the batch of all kinds differs from the single exports: {s}", .{why});
        std.debug.print("resource-editor: batch check: all kinds: {d} files, byte for byte the 21 single exports\n", .{batched.names.items.len});
    }

    // 2. Each kind with a project of that kind open and edited: the batch reads files, not the session.
    var edited: usize = 0;
    for (kinds, 0..) |kind, i| {
        var path_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
        const project = kindProject(&path_buffer, xsrc, kind) catch return "the project path is too long";
        if (b.open(project) != .ok) return failure.text(arena, "{s} did not open: {s}", .{ project, b.lastMessage() });
        var before: [256]core.bridge.NodeRecord = undefined;
        var total: usize = 0;
        _ = b.nodes(&before, &total);
        if (total > 0 and b.setNodeName(before[0].id, "edited in the session") == .ok) edited += 1;
        _ = b.nodes(&before, &total);
        const total_before = total;
        var settings_before: core.bridge.ModSettings = .{};
        _ = b.modSettingsGet(&settings_before);
        const dst = std.fmt.allocPrint(arena, "{s}{c}batch-open{c}{s}", .{ scratch, sep, sep, kind.extension() }) catch return "out of memory";
        if (kind == .effect) {
            const particle = std.fmt.allocPrint(arena, "{s}{c}single{c}pcp{c}data", .{ scratch, sep, sep, sep }) catch return "out of memory";
            const to = std.fmt.allocPrint(arena, "{s}{c}data", .{ dst, sep }) catch return "out of memory";
            copyFolder(gpa, io, particle, to) catch |err| return failure.text(arena, "the particle could not be copied beside the effect: {s}", .{@errorName(err)});
        }
        var summary = execute(gpa, io, b, .{ .mask = .{ .one = kind }, .src = xsrc, .dst = dst, .flags = .{ .force = true } }) catch |err| return failure.text(arena, "the {s} batch: {s}", .{ kind.extension(), @errorName(err) });
        defer summary.deinit(gpa);
        if (summary.status != .ok or summary.failed_projects != 0 or summary.exitCode() != 0)
            return failure.text(arena, "the {s} batch with a {s} open failed ({s}): {s}", .{ kind.extension(), kind.extension(), @tagName(summary.status), summary.message() });
        const data = std.fmt.allocPrint(arena, "{s}{c}data", .{ dst, sep }) catch return "out of memory";
        var batched = snapshot(gpa, io, data) catch return "the batch's files could not be read";
        defer batched.deinit(gpa);
        var buffer: [512]u8 = undefined;
        if (singles[i].difference(&batched, &buffer)) |why| return failure.text(arena, "the {s} batch with a {s} open differs from its single export: {s}", .{ kind.extension(), kind.extension(), why });
        var after: [256]core.bridge.NodeRecord = undefined;
        _ = b.nodes(&after, &total);
        var settings_after: core.bridge.ModSettings = .{};
        _ = b.modSettingsGet(&settings_after);
        const same_tree = total == total_before and (total == 0 or std.mem.eql(u8, before[0].displaySlice(), after[0].displaySlice()));
        _ = b.close();
        if (!same_tree or !std.mem.eql(u8, settings_before.exportDirSlice(), settings_after.exportDirSlice()))
            return failure.text(arena, "the {s} batch changed the open project or the export settings", .{kind.extension()});
        std.debug.print("resource-editor: batch check: {s}: {d} files, equal to the single export, the open project unchanged\n", .{ kind.extension(), batched.names.items.len });
    }
    if (edited == 0) return "no kind took an edit before its batch";

    // 3. The engine's own reader opens the weapon the batch wrote, as the Game's stats loader does.
    {
        if (!exportDir(b, all_dst)) return "the batch folder was refused as an export folder";
        var found: i32 = 0;
        if (b.readBack("weapons/wpn.xml", "base", "RPG", &found) != .ok or found < 1)
            return failure.text(arena, "the engine's reader did not open the exported weapon: {s}", .{b.lastMessage()});
    }

    // 4. A mixed folder: valid projects, a corrupt one, one of another kind under the wrong extension, and
    // files that are no project. Each failure is named, the batch goes on, and its answer and exit say so.
    {
        const mixed = std.fmt.allocPrint(arena, "{s}{c}mixed", .{ scratch, sep }) catch return "out of memory";
        const mixed_dst = std.fmt.allocPrint(arena, "{s}{c}mixed-out", .{ scratch, sep }) catch return "out of memory";
        const good_from = std.fmt.allocPrint(arena, "{s}{c}wpn", .{ xsrc, sep }) catch return "out of memory";
        const good_to = std.fmt.allocPrint(arena, "{s}{c}a-good", .{ mixed, sep }) catch return "out of memory";
        copyFolder(gpa, io, good_from, good_to) catch |err| return failure.text(arena, "the mixed folder: {s}", .{@errorName(err)});
        const last_from = std.fmt.allocPrint(arena, "{s}{c}mcp", .{ xsrc, sep }) catch return "out of memory";
        const last_to = std.fmt.allocPrint(arena, "{s}{c}d-last", .{ mixed, sep }) catch return "out of memory";
        copyFolder(gpa, io, last_from, last_to) catch |err| return failure.text(arena, "the mixed folder: {s}", .{@errorName(err)});
        const medal = std.fmt.allocPrint(arena, "{s}{c}mdc{c}project.mdc", .{ xsrc, sep, sep }) catch return "out of memory";
        const medal_bytes = cwd.readFileAlloc(io, medal, arena, .limited(16 << 20)) catch |err| return failure.text(arena, "{s}: {s}", .{ medal, @errorName(err) });
        const corrupt = std.fmt.allocPrint(arena, "{s}{c}b-corrupt{c}project.wpn", .{ mixed, sep, sep }) catch return "out of memory";
        const wrong = std.fmt.allocPrint(arena, "{s}{c}c-wrong-kind{c}project.wpn", .{ mixed, sep, sep }) catch return "out of memory";
        const notes = std.fmt.allocPrint(arena, "{s}{c}notes.txt", .{ mixed, sep }) catch return "out of memory";
        const unknown = std.fmt.allocPrint(arena, "{s}{c}project.xyz", .{ mixed, sep }) catch return "out of memory";
        writeText(io, corrupt, "<Weapon_Composer_Project><childs") catch |err| return failure.text(arena, "{s}: {s}", .{ corrupt, @errorName(err) });
        writeText(io, wrong, medal_bytes) catch |err| return failure.text(arena, "{s}: {s}", .{ wrong, @errorName(err) });
        writeText(io, notes, "not a project") catch |err| return failure.text(arena, "{s}: {s}", .{ notes, @errorName(err) });
        writeText(io, unknown, "<a/>") catch |err| return failure.text(arena, "{s}: {s}", .{ unknown, @errorName(err) });
        var summary = execute(gpa, io, b, .{ .mask = .all, .src = mixed, .dst = mixed_dst, .flags = .{ .force = true } }) catch |err| return failure.text(arena, "the mixed batch: {s}", .{@errorName(err)});
        defer summary.deinit(gpa);
        printReport(gpa, &summary);
        if (summary.status != .failed) return failure.text(arena, "the mixed batch answered {s}, not failed", .{@tagName(summary.status)});
        if (summary.project_count != 4) return failure.text(arena, "the mixed batch found {d} projects, not 4 (the notes and the .xyz file are no project)", .{summary.project_count});
        if (summary.failed_projects != 2) return failure.text(arena, "the mixed batch named {d} failed projects, not 2", .{summary.failed_projects});
        if (summary.exitCode() != 1) return "the mixed batch's exit status does not say that projects failed";
        var corrupt_named = false;
        var wrong_named = false;
        for (summary.failed.items) |line| {
            if (std.mem.indexOf(u8, line.project, "b-corrupt") != null and std.mem.indexOf(u8, line.reason, "cannot read the project") != null) corrupt_named = true;
            if (std.mem.indexOf(u8, line.project, "c-wrong-kind") != null and std.mem.indexOf(u8, line.reason, "Medal_Composer_Project") != null) wrong_named = true;
        }
        if (!corrupt_named or !wrong_named) return "the mixed batch did not name the corrupt and the wrong-kind project with their reasons";
        const data = std.fmt.allocPrint(arena, "{s}{c}data", .{ mixed_dst, sep }) catch return "out of memory";
        var batched = snapshot(gpa, io, data) catch return "the mixed batch's files could not be read";
        defer batched.deinit(gpa);
        if (!batched.contains("a-good") or !batched.contains("d-last")) return "the projects around the failures were not exported";
        std.debug.print("resource-editor: batch check: mixed folder: 2 of 4 projects failed and were named, the rest exported, exit 1\n", .{});
    }
    return null;
}
