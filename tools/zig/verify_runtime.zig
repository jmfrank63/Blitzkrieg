const std = @import("std");

pub const RuntimeTarget = enum { windows, linux, macos };

const windows_runtime_names = [_][]const u8{
    "Game.exe",  "PlatformRuntime.dll", "StreamIO.dll", "StreamIOOptionsAbi.dll",
    "Anim.dll",  "GFXGPU.dll",          "SDL3.dll",     "Image.dll",
    "Input.dll", "Net.dll",             "SFX.dll",      "UI.dll",
    "Scene.dll", "AILogic.dll",         "GameTT.dll",
};
const linux_runtime_names = [_][]const u8{
    "Game",        "libPlatformRuntime.so", "libStreamIO.so", "libStreamIOOptionsAbi.so",
    "libAnim.so",  "libGFXGPU.so",          "libSDL3.so.0",   "libImage.so",
    "libInput.so", "libNet.so",             "libSFX.so",      "libUI.so",
    "libScene.so", "libAILogic.so",         "libGameTT.so",
};
const macos_runtime_names = [_][]const u8{
    "Game",           "libPlatformRuntime.dylib", "libStreamIO.dylib", "libStreamIOOptionsAbi.dylib",
    "libAnim.dylib",  "libGFXGPU.dylib",          "libSDL3.dylib",     "libImage.dylib",
    "libInput.dylib", "libNet.dylib",             "libSFX.dylib",      "libUI.dylib",
    "libScene.dylib", "libAILogic.dylib",         "libGameTT.dylib",
};

pub const shader_manifest_name = "Shaders/GfxGpu/gfxgpu-shaders.manifest";
/// The nine README translations stage at the root beside README.md, so the
/// language bar every README carries links to files the install holds.
pub const translated_readme_files = [_][]const u8{
    "README_Chinese.md",    "README_French.md",   "README_German.md",
    "README_Hindi.md",      "README_Indonesian.md", "README_Japanese.md",
    "README_Portuguese.md", "README_Russian.md",  "README_Spanish.md",
};
pub const required_metadata_files = [_][]const u8{ "LICENSE.md", "README.md" } ++ translated_readme_files;

/// The tools guide in ten languages, staged under Tools/ beside the bk-* tools
/// it describes (tools/guide in the repository).
pub const required_tool_docs = [_][]const u8{
    "Tools/README.md",         "Tools/README_Chinese.md",    "Tools/README_French.md",
    "Tools/README_German.md",  "Tools/README_Hindi.md",      "Tools/README_Indonesian.md",
    "Tools/README_Japanese.md", "Tools/README_Portuguese.md", "Tools/README_Russian.md",
    "Tools/README_Spanish.md",
};
pub const required_data_files = [_][]const u8{"Data/Configs/defconf.cfg"};

/// rclone ships beside the game executable so cloud sync works on a machine
/// with nothing on PATH; discovery searches that directory before PATH. A
/// package without it is a package whose cloud sync only works for players who
/// already installed rclone themselves, which is the whole point of bundling.
pub fn bundledToolName(target: RuntimeTarget) []const u8 {
    return if (target == .windows) "rclone.exe" else "rclone";
}

/// MIT obliges us to carry rclone's copyright line and permission notice with
/// every copy of its binary, and the official archive has no COPYING file to
/// stage. The notice is therefore a file we own and review; staging pairs it
/// with the binary so neither can ship without the other.
pub const third_party_notices_source = "Data/THIRD-PARTY-NOTICES.txt";
pub const third_party_notices_name = "THIRD-PARTY-NOTICES.txt";

pub const VerifyError = error{
    DuplicatePlatformRuntime,
    MissingGame,
    MissingPlatformRuntime,
    MissingRuntimeEntry,
    UnexpectedPlatformRuntime,
    MissingShaderManifest,
    MissingShader,
    MissingConfig,
    MissingDefaultConfig,
    MissingMetadata,
    MissingToolDoc,
    MissingBundledTool,
    MissingThirdPartyNotices,
    MissingDataRoot,
    MissingConfigRoot,
    MissingDataFile,
    UnsafeManifestPath,
    ForbiddenArtifact,
    DuplicateManifestEntry,
    UnexpectedRuntimeEntry,
    UnexpectedShader,
    UnexpectedManifestEntry,
    MissingL10nInstall,
};

pub fn runtimeName(target: RuntimeTarget) []const u8 {
    return switch (target) {
        .windows => "PlatformRuntime.dll",
        .linux => "libPlatformRuntime.so",
        .macos => "libPlatformRuntime.dylib",
    };
}

pub fn gameName(target: RuntimeTarget) []const u8 {
    return if (target == .windows) "Game.exe" else "Game";
}

/// The complete top-level runtime file set emitted by the staged package.
/// The first entry is the target executable and the second is PlatformRuntime.
pub fn requiredRuntimeNames(target: RuntimeTarget) []const []const u8 {
    return switch (target) {
        .windows => &windows_runtime_names,
        .linux => &linux_runtime_names,
        .macos => &macos_runtime_names,
    };
}

pub fn shaderExtension(target: RuntimeTarget) []const u8 {
    return switch (target) {
        .windows => ".dxil",
        .linux => ".spirv",
        .macos => ".msl",
    };
}

pub fn hasExactlyOneRuntime(names: []const []const u8, target: RuntimeTarget) bool {
    var count: usize = 0;
    for (names) |name| {
        if (pathEqual(name, runtimeName(target))) count += 1;
    }
    return count == 1;
}

