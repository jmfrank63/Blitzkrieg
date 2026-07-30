const errors = @import("../errors.zig");
const gpu = @import("../gpu.zig");
const log = @import("../log.zig");
const std = @import("std");

/// Metadata for compute pipelines.
pub const ComputePipelineMetadata = @import("gpu/ComputePipelineMetadata.zig");

/// Metadata for graphics shaders.
pub const GraphicsShaderMetadata = @import("gpu/GraphicsShaderMetadata.zig");

/// An error with shader compatibility.
pub const CompatibilityError = error{
    /// There are some fragment shader inputs with no output from the vertex shader.
    FragmentShaderInputNotSatisfied,
    /// The types between the vertex and fragment shader are not compatible.
    TypesMismatch,
};

/// Error encountered when loading a shader.
pub const LoadError = error{
    /// Unable to load a shader format.
    UnsupportedFormat,
};

/// Format enumeration for a shader.
pub const ShaderFormat = std.meta.FieldEnum(gpu.ShaderFormatFlags);

/// Specifies a location for a variable in shader metadata.
pub const VariableLocation = union(enum) {
    /// Location specified by name.
    name: []const u8,
    /// Location specified by location.
    location: u32,
};

// /// Information relevant to vertex buffers extracted from graphics shader metadata.
// pub const VertexBuffersInfo = struct {
//     /// Descriptions of generated vertex buffers.
//     vertex_buffer_descriptions: []const gpu.VertexBufferDescription,
//     /// Generated vertex attributes.
//     vertex_attributes: []const gpu.VertexAttribute,
//     /// Types for each vertex buffer.
//     vertex_buffers: []const type,
// };

/// Ensure a vertex and fragment shader are compatible.
///
/// ## Function Parameters
/// * `vertex_shader_metadata`: The metadata for the vertex shader.
/// * `fragment_shader_metadata`: The metadata for the fragment shader.
pub fn ensureCompatibleGraphicsShaders(
    vertex_shader_metadata: GraphicsShaderMetadata,
    fragment_shader_metadata: GraphicsShaderMetadata,
) !void {
    outer: for (fragment_shader_metadata.inputs) |input| {
        for (vertex_shader_metadata.outputs) |output| {
            if (output.location == input.location) {
                if (output.type != input.type) {
                    try log.Category.gpu.logError("Fragment shader input \"{s}\" with type \"{s}\" and vertex shader output \"{s}\" with type \"{s}\" at location {d} have mismatching types", .{
                        input.name,
                        @tagName(input.type),
                        output.name,
                        @tagName(output.type),
                        input.location,
                    });
                    return error.TypesMismatch;
                }
                continue :outer;
            }
        } else {
            try log.Category.gpu.logError("Fragment shader input \"{s}\" at location {d} has no corresponding vertex shader output", .{ input.name, input.location });
            return error.FragmentShaderInputNotSatisfied;
        }
    }
}

/// Ensure a vertex and fragment shader are compatible at comptime.
///
/// ## Function Parameters
/// * `vertex_shader_metadata`: The metadata for the vertex shader.
/// * `fragment_shader_metadata`: The metadata for the fragment shader.
pub inline fn ensureCompatibleGraphicsShadersComptime(
    comptime vertex_shader_metadata: anytype,
    comptime fragment_shader_metadata: anytype,
) void {
    const vertex_shader_metadata_true = getGraphicsShaderMetadata(vertex_shader_metadata);
    const fragment_shader_metadata_true = getGraphicsShaderMetadata(fragment_shader_metadata);
    outer: inline for (fragment_shader_metadata_true.inputs) |input| {
        inline for (vertex_shader_metadata_true.outputs) |output| {
            if (output.location == input.location) {
                if (output.type != input.type) {
                    @compileError(std.fmt.comptimePrint("Fragment shader input \"{s}\" with type \"{s}\" and vertex shader output \"{s}\" with type \"{s}\" at location {d} have mismatching types", .{
                        input.name,
                        @tagName(input.type),
                        output.name,
                        @tagName(output.type),
                        input.location,
                    }));
                }
                continue :outer;
            }
        } else {
            @compileError(std.fmt.comptimePrint("Fragment shader input \"{s}\" at location {d} has no corresponding vertex shader output", .{ input.name, input.location }));
        }
    }
}

