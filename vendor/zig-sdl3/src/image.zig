const c = @import("c");
const errors = @import("errors.zig");
const io_stream = @import("io_stream.zig");
const render = @import("render.zig");
const version = @import("version.zig");
const surface = @import("surface.zig");
const std = @import("std");

comptime {
    if (!@hasDecl(c, "IMG_Animation"))
        @compileError("The ext_image feature is either not enabled or not working!");
}

/// Animated image support.
///
/// ## Remarks
/// Currently only animated GIFs and WEBP images are supported.
///
/// ## Version
/// This struct is available since SDL image 3.0.0.
pub const Animation = struct {
    value: *c.IMG_Animation,

    /// Dispose of an animation and free its resources.
    ///
    /// ## Function Parameters
    /// * `self`: The animation to dispose of.
    ///
    /// ## Remarks
    /// The provided anim pointer is not valid once this call returns.
    ///
    /// ## Version
    /// This function is available since SDL image 3.0.0.
    pub fn deinit(
        self: Animation,
    ) void {
        c.IMG_FreeAnimation(
            self.value,
        );
    }

    /// Get a frame of animation.
    ///
    /// ## Function Parameters
    /// * `self`: The animation to query.
    /// * `index`: The index of the frame.
    ///
    /// ## Return Value
    /// Returns the animation frame surface and delay in that order, or `null` if out of bounds.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getFrame(
        self: Animation,
        index: usize,
    ) ?struct { surface.Surface, usize } {
        if (index >= self.getNumFrames())
            return null;
        return .{
            .{ .value = self.value.frames[index] },
            @intCast(self.value.delays[index]),
        };
    }

    /// Get the height of an animation.
    ///
    /// ## Function Parameters
    /// * `self`: The animation to query.
    ///
    /// ## Return Value
    /// Returns the height of the animation.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getHeight(
        self: Animation,
    ) usize {
        return @intCast(self.value.h);
    }

    /// Get the number of frames of an animation.
    ///
    /// ## Function Parameters
    /// * `self`: The animation to query.
    ///
    /// ## Return Value
    /// Returns the number of frames of the animation.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getNumFrames(
        self: Animation,
    ) usize {
        return @intCast(self.value.count);
    }

    /// Get the width of an animation.
    ///
    /// ## Function Parameters
    /// * `self`: The animation to query.
    ///
    /// ## Return Value
    /// Returns the width of the animation.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn getWidth(
        self: Animation,
    ) usize {
        return @intCast(self.value.w);
    }

    /// Load an animation from a file.
    pub fn init(
        file: [:0]const u8,
    ) !Animation {
        const ret = c.IMG_LoadAnimation(
            file,
        );
        if (ret == null)
            return error.SdlError;
        return Animation{ .value = ret };
    }

    /// Load an animation from an SDL_IOStream.
    pub fn initFromIo(
        source: io_stream.Stream,
        close_when_done: bool,
    ) !Animation {
        const ret = c.IMG_LoadAnimation_IO(
            source.value,
            close_when_done,
        );
        if (ret == null)
            return error.SdlError;
        return Animation{ .value = ret };
    }

    /// Load an animation from an SDL datasource.
    pub fn initFromTypedIo(
        source: io_stream.Stream,
        close_when_done: bool,
        file_type: [:0]const u8,
    ) !Animation {
        const ret = c.IMG_LoadAnimationTyped_IO(
            source.value,
            close_when_done,
            file_type,
        );
        if (ret == null)
            return error.SdlError;
        return Animation{ .value = ret };
    }

    /// Load a GIF animation directly.
    pub fn initFromGifIo(
        source: io_stream.Stream,
    ) !Animation {
        const ret = c.IMG_LoadGIFAnimation_IO(
            source.value,
        );
        if (ret == null)
            return error.SdlError;
        return Animation{ .value = ret };
    }

    /// Load a WEBP animation directly.
    pub fn initFromWebpIo(
        source: io_stream.Stream,
    ) !Animation {
        const ret = c.IMG_LoadWEBPAnimation_IO(
            source.value,
        );
        if (ret == null)
            return error.SdlError;
        return Animation{ .value = ret };
    }
};

/// Detect AVIF image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is AVIF data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isAvif(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isAVIF(
        src.value,
    );
    return ret;
}

/// Detect BMP image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is BMP data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isBmp(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isBMP(
        src.value,
    );
    return ret;
}

/// Detect CUR image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is CUR data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isCur(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isCUR(
        src.value,
    );
    return ret;
}

/// Detect GIF image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is GIF data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isGif(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isGIF(
        src.value,
    );
    return ret;
}

