const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");
const surface = @import("surface.zig");

/// A fully opaque 8-bit alpha value.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub const alpha_opaque: u8 = 255;

/// A fully opaque floating point alpha value.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub const alpha_opaque_float: f32 = 1;

/// A fully transparent 8-bit alpha value.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub const alpha_transparent: u8 = 0;

/// A fully transparent floating point alpha value.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub const alpha_transparent_float: f32 = 0;

/// Array component order, low byte -> high byte.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const ArrayOrder = enum(c.SDL_ArrayOrder) {
    rgb = c.SDL_ARRAYORDER_RGB,
    rgba = c.SDL_ARRAYORDER_RGBA,
    argb = c.SDL_ARRAYORDER_ARGB,
    bgr = c.SDL_ARRAYORDER_BGR,
    bgra = c.SDL_ARRAYORDER_BGRA,
    abgr = c.SDL_ARRAYORDER_ABGR,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_ArrayOrder) ?ArrayOrder {
        if (value == c.SDL_ARRAYORDER_NONE)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?ArrayOrder) c.SDL_ArrayOrder {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_ARRAYORDER_NONE;
    }
};

/// Bitmap pixel order, high bit -> low bit.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const BitmapOrder = enum(c.SDL_BitmapOrder) {
    high_to_low = c.SDL_BITMAPORDER_4321,
    low_to_high = c.SDL_BITMAPORDER_1234,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_BitmapOrder) ?BitmapOrder {
        if (value == c.SDL_BITMAPORDER_NONE)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?BitmapOrder) c.SDL_BitmapOrder {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_BITMAPORDER_NONE;
    }
};

/// Colorspace chroma sample location.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const ChromaLocation = enum(c.SDL_ChromaLocation) {
    /// In MPEG-2, MPEG-4, and AVC, Cb and Cr are taken on midpoint of the left-edge of the 2x2 square.
    /// In other words, they have the same horizontal location as the top-left pixel, but is shifted one-half pixel down vertically.
    left = c.SDL_CHROMA_LOCATION_LEFT,
    /// In JPEG/JFIF, H.261, and MPEG-1, Cb and Cr are taken at the center of the 2x2 square.
    /// In other words, they are offset one-half pixel to the right and one-half pixel down compared to the top-left pixel.
    center = c.SDL_CHROMA_LOCATION_CENTER,
    /// In HEVC for BT.2020 and BT.2100 content (in particular on Blu-rays), Cb and Cr are sampled at the same location as the group's top-left Y pixel (co-sited, co-located).
    top_left = c.SDL_CHROMA_LOCATION_TOPLEFT,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_ChromaLocation) ?ChromaLocation {
        if (value == c.SDL_CHROMA_LOCATION_NONE)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?ChromaLocation) c.SDL_ChromaLocation {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_CHROMA_LOCATION_NONE;
    }
};

/// A structure that represents a color as RGBA components.
///
/// ## Remarks
/// The bits of this structure can be directly reinterpreted as an integer-packed color which uses
/// the `pixels.Format.array_rgba_32` format (`pixels.Format.packed_abgr_8_8_8_8` on little-endian systems and `pixels.Format.packed_rgba_8_8_8_8` on big-endian systems).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Color = c.SDL_Color;

