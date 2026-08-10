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
        copyShaderAssets(io, allocator, repo, destination) catch |err| return failStep("copyShaderAssets", err);
        seedConfigIfMissing(io, repo, destination) catch |err| return failStep("seed config.cfg", err);
        copyFile(io, repo, "Data/Configs/defconf.cfg", destination, "defconf.cfg") catch |err| return failStep("copy defconf.cfg", err);
        copyMetadata(io, repo, destination, options.layout.metadata_files) catch |err| return failStep("copy package metadata", err);
        destination.createDirPath(io, "saves") catch |err| return failStep("create saves dir", err);
        switch (options.data_mode) {
            .copy => syncData(io, allocator, repo, destination) catch |err| return failStep("syncData", err),
            .link => {
                removeTreeIfPresent(io, destination, "Data") catch |err| return failStep("remove staged Data", err);
                linkData(io, allocator, repo, destination) catch |err| return failStep("linkData", err);
            },
        }
    } else if (!options.layout.editors_supported) {
        return error.EditorsUnsupported;
    }

    try removeTreeIfPresent(io, destination, "Editors");
    if (options.include_editors) {
        if (!options.layout.editors_supported) return error.EditorsUnsupported;
        try copyEditors(io, repo, destination);
    }
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

fn copyRuntimeFile(io: std.Io, binaries: std.Io.Dir, libraries: ?std.Io.Dir, name: []const u8, destination: std.Io.Dir) !void {
    const source = runtimeSourceName(name);
    copyFile(io, binaries, source, destination, name) catch |err| {
        if (err != error.FileNotFound) return err;
        const lib_dir = libraries orelse return err;
        try copyFile(io, lib_dir, source, destination, name);
        return;
    };
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
    try syncTree(io, allocator, data, destination_data);
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

fn syncTree(io: std.Io, allocator: std.mem.Allocator, source: std.Io.Dir, destination: std.Io.Dir) !void {
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
        if (isStagedCopyCurrent(io, entry.dir, entry.basename, destination, entry.path)) continue;
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

fn isForbiddenStagedPath(path: []const u8) bool {
    var parts = std.mem.splitAny(u8, path, "/\\");
    while (parts.next()) |part| {
        if (part.len == 0) continue;
        if (isCacheName(part) or isTempName(part) or isUserWriteName(part)) return true;
    }
    return false;
}

fn isCacheName(name: []const u8) bool {
    return eqlIgnoreCase(name, ".zig-cache") or eqlIgnoreCase(name, "zig-out") or
        eqlIgnoreCase(name, ".cache") or eqlIgnoreCase(name, "cache") or
        startsWithIgnoreCase(name, "cache-") or startsWithIgnoreCase(name, "cache_");
}

fn isTempName(name: []const u8) bool {
    return eqlIgnoreCase(name, "temp") or eqlIgnoreCase(name, "tmp") or
        startsWithIgnoreCase(name, "temp-") or startsWithIgnoreCase(name, "temp_") or
        startsWithIgnoreCase(name, "temp.") or startsWithIgnoreCase(name, "tmp-") or
        startsWithIgnoreCase(name, "tmp_") or startsWithIgnoreCase(name, "tmp.") or
        endsWithIgnoreCase(name, ".tmp") or endsWithIgnoreCase(name, ".temp");
}

fn isUserWriteName(name: []const u8) bool {
    return eqlIgnoreCase(name, "saves") or eqlIgnoreCase(name, "save") or
        eqlIgnoreCase(name, "userdata") or eqlIgnoreCase(name, "user-data") or
        eqlIgnoreCase(name, "logs") or eqlIgnoreCase(name, "crashdumps") or
        eqlIgnoreCase(name, "crash-dumps") or endsWithIgnoreCase(name, ".log") or
        endsWithIgnoreCase(name, ".lock");
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
