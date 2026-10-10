//! The Tools half of the resource app that needs no window, no ImGui and no
//! SDL: File > MOD Settings, Export Result and Export Stats Only, Compress
//! to PAK, Tools > Set Directories, Picture Options with its `gamma.cfg`,
//! Batch Mode (the dialog's and the `--batch` command line's request and
//! report), Run Blitzkrieg's command line, and the Editors menu's memory of
//! the last sub-editor. Kept apart from tools_ui.zig so all of it runs under
//! `zig build test-resource-app-logic` against resource_core's fake bridge.
//!
//! Picture options are app-side, as S05's research decided: the bridge has no
//! call for them and the MFC kept them outside the project too. `gamma.cfg`
//! is the engine's own tree format (`<base Brightness=".." Contrast=".."
//! Gamma=".."/>`, the attributes CDataTreeXML and StreamIOZig write for a
//! number), searched upward from the project's folder as
//! CParentFrame::ReadConfigFile did.
const std = @import("std");
const kit = @import("editor_kit");
const core = @import("resource_core");
const logic = @import("panels_logic.zig");
const app_settings = @import("settings.zig");

const bridge = core.bridge;
const Kind = bridge.Kind;
const ResBridge = bridge.ResBridge;
const Status = bridge.Status;
const ExportFlags = bridge.ExportFlags;
const ExportReport = bridge.ExportReport;
const Warning = bridge.Warning;
const Files = kit.files.Files;
const testlaunch = kit.testlaunch;

/// D-02 for this editor: Run Blitzkrieg plays in a session-only profile of
/// its own, never the player's and never the Map Editor's test profile.
pub const test_profile = "ResourceEditorTest";
pub const gamma_file_name = "gamma.cfg";

// --- Sub-editor folders ---------------------------------------------------

/// Each sub-editor's folder under the source and export roots: the MFC
/// frames' szAddDir, with forward slashes. Picture Options writes a
/// sub-editor's gamma.cfg into `<source folder>/<this>`. The GUI frame set
/// none.
pub fn kindFolder(kind: Kind) []const u8 {
    return switch (kind) {
        .weapon => "weapons/",
        .mesh_unit => "units/technics/",
        .animation_infantry => "units/humans/",
        .squad => "squads/",
        .mine => "objects/simpleobjects/common/summer/mine/",
        .particle => "effects/particles/",
        .sprite => "effects/sprites/",
        .effect => "effects/effects/",
        .build => "buildings/",
        .object => "objects/",
        .fence => "fences/",
        .bridge => "bridges/",
        .trench => "units/technics/common/entrenchment/",
        .mission, .chapter => "scenarios/",
        .campaign => "scenarios/campaigns/",
        .medal => "medals/",
        .tile_set, .road_3d, .river_3d => "terrain/sets/",
        .gui_frame => "",
    };
}

// --- Picture options and gamma.cfg -----------------------------------------

/// Edit > Set Picture Options: brightness, contrast and gamma, each -1..1
/// (CPictureOptions clamps what is typed to that range).
pub const PictureOptions = struct {
    brightness: f32 = 0,
    contrast: f32 = 0,
    gamma: f32 = 0,

    pub fn clamped(self: PictureOptions) PictureOptions {
        return .{
            .brightness = std.math.clamp(self.brightness, -1, 1),
            .contrast = std.math.clamp(self.contrast, -1, 1),
            .gamma = std.math.clamp(self.gamma, -1, 1),
        };
    }

    pub fn isNeutral(self: PictureOptions) bool {
        return self.brightness == 0 and self.contrast == 0 and self.gamma == 0;
    }
};

/// One colour channel through CImageProcessor::CreateGammaCorrection, the
/// engine's formula the MFC dialog's right-hand preview used: each value is
/// halved after clamping, contrast is a slope about the middle, gamma a power,
/// brightness an offset, and the result is cut (not rounded) back to a byte.
pub fn correctChannel(value: u8, options: PictureOptions) u8 {
    if (options.isNeutral()) return value;
    const brightness = std.math.clamp(options.brightness, -1, 1) * 0.5;
    const contrast = std.math.clamp(options.contrast, -1, 1) * 0.5;
    const gamma = std.math.clamp(options.gamma, -1, 1) * 0.5;
    var slope: f32 = 1 + 4 * @abs(contrast);
    if (contrast < 0) slope = 1 / slope;
    const offset = 0.5 * (1 - slope);
    var power: f32 = 1;
    if (gamma > 0) power = 1 / (5 * gamma + 1) else if (gamma < 0) power = 1 / (0.5 * gamma + 1);
    const v = @as(f32, @floatFromInt(value)) / 255;
    const curved = std.math.pow(f32, v, power);
    const contrasted = std.math.clamp(slope * curved + offset, 0, 1);
    const result = std.math.clamp(contrasted + brightness, 0, 1);
    return @intFromFloat(result * 255);
}

/// A gamma.cfg's three numbers, as the engine's tree reader takes them: an
/// attribute of the root first, else a child element's text. A value that is
/// missing or does not parse stays 0, MFC's starting value.
pub fn parseGammaCfg(text: []const u8) PictureOptions {
    return .{
        .brightness = gammaField(text, "Brightness") orelse 0,
        .contrast = gammaField(text, "Contrast") orelse 0,
        .gamma = gammaField(text, "Gamma") orelse 0,
    };
}

