const c = @import("c");
const errors = @import("errors.zig");
const pixels = @import("pixels.zig");
const properties = @import("properties.zig");
const rect = @import("rect.zig");
const std = @import("std");
const surface = @import("surface.zig");
const video = @import("video.zig");

/// Callback used for hit-testing.
///
/// ## Function Parameters
/// `win`: The window where hit-testing was set on.
/// `area`: A point which should be hit-tested.
/// `user_data`: What was passed as callback_data to `video.Window.setHitTest()`.
///
/// ## Return Value
/// Returns a hit test value.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn HitTest(comptime UserData: type) type {
    return *const fn (
        win: video.Window,
        area: rect.IPoint,
        user_data: ?*UserData,
    ) HitTestResult;
}

/// Window progress state.
///
/// ## Version
/// This enum is available since SDL 3.2.8.
pub const ProgressState = enum(c.SDL_ProgressState) {
    /// The progress bar is shown in a indeterminate state.
    indeterminate,
    /// The progress bar is shown in a normal state.
    normal,
    /// The progress bar is shown in a paused state.
    paused,
    /// The progress bar is shown in a state indicating the application had an error.
    err,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_ProgressState) !?ProgressState {
        if (value == c.SDL_PROGRESS_STATE_INVALID) {
            errors.callErrorCallback();
            return error.SdlError;
        }
        if (value == c.SDL_PROGRESS_STATE_NONE)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?ProgressState) c.SDL_ProgressState {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_PROGRESS_STATE_NONE;
    }
};

/// System theme.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const SystemTheme = enum(c.SDL_SystemTheme) {
    /// Light colored theme.
    light = c.SDL_SYSTEM_THEME_LIGHT,
    /// Dark colored theme.
    dark = c.SDL_SYSTEM_THEME_DARK,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_SystemTheme) ?SystemTheme {
        if (value == c.SDL_SYSTEM_THEME_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?SystemTheme) c.SDL_SystemTheme {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_SYSTEM_THEME_UNKNOWN;
    }
};

/// This is a unique for a display for the time it is connected to the system, and is never reused for the lifetime of the application.
///
/// ## Remarks
/// If the display is disconnected and reconnected, it will get a new ID.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Display = packed struct {
    value: c.SDL_DisplayID,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(Display) == @sizeOf(c.SDL_DisplayID));
    }

    /// The structure that defines a display mode.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const Mode = struct {
        /// The display this mode is associated with.
        display: ?Display,
        /// Pixel format.
        format: ?pixels.Format,
        /// Width.
        width: usize,
        /// Height.
        height: usize,
        /// Scale converting size to pixels (e.g. a 1920x1080 mode with 2.0 scale would have 3840x2160 pixels).
        pixel_density: f32,
        /// Refresh rate (or `null` for unspecified).
        refresh_rate: ?f32,
        /// Precise refresh rate numerator (or 0 for unspecified).
        refresh_rate_numerator: u32,
        /// Precise refresh rate denominator.
        refresh_rate_denominator: u32,
        /// Private.
        internal: Data,

        /// Internal display mode data.
        ///
        /// ## Remarks
        /// This lives as a field in `video.Display.Mode`, as opaque data.
        ///
        /// ## Version
        /// This struct is available since SDL 3.2.0.
        pub const Data = ?*c.SDL_DisplayModeData;

        /// Convert from SDL.
        pub fn fromSdl(mode: c.SDL_DisplayMode) Mode {
            return .{
                .display = if (mode.displayID == 0) null else .{ .value = mode.displayID },
                .format = pixels.Format.fromSdl(mode.format),
                .width = @intCast(mode.w),
                .height = @intCast(mode.h),
                .pixel_density = mode.pixel_density,
                .refresh_rate = if (mode.refresh_rate == 0) null else mode.refresh_rate,
                .refresh_rate_numerator = @intCast(mode.refresh_rate_numerator),
                .refresh_rate_denominator = @intCast(mode.refresh_rate_denominator),
                .internal = mode.internal,
            };
        }

        /// Convert to SDL.
        pub fn toSdl(self: Mode) c.SDL_DisplayMode {
            return .{
                .displayID = if (self.display) |val| val.value else 0,
                .format = pixels.Format.toSdl(self.format),
                .w = @intCast(self.width),
                .h = @intCast(self.height),
                .pixel_density = self.pixel_density,
                .refresh_rate = if (self.refresh_rate) |val| val else 0,
                .refresh_rate_numerator = @intCast(self.refresh_rate_numerator),
                .refresh_rate_denominator = @intCast(self.refresh_rate_denominator),
                .internal = self.internal,
            };
        }
    };

    /// Display orientation values; the way a display is rotated.
    ///
    /// ## Version
    /// This enum is available since SDL 3.2.0.
    pub const Orientation = enum(c.SDL_DisplayOrientation) {
        /// The display is in landscape mode, with the right side up, relative to portrait mode.
        landscape = c.SDL_ORIENTATION_LANDSCAPE,
        /// The display is in landscape mode, with the left side up, relative to portrait mode.
        landscape_flipped = c.SDL_ORIENTATION_LANDSCAPE_FLIPPED,
        /// The display is in portrait mode.
        portrait = c.SDL_ORIENTATION_PORTRAIT,
        /// The display is in portrait mode, upside down.
        portrait_flipped = c.SDL_ORIENTATION_PORTRAIT_FLIPPED,

        /// Convert from SDL.
        pub fn fromSdl(val: c.SDL_DisplayOrientation) ?Orientation {
            return switch (val) {
                c.SDL_ORIENTATION_LANDSCAPE => .landscape,
                c.SDL_ORIENTATION_LANDSCAPE_FLIPPED => .landscape_flipped,
                c.SDL_ORIENTATION_PORTRAIT => .portrait,
                c.SDL_ORIENTATION_PORTRAIT_FLIPPED => .portrait_flipped,
                else => null,
            };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: ?Orientation) c.SDL_DisplayOrientation {
            const val = self orelse return c.SDL_ORIENTATION_UNKNOWN;
            return @intFromEnum(val);
        }
    };

    /// Display properties.
    ///
    /// ## Version
    /// Provided by zig-sdl3.
    pub const Properties = struct {
        /// True if the display has HDR headroom above the SDR white point.
        /// This is for informational and diagnostic purposes only, as not all platforms provide this information at the display level.
        hdr_enabled: ?bool,
        /// The "panel orientation" property for the display in degrees of clockwise rotation.
        /// Note that this is provided only as a hint, and the application is responsible for any coordinate transformations needed to conform to the requested display orientation.
        kmsdrm_panel_orientation: ?i64,
        /// The `wl_output` associated with the display.
        wayland_wl_output: ?struct { value: ?*anyopaque },
        /// The monitor handle (`HMONITOR`) associated with the display.
        windows_hmonitor: ?struct { value: ?*anyopaque },

        /// Get properties from SDL.
        pub fn fromSdl(props: properties.Group) Properties {
            return .{
                .hdr_enabled = if (props.get(c.SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN)) |val| val.boolean else null,
                .kmsdrm_panel_orientation = if (props.get(c.SDL_PROP_DISPLAY_KMSDRM_PANEL_ORIENTATION_NUMBER)) |val| val.number else null,
                .wayland_wl_output = if (props.get(c.SDL_PROP_DISPLAY_WAYLAND_WL_OUTPUT_POINTER)) |val| .{ .value = val.pointer } else null,
                .windows_hmonitor = if (props.get(c.SDL_PROP_DISPLAY_WINDOWS_HMONITOR_POINTER)) |val| .{ .value = val.pointer } else null,
            };
        }
    };

    /// Get the closest match to the requested display mode.
    ///
    /// ## Function Parameters
    /// * `self`: The instance ID of the display to query.
    /// * `width`: The width in pixels of the desired display mode.
    /// * `height`: The height in pixels of the desired display mode.
    /// * `refresh_rate`: The refresh rate of the desired display mode, or `null` for the desktop refresh rate.
    /// * `include_high_density_modes`: Boolean to include high density modes in the search.
    ///
    /// ## Return Value
    /// A display mode with the closest display mode equal to or larger than the desired mode.
    /// Will return an error if any mode could not be found, or all modes are smaller.
    ///
    /// ## Remarks
    /// The available display modes are scanned and closest is filled in with the closest mode matching the requested mode and returned.
    /// The mode format and refresh rate default to the desktop mode if they are set to 0.
    /// The modes are scanned with size being first priority, format being second priority, and finally checking the refresh rate.
    /// If all the available modes are too small, then an error is returned.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getClosestFullscreenMode(
        self: Display,
        width: usize,
        height: usize,
        refresh_rate: ?f32,
        include_high_density_modes: bool,
    ) !Mode {
        var mode: c.SDL_DisplayMode = undefined;
        const ret = c.SDL_GetClosestFullscreenDisplayMode(
            self.value,
            @intCast(width),
            @intCast(height),
            if (refresh_rate) |val| val else 0,
            include_high_density_modes,
            &mode,
        );
        try errors.wrapCallBool(ret);
        return Mode.fromSdl(mode);
    }

    /// Get information about the current display mode.
    ///
    /// ## Function Parameters
    /// * `self`: The display to query.
    ///
    /// ## Return Value
    /// Returns the desktop display mode.
    ///
    /// ## Remarks
    /// There's a difference between this function and `video.Display.getDesktopMode()` when SDL runs fullscreen and has changed the resolution.
    /// In that case this function will return the current display mode, and not the previous native display mode.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getCurrentMode(
        self: Display,
    ) !Mode {
        const ret = c.SDL_GetCurrentDisplayMode(self.value);
        const mode = try errors.wrapCallCPtrConst(c.SDL_DisplayMode, ret);
        return Mode.fromSdl(mode[0]);
    }

    /// Get the orientation of a display.
    ///
    /// ## Function Parameters
    /// * `self`: The display to query.
    ///
    /// ## Return Value
    /// Returns the orientation value of the display.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getCurrentOrientation(
        self: Display,
    ) ?Orientation {
        const ret = c.SDL_GetCurrentDisplayOrientation(
            self.value,
        );
        return Orientation.fromSdl(ret);
    }

    /// Get information about the desktop's display mode.
    ///
    /// ## Function Parameter
    /// * `self`: The display to query.
    ///
    /// ## Return Value
    /// Returns the desktop display mode.
    ///
    /// ## Remarks
    /// There's a difference between this function and `video.Display.getCurrentMode()` when SDL runs fullscreen and has changed the resolution.
    /// In that case this function will return the previous native display mode, and not the current display mode.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getDesktopMode(
        self: Display,
    ) !Mode {
        const ret = c.SDL_GetDesktopDisplayMode(self.value);
        const val = try errors.wrapCallCPtrConst(c.SDL_DisplayMode, ret);
        return Mode.fromSdl(val[0]);
    }

    /// Get the desktop area represented by a display.
    ///
    /// ## Function Parameters
    /// * `self`: The display to query.
    ///
    /// ## Return Value
    /// The rectangle filled in with the display bounds.
    ///
    /// ## Remarks
    /// The primary display is often located at `(0, 0)`, but may be placed at a different location depending on monitor layout.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getBounds(
        self: Display,
    ) !rect.IRect {
        var area: c.SDL_Rect = undefined;
        const ret = c.SDL_GetDisplayBounds(
            self.value,
            &area,
        );
        try errors.wrapCallBool(ret);
        return rect.IRect.fromSdl(area);
    }

    /// Get the content scale of a display.
    ///
    /// ## Function Parameters
    /// * `self`: The display to query.
    ///
    /// ## Return Value
    /// Returns the content scale of the display.
    ///
    /// ## Remarks
    /// The content scale is the expected scale for content based on the DPI settings of the display.
    /// For example, a 4K display might have a 2.0 (200%) display scale,
    /// which means that the user expects UI elements to be twice as big on this display, to aid in readability.
    ///
    /// After window creation, `video.Window.getDisplayScale()` should be used to query the content scale factor
    /// for individual windows instead of querying the display for a window and calling this function,
    /// as the per-window content scale factor may differ from the base value of the display it is on,
    /// particularly on high-DPI and/or multi-monitor desktop configurations.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getContentScale(
        self: Display,
    ) !f32 {
        const ret = c.SDL_GetDisplayContentScale(
            self.value,
        );
        return errors.wrapCall(f32, ret, 0.0);
    }

    /// Get the usable desktop area represented by a display, in screen coordinates.
    ///
    /// ## Function Parameters
    /// * `self`: The display to query.
    ///
    /// ## Return Value
    /// The rectangle filled in with the display bounds.
    ///
    /// ## Remarks
    /// This is the same area as `video.Display.getBounds()` reports, but with portions reserved by the system removed.
    /// For example, on Apple's macOS, this subtracts the area occupied by the menu bar and dock.
    ///
    /// Setting a window to be fullscreen generally bypasses these unusable areas, so these are good guidelines for the maximum space available to a non-fullscreen window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getUsableBounds(
        self: Display,
    ) !rect.IRect {
        var area: c.SDL_Rect = undefined;
        const ret = c.SDL_GetDisplayUsableBounds(
            self.value,
            &area,
        );
        try errors.wrapCallBool(ret);
        return rect.IRect.fromSdl(area);
    }

    /// Get the name of a display in UTF-8 encoding.
    ///
    /// ## Function Parameters
    /// * `self`: The display to query.
    ///
    /// ## Return Value
    /// Returns the name of a display.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn getName(
        self: Display,
    ) ![:0]const u8 {
        const ret = c.SDL_GetDisplayName(
            self.value,
        );
        return try errors.wrapCallCString(ret);
    }

    /// Get the properties associated with a display.
    ///
    /// ## Function Parameters
    /// * `self`: The display to query.
    ///
    /// ## Return Value
    /// Returns the display properties.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn getProperties(
        self: Display,
    ) !Properties {
        const ret = c.SDL_GetDisplayProperties(self.value);
        return Properties.fromSdl(properties.Group{ .value = try errors.wrapCall(c.SDL_PropertiesID, ret, 0) });
    }

    /// Get a list of fullscreen display modes available on a display.
    ///
    /// ## Function Parameter
    /// * `self`: The display to query.
    /// * `allocator`: Allocator used to allocator the display modes.
    ///
    /// ## Return Value
    /// Returns a slice of display modes, this needs to be freed.
    ///
    /// ## Remarks
    /// The display modes are sorted in this priority:.
    /// * Width -> Largest to smallest.
    /// * Height -> Largest to smallest.
    /// * Bits per pixel -> More colors to fewer colors.
    /// * Packed pixel layout -> Largest to smallest.
    /// * Refresh rate -> Highest to lowest.
    /// * Pixel density -> Lowest to highest.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFullscreenModes(
        self: Display,
        allocator: std.mem.Allocator,
    ) ![]Mode {
        var count: c_int = undefined;
        const val = try errors.wrapCallCPtr([*c]c.SDL_DisplayMode, c.SDL_GetFullscreenDisplayModes(self.value, &count));
        defer c.SDL_free(@ptrCast(val));
        var ret = try allocator.alloc(Mode, @intCast(count));
        for (0..@intCast(count)) |ind| {
            ret[ind] = Mode.fromSdl(val[ind].*);
        }
        return ret;
    }

    /// Get the orientation of a display when it is unrotated.
    ///
    /// ## Function Parameters
    /// * `self`; The display to query.
    ///
    /// ## Return Value
    /// Returns the display orientation value enum of the display.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNaturalOrientation(
        self: Display,
    ) ?Orientation {
        const ret = c.SDL_GetNaturalDisplayOrientation(
            self.value,
        );
        return Orientation.fromSdl(ret);
    }

    /// Return the primary display.
    ///
    /// ## Return Value
    /// Returns the primary display.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPrimaryDisplay() !Display {
        const ret = c.SDL_GetPrimaryDisplay();
        return Display{ .value = try errors.wrapCall(c.SDL_DisplayID, ret, 0) };
    }
};