/// Colorspace definitions.
/// How these are formed can be seen at: https://wiki.libsdl.org/SDL3/SDL_Colorspace
///
/// ## Remarks
/// Since similar colorspaces may vary in their details (matrix, transfer function, etc.),
/// this is not an exhaustive list, but rather a representative sample of the kinds of colorspaces supported in SDL.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Colorspace = enum(c.SDL_Colorspace) {
    /// sRGB is a gamma corrected colorspace, and the default colorspace for SDL rendering and 8-bit RGB surfaces.
    srgb = c.SDL_COLORSPACE_SRGB,
    /// This is a linear colorspace and the default colorspace for floating point surfaces.
    /// On Windows this is the scRGB colorspace, and on Apple platforms this is `kCGColorSpaceExtendedLinearSRGB` for EDR content.
    srgb_linear = c.SDL_COLORSPACE_SRGB_LINEAR,
    /// HDR10 is a non-linear HDR colorspace and the default colorspace for 10-bit surfaces.
    hdr10 = c.SDL_COLORSPACE_HDR10,
    /// Equivalent to `DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601`.
    jpeg = c.SDL_COLORSPACE_JPEG,
    /// Equivalent to `DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601`.
    bt601_limited = c.SDL_COLORSPACE_BT601_LIMITED,
    /// Equivalent to `DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601`.
    bt601_full = c.SDL_COLORSPACE_BT601_FULL,
    /// Equivalent to `DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709`.
    bt709_limited = c.SDL_COLORSPACE_BT709_LIMITED,
    /// Equivalent to `DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709`.
    bt709_full = c.SDL_COLORSPACE_BT709_FULL,
    /// Equivalent to `DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020`.
    bt2020_limited = c.SDL_COLORSPACE_BT2020_LIMITED,
    /// Equivalent to `DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020`.
    bt2020_full = c.SDL_COLORSPACE_BT2020_FULL,
    _,

    /// The default colorspace for RGB surfaces if no colorspace is specified.
    pub const rgb_default: Colorspace = @enumFromInt(c.SDL_COLORSPACE_RGB_DEFAULT);
    /// The default colorspace for YUV surfaces if no colorspace is specified.
    pub const yuv_default: Colorspace = @enumFromInt(c.SDL_COLORSPACE_YUV_DEFAULT);

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Colorspace) ?Colorspace {
        if (val == c.SDL_COLORSPACE_UNKNOWN)
            return null;
        return @enumFromInt(val);
    }

    /// Convert to SDL.
    pub fn toSdl(self: ?Colorspace) c.SDL_Colorspace {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_COLORSPACE_UNKNOWN;
    }

    /// Create a colorspace.
    ///
    /// ## Function Parameters
    /// * `color_type`: The type of the new format, probably a color type value.
    /// * `range`: The range of the new format, probably a color range value.
    /// * `primaries`: The primaries of the new format, probably a color primaries value.
    /// * `transfer`: The transfer characteristics of the new format, probably a transfer characteristics value.
    /// * `matrix`: The matrix coefficients of the new format, probably a matrix coefficients value.
    /// * `chroma`: The chroma sample location of the new format, probably an chroma location value.
    ///
    /// ## Return Value
    /// Returns a color space.
    ///
    /// ## Remarks
    /// For example, defining `pixels.Colorspace.srgb` looks like this:
    ///
    /// ```zig
    /// pixels.Colorspace.define(
    ///     pixels.ColorType.rgb,
    ///     pixels.ColorRange.full,
    ///     pixels.ColorPrimaries.bt709,
    ///     pixels.TransferCharacters.srgb,
    ///     pixels.MatrixCoefficients.identity,
    ///     null,
    /// )
    /// ```
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This macro is available since SDL 3.2.0.
    pub fn define(
        color_type: ?ColorType,
        range: ?ColorRange,
        primaries: ?ColorPrimaries,
        transfer: ?TransferCharacteristics,
        matrix: MatrixCoefficients,
        chroma: ?ChromaLocation,
    ) Colorspace {
        const ret = c.SDL_DEFINE_COLORSPACE(
            ColorType.toSdl(color_type),
            ColorRange.toSdl(range),
            ColorPrimaries.toSdl(primaries),
            TransferCharacteristics.toSdl(transfer),
            @intFromEnum(matrix),
            ChromaLocation.toSdl(chroma),
        );
        return @enumFromInt(ret);
    }

    /// Get the color space chroma.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// The chroma location of the color space.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getChromaLocation(
        self: Colorspace,
    ) ?ChromaLocation {
        const ret = c.SDL_COLORSPACECHROMA(
            @intFromEnum(self),
        );
        return ChromaLocation.fromSdl(ret);
    }

    /// Get the color space primaries.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// The primaries of the color space.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getColorPrimaries(
        self: Colorspace,
    ) ?ColorPrimaries {
        const ret = c.SDL_COLORSPACEPRIMARIES(
            @intFromEnum(self),
        );
        return ColorPrimaries.fromSdl(ret);
    }

    /// Get the color space matrix.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// The matrix coefficients of the color space.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMatrix(
        self: Colorspace,
    ) MatrixCoefficients {
        const ret = c.SDL_COLORSPACEMATRIX(
            @intFromEnum(self),
        );
        return @enumFromInt(ret);
    }

    /// Get the color space range.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// The range of the color space.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getRange(
        self: Colorspace,
    ) ?ColorRange {
        const ret = c.SDL_COLORSPACERANGE(
            @intFromEnum(self),
        );
        return ColorRange.fromSdl(ret);
    }

    /// Get the color space transfer characteristics.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// The transfer characteristics of the color space.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getTransferCharacteristics(
        self: Colorspace,
    ) ?TransferCharacteristics {
        const ret = c.SDL_COLORSPACETRANSFER(
            @intFromEnum(self),
        );
        return TransferCharacteristics.fromSdl(ret);
    }

    /// Get the color space type.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// The type of the color space.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getType(
        self: Colorspace,
    ) ?ColorType {
        const ret = c.SDL_COLORSPACETYPE(
            @intFromEnum(self),
        );
        return ColorType.fromSdl(ret);
    }

    /// If the matrix is BT2020 NCL.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// Returns true if `bt2020_ncl`, false otherwise.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isMatrixBt2020Ncl(
        self: Colorspace,
    ) bool {
        const ret = c.SDL_ISCOLORSPACE_MATRIX_BT2020_NCL(
            @intFromEnum(self),
        );
        return ret;
    }

    /// If the matrix is BT601.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// Returns true if BT601 or BT470BG, false otherwise.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isMatrixBt601(
        self: Colorspace,
    ) bool {
        const ret = c.SDL_ISCOLORSPACE_MATRIX_BT601(
            @intFromEnum(self),
        );
        return ret;
    }

    /// If the matrix is BT709.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// Returns true if BT709, false otherwise.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isMatrixBT709(
        self: Colorspace,
    ) bool {
        const ret = c.SDL_ISCOLORSPACE_MATRIX_BT709(
            @intFromEnum(self),
        );
        return ret;
    }

    /// If the color space is full range.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// Returns true if full range, false otherwise.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isFullRange(
        self: Colorspace,
    ) bool {
        const ret = c.SDL_ISCOLORSPACE_FULL_RANGE(
            @intFromEnum(self),
        );
        return ret;
    }

    /// If the color space is limited range.
    ///
    /// ## Function Parameters
    /// * `self`: The color space to check.
    ///
    /// ## Return Value
    /// Returns true if limited range, false otherwise.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isLimitedRange(
        self: Colorspace,
    ) bool {
        const ret = c.SDL_ISCOLORSPACE_LIMITED_RANGE(
            @intFromEnum(self),
        );
        return ret;
    }
};

/// Colorspace color primaries, as described by https://www.itu.int/rec/T-REC-H.273-201612-S/en.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const ColorPrimaries = enum(c.SDL_ColorPrimaries) {
    /// ITU-R BT.709-6.
    bt709 = c.SDL_COLOR_PRIMARIES_BT709,
    unspecified = c.SDL_COLOR_PRIMARIES_UNSPECIFIED,
    /// ITU-R BT.470-6 System M.
    bt470m = c.SDL_COLOR_PRIMARIES_BT470M,
    /// ITU-R BT.470-6 System B, G / ITU-R BT.601-7 625.
    bt470bg = c.SDL_COLOR_PRIMARIES_BT470BG,
    /// ITU-R BT.601-7 525, SMPTE 170M.
    bt601 = c.SDL_COLOR_PRIMARIES_BT601,
    /// SMPTE 240M, functionally the same as SDL_COLOR_PRIMARIES_BT601.
    smpte240 = c.SDL_COLOR_PRIMARIES_SMPTE240,
    /// Generic film (color filters using Illuminant C).
    generic_film = c.SDL_COLOR_PRIMARIES_GENERIC_FILM,
    /// ITU-R BT.2020-2 / ITU-R BT.2100-0
    bt2020 = c.SDL_COLOR_PRIMARIES_BT2020,
    /// SMPTE ST 428-1SMPTE ST 428-1.
    xyz = c.SDL_COLOR_PRIMARIES_XYZ,
    /// SMPTE RP 431-2.
    smpte431 = c.SDL_COLOR_PRIMARIES_SMPTE431,
    /// SMPTE EG 432-1 / DCI P3.
    smpte432 = c.SDL_COLOR_PRIMARIES_SMPTE432,
    /// EBU Tech. 3213-E.
    ebu3213 = c.SDL_COLOR_PRIMARIES_EBU3213,
    custom = c.SDL_COLOR_PRIMARIES_CUSTOM,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_ColorPrimaries) ?ColorPrimaries {
        if (value == c.SDL_COLOR_PRIMARIES_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?ColorPrimaries) c.SDL_ColorPrimaries {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_COLOR_PRIMARIES_UNKNOWN;
    }
};

/// Colorspace color range, as described by https://www.itu.int/rec/R-REC-BT.2100-2-201807-I/en.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const ColorRange = enum(c.SDL_ColorRange) {
    /// Narrow range, e.g. 16-235 for 8-bit RGB and luma, and 16-240 for 8-bit chroma.
    limited = c.SDL_COLOR_RANGE_LIMITED,
    /// Full range, e.g. 0-255 for 8-bit RGB and luma, and 1-255 for 8-bit chroma.
    full = c.SDL_COLOR_RANGE_FULL,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_ColorRange) ?ColorRange {
        if (value == c.SDL_COLOR_RANGE_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?ColorRange) c.SDL_ColorRange {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_COLOR_RANGE_UNKNOWN;
    }
};

