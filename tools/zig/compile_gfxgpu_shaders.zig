const std = @import("std");

const Stage = enum { vertex, fragment, compute };

const Define = struct {
    name: []const u8,
    value: ?[]const u8 = null,
};

const SourceRecord = struct {
    effect: []const u8,
    name: []const u8,
    stage: Stage,
    source: []const u8,
    entry: []const u8,
    defines: []Define = &.{},
    required_vertex_mask: u32,
    sampler_count: u32,
    storage_texture_count: u32,
    storage_buffer_count: u32,
    uniform_buffer_count: u32,
};

const ManifestHeader = struct {
    magic: [4]u8 = .{ 'G', 'F', 'X', 'S' },
    schema_version: u16 = 1,
    format: u8 = 1,
    reserved: u8 = 0,
    record_count: u32,
};

const CompiledRecord = struct {
    source: SourceRecord,
    blob_path: []const u8,
    byte_length: u32,
    hash: [32]u8,
};

fn parseManifest(io: std.Io, allocator: std.mem.Allocator, json_text: []const u8, shader_dir: []const u8) ![]SourceRecord {
    var parsed = std.json.parseFromSlice([]SourceRecord, allocator, json_text, .{}) catch |err| switch (err) {
        error.MissingField => return error.MissingEntryPoint,
        error.InvalidEnumTag => return error.UnknownStage,
        else => return err,
    };
    const records = try allocator.alloc(SourceRecord, parsed.value.len);
    @memcpy(records, parsed.value);
    for (records, parsed.value) |*record, source_record| {
        record.effect = try allocator.dupe(u8, source_record.effect);
        record.name = try allocator.dupe(u8, source_record.name);
        record.source = try allocator.dupe(u8, source_record.source);
        record.entry = try allocator.dupe(u8, source_record.entry);
        record.defines = try allocator.alloc(Define, source_record.defines.len);
        for (source_record.defines, 0..) |source_define, index| {
            record.defines[index].name = try allocator.dupe(u8, source_define.name);
            record.defines[index].value = if (source_define.value) |value| try allocator.dupe(u8, value) else null;
        }
    }
    parsed.deinit();
    errdefer freeRecords(allocator, records);
    for (records) |record| {
        if (record.effect.len == 0 or record.name.len == 0 or record.entry.len == 0)
            return error.MissingEntryPoint;
        if (invalidPath(record.source)) return error.AbsoluteSourcePath;
        if (hasTraversal(record.source)) return error.SourcePathTraversal;
        const source_path = try std.fs.path.join(allocator, &.{ shader_dir, record.source });
        defer allocator.free(source_path);
        std.Io.Dir.cwd().access(io, source_path, .{}) catch return error.MissingSource;
        for (record.defines, 0..) |define, index| {
            if (define.name.len == 0) return error.DuplicateDefine;
            for (record.defines[0..index]) |previous| {
                if (std.mem.eql(u8, define.name, previous.name)) return error.DuplicateDefine;
            }
        }
    }
    for (records, 0..) |record, index| {
        for (records[0..index]) |previous| {
            if (std.mem.eql(u8, record.effect, previous.effect) and record.stage == previous.stage)
                return error.DuplicateEffectStage;
        }
    }
    sortRecords(records);
    return records;
}

fn freeRecords(allocator: std.mem.Allocator, records: []SourceRecord) void {
    for (records) |record| {
        allocator.free(record.effect);
        allocator.free(record.name);
        allocator.free(record.source);
        allocator.free(record.entry);
        for (record.defines) |define| {
            allocator.free(define.name);
            if (define.value) |value| allocator.free(value);
        }
        allocator.free(record.defines);
    }
    allocator.free(records);
}

fn invalidPath(path: []const u8) bool {
    return path.len == 0 or path[0] == '/' or path[0] == '\\' or
        (path.len >= 2 and path[1] == ':');
}

fn hasTraversal(path: []const u8) bool {
    var iterator = std.mem.splitAny(u8, path, "/\\");
    while (iterator.next()) |part| if (std.mem.eql(u8, part, "..")) return true;
    return false;
}