/// Validate a staged manifest without touching the filesystem.
/// Entries may use either slash convention, but must remain relative to the
/// stage root and may not contain build caches or user-write artifacts.
pub fn verifyStagedLayout(names: []const []const u8, target: RuntimeTarget) VerifyError!void {
    for (names) |name| {
        if (!isSafeManifestPath(name)) return error.UnsafeManifestPath;
        if (isForbiddenArtifactPath(name)) return error.ForbiddenArtifact;
    }

    var runtime_count: usize = 0;
    for (names) |name| {
        if (pathEqual(name, runtimeName(target))) runtime_count += 1;
        if (isPlatformRuntimeName(name) and !pathEqual(name, runtimeName(target)))
            return error.UnexpectedPlatformRuntime;
    }
    if (runtime_count > 1) return error.DuplicatePlatformRuntime;
    if (runtime_count == 0) return error.MissingPlatformRuntime;
    if (!containsPath(names, gameName(target))) return error.MissingGame;

    for (requiredRuntimeNames(target)[2..]) |required| {
        if (!containsPath(names, required)) return error.MissingRuntimeEntry;
    }

    if (!containsPath(names, shader_manifest_name)) return error.MissingShaderManifest;
    for (names) |name| {
        if (shaderNameAtPath(name)) |shader_name| {
            if (hasKnownShaderExtension(shader_name) and !std.mem.endsWith(u8, shader_name, shaderExtension(target)))
                return error.UnexpectedShader;
        }
    }
    if (!containsShader(names, target)) return error.MissingShader;
    if (!containsPath(names, "config.cfg")) return error.MissingConfig;
    if (!containsPath(names, "defconf.cfg")) return error.MissingDefaultConfig;
    for (required_metadata_files) |required| {
        if (!containsPath(names, required)) return error.MissingMetadata;
    }
    for (required_tool_docs) |required| {
        if (!containsPath(names, required)) {
            std.debug.print("verify-runtime: missing tools guide file: {s}\n", .{required});
            return error.MissingToolDoc;
        }
    }
    if (!containsPath(names, bundledToolName(target))) return error.MissingBundledTool;
    if (!containsPath(names, third_party_notices_name)) return error.MissingThirdPartyNotices;
    if (!containsPathUnder(names, "Data")) return error.MissingDataRoot;
    if (!containsPathUnder(names, "Data/Configs")) return error.MissingConfigRoot;
    for (required_data_files) |required| {
        if (!containsPath(names, required)) return error.MissingDataFile;
    }

    for (names, 0..) |left, index| {
        for (names[index + 1 ..]) |right| {
            if (pathEqual(left, right)) return error.DuplicateManifestEntry;
        }
    }

    for (names) |name| {
        if (isKnownRuntimeName(name) and !containsPath(requiredRuntimeNames(target), name))
            return error.UnexpectedRuntimeEntry;
    }
}

pub fn isValidStagedLayout(names: []const []const u8, target: RuntimeTarget) bool {
    verifyStagedLayout(names, target) catch return false;
    return true;
}

/// Walk a staged package with Zig I/O, collect its relative file manifest,
/// and apply the same exact-layout rules as verifyStagedLayout.
pub fn verifyStagedRoot(io: std.Io, allocator: std.mem.Allocator, root_path: []const u8, target: RuntimeTarget) !void {
    var root = try std.Io.Dir.cwd().openDir(io, root_path, .{ .iterate = true, .access_sub_paths = true });
    defer root.close(io);
    try verifyStagedDirectory(io, allocator, root, target);
}

/// Validate an already-open staged directory. The directory remains owned by
/// the caller and is not closed here.
pub fn verifyStagedDirectory(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, target: RuntimeTarget) !void {
    var names: std.ArrayListUnmanaged([]const u8) = .empty;
    defer {
        for (names.items) |name| allocator.free(name);
        names.deinit(allocator);
    }

    var walker = try root.walk(allocator);
    defer walker.deinit();
    while (try walker.next(io)) |entry| {
        if (!isSafeManifestPath(entry.path)) return error.UnsafeManifestPath;
        if (isForbiddenArtifactPath(entry.path)) return error.ForbiddenArtifact;
        switch (entry.kind) {
            .file => try names.append(allocator, try allocator.dupe(u8, entry.path)),
            .directory => {},
            else => return error.UnexpectedManifestEntry,
        }
    }

    try verifyStagedLayout(names.items, target);
}

pub fn isSafeManifestPath(path: []const u8) bool {
    if (path.len == 0 or path[0] == '/' or path[0] == '\\') return false;
    if (path.len >= 2 and path[1] == ':') return false;

    var parts = std.mem.splitAny(u8, path, "/\\");
    while (parts.next()) |part| {
        if (part.len == 0 or std.mem.eql(u8, part, ".") or std.mem.eql(u8, part, "..")) return false;
        if (std.mem.indexOfScalar(u8, part, ':') != null) return false;
        if (std.mem.indexOfScalar(u8, part, 0) != null) return false;
    }
    return true;
}

fn containsPath(names: []const []const u8, required: []const u8) bool {
    for (names) |name| if (pathEqual(name, required)) return true;
    return false;
}

fn containsPathUnder(names: []const []const u8, root: []const u8) bool {
    for (names) |name| {
        var name_parts = std.mem.splitAny(u8, name, "/\\");
        var root_parts = std.mem.splitAny(u8, root, "/\\");
        var matched = true;
        while (root_parts.next()) |root_part| {
            const name_part = name_parts.next() orelse {
                matched = false;
                break;
            };
            if (!std.mem.eql(u8, name_part, root_part)) {
                matched = false;
                break;
            }
        }
        if (matched) return true;
    }
    return false;
}

fn pathEqual(left: []const u8, right: []const u8) bool {
    var left_parts = std.mem.splitAny(u8, left, "/\\");
    var right_parts = std.mem.splitAny(u8, right, "/\\");
    while (true) {
        const left_part = left_parts.next();
        const right_part = right_parts.next();
        if (left_part == null or right_part == null) return left_part == null and right_part == null;
        if (!std.mem.eql(u8, left_part.?, right_part.?)) return false;
    }
}