fn gammaField(text: []const u8, name: []const u8) ?f32 {
    var at: usize = 0;
    while (std.mem.indexOfPos(u8, text, at, name)) |found| : (at = found + name.len) {
        if (found == 0) continue;
        const before = text[found - 1];
        const rest = text[found + name.len ..];
        if ((before == ' ' or before == '\t' or before == '\r' or before == '\n') and std.mem.startsWith(u8, rest, "=\"")) {
            const value = rest[2..];
            const end = std.mem.indexOfScalar(u8, value, '"') orelse return null;
            return parseNumber(value[0..end]);
        }
        if (before == '<' and std.mem.startsWith(u8, rest, ">")) {
            const value = rest[1..];
            const end = std.mem.indexOfScalar(u8, value, '<') orelse return null;
            return parseNumber(value[0..end]);
        }
    }
    return null;
}

fn parseNumber(text: []const u8) ?f32 {
    return std.fmt.parseFloat(f32, std.mem.trim(u8, text, " \t\r\n")) catch null;
}

/// gamma.cfg as the engine's tree writer lays it out (StreamIOZig's
/// bk_tree_flush: the XML declaration, CRLF, the root with the three numbers
/// as attributes, CRLF). Null when `buffer` is too small.
pub fn formatGammaCfg(buffer: []u8, options: PictureOptions) ?[]const u8 {
    const o = options.clamped();
    return std.fmt.bufPrint(buffer, "<?xml version=\"1.0\"?>\r\n<base Brightness=\"{d}\" Contrast=\"{d}\" Gamma=\"{d}\"/>\r\n", .{ o.brightness, o.contrast, o.gamma }) catch null;
}

/// CParentFrame::ReadConfigFile's search: gamma.cfg in `start_dir`, then in
/// each folder above it up to the root. The path found is written into
/// `buffer`; null when there is none.
pub fn findGammaCfg(files: Files, start_dir: []const u8, buffer: []u8) ?[]const u8 {
    var dir: ?[]const u8 = trimSeparators(start_dir);
    if (dir.?.len == 0) dir = if (start_dir.len != 0) start_dir[0..1] else null;
    while (dir) |current| {
        const candidate = joinFile(buffer, current, gamma_file_name) orelse return null;
        if (files.exists(candidate)) return candidate;
        dir = std.fs.path.dirname(current);
    }
    return null;
}

/// Where Picture Options writes (CParentFrame::WriteConfigFile): the open
/// project's own folder when "current project only" is ticked, else the
/// sub-editor's folder under the source folder.
pub const GammaTarget = union(enum) {
    path: []const u8,
    /// "Current project only" on a project that has no file yet.
    untitled,
    /// No source folder in Set Directories yet.
    no_source_folder,
    too_long,
};

pub fn gammaTarget(buffer: []u8, current_project_only: bool, project_path: ?[]const u8, source_folder: []const u8, kind: Kind) GammaTarget {
    if (current_project_only) {
        const path = project_path orelse return .untitled;
        if (path.len == 0) return .untitled;
        const folder = std.fs.path.dirname(path) orelse return .{ .path = std.fmt.bufPrint(buffer, "{s}", .{gamma_file_name}) catch return .too_long };
        return .{ .path = joinFile(buffer, folder, gamma_file_name) orelse return .too_long };
    }
    if (source_folder.len == 0) return .no_source_folder;
    var folder_buffer: [logic.path_capacity]u8 = undefined;
    const folder = joinFile(&folder_buffer, source_folder, kindFolder(kind)) orelse return .too_long;
    return .{ .path = joinFile(buffer, folder, gamma_file_name) orelse return .too_long };
}

fn trimSeparators(path: []const u8) []const u8 {
    return std.mem.trimEnd(u8, path, "/\\");
}

/// `folder` + one separator + `name`, never a doubled separator. A '/'
/// inside `name` (kindFolder's) becomes the platform's own.
fn joinFile(buffer: []u8, folder: []const u8, name: []const u8) ?[]const u8 {
    const base = trimSeparators(folder);
    const keep_root = base.len == 0 and folder.len != 0;
    const head = if (keep_root) folder[0..1] else base;
    const sep_len: usize = if (keep_root or head.len == 0) 0 else 1;
    const total = head.len + sep_len + name.len;
    if (total > buffer.len) return null;
    @memcpy(buffer[0..head.len], head);
    if (sep_len == 1) buffer[head.len] = std.fs.path.sep;
    const tail = buffer[head.len + sep_len .. total];
    @memcpy(tail, name);
    for (tail) |*byte| if (byte.* == '/') {
        byte.* = std.fs.path.sep;
    };
    return buffer[0..total];
}

// --- Export ----------------------------------------------------------------

/// Why File > Export Result or Tools > Export Stats Only cannot run. The app's
/// own path decides, not the bridge's: an autosave recovery copy moves the
/// bridge's idea of the project's file into the recovery folder (T09), and
/// a project's sources are relative to its real file.
pub const ExportBlock = enum {
    no_project,
    untitled,
    recovery_copy,

    pub fn text(self: ExportBlock) []const u8 {
        return switch (self) {
            .no_project => "no project is open",
            .untitled => "save the project first: an export reads its sources beside the project file",
            .recovery_copy => "this is a recovery copy: save it with Save As before exporting",
        };
    }
};

pub fn exportBlock(life: *const logic.Lifecycle, recovery_active: bool) ?ExportBlock {
    if (!life.is_open) return .no_project;
    const path = life.doc.pathSlice() orelse return .untitled;
    if (path.len == 0) return .untitled;
    if (recovery_active) return .recovery_copy;
    return null;
}

pub const max_report_warnings = 64;