/// Colorspace color type.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const ColorType = enum(c.SDL_ColorType) {
    rgb = c.SDL_COLOR_TYPE_RGB,
    ycbcr = c.SDL_COLOR_TYPE_YCBCR,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_ColorType) ?ColorType {
        if (value == c.SDL_COLOR_TYPE_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?ColorType) c.SDL_ColorType {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_COLOR_TYPE_UNKNOWN;
    }
};

/// The bits of this structure can be directly reinterpreted as a float-packed color which uses the `pixels.Format.array_rgba_128_float` format.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const FColor = c.SDL_FColor;

/// Pixel format.
///
/// ## Remarks
/// SDL's pixel formats have the following naming convention:
/// * Names with a list of components and a single bit count, such as `array_rgb_24` and `array_abgr_32`, define a platform-independent encoding into bytes in the order specified.
/// For example, in `array_rgb_24` data, each pixel is encoded in 3 bytes (red, green, blue) in that order,
/// and in `array_abgr_32` data, each pixel is encoded in 4 bytes alpha, blue, green, red) in that order.
/// Use these names if the property of a format that is important to you is the order of the bytes in memory or on disk.
/// * Names with a bit count per component, such as `packed_argb_8_8_8_8` and `packed_xrgb_1_5_5_5`, are "packed" into an appropriately-sized integer in the platform's native endianness.
/// For example, `packed_argb_8_8_8_8` is a sequence of 32-bit integers; in each integer, the most significant bits are alpha, and the least significant bits are blue.
/// On a little-endian CPU such as x86, the least significant bits of each integer are arranged first in memory, but on a big-endian CPU such as s390x,
/// the most significant bits are arranged first.
/// Use these names if the property of a format that is important to you is the meaning of each bit position within a native-endianness integer.
/// * In indexed formats such as `index_4_lsb`, each pixel is represented by encoding an index into the palette into the indicated number of bits,
/// with multiple pixels packed into each byte if appropriate.
/// In LSB formats, the first (leftmost) pixel is stored in the least-significant bits of the byte; in MSB formats, it's stored in the most-significant bits.
/// `index_8` does not need LSB/MSB variants, because each pixel exactly fills one byte.
/// The 32-bit byte-array encodings such as `array_rgba_32` are aliases for the appropriate `8_8_8_8` encoding for the current platform.
/// For example, `array_rgba_32` is an alias for `packed_abgr_8_8_8_8` on little-endian CPUs like x86, or an alias for `packed_rgba_8_8_8_8` on big-endian CPUs.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Format = enum(c.SDL_PixelFormat) {
    index_1_lsb = c.SDL_PIXELFORMAT_INDEX1LSB,
    index_1_msb = c.SDL_PIXELFORMAT_INDEX1MSB,
    index_2_lsb = c.SDL_PIXELFORMAT_INDEX2LSB,
    index_2_msb = c.SDL_PIXELFORMAT_INDEX2MSB,
    index_4_lsb = c.SDL_PIXELFORMAT_INDEX4LSB,
    index_4_msb = c.SDL_PIXELFORMAT_INDEX4MSB,
    index_8 = c.SDL_PIXELFORMAT_INDEX8,
    packed_rgb_3_3_2 = c.SDL_PIXELFORMAT_RGB332,
    packed_xrgb_4_4_4_4 = c.SDL_PIXELFORMAT_XRGB4444,
    packed_xbgr_4_4_4_4 = c.SDL_PIXELFORMAT_XBGR4444,
    packed_xrgb_1_5_5_5 = c.SDL_PIXELFORMAT_XRGB1555,
    packed_xbgr_1_5_5_5 = c.SDL_PIXELFORMAT_XBGR1555,
    packed_argb_4_4_4_4 = c.SDL_PIXELFORMAT_ARGB4444,
    packed_rgba_4_4_4_4 = c.SDL_PIXELFORMAT_RGBA4444,
    packed_abgr_4_4_4_4 = c.SDL_PIXELFORMAT_ABGR4444,
    packed_bgra_4_4_4_4 = c.SDL_PIXELFORMAT_BGRA4444,
    packed_argb_1_5_5_5 = c.SDL_PIXELFORMAT_ARGB1555,
    packed_rgba_5_5_5_1 = c.SDL_PIXELFORMAT_RGBA5551,
    packed_abgr_1_5_5_5 = c.SDL_PIXELFORMAT_ABGR1555,
    packed_bgra_5_5_5_1 = c.SDL_PIXELFORMAT_BGRA5551,
    packed_rgb_5_6_5 = c.SDL_PIXELFORMAT_RGB565,
    packed_bgr_5_6_5 = c.SDL_PIXELFORMAT_BGR565,
    array_rgb_24 = c.SDL_PIXELFORMAT_RGB24,
    array_bgr_24 = c.SDL_PIXELFORMAT_BGR24,
    packed_xrgb_8_8_8_8 = c.SDL_PIXELFORMAT_XRGB8888,
    packed_rgbx_8_8_8_8 = c.SDL_PIXELFORMAT_RGBX8888,
    packed_xbgr_8_8_8_8 = c.SDL_PIXELFORMAT_XBGR8888,
    packed_bgrx_8_8_8_8 = c.SDL_PIXELFORMAT_BGRX8888,
    packed_argb_8_8_8_8 = c.SDL_PIXELFORMAT_ARGB8888,
    packed_rgba_8_8_8_8 = c.SDL_PIXELFORMAT_RGBA8888,
    packed_abgr_8_8_8_8 = c.SDL_PIXELFORMAT_ABGR8888,
    packed_bgra_8_8_8_8 = c.SDL_PIXELFORMAT_BGRA8888,
    packed_xrgb_2_10_10_10 = c.SDL_PIXELFORMAT_XRGB2101010,
    packed_xbgr_2_10_10_10 = c.SDL_PIXELFORMAT_XBGR2101010,
    packed_argb_2_10_10_10 = c.SDL_PIXELFORMAT_ARGB2101010,
    packed_abgr_2_10_10_10 = c.SDL_PIXELFORMAT_ABGR2101010,
    array_rgb_48 = c.SDL_PIXELFORMAT_RGB48,
    array_bgr_48 = c.SDL_PIXELFORMAT_BGR48,
    array_rgba_64 = c.SDL_PIXELFORMAT_RGBA64,
    array_argb_64 = c.SDL_PIXELFORMAT_ARGB64,
    array_bgra_64 = c.SDL_PIXELFORMAT_BGRA64,
    array_abgr_64 = c.SDL_PIXELFORMAT_ABGR64,
    array_rgb_48_float = c.SDL_PIXELFORMAT_RGB48_FLOAT,
    array_bgr_48_float = c.SDL_PIXELFORMAT_BGR48_FLOAT,
    array_rgba_64_float = c.SDL_PIXELFORMAT_RGBA64_FLOAT,
    array_argb_64_float = c.SDL_PIXELFORMAT_ARGB64_FLOAT,
    array_bgra_64_float = c.SDL_PIXELFORMAT_BGRA64_FLOAT,
    array_abgr_64_float = c.SDL_PIXELFORMAT_ABGR64_FLOAT,
    array_rgb_96_float = c.SDL_PIXELFORMAT_RGB96_FLOAT,
    array_bgr_96_float = c.SDL_PIXELFORMAT_BGR96_FLOAT,
    array_rgba_128_float = c.SDL_PIXELFORMAT_RGBA128_FLOAT,
    array_argb_128_float = c.SDL_PIXELFORMAT_ARGB128_FLOAT,
    array_bgra_128_float = c.SDL_PIXELFORMAT_BGRA128_FLOAT,
    array_abgr_128_float = c.SDL_PIXELFORMAT_ABGR128_FLOAT,
    fourcc_yv12 = c.SDL_PIXELFORMAT_YV12,
    fourcc_iyuv = c.SDL_PIXELFORMAT_IYUV,
    fourcc_yuy2 = c.SDL_PIXELFORMAT_YUY2,
    fourcc_uyvy = c.SDL_PIXELFORMAT_UYVY,
    fourcc_yvyu = c.SDL_PIXELFORMAT_YVYU,
    fourcc_nv12 = c.SDL_PIXELFORMAT_NV12,
    fourcc_nv21 = c.SDL_PIXELFORMAT_NV21,
    fourcc_p010 = c.SDL_PIXELFORMAT_P010,
    fourcc_oes = c.SDL_PIXELFORMAT_EXTERNAL_OES,
    // MJPG is in SDL 3.4.0?
    _,

    pub const array_rgba_32: Format = @enumFromInt(c.SDL_PIXELFORMAT_RGBA32);
    pub const array_argb_32: Format = @enumFromInt(c.SDL_PIXELFORMAT_ARGB32);
    pub const array_bgra_32: Format = @enumFromInt(c.SDL_PIXELFORMAT_BGRA32);
    pub const array_abgr_32: Format = @enumFromInt(c.SDL_PIXELFORMAT_ABGR32);
    pub const array_rgbx_32: Format = @enumFromInt(c.SDL_PIXELFORMAT_RGBX32);
    pub const array_xrgb_32: Format = @enumFromInt(c.SDL_PIXELFORMAT_XRGB32);
    pub const array_bgrx_32: Format = @enumFromInt(c.SDL_PIXELFORMAT_BGRX32);
    pub const array_xbgr_32: Format = @enumFromInt(c.SDL_PIXELFORMAT_XBGR32);

    /// Details about the format of a pixel.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const Details = struct {
        value: c.SDL_PixelFormatDetails,

        /// Initialize the format details.
        ///
        /// ## Function Parameters
        /// * `self`: The details of the pixel format.
        ///
        /// ## Return Value
        /// These are the zig-named members of the struct.
        ///
        /// ## Version
        /// This function is provided by zig-sdl3.
        pub fn getDetails(
            self: Details,
        ) struct {
            format: ?Format,
            bits_per_pixel: u8,
            bytes_per_pixel: u8,
            r_mask: u32,
            g_mask: u32,
            b_mask: u32,
            a_mask: u32,
            r_bits: u8,
            g_bits: u8,
            b_bits: u8,
            a_bits: u8,
            r_shift: u8,
            g_shift: u8,
            b_shift: u8,
            a_shift: u8,
        } {
            return .{
                .format = Format.fromSdl(self.value.format),
                .bits_per_pixel = self.value.bits_per_pixel,
                .bytes_per_pixel = self.value.bytes_per_pixel,
                .r_mask = self.value.Rmask,
                .g_mask = self.value.Gmask,
                .b_mask = self.value.Bmask,
                .a_mask = self.value.Amask,
                .r_bits = self.value.Rbits,
                .g_bits = self.value.Gbits,
                .b_bits = self.value.Bbits,
                .a_bits = self.value.Abits,
                .r_shift = self.value.Rshift,
                .g_shift = self.value.Gshift,
                .b_shift = self.value.Bshift,
                .a_shift = self.value.Ashift,
            };
        }

        /// Get RGB values from a pixel in the specified format.
        ///
        /// ## Function Parameters
        /// * `self`: Describes the pixel format.
        /// * `pixel`: A pixel value.
        /// * `palette`: An optional palette for indexed formats.
        ///
        /// ## Return Value
        /// Returns the RGB8 color value of the pixel.
        ///
        /// ## Remarks
        /// This function uses the entire 8-bit [0..255] range when converting color components from pixel formats with less than 8-bits per RGB component
        /// (e.g., a completely white pixel in 16-bit RGB565 format would return [0xff, 0xff, 0xff] not [0xf8, 0xfc, 0xf8]).
        ///
        /// ## Thread Safety
        /// It is safe to call this function from any thread, as long as the palette is not modified.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub fn getRgb(
            self: Details,
            pixel: Pixel,
            palette: ?Palette,
        ) struct { r: u8, g: u8, b: u8 } {
            var r: u8 = undefined;
            var g: u8 = undefined;
            var b: u8 = undefined;
            c.SDL_GetRGB(
                @intCast(pixel.value),
                &self.value,
                if (palette) |palette_val| palette_val.value else null,
                &r,
                &g,
                &b,
            );
            return .{ .r = r, .g = g, .b = b };
        }

        /// Get RGBA values from a pixel in the specified format.
        ///
        /// ## Function Parameters
        /// * `self`: Describes the pixel format.
        /// * `pixel`: A pixel value.
        /// * `palette`: An optional palette for indexed formats.
        ///
        /// ## Return Value
        /// Returns the RGB8 color value of the pixel.
        ///
        /// ## Remarks
        /// This function uses the entire 8-bit [0..255] range when converting color components from pixel formats with less than 8-bits per RGB component
        /// (e.g., a completely white pixel in 16-bit RGB565 format would return [0xff, 0xff, 0xff] not [0xf8, 0xfc, 0xf8]).
        ///
        /// If the surface has no alpha component, the alpha will be returned as 0xff (100% opaque).
        ///
        /// ## Thread Safety
        /// It is safe to call this function from any thread, as long as the palette is not modified.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub fn getRgba(
            self: Details,
            pixel: Pixel,
            palette: ?Palette,
        ) struct { r: u8, g: u8, b: u8, a: u8 } {
            var r: u8 = undefined;
            var g: u8 = undefined;
            var b: u8 = undefined;
            var a: u8 = undefined;
            c.SDL_GetRGBA(
                @intCast(pixel.value),
                &self.value,
                if (palette) |palette_val| palette_val.value else null,
                &r,
                &g,
                &b,
                &a,
            );
            return .{ .r = r, .g = g, .b = b, .a = a };
        }

        /// Initialize the format details.
        ///
        /// ## Function Parameters
        /// These are the zig-named members of the struct.
        ///
        /// ## Version
        /// This function is provided by zig-sdl3.
        pub fn init(
            format: ?Format,
            bits_per_pixel: u8,
            bytes_per_pixel: u8,
            r_mask: u32,
            g_mask: u32,
            b_mask: u32,
            a_mask: u32,
            r_bits: u8,
            g_bits: u8,
            b_bits: u8,
            a_bits: u8,
            r_shift: u8,
            g_shift: u8,
            b_shift: u8,
            a_shift: u8,
        ) Details {
            return .{
                .value = .{
                    .format = Format.toSdl(format),
                    .bits_per_pixel = bits_per_pixel,
                    .bytes_per_pixel = bytes_per_pixel,
                    .Rmask = r_mask,
                    .Gmask = g_mask,
                    .Bmask = b_mask,
                    .Amask = a_mask,
                    .Rbits = r_bits,
                    .Gbits = g_bits,
                    .Bbits = b_bits,
                    .Abits = a_bits,
                    .Rshift = r_shift,
                    .Gshift = g_shift,
                    .Bshift = b_shift,
                    .Ashift = a_shift,
                },
            };
        }

        /// Map an RGB triple to an opaque pixel value for a given pixel format.
        ///
        /// ## Function Parameters
        /// * `self`: The details describing the format.
        /// * `palette`: An optional palette for indexed formats.
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
        /// If the format has a palette (8-bit) the index of the closest matching color in the palette will be returned.
        ///
        /// If the specified pixel format has an alpha component it will be returned as all 1 bits (fully opaque).
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
            self: Details,
            palette: ?Palette,
            r: u8,
            g: u8,
            b: u8,
        ) Pixel {
            const ret = c.SDL_MapRGB(
                &self.value,
                if (palette) |palette_val| palette_val.value else null,
                r,
                g,
                b,
            );
            return Pixel{ .value = ret };
        }

        /// Map an RGBA quadruple to a pixel value for a given pixel format.
        ///
        /// ## Function Parameters
        /// * `self`: The details describing the format.
        /// * `palette`: An optional palette for indexed formats.
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
        /// If the specified pixel format has no alpha component the alpha value will be ignored (as it will be in formats with a palette).
        ///
        /// If the format has a palette (8-bit) the index of the closest matching color in the palette will be returned.
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
            self: Details,
            palette: ?Palette,
            r: u8,
            g: u8,
            b: u8,
            a: u8,
        ) Pixel {
            const ret = c.SDL_MapRGBA(
                &self.value,
                if (palette) |palette_val| palette_val.value else null,
                r,
                g,
                b,
                a,
            );
            return Pixel{ .value = ret };
        }
    };

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_PixelFormat) ?Format {
        if (value == c.SDL_PIXELFORMAT_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?Format) c.SDL_PixelFormat {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_PIXELFORMAT_UNKNOWN;
    }

    /// Define a pixel format.
    ///
    /// ## Function Parameters
    /// * `pixel_type`: The type of the new format.
    /// * `order`: Order of the new format.
    /// * `layout`: The layout of the new format.
    /// * `bits`: The number of bits per pixel of the new format.
    /// * `bytes`: The number of bytes per pixel of the new format.
    ///
    /// ## Return Value
    /// Returns a pixel format.
    ///
    /// ## Remarks
    /// For example, defining `pixels.Format.packed_rgba_8_8_8_8` looks like this:
    /// ```zig
    /// pixels.Format.define(
    ///     pixels.Type.packed32,
    ///     pixels.PackedOrder.rgba,
    ///     pixels.PackedLayout.bit_8_8_8_8,
    ///     32,
    ///     4,
    /// )
    /// ```
    ///
    /// ## Thread Safety
    /// It is safe to call this macro from any thread.
    ///
    /// ##Version
    /// This macro is available since SDL 3.2.0.
    pub inline fn define(
        comptime pixel_type: Type,
        order: OrderType(pixel_type),
        layout: ?PackedLayout,
        bits: u8,
        bytes: u8,
    ) Format {
        const ret = c.SDL_DEFINE_PIXELFORMAT(
            @intFromEnum(pixel_type),
            @intFromEnum(order),
            PackedLayout.toSdl(layout),
            @intCast(bits),
            @intCast(bytes),
        );
        return @enumFromInt(ret);
    }

    /// Define a format using 4 characters (Ex: YV12).
    ///
    /// ## Function Parameters
    /// * `c1`: The first character of the FourCC code.
    /// * `c2`: The second character of the FourCC code.
    /// * `c3`: The third character of the FourCC code.
    /// * `c4`: The fourth character of the FourCC code.
    ///
    /// ## Return Value
    /// Return a pixel format.
    ///
    /// ## Remarks
    /// Defining YV12 would be `pixels.Format.define4CC('Y', 'V', '1', '2')`.
    ///
    /// ## Thread Safety
    /// It is safe to call this macro from any thread.
    ///
    /// ## Version
    /// This macro is available since SDL 3.2.0.
    pub fn define4CC(
        c1: u8,
        c2: u8,
        c3: u8,
        c4: u8,
    ) Format {
        const ret = c.SDL_DEFINE_PIXELFOURCC(
            @as(c_uint, @intCast(c1)),
            @as(c_uint, @intCast(c2)),
            @as(c_uint, @intCast(c3)),
            @as(c_uint, @intCast(c4)),
        );
        return @enumFromInt(ret);
    }

    /// Define a pixel format.
    ///
    /// ## Function Parameters
    /// * `pixel_type`: The type of the new format.
    /// * `order`: Order of the new format.
    /// * `layout`: The layout of the new format.
    /// * `bits`: The number of bits per pixel of the new format.
    /// * `bytes`: The number of bytes per pixel of the new format.
    ///
    /// ## Return Value
    /// Returns a pixel format.
    ///
    /// ## Remarks
    /// For example, defining `pixels.Format.packed_rgba_8_8_8_8` looks like this:
    /// ```zig
    /// pixels.Format.define(
    ///     pixels.Type.packed32,
    ///     pixels.PackedOrder.rgba,
    ///     pixels.PackedLayout.bit_8_8_8_8,
    ///     32,
    ///     4,
    /// )
    /// ```
    ///
    /// ## Thread Safety
    /// It is safe to call this macro from any thread.
    ///
    /// ##Version
    /// This macro is available since SDL 3.2.0.
    pub fn defineRuntime(
        pixel_type: ?Type,
        order: c_uint,
        layout: ?PackedLayout,
        bits: u8,
        bytes: u8,
    ) Format {
        const ret = c.SDL_DEFINE_PIXELFORMAT(
            @as(c_int, @intCast(Type.toSdl(pixel_type))),
            @as(c_int, @intCast(order)),
            @as(c_int, @intCast(PackedLayout.toSdl(layout))),
            @as(c_int, @intCast(bits)),
            @as(c_int, @intCast(bytes)),
        );
        return @enumFromInt(ret);
    }

    /// Convert a bpp value and RGBA masks to an enumerated pixel format.
    ///
    /// ## Function Parameters
    /// * `bpp`: A bits per pixel value; usually 15, 16, or 32.
    /// * `r_mask`: The red mask for the format.
    /// * `g_mask`: The green mask for the format.
    /// * `b_mask`: The blue mask for the format.
    /// * `a_mask`: The alpha mask for the format.
    ///
    /// ## Return Value
    /// Returns a format value corresponding to the format masks or `null` if there is none.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fromMasks(
        bpp: u8,
        r_mask: u32,
        g_mask: u32,
        b_mask: u32,
        a_mask: u32,
    ) ?Format {
        const ret = c.SDL_GetPixelFormatForMasks(
            @intCast(bpp),
            r_mask,
            g_mask,
            b_mask,
            a_mask,
        );
        return Format.fromSdl(ret);
    }

    /// Get the bits per pixel of the format.
    ///
    /// ## Function Parameters
    /// * `self`: The format to check.
    ///
    /// ## Return Value
    /// Returns the bits-per-pixel of the format.
    ///
    /// ## Remarks
    /// FourCC formats will report zero here, as it rarely makes sense to measure them per-pixel.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getBitsPerPixel(
        self: Format,
    ) u8 {

        // Translate C break macro so do it manually.
        return if (self.is4cc()) 0 else @intCast((@intFromEnum(self) >> 8) & 0xff);
    }

    /// Get the bytes per pixel of the format.
    ///
    /// ## Function Parameters
    /// * `self`: The format to check.
    ///
    /// ## Return Value
    /// Returns the bytes-per-pixel of the format.
    ///
    /// ## Remarks
    /// FourCC formats do their best here, but many of them don't have a meaningful measurement of bytes per pixel.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getBytesPerPixel(
        self: Format,
    ) u8 {

        // Ported manually because translate C not working properly for this macro.
        return if (self.is4cc()) (if (self == .fourcc_yuy2 or self == .fourcc_uyvy or self == .fourcc_yvyu or self == .fourcc_p010) 2 else 1) else @intCast(@intFromEnum(self) & 0xff);
    }

    /// Create a `pixels.Details` structure corresponding to a pixel format.
    ///
    /// ## Function Parameters
    /// * `self`: A pixel format value.
    ///
    /// ## Return Value
    /// Returns a format details structure.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getDetails(
        self: Format,
    ) !Details {
        const ret = try errors.wrapCallNull(*const c.SDL_PixelFormatDetails, c.SDL_GetPixelFormatDetails(
            @intFromEnum(self),
        ));
        return .{ .value = (try errors.wrapCallNull(*const c.SDL_PixelFormatDetails, ret)).* };
    }

    /// If format was created by `define` rather than `define4CC`.
    ///
    /// ## Function Parameters
    /// * `self`: The pixel format to query.
    ///
    /// ## Return Value
    /// Returns the flags of format.
    ///
    /// ## Remarks
    /// This macro is generally not needed directly by an app, which should use specific tests, like `pixels.Format.is4cc()`, instead.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFlag(
        self: Format,
    ) bool {
        const ret = c.SDL_PIXELFLAG(
            @intFromEnum(self),
        );
        return ret != 0;
    }

    /// Get the layout component of the format.
    ///
    /// ## Function Parameters
    /// * `self`: The pixel format to query.
    ///
    /// ## Return Value
    /// Returns the layout of format.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getLayout(
        self: Format,
    ) ?PackedLayout {
        const ret = c.SDL_PIXELLAYOUT(
            @intFromEnum(self),
        );
        return PackedLayout.fromSdl(ret);
    }

    /// Convert one of the enumerated pixel formats to a bpp value and RGBA masks.
    ///
    /// ## Function Parameters
    /// * `self`: Pixel format value.
    ///
    /// ## Return Value
    /// Returns the bits per pixel, along with the RGBA mask values.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMasks(
        self: Format,
    ) !struct { bpp: u8, r_mask: u32, g_mask: u32, b_mask: u32, a_mask: u32 } {
        var bpp: c_int = undefined;
        var r_mask: u32 = undefined;
        var g_mask: u32 = undefined;
        var b_mask: u32 = undefined;
        var a_mask: u32 = undefined;
        const ret = c.SDL_GetMasksForPixelFormat(
            @intFromEnum(self),
            &bpp,
            &r_mask,
            &g_mask,
            &b_mask,
            &a_mask,
        );
        try errors.wrapCallBool(ret);
        return .{ .bpp = @intCast(bpp), .r_mask = r_mask, .g_mask = g_mask, .b_mask = b_mask, .a_mask = a_mask };
    }

    /// Get the human readable name of a pixel format.
    ///
    /// ## Function Parameters
    /// * `self`: The pixel format to query.
    ///
    /// ## Return Value
    /// Returns the human readable name of the specified pixel format or `null` if the format is not recognized.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Format,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetPixelFormatName(
            @intFromEnum(self),
        );
        const converted_ret = std.mem.span(ret);
        if (std.mem.eql(u8, converted_ret, "SDL_PIXELFORMAT_UNKNOWN"))
            return null;
        return converted_ret;
    }

    /// Get the order component of the format.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// Returns the order of format.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getOrder(
        self: Format,
    ) c_uint {
        const ret = c.SDL_PIXELORDER(
            @intFromEnum(self),
        );
        return @bitCast(ret);
    }

    /// Get the typed order component of the format.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// The order typed for the format.
    ///
    /// ## Remarks
    /// Do not run this on formats with an invalid type!
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getOrderTyped(
        self: Format,
    ) ?OrderType(self.getType().?) {
        const ret = c.SDL_PIXELORDER(
            @intFromEnum(self),
        );
        return OrderType(self.getType().?).fromSdl(ret);
    }

    /// Get the type component of the format.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// Returns the type of format.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getType(
        self: Format,
    ) ?Type {
        const ret = c.SDL_PIXELTYPE(
            @intFromEnum(self),
        );
        return Type.fromSdl(ret);
    }

    /// If the format has alpha.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// Returns true if the format has alpha, false otherwise.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn hasAlpha(
        self: Format,
    ) bool {
        if (self.isPacked() or self.isArray()) {
            const order = PackedOrder.fromSdl(self.getOrder());
            return order == .argb or
                order == .rgba or
                order == .abgr or
                order == .bgra;
        }
        return false;
    }

    /// If the format is 10-bit.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// Returns true if the format is 10-bit, false otherwise.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn is10Bit(
        self: Format,
    ) bool {
        const format = @intFromEnum(self);
        return !(c.SDL_ISPIXELFORMAT_FOURCC(format)) and ((c.SDL_PIXELTYPE(format) == c.SDL_PIXELTYPE_PACKED32) and (c.SDL_PIXELLAYOUT(format) == c.SDL_PACKEDLAYOUT_2101010));
    }

    /// If the format is 4cc.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// Returns true if the format has alpha, false otherwise.
    ///
    /// ## Remarks
    /// This covers custom and other unusual formats.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn is4cc(
        self: Format,
    ) bool {
        const ret = c.SDL_ISPIXELFORMAT_FOURCC(
            @intFromEnum(self),
        );
        return ret;
    }

    /// If the format is array.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// Returns true if the format is an array, false otherwise.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn isArray(
        self: Format,
    ) bool {
        const new_type = self.getType() orelse return false;
        return !self.is4cc() and (new_type == .array_u8 or new_type == .array_u16 or new_type == .array_u32 or new_type == .array_f16 or new_type == .array_f32);
    }

    /// If the format is floating point.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// Returns true if the format is 10-bit, false otherwise.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn isFloat(
        self: Format,
    ) bool {
        const new_type = self.getType() orelse return false;
        return !self.is4cc() and (new_type == .array_f16 or new_type == .array_f32);
    }

    /// If the format is indexed.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// Returns true if the format is indexed, false otherwise.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn isIndexed(
        self: Format,
    ) bool {
        const new_type = self.getType() orelse return false;
        return !self.is4cc() and (new_type == .index1 or new_type == .index2 or new_type == .index4 or new_type == .index8);
    }

    /// If the format is packed.
    ///
    /// ## Function Parameters
    /// * `self`: The format of the pixel.
    ///
    /// ## Return Value
    /// Returns true if the format is packed, false otherwise.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn isPacked(
        self: Format,
    ) bool {
        const new_type = self.getType() orelse return false;
        return !self.is4cc() and (new_type == .packed8 or new_type == .packed16 or new_type == .packed32);
    }
};