fn containsShader(names: []const []const u8, target: RuntimeTarget) bool {
    for (names) |name| {
        var parts = std.mem.splitAny(u8, name, "/\\");
        if (!std.mem.eql(u8, parts.next() orelse continue, "Shaders")) continue;
        if (!std.mem.eql(u8, parts.next() orelse continue, "GfxGpu")) continue;
        const shader_name = parts.next() orelse continue;
        if (parts.next() != null) continue;
        if (shader_name.len > shaderExtension(target).len and std.mem.endsWith(u8, shader_name, shaderExtension(target))) return true;
    }
    return false;
}

fn isPlatformRuntimeName(name: []const u8) bool {
    return pathEqual(name, "PlatformRuntime.dll") or
        pathEqual(name, "libPlatformRuntime.so") or
        pathEqual(name, "libPlatformRuntime.dylib");
}

fn isKnownRuntimeName(name: []const u8) bool {
    return containsPath(&windows_runtime_names, name) or
        containsPath(&linux_runtime_names, name) or
        containsPath(&macos_runtime_names, name);
}

fn shaderNameAtPath(name: []const u8) ?[]const u8 {
    var parts = std.mem.splitAny(u8, name, "/\\");
    if (!std.mem.eql(u8, parts.next() orelse return null, "Shaders")) return null;
    if (!std.mem.eql(u8, parts.next() orelse return null, "GfxGpu")) return null;
    const shader_name = parts.next() orelse return null;
    if (parts.next() != null) return null;
    return shader_name;
}

fn hasKnownShaderExtension(name: []const u8) bool {
    return (name.len > ".dxil".len and std.mem.endsWith(u8, name, ".dxil")) or
        (name.len > ".spirv".len and std.mem.endsWith(u8, name, ".spirv")) or
        (name.len > ".msl".len and std.mem.endsWith(u8, name, ".msl"));
}