/// Get the compute pipeline metadata at compile time.
///
/// ## Function Parameters
/// * `metadata`: The embedded shader data to load.
///
/// ## Return Value
/// Returns the compute pipeline metadata.
pub fn getComputePipelineMetadata(
    metadata: anytype,
) ComputePipelineMetadata {
    return .{
        .readonly_storage_buffers = metadata.readonly_storage_buffers,
        .readonly_storage_textures = metadata.readonly_storage_textures,
        .readwrite_storage_buffers = metadata.readwrite_storage_buffers,
        .readwrite_storage_textures = metadata.readwrite_storage_textures,
        .samplers = metadata.samplers,
        .threadcount_x = metadata.threadcount_x,
        .threadcount_y = metadata.threadcount_y,
        .threadcount_z = metadata.threadcount_z,
        .uniform_buffers = metadata.uniform_buffers,
    };
}

/// Get the graphics shader metadata at compile time.
///
/// ## Function Parameters
/// * `metadata`: The embedded shader data to load.
///
/// ## Return Value
/// Returns the graphics shader metadata.
pub fn getGraphicsShaderMetadata(
    metadata: anytype,
) GraphicsShaderMetadata {
    var inputs: [metadata.inputs.len]GraphicsShaderMetadata.Var = undefined;
    var outputs: [metadata.outputs.len]GraphicsShaderMetadata.Var = undefined;
    inline for (0..inputs.len) |ind| {
        inputs[ind] = .{
            .name = metadata.inputs[ind].name,
            .type = metadata.inputs[ind].type,
            .location = metadata.inputs[ind].location,
        };
    }
    inline for (0..outputs.len) |ind| {
        outputs[ind] = .{
            .name = metadata.outputs[ind].name,
            .type = metadata.outputs[ind].type,
            .location = metadata.outputs[ind].location,
        };
    }
    return GraphicsShaderMetadata{
        .inputs = &inputs,
        .outputs = &outputs,
        .samplers = metadata.samplers,
        .storage_buffers = metadata.storage_buffers,
        .storage_textures = metadata.storage_textures,
        .uniform_buffers = metadata.uniform_buffers,
    };
}

/// Get the metadata field for a shader.
///
/// ## Function Parameters
/// * `shader`: The shader to get the metadata field of.
///
/// ## Return Value
/// Returns the field containing shader metadata.
pub fn getShaderMetadataField(
    shader: anytype,
) @TypeOf(shader.zon.data) {
    return shader.zon.data;
}

/// Load an embedded compute pipeline and use its metadata (will error if not exists).
///
/// ## Function Parameters
/// * `device`: The GPU device.
/// * `compute_kernel`: The embedded compute kernel to load.
///
/// ## Return Value
/// Returns the loaded compute pipeline.
pub fn loadComputePipelineEmbeddedWithMetadata(
    device: gpu.Device,
    compute_kernel: anytype,
) !gpu.ComputePipeline {
    var compute_kernels = std.EnumMap(ShaderFormat, []const u8){};
    inline for (@typeInfo(gpu.ShaderFormatFlags).@"struct".fields) |field| {
        if (@hasDecl(compute_kernel, field.name))
            compute_kernels.put(comptime std.meta.stringToEnum(ShaderFormat, field.name).?, @field(compute_kernel, field.name).data);
    }
    return loadComputePipelineWithMetadata(
        device,
        getComputePipelineMetadata(getShaderMetadataField(compute_kernel)),
        compute_kernels,
        compute_kernel.entry_point,
        compute_kernel.name,
    );
}

