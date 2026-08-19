const std = @import("std");

/// A graphics pipeline that fails to build fails again on every frame, so the
/// reason is reported once rather than once per draw.
var reported_pipeline_failure: bool = false;
const device_mod = @import("device.zig");
const frame_mod = @import("frame.zig");
const sdl = @import("sdl.zig");
const manifest = @import("shader_manifest.zig");
const effects = @import("effects.zig");
const formats = @import("formats.zig");
const vertex_layout = @import("vertex_layout.zig");

const io_c = @cImport({
    @cInclude("stdio.h");
});

// One shader slot per Renderer.ShaderVariant. Declared here so the caches below
// cannot fall behind the enum.
const shader_variant_count = @typeInfo(Renderer.ShaderVariant).@"enum".fields.len;

pub const Renderer = struct {
    allocator: std.mem.Allocator,
    device: ?device_mod.Device = null,
    frame: frame_mod.Frame = .{},
    resources: std.AutoHashMapUnmanaged(u64, void) = .empty,
    textures: std.AutoHashMapUnmanaged(u64, TextureResource) = .empty,
    buffers: std.AutoHashMapUnmanaged(u64, BufferResource) = .empty,
    temporary_buffers: std.ArrayListUnmanaged(u64) = .empty,
    // Pooled GPU buffers for the immediate-mode temporary draws. Allocating and
    // freeing a vertex buffer and a transfer buffer for every UI rectangle,
    // sprite and text string ran the menu at 12 fps; reusing them across frames
    // keeps the allocation out of the hot path. A buffer used this frame returns
    // to the free list at present and is handed out again next frame; the copy
    // that fills it cycles so overwriting one the GPU has not finished reading
    // does not stall.
    free_temp_vertex: std.ArrayListUnmanaged(TempBuffer) = .empty,
    in_use_temp_vertex: std.ArrayListUnmanaged(TempBuffer) = .empty,
    free_temp_index: std.ArrayListUnmanaged(TempBuffer) = .empty,
    in_use_temp_index: std.ArrayListUnmanaged(TempBuffer) = .empty,
    free_temp_transfer: std.ArrayListUnmanaged(TempTransfer) = .empty,
    in_use_temp_transfer: std.ArrayListUnmanaged(TempTransfer) = .empty,
    // The per-frame arena those pools were replaced by, and the switch that
    // selects it. The pools above are exactly-sized and never shrink: the menu
    // held 26 vertex buffers, 16 index buffers and 42 transfer buffers live for
    // 21 KB of geometry, and did 42 map/unmap pairs a frame to fill them.
    // BK_GPU_ARENA=0 goes back to that path, unchanged, so a rendering
    // regression bisects on one variable.
    arena_enabled: bool = true,
    arena: TempArena = .{},
    // BK_PERF measurement only - nothing here feeds a decision. The draw and
    // byte counters are free-running totals; the six counters below them add one
    // sample per frame. Both are read once a second through get_live_counts,
    // which subtracts its previous sample and divides by the frames it saw, so
    // they are never reset and wrap harmlessly.
    //
    // The six carry the pool sizes on the BK_GPU_ARENA=0 path and the arena's
    // numbers on the default one - bytes the frame asked of each arena, the
    // capacity that served it, and the draws that had to fall back. Same fields,
    // because the C header they cross is shared; releaseTemporaryBuffers is
    // where the two meanings are set.
    perf_temp_draws: u32 = 0,
    perf_temp_bytes: u32 = 0,
    perf_temp_vertex_free: u32 = 0,
    perf_temp_vertex_in_use: u32 = 0,
    perf_temp_index_free: u32 = 0,
    perf_temp_index_in_use: u32 = 0,
    perf_temp_transfer_free: u32 = 0,
    perf_temp_transfer_in_use: u32 = 0,
    // One command buffer per frame collects every temporary upload copy. It is
    // submitted once, before the frame's draw command buffer, rather than a
    // command buffer submitted per draw: a D3D12 queue submission costs enough
    // that hundreds a frame - one per UI rectangle - was the whole frame budget.
    frame_upload_command: ?*sdl.GpuCommandBuffer = null,
    next_resource_handle: u64 = 1,
    window: ?*anyopaque = null,
    window_claimed: bool = false,
    swapchain_format: u32 = 0,
    // The swapchain (window) size. The scene may legitimately differ: an
    // explicit fullscreen resolution renders at scene_* and is presented
    // centered on the drawable - black borders when smaller, cropped when
    // larger, never scaled.
    drawable_width: u32 = 0,
    drawable_height: u32 = 0,
    scene_width: u32 = 0,
    scene_height: u32 = 0,
    // true: aspect-fit scale (menus/videos - nothing may be clipped away);
    // false: centered 1:1 with borders/crop (gameplay - exact pixels win).
    present_fit: bool = false,
    // How finished frames reach the display (GFX.Present.Mode). Defaults to
    // vsync: mailbox is opt-in because it lets the GPU run unthrottled.
    present_mode: sdl.PresentMode = .vsync,
    scene_texture: ?*sdl.GpuTexture = null,
    scene_depth: ?*sdl.GpuTexture = null,
    shader_directory: ?[]u8 = null,
    // Pipelines are derived from the FVF of the vertex buffer being drawn and
    // cached on (fvf, textured, blend). Hardcoding one attribute set per
    // pipeline only ever described SGFXLVertex and SGFXTLVertex correctly; every
    // other format the engine draws with -- terrain tiles, mesh vertices, line
    // vertices -- was strided and swizzled as garbage.
    pipelines: std.AutoHashMapUnmanaged(u64, *anyopaque) = .empty,
    // Sized from the variant enum: these are indexed by @intFromEnum, so a fixed
    // count silently goes out of bounds the moment a variant is added, and a
    // release build has no check to catch it.
    vertex_shaders: [shader_variant_count]?*anyopaque = @splat(null),
    fragment_shaders: [shader_variant_count]?*anyopaque = @splat(null),
    shade_effect: u32 = 0,
    // The blend in force, carried across effects that do not write one. See
    // effects.blendChangeFor: 22 only turns Z writes off, so it must not undo
    // the additive blend 16 set for the flash sprites just before it.
    blend_mode: effects.BlendMode = .replace,
    sampler: ?*sdl.c.SDL_GPUSampler = null,
    linear_sampler: ?*sdl.c.SDL_GPUSampler = null,
    use_linear_sampler: bool = false,
    // Stage 0 is the tileset or sprite; stage 1 is the terrain's noise or the
    // crosset whose alpha masks a tile transition.
    bound_textures: [2]?u64 = .{ null, null },
    bound_vertex_buffer: ?u64 = null,
    viewport: ?ViewportState = null,
    world_matrix: [16]f32 = .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },
    view_proj_matrix: [16]f32 = .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },
    draw_color: [4]f32 = .{ 1, 1, 1, 1 },
    // Sticky, because CGraphicsEngine::SetShadingEffect applies an effect as a
    // delta: 110 opens the shadow pass and 113 closes it, and the effects that
    // draw in between inherit what those set.
    stencil_mode: effects.StencilMode = .off,
    specular_enabled: bool = false,
    // With D3D's fixed-function lighting on, the vertex colour comes from the
    // material, not from the vertex: the mesh formats carry a normal where the
    // lit formats keep their diffuse DWORD and so have no colour of their own.
    // The shadow passes depend on that -- CScene::Draw sets an all-black
    // material whose alpha is MESH_SHADOW_DENSITY and draws untextured meshes
    // through it -- so discarding the material turned every mesh shadow into an
    // opaque white silhouette on the ground.
    lighting_enabled: bool = false,
    material_diffuse: [4]f32 = .{ 1, 1, 1, 1 },
    // What the geometry of the next draw actually is. Every pipeline used to be
    // built as a triangle list and every primitive count multiplied by three, so
    // a line list -- the selection rectangle, UI borders, gun traces, minimap
    // markers -- was rasterised as triangles over three times as many vertices
    // as the buffer held, reading past its end and painting the garbage as a
    // solid wedge across the screen.
    topology: formats.Topology = .triangle_list,
    // The stage-0 texture matrix. CTerrainWater::DrawWater scrolls each river
    // layer by translating u through it, so with the transform discarded the
    // water was motionless. Only effects that enable D3DTTFF_COUNT2 see it;
    // every other draw gets the identity.
    texture_matrix: [16]f32 = .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 },
    // EGFXDepthBuffer and its compare function. The pass carried no
    // depth-stencil attachment at all and every pipeline declared it had none,
    // so meshes had no hidden-surface removal: a truck's far wheels and the
    // opposite wall of its cargo bed drew straight through the near side,
    // whichever triangle happened to come last.
    depth_mode: u32 = 0,
    depth_compare: u32 = 0,
    last_error: []const u8 = "",

    pub const ViewportState = struct { x: f32, y: f32, width: f32, height: f32, min_depth: f32, max_depth: f32 };

    pub const LiveCounts = struct {
        textures: u32 = 0,
        buffers: u32 = 0,
        samplers: u32 = 0,
        render_targets: u32 = 0,
        shaders: u32 = 0,
        pipelines: u32 = 0,
        passes: u32 = 0,
    };

    pub const BufferResource = struct {
        gpu: *sdl.GpuBuffer,
        size: u32,
        stride: u32,
        // The FVF the buffer was created with, so a draw picks its pipeline from
        // the geometry it is actually drawing rather than from whichever buffer
        // happened to be created most recently.
        format: u32,
    };

    // A pooled temporary buffer and its allocated capacity. Capacity is a power
    // of two so buffers of similar size share a bucket and get reused.
    const TempBuffer = struct { gpu: *sdl.GpuBuffer, capacity: u32 };
    const TempTransfer = struct { transfer: *sdl.GpuTransferBuffer, capacity: u32 };

    // Every arena offset, on the transfer side and the GPU side alike, is a
    // multiple of this. SDL validates no alignment at all for a buffer copy, but
    // the backends underneath do: Metal's blit encoder wants four bytes on
    // macOS, D3D12 and Vulkan are happier with more. Sixteen costs a few bytes
    // of padding per draw against a 64 KB arena, which is not worth measuring.
    const arena_copy_alignment: u32 = 16;

    // Where a suballocation may start. SDL_GPUBufferBinding.offset is where
    // element zero of the draw lives, so the offset has to be a whole number of
    // elements as well as a legal copy offset, and the smallest step that is
    // both is their least common multiple. The strides here are 20..40 bytes, so
    // this lands on 80..160.
    fn arenaAlignmentFor(element_size: u32) u32 {
        if (element_size == 0 or element_size > 1 << 20) return arena_copy_alignment;
        const divisor = std.math.gcd(element_size, arena_copy_alignment);
        return (element_size / divisor) * arena_copy_alignment;
    }

    fn alignUpSaturating(value: u32, alignment: u32) u32 {
        if (alignment <= 1) return value;
        const remainder = value % alignment;
        if (remainder == 0) return value;
        return value +| (alignment - remainder);
    }

    // The bump allocator behind one arena buffer. It deliberately holds no GPU
    // handle: everything an arena gets wrong is arithmetic - a misaligned bind,
    // a range that runs off the end, a capacity that never comes back down - and
    // none of that should need a device to test.
    pub const ArenaCursor = struct {
        // The size of the GPU buffer this hands out ranges of. Zero until that
        // buffer exists, which makes every draw fall back, which is correct.
        capacity: u32 = 0,
        // The size the buffer should be recreated at before the next frame.
        pending: u32 = 0,
        cursor: u32 = 0,
        // What the frame asked for, aligned the same way, counting the requests
        // that did not fit. A frame that overflowed and fell back to per-draw
        // buffers grows the arena to cover it rather than falling back forever.
        wanted: u32 = 0,
        // Sliding maximum of `wanted` over two buckets, the window under way and
        // the one before it. Shrinking off a single frame's low sample would let
        // a screen that redraws unevenly thrash the arena between two sizes.
        window_peak: u32 = 0,
        previous_peak: u32 = 0,
        window_frames: u32 = 0,
        minimum: u32 = 64 * 1024,
        maximum: u32 = 64 * 1024 * 1024,
        // How long the arena may sit above what it needs. 240 frames is a few
        // seconds - long enough that opening a dialog and closing it again does
        // not resize the arena twice.
        review_frames: u32 = 240,

        pub fn beginFrame(self: *ArenaCursor) void {
            self.cursor = 0;
            self.wanted = 0;
        }

        // A range of `length` bytes starting at a multiple of `alignment`, or
        // null when it does not fit - the caller then draws through a per-draw
        // buffer, and `wanted` has recorded the size either way.
        pub fn alloc(self: *ArenaCursor, length: u32, alignment: u32) ?u32 {
            if (length == 0) return null;
            // `wanted` is a shadow cursor that advances even when the real one
            // cannot, so it says how large the arena would have had to be to
            // serve the whole frame.
            self.wanted = alignUpSaturating(self.wanted, alignment) +| length;
            const start = alignUpSaturating(self.cursor, alignment);
            const end = start +| length;
            if (end > self.capacity) return null;
            self.cursor = end;
            return start;
        }

        // Decide the capacity for the next frame. Growth is immediate, because a
        // frame that overflowed has already paid for it; shrinking waits out a
        // whole review window so it cannot chase a dip.
        pub fn endFrame(self: *ArenaCursor) void {
            self.window_peak = @max(self.window_peak, self.wanted);
            self.window_frames +|= 1;
            if (self.wanted > self.capacity) {
                self.pending = capacityFor(self.wanted, self.minimum, self.maximum);
                self.previous_peak = self.window_peak;
                self.window_peak = self.wanted;
                self.window_frames = 0;
                return;
            }
            if (self.window_frames < self.review_frames) return;
            const peak = @max(self.window_peak, self.previous_peak);
            const target = capacityFor(peak, self.minimum, self.maximum);
            if (target < self.capacity) self.pending = target;
            self.previous_peak = self.window_peak;
            self.window_peak = self.wanted;
            self.window_frames = 0;
        }

        // The smallest power-of-two multiple of `minimum` that covers `need`.
        fn capacityFor(need: u32, minimum: u32, maximum: u32) u32 {
            var value: u32 = if (minimum == 0) 4096 else minimum;
            while (value < need) {
                if (value >= maximum) return maximum;
                value = if (value > maximum / 2) maximum else value * 2;
            }
            return value;
        }
    };

    // One arena per frame: a vertex buffer, an index buffer and a transfer
    // buffer that every immediate-mode draw takes a range out of, instead of a
    // GPU buffer and a transfer buffer per draw.
    //
    // Rotation. The previous frame's draws may still be reading these buffers,
    // so the frame's first copy into each one, and the frame's single map of the
    // transfer buffer, ask SDL to cycle. SDL swaps in a free backing exactly
    // when the current one is still referenced by an in-flight command buffer
    // and reuses it when it is not (METAL/D3D12/VULKAN_INTERNAL_PrepareBufferFor
    // Write, all three gated on activeBuffer->referenceCount), so the ring is
    // there and is sized to the frames actually in flight rather than to a
    // guess. That is the same mechanism the pooled path's uploadBufferCycle(...,
    // true) relied on. Only the frame's FIRST copy into a buffer may cycle: a
    // later one would strand the ranges already written into the old backing.
    const TempArena = struct {
        vertex_gpu: ?*sdl.GpuBuffer = null,
        index_gpu: ?*sdl.GpuBuffer = null,
        transfer: ?*sdl.GpuTransferBuffer = null,
        // Mapped once per frame and unmapped once, before the upload command
        // buffer is submitted.
        mapped: ?[*]u8 = null,
        copy_pass: ?*sdl.GpuCopyPass = null,
        vertex: ArenaCursor = .{ .minimum = 64 * 1024, .pending = 64 * 1024 },
        index: ArenaCursor = .{ .minimum = 16 * 1024, .pending = 16 * 1024 },
        // The transfer buffer carries the vertex and the index bytes both, so it
        // is sized to their sum.
        staging: ArenaCursor = .{ .minimum = 64 * 1024, .pending = 64 * 1024 },
        // Cleared by the frame's first copy into the buffer each one names.
        cycle_vertex: bool = true,
        cycle_index: bool = true,
        // Draws this frame that did not fit and went through a per-draw buffer.
        fallbacks: u32 = 0,
        // Between the first temporary draw of a frame and present or cancel.
        open: bool = false,
    };

    const ArenaTarget = enum { vertex, index };
    const ArenaRange = struct { destination: u32, source: u32 };

    // Which vertex shader a draw needs. The two "nocolor" entry points exist for
    // vertex formats that carry no GFXFVF_DIFFUSE.
    pub const ShaderVariant = enum(u3) { untextured = 0, untextured_nocolor = 1, textured = 2, textured_nocolor = 3, textured_dual = 4, untextured_specular = 5, textured_specular = 6 };

    fn variantEffect(variant: ShaderVariant) []const u8 {
        return switch (variant) {
            .untextured => "untextured",
            .untextured_nocolor => "untextured_nocolor",
            .textured => "textured",
            .textured_nocolor => "textured_nocolor",
            .textured_dual => "textured_dual",
            .untextured_specular => "untextured_specular",
            .textured_specular => "textured_specular",
        };
    }
    fn variantVertexEntry(variant: ShaderVariant) [:0]const u8 {
        return switch (variant) {
            .untextured => "vs_untextured",
            .untextured_nocolor => "vs_untextured_nocolor",
            .textured => "vs_textured",
            .textured_nocolor => "vs_textured_nocolor",
            .textured_dual => "vs_textured_dual",
            .untextured_specular => "vs_untextured_specular",
            .textured_specular => "vs_textured_specular",
        };
    }
    // The nocolor variants reuse the base fragment entry point, so the blob
    // compiled for them exposes that name.
    fn variantFragmentEntry(variant: ShaderVariant) [:0]const u8 {
        return switch (variant) {
            .untextured, .untextured_nocolor => "ps_untextured",
            .textured, .textured_nocolor => "ps_textured",
            .textured_dual => "ps_textured_dual",
            .untextured_specular => "ps_untextured_specular",
            .textured_specular => "ps_textured_specular",
        };
    }
    fn variantSamplerCount(variant: ShaderVariant) u32 {
        return switch (variant) {
            .untextured, .untextured_nocolor, .untextured_specular => 0,
            .textured, .textured_nocolor, .textured_specular => 1,
            .textured_dual => 2,
        };
    }

    // SGFXTLVertex: XYZRHW | DIFFUSE | SPECULAR | TEX1. Used to warm the shader
    // path before a swapchain texture is acquired, and as the layout for draws
    // whose buffer carries no FVF.
    const default_fvf: u32 = 0x004 | 0x040 | 0x080 | 0x100;
    pub const TextureResource = struct { gpu: *sdl.GpuTexture, width: u32, height: u32 };

    const MatrixUniforms = extern struct { matrix: [16]f32, padding: [4]f32 };
    // `screen` is g_screen: (pre_transformed, 1/width, 1/height, 0).
    // stage.x is the stage-0 colour op the fragment shader applies; see
    // effects.colorOpFor.
    const DrawUniforms = extern struct { matrix: [16]f32, color: [4]f32, screen: [4]f32, texture_matrix: [16]f32, stage: [4]f32 };
    const identity_matrix: [16]f32 = .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

    pub fn init(allocator: std.mem.Allocator) Renderer {
        var renderer = Renderer{ .allocator = allocator, .arena_enabled = arenaFlagEnabled() };
        if (arenaSizeOverride()) |bytes| {
            inline for (.{ &renderer.arena.vertex, &renderer.arena.index, &renderer.arena.staging }) |cursor| {
                cursor.minimum = bytes;
                cursor.maximum = bytes;
                cursor.pending = bytes;
            }
        }
        return renderer;
    }

    // BK_GPU_ARENA=0 selects the pooled per-draw path. Anything else - unset
    // included - selects the arena.
    fn arenaFlagEnabled() bool {
        const value = sdl.getEnv("BK_GPU_ARENA") orelse return true;
        return !(value.len == 1 and value[0] == '0');
    }

    // BK_GPU_ARENA_KB pins every arena to one size, minimum and maximum alike,
    // so it can neither grow nor shrink. Only testing wants that: pinning it
    // small is the one way to make the overflow fallback - the path a frame that
    // fits never reaches - run on every draw, and it also parks the growth logic
    // against its ceiling.
    fn arenaSizeOverride() ?u32 {
        const value = sdl.getEnv("BK_GPU_ARENA_KB") orelse return null;
        const kilobytes = std.fmt.parseInt(u32, value, 10) catch return null;
        if (kilobytes == 0 or kilobytes > 64 * 1024) return null;
        return kilobytes * 1024;
    }

    pub fn deinit(self: *Renderer) void {
        self.cancelFrame();
        if (self.window_claimed) {
            if (self.device) |device| device.api.release_window(device.handle.?, self.window.?);
            self.window_claimed = false;
        }
        self.resources.deinit(self.allocator);
        if (self.device) |device| {
            const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
            var texture_iterator = self.textures.valueIterator();
            while (texture_iterator.next()) |texture| sdl.releaseTexture(gpu_device, texture.gpu);
            if (self.scene_texture) |texture| sdl.releaseTexture(gpu_device, texture);
            if (self.scene_depth) |texture| sdl.releaseTexture(gpu_device, texture);
            var pipeline_iterator = self.pipelines.valueIterator();
            while (pipeline_iterator.next()) |pipeline| sdl.c.SDL_ReleaseGPUGraphicsPipeline(gpu_device, @ptrCast(@alignCast(pipeline.*)));
            for (self.fragment_shaders) |shader| {
                if (shader) |handle| sdl.c.SDL_ReleaseGPUShader(gpu_device, @ptrCast(@alignCast(handle)));
            }
            for (self.vertex_shaders) |shader| {
                if (shader) |handle| sdl.c.SDL_ReleaseGPUShader(gpu_device, @ptrCast(@alignCast(handle)));
            }
            if (self.sampler) |sampler| sdl.c.SDL_ReleaseGPUSampler(gpu_device, sampler);
            if (self.linear_sampler) |sampler| sdl.c.SDL_ReleaseGPUSampler(gpu_device, sampler);
            for (self.free_temp_vertex.items) |b| sdl.releaseBuffer(gpu_device, b.gpu);
            for (self.in_use_temp_vertex.items) |b| sdl.releaseBuffer(gpu_device, b.gpu);
            for (self.free_temp_index.items) |b| sdl.releaseBuffer(gpu_device, b.gpu);
            for (self.in_use_temp_index.items) |b| sdl.releaseBuffer(gpu_device, b.gpu);
            for (self.free_temp_transfer.items) |t| sdl.releaseTransferBuffer(gpu_device, t.transfer);
            for (self.in_use_temp_transfer.items) |t| sdl.releaseTransferBuffer(gpu_device, t.transfer);
            // cancelFrame above has already ended the copy pass and unmapped the
            // transfer buffer, so these are safe to release here.
            if (self.arena.vertex_gpu) |buffer| sdl.releaseBuffer(gpu_device, buffer);
            if (self.arena.index_gpu) |buffer| sdl.releaseBuffer(gpu_device, buffer);
            if (self.arena.transfer) |transfer| sdl.releaseTransferBuffer(gpu_device, transfer);
        }
        self.free_temp_vertex.deinit(self.allocator);
        self.in_use_temp_vertex.deinit(self.allocator);
        self.free_temp_index.deinit(self.allocator);
        self.in_use_temp_index.deinit(self.allocator);
        self.free_temp_transfer.deinit(self.allocator);
        self.in_use_temp_transfer.deinit(self.allocator);
        self.pipelines.deinit(self.allocator);
        if (self.shader_directory) |directory| self.allocator.free(directory);
        if (self.device) |device| {
            var iterator = self.buffers.valueIterator();
            while (iterator.next()) |buffer| sdl.releaseBuffer(@ptrCast(@alignCast(device.handle.?)), buffer.gpu);
        }
        self.buffers.deinit(self.allocator);
        self.textures.deinit(self.allocator);
        self.temporary_buffers.deinit(self.allocator);
        if (self.device) |*device| device.deinit();
        self.* = undefined;
    }

    pub fn attachWindow(self: *Renderer, window: ?*anyopaque, width: u32, height: u32) !void {
        const device = &(self.device orelse return error.NoDevice);
        const window_ptr = window orelse return error.NullWindow;
        if (!device.api.claim_window(device.handle.?, window_ptr)) return error.ClaimFailed;
        errdefer device.api.release_window(device.handle.?, window_ptr);
        if (!device.api.configure_swapchain(device.handle.?, window_ptr, self.present_mode)) return error.SwapchainConfigurationFailed;
        self.window = window_ptr;
        self.window_claimed = true;
        self.swapchain_format = device.api.swapchain_format(device.handle.?, window_ptr);
        self.drawable_width = width;
        self.drawable_height = height;
        self.scene_width = width;
        self.scene_height = height;
        self.scene_texture = sdl.createColorTexture(@ptrCast(@alignCast(device.handle.?)), @intCast(self.swapchain_format), width, height) orelse return error.SceneTextureCreateFailed;
        self.scene_depth = sdl.createDepthTexture(@ptrCast(@alignCast(device.handle.?)), width, height) orelse return error.DepthTextureCreateFailed;
    }

    // Re-runs the swapchain configure so the option applies without a
    // restart, the way GFX.Present.Fit does. Called every frame from the
    // C++ side, so an unchanged mode must cost nothing.
    //
    // Only ever between frames. SDL_ConfigureGPUSwapchain recreates the
    // swapchain on the D3D12 and Vulkan backends, and an acquired swapchain
    // texture belongs to the command buffer that acquired it until that
    // buffer is submitted - so reconfiguring inside a frame pulls the target
    // out from under the frame in flight. The state check makes that a
    // refusal rather than a corrupted first frame; the caller applies the
    // mode after the present.
    pub fn setPresentMode(self: *Renderer, mode: sdl.PresentMode) !void {
        if (self.present_mode == mode) return;
        if (self.frame.state != .idle) return error.InvalidState;
        // With no device or window yet there is nothing to reconfigure and
        // nothing that can fail: the mode is remembered for attachWindow.
        const device = &(self.device orelse {
            self.present_mode = mode;
            return;
        });
        const window_ptr = self.window orelse {
            self.present_mode = mode;
            return;
        };
        if (!device.api.configure_swapchain(device.handle.?, window_ptr, mode)) return error.SwapchainConfigurationFailed;
        // Recorded only once the device has taken it, so a rejected mode is
        // retried next frame instead of being remembered as the live one.
        self.present_mode = mode;
    }

    pub fn setShaderDirectory(self: *Renderer, directory: ?[*:0]const u8) !void {
        if (directory) |value| {
            const bytes = std.mem.span(value);
            self.shader_directory = try self.allocator.dupe(u8, bytes);
        }
    }

    pub fn resize(self: *Renderer, width: u32, height: u32) !void {
        if (width == 0 or height == 0) return error.InvalidViewport;
        const device = &(self.device orelse return error.NoDevice);
        if (self.frame.state != .idle) return error.InvalidState;
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        if (self.scene_texture) |texture| sdl.releaseTexture(gpu_device, texture);
        if (self.scene_depth) |texture| sdl.releaseTexture(gpu_device, texture);
        self.scene_texture = null;
        self.scene_depth = null;
        self.scene_texture = sdl.createColorTexture(gpu_device, @intCast(self.swapchain_format), width, height) orelse return error.SceneTextureCreateFailed;
        self.scene_depth = sdl.createDepthTexture(gpu_device, width, height) orelse return error.DepthTextureCreateFailed;
        // Only the scene follows the requested mode; the drawable keeps
        // tracking the swapchain (refreshed on every acquire), which is what
        // makes a fixed fullscreen resolution possible at all.
        self.scene_width = width;
        self.scene_height = height;
    }

    pub fn beginFrame(self: *Renderer) !bool {
        const device = &(self.device orelse return error.NoDevice);
        const window = self.window orelse return error.NoWindow;
        if (self.frame.state != .idle) return error.InvalidState;
        // Validate/create a pipeline before acquiring a swapchain texture.
        // SDL_GPU cannot cancel a command buffer after acquisition, so a
        // pipeline failure must happen before the frame enters that state.
        _ = try self.ensurePipeline(default_fvf, false, .replace);
        const command = device.api.acquire_command_buffer(device.handle.?) orelse return error.CommandBufferFailed;
        var texture: ?*anyopaque = null;
        var width = self.drawable_width;
        var height = self.drawable_height;
        if (!device.api.wait_acquire_swapchain(command, window, &texture, &width, &height)) {
            _ = device.api.cancel_command_buffer(command);
            return error.SwapchainAcquireFailed;
        }
        if (texture == null) {
            _ = device.api.cancel_command_buffer(command);
            self.frame.skipped = true;
            return false;
        }
        self.drawable_width = width;
        self.drawable_height = height;
        try self.frame.begin(true);
        self.frame.command_buffer = command;
        self.frame.swapchain_texture = texture;
        return true;
    }

    pub fn endFrame(self: *Renderer) !void {
        if (self.frame.skipped) return;
        if (self.frame.render_pass) |pass| {
            const device = &(self.device orelse return error.NoDevice);
            device.api.end_render_pass(pass);
            self.frame.render_pass = null;
            self.frame.endPass() catch return error.InvalidState;
        }
        if (self.scene_texture) |scene| {
            const command = self.frame.command_buffer orelse return error.InvalidState;
            const swapchain = self.frame.swapchain_texture orelse return error.InvalidState;
            if (self.present_fit)
                sdl.blitTextureFit(@ptrCast(@alignCast(command)), scene, self.scene_width, self.scene_height, @ptrCast(@alignCast(swapchain)), self.drawable_width, self.drawable_height)
            else
                sdl.blitTextureCentered(@ptrCast(@alignCast(command)), scene, self.scene_width, self.scene_height, @ptrCast(@alignCast(swapchain)), self.drawable_width, self.drawable_height);
        }
        try self.frame.end();
    }

    pub fn clear(self: *Renderer, color: [4]f32) !void {
        if (self.frame.state != .recording) return error.InvalidState;
        const device = &(self.device orelse return error.NoDevice);
        const texture: *sdl.GpuTexture = if (self.scene_texture) |scene| scene else @ptrCast(@alignCast(self.frame.swapchain_texture orelse return error.InvalidState));
        const command = self.frame.command_buffer orelse return error.InvalidState;
        // Keep the reference path's render pass color-only until depth state is
        // wired through the legacy adapter.  Some D3D12 devices reject the
        // temporary D24S8 target during pass creation even though the texture
        // itself was created successfully.
        // The depth target is attached whenever one exists, for the windowed and
        // the offscreen reference path alike; pipelines declare a depth-stencil
        // target on exactly the same condition, so the two cannot disagree.
        const pass: ?*anyopaque = if (self.scene_depth) |depth|
            @ptrCast(sdl.beginColorDepthPass(@ptrCast(@alignCast(command)), @ptrCast(@alignCast(texture)), depth, color, sdl.c.SDL_GPU_LOADOP_CLEAR))
        else if (self.scene_texture != null)
            @ptrCast(sdl.beginColorPass(@ptrCast(@alignCast(command)), @ptrCast(@alignCast(texture)), color, sdl.c.SDL_GPU_LOADOP_CLEAR))
        else
            device.api.begin_clear_pass(command, @ptrCast(texture), color);
        const render_pass = pass orelse return error.RenderPassFailed;
        self.frame.beginPass() catch {
            if (self.scene_texture != null) {
                sdl.endRenderPass(@ptrCast(@alignCast(render_pass)));
            } else {
                device.api.end_render_pass(render_pass);
            }
            return error.InvalidState;
        };
        self.frame.render_pass = render_pass;
    }

    pub fn setViewport(self: *Renderer, viewport: ViewportState) !void {
        if (self.frame.state != .recording and self.frame.state != .pass_active) return error.InvalidState;
        if (viewport.width <= 0 or viewport.height <= 0 or viewport.min_depth < 0 or viewport.max_depth > 1 or viewport.min_depth > viewport.max_depth) return error.InvalidViewport;
        self.viewport = viewport;
        if (self.frame.render_pass) |pass| {
            sdl.setViewport(@ptrCast(@alignCast(pass)), viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth);
        }
    }

    pub fn present(self: *Renderer) !void {
        // The arena's copy pass has to be ended and its transfer buffer unmapped
        // before the command buffer holding the copies is submitted.
        self.closeArenaFrame();
        if (self.frame.skipped) {
            // A skipped frame still recorded its uploads. Submitting them rather
            // than carrying the command buffer into the next frame keeps a
            // minimised window from accumulating copy passes without bound; the
            // copies land in buffers nothing reads this frame.
            if (self.frame_upload_command) |upload| {
                _ = sdl.submitCommandBuffer(upload);
                self.frame_upload_command = null;
            }
            self.frame.cancel();
            return;
        }
        const device = &(self.device orelse return error.NoDevice);
        const command = self.frame.command_buffer orelse return error.InvalidState;
        // The uploads must land before the draws that read them, and SDL runs
        // command buffers in submission order, so the upload buffer goes first.
        if (self.frame_upload_command) |upload| {
            _ = sdl.submitCommandBuffer(upload);
            self.frame_upload_command = null;
        }
        if (!device.api.submit_command_buffer(command)) {
            self.frame.cancel();
            return error.SubmitFailed;
        }
        try self.frame.present();
        self.releaseTemporaryBuffers();
    }

    pub fn cancelFrame(self: *Renderer) void {
        // The arena's copy pass has to be closed and its transfer buffer
        // unmapped before the command buffer holding them goes away.
        self.closeArenaFrame();
        // The upload buffer never acquires a swapchain texture, so it is always
        // safe to cancel; its recorded copies are discarded with the frame.
        if (self.frame_upload_command) |upload| {
            _ = sdl.cancelCommandBuffer(upload);
            self.frame_upload_command = null;
        }
        if (self.frame.render_pass) |pass| {
            if (self.device) |device| device.api.end_render_pass(pass);
        }
        if (self.frame.command_buffer) |command| {
            if (self.device) |device| {
                // SDL forbids cancelling a command buffer after a swapchain
                // texture has been acquired. Close the pass and submit the
                // command buffer when that acquisition already happened.
                if (self.frame.swapchain_texture != null) {
                    _ = device.api.submit_command_buffer(command);
                } else {
                    _ = device.api.cancel_command_buffer(command);
                }
            }
        }
        self.frame.cancel();
    }

    pub fn createBuffer(self: *Renderer, element_count: u32, format: u32, stride: u32, usage_flags: u32) !u64 {
        const device = &(self.device orelse return error.NoDevice);
        const size = std.math.mul(u32, element_count, stride) catch return error.BufferTooLarge;
        const usage: sdl.c.SDL_GPUBufferUsageFlags = if ((usage_flags & 2) != 0 or format == 101 or format == 102)
            sdl.c.SDL_GPU_BUFFERUSAGE_INDEX
        else
            sdl.c.SDL_GPU_BUFFERUSAGE_VERTEX;
        const gpu = sdl.createBuffer(@ptrCast(@alignCast(device.handle.?)), usage, size) orelse return error.BufferCreateFailed;
        errdefer sdl.releaseBuffer(@ptrCast(@alignCast(device.handle.?)), gpu);
        const id = self.next_resource_handle;
        self.next_resource_handle += 1;
        try self.buffers.put(self.allocator, id, .{ .gpu = gpu, .size = size, .stride = stride, .format = format });
        return id;
    }

    pub fn uploadBuffer(self: *Renderer, id: u64, data: *const anyopaque, byte_length: u32, byte_offset: u32) !void {
        const resource = self.buffers.get(id) orelse return error.InvalidBuffer;
        if (byte_length == 0 or byte_offset > resource.size or byte_length > resource.size - byte_offset) return error.BufferUploadOutOfBounds;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        const transfer = sdl.createUploadBuffer(gpu_device, byte_length) orelse return error.TransferBufferCreateFailed;
        defer sdl.releaseTransferBuffer(gpu_device, transfer);
        const mapped = sdl.mapTransferBuffer(gpu_device, transfer) orelse return error.TransferBufferMapFailed;
        @memcpy(@as([*]u8, @ptrCast(mapped))[0..byte_length], @as([*]const u8, @ptrCast(data))[0..byte_length]);
        sdl.unmapTransferBuffer(gpu_device, transfer);
        const command = sdl.acquireCommandBuffer(gpu_device) orelse return error.CommandBufferFailed;
        if (!sdl.uploadBuffer(gpu_device, command, transfer, resource.gpu, byte_offset, byte_length)) {
            _ = sdl.cancelCommandBuffer(command);
            return error.CopyPassFailed;
        }
        // Submit and move on. A full SDL_WaitForGPUIdle here stalled the CPU on
        // the whole GPU after every upload, and drawTemporary uploads a fresh
        // vertex buffer for each UI rectangle, sprite and text string - hundreds
        // of pipeline flushes a frame, which showed up as a stuttering cursor
        // and a barely-moving interface. SDL_GPU runs command buffers in
        // submission order on one queue, so this upload, submitted before the
        // frame's draw command buffer, is guaranteed to complete before the draw
        // that reads the buffer, and the transfer buffer's release is deferred
        // by SDL until the copy actually finishes.
        if (!sdl.submitCommandBuffer(command)) return error.SubmitFailed;
    }

    pub fn destroyBuffer(self: *Renderer, id: u64) !void {
        const resource = self.buffers.fetchRemove(id) orelse return error.InvalidBuffer;
        const device = &(self.device orelse return error.NoDevice);
        sdl.releaseBuffer(@ptrCast(@alignCast(device.handle.?)), resource.value.gpu);
    }

    fn textureFormat(format: u32) ?sdl.c.SDL_GPUTextureFormat {
        return switch (format) {
            6 => sdl.c.SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
            32 => sdl.c.SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            else => null,
        };
    }

    pub fn createTexture(self: *Renderer, width: u32, height: u32, format: u32) !u64 {
        const device = &(self.device orelse return error.NoDevice);
        const texture_format = textureFormat(format) orelse return error.UnsupportedTextureFormat;
        const info = sdl.c.SDL_GPUTextureCreateInfo{ .type = sdl.c.SDL_GPU_TEXTURETYPE_2D, .format = texture_format, .usage = sdl.c.SDL_GPU_TEXTUREUSAGE_SAMPLER, .width = width, .height = height, .layer_count_or_depth = 1, .num_levels = 1, .sample_count = sdl.c.SDL_GPU_SAMPLECOUNT_1, .props = 0 };
        const gpu = sdl.c.SDL_CreateGPUTexture(@ptrCast(@alignCast(device.handle.?)), &info) orelse return error.TextureCreateFailed;
        errdefer sdl.releaseTexture(@ptrCast(@alignCast(device.handle.?)), gpu);
        const id = self.next_resource_handle;
        self.next_resource_handle += 1;
        try self.textures.put(self.allocator, id, .{ .gpu = gpu, .width = width, .height = height });
        return id;
    }

    pub fn uploadTexture(self: *Renderer, id: u64, data: *const anyopaque, byte_length: u32, row_pitch: u32) !void {
        const texture = self.textures.get(id) orelse return error.InvalidTexture;
        if (row_pitch < texture.width * 4 or byte_length < row_pitch * texture.height) return error.TextureUploadOutOfBounds;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        const transfer = sdl.createUploadBuffer(gpu_device, row_pitch * texture.height) orelse return error.TransferBufferCreateFailed;
        defer sdl.releaseTransferBuffer(gpu_device, transfer);
        const mapped = sdl.mapTransferBuffer(gpu_device, transfer) orelse return error.TransferBufferMapFailed;
        @memcpy(@as([*]u8, @ptrCast(mapped))[0 .. row_pitch * texture.height], @as([*]const u8, @ptrCast(data))[0 .. row_pitch * texture.height]);
        sdl.unmapTransferBuffer(gpu_device, transfer);
        const command = sdl.acquireCommandBuffer(gpu_device) orelse return error.CommandBufferFailed;
        if (!sdl.uploadTexture(command, transfer, texture.gpu, texture.width, texture.height, row_pitch)) {
            _ = sdl.cancelCommandBuffer(command);
            return error.CopyPassFailed;
        }
        // No SDL_WaitForGPUIdle: same reasoning as uploadBuffer. The upload is
        // submitted before any frame that samples the texture, and SDL runs
        // command buffers in submission order, so the wait only stalled the
        // pipeline - badly during video, which uploads a frame-sized texture
        // every frame.
        if (!sdl.submitCommandBuffer(command)) return error.SubmitFailed;
    }

    pub fn destroyTexture(self: *Renderer, id: u64) !void {
        const texture = self.textures.fetchRemove(id) orelse return error.InvalidTexture;
        const device = &(self.device orelse return error.NoDevice);
        sdl.releaseTexture(@ptrCast(@alignCast(device.handle.?)), texture.value.gpu);
        for (&self.bound_textures) |*slot| {
            if (slot.* == id) slot.* = null;
        }
    }

    pub fn bindTextureStage(self: *Renderer, stage: u32, id: u64) !void {
        if (stage > 1) return error.InvalidTexture;
        if (!self.textures.contains(id)) return error.InvalidTexture;
        self.bound_textures[stage] = id;
    }

    pub fn bindVertexBuffer(self: *Renderer, id: u64) !void {
        if (!self.buffers.contains(id)) return error.InvalidBuffer;
        self.bound_vertex_buffer = id;
    }

    fn releaseTemporaryBuffers(self: *Renderer) void {
        while (self.temporary_buffers.pop()) |id| self.destroyBuffer(id) catch {};
        // Sampled before the recycling below: afterwards every in-use list is
        // empty, and it is the frame's own high-water mark that says how many
        // pooled buffers the frame actually needed.
        if (self.arena_enabled) {
            // present() closed the arena before this ran, and closing only rolls
            // the capacity decision forward - `wanted` and `capacity` still
            // describe the frame that just drew.
            self.perf_temp_vertex_free +%= self.arena.vertex.wanted;
            self.perf_temp_vertex_in_use +%= self.arena.vertex.capacity;
            self.perf_temp_index_free +%= self.arena.index.wanted;
            self.perf_temp_index_in_use +%= self.arena.index.capacity;
            self.perf_temp_transfer_free +%= self.arena.staging.capacity;
            self.perf_temp_transfer_in_use +%= self.arena.fallbacks;
        } else {
            self.perf_temp_vertex_free +%= @intCast(self.free_temp_vertex.items.len);
            self.perf_temp_vertex_in_use +%= @intCast(self.in_use_temp_vertex.items.len);
            self.perf_temp_index_free +%= @intCast(self.free_temp_index.items.len);
            self.perf_temp_index_in_use +%= @intCast(self.in_use_temp_index.items.len);
            self.perf_temp_transfer_free +%= @intCast(self.free_temp_transfer.items.len);
            self.perf_temp_transfer_in_use +%= @intCast(self.in_use_temp_transfer.items.len);
        }
        // The temp buffers used this frame go back to the free pool rather than
        // being destroyed, so next frame hands them straight out again.
        self.free_temp_vertex.appendSlice(self.allocator, self.in_use_temp_vertex.items) catch {};
        self.in_use_temp_vertex.clearRetainingCapacity();
        self.free_temp_index.appendSlice(self.allocator, self.in_use_temp_index.items) catch {};
        self.in_use_temp_index.clearRetainingCapacity();
        self.free_temp_transfer.appendSlice(self.allocator, self.in_use_temp_transfer.items) catch {};
        self.in_use_temp_transfer.clearRetainingCapacity();
    }

    fn tempCapacityFor(needed: u32) u32 {
        var capacity: u32 = 4096;
        while (capacity < needed) capacity <<= 1;
        return capacity;
    }

    fn acquireTempBuffer(self: *Renderer, pool: *std.ArrayListUnmanaged(TempBuffer), usage: sdl.c.SDL_GPUBufferUsageFlags, needed: u32) !TempBuffer {
        const capacity = tempCapacityFor(needed);
        var i: usize = 0;
        while (i < pool.items.len) : (i += 1) {
            if (pool.items[i].capacity >= capacity) return pool.swapRemove(i);
        }
        const device = &(self.device orelse return error.NoDevice);
        const gpu = sdl.createBuffer(@ptrCast(@alignCast(device.handle.?)), usage, capacity) orelse return error.BufferCreateFailed;
        return .{ .gpu = gpu, .capacity = capacity };
    }

    fn acquireTempTransfer(self: *Renderer, needed: u32) !TempTransfer {
        const capacity = tempCapacityFor(needed);
        var i: usize = 0;
        while (i < self.free_temp_transfer.items.len) : (i += 1) {
            if (self.free_temp_transfer.items[i].capacity >= capacity)
                return self.free_temp_transfer.swapRemove(i);
        }
        const device = &(self.device orelse return error.NoDevice);
        const transfer = sdl.createUploadBuffer(@ptrCast(@alignCast(device.handle.?)), capacity) orelse return error.TransferBufferCreateFailed;
        return .{ .transfer = transfer, .capacity = capacity };
    }

    // Recreate one arena buffer at the capacity its cursor asked for. A failure
    // leaves the handle null and the capacity zero, which sends every draw down
    // the per-draw path - degraded, never wrong. SDL defers the release of a
    // buffer an in-flight command buffer still references, so growing or
    // shrinking mid-run cannot pull the previous frame's geometry out from
    // under it.
    fn resizeArenaBuffer(gpu_device: *sdl.GpuDevice, slot: *?*sdl.GpuBuffer, cursor: *ArenaCursor, usage: sdl.c.SDL_GPUBufferUsageFlags) void {
        if (slot.* != null and cursor.pending == cursor.capacity) return;
        if (slot.*) |old| sdl.releaseBuffer(gpu_device, old);
        slot.* = sdl.createBuffer(gpu_device, usage, cursor.pending);
        cursor.capacity = if (slot.* == null) 0 else cursor.pending;
    }

    fn resizeArenaTransfer(self: *Renderer, gpu_device: *sdl.GpuDevice) void {
        const cursor = &self.arena.staging;
        if (self.arena.transfer != null and cursor.pending == cursor.capacity) return;
        if (self.arena.transfer) |old| sdl.releaseTransferBuffer(gpu_device, old);
        self.arena.transfer = sdl.createUploadBuffer(gpu_device, cursor.pending);
        cursor.capacity = if (self.arena.transfer == null) 0 else cursor.pending;
    }

    // Open the frame's arena: apply any capacity change, reset the cursors and
    // map the transfer buffer once. Called from the frame's first temporary
    // draw; closeArenaFrame at present or cancel is the other half, and the two
    // are what make the cursors' "one frame" mean one frame.
    fn beginArenaFrame(self: *Renderer, gpu_device: *sdl.GpuDevice) void {
        if (self.arena.open) return;
        self.arena.open = true;
        self.arena.fallbacks = 0;
        self.arena.cycle_vertex = true;
        self.arena.cycle_index = true;
        resizeArenaBuffer(gpu_device, &self.arena.vertex_gpu, &self.arena.vertex, sdl.c.SDL_GPU_BUFFERUSAGE_VERTEX);
        resizeArenaBuffer(gpu_device, &self.arena.index_gpu, &self.arena.index, sdl.c.SDL_GPU_BUFFERUSAGE_INDEX);
        self.resizeArenaTransfer(gpu_device);
        self.arena.vertex.beginFrame();
        self.arena.index.beginFrame();
        self.arena.staging.beginFrame();
        // mapTransferBuffer maps with cycle set, which is what keeps this frame
        // off the backing the previous frame's copy pass may still be reading.
        self.arena.mapped = null;
        if (self.arena.transfer) |transfer| {
            if (sdl.mapTransferBuffer(gpu_device, transfer)) |pointer| self.arena.mapped = @ptrCast(pointer);
        }
    }

    // Close the frame's arena. The transfer buffer must be unmapped before the
    // upload command buffer is submitted, and the copy pass must be ended before
    // that command buffer is submitted or cancelled.
    fn closeArenaFrame(self: *Renderer) void {
        if (!self.arena.open) return;
        self.endArenaCopyPass();
        if (self.arena.mapped != null) {
            if (self.device) |device| {
                if (self.arena.transfer) |transfer| sdl.unmapTransferBuffer(@ptrCast(@alignCast(device.handle.?)), transfer);
            }
            self.arena.mapped = null;
        }
        self.arena.vertex.endFrame();
        self.arena.index.endFrame();
        self.arena.staging.endFrame();
        self.arena.open = false;
    }

    fn endArenaCopyPass(self: *Renderer) void {
        if (self.arena.copy_pass) |pass| {
            sdl.endCopyPass(pass);
            self.arena.copy_pass = null;
        }
    }

    // One copy pass for the whole frame, on the shared upload command buffer.
    fn arenaCopyPass(self: *Renderer, gpu_device: *sdl.GpuDevice) ?*sdl.GpuCopyPass {
        if (self.arena.copy_pass) |pass| return pass;
        if (self.frame_upload_command == null)
            self.frame_upload_command = sdl.acquireCommandBuffer(gpu_device);
        const command = self.frame_upload_command orelse return null;
        self.arena.copy_pass = sdl.beginCopyPass(command);
        return self.arena.copy_pass;
    }

    // Reserve a destination range in one arena buffer and the staging range in
    // the transfer buffer that will feed it. Both cursors are asked even when
    // the first one fails, so `wanted` describes the frame's whole demand and
    // the next frame's arena is sized for it.
    fn arenaReserve(self: *Renderer, target: ArenaTarget, byte_length: u32, alignment: u32) ?ArenaRange {
        const cursor = switch (target) {
            .vertex => &self.arena.vertex,
            .index => &self.arena.index,
        };
        const destination = cursor.alloc(byte_length, alignment);
        const source = self.arena.staging.alloc(byte_length, arena_copy_alignment);
        if (self.arena.mapped == null) return null;
        return .{ .destination = destination orelse return null, .source = source orelse return null };
    }

    // Copy the reserved bytes into the mapped transfer buffer and record the
    // copy into the frame's one copy pass.
    fn arenaCopy(self: *Renderer, gpu_device: *sdl.GpuDevice, target: ArenaTarget, range: ArenaRange, data: *const anyopaque, byte_length: u32) bool {
        const mapped = self.arena.mapped orelse return false;
        const transfer = self.arena.transfer orelse return false;
        const buffer = (switch (target) {
            .vertex => self.arena.vertex_gpu,
            .index => self.arena.index_gpu,
        }) orelse return false;
        const pass = self.arenaCopyPass(gpu_device) orelse return false;
        const length: usize = byte_length;
        const offset: usize = range.source;
        @memcpy(mapped[offset .. offset + length], @as([*]const u8, @ptrCast(data))[0..length]);
        const cycle = switch (target) {
            .vertex => self.arena.cycle_vertex,
            .index => self.arena.cycle_index,
        };
        sdl.recordBufferUpload(pass, transfer, range.source, buffer, range.destination, byte_length, cycle);
        switch (target) {
            .vertex => self.arena.cycle_vertex = false,
            .index => self.arena.cycle_index = false,
        }
        self.perf_temp_bytes +%= byte_length;
        return true;
    }

    // Stage bytes into a pooled destination through a pooled transfer buffer and
    // record the copy into the frame's shared upload command buffer. The transfer
    // buffer is remembered for recycling at present; the caller keeps the
    // destination alive the same way. cycle: a pooled buffer may still be read by
    // the previous frame's in-flight draw, so ask SDL for a fresh backing rather
    // than stall or corrupt.
    fn stageTempCopy(self: *Renderer, gpu_device: *sdl.GpuDevice, dest: *sdl.GpuBuffer, data: *const anyopaque, byte_length: u32) !void {
        // The arena keeps one copy pass open on the shared upload command
        // buffer, and a second cannot be begun while it is. Only the arena's
        // overflow fallback reaches this with one open.
        self.endArenaCopyPass();
        self.perf_temp_bytes +%= byte_length;
        const transfer = try self.acquireTempTransfer(byte_length);
        errdefer self.free_temp_transfer.append(self.allocator, transfer) catch {};
        const mapped = sdl.mapTransferBuffer(gpu_device, transfer.transfer) orelse return error.TransferBufferMapFailed;
        @memcpy(@as([*]u8, @ptrCast(mapped))[0..byte_length], @as([*]const u8, @ptrCast(data))[0..byte_length]);
        sdl.unmapTransferBuffer(gpu_device, transfer.transfer);
        if (self.frame_upload_command == null)
            self.frame_upload_command = sdl.acquireCommandBuffer(gpu_device) orelse return error.CommandBufferFailed;
        if (!sdl.uploadBufferCycle(gpu_device, self.frame_upload_command.?, transfer.transfer, dest, 0, byte_length, true))
            return error.CopyPassFailed;
        try self.in_use_temp_transfer.append(self.allocator, transfer);
    }

    pub fn drawTemporary(self: *Renderer, data: *const anyopaque, byte_length: u32, stride: u32, format: u32, primitive_count: u32) !void {
        if (byte_length == 0 or stride == 0 or byte_length % stride != 0 or primitive_count == 0) return error.InvalidDraw;
        const vertex_count = formats.primitiveVertexCount(self.topology, primitive_count) catch return error.InvalidDraw;
        if (vertex_count > byte_length / stride) return error.InvalidDraw;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        self.perf_temp_draws +%= 1;

        if (self.arena_enabled) {
            self.beginArenaFrame(gpu_device);
            if (self.arenaReserve(.vertex, byte_length, arenaAlignmentFor(stride))) |range| {
                if (self.arenaCopy(gpu_device, .vertex, range, data, byte_length)) {
                    const arena_resource = BufferResource{ .gpu = self.arena.vertex_gpu.?, .size = byte_length, .stride = stride, .format = format };
                    return self.drawResourceAt(arena_resource, range.destination, primitive_count);
                }
            }
            // Bigger than the whole arena, or the arena is not built yet. The
            // per-draw path below still draws it, and the cursors have recorded
            // the size, so the next frame's arena covers it.
            self.arena.fallbacks +%= 1;
        }

        const vertex = try self.acquireTempBuffer(&self.free_temp_vertex, sdl.c.SDL_GPU_BUFFERUSAGE_VERTEX, byte_length);
        errdefer self.free_temp_vertex.append(self.allocator, vertex) catch {};
        try self.stageTempCopy(gpu_device, vertex.gpu, data, byte_length);

        const resource = BufferResource{ .gpu = vertex.gpu, .size = byte_length, .stride = stride, .format = format };
        try self.drawResource(resource, primitive_count);
        try self.in_use_temp_vertex.append(self.allocator, vertex);
    }

    pub fn drawTemporaryIndexed(self: *Renderer, vertex_data: *const anyopaque, vertex_bytes: u32, stride: u32, format: u32, index_data: *const anyopaque, index_bytes: u32, index_size: u32, index_count: u32) !void {
        if (vertex_bytes == 0 or stride == 0 or vertex_bytes % stride != 0) return error.InvalidDraw;
        if (index_count == 0 or (index_size != 2 and index_size != 4) or index_bytes < index_count * index_size) return error.InvalidDraw;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        self.perf_temp_draws +%= 1;

        if (self.arena_enabled) {
            self.beginArenaFrame(gpu_device);
            // Both reservations are taken before either copy: a vertex range
            // written into the arena whose index range then did not fit would be
            // dead space and would double-count the staged bytes.
            const vertex_range = self.arenaReserve(.vertex, vertex_bytes, arenaAlignmentFor(stride));
            const index_range = self.arenaReserve(.index, index_bytes, arenaAlignmentFor(index_size));
            if (vertex_range) |vertices| {
                if (index_range) |indices| {
                    if (self.arenaCopy(gpu_device, .vertex, vertices, vertex_data, vertex_bytes) and
                        self.arenaCopy(gpu_device, .index, indices, index_data, index_bytes))
                    {
                        const arena_resource = BufferResource{ .gpu = self.arena.vertex_gpu.?, .size = vertex_bytes, .stride = stride, .format = format };
                        return self.drawResourceIndexedAt(arena_resource, vertices.destination, self.arena.index_gpu.?, indices.destination, index_size, index_count);
                    }
                }
            }
            self.arena.fallbacks +%= 1;
        }

        const vertex = try self.acquireTempBuffer(&self.free_temp_vertex, sdl.c.SDL_GPU_BUFFERUSAGE_VERTEX, vertex_bytes);
        errdefer self.free_temp_vertex.append(self.allocator, vertex) catch {};
        const index = try self.acquireTempBuffer(&self.free_temp_index, sdl.c.SDL_GPU_BUFFERUSAGE_INDEX, index_bytes);
        errdefer self.free_temp_index.append(self.allocator, index) catch {};

        try self.stageTempCopy(gpu_device, vertex.gpu, vertex_data, vertex_bytes);
        try self.stageTempCopy(gpu_device, index.gpu, index_data, index_bytes);

        const resource = BufferResource{ .gpu = vertex.gpu, .size = vertex_bytes, .stride = stride, .format = format };
        try self.drawResourceIndexed(resource, index.gpu, index_size, index_count);
        try self.in_use_temp_vertex.append(self.allocator, vertex);
        try self.in_use_temp_index.append(self.allocator, index);
    }

    fn readShader(self: *Renderer, name: []const u8) ![]u8 {
        const directory = self.shader_directory orelse return error.ShaderDirectoryMissing;
        const path = try std.fmt.allocPrint(self.allocator, "{s}/{s}", .{ directory, name });
        defer self.allocator.free(path);
        var path_z = try self.allocator.alloc(u8, path.len + 1);
        defer self.allocator.free(path_z);
        @memcpy(path_z[0..path.len], path);
        path_z[path.len] = 0;
        const file = io_c.fopen(@ptrCast(path_z.ptr), "rb") orelse return error.ShaderFileMissing;
        defer _ = io_c.fclose(file);
        if (io_c.fseek(file, 0, io_c.SEEK_END) != 0) return error.ShaderFileReadFailed;
        const length = io_c.ftell(file);
        if (length <= 0 or length > 64 * 1024 * 1024) return error.ShaderFileReadFailed;
        if (io_c.fseek(file, 0, io_c.SEEK_SET) != 0) return error.ShaderFileReadFailed;
        const bytes = try self.allocator.alloc(u8, @intCast(length));
        errdefer self.allocator.free(bytes);
        if (io_c.fread(bytes.ptr, 1, bytes.len, file) != bytes.len) return error.ShaderFileReadFailed;
        return bytes;
    }

    fn shaderName(self: *Renderer, effect: []const u8, stage: []const u8, format: manifest.Format) ![]u8 {
        const suffix = switch (format) {
            .dxil => "dxil",
            .spirv => "spirv",
            .msl => "msl",
        };
        return std.fmt.allocPrint(self.allocator, "{s}.{s}.{s}", .{ effect, stage, suffix });
    }

    // The legacy effect table decides how a draw blends. Collapsing it to a bool
    // could only express straight alpha, so the terrain noise passes -- which
    // multiply into the framebuffer -- came out as flat grey overlays.
    fn blendModeForDraw(self: *const Renderer) effects.BlendMode {
        return self.blend_mode;
    }

    // SDL's C enums translate to a different backing integer per target ABI
    // (unsigned on the Apple/clang headers, signed on x86_64-windows-msvc), so
    // take the blend factor type from the struct we assign into rather than
    // naming a fixed width here.
    const BlendFactor = @FieldType(@FieldType(sdl.c.SDL_GPUColorTargetDescription, "blend_state"), "src_color_blendfactor");
    const BlendFactors = struct { source: BlendFactor, destination: BlendFactor, enabled: bool };

    fn blendFactors(mode: effects.BlendMode) BlendFactors {
        return switch (mode) {
            .replace => .{ .source = sdl.c.SDL_GPU_BLENDFACTOR_ONE, .destination = sdl.c.SDL_GPU_BLENDFACTOR_ZERO, .enabled = false },
            .straight_alpha => .{ .source = sdl.c.SDL_GPU_BLENDFACTOR_SRC_ALPHA, .destination = sdl.c.SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, .enabled = true },
            // D3DBLEND_DESTCOLOR / D3DBLEND_ZERO.
            .multiply => .{ .source = sdl.c.SDL_GPU_BLENDFACTOR_DST_COLOR, .destination = sdl.c.SDL_GPU_BLENDFACTOR_ZERO, .enabled = true },
            .additive => .{ .source = sdl.c.SDL_GPU_BLENDFACTOR_SRC_ALPHA, .destination = sdl.c.SDL_GPU_BLENDFACTOR_ONE, .enabled = true },
        };
    }

    fn ensureShaders(self: *Renderer, variant: ShaderVariant) !struct { vertex: *anyopaque, fragment: *anyopaque } {
        const slot = @intFromEnum(variant);
        if (self.vertex_shaders[slot]) |vertex| {
            if (self.fragment_shaders[slot]) |fragment| return .{ .vertex = vertex, .fragment = fragment };
        }
        const device = &(self.device orelse return error.NoDevice);
        const format = try device.shaderFormat();
        const shader_format = device_mod.formatFlag(format);
        const effect = variantEffect(variant);
        const vertex_name = try self.shaderName(effect, "vertex", format);
        defer self.allocator.free(vertex_name);
        const fragment_name = try self.shaderName(effect, "fragment", format);
        defer self.allocator.free(fragment_name);
        const vertex_code = try self.readShader(vertex_name);
        defer self.allocator.free(vertex_code);
        const fragment_code = try self.readShader(fragment_name);
        defer self.allocator.free(fragment_code);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        const samplers = variantSamplerCount(variant);
        const vertex_info = sdl.c.SDL_GPUShaderCreateInfo{ .code_size = vertex_code.len, .code = vertex_code.ptr, .entrypoint = variantVertexEntry(variant).ptr, .format = shader_format, .stage = sdl.c.SDL_GPU_SHADERSTAGE_VERTEX, .num_samplers = 0, .num_storage_textures = 0, .num_storage_buffers = 0, .num_uniform_buffers = 3, .props = 0 };
        const vertex = sdl.c.SDL_CreateGPUShader(gpu_device, &vertex_info) orelse return error.ShaderCreationFailed;
        errdefer sdl.c.SDL_ReleaseGPUShader(gpu_device, vertex);
        // The fragment stage only consumes the interpolated colour and, when
        // textured, sampler 0. Its cbuffer declarations come from the shared
        // header and are not bindings the stage references.
        const fragment_info = sdl.c.SDL_GPUShaderCreateInfo{ .code_size = fragment_code.len, .code = fragment_code.ptr, .entrypoint = variantFragmentEntry(variant).ptr, .format = shader_format, .stage = sdl.c.SDL_GPU_SHADERSTAGE_FRAGMENT, .num_samplers = samplers, .num_storage_textures = 0, .num_storage_buffers = 0, .num_uniform_buffers = 3, .props = 0 };
        const fragment = sdl.c.SDL_CreateGPUShader(gpu_device, &fragment_info) orelse return error.ShaderCreationFailed;
        errdefer sdl.c.SDL_ReleaseGPUShader(gpu_device, fragment);
        self.vertex_shaders[slot] = vertex;
        self.fragment_shaders[slot] = fragment;
        return .{ .vertex = vertex, .fragment = fragment };
    }

    fn ensureSamplers(self: *Renderer) !void {
        if (self.sampler != null and self.linear_sampler != null) return;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        if (self.sampler == null) self.sampler = sdl.createSampler(gpu_device, false) orelse return error.SamplerCreateFailed;
        if (self.linear_sampler == null) self.linear_sampler = sdl.createSampler(gpu_device, true) orelse return error.SamplerCreateFailed;
    }

    fn pipelineCacheKey(fvf: u32, textured: bool, blend: effects.BlendMode, dual: bool, topology: formats.Topology, depth: DepthState) u64 {
        return @as(u64, fvf) | (@as(u64, @intFromBool(textured)) << 32) |
            (@as(u64, @intFromEnum(blend)) << 33) | (@as(u64, @intFromBool(dual)) << 35) |
            (@as(u64, @intFromEnum(topology)) << 36) |
            (@as(u64, @intFromBool(depth.test_enabled)) << 39) |
            (@as(u64, @intFromBool(depth.write_enabled)) << 40) |
            (@as(u64, depth.compare) << 41) |
            (@as(u64, @intFromEnum(depth.stencil)) << 45);
    }

    pub const DepthState = struct { test_enabled: bool, write_enabled: bool, compare: u4, stencil: effects.StencilMode = .off };

    // GFXDB_NONE turns the test off entirely, as D3DRS_ZENABLE FALSE does; the
    // effect table says whether a pass that does test also writes, which is how
    // particles and transparencies sort behind the geometry they overlay.
    fn depthStateForDraw(self: *const Renderer) DepthState {
        const test_enabled = self.depth_mode != 0;
        const writes = if (effects.find(self.shade_effect)) |spec| spec.depth_write else true;
        // GFXCMP_DEFAULT means LESSEQUAL for the z-buffer.
        const compare: u4 = @intCast(if (self.depth_compare == 0) 4 else @min(self.depth_compare, 8));
        return .{ .test_enabled = test_enabled, .write_enabled = test_enabled and writes, .compare = compare, .stencil = self.stencil_mode };
    }

    // EQUAL against a reference of 0, incrementing on pass: the first fragment to
    // reach a pixel darkens it and stamps the stencil, and every later one fails.
    fn sdlStencilState(mode: effects.StencilMode) sdl.c.SDL_GPUStencilOpState {
        return switch (mode) {
            .off => .{ .fail_op = sdl.c.SDL_GPU_STENCILOP_KEEP, .pass_op = sdl.c.SDL_GPU_STENCILOP_KEEP, .depth_fail_op = sdl.c.SDL_GPU_STENCILOP_KEEP, .compare_op = sdl.c.SDL_GPU_COMPAREOP_ALWAYS },
            .darken_once => .{ .fail_op = sdl.c.SDL_GPU_STENCILOP_KEEP, .pass_op = sdl.c.SDL_GPU_STENCILOP_INCREMENT_AND_CLAMP, .depth_fail_op = sdl.c.SDL_GPU_STENCILOP_KEEP, .compare_op = sdl.c.SDL_GPU_COMPAREOP_EQUAL },
        };
    }

    fn sdlCompare(compare: u4) sdl.c.SDL_GPUCompareOp {
        return switch (compare) {
            1 => sdl.c.SDL_GPU_COMPAREOP_NEVER,
            2 => sdl.c.SDL_GPU_COMPAREOP_LESS,
            3 => sdl.c.SDL_GPU_COMPAREOP_EQUAL,
            4 => sdl.c.SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
            5 => sdl.c.SDL_GPU_COMPAREOP_GREATER,
            6 => sdl.c.SDL_GPU_COMPAREOP_NOT_EQUAL,
            7 => sdl.c.SDL_GPU_COMPAREOP_GREATER_OR_EQUAL,
            else => sdl.c.SDL_GPU_COMPAREOP_ALWAYS,
        };
    }

    fn sdlTopology(topology: formats.Topology) sdl.c.SDL_GPUPrimitiveType {
        return switch (topology) {
            .point_list => sdl.c.SDL_GPU_PRIMITIVETYPE_POINTLIST,
            .line_list => sdl.c.SDL_GPU_PRIMITIVETYPE_LINELIST,
            .line_strip => sdl.c.SDL_GPU_PRIMITIVETYPE_LINESTRIP,
            .triangle_list => sdl.c.SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
            .triangle_strip => sdl.c.SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
        };
    }

    // Builds the pipeline for one vertex format. Attribute locations follow the
    // shader's input signature, not the legacy semantic numbering: position is
    // always 0, then the colour if the format has a diffuse, then the texcoord.
    fn ensurePipeline(self: *Renderer, fvf: u32, want_texture: bool, blend_mode: effects.BlendMode) !*anyopaque {
        const layout = vertex_layout.decodeFvf(fvf) catch return error.UnsupportedVertexFormat;
        if (layout.stride == 0) return error.UnsupportedVertexFormat;
        const position = vertex_layout.find(layout, .position, 0) orelse return error.UnsupportedVertexFormat;
        const diffuse = vertex_layout.find(layout, .diffuse, 5);
        const texcoord = vertex_layout.find(layout, .texcoord, 0);
        const texcoord1 = vertex_layout.find(layout, .texcoord, 1);
        // A draw can bind a texture for geometry that carries no texcoord (the
        // terrain clears stage 0 between passes). There is nothing to sample
        // then, so it falls back to the untextured shader.
        const textured = want_texture and texcoord != null;
        // Two stages only when the effect combines them, the second texture is
        // actually bound, and the vertex format carries the second texcoord set.
        const dual = textured and texcoord1 != null and self.bound_textures[1] != null and
            effects.combineFor(self.shade_effect) != .single;
        // D3DRS_SPECULARENABLE. CGraphicsEngine::DrawRects turns it on for a
        // batch whose rects carry a non-black specular and off again after, which
        // is how a blinking UI element flashes.
        const specular = if (self.specular_enabled and diffuse != null and !dual)
            vertex_layout.find(layout, .specular, 6)
        else
            null;
        const depth = self.depthStateForDraw();
        const key = pipelineCacheKey(fvf, textured, blend_mode, dual, self.topology, depth) |
            (@as(u64, @intFromBool(specular != null)) << 47);
        if (self.pipelines.get(key)) |pipeline| return pipeline;
        const variant: ShaderVariant = if (dual)
            .textured_dual
        else if (textured)
            (if (specular != null) .textured_specular else if (diffuse != null) .textured else .textured_nocolor)
        else
            (if (specular != null) .untextured_specular else if (diffuse != null) .untextured else .untextured_nocolor);
        const shaders = try self.ensureShaders(variant);
        if (textured) try self.ensureSamplers();
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        var attributes: [4]sdl.c.SDL_GPUVertexAttribute = undefined;
        var attribute_count: u32 = 0;
        attributes[attribute_count] = .{ .location = 0, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = position.offset };
        attribute_count += 1;
        if (diffuse) |attribute| {
            attributes[attribute_count] = .{ .location = attribute_count, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = attribute.offset };
            attribute_count += 1;
        }
        // COLOR1 sits between the diffuse and the texcoord in the specular
        // variants' input signature, so it has to be bound there too.
        if (specular) |attribute| {
            attributes[attribute_count] = .{ .location = attribute_count, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, .offset = attribute.offset };
            attribute_count += 1;
        }
        if (textured) {
            attributes[attribute_count] = .{ .location = attribute_count, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = texcoord.?.offset };
            attribute_count += 1;
        }
        if (dual) {
            attributes[attribute_count] = .{ .location = attribute_count, .buffer_slot = 0, .format = sdl.c.SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = texcoord1.?.offset };
            attribute_count += 1;
        }
        var vertex_buffers = [_]sdl.c.SDL_GPUVertexBufferDescription{.{ .slot = 0, .pitch = layout.stride, .input_rate = sdl.c.SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 }};
        const color_format: sdl.c.SDL_GPUTextureFormat = @intCast(self.swapchain_format);
        const factors = blendFactors(blend_mode);
        const target = sdl.c.SDL_GPUColorTargetDescription{ .format = color_format, .blend_state = .{ .src_color_blendfactor = factors.source, .dst_color_blendfactor = factors.destination, .color_blend_op = sdl.c.SDL_GPU_BLENDOP_ADD, .src_alpha_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ONE, .dst_alpha_blendfactor = sdl.c.SDL_GPU_BLENDFACTOR_ZERO, .alpha_blend_op = sdl.c.SDL_GPU_BLENDOP_ADD, .color_write_mask = 0x0f, .enable_blend = factors.enabled, .enable_color_write_mask = true } };
        const pipeline_info = sdl.c.SDL_GPUGraphicsPipelineCreateInfo{ .vertex_shader = @ptrCast(@alignCast(shaders.vertex)), .fragment_shader = @ptrCast(@alignCast(shaders.fragment)), .vertex_input_state = .{ .vertex_buffer_descriptions = &vertex_buffers, .num_vertex_buffers = 1, .vertex_attributes = &attributes, .num_vertex_attributes = attribute_count }, .primitive_type = sdlTopology(self.topology), .rasterizer_state = .{ .fill_mode = sdl.c.SDL_GPU_FILLMODE_FILL, .cull_mode = sdl.c.SDL_GPU_CULLMODE_NONE, .front_face = sdl.c.SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE, .enable_depth_clip = true }, .multisample_state = .{ .sample_count = sdl.c.SDL_GPU_SAMPLECOUNT_1 }, .depth_stencil_state = .{ .compare_op = sdlCompare(depth.compare), .back_stencil_state = sdlStencilState(depth.stencil), .front_stencil_state = sdlStencilState(depth.stencil), .compare_mask = 0xff, .write_mask = 0xff, .enable_depth_test = depth.test_enabled, .enable_depth_write = depth.write_enabled, .enable_stencil_test = depth.stencil != .off }, .target_info = .{ .color_target_descriptions = &target, .num_color_targets = 1, .depth_stencil_format = if (self.scene_depth != null) sdl.depthFormat() else 0, .has_depth_stencil_target = self.scene_depth != null }, .props = 0 };
        const pipeline = sdl.c.SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info) orelse {
            // The C++ side only ever sees the name of this error, so SDL's own
            // reason - the only thing that says which part of the description
            // the driver rejected - was being dropped. Report it once: a
            // pipeline that fails to build fails again on every frame, and
            // every frame reporting it buries everything else in the log.
            if (!reported_pipeline_failure) {
                reported_pipeline_failure = true;
                const reason = sdl.c.SDL_GetError();
                std.debug.print("BK_GFX_TRACE: pipeline create failed variant={s} color_format={d} depth_format={d} has_depth={} attributes={d} topology={d} reason={s}\n", .{
                    @tagName(variant),
                    color_format,
                    pipeline_info.target_info.depth_stencil_format,
                    pipeline_info.target_info.has_depth_stencil_target,
                    attribute_count,
                    @intFromEnum(self.topology),
                    if (reason != null) std.mem.span(reason) else "(none)",
                });
            }
            return error.PipelineCreateFailed;
        };
        errdefer sdl.c.SDL_ReleaseGPUGraphicsPipeline(gpu_device, pipeline);
        try self.pipelines.put(self.allocator, key, pipeline);
        return pipeline;
    }

    // Picks the pipeline for a draw from the bound vertex buffer's own format.
    fn pipelineForDraw(self: *Renderer, buffer: BufferResource) !*anyopaque {
        const fvf = if (buffer.format != 0) buffer.format else default_fvf;
        return self.ensurePipeline(fvf, self.bound_textures[0] != null, self.blendModeForDraw());
    }

    // GFXFVF_XYZRHW (0x004) marks a position the engine already transformed to
    // screen pixels, which the shader must pass straight to clip space.
    fn isPreTransformed(fvf: u32) bool {
        return (fvf & 0x0f) == 0x004;
    }

    // Stage 1 participates only when a texture is bound to it and the current
    // effect actually combines the two stages.
    fn dualTextureActive(self: *const Renderer) ?u64 {
        if (effects.combineFor(self.shade_effect) == .single) return null;
        return self.bound_textures[1];
    }

    // The colour a draw modulates by. Lit draws fold in the material because
    // that is where their diffuse lives; unlit ones keep the draw colour alone.
    fn effectiveDrawColor(self: *const Renderer) [4]f32 {
        if (!self.lighting_enabled) return self.draw_color;
        return .{
            self.draw_color[0] * self.material_diffuse[0],
            self.draw_color[1] * self.material_diffuse[1],
            self.draw_color[2] * self.material_diffuse[2],
            self.draw_color[3] * self.material_diffuse[3],
        };
    }

    fn pushDrawUniforms(self: *Renderer, fvf: u32) !void {
        const command = self.frame.command_buffer orelse return error.InvalidState;
        const frame_uniforms = MatrixUniforms{ .matrix = self.view_proj_matrix, .padding = .{ 0, 0, 0, 0 } };
        // Pre-transformed vertices are in game-screen pixels, and they render
        // into the scene texture - normalize against it, not the swapchain.
        const width: f32 = @floatFromInt(@max(self.scene_width, 1));
        const height: f32 = @floatFromInt(@max(self.scene_height, 1));
        // w carries the stage combine the fragment shader should apply.
        const combine: f32 = if (self.dualTextureActive() != null) @floatFromInt(@intFromEnum(effects.combineFor(self.shade_effect))) else 0;
        const screen: [4]f32 = if (isPreTransformed(fvf))
            .{ 1, 1 / width, 1 / height, combine }
        else
            .{ 0, 1 / width, 1 / height, combine };
        const texture_matrix = if (effects.usesTextureTransform(self.shade_effect)) self.texture_matrix else identity_matrix;
        const color_op: f32 = @floatFromInt(@intFromEnum(effects.colorOpFor(self.shade_effect)));
        const alpha_ref: f32 = @as(f32, @floatFromInt(effects.alphaRefFor(self.shade_effect))) / 255.0;
        const draw_uniforms = DrawUniforms{ .matrix = self.world_matrix, .color = self.effectiveDrawColor(), .screen = screen, .texture_matrix = texture_matrix, .stage = .{ color_op, alpha_ref, 0, 0 } };
        sdl.pushVertexUniformData(@ptrCast(@alignCast(command)), 0, @ptrCast(&frame_uniforms), @sizeOf(MatrixUniforms));
        sdl.pushVertexUniformData(@ptrCast(@alignCast(command)), 1, @ptrCast(&draw_uniforms), @sizeOf(DrawUniforms));
        // The fragment stage declares the same cbuffers and reads g_screen for
        // the stage combine, so it needs its own copy: uniforms pushed to the
        // vertex stage are not visible to it.
        sdl.pushFragmentUniformData(@ptrCast(@alignCast(command)), 0, @ptrCast(&frame_uniforms), @sizeOf(MatrixUniforms));
        sdl.pushFragmentUniformData(@ptrCast(@alignCast(command)), 1, @ptrCast(&draw_uniforms), @sizeOf(DrawUniforms));
    }

    pub fn draw(self: *Renderer, vertex_buffer: u64, primitive_count: u32) !void {
        if (primitive_count == 0) return error.InvalidDraw;
        const buffer = self.buffers.get(vertex_buffer) orelse return error.InvalidBuffer;
        self.bound_vertex_buffer = vertex_buffer;
        try self.drawResource(buffer, primitive_count);
    }

    // Pipeline, uniforms, texture/sampler and viewport state common to every
    // draw. Returns the render pass so the caller only has to bind buffers and
    // issue the draw.
    fn bindDrawState(self: *Renderer, buffer: BufferResource) !*anyopaque {
        const pass = self.frame.render_pass orelse return error.InvalidState;
        const pipeline = try self.pipelineForDraw(buffer);
        try self.pushDrawUniforms(buffer.format);
        sdl.bindPipeline(@ptrCast(@alignCast(pass)), @ptrCast(@alignCast(pipeline)));
        if (self.bound_textures[0]) |texture_id| {
            const texture = self.textures.get(texture_id) orelse return error.InvalidTexture;
            const sampler = if (self.use_linear_sampler) self.linear_sampler else self.sampler;
            const sampler_handle = sampler orelse return error.SamplerMissing;
            // A pipeline built for two samplers must have both bound.
            if (self.dualTextureActive()) |second_id| {
                const second = self.textures.get(second_id) orelse return error.InvalidTexture;
                sdl.bindFragmentSamplers2(@ptrCast(@alignCast(pass)), texture.gpu, second.gpu, sampler_handle);
            } else {
                sdl.bindFragmentSampler(@ptrCast(@alignCast(pass)), texture.gpu, sampler_handle);
            }
        }
        if (self.viewport) |viewport| sdl.setViewport(@ptrCast(@alignCast(pass)), viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth);
        return pass;
    }

    // The shared draw tail, over a buffer that need not live in the buffers map.
    // drawTemporary builds a BufferResource around a pooled buffer and reaches
    // it here without registering an id.
    fn drawResource(self: *Renderer, buffer: BufferResource, primitive_count: u32) !void {
        return self.drawResourceAt(buffer, 0, primitive_count);
    }

    // The same at a byte offset into the buffer. The arena binds one big vertex
    // buffer and moves the binding offset instead of handing out a buffer per
    // draw; the pipeline's attribute offsets are relative to the binding, so
    // element zero of the draw sits exactly at `vertex_offset` and the draw
    // still starts from vertex 0.
    fn drawResourceAt(self: *Renderer, buffer: BufferResource, vertex_offset: u32, primitive_count: u32) !void {
        const pass = try self.bindDrawState(buffer);
        sdl.bindVertexBuffer(@ptrCast(@alignCast(pass)), buffer.gpu, vertex_offset);
        const vertex_count = formats.primitiveVertexCount(self.topology, primitive_count) catch return error.InvalidDraw;
        sdl.drawPrimitives(@ptrCast(@alignCast(pass)), vertex_count, 0);
    }

    // The indexed counterpart, over pooled vertex and index buffers.
    fn drawResourceIndexed(self: *Renderer, vertex: BufferResource, index_gpu: *sdl.GpuBuffer, index_size: u32, index_count: u32) !void {
        return self.drawResourceIndexedAt(vertex, 0, index_gpu, 0, index_size, index_count);
    }

    // The indexed counterpart at arena offsets. Index zero of the draw sits at
    // `index_offset`, so first_index stays 0 and the index values keep counting
    // from the vertex the binding offset put first.
    fn drawResourceIndexedAt(self: *Renderer, vertex: BufferResource, vertex_offset: u32, index_gpu: *sdl.GpuBuffer, index_offset: u32, index_size: u32, index_count: u32) !void {
        const pass = try self.bindDrawState(vertex);
        sdl.bindVertexBuffer(@ptrCast(@alignCast(pass)), vertex.gpu, vertex_offset);
        if (!sdl.bindIndexBuffer(@ptrCast(@alignCast(pass)), index_gpu, index_offset, index_size)) return error.InvalidDraw;
        sdl.drawIndexedPrimitives(@ptrCast(@alignCast(pass)), index_count, 0, 0);
    }

    pub fn drawIndexed(self: *Renderer, index_buffer: u64, index_size: u32, first_index: u32, index_count: u32, vertex_offset: i32) !void {
        if (index_count == 0 or (index_size != 2 and index_size != 4)) return error.InvalidDraw;
        const pass = self.frame.render_pass orelse return error.InvalidState;
        const buffer = self.buffers.get(index_buffer) orelse return error.InvalidBuffer;
        const vertex_id = self.bound_vertex_buffer orelse return error.VertexBufferMissing;
        const vertex_buffer = self.buffers.get(vertex_id) orelse return error.InvalidBuffer;
        const index_offset = std.math.mul(u32, first_index, index_size) catch return error.InvalidDraw;
        if (index_offset > buffer.size or index_count > (buffer.size - index_offset) / index_size) return error.InvalidDraw;
        const pipeline = try self.pipelineForDraw(vertex_buffer);
        try self.pushDrawUniforms(vertex_buffer.format);
        sdl.bindPipeline(@ptrCast(@alignCast(pass)), @ptrCast(@alignCast(pipeline)));
        if (self.bound_textures[0]) |texture_id| {
            const texture = self.textures.get(texture_id) orelse return error.InvalidTexture;
            const sampler = if (self.use_linear_sampler) self.linear_sampler else self.sampler;
            const sampler_handle = sampler orelse return error.SamplerMissing;
            // A pipeline built for two samplers must have both bound.
            if (self.dualTextureActive()) |second_id| {
                const second = self.textures.get(second_id) orelse return error.InvalidTexture;
                sdl.bindFragmentSamplers2(@ptrCast(@alignCast(pass)), texture.gpu, second.gpu, sampler_handle);
            } else {
                sdl.bindFragmentSampler(@ptrCast(@alignCast(pass)), texture.gpu, sampler_handle);
            }
        }
        if (self.viewport) |viewport| sdl.setViewport(@ptrCast(@alignCast(pass)), viewport.x, viewport.y, viewport.width, viewport.height, viewport.min_depth, viewport.max_depth);
        sdl.bindVertexBuffer(@ptrCast(@alignCast(pass)), vertex_buffer.gpu, 0);
        if (!sdl.bindIndexBuffer(@ptrCast(@alignCast(pass)), buffer.gpu, 0, index_size)) return error.InvalidDraw;
        sdl.drawIndexedPrimitives(@ptrCast(@alignCast(pass)), index_count, first_index, vertex_offset);
    }

    pub fn readback(self: *Renderer, destination: []u8, width: u32, height: u32, row_pitch: u32) !void {
        const texture = self.scene_texture orelse return error.ReadbackUnavailable;
        if (width == 0 or height == 0 or row_pitch < width * 4 or destination.len < @as(usize, row_pitch) * height) return error.ReadbackInvalid;
        const device = &(self.device orelse return error.NoDevice);
        const gpu_device: *sdl.GpuDevice = @ptrCast(@alignCast(device.handle.?));
        const transfer = sdl.createDownloadBuffer(gpu_device, row_pitch * height) orelse return error.TransferBufferCreateFailed;
        defer sdl.releaseTransferBuffer(gpu_device, transfer);
        const command = sdl.acquireCommandBuffer(gpu_device) orelse return error.CommandBufferFailed;
        if (!sdl.downloadTexture(command, texture, transfer, width, height)) {
            _ = sdl.cancelCommandBuffer(command);
            return error.CopyPassFailed;
        }
        if (!sdl.submitCommandBuffer(command)) return error.SubmitFailed;
        if (!sdl.waitForIdle(gpu_device)) return error.WaitForIdleFailed;
        const mapped = sdl.mapTransferBuffer(gpu_device, transfer) orelse return error.TransferBufferMapFailed;
        @memcpy(destination[0 .. @as(usize, row_pitch) * height], @as([*]const u8, @ptrCast(mapped))[0 .. @as(usize, row_pitch) * height]);
        sdl.unmapTransferBuffer(gpu_device, transfer);
    }
};