fn stageOrder(stage: Stage) u8 {
    return switch (stage) {
        .vertex => 0,
        .fragment => 1,
        .compute => 2,
    };
}

fn lessThan(left: SourceRecord, right: SourceRecord) bool {
    const effect_order = std.mem.order(u8, left.effect, right.effect);
    if (effect_order != .eq) return effect_order == .lt;
    if (stageOrder(left.stage) != stageOrder(right.stage)) return stageOrder(left.stage) < stageOrder(right.stage);
    return std.mem.order(u8, left.name, right.name) == .lt;
}

fn sortRecords(records: []SourceRecord) void {
    var index: usize = 1;
    while (index < records.len) : (index += 1) {
        const value = records[index];
        var position = index;
        while (position > 0 and lessThan(value, records[position - 1])) : (position -= 1)
            records[position] = records[position - 1];
        records[position] = value;
    }
}

fn stageName(stage: Stage) []const u8 {
    return switch (stage) {
        .vertex => "vertex",
        .fragment => "fragment",
        .compute => "compute",
    };
}

fn stageByte(stage: Stage) u8 {
    return stageOrder(stage);
}

fn appendU16(output: *std.ArrayListUnmanaged(u8), allocator: std.mem.Allocator, value: u16) !void {
    var bytes: [2]u8 = undefined;
    std.mem.writeInt(u16, &bytes, value, .little);
    try output.appendSlice(allocator, &bytes);
}

fn appendU32(output: *std.ArrayListUnmanaged(u8), allocator: std.mem.Allocator, value: u32) !void {
    var bytes: [4]u8 = undefined;
    std.mem.writeInt(u32, &bytes, value, .little);
    try output.appendSlice(allocator, &bytes);
}

fn appendString(output: *std.ArrayListUnmanaged(u8), allocator: std.mem.Allocator, value: []const u8) !void {
    try appendU16(output, allocator, @intCast(value.len));
    try output.appendSlice(allocator, value);
}

fn writeRuntimeManifest(io: std.Io, allocator: std.mem.Allocator, output_dir: []const u8, records: []const CompiledRecord) !void {
    var output: std.ArrayListUnmanaged(u8) = .empty;
    defer output.deinit(allocator);
    try output.appendSlice(allocator, "GFXS");
    try appendU16(&output, allocator, 2);
    try output.appendSlice(allocator, &.{ 1, 0 });
    try appendU32(&output, allocator, @intCast(records.len));
    for (records) |record| {
        try appendString(&output, allocator, record.source.effect);
        try appendString(&output, allocator, record.source.name);
        try appendString(&output, allocator, record.source.entry);
        try output.append(allocator, stageByte(record.source.stage));
        try output.append(allocator, 0);
        try appendString(&output, allocator, record.blob_path);
        try appendU32(&output, allocator, record.byte_length);
        try appendU32(&output, allocator, record.source.required_vertex_mask);
        try appendU32(&output, allocator, record.source.sampler_count);
        try appendU32(&output, allocator, record.source.storage_texture_count);
        try appendU32(&output, allocator, record.source.storage_buffer_count);
        try appendU32(&output, allocator, record.source.uniform_buffer_count);
        try output.appendSlice(allocator, &record.hash);
    }
    try std.Io.Dir.cwd().writeFile(io, .{
        .sub_path = try std.fs.path.join(allocator, &.{ output_dir, "gfxgpu-shaders.manifest" }),
        .data = output.items,
    });
}

