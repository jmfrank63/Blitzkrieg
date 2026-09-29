const std = @import("std");
const runtime_verify = @import("verify_runtime.zig");

pub const DataMode = enum { copy, link };

pub const RuntimeLayout = struct {
    game_name: []const u8,
    runtime_files: []const []const u8,
    debug_files: []const []const u8,
    metadata_files: []const []const u8 = &runtime_verify.required_metadata_files,
    editors_supported: bool,
};

pub const Options = struct {
    repo_root: []const u8,
    install_dir: []const u8,
    data_mode: DataMode = .copy,
    include_editors: bool = false,
    editors_only: bool = false,
    // D-08: a path (absolute, or relative to repo_root) to a built MapEditor
    // binary. When set, it is copied into the staged root under its own base
    // name (MapEditor or MapEditor.exe) after the runtime files, and required
    // present by verifyStagedPayload - so a package missing it fails to stage
    // rather than shipping without the editor test-launch expects beside it.
    map_editor: ?[]const u8 = null,
    // The generated winter/Africa unit textures (tools/zig/season_textures.zig):
    // a directory (absolute, or relative to repo_root) laid out like Data,
    // synced to <install>/SeasonData, which the engine mounts over Data
    // (Sources/src/StreamIO/SeasonData.h). A build output rather than part of
    // Data, so it is staged the same way under --link-data, where Data is a
    // link into the repository and must not be written.
    season_data: ?[]const u8 = null,
    layout: RuntimeLayout = .{
        .game_name = "Game.exe",
        .runtime_files = &.{},
        .debug_files = &.{},
        .editors_supported = true,
    },
};

pub fn main(init: std.process.Init) !void {
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.skip();
    var options = try parseArgs(&args, init.gpa);
    defer options.deinit(init.gpa);
    try stage(init.io, init.gpa, options.value);
}

const ParsedOptions = struct {
    value: Options,
    runtime_files: std.ArrayList([]const u8),
    debug_files: std.ArrayList([]const u8),
    metadata_files: std.ArrayList([]const u8),

    fn deinit(self: *ParsedOptions, allocator: std.mem.Allocator) void {
        self.runtime_files.deinit(allocator);
        self.debug_files.deinit(allocator);
        self.metadata_files.deinit(allocator);
    }
};

fn parseArgs(args: *std.process.Args.Iterator, allocator: std.mem.Allocator) !ParsedOptions {
    var runtime_files = std.ArrayList([]const u8).empty;
    errdefer runtime_files.deinit(allocator);
    var debug_files = std.ArrayList([]const u8).empty;
    errdefer debug_files.deinit(allocator);
    var metadata_files = std.ArrayList([]const u8).empty;
    errdefer metadata_files.deinit(allocator);

    var options = Options{
        .repo_root = args.next() orelse return error.InvalidArguments,
        .install_dir = args.next() orelse return error.InvalidArguments,
    };
    var game_name: []const u8 = options.layout.game_name;
    var editors_supported = false;
    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--copy-data")) {
            options.data_mode = .copy;
        } else if (std.mem.eql(u8, arg, "--link-data")) {
            options.data_mode = .link;
        } else if (std.mem.eql(u8, arg, "--include-editors")) {
            options.include_editors = true;
        } else if (std.mem.eql(u8, arg, "--editors-only")) {
            options.editors_only = true;
        } else if (std.mem.eql(u8, arg, "--editors-supported")) {
            editors_supported = true;
        } else if (std.mem.eql(u8, arg, "--game-name")) {
            game_name = args.next() orelse return error.InvalidArguments;
        } else if (std.mem.eql(u8, arg, "--runtime-file")) {
            try runtime_files.append(allocator, args.next() orelse return error.InvalidArguments);
        } else if (std.mem.eql(u8, arg, "--debug-file")) {
            try debug_files.append(allocator, args.next() orelse return error.InvalidArguments);
        } else if (std.mem.eql(u8, arg, "--metadata-file")) {
            try metadata_files.append(allocator, args.next() orelse return error.InvalidArguments);
        } else if (std.mem.eql(u8, arg, "--map-editor")) {
            options.map_editor = args.next() orelse return error.InvalidArguments;
        } else if (std.mem.eql(u8, arg, "--season-data")) {
            options.season_data = args.next() orelse return error.InvalidArguments;
        } else {
            return error.InvalidArguments;
        }
    }
    const selected_metadata_files = if (metadata_files.items.len == 0) &runtime_verify.required_metadata_files else metadata_files.items;
    options.layout = .{
        .game_name = game_name,
        .runtime_files = runtime_files.items,
        .debug_files = debug_files.items,
        .metadata_files = selected_metadata_files,
        .editors_supported = editors_supported,
    };
    return .{ .value = options, .runtime_files = runtime_files, .debug_files = debug_files, .metadata_files = metadata_files };
}