test "the present mode is recorded only once the device takes it, and never mid-frame" {
    const Fake = struct {
        var configures: u32 = 0;
        var accept: bool = true;
        fn destroy(_: *anyopaque) void {}
        fn configure(_: *anyopaque, _: *anyopaque, _: sdl.PresentMode) bool {
            configures += 1;
            return accept;
        }
    };
    Fake.configures = 0;
    Fake.accept = false;
    var api = device_mod.real_api;
    api.destroy = Fake.destroy;
    api.configure_swapchain = Fake.configure;

    var renderer = Renderer.init(std.testing.allocator);
    var window: u8 = 0;
    var handle: u8 = 0;
    renderer.device = device_mod.Device{ .allocator = std.testing.allocator, .api = api, .handle = &handle };
    renderer.window = &window;
    // deinit would drive the real SDL release calls against the fake handle.
    defer {
        renderer.device = null;
        renderer.window = null;
        renderer.deinit();
    }

    // A refused reconfiguration leaves the live mode alone, so the next frame
    // asks again instead of believing the new mode took effect.
    try std.testing.expectError(error.SwapchainConfigurationFailed, renderer.setPresentMode(.immediate));
    try std.testing.expectEqual(@as(u32, 1), Fake.configures);
    try std.testing.expectEqual(sdl.PresentMode.vsync, renderer.present_mode);

    Fake.accept = true;
    try renderer.setPresentMode(.immediate);
    try std.testing.expectEqual(sdl.PresentMode.immediate, renderer.present_mode);
    // Called every frame, so an unchanged mode must not touch the swapchain.
    try renderer.setPresentMode(.immediate);
    try std.testing.expectEqual(@as(u32, 2), Fake.configures);

    // A frame in flight owns the acquired swapchain texture until its command
    // buffer is submitted; recreating the swapchain under it is refused.
    renderer.frame.state = .recording;
    try std.testing.expectError(error.InvalidState, renderer.setPresentMode(.vsync));
    try std.testing.expectEqual(sdl.PresentMode.immediate, renderer.present_mode);
    try std.testing.expectEqual(@as(u32, 2), Fake.configures);
    renderer.frame.state = .idle;
}