/// Detect ICO image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is ICO data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isIco(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isICO(
        src.value,
    );
    return ret;
}

/// Detect JPG image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is JPG data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isJpg(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isJPG(
        src.value,
    );
    return ret;
}

/// Detect JXL image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is JXL data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isJxl(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isJXL(
        src.value,
    );
    return ret;
}

/// Detect LBM image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is LBM data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isLbm(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isLBM(
        src.value,
    );
    return ret;
}

/// Detect PCX image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is PCX data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isPcx(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isPCX(
        src.value,
    );
    return ret;
}

/// Detect PNG image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is PNG data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isPng(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isPNG(
        src.value,
    );
    return ret;
}

/// Detect PNM image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is PNM data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isPnm(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isPNM(
        src.value,
    );
    return ret;
}

/// Detect QOI image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is QOI data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isQoi(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isQOI(
        src.value,
    );
    return ret;
}

/// Detect SVG image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is SVG data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isSvg(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isSVG(
        src.value,
    );
    return ret;
}

/// Detect TIF image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is TIF data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isTif(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isTIF(
        src.value,
    );
    return ret;
}

/// Detect WEBP image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is WEBP data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isWebp(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isWEBP(
        src.value,
    );
    return ret;
}

/// Detect XCF image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is XCF data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isXcf(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isXCF(
        src.value,
    );
    return ret;
}

/// Detect XPM image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is XPM data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isXpm(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isXPM(
        src.value,
    );
    return ret;
}

/// Detect XV image data on a readable/seekable `io.Stream`.
///
/// ## Function Parameters
/// * `src`: A seekable/readable IO stream to provide image data.
///
/// ## Return Value
/// Returns true if this is XV data, false otherwise.
///
/// ## Remarks
/// This function attempts to determine if a file is a given filetype, reading the least amount possible from the `io.Stream` (usually a few bytes).
///
/// There is no distinction made between "not the filetype in question" and basic i/o errors.
///
/// This function will always attempt to seek `src` back to where it started when this function was called, but it will not report any errors in doing so,
/// but assuming seeking works, this means you can immediately use this with a different "is type" function, or load the image without further seeking.
///
/// You do not need to call this function to load data; SDL image can work to determine file type in many cases in its standard load functions.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn isXv(
    src: io_stream.Stream,
) bool {
    const ret = c.IMG_isXV(
        src.value,
    );
    return ret;
}

/// Load an image from a filesystem path into a software surface.
///
/// ## Function Parameters
/// * `path`: A path on the filesystem to load an image from.
///
/// ## Return Value
/// Returns a new SDL surface.
///
/// ## Remarks
/// An surface is a buffer of pixels in memory accessible by the CPU.
/// Use this if you plan to hand the data to something else or manipulate it further in code.
///
/// There are no guarantees about what format the new surface data will be; in many cases, SDL image will attempt to supply a surface that exactly matches the provided image,
/// but in others it might have to convert (either because the image is in a format that SDL doesn't directly support or because it's compressed data that could reasonably
/// uncompress to various formats and SDL image had to pick one).
/// You can inspect an SDL Surface for its specifics, and use `surface.Surface.convert()` to then migrate to any supported format.
///
/// If the image format supports a transparent pixel, SDL will set the colorkey for the surface.
/// You can enable RLE acceleration on the surface afterwards by calling: `surface.Surface.setColorKey(image, image.getFormat().color_key);`
///
/// There is a separate function to read files from an SDL IO stream, if you need an i/o abstraction to provide data from anywhere instead of a simple filesystem read;
/// that function is `loadIo()`.
///
/// If you are using SDL's 2D rendering API, there is an equivalent call to load images directly into a `render.Texture` for use by the GPU
/// without using a software surface: call `image.loadTexture()` instead.
///
/// When done with the returned surface, the app should dispose of it with a call to `surface.Surface.deinit()`.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn loadFile(
    path: [:0]const u8,
) !surface.Surface {
    const ret = c.IMG_Load(
        path,
    );
    return surface.Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
}