/// Wrapper for EGL related functions.
///
/// ## Version
/// Provided by zig-sdl3.
pub const egl = struct {
    /// An EGL attribute, used when creating an EGL context.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const EglAttrib = c.SDL_EGLAttrib;

    /// EGL platform attribute initialization callback.
    ///
    /// ## Function Parameters
    /// * `user_data`: An app-controlled pointer that is passed to the callback.
    ///
    /// ## Return Value.
    /// Returns a newly-allocated array of attributes, terminated with `EGL_NONE`.
    ///
    /// ## Remarks
    /// This is called when SDL is attempting to create an EGL context, to let the app add extra attributes to its `eglGetPlatformDisplay()` call.
    ///
    /// The callback should return a pointer to an EGL attribute array terminated with `EGL_NONE`.
    /// If this function returns `null`, the `video.createWindow()` process will fail gracefully.
    ///
    /// The returned pointer should be allocated with `allocator` and will be passed to `free()`.
    ///
    /// The arrays returned by each callback will be appended to the existing attribute arrays defined by SDL.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub fn EglAttribArrayCallback(comptime UserData: type) type {
        return *const fn (user_data: ?*UserData) [*]EglAttrib;
    }

    /// Opaque type for an EGL config.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const EglConfig = *anyopaque;

    /// Opaque type for an EGL display.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const EglDisplay = *anyopaque;

    /// An EGL integer attribute, used when creating an EGL surface.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const EglInt = c.SDL_EGLint;

    /// EGL surface/context attribute initialization callback types.
    ///
    /// ## Function Parameters
    /// * `user_data`: An app-controlled pointer that is passed to the callback.
    /// * `display`: The EGL display to be used.
    /// * `config`: The EGL config to be used.
    ///
    /// ## Return Value
    /// Returns a newly-allocated array of attributes, terminated with `EGL_NONE`.
    ///
    /// ## Remarks
    /// This is called when SDL is attempting to create an EGL surface, to let the app add extra attributes to its `eglCreateWindowSurface()` or `eglCreateContext()` calls.
    ///
    /// For convenience, the `EGLDisplay` and `EGLConfig` to use are provided to the callback.
    ///
    /// The callback should return a pointer to an EGL attribute array terminated with `EGL_NONE`.
    /// If this function returns `null`, the `video.Window.init()` process will fail gracefully.
    ///
    /// The returned pointer should be allocated with `malloc()` and will be passed to `free()`.
    ///
    /// The arrays returned by each callback will be appended to the existing attribute arrays defined by SDL.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub fn EglIntArrayCallback(comptime UserData: type) type {
        return *const fn (user_data: ?*UserData, display: EglDisplay, config: EglConfig) [*]EglInt;
    }

    /// Opaque type for an EGL surface.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const EglSurface = *anyopaque;

    /// Get the currently active EGL config.
    ///
    /// ## Return Value
    /// Returns the currently active EGL config.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getCurrentConfig() !EglConfig {
        const ret = c.SDL_EGL_GetCurrentConfig();
        return errors.wrapCallNull(EglConfig, ret);
    }

    /// Get the currently active EGL display.
    ///
    /// ## Return Value
    /// Returns the currently active EGL display.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getCurrentDisplay() !EglDisplay {
        const ret = c.SDL_EGL_GetCurrentDisplay();
        return errors.wrapCallNull(EglDisplay, ret);
    }

    /// Get an EGL library function by name.
    ///
    /// ## Function Parameters
    /// * `proc`: The name of the EGL function.
    ///
    /// ## Return Value
    /// Returns a pointer to the named EGL function.
    /// The returned pointer should be cast to the appropriate function signature.
    ///
    /// ## Remarks
    /// If an EGL library is loaded, this function allows applications to get entry points for EGL functions.
    /// This is useful to provide to an EGL API and extension loader.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProcAddress(
        proc: [:0]const u8,
    ) !*const anyopaque {
        const ret = c.SDL_EGL_GetProcAddress(proc.ptr);
        return errors.wrapCallNull(*const anyopaque, @ptrCast(ret));
    }

    /// Get the EGL surface associated with the window.
    ///
    /// ## Function Parameters
    /// * `window`: The window to query.
    ///
    /// ## Return value.
    /// Returns the pointer to the surface.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getWindowSurface(
        window: Window,
    ) !EglSurface {
        const ret = c.SDL_EGL_GetWindowSurface(window.value);
        return errors.wrapCallNull(EglSurface, ret);
    }

    /// Sets the callbacks for defining custom `EGLAttrib` arrays for EGL initialization.
    ///
    /// ## Function Parameters
    /// * `UserData`: Type of user data.
    /// * `platform_attrib_callback`: Callback for attributes to pass to `eglGetPlatformDisplay()`. May be `null`.
    /// * `surface_attrib_callback`: Callback for attributes to pass to `eglCreateSurface()`. May be `null`.
    /// * `context_attrib_callback`: Callback for attributes to pass to `eglCreateContext()`. May be `null`.
    /// * `user_data`: A pointer that is passed to the callbacks.
    ///
    /// ## Remarks
    /// Callbacks that aren't needed can be set to `null`.
    ///
    /// NOTE: These callback pointers will be reset after `video.gl.resetAttributes()`.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setAttributeCallbacks(
        comptime UserData: type,
        comptime platform_attrib_callback: ?EglAttribArrayCallback(UserData),
        comptime surface_attrib_callback: ?EglIntArrayCallback(UserData),
        comptime context_attrib_callback: ?EglIntArrayCallback(UserData),
        user_data: ?*UserData,
    ) void {
        const Cb = struct {
            pub fn platform_attrib(user_data_c: ?*anyopaque) callconv(.c) [*c]EglAttrib {
                return platform_attrib_callback.?(@ptrCast(@alignCast(user_data_c)));
            }
            pub fn surface_attrib(user_data_c: ?*anyopaque, display_c: c.SDL_EGLDisplay, config_c: c.SDL_EGLConfig) callconv(.c) [*c]EglInt {
                return surface_attrib_callback.?(@ptrCast(@alignCast(user_data_c)), display_c, config_c);
            }
            pub fn context_attrib(user_data_c: ?*anyopaque, display_c: c.SDL_EGLDisplay, config_c: c.SDL_EGLConfig) callconv(.c) [*c]EglInt {
                return surface_attrib_callback.?(@ptrCast(@alignCast(user_data_c)), display_c, config_c);
            }
        };
        c.SDL_EGL_SetAttributeCallbacks(
            if (platform_attrib_callback != null) Cb.platform_attrib else null,
            if (surface_attrib_callback != null) Cb.surface_attrib else null,
            if (context_attrib_callback != null) Cb.context_attrib else null,
            @ptrCast(user_data),
        );
    }
};

