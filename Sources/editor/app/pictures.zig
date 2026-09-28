//! Per-object pictures for the palette (D-29): each object's own icon.tga,
//! decoded by the engine on demand (BkEditorObjectPicture) and cached as an
//! SDL GPU texture ImGui can draw with `igImage` - never a per-type symbol.
//!
//! `request` queues a name once; `pump` drains a budget of the queue each
//! frame, decoding through the bridge and uploading into its own texture;
//! `lookup` reports what a row should draw right now. `clear` (a mod switch,
//! D-26) and `deinit` (shutdown) release every texture. The queue's own
//! ordering and dedup rules (queued once, request order, a per-frame
//! budget, a missing mark that is never retried) live in
//! `panels_logic.PictureQueue`, pure enough to run under its own tests with
//! no GPU or bridge - only the ready textures below need either.
const std = @import("std");
const sdl3 = @import("sdl3");
const c_bridge = @import("c_bridge.zig");
const logic = @import("panels_logic.zig");

const RealBridge = c_bridge.RealBridge;
const Picture = c_bridge.Picture;
const PictureQueue = logic.PictureQueue;

/// The side BkEditorObjectPicture decodes to (within its own 8..256
/// contract): big enough for the palette's 48x48 row picture, small enough
/// that a decode is cheap. `pump`'s own scratch buffer is sized for this.
pub const decode_max_side: i32 = 64;

/// The most names one `pump` call can drain from the queue - callers pass a
/// smaller `budget` (panels.zig's own frame-time choice, see 03-09-SUMMARY);
/// this only bounds `pump`'s own stack buffer.
const max_pump_budget: usize = 32;

const Entry = struct {
    texture: *sdl3.c.SDL_GPUTexture,
    width: i32,
    height: i32,
};

/// What `lookup` reports for a name: nothing decoded yet, decoded to
/// nothing (no icon.tga, or it would not fit), or a ready texture.
pub const Lookup = union(enum) {
    pending,
    missing,
    ready: struct { texture: *sdl3.c.SDL_GPUTexture, width: i32, height: i32 },
};

