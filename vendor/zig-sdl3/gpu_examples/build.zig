const sdl3_build = @import("sdl3");
const std = @import("std");

const examples = [_]Example{
    .{ "basic-triangle", &.{ "rawTriangle.vert", "solidColor.frag" } },
    .{ "basic-compute", &.{ "texturedQuad.vert", "texturedQuad.frag", "fillTexture.comp" } },
    .{ "clear-screen", &.{} },
    .{ "textured-quad", &.{ "texturedQuad.vert", "texturedQuad.frag" } },
};

const shaders = [_][]const u8{
    "rawTriangle.vert",
    "texturedQuad.vert",

    "solidColor.frag",
    "texturedQuad.frag",

    "fillTexture.comp",
};

const zig_spirv_execution_mode_fix_args = std.StaticStringMap([]const []const u8).initComptime(.{
    .{ "fillTexture.comp", &.{ "LocalSize", "8", "8", "1" } },
});

const Example = struct {
    []const u8, // Name.
    []const []const u8, // Shader deps.
};

const ShaderFormat = enum {
    glsl,
    hlsl,
    zig,
};

fn compileShader(
    b: *std.Build,
    shadercross: *std.Build.Step.Compile,
    shader_metadata2zon: *std.Build.Step.Compile,
    write_files: *std.Build.Step.WriteFile,
    shader_format: ShaderFormat,
    name: []const u8,
    gpu_debug: bool,
    spirv_execution_mode: *std.Build.Step.Compile,
) sdl3_build.shaders.ShaderFiles {
    const shader_formats = std.EnumSet(sdl3_build.shaders.ShadercrossFormat).initMany(&.{
        .dxil,
        .msl,
        .spirv,
        .zon,
    });

    const stage: sdl3_build.shaders.ShadercrossStage = if (std.mem.endsWith(u8, name, ".vert")) .vertex else if (std.mem.endsWith(u8, name, ".frag")) .fragment else if (std.mem.endsWith(u8, name, ".comp")) .compute else @panic(b.fmt("No stage detected for shader {s}", .{name}));
    return switch (shader_format) {
        .glsl => sdl3_build.shaders.compileGlslForFormats(
            b,
            .{
                .input = b.path(b.fmt("shaders/glsl/{s}.glsl", .{name})),
                .shadercross = shadercross,
                .stage = stage,
                .debug = gpu_debug,
                .shader_metadata2zon = shader_metadata2zon,
            },
            shader_formats,
            name,
            write_files,
        ),
        .hlsl => sdl3_build.shaders.compileHlslForFormats(
            b,
            .{
                .input = b.path(b.fmt("shaders/hlsl/{s}.hlsl", .{name})),
                .shadercross = shadercross,
                .stage = stage,
                .debug = gpu_debug,
                .shader_metadata2zon = shader_metadata2zon,
            },
            shader_formats,
            name,
            write_files,
            true,
        ),
        .zig => sdl3_build.shaders.compileZigForFormats(
            b,
            .{
                .input = b.path(b.fmt("shaders/zig/{s}.zig", .{name})),
                .shadercross = shadercross,
                .stage = stage,
                .debug = gpu_debug,
                .shader_metadata2zon = shader_metadata2zon,
            },
            shader_formats,
            name,
            write_files,
            spirv_execution_mode,
            if (zig_spirv_execution_mode_fix_args.get(name)) |val| val else null,
        ),
    };
}

fn setupExample(
    b: *std.Build,
    sdl3: *std.Build.Module,
    options: *std.Build.Step.Options,
    name: []const u8,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) struct { *std.Build.Step.Compile, *std.Build.Step.InstallArtifact } {
    const exe_mod = b.createModule(.{
        .root_source_file = b.path(b.fmt("src/{s}.zig", .{name})),
        .target = target,
        .optimize = optimize,
    });
    exe_mod.addImport("sdl3", sdl3);
    exe_mod.addOptions("options", options);

    const exe = b.addExecutable(.{
        .name = name,
        .root_module = exe_mod,
    });
    return .{ exe, b.addInstallArtifact(exe, .{}) };
}

pub fn build(
    b: *std.Build,
) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const sdl3 = b.dependency("sdl3", .{
        .target = target,
        .optimize = optimize,
    });
    const sdl3_mod = sdl3.module("sdl3");
    const options = b.addOptions();

    const shader_format = b.option(ShaderFormat, "shader_format", "Shader format to use") orelse .zig;
    const gpu_debug = b.option(bool, "gpu_debug", "Enable GPU debugging functionality") orelse false;
    options.addOption(bool, "gpu_debug", gpu_debug);

    const shadercross = sdl3_build.shadercross.cli(sdl3.builder, null, true, true) orelse return;
    const shader_metadata2zon = sdl3_build.shaders.compileShaderMetadata2Zon(sdl3.builder);
    const spirv_execution_mode = sdl3_build.shaders.compileSpirvExecutionMode(sdl3.builder);
    const write_files = b.addWriteFiles();

    var compiled_shaders = std.StringHashMap(sdl3_build.shaders.ShaderFiles).init(b.allocator);
    defer compiled_shaders.deinit();
    for (shaders) |shader| {
        try compiled_shaders.put(shader, compileShader(
            b,
            shadercross,
            shader_metadata2zon,
            write_files,
            shader_format,
            shader,
            gpu_debug,
            spirv_execution_mode,
        ));
    }

    const examples_step = b.step("examples", "Build all examples");
    const run_example: ?[]const u8 = b.option([]const u8, "example", "The example name for running an example") orelse null;
    const run = b.step("run", "Run an example with -Dexample=<example_name> option");
    for (examples) |example| {
        const exe, const install_artifact = setupExample(b, sdl3_mod, options, example[0], target, optimize);
        examples_step.dependOn(&install_artifact.step);
        if (run_example) |run_example_name| {
            if (std.mem.eql(u8, run_example_name, example[0])) {
                const run_art = b.addRunArtifact(exe);
                run_art.step.dependOn(&install_artifact.step);
                run.dependOn(&run_art.step);
            }
        }

        for (example[1]) |shader_dep| {
            try compiled_shaders.getPtr(shader_dep).?.addSource(b, exe.root_module, write_files);
        }
    }
}