/// Wrapper for GL related functions.
///
/// ## Version
/// Provided by zig-sdl3.
pub const gl = struct {
    /// An enumeration of OpenGL configuration attributes.
    ///
    /// ## Remarks
    /// While you can set most OpenGL attributes normally, the attributes listed above must be known before SDL creates the window that will be used with the OpenGL context.
    /// These attributes are set and read with `video.gl.setAttribute()` and `video.gl.getAttribute()`.
    ///
    /// In some cases, these attributes are minimum requests; the GL does not promise to give you exactly what you asked for.
    /// It's possible to ask for a 16-bit depth buffer and get a 24-bit one instead, for example, or to ask for no stencil buffer and still have one available.
    /// Context creation should fail if the GL can't provide your requested attributes at a minimum, but you should check to see exactly what you got.
    ///
    /// ## Version
    /// This enum is available since SDL 3.2.0.
    pub const Attribute = enum(c.SDL_GLAttr) {
        /// The minimum number of bits for the red channel of the color buffer; defaults to 8.
        red_size = c.SDL_GL_RED_SIZE,
        /// The minimum number of bits for the green channel of the color buffer; defaults to 8.
        green_size = c.SDL_GL_GREEN_SIZE,
        /// The minimum number of bits for the blue channel of the color buffer; defaults to 8.
        blue_size = c.SDL_GL_BLUE_SIZE,
        /// The minimum number of bits for the alpha channel of the color buffer; defaults to 8.
        alpha_size = c.SDL_GL_ALPHA_SIZE,
        /// The minimum number of bits for frame buffer size; defaults to 0.
        buffer_size = c.SDL_GL_BUFFER_SIZE,
        /// Whether the output is single or double buffered; defaults to double buffering on.
        double_buffer = c.SDL_GL_DOUBLEBUFFER,
        /// The minimum number of bits in the depth buffer; defaults to 16.
        depth_size = c.SDL_GL_DEPTH_SIZE,
        /// The minimum number of bits in the stencil buffer; defaults to 0.
        stencil_size = c.SDL_GL_STENCIL_SIZE,
        /// The minimum number of bits for the red channel of the accumulation buffer; defaults to 0.
        accum_red_size = c.SDL_GL_ACCUM_RED_SIZE,
        /// The minimum number of bits for the green channel of the accumulation buffer; defaults to 0.
        accum_green_size = c.SDL_GL_ACCUM_GREEN_SIZE,
        /// The minimum number of bits for the blue channel of the accumulation buffer; defaults to 0.
        accum_blue_size = c.SDL_GL_ACCUM_BLUE_SIZE,
        /// The minimum number of bits for the alpha channel of the accumulation buffer; defaults to 0.
        accum_alpha_size = c.SDL_GL_ACCUM_ALPHA_SIZE,
        /// Whether the output is stereo 3D; defaults to off.
        stereo = c.SDL_GL_STEREO,
        /// The number of buffers used for multisample anti-aliasing; defaults to 0.
        multi_sample_buffers = c.SDL_GL_MULTISAMPLEBUFFERS,
        /// The number of samples used around the current pixel used for multisample anti-aliasing.
        multi_sample_samples = c.SDL_GL_MULTISAMPLESAMPLES,
        /// Set to 1 to require hardware acceleration, set to 0 to force software rendering; defaults to allow either.
        accelerated_visual = c.SDL_GL_ACCELERATED_VISUAL,
        /// Not used (deprecated).
        retained_backing = c.SDL_GL_RETAINED_BACKING,
        /// OpenGL context major version.
        context_major_version = c.SDL_GL_CONTEXT_MAJOR_VERSION,
        /// OpenGL context minor version.
        context_minor_version = c.SDL_GL_CONTEXT_MINOR_VERSION,
        /// Some combination of 0 or more of elements of the `video.gl.ContextFlag` enumeration; defaults to 0.
        context_flags = c.SDL_GL_CONTEXT_FLAGS,
        /// Type of GL context (Core, Compatibility, ES). See `video.gl.Profile`; default value depends on platform.
        context_profile_mask = c.SDL_GL_CONTEXT_PROFILE_MASK,
        /// OpenGL context sharing; defaults to 0.
        share_with_current_context = c.SDL_GL_SHARE_WITH_CURRENT_CONTEXT,
        /// Requests sRGB capable visual; defaults to 0.
        framebuffer_srgb_capable = c.SDL_GL_FRAMEBUFFER_SRGB_CAPABLE,
        /// Sets context the release behavior. See `video.gl.ContextReleaseFlag`; defaults to flush.
        context_release_behavior = c.SDL_GL_CONTEXT_RELEASE_BEHAVIOR,
        /// Set context reset notification. See `video.gl.ContextResetNotification`; defaults to no_notification.
        context_reset_notification = c.SDL_GL_CONTEXT_RESET_NOTIFICATION,
        context_no_error = c.SDL_GL_CONTEXT_NO_ERROR,
        float_buffers = c.SDL_GL_FLOATBUFFERS,
        egl_platform = c.SDL_GL_EGL_PLATFORM,
    };

    /// Possible values to be set for the `video.gl.Attribute.context_profile_mask`.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const Profile = enum(c.SDL_GLProfile) {
        /// OpenGL core profile - deprecated functions are disabled.
        core = @intCast(c.SDL_GL_CONTEXT_PROFILE_CORE),
        /// OpenGL compatibility profile - deprecated functions are allowed.
        compatibility = @intCast(c.SDL_GL_CONTEXT_PROFILE_COMPATIBILITY),
        /// OpenGL ES profile - only a subset of the base OpenGL functionality is available.
        es = @intCast(c.SDL_GL_CONTEXT_PROFILE_ES),
    };

    /// Swap interval.
    ///
    /// ## Version
    /// Provided by zig-sdl3.
    pub const SwapInterval = enum(c_int) {
        /// Immediate updates.
        immediate = 0,
        /// Updates synchronized with the vertical retrace.
        synchronized = 1,
        /// Adaptive vsync.
        vsync = -1,
    };

    /// An opaque handle to an OpenGL context.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const Context = struct {
        value: *c.SDL_GLContextState,

        /// Create an OpenGL context for an OpenGL window, and make it current.
        ///
        /// ## Function Parameters
        /// * `window`: The window to associate with the context.
        ///
        /// ## Return Value
        /// Returns the OpenGL context associated with window.
        ///
        /// ## Remarks
        /// Windows users new to OpenGL should note that, for historical reasons, GL functions added after OpenGL version 1.1 are not available by default.
        /// Those functions must be loaded at run-time, either with an OpenGL extension-handling library or with `video.sdl.getProcAddress()` and its related functions.
        ///
        /// ## Thread Safety
        /// This function should only be called on the main thread.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        ///
        /// ## Code Examples
        /// TODO!!!
        pub fn init(
            window: Window,
        ) !gl.Context {
            const ret = c.SDL_GL_CreateContext(window.value);
            return .{ .value = try errors.wrapCallNull(*c.SDL_GLContextState, ret) };
        }

        /// Delete an OpenGL context.
        ///
        /// ## Function Parameters
        /// * `self`: The OpenGL context to be deleted.
        ///
        /// ## Thread Safety
        /// This function should only be called on the main thread.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub fn deinit(
            self: gl.Context,
        ) !void {
            return errors.wrapCallBool(c.SDL_GL_DestroyContext(self.value));
        }

        /// Set up an OpenGL context for rendering into an OpenGL window.
        ///
        /// ## Function Parameters
        /// * `self`: The OpenGL context to associate with the window.
        /// * `window`: The window to associate with the context.
        ///
        /// ## Remarks
        /// The context must have been created with a compatible window.
        ///
        /// ## Thread Safety
        /// This function should only be called on the main thread.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub fn makeCurrent(
            self: gl.Context,
            window: Window,
        ) !void {
            const ret = c.SDL_GL_MakeCurrent(window.value, self.value);
            return errors.wrapCallBool(ret);
        }
    };

    /// Possible flags to be set for the `video.gl.Attribute.context_flags` attribute.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const ContextFlag = enum(c.SDL_GLContextFlag) {
        debug = @intCast(c.SDL_GL_CONTEXT_DEBUG_FLAG),
        forward_compatible = @intCast(c.SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG),
        robust_access = @intCast(c.SDL_GL_CONTEXT_ROBUST_ACCESS_FLAG),
        reset_isolation = @intCast(c.SDL_GL_CONTEXT_RESET_ISOLATION_FLAG),
    };

    /// Possible values to be set for the `video.gl.Attribute.context_release_behavior` attribute.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const ContextReleaseFlag = enum(c.SDL_GLContextReleaseFlag) {
        none = @intCast(c.SDL_GL_CONTEXT_RELEASE_BEHAVIOR_NONE),
        flush = @intCast(c.SDL_GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH),
    };

    /// Possible values to be set `video.gl.Attribute.context_reset_notification` attribute.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const ContextResetNotification = enum(c.SDL_GLContextResetNotification) {
        no_notification = @intCast(c.SDL_GL_CONTEXT_RESET_NO_NOTIFICATION),
        lose_context = @intCast(c.SDL_GL_CONTEXT_RESET_LOSE_CONTEXT),
    };

    /// Check if an OpenGL extension is supported for the current context.
    ///
    /// ## Function Parameters
    /// * `extension`: The name of the extension to check.
    ///
    /// ## Return Value
    /// Returns true if the extension is supported, false otherwise.
    ///
    /// ## Remarks
    /// This function operates on the current GL context; you must have created a context and it must be current before calling this function.
    /// Do not assume that all contexts you create will have the same set of extensions available, or that recreating an existing context will offer the same extensions again.
    ///
    /// While it's probably not a massive overhead, this function is not an O(1) operation.
    /// Check the extensions you care about after creating the GL context and save that information somewhere instead of calling the function every time you need to know.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn extensionSupported(
        extension: [:0]const u8,
    ) bool {
        return c.SDL_GL_ExtensionSupported(extension.ptr);
    }

    /// Get the actual value for an attribute from the current context.
    ///
    /// ## Function Parameters
    /// * `attr`: An attribute enum value specifying the OpenGL attribute to get.
    ///
    /// ## Return Value
    /// Return the attribute value.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getAttribute(
        attr: gl.Attribute,
    ) !u32 {
        var value: c_int = undefined;
        const ret = c.SDL_GL_GetAttribute(@intFromEnum(attr), &value);
        try errors.wrapCallBool(ret);
        return @intCast(value);
    }

    /// Get the currently active OpenGL context.
    ///
    /// ## Return Value
    /// Returns the currently active OpenGL context.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getCurrentContext() !gl.Context {
        const ret = c.SDL_GL_GetCurrentContext();
        return .{ .value = try errors.wrapCallNull(*c.SDL_GLContextState, ret) };
    }

    /// Get the currently active OpenGL window.
    ///
    /// ## Return Value
    /// Returns the currently active OpenGL window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getCurrentWindow() !Window {
        const ret = c.SDL_GL_GetCurrentWindow();
        return Window{ .value = try errors.wrapCallNull(*c.SDL_Window, ret) };
    }

    /// Get an OpenGL function by name.
    ///
    /// ## Return Value
    /// Returns a pointer to the named OpenGL function.
    /// The returned pointer should be cast to the appropriate function signature.
    ///
    /// # Remarks
    /// If the GL library is loaded at runtime with `video.gl.loadLibrary()`, then all GL functions must be retrieved this way.
    /// Usually this is used to retrieve function pointers to OpenGL extensions.
    ///
    /// There are some quirks to looking up OpenGL functions that require some extra care from the application.
    /// If you code carefully, you can handle these quirks without any platform-specific code, though:
    /// * On Windows, function pointers are specific to the current GL context; this means you need to have created a GL context and made it current before calling `video.gl.getProcAddress()`.
    /// If you recreate your context or create a second context, you should assume that any existing function pointers aren't valid to use with it.
    /// This is (currently) a Windows-specific limitation, and in practice lots of drivers don't suffer this limitation,
    /// but it is still the way the wgl API is documented to work and you should expect crashes if you don't respect it.
    /// Store a copy of the function pointers that comes and goes with context lifespan.
    /// * On X11, function pointers returned by this function are valid for any context, and can even be looked up before a context is created at all.
    /// This means that, for at least some common OpenGL implementations, if you look up a function that doesn't exist, you'll get a non-null result that is NOT safe to call.
    /// You must always make sure the function is actually available for a given GL context before calling it,
    /// by checking for the existence of the appropriate extension with `video.gl.extensionSupported()`,
    /// or verifying that the version of OpenGL you're using offers the function as core functionality.
    /// * Some OpenGL drivers, on all platforms, will return null if a function isn't supported, but you can't count on this behavior.
    /// Check for extensions you use, and if you get a null anyway, act as if that extension wasn't available.
    /// This is probably a bug in the driver, but you can code defensively for this scenario anyhow.
    /// * Just because you're on Linux/Unix, don't assume you'll be using X11.
    /// Next-gen display servers are waiting to replace it, and may or may not make the same promises about function pointers.
    /// * OpenGL function pointers must be declared APIENTRY as in the example code.
    /// This will ensure the proper calling convention is followed on platforms where this matters (Win32) thereby avoiding stack corruption.
    pub fn getProcAddress(
        proc: [:0]const u8,
    ) *const anyopaque {
        return @ptrCast(c.SDL_GL_GetProcAddress(proc));
    }

    /// Get the swap interval for the current OpenGL context.
    ///
    /// ## Return Value
    /// Output interval value.
    ///
    /// ## Remarks
    /// If the system can't determine the swap interval, or there isn't a valid current context, this function will return `immediate`.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSwapInterval() !SwapInterval {
        var interval: c_int = undefined;
        const ret = c.SDL_GL_GetSwapInterval(&interval);
        try errors.wrapCallBool(ret);
        return @enumFromInt(interval);
    }

    /// Dynamically load an OpenGL library.
    ///
    /// ## Function Parameters
    /// * `path`: The platform dependent OpenGL library name, or `null` to open the default OpenGL library.
    ///
    /// ## Remarks
    /// This should be done after initializing the video driver, but before creating any OpenGL windows.
    /// If no OpenGL library is loaded, the default library will be loaded upon creation of the first OpenGL window.
    ///
    /// If you do this, you need to retrieve all of the GL functions used in your program from the dynamic library using `video.gl.getProcAddress()`.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn loadLibrary(
        path: ?[:0]const u8,
    ) !void {
        const ret = c.SDL_GL_LoadLibrary(if (path) |val| val.ptr else null);
        return errors.wrapCallBool(ret);
    }

    /// Reset all previously set OpenGL context attributes to their default values.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn resetAttributes() void {
        c.SDL_GL_ResetAttributes();
    }

    /// Set an OpenGL window attribute before window creation.
    ///
    /// ## Function Parameters
    /// * `attr`: The attribute to set.
    /// * `value`: The desired value for the attribute.
    ///
    /// ## Remarks
    /// This function sets the OpenGL attribute attr to value. The requested attributes should be set before creating an OpenGL window.
    /// You should use `video.gl.getAttribute()` to check the values after creating the OpenGL context, since the values obtained can differ from the requested ones.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setAttribute(
        attr: gl.Attribute,
        value: u32,
    ) !void {
        const ret = c.SDL_GL_SetAttribute(@intFromEnum(attr), @intCast(value));
        try errors.wrapCallBool(ret);
    }

    /// Set the swap interval for the current OpenGL context.
    ///
    /// ## Function Parameters
    /// * `interval`: The swap interval to set.
    ///
    /// ## Remarks
    /// If an application requests adaptive vsync and the system does not support it, this function will fail and return an error.
    /// Adaptive vsync is implemented for some glX drivers with `GLX_EXT_swap_control_tear`, and for some Windows drivers with `WGL_EXT_swap_control_tear`.
    ///
    /// Read more on the Khronos wiki: https://www.khronos.org/opengl/wiki/Swap_Interval#Adaptive_Vsync
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setSwapInterval(
        interval: SwapInterval,
    ) !void {
        const ret = c.SDL_GL_SetSwapInterval(@intFromEnum(interval));
        try errors.wrapCallBool(ret);
    }

    /// Update a window with OpenGL rendering.
    ///
    /// ## Function Parameters
    /// * `window`: The window to change.
    ///
    /// ## Remarks
    /// This is used with double-buffered OpenGL contexts, which are the default.
    ///
    /// On macOS, make sure you bind 0 to the draw framebuffer before swapping the window, otherwise nothing will happen.
    /// If you aren't using `glBindFramebuffer()`, this is the default and you won't have to do anything extra.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn swapWindow(
        window: Window,
    ) !void {
        const ret = c.SDL_GL_SwapWindow(window.value);
        try errors.wrapCallBool(ret);
    }

    /// Unload the OpenGL library previously loaded by `video.gl.loadLibrary()`.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn unloadLibrary() void {
        c.SDL_GL_UnloadLibrary();
    }
};

/// Return values from a hit test callback.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const HitTestResult = enum(c.SDL_HitTestResult) {
    /// Region is normal. No special properties.
    normal = c.SDL_HITTEST_NORMAL,
    /// Region can drag entire window.
    draggable = c.SDL_HITTEST_DRAGGABLE,
    /// Region is the resizable top-left corner border.
    resize_top_left = c.SDL_HITTEST_RESIZE_TOPLEFT,
    /// Region is the resizable top border.
    resize_top = c.SDL_HITTEST_RESIZE_TOP,
    /// Region is the resizable top-right corner border.
    resize_top_right = c.SDL_HITTEST_RESIZE_TOPRIGHT,
    /// Region is the resizable right border.
    resize_right = c.SDL_HITTEST_RESIZE_RIGHT,
    /// Region is the resizable bottom-right corner border.
    resize_bottom_right = c.SDL_HITTEST_RESIZE_BOTTOMRIGHT,
    /// Region is the resizable bottom border.
    resize_bottom = c.SDL_HITTEST_RESIZE_BOTTOM,
    /// Region is the resizable bottom-left corner border.
    resize_bottom_left = c.SDL_HITTEST_RESIZE_BOTTOMLEFT,
    /// Region is the resizable left border.
    resize_left = c.SDL_HITTEST_RESIZE_LEFT,
};

/// Window flash operation.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const FlashOperation = enum(c.SDL_FlashOperation) {
    /// Cancel any window flash state.
    cancel = c.SDL_FLASH_CANCEL,
    /// Flash the window briefly to get attention
    briefly = c.SDL_FLASH_BRIEFLY,
    /// Flash the window until it gets focus
    until_focused = c.SDL_FLASH_UNTIL_FOCUSED,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_FlashOperation) FlashOperation {
        return @enumFromInt(val);
    }

    /// Convert to SDL.
    pub fn toSdl(self: FlashOperation) c.SDL_FlashOperation {
        return @intFromEnum(self);
    }
};