/// Colorspace matrix coefficients.
///
/// ## Remarks
/// These are as described by https://www.itu.int/rec/T-REC-H.273-201612-S/en.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const MatrixCoefficients = enum(c.SDL_MatrixCoefficients) {
    identity = c.SDL_MATRIX_COEFFICIENTS_IDENTITY,
    /// ITU-R BT.709-6.
    bt709 = c.SDL_MATRIX_COEFFICIENTS_BT709,
    unspecified = c.SDL_MATRIX_COEFFICIENTS_UNSPECIFIED,
    /// US FCC Title 47.
    fcc = c.SDL_MATRIX_COEFFICIENTS_FCC,
    /// ITU-R BT.470-6 System B, G / ITU-R BT.601-7 625, functionally the same as SDL_MATRIX_COEFFICIENTS_BT601.
    bt470bg = c.SDL_MATRIX_COEFFICIENTS_BT470BG,
    /// ITU-R BT.601-7 525.
    bt601 = c.SDL_MATRIX_COEFFICIENTS_BT601,
    /// SMPTE 240M.
    smpte240 = c.SDL_MATRIX_COEFFICIENTS_SMPTE240,
    ycgco = c.SDL_MATRIX_COEFFICIENTS_YCGCO,
    /// ITU-R BT.2020-2 non-constant luminance.
    bt2020_ncl = c.SDL_MATRIX_COEFFICIENTS_BT2020_NCL,
    /// ITU-R BT.2020-2 constant luminance.
    bt2020_cl = c.SDL_MATRIX_COEFFICIENTS_BT2020_CL,
    /// SMPTE ST 2085.
    smpte2085 = c.SDL_MATRIX_COEFFICIENTS_SMPTE2085,
    chroma_derived_ncl = c.SDL_MATRIX_COEFFICIENTS_CHROMA_DERIVED_NCL,
    chroma_derived_cl = c.SDL_MATRIX_COEFFICIENTS_CHROMA_DERIVED_CL,
    /// ITU-R BT.2100-0 ICTCP.
    ictcp = c.SDL_MATRIX_COEFFICIENTS_ICTCP,
    custom = c.SDL_MATRIX_COEFFICIENTS_CUSTOM,
};