/// One export's result as the report window shows it: written, skipped, the
/// warnings and whether the project's folder had a gamma.cfg above it.
pub const ExportOutcome = struct {
    status: Status = .ok,
    stats_only: bool = false,
    report: ExportReport = .{},
    warnings: [max_report_warnings]Warning = undefined,
    gamma_missing: bool = false,
    blocked: ?ExportBlock = null,
    message_buffer: [512]u8 = undefined,
    message_len: usize = 0,

    pub fn warningLines(self: *const ExportOutcome) []const Warning {
        return self.warnings[0..@min(self.report.warning_total, max_report_warnings)];
    }

    pub fn message(self: *const ExportOutcome) []const u8 {
        return self.message_buffer[0..self.message_len];
    }

    fn setMessage(self: *ExportOutcome, text: []const u8) void {
        self.message_len = @min(text.len, self.message_buffer.len);
        @memcpy(self.message_buffer[0..self.message_len], text[0..self.message_len]);
    }
};

/// Export Result (stats_only false) or Export Stats Only through the bridge.
/// A block refuses before the bridge is asked; `gamma_found` is the caller's
/// findGammaCfg answer for the project's folder (MFC asked to create one
/// here; the port reports it and leaves creating it to Picture Options).
pub fn runExport(b: ResBridge, life: *const logic.Lifecycle, recovery_active: bool, stats_only: bool, gamma_found: bool, out: *ExportOutcome) void {
    out.* = .{ .stats_only = stats_only };
    if (exportBlock(life, recovery_active)) |block| {
        out.blocked = block;
        out.status = .refused;
        out.setMessage(block.text());
        return;
    }
    out.gamma_missing = !gamma_found;
    out.status = b.exportProject(.{}, stats_only, &out.report, &out.warnings);
    out.setMessage(b.lastMessage());
}

/// The report's text, line by line: a heading with the counts, the warnings
/// (and how many did not fit), then the missing gamma.cfg.
pub fn formatExportReport(outcome: *const ExportOutcome, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    const title = if (outcome.stats_only) "Export Stats Only" else "Export Result";
    if (outcome.status != .ok) {
        try writer.print("{s} refused: {s}\n", .{ title, outcome.message() });
    } else {
        try writer.print("{s}: written {d}, skipped {d}, warnings {d}\n", .{ title, outcome.report.written, outcome.report.skipped, outcome.report.warning_total });
    }
    for (outcome.warningLines()) |*line| try writer.print("  {s}\n", .{line.textSlice()});
    if (outcome.report.warning_total > max_report_warnings) try writer.print("  ... and {d} more\n", .{outcome.report.warning_total - max_report_warnings});
    if (outcome.status == .ok and outcome.gamma_missing) try writer.print("no gamma.cfg in the project's folder or above it: picture options are 0\n", .{});
}

// --- Compress to PAK ---------------------------------------------------------

/// The PAK name the save dialog answered, with `.pak` added when it has none
/// (MFC's dialog added it).
pub fn withPakExtension(buffer: []u8, path: []const u8) ?[]const u8 {
    if (std.ascii.eqlIgnoreCase(std.fs.path.extension(path), ".pak")) return std.fmt.bufPrint(buffer, "{s}", .{path}) catch null;
    return std.fmt.bufPrint(buffer, "{s}.pak", .{path}) catch null;
}

// --- Batch mode ----------------------------------------------------------------

/// Which projects a batch takes: one kind, or all 21 (`-1` to BkResBatch).
pub const KindMask = union(enum) {
    all,
    one: Kind,

    pub fn kind(self: KindMask) ?Kind {
        return switch (self) {
            .all => null,
            .one => |k| k,
        };
    }

    pub fn label(self: KindMask) []const u8 {
        return switch (self) {
            .all => "all",
            .one => |k| k.extension(),
        };
    }
};

/// MFC's mask (`*.wpn`) or a bare extension (`wpn`, `.wpn`), or `all` /
/// `*.*` / `*` for every kind.
pub fn parseKindMask(text: []const u8) ?KindMask {
    if (std.ascii.eqlIgnoreCase(text, "all") or std.mem.eql(u8, text, "*.*") or std.mem.eql(u8, text, "*")) return .all;
    const bare = if (std.mem.startsWith(u8, text, "*.")) text[2..] else text;
    const kind = logic.kindFromExtension(bare) orelse return null;
    return .{ .one = kind };
}

/// One batch: MFC's RunBatchExporter arguments.
pub const BatchRequest = struct {
    mask: KindMask = .all,
    src: []const u8,
    dst: []const u8,
    flags: ExportFlags = .{},
};

pub const BatchParse = union(enum) {
    ok: BatchRequest,
    bad: []const u8,
};

/// `--batch <kind|all> <src> <dst> [-f] [-os]` after `--batch`: MFC's
/// `reseditor.exe <*.ext> <src> <dst> [-f] [-os]`. Unlike the MFC, which
/// ignored them, an unknown flag is refused, so a typo never runs a batch
/// that was not asked for.
pub fn parseBatchArgs(args: []const []const u8) BatchParse {
    if (args.len < 3) return .{ .bad = "a batch needs <kind|all> <source folder> <destination folder>" };
    const mask = parseKindMask(args[0]) orelse return .{ .bad = "the batch kind is not a project extension or 'all'" };
    var request: BatchRequest = .{ .mask = mask, .src = args[1], .dst = args[2] };
    for (args[3..]) |flag| {
        if (std.mem.eql(u8, flag, "-f")) {
            request.flags.force = true;
        } else if (std.mem.eql(u8, flag, "-os")) {
            request.flags.open_save = true;
        } else return .{ .bad = "a batch flag is -f or -os" };
    }
    return .{ .ok = request };
}