/// VSync mode.
///
/// ## Version
/// This function is provided by zig-sdl3.
pub const VSync = union(enum) {
    on_each_num_refresh: usize,
    adaptive: void,

    /// Convert from an SDL value.
    pub fn fromSdl(val: c_int) ?VSync {
        return switch (val) {
            0 => null,
            -1 => VSync{ .adaptive = {} },
            else => VSync{ .on_each_num_refresh = @intCast(val) },
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?VSync) c_int {
        return if (self) |sync|
            switch (sync) {
                .on_each_num_refresh => |val| @intCast(val),
                .adaptive => -1,
            }
        else
            0;
    }
};

/// This is a unique ID for a window.
///
/// ## Remarks
/// The value `0` is an invalid ID.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const WindowId = c.SDL_WindowID;

/// The struct used as an opaque handle to a window.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Window = extern struct {
    value: *c.SDL_Window,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(Window) == @sizeOf(*c.SDL_Window));
    }

    /// Supported properties for creating a window.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const CreateProperties = struct {
        /// True if the window should always be on top.
        always_on_top: ?bool = null,
        /// True if the window has no window decoration.
        borderless: ?bool = null,
        /// True if the window will be used with an externally managed graphics context.
        external_graphics_context: ?bool = null,
        /// True if the window should accept keyboard input (defaults true).
        focusable: ?bool = null,
        /// True if the window should start in fullscreen mode in desktop resolution.
        fullscreen: ?bool = null,
        /// The height of the window.
        height: ?usize = null,
        /// True if the window should start hidden.
        hidden: ?bool = null,
        /// True if the window uses a high pixel density buffer if possible.
        high_pixel_density: ?bool = null,
        /// True if the window should start maximized.
        maximized: ?bool = null,
        /// True if the window is a popup menu.
        menu: ?bool = null,
        /// True if the window will be used with metal rendering.
        metal: ?bool = null,
        /// True if the window should start minimized.
        minimized: ?bool = null,
        /// True if the window is modal to its parent.
        modal: ?bool = null,
        /// True if the window starts with grabbed mouse focus.
        mouse_grabbed: ?bool = null,
        /// True if the window will be used with OpenGL rendering.
        open_gl: ?bool = null,
        /// Window that will be the parent of this window, required for windows with the "tooltip", "menu", and "modal" properties.
        parent: ?Window = null,
        /// True if the window should be resizable.
        resizable: ?bool = null,
        /// The title of the window, in UTF-8 encoding.
        title: ?[:0]const u8 = null,
        /// True if the window shows transparent in the areas with alpha of 0.
        transparent: ?bool = null,
        /// True if the window is a tooltip.
        tooltip: ?bool = null,
        /// True if the window is a utility window, not showing in the task bar and window list.
        utility: ?bool = null,
        /// True if the window will be used with Vulkan rendering.
        vulkan: ?bool = null,
        /// The width of the window.
        width: ?usize = null,
        /// The x position of the window.
        x: ?Position = null,
        /// The y position of the window.
        y: ?Position = null,
        /// MacOS only.
        /// The (`__unsafe_unretained) NSWindow` associated with the window, if you want to wrap an existing window.
        cocoa_window: ?struct { value: ?*anyopaque } = null,
        /// MacOS only.
        /// The (`__unsafe_unretained) NSView` associated  the window, defaults to `[window contentView]`
        cocoa_view: ?struct { value: ?*anyopaque } = null,
        /// The `(__unsafe_unretained) UIWindowScene` associated with the window, defaults to the active window scene.
        /// For iOS, tvOS, and visionOS.
        window_scene: ?struct { value: ?*anyopaque } = null,
        /// Wayland only.
        /// True if the application wants to use the Wayland surface for a custom role and does not want it attached to an XDG toplevel window.
        /// See SDL3's README/wayland for more information on using custom surfaces.
        wayland_surface_role_custom: ?bool = null,
        /// Wayland only.
        /// True if the application wants an associated `wl_egl_window object` to be created and attached to the window,
        /// even if the window does not have the OpenGL property or `video.Window.Flags.open_gl` flag set.
        wayland_create_egl_window: ?bool = null,
        /// Wayland only.
        /// The `wl_surface` associated with the window, if you want to wrap an existing window.
        /// See README/wayland for more information on SDL3's github.
        wayland_create_wl_surface: ?struct { value: ?*anyopaque } = null,
        /// Windows only.
        /// The `HWND` associated with the window, if you want to wrap an existing window.
        win32_hwnd: ?struct { value: ?*anyopaque } = null,
        /// Windows only.
        /// Optional, another window to share pixel format with, useful for OpenGL windows.
        win32_pixel_format_hwnd: ?struct { value: ?*anyopaque } = null,
        /// x11 only.
        /// The X11 Window associated with the window, if you want to wrap an existing window.
        x11_window: ?i64 = null,
        /// The id given to the canvas element.
        /// This should start with a `#` sign.
        emscripten_canvas_id: ?[:0]const u8 = null,
        /// Override the binding element for keyboard inputs for this canvas.
        /// The variable can be one of:
        /// * `#window`: The javascript window object (default).
        /// * `#document`: The javascript document object.
        /// * `#screen`: The javascript window.screen object.
        /// * `#canvas`: The WebGL canvas element.
        /// * `#none`: Don't bind anything at all.
        ///
        /// Any other string without a leading `#` sign applies to the element on the page with that ID.
        /// Windows with the "tooltip" and "menu" properties are popup windows and have the behaviors and guidelines outlined in `Window.createPopup()`.
        emscripten_keyboard_element: ?[:0]const u8 = null,

        /// Create SDL3 properties from this properties structure.
        ///
        /// Returned properties must be `deinit()` manually.
        pub fn toProperties(
            self: CreateProperties,
        ) !properties.Group {
            const ret = try properties.Group.init();
            if (self.always_on_top) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN, .{ .boolean = val });
            if (self.borderless) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, .{ .boolean = val });
            if (self.external_graphics_context) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, .{ .boolean = val });
            if (self.focusable) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_FOCUSABLE_BOOLEAN, .{ .boolean = val });
            if (self.fullscreen) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, .{ .boolean = val });
            if (self.height) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, .{ .number = @intCast(val) });
            if (self.hidden) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, .{ .boolean = val });
            if (self.high_pixel_density) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, .{ .boolean = val });
            if (self.maximized) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, .{ .boolean = val });
            if (self.menu) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_MENU_BOOLEAN, .{ .boolean = val });
            if (self.metal) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_METAL_BOOLEAN, .{ .boolean = val });
            if (self.minimized) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_MINIMIZED_BOOLEAN, .{ .boolean = val });
            if (self.modal) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_MODAL_BOOLEAN, .{ .boolean = val });
            if (self.mouse_grabbed) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_MOUSE_GRABBED_BOOLEAN, .{ .boolean = val });
            if (self.open_gl) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, .{ .boolean = val });
            if (self.parent) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_PARENT_POINTER, .{ .pointer = val.value });
            if (self.resizable) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, .{ .boolean = val });
            if (self.title) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_TITLE_STRING, .{ .string = val });
            if (self.transparent) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_TRANSPARENT_BOOLEAN, .{ .boolean = val });
            if (self.tooltip) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_TOOLTIP_BOOLEAN, .{ .boolean = val });
            if (self.utility) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_UTILITY_BOOLEAN, .{ .boolean = val });
            if (self.vulkan) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, .{ .boolean = val });
            if (self.width) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, .{ .number = @intCast(val) });
            if (self.x) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_X_NUMBER, .{ .number = @intCast(val.toSdl()) });
            if (self.y) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_Y_NUMBER, .{ .number = @intCast(val.toSdl()) });
            if (self.cocoa_window) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_COCOA_WINDOW_POINTER, .{ .pointer = val.value });
            if (self.cocoa_view) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_COCOA_VIEW_POINTER, .{ .pointer = val.value });
            if (self.window_scene) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_WINDOWSCENE_POINTER, .{ .pointer = val.value });
            if (self.wayland_surface_role_custom) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_WAYLAND_SURFACE_ROLE_CUSTOM_BOOLEAN, .{ .boolean = val });
            if (self.wayland_create_egl_window) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_WAYLAND_CREATE_EGL_WINDOW_BOOLEAN, .{ .boolean = val });
            if (self.wayland_create_wl_surface) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_WAYLAND_WL_SURFACE_POINTER, .{ .pointer = val.value });
            if (self.win32_hwnd) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_WIN32_HWND_POINTER, .{ .pointer = val.value });
            if (self.win32_pixel_format_hwnd) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_WIN32_PIXEL_FORMAT_HWND_POINTER, .{ .pointer = val.value });
            if (self.x11_window) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER, .{ .number = @intCast(val) });
            if (self.emscripten_canvas_id) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_EMSCRIPTEN_CANVAS_ID_STRING, .{ .string = val });
            if (self.emscripten_keyboard_element) |val|
                try ret.set(c.SDL_PROP_WINDOW_CREATE_EMSCRIPTEN_KEYBOARD_ELEMENT_STRING, .{ .string = val });
            return ret;
        }
    };

    /// The flags on a window.
    ///
    /// ## Remarks
    /// These cover a lot of true/false, or on/off, window state.
    /// Some of it is immutable after being set through `video.Window.init()`,
    /// some of it can be changed on existing windows by the app,
    /// and some of it might be altered by the user or system outside of the app's control.
    ///
    /// ## Version
    /// This datatype is available since SDL 3.2.0.
    pub const Flags = struct {
        /// Window is in fullscreen mode.
        fullscreen: bool = false,
        /// Window usable with OpenGL context.
        open_gl: bool = false,
        /// Window is occluded.
        occluded: bool = false,
        /// Window is neither mapped onto the desktop nor shown in the taskbar/dock/window list.
        /// The `video.Window.show()` function must be called for the window.
        hidden: bool = false,
        /// No window decoration.
        borderless: bool = false,
        /// Window can be resized.
        resizable: bool = false,
        /// Window is minimized.
        minimized: bool = false,
        /// Window is maximized.
        maximized: bool = false,
        /// Window has grabbed mouse input.
        mouse_grabbed: bool = false,
        /// Window has input focus.
        input_focus: bool = false,
        /// Window has mouse focus.
        mouse_focus: bool = false,
        /// Window not created by SDL.
        external: bool = false,
        /// Window is modal.
        modal: bool = false,
        /// Window uses high pixel density back buffer if possible.
        high_pixel_density: bool = false,
        /// Window has mouse captured (unrelated to `video.Window.Flags.mouse_grabbed`)
        mouse_capture: bool = false,
        /// Window has relative mode enabled.
        mouse_relative_mode: bool = false,
        /// Window should always be above others.
        always_on_top: bool = false,
        /// Window should be treated as a utility window, not showing in the task bar and window list.
        utility: bool = false,
        /// Window should be treated as a tooltip and does not get mouse or keyboard focus, requires a parent window.
        tooltip: bool = false,
        /// Window should be treated as a popup menu, requires a parent window.
        popup_menu: bool = false,
        /// Window has grabbed keyboard input.
        keyboard_grabbed: bool = false,
        /// Window is in fill-document mode (Emscripten only), since SDL 3.4.0.
        fill_document: bool = false,
        /// Window usable for Vulkan surface.
        vulkan: bool = false,
        /// Window usable for Metal view.
        metal: bool = false,
        /// Window with transparent buffer.
        transparent: bool = false,
        /// Window should not be focusable.
        not_focusable: bool = false,

        /// Convert from an SDL value.
        pub fn fromSdl(flags: c.SDL_WindowFlags) Flags {
            return .{
                .fullscreen = (flags & c.SDL_WINDOW_FULLSCREEN) != 0,
                .open_gl = (flags & c.SDL_WINDOW_OPENGL) != 0,
                .occluded = (flags & c.SDL_WINDOW_OCCLUDED) != 0,
                .hidden = (flags & c.SDL_WINDOW_HIDDEN) != 0,
                .borderless = (flags & c.SDL_WINDOW_BORDERLESS) != 0,
                .resizable = (flags & c.SDL_WINDOW_RESIZABLE) != 0,
                .minimized = (flags & c.SDL_WINDOW_MINIMIZED) != 0,
                .maximized = (flags & c.SDL_WINDOW_MAXIMIZED) != 0,
                .mouse_grabbed = (flags & c.SDL_WINDOW_MOUSE_GRABBED) != 0,
                .input_focus = (flags & c.SDL_WINDOW_INPUT_FOCUS) != 0,
                .mouse_focus = (flags & c.SDL_WINDOW_MOUSE_FOCUS) != 0,
                .external = (flags & c.SDL_WINDOW_EXTERNAL) != 0,
                .modal = (flags & c.SDL_WINDOW_MODAL) != 0,
                .high_pixel_density = (flags & c.SDL_WINDOW_HIGH_PIXEL_DENSITY) != 0,
                .mouse_capture = (flags & c.SDL_WINDOW_MOUSE_CAPTURE) != 0,
                .mouse_relative_mode = (flags & c.SDL_WINDOW_MOUSE_RELATIVE_MODE) != 0,
                .always_on_top = (flags & c.SDL_WINDOW_ALWAYS_ON_TOP) != 0,
                .utility = (flags & c.SDL_WINDOW_UTILITY) != 0,
                .tooltip = (flags & c.SDL_WINDOW_TOOLTIP) != 0,
                .popup_menu = (flags & c.SDL_WINDOW_POPUP_MENU) != 0,
                .keyboard_grabbed = (flags & c.SDL_WINDOW_KEYBOARD_GRABBED) != 0,
                .fill_document = (flags & c.SDL_WINDOW_FILL_DOCUMENT) != 0,
                .vulkan = (flags & c.SDL_WINDOW_VULKAN) != 0,
                .metal = (flags & c.SDL_WINDOW_METAL) != 0,
                .transparent = (flags & c.SDL_WINDOW_TRANSPARENT) != 0,
                .not_focusable = (flags & c.SDL_WINDOW_NOT_FOCUSABLE) != 0,
            };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: Flags) c.SDL_WindowFlags {
            return (if (self.fullscreen) @as(c.SDL_WindowFlags, c.SDL_WINDOW_FULLSCREEN) else 0) |
                (if (self.open_gl) @as(c.SDL_WindowFlags, c.SDL_WINDOW_OPENGL) else 0) |
                (if (self.occluded) @as(c.SDL_WindowFlags, c.SDL_WINDOW_OCCLUDED) else 0) |
                (if (self.hidden) @as(c.SDL_WindowFlags, c.SDL_WINDOW_HIDDEN) else 0) |
                (if (self.borderless) @as(c.SDL_WindowFlags, c.SDL_WINDOW_BORDERLESS) else 0) |
                (if (self.resizable) @as(c.SDL_WindowFlags, c.SDL_WINDOW_RESIZABLE) else 0) |
                (if (self.minimized) @as(c.SDL_WindowFlags, c.SDL_WINDOW_MINIMIZED) else 0) |
                (if (self.maximized) @as(c.SDL_WindowFlags, c.SDL_WINDOW_MAXIMIZED) else 0) |
                (if (self.mouse_grabbed) @as(c.SDL_WindowFlags, c.SDL_WINDOW_MOUSE_GRABBED) else 0) |
                (if (self.input_focus) @as(c.SDL_WindowFlags, c.SDL_WINDOW_INPUT_FOCUS) else 0) |
                (if (self.mouse_focus) @as(c.SDL_WindowFlags, c.SDL_WINDOW_MOUSE_FOCUS) else 0) |
                (if (self.external) @as(c.SDL_WindowFlags, c.SDL_WINDOW_EXTERNAL) else 0) |
                (if (self.modal) @as(c.SDL_WindowFlags, c.SDL_WINDOW_MODAL) else 0) |
                (if (self.high_pixel_density) @as(c.SDL_WindowFlags, c.SDL_WINDOW_HIGH_PIXEL_DENSITY) else 0) |
                (if (self.mouse_capture) @as(c.SDL_WindowFlags, c.SDL_WINDOW_MOUSE_CAPTURE) else 0) |
                (if (self.mouse_relative_mode) @as(c.SDL_WindowFlags, c.SDL_WINDOW_MOUSE_RELATIVE_MODE) else 0) |
                (if (self.always_on_top) @as(c.SDL_WindowFlags, c.SDL_WINDOW_ALWAYS_ON_TOP) else 0) |
                (if (self.utility) @as(c.SDL_WindowFlags, c.SDL_WINDOW_UTILITY) else 0) |
                (if (self.tooltip) @as(c.SDL_WindowFlags, c.SDL_WINDOW_TOOLTIP) else 0) |
                (if (self.popup_menu) @as(c.SDL_WindowFlags, c.SDL_WINDOW_POPUP_MENU) else 0) |
                (if (self.keyboard_grabbed) @as(c.SDL_WindowFlags, c.SDL_WINDOW_KEYBOARD_GRABBED) else 0) |
                (if (self.fill_document) @as(c.SDL_WindowFlags, c.SDL_WINDOW_FILL_DOCUMENT) else 0) |
                (if (self.vulkan) @as(c.SDL_WindowFlags, c.SDL_WINDOW_VULKAN) else 0) |
                (if (self.metal) @as(c.SDL_WindowFlags, c.SDL_WINDOW_METAL) else 0) |
                (if (self.transparent) @as(c.SDL_WindowFlags, c.SDL_WINDOW_TRANSPARENT) else 0) |
                (if (self.not_focusable) @as(c.SDL_WindowFlags, c.SDL_WINDOW_NOT_FOCUSABLE) else 0) |
                0;
        }
    };

    /// Position of a window.
    ///
    /// ## Version
    /// This union is provided by zig-sdl3.
    pub const Position = union(enum) {
        /// Specify the absolute position of the window.
        absolute: isize,
        /// Used to indicate that the window position should be centered on a display.
        /// You can also ensure the display, or `null` for primary.
        centered: ?Display,
        /// Used to indicate that you don't care what the window position is.
        /// You can also ensure the display, or `null` for primary.
        undefined: ?Display,

        /// Convert to the SDL representation.
        pub fn toSdl(
            self: Position,
        ) c_int {
            return switch (self) {
                .absolute => |val| @intCast(val),
                .centered => |val| if (val) |display| @bitCast(c.SDL_WINDOWPOS_CENTERED_DISPLAY(display.value)) else c.SDL_WINDOWPOS_CENTERED,
                .undefined => |val| if (val) |display| @bitCast(c.SDL_WINDOWPOS_UNDEFINED_DISPLAY(display.value)) else c.SDL_WINDOWPOS_UNDEFINED,
            };
        }
    };

    /// Window properties.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const Properties = struct {
        /// The surface associated with a shaped window.
        shape: ?surface.Surface,
        /// True if the window has HDR headroom above the SDR white point.
        /// This property can change dynamically when `events.Type.window_hdr_state_changed` is sent.
        hdr_enabled: ?bool,
        /// The value of SDR white in the `pixels.Colorspace.srgb_linear` colorspace.
        /// On Windows this corresponds to the SDR white level in scRGB colorspace, and on Apple platforms this is always `1` for EDR content.
        /// This property can change dynamically when `events.Type.window_hdr_state_changed` is sent.
        sdr_white_level: ?f32,
        /// Tthe additional high dynamic range that can be displayed, in terms of the SDR white point.
        /// When HDR is not enabled, this will be `1`.
        /// This property can change dynamically when `events.Type.window_hdr_state_changed` is sent.
        hdr_headroom: ?f32,
        /// The `ANativeWindow` associated with the window.
        android_window: ?struct { value: ?*anyopaque },
        /// The `EGLSurface` associated with the window.
        android_surface: ?struct { value: ?*anyopaque },
        /// The `(__unsafe_unretained) UIWindow` associated with the window.
        ui_kit_window: ?struct { value: ?*anyopaque },
        /// The `NSInteger` tag associated with metal views on the window.
        ui_kit_metal_view_tag: ?i64,
        /// The OpenGL view's framebuffer object. It must be bound when rendering to the screen using OpenGL.
        ui_kit_opengl_framebuffer: ?i64,
        /// The OpenGL view's renderbuffer object. It must be bound when `video.gl.swapWindow()` is called.
        ui_kit_opengl_renderbuffer: ?i64,
        /// The OpenGL view's resolve framebuffer, when MSAA is used.
        ui_kit_opengl_resolve_framebuffer: ?i64,
        /// The device index associated with the window (e.g. the `X` in `/dev/dri/cardX`).
        kmsdrm_device_index: ?i64,
        /// The DRM FD associated with the window.
        kmsdrm_drm_fd: ?i64,
        /// The GBM device associated with the window.
        kmsdrm_gbm_device: ?struct { value: ?*anyopaque },
        /// The `(__unsafe_unretained) NSWindow` associated with the window.
        cocoa_window: ?struct { value: ?*anyopaque },
        /// The `NSInteger` tag assocated with metal views on the window.
        cocoa_metal_view_tag: ?i64,
        /// The OpenVR Overlay Handle ID for the associated overlay window.
        openvr_overlay_id: ?i64,
        /// The `EGLNativeDisplayType` associated with the window.
        vivante_display: ?struct { value: ?*anyopaque },
        /// The `EGLNativeWindowType` associated with the window.
        vivante_window: ?struct { value: ?*anyopaque },
        /// The `EGLSurface` associated with the window.
        vivante_surface: ?struct { value: ?*anyopaque },
        /// The `HWND` associated with the window.
        win32_hwnd: ?struct { value: ?*anyopaque },
        /// The `HDC` associated with the window.
        win32_hdc: ?struct { value: ?*anyopaque },
        /// The `HINSTANCE` associated with the window.
        win32_instance: ?struct { value: ?*anyopaque },
        /// The `wl_display` associated with the window.
        wayland_display: ?struct { value: ?*anyopaque },
        /// The `wl_surface` associated with the window.
        wayland_surface: ?struct { value: ?*anyopaque },
        /// The `wp_viewport` associated with the window.
        wayland_viewport: ?struct { value: ?*anyopaque },
        /// The `wl_egl_window` associated with the window.
        wayland_egl_window: ?struct { value: ?*anyopaque },
        /// The `xdg_surface` associated with the window.
        wayland_xdg_surface: ?struct { value: ?*anyopaque },
        /// The `xdg_toplevel` role associated with the window.
        wayland_xdg_toplevel: ?struct { value: ?*anyopaque },
        /// The export handle associated with the window.
        wayland_xdg_toplevel_export_handle: ?[:0]const u8,
        /// The `xdg_popup` role associated with the window.
        wayland_xdg_popup: ?struct { value: ?*anyopaque },
        /// The `xdg_positioner` associated with the window, in popup mode.
        wayland_xdg_positioner: ?struct { value: ?*anyopaque },
        /// The X11 Display associated with the window.
        x11_display: ?struct { value: ?*anyopaque },
        /// The screen number associated with the window.
        x11_screen: ?i64,
        /// The X11 Window associated with the window.
        x11_window: ?i64,
        /// The id the canvas element will have.
        emscripten_canvas_id: ?[:0]const u8 = null,
        /// The keyboard element that associates keyboard events to this window.
        emscripten_keyboard_element: ?[:0]const u8 = null,

        /// Convert from an SDL value.
        pub fn fromSdl(value: properties.Group) Properties {
            return .{
                .shape = if (value.get(c.SDL_PROP_WINDOW_SHAPE_POINTER)) |val| .{ .value = @ptrCast(@alignCast(val.pointer.?)) } else null,
                .hdr_enabled = if (value.get(c.SDL_PROP_WINDOW_HDR_ENABLED_BOOLEAN)) |val| val.boolean else null,
                .sdr_white_level = if (value.get(c.SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT)) |val| val.float else null,
                .hdr_headroom = if (value.get(c.SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT)) |val| val.float else null,
                .android_window = if (value.get(c.SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER)) |val| .{ .value = val.pointer } else null,
                .android_surface = if (value.get(c.SDL_PROP_WINDOW_ANDROID_SURFACE_POINTER)) |val| .{ .value = val.pointer } else null,
                .ui_kit_window = if (value.get(c.SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER)) |val| .{ .value = val.pointer } else null,
                .ui_kit_metal_view_tag = if (value.get(c.SDL_PROP_WINDOW_UIKIT_METAL_VIEW_TAG_NUMBER)) |val| val.number else null,
                .ui_kit_opengl_framebuffer = if (value.get(c.SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER)) |val| val.number else null,
                .ui_kit_opengl_renderbuffer = if (value.get(c.SDL_PROP_WINDOW_UIKIT_OPENGL_RENDERBUFFER_NUMBER)) |val| val.number else null,
                .ui_kit_opengl_resolve_framebuffer = if (value.get(c.SDL_PROP_WINDOW_UIKIT_OPENGL_RESOLVE_FRAMEBUFFER_NUMBER)) |val| val.number else null,
                .kmsdrm_device_index = if (value.get(c.SDL_PROP_WINDOW_KMSDRM_DEVICE_INDEX_NUMBER)) |val| val.number else null,
                .kmsdrm_drm_fd = if (value.get(c.SDL_PROP_WINDOW_KMSDRM_DRM_FD_NUMBER)) |val| val.number else null,
                .kmsdrm_gbm_device = if (value.get(c.SDL_PROP_WINDOW_KMSDRM_GBM_DEVICE_POINTER)) |val| .{ .value = val.pointer } else null,
                .cocoa_window = if (value.get(c.SDL_PROP_WINDOW_COCOA_WINDOW_POINTER)) |val| .{ .value = val.pointer } else null,
                .cocoa_metal_view_tag = if (value.get(c.SDL_PROP_WINDOW_COCOA_METAL_VIEW_TAG_NUMBER)) |val| val.number else null,
                .openvr_overlay_id = if (value.get(c.SDL_PROP_WINDOW_OPENVR_OVERLAY_ID_NUMBER)) |val| val.number else null,
                .vivante_display = if (value.get(c.SDL_PROP_WINDOW_VIVANTE_DISPLAY_POINTER)) |val| .{ .value = val.pointer } else null,
                .vivante_window = if (value.get(c.SDL_PROP_WINDOW_VIVANTE_WINDOW_POINTER)) |val| .{ .value = val.pointer } else null,
                .vivante_surface = if (value.get(c.SDL_PROP_WINDOW_VIVANTE_SURFACE_POINTER)) |val| .{ .value = val.pointer } else null,
                .win32_hwnd = if (value.get(c.SDL_PROP_WINDOW_WIN32_HWND_POINTER)) |val| .{ .value = val.pointer } else null,
                .win32_hdc = if (value.get(c.SDL_PROP_WINDOW_WIN32_HDC_POINTER)) |val| .{ .value = val.pointer } else null,
                .win32_instance = if (value.get(c.SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER)) |val| .{ .value = val.pointer } else null,
                .wayland_display = if (value.get(c.SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER)) |val| .{ .value = val.pointer } else null,
                .wayland_surface = if (value.get(c.SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER)) |val| .{ .value = val.pointer } else null,
                .wayland_viewport = if (value.get(c.SDL_PROP_WINDOW_WAYLAND_VIEWPORT_POINTER)) |val| .{ .value = val.pointer } else null,
                .wayland_egl_window = if (value.get(c.SDL_PROP_WINDOW_WAYLAND_EGL_WINDOW_POINTER)) |val| .{ .value = val.pointer } else null,
                .wayland_xdg_surface = if (value.get(c.SDL_PROP_WINDOW_WAYLAND_XDG_SURFACE_POINTER)) |val| .{ .value = val.pointer } else null,
                .wayland_xdg_toplevel = if (value.get(c.SDL_PROP_WINDOW_WAYLAND_XDG_TOPLEVEL_POINTER)) |val| .{ .value = val.pointer } else null,
                .wayland_xdg_toplevel_export_handle = if (value.get(c.SDL_PROP_WINDOW_WAYLAND_XDG_TOPLEVEL_EXPORT_HANDLE_STRING)) |val| val.string else null,
                .wayland_xdg_popup = if (value.get(c.SDL_PROP_WINDOW_WAYLAND_XDG_POPUP_POINTER)) |val| .{ .value = val.pointer } else null,
                .wayland_xdg_positioner = if (value.get(c.SDL_PROP_WINDOW_WAYLAND_XDG_POSITIONER_POINTER)) |val| .{ .value = val.pointer } else null,
                .x11_display = if (value.get(c.SDL_PROP_WINDOW_X11_DISPLAY_POINTER)) |val| .{ .value = val.pointer } else null,
                .x11_screen = if (value.get(c.SDL_PROP_WINDOW_X11_SCREEN_NUMBER)) |val| val.number else null,
                .x11_window = if (value.get(c.SDL_PROP_WINDOW_X11_WINDOW_NUMBER)) |val| val.number else null,
                .emscripten_canvas_id = if (value.get(c.SDL_PROP_WINDOW_EMSCRIPTEN_CANVAS_ID_STRING)) |val| val.string else null,
                .emscripten_keyboard_element = if (value.get(c.SDL_PROP_WINDOW_EMSCRIPTEN_KEYBOARD_ELEMENT_STRING)) |val| val.string else null,
            };
        }
    };

    /// Create a child popup window of the specified parent window.
    ///
    /// ## Function Parameters
    /// * `self`: Parent window to make a popup for.
    /// * `offset_x`: The x position of the popup window relative to the origin of the parent.
    /// * `offset_y`: The y position of the popup window relative to the origin of the parent.
    /// * `width`: The width of the window.
    /// * `height`: The height of the window.
    /// * `flags`: Window flags that must contain `tooltip` or `popup_menu`.
    ///
    /// ## Return Value
    /// Returns the window created.
    ///
    /// ## Remarks
    /// The flags parameter must contain at least one of the following:
    /// * `tooltip`: The popup window is a tooltip and will not pass any input events.
    /// * `popup_menu`: The popup window is a popup menu. The topmost popup menu will implicitly gain the keyboard focus.
    ///
    /// The following flags are not relevant to popup window creation and will be ignored:
    /// * `minimized`
    /// * `maximized`
    /// * `fullscreen`
    /// * `borderless`
    ///
    /// The following flags are incompatible with popup window creation and will cause it to fail:
    /// * `utility`
    /// * `modal`
    ///
    /// The parent of a popup window can be either a regular, toplevel window, or another popup window.
    ///
    /// Popup windows cannot be minimized, maximized, made fullscreen, raised, flash, be made a modal window,
    /// be the parent of a toplevel window, or grab the mouse and/or keyboard.
    /// Attempts to do so will fail.
    ///
    /// Popup windows implicitly do not have a border/decorations and do not appear on the taskbar/dock or in lists
    /// of windows such as alt-tab menus.
    ///
    /// If a parent window is hidden or destroyed, any child popup windows will be recursively hidden or destroyed as well.
    /// Child popup windows not explicitly hidden will be restored when the parent is shown.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn createPopup(
        self: Window,
        offset_x: isize,
        offset_y: isize,
        width: usize,
        height: usize,
        flags: Flags,
    ) !Window {
        const ret = c.SDL_CreatePopupWindow(
            self.value,
            @intCast(offset_x),
            @intCast(offset_y),
            @intCast(width),
            @intCast(height),
            flags.toSdl(),
        );
        return .{ .value = try errors.wrapCallNull(*c.SDL_Window, ret) };
    }

    /// Create a window with the specified dimensions and flags.
    ///
    /// ## Function Parameters
    /// * `title`: The title of the window, in UTF-8 encoding.
    /// * `width`: The width of the window.
    /// * `height`: The height of the window.
    /// * `flags`: Window flags.
    ///
    /// ## Return Value
    /// Returns the window that was created.
    ///
    /// ## Remarks
    /// The window is implicitly shown if `video.Window.Flags.hidden` is not set.
    ///
    /// On Apple's macOS, you must set the `NSHighResolutionCapable` `Info.plist` property to `YES`,
    /// otherwise you will not receive a High-DPI OpenGL canvas.
    ///
    /// The window pixel size may differ from its window coordinate size if the window is on a high pixel density display.
    /// Use `video.Window.getSize()` to query the client area's size in window coordinates,
    /// and `video.Window.getSizeInPixels()` or `renderer.Renderer.getOutputSize()` to query the drawable size in pixels.
    /// Note that the drawable size can vary after the window is created and should be queried again
    /// if you get a `event.Window.pixel_size_changed` event.
    ///
    /// If the window is created with any of the `video.Window.Flags.open_gl` or `video.Window.Flags.vulkan` flags,
    /// then the corresponding LoadLibrary function (`video.gl_load_library()` or `video.vulkan_load_library()`) is called
    /// and the corresponding UnloadLibrary function is called by `video.Window.deinit()`.
    ///
    /// If `video.Window.Flags.vulkan` is specified and there isn't a working Vulkan driver, `video.Window.init()` will fail,
    /// because `video.vulkan_load_library()` will fail.
    ///
    /// If `video.Window.Flags.metal` is specified on an OS that does not support Metal, `video.Window.init()` will fail.
    ///
    /// If you intend to use this window with a `renderer.Renderer`,
    /// you should use `renderer.Render.initWithWindow()` instead of this function, to avoid window flicker.
    ///
    /// On non-Apple devices, SDL requires you to either not link to the Vulkan loader or link to a dynamic library version.
    /// This limitation may be removed in a future version of SDL.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// ```zig
    /// const std = @import("std");
    /// const sdl3 = @import("sdl3");
    ///
    /// const SCREEN_WIDTH = 640;
    /// const SCREEN_HEIGHT = 480;
    ///
    /// pub fn main() !void {
    ///     defer sdl3.init.shutdown();
    ///
    ///     const init_flags = sdl3.init.Flags{ .video = true };
    ///     try sdl3.init.init(init_flags);
    ///     defer sdl3.init.quit(init_flags);
    ///
    ///     const window = try sdl3.video.Window.init("Hello SDL3", SCREEN_WIDTH, SCREEN_HEIGHT, .{});
    ///     defer window.deinit();
    ///
    ///     const surface = try window.getSurface();
    ///     try surface.fillRect(null, surface.mapRgb(128, 30, 255));
    ///     try window.updateSurface();
    ///
    ///     while (true) {
    ///         switch ((try sdl3.events.wait(true)).?) {
    ///             .quit => break,
    ///             .terminating => break,
    ///             else => {}
    ///         }
    ///     }
    /// }
    /// ```
    ///
    /// TODO: Switch to example that also shows handling events!!!
    pub fn init(
        title: [:0]const u8,
        width: usize,
        height: usize,
        flags: Flags,
    ) !Window {
        const ret = c.SDL_CreateWindow(
            title,
            @intCast(width),
            @intCast(height),
            flags.toSdl(),
        );
        return .{ .value = try errors.wrapCallNull(*c.SDL_Window, ret) };
    }

    /// Create a window with the specified properties.
    ///
    /// ## Function Parameters
    /// * `props`: The properties to use.
    ///
    /// ## Return Value
    /// Returns the window that was created along with a properties group that you must free with `properties.Group.deinit()`.
    ///
    /// ## Remarks
    /// The window is implicitly shown if the "hidden" property is not set.
    ///
    /// Windows with the "tooltip" and "menu" properties are popup windows and have the behaviors and guidelines outlined in `video.Window.createPopup()`.
    ///
    /// If this window is being created to be used with a `video.Renderer`, you should not add a graphics API specific property (`video.Window.CreateProperites.open_gl`, etc),
    /// as SDL will handle that internally when it chooses a renderer.
    /// However, SDL might need to recreate your window at that point, which may cause the window to appear briefly, and then flicker as it is recreated.
    /// The correct approach to this is to create the window with the `video.Window.CreateProperites.hidden` property set to true, then create the renderer,
    /// then show the window with `video.Window.show()`.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO: ADD EXAMPLE!!!
    pub fn initWithProperties(
        props: CreateProperties,
    ) !struct { Window, properties.Group } {
        const group = try props.toProperties();
        errdefer group.deinit();

        const window = try errors.wrapCallNull(*c.SDL_Window, c.SDL_CreateWindowWithProperties(group.value));
        return .{ .{ .value = window }, group };
    }

    /// Destroy a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to destroy.
    ///
    /// ## Remarks
    /// Any child windows owned by the window will be recursively destroyed as well.
    ///
    /// Note that on some platforms, the visible window may not actually be removed from the screen until the SDL event loop is pumped again,
    /// even though the `video.Window` is no longer valid after this call.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Window,
    ) void {
        c.SDL_DestroyWindow(self.value);
    }

    /// Destroy the surface associated with the window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to update.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn destroySurface(
        self: Window,
    ) !void {
        const ret = c.SDL_DestroyWindowSurface(self.value);
        return errors.wrapCallBool(ret);
    }

    /// Request a window to demand attention from the user.
    ///
    /// ## Function parameters
    /// * `self`: The window to be flashed.
    /// * `operation`: The operation to perform.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn flash(
        self: Window,
        operation: FlashOperation,
    ) !void {
        const ret = c.SDL_FlashWindow(self.value, operation.toSdl());
        return errors.wrapCallBool(ret);
    }

    /// Get the window that currently has an input grab enabled.
    ///
    /// ## Return Value
    /// Returns the window if input is grabbed or `null` otherwise.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getGrabbed() ?Window {
        return .{ .value = c.SDL_GetGrabbedWindow() orelse return null };
    }

    /// Get the size of a window's client area.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query the width and height from.
    ///
    /// ## Return Value
    /// The minimum and maximum aspect ratio of the window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getAspectRatio(
        self: Window,
    ) !struct { f32, f32 } {
        var min_aspect: f32 = undefined;
        var max_aspect: f32 = undefined;
        const ret = c.SDL_GetWindowAspectRatio(
            self.value,
            &min_aspect,
            &max_aspect,
        );
        try errors.wrapCallBool(ret);
        return .{ min_aspect, max_aspect };
    }

    /// Get the size of a window's borders (decorations) around the client area.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query the size values of the border (decorations) from.
    ///
    /// ## Return Value
    /// Returns the border size for each side of the window.
    ///
    /// ## Remarks
    /// Note: This function may fail on systems where the window has not yet been decorated by the display server (for example, immediately after calling `video.Window.init()`).
    /// It is recommended that you wait at least until the window has been presented and composited,
    /// so that the window system has a chance to decorate the window and provide the border dimensions to SDL.
    ///
    /// This function also returns false if getting the information is not supported.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getBordersSize(
        self: Window,
    ) !struct { top: usize, left: usize, bottom: usize, right: usize } {
        var top: c_int = undefined;
        var left: c_int = undefined;
        var bottom: c_int = undefined;
        var right: c_int = undefined;
        const ret = c.SDL_GetWindowBordersSize(
            self.value,
            &top,
            &left,
            &bottom,
            &right,
        );
        try errors.wrapCallBool(ret);
        return .{ .top = @intCast(top), .left = @intCast(left), .bottom = @intCast(bottom), .right = @intCast(right) };
    }

    /// Get the display associated with a window.
    ///
    /// ## Function Parameters
    /// * `window`: The window to query.
    ///
    /// ## Return Value
    /// Returns the display containing the center of the window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn getDisplayForWindow(
        self: Window,
    ) !Display {
        const ret = c.SDL_GetDisplayForWindow(self.value);
        return .{ .value = try errors.wrapCall(c.SDL_DisplayID, ret, 0) };
    }

    /// Get the content display scale relative to a window's pixel size.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the display scale.
    ///
    /// ## Remarks
    /// This is a combination of the window pixel density and the display content scale, and is the expected scale for displaying content in this window.
    /// For example, if a 3840x2160 window had a display scale of 2.0, the user expects the content to take twice as many pixels and be the same physical size
    /// as if it were being displayed in a 1920x1080 window with a display scale of 1.0.
    ///
    /// Conceptually this value corresponds to the scale display setting, and is updated when that setting is changed,
    /// or the window moves to a display with a different scale setting.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getDisplayScale(
        self: Window,
    ) !f32 {
        const ret = c.SDL_GetWindowDisplayScale(self.value);
        return errors.wrapCall(f32, ret, 0);
    }

    /// Get the window flags.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the flags associated with the window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFlags(
        self: Window,
    ) Flags {
        return Flags.fromSdl(c.SDL_GetWindowFlags(self.value));
    }

    /// Get a window from a stored ID.
    ///
    /// ## Function Parameters
    /// * `id`: The ID of the window.
    ///
    /// ## Return Value
    /// Returns the window associated with `id`.
    ///
    /// ## Remarks
    /// The numeric ID is what `event.Window` references, and is necessary to map these events to specific `video.Window` objects.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fromId(
        id: WindowId,
    ) !Window {
        const ret = c.SDL_GetWindowFromID(id);
        return .{ .value = try errors.wrapCallNull(*c.SDL_Window, ret) };
    }

    /// Query the display mode to use when a window is visible at fullscreen.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns a the exclusive fullscreen mode or `null` for borderless fullscreen desktop mode.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFullscreenMode(
        self: Window,
    ) ?Display.Mode {
        const ret = c.SDL_GetWindowFullscreenMode(self.value);
        if (ret) |val| {
            return Display.Mode.fromSdl(val.*);
        }
        return null;
    }

    /// Get the raw ICC profile data for the screen the window is currently on.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the ICC profile data.
    /// This should be freed with `free()` when no longer needed.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getIccProfile(
        self: Window,
    ) ![]u8 {
        var size: usize = undefined;
        const ret = @as([*]u8, @ptrCast(@alignCast(try errors.wrapCallNull(*anyopaque, c.SDL_GetWindowICCProfile(self.value, &size)))));
        return ret[0..size];
    }

    /// Get the numeric ID of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the window's ID.
    ///
    /// ## Remarks
    /// The numeric ID is what `video.WindowEvent` references, and is necessary to map these events to specific `video.Window` objects.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getId(
        self: Window,
    ) !WindowId {
        const ret = c.SDL_GetWindowID(self.value);
        return errors.wrapCall(WindowId, ret, 0);
    }

    /// Get a window's keyboard grab mode.
    ///
    /// ## Return Value
    /// Returns true if keyboard is grabbed, and false otherwise.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getKeyboardGrab(
        self: Window,
    ) bool {
        return c.SDL_GetWindowKeyboardGrab(self.value);
    }

    /// Get the maximum size of a window's client area.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the window's maximum size with width and height in that order.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMaximumSize(
        self: Window,
    ) !struct { usize, usize } {
        var width: c_int = undefined;
        var height: c_int = undefined;
        const ret = c.SDL_GetWindowMaximumSize(self.value, &width, &height);
        try errors.wrapCallBool(ret);
        return .{ @intCast(width), @intCast(height) };
    }

    /// Get the minimum size of a window's client area.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the window's maximum size, the width and height in order.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMinimumSize(
        self: Window,
    ) !struct { usize, usize } {
        var width: c_int = undefined;
        var height: c_int = undefined;
        const ret = c.SDL_GetWindowMinimumSize(self.value, &width, &height);
        try errors.wrapCallBool(ret);
        return .{ @intCast(width), @intCast(height) };
    }

    /// Get a window's mouse grab mode.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns true if mouse is grabbed, and false otherwise.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMouseGrab(
        self: Window,
    ) bool {
        return c.SDL_GetWindowMouseGrab(self.value);
    }

    /// Get the mouse confinement rectangle of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns a pointer to the mouse confinement rectangle of a window, or `null` if there is not one.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMouseRect(
        self: Window,
    ) ?rect.IRect {
        const ret = c.SDL_GetWindowMouseRect(self.value);
        return if (ret != null) rect.IRect.fromSdl(ret.*) else null;
    }

    /// Get the opacity of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to get the current opacity value from.
    ///
    /// ## Return Value
    /// Returns the opacity, (`0` - transparent, `1` - opaque).
    ///
    /// ## Remarks
    /// If transparency isn't supported on this platform, opacity will be returned as `1` without error.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getOpacity(
        self: Window,
    ) !f32 {
        const ret = c.SDL_GetWindowOpacity(self.value);
        return try errors.wrapCall(f32, ret, -1);
    }

    /// Get parent of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the parent of the window on success or `null` if the window has no parent.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getParent(
        self: Window,
    ) ?Window {
        const ret = c.SDL_GetWindowParent(self.value);
        return .{ .value = ret orelse return null };
    }

    /// Get the pixel density of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the pixel density.
    ///
    /// ## Remarks
    /// This is a ratio of pixel size to window size.
    /// For example, if the window is 1920x1080 and it has a high density back buffer of 3840x2160 pixels, it would have a pixel density of 2.0.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPixelDensity(
        self: Window,
    ) !f32 {
        const ret = c.SDL_GetWindowPixelDensity(self.value);
        return try errors.wrapCall(f32, ret, 0);
    }

    /// Get the pixel format associated with the window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the pixel format of the window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPixelFormat(
        self: Window,
    ) !pixels.Format {
        const ret = c.SDL_GetWindowPixelFormat(self.value);
        return @enumFromInt(try errors.wrapCall(c_uint, ret, 0));
    }

    /// Get the position of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// The position of the window.
    ///
    /// ## Remarks
    /// This is the current position of the window as last reported by the windowing system.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPosition(
        self: Window,
    ) !struct { isize, isize } {
        var x: c_int = undefined;
        var y: c_int = undefined;
        const ret = c.SDL_GetWindowPosition(self.value, &x, &y);
        try errors.wrapCallBool(ret);
        return .{ @intCast(x), @intCast(y) };
    }

    /// Get the properties associated with a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the window properties.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProperties(
        self: Window,
    ) !Properties {
        const ret = c.SDL_GetWindowProperties(self.value);
        return Properties.fromSdl(properties.Group{ .value = try errors.wrapCall(c.SDL_PropertiesID, ret, 0) });
    }

    /// Get the safe area for this window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// The client area that is safe for interactive content.
    ///
    /// ## Remarks
    /// Some devices have portions of the screen which are partially obscured or not interactive, possibly due to on-screen controls, curved edges, camera notches, TV overscan, etc.
    /// This function provides the area of the window which is safe to have interactable content.
    /// You should continue rendering into the rest of the window, but it should not contain visually important or interactible content.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSafeArea(
        self: Window,
    ) !rect.IRect {
        var ret: c.SDL_Rect = undefined;
        try errors.wrapCallBool(c.SDL_GetWindowSafeArea(self.value, &ret));
        return rect.IRect.fromSdl(ret);
    }

    /// Get the size of the window's client area.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the size of the window's client area, width and height in order.
    ///
    /// ## Remarks
    /// The window pixel size may differ from its window coordinate size if the window is on a high pixel density display.
    /// Use `video.Window.getWindowSizeInPixels()` or `video.render.Renderer.getOutputSize()` to get the real client area size in pixels.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSize(
        self: Window,
    ) !struct { usize, usize } {
        var width: c_int = undefined;
        var height: c_int = undefined;
        const ret = c.SDL_GetWindowSize(self.value, &width, &height);
        try errors.wrapCallBool(ret);
        return .{ @intCast(width), @intCast(height) };
    }

    /// Get the size of the window's client area in pixels.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the size of the window's client area in pixels, the width and height in order.
    ///
    /// ## Remarks
    /// The window pixel size may differ from its window coordinate size if the window is on a high pixel density display.
    /// Use `video.Window.getSizeInPixels()` or `video.render.Renderer.getOutputSize()` to get the real client area size in pixels.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSizeInPixels(
        self: Window,
    ) !struct { usize, usize } {
        var width: c_int = undefined;
        var height: c_int = undefined;
        const ret = c.SDL_GetWindowSizeInPixels(self.value, &width, &height);
        try errors.wrapCallBool(ret);
        return .{ @intCast(width), @intCast(height) };
    }

    /// Get the SDL surface associated with the window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Remarks
    /// A new surface will be created with the optimal format for the window, if necessary.
    /// This surface will be freed when the window is destroyed.
    /// Do not free this surface.
    ///
    /// This surface will be invalidated if the window is resized.
    /// After resizing a window this function must be called again to return a valid surface.
    ///
    /// You may not combine this with 3D or the rendering API on this window.
    ///
    /// This function is affected by `hints.Type.framebuffer_acceleration`.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSurface(
        self: Window,
    ) !surface.Surface {
        const ret = c.SDL_GetWindowSurface(self.value);
        return surface.Surface{ .value = try errors.wrapCallNull(*c.SDL_Surface, ret) };
    }

    /// Get VSync for the window surface.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the VSync for the window surface.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSurfaceVSync(
        self: Window,
    ) !?VSync {
        var ret: c_int = undefined;
        try errors.wrapCallBool(c.SDL_GetWindowSurfaceVSync(self.value, &ret));
        return VSync.fromSdl(ret);
    }

    /// Get the title of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns the title of the window in UTF-8 format or `null` if there is no title.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getTitle(
        self: Window,
    ) ?[:0]const u8 {
        const ret = std.mem.span(c.SDL_GetWindowTitle(self.value));
        if (std.mem.eql(u8, ret, ""))
            return null;
        return ret;
    }

    /// Get the state of the progress bar for the given window’s taskbar icon.
    ///
    /// ## Function Parameters
    /// * `self`: The window to get the current progress state from.
    ///
    /// ## Return Value
    /// Returns the progress state.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn getWindowProgressState(
        self: Window,
    ) !?ProgressState {
        return ProgressState.fromSdl(c.SDL_GetWindowProgressState(self.value));
    }

    /// Get the value of the progress bar for the given window’s taskbar icon.
    ///
    /// ## Function Parameters
    /// * `self`: The window to get the current progress value from.
    ///
    /// ## Return Value
    /// Returns the progress value in the range of `[0, 1]`.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn getWindowProgressValue(
        self: Window,
    ) !f32 {
        return errors.wrapCall(f32, c.SDL_GetWindowProgressValue(self.value), -1);
    }

    /// Hide a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to hide.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn hide(
        self: Window,
    ) !void {
        const ret = c.SDL_HideWindow(self.value);
        try errors.wrapCallBool(ret);
    }

    /// Request that the window be made as large as possible.
    ///
    /// ## Function Parameters
    /// * `self`: The window to maximize.
    ///
    /// ## Remarks
    /// Non-resizable windows can't be maximized. The window must have the `video.Window.Flags.resizable` flag set, or this will have no effect.
    ///
    /// On some windowing systems this request is asynchronous and the new window state may not have have been applied immediately upon the return of this function.
    /// If an immediate change is required, call `video.Window.sync()` to block until the changes have taken effect.
    ///
    /// When the window state changes, an `events.Type.window_maximized` event will be emitted.
    /// Note that, as this is just a request, the windowing system can deny the state change.
    ///
    /// When maximizing a window, whether the constraints set via `video.Window.setMaximumSize()` are honored depends on the policy of the window manager.
    /// Win32 and macOS enforce the constraints when maximizing, while X11 and Wayland window managers may vary.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn maximize(
        self: Window,
    ) !void {
        const ret = c.SDL_MaximizeWindow(self.value);
        return errors.wrapCallBool(ret);
    }

    /// Request that the window be minimized to an iconic representation.
    ///
    /// ## Function Parameters
    /// * `self`: The window to minimize.
    ///
    /// ## Remarks
    /// If the window is in a fullscreen state, this request has no direct effect.
    /// It may alter the state the window is returned to when leaving fullscreen.
    ///
    /// On some windowing systems this request is asynchronous and the new window state may not have been applied immediately upon the return of this function.
    /// If an immediate change is required, call `video.Window.sync()` to block until the changes have taken effect.
    ///
    /// When the window state changes, an `events.Type.window_minimized` event will be emitted.
    /// Note that, as this is just a request, the windowing system can deny the state change.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn minimize(
        self: Window,
    ) !void {
        const ret = c.SDL_MinimizeWindow(self.value);
        return errors.wrapCallBool(ret);
    }

    /// Request that a window be raised above other windows and gain the input focus.
    ///
    /// ## Function Parameters
    /// * `self`: The window to raise.
    ///
    /// ## Remarks
    /// The result of this request is subject to desktop window manager policy,
    /// particularly if raising the requested window would result in stealing focus from another application.
    /// If the window is successfully raised and gains input focus, an `events.Type.window_focus_gained` event will be emitted,
    /// and the window will have the `video.Window.Flags.input_focus` flag set.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn raise(
        self: Window,
    ) !void {
        const ret = c.SDL_RaiseWindow(self.value);
        return errors.wrapCallBool(ret);
    }

    /// Request that the size and position of a minimized or maximized window be restored.
    ///
    /// ## Function Parameters
    /// * `self`: The window to restore.
    ///
    /// ## Remarks
    /// If the window is in a fullscreen state, this request has no direct effect.
    /// It may alter the state the window is returned to when leaving fullscreen.
    ///
    /// On some windowing systems this request is asynchronous and the new window state may not have have been applied immediately upon the return of this function.
    /// If an immediate change is required, call `video.Window.sync()` to block until the changes have taken effect.
    ///
    /// When the window state changes, an `events.Type.window_restored` event will be emitted.
    /// Note that, as this is just a request, the windowing system can deny the state change.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn restore(
        self: Window,
    ) !void {
        const ret = c.SDL_RestoreWindow(self.value);
        return errors.wrapCallBool(ret);
    }

    /// Set the window to always be above the others.
    ///
    /// ## Function Parameters
    /// * `self`: The window of which to change the always on top state.
    /// * `on_top`: True to set the window always on top, false to disable.
    ///
    /// ## Remarks
    /// This will add or remove the window's `video.Window.Flags.always_on_top` flag.
    /// This will bring the window to the front and keep the window above the rest.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setAlwaysOnTop(
        self: Window,
        on_top: bool,
    ) !void {
        const ret = c.SDL_SetWindowAlwaysOnTop(self.value, on_top);
        return errors.wrapCallBool(ret);
    }

    /// Request that the aspect ratio of a window's client area be set.
    ///
    /// ## Function Parameters
    /// * `self`: The window to change.
    /// * `min_aspect`: The minimum aspect ratio of the window, or `0` for no limit.
    /// * `max_aspect`: The maximum aspect ratio of the window, or `0` for no limit.
    ///
    /// ## Remarks
    /// The aspect ratio is the ratio of width divided by height, e.g. 2560x1600 would be 1.6.
    /// Larger aspect ratios are wider and smaller aspect ratios are narrower.
    ///
    /// If, at the time of this request, the window in a fixed-size state, such as maximized or fullscreen,
    /// the request will be deferred until the window exits this state and becomes resizable again.
    ///
    /// On some windowing systems, this request is asynchronous and the new window aspect ratio may not have have been applied immediately upon the return of this function.
    /// If an immediate change is required, call `video.Window.sync()` to block until the changes have taken effect.
    ///
    /// When the window size changes, an `events.Type.window_resized` event will be emitted with the new window dimensions.
    /// Note that the new dimensions may not match the exact aspect ratio requested,
    /// as some windowing systems can restrict the window size in certain scenarios (e.g. constraining the size of the content area to remain within the usable desktop bounds).
    /// Additionally, as this is just a request, it can be denied by the windowing system.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setAspectRatio(
        self: Window,
        min_aspect: f32,
        max_aspect: f32,
    ) !void {
        const ret = c.SDL_SetWindowAspectRatio(self.value, min_aspect, max_aspect);
        return errors.wrapCallBool(ret);
    }

    /// Set the border state of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window of which to change the border state.
    /// * `bordered`: False to remove border, true to add border.
    ///
    /// ## Remarks
    /// This will add or remove the window's `video.Window.Flags.borderless` flag and add or remove the border from the actual window.
    /// This is a no-op if the window's border already matches the requested state.
    ///
    /// You can't change the border state of a fullscreen window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setBordered(
        self: Window,
        bordered: bool,
    ) !void {
        const ret = c.SDL_SetWindowBordered(self.value, bordered);
        return errors.wrapCallBool(ret);
    }

    /// Set the window to fill the current document space (Emscripten only).
    ///
    /// ## Function Parameters
    /// * `self`: The window of which to change the fill-document state.
    /// * `fill`: True to set the window to fill the document, false to disable.
    ///
    /// ## Remarks
    /// This will add or remove the window's `fill_document` flag.
    ///
    /// Currently this flag only applies to the Emscripten target.
    ///
    /// When enabled, the canvas element fills the entire document.
    /// Resize events will be generated as the browser window is resized, as that will adjust the canvas size as well.
    /// The canvas will cover anything else on the page, including any controls provided by Emscripten in its generated HTML file
    /// (in fact, any elements on the page that aren't the canvas will be moved into a hidden div element).
    ///
    /// Often times this is desirable for a browser-based game, but it means several things that we expect of an SDL window on other platforms might not work as expected,
    /// such as minimum window sizes and aspect ratios.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn setFillDocument(
        self: Window,
        fill: bool,
    ) !void {
        return errors.wrapCallBool(c.SDL_SetWindowFillDocument(self.value, fill));
    }

    /// Set whether the window may have input focus.
    ///
    /// ## Function Parameters
    /// * `self`: The window to set focusable state.
    /// * `focusable`: True to allow input focus, false to not allow input focus.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setFocusable(
        self: Window,
        focusable: bool,
    ) !void {
        const ret = c.SDL_SetWindowFocusable(self.value, focusable);
        return errors.wrapCallBool(ret);
    }

    /// Request that the window's fullscreen state be changed.
    ///
    /// ## Function Parameters
    /// * `self`: The window to change.
    /// * `fullscreen`: True for fullscreen mode, false for windowed mode.
    ///
    /// ## Remarks
    /// By default a window in fullscreen state uses borderless fullscreen desktop mode, but a specific exclusive display mode can be set using `video.Window.setFullscreenMode()`.
    ///
    /// On some windowing systems this request is asynchronous and the new fullscreen state may not have have been applied immediately upon the return of this function.
    /// If an immediate change is required, call `video.Window.sync()` to block until the changes have taken effect.
    ///
    /// When the window state changes, an `events.Type.window_enter_fullscreen` or `events.Type.window_leave_fullscreen` event will be emitted.
    /// Note that, as this is just a request, it can be denied by the windowing system.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setFullscreen(
        self: Window,
        fullscreen: bool,
    ) !void {
        const ret = c.SDL_SetWindowFullscreen(self.value, fullscreen);
        return errors.wrapCallBool(ret);
    }

    /// Set the display mode to use when a window is visible and fullscreen.
    ///
    /// ## Function Parameters
    /// * `self`: The window to affect.
    /// * `mode`: The display mode to use. Can be `null` for borderless foullscreen desktop mode.
    ///
    /// ## Remarks
    /// This only affects the display mode used when the window is fullscreen.
    /// To change the window size when the window is not fullscreen, use `video.Window.setSize()`.
    ///
    /// If the window is currently in the fullscreen state,
    /// this request is asynchronous on some windowing systems and the new mode dimensions may not be applied immediately upon the return of this function.
    /// If an immediate change is required, call `video.Window.sync()` to block until the changes have taken effect.
    ///
    /// When the new mode takes effect, an `events.Type.window_resized` and/or an `events.Type.window_pixel_size_changed`event will be emitted with the new mode dimensions.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setFullscreenMode(
        self: Window,
        mode: ?Display.Mode,
    ) !void {
        const mode_sdl = if (mode) |val| val.toSdl() else undefined;
        const ret = c.SDL_SetWindowFullscreenMode(self.value, if (mode != null) &mode_sdl else null);
        try errors.wrapCallBool(ret);
    }

    /// Provide a callback that decides if a window region has special properties.
    ///
    /// ## Function Parameters
    /// * `self`: The window to set hit-testing on.
    /// * `callback`: The function to call when doing a hit-test.
    /// * `user_data`: An app-defined void pointer passed to callback.
    ///
    /// ## Remarks
    /// Normally windows are dragged and resized by decorations provided by the system window manager (a title bar, borders, etc), but for some apps,
    /// it makes sense to drag them from somewhere else inside the window itself; for example, one might have a borderless window that wants to be draggable from any part,
    /// or simulate its own title bar, etc.
    ///
    /// This function lets the app provide a callback that designates pieces of a given window as special.
    /// This callback is run during event processing if we need to tell the OS to treat a region of the window specially; the use of this callback is known as "hit testing."
    ///
    /// Mouse input may not be delivered to your application if it is within a special area;
    /// the OS will often apply that input to moving the window or resizing the window and not deliver it to the application.
    ///
    /// Specifying `null` for a callback disables hit-testing.
    /// Hit-testing is disabled by default.
    ///
    /// Platforms that don't support this functionality will error unconditionally, even if you're attempting to disable hit-testing.
    ///
    /// Your callback may fire at any time, and its firing does not indicate any specific behavior
    /// (for example, on Windows, this certainly might fire when the OS is deciding whether to drag your window, but it fires for lots of other reasons, too,
    /// some unrelated to anything you probably care about and when the mouse isn't actually at the location it is testing).
    /// Since this can fire at any time, you should try to keep your callback efficient, devoid of allocations, etc.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setHitTest(
        self: Window,
        comptime UserData: type,
        comptime callback: ?HitTest(UserData),
        user_data: ?*anyopaque,
    ) !void {
        const Cb = struct {
            pub fn run(win_c: ?*c.SDL_Window, area_c: [*c]const c.SDL_Point, user_data_c: ?*anyopaque) callconv(.c) c.SDL_HitTestResult {
                return @intFromEnum(callback.?(.{ .value = win_c.? }, rect.IPoint.fromSdl(area_c.*), @ptrCast(@alignCast(user_data_c))));
            }
        };
        return errors.wrapCallBool(c.SDL_SetWindowHitTest(self.value, if (callback != null) Cb.run else null, @ptrCast(user_data)));
    }

    /// Set the icon for a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to change.
    /// * `icon`: The surface structure containing the icon for the window.
    ///
    /// ## Remarks
    /// If this function is passed a surface with alternate representations, the surface will be interpreted as the content to be used for 100% display scale,
    /// and the alternate representations will be used for high DPI situations.
    /// For example, if the original surface is 32x32, then on a 2x macOS display or 200% display scale on Windows, a 64x64 version of the image will be used, if available.
    /// If a matching version of the image isn't available, the closest larger size image will be downscaled to the appropriate size and be used instead, if available.
    /// Otherwise, the closest smaller image will be upscaled and be used instead.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setIcon(
        self: Window,
        icon: surface.Surface,
    ) !void {
        const ret = c.SDL_SetWindowIcon(self.value, icon.value);
        return errors.wrapCallBool(ret);
    }

    /// Set a window's keyboard grab mode.
    ///
    /// ## Function Parameters
    /// * `self`: The window for which the keyboard grab mode should be set.
    /// * `grabbed`: This is true to grab keyboard, and false to release.
    ///
    /// ## Remarks
    /// Keyboard grab enables capture of system keyboard shortcuts like Alt+Tab or the Meta/Super key.
    /// Note that not all system keyboard shortcuts can be captured by applications (one example is Ctrl+Alt+Del on Windows).
    ///
    /// This is primarily intended for specialized applications such as VNC clients or VM frontends. Normal games should not use keyboard grab.
    ///
    /// When keyboard grab is enabled, SDL will continue to handle Alt+Tab when the window is full-screen to ensure the user is not trapped in your application.
    /// If you have a custom keyboard shortcut to exit fullscreen mode, you may suppress this behavior with `hints.Type.allow_alt_tab_while_grabbed`.
    ///
    /// If the caller enables a grab while another window is currently grabbed, the other window loses its grab in favor of the caller's window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setKeyboardGrab(
        self: Window,
        grabbed: bool,
    ) !void {
        const ret = c.SDL_SetWindowKeyboardGrab(self.value, grabbed);
        try errors.wrapCallBool(ret);
    }

    /// Set the maximum size of a window's client area.
    ///
    /// ## Function Parameters
    /// * `self`: The window to change.
    /// * `max_width`: The maximum width of the window, or `null` for no limit.
    /// * `max_height`: The maximum height of the window, or `null` for no limit.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setMaximumSize(
        self: Window,
        max_width: ?usize,
        max_height: ?usize,
    ) !void {
        const ret = c.SDL_SetWindowMaximumSize(self.value, if (max_width) |val| @intCast(val) else 0, if (max_height) |val| @intCast(val) else 0);
        return errors.wrapCallBool(ret);
    }

    /// Set the minimum size of a window's client area.
    ///
    /// ## Function Parameters
    /// * `self`: The window to change.
    /// * `min_width`: The minimum width of the window, or `null` for no limit.
    /// * `min_height`: The minimum height of the window, or `null` for no limit.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setMinimumSize(
        self: Window,
        min_width: ?usize,
        min_height: ?usize,
    ) !void {
        const ret = c.SDL_SetWindowMinimumSize(self.value, if (min_width) |val| @intCast(val) else 0, if (min_height) |val| @intCast(val) else 0);
        return errors.wrapCallBool(ret);
    }

    /// Toggle the state of the window as modal.
    ///
    /// ## Function Parameters
    /// * `self`: The window on which to set the modal state.
    /// * `modal`: True to toggle modal status on, false to toggle it off.
    ///
    /// ## Remarks
    /// To enable modal status on a window, the window must currently be the child window of a parent, or toggling modal status on will fail.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setModal(
        self: Window,
        modal: bool,
    ) !void {
        const ret = c.SDL_SetWindowModal(self.value, modal);
        return errors.wrapCallBool(ret);
    }

    /// Set a window's mouse grab mode.
    ///
    /// ## Function Parameters
    /// * `self`: The window for which the mouse grab mode should be set.
    /// * `grabbed`: This is true to grab mouse, and false to release.
    ///
    /// ## Remarks
    /// Mouse grab confines the mouse cursor to the window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setMouseGrab(
        self: Window,
        grabbed: bool,
    ) !void {
        const ret = c.SDL_SetWindowMouseGrab(self.value, grabbed);
        return errors.wrapCallBool(ret);
    }

    /// Confines the cursor to the specified area of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window that will be associated with the barrier.
    /// * `area`: A rectangle area in window-relative coordinates. If `null`, the barrier for the specified window will be destroyed.
    ///
    /// ## Remarks
    /// Note that this does NOT grab the cursor, it only defines the area a cursor is restricted to when the window has mouse focus.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setMouseRect(
        self: Window,
        area: ?rect.IRect,
    ) !void {
        const area_sdl: c.SDL_Rect = if (area) |val| val.toSdl() else undefined;
        const ret = c.SDL_SetWindowMouseRect(
            self.value,
            if (area != null) &area_sdl else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the opacity for a window.
    ///
    /// ## Function Parameters
    /// * `self`: Set the opacity for a window.
    /// * `opacity`: The opacity value (0 - transparent, 1 - opaque).
    ///
    /// ## Remarks
    /// The parameter `opacity` will be clamped internally between `0` (transparent) and `1` (opaque).
    ///
    /// This function also returns an error if setting the opacity isn't supported.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setOpacity(
        self: Window,
        opacity: f32,
    ) !void {
        const ret = c.SDL_SetWindowOpacity(self.value, opacity);
        return errors.wrapCallBool(ret);
    }

    /// Set the window as a child of a parent window.
    ///
    /// ## Function Parameters
    /// * `self`: The window that should become the child of a parent.
    /// * `parent`: The new parent window for the child window.
    ///
    /// ## Remarks
    /// If the window is already the child of an existing window, it will be reparented to the new owner.
    /// Setting the parent window to `null` unparents the window and removes child window status.
    ///
    /// If a parent window is hidden or destroyed, the operation will be recursively applied to child windows.
    /// Child windows hidden with the parent that did not have their hidden status explicitly set will be restored when the parent is shown.
    ///
    /// Attempting to set the parent of a window that is currently in the modal state will fail.
    /// Use `video.Window.setModal()` to cancel the modal status before attempting to change the parent.
    ///
    /// Popup windows cannot change parents and attempts to do so will fail.
    ///
    /// Setting a parent window that is currently the sibling or descendent of the child window results in undefined behavior.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setParent(
        self: Window,
        parent: ?Window,
    ) !void {
        const ret = c.SDL_SetWindowParent(
            self.value,
            if (parent) |val| val.value else null,
        );
        return errors.wrapCallBool(ret);
    }

    /// Request that the window's position be set.
    ///
    /// ## Function Parameters
    /// * `self`: The window to reposition.
    /// * `x`: The x coordinate of the window.
    /// * `y`: The y coordinate of the window.
    ///
    /// ## Remarks
    /// If the window is in an exclusive fullscreen or maximized state, this request has no effect.
    ///
    /// This can be used to reposition fullscreen-desktop windows onto a different display, however,
    /// as exclusive fullscreen windows are locked to a specific display, they can only be repositioned programmatically via `video.Window.setFullScreenMode()`.
    ///
    /// On some windowing systems this request is asynchronous and the new coordinates may not have have been applied immediately upon the return of this function.
    /// If an immediate change is required, call `video.Window.sync()` to block until the changes have taken effect.
    ///
    /// When the window position changes, an `events.Type.window_moved` event will be emitted with the window's new coordinates.
    /// Note that the new coordinates may not match the exact coordinates requested,
    /// as some windowing systems can restrict the position of the window in certain scenarios (e.g. constraining the position so the window is always within desktop bounds).
    /// Additionally, as this is just a request, it can be denied by the windowing system.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setPosition(
        self: Window,
        x: Position,
        y: Position,
    ) !void {
        const ret = c.SDL_SetWindowPosition(
            self.value,
            x.toSdl(),
            y.toSdl(),
        );
        return errors.wrapCallBool(ret);
    }

    /// Sets the state of the progress bar for the given window’s taskbar icon.
    ///
    /// ## Function Parameters
    /// * `self`: The window whose progress state is to be modified.
    /// * `state`: The progress state, `null` stops displaying the progress bar.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn setProgressState(
        self: Window,
        state: ?ProgressState,
    ) !void {
        return errors.wrapCallBool(c.SDL_SetWindowProgressState(self.value, ProgressState.toSdl(state)));
    }

    /// Sets the value of the progress bar for the given window’s taskbar icon.
    ///
    /// ## Function Parameters
    /// * `self`: The window whose progress value is to be modified.
    /// * `value`: The progress value in the range of `[0, 1]`. If the value is outside the valid range, it gets clamped
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn setProgressValue(
        self: Window,
        value: f32,
    ) !void {
        return errors.wrapCallBool(c.SDL_SetWindowProgressValue(self.value, value));
    }

    /// Set the user-resizable state of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window of which to change the resizable state.
    /// * `resizable`: True to allow resizing, false to disallow.
    ///
    /// ## Remarks
    /// This will add or remove the window's `video.Window.Flags.resizable` flag and allow/disallow user resizing of the window.
    /// This is a no-op if the window's resizable state already matches the requested state.
    ///
    /// You can't change the resizable state of a fullscreen window.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setResizable(
        self: Window,
        resizable: bool,
    ) !void {
        const ret = c.SDL_SetWindowResizable(self.value, resizable);
        return errors.wrapCallBool(ret);
    }

    /// Set the shape of a transparent window.
    ///
    /// ## Function Parameters
    /// * `self`: The window.
    /// * `shape`: The surface representing the shape of the window, or `null` to remove any current shape.
    ///
    /// ## Remarks
    /// This sets the alpha channel of a transparent window and any fully transparent areas are also transparent to mouse clicks.
    /// If you are using something besides the SDL render API,
    /// then you are responsible for drawing the alpha channel of the window to match the shape alpha channel to get consistent cross-platform results.
    ///
    /// The shape is copied inside this function, so you can free it afterwards.
    /// If your shape surface changes, you should call `video.Window.setShape()` again to update the window.
    /// This is an expensive operation, so should be done sparingly.
    ///
    /// The window must have been created with the `video.Window.Flags.transparent` flag.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setShape(
        self: Window,
        shape: ?surface.Surface,
    ) !void {
        const ret = c.SDL_SetWindowShape(self.value, if (shape) |val| val.value else null);
        return errors.wrapCallBool(ret);
    }

    /// Request that the size of a window's client area be set.
    ///
    /// ## Function Parameters
    /// * `self`: The window to change.
    /// * `width`: The width of the window, must be > 0.
    /// * `height`: The height of the window, must be > 0.
    ///
    /// ## Remarks
    /// If the window is in a fullscreen or maximized state, this request has no effect.
    ///
    /// To change the exclusive fullscreen mode of a window, use `video.Window.setFullScreenMode()`.
    ///
    /// On some windowing systems, this request is asynchronous and the new window size may not have have been applied immediately upon the return of this function.
    /// If an immediate change is required, call `video.Window.sync()` to block until the changes have taken effect.
    ///
    /// When the window size changes, an `events.Type.window_resized` event will be emitted with the new window dimensions.
    /// Note that the new dimensions may not match the exact size requested,
    /// as some windowing systems can restrict the window size in certain scenarios (e.g. constraining the size of the content area to remain within the usable desktop bounds).
    /// Additionally, as this is just a request, it can be denied by the windowing system.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setSize(
        self: Window,
        width: usize,
        height: usize,
    ) !void {
        const ret = c.SDL_SetWindowSize(
            self.value,
            @intCast(width),
            @intCast(height),
        );
        return errors.wrapCallBool(ret);
    }

    /// Toggle VSync for the window surface.
    ///
    /// ## Function Parameters
    /// * `self`: The window.
    /// * `vsync`: The vertical refresh sync interval.
    ///
    /// ## Remarks
    /// When a window surface is created, vsync defaults to `null`.
    ///
    /// The vsync parameter can be 1 to synchronize present with every vertical refresh, 2 to synchronize present with every second vertical refresh,
    /// etc., `adaptive` for late swap tearing (adaptive vsync), or `null` to disable.
    /// Not every value is supported by every driver, so you should check the return value to see whether the requested setting is supported.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setSurfaceVSync(
        self: Window,
        vsync: ?VSync,
    ) !void {
        const ret = c.SDL_SetWindowSurfaceVSync(
            self.value,
            VSync.toSdl(vsync),
        );
        try errors.wrapCallBool(ret);
    }

    /// Set the title of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to change.
    /// * `title`: The desired window title in UTF-8 format.
    ///
    /// ## Remarks
    /// This string is expected to be in UTF-8 encoding.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setTitle(
        self: Window,
        title: [:0]const u8,
    ) !void {
        const ret = c.SDL_SetWindowTitle(self.value, title);
        return errors.wrapCallBool(ret);
    }

    /// Show a window.
    ///
    /// ## Function Parameters
    /// * `self`: The window to show.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn show(
        self: Window,
    ) !void {
        const ret = c.SDL_ShowWindow(self.value);
        try errors.wrapCallBool(ret);
    }

    /// Display the system-level window menu.
    ///
    /// ## Function Parameters
    /// * `self`: The window for which the menu will be displayed.
    /// * `x`: The x coordinate of the menu, relative to the origin (top-left) of the client area.
    /// * `y`: The y coordinate of the menu, relative to the origin (top-left) of the client area.
    ///
    /// ## Remarks
    /// This default window menu is provided by the system and on some platforms provides functionality for setting or changing privileged state on the window,
    /// such as moving it between workspaces or displays, or toggling the always-on-top property.
    ///
    /// On platforms or desktops where this is unsupported, this function does nothing.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn showSystemMenu(
        self: Window,
        x: isize,
        y: isize,
    ) !void {
        return errors.wrapCallBool(c.SDL_ShowWindowSystemMenu(self.value, @intCast(x), @intCast(y)));
    }

    /// Block until any pending window state is finalized.
    ///
    /// ## Function Parameters
    /// * `self`: The window for which to wait for the pending state to be applied.
    ///
    /// ## Remarks
    /// On asynchronous windowing systems, this acts as a synchronization barrier for pending window state.
    /// It will attempt to wait until any pending window state has been applied and is guaranteed to return within finite time.
    /// Note that for how long it can potentially block depends on the underlying window system,
    /// as window state changes may involve somewhat lengthy animations that must complete before the window is in its final requested state.
    ///
    /// On windowing systems where changes are immediate, this does nothing.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn sync(
        self: Window,
    ) !void {
        const ret = c.SDL_SyncWindow(self.value);
        return errors.wrapCallBool(ret);
    }

    /// Copy the window surface to the screen.
    ///
    /// ## Function Parameters
    /// * `self`: The window to update.
    ///
    /// ## Remarks
    /// This is the function you use to reflect any changes to the surface on the screen.
    ///
    /// This function is equivalent to the SDL 1.2 API `SDL_Flip()`.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn updateSurface(
        self: Window,
    ) !void {
        const ret = c.SDL_UpdateWindowSurface(self.value);
        return errors.wrapCallBool(ret);
    }

    /// Copy the window surface to the screen.
    ///
    /// ## Function Parameters
    /// * `self`: The window to update.
    /// * `rects`: Rectangles representing areas of the surface to copy, in pixels.
    ///
    /// ## Remarks
    /// This is the function you use to reflect changes to portions of the surface on the screen.
    ///
    /// This function is equivalent to the SDL 1.2 API `SDL_UpdateRects()`.
    ///
    /// Note that this function will update at least the rectangles specified, but this is only intended as an optimization; in practice,
    /// this might update more of the screen (or all of the screen!), depending on what method SDL uses to send pixels to the system.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn updateSurfaceRects(
        self: Window,
        rects: []rect.IRect,
    ) !void {
        const ret = c.SDL_UpdateWindowSurfaceRects(self.value, @ptrCast(rects.ptr), @intCast(rects.len));
        return errors.wrapCallBool(ret);
    }

    /// Return whether the window has a surface associated with it.
    ///
    /// ## Function Parameters
    /// * `self`: The window to query.
    ///
    /// ## Return Value
    /// Returns true if there is a surface associated with the window, or false otherwise.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn hasSurface(
        self: Window,
    ) bool {
        return c.SDL_WindowHasSurface(self.value);
    }
};