pub fn stage(io: std.Io, allocator: std.mem.Allocator, options: Options) !void {
    const cwd = std.Io.Dir.cwd();
    var repo = try cwd.openDir(io, options.repo_root, .{ .access_sub_paths = true });
    defer repo.close(io);
    try cwd.createDirPath(io, options.install_dir);
    var destination = try cwd.openDir(io, options.install_dir, .{ .iterate = true, .access_sub_paths = true });
    defer destination.close(io);

    rejectStaleImages(io, destination) catch |err| return failStep("reject stale runtime images", err);
    if (!options.editors_only) {
        var binaries = try repo.openDir(io, "zig-out/bin", .{ .iterate = true });
        defer binaries.close(io);
        var libraries: ?std.Io.Dir = repo.openDir(io, "zig-out/lib", .{ .iterate = true }) catch |err| switch (err) {
            error.FileNotFound => null,
            else => return failStep("open runtime libraries", err),
        };
        defer if (libraries) |*dir| dir.close(io);
        copyGameRuntime(io, binaries, libraries, destination, options.layout) catch |err| return failStep("copyGameRuntime", err);
        if (options.map_editor) |map_editor_path| {
            copyMapEditor(io, repo, map_editor_path, destination) catch |err| return failStep("copyMapEditor", err);
        }
        copyShaderAssets(io, allocator, repo, destination) catch |err| return failStep("copyShaderAssets", err);
        seedConfigIfMissing(io, repo, destination) catch |err| return failStep("seed config.cfg", err);
        copyFile(io, repo, "Data/Configs/defconf.cfg", destination, "defconf.cfg") catch |err| return failStep("copy defconf.cfg", err);
        copyMetadata(io, repo, destination, options.layout.metadata_files) catch |err| return failStep("copy package metadata", err);
        // rclone is MIT: the copy of its binary staged beside the game has to
        // travel with its copyright line and permission notice, and its
        // official archive carries no COPYING to stage. The notice is a file we
        // own and review, staged at the root beside LICENSE.md rather than left
        // in Data, so it sits with the licences a player looks for and stays
        // present under --link-data, which replaces the staged Data tree with a
        // link into the repository.
        copyFile(io, repo, runtime_verify.third_party_notices_source, destination, runtime_verify.third_party_notices_name) catch |err| return failStep("copy third-party notices", err);
        destination.createDirPath(io, "saves") catch |err| return failStep("create saves dir", err);
        switch (options.data_mode) {
            .copy => syncData(io, allocator, repo, destination) catch |err| return failStep("syncData", err),
            .link => {
                removeTreeIfPresent(io, destination, "Data") catch |err| return failStep("remove staged Data", err);
                linkData(io, allocator, repo, destination) catch |err| return failStep("linkData", err);
            },
        }
        if (options.season_data) |season_data_path| {
            syncSeasonData(io, allocator, repo, season_data_path, destination) catch |err| return failStep("syncSeasonData", err);
        }
        verifyStagedPayload(io, destination, options) catch |err| return failStep("verify staged payload", err);
    } else if (!options.layout.editors_supported) {
        return error.EditorsUnsupported;
    }

    try removeTreeIfPresent(io, destination, "Editors");
    if (options.include_editors) {
        if (!options.layout.editors_supported) return error.EditorsUnsupported;
        try copyEditors(io, repo, destination);
    }
}