/// Load an image from an SDL data source into a software surface.
///
/// ## Function Parameters
/// * `src`: An SDL IO stream that data will be read from.
/// * `close_when_done`: True to close/free the SDL IO stream before returning, false to leave it open.
///
/// ## Return Value
/// Returns a new SDL surface.
///
/// ## Remarks
/// An surface is a buffer of pixels in memory accessible by the CPU.
/// Use this if you plan to hand the data to something else or manipulate it further in code.
///
/// There are no guarantees about what format the new surface data will be; in many cases, SDL image will attempt to supply a surface that exactly matches the provided image,
/// but in others it might have to convert (either because the image is in a format that SDL doesn't directly support or because it's compressed data that could reasonably
/// uncompress to various formats and SDL image had to pick one).
/// You can inspect an SDL Surface for its specifics, and use `surface.Surface.convert()` to then migrate to any supported format.
///
/// If the image format supports a transparent pixel, SDL will set the colorkey for the surface.
/// You can enable RLE acceleration on the surface afterwards by calling: `surface.Surface.setColorKey(image, image.getFormat().color_key);`
///
/// There is a separate function to read files from disk without having to deal with SDL IO stream: `image.loadFile("filename.jpg")` will call this function and manage those details for you,
/// determining the file type from the filename's extension.
///
/// There is also `image.loadTypedIo()`, which is equivalent to this function except a file extension (like "BMP", "JPG", etc) can be specified,
/// in case SDL_image cannot autodetect the file format.
///
/// If you are using SDL's 2D rendering API, there is an equivalent call to load images directly into a `render.Texture` for use by the GPU
/// without using a software surface: call `image.loadTexture()` instead.
///
/// When done with the returned surface, the app should dispose of it with a call to `surface.Surface.deinit()`.
///
/// ## Version
/// This function is available since SDL image 3.0.0.
pub fn loadIo(
    src: io_stream.Stream,
    close_when_done: bool,
) !surface.Surface {
    const ret = c.IMG_Load_IO(
        src.value,
        close_when_done,
    );
    return surface.Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
}

/// SDL image version information.
pub const Version = struct {
    value: c_int,
    /// SDL version compiled against.
    pub const compiled_against = Version{ .value = c.SDL_IMAGE_VERSION };

    /// Create an SDL image version number.
    pub fn make(
        major: u32,
        minor: u32,
        micro: u32,
    ) Version {
        const ret = c.SDL_VERSIONNUM(
            @as(c_int, @intCast(major)),
            @as(c_int, @intCast(minor)),
            @as(c_int, @intCast(micro)),
        );
        return Version{ .value = ret };
    }

    /// Major version number.
    pub fn getMajor(
        self: version.Version,
    ) u32 {
        const ret = c.SDL_VERSIONNUM_MAJOR(
            self.value,
        );
        return @intCast(ret);
    }

    /// Minor version number.
    pub fn getMinor(
        self: version.Version,
    ) u32 {
        const ret = c.SDL_VERSIONNUM_MINOR(
            self.value,
        );
        return @intCast(ret);
    }

    /// Micro version number.
    pub fn getMicro(
        self: version.Version,
    ) u32 {
        const ret = c.SDL_VERSIONNUM_MICRO(
            self.value,
        );
        return @intCast(ret);
    }

    /// Check if the SDL image version is at least greater than the given one.
    pub fn atLeast(
        major: u32,
        minor: u32,
        micro: u32,
    ) bool {
        const ret = c.SDL_IMAGE_VERSION_ATLEAST(
            major,
            minor,
            micro,
        );
        return ret;
    }

    /// Get the version of SDL image that is linked against your program. Possibly different than the compiled against version.
    pub fn get() Version {
        const ret = c.IMG_Version();
        return Version{ .value = ret };
    }
};

