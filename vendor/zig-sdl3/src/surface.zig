const blend_mode = @import("blend_mode.zig");
const c = @import("c");
const errors = @import("errors.zig");
const io_stream = @import("io_stream.zig");
const pixels = @import("pixels.zig");
const properties = @import("properties.zig");
const rect = @import("rect.zig");
const std = @import("std");

/// The flags on an SDL Surface.
///
/// ## Remarks
/// These are generally considered read-only.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Flags = struct {
    /// Surface uses preallocated pixel memory.
    preallocated: bool,
    /// Surface needs to be locked to access pixels.
    lock_needed: bool,
    /// Surface is currently locked.
    locked: bool,
    /// Surface uses pixel memory allocated with aligned allocator.
    simd_aligned: bool,

    /// Convert from an SDL value.
    pub fn fromSdl(flags: c.SDL_SurfaceFlags) Flags {
        return .{
            .preallocated = (flags & c.SDL_SURFACE_PREALLOCATED) != 0,
            .lock_needed = (flags & c.SDL_SURFACE_LOCK_NEEDED) != 0,
            .locked = (flags & c.SDL_SURFACE_LOCKED) != 0,
            .simd_aligned = (flags & c.SDL_SURFACE_SIMD_ALIGNED) != 0,
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: Flags) c.SDL_SurfaceFlags {
        return (if (self.preallocated) @as(c.SDL_SurfaceFlags, c.SDL_SURFACE_PREALLOCATED) else 0) |
            (if (self.lock_needed) @as(c.SDL_SurfaceFlags, c.SDL_SURFACE_LOCK_NEEDED) else 0) |
            (if (self.locked) @as(c.SDL_SurfaceFlags, c.SDL_SURFACE_LOCKED) else 0) |
            (if (self.simd_aligned) @as(c.SDL_SurfaceFlags, c.SDL_SURFACE_SIMD_ALIGNED) else 0) |
            0;
    }
};

/// The flip mode.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const FlipMode = struct {
    /// Flip horizontally.
    horizontal: bool = false,
    /// Flip vertically.
    vertical: bool = false,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_FlipMode) FlipMode {
        var ret: FlipMode = .{};
        if (value & c.SDL_FLIP_HORIZONTAL != 0)
            ret.horizontal = true;
        if (value & c.SDL_FLIP_VERTICAL != 0)
            ret.vertical = true;
        return ret;
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: FlipMode) c.SDL_FlipMode {
        var ret: c_uint = c.SDL_FLIP_NONE;
        if (self.horizontal)
            ret |= c.SDL_FLIP_HORIZONTAL;
        if (self.vertical)
            ret |= c.SDL_FLIP_VERTICAL;
        return ret;
    }
};

/// The scaling mode.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const ScaleMode = enum(c.SDL_ScaleMode) {
    /// Nearest pixel sampling.
    nearest = c.SDL_SCALEMODE_NEAREST,
    /// Linear pixel sampling.
    linear = c.SDL_SCALEMODE_LINEAR,
    /// Nearest pixel sampling with improved scaling for pixel art, available since SDL 3.4.0
    pixel_art = c.SDL_SCALEMODE_PIXELART,
};