/// Everything the staged layout promises to hold, checked once where it is
/// produced, so a package that is wrong is wrong here rather than at a player's
/// install. The bundled rclone is one of the runtime files, so the binary and
/// the third-party notice MIT requires beside it are asserted together: neither
/// may ship without the other. When options.map_editor is set (D-08), its
/// staged base name is required too - copyMapEditor already fails the stage if
/// the source is missing, but a layout that skipped the copy for any other
/// reason should still be caught here, the same way every other promised file
/// is.
fn verifyStagedPayload(io: std.Io, destination: std.Io.Dir, options: Options) !void {
    const layout = options.layout;
    try requireStagedFile(io, destination, layout.game_name);
    for (layout.runtime_files) |name| try requireStagedFile(io, destination, name);
    for (layout.metadata_files) |name| try requireStagedFile(io, destination, name);
    try requireStagedFile(io, destination, runtime_verify.third_party_notices_name);
    if (options.map_editor) |map_editor_path| {
        try requireStagedFile(io, destination, std.fs.path.basename(map_editor_path));
    }
    if (options.season_data != null) try requireStagedFile(io, destination, season_data_dir);
}

/// Where the staged layout keeps the generated season textures, beside Data.
/// The engine's name for it is NPlatform::Paths::SeasonDataRoot().
pub const season_data_dir = "SeasonData";

/// Syncs the generated season textures into <install>/SeasonData the way Data
/// is synced: changed files copied, files the generation no longer makes
/// removed, current ones left alone.
fn syncSeasonData(io: std.Io, allocator: std.mem.Allocator, repo: std.Io.Dir, source_path: []const u8, destination: std.Io.Dir) !void {
    const source_root = if (std.fs.path.isAbsolute(source_path)) std.Io.Dir.cwd() else repo;
    var source = source_root.openDir(io, source_path, .{ .iterate = true }) catch |err| {
        std.debug.print("stage: season data '{s}' could not be opened: {s}\n", .{ source_path, @errorName(err) });
        return err;
    };
    defer source.close(io);
    try destination.createDirPath(io, season_data_dir);
    var staged = try destination.openDir(io, season_data_dir, .{ .iterate = true, .access_sub_paths = true });
    defer staged.close(io);
    try syncTree(io, allocator, source, staged, .contents);
}

/// Copies the built MapEditor binary (D-08) into the staged root under its own
/// base name, beside Game. `source_path` may be absolute (the common case: the
/// package steps pass a build-system-resolved cache path) or relative to
/// `repo`; either way, a missing source fails naming the path, rather than
/// silently shipping a package with no editor.
fn copyMapEditor(io: std.Io, repo: std.Io.Dir, source_path: []const u8, destination: std.Io.Dir) !void {
    const base_name = std.fs.path.basename(source_path);
    const source_dir = if (std.fs.path.isAbsolute(source_path)) std.Io.Dir.cwd() else repo;
    copyFile(io, source_dir, source_path, destination, base_name) catch |err| {
        std.debug.print("stage: map editor '{s}' could not be staged: {s}\n", .{ source_path, @errorName(err) });
        return error.MissingMapEditor;
    };
}

fn requireStagedFile(io: std.Io, destination: std.Io.Dir, name: []const u8) !void {
    destination.access(io, name, .{}) catch |err| {
        std.debug.print("stage: staged layout is missing '{s}': {s}\n", .{ name, @errorName(err) });
        return error.MissingStagedFile;
    };
}

fn failStep(step: []const u8, err: anyerror) anyerror {
    std.debug.print("stage: step '{s}' failed: {s}\n", .{ step, @errorName(err) });
    return err;
}

pub fn shouldReplaceRuntime(name: []const u8) bool {
    return !std.mem.endsWith(u8, name, ".stale");
}

/// SDL's Linux shared object is emitted with a versioned filename and a
/// symlink chain. Stage the SONAME file as a regular file so the package does
/// not depend on symlink preservation by the host filesystem.
pub fn runtimeSourceName(name: []const u8) []const u8 {
    if (std.mem.eql(u8, name, "libSDL3.so.0")) return "libSDL3.so.0.4.0";
    return name;
}

pub fn classifyDataLinkError(err: anyerror) anyerror {
    return switch (err) {
        error.AccessDenied, error.PermissionDenied => error.DataLinkPermissionDenied,
        else => err,
    };
}