/// Packed component layout.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const PackedLayout = enum(c.SDL_PackedLayout) {
    bit_3_3_2 = c.SDL_PACKEDLAYOUT_332,
    bit_4_4_4_4 = c.SDL_PACKEDLAYOUT_4444,
    bit_1_5_5_5 = c.SDL_PACKEDLAYOUT_1555,
    bit_5_5_5_1 = c.SDL_PACKEDLAYOUT_5551,
    bit_5_6_5 = c.SDL_PACKEDLAYOUT_565,
    bit_8_8_8_8 = c.SDL_PACKEDLAYOUT_8888,
    bit_2_10_10_10 = c.SDL_PACKEDLAYOUT_2101010,
    bit_10_10_10_2 = c.SDL_PACKEDLAYOUT_1010102,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_PackedLayout) ?PackedLayout {
        if (value == c.SDL_PACKEDLAYOUT_NONE)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?PackedLayout) c.SDL_PackedLayout {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_PACKEDLAYOUT_NONE;
    }
};

/// Packed component order, high bit -> low bit.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const PackedOrder = enum(c.SDL_PackedOrder) {
    xrgb = c.SDL_PACKEDORDER_XRGB,
    rgbx = c.SDL_PACKEDORDER_RGBX,
    argb = c.SDL_PACKEDORDER_ARGB,
    rgba = c.SDL_PACKEDORDER_RGBA,
    xbgr = c.SDL_PACKEDORDER_XBGR,
    bgrx = c.SDL_PACKEDORDER_BGRX,
    abgr = c.SDL_PACKEDORDER_ABGR,
    bgra = c.SDL_PACKEDORDER_BGRA,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_PackedOrder) ?PackedOrder {
        if (value == c.SDL_PACKEDORDER_NONE)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?PackedOrder) c.SDL_PackedOrder {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_PACKEDORDER_NONE;
    }
};