/// Load a compute pipeline from a directory with metadata.
///
/// ## Function Parameters
/// * `allocator`: Allocator used to temporarily allocate the ZON data and kernel file.
/// * `io`: IO used to load files from the directory.
/// * `read_buffer`: Buffer used to temporarilty read files.
/// * `device`: GPU device.
/// * `dir`: Kernel directory.
/// * `entry_point`: Name of the entry point of the kernel.
/// * `name`: Name of the kernel to load.
///
/// ## Return Value
/// Returns the loaded compute pipeline.
///
/// ## Remarks
/// This requires a ZON file to exist in the directory along with the metadata.
pub fn loadComputePipelineFromDirWithMetadata(
    allocator: std.mem.Allocator,
    io: std.Io,
    read_buffer: []u8,
    device: gpu.Device,
    dir: std.Io.Dir,
    entry_point: [:0]const u8,
    name: [:0]const u8,
) !struct { gpu.ComputePipeline, ComputePipelineMetadata } {
    var file_path: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const zon = try dir.openFile(io, try std.fmt.bufPrint(&file_path, "{s}.zon", .{name}), .{});
    defer zon.close(io);
    var zon_file = std.Io.Writer.Allocating.init(allocator);
    defer zon_file.deinit();
    var zon_reader = zon.reader(io, read_buffer);
    _ = try zon_reader.interface.streamRemaining(&zon_file.writer);
    const zon_data = try zon_file.toOwnedSliceSentinel(0);
    defer allocator.free(zon_data);
    const metadata = try std.zon.parse.fromSlice(ComputePipelineMetadata, allocator, zon_data, null, .{});

    const supported_formats = device.getShaderFormats();
    inline for (@typeInfo(gpu.ShaderFormatFlags).@"struct".fields) |field| {
        if (@field(supported_formats, field.name)) {
            if (dir.openFile(io, try std.fmt.bufPrint(&file_path, "{s}.{s}", .{ name, field.name }), .{})) |file| {
                defer file.close(io);

                var kernel_file = std.Io.Writer.Allocating.init(allocator);
                defer kernel_file.deinit();
                var kernel_reader = file.reader(io, read_buffer);
                _ = try kernel_reader.interface.streamRemaining(&kernel_file.writer);
                const kernel_data = try kernel_file.toOwnedSlice();
                defer allocator.free(kernel_data);

                var created_format = gpu.ShaderFormatFlags{};
                @field(created_format, field.name) = true;
                return .{
                    try device.createComputePipeline(.{
                        .code = kernel_data,
                        .entry_point = entry_point,
                        .format = created_format,
                        .thread_count_x = metadata.threadcount_x,
                        .thread_count_y = metadata.threadcount_y,
                        .thread_count_z = metadata.threadcount_z,
                        .num_readonly_storage_buffers = metadata.readonly_storage_buffers,
                        .num_readonly_storage_textures = metadata.readonly_storage_textures,
                        .num_readwrite_storage_buffers = metadata.readwrite_storage_buffers,
                        .num_readwrite_storage_textures = metadata.readwrite_storage_textures,
                        .num_samplers = metadata.samplers,
                        .num_uniform_buffers = metadata.uniform_buffers,
                        .props = .{ .name = name },
                    }),
                    metadata,
                };
            } else |_| {}
        }
    }
    try log.Category.gpu.logError("Unable to load shader for graphics format {any}", .{supported_formats});
    return error.UnsupportedFormat;
}