/// A collection of pixels used in software blitting.
///
/// ## Remarks
/// Pixels are arranged in memory in rows, with the top row first.
/// Each row occupies an amount of memory given by the pitch (sometimes known as the row stride in non-SDL APIs).
///
/// Within each row, pixels are arranged from left to right until the width is reached.
/// Each pixel occupies a number of bits appropriate for its format, with most formats representing each pixel as one or more whole bytes (in some indexed formats,
/// instead multiple pixels are packed into each byte), and a byte order given by the format.
/// After encoding all pixels, any remaining bytes to reach the pitch are used as padding to reach a desired alignment, and have undefined contents.
///
/// When a surface holds YUV format data, the planes are assumed to be contiguous without padding between them,
/// e.g. a 32x32 surface in NV12 format with a pitch of 32 would consist of 32x32 bytes of Y plane followed by 32x16 bytes of UV plane.
///
/// When a surface holds MJPG format data, pixels points at the compressed JPEG image and pitch is the length of that data.
///
/// Note that the reference count will be `1` when initialized, and decremented on each call to `surface.Surface.deinit()`.
/// An application is free to increment the reference count when an additional uses is added, just be sure it has a corresponding deinit call.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Surface = extern struct {
    value: *c.SDL_Surface,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(Surface) == @sizeOf(*c.SDL_Surface));
    }

    /// Surface properties.
    ///
    /// ## Version
    /// This structure is provided by zig-sdl3.
    pub const Properties = struct {
        /// For HDR10 and floating point surfaces, this defines the value of 100% diffuse white, with higher values being displayed in the High Dynamic Range headroom.
        /// This defaults to `203` for HDR10 surfaces and `1` for floating point surfaces.
        sdr_white_point: ?f32 = null,
        /// For HDR10 and floating point surfaces, this defines the maximum dynamic range used by the content, in terms of the SDR white point.
        /// This defaults to `0`, which disables tone mapping.
        hdr_headroom: ?f32 = null,
        /// The tone mapping operator used when compressing from a surface with high dynamic range to another with lower dynamic range.
        /// Currently this supports "chrome", which uses the same tone mapping that Chrome uses for HDR content, the form "*=N",
        /// where N is a floating point scale factor applied in linear space, and "none", which disables tone mapping.
        /// This defaults to "chrome".
        tonemap_operator: ?[:0]const u8 = null,
        /// The hotspot pixel offset from the left edge of the image, if this surface is being used as a cursor.
        hotspot_x: ?i64 = null,
        /// The hotspot pixel offset from the top edge of the image, if this surface is being used as a cursor.
        hotspot_y: ?i64 = null,
        /// he number of degrees a surface's data is meant to be rotated clockwise to make the image right-side up.
        /// Default 0. This is used by the camera API, if a mobile device is oriented differently than what its camera provides
        /// (i.e. - the camera always provides portrait images but the phone is being held in landscape orientation).
        /// Since SDL 3.4.0.
        rotation: ?f32 = null,

        /// Convert from an SDL group.
        pub fn fromSdl(value: properties.Group) Properties {
            return .{
                .sdr_white_point = if (value.get(c.SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT)) |val| val.float else null,
                .hdr_headroom = if (value.get(c.SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT)) |val| val.float else null,
                .tonemap_operator = if (value.get(c.SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING)) |val| val.string else null,
                .hotspot_x = if (value.get(c.SDL_PROP_SURFACE_HOTSPOT_X_NUMBER)) |val| val.number else null,
                .hotspot_y = if (value.get(c.SDL_PROP_SURFACE_HOTSPOT_Y_NUMBER)) |val| val.number else null,
                .rotation = if (value.get(c.SDL_PROP_SURFACE_ROTATION_FLOAT)) |val| val.float else null,
            };
        }

        /// Convert to an SDL group.
        pub fn toSdl(self: Properties, value: properties.Group) !void {
            if (self.sdr_white_point) |val|
                try value.set(c.SDL_PROP_SURFACE_SDR_WHITE_POINT_FLOAT, .{ .float = val });
            if (self.hdr_headroom) |val|
                try value.set(c.SDL_PROP_SURFACE_HDR_HEADROOM_FLOAT, .{ .float = val });
            if (self.tonemap_operator) |val|
                try value.set(c.SDL_PROP_SURFACE_TONEMAP_OPERATOR_STRING, .{ .string = val });
            if (self.hotspot_x) |val|
                try value.set(c.SDL_PROP_SURFACE_HOTSPOT_X_NUMBER, .{ .number = val });
            if (self.hotspot_y) |val|
                try value.set(c.SDL_PROP_SURFACE_HOTSPOT_Y_NUMBER, .{ .number = val });
            if (self.rotation) |val|
                try value.set(c.SDL_PROP_SURFACE_ROTATION_FLOAT, .{ .float = val });
        }
    };

    /// Add an alternate version of a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to update.
    /// * `image`: The alternate image to associate with the surface.
    ///
    /// ## Remarks
    /// This function adds an alternate version of this surface, usually used for content with high DPI representations like cursors or icons.
    /// The size, format, and content do not need to match the original surface, and these alternate versions will not be updated when the original surface changes.
    ///
    /// This function adds a reference to the alternate version, so you should call `surface.Surface.deinit()` on the image after this call.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn addAlternateImage(
        self: Surface,
        image: Surface,
    ) !void {
        const ret = c.SDL_AddSurfaceAlternateImage(
            self.value,
            image.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Performs a fast blit from the source surface to the destination surface with clipping.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to be copied from.
    /// * `area_to_copy`: Rectangle to be copied, or `null` for the entire surface.
    /// * `dest`: Blit target surface.
    /// * `point_to_copy_to`: Point to copy to the destination surface, or `(0, 0)` if `null`. The width and height from `area_to_copy` are used.
    ///
    /// ## Remarks
    /// If either `area_to_copy` or `point_to_copy_to` are `null`, the entire surface (`self` or `dest`) is copied while ensuring clipping to `dest.clip_rect`.
    ///
    /// The blit function should not be called on a locked surface.
    ///
    /// The blit semantics for surfaces with and without blending and colorkey are defined as follows:
    /// * RGBA->RGB:
    ///    - Source surface blend mode set to `blend_mode.Mode.blend`:
    ///     alpha-blend (using the source alpha-channel and per-surface alpha)
    ///     source color key ignored.
    ///   - Source surface blend mode set to `blend_mode.Mode.none`:
    ///     copy RGB.
    ///     if source color key set, only copy the pixels that do not match the
    ///     RGB values of the source color key, ignoring alpha in the
    ///     comparison.
    /// * RGB->RGBA:
    ///    - Source surface blend mode set to  `blend_mode.Mode.blend`:
    ///       alpha-blend (using the source per-surface alpha)
    ///    - Source surface blend mode set to `blend_mode.Mode.none`:
    ///       copy RGB, set destination alpha to source per-surface alpha value.
    ///     both:
    ///       if source color set, only copy the pixels that do not match the
    ///       source color key.
    ///
    /// * RGBA->RGBA:
    ///     - Source surface blend mode set to  `blend_mode.Mode.blend`:
    ///       alpha-blend (using the source alpha-channel and per-surface alpha)
    ///       source color ignored.
    ///     - Source surface blend mode set to `blend_mode.Mode.none`:
    ///       copy all of RGBA to the destination.
    ///       if source color set, only copy the pixels that do not match the
    ///       RGB values of the source color key, ignoring alpha in the
    ///       comparison.
    ///
    /// * RGB->RGB:
    ///     - Source surface blend mode set to  `blend_mode.Mode.blend`:
    ///       alpha-blend (using the source per-surface alpha)
    ///     - Source surface blend mode set to `blend_mode.Mode.none`:
    ///       copy RGB.
    ///     both:
    ///       if source color set, only copy the pixels that do not match the
    ///       source color key.
    ///
    /// ## Thread Safety
    /// Only one thread should be using the `self` and `dest` surfaces at any given time.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn blit(
        self: Surface,
        area_to_copy: ?rect.IRect,
        dest: Surface,
        point_to_copy_to: ?rect.IPoint,
    ) !void {
        const area_to_copy_sdl: c.SDL_Rect = if (area_to_copy) |val| val.toSdl() else undefined;
        const point_to_copy_to_sdl: c.SDL_Rect = if (point_to_copy_to) |val| .{
            .x = @intCast(val.x),
            .y = @intCast(val.y),
            .w = undefined,
            .h = undefined,
        } else undefined;
        const ret = c.SDL_BlitSurface(
            self.value,
            if (area_to_copy != null) &area_to_copy_sdl else null,
            dest.value,
            if (point_to_copy_to != null) &point_to_copy_to_sdl else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Perform a scaled blit using the 9-grid algorithm to a destination surface, which may be of a different format.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to be copied from.
    /// * `area_to_copy_from`: Rectangle to be copied, or `null` for the entire surface.
    /// * `left_width`: The width in pixels of the left corners in `area_to_copy_from`.
    /// * `right_width`: The width in pixels of the right corners in `area_to_copy_from`.
    /// * `top_height`: The height in pixels of the top corners in `area_to_copy_from`.
    /// * `bottom_height`: The height in pixels of the bottom corners in `area_to_copy_from`.
    /// * `scale_amount`: Scale used to transform corner of `area_to_copy` to `area_to_copy_to` or `null` for unscaled.
    /// * `scale_mode`: The scaling mode algorithm to be used.
    /// * `dest`: Blit target surface.
    /// * `area_to_copy_to`: Target area to copy to, or `null` to copy the whole thing.
    ///
    /// ## Remarks
    /// The pixels in the source surface are split into a 3x3 grid, using the different corner sizes for each corner, and the sides and center making up the remaining pixels.
    /// The corners are then scaled using scale and fit into the corners of the destination rectangle.
    /// The sides and center are then stretched into place to cover the remaining destination rectangle.
    ///
    /// ## Thread Safety
    /// Only one thread should be using the `self` and `dest` surfaces at any given time.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn blit9Grid(
        self: Surface,
        area_to_copy_from: ?rect.IRect,
        left_width: usize,
        right_width: usize,
        top_height: usize,
        bottom_height: usize,
        scale_amount: ?f32,
        scale_mode: ScaleMode,
        dest: Surface,
        area_to_copy_to: ?rect.IRect,
    ) !void {
        const area_to_copy_from_sdl: c.SDL_Rect = if (area_to_copy_from) |val| val.toSdl() else undefined;
        const area_to_copy_to_sdl: c.SDL_Rect = if (area_to_copy_to) |val| val.toSdl() else undefined;
        const ret = c.SDL_BlitSurface9Grid(
            self.value,
            if (area_to_copy_from != null) &area_to_copy_from_sdl else null,
            @intCast(left_width),
            @intCast(right_width),
            @intCast(top_height),
            @intCast(bottom_height),
            if (scale_amount) |val| val else 0,
            @bitCast(@intFromEnum(scale_mode)),
            dest.value,
            if (area_to_copy_to != null) &area_to_copy_to_sdl else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Perform a scaled blit to a destination surface, which may be of a different format.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to be copied from.
    /// * `area_to_copy_from`: Rectangle to be copied, or `null` for the entire surface.
    /// * `dest`: Blit target surface.
    /// * `area_to_copy_to`: Target area to copy to, or `null` to copy the whole thing.
    /// * `scale_mode`: The scaling mode algorithm to be used.
    ///
    /// ## Thread Safety
    /// Only one thread should be using the `self` and `dest` surfaces at any given time.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn blitScaled(
        self: Surface,
        area_to_copy_from: ?rect.IRect,
        dest: Surface,
        area_to_copy_to: ?rect.IRect,
        scale_mode: ScaleMode,
    ) !void {
        const area_to_copy_from_sdl: c.SDL_Rect = if (area_to_copy_from) |val| val.toSdl() else undefined;
        const area_to_copy_to_sdl: c.SDL_Rect = if (area_to_copy_to) |val| val.toSdl() else undefined;
        const ret = c.SDL_BlitSurfaceScaled(
            self.value,
            if (area_to_copy_from != null) &area_to_copy_from_sdl else null,
            dest.value,
            if (area_to_copy_to != null) &area_to_copy_to_sdl else null,
            @bitCast(@intFromEnum(scale_mode)),
        );
        return errors.wrapCallBool(ret);
    }

    /// Perform a tiled blit to a destination surface, which may be of a different format.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to be copied from.
    /// * `area_to_copy_from`: Rectangle to be copied, or `null` for the entire surface.
    /// * `dest`: Blit target surface.
    /// * `area_to_copy_to`: Target area to copy to, or `null` to copy the whole thing.
    ///
    /// ## Remarks
    /// The pixels in `area_to_copy_from` will be repeated as many times as needed to completely fill `area_to_copy_to`.
    ///
    /// ## Thread Safety
    /// Only one thread should be using the `self` and `dest` surfaces at any given time.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn blitTiled(
        self: Surface,
        area_to_copy_from: ?rect.IRect,
        dest: Surface,
        area_to_copy_to: ?rect.IRect,
    ) !void {
        const area_to_copy_from_sdl: c.SDL_Rect = if (area_to_copy_from) |val| val.toSdl() else undefined;
        const area_to_copy_to_sdl: c.SDL_Rect = if (area_to_copy_to) |val| val.toSdl() else undefined;
        const ret = c.SDL_BlitSurfaceTiled(
            self.value,
            if (area_to_copy_from != null) &area_to_copy_from_sdl else null,
            dest.value,
            if (area_to_copy_to != null) &area_to_copy_to_sdl else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Perform a scaled and tiled blit to a destination surface, which may be of a different format.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to be copied from.
    /// * `area_to_copy_from`: Rectangle to be copied, or `null` for the entire surface.
    /// * `dest`: Blit target surface.
    /// * `area_to_copy_to`: Target area to copy to, or `null` to copy the whole thing.
    ///
    /// ## Remarks
    /// The pixels in `area_to_copy_from` will be repeated as many times as needed to completely fill `area_to_copy_to`.
    ///
    /// ## Thread Safety
    /// Only one thread should be using the `self` and `dest` surfaces at any given time.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn blitTiledWithScale(
        self: Surface,
        area_to_copy_from: ?rect.IRect,
        scale_amount: f32,
        scale_mode: ScaleMode,
        dest: Surface,
        area_to_copy_to: ?rect.IRect,
    ) !void {
        const area_to_copy_from_sdl: c.SDL_Rect = if (area_to_copy_from) |val| val.toSdl() else undefined;
        const area_to_copy_to_sdl: c.SDL_Rect = if (area_to_copy_to) |val| val.toSdl() else undefined;
        const ret = c.SDL_BlitSurfaceTiledWithScale(
            self.value,
            if (area_to_copy_from != null) &area_to_copy_from_sdl else null,
            scale_amount,
            @bitCast(@intFromEnum(scale_mode)),
            dest.value,
            if (area_to_copy_to != null) &area_to_copy_to_sdl else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Perform low-level surface blitting only.
    /// ## Function Parameters
    /// * `self`: The surface to be copied from.
    /// * `area_to_copy_from`: Rectangle to be copied, or `null` for the entire surface.
    /// * `dest`: Blit target surface.
    /// * `area_to_copy_to`: Target area to copy to, or `null` to copy the whole thing.
    ///
    /// ## Remarks
    /// This is a semi-private blit function and it performs low-level surface blitting, assuming the input rectangles have already been clipped.
    ///
    /// ## Thread Safety
    /// Only one thread should be using the `self` and `dest` surfaces at any given time.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn blitUnchecked(
        self: Surface,
        area_to_copy_from: ?rect.IRect,
        dest: Surface,
        area_to_copy_to: ?rect.IRect,
    ) !void {
        const area_to_copy_from_sdl: c.SDL_Rect = if (area_to_copy_from) |val| val.toSdl() else undefined;
        const area_to_copy_to_sdl: c.SDL_Rect = if (area_to_copy_to) |val| val.toSdl() else undefined;
        const ret = c.SDL_BlitSurfaceUnchecked(
            self.value,
            if (area_to_copy_from != null) &area_to_copy_from_sdl else null,
            dest.value,
            if (area_to_copy_to != null) &area_to_copy_to_sdl else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Perform low-level surface scaled blitting only.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to be copied from.
    /// * `area_to_copy_from`: Rectangle to be copied, or `null` for the entire surface.
    /// * `dest`: Blit target surface.
    /// * `area_to_copy_to`: Target area to copy to, or `null` to copy the whole thing.
    /// * `scale_mode`: The scaling mode algorithm to be used.
    ///
    /// ## Remarks
    /// This is a semi-private function and it performs low-level surface blitting, assuming the input rectangles have already been clipped.
    ///
    /// ## Thread Safety
    /// Only one thread should be using the `self` and `dest` surfaces at any given time.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn blitUncheckedScaled(
        self: Surface,
        area_to_copy_from: ?rect.IRect,
        dest: Surface,
        area_to_copy_to: ?rect.IRect,
        scale_mode: ScaleMode,
    ) !void {
        const area_to_copy_from_sdl: c.SDL_Rect = if (area_to_copy_from) |val| val.toSdl() else undefined;
        const area_to_copy_to_sdl: c.SDL_Rect = if (area_to_copy_to) |val| val.toSdl() else undefined;
        const ret = c.SDL_BlitSurfaceUncheckedScaled(
            self.value,
            if (area_to_copy_from != null) &area_to_copy_from_sdl else null,
            dest.value,
            if (area_to_copy_to != null) &area_to_copy_to_sdl else null,
            @bitCast(@intFromEnum(scale_mode)),
        );
        return errors.wrapCallBool(ret);
    }

    /// Clear a surface with a specific color, with floating point precision.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to clear.
    /// * `color`: The components of the pixel, usually in the `0` to `1` range.
    ///
    /// ## Remarks
    /// This function handles all surface formats, and ignores any clip rectangle.
    ///
    /// If the surface is YUV, the color is assumed to be in the sRGB colorspace, otherwise the color is assumed to be in the colorspace of the suface.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn clear(
        self: Surface,
        color: pixels.FColor,
    ) !void {
        const ret = c.SDL_ClearSurface(
            self.value,
            color.r,
            color.g,
            color.b,
            color.a,
        );
        return errors.wrapCallBool(ret);
    }

    /// Copy an existing surface to a new surface of the specified format.
    ///
    /// ## Function Parameters
    /// * `self`: The existing surface to convert.
    /// * `format`: The new pixel format.
    ///
    /// ## Return Value
    /// Returns a new surface value.
    ///
    /// ## Remarks
    /// This function is used to optimize images for faster repeat blitting.
    /// This is accomplished by converting the original and storing the result as a new surface.
    /// The new, optimized surface can then be used as the source for future blits, making them faster.
    ///
    /// If you are converting to an indexed surface and want to map colors to a palette, you can use `surface.Surface.convertFormatAndColorspace()` instead.
    ///
    /// If the original surface has alternate images, the new surface will have a reference to them as well.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn convertFormat(
        self: Surface,
        format: pixels.Format,
    ) !Surface {
        const ret = c.SDL_ConvertSurface(
            self.value,
            @intFromEnum(format),
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Copy an existing surface to a new surface of the specified format and colorspace.
    ///
    /// ## Function Parameters
    /// * `self`: The existing surface to convert.
    /// * `format`: The new pixel format.
    /// * `palette`: An optional palette for indexed formats.
    /// * `colorspace`: The new colorspace.
    /// * `color_properties`: Optional additional color properties.
    ///
    /// ## Return Value
    /// Returns a new surface value.
    ///
    /// ## Remarks
    /// This function converts an existing surface to a new format and colorspace and returns the new surface.
    /// This will perform any pixel format and colorspace conversion needed.
    ///
    /// If the original surface has alternate images, the new surface will have a reference to them as well.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn convertFormatAndColorspace(
        self: Surface,
        format: pixels.Format,
        palette: ?pixels.Palette,
        colorspace: pixels.Colorspace,
        color_properties: ?properties.Group,
    ) !Surface {
        const ret = c.SDL_ConvertSurfaceAndColorspace(
            self.value,
            @intFromEnum(format),
            if (palette) |palette_val| palette_val.value else null,
            @intFromEnum(colorspace),
            if (color_properties) |val| val.value else 0,
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Create a palette and associate it with a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to update.
    ///
    /// ## Return Value
    /// Returns a new palette for the surface.
    /// You do not need to free the palette.
    ///
    /// ## Remarks
    /// This function creates a palette compatible with the provided surface.
    /// The palette is then returned for you to modify, and the surface will automatically use the new palette in future operations.
    /// You do not need to destroy the returned palette, it will be freed when the reference count reaches `0`, usually when the surface is destroyed.
    ///
    /// Bitmap surfaces (with format `pixels.Format.index_1_lsb` or `pixels.Format.index_1_msb`) will have the palette initialized with `0` as white and `1` as black.
    /// Other surfaces will get a palette initialized with white in every entry.
    ///
    /// If this function is called for a surface that already has a palette, a new palette will be created to replace it.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn createPalette(
        self: Surface,
    ) !pixels.Palette {
        const ret = c.SDL_CreateSurfacePalette(
            self.value,
        );
        return pixels.Palette{ .value = try errors.wrapCallNull(*c.SDL_Palette, ret) };
    }

    /// Free a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to free.
    ///
    /// ## Thread Safety
    /// No other thread should be using the surface when it is freed.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Surface,
    ) void {
        c.SDL_DestroySurface(
            self.value,
        );
    }

    /// Creates a new surface identical to the existing surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to duplicate.
    ///
    /// ## Return Value
    /// Returns a copy of the surface.
    ///
    /// ## Remarks
    /// If the original surface has alternate images, the new surface will have a reference to them as well.
    ///
    /// The returned surface should be freed with `surface.Surface.deinit()`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn duplicate(
        self: Surface,
    ) !Surface {
        const ret = c.SDL_DuplicateSurface(
            self.value,
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Perform a fast fill of a rectangle with a specific color.
    ///
    /// ## Function Parameters
    /// * `self`: The surface that is the drawing target.
    /// * `area`: The area to fill, or `null` for the whole surface.
    /// * `color`: The color to fill with.
    ///
    /// ## Remarks
    /// The `color` should be a pixel of the format used by the surface, and can be generated by `surface.Surface.mapRgb()` or `surface.Surface.mapRgba()`.
    /// If the color value contains an alpha component then the destination is simply filled with that alpha information, no blending takes place.
    ///
    /// If there is a clip rectangle set on the destination (set via `surface.Surface.setClipRect()`),
    /// then this function will fill based on the intersection of the clip rectangle and `area`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fillRect(
        self: Surface,
        area: ?rect.IRect,
        color: pixels.Pixel,
    ) !void {
        const area_sdl: c.SDL_Rect = if (area) |val| val.toSdl() else undefined;
        const ret = c.SDL_FillSurfaceRect(
            self.value,
            if (area != null) &area_sdl else null,
            color.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Perform a fast fill of a set of rectangles with a specific color.
    ///
    /// ## Function Parameters
    /// * `self`: The surface that is the drawing target.
    /// * `areas`: The areas to fill.
    /// * `color`: The color to fill with.
    ///
    /// ## Remarks
    /// The `color` should be a pixel of the format used by the surface, and can be generated by `surface.Surface.mapRgb()` or `surface.Surface.mapRgba()`.
    /// If the color value contains an alpha component then the destination is simply filled with that alpha information, no blending takes place.
    ///
    /// If there is a clip rectangle set on the destination (set via `surface.Surface.setClipRect()`),
    /// then this function will fill based on the intersection of the clip rectangle and `areas`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fillRects(
        self: Surface,
        rects: []const rect.IRect,
        color: pixels.Pixel,
    ) !void {
        const ret = c.SDL_FillSurfaceRects(
            self.value,
            @ptrCast(rects.ptr),
            @intCast(rects.len),
            color.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Flip a surface vertically or horizontally.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to flip.
    /// * `flip_mode`: The direction to flip.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn flip(
        self: Surface,
        flip_mode: FlipMode,
    ) !void {
        const ret = c.SDL_FlipSurface(
            self.value,
            flip_mode.toSdl(),
        );
        return errors.wrapCallBool(ret);
    }

    /// Get the additional alpha value used in blit operations.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns the current alpha value.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getAlphaMod(
        self: Surface,
    ) !u8 {
        var alpha: u8 = undefined;
        const ret = c.SDL_GetSurfaceAlphaMod(
            self.value,
            &alpha,
        );
        try errors.wrapCallBool(ret);
        return alpha;
    }

    /// Get the blend mode used for blit operations.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns the current blend mode value.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getBlendMode(
        self: Surface,
    ) !blend_mode.Mode {
        var mode: c.SDL_BlendMode = undefined;
        const ret = c.SDL_GetSurfaceBlendMode(
            self.value,
            &mode,
        );
        try errors.wrapCallBool(ret);
        return errors.wrapCallNull(blend_mode.Mode, blend_mode.Mode.fromSdl(mode));
    }

    /// Get the clipping rectangle for a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns the clipping rectangle for the surface.
    ///
    /// ## Remarks
    /// When surface is the destination of a blit, only the area within the clip rectangle is drawn into.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getClipRect(
        self: Surface,
    ) !rect.IRect {
        var val: c.SDL_Rect = undefined;
        const ret = c.SDL_GetSurfaceClipRect(
            self.value,
            &val,
        );
        try errors.wrapCallBool(ret);
        return rect.IRect.fromSdl(val);
    }

    /// Get the color key (transparent pixel) for a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns the transparent pixel color.
    ///
    /// ## Remarks
    /// The color key is a pixel of the format used by the surface, as generated by `surface.Surface.mapRgb()`.
    ///
    /// If the surface doesn't have color key enabled this function returns an error.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getColorKey(
        self: Surface,
    ) !pixels.Pixel {
        var key: u32 = undefined;
        const ret = c.SDL_GetSurfaceColorKey(
            self.value,
            &key,
        );
        try errors.wrapCallBool(ret);
        return pixels.Pixel{ .value = key };
    }

    /// Get the additional color value multiplied into blit operations.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns the color mod color.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getColorMod(
        self: Surface,
    ) !struct { r: u8, g: u8, b: u8 } {
        var r: u8 = undefined;
        var g: u8 = undefined;
        var b: u8 = undefined;
        const ret = c.SDL_GetSurfaceColorMod(
            self.value,
            &r,
            &g,
            &b,
        );
        try errors.wrapCallBool(ret);
        return .{ .r = r, .g = g, .b = b };
    }

    /// Get the colorspace used by a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns the colorspace.
    ///
    /// ## Remarks
    /// The colorspace defaults to `pixels.Colorspace.srgb_linear` for floating point formats, `pixels.Colorspace.hdr10` for 10-bit formats,
    /// `pixels.Colorspace.srgb` for other RGB surfaces and `pixels.Colorspace.bt709_full` for YUV textures.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getColorspace(
        self: Surface,
    ) ?pixels.Colorspace {
        const ret = c.SDL_GetSurfaceColorspace(
            self.value,
        );
        return pixels.Colorspace.fromSdl(ret);
    }

    /// Get the surface flags.
    ///
    /// ## Function Parameters
    /// * `self`: The surface.
    ///
    /// ## Return Value
    /// The flags of the surface.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getFlags(
        self: Surface,
    ) Flags {
        return Flags.fromSdl(self.value.flags);
    }

    /// Get the surface format.
    ///
    /// ## Function Parameters
    /// * `self`: The surface.
    ///
    /// ## Return Value
    /// The format of the surface.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getFormat(
        self: Surface,
    ) ?pixels.Format {
        return pixels.Format.fromSdl(self.value.format);
    }

    /// Get the surface height.
    ///
    /// ## Function Parameters
    /// * `self`: The surface.
    ///
    /// ## Return Value
    /// The height of the surface.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getHeight(
        self: Surface,
    ) usize {
        return @intCast(self.value.h);
    }

    /// Get a slice including all versions of a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns a slice of images.
    /// This should be freed with `free()`.
    ///
    /// ## Remarks
    /// This returns all versions of a surface, with the surface being queried as the first element in the returned array.
    ///
    /// Freeing the array of surfaces does not affect the surfaces in the array.
    /// They are still referenced by the surface being queried and will be cleaned up normally.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getImages(
        self: Surface,
    ) ![]Surface {
        var count: c_int = undefined;
        const ret = c.SDL_GetSurfaceImages(self.value, &count);
        return @as([*]Surface, @ptrCast(try errors.wrapCallNull(*[*c]c.SDL_Surface, ret)))[0..@intCast(count)];
    }

    /// Get the palette used by a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// The palette for the surface, if used.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPalette(
        self: Surface,
    ) ?pixels.Palette {
        const ret = c.SDL_GetSurfacePalette(
            self.value,
        );
        if (ret == null)
            return null;
        return pixels.Palette{ .value = ret };
    }

    /// Get the byte distance between rows of pixels.
    ///
    /// ## Function Parameters
    /// * `self`: The surface.
    ///
    /// ## Return Value
    /// The byte-size for a row of pixels.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getPitch(
        self: Surface,
    ) usize {
        return @intCast(self.value.pitch);
    }

    /// Get a slice to writable pixels.
    ///
    /// ## Function Parameters
    /// * `self`: The surface.
    ///
    /// ## Return Value
    /// This will only return the pixels if they are writeable.
    /// If the pixels are not writeable, `null` is returned.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getPixels(
        self: Surface,
    ) ?[]u8 {
        if (self.value.pixels) |pixel|
            return @as([*]u8, @ptrCast(pixel))[0..@intCast(self.value.h * self.value.pitch)];
        return null;
    }

    /// Get the properties associated with a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The structure to query.
    ///
    /// ## Return Value
    /// Returns the properties for a surface.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProperties(
        self: Surface,
    ) !Properties {
        const ret = c.SDL_GetSurfaceProperties(
            self.value,
        );
        const group = properties.Group{ .value = try errors.wrapCall(c.SDL_PropertiesID, ret, 0) };
        return Properties.fromSdl(group);
    }

    /// Get the reference count of the surface.
    ///
    /// ## Function Parameters
    /// * `self`: The texturesurface
    ///
    /// ## Return Value
    /// The reference count of the surface.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getRefCount(
        self: Surface,
    ) usize {
        return @intCast(self.value.refcount);
    }

    /// Get the surface width.
    ///
    /// ## Function Parameters
    /// * `self`: The surface.
    ///
    /// ## Return Value
    /// The width of the surface.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getWidth(
        self: Surface,
    ) usize {
        return @intCast(self.value.w);
    }

    /// Return whether a surface has alternate versions available.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns true if alternate versions are available or false otherwise.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn hasAlternateImage(
        self: Surface,
    ) bool {
        const ret = c.SDL_SurfaceHasAlternateImages(
            self.value,
        );
        return ret;
    }

    /// Returns whether the surface has a color key.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns true if the surface has a color key, false otherwise.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn hasColorKey(
        self: Surface,
    ) bool {
        const ret = c.SDL_SurfaceHasColorKey(
            self.value,
        );
        return ret;
    }

    /// Returns whether the surface is RLE enabled.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to query.
    ///
    /// ## Return Value
    /// Returns true if the surface is RLE enabled, false otherwise.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn hasRle(
        self: Surface,
    ) bool {
        const ret = c.SDL_SurfaceHasRLE(
            self.value,
        );
        return ret;
    }

    /// Increment the ref count of the surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface.
    ///
    /// ## Remarks
    /// All calls to this function must be matched with `surface.Surface.deinit()`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn incrementRefCount(
        self: Surface,
    ) void {
        self.value.refcount += 1;
    }

    /// Allocate a new surface with a specific pixel format.
    ///
    /// ## Function Paramters
    /// * `width`: The width of the surface.
    /// * `height`: The height of the surface.
    /// * `format`: Format for the new surface's pixel format.
    ///
    /// ## Return Value
    /// Return the newly created surface.
    ///
    /// ## Remarks
    /// The pixels of the new surface are initialized to zero.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        width: usize,
        height: usize,
        format: pixels.Format,
    ) !Surface {
        const ret = c.SDL_CreateSurface(
            @intCast(width),
            @intCast(height),
            @intFromEnum(format),
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Allocate a new surface with a specific pixel format and data in the format.
    ///
    /// ## Function Paramters
    /// * `width`: The width of the surface.
    /// * `height`: The height of the surface.
    /// * `format`: Format for the new surface's pixel format.
    /// * `pixel_data`: Existing pixel data. If present, it's length must be a multiple of the stride.
    ///
    /// ## Return Value
    /// Return the newly created surface.
    ///
    /// ## Remarks
    /// No copy is made of the pixel data.
    /// Pixel data is not managed automatically; you must free the surface before you free the pixel data.
    ///
    /// Pitch is the offset in bytes from one row of pixels to the next, e.g. width*4 for `pixels.Format.packed_rgba_8_8_8_8`.
    ///
    /// You may pass `null` for pixels to create a surface that you will fill in with valid values later.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initFrom(
        width: usize,
        height: usize,
        format: pixels.Format,
        pixel_data: ?[]const u8,
    ) !Surface {
        const ret = c.SDL_CreateSurfaceFrom(
            @intCast(width),
            @intCast(height),
            @intFromEnum(format),
            if (pixel_data) |val| @constCast(val.ptr) else null,
            if (pixel_data) |val| @intCast(val.len / height) else 0,
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Load a BMP or PNG image from a file.
    ///
    /// ## Function Parameters
    /// * `path`: The file to load.
    ///
    /// ## Return Value
    /// Returns the new surface structure.
    ///
    /// ## Remarks
    /// The new surface should be freed with `surface.Surface.deinit()`.
    /// Not doing so will result in a memory leak.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn initFromFile(
        path: [:0]const u8,
    ) !Surface {
        const ret = c.SDL_LoadSurface(
            path.ptr,
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Load a BMP image from a file.
    ///
    /// ## Function Parameters
    /// * `path`: The BMP file to load.
    ///
    /// ## Return Value
    /// Returns the new surface structure.
    ///
    /// ## Remarks
    /// The new surface should be freed with `surface.Surface.deinit()`.
    /// Not doing so will result in a memory leak.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initFromBmpFile(
        path: [:0]const u8,
    ) !Surface {
        const ret = c.SDL_LoadBMP(
            path.ptr,
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Create a surface from a BMP image from a seekable stream.
    ///
    /// ## Function Parameters
    /// * `stream`: The data stream for the surface.
    /// * `close_stream_after`: Close `stream` before returning, even on error.
    ///
    /// ## Return Value
    /// Returns the new surface structure.
    ///
    /// ## Remarks
    /// The new surface should be freed with `surface.Surface.deinit()`.
    /// Not doing so will result in a memory leak.
    ///
    /// Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// Version
    /// This function is available since SDL 3.2.0.
    pub fn initFromBmpIo(
        stream: io_stream.Stream,
        close_stream_after: bool,
    ) !Surface {
        const ret = c.SDL_LoadBMP_IO(
            stream.value,
            close_stream_after,
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Load a BMP or PNG image from a seekable SDL data stream.
    ///
    /// ## Function Parameters
    /// * `stream`: The data stream for the surface.
    /// * `close_stream_after`: Close `stream` before returning, even on error.
    ///
    /// ## Return Value
    /// Returns the new surface structure.
    ///
    /// ## Remarks
    /// The new surface should be freed with `surface.Surface.deinit()`.
    /// Not doing so will result in a memory leak.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn initFromIo(
        stream: io_stream.Stream,
        close_stream_after: bool,
    ) !Surface {
        const ret = c.SDL_LoadSurface_IO(
            stream.value,
            close_stream_after,
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Load a PNG image from a file.
    ///
    /// ## Function Parameters
    /// * `path`: The PNG file to load.
    ///
    /// ## Return Value
    /// Returns the new surface structure.
    ///
    /// ## Remarks
    /// This is intended as a convenience function for loading images from trusted sources.
    /// If you want to load arbitrary images you should use `libpng` or another image loading library designed with security in mind.
    ///
    /// The new surface should be freed with `surface.Surface.deinit()`.
    /// Not doing so will result in a memory leak.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn initFromPngFile(
        path: [:0]const u8,
    ) !Surface {
        const ret = c.SDL_LoadPNG(
            path.ptr,
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Create a surface from a PNG image from a seekable stream.
    ///
    /// ## Function Parameters
    /// * `stream`: The data stream for the surface.
    /// * `close_stream_after`: Close `stream` before returning, even on error.
    ///
    /// ## Return Value
    /// Returns the new surface structure.
    ///
    /// ## Remarks
    /// This is intended as a convenience function for loading images from trusted sources.
    /// If you want to load arbitrary images you should use `libpng` or another image loading library designed with security in mind.
    ///
    /// The new surface should be freed with `surface.Surface.deinit()`.
    /// Not doing so will result in a memory leak.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn initFromPngIo(
        stream: io_stream.Stream,
        close_stream_after: bool,
    ) !Surface {
        const ret = c.SDL_LoadPNG_IO(
            stream.value,
            close_stream_after,
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Set up a surface for directly accessing the pixels.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to be locked.
    ///
    /// ## Remarks
    /// Between calls to `surface.Surface.lock()` / `surface.Surface.unlock()`, you can write to and read from `surface.Surface.getPixels()`,
    /// using the pixel format stored in `surface.Surface.getFormat()`.
    /// Once you are done accessing the surface, you should use `surface.Surface.unlock()` to release it.
    ///
    /// Not all surfaces require locking.
    /// If `surface.Surface.mustLock()` evaluates to `false`, then you can read and write to the surface at any time, and the pixel format of the surface will not change.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    /// The locking referred to by this function is making the pixels available for direct access, not thread-safe locking.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn lock(
        self: Surface,
    ) !void {
        const ret = c.SDL_LockSurface(
            self.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Map an RGB triple to an opaque pixel value for a surface.
    ///
    /// ## Function Parameters
    /// * `surface`: The surface to use for the pixel format and palette.
    /// * `r`: The red component of the pixel in the range 0-255.
    /// * `g`: The green component of the pixel in the range 0-255.
    /// * `b`: The blue component of the pixel in the range 0-255.
    ///
    /// ## Return Value
    /// Returns a pixel value.
    ///
    /// ## Remarks
    /// This function maps the RGB color value to the specified pixel format and returns the pixel value best approximating the given RGB color value for the given pixel format.
    ///
    /// If the surface has a palette, the index of the closest matching color in the palette will be returned.
    ///
    /// If the surface pixel format has an alpha component it will be returned as all 1 bits (fully opaque).
    ///
    /// If the pixel format bpp (color depth) is less than 32-bpp then the unused upper bits of the return value can safely be ignored
    /// (e.g., with a 16-bpp format the return value can be assigned to a `u16`, and similarly a `u8` for an 8-bpp format).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as long as the palette is not modified.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn mapRgb(
        self: Surface,
        r: u8,
        g: u8,
        b: u8,
    ) pixels.Pixel {
        const ret = c.SDL_MapSurfaceRGB(
            self.value,
            r,
            g,
            b,
        );
        return pixels.Pixel{ .value = ret };
    }

    /// Map an RGBA quadruple to a pixel value for a surface.
    ///
    /// ## Function Parameters
    /// * `surface`: The surface to use for the pixel format and palette.
    /// * `r`: The red component of the pixel in the range 0-255.
    /// * `g`: The green component of the pixel in the range 0-255.
    /// * `b`: The blue component of the pixel in the range 0-255.
    /// * `a`: The alpha component of the pixel in the range 0-255.
    ///
    /// ## Return Value
    /// Returns a pixel value.
    ///
    /// ## Remarks
    /// This function maps the RGBA color value to the specified pixel format and returns the pixel value best approximating the given RGBA color value for the given pixel format.
    ///
    /// If the surface pixel format has no alpha component the alpha value will be ignored (as it will be in formats with a palette).
    ///
    /// If the surface has a palette, the index of the closest matching color in the palette will be returned.
    ///
    /// If the pixel format bpp (color depth) is less than 32-bpp then the unused upper bits of the return value can safely be ignored
    /// (e.g., with a 16-bpp format the return value can be assigned to a `u16`, and similarly a `u8` for an 8-bpp format).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as long as the palette is not modified.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn mapRgba(
        self: Surface,
        r: u8,
        g: u8,
        b: u8,
        a: u8,
    ) pixels.Pixel {
        const ret = c.SDL_MapSurfaceRGBA(
            self.value,
            r,
            g,
            b,
            a,
        );
        return pixels.Pixel{ .value = ret };
    }

    /// Evaluates to true if the surface needs to be locked before access.
    ///
    /// ## Function Parameters
    /// * `self`: The surface.
    ///
    /// ## Return Value
    /// If the surface needs to be locked before accessing it.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn mustLock(
        self: Surface,
    ) bool {
        return c.SDL_MUSTLOCK(self.value);
    }

    /// Premultiply the alpha in a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to modify.
    /// * `linear`: True to convert from sRGB to linear space for the alpha multiplication, false to do multiplication in sRGB space.
    ///
    /// ## Remarks
    /// This is safe to use with `src.ptr == dst.ptr`, but not for other overlapping areas.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn premultiplySurfaceAlpha(
        self: Surface,
        linear: bool,
    ) !void {
        const ret = c.SDL_PremultiplySurfaceAlpha(
            self.value,
            linear,
        );
        return errors.wrapCallBool(ret);
    }

    /// Retrieves a single pixel from a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to read.
    /// * `x`: The horizontal coordinate, `0 <= x < width`.
    /// * `y`: The vertical coordinate, `0 <= y < height`.
    ///
    /// ## Return Value
    /// Returns the value of the read pixel.
    ///
    /// ## Remarks
    /// This function prioritizes correctness over speed: it is suitable for unit tests, but is not intended for use in a game engine.
    ///
    /// Like `pixels.FormatDetails.getRgba()`, this uses the entire `0..255` range when converting color components from pixel formats with less than 8 bits per RGB component.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readPixel(
        self: Surface,
        x: usize,
        y: usize,
    ) !pixels.Color {
        var r: u8 = undefined;
        var g: u8 = undefined;
        var b: u8 = undefined;
        var a: u8 = undefined;
        const ret = c.SDL_ReadSurfacePixel(
            self.value,
            @intCast(x),
            @intCast(y),
            &r,
            &g,
            &b,
            &a,
        );
        try errors.wrapCallBool(ret);
        return .{ .r = r, .g = g, .b = b, .a = a };
    }

    /// Retrieves a single pixel from a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to read.
    /// * `x`: The horizontal coordinate, `0 <= x < width`.
    /// * `y`: The vertical coordinate, `0 <= y < height`.
    ///
    /// ## Return Value
    /// Returns the value of the read pixel.
    ///
    /// ## Remarks
    /// This function prioritizes correctness over speed: it is suitable for unit tests, but is not intended for use in a game engine.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn readPixelFloat(
        self: Surface,
        x: usize,
        y: usize,
    ) !pixels.FColor {
        var r: f32 = undefined;
        var g: f32 = undefined;
        var b: f32 = undefined;
        var a: f32 = undefined;
        const ret = c.SDL_ReadSurfacePixelFloat(
            self.value,
            @intCast(x),
            @intCast(y),
            &r,
            &g,
            &b,
            &a,
        );
        try errors.wrapCallBool(ret);
        return .{ .r = r, .g = g, .b = b, .a = a };
    }

    /// Remove all alternate versions of a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to update.
    ///
    /// ## Remarks
    /// This function removes a reference from all the alternative versions, destroying them if this is the last reference to them.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn removeAlternateImages(
        self: Surface,
    ) void {
        c.SDL_RemoveSurfaceAlternateImages(
            self.value,
        );
    }

    /// Return a copy of a surface rotated clockwise a number of degrees.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to rotate.
    /// * `angle`: The rotation angle, in degrees.
    ///
    /// ## Return Value
    /// Returns a rotated copy of the surface.
    ///
    /// ## Remarks
    /// The angle of rotation can be negative for counter-clockwise rotation.
    ///
    /// When the rotation isn't a multiple of 90 degrees, the resulting surface is larger than the original, with the background filled in with the colorkey,
    /// if available, or RGBA 255/255/255/0 if not.
    ///
    /// If surface has the `rotation` property set on it, the new copy will have the adjusted value set: if the rotation property is 90 and angle was 30,
    /// the new surface will have a property value of 60 (that is: to be upright vs gravity, this surface needs to rotate 60 more degrees).
    /// However, note that further rotations on the new surface in this example will produce unexpected results,
    /// since the image will have resized and padded to accommodate the not-90 degree angle.
    ///
    /// ## Thread Safety
    /// This function can be called on different threads with different surfaces.
    ///
    /// ## Version
    /// This function is available since SDL 3.4
    pub fn rotate(
        self: Surface,
        angle: f32,
    ) !Surface {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Surface, c.SDL_RotateSurface(self.value, angle)) };
    }

    /// Save a surface to a file.
    ///
    /// ## Function Parameters
    /// * `self`: The surface containing the image to be saved.
    /// * `path`: A file to save to.
    ///
    /// ## Remarks
    /// Surfaces with a 24-bit, 32-bit and paletted 8-bit format get saved in the BMP directly.
    /// Other RGB formats with 8-bit or higher get converted to a 24-bit surface or, if they have an alpha mask or a colorkey, to a 32-bit surface before they are saved.
    /// YUV and paletted 1-bit and 4-bit formats are not supported.
    ///
    /// ## Thread Safety
    /// This function can be called on different threads with different surfaces.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn saveBmpFile(
        self: Surface,
        path: [:0]const u8,
    ) !void {
        const ret = c.SDL_SaveBMP(
            self.value,
            path.ptr,
        );
        return errors.wrapCallBool(ret);
    }

    /// Save a surface to a seekable SDL data stream in BMP format.
    ///
    /// ## Function Parameters
    /// * `self`: The surface containing the image to be saved.
    /// * `stream`: The data stream to save to.
    /// * `close_stream_after`: Will close the stream before returning, even on error.
    ///
    /// ## Remarks
    /// Surfaces with a 24-bit, 32-bit and paletted 8-bit format get saved in the BMP directly.
    /// Other RGB formats with 8-bit or higher get converted to a 24-bit surface or, if they have an alpha mask or a colorkey, to a 32-bit surface before they are saved.
    /// YUV and paletted 1-bit and 4-bit formats are not supported.
    ///
    /// ## Thread Safety
    /// This function can be called on different threads with different surfaces.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn saveBmpIo(
        self: Surface,
        stream: io_stream.Stream,
        close_stream_after: bool,
    ) !void {
        const ret = c.SDL_SaveBMP_IO(
            self.value,
            stream.value,
            close_stream_after,
        );
        return errors.wrapCallBool(ret);
    }

    /// Save a surface to a file.
    ///
    /// ## Function Parameters
    /// * `self`: The surface containing the image to be saved.
    /// * `path`: A file to save to.
    ///
    /// ## Thread Safety
    /// This function can be called on different threads with different surfaces.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn savePngFile(
        self: Surface,
        path: [:0]const u8,
    ) !void {
        const ret = c.SDL_SavePNG(
            self.value,
            path.ptr,
        );
        return errors.wrapCallBool(ret);
    }

    /// Save a surface to a seekable SDL data stream in PNG format.
    ///
    /// ## Function Parameters
    /// * `self`: The surface containing the image to be saved.
    /// * `stream`: The data stream to save to.
    /// * `close_stream_after`: Will close the stream before returning, even on error.
    ///
    /// ## Thread Safety
    /// This function can be called on different threads with different surfaces.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn savePngIo(
        self: Surface,
        stream: io_stream.Stream,
        close_stream_after: bool,
    ) !void {
        const ret = c.SDL_SavePNG_IO(
            self.value,
            stream.value,
            close_stream_after,
        );
        return errors.wrapCallBool(ret);
    }

    /// Creates a new surface identical to the existing surface, scaled to the desired size.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to duplicate and scale.
    /// * `width`: The width of the new surface.
    /// * `height`: The height of the new surface.
    /// * `scale_mode`: The scaling mode to be used.
    ///
    /// ## Return Value
    /// Returns a copy of the surface.
    ///
    /// ## Remarks
    /// The returned surface should be freed with `surface.Surface.deinit()`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn scale(
        self: Surface,
        width: usize,
        height: usize,
        scale_mode: ScaleMode,
    ) !Surface {
        const ret = c.SDL_ScaleSurface(
            self.value,
            @intCast(width),
            @intCast(height),
            @bitCast(@intFromEnum(scale_mode)),
        );
        return Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Set an additional alpha value used in blit operations.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to update.
    /// * `alpha`: The alpha value multiplied into blit operations.
    ///
    /// ## Remarks
    /// When this surface is blitted, during the blit operation the source alpha value is modulated by this alpha value according to the following formula:
    /// `srcA = srcA * (alpha / 255)`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setAlphaMod(
        self: Surface,
        alpha: u8,
    ) !void {
        const ret = c.SDL_SetSurfaceAlphaMod(
            self.value,
            @intCast(alpha),
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the blend mode used for blit operations.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to update.
    /// * `blend_mode`: The blend mode to use for blit blending.
    ///
    /// ## Remarks
    /// To copy a surface to another surface (or texture) without blending with the existing data, the blendmode of the source surface should be set to `blend_mode.Mode.none`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setBlendMode(
        self: Surface,
        mode: blend_mode.Mode,
    ) !void {
        const ret = c.SDL_SetSurfaceBlendMode(
            self.value,
            blend_mode.Mode.toSdl(mode),
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the clipping rectangle for a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to update.
    /// * `val`: The value to set for clipping, or `null` to disable.
    ///
    /// ## Return Value
    /// Returns true if the rectangle intersects the surface, otherwise false and blits will be completely clipped.
    ///
    /// ## Remarks
    /// When surface is the destination of a blit, only the area within the clip rectangle is drawn into.
    ///
    /// Note that blits are automatically clipped to the edges of the source and destination surfaces.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setClipRect(
        self: Surface,
        val: ?rect.IRect,
    ) bool {
        const val_sdl: c.SDL_Rect = if (val) |v| v.toSdl() else undefined;
        const ret = c.SDL_SetSurfaceClipRect(
            self.value,
            if (val != null) &val_sdl else null,
        );
        return ret;
    }

    /// Set the color key (transparent pixel) in a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to update.
    /// * `pixel`: The value to enable color key for, or `null` to disable.
    ///
    /// ## Remarks
    /// The color key defines a pixel value that will be treated as transparent in a blit.
    /// For example, one can use this to specify that cyan pixels should be considered transparent, and therefore not rendered.
    ///
    /// It is a pixel of the format used by the surface, as generated by `surface.Surface.mapRgb()`.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setColorKey(
        self: Surface,
        pixel: ?pixels.Pixel,
    ) !void {
        const ret = c.SDL_SetSurfaceColorKey(
            self.value,
            pixel != null,
            if (pixel) |val| val.value else 0,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set an additional color value multiplied into blit operations.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to update.
    /// * `r`: The red color value multiplied into blit operations.
    /// * `g`: The green color value multiplied into blit operations.
    /// * `b`: The blue color value multiplied into blit operations.
    ///
    /// ## Remarks
    /// When this surface is blitted, during the blit operation each source color channel is modulated by the appropriate color value according to the following formula:
    /// `srcC = srcC * (color / 255)`
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setColorMod(
        self: Surface,
        r: u8,
        g: u8,
        b: u8,
    ) !void {
        const ret = c.SDL_SetSurfaceColorMod(
            self.value,
            r,
            g,
            b,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the colorspace used by a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to update.
    /// * `colorspace`: The surface colorspace.
    ///
    /// ## Remarks
    /// Setting the colorspace doesn't change the pixels, only how they are interpreted in color operations.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setColorspace(
        self: Surface,
        colorspace: pixels.Colorspace,
    ) !void {
        const ret = c.SDL_SetSurfaceColorspace(
            self.value,
            @intFromEnum(colorspace),
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the palette used by a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to update.
    /// * `palette`: The palette to use.
    ///
    /// ## Remarks
    /// A single palette can be shared with many surfaces.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setPalette(
        self: Surface,
        palette: pixels.Palette,
    ) !void {
        const ret = c.SDL_SetSurfacePalette(
            self.value,
            palette.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the properties associated with a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The structure to query.
    /// * `props`: The properties to set.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setProperties(
        self: Surface,
        props: Properties,
    ) !void {
        const ret = c.SDL_GetSurfaceProperties(
            self.value,
        );
        const group = properties.Group{ .value = try errors.wrapCall(c.SDL_PropertiesID, ret, 0) };
        try props.toSdl(group);
    }

    /// Set the RLE acceleration hint for a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to optimize.
    /// * `enabled`: Whether to enable or disable RLE acceleration.
    ///
    /// ## Remarks
    /// If RLE is enabled, color key and alpha blending blits are much faster, but the surface must be locked before directly accessing the pixels.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setRle(
        self: Surface,
        enabled: bool,
    ) !void {
        const ret = c.SDL_SetSurfaceRLE(
            self.value,
            enabled,
        );
        return errors.wrapCallBool(ret);
    }

    /// Perform a stretched pixel copy from one surface to another.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to be copied from.
    /// * `area_to_copy_from`: The rectangle to be copied.
    /// * `dst`: Surface that is the blit target.
    /// * `area_to_copy_to`: The target rectangle in the destination surface.
    /// * `scale_mode`: The scale mode to be used.
    ///
    /// ## Thread Safety
    /// Only one thread should be using the `self` and `dst` surfaces at any given time.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn stretch(
        self: Surface,
        area_to_copy_from: rect.IRect,
        dst: Surface,
        area_to_copy_to: rect.IRect,
        scale_mode: ScaleMode,
    ) !void {
        const area_to_copy_from_sdl = area_to_copy_from.toSdl();
        const area_to_copy_to_sdl = area_to_copy_to.toSdl();
        return errors.wrapCallBool(c.SDL_StretchSurface(
            self.value,
            &area_to_copy_from_sdl,
            dst.value,
            &area_to_copy_to_sdl,
            @bitCast(@intFromEnum(scale_mode)),
        ));
    }

    /// Release a surface after directly accessing the pixels.
    ///
    /// ## Function Parameters
    /// * `self`: The surface structure to be unlocked.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    /// The locking referred to by this function is making the pixels available for direct access, not thread-safe locking.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn unlock(
        self: Surface,
    ) void {
        c.SDL_UnlockSurface(
            self.value,
        );
    }

    /// Writes a single pixel to a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to write.
    /// * `x`: The horizontal coordinate, `0 <= x < width`.
    /// * `y`: The vertical coordinate, `0 <= y < height`.
    /// * `color`: Color channel values, in `0` to `255` range.
    ///
    /// ## Remarks
    /// This function prioritizes correctness over speed: it is suitable for unit tests, but is not intended for use in a game engine.
    ///
    /// Like `pixels.mapRgba`, this uses the entire `0..255` range when converting color components from pixel formats with less than 8 bits per RGB component.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writePixel(
        self: Surface,
        x: usize,
        y: usize,
        color: pixels.Color,
    ) !void {
        const ret = c.SDL_WriteSurfacePixel(
            self.value,
            @intCast(x),
            @intCast(y),
            color.r,
            color.g,
            color.b,
            color.a,
        );
        return errors.wrapCallBool(ret);
    }

    /// Writes a single pixel to a surface.
    ///
    /// ## Function Parameters
    /// * `self`: The surface to write.
    /// * `x`: The horizontal coordinate, `0 <= x < width`.
    /// * `y`: The vertical coordinate, `0 <= y < height`.
    /// * `color`: Color channel values, usually in `0` to `1` range.
    ///
    /// ## Remarks
    /// This function prioritizes correctness over speed: it is suitable for unit tests, but is not intended for use in a game engine.
    ///
    /// ## Thread Safety
    /// This function is not thread safe.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn writePixelFloat(
        self: Surface,
        x: usize,
        y: usize,
        color: pixels.FColor,
    ) !void {
        const ret = c.SDL_WriteSurfacePixelFloat(
            self.value,
            @intCast(x),
            @intCast(y),
            color.r,
            color.g,
            color.b,
            color.a,
        );
        return errors.wrapCallBool(ret);
    }
};