/// A set of indexed colors representing a palette.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Palette = struct {
    value: *c.SDL_Palette,

    /// Free a palette created earlier.
    ///
    /// ## Function Parameters
    /// * `self`: The palette to be freed.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as long as the palette is not modified or destroyed in another thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Palette,
    ) void {
        c.SDL_DestroyPalette(
            self.value,
        );
    }

    /// A set of indexed colors representing a palette.
    ///
    /// ## Function Parameters
    /// * `self`: Palette to get the colors of.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getColors(
        self: Palette,
    ) []const Color {
        return self.value.colors[0..@intCast(self.value.ncolors)];
    }

    /// Create a palette structure with the specified number of color entries.
    ///
    /// ## Function Parameters
    /// * `num_colors`: Represents the number of color entries in the color palette.
    ///
    /// ## Return Value
    /// Returns the palette structure.
    ///
    /// ## Remarks
    /// The palette entries are initialized to white.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        num_colors: usize,
    ) !Palette {
        const ret = c.SDL_CreatePalette(
            @intCast(num_colors),
        );
        return Palette{ .value = try errors.wrapCallNull(*c.SDL_Palette, ret) };
    }

    /// Set a range of colors in a palette.
    ///
    /// ## Function Parameters
    /// * `self`: The palette to modify.
    /// * `colors`: Colors to copy to the palette.
    /// * `first_color`: The index of the first palette entry to modify.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, as long as the palette is not modified or destroyed in another thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setColors(
        self: Palette,
        colors: []const Color,
        first_color: usize,
    ) !void {
        const ret = c.SDL_SetPaletteColors(
            self.value,
            colors.ptr,
            @intCast(first_color),
            @intCast(colors.len),
        );
        return errors.wrapCallBool(ret);
    }
};

