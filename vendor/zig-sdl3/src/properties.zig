const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// A callback used to free resources when a property is deleted.
///
/// ## Function Parameters
/// * `user_data`: An app-defined pointer passed to the callback.
/// * `value`: The pointer assigned to the property to clean up.
///
/// ## Remarks
/// This should release any resources associated with value that are no longer needed.
///
/// This callback is set per-property.
/// Different properties in the same group can have different cleanup callbacks.
///
/// This callback will be called during `properties.Group.setPointerPropertyWithCleanup()` if the function fails for any reason.
///
/// ## Thread Safety
/// This callback may fire without any locks held; if this is a concern, the app should provide its own locking.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn CleanupCallback(comptime UserData: type, comptime ValueType: type) type {
    return *const fn (
        user_data: ?*UserData,
        value: *ValueType,
    ) void;
}

/// A callback used to enumerate all the properties in a group of properties.
///
/// ## Function Parameters
/// * `user_data`: An app-defined pointer passed to the callback.
/// * `props`: The group that is being enumerated.
/// * `name`: The next property name in the enumeration.
///
/// ## Remarks
/// This callback is called from `properties.group.enumerateProperties()`, and is called once per property in the set.
///
/// ## Thread Safety
/// `properties.group.enumerateProperties()` holds a lock on props during this callback.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn EnumerateCallback(
    comptime UserData: type,
) type {
    return *const fn (
        user_data: ?*UserData,
        props: Group,
        name: [:0]const u8,
    ) void;
}

/// SDL properties type.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Type = enum(c.SDL_PropertyType) {
    pointer = c.SDL_PROPERTY_TYPE_POINTER,
    string = c.SDL_PROPERTY_TYPE_STRING,
    number = c.SDL_PROPERTY_TYPE_NUMBER,
    float = c.SDL_PROPERTY_TYPE_FLOAT,
    boolean = c.SDL_PROPERTY_TYPE_BOOLEAN,
};