/// Load an image from an SDL data source into a software surface.
pub fn loadTypedIo(
    source: io_stream.Stream,
    close_when_done: bool,
    file_type: [:0]const u8,
) !surface.Surface {
    const ret = c.IMG_LoadTyped_IO(
        source.value,
        close_when_done,
        file_type,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load an image from a filesystem path into a GPU texture.
pub fn loadTexture(
    renderer: render.Renderer,
    path: [:0]const u8,
) !render.Texture {
    const ret = c.IMG_LoadTexture(
        renderer.value,
        path,
    );
    if (ret == null)
        return error.SdlError;
    return render.Texture{ .value = ret };
}

/// Load an image from an SDL data source into a GPU texture.
pub fn loadTextureIo(
    renderer: render.Renderer,
    source: io_stream.Stream,
    close_when_done: bool,
) !render.Texture {
    const ret = c.IMG_LoadTexture_IO(
        renderer.value,
        source.value,
        close_when_done,
    );
    if (ret == null)
        return error.SdlError;
    return render.Texture{ .value = ret };
}

/// Load an image from an SDL data source into a GPU texture.
pub fn loadTextureTypedIo(
    renderer: render.Renderer,
    source: io_stream.Stream,
    close_when_done: bool,
    file_type: [:0]const u8,
) !render.Texture {
    const ret = c.IMG_LoadTextureTyped_IO(
        renderer.value,
        source.value,
        close_when_done,
        file_type,
    );
    if (ret == null)
        return error.SdlError;
    return render.Texture{ .value = ret };
}

/// Load a AVIF image directly.
pub fn loadAvifIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadAVIF_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a ICO image directly.
pub fn loadIcoIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadICO_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a CUR image directly.
pub fn loadCurIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadCUR_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a BMP image directly.
pub fn loadBmpIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadBMP_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a GIF image directly.
pub fn loadGifIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadGIF_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a JPG image directly.
pub fn loadJpgIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadJPG_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a JXL image directly.
pub fn loadJxlIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadJXL_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a LBM image directly.
pub fn loadLbmIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadLBM_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a PCX image directly.
pub fn loadPcxIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadPCX_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a PNG image directly.
pub fn loadPngIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadPNG_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a PNM image directly.
pub fn loadPnmIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadPNM_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a SVG image directly.
pub fn loadSvgIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadSVG_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a QOI image directly.
pub fn loadQoiIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadQOI_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a TGA image directly.
pub fn loadTgaIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadTGA_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a TIF image directly.
pub fn loadTifIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadTIF_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a XCF image directly.
pub fn loadXcfIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadXCF_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a XPM image directly.
pub fn loadXpmIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadXPM_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a XV image directly.
pub fn loadXvIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadXV_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load a Webp image directly.
pub fn loadWebpIo(
    source: io_stream.Stream,
) !surface.Surface {
    const ret = c.IMG_LoadWEBP_IO(
        source.value,
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load an SVG image, scaled to a specific size.
pub fn loadSizedSvgIo(
    source: io_stream.Stream,
    width: usize,
    height: usize,
) !surface.Surface {
    const ret = c.IMG_LoadSizedSVG_IO(
        source.value,
        @intCast(width),
        @intCast(height),
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load an XPM image from a memory array.
pub fn readXpmFromArray(
    xpm: [:null]const ?[*:0]const u8,
) !surface.Surface {
    const ret = c.IMG_ReadXPMFromArray(
        @ptrCast(@constCast(xpm.ptr)),
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Load an XPM image from a memory array.
pub fn readXpmFromArrayToRgb8888(
    xpm: [:null]const ?[*:0]const u8,
) !surface.Surface {
    const ret = c.IMG_ReadXPMFromArrayToRGB888(
        @ptrCast(@constCast(xpm.ptr)),
    );
    if (ret == null)
        return error.SdlError;
    return surface.Surface{ .value = ret };
}

/// Save an SDL_Surface into a AVIF image file.
pub fn saveAvif(
    source: surface.Surface,
    file: [:0]const u8,
    quality: u7,
) !void {
    const ret = c.IMG_SaveAVIF(
        source.value,
        file,
        @intCast(quality),
    );
    if (!ret)
        return error.SdlError;
}

/// Save an SDL_Surface into AVIF image data, via an SDL_IOStream.
pub fn saveAvifIo(
    source: surface.Surface,
    dst: io_stream.Stream,
    close_when_done: bool,
    quality: u7,
) !void {
    const ret = c.IMG_SaveAVIF_IO(
        source.value,
        dst.value,
        close_when_done,
        @intCast(quality),
    );
    if (!ret)
        return error.SdlError;
}

/// Save an SDL_Surface into a PNG image file.
pub fn savePng(
    source: surface.Surface,
    file: [:0]const u8,
) !void {
    const ret = c.IMG_SavePNG(
        source.value,
        file,
    );
    if (!ret)
        return error.SdlError;
}

/// Save an SDL_Surface into PNG image data, via an SDL_IOStream.
pub fn savePngIo(
    source: surface.Surface,
    dst: io_stream.Stream,
    close_when_done: bool,
) !void {
    const ret = c.IMG_SavePNG_IO(
        source.value,
        dst.value,
        close_when_done,
    );
    if (!ret)
        return error.SdlError;
}

/// Save an SDL_Surface into a JPG image file.
pub fn saveJpg(
    source: surface.Surface,
    file: [:0]const u8,
    quality: u7,
) !void {
    const ret = c.IMG_SaveJPG(
        source.value,
        file,
        @intCast(quality),
    );
    if (!ret)
        return error.SdlError;
}

/// Save an SDL_Surface into JPG image data, via an SDL_IOStream.
pub fn saveJpgIo(
    source: surface.Surface,
    dst: io_stream.Stream,
    close_when_done: bool,
    quality: u7,
) !void {
    const ret = c.IMG_SaveJPG_IO(
        source.value,
        dst.value,
        close_when_done,
        @intCast(quality),
    );
    if (!ret)
        return error.SdlError;
}

test "Image" {
    errors.refAllDeclsRecursive(@This());
}
