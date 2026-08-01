const c = @import("c");
const errors = @import("errors.zig");
const pixels = @import("pixels.zig");
const properties = @import("properties.zig");
const sdl3 = @import("sdl3.zig");
const std = @import("std");
const surface = @import("surface.zig");

/// If camera is approved by user.
///
/// ## Version
/// This enum is provided by zig-sdl3.
pub const PermissionState = enum(c_int) {
    /// User has denied access to camera.
    denied = -1,
    /// User has not responded to allow camera yet.
    awaiting = 0,
    /// User has approved access to camera.
    approved = 1,
};

/// The position of camera in relation to system device.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Position = enum(c.SDL_CameraPosition) {
    front_facing = c.SDL_CAMERA_POSITION_FRONT_FACING,
    back_facing = c.SDL_CAMERA_POSITION_BACK_FACING,
};

/// This is a unique ID for a camera device for the time it is connected to the system, and is never reused for the lifetime of the application.
///
/// ## Remarks
/// If the device is disconnected and reconnected, it will get a new ID.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Id = packed struct {
    value: c.SDL_CameraID,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_CameraID) == @sizeOf(Id));
    }

    /// Get the human-readable device name for a camera.
    ///
    /// ## Function Parameters
    /// * `self`: The camera device instance ID.
    ///
    /// ## Return Value
    /// Returns a human-readable device name.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Id,
    ) ![:0]const u8 {
        const ret = c.SDL_GetCameraName(
            self.value,
        );
        return errors.wrapCallCString(ret);
    }

    /// Get the position of the camera in relation to the system.
    ///
    /// ## Function Parameters
    /// * `self`: The camera device instance ID.
    ///
    /// ## Return Value
    /// Returns the position of the camera on the system hardware.
    ///
    /// ## Remarks
    /// Most platforms will report `null`, but mobile devices, like phones, can often make a distinction between cameras on the front of the device
    /// (that points towards the user, for taking "selfies") and cameras on the back (for filming in the direction the user is facing).
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPosition(
        self: Id,
    ) ?Position {
        const ret = c.SDL_GetCameraPosition(
            self.value,
        );
        if (ret == c.SDL_CAMERA_POSITION_UNKNOWN)
            return null;
        return @enumFromInt(ret);
    }

    /// Get the list of native formats/sizes a camera supports.
    ///
    /// ## Function Parameters
    /// * `self`: The camera device instance ID.
    /// * `allocator`: Allocator to allocate the slice of specifications for.
    ///
    /// ## Return Value
    /// Returns a slice of camera specifications.
    /// Result must be freed.
    ///
    /// ## Remarks
    /// This returns a list of all formats and frame sizes that a specific camera can offer.
    /// This is useful if your app can accept a variety of image formats and sizes and so want to find the optimal spec that doesn't require conversion.
    ///
    /// This function isn't strictly required; if you call `camera.Camera.init()` with a `null` spec,
    /// SDL will choose a native format for you, and if you instead specify a desired format, it will transparently convert to the requested format on your behalf.
    ///
    /// Note that it's legal for a camera to supply an empty list. This is what will happen on Emscripten builds,
    /// since that platform won't tell anything about available cameras until you've opened one,
    /// and won't even tell if there is a camera until the user has given you permission to check through a scary warning popup.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSupportedFormats(
        self: Id,
        allocator: std.mem.Allocator,
    ) ![]Specification {
        var count: c_int = undefined;
        const val = c.SDL_GetCameraSupportedFormats(
            self.value,
            &count,
        );
        defer c.SDL_free(@ptrCast(val));
        const ret = try errors.wrapCallCPtr([*c]c.SDL_CameraSpec, val);
        var converted_ret = try allocator.alloc(Specification, @intCast(count));
        for (0..@intCast(count)) |ind| {
            converted_ret[ind] = Specification.fromSdl(ret[ind].*);
        }
        return converted_ret;
    }
};

