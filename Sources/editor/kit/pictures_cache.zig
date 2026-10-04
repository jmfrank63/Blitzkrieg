//! Per-name picture cache (D-29): an ordered request queue with per-frame
//! decode budget, backed by one SDL_GPU texture per decoded name, parameterised
//! on the caller's decoder callback - so the kit never knows what an engine
//! bridge is. MapEditor's own two cache instances (object pictures and tile
//! pictures) each hand `pump` a decoder that calls BkEditorObjectPicture or
//! BkEditorTilePicture (see `Sources/editor/app/pictures.zig`).
//!
//! The queue (names queued once, drained in request order, a `missing` set the
//! queue refuses to re-queue until `clear`) is pure std and runs under its own
//! tests; the texture ladder uploads an RGBA8 image into a new SDL_GPU texture
//! and surrenders it from `clear`/`deinit`.
const std = @import("std");
const sdl3 = @import("sdl3");

/// One decoded picture a decoder callback returned, pointing into the caller's
/// own scratch buffer: RGBA8 top row first, `width * height * 4` of it. The
/// slice is valid only until the next decode writes the buffer again, so the
/// cache uploads the pixels into its texture before the next pump iteration.
pub const Decoded = struct {
    width: i32,
    height: i32,
    bytes: []const u8,
};

/// A decoder: given a queued name and a scratch pixel buffer sized for
/// `max_side * max_side * 4`, write the decoded RGBA8 into the buffer and
/// return a `Decoded` whose `bytes` points into it, or null when the name
/// has no picture (which the queue remembers as missing so it is never
/// retried).
pub const DecoderFn = *const fn (ctx: *anyopaque, name: []const u8, pixel_buffer: []u8, max_side: i32) ?Decoded;

/// What `lookup` reports for a name: nothing decoded yet, decoded to nothing
/// (no picture, or one that would not fit), or a ready texture.
pub const Lookup = union(enum) {
    pending,
    missing,
    ready: struct { texture: *sdl3.c.SDL_GPUTexture, width: i32, height: i32 },
};

/// Decode budget's own cap, matching MapEditor's `panels.picture_pump_budget`
/// headroom; the caller passes a smaller `budget` to `pump` and this only
/// bounds `pump`'s own stack buffer.
pub const max_pump_budget: usize = 32;

/// The side the decoder may upscale to (within its own 8..256 contract),
/// matched to the palette's 48x48 row picture. `pump`'s own scratch buffer
/// is sized for this.
pub const default_decode_max_side: i32 = 64;

const Entry = struct {
    texture: *sdl3.c.SDL_GPUTexture,
    width: i32,
    height: i32,
};

/// A reusable queue: a name is queued once, served in request order, and a
/// name reported `markMissing` (no shipped picture, or one that would not
/// fit) is never queued again until `clear` (a mod switch, D-26, drops every
/// name so the new mod's objects get a fresh try).
pub const PictureQueue = struct {
    allocator: std.mem.Allocator,
    missing: std.StringHashMapUnmanaged(void) = .empty,
    queue: std.ArrayListUnmanaged([]u8) = .empty,
    queued: std.StringHashMapUnmanaged(void) = .empty,

    pub fn init(allocator: std.mem.Allocator) PictureQueue {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *PictureQueue) void {
        self.clear();
        self.missing.deinit(self.allocator);
        self.queue.deinit(self.allocator);
        self.queued.deinit(self.allocator);
        self.* = undefined;
    }

    /// Forgets every queued and missing name - a mod switch (D-26): the new
    /// mod's objects may reuse a name with a different icon, or none at all.
    pub fn clear(self: *PictureQueue) void {
        var it = self.missing.keyIterator();
        while (it.next()) |key| self.allocator.free(key.*);
        self.missing.clearAndFree(self.allocator);
        for (self.queue.items) |name| self.allocator.free(name);
        self.queue.clearAndFree(self.allocator);
        self.queued.clearAndFree(self.allocator);
    }

    pub fn isMissing(self: *const PictureQueue, name: []const u8) bool {
        return self.missing.contains(name);
    }

    pub fn pendingCount(self: *const PictureQueue) usize {
        return self.queue.items.len;
    }

    /// Queues `name` once: already queued, or already marked missing, is a
    /// no-op. The caller (the texture cache) must also check its own
    /// resolved names before calling - this queue only knows about names
    /// still pending or already missing.
    pub fn request(self: *PictureQueue, name: []const u8) void {
        if (self.missing.contains(name)) return;
        if (self.queued.contains(name)) return;
        const owned = self.allocator.dupe(u8, name) catch return;
        self.queued.put(self.allocator, owned, {}) catch {
            self.allocator.free(owned);
            return;
        };
        self.queue.append(self.allocator, owned) catch {
            _ = self.queued.remove(owned);
            self.allocator.free(owned);
        };
    }

    /// Removes and returns up to `buffer.len` names from the front of the
    /// queue, in request order. Ownership of each returned name passes to
    /// the caller, who reports back with `markMissing` on a failed decode
    /// and frees the name either way; a name not marked missing is simply
    /// forgotten (its ready texture, if any, is the caller's own cache to
    /// keep).
    pub fn take(self: *PictureQueue, buffer: [][]u8) [][]u8 {
        var count: usize = 0;
        while (count < buffer.len and self.queue.items.len != 0) : (count += 1) {
            const name = self.queue.orderedRemove(0);
            _ = self.queued.remove(name);
            buffer[count] = name;
        }
        return buffer[0..count];
    }

    /// Marks `name` (already taken) as missing - never queued again until
    /// `clear`.
    pub fn markMissing(self: *PictureQueue, name: []const u8) void {
        if (self.missing.contains(name)) return;
        const owned = self.allocator.dupe(u8, name) catch return;
        self.missing.put(self.allocator, owned, {}) catch self.allocator.free(owned);
    }
};