/// SDL properties group.
/// Properties can be added or removed at runtime.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Group = packed struct {
    value: c.SDL_PropertiesID,

    /// Clear a property from a group of properties.
    ///
    /// ## Function Parameters
    /// * `self`: Properties group to modify.
    /// * `name`: The name of the property to clear.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn clear(
        self: Group,
        name: [:0]const u8,
    ) !void {
        const ret = c.SDL_ClearProperty(
            self.value,
            name.ptr,
        );
        return errors.wrapCallBool(ret);
    }

    /// Copy a group of properties.
    ///
    /// ## Function Parameters
    /// * `self`: Properties group to copy from.
    /// * `dest`: The destination properties.
    ///
    /// ## Remarks
    /// Copy all the properties from one group of properties to another,
    /// with the exception of properties requiring cleanup (set using `properties.Group.setPointerPropertyWithCleanup()`), which will not be copied.
    /// Any property that already exists on `dest` will be overwritten.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn copyTo(
        self: Group,
        dest: Group,
    ) !void {
        const ret = c.SDL_CopyProperties(
            self.value,
            dest.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Destroy a group of properties.
    ///
    /// ## Function Parameters
    /// * `self`: Properties to destroy.
    ///
    /// ## Remarks
    /// All properties are deleted and their cleanup functions will be called, if any.
    ///
    /// ## Thread Safety
    /// This function should not be called while these properties are locked or other threads might be setting or getting values from these properties.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Group,
    ) void {
        c.SDL_DestroyProperties(
            self.value,
        );
    }

    /// Enumerate the properties contained in a group of properties.
    ///
    /// ## Function Parameters
    /// * `self`: The properties group to iterate.
    /// * `UserData`: Type of user data.
    /// * `callback`: Callback function to run for each property in the group.
    /// * `user_data`: User data to pass to the enumeration callback.
    ///
    /// ## Remarks
    /// The callback function is called for each property in the group of properties. The properties are locked during enumeration.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn enumerateProperties(
        self: Group,
        comptime UserData: type,
        comptime callback: EnumerateCallback(UserData),
        user_data: ?*UserData,
    ) !void {
        const Cb = struct {
            pub fn run(user_data_c: ?*anyopaque, props: c.SDL_PropertiesID, name: [*c]const u8) callconv(.c) void {
                callback(@ptrCast(@alignCast(user_data_c)), Group{ .value = props }, std.mem.span(name));
            }
        };
        const ret = c.SDL_EnumerateProperties(self.value, Cb.run, @ptrCast(user_data));
        return errors.wrapCallBool(ret);
    }

    /// Data for getting all properties.
    const GetAllData = struct {
        map: *std.StringHashMap(Property),
        err: ?std.mem.Allocator.Error,
    };

    /// Get a property from a group of properties.
    ///
    /// ## Function Parameters
    /// * `self`: The properties to query.
    /// * `name`: The name of the property to query.
    ///
    /// ## Return Value
    /// Returns the property, or `null` if it does not exist.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn get(
        self: Group,
        name: [:0]const u8,
    ) ?Property {
        return switch (c.SDL_GetPropertyType(self.value, name.ptr)) {
            c.SDL_PROPERTY_TYPE_POINTER => Property{ .pointer = c.SDL_GetPointerProperty(self.value, name.ptr, null) },
            c.SDL_PROPERTY_TYPE_STRING => Property{ .string = std.mem.span(c.SDL_GetStringProperty(self.value, name.ptr, "")) },
            c.SDL_PROPERTY_TYPE_NUMBER => Property{ .number = c.SDL_GetNumberProperty(self.value, name.ptr, 0) },
            c.SDL_PROPERTY_TYPE_FLOAT => Property{ .float = c.SDL_GetFloatProperty(self.value, name.ptr, 0) },
            c.SDL_PROPERTY_TYPE_BOOLEAN => Property{ .boolean = c.SDL_GetBooleanProperty(self.value, name.ptr, false) },
            else => null,
        };
    }

    /// Used for adding properties to a list.
    fn getAllEnumerateCallback(user_data: ?*GetAllData, props: Group, name: [:0]const u8) void {
        const data_ptr: *GetAllData = @ptrCast(@alignCast(user_data));
        if (props.get(name)) |val|
            data_ptr.map.put(name, val) catch |err| {
                data_ptr.err = err;
            };
    }

    /// Utility function for getting all the properties in the group.
    ///
    /// ## Function Parameters
    /// * `self`: Properties group to get all properties from.
    /// * `allocator`: Memory allocator to use to gather properties into.
    ///
    /// ## Return Value
    /// Returns a string hash map of the property or an error.
    ///
    /// ## Remarks
    /// Resulting map is owned and needs to be freed.
    ///
    /// ## Version
    /// This is provided by zig-sdl3.
    pub fn getAll(
        self: Group,
        allocator: std.mem.Allocator,
    ) !std.StringHashMap(Property) {
        var map = std.StringHashMap(Property).init(allocator);
        var data = GetAllData{
            .map = &map,
            .err = null,
        };
        try self.enumerateProperties(GetAllData, getAllEnumerateCallback, &data);
        if (data.err) |err|
            return err;
        return map;
    }

    /// Get the type of a property in a group of properties.
    ///
    /// ## Function Parameters
    /// * `self`: The properties to query.
    /// * `name`: The name of the property to query.
    ///
    /// ## Remarks
    /// Returns null if the property is not found.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getType(
        self: Group,
        name: [:0]const u8,
    ) ?Type {
        const ret = errors.wrapCall(c.SDL_PropertyType, c.SDL_GetPropertyType(
            self.value,
            name.ptr,
        ), c.SDL_PROPERTY_TYPE_INVALID) catch return null;
        return @enumFromInt(ret);
    }

    /// Return whether a property exists in a group of properties.
    ///
    /// ## Function Parameters
    /// * `self`: The properties to query.
    /// * `name`: The name of the property to query.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn has(
        self: Group,
        name: [:0]const u8,
    ) bool {
        const ret = c.SDL_HasProperty(
            self.value,
            name.ptr,
        );
        return ret;
    }

    /// Create a group of properties.
    ///
    /// ## Return Value
    /// Returns the created group of properties.
    ///
    /// ## Remarks
    /// All properties are automatically destroyed when `init.shutdown()` is called.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init() !Group {
        const ret = c.SDL_CreateProperties();
        return Group{ .value = try errors.wrapCall(c.SDL_PropertiesID, ret, 0) };
    }

    /// Lock a group of properties.
    ///
    /// ## Function Parameters
    /// * `self`: The properties to lock.
    ///
    /// ## Remarks
    /// Obtain a multi-threaded lock for these properties.
    /// Other threads will wait while trying to lock these properties until they are unlocked.
    /// Properties must be unlocked before they are destroyed.
    ///
    /// The lock is automatically taken when setting individual properties,
    /// this function is only needed when you want to set several properties atomically or want to guarantee that properties being queried aren't freed in another thread.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn lock(
        self: Group,
    ) !void {
        const ret = c.SDL_LockProperties(
            self.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set a property in the group.
    ///
    /// ## Function Parameters
    /// * `self`: Group of properties to set property in.
    /// * `name`: Name of property to set.
    /// * `value`: Value of the property to set.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn set(
        self: Group,
        name: [:0]const u8,
        value: Property,
    ) !void {
        const ret = switch (value) {
            .pointer => |pt| c.SDL_SetPointerProperty(self.value, name, pt),
            .string => |str| c.SDL_SetStringProperty(self.value, name, str),
            .number => |num| c.SDL_SetNumberProperty(self.value, name, num),
            .float => |num| c.SDL_SetFloatProperty(self.value, name, num),
            .boolean => |val| c.SDL_SetBooleanProperty(self.value, name, val),
        };
        return errors.wrapCallBool(ret);
    }

    /// Set a pointer property in a group of properties with a cleanup function that is called when the property is deleted.
    ///
    /// ## Function Parameters
    /// * `self`: The properties to modify.
    /// * `name`: The name of the property to modify.
    /// * `ValueType`: Type of the property value.
    /// * `value`: The new value of the property, or `null` to delete the property.
    /// * `UserData`: Type of user data for the callback.
    /// * `cleanup`: The function to call when this property is deleted, or `null` if no cleanup is necessary.
    /// * `user_data`: A pointer that is passed to the cleanup function.
    ///
    /// ## Remarks
    /// The cleanup function is also called if setting the property fails for any reason.
    ///
    /// For simply setting basic data types, like numbers, bools, or strings, use `properties.Group.set()` instead, as those functions will handle cleanup on your behalf.
    /// This function is only for more complex, custom data.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setPointerPropertyWithCleanup(
        self: Group,
        name: [:0]const u8,
        comptime ValueType: type,
        value: ?*anyopaque,
        comptime UserData: type,
        comptime cleanup: ?CleanupCallback(UserData, ValueType),
        user_data: ?*UserData,
    ) !void {
        const Cb = struct {
            pub fn run(user_data_c: ?*anyopaque, value_c: ?*anyopaque) callconv(.c) void {
                cleanup.?(@ptrCast(@alignCast(user_data_c)), @ptrCast(@alignCast(value_c)));
            }
        };
        const ret = c.SDL_SetPointerPropertyWithCleanup(
            self.value,
            name.ptr,
            value,
            if (cleanup != null) Cb.run else null,
            @ptrCast(user_data),
        );
        return errors.wrapCallBool(ret);
    }

    /// Unlock a group of properties.
    ///
    /// ## Function Parameters
    /// * `self`: The properties to unlock.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn unlock(
        self: Group,
    ) void {
        c.SDL_UnlockProperties(
            self.value,
        );
    }
};