/// `path` is relative to the stage root (e.g. `Data/Objects/.../logs/01/1.xml`
/// or `saves/profile.sav`). A user-write or cache directory can only be the
/// root's own top level, or the top level of `Data` within it, so the
/// directory-name rules check the first component, and the second when the
/// first is `Data`. Anywhere deeper, a same-named directory is a game asset,
/// not a place the game or a stray tool writes to - e.g. the Logs01-08
/// log-pile objects live under `Data/Objects/SimpleObjects/common/summer/logs`,
/// five components deep. The file-suffix rules apply to the last component at
/// any depth, since a stray `*.log`/`*.tmp`/`*.stale` file can appear
/// anywhere in the tree.
fn isForbiddenArtifactPath(path: []const u8) bool {
    var parts = std.mem.splitAny(u8, path, "/\\");
    var last: []const u8 = path;
    if (parts.next()) |first| {
        if (first.len != 0) {
            last = first;
            if (isCacheName(first) or isTempDirName(first) or isUserWriteDirName(first)) return true;
            if (eqlIgnoreCase(first, "Data")) {
                if (parts.next()) |second| {
                    if (second.len != 0) {
                        last = second;
                        if (isCacheName(second) or isTempDirName(second) or isUserWriteDirName(second)) return true;
                    }
                }
            }
        }
    }
    while (parts.next()) |part| if (part.len != 0) {
        last = part;
    };
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

test "runtime verifier uses target-correct names" {
    try std.testing.expectEqualStrings("PlatformRuntime.dll", runtimeName(.windows));
    try std.testing.expectEqualStrings("libPlatformRuntime.so", runtimeName(.linux));
    try std.testing.expectEqualStrings("libPlatformRuntime.dylib", runtimeName(.macos));
    try std.testing.expectEqualStrings("Game.exe", gameName(.windows));
    try std.testing.expectEqualStrings("Game", gameName(.linux));
}

test "runtime verifier rejects duplicate runtime copies" {
    try std.testing.expect(hasExactlyOneRuntime(&.{ "Game", "libPlatformRuntime.so", "libSDL3.so.0" }, .linux));
    try std.testing.expect(!hasExactlyOneRuntime(&.{ "Game", "libPlatformRuntime.so", "libPlatformRuntime.so" }, .linux));
}

const staged_docs = translated_readme_files ++ required_tool_docs;

test "staged layout verifier accepts every target matrix" {
    try verifyStagedLayout(&([_][]const u8{
        "Game.exe",           "PlatformRuntime.dll",                 "StreamIO.dll",          "StreamIOOptionsAbi.dll", "Anim.dll",
        "GFXGPU.dll",         "SDL3.dll",                            "Image.dll",             "Input.dll",              "Net.dll",
        "SFX.dll",            "UI.dll",                              "Scene.dll",             "AILogic.dll",            "GameTT.dll",
        shader_manifest_name, "Shaders/GfxGpu/textured.vertex.dxil", "config.cfg",            "defconf.cfg",            "LICENSE.md",
        "README.md",          "Data/Configs/defconf.cfg",            "Data/Maps/fixture.map", "rclone.exe",             third_party_notices_name,
    } ++ staged_docs), .windows);
    try verifyStagedLayout(&([_][]const u8{
        "Game",               "libPlatformRuntime.so",                "libStreamIO.so",        "libStreamIOOptionsAbi.so", "libAnim.so",
        "libGFXGPU.so",       "libSDL3.so.0",                         "libImage.so",           "libInput.so",              "libNet.so",
        "libSFX.so",          "libUI.so",                             "libScene.so",           "libAILogic.so",            "libGameTT.so",
        shader_manifest_name, "Shaders/GfxGpu/textured.vertex.spirv", "config.cfg",            "defconf.cfg",              "LICENSE.md",
        "README.md",          "Data/Configs/defconf.cfg",             "Data/Maps/fixture.map", "rclone",                   third_party_notices_name,
    } ++ staged_docs), .linux);
    try verifyStagedLayout(&([_][]const u8{
        "Game",               "libPlatformRuntime.dylib",           "libStreamIO.dylib",     "libStreamIOOptionsAbi.dylib", "libAnim.dylib",
        "libGFXGPU.dylib",    "libSDL3.dylib",                      "libImage.dylib",        "libInput.dylib",              "libNet.dylib",
        "libSFX.dylib",       "libUI.dylib",                        "libScene.dylib",        "libAILogic.dylib",            "libGameTT.dylib",
        shader_manifest_name, "Shaders/GfxGpu/textured.vertex.msl", "config.cfg",            "defconf.cfg",                 "LICENSE.md",
        "README.md",          "Data/Configs/defconf.cfg",           "Data/Maps/fixture.map", "rclone",                      third_party_notices_name,
    } ++ staged_docs), .macos);
}

test "staged layout ships the bundled rclone with the notice its licence requires" {
    // The binary and the notice are one shipment: rclone is MIT, so the copy we
    // distribute has to travel with its copyright line and permission notice,
    // and a layout carrying one without the other is rejected either way round.
    const engine = [_][]const u8{
        "Game",               "libPlatformRuntime.so",             "libStreamIO.so",        "libStreamIOOptionsAbi.so", "libAnim.so",
        "libGFXGPU.so",       "libSDL3.so.0",                      "libImage.so",           "libInput.so",              "libNet.so",
        "libSFX.so",          "libUI.so",                          "libScene.so",           "libAILogic.so",            "libGameTT.so",
        shader_manifest_name, "Shaders/GfxGpu/probe.vertex.spirv", "config.cfg",            "defconf.cfg",              "LICENSE.md",
        "README.md",          "Data/Configs/defconf.cfg",          "Data/Maps/fixture.map",
    } ++ staged_docs;
    try verifyStagedLayout(&(engine ++ [_][]const u8{ "rclone", third_party_notices_name }), .linux);
    try std.testing.expectError(error.MissingBundledTool, verifyStagedLayout(&(engine ++ [_][]const u8{third_party_notices_name}), .linux));
    try std.testing.expectError(error.MissingThirdPartyNotices, verifyStagedLayout(&(engine ++ [_][]const u8{"rclone"}), .linux));

    // Windows ships the same tool under the name Win32 will execute, so a
    // package built for it is not satisfied by the POSIX name.
    try std.testing.expectEqualStrings("rclone.exe", bundledToolName(.windows));
    try std.testing.expectEqualStrings("rclone", bundledToolName(.linux));
    try std.testing.expectEqualStrings("rclone", bundledToolName(.macos));
    try std.testing.expectEqualStrings("Data/THIRD-PARTY-NOTICES.txt", third_party_notices_source);
}

test "staged layout requires canonical metadata and essential data roots" {
    const complete = [_][]const u8{
        "Game",                     "libPlatformRuntime.so",             "libStreamIO.so", "libStreamIOOptionsAbi.so", "libAnim.so",
        "libGFXGPU.so",             "libSDL3.so.0",                      "libImage.so",    "libInput.so",              "libNet.so",
        "libSFX.so",                "libUI.so",                          "libScene.so",    "libAILogic.so",            "libGameTT.so",
        shader_manifest_name,       "Shaders/GfxGpu/probe.vertex.spirv", "config.cfg",     "defconf.cfg",              "LICENSE.md",
        "Data/Configs/defconf.cfg", "Data/Maps/fixture.map",             "rclone",         third_party_notices_name,   "README.md",
    } ++ staged_docs;
    try std.testing.expect(isValidStagedLayout(&complete, .linux));

    try std.testing.expect(!isValidStagedLayout(complete[0 .. complete.len - 1], .linux));

    const missing_data = [_][]const u8{
        "Game",               "libPlatformRuntime.so",             "libStreamIO.so",         "libStreamIOOptionsAbi.so", "libAnim.so",
        "libGFXGPU.so",       "libSDL3.so.0",                      "libImage.so",            "libInput.so",              "libNet.so",
        "libSFX.so",          "libUI.so",                          "libScene.so",            "libAILogic.so",            "libGameTT.so",
        shader_manifest_name, "Shaders/GfxGpu/probe.vertex.spirv", "config.cfg",             "defconf.cfg",              "LICENSE.md",
        "README.md",          "rclone",                            third_party_notices_name,
    } ++ staged_docs;
    try std.testing.expect(!isValidStagedLayout(&missing_data, .linux));
}

test "staged layout requires the tools guide in every language" {
    const complete = [_][]const u8{
        "Game",                     "libPlatformRuntime.so",             "libStreamIO.so", "libStreamIOOptionsAbi.so", "libAnim.so",
        "libGFXGPU.so",             "libSDL3.so.0",                      "libImage.so",    "libInput.so",              "libNet.so",
        "libSFX.so",                "libUI.so",                          "libScene.so",    "libAILogic.so",            "libGameTT.so",
        shader_manifest_name,       "Shaders/GfxGpu/probe.vertex.spirv", "config.cfg",     "defconf.cfg",              "LICENSE.md",
        "Data/Configs/defconf.cfg", "Data/Maps/fixture.map",             "rclone",         third_party_notices_name,   "README.md",
    } ++ staged_docs;
    try verifyStagedLayout(&complete, .linux);
    // Each guide file is required on its own: dropping any one is named.
    for (required_tool_docs) |dropped| {
        var kept: [complete.len - 1][]const u8 = undefined;
        var count: usize = 0;
        for (complete) |name| {
            if (std.mem.eql(u8, name, dropped)) continue;
            kept[count] = name;
            count += 1;
        }
        try std.testing.expectError(error.MissingToolDoc, verifyStagedLayout(&kept, .linux));
    }
    // A missing root translation is a metadata fault, not a guide fault.
    var no_german: [complete.len - 1][]const u8 = undefined;
    var n: usize = 0;
    for (complete) |name| {
        if (std.mem.eql(u8, name, "README_German.md")) continue;
        no_german[n] = name;
        n += 1;
    }
    try std.testing.expectError(error.MissingMetadata, verifyStagedLayout(&no_german, .linux));
}

test "staged layout verifier rejects missing required entries" {
    try std.testing.expectError(error.MissingRuntimeEntry, verifyStagedLayout(&.{
        "Game", "libPlatformRuntime.so", shader_manifest_name, "Shaders/GfxGpu/probe.vertex.spirv", "config.cfg", "defconf.cfg",
    }, .linux));
    try std.testing.expectError(error.MissingGame, verifyStagedLayout(&.{
        "libPlatformRuntime.so", "libStreamIO.so", "libStreamIOOptionsAbi.so", "libAnim.so",         "libGFXGPU.so",                      "libSDL3.so.0",
        "libImage.so",           "libInput.so",    "libNet.so",                "libSFX.so",          "libUI.so",                          "libScene.so",
        "libAILogic.so",         "libGameTT.so",   "libGameTT.so",             shader_manifest_name, "Shaders/GfxGpu/probe.vertex.spirv", "config.cfg",
        "defconf.cfg",
    }, .linux));
    try std.testing.expectError(error.MissingShader, verifyStagedLayout(&.{
        "Game",         "libPlatformRuntime.so", "libStreamIO.so", "libStreamIOOptionsAbi.so", "libAnim.so", "libGFXGPU.so", "libSDL3.so.0",
        "libImage.so",  "libInput.so",           "libNet.so",      "libSFX.so",                "libUI.so",   "libScene.so",  "libAILogic.so",
        "libGameTT.so", shader_manifest_name,    "config.cfg",     "defconf.cfg",
    }, .linux));
    try std.testing.expectError(error.MissingConfig, verifyStagedLayout(&.{
        "Game",         "libPlatformRuntime.so", "libStreamIO.so",                    "libStreamIOOptionsAbi.so", "libAnim.so", "libGFXGPU.so", "libSDL3.so.0",
        "libImage.so",  "libInput.so",           "libNet.so",                         "libSFX.so",                "libUI.so",   "libScene.so",  "libAILogic.so",
        "libGameTT.so", shader_manifest_name,    "Shaders/GfxGpu/probe.vertex.spirv", "defconf.cfg",
    }, .linux));
}

test "staged layout verifier rejects duplicate and foreign runtimes" {
    try std.testing.expectError(error.DuplicatePlatformRuntime, verifyStagedLayout(&.{ "Game", "libPlatformRuntime.so", "libPlatformRuntime.so" }, .linux));
    try std.testing.expectError(error.UnexpectedPlatformRuntime, verifyStagedLayout(&.{ "Game", "PlatformRuntime.dll", "libPlatformRuntime.so" }, .linux));
}

test "staged layout verifier rejects unsafe and writable artifacts" {
    try expectLinuxManifestError("C:/outside.dll", error.UnsafeManifestPath);
    try expectLinuxManifestError("Data/../outside.dll", error.UnsafeManifestPath);
    try expectLinuxManifestError(".zig-cache/o/hash", error.ForbiddenArtifact);
    try expectLinuxManifestError("zig-out/bin/Game", error.ForbiddenArtifact);
    try expectLinuxManifestError("temp-runtime.dll", error.ForbiddenArtifact);
    try expectLinuxManifestError("Game.exe.stale", error.ForbiddenArtifact);
    try expectLinuxManifestError("saves/profile.sav", error.ForbiddenArtifact);
    try expectLinuxManifestError("Data/logs/stage.log", error.ForbiddenArtifact);
    try expectLinuxManifestError("Data/saves/profile.sav", error.ForbiddenArtifact);
}

test "forbidden-artifact rule spares a nested game asset directory named like a user-write directory" {
    // The Logs01-08 log-pile objects live at this path; only the stage root's
    // own top level, and Data's top level, are treated as user-write/cache
    // directories.
    try std.testing.expect(!isForbiddenArtifactPath("Data/Objects/SimpleObjects/common/summer/logs/01/1.xml"));
    try std.testing.expect(isForbiddenArtifactPath("Data/logs/stage.log"));
    try std.testing.expect(isForbiddenArtifactPath("logs/stage.log"));
}

test "staged layout verifier rejects duplicate entries and foreign target assets" {
    try expectLinuxManifestError("config.cfg", error.DuplicateManifestEntry);
    try expectLinuxManifestError("Game.exe", error.UnexpectedRuntimeEntry);
    try expectLinuxManifestError("Shaders/GfxGpu/probe.vertex.dxil", error.UnexpectedShader);
}

test "filesystem verifier walks staged roots with portable paths" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();

    const stage_path = "stage space/тест";
    try tmp.dir.createDirPath(io, stage_path);
    var stage = try tmp.dir.openDir(io, stage_path, .{ .iterate = true, .access_sub_paths = true });
    defer stage.close(io);
    try createLinuxStageFixture(io, stage);

    const root_path = try tmp.dir.realPathFileAlloc(io, stage_path, std.testing.allocator);
    defer std.testing.allocator.free(root_path);
    try verifyStagedRoot(io, std.testing.allocator, root_path, .linux);
}