test "arena offsets are a whole number of elements and a legal copy offset" {
    // SDL_GPUBufferBinding.offset is where element zero of the draw lives, so an
    // offset that is not a multiple of the stride reads every attribute from the
    // middle of a vertex. It also has to be a legal buffer-copy offset, which is
    // what the 16 is for.
    const Cursor = Renderer.ArenaCursor;
    inline for (.{ @as(u32, 20), 24, 28, 32, 36, 40 }) |stride| {
        const alignment = Renderer.arenaAlignmentFor(stride);
        try std.testing.expectEqual(@as(u32, 0), alignment % stride);
        try std.testing.expectEqual(@as(u32, 0), alignment % 16);
    }
    // Index elements are 2 or 4 bytes, both of which divide 16.
    try std.testing.expectEqual(@as(u32, 16), Renderer.arenaAlignmentFor(2));
    try std.testing.expectEqual(@as(u32, 16), Renderer.arenaAlignmentFor(4));

    var cursor = Cursor{ .capacity = 4096, .pending = 4096 };
    cursor.beginFrame();
    const stride: u32 = 28;
    const alignment = Renderer.arenaAlignmentFor(stride);
    var previous_end: u32 = 0;
    for (0..8) |index| {
        // 3, 6, 9, ... vertices, so the next request never starts aligned by luck.
        const length: u32 = @as(u32, @intCast(index + 1)) * 3 * stride;
        const offset = cursor.alloc(length, alignment) orelse return error.TestUnexpectedResult;
        try std.testing.expectEqual(@as(u32, 0), offset % stride);
        try std.testing.expectEqual(@as(u32, 0), offset % 16);
        try std.testing.expect(offset >= previous_end);
        previous_end = offset + length;
    }
}

