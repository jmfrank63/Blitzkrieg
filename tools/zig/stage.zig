const std = @import("std");
const builtin = @import("builtin");

const DataMode = enum { link, copy };

const Options = struct {
    repo_root: []const u8,
    install_dir: []const u8,
    data_mode: DataMode = .link,
    include_editors: bool = false,
    editors_only: bool = false,
};

pub fn main(init: std.process.Init) !void {
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.skip();

    const options = try parseArgs(&args);
    try stage(init.io, init.gpa, options);
}

fn parseArgs(args: *std.process.Args.Iterator) !Options {
    var options = Options{
        .repo_root = args.next() orelse return error.InvalidArguments,
        .install_dir = args.next() orelse return error.InvalidArguments,
    };
    while (args.next()) |arg| {
        if (std.mem.eql(u8, arg, "--copy-data")) {
            options.data_mode = .copy;
        } else if (std.mem.eql(u8, arg, "--include-editors")) {
            options.include_editors = true;
        } else if (std.mem.eql(u8, arg, "--editors-only")) {
            options.editors_only = true;
        } else {
            return error.InvalidArguments;
        }
    }
    return options;
}

fn stage(io: std.Io, allocator: std.mem.Allocator, options: Options) !void {
    const cwd = std.Io.Dir.cwd();
    var repo = try cwd.openDir(io, options.repo_root, .{ .access_sub_paths = true });
    defer repo.close(io);
    try cwd.createDirPath(io, options.install_dir);
    var destination = try cwd.openDir(io, options.install_dir, .{ .access_sub_paths = true });
    defer destination.close(io);

    if (!options.editors_only) {
        var binaries = try repo.openDir(io, "zig-out/bin", .{ .iterate = true });
        defer binaries.close(io);
        copyGameRuntime(io, allocator, binaries, destination, options.install_dir) catch |err| return failStep("copyGameRuntime", err);
        copyFile(io, repo, "Data/Configs/config.cfg", destination, "config.cfg") catch |err| return failStep("copy config.cfg", err);
        copyFile(io, repo, "Data/Configs/defconf.cfg", destination, "defconf.cfg") catch |err| return failStep("copy defconf.cfg", err);
        // The game silently fails to write saves when this is missing.
        destination.createDirPath(io, "saves") catch |err| return failStep("create saves dir", err);
        removeTreeIfPresent(io, destination, "Data") catch |err| return failStep("remove staged Data", err);
        if (options.data_mode == .copy) {
            copyData(io, allocator, repo, destination) catch |err| return failStep("copyData", err);
        } else {
            linkData(io, allocator, repo, cwd, destination, options.install_dir) catch |err| return failStep("linkData", err);
        }
    } else {
        try destination.access(io, "Data", .{});
    }

    try removeTreeIfPresent(io, destination, "Editors");
    if (options.include_editors) try copyEditors(io, repo, destination);
}

fn failStep(step: []const u8, err: anyerror) anyerror {
    std.debug.print("stage: step '{s}' failed: {s}\n", .{ step, @errorName(err) });
    return err;
}

// Zig's Dir.rename opens the file with sharing flags the image loader refuses
// (FileBusy on a running exe/dll), while a plain Win32 rename succeeds — fall
// back to PowerShell for locked images, mirroring createDirectoryJunction.
fn moveAside(io: std.Io, allocator: std.mem.Allocator, destination: std.Io.Dir, install_dir: []const u8, name: []const u8, aside: []const u8) !void {
    if (destination.rename(name, destination, aside, io)) |_| {
        return;
    } else |err| switch (err) {
        error.FileBusy, error.AccessDenied, error.PermissionDenied => {},
        else => return err,
    }
    if (builtin.os.tag != .windows) return error.FileBusy;
    const cmd = try std.fmt.allocPrint(allocator, "Rename-Item -LiteralPath '{s}/{s}' -NewName '{s}'", .{ install_dir, name, aside });
    defer allocator.free(cmd);
    const result = try std.process.run(allocator, io, .{
        .argv = &.{ "powershell.exe", "-NoProfile", "-NonInteractive", "-Command", cmd },
    });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);
    switch (result.term) {
        .exited => |code| if (code != 0) return error.FileBusy,
        else => return error.FileBusy,
    }
}