/// The opaque structure used to identify an opened SDL camera.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Camera = extern struct {
    value: *c.SDL_Camera,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(*c.SDL_Camera) == @sizeOf(Camera));
    }

    /// Acquire a frame.
    ///
    /// ## Function Parameters
    /// * `self`: Opened camera device.
    ///
    /// ## Return Value
    /// Returns a new frame of video on success, `null` if none is currently available.
    /// Also returns frame's timestamp in nanoseconds, or `null` on error.
    ///
    /// ## Remarks
    /// The frame is a memory pointer to the image data, whose size and format are given by the spec requested when opening the device.
    ///
    /// This is a non blocking API. If there is a frame available, a not `null` surface is returned, and `timestamp_nanoseconds` is not null either.
    ///
    /// Note that an error case can also return `null`, but a `null` by itself is normal and just signifies that a new frame is not yet available.
    /// Note that even if a camera device fails outright (a USB camera is unplugged while in use, etc), SDL will send an event separately to notify the app,
    /// but continue to provide blank frames at ongoing intervals until `camera.Camera.deinit()` is called, so real failure here is almost always an out of memory condition.
    ///
    /// After use, the frame should be released with `camera.Camera.releaseFrame()`.
    /// If you don't do this, the system may stop providing more video!
    ///
    /// Do not call `surface.Surface.deinit()` on the returned surface!
    /// It must be given back to the camera subsystem with `camera.Camera.releaseFrame()`!
    ///
    /// If the system is waiting for the user to approve access to the camera, as some platforms require, this will return `null` (no frames available);
    /// you should either wait for an `event.Type.camera_device_approved` (or `event.Type.camera_device_denied`) event,
    /// or poll `camera.Camera.getPermissionState()` occasionally until it returns non-zero.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn acquireFrame(
        self: Camera,
    ) struct { ?surface.Surface, ?u64 } {
        var timestamp_nanoseconds: u64 = undefined;
        const ret = c.SDL_AcquireCameraFrame(
            self.value,
            &timestamp_nanoseconds,
        );
        return .{ if (ret != null) .{ .value = ret } else null, if (timestamp_nanoseconds == 0) null else timestamp_nanoseconds };
    }

    /// Use this function to shut down camera processing and close the camera device.
    ///
    /// ## Function Parameters
    /// * `self`: Opened camera device.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread, but no thread may reference device once this function is called.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Camera,
    ) void {
        c.SDL_CloseCamera(
            self.value,
        );
    }

    /// Get the spec that a camera is using when generating images.
    ///
    /// ## Function Parameters
    /// * `self`: Opened camera device.
    ///
    /// ## Return Value
    /// Camera specification.
    ///
    /// ## Remarks
    /// Note that this might not be the native format of the hardware, as SDL might be converting to this format behind the scenes.
    ///
    /// If the system is waiting for the user to approve access to the camera, as some platforms require, this will return false, but this isn't necessarily a fatal error;
    /// you should either wait for an `event.Type.camera_device_approved` (or `event.Type.camera_device_denied`) event,
    /// or poll `camera.Camera.getPermissionState()` occasionally until it returns non-zero.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFormat(
        self: Camera,
    ) !Specification {
        var specification: c.SDL_CameraSpec = undefined;
        const ret = c.SDL_GetCameraFormat(
            self.value,
            &specification,
        );
        try errors.wrapCallBool(ret);
        return Specification.fromSdl(specification);
    }

    /// Get the instance ID of an opened camera.
    ///
    /// ## Function Parameters
    /// * `self`: Opened camera device.
    ///
    /// ## Return Value
    /// Returns the instance ID of the specified camera.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getId(
        self: Camera,
    ) !Id {
        const ret = c.SDL_GetCameraID(
            self.value,
        );
        return Id{ .value = try errors.wrapCall(c.SDL_CameraID, ret, 0) };
    }

    /// Query if camera access has been approved by the user.
    ///
    /// ## Function Parameters
    /// * `self`: Opened camera device.
    ///
    /// ## Return Value
    /// Returns permission state.
    ///
    /// ## Remarks
    /// Cameras will not function between when the device is opened by the app and when the user permits access to the hardware.
    /// On some platforms, this presents as a popup dialog where the user has to explicitly approve access;
    /// on others the approval might be implicit and not alert the user at all.
    ///
    /// Instead of polling with this function, you can wait for a `event.Type.camera_device_approved` (or `event.Type.camera_device_denied`) event in the standard SDL event loop,
    /// which is guaranteed to be sent once when permission to use the camera is decided.
    ///
    /// If a camera is declined, there's nothing to be done but call `camera.Camera.deinit()` to dispose of it.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPermissionState(
        self: Camera,
    ) PermissionState {
        const ret = c.SDL_GetCameraPermissionState(
            self.value,
        );
        return @enumFromInt(ret);
    }

    /// Get the properties associated with an opened camera.
    ///
    /// ## Function Parameters
    /// * `self`: Opened camera device.
    ///
    /// ## Return Value
    /// Returns a valid properties group.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProperties(
        self: Camera,
    ) !properties.Group {
        const ret = c.SDL_GetCameraProperties(
            self.value,
        );
        if (ret == 0)
            return error.SdlError;
        return properties.Group{ .value = ret };
    }

    /// Open a video recording device (a camera).
    ///
    /// ## Function Parameters
    /// * `id`: The camera device instance ID.
    /// * `specification`: The desired format for data the device will provide. Can be `null`.
    ///
    /// ## Return Value
    /// Returns a camera object.
    ///
    /// ## Remarks
    /// You can open the device with any reasonable spec, and if the hardware can't directly support it, it will convert data seamlessly to the requested format.
    /// This might incur overhead, including scaling of image data.
    ///
    /// If you would rather accept whatever format the device offers,
    /// you can pass a `null` spec here and it will choose one for you (and you can use `surface.Surface`'s conversion/scaling functions directly if necessary).
    ///
    /// You can call `camera.Camera.getFormat()` to get the actual data format if passing a `null` spec here.
    /// You can see the exact specs a device can support without conversion with `camera.ID.getSupportedFormats()`.
    ///
    /// SDL will not attempt to emulate framerate; it will try to set the hardware to the rate closest to the requested speed,
    /// but it won't attempt to limit or duplicate frames artificially; call `camera.Camera.getFormat()` to see the actual framerate of the opened the device,
    /// and check your timestamps if this is crucial to your app!
    ///
    /// Note that the camera is not usable until the user approves its use!
    /// On some platforms, the operating system will prompt the user to permit access to the camera, and they can choose Yes or No at that point.
    /// Until they do, the camera will not be usable.
    /// The app should either wait for an `event.Type.camera_device_approved` (or `event.Type.camera_device_denied`) event,
    /// or poll `camera.Camera.getPermissionState()` occasionally until it returns non-zero.
    /// On platforms that don't require explicit user approval (and perhaps in places where the user previously permitted access),
    /// the approval event might come immediately, but it might come seconds, minutes, or hours later!
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        id: Id,
        specification: ?Specification,
    ) !Camera {
        const specification_sdl: ?c.SDL_CameraSpec = if (specification) |val| val.toSdl() else null;
        const ret = c.SDL_OpenCamera(
            id.value,
            if (specification != null) &specification_sdl.? else null,
        );
        if (ret == null)
            return error.SdlError;
        return Camera{ .value = ret.? };
    }

    /// Release a frame of video acquired from a camera.
    ///
    /// ## Function Parameters
    /// * `self`: Opened camera device.
    /// * `frame`: The video frame surface to release.
    ///
    /// ## Remarks
    /// Let the back-end re-use the internal buffer for camera.
    ///
    /// This function must be called only on surface objects returned by `camera.Camera.acquireFrame()`.
    /// This function should be called as quickly as possible after acquisition, as SDL keeps a small FIFO queue of surfaces for video frames;
    /// if surfaces aren't released in a timely manner, SDL may drop upcoming video frames from the camera.
    ///
    /// If the app needs to keep the surface for a significant time, they should make a copy of it and release the original.
    ///
    /// The app should not use the surface again after calling this function; assume the surface is freed and the pointer is invalid.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn releaseFrame(
        self: Camera,
        frame: surface.Surface,
    ) void {
        c.SDL_ReleaseCameraFrame(
            self.value,
            frame.value,
        );
    }
};