/// Property.
pub const Property = union(Type) {
    pointer: ?*anyopaque,
    string: [:0]const u8,
    number: i64,
    float: f32,
    boolean: bool,
};

/// Get the global SDL properties.
///
/// ## Return Value
/// Returns the global properties map.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getGlobal() !Group {
    const ret = try errors.wrapCall(c.SDL_PropertiesID, c.SDL_GetGlobalProperties(), 0);
    return Group{ .value = ret };
}

fn testPropertiesCleanupCb(user_data: ?*void, value: *std.array_list.Managed(u32)) void {
    _ = user_data;
    value.deinit();
}

fn testEnumeratePropertiesCb(user_data: ?*void, props: Group, name: [:0]const u8) void {
    _ = user_data;
    _ = props;
    _ = name;
}

// Test out properties.
test "Properties" {
    errors.refAllDeclsRecursive(@This());

    _ = try getGlobal();
    const group = try Group.init();
    defer group.deinit();
    try group.lock();
    group.unlock();

    try std.testing.expectEqual(false, group.has("trial"));
    try std.testing.expectEqual(null, group.getType("trial"));
    try std.testing.expectEqual(null, group.get("trial"));

    try group.set("trial", Property{ .boolean = false });
    try std.testing.expectEqual(true, group.has("trial"));
    try std.testing.expectEqual(.boolean, group.getType("trial"));
    try std.testing.expectEqual(Property{ .boolean = false }, group.get("trial"));

    try group.set("trial", Property{ .boolean = true });
    try std.testing.expectEqual(true, group.has("trial"));
    try std.testing.expectEqual(.boolean, group.getType("trial"));
    try std.testing.expectEqual(Property{ .boolean = true }, group.get("trial"));

    try group.clear("trial");
    try std.testing.expectEqual(false, group.has("trial"));
    try std.testing.expectEqual(null, group.getType("trial"));
    try std.testing.expectEqual(null, group.get("trial"));

    try group.set("trial", Property{ .string = "Hello World!" });
    try std.testing.expectEqual(true, group.has("trial"));
    try std.testing.expectEqual(.string, group.getType("trial"));
    try std.testing.expectEqualStrings("Hello World!", group.get("trial").?.string);

    try group.set("trial", Property{ .number = -4 });
    try std.testing.expectEqual(true, group.has("trial"));
    try std.testing.expectEqual(.number, group.getType("trial"));
    try std.testing.expectEqual(Property{ .number = -4 }, group.get("trial"));

    try group.set("trial", Property{ .float = 5.3 });
    try std.testing.expectEqual(true, group.has("trial"));
    try std.testing.expectEqual(.float, group.getType("trial"));
    try std.testing.expectEqual(Property{ .float = 5.3 }, group.get("trial"));

    var num: i32 = 3;
    try group.set("trial", Property{ .pointer = &num });
    try std.testing.expectEqual(true, group.has("trial"));
    try std.testing.expectEqual(.pointer, group.getType("trial"));
    try std.testing.expectEqual(Property{ .pointer = &num }, group.get("trial"));

    try group.set("a", Property{ .number = 5 });
    try group.set("b", Property{ .boolean = false });
    var arr = std.array_list.Managed(u32).init(std.testing.allocator);
    try arr.append(8); // Ensure no memory leakage.
    try group.setPointerPropertyWithCleanup("c", std.array_list.Managed(u32), &arr, void, testPropertiesCleanupCb, null);

    const copied = try Group.init();
    defer copied.deinit();
    try group.copyTo(copied);

    var map = try copied.getAll(std.testing.allocator);
    defer map.deinit();
    try std.testing.expectEqual(3, map.count());

    try std.testing.expectEqual(Property{ .number = 5 }, map.get("a"));
    try std.testing.expectEqual(Property{ .boolean = false }, map.get("b"));
    try std.testing.expectEqual(Property{ .pointer = &num }, map.get("trial"));

    try group.enumerateProperties(void, testEnumeratePropertiesCb, null);
}