test "filesystem verifier rejects forbidden directories" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();

    try createLinuxStageFixture(io, tmp.dir);
    try tmp.dir.createDirPath(io, "cache-temp");
    const root_path = try tmp.dir.realPathFileAlloc(io, ".", std.testing.allocator);
    defer std.testing.allocator.free(root_path);
    try std.testing.expectError(error.ForbiddenArtifact, verifyStagedRoot(io, std.testing.allocator, root_path, .linux));
}

fn createLinuxStageFixture(io: std.Io, root: std.Io.Dir) !void {
    try root.createDirPath(io, "Shaders/GfxGpu");
    try root.createDirPath(io, "Data/Configs");
    try root.createDirPath(io, "Data/Maps");
    try root.createDirPath(io, "Tools");
    for (requiredRuntimeNames(.linux)) |name| try touchFile(io, root, name);
    try touchFile(io, root, shader_manifest_name);
    try touchFile(io, root, "Shaders/GfxGpu/probe.vertex.spirv");
    try touchFile(io, root, "config.cfg");
    try touchFile(io, root, "defconf.cfg");
    try touchFile(io, root, "LICENSE.md");
    try touchFile(io, root, "README.md");
    for (staged_docs) |name| try touchFile(io, root, name);
    try touchFile(io, root, "Data/Configs/defconf.cfg");
    try touchFile(io, root, "Data/Maps/fixture.map");
    try touchFile(io, root, bundledToolName(.linux));
    try touchFile(io, root, third_party_notices_name);
}