test "arena suballocation packs, fits exactly, and refuses to run off the end" {
    const Cursor = Renderer.ArenaCursor;
    var cursor = Cursor{ .capacity = 64, .pending = 64 };
    cursor.beginFrame();
    try std.testing.expectEqual(@as(?u32, 0), cursor.alloc(16, 16));
    try std.testing.expectEqual(@as(?u32, 16), cursor.alloc(16, 16));
    // Exactly the rest of the arena still fits.
    try std.testing.expectEqual(@as(?u32, 32), cursor.alloc(32, 16));
    try std.testing.expectEqual(@as(u32, 64), cursor.cursor);
    // One byte past the end does not, and does not move the cursor either.
    try std.testing.expectEqual(@as(?u32, null), cursor.alloc(1, 1));
    try std.testing.expectEqual(@as(u32, 64), cursor.cursor);
    // A zero-length request is not a draw.
    try std.testing.expectEqual(@as(?u32, null), cursor.alloc(0, 16));
}

test "an overflowed frame grows the arena instead of falling back again" {
    const Cursor = Renderer.ArenaCursor;
    var cursor = Cursor{ .capacity = 4096, .pending = 4096, .minimum = 4096, .maximum = 1 << 20 };
    cursor.beginFrame();
    _ = cursor.alloc(4096, 16);
    // Overflow: the draw falls back to a per-draw buffer, but `wanted` still
    // counts it, which is what makes the next frame large enough.
    try std.testing.expectEqual(@as(?u32, null), cursor.alloc(3000, 16));
    try std.testing.expectEqual(@as(u32, 7096), cursor.wanted);
    cursor.endFrame();
    try std.testing.expectEqual(@as(u32, 8192), cursor.pending);

    // Capacity only changes when the buffer is really recreated, which is the
    // renderer's job; from there the same frame fits with nothing left over.
    cursor.capacity = cursor.pending;
    cursor.beginFrame();
    try std.testing.expectEqual(@as(?u32, 0), cursor.alloc(4096, 16));
    try std.testing.expectEqual(@as(?u32, 4096), cursor.alloc(3000, 16));
}

