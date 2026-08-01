const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// An opaque datatype that represents a loaded shared object.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const SharedObject = extern struct {
    value: *c.SDL_SharedObject,

    /// Dynamically load a shared object.
    ///
    /// ## Function Parameters
    /// * `name`: A system-dependent name of the object file.
    ///
    /// ## Return Value
    /// Returns an opaque pointer to the object handle.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn load(
        name: [:0]const u8,
    ) !SharedObject {
        const ret = c.SDL_LoadObject(
            name.ptr,
        );
        return SharedObject{ .value = try errors.wrapCallNull(*c.SDL_SharedObject, ret) };
    }

    /// Look up the address of the named function in a shared object.
    ///
    /// ## Function Parameters
    /// * `self`: A valid shared object handle from `SharedObject.load()`.
    /// * `name`: The name of the function to look up.
    ///
    /// ## Return Value
    /// Returns a pointer to the function.
    ///
    /// ## Remarks
    /// This function pointer is no longer valid after calling `SharedObject.unload()`.
    ///
    /// This function can only look up C function names.
    /// Other languages may have name mangling and intrinsic language support that varies from compiler to compiler.
    ///
    /// Make sure you declare your function pointers with the same calling convention as the actual library function.
    /// Your code will crash mysteriously if you do not do this.
    ///
    /// If the requested function doesn't exist, an error is returned.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// ```zig
    /// const lib = try sdl3.SharedObject.load("mylib.so");
    /// defer lib.unload();
    ///
    /// const my_func: *const fn(num: c_int) callconv(.c) void = @alignCast(@ptrCast(try lib.loadFunction("myfunc")));
    /// return my_func(15);
    /// ```
    pub fn loadFunction(
        self: SharedObject,
        name: [:0]const u8,
    ) !*const anyopaque {
        const ret = c.SDL_LoadFunction(
            self.value,
            name.ptr,
        );
        return errors.wrapCallNull(*const anyopaque, @ptrCast(ret));
    }

    /// Unload a shared object from memory.
    ///
    /// ## Function Parameters
    /// * `self`: A valid shared object handle returned by `SharedObject.load()`.
    ///
    /// ## Remarks
    /// Note that any pointers from this object looked up through `SharedObject.loadFunction()` will no longer be valid.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn unload(
        self: SharedObject,
    ) void {
        c.SDL_UnloadObject(
            self.value,
        );
    }
};

// Shared object functionality.
test "SharedObject" {
    errors.refAllDeclsRecursive(@This());

    const obj: ?SharedObject = SharedObject.load("Gota") catch null;
    if (obj) |val| {
        _ = val.loadFunction("Gota") catch {};
        val.unload();
    }
}