/// The texture cache: `request` queues a name once; `pump` drains a budget of
/// the queue each frame, decoding through the caller's callback and uploading
/// into its own texture; `lookup` reports what a row should draw right now.
/// `clear` (a mod switch, D-26) and `deinit` (shutdown) release every texture.
pub const Cache = struct {
    allocator: std.mem.Allocator,
    /// The engine's SDL_GPUDevice, learned from `pump`'s own argument -
    /// `request`/`lookup`/`clear` need no device, and `init` is called before
    /// the first frame has one to give `pump`. Textures are released against
    /// whichever device most recently uploaded one, which is always the
    /// session's single device.
    device: ?*anyopaque = null,
    /// Ready textures only - a name whose decode failed never lands here
    /// (see `queue.isMissing` instead), so `deinit`/`clear` only ever
    /// release real textures.
    entries: std.StringHashMapUnmanaged(Entry) = .empty,
    queue: PictureQueue,
    decode_max_side: i32,

    pub fn init(allocator: std.mem.Allocator) Cache {
        return initWith(allocator, default_decode_max_side);
    }

    pub fn initWith(allocator: std.mem.Allocator, decode_max_side: i32) Cache {
        return .{ .allocator = allocator, .queue = PictureQueue.init(allocator), .decode_max_side = decode_max_side };
    }

    pub fn deinit(self: *Cache) void {
        self.clear();
        self.entries.deinit(self.allocator);
        self.queue.deinit();
        self.* = undefined;
    }

    /// Releases every texture and forgets every cached, queued and missing
    /// name.
    pub fn clear(self: *Cache) void {
        var it = self.entries.iterator();
        while (it.next()) |kv| {
            if (self.device) |device| releaseTexture(device, kv.value_ptr.texture);
            self.allocator.free(kv.key_ptr.*);
        }
        self.entries.clearAndFree(self.allocator);
        self.queue.clear();
    }

    /// Queues `name` once: already cached (ready or missing) or already
    /// queued is a no-op, so a group redrawn every frame does not re-queue
    /// its own rows every frame.
    pub fn request(self: *Cache, name: []const u8) void {
        if (self.entries.contains(name)) return;
        self.queue.request(name);
    }

    /// What a row should draw for `name` right now.
    pub fn lookup(self: *const Cache, name: []const u8) Lookup {
        if (self.entries.get(name)) |entry| return .{ .ready = .{ .texture = entry.texture, .width = entry.width, .height = entry.height } };
        if (self.queue.isMissing(name)) return .missing;
        return .pending;
    }

    /// Decodes up to `budget` queued names through `decoder` (with its opaque
    /// `ctx`) and uploads each into its own SDL_GPUTexture (R8G8B8A8_UNORM,
    /// sampler usage) - a transfer buffer written once, then a copy pass on
    /// its own command buffer, submitted at once. Call once per frame; a
    /// name whose decoder returns null is marked missing and never retried.
    pub fn pump(self: *Cache, decoder: DecoderFn, ctx: *anyopaque, device: *anyopaque, budget: usize) void {
        if (self.queue.pendingCount() == 0) return;
        self.device = device;
        var names_buffer: [max_pump_budget][]u8 = undefined;
        const take_budget = @min(budget, names_buffer.len);
        const names = self.queue.take(names_buffer[0..take_budget]);
        var pixels: [@as(usize, @intCast(default_decode_max_side)) * @as(usize, @intCast(default_decode_max_side)) * 4]u8 = undefined;
        for (names) |name| {
            defer self.allocator.free(name);
            const decoded: Decoded = decoder(ctx, name, &pixels, self.decode_max_side) orelse {
                self.queue.markMissing(name);
                continue;
            };
            const texture = upload(device, decoded) orelse {
                self.queue.markMissing(name);
                continue;
            };
            const owned = self.allocator.dupe(u8, name) catch {
                releaseTexture(device, texture);
                continue;
            };
            self.entries.put(self.allocator, owned, .{ .texture = texture, .width = decoded.width, .height = decoded.height }) catch {
                releaseTexture(device, texture);
                self.allocator.free(owned);
            };
        }
    }
};

