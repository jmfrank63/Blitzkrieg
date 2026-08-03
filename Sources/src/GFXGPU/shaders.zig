const std = @import("std");
const manifest = @import("shader_manifest.zig");
const bindings = @import("bindings.zig");
const sdl = @import("sdl.zig");
const device_mod = @import("device.zig");

pub const Error = manifest.Error || bindings.UniformError || std.mem.Allocator.Error || error{
    EffectNotFound,
    MissingShaderStage,
    ShaderCreationFailed,
    ByteLengthMismatch,
    HashMismatch,
    InvalidEntryPoint,
    FormatMismatch,
};

pub const ShaderCreateInfo = struct {
    code: []const u8,
    entry_point: []const u8,
    format: u32,
    stage: manifest.Stage,
    sampler_count: u32,
    storage_texture_count: u32,
    storage_buffer_count: u32,
    uniform_buffer_count: u32,
};

pub const Api = struct {
    create: *const fn (*anyopaque, ShaderCreateInfo) ?*anyopaque,
    release: *const fn (*anyopaque, *anyopaque) void,
};

fn realCreate(device: *anyopaque, info: ShaderCreateInfo) ?*anyopaque {
    var entry: [std.math.maxInt(u16) + 1]u8 = undefined;
    if (info.entry_point.len >= entry.len) return null;
    @memcpy(entry[0..info.entry_point.len], info.entry_point);
    entry[info.entry_point.len] = 0;
    const create_info = sdl.c.SDL_GPUShaderCreateInfo{
        .code_size = info.code.len,
        .code = info.code.ptr,
        .entrypoint = @ptrCast(&entry),
        .format = info.format,
        .stage = @intFromEnum(info.stage),
        .num_samplers = info.sampler_count,
        .num_storage_textures = info.storage_texture_count,
        .num_storage_buffers = info.storage_buffer_count,
        .num_uniform_buffers = info.uniform_buffer_count,
        .props = 0,
    };
    return @ptrCast(sdl.c.SDL_CreateGPUShader(@ptrCast(@alignCast(device)), &create_info));
}

fn realRelease(device: *anyopaque, shader: *anyopaque) void {
    sdl.c.SDL_ReleaseGPUShader(@ptrCast(@alignCast(device)), @ptrCast(@alignCast(shader)));
}

pub const real_api = Api{ .create = realCreate, .release = realRelease };

const Pair = struct {
    effect: []u8,
    vertex: *anyopaque,
    fragment: *anyopaque,
};

pub const Loader = struct {
    allocator: std.mem.Allocator,
    device: *anyopaque,
    api: Api,
    pairs: std.ArrayListUnmanaged(Pair) = .empty,

    pub fn init(allocator: std.mem.Allocator, device: *anyopaque, api: Api) Loader {
        return .{ .allocator = allocator, .device = device, .api = api };
    }

    pub fn deinit(self: *Loader) void {
        var index = self.pairs.items.len;
        while (index > 0) {
            index -= 1;
            const loaded_pair = self.pairs.items[index];
            self.api.release(self.device, loaded_pair.fragment);
            self.api.release(self.device, loaded_pair.vertex);
            self.allocator.free(loaded_pair.effect);
        }
        self.pairs.deinit(self.allocator);
        self.* = undefined;
    }

    pub fn count(self: *const Loader) usize {
        return self.pairs.items.len;
    }

    pub fn pair(self: *const Loader, effect: []const u8) ?struct { vertex: *anyopaque, fragment: *anyopaque } {
        for (self.pairs.items) |value| {
            if (std.mem.eql(u8, value.effect, effect)) return .{ .vertex = value.vertex, .fragment = value.fragment };
        }
        return null;
    }

    pub fn loadPair(self: *Loader, shader_manifest: *const manifest.Manifest, effect: []const u8, vertex_blob: []const u8, fragment_blob: []const u8, selected_format: u32) Error!void {
        const format = switch (selected_format) {
            sdl.c.SDL_GPU_SHADERFORMAT_DXIL => manifest.Format.dxil,
            sdl.c.SDL_GPU_SHADERFORMAT_SPIRV => manifest.Format.spirv,
            sdl.c.SDL_GPU_SHADERFORMAT_MSL => manifest.Format.msl,
            else => return Error.FormatMismatch,
        };
        if (selected_format & device_mod.formatFlag(format) == 0) return Error.FormatMismatch;
        for (self.pairs.items) |loaded_pair| if (std.mem.eql(u8, loaded_pair.effect, effect)) return;
        var vertex_record: ?*const manifest.Record = null;
        var fragment_record: ?*const manifest.Record = null;
        for (shader_manifest.records) |*record| {
            if (!std.mem.eql(u8, record.effect, effect) or record.format != format) continue;
            switch (record.stage) {
                .vertex => vertex_record = record,
                .fragment => fragment_record = record,
                .compute => {},
            }
        }
        const vertex = vertex_record orelse return Error.MissingShaderStage;
        const fragment = fragment_record orelse return Error.MissingShaderStage;
        try bindings.validateResourceCounts(vertex);
        try bindings.validateResourceCounts(fragment);
        try validateBlob(vertex, vertex_blob);
        try validateBlob(fragment, fragment_blob);
        const vertex_handle = self.api.create(self.device, info(vertex, vertex_blob, selected_format)) orelse return Error.ShaderCreationFailed;
        errdefer self.api.release(self.device, vertex_handle);
        const fragment_handle = self.api.create(self.device, info(fragment, fragment_blob, selected_format)) orelse return Error.ShaderCreationFailed;
        errdefer self.api.release(self.device, fragment_handle);
        const effect_copy = try self.allocator.dupe(u8, effect);
        errdefer self.allocator.free(effect_copy);
        try self.pairs.append(self.allocator, .{ .effect = effect_copy, .vertex = vertex_handle, .fragment = fragment_handle });
    }

    fn validateBlob(record: *const manifest.Record, blob: []const u8) Error!void {
        if (record.byte_length != blob.len) return Error.ByteLengthMismatch;
        var hash: [32]u8 = undefined;
        std.crypto.hash.sha2.Sha256.hash(blob, &hash, .{});
        if (!std.mem.eql(u8, &hash, &record.hash)) return Error.HashMismatch;
        if (record.entry_point.len == 0) return Error.InvalidEntryPoint;
    }

    fn info(record: *const manifest.Record, blob: []const u8, format: u32) ShaderCreateInfo {
        return .{ .code = blob, .entry_point = record.entry_point, .format = format, .stage = record.stage, .sampler_count = record.sampler_count, .storage_texture_count = record.storage_texture_count, .storage_buffer_count = record.storage_buffer_count, .uniform_buffer_count = record.uniform_buffer_count };
    }
};