fn touchFile(io: std.Io, dir: std.Io.Dir, path: []const u8) !void {
    const file = try dir.createFile(io, path, .{});
    file.close(io);
}

fn expectLinuxManifestError(extra: []const u8, expected: VerifyError) !void {
    const names = [_][]const u8{
        "Game",               "libPlatformRuntime.so",             "libStreamIO.so",        "libStreamIOOptionsAbi.so", "libAnim.so",
        "libGFXGPU.so",       "libSDL3.so.0",                      "libImage.so",           "libInput.so",              "libNet.so",
        "libSFX.so",          "libUI.so",                          "libScene.so",           "libAILogic.so",            "libGameTT.so",
        shader_manifest_name, "Shaders/GfxGpu/probe.vertex.spirv", "config.cfg",            "defconf.cfg",              "LICENSE.md",
        "README.md",          "Data/Configs/defconf.cfg",          "Data/Maps/fixture.map", "rclone",                   third_party_notices_name,
        extra,
    } ++ staged_docs;
    try std.testing.expectError(expected, verifyStagedLayout(&names, .linux));
}

/// The ten-language install (M002 D087): the nine language mods the language-mods
/// step writes below `mods/`, the Noto faces the editors and tools read from
/// `Fonts/` beside their executables, and the tools guide. The mod folders come
/// from the same table the language mods build reads, so a tenth language is
/// required here the moment the build produces it.
pub const l10n_languages = @import("l10n_languages.zig");

/// The files every language mod holds, relative to `mods/<mod>/`; the game reads a mod's
/// `mod.xml` from its `data/` folder, where language_mods.zig writes it.
pub const language_mod_files = [_][]const u8{ "data/mod.xml", "data/texts.pak" };

/// The licence travels with the faces it covers (SIL OFL).
pub const font_licence_file = "Fonts/OFL.txt";

/// Collect, in install order, every relative path the ten-language install
/// lacks. The caller owns the returned paths; an empty list is a complete install.
/// The faces the editors merge (Sources/editor/kit/fonts.zig Face), read from `<exe dir>/Fonts/`. The
/// game's language mods carry their atlases inside their paks and need no face installed.
const editor_faces = [_][]const u8{ "NotoSans-Regular.ttf", "NotoSansSC-Regular.otf", "NotoSansJP-Regular.otf", "NotoSansDevanagari-Regular.ttf" };

pub fn collectMissingL10nInstall(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, missing: *std.ArrayListUnmanaged([]const u8)) !void {
    for (l10n_languages.languages) |language| {
        for (language_mod_files) |file| {
            const path = try std.fmt.allocPrint(allocator, "mods/{s}/{s}", .{ language.mod, file });
            errdefer allocator.free(path);
            try appendIfMissing(io, allocator, root, path, missing);
        }
    }
    try appendIfMissing(io, allocator, root, try allocator.dupe(u8, font_licence_file), missing);
    for (editor_faces) |face| {
        const path = try std.fmt.allocPrint(allocator, "Fonts/{s}", .{face});
        try appendIfMissing(io, allocator, root, path, missing);
    }
    for (required_tool_docs) |doc| try appendIfMissing(io, allocator, root, try allocator.dupe(u8, doc), missing);
}

/// Takes ownership of `path`: kept in `missing` when the file is absent, freed otherwise.
fn appendIfMissing(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, path: []const u8, missing: *std.ArrayListUnmanaged([]const u8)) !void {
    errdefer allocator.free(path);
    root.access(io, path, .{}) catch {
        try missing.append(allocator, path);
        return;
    };
    allocator.free(path);
}

/// A language mod is texts only (M002 D060): bk_text draws plain Unicode from the engine's own faces, so a
/// mod carries no fonts and no Private Use Area code points (the retired bitmap-font hack mapped glyphs
/// there). Collect, as `<what>: <why>` lines, every mod file other than data/mod.xml and data/texts.pak,
/// and every texts.pak entry that is not a UTF-16 `.txt` free of U+E000..U+F8FF. A mod that is absent is
/// left to `collectMissingL10nInstall`.
pub fn collectNotTextsOnly(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, found: *std.ArrayListUnmanaged([]const u8)) !void {
    for (l10n_languages.languages) |language| {
        var path_buf: [128]u8 = undefined;
        const mod_path = try std.fmt.bufPrint(&path_buf, "mods/{s}", .{language.mod});
        var mod_dir = root.openDir(io, mod_path, .{ .iterate = true }) catch continue;
        defer mod_dir.close(io);
        var top = mod_dir.iterate();
        while (try top.next(io)) |entry| {
            if (entry.kind == .directory and std.mem.eql(u8, entry.name, "data")) continue;
            try appendFinding(allocator, found, "{s}/{s}: a language mod holds only data/", .{ mod_path, entry.name });
        }
        var data_dir = mod_dir.openDir(io, "data", .{ .iterate = true }) catch continue;
        defer data_dir.close(io);
        var inner = data_dir.iterate();
        while (try inner.next(io)) |entry| {
            if (entry.kind == .file and (std.mem.eql(u8, entry.name, "mod.xml") or std.mem.eql(u8, entry.name, "texts.pak"))) continue;
            try appendFinding(allocator, found, "{s}/data/{s}: a language mod holds only mod.xml and texts.pak (no fonts)", .{ mod_path, entry.name });
        }
        var pak_buf: [160]u8 = undefined;
        const pak_path = try std.fmt.bufPrint(&pak_buf, "{s}/data/texts.pak", .{mod_path});
        try scanPakForNonText(io, allocator, root, pak_path, found);
    }
}