fn rejectStaleImages(io: std.Io, destination: std.Io.Dir) !void {
    var iterator = destination.iterate();
    while (try iterator.next(io)) |entry| {
        if (entry.kind == .file and std.mem.endsWith(u8, entry.name, ".stale")) {
            std.debug.print("stage: stale runtime image '{s}' remains; close the game and remove it before rebuilding\n", .{entry.name});
            return error.StaleRuntimeImage;
        }
    }
}

fn copyGameRuntime(io: std.Io, binaries: std.Io.Dir, libraries: ?std.Io.Dir, destination: std.Io.Dir, layout: RuntimeLayout) !void {
    const stale_root_files = [_][]const u8{ "BetaKeyGen.exe", "BuildVersion.exe", "FontGen.exe", "A7ExportModel.dll", "fmod.dll", "mfc42.dll", "msvcp60.dll", "msvcrt.dll" };
    for (stale_root_files) |name| destination.deleteFile(io, name) catch {};
    for (layout.runtime_files) |name| {
        destination.deleteFile(io, name) catch {};
        if (!shouldReplaceRuntime(name)) continue;
        copyRuntimeFile(io, binaries, libraries, name, destination) catch |err| switch (err) {
            error.AccessDenied, error.PermissionDenied, error.FileBusy => {
                var aside_buf: [256]u8 = undefined;
                const aside = std.fmt.bufPrint(&aside_buf, "{s}.stale", .{name}) catch return err;
                destination.deleteFile(io, aside) catch {};
                destination.rename(name, destination, aside, io) catch |rename_err| {
                    std.debug.print("stage: could not replace locked '{s}': {s} — close the running game and rebuild\n", .{ name, @errorName(rename_err) });
                    return error.RuntimeReplacementDenied;
                };
                copyRuntimeFile(io, binaries, libraries, name, destination) catch |copy_err| {
                    std.debug.print("stage: fresh copy of '{s}' failed after move-aside: {s}\n", .{ name, @errorName(copy_err) });
                    return copy_err;
                };
            },
            else => return err,
        };
    }
    for (layout.debug_files) |name| {
        destination.deleteFile(io, name) catch {};
        copyFile(io, binaries, name, destination, name) catch |err| switch (err) {
            error.FileNotFound => {},
            else => return err,
        };
    }
}

/// A runtime file in neither zig-out/bin nor zig-out/lib fails naming the
/// file. It used to surface as a bare FileNotFound from copyGameRuntime, which
/// is all a Windows --release=fast package run said when its staging raced
/// the installs that write those directories (see addStageGameRun in
/// build.zig).
fn copyRuntimeFile(io: std.Io, binaries: std.Io.Dir, libraries: ?std.Io.Dir, name: []const u8, destination: std.Io.Dir) !void {
    const source = runtimeSourceName(name);
    copyFile(io, binaries, source, destination, name) catch |err| {
        if (err != error.FileNotFound) return err;
        if (libraries) |lib_dir| {
            copyFile(io, lib_dir, source, destination, name) catch |lib_err| {
                if (lib_err != error.FileNotFound) return lib_err;
                return missingRuntimeFile(source);
            };
            return;
        }
        return missingRuntimeFile(source);
    };
}

fn missingRuntimeFile(source: []const u8) anyerror {
    std.debug.print("stage: runtime file '{s}' is in neither zig-out/bin nor zig-out/lib; build game-all first\n", .{source});
    return error.MissingRuntimeFile;
}

/// Staging used to delete the staged Data tree and copy all of it back. On
/// Windows a deleted file lingers in delete-pending for as long as any handle
/// to it is open - a file indexer or a virus scanner is enough - and creating
/// the same path again then fails with DELETE_PENDING, which std answers by
/// parking on a timed sleep inside its create-file retry. Copying over the
/// tree and removing only what the repository no longer has keeps any path out
/// of delete-pending ahead of the write that needs it, and costs a stat per
/// file rather than a copy once the tree is in place.
fn syncData(io: std.Io, allocator: std.mem.Allocator, repo: std.Io.Dir, destination: std.Io.Dir) !void {
    var data = try repo.openDir(io, "Data", .{ .iterate = true });
    defer data.close(io);
    try removeDataLinkIfPresent(io, destination);
    try destination.createDirPath(io, "Data");
    var destination_data = try destination.openDir(io, "Data", .{ .iterate = true, .access_sub_paths = true });
    defer destination_data.close(io);
    try syncTree(io, allocator, data, destination_data, .size_and_time);
}