pub const LiveCounts = struct { shaders: u32 = 0 };

fn testRecord(allocator: std.mem.Allocator, stage: manifest.Stage, blob: []const u8) !manifest.Record {
    var hash: [32]u8 = undefined;
    std.crypto.hash.sha2.Sha256.hash(blob, &hash, .{});
    return .{ .effect = try allocator.dupe(u8, "probe"), .name = try allocator.dupe(u8, "Probe"), .entry_point = try allocator.dupe(u8, if (stage == .vertex) "VSMain" else "PSMain"), .stage = stage, .blob_path = try allocator.dupe(u8, if (stage == .vertex) "probe.vertex.dxil" else "probe.fragment.dxil"), .byte_length = @intCast(blob.len), .required_vertex_mask = 0, .sampler_count = 2, .storage_texture_count = 3, .storage_buffer_count = 4, .uniform_buffer_count = 5, .hash = hash };
}

test "shader loader validates byte lengths and hashes" {
    const vertex_blob = "vertex";
    const fragment_blob = "fragment";
    var records = try std.testing.allocator.alloc(manifest.Record, 2);
    records[0] = try testRecord(std.testing.allocator, .vertex, vertex_blob);
    records[1] = try testRecord(std.testing.allocator, .fragment, fragment_blob);
    var shader_manifest = manifest.Manifest{ .format = .dxil, .records = records };
    defer shader_manifest.deinit(std.testing.allocator);
    var loader = Loader.init(std.testing.allocator, @ptrFromInt(1), real_api);
    try std.testing.expectError(Error.ByteLengthMismatch, loader.loadPair(&shader_manifest, "probe", "bad", fragment_blob, sdl.c.SDL_GPU_SHADERFORMAT_DXIL));
    try std.testing.expectError(Error.HashMismatch, loader.loadPair(&shader_manifest, "probe", vertex_blob, "bad!", sdl.c.SDL_GPU_SHADERFORMAT_DXIL));
    loader.deinit();
}

test "shader loader rolls back vertex when fragment creation fails" {
    const Context = struct { creates: u32 = 0, releases: u32 = 0, fail_at: u32 = 2 };
    var context = Context{};
    const Fake = struct {
        var ctx: *Context = undefined;
        fn create(_: *anyopaque, _: ShaderCreateInfo) ?*anyopaque {
            ctx.creates += 1;
            if (ctx.creates == ctx.fail_at) return null;
            return @ptrCast(&ctx.creates);
        }
        fn release(_: *anyopaque, _: *anyopaque) void {
            ctx.releases += 1;
        }
    };
    Fake.ctx = &context;
    var loader = Loader.init(std.testing.allocator, @ptrCast(&context), .{ .create = Fake.create, .release = Fake.release });
    defer loader.deinit();
    const vertex_blob = "vertex";
    const fragment_blob = "fragment";
    var records = try std.testing.allocator.alloc(manifest.Record, 2);
    records[0] = try testRecord(std.testing.allocator, .vertex, vertex_blob);
    records[1] = try testRecord(std.testing.allocator, .fragment, fragment_blob);
    var shader_manifest = manifest.Manifest{ .format = .dxil, .records = records };
    defer shader_manifest.deinit(std.testing.allocator);
    try std.testing.expectError(Error.ShaderCreationFailed, loader.loadPair(&shader_manifest, "probe", vertex_blob, fragment_blob, sdl.c.SDL_GPU_SHADERFORMAT_DXIL));
    try std.testing.expectEqual(@as(usize, 0), loader.count());
    try std.testing.expectEqual(@as(u32, 1), context.releases);
}