/// The details of an output format for a camera device.
///
/// ## Remarks
/// Cameras often support multiple formats; each one will be encapsulated in this struct.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Specification = struct {
    format: ?pixels.Format,
    colorspace: ?pixels.Colorspace,
    width: usize,
    height: usize,
    framerate_numerator: usize,
    framerate_denominator: usize,

    /// Convert from an SDL value.
    pub fn fromSdl(data: c.SDL_CameraSpec) Specification {
        return .{
            .format = if (data.format == c.SDL_PIXELFORMAT_UNKNOWN) null else @enumFromInt(data.format),
            .colorspace = if (data.colorspace == c.SDL_COLORSPACE_UNKNOWN) null else @enumFromInt(data.colorspace),
            .width = @intCast(data.width),
            .height = @intCast(data.height),
            .framerate_numerator = @intCast(data.framerate_numerator),
            .framerate_denominator = @intCast(data.framerate_denominator),
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: Specification) c.SDL_CameraSpec {
        return .{
            .format = if (self.format) |val| @intFromEnum(val) else c.SDL_PIXELFORMAT_UNKNOWN,
            .colorspace = if (self.colorspace) |val| @intFromEnum(val) else c.SDL_COLORSPACE_UNKNOWN,
            .width = @intCast(self.width),
            .height = @intCast(self.height),
            .framerate_numerator = @intCast(self.framerate_numerator),
            .framerate_denominator = @intCast(self.framerate_denominator),
        };
    }
};