test "a single draw larger than the arena maximum keeps falling back, never fails" {
    const Cursor = Renderer.ArenaCursor;
    var cursor = Cursor{ .capacity = 4096, .pending = 4096, .minimum = 4096, .maximum = 8192 };
    cursor.beginFrame();
    // One draw that no arena will ever hold. alloc says so rather than
    // truncating, and the caller draws it through its own buffer.
    try std.testing.expectEqual(@as(?u32, null), cursor.alloc(100000, 16));
    // Smaller draws behind it still get arena space: one huge sprite must not
    // push the whole frame onto the slow path.
    try std.testing.expectEqual(@as(?u32, 0), cursor.alloc(256, 16));
    cursor.endFrame();
    // The capacity climbs to the cap and stops there.
    try std.testing.expectEqual(@as(u32, 8192), cursor.pending);
    cursor.capacity = cursor.pending;
    cursor.beginFrame();
    try std.testing.expectEqual(@as(?u32, null), cursor.alloc(100000, 16));
    cursor.endFrame();
    try std.testing.expectEqual(@as(u32, 8192), cursor.pending);
}

test "the arena comes back down to the trailing peak instead of keeping it" {
    const Cursor = Renderer.ArenaCursor;
    var cursor = Cursor{ .capacity = 4096, .pending = 4096, .minimum = 4096, .maximum = 1 << 20, .review_frames = 4 };
    // One expensive frame - a screen full of text - takes the arena up.
    cursor.beginFrame();
    _ = cursor.alloc(60000, 16);
    cursor.endFrame();
    try std.testing.expectEqual(@as(u32, 65536), cursor.pending);
    cursor.capacity = cursor.pending;

    // The pool problem this replaces: quiet frames afterwards must not keep the
    // peak forever. Two review windows of small frames bring it back.
    var frame: u32 = 0;
    while (frame < 4 * cursor.review_frames) : (frame += 1) {
        cursor.beginFrame();
        _ = cursor.alloc(1000, 16);
        cursor.endFrame();
        cursor.capacity = cursor.pending;
    }
    try std.testing.expectEqual(@as(u32, 4096), cursor.capacity);

    // ...but not off a single dip: one quiet frame inside a busy window leaves
    // the capacity where the busy frames put it.
    cursor.capacity = 65536;
    cursor.pending = 65536;
    cursor.window_peak = 0;
    cursor.previous_peak = 0;
    cursor.window_frames = 0;
    frame = 0;
    while (frame < 3) : (frame += 1) {
        cursor.beginFrame();
        _ = cursor.alloc(if (frame == 1) 100 else 60000, 16);
        cursor.endFrame();
        cursor.capacity = cursor.pending;
    }
    try std.testing.expectEqual(@as(u32, 65536), cursor.capacity);
}

