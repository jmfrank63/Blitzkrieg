//! MapEditor's two picture caches: palette object pictures (D-29) and the
//! Brush's tile picker (03-15 gap fix). Each wraps a `kit.pictures_cache.Cache`
//! and plugs in a decoder callback that reaches the engine through the real
//! bridge - BkEditorObjectPicture by object name, BkEditorTilePicture by tile
//! index decoded from the panels_logic key. The queue, texture upload, budget
//! pump and clear/deinit live in the kit; this file is only the MapEditor-
//! shaped surface callers already use (`Pictures.init`, `.initFor(.tile)`,
//! `.request(name)`, `.pump(real, device, budget)`, `.lookup(name)`,
//! `.clear()`, `.deinit()`).
const std = @import("std");
const c_bridge = @import("c_bridge.zig");
const kit = @import("editor_kit");
const logic = @import("panels_logic.zig");

const RealBridge = c_bridge.RealBridge;
const Cache = kit.pictures_cache.Cache;

/// The side the engine's own decoders may upscale to - matched to
/// `kit.pictures_cache.default_decode_max_side` (64).
pub const decode_max_side: i32 = kit.pictures_cache.default_decode_max_side;

/// What `lookup` reports for a name - re-export from the kit so MapEditor
/// callers keep seeing `pictures.Lookup`.
pub const Lookup = kit.pictures_cache.Lookup;

/// What a cache's names are, and so which bridge call its decoder runs.
pub const Source = enum {
    /// Object names from the catalogue: BkEditorObjectPicture (D-29).
    object,
    /// Tile indices in decimal: BkEditorTilePicture (03-15 gap fix).
    tile,
};

pub const Pictures = struct {
    cache: Cache,
    source: Source,

    pub fn init(allocator: std.mem.Allocator) Pictures {
        return initFor(allocator, .object);
    }

    pub fn initFor(allocator: std.mem.Allocator, source: Source) Pictures {
        return .{ .cache = Cache.init(allocator), .source = source };
    }

    pub fn deinit(self: *Pictures) void {
        self.cache.deinit();
        self.* = undefined;
    }

    pub fn clear(self: *Pictures) void {
        self.cache.clear();
    }

    pub fn request(self: *Pictures, name: []const u8) void {
        self.cache.request(name);
    }

    pub fn lookup(self: *const Pictures, name: []const u8) Lookup {
        return self.cache.lookup(name);
    }

    pub fn pump(self: *Pictures, real: *RealBridge, device: *anyopaque, budget: usize) void {
        const decoder: kit.pictures_cache.DecoderFn = switch (self.source) {
            .object => &decodeObject,
            .tile => &decodeTile,
        };
        self.cache.pump(decoder, @ptrCast(real), device, budget);
    }

    /// How many names are waiting for a decode this frame - mirrors the
    /// pre-S02 panels.zig call-site `state.tile_pictures.queue.pendingCount()`,
    /// which used to reach straight into the (now kit-side) queue.
    pub fn pendingCount(self: *const Pictures) usize {
        return self.cache.queue.pendingCount();
    }
};

fn decodeObject(ctx: *anyopaque, name: []const u8, pixel_buffer: []u8, max_side: i32) ?kit.pictures_cache.Decoded {
    const real: *RealBridge = @ptrCast(@alignCast(ctx));
    const picture = real.objectPicture(name, pixel_buffer, max_side) orelse return null;
    return .{ .width = picture.width, .height = picture.height, .bytes = picture.bytes };
}

fn decodeTile(ctx: *anyopaque, name: []const u8, pixel_buffer: []u8, max_side: i32) ?kit.pictures_cache.Decoded {
    const real: *RealBridge = @ptrCast(@alignCast(ctx));
    const tile = logic.tileFromKey(name) orelse return null;
    const picture = real.tilePicture(tile, pixel_buffer, max_side) orelse return null;
    return .{ .width = picture.width, .height = picture.height, .bytes = picture.bytes };
}