/// The project files a batch takes from `src`, as BkResBatch finds them:
/// recursively, by extension in any case, ordered by kind (BkResKind order)
/// and then by path. Each path is `src` joined with the file's path below it.
/// Owned by `allocator`; free with `freeProjects`.
pub fn listProjects(io: std.Io, allocator: std.mem.Allocator, src: []const u8, mask: KindMask) ![][]u8 {
    const Entry = struct { kind: Kind, path: []u8 };
    var found: std.ArrayList(Entry) = .empty;
    defer found.deinit(allocator);
    errdefer for (found.items) |entry| allocator.free(entry.path);
    var dir = try std.Io.Dir.cwd().openDir(io, src, .{ .iterate = true });
    defer dir.close(io);
    var walker = try dir.walk(allocator);
    defer walker.deinit();
    const base = trimSeparators(src);
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        const kind = logic.kindFromPath(entry.basename) orelse continue;
        if (mask.kind()) |only| if (only != kind) continue;
        const path = try std.fmt.allocPrint(allocator, "{s}{c}{s}", .{ base, std.fs.path.sep, entry.path });
        try found.append(allocator, .{ .kind = kind, .path = path });
    }
    std.mem.sort(Entry, found.items, {}, struct {
        fn less(_: void, a: Entry, b: Entry) bool {
            if (a.kind != b.kind) return @intFromEnum(a.kind) < @intFromEnum(b.kind);
            return std.mem.lessThan(u8, a.path, b.path);
        }
    }.less);
    const out = try allocator.alloc([]u8, found.items.len);
    for (found.items, out) |entry, *slot| slot.* = entry.path;
    found.clearRetainingCapacity();
    return out;
}

pub fn freeProjects(allocator: std.mem.Allocator, projects: [][]u8) void {
    for (projects) |path| allocator.free(path);
    allocator.free(projects);
}

/// A batch's report (MFC's two message boxes, "Can not export project files"
/// and "Following projects do not have config file (gamma.cfg)", in one):
/// how many projects there were and how many were exported or re-saved, which
/// failed and why, other warnings, and which have no gamma.cfg. A failed
/// project is one BkResBatch named in a warning (`<path>: <reason>`), the
/// way MFC listed errorFiles.
pub const BatchSummary = struct {
    request: BatchRequest,
    status: Status = .ok,
    message_buffer: [512]u8 = undefined,
    message_len: usize = 0,
    project_count: usize = 0,
    report: ExportReport = .{},
    /// Every warning line BkResBatch reported (at most max_report_warnings
    /// kept), split into failed projects and the rest.
    failed: std.ArrayList(Line) = .empty,
    failed_projects: usize = 0,
    other: std.ArrayList([]u8) = .empty,
    missing_gamma: std.ArrayList([]u8) = .empty,

    pub const Line = struct { project: []u8, reason: []u8 };

    pub fn deinit(self: *BatchSummary, allocator: std.mem.Allocator) void {
        for (self.failed.items) |line| {
            allocator.free(line.project);
            allocator.free(line.reason);
        }
        self.failed.deinit(allocator);
        for (self.other.items) |line| allocator.free(line);
        self.other.deinit(allocator);
        for (self.missing_gamma.items) |path| allocator.free(path);
        self.missing_gamma.deinit(allocator);
    }

    pub fn message(self: *const BatchSummary) []const u8 {
        return self.message_buffer[0..self.message_len];
    }

    /// Whether the batch ran over its projects: BkResBatch answers ok when all
    /// went through and failed when some did not, with the report filled either
    /// way; any other answer is a refusal before a project was touched.
    pub fn ran(self: *const BatchSummary) bool {
        return self.status == .ok or self.status == .failed;
    }

    /// Projects that were exported (or, with -os, re-saved) without a failure.
    pub fn succeeded(self: *const BatchSummary) usize {
        return self.project_count - @min(self.project_count, self.failed_projects);
    }

    /// The command line's exit: 0 only when the batch ran and no project
    /// failed. A missing gamma.cfg is reported but, as in MFC, still exports.
    pub fn exitCode(self: *const BatchSummary) u8 {
        return if (self.status == .ok and self.failed_projects == 0) 0 else 1;
    }
};

/// Runs `request` through BkResBatch over `projects` (listProjects' answer)
/// and sorts its warnings into the summary. `files` checks gamma.cfg above
/// each project for an export (not for -os, which exports nothing).
pub fn runBatch(allocator: std.mem.Allocator, b: ResBridge, files: Files, request: BatchRequest, projects: []const []const u8) !BatchSummary {
    var summary: BatchSummary = .{ .request = request, .project_count = projects.len };
    errdefer summary.deinit(allocator);
    var warnings: [max_report_warnings]Warning = undefined;
    summary.status = b.batch(request.mask.kind(), request.src, request.dst, request.flags, &summary.report, &warnings);
    const text = b.lastMessage();
    summary.message_len = @min(text.len, summary.message_buffer.len);
    @memcpy(summary.message_buffer[0..summary.message_len], text[0..summary.message_len]);
    if (!summary.ran()) return summary;

    var named = try allocator.alloc(bool, projects.len);
    defer allocator.free(named);
    @memset(named, false);
    for (warnings[0..@min(summary.report.warning_total, max_report_warnings)]) |*warning| {
        const line = warning.textSlice();
        const owner = for (projects, 0..) |project, i| {
            if (line.len > project.len + 2 and std.mem.startsWith(u8, line, project) and std.mem.startsWith(u8, line[project.len..], ": ")) break i;
        } else null;
        if (owner) |i| {
            const project = try allocator.dupe(u8, projects[i]);
            errdefer allocator.free(project);
            const reason = try allocator.dupe(u8, line[projects[i].len + 2 ..]);
            errdefer allocator.free(reason);
            try summary.failed.append(allocator, .{ .project = project, .reason = reason });
            if (!named[i]) summary.failed_projects += 1;
            named[i] = true;
        } else {
            const copy = try allocator.dupe(u8, line);
            errdefer allocator.free(copy);
            try summary.other.append(allocator, copy);
        }
    }
    if (!request.flags.open_save) {
        for (projects) |project| {
            var buffer: [logic.path_capacity]u8 = undefined;
            const folder = std.fs.path.dirname(project) orelse ".";
            if (findGammaCfg(files, folder, &buffer) != null) continue;
            const copy = try allocator.dupe(u8, project);
            errdefer allocator.free(copy);
            try summary.missing_gamma.append(allocator, copy);
        }
    }
    return summary;
}