/// Raw pixel value.
///
/// ## Version
/// This struct is provided by zig-sdl3.
pub const Pixel = packed struct {
    value: u32,
};

/// Colorspace transfer characteristics.
///
/// ## Remarks
/// These are as described by https://www.itu.int/rec/T-REC-H.273-201612-S/en.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const TransferCharacteristics = enum(c.SDL_TransferCharacteristics) {
    /// Rec. ITU-R BT.709-6 / ITU-R BT1361.
    bt709 = c.SDL_TRANSFER_CHARACTERISTICS_BT709,
    unspecified = c.SDL_TRANSFER_CHARACTERISTICS_UNSPECIFIED,
    /// ITU-R BT.470-6 System M / ITU-R BT1700 625 PAL & SECAM.
    gamma22 = c.SDL_TRANSFER_CHARACTERISTICS_GAMMA22,
    /// ITU-R BT.470-6 System B, G.
    gamma28 = c.SDL_TRANSFER_CHARACTERISTICS_GAMMA28,
    /// SMPTE ST 170M / ITU-R BT.601-7 525 or 625.
    bt601 = c.SDL_TRANSFER_CHARACTERISTICS_BT601,
    /// SMPTE ST 240M.
    smpte240 = c.SDL_TRANSFER_CHARACTERISTICS_SMPTE240,
    linear = c.SDL_TRANSFER_CHARACTERISTICS_LINEAR,
    log100 = c.SDL_TRANSFER_CHARACTERISTICS_LOG100,
    log100_sqrt10 = c.SDL_TRANSFER_CHARACTERISTICS_LOG100_SQRT10,
    /// IEC 61966-2-4.
    iec61966 = c.SDL_TRANSFER_CHARACTERISTICS_IEC61966,
    /// ITU-R BT1361 Extended Colour Gamut.
    bt1361 = c.SDL_TRANSFER_CHARACTERISTICS_BT1361,
    /// IEC 61966-2-1 (sRGB or sYCC).
    srgb = c.SDL_TRANSFER_CHARACTERISTICS_SRGB,
    /// ITU-R BT2020 for 10-bit system.
    bt2020_10bit = c.SDL_TRANSFER_CHARACTERISTICS_BT2020_10BIT,
    /// ITU-R BT2020 for 12-bit system.
    bt2020_12bit = c.SDL_TRANSFER_CHARACTERISTICS_BT2020_12BIT,
    /// SMPTE ST 2084 for 10-, 12-, 14- and 16-bit system.
    pq = c.SDL_TRANSFER_CHARACTERISTICS_PQ,
    /// SMPTE ST 428-1.
    smpte428 = c.SDL_TRANSFER_CHARACTERISTICS_SMPTE428,
    /// ARIB STD-B67, known as hybrid log-gamma (HLG).
    hlg = c.SDL_TRANSFER_CHARACTERISTICS_HLG,
    custom = c.SDL_TRANSFER_CHARACTERISTICS_CUSTOM,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_TransferCharacteristics) ?TransferCharacteristics {
        if (value == c.SDL_TRANSFER_CHARACTERISTICS_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?TransferCharacteristics) c.SDL_TransferCharacteristics {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_TRANSFER_CHARACTERISTICS_UNKNOWN;
    }
};