/// An earlier --link-data run leaves Data as a link into the repository.
/// Copying through it would write the staged tree back over its own source, so
/// the link has to go before a copy starts.
fn removeDataLinkIfPresent(io: std.Io, destination: std.Io.Dir) !void {
    const staged = destination.statFile(io, "Data", .{ .follow_symlinks = false }) catch |err| switch (err) {
        error.FileNotFound => return,
        else => return err,
    };
    if (staged.kind == .directory) return;
    try removeTreeIfPresent(io, destination, "Data");
}

/// How syncTree decides a staged file is already the copy it would write.
/// Data is compared by size and time: 2.7 GB, and a file only ever moves
/// forward in time. A generated tree is compared by contents: a season file
/// has its summer file's size, and going back to an earlier input brings back
/// an older cached output, which size and time would read as current.
const CurrentCheck = enum { size_and_time, contents };

fn syncTree(io: std.Io, allocator: std.mem.Allocator, source: std.Io.Dir, destination: std.Io.Dir, check: CurrentCheck) !void {
    var staged: std.StringHashMapUnmanaged(void) = .empty;
    defer {
        var keys = staged.keyIterator();
        while (keys.next()) |key| allocator.free(key.*);
        staged.deinit(allocator);
    }

    var walker = try source.walk(allocator);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file or isForbiddenStagedPath(entry.path)) continue;
        const path = try allocator.dupe(u8, entry.path);
        errdefer allocator.free(path);
        try staged.put(allocator, path, {});
        const current = switch (check) {
            .size_and_time => isStagedCopyCurrent(io, entry.dir, entry.basename, destination, entry.path),
            .contents => isStagedCopySame(io, allocator, entry.dir, entry.basename, destination, entry.path),
        };
        if (current) continue;
        copyFile(io, entry.dir, entry.basename, destination, entry.path) catch |err| {
            std.debug.print("stage: data file '{s}' failed: {s}\n", .{ entry.path, @errorName(err) });
            return err;
        };
    }

    try pruneStagedTree(io, allocator, destination, staged);
}

/// A copy carries the time it was made rather than the time of its source, so
/// a staged file that is the same size and no older than the file it came from
/// is already the copy this run would write.
fn isStagedCopyCurrent(io: std.Io, source_dir: std.Io.Dir, source: []const u8, destination_dir: std.Io.Dir, destination: []const u8) bool {
    const source_info = source_dir.statFile(io, source, .{}) catch return false;
    const staged_info = destination_dir.statFile(io, destination, .{}) catch return false;
    if (staged_info.kind != .file) return false;
    if (staged_info.size != source_info.size) return false;
    return staged_info.mtime.nanoseconds >= source_info.mtime.nanoseconds;
}

fn isStagedCopySame(io: std.Io, allocator: std.mem.Allocator, source_dir: std.Io.Dir, source: []const u8, destination_dir: std.Io.Dir, destination: []const u8) bool {
    const limit: std.Io.Limit = .limited(64 << 20);
    const staged = destination_dir.readFileAlloc(io, destination, allocator, limit) catch return false;
    defer allocator.free(staged);
    const wanted = source_dir.readFileAlloc(io, source, allocator, limit) catch return false;
    defer allocator.free(wanted);
    return std.mem.eql(u8, staged, wanted);
}

fn pruneStagedTree(io: std.Io, allocator: std.mem.Allocator, destination: std.Io.Dir, staged: std.StringHashMapUnmanaged(void)) !void {
    var stale = std.ArrayList([]const u8).empty;
    defer {
        for (stale.items) |path| allocator.free(path);
        stale.deinit(allocator);
    }

    {
        // Deleting during the walk would pull entries out from under the
        // iterator, so the tree is read first and pruned afterwards.
        var walker = try destination.walk(allocator);
        defer walker.deinit();
        while (try walker.next(io)) |entry| {
            if (entry.kind != .file) continue;
            // Forbidden paths are never staged, so a staged file matching one
            // was written by the game rather than copied here.
            if (isForbiddenStagedPath(entry.path) or staged.contains(entry.path)) continue;
            try stale.append(allocator, try allocator.dupe(u8, entry.path));
        }
    }

    for (stale.items) |path| destination.deleteFile(io, path) catch |err| {
        std.debug.print("stage: stale data file '{s}' could not be removed: {s}\n", .{ path, @errorName(err) });
        return err;
    };
}