pub fn formatBatchReport(summary: *const BatchSummary, writer: *std.Io.Writer) std.Io.Writer.Error!void {
    const request = summary.request;
    const verb = if (request.flags.open_save) "open and save" else "export";
    try writer.print("batch {s} {s}: {s} -> {s}{s}\n", .{ verb, request.mask.label(), request.src, request.dst, if (request.flags.force) " (forced)" else "" });
    if (!summary.ran()) {
        try writer.print("refused: {s}\n", .{summary.message()});
        return;
    }
    try writer.print("projects {d}, {s} {d}, failed {d}; files written {d}, skipped {d}\n", .{
        summary.project_count,
        if (request.flags.open_save) "re-saved" else "exported",
        summary.succeeded(),
        summary.failed_projects,
        summary.report.written,
        summary.report.skipped,
    });
    if (summary.failed.items.len != 0) {
        try writer.print("failed:\n", .{});
        for (summary.failed.items) |line| try writer.print("  {s}: {s}\n", .{ line.project, line.reason });
    }
    if (summary.other.items.len != 0) {
        try writer.print("warnings:\n", .{});
        for (summary.other.items) |line| try writer.print("  {s}\n", .{line});
    }
    if (summary.report.warning_total > max_report_warnings) try writer.print("  ... and {d} more warnings\n", .{summary.report.warning_total - max_report_warnings});
    if (summary.missing_gamma.items.len != 0) {
        try writer.print("missing gamma.cfg ({d}):\n", .{summary.missing_gamma.items.len});
        for (summary.missing_gamma.items) |path| try writer.print("  {s}\n", .{path});
    }
}

// --- Run Blitzkrieg ---------------------------------------------------------------

/// The -mod= folder name Game takes for the export folder: its last path
/// component, when the folder sits directly in `<base_root>mods/` (the only
/// place the game looks for a mod by name). Null otherwise. MFC passed the
/// last component unconditionally (and, by a slip, an empty one when the
/// folder ended in a separator).
pub fn modFolderForGame(export_dir: []const u8, base_root: []const u8) ?[]const u8 {
    const dir = trimSeparators(export_dir);
    if (dir.len == 0) return null;
    const name = std.fs.path.basename(dir);
    if (name.len == 0) return null;
    const parent = std.fs.path.dirname(dir) orelse return null;
    var expected_buffer: [logic.path_capacity]u8 = undefined;
    const expected = joinFile(&expected_buffer, base_root, "mods") orelse return null;
    if (!samePath(parent, expected)) return null;
    return name;
}

/// Case- and separator-insensitive, trailing separators ignored: the engine's
/// roots and a dialog's answer may differ in both.
fn samePath(a: []const u8, b: []const u8) bool {
    const x = trimSeparators(a);
    const y = trimSeparators(b);
    if (x.len != y.len) return false;
    for (x, y) |p, q| {
        const p_sep = p == '/' or p == '\\';
        const q_sep = q == '/' or q == '\\';
        if (p_sep != q_sep) return false;
        if (!p_sep and std.ascii.toLower(p) != std.ascii.toLower(q)) return false;
    }
    return true;
}

/// Game in Set Directories' game folder, or `installed` (testlaunch.gamePath,
/// the Game beside the editor) when that is empty.
pub fn gamePath(buffer: []u8, game_folder: []const u8, installed: []const u8, windows: bool) ?[]const u8 {
    if (game_folder.len == 0) return std.fmt.bufPrint(buffer, "{s}", .{installed}) catch null;
    return joinFile(buffer, game_folder, if (windows) "Game.exe" else "Game");
}

/// F7's launch: Game with this editor's test profile, the export folder's
/// mod, Set Directories' extra arguments and no map. The kit's -editor-test
/// keeps the profile session-only and cloud sync off.
pub fn runGameOptions(game_path: []const u8, mod_folder: []const u8, settings: *const app_settings.Settings, log_path: []const u8) testlaunch.Options {
    return .{
        .game_path = game_path,
        .mod_folder = mod_folder,
        .log_path = log_path,
        .game_parameters = settings.gameParameters(),
        .profile = test_profile,
        .map_name = null,
    };
}

// --- Editors menu ---------------------------------------------------------------

/// MFC saved the active frame ("Active Frame") at exit, whichever way it
/// became active - the Editors menu, New or Open. True when the settings
/// changed. The GUI frame has no menu entry and is not remembered.
pub fn rememberActive(settings: *app_settings.Settings, active: Kind) bool {
    if (logic.menuEntry(active) == null) return false;
    const value: i32 = @intFromEnum(active);
    if (settings.last_editor == value) return false;
    settings.last_editor = value;
    return true;
}

// --- Shortcuts ---------------------------------------------------------------

pub const ToolAction = enum { mod_settings, export_result, export_stats_only, set_directories, batch_mode, run_game };

pub const ShortcutKey = enum { m, e, r, t, b, f7 };