fn upload(device: *anyopaque, picture: Decoded) ?*sdl3.c.SDL_GPUTexture {
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

fn releaseTexture(device: *anyopaque, texture: *sdl3.c.SDL_GPUTexture) void {
    const gpu: *sdl3.c.SDL_GPUDevice = @ptrCast(@alignCast(device));
    sdl3.c.SDL_ReleaseGPUTexture(gpu, texture);
}

test "PictureQueue: a name is queued once, served in request order" {
    var queue = PictureQueue.init(std.testing.allocator);
    defer queue.deinit();
    queue.request("b");
    queue.request("a");
    queue.request("b"); // already queued: not requeued, not duplicated
    try std.testing.expectEqual(@as(usize, 2), queue.pendingCount());
    var buffer: [8][]u8 = undefined;
    const taken = queue.take(&buffer);
    defer for (taken) |name| std.testing.allocator.free(name);
    try std.testing.expectEqual(@as(usize, 2), taken.len);
    try std.testing.expectEqualStrings("b", taken[0]);
    try std.testing.expectEqualStrings("a", taken[1]);
    try std.testing.expectEqual(@as(usize, 0), queue.pendingCount());
}

test "PictureQueue: take serves at most the given budget, the rest waits for the next frame" {
    var queue = PictureQueue.init(std.testing.allocator);
    defer queue.deinit();
    queue.request("a");
    queue.request("b");
    queue.request("c");
    var buffer: [2][]u8 = undefined;
    const first = queue.take(&buffer);
    try std.testing.expectEqual(@as(usize, 2), first.len);
    try std.testing.expectEqualStrings("a", first[0]);
    try std.testing.expectEqualStrings("b", first[1]);
    for (first) |name| std.testing.allocator.free(name);
    try std.testing.expectEqual(@as(usize, 1), queue.pendingCount());
    const second = queue.take(&buffer);
    try std.testing.expectEqual(@as(usize, 1), second.len);
    try std.testing.expectEqualStrings("c", second[0]);
    for (second) |name| std.testing.allocator.free(name);
}

test "PictureQueue: a name marked missing is never queued again until clear" {
    var queue = PictureQueue.init(std.testing.allocator);
    defer queue.deinit();
    queue.request("x");
    var buffer: [1][]u8 = undefined;
    const taken = queue.take(&buffer);
    try std.testing.expectEqual(@as(usize, 1), taken.len);
    queue.markMissing(taken[0]);
    std.testing.allocator.free(taken[0]);
    try std.testing.expect(queue.isMissing("x"));
    queue.request("x"); // already missing: never retried
    try std.testing.expectEqual(@as(usize, 0), queue.pendingCount());
    queue.clear();
    try std.testing.expect(!queue.isMissing("x"));
    queue.request("x");
    try std.testing.expectEqual(@as(usize, 1), queue.pendingCount());
}