/// Load a compute pipeline based on metadata.
///
/// ## Function Parameters
/// * `device`: The GPU device.
/// * `metadata`: Metadata for the shader to load.
/// * `compute_kernels`: Map of kernel data for each format.
/// * `entry_point`: Name of the entry point of the kernel.
/// * `name`: Optional name of the pipeline to load.
///
/// ## Return Value
/// Returns the loaded graphics shader.
pub fn loadComputePipelineWithMetadata(
    device: gpu.Device,
    metadata: ComputePipelineMetadata,
    compute_kernels: std.EnumMap(ShaderFormat, []const u8),
    entry_point: [:0]const u8,
    name: ?[:0]const u8,
) !gpu.ComputePipeline {
    const supported_formats = device.getShaderFormats();
    inline for (@typeInfo(gpu.ShaderFormatFlags).@"struct".fields) |field| {
        if (@field(supported_formats, field.name)) {
            if (compute_kernels.get(comptime std.meta.stringToEnum(ShaderFormat, field.name).?)) |kernel| {
                var created_format = gpu.ShaderFormatFlags{};
                @field(created_format, field.name) = true;
                return try device.createComputePipeline(.{
                    .code = kernel,
                    .entry_point = entry_point,
                    .format = created_format,
                    .thread_count_x = metadata.threadcount_x,
                    .thread_count_y = metadata.threadcount_y,
                    .thread_count_z = metadata.threadcount_z,
                    .num_readonly_storage_buffers = metadata.readonly_storage_buffers,
                    .num_readonly_storage_textures = metadata.readonly_storage_textures,
                    .num_readwrite_storage_buffers = metadata.readwrite_storage_buffers,
                    .num_readwrite_storage_textures = metadata.readwrite_storage_textures,
                    .num_samplers = metadata.samplers,
                    .num_uniform_buffers = metadata.uniform_buffers,
                    .props = .{ .name = name },
                });
            }
        }
    }
    try log.Category.gpu.logError("Unable to load shader for graphics format {any}", .{supported_formats});
    return error.UnsupportedFormat;
}

/// Load an embedded graphics shader and use its metadata (will error if not exists).
///
/// ## Function Parameters
/// * `device`: The GPU device.
/// * `shader`: The embedded shader to load.
///
/// ## Return Value
/// Returns the loaded graphics shader.
pub fn loadGraphicsShaderEmbeddedWithMetadata(
    device: gpu.Device,
    shader: anytype,
) !gpu.Shader {
    var shaders = std.EnumMap(ShaderFormat, []const u8){};
    inline for (@typeInfo(gpu.ShaderFormatFlags).@"struct".fields) |field| {
        if (@hasDecl(shader, field.name))
            shaders.put(comptime std.meta.stringToEnum(ShaderFormat, field.name).?, @field(shader, field.name).data);
    }
    return loadGraphicsShaderWithMetadata(
        device,
        getGraphicsShaderMetadata(getShaderMetadataField(shader)),
        shaders,
        shader.entry_point,
        std.meta.stringToEnum(gpu.ShaderStage, shader.stage).?,
        shader.name,
    );
}

/// Load graphics shader from a directory with metadata.
///
/// ## Function Parameters
/// * `allocator`: Allocator used to temporarily allocate the ZON data and shader file.
/// * `io`: IO used to load files from the directory.
/// * `read_buffer`: Buffer used to temporarilty read files.
/// * `device`: GPU device.
/// * `dir`: Shader directory.
/// * `entry_point`: Name of the entry point of the shader.
/// * `stage`: Stage of the shader to load.
/// * `name`: Name of the shader to load.
///
/// ## Return Value
/// Returns the loaded graphics shader.
///
/// ## Remarks
/// This requires a ZON file to exist in the directory.
pub fn loadGraphicsShaderFromDirWithMetadata(
    allocator: std.mem.Allocator,
    io: std.Io,
    read_buffer: []u8,
    device: gpu.Device,
    dir: std.Io.Dir,
    entry_point: [:0]const u8,
    stage: gpu.ShaderStage,
    name: [:0]const u8,
) !gpu.Shader {
    var file_path: [std.Io.Dir.max_path_bytes]u8 = undefined;
    const zon = try dir.openFile(io, try std.fmt.bufPrint(&file_path, "{s}.zon", .{name}), .{});
    defer zon.close(io);
    var zon_file = std.Io.Writer.Allocating.init(allocator);
    defer zon_file.deinit();
    var zon_reader = zon.reader(io, read_buffer);
    _ = try zon_reader.interface.streamRemaining(&zon_file.writer);
    const zon_data = try zon_file.toOwnedSliceSentinel(0);
    defer allocator.free(zon_data);
    const metadata = try std.zon.parse.fromSliceAlloc(GraphicsShaderMetadata, allocator, zon_data, null, .{});
    defer std.zon.parse.free(allocator, metadata);

    const supported_formats = device.getShaderFormats();
    inline for (@typeInfo(gpu.ShaderFormatFlags).@"struct".fields) |field| {
        if (@field(supported_formats, field.name)) {
            if (dir.openFile(io, try std.fmt.bufPrint(&file_path, "{s}.{s}", .{ name, field.name }), .{})) |file| {
                defer file.close(io);

                var shader_file = std.Io.Writer.Allocating.init(allocator);
                defer shader_file.deinit();
                var shader_reader = file.reader(io, read_buffer);
                _ = try shader_reader.interface.streamRemaining(&shader_file.writer);
                const shader_data = try shader_file.toOwnedSlice();
                defer allocator.free(shader_data);

                var created_format = gpu.ShaderFormatFlags{};
                @field(created_format, field.name) = true;
                return try device.createShader(.{
                    .code = shader_data,
                    .entry_point = entry_point,
                    .format = created_format,
                    .stage = stage,
                    .num_samplers = metadata.samplers,
                    .num_storage_textures = metadata.storage_textures,
                    .num_storage_buffers = metadata.storage_buffers,
                    .num_uniform_buffers = metadata.uniform_buffers,
                    .props = .{ .name = name },
                });
            } else |_| {}
        }
    }
    try log.Category.gpu.logError("Unable to load shader for graphics format {any}", .{supported_formats});
    return error.UnsupportedFormat;
}