/// MFC's accelerators for these commands (editor.rc): Ctrl+M, Ctrl+E,
/// Ctrl+R, Ctrl+T, Ctrl+B and F7 alone.
pub fn shortcutFor(ctrl: bool, shift: bool, key: ShortcutKey) ?ToolAction {
    if (shift) return null;
    if (key == .f7) return if (ctrl) null else .run_game;
    if (!ctrl) return null;
    return switch (key) {
        .m => .mod_settings,
        .e => .export_result,
        .r => .export_stats_only,
        .t => .set_directories,
        .b => .batch_mode,
        .f7 => unreachable,
    };
}

// --- Tests ----------------------------------------------------------------------

const testing = std.testing;
const FakeResBridge = core.fake_bridge.FakeResBridge;
const FakeFiles = kit.files.FakeFiles;

fn native(comptime path: []const u8) []const u8 {
    if (std.fs.path.sep == '/') return path;
    comptime var out: [path.len]u8 = undefined;
    inline for (path, 0..) |byte, i| out[i] = if (byte == '/') '\\' else byte;
    const final = out;
    return &final;
}

test "every kind has its MFC folder; the GUI frame none" {
    try testing.expectEqualStrings("weapons/", kindFolder(.weapon));
    try testing.expectEqualStrings("units/humans/", kindFolder(.animation_infantry));
    try testing.expectEqualStrings("terrain/sets/", kindFolder(.river_3d));
    try testing.expectEqualStrings("", kindFolder(.gui_frame));
    inline for (std.enums.values(Kind)) |kind| {
        const folder = kindFolder(kind);
        if (kind != .gui_frame) try testing.expect(std.mem.endsWith(u8, folder, "/"));
    }
}

test "the engine's gamma correction: neutral is identity, each control moves the ramp its way" {
    var v: u16 = 0;
    while (v < 256) : (v += 1) try testing.expectEqual(@as(u8, @intCast(v)), correctChannel(@intCast(v), .{}));
    // brightness 1 -> +0.5: 0 becomes 127 (0.5 * 255 cut), the top stays 255.
    try testing.expectEqual(@as(u8, 127), correctChannel(0, .{ .brightness = 1 }));
    try testing.expectEqual(@as(u8, 255), correctChannel(255, .{ .brightness = 1 }));
    try testing.expectEqual(@as(u8, 0), correctChannel(255, .{ .brightness = -1 }) -| 128);
    // Contrast 1: slope 3 about the middle, so a quarter grey goes black.
    try testing.expectEqual(@as(u8, 0), correctChannel(64, .{ .contrast = 1 }));
    try testing.expectEqual(@as(u8, 255), correctChannel(192, .{ .contrast = 1 }));
    // Gamma > 0 brightens the middle, gamma < 0 darkens it; the ends stay.
    try testing.expect(correctChannel(128, .{ .gamma = 1 }) > 128);
    try testing.expect(correctChannel(128, .{ .gamma = -1 }) < 128);
    try testing.expectEqual(@as(u8, 255), correctChannel(255, .{ .gamma = 1 }));
    // Out of range is clamped as the engine clamps.
    try testing.expectEqual(correctChannel(100, .{ .gamma = 1 }), correctChannel(100, .{ .gamma = 7 }));
}

test "gamma.cfg: the engine's layout round-trips, attributes and child elements both read" {
    var buffer: [256]u8 = undefined;
    const text = formatGammaCfg(&buffer, .{ .brightness = 0.25, .contrast = -0.5, .gamma = 2 }).?;
    try testing.expectEqualStrings("<?xml version=\"1.0\"?>\r\n<base Brightness=\"0.25\" Contrast=\"-0.5\" Gamma=\"1\"/>\r\n", text);
    const back = parseGammaCfg(text);
    try testing.expectEqual(@as(f32, 0.25), back.brightness);
    try testing.expectEqual(@as(f32, -0.5), back.contrast);
    try testing.expectEqual(@as(f32, 1), back.gamma);

    const elements = parseGammaCfg("<?xml version=\"1.0\"?>\n<base><Brightness>0.1</Brightness>\n<Contrast> 0.2 </Contrast><Gamma>x</Gamma></base>");
    try testing.expectEqual(@as(f32, 0.1), elements.brightness);
    try testing.expectEqual(@as(f32, 0.2), elements.contrast);
    try testing.expectEqual(@as(f32, 0), elements.gamma);
    const empty = parseGammaCfg("");
    try testing.expect(empty.isNeutral());
}

test "findGammaCfg searches the folder, then each one above it" {
    var files = FakeFiles.init(testing.allocator);
    defer files.deinit();
    try files.write(native("/src/units/gamma.cfg"), "x");
    var buffer: [256]u8 = undefined;
    try testing.expectEqualStrings(native("/src/units/gamma.cfg"), findGammaCfg(files.files(), native("/src/units/humans/rifleman"), &buffer).?);
    try testing.expectEqualStrings(native("/src/units/gamma.cfg"), findGammaCfg(files.files(), native("/src/units/"), &buffer).?);
    try testing.expect(findGammaCfg(files.files(), native("/src/weapons"), &buffer) == null);
    try files.write(native("/gamma.cfg"), "x");
    try testing.expectEqualStrings(native("/gamma.cfg"), findGammaCfg(files.files(), native("/src/weapons"), &buffer).?);
    try testing.expect(findGammaCfg(files.files(), "", &buffer) == null);
}