fn copyMetadata(io: std.Io, repo: std.Io.Dir, destination: std.Io.Dir, metadata_files: []const []const u8) !void {
    for (metadata_files) |name| try copyFile(io, repo, name, destination, name);
}

fn copyShaderAssets(io: std.Io, allocator: std.mem.Allocator, repo: std.Io.Dir, destination: std.Io.Dir) !void {
    try removeTreeIfPresent(io, destination, "Shaders/GfxGpu");
    var source = try repo.openDir(io, "zig-out/shaders", .{ .iterate = true });
    defer source.close(io);
    try destination.createDirPath(io, "Shaders/GfxGpu");
    var shader_dir = try destination.openDir(io, "Shaders/GfxGpu", .{ .access_sub_paths = true });
    defer shader_dir.close(io);
    try copyTree(io, allocator, source, shader_dir);
}

fn linkData(io: std.Io, allocator: std.mem.Allocator, repo: std.Io.Dir, destination: std.Io.Dir) !void {
    const data_path = try repo.realPathFileAlloc(io, "Data", allocator);
    defer allocator.free(data_path);
    const install_path = try destination.realPathFileAlloc(io, ".", allocator);
    defer allocator.free(install_path);
    const link_path = try std.fs.path.join(allocator, &.{ install_path, "Data" });
    defer allocator.free(link_path);
    std.Io.Dir.symLinkAbsolute(io, data_path, link_path, .{ .is_directory = true }) catch |err| switch (classifyDataLinkError(err)) {
        error.DataLinkPermissionDenied => {
            std.debug.print("stage: --link-data needs filesystem symlink permission; rerun with --copy-data (the default)\n", .{});
            return error.DataLinkPermissionDenied;
        },
        else => return err,
    };
    try destination.access(io, "Data", .{});
}

fn copyTree(io: std.Io, allocator: std.mem.Allocator, source: std.Io.Dir, destination: std.Io.Dir) !void {
    var walker = try source.walk(allocator);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file or isForbiddenStagedPath(entry.path)) continue;
        copyFile(io, entry.dir, entry.basename, destination, entry.path) catch |err| {
            std.debug.print("stage: data file '{s}' failed: {s}\n", .{ entry.path, @errorName(err) });
            return err;
        };
    }
}

/// `path` is relative to whichever tree the caller is copying: the
/// repository's `Data` directory for game data (syncTree/pruneStagedTree), or
/// the built shader tree for `Shaders/GfxGpu` (copyShaderAssets). A
/// user-write or cache directory (saves, logs, cache, temp, ...) can only
/// ever be a direct child of that tree's root, so the directory-name rules
/// apply to the path's first component only. Anywhere deeper, a directory
/// with the same name is a game asset, not a place the game or a stray tool
/// writes to - e.g. the Logs01-08 log-pile objects live under
/// `Objects/SimpleObjects/common/summer/logs`, four levels under `Data`. The
/// file-suffix rules apply to the last component at any depth, since a stray
/// `*.log`/`*.tmp`/`*.stale` file can appear anywhere in the tree.
fn isForbiddenStagedPath(path: []const u8) bool {
    var parts = std.mem.splitAny(u8, path, "/\\");
    var first: ?[]const u8 = null;
    var last: []const u8 = path;
    while (parts.next()) |part| {
        if (part.len == 0) continue;
        if (first == null) first = part;
        last = part;
    }
    if (first) |name| {
        if (isCacheName(name) or isTempDirName(name) or isUserWriteDirName(name)) return true;
    }
    return isForbiddenSuffix(last);
}

fn isCacheName(name: []const u8) bool {
    return eqlIgnoreCase(name, ".zig-cache") or eqlIgnoreCase(name, "zig-out") or
        eqlIgnoreCase(name, ".cache") or eqlIgnoreCase(name, "cache") or
        startsWithIgnoreCase(name, "cache-") or startsWithIgnoreCase(name, "cache_");
}