/// Load a graphics shader based on metadata.
///
/// ## Function Parameters
/// * `device`: The GPU device.
/// * `metadata`: Metadata for the shader to load.
/// * `shaders`: Map of shader data for each format.
/// * `entry_point`: Name of the entry point of the shader.
/// * `stage`: Stage of the shader to load.
/// * `name`: Optional name of the shader to load.
///
/// ## Return Value
/// Returns the loaded graphics shader.
pub fn loadGraphicsShaderWithMetadata(
    device: gpu.Device,
    metadata: GraphicsShaderMetadata,
    shaders: std.EnumMap(ShaderFormat, []const u8),
    entry_point: [:0]const u8,
    stage: gpu.ShaderStage,
    name: ?[:0]const u8,
) !gpu.Shader {
    const supported_formats = device.getShaderFormats();
    inline for (@typeInfo(gpu.ShaderFormatFlags).@"struct".fields) |field| {
        if (@field(supported_formats, field.name)) {
            if (shaders.get(comptime std.meta.stringToEnum(ShaderFormat, field.name).?)) |shader| {
                var created_format = gpu.ShaderFormatFlags{};
                @field(created_format, field.name) = true;
                return try device.createShader(.{
                    .code = shader,
                    .entry_point = entry_point,
                    .format = created_format,
                    .stage = stage,
                    .num_samplers = metadata.samplers,
                    .num_storage_textures = metadata.storage_textures,
                    .num_storage_buffers = metadata.storage_buffers,
                    .num_uniform_buffers = metadata.uniform_buffers,
                    .props = .{ .name = name },
                });
            }
        }
    }
    try log.Category.gpu.logError("Unable to load shader for graphics format {any}", .{supported_formats});
    return error.UnsupportedFormat;
}