test "Picture Options writes beside the project or into the sub-editor's source folder" {
    var buffer: [256]u8 = undefined;
    switch (gammaTarget(&buffer, true, native("/p/rifle/project.unt"), "", .animation_infantry)) {
        .path => |p| try testing.expectEqualStrings(native("/p/rifle/gamma.cfg"), p),
        else => return error.TestUnexpectedResult,
    }
    try testing.expect(gammaTarget(&buffer, true, null, "/src", .weapon) == .untitled);
    try testing.expect(gammaTarget(&buffer, false, native("/p/x.wpn"), "", .weapon) == .no_source_folder);
    switch (gammaTarget(&buffer, false, null, native("/src/"), .animation_infantry)) {
        .path => |p| try testing.expectEqualStrings(native("/src/units/humans/gamma.cfg"), p),
        else => return error.TestUnexpectedResult,
    }
    switch (gammaTarget(&buffer, false, null, native("/src"), .gui_frame)) {
        .path => |p| try testing.expectEqualStrings(native("/src/gamma.cfg"), p),
        else => return error.TestUnexpectedResult,
    }
}

test "Export refuses by the app's own path: no project, untitled, a recovery copy" {
    const allocator = testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    var life: logic.Lifecycle = .{};
    defer life.deinit(allocator);
    var outcome: ExportOutcome = undefined;

    runExport(fake.bridge(), &life, false, false, true, &outcome);
    try testing.expectEqual(ExportBlock.no_project, outcome.blocked.?);
    try life.newProject(allocator, fake.bridge(), .weapon);
    runExport(fake.bridge(), &life, false, false, true, &outcome);
    try testing.expectEqual(ExportBlock.untitled, outcome.blocked.?);
    try testing.expectEqual(@as(u32, 0), fake.exports);
    try life.saveProject(allocator, fake.bridge(), "p/gun.wpn");
    runExport(fake.bridge(), &life, true, false, true, &outcome);
    try testing.expectEqual(ExportBlock.recovery_copy, outcome.blocked.?);
    try testing.expectEqual(@as(u32, 0), fake.exports);

    var text_buffer: [512]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&text_buffer);
    try formatExportReport(&outcome, &writer);
    try testing.expect(std.mem.startsWith(u8, writer.buffered(), "Export Result refused: this is a recovery copy"));
}

test "Export and Export Stats Only report written, skipped, warnings and a missing gamma.cfg" {
    const allocator = testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    var life: logic.Lifecycle = .{};
    defer life.deinit(allocator);
    try life.newProject(allocator, fake.bridge(), .weapon);
    try life.saveProject(allocator, fake.bridge(), "p/gun.wpn");

    var outcome: ExportOutcome = undefined;
    // The fake has no exporter for wpn until one is registered: refused,
    // with the bridge's reason.
    runExport(fake.bridge(), &life, false, false, false, &outcome);
    try testing.expectEqual(Status.refused, outcome.status);
    try testing.expect(std.mem.indexOf(u8, outcome.message(), "not ported") != null);

    fake.setExportable(.weapon, true);
    runExport(fake.bridge(), &life, false, true, false, &outcome);
    try testing.expectEqual(Status.ok, outcome.status);
    try testing.expect(fake.last_stats_only);
    try testing.expectEqual(@as(i32, 1), outcome.report.written);
    var text_buffer: [512]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&text_buffer);
    try formatExportReport(&outcome, &writer);
    try testing.expectEqualStrings("Export Stats Only: written 1, skipped 0, warnings 0\nno gamma.cfg in the project's folder or above it: picture options are 0\n", writer.buffered());

    runExport(fake.bridge(), &life, false, false, true, &outcome);
    try testing.expect(!fake.last_stats_only);
    var plain: std.Io.Writer = .fixed(&text_buffer);
    try formatExportReport(&outcome, &plain);
    try testing.expectEqualStrings("Export Result: written 1, skipped 0, warnings 0\n", plain.buffered());
}

test "batch arguments: MFC's mask or an extension, all, -f and -os; anything else is refused" {
    switch (parseBatchArgs(&.{ "*.wpn", "src", "dst", "-f" })) {
        .ok => |r| {
            try testing.expectEqual(Kind.weapon, r.mask.kind().?);
            try testing.expect(r.flags.force and !r.flags.open_save);
            try testing.expectEqualStrings("src", r.src);
            try testing.expectEqualStrings("dst", r.dst);
        },
        .bad => return error.TestUnexpectedResult,
    }
    switch (parseBatchArgs(&.{ "all", "a", "b", "-os", "-f" })) {
        .ok => |r| {
            try testing.expect(r.mask == .all);
            try testing.expect(r.flags.force and r.flags.open_save);
        },
        .bad => return error.TestUnexpectedResult,
    }
    try testing.expectEqual(Kind.river_3d, parseKindMask(".3RV").?.kind().?);
    try testing.expect(parseKindMask("*.*").? == .all);
    try testing.expect(parseKindMask("bzm") == null);
    try testing.expect(parseBatchArgs(&.{ "wpn", "a" }) == .bad);
    try testing.expect(parseBatchArgs(&.{ "xyz", "a", "b" }) == .bad);
    try testing.expect(parseBatchArgs(&.{ "wpn", "a", "b", "-force" }) == .bad);
}