fn appendFinding(allocator: std.mem.Allocator, found: *std.ArrayListUnmanaged([]const u8), comptime fmt: []const u8, args: anytype) !void {
    const line = try std.fmt.allocPrint(allocator, fmt, args);
    errdefer allocator.free(line);
    try found.append(allocator, line);
}

fn scanPakForNonText(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir, pak_path: []const u8, found: *std.ArrayListUnmanaged([]const u8)) !void {
    var file = root.openFile(io, pak_path, .{}) catch return;
    defer file.close(io);
    var read_buffer: [16 * 1024]u8 = undefined;
    var reader = file.reader(io, &read_buffer);
    var iterator = std.zip.Iterator.init(&reader) catch {
        try appendFinding(allocator, found, "{s}: not a readable zip", .{pak_path});
        return;
    };
    var name_buffer: [std.fs.max_path_bytes]u8 = undefined;
    while (try iterator.next()) |entry| {
        const name = try entry.getFilename(&reader, &name_buffer, .{ .allow_backslashes = true });
        if (name.len != 0 and name[name.len - 1] == '/') continue;
        if (!std.mem.endsWith(u8, name, ".txt")) {
            try appendFinding(allocator, found, "{s}: entry {s} is not a text", .{ pak_path, name });
            continue;
        }
        var text: std.Io.Writer.Allocating = .init(allocator);
        defer text.deinit();
        try entry.extractTo(&reader, &text.writer);
        const bytes = text.written();
        var i: usize = if (bytes.len >= 2 and bytes[0] == 0xFF and bytes[1] == 0xFE) 2 else 0;
        while (i + 1 < bytes.len) : (i += 2) {
            const unit = std.mem.readInt(u16, bytes[i..][0..2], .little);
            if (unit >= 0xE000 and unit <= 0xF8FF) {
                try appendFinding(allocator, found, "{s}: entry {s} holds Private Use Area code point U+{X:0>4}", .{ pak_path, name, unit });
                break;
            }
        }
    }
}

/// Print one `verify-runtime: missing <path>` line per absent item and fail when
/// there is any, so a failing install names everything to fix in one run.
pub fn verifyL10nInstall(io: std.Io, allocator: std.mem.Allocator, root: std.Io.Dir) !void {
    var missing: std.ArrayListUnmanaged([]const u8) = .empty;
    defer {
        for (missing.items) |path| allocator.free(path);
        missing.deinit(allocator);
    }
    try collectMissingL10nInstall(io, allocator, root, &missing);
    for (missing.items) |path| std.debug.print("verify-runtime: missing {s}\n", .{path});
    var not_texts: std.ArrayListUnmanaged([]const u8) = .empty;
    defer {
        for (not_texts.items) |line| allocator.free(line);
        not_texts.deinit(allocator);
    }
    try collectNotTextsOnly(io, allocator, root, &not_texts);
    for (not_texts.items) |line| std.debug.print("verify-runtime: not texts only {s}\n", .{line});
    if (missing.items.len != 0 or not_texts.items.len != 0) return error.MissingL10nInstall;
}

fn writeL10nInstallFixture(io: std.Io, root: std.Io.Dir) !void {
    for (l10n_languages.languages) |language| {
        var buf: [96]u8 = undefined;
        try root.createDirPath(io, try std.fmt.bufPrint(&buf, "mods/{s}/data", .{language.mod}));
        for (language_mod_files) |file| {
            const path = try std.fmt.bufPrint(&buf, "mods/{s}/{s}", .{ language.mod, file });
            try root.writeFile(io, .{ .sub_path = path, .data = "x" });
        }
        try writeTextsPak(io, root, language.mod, &.{ 0xFEFF, 'a' });
    }
    try root.createDirPath(io, "Fonts");
    try root.createDirPath(io, "Tools");
    try root.writeFile(io, .{ .sub_path = font_licence_file, .data = "x" });
    for (editor_faces) |face| {
        var buf: [96]u8 = undefined;
        try root.writeFile(io, .{ .sub_path = try std.fmt.bufPrint(&buf, "Fonts/{s}", .{face}), .data = "x" });
    }
    for (required_tool_docs) |doc| try root.writeFile(io, .{ .sub_path = doc, .data = "x" });
}