/// Prevent the screen from being blanked by a screen saver.
///
/// ## Remarks
/// If you disable the screensaver, it is automatically re-enabled when SDL quits.
///
/// The screensaver is disabled by default, but this may by changed by `hints.Type.allow_screensaver`.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn disableScreenSaver() !void {
    const ret = c.SDL_DisableScreenSaver();
    return errors.wrapCallBool(ret);
}

/// Allow the screen to be blanked by a screen saver.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn enableScreenSaver() !void {
    const ret = c.SDL_EnableScreenSaver();
    return errors.wrapCallBool(ret);
}

/// Get the name of the currently initialized video driver.
///
/// ## Return Value
/// Returns the name of the current video driver or `null` if no driver has been initialized.
///
/// ## Remarks
/// The names of drivers are all simple, low-ASCII identifiers, like "cocoa", "x11" or "windows".
/// These never have Unicode characters, and are not meant to be proper names.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getCurrentDriverName() ?[:0]const u8 {
    const ret = c.SDL_GetCurrentVideoDriver();
    if (ret) |val|
        return std.mem.span(val);
    return null;
}

/// Get a list of currently connected displays.
///
/// ## Return Value
/// Returns a slice of display items.
/// Return value must be freed with `free()`.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
///
/// ## Code Examples
/// TODO!!!
pub fn getDisplays() ![]Display {
    var count: c_int = undefined;
    const ret = try errors.wrapCallCPtr(c.SDL_DisplayID, c.SDL_GetDisplays(&count));
    return @as([*]Display, @ptrCast(ret))[0..@intCast(count)];
}