test "a batch report lists exported, failed with reasons, other warnings and missing gamma.cfg" {
    const allocator = testing.allocator;
    var fake = FakeResBridge.init(allocator);
    defer fake.deinit();
    var files = FakeFiles.init(allocator);
    defer files.deinit();
    const gun = native("/src/weapons/gun.wpn");
    const rifle = native("/src/units/humans/rifle.unt");
    const tank = native("/src/units/technics/tank.msh");
    try fake.addBatchProject(gun, .weapon);
    try fake.addBatchProject(rifle, .animation_infantry);
    try fake.addBatchProject(tank, .mesh_unit);
    fake.setExportable(.weapon, true);
    try files.write(native("/src/units/humans/gamma.cfg"), "x");
    const projects = [_][]const u8{ gun, rifle, tank };

    var summary = try runBatch(allocator, fake.bridge(), files.files(), .{ .src = native("/src"), .dst = "/out" }, &projects);
    defer summary.deinit(allocator);
    try testing.expectEqual(Status.ok, summary.status);
    try testing.expectEqual(@as(usize, 3), summary.project_count);
    try testing.expectEqual(@as(usize, 2), summary.failed_projects);
    try testing.expectEqual(@as(usize, 1), summary.succeeded());
    try testing.expectEqual(@as(u8, 1), summary.exitCode());
    try testing.expectEqual(@as(usize, 2), summary.missing_gamma.items.len);
    try testing.expectEqualStrings(gun, summary.missing_gamma.items[0]);
    try testing.expectEqualStrings(tank, summary.missing_gamma.items[1]);

    var text_buffer: [2048]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&text_buffer);
    try formatBatchReport(&summary, &writer);
    const text = writer.buffered();
    try testing.expect(std.mem.indexOf(u8, text, "projects 3, exported 1, failed 2") != null);
    try testing.expect(std.mem.indexOf(u8, text, "failed:\n") != null);
    try testing.expect(std.mem.indexOf(u8, text, "exporting .unt projects is not ported yet") != null);
    try testing.expect(std.mem.indexOf(u8, text, "missing gamma.cfg (2):\n") != null);

    // -os re-saves everything, exports nothing, and so checks no gamma.cfg.
    var resave = try runBatch(allocator, fake.bridge(), files.files(), .{ .src = native("/src"), .dst = "/out", .flags = .{ .open_save = true } }, &projects);
    defer resave.deinit(allocator);
    try testing.expectEqual(@as(usize, 3), resave.succeeded());
    try testing.expectEqual(@as(u8, 0), resave.exitCode());
    try testing.expectEqual(@as(usize, 0), resave.missing_gamma.items.len);

    // Into the shipped Data folder is refused before anything runs.
    fake.shipped_root = "/game";
    var refused = try runBatch(allocator, fake.bridge(), files.files(), .{ .src = native("/src"), .dst = "/game/Data" }, &projects);
    defer refused.deinit(allocator);
    try testing.expectEqual(Status.refused, refused.status);
    try testing.expectEqual(@as(u8, 1), refused.exitCode());
}

test "Run Blitzkrieg: the export folder's mod name only under <base>mods, this editor's profile, no map" {
    try testing.expectEqualStrings("mymod", modFolderForGame(native("/game/mods/mymod/"), native("/game/")).?);
    try testing.expectEqualStrings("My Mod", modFolderForGame(native("/Game/Mods/My Mod"), native("/game/")).?);
    try testing.expect(modFolderForGame(native("/elsewhere/mymod"), native("/game/")) == null);
    try testing.expect(modFolderForGame(native("/game/mods"), native("/game/")) == null);
    try testing.expect(modFolderForGame("", native("/game/")) == null);

    var buffer: [128]u8 = undefined;
    try testing.expectEqualStrings("/stage/Game", gamePath(&buffer, "", "/stage/Game", false).?);
    try testing.expectEqualStrings(native("/opt/bk/Game.exe"), gamePath(&buffer, native("/opt/bk/"), "/stage/Game", true).?);

    var settings: app_settings.Settings = .{};
    settings.setGameParameters("-nosound");
    const options = runGameOptions("/stage/Game", "mymod", &settings, "log");
    var storage: testlaunch.ArgvStorage = .{};
    const argv = testlaunch.buildArgv(&storage, options);
    try testing.expectEqualStrings("-profile=ResourceEditorTest", argv[2]);
    try testing.expectEqualStrings("-mod=mymod", argv[3]);
    try testing.expectEqualStrings("-nosound", argv[argv.len - 1]);
}

test "the active sub-editor is remembered however it became active, the GUI frame never" {
    var settings: app_settings.Settings = .{};
    try testing.expect(rememberActive(&settings, .medal));
    try testing.expectEqual(@as(i32, @intFromEnum(Kind.medal)), settings.last_editor.?);
    try testing.expect(!rememberActive(&settings, .medal));
    try testing.expect(!rememberActive(&settings, .gui_frame));
    try testing.expectEqual(Kind.medal, logic.restoreEditor(settings.last_editor));
}

test "the Tools accelerators are MFC's" {
    try testing.expectEqual(ToolAction.mod_settings, shortcutFor(true, false, .m).?);
    try testing.expectEqual(ToolAction.export_result, shortcutFor(true, false, .e).?);
    try testing.expectEqual(ToolAction.export_stats_only, shortcutFor(true, false, .r).?);
    try testing.expectEqual(ToolAction.set_directories, shortcutFor(true, false, .t).?);
    try testing.expectEqual(ToolAction.batch_mode, shortcutFor(true, false, .b).?);
    try testing.expectEqual(ToolAction.run_game, shortcutFor(false, false, .f7).?);
    try testing.expect(shortcutFor(false, false, .m) == null);
    try testing.expect(shortcutFor(true, true, .e) == null);
    try testing.expect(shortcutFor(true, false, .f7) == null);
    var pak_buffer: [64]u8 = undefined;
    try testing.expectEqualStrings(native("/a/b.pak"), withPakExtension(&pak_buffer, native("/a/b")).?);
    try testing.expectEqualStrings("b.PAK", withPakExtension(&pak_buffer, "b.PAK").?);
}