/// Copy a block of pixels of one format to another format.
///
/// ## Function Parameters
/// * `width`: The width of the block to copy, in pixels.
/// * `height`: The height of the block to copy, in pixels.
/// * `src_format`: The format of the `src` pixels.
/// * `src`: Slice to the source pixels, length must be a multiple of its stride.
/// * `dst_format`: The format of the `dst` pixels.
/// * `dst`: Slice to the destination pixels, length must be a multiple of its stride.
///
/// ## Thread Safety
/// The same destination pixels should not be used from two threads at once.
/// It is safe to use the same source pixels from multiple threads.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn convertPixels(
    width: usize,
    height: usize,
    src_format: pixels.Format,
    src: []const u8,
    dst_format: pixels.Format,
    dst: []u8,
) !void {
    const ret = c.SDL_ConvertPixels(
        @intCast(width),
        @intCast(height),
        @intFromEnum(src_format),
        src.ptr,
        @intCast(src.len / height),
        @intFromEnum(dst_format),
        dst.ptr,
        @intCast(dst.len / height),
    );
    return errors.wrapCallBool(ret);
}

/// Copy a block of pixels of one format and colorspace to another format and colorspace.
///
/// ## Function Parameters
/// * `width`: The width of the block to copy, in pixels.
/// * `height`: The height of the block to copy, in pixels.
/// * `src_format`: The format of the `src` pixels.
/// * `src_colorspace`: The colorspace describing the `src` pixels.
/// * `src_properties`: Additional source color properties, or `null`.
/// * `src`: Slice to the source pixels, length must be a multiple of its stride.
/// * `dst_format`: The format of the `dst` pixels.
/// * `dst_colorspace`: The colorspace describing the `dst` pixels.
/// * `dst_properties`: Additional destination color properties, or `null`.
/// * `dst`: Slice to the destination pixels, length must be a multiple of its stride.
///
/// ## Thread Safety
/// The same destination pixels should not be used from two threads at once.
/// It is safe to use the same source pixels from multiple threads.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn convertPixelsAndColorspace(
    width: usize,
    height: usize,
    src_format: pixels.Format,
    src_colorspace: pixels.Colorspace,
    src_properties: ?properties.Group,
    src: []const u8,
    dst_format: pixels.Format,
    dst_colorspace: pixels.Colorspace,
    dst_properties: ?properties.Group,
    dst: []u8,
) !void {
    const ret = c.SDL_ConvertPixelsAndColorspace(
        @intCast(width),
        @intCast(height),
        @intFromEnum(src_format),
        @intFromEnum(src_colorspace),
        if (src_properties) |val| val.value else 0,
        src.ptr,
        @intCast(src.len / height),
        @intFromEnum(dst_format),
        @intFromEnum(dst_colorspace),
        if (dst_properties) |val| val.value else 0,
        dst.ptr,
        @intCast(dst.len / height),
    );
    return errors.wrapCallBool(ret);
}