pub const Pictures = struct {
    allocator: std.mem.Allocator,
    /// The engine's SDL_GPUDevice (RealBridge.gpuDevice), learned from
    /// `pump`'s own argument - `request`/`lookup`/`clear` need no device,
    /// and `init` is called before the palette's first frame has one to
    /// give `pump`. Textures are released against whichever device most
    /// recently uploaded one, which is always the session's single device.
    device: ?*anyopaque = null,
    /// Ready textures only - a name whose decode failed never lands here
    /// (see `queue.isMissing` instead), so `deinit`/`clear` only ever
    /// release real textures.
    entries: std.StringHashMapUnmanaged(Entry) = .empty,
    /// Which names are queued or already known missing, and the order to
    /// serve them in (panels_logic.PictureQueue).
    queue: PictureQueue,

    pub fn init(allocator: std.mem.Allocator) Pictures {
        return .{ .allocator = allocator, .queue = PictureQueue.init(allocator) };
    }

    pub fn deinit(self: *Pictures) void {
        self.clear();
        self.entries.deinit(self.allocator);
        self.queue.deinit();
        self.* = undefined;
    }

    /// Releases every texture and forgets every cached, queued and missing
    /// name - a mod switch (D-26) invalidates every picture: the new mod's
    /// objects may reuse a name with a different icon, or none at all.
    pub fn clear(self: *Pictures) void {
        var it = self.entries.iterator();
        while (it.next()) |kv| {
            if (self.device) |device| releaseTexture(device, kv.value_ptr.texture);
            self.allocator.free(kv.key_ptr.*);
        }
        self.entries.clearAndFree(self.allocator);
        self.queue.clear();
    }

    /// Queues `name` for `pump` to decode, once: already cached (ready or
    /// missing) or already queued is a no-op, so a group redrawn every
    /// frame does not re-queue its own rows every frame.
    pub fn request(self: *Pictures, name: []const u8) void {
        if (self.entries.contains(name)) return;
        self.queue.request(name);
    }

    /// What a row should draw for `name` right now.
    pub fn lookup(self: *const Pictures, name: []const u8) Lookup {
        if (self.entries.get(name)) |entry| return .{ .ready = .{ .texture = entry.texture, .width = entry.width, .height = entry.height } };
        if (self.queue.isMissing(name)) return .missing;
        return .pending;
    }

    /// Decodes up to `budget` queued names through `real` and uploads each
    /// into its own SDL_GPUTexture (R8G8B8A8_UNORM, sampler usage) - a
    /// transfer buffer written once, then a copy pass on its own command
    /// buffer, submitted at once. Call once per frame; a name whose decode
    /// fails (no picture, or one somehow too big for `decode_max_side`) is
    /// marked missing (PictureQueue) and never retried.
    pub fn pump(self: *Pictures, real: *RealBridge, device: *anyopaque, budget: usize) void {
        if (self.queue.pendingCount() == 0) return;
        self.device = device;
        var names_buffer: [max_pump_budget][]u8 = undefined;
        const take_budget = @min(budget, names_buffer.len);
        const names = self.queue.take(names_buffer[0..take_budget]);
        var pixels: [@as(usize, @intCast(decode_max_side)) * @as(usize, @intCast(decode_max_side)) * 4]u8 = undefined;
        for (names) |name| {
            defer self.allocator.free(name);
            const picture: Picture = real.objectPicture(name, &pixels, decode_max_side) orelse {
                self.queue.markMissing(name);
                continue;
            };
            const texture = self.upload(device, picture) orelse {
                self.queue.markMissing(name);
                continue;
            };
            const owned = self.allocator.dupe(u8, name) catch {
                releaseTexture(device, texture);
                continue;
            };
            self.entries.put(self.allocator, owned, .{ .texture = texture, .width = picture.width, .height = picture.height }) catch {
                releaseTexture(device, texture);
                self.allocator.free(owned);
            };
        }
    }

    fn upload(self: *Pictures, device: *anyopaque, picture: Picture) ?*sdl3.c.SDL_GPUTexture {
        _ = self;
        const gpu: *sdl3.c.SDL_GPUDevice = @ptrCast(@alignCast(device));
        const width: u32 = @intCast(picture.width);
        const height: u32 = @intCast(picture.height);
        const texture = sdl3.c.SDL_CreateGPUTexture(gpu, &.{
            .type = sdl3.c.SDL_GPU_TEXTURETYPE_2D,
            .format = sdl3.c.SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
            .usage = sdl3.c.SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .width = width,
            .height = height,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .sample_count = sdl3.c.SDL_GPU_SAMPLECOUNT_1,
            .props = 0,
        }) orelse return null;

        // No errdefer below: this function returns a plain optional, not an
        // error union, so a failure past this point releases the texture by
        // hand on every remaining path rather than leaking it.
        const transfer = sdl3.c.SDL_CreateGPUTransferBuffer(gpu, &.{
            .usage = sdl3.c.SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = @intCast(picture.bytes.len),
            .props = 0,
        }) orelse {
            sdl3.c.SDL_ReleaseGPUTexture(gpu, texture);
            return null;
        };
        defer sdl3.c.SDL_ReleaseGPUTransferBuffer(gpu, transfer);

        const mapped = sdl3.c.SDL_MapGPUTransferBuffer(gpu, transfer, false) orelse {
            sdl3.c.SDL_ReleaseGPUTexture(gpu, texture);
            return null;
        };
        const dest: [*]u8 = @ptrCast(mapped);
        @memcpy(dest[0..picture.bytes.len], picture.bytes);
        sdl3.c.SDL_UnmapGPUTransferBuffer(gpu, transfer);

        const command_buffer = sdl3.c.SDL_AcquireGPUCommandBuffer(gpu) orelse {
            sdl3.c.SDL_ReleaseGPUTexture(gpu, texture);
            return null;
        };
        const copy_pass = sdl3.c.SDL_BeginGPUCopyPass(command_buffer) orelse {
            _ = sdl3.c.SDL_SubmitGPUCommandBuffer(command_buffer);
            sdl3.c.SDL_ReleaseGPUTexture(gpu, texture);
            return null;
        };
        sdl3.c.SDL_UploadToGPUTexture(copy_pass, &.{
            .transfer_buffer = transfer,
            .offset = 0,
            .pixels_per_row = width,
            .rows_per_layer = height,
        }, &.{
            .texture = texture,
            .mip_level = 0,
            .layer = 0,
            .x = 0,
            .y = 0,
            .z = 0,
            .w = width,
            .h = height,
            .d = 1,
        }, false);
        sdl3.c.SDL_EndGPUCopyPass(copy_pass);
        _ = sdl3.c.SDL_SubmitGPUCommandBuffer(command_buffer);
        return texture;
    }
};

fn releaseTexture(device: *anyopaque, texture: *sdl3.c.SDL_GPUTexture) void {
    const gpu: *sdl3.c.SDL_GPUDevice = @ptrCast(@alignCast(device));
    sdl3.c.SDL_ReleaseGPUTexture(gpu, texture);
}