/// Generate vertex buffer types based on a vertex shader's metadata. TODO: ADD FIELD THAT CONTAINS SLICE OF LOCATIONS TO NORMALIZE PAIRED WITH TYPES!
///
/// ## Function Parameters
/// * `vertex_shader_metadata`: Shader metadata for the vertex shader.
/// * `other_buffers`: Any grouping of variables to put in other buffers (starting at index `1`). Any unspecified locations will end up in buffer `0`.
///
/// ## Return Value
/// Returns the vertex buffer information.
pub fn vertexBufferTypes(
    comptime vertex_shader_metadata: anytype,
    comptime other_buffers: []const []VariableLocation,
) [other_buffers.len + 1]type {
    comptime {
        const vertex_shader_metadata_true = getGraphicsShaderMetadata(vertex_shader_metadata);
        const num_attribs = vertex_shader_metadata_true.inputs.len;
        const num_buffers = other_buffers.len + 1;

        var vertex_attrib_buffer_slots: [num_attribs]usize = undefined;
        var vertex_attrib_types: [num_attribs]type = undefined;
        var vertex_buffers: [num_buffers]type = undefined;
        var vertex_buffer_lens: [num_buffers]usize = @splat(0);

        // Populate vertex attributes mostly.
        for (&vertex_attrib_buffer_slots, &vertex_attrib_types, vertex_shader_metadata_true.inputs) |*attrib_buffer_slot, *attrib_type, input| {
            attrib_buffer_slot.* = 0; // In case no other buffer found.
            attrib_type.* = input.type.zigType();
            for (other_buffers, 1..) |other_buffer, other_buffer_ind| {
                for (other_buffer) |variable_location| {
                    switch (variable_location) {
                        .location => |location| {
                            if (input.location == location)
                                attrib_buffer_slot.* = other_buffer_ind;
                        },
                        .name => |name| {
                            if (std.mem.eql(u8, if (std.mem.cutScalarLast(u8, input.name, '.')) |val| val[1] else input.name, name))
                                attrib_buffer_slot.* = other_buffer_ind;
                        },
                    }
                }
            }
            vertex_buffer_lens[attrib_buffer_slot.*] += 1;
        }

        // Populate buffer types.
        for (&vertex_buffers, &vertex_buffer_lens, 0..) |*vertex_buffer, vertex_buffer_len, vertex_buffer_ind| {
            var field_names: [vertex_buffer_len][]const u8 = undefined;
            var field_types: [vertex_buffer_len]type = undefined;
            var attrib_indices: [vertex_buffer_len]usize = undefined;

            var curr_field_ind = 0;
            for (&vertex_attrib_buffer_slots, &vertex_attrib_types, vertex_shader_metadata_true.inputs, 0..) |attrib_buffer_slot, vertex_attrib_type, input, attrib_index| {
                if (attrib_buffer_slot == vertex_buffer_ind) {
                    field_names[curr_field_ind] = if (std.mem.cutScalarLast(u8, input.name, '.')) |val| val[1] else input.name;
                    field_types[curr_field_ind] = vertex_attrib_type;
                    attrib_indices[curr_field_ind] = attrib_index;
                    curr_field_ind += 1;
                }
            }

            vertex_buffer.* = @Struct(
                .@"extern",
                null,
                &field_names,
                &field_types,
                &@splat(.{}),
            );
        }

        return vertex_buffers;
    }
}

// /// Generate vertex buffers info based on a vertex shader's metadata. TODO: ADD FIELD THAT CONTAINS SLICE OF LOCATIONS TO NORMALIZE PAIRED WITH TYPES!
// ///
// /// ## Function Parameters
// /// * `vertex_shader_metadata`: Shader metadata for the vertex shader.
// /// * `other_buffers`: Any grouping of variables to put in other buffers (starting at index `1`). Any unspecified locations will end up in buffer `0`.
// ///
// /// ## Return Value
// /// Returns the vertex buffer information.
// pub fn vertexBuffersInfo(
//     comptime vertex_shader_metadata: GraphicsShaderMetadata,
//     comptime other_buffers: []const []VariableLocation,
// ) VertexBuffersInfo {
//     comptime {
//         const num_attribs = vertex_shader_metadata.inputs.len;
//         const num_buffers = other_buffers.len + 1;

//         var vertex_buffer_descriptions: [num_buffers]gpu.VertexBufferDescription = undefined;
//         var vertex_attributes: [num_attribs]gpu.VertexAttribute = undefined;
//         var vertex_buffers: [num_buffers]type = undefined;
//         var vertex_buffer_lens: [num_buffers]usize = @splat(0);