fn compile(init: std.process.Init, manifest_path: []const u8, shadercross: []const u8, output_dir: []const u8) !void {
    var arena = std.heap.ArenaAllocator.init(init.gpa);
    defer arena.deinit();
    const allocator = arena.allocator();
    const manifest_text = try std.Io.Dir.cwd().readFileAlloc(init.io, manifest_path, allocator, .limited(1024 * 1024));
    const shader_dir = std.fs.path.dirname(manifest_path) orelse ".";
    const records = try parseManifest(init.io, allocator, manifest_text, shader_dir);
    try std.Io.Dir.cwd().deleteTree(init.io, output_dir);
    try std.Io.Dir.cwd().createDirPath(init.io, output_dir);

    const compiled = try allocator.alloc(CompiledRecord, records.len);
    for (records, 0..) |record, index| {
        const blob_path = try std.fmt.allocPrint(allocator, "{s}.{s}.dxil", .{ record.effect, stageName(record.stage) });
        const source_path = try std.fs.path.join(allocator, &.{ shader_dir, record.source });
        const output_path = try std.fs.path.join(allocator, &.{ output_dir, blob_path });
        var command: std.ArrayListUnmanaged([]const u8) = .empty;
        try command.appendSlice(allocator, &.{ shadercross, source_path, "-s", "HLSL", "-d", "DXIL", "-t", stageName(record.stage), "-e", record.entry, "-I", shader_dir, "-o", output_path });
        for (record.defines) |define| {
            const value = define.value orelse "1";
            try command.append(allocator, try std.fmt.allocPrint(allocator, "-D{s}={s}", .{ define.name, value }));
        }
        std.debug.print("shadercross", .{});
        for (command.items) |argument| std.debug.print(" {s}", .{argument});
        std.debug.print("\n", .{});
        const result = try std.process.run(allocator, init.io, .{ .argv = command.items });
        if (result.term != .exited or result.term.exited != 0) {
            std.debug.print("shadercross failed: {s}\n{s}", .{ record.effect, result.stderr });
            return error.ShaderCompilationFailed;
        }
        const blob = try std.Io.Dir.cwd().readFileAlloc(init.io, output_path, allocator, .limited(64 * 1024 * 1024));
        var hash: [32]u8 = undefined;
        std.crypto.hash.sha2.Sha256.hash(blob, &hash, .{});
        compiled[index] = .{ .source = record, .blob_path = blob_path, .byte_length = @intCast(blob.len), .hash = hash };
        std.debug.print("generated {s} bytes={d}\n", .{ blob_path, blob.len });
    }
    try writeRuntimeManifest(init.io, allocator, output_dir, compiled);
}

pub fn main(init: std.process.Init) !void {
    var args = try std.process.Args.Iterator.initAllocator(init.minimal.args, init.gpa);
    defer args.deinit();
    _ = args.skip();
    const manifest = args.next() orelse return error.MissingManifestPath;
    const shadercross = args.next() orelse return error.MissingShadercrossPath;
    const output = args.next() orelse return error.MissingOutputPath;
    try compile(init, manifest, shadercross, output);
}

test "valid probe manifest parses and sorts stages" {
    const json_text =
        \\[
        \\  {"effect":"probe","name":"Probe","stage":"fragment","source":"probe.hlsl","entry":"PSMain","required_vertex_mask":0,"sampler_count":0,"storage_texture_count":0,"storage_buffer_count":0,"uniform_buffer_count":0},
        \\  {"effect":"probe","name":"Probe","stage":"vertex","source":"probe.hlsl","entry":"VSMain","required_vertex_mask":3,"sampler_count":0,"storage_texture_count":0,"storage_buffer_count":0,"uniform_buffer_count":0}
        \\]
    ;
    const records = try parseManifest(std.testing.io, std.testing.allocator, json_text, "Sources/src/GFXGPU/shaders");
    defer freeRecords(std.testing.allocator, records);
    try std.testing.expectEqual(Stage.vertex, records[0].stage);
    try std.testing.expectEqual(Stage.fragment, records[1].stage);
}

