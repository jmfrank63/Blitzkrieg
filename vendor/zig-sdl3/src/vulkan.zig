const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");
const video = @import("video.zig");

/// Allocation callbacks.
pub const AllocationCallbacks = *const c.VkAllocationCallbacks;

/// Vulkan instance handle.
pub const Instance = c.VkInstance;

/// Vulkan physical device handle.
pub const PhysicalDevice = c.VkPhysicalDevice;

/// Vulkan surface handle.
pub const SurfaceKhr = c.VkSurfaceKHR;

/// Vulkan surface.
pub const Surface = struct {
    instance: Instance,
    surface: SurfaceKhr,
    allocator: ?AllocationCallbacks,

    /// Destroy the Vulkan rendering surface of a window.
    ///
    /// ## Function Parameters
    /// * `self`: The Vulkan surface to destroy.
    ///
    /// ## Remarks
    /// This should be called before `video.Window.deinit()`, if `vulkan.Surface.init()` was called after `video.Window.init()`.
    ///
    /// The instance must have been created with extensions returned by vulkan.getInstanceExtensions() enabled
    /// and surface must have been created successfully by an `vulkan.Surface.init()` call.
    ///
    /// If allocator is `null`, Vulkan will use the system default allocator.
    /// This argument is passed directly to Vulkan and isn't used by SDL itself.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(self: Surface) void {
        c.SDL_Vulkan_DestroySurface(
            self.instance,
            self.surface,
            self.allocator,
        );
    }

    /// Create a Vulkan rendering surface for a window.
    ///
    /// ## Function Parameters
    /// * `window`: The window to which to attach the Vulkan surface.
    /// * `instance`: The Vulkan instance handle.
    /// * `allocator`: A VkAllocationCallbacks struct, which lets the app set the allocator that creates the surface. Can be `null`.
    ///
    /// ## Return Value
    /// The newly created Vulkan surface.
    ///
    /// ## Remarks
    /// The window must have been created with the `video.WindowFlags.vulkan` flag
    /// and instance must have been created with extensions returned by `vulkan.getInstanceExtensions()` enabled.
    ///
    /// If allocator is `null`, Vulkan will use the system default allocator.
    /// This argument is passed directly to Vulkan and isn't used by SDL itself.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn init(
        window: video.Window,
        instance: Instance,
        allocator: ?AllocationCallbacks,
    ) !Surface {
        var surface: c.VkSurfaceKHR = undefined;
        const ret = c.SDL_Vulkan_CreateSurface(
            window.value,
            instance,
            allocator,
            &surface,
        );
        try errors.wrapCallBool(ret);
        return .{
            .instance = instance,
            .surface = surface,
            .allocator = allocator,
        };
    }
};

/// Get the Vulkan instance extensions needed for vkCreateInstance.
///
/// ## Return Value
/// Returns a slice of extension name strings on success.
///
/// ## Remarks
/// This should be called after either calling `vulkan.loadLibrary()` or creating a `video.Window` with the `video.WindowFlags.vulkan` flag.
///
/// On return, the variable pointed to by count will be set to the number of elements returned,
/// suitable for using with `VkInstanceCreateInfo::enabledExtensionCount`,
/// and the returned array can be used with `VkInstanceCreateInfo::ppEnabledExtensionNames`, for calling Vulkan's `vkCreateInstance` API.
///
/// You should not free the returned array; it is owned by SDL.
///
/// ## Version
/// This function is available since SDL 3.2.0.
///
/// ## Code Examples
/// TODO!!!
pub fn getInstanceExtensions() ![]const [*:0]const u8 {
    var count: u32 = undefined;
    const val = c.SDL_Vulkan_GetInstanceExtensions(&count);
    const ret = try errors.wrapCallCPtrConst([*c]const u8, val);
    return @as([*]const [*:0]const u8, @ptrCast(ret))[0..count];
}