test "every frame starts the arena over at offset zero" {
    // The rotation invariant, on the CPU side: beginFrame is the only thing that
    // resets the cursor, so a frame can never hand out a range the frame before
    // it also handed out - and the GPU side of it is that the frame's first copy
    // into each buffer cycles, so the backing being written is never one an
    // in-flight command buffer is still reading.
    const Cursor = Renderer.ArenaCursor;
    var cursor = Cursor{ .capacity = 4096, .pending = 4096 };
    var first: [4]?u32 = undefined;
    cursor.beginFrame();
    for (&first) |*slot| slot.* = cursor.alloc(64, 16);
    cursor.endFrame();

    var second: [4]?u32 = undefined;
    cursor.beginFrame();
    try std.testing.expectEqual(@as(u32, 0), cursor.cursor);
    try std.testing.expectEqual(@as(u32, 0), cursor.wanted);
    for (&second) |*slot| slot.* = cursor.alloc(64, 16);
    try std.testing.expectEqualSlices(?u32, &first, &second);

    // Within a frame the ranges never overlap, whatever the mix of sizes.
    cursor.beginFrame();
    var end: u32 = 0;
    for ([_]u32{ 12, 96, 3, 400, 1 }) |length| {
        const offset = cursor.alloc(length, 16) orelse return error.TestUnexpectedResult;
        try std.testing.expect(offset >= end);
        end = offset + length;
    }
}