test "manifest rejects duplicate effect and stage" {
    const json_text = "[{\"effect\":\"x\",\"name\":\"a\",\"stage\":\"vertex\",\"source\":\"probe.hlsl\",\"entry\":\"VSMain\",\"required_vertex_mask\":0,\"sampler_count\":0,\"storage_texture_count\":0,\"storage_buffer_count\":0,\"uniform_buffer_count\":0},{\"effect\":\"x\",\"name\":\"b\",\"stage\":\"vertex\",\"source\":\"probe.hlsl\",\"entry\":\"VSMain\",\"required_vertex_mask\":0,\"sampler_count\":0,\"storage_texture_count\":0,\"storage_buffer_count\":0,\"uniform_buffer_count\":0}]";
    try std.testing.expectError(error.DuplicateEffectStage, parseManifest(std.testing.io, std.testing.allocator, json_text, "Sources/src/GFXGPU/shaders"));
}

test "manifest rejects unknown stage" {
    const json_text = "[{\"effect\":\"x\",\"name\":\"a\",\"stage\":\"geometry\",\"source\":\"probe.hlsl\",\"entry\":\"main\",\"required_vertex_mask\":0,\"sampler_count\":0,\"storage_texture_count\":0,\"storage_buffer_count\":0,\"uniform_buffer_count\":0}]";
    try std.testing.expectError(error.UnknownStage, parseManifest(std.testing.io, std.testing.allocator, json_text, "Sources/src/GFXGPU/shaders"));
}

test "manifest rejects missing source" {
    const json_text = "[{\"effect\":\"x\",\"name\":\"a\",\"stage\":\"vertex\",\"source\":\"missing.hlsl\",\"entry\":\"main\",\"required_vertex_mask\":0,\"sampler_count\":0,\"storage_texture_count\":0,\"storage_buffer_count\":0,\"uniform_buffer_count\":0}]";
    try std.testing.expectError(error.MissingSource, parseManifest(std.testing.io, std.testing.allocator, json_text, "Sources/src/GFXGPU/shaders"));
}

test "manifest rejects absolute and traversal source paths" {
    const absolute = "[{\"effect\":\"x\",\"name\":\"a\",\"stage\":\"vertex\",\"source\":\"C:/probe.hlsl\",\"entry\":\"main\",\"required_vertex_mask\":0,\"sampler_count\":0,\"storage_texture_count\":0,\"storage_buffer_count\":0,\"uniform_buffer_count\":0}]";
    try std.testing.expectError(error.AbsoluteSourcePath, parseManifest(std.testing.io, std.testing.allocator, absolute, "Sources/src/GFXGPU/shaders"));
    const traversal = "[{\"effect\":\"x\",\"name\":\"a\",\"stage\":\"vertex\",\"source\":\"../probe.hlsl\",\"entry\":\"main\",\"required_vertex_mask\":0,\"sampler_count\":0,\"storage_texture_count\":0,\"storage_buffer_count\":0,\"uniform_buffer_count\":0}]";
    try std.testing.expectError(error.SourcePathTraversal, parseManifest(std.testing.io, std.testing.allocator, traversal, "Sources/src/GFXGPU/shaders"));
}

test "manifest rejects duplicate defines and missing entry point" {
    const duplicate = "[{\"effect\":\"x\",\"name\":\"a\",\"stage\":\"vertex\",\"source\":\"probe.hlsl\",\"entry\":\"main\",\"defines\":[{\"name\":\"A\"},{\"name\":\"A\"}],\"required_vertex_mask\":0,\"sampler_count\":0,\"storage_texture_count\":0,\"storage_buffer_count\":0,\"uniform_buffer_count\":0}]";
    try std.testing.expectError(error.DuplicateDefine, parseManifest(std.testing.io, std.testing.allocator, duplicate, "Sources/src/GFXGPU/shaders"));
    const missing_entry = "[{\"effect\":\"x\",\"name\":\"a\",\"stage\":\"vertex\",\"source\":\"probe.hlsl\",\"required_vertex_mask\":0,\"sampler_count\":0,\"storage_texture_count\":0,\"storage_buffer_count\":0,\"uniform_buffer_count\":0}]";
    try std.testing.expectError(error.MissingEntryPoint, parseManifest(std.testing.io, std.testing.allocator, missing_entry, "Sources/src/GFXGPU/shaders"));
}