/// A one-entry stored zip, as the pak writer makes them, holding `units` as UTF-16LE with a BOM.
fn writeTextsPak(io: std.Io, root: std.Io.Dir, mod: []const u8, units: []const u16) !void {
    var data: [64]u8 = undefined;
    var n: usize = 0;
    for (units) |unit| {
        std.mem.writeInt(u16, data[n..][0..2], unit, .little);
        n += 2;
    }
    const name = "a.txt";
    const crc = std.hash.Crc32.hash(data[0..n]);
    var zip: [512]u8 = undefined;
    var w: std.Io.Writer = .fixed(&zip);
    // local file header
    try w.writeAll("PK\x03\x04");
    try w.writeInt(u16, 10, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u32, 0x00210000, .little);
    try w.writeInt(u32, crc, .little);
    try w.writeInt(u32, @intCast(n), .little);
    try w.writeInt(u32, @intCast(n), .little);
    try w.writeInt(u16, name.len, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeAll(name);
    try w.writeAll(data[0..n]);
    const central_offset = w.end;
    try w.writeAll("PK\x01\x02");
    try w.writeInt(u16, 20, .little);
    try w.writeInt(u16, 10, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u32, 0x00210000, .little);
    try w.writeInt(u32, crc, .little);
    try w.writeInt(u32, @intCast(n), .little);
    try w.writeInt(u32, @intCast(n), .little);
    try w.writeInt(u16, name.len, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u32, 0, .little);
    try w.writeInt(u32, 0, .little);
    try w.writeAll(name);
    const central_size = w.end - central_offset;
    try w.writeAll("PK\x05\x06");
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u16, 0, .little);
    try w.writeInt(u16, 1, .little);
    try w.writeInt(u16, 1, .little);
    try w.writeInt(u32, @intCast(central_size), .little);
    try w.writeInt(u32, @intCast(central_offset), .little);
    try w.writeInt(u16, 0, .little);
    var buf: [96]u8 = undefined;
    const path = try std.fmt.bufPrint(&buf, "mods/{s}/data/texts.pak", .{mod});
    try root.writeFile(io, .{ .sub_path = path, .data = zip[0..w.end] });
}

fn expectNotTextsOnly(io: std.Io, root: std.Io.Dir, expected: []const []const u8) !void {
    const allocator = std.testing.allocator;
    var found: std.ArrayListUnmanaged([]const u8) = .empty;
    defer {
        for (found.items) |line| allocator.free(line);
        found.deinit(allocator);
    }
    try collectNotTextsOnly(io, allocator, root, &found);
    try std.testing.expectEqual(expected.len, found.items.len);
    for (expected, found.items) |want, got| try std.testing.expectEqualStrings(want, got);
}

fn expectMissing(io: std.Io, root: std.Io.Dir, expected: []const []const u8) !void {
    const allocator = std.testing.allocator;
    var missing: std.ArrayListUnmanaged([]const u8) = .empty;
    defer {
        for (missing.items) |path| allocator.free(path);
        missing.deinit(allocator);
    }
    try collectMissingL10nInstall(io, allocator, root, &missing);
    try std.testing.expectEqual(expected.len, missing.items.len);
    for (expected, missing.items) |want, got| try std.testing.expectEqualStrings(want, got);
}

test "l10n install requires nine language mods" {
    try std.testing.expectEqual(@as(usize, 9), l10n_languages.languages.len);
}

test "l10n install check accepts a complete ten-language install" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();
    try writeL10nInstallFixture(io, tmp.dir);
    try expectMissing(io, tmp.dir, &.{});
    try verifyL10nInstall(io, std.testing.allocator, tmp.dir);
}

test "l10n install check names a language mod without its texts.pak" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();
    try writeL10nInstallFixture(io, tmp.dir);
    try tmp.dir.deleteFile(io, "mods/lang-pt-br/data/texts.pak");
    try expectMissing(io, tmp.dir, &.{"mods/lang-pt-br/data/texts.pak"});
    try std.testing.expectError(error.MissingL10nInstall, verifyL10nInstall(io, std.testing.allocator, tmp.dir));
}

test "l10n install check names a missing mod.xml and a whole missing mod" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();
    try writeL10nInstallFixture(io, tmp.dir);
    try tmp.dir.deleteFile(io, "mods/lang-ru/data/mod.xml");
    try tmp.dir.deleteTree(io, "mods/lang-id");
    try expectMissing(io, tmp.dir, &.{ "mods/lang-ru/data/mod.xml", "mods/lang-id/data/mod.xml", "mods/lang-id/data/texts.pak" });
}

test "l10n install check names the editor Fonts directory when it is missing" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();
    try writeL10nInstallFixture(io, tmp.dir);
    try tmp.dir.deleteTree(io, "Fonts");
    try expectMissing(io, tmp.dir, &.{
        "Fonts/OFL.txt",
        "Fonts/NotoSans-Regular.ttf",
        "Fonts/NotoSansSC-Regular.otf",
        "Fonts/NotoSansJP-Regular.otf",
        "Fonts/NotoSansDevanagari-Regular.ttf",
    });
}

test "l10n install check names a missing tools guide translation" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();
    try writeL10nInstallFixture(io, tmp.dir);
    try tmp.dir.deleteFile(io, "Tools/README_Hindi.md");
    try expectMissing(io, tmp.dir, &.{"Tools/README_Hindi.md"});
}

test "texts-only check accepts plain Unicode texts" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();
    try writeL10nInstallFixture(io, tmp.dir);
    try expectNotTextsOnly(io, tmp.dir, &.{});
}

test "texts-only check names a Private Use Area code point in a pak entry" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();
    try writeL10nInstallFixture(io, tmp.dir);
    try writeTextsPak(io, tmp.dir, "lang-fr", &.{ 0xFEFF, 'a', 0xE123 });
    try expectNotTextsOnly(io, tmp.dir, &.{"mods/lang-fr/data/texts.pak: entry a.txt holds Private Use Area code point U+E123"});
    try std.testing.expectError(error.MissingL10nInstall, verifyL10nInstall(io, std.testing.allocator, tmp.dir));
}

test "texts-only check names a fonts directory and a face inside a mod" {
    const io = std.testing.io;
    var tmp = std.testing.tmpDir(.{ .iterate = true });
    defer tmp.cleanup();
    try writeL10nInstallFixture(io, tmp.dir);
    try tmp.dir.createDirPath(io, "mods/lang-ja/fonts");
    try tmp.dir.writeFile(io, .{ .sub_path = "mods/lang-ja/data/NotoSansJP-Regular.otf", .data = "x" });
    try expectNotTextsOnly(io, tmp.dir, &.{
        "mods/lang-ja/fonts: a language mod holds only data/",
        "mods/lang-ja/data/NotoSansJP-Regular.otf: a language mod holds only mod.xml and texts.pak (no fonts)",
    });
}