/// Get a list of currently connected camera devices.
///
/// ## Return Value
/// Returns a slice of IDs terminated by 0.
/// This needs to be freed with `free()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getCameras() ![]Id {
    var count: c_int = undefined;
    const val = c.SDL_GetCameras(
        &count,
    );
    const ret = try errors.wrapCallCPtr(c.SDL_CameraID, val);
    return @as([*]Id, @ptrCast(ret))[0..@intCast(count)];
}

/// Get the name of the current camera driver.
///
/// ## Return Value
/// Returns the name of the current camera driver or `null` if no driver has been initialized.
///
/// ## Remarks
/// The names of drivers are all simple, low-ASCII identifiers, like "v4l2", "coremedia" or "android".
/// These never have Unicode characters, and are not meant to be proper names.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getCurrentDriverName() ?[:0]const u8 {
    const ret = c.SDL_GetCurrentCameraDriver();
    if (ret == null)
        return null;
    return std.mem.span(ret);
}

/// Use this function to get the name of a built in camera driver.
///
/// ## Function Parameters
/// * `driver_index`: The index of the camera driver, the value ranges from 0 to `camera.getNumDrivers() - 1` inclusively.
///
/// ## Return Value
/// Returns the name of the camera driver at the requested index, or `null` if an invalid index was specified.
///
/// ## Remarks
/// The list of camera drivers is given in the order that they are normally initialized by default;
/// the drivers that seem more reasonable to choose first (as far as the SDL developers believe) are earlier in the list.
///
/// The names of drivers are all simple, low-ASCII identifiers, like "v4l2", "coremedia" or "android".
/// These never have Unicode characters, and are not meant to be proper names.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getDriverName(
    driver_index: usize,
) ?[:0]const u8 {
    const ret = c.SDL_GetCameraDriver(
        @intCast(driver_index),
    );
    if (ret == null)
        return null;
    return std.mem.span(ret);
}

/// Use this function to get the number of built-in camera drivers.
///
/// ## Return Value
/// Returns the number of built-in camera drivers.
///
/// ## Remarks
/// This function returns a hardcoded number. This never returns a negative value; if there are no drivers compiled into this build of SDL,
/// this function returns zero.
/// The presence of a driver in this list does not mean it will function, it just means SDL is capable of interacting with that interface.
/// For example, a build of SDL might have v4l2 support, but if there's no kernel support available, SDL's v4l2 driver would fail if used.
///
/// By default, SDL tries all drivers, in its preferred order, until one is found to be usable.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getNumDrivers() usize {
    const ret = c.SDL_GetNumCameraDrivers();
    return @intCast(ret);
}

// Test camera functionality.
test "Camera" {
    errors.refAllDeclsRecursive(@This());

    // Global functions.
    const num_drivers = getNumDrivers();
    for (0..num_drivers) |driver_num| {
        _ = getDriverName(driver_num);
    }
    _ = getCurrentDriverName();

    // ID functions.
    const ids_raw: ?[]Id = getCameras() catch null;
    if (ids_raw) |ids| {
        defer sdl3.free(ids.ptr);
        for (ids) |id| {
            _ = id.getName() catch {};
            _ = id.getPosition();
            const specs: ?[]Specification = id.getSupportedFormats(std.testing.allocator) catch null;
            if (specs) |val| {
                std.testing.allocator.free(val);
            }

            // Camera functions.
            const cam_raw: ?Camera = Camera.init(id, null) catch null;
            if (cam_raw) |cam| {
                defer cam.deinit();
                _ = cam.getId() catch {};
                _ = cam.getFormat() catch {};
                _ = cam.getPermissionState();
                _ = cam.getProperties() catch {};
                const frame, _ = cam.acquireFrame();
                if (frame) |frame_surface| cam.releaseFrame(frame_surface);
            }
        }
    }
}