test "the arena path is on unless BK_GPU_ARENA=0 says otherwise" {
    // The flag exists so a rendering regression can be bisected on one variable
    // with the same binary; defaulting it off would defeat that. The value the
    // process was started with is whatever it is, so the expectation is derived
    // from the same string rather than assumed.
    const expected = if (sdl.getEnv("BK_GPU_ARENA")) |value| !(value.len == 1 and value[0] == '0') else true;
    var renderer = Renderer.init(std.testing.allocator);
    defer renderer.deinit();
    try std.testing.expectEqual(expected, renderer.arena_enabled);
}

test "every shader variant has a slot and a name" {
    // The caches are indexed by @intFromEnum. They used to be a hardcoded [5],
    // so adding the specular variants wrote past the end of the Renderer -- and
    // a release build has no bounds check, so it handed Metal a garbage vertex
    // function and crashed inside setVertexFunction:.
    const fields = @typeInfo(Renderer.ShaderVariant).@"enum".fields;
    try std.testing.expectEqual(fields.len, shader_variant_count);
    inline for (fields) |field| {
        const variant: Renderer.ShaderVariant = @enumFromInt(field.value);
        try std.testing.expect(@intFromEnum(variant) < shader_variant_count);
        try std.testing.expect(Renderer.variantEffect(variant).len > 0);
        try std.testing.expect(Renderer.variantVertexEntry(variant).len > 0);
        try std.testing.expect(Renderer.variantFragmentEntry(variant).len > 0);
        try std.testing.expect(Renderer.variantSamplerCount(variant) <= 2);
    }
}