/// Query support for presentation via a given physical device and queue family.
///
/// ## Function Parameters
/// * `instance`: The Vulkan instance handle.
/// * `physical_device`: A valid Vulkan physical device handle.
/// * `queue_family_index`: A valid queue family index for the given physical device.
///
/// ## Return Value
/// Returns true if supported, false if unsupported or an error occurred.
///
/// ## Remarks
/// The instance must have been created with extensions returned by `vulkan.getInstanceExtensions()` enabled.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPresentationSupport(
    instance: Instance,
    physical_device: PhysicalDevice,
    queue_family_index: u32,
) bool {
    return c.SDL_Vulkan_GetPresentationSupport(
        instance,
        physical_device,
        queue_family_index,
    );
}

/// Get the address of the `vkGetInstanceProcAddr` function.
///
/// ## Return Value
/// Returns the function pointer for `vkGetInstanceProcAddr`.
///
/// ## Remarks
/// This should be called after either calling `vulkan.loadLibrary()` or creating a `video.Window` with the `video.WindowFlags.vulkan` flag.
///
/// The actual type of the returned function pointer is `PFN_vkGetInstanceProcAddr`, but that isn't available because the Vulkan headers are not included here.
/// You should cast the return value of this function to that type.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getVkGetInstanceProcAddr() !*const anyopaque {
    const ret = c.SDL_Vulkan_GetVkGetInstanceProcAddr();
    return errors.wrapCallNull(*const anyopaque, @ptrCast(ret));
}

/// Dynamically load the Vulkan loader library.
///
/// ## Function Parameters
/// * `path`: The platform dependent Vulkan loader library name or `null`.
///
/// ## Remarks
/// This should be called after initializing the video driver, but before creating any Vulkan windows.
/// If no Vulkan loader library is loaded, the default library will be loaded upon creation of the first Vulkan window.
///
/// SDL keeps a counter of how many times this function has been successfully called, so it is safe to call this function multiple times,
/// so long as it is eventually paired with an equivalent number of calls to `vulkan.unloadLibrary()`.
/// The path argument is ignored unless there is no library currently loaded,
/// and the library isn't actually unloaded until there have been an equivalent number of calls to `vulkan.unloadLibrary()`.
///
/// It is fairly common for Vulkan applications to link with libvulkan instead of explicitly loading it at run time.
/// This will work with SDL provided the application links to a dynamic library and both it and SDL use the same search path.
///
/// If you specify a non-null path, an application should retrieve all of the Vulkan functions it uses from the dynamic library using `vulkan.getVkGetInstanceProcAddr`
/// unless you can guarantee path points to the same vulkan loader library the application linked to.
///
/// On Apple devices, if path is `null`, SDL will attempt to find the `vkGetInstanceProcAddr` address within all the Mach-O images of the current process.
/// This is because it is fairly common for Vulkan applications to link with `libvulkan` (and historically MoltenVK was provided as a static library).
/// If it is not found, on macOS, SDL will attempt to load `vulkan.framework/vulkan`, `libvulkan.1.dylib`, `MoltenVK.framework/MoltenVK`, and `libMoltenVK.dylib`, in that order.
/// On iOS, SDL will attempt to load `libMoltenVK.dylib`.
/// Applications using a dynamic framework or `.dylib` must ensure it is included in its application bundle.
///
/// On non-Apple devices, application linking with a static `libvulkan` is not supported.
/// Either do not link to the Vulkan loader or link to a dynamic library version.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn loadLibrary(path: ?[:0]const u8) !void {
    const lib: [*c]const u8 = if (path) |val| val.ptr else null;
    return errors.wrapCallBool(c.SDL_Vulkan_LoadLibrary(lib));
}

/// Unload the Vulkan library previously loaded by `vulkan.loadLibrary()`.
///
/// ## Remarks
/// SDL keeps a counter of how many times this function has been called, so it is safe to call this function multiple times,
/// so long as it is paired with an equivalent number of calls to `vulkan.loadLibrary()`.
/// The library isn't actually unloaded until there have been an equivalent number of calls to `vulkan.unloadLibrary()`.
///
/// Once the library has actually been unloaded, if any Vulkan instances remain, they will likely crash the program.
/// Clean up any existing Vulkan resources, and destroy appropriate windows, renderers and GPU devices before calling this function.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn unloadLibrary() void {
    c.SDL_Vulkan_UnloadLibrary();
}

// Vulkan related testing.
test "Vulkan" {
    errors.refAllDeclsRecursive(@This());
}
