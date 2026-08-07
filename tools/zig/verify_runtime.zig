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
    UnsafeManifestPath,
    ForbiddenArtifact,
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
    if (!containsShader(names, target)) return error.MissingShader;
    if (!containsPath(names, "config.cfg")) return error.MissingConfig;
    if (!containsPath(names, "defconf.cfg")) return error.MissingDefaultConfig;
}

pub fn isValidStagedLayout(names: []const []const u8, target: RuntimeTarget) bool {
    verifyStagedLayout(names, target) catch return false;
    return true;
}

pub fn isSafeManifestPath(path: []const u8) bool {
    if (path.len == 0 or path[0] == '/' or path[0] == '\\') return false;
    if (path.len >= 2 and path[1] == ':') return false;

    var parts = std.mem.splitAny(u8, path, "/\\");
    while (parts.next()) |part| {
        if (std.mem.eql(u8, part, "..")) return false;
    }
    return true;
}

fn containsPath(names: []const []const u8, required: []const u8) bool {
    for (names) |name| if (pathEqual(name, required)) return true;
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

fn isForbiddenArtifactPath(path: []const u8) bool {
    var parts = std.mem.splitAny(u8, path, "/\\");
    while (parts.next()) |part| {
        if (part.len == 0) continue;
        if (isCacheName(part) or isTempName(part) or isUserWriteName(part)) return true;
        if (endsWithIgnoreCase(part, ".stale")) return true;
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

test "staged layout verifier accepts every target matrix" {
    try verifyStagedLayout(&.{
        "Game.exe",           "PlatformRuntime.dll",                 "StreamIO.dll", "StreamIOOptionsAbi.dll", "Anim.dll",
        "GFXGPU.dll",         "SDL3.dll",                            "Image.dll",    "Input.dll",              "Net.dll",
        "SFX.dll",            "UI.dll",                              "Scene.dll",    "AILogic.dll",            "GameTT.dll",
        shader_manifest_name, "Shaders/GfxGpu/textured.vertex.dxil", "config.cfg",   "defconf.cfg",
    }, .windows);
    try verifyStagedLayout(&.{
        "Game",               "libPlatformRuntime.so",                "libStreamIO.so", "libStreamIOOptionsAbi.so", "libAnim.so",
        "libGFXGPU.so",       "libSDL3.so.0",                         "libImage.so",    "libInput.so",              "libNet.so",
        "libSFX.so",          "libUI.so",                             "libScene.so",    "libAILogic.so",            "libGameTT.so",
        shader_manifest_name, "Shaders/GfxGpu/textured.vertex.spirv", "config.cfg",     "defconf.cfg",
    }, .linux);
    try verifyStagedLayout(&.{
        "Game",               "libPlatformRuntime.dylib",           "libStreamIO.dylib", "libStreamIOOptionsAbi.dylib", "libAnim.dylib",
        "libGFXGPU.dylib",    "libSDL3.dylib",                      "libImage.dylib",    "libInput.dylib",              "libNet.dylib",
        "libSFX.dylib",       "libUI.dylib",                        "libScene.dylib",    "libAILogic.dylib",            "libGameTT.dylib",
        shader_manifest_name, "Shaders/GfxGpu/textured.vertex.msl", "config.cfg",        "defconf.cfg",
    }, .macos);
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
}

fn expectLinuxManifestError(extra: []const u8, expected: VerifyError) !void {
    const names = [_][]const u8{
        "Game",               "libPlatformRuntime.so",             "libStreamIO.so", "libStreamIOOptionsAbi.so", "libAnim.so",
        "libGFXGPU.so",       "libSDL3.so.0",                      "libImage.so",    "libInput.so",              "libNet.so",
        "libSFX.so",          "libUI.so",                          "libScene.so",    "libAILogic.so",            "libGameTT.so",
        shader_manifest_name, "Shaders/GfxGpu/probe.vertex.spirv", "config.cfg",     "defconf.cfg",              extra,
    };
    try std.testing.expectError(expected, verifyStagedLayout(&names, .linux));
}