fn isTempDirName(name: []const u8) bool {
    return eqlIgnoreCase(name, "temp") or eqlIgnoreCase(name, "tmp") or
        startsWithIgnoreCase(name, "temp-") or startsWithIgnoreCase(name, "temp_") or
        startsWithIgnoreCase(name, "temp.") or startsWithIgnoreCase(name, "tmp-") or
        startsWithIgnoreCase(name, "tmp_") or startsWithIgnoreCase(name, "tmp.");
}

fn isUserWriteDirName(name: []const u8) bool {
    return eqlIgnoreCase(name, "saves") or eqlIgnoreCase(name, "save") or
        eqlIgnoreCase(name, "userdata") or eqlIgnoreCase(name, "user-data") or
        eqlIgnoreCase(name, "logs") or eqlIgnoreCase(name, "crashdumps") or
        eqlIgnoreCase(name, "crash-dumps");
}

fn isForbiddenSuffix(name: []const u8) bool {
    return endsWithIgnoreCase(name, ".log") or endsWithIgnoreCase(name, ".lock") or
        endsWithIgnoreCase(name, ".tmp") or endsWithIgnoreCase(name, ".temp") or
        endsWithIgnoreCase(name, ".stale");
}

fn eqlIgnoreCase(left: []const u8, right: []const u8) bool {
    if (left.len != right.len) return false;
    for (left, right) |left_byte, right_byte| if (asciiLower(left_byte) != asciiLower(right_byte)) return false;
    return true;
}

fn startsWithIgnoreCase(value: []const u8, prefix: []const u8) bool {
    return value.len >= prefix.len and eqlIgnoreCase(value[0..prefix.len], prefix);
}

fn endsWithIgnoreCase(value: []const u8, suffix: []const u8) bool {
    return value.len >= suffix.len and eqlIgnoreCase(value[value.len - suffix.len ..], suffix);
}

fn asciiLower(byte: u8) u8 {
    return if (byte >= 'A' and byte <= 'Z') byte + ('a' - 'A') else byte;
}

fn copyFile(io: std.Io, source_dir: std.Io.Dir, source: []const u8, destination_dir: std.Io.Dir, destination: []const u8) !void {
    try source_dir.copyFile(source, destination_dir, destination, io, .{ .make_path = true, .replace = true });
}

fn seedConfigIfMissing(io: std.Io, repo: std.Io.Dir, destination: std.Io.Dir) !void {
    destination.access(io, "config.cfg", .{}) catch |err| switch (err) {
        error.FileNotFound => return copyFile(io, repo, "Data/Configs/config.cfg", destination, "config.cfg") catch |copy_err| switch (copy_err) {
            error.FileNotFound => copyFile(io, repo, "Data/Configs/defconf.cfg", destination, "config.cfg"),
            else => return copy_err,
        },
        else => return err,
    };
}

fn removeTreeIfPresent(io: std.Io, dir: std.Io.Dir, path: []const u8) !void {
    dir.access(io, path, .{}) catch |err| switch (err) {
        error.FileNotFound => return,
        else => return err,
    };
    try dir.deleteTree(io, path);
}

fn copyEditors(io: std.Io, repo: std.Io.Dir, destination: std.Io.Dir) !void {
    const editors = [_]struct { source: []const u8, destination: []const u8 }{
        .{ .source = "Sources/src/bin/editor.exe", .destination = "Editors/editor.exe" },
        .{ .source = "Sources/src/bin/MapEditor.exe", .destination = "Editors/MapEditor.exe" },
        .{ .source = "Sources/src/bin/ExcelExporter.exe", .destination = "Editors/ExcelExporter.exe" },
        .{ .source = "Sources/elk/ELK.exe", .destination = "Editors/ELK.exe" },
    };
    var copied: usize = 0;
    for (editors) |editor| {
        copyFile(io, repo, editor.source, destination, editor.destination) catch |err| switch (err) {
            error.FileNotFound => continue,
            else => return err,
        };
        copied += 1;
    }
    if (copied == 0) return error.NoEditorsFound;
}

test "copy is the default data mode" {
    try std.testing.expectEqual(DataMode.copy, (Options{ .repo_root = ".", .install_dir = "out" }).data_mode);
}

test "runtime replacement rejects stale image names" {
    try std.testing.expect(!shouldReplaceRuntime("Game.exe.stale"));
    try std.testing.expect(shouldReplaceRuntime("Game.exe"));
}