fn copyGameRuntime(io: std.Io, allocator: std.mem.Allocator, binaries: std.Io.Dir, destination: std.Io.Dir, install_dir: []const u8) !void {
    const stale_root_files = [_][]const u8{
        "BetaKeyGen.exe",    "BuildVersion.exe",       "FontGen.exe",
        // Legacy x86-only payloads must never survive a target switch into an
        // x64 staged runtime.
        "A7ExportModel.dll", "fmod.dll",               "mfc42.dll",
        "msvcp60.dll",       "msvcrt.dll",
        // Clear prior runtime outputs so staging is deterministic.
        "Game.exe",        "Game.pdb",
        "StreamIO.dll",      "StreamIOOptionsAbi.dll", "Anim.dll",
        "GFX.dll",           "Image.dll",              "Input.dll",
        "Net.dll",           "SFX.dll",                "UI.dll",
        "Scene.dll",         "AILogic.dll",            "GameTT.dll",
        "StreamIO.pdb",      "StreamIOOptionsAbi.pdb", "Anim.pdb",
        "GFX.pdb",           "Image.pdb",              "Input.pdb",
        "Net.pdb",           "SFX.pdb",                "UI.pdb",
        "Scene.pdb",         "AILogic.pdb",            "GameTT.pdb",
    };
    for (stale_root_files) |name| destination.deleteFile(io, name) catch {};
    const runtime_files = [_][]const u8{
        "Game.exe",
        "Game.pdb",
        "StreamIO.dll",
        "StreamIOOptionsAbi.dll",
        "Anim.dll",
        "GFX.dll",
        "Image.dll",
        "Input.dll",
        "Net.dll",
        "SFX.dll",
        "UI.dll",
        "Scene.dll",
        "AILogic.dll",
        "GameTT.dll",
        "StreamIO.pdb",
        "StreamIOOptionsAbi.pdb",
        "Anim.pdb",
        "GFX.pdb",
        "Image.pdb",
        "Input.pdb",
        "Net.pdb",
        "SFX.pdb",
        "UI.pdb",
        "Scene.pdb",
        "AILogic.pdb",
        "GameTT.pdb",
    };
    for (runtime_files) |name| {
        var stale_buf: [64]u8 = undefined;
        if (std.fmt.bufPrint(&stale_buf, "{s}.stale", .{name})) |aside|
            destination.deleteFile(io, aside) catch {}
        else |_| {}
    }
    for (runtime_files) |name| {
        copyFile(io, binaries, name, destination, name) catch |err| switch (err) {
            error.AccessDenied, error.PermissionDenied => {
                // A running Game.exe keeps runtime DLLs mapped; Windows blocks
                // overwriting a mapped image but allows renaming it. Move the
                // locked file aside and copy fresh so the staged runtime can
                // never silently remain stale. Failing here is better than
                // keeping an old binary that no longer matches the build.
                var aside_buf: [64]u8 = undefined;
                const aside = std.fmt.bufPrint(&aside_buf, "{s}.stale", .{name}) catch return err;
                destination.deleteFile(io, aside) catch {};
                moveAside(io, allocator, destination, install_dir, name, aside) catch |rename_err| {
                    std.debug.print("stage: could not move locked '{s}' aside: {s} — close the running game and rebuild\n", .{ name, @errorName(rename_err) });
                    return err;
                };
                copyFile(io, binaries, name, destination, name) catch |copy_err| {
                    std.debug.print("stage: fresh copy of '{s}' failed after move-aside: {s}\n", .{ name, @errorName(copy_err) });
                    return copy_err;
                };
            },
            else => return err,
        };
    }
}

fn copyData(io: std.Io, allocator: std.mem.Allocator, repo: std.Io.Dir, destination: std.Io.Dir) !void {
    var data = try repo.openDir(io, "Data", .{ .iterate = true });
    defer data.close(io);
    try destination.createDirPath(io, "Data");
    var destination_data = try destination.openDir(io, "Data", .{ .access_sub_paths = true });
    defer destination_data.close(io);
    try copyTree(io, allocator, data, destination_data);
}

fn linkData(io: std.Io, allocator: std.mem.Allocator, repo: std.Io.Dir, cwd: std.Io.Dir, destination: std.Io.Dir, install_dir: []const u8) !void {
    const data_path = try repo.realPathFileAlloc(io, "Data", allocator);
    defer allocator.free(data_path);
    const install_path = try cwd.realPathFileAlloc(io, install_dir, allocator);
    defer allocator.free(install_path);
    const link_path = try std.fs.path.join(allocator, &.{ install_path, "Data" });
    defer allocator.free(link_path);
    try createDirectoryJunction(io, allocator, data_path, link_path);
    try destination.access(io, "Data", .{});
}

fn createDirectoryJunction(io: std.Io, allocator: std.mem.Allocator, target_path: []const u8, link_path: []const u8) !void {
    if (builtin.os.tag == .windows) {
        const cmd = try std.fmt.allocPrint(allocator, "New-Item -ItemType Junction -Path '{s}' -Target '{s}'", .{ link_path, target_path });
        defer allocator.free(cmd);
        const result = try std.process.run(allocator, io, .{
            .argv = &.{ "powershell.exe", "-NoProfile", "-NonInteractive", "-Command", cmd },
        });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);
        switch (result.term) {
            .exited => |code| if (code != 0) return error.CreateJunctionFailed,
            else => return error.CreateJunctionFailed,
        }
    } else {
        const result = try std.process.run(allocator, io, .{
            .argv = &.{ "ln", "-s", target_path, link_path },
        });
        defer allocator.free(result.stdout);
        defer allocator.free(result.stderr);
        switch (result.term) {
            .exited => |code| if (code != 0) return error.CreateJunctionFailed,
            else => return error.CreateJunctionFailed,
        }
    }
}

test "link mode creates a directory reparse point without symbolic-link privileges" {
    var tmp = std.testing.tmpDir(.{ .access_sub_paths = true });
    defer tmp.cleanup();

    try tmp.dir.createDirPath(std.testing.io, "target");
    const target_path = try tmp.dir.realPathFileAlloc(std.testing.io, "target", std.testing.allocator);
    defer std.testing.allocator.free(target_path);
    const root_path = try tmp.dir.realPathFileAlloc(std.testing.io, ".", std.testing.allocator);
    defer std.testing.allocator.free(root_path);
    const link_path = try std.fs.path.join(std.testing.allocator, &.{ root_path, "link" });
    defer std.testing.allocator.free(link_path);

    try createDirectoryJunction(std.testing.io, std.testing.allocator, target_path, link_path);
    try tmp.dir.access(std.testing.io, "link", .{});
}

fn copyTree(io: std.Io, allocator: std.mem.Allocator, source: std.Io.Dir, destination: std.Io.Dir) !void {
    var walker = try source.walk(allocator);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (entry.kind != .file) continue;
        try copyFile(io, entry.dir, entry.basename, destination, entry.path);
    }
}

fn copyFile(io: std.Io, source_dir: std.Io.Dir, source: []const u8, destination_dir: std.Io.Dir, destination: []const u8) !void {
    try source_dir.copyFile(source, destination_dir, destination, io, .{ .make_path = true, .replace = true });
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
