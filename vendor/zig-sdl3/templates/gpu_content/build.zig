const sdl3_build = @import("sdl3");
const std = @import("std");

const shaders = [_][]const u8{
    "texturedQuad.vert",

    "texturedQuad.frag",

    "fillTexture.comp",
};

const zig_spirv_execution_mode_fix_args = std.StaticStringMap([]const []const u8).initComptime(.{
    .{ "fillTexture.comp", &.{ "LocalSize", "8", "8", "1" } },
});

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
            false,
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
    const gpu_debug = b.option(bool, "gpu_debug", "Enable GPU debugging functionality") orelse (optimize == .Debug);
    options.addOption(bool, "gpu_debug", gpu_debug);

    const shadercross = sdl3_build.shadercross.cli(sdl3.builder, null, true, true) orelse return;
    const shader_metadata2zon = sdl3_build.shaders.compileShaderMetadata2Zon(sdl3.builder);
    const spirv_execution_mode = sdl3_build.shaders.compileSpirvExecutionMode(sdl3.builder);
    const write_files = b.addWriteFiles();

    const run = b.step("run", "Run the template");

    const exe_mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    exe_mod.addImport("sdl3", sdl3_mod);
    exe_mod.addOptions("options", options);

    const exe = b.addExecutable(.{
        .name = "gpu-content-template",
        .root_module = exe_mod,
    });
    b.installArtifact(exe);

    const run_art = b.addRunArtifact(exe);
    run_art.step.dependOn(b.getInstallStep());
    run.dependOn(&run_art.step);

    for (shaders) |shader| {
        var shader_files = compileShader(
            b,
            shadercross,
            shader_metadata2zon,
            write_files,
            shader_format,
            shader,
            gpu_debug,
            spirv_execution_mode,
        );
        shader_files.install(b, b.getInstallStep(), "content/shaders");
    }
}