/// Pixel type.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Type = enum(c.SDL_PixelType) {
    index1 = c.SDL_PIXELTYPE_INDEX1,
    index2 = c.SDL_PIXELTYPE_INDEX2,
    index4 = c.SDL_PIXELTYPE_INDEX4,
    index8 = c.SDL_PIXELTYPE_INDEX8,
    packed8 = c.SDL_PIXELTYPE_PACKED8,
    packed16 = c.SDL_PIXELTYPE_PACKED16,
    packed32 = c.SDL_PIXELTYPE_PACKED32,
    array_u8 = c.SDL_PIXELTYPE_ARRAYU8,
    array_u16 = c.SDL_PIXELTYPE_ARRAYU16,
    array_u32 = c.SDL_PIXELTYPE_ARRAYU32,
    array_f16 = c.SDL_PIXELTYPE_ARRAYF16,
    array_f32 = c.SDL_PIXELTYPE_ARRAYF32,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_PixelType) ?Type {
        if (value == c.SDL_PIXELTYPE_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?Type) c.SDL_PixelType {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_PIXELTYPE_UNKNOWN;
    }
};

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
pub fn mapSurfaceRgb(
    surface_value: surface.Surface,
    r: u8,
    g: u8,
    b: u8,
) Pixel {
    const ret = c.SDL_MapSurfaceRGB(
        surface_value.value,
        r,
        g,
        b,
    );
    return Pixel{ .value = ret };
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
pub fn mapSurfaceRgba(
    surface_value: surface.Surface,
    r: u8,
    g: u8,
    b: u8,
    a: u8,
) Pixel {
    const ret = c.SDL_MapSurfaceRGBA(
        surface_value.value,
        r,
        g,
        b,
        a,
    );
    return Pixel{ .value = ret };
}

/// Get the order type of a pixel type.
///
/// ## Function Parameters
/// * `pixel_type`: The pixel type.
///
/// ## Return Value
/// The type for the pixel order.
///
/// ## Version
/// This function is provided by zig-sdl3.
inline fn OrderType(
    pixel_type: Type,
) type {
    return switch (pixel_type) {
        .index1, .index2, .index4, .index8 => BitmapOrder,
        .packed8, .packed16, .packed32 => PackedOrder,
        .arrayU8, .arrayU16, .arrayU32, .arrayF16, .arrayF32 => ArrayOrder,
    };
}

// Pixel tests.
test "Pixels" {
    errors.refAllDeclsRecursive(@This());
}