//         // Populate vertex attributes mostly.
//         for (&vertex_attributes, vertex_shader_metadata.inputs) |*attrib, input| {
//             attrib.buffer_slot = 0; // In case no other buffer found.
//             attrib.format = switch (input.type) {
//                 .byte => @compileError("Single element byte unsupported"),
//                 .byte2 => .i8x2,
//                 .byte3 => @compileError("Triple element byte unsupported"),
//                 .byte4 => .i8x4,
//                 .double, .double2, .double3, .double4 => @compileError("Double unsupported"),
//                 .float => .f32x1,
//                 .float2 => .f32x2,
//                 .float3 => .f32x3,
//                 .float4 => .f32x4,
//                 .half => @compileError("Single element half unsupported"),
//                 .half2 => .f16x2,
//                 .half3 => @compileError("Triple element half unsupported"),
//                 .half4 => .f16x4,
//                 .int => .i32x1,
//                 .int2 => .i32x2,
//                 .int3 => .i32x3,
//                 .int4 => .i32x4,
//                 .long, .long2, .long3, .long4 => @compileError("Long unsupported"),
//                 .short => @compileError("Single element short unsupported"),
//                 .short2 => .i16x2,
//                 .short3 => @compileError("Triple element short unsupported"),
//                 .short4 => .i16x4,
//                 .ubyte => @compileError("Single element ubyte unsupported"),
//                 .ubyte2 => .u8x2,
//                 .ubyte3 => @compileError("Triple element ubyte unsupported"),
//                 .ubyte4 => .u8x4,
//                 .uint => .u32x1,
//                 .uint2 => .u32x2,
//                 .uint3 => .u32x3,
//                 .uint4 => .u32x4,
//                 .ulong, .ulong2, .ulong3, .ulong4 => @compileError("Ulong unsupported"),
//                 .ushort => @compileError("Single element ushort unsupported"),
//                 .ushort2 => .u16x2,
//                 .ushort3 => @compileError("Triple element ushort unsupported"),
//                 .ushort4 => .u16x4,
//             };
//             attrib.location = input.location;
//             for (other_buffers, 1..) |other_buffer, other_buffer_ind| {
//                 for (other_buffer) |variable_location| {
//                     switch (variable_location) {
//                         .location => |location| {
//                             if (input.location == location)
//                                 attrib.buffer_slot = other_buffer_ind;
//                         },
//                         .name => |name| {
//                             if (std.mem.eql(u8, input.name, name))
//                                 attrib.buffer_slot = other_buffer_ind;
//                         },
//                     }
//                 }
//             }
//             vertex_buffer_lens[attrib.buffer_slot] += 1;
//         }

//         // Populate buffer types.
//         for (&vertex_buffers, &vertex_buffer_descriptions, &vertex_buffer_lens, 0..) |*vertex_buffer, *vertex_buffer_description, vertex_buffer_len, vertex_buffer_ind| {
//             var field_names: [vertex_buffer_len][]const u8 = undefined;
//             var field_types: [vertex_buffer_len]type = undefined;
//             var attrib_indices: [vertex_buffer_len]usize = undefined;

//             var curr_field_ind = 0;
//             for (vertex_attributes, vertex_shader_metadata.inputs, 0..) |attrib, input, attrib_index| {
//                 if (attrib.buffer_slot == vertex_buffer_ind) {
//                     field_names[curr_field_ind] = input.name;
//                     field_types[curr_field_ind] = input.type.zigType();
//                     attrib_indices[curr_field_ind] = attrib_index;
//                     curr_field_ind += 1;
//                 }
//             }

//             vertex_buffer.* = @Struct(
//                 .@"extern",
//                 null,
//                 &field_names,
//                 &field_types,
//                 &@splat(.{}),
//             );
//             vertex_buffer_description.* = .{
//                 .input_rate = .vertex,
//                 .pitch = @sizeOf(vertex_buffer.*),
//                 .slot = vertex_buffer_ind,
//                 .instance_step_rate = 0,
//             };
//             for (0..vertex_buffer_len) |ind| {
//                 const attrib_ind = attrib_indices[ind];
//                 vertex_attributes[attrib_ind].offset = @offsetOf(vertex_buffer.*, vertex_shader_metadata.inputs[attrib_ind].name);
//             }
//         }

//         return .{
//             .vertex_attributes = &vertex_attributes,
//             .vertex_buffer_descriptions = &vertex_buffer_descriptions,
//             .vertex_buffers = &vertex_buffers,
//         };
//     }
// }

test "extras gpu" {
    errors.refAllDeclsRecursive(@This());
}