test "line geometry gets its own pipeline and its own vertex count" {
    // A line list and a triangle list of the same format must not share a
    // pipeline: the cached one carried the primitive type, so whichever draw ran
    // first decided how both rasterised.
    const fvf: u32 = 0x002 | 0x040 | 0x080 | 0x100; // SGFXLVertex
    const tested: Renderer.DepthState = .{ .test_enabled = true, .write_enabled = true, .compare = 4 };
    const lines = Renderer.pipelineCacheKey(fvf, false, .straight_alpha, false, .line_list, tested);
    const triangles = Renderer.pipelineCacheKey(fvf, false, .straight_alpha, false, .triangle_list, tested);
    try std.testing.expect(lines != triangles);

    // Depth state is part of the key too: the terrain draws with the test off
    // and meshes with it on, and one pipeline cannot serve both.
    const untested: Renderer.DepthState = .{ .test_enabled = false, .write_enabled = false, .compare = 4 };
    try std.testing.expect(Renderer.pipelineCacheKey(fvf, false, .straight_alpha, false, .triangle_list, untested) != triangles);
    // Depth-writing and read-only passes are distinct as well: particles test
    // against the scene without writing into it.
    const read_only: Renderer.DepthState = .{ .test_enabled = true, .write_enabled = false, .compare = 4 };
    try std.testing.expect(Renderer.pipelineCacheKey(fvf, false, .straight_alpha, false, .triangle_list, read_only) != triangles);

    // DrawRects lays a non-solid rect out as eight vertices, four lines. Read as
    // triangles those four primitives claim twelve vertices from an eight-vertex
    // buffer.
    try std.testing.expectEqual(@as(u32, 8), try formats.primitiveVertexCount(.line_list, 4));
    try std.testing.expectEqual(@as(u32, 12), try formats.primitiveVertexCount(.triangle_list, 4));
}

test "lit draws take their diffuse from the material" {
    var renderer = Renderer.init(std.testing.allocator);
    renderer.draw_color = .{ 1, 1, 1, 1 };

    // CScene::Draw's mesh shadow pass: an all-black material whose alpha is
    // MESH_SHADOW_DENSITY, drawn untextured with lighting on. Without the
    // material the shadow is an opaque white silhouette.
    renderer.lighting_enabled = true;
    renderer.material_diffuse = .{ 0, 0, 0, 0.5 };
    try std.testing.expectEqual([4]f32{ 0, 0, 0, 0.5 }, renderer.effectiveDrawColor());

    // CMeshVisObj's default material is opaque white, so lit meshes are
    // unchanged, and SetOpacity only moves alpha.
    renderer.material_diffuse = .{ 1, 1, 1, 1 };
    try std.testing.expectEqual([4]f32{ 1, 1, 1, 1 }, renderer.effectiveDrawColor());
    renderer.material_diffuse = .{ 1, 1, 1, 0.25 };
    try std.testing.expectEqual([4]f32{ 1, 1, 1, 0.25 }, renderer.effectiveDrawColor());

    // Unlit draws never consult it, however stale it is.
    renderer.lighting_enabled = false;
    renderer.material_diffuse = .{ 0, 0, 0, 0 };
    try std.testing.expectEqual([4]f32{ 1, 1, 1, 1 }, renderer.effectiveDrawColor());
}
