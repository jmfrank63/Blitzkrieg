const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// Application sandbox environment.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Sandbox = enum(c.SDL_Sandbox) {
    unknown_container = c.SDL_SANDBOX_UNKNOWN_CONTAINER,
    flatpak = c.SDL_SANDBOX_FLATPAK,
    snap = c.SDL_SANDBOX_SNAP,
    macos = c.SDL_SANDBOX_MACOS,
};

/// A callback to be used with `system.setX11EventHook()`.
///
/// ## Function Parameters
/// * `user_data`: The app-defined pointer provided to `system.setX11EventHook()`.
/// * `xevent`: A pointer to an Xlib XEvent union to process.
///
/// ## Return Value
/// Returns true to let event continue on, false to drop it.
///
/// ## Remarks
/// This callback may modify the event, and should return true if the event should continue to be processed, or false to prevent further processing.
///
/// As this is processing an event directly from the X11 event loop, this callback should do the minimum required work and return quickly.
///
/// ## Thread Safety
/// This may only be called (by SDL) from the thread handling the X11 event loop.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn X11EventHook(comptime UserData: type) type {
    return *const fn (user_data: ?*UserData, xevent: *c.XEvent) bool;
}

// getAndroidActivity
// getAndroidExternalStoragePath
// getAndroidExternalStorageState
// getAndroidInternalStoragePath
// getAndroidJNIEnv
// getAndroidSDKVersion
// getDirect3D9AdapterIndex
// getDXGIOutputInfo
// getGDKDefaultUser
// getGDKTaskQueue

/// Get the application sandbox environment, if any.
///
/// ## Return Value
/// Returns the application sandbox environment or `null` if there is none.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getSandbox() ?Sandbox {
    const ret = c.SDL_GetSandbox();
    if (ret == c.SDL_SANDBOX_NONE)
        return null;
    return @enumFromInt(ret);
}

// isChromebook
// isDeXMode

/// Query if the current device is a tablet.
///
/// ## Return Value
/// Returns true if the device is a tablet, false otherwise.
///
/// ## Remarks
/// If SDL can't determine this, it will return false.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn isTablet() bool {
    return c.SDL_IsTablet();
}

/// Query if the current device is a TV.
///
/// ## Return Value
/// Returns true if the device is a TV, false otherwise.
///
/// ## Remarks
/// If SDL can't determine this, it will return false.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn isTv() bool {
    return c.SDL_IsTablet();
}

/// Let iOS apps with external event handling report `onApplicationDidEnterBackground`.
///
/// ## Remarks
/// This functions allows iOS apps that have their own event handling to hook into SDL to generate SDL events.
/// This maps directly to an iOS-specific event, but since it doesn't do anything iOS-specific internally, it is available on all platforms,
/// in case it might be useful for some specific paradigm.
/// Most apps do not need to use this directly; SDL's internal event code will handle all this for windows created by `video.Window.init()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn onApplicationDidEnterBackground() void {
    c.SDL_OnApplicationDidEnterBackground();
}

/// Let iOS apps with external event handling report `onApplicationDidBecomeActive`.
///
/// ## Remarks
/// This functions allows iOS apps that have their own event handling to hook into SDL to generate SDL events.
/// This maps directly to an iOS-specific event, but since it doesn't do anything iOS-specific internally, it is available on all platforms,
/// in case it might be useful for some specific paradigm.
/// Most apps do not need to use this directly; SDL's internal event code will handle all this for windows created by `video.Window.init()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn onApplicationDidEnterForeground() void {
    c.SDL_OnApplicationDidEnterForeground();
}

/// Let iOS apps with external event handling report `onApplicationDidReceiveMemoryWarning`.
///
/// ## Remarks
/// This functions allows iOS apps that have their own event handling to hook into SDL to generate SDL events.
/// This maps directly to an iOS-specific event, but since it doesn't do anything iOS-specific internally, it is available on all platforms,
/// in case it might be useful for some specific paradigm.
/// Most apps do not need to use this directly; SDL's internal event code will handle all this for windows created by `video.Window.init()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn onApplicationDidReceiveMemoryWarning() void {
    c.SDL_OnApplicationDidReceiveMemoryWarning();
}

/// Let iOS apps with external event handling report `onApplicationWillEnterBackground`.
///
/// ## Remarks
/// This functions allows iOS apps that have their own event handling to hook into SDL to generate SDL events.
/// This maps directly to an iOS-specific event, but since it doesn't do anything iOS-specific internally, it is available on all platforms,
/// in case it might be useful for some specific paradigm.
/// Most apps do not need to use this directly; SDL's internal event code will handle all this for windows created by `video.Window.init()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn onApplicationWillEnterBackground() void {
    c.SDL_OnApplicationWillEnterBackground();
}

/// Let iOS apps with external event handling report `onApplicationWillEnterForeground`.
///
/// ## Remarks
/// This functions allows iOS apps that have their own event handling to hook into SDL to generate SDL events.
/// This maps directly to an iOS-specific event, but since it doesn't do anything iOS-specific internally, it is available on all platforms,
/// in case it might be useful for some specific paradigm.
/// Most apps do not need to use this directly; SDL's internal event code will handle all this for windows created by `video.Window.init()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn onApplicationWillEnterForeground() void {
    c.SDL_OnApplicationWillEnterForeground();
}

/// Let iOS apps with external event handling report `onApplicationWillTerminate`.
///
/// ## Remarks
/// This functions allows iOS apps that have their own event handling to hook into SDL to generate SDL events.
/// This maps directly to an iOS-specific event, but since it doesn't do anything iOS-specific internally, it is available on all platforms,
/// in case it might be useful for some specific paradigm.
/// Most apps do not need to use this directly; SDL's internal event code will handle all this for windows created by `video.Window.init()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn onApplicationWillTerminate() void {
    c.SDL_OnApplicationWillTerminate();
}

// requestAndroidPermission
// sendAndroidBackButton
// sendAndroidMessage
// setiOSAnimationCallback
// setiOSEventPump
// setLinuxThreadPriority
// setLinuxThreadPriorityAndPolicy
// setWindowsMessageHook

/// Set a callback for every X11 event.
///
/// ## Function Parameters
/// * `UserData`: Type for user data.
/// * `callback`: The event hook function to call.
/// * `user_data`: A pointer to pass to every iteration of `callback`.
///
/// ## Remarks
/// The callback may modify the event, and should return true if the event should continue to be processed, or false to prevent further processing.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setX11EventHook(
    comptime UserData: type,
    comptime callback: X11EventHook(UserData),
    user_data: ?*UserData,
) void {
    const Cb = struct {
        pub fn run(user_data_c: ?*anyopaque, xevent_c: ?*c.XEvent) callconv(.c) bool {
            return callback(@ptrCast(@alignCast(user_data_c)), xevent_c);
        }
    };
    c.SDL_SetX11EventHook(Cb.run, @ptrCast(user_data));
}

// showAndroidToast

// System tests.
test "System" {
    errors.refAllDeclsRecursive(@This());
}