/// Get the display containing a point.
///
/// ## Function Parameters
/// * `point`: The point to query.
///
/// ## Return Value
/// Returns the display containing the point.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getDisplayForPoint(point: rect.IPoint) !Display {
    const c_point = point.toSdl();
    const ret = c.SDL_GetDisplayForPoint(&c_point);
    return .{ .value = try errors.wrapCall(c.SDL_DisplayID, ret, 0) };
}

/// Get the display primarily containing a rect.
///
/// ## Function Parameters
/// * `space`: The rect to query.
///
/// ## Return Value
/// Returns the display containing the rect.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getDisplayForRect(space: rect.IRect) !Display {
    const c_rect = space.toSdl();
    const ret = c.SDL_GetDisplayForRect(&c_rect);
    return .{ .value = try errors.wrapCall(c.SDL_DisplayID, ret, 0) };
}

/// Get the number of video drivers compiled into SDL.
///
/// ## Return Value
/// Returns the number of built in video drivers.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getNumDrivers() usize {
    const ret = c.SDL_GetNumVideoDrivers();
    return @intCast(ret);
}

/// Get the current system theme.
///
/// ## Return Value
/// Returns the current system theme, light, dark, or unknown.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getSystemTheme() ?SystemTheme {
    const ret = c.SDL_GetSystemTheme();
    return SystemTheme.fromSdl(ret);
}

/// Get the name of a built in video driver.
///
/// ## Function Parameters
/// * `index`: The index of a video driver.
///
/// ## Return Value
/// Returns the name of the video driver with the given index.
///
/// ## Remarks
/// The video drivers are presented in the order in which they are normally checked during initialization.
///
/// The names of drivers are all simple, low-ASCII identifiers, like "cocoa", "x11" or "windows".
/// These never have Unicode characters, and are not meant to be proper names.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getDriverName(
    index: usize,
) ?[:0]const u8 {
    const ret = c.SDL_GetVideoDriver(
        @intCast(index),
    );
    if (ret == null)
        return null;
    return std.mem.span(ret);
}

/// Get a list of valid windows.
///
/// ## Return Value
/// Returns a slice of windows.
/// This is a single allocation that should be freed with `free()` when it is no longer needed.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getWindows() ![]Window {
    var count: c_int = undefined;
    const ret = try errors.wrapCallCPtr(?*c.SDL_Window, c.SDL_GetWindows(&count));
    return @as([*]Window, @ptrCast(ret))[0..@intCast(count)];
}

// Tests for the video subsystem.
test "Video" {
    errors.refAllDeclsRecursive(@This());
}