/// Premultiply the alpha on a block of pixels.
///
/// ## Function Parameters
/// * `width`: The width of the block to convert, in pixels.
/// * `height`: The height of the block to convert, in pixels.
/// * `src_format`: The format of the source pixels.
/// * `src`: Source pixels, must be a multiple of the pitch of the format.
/// * `dst_format`: The format of the destination pixels.
/// * `dst`: Destination pixels, must be a multiple of the pitch of the format.
/// * `linear`: True to convert from sRGB to linear space for the alpha multiplication, false to do multiplication in sRGB space.
///
/// ## Remarks
/// This is safe to use with `src.ptr == dst.ptr`, but not for other overlapping areas.
///
/// ## Thread Safety
/// The same destination pixels should not be used from two threads at once.
/// It is safe to use the same source pixels from multiple threads.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn premultiplyAlpha(
    width: usize,
    height: usize,
    src_format: pixels.Format,
    src: []const u8,
    dst_format: pixels.Format,
    dst: []u8,
    linear: bool,
) !void {
    const ret = c.SDL_PremultiplyAlpha(
        @intCast(width),
        @intCast(height),
        @intFromEnum(src_format),
        src.ptr,
        @intCast(src.len / height),
        @intFromEnum(dst_format),
        dst.ptr,
        @intCast(dst.len / height),
        linear,
    );
    try errors.wrapCallBool(ret);
}

// Surface tests.
test "Surface" {
    errors.refAllDeclsRecursive(@This());
}
