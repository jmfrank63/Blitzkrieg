const c = @import("c");
const errors = @import("errors.zig");
const properties = @import("properties.zig");
const sdl3 = @import("sdl3.zig");
const std = @import("std");

/// A constant to represent standard gravity for accelerometer sensors.
///
/// ## Remarks
/// The accelerometer returns the current acceleration in SI meters per second squared.
/// This measurement includes the force of gravity, so a device at rest will have an value of `sensor.gravity` away from the center of the earth, which is a positive Y value.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub const gravity: f32 = c.SDL_STANDARD_GRAVITY;

/// The different sensors defined by SDL.
///
/// ## Remarks
/// Additional sensors may be available, using platform dependent semantics.
///
/// Here are the additional Android sensors:
/// https://developer.android.com/reference/android/hardware/SensorEvent.html#values
///
/// Accelerometer sensor notes:
/// The accelerometer returns the current acceleration in SI meters per second squared.
/// This measurement includes the force of gravity, so a device at rest will have an value of `sensor.gravity` away from the center of the earth, which is a positive Y value:
/// * `values[0]`: Acceleration on the x axis.
/// * `values[1]`: Acceleration on the y axis.
/// * `values[2]`: Acceleration on the z axis.
///
/// For phones and tablets held in natural orientation and game controllers held in front of you, the axes are defined as follows:
/// * -X ... +X : left ... right
/// * -Y ... +Y : bottom ... top
/// * -Z ... +Z : farther ... closer
///
/// The accelerometer axis data is not changed when the device is rotated.
///
/// Gyroscope sensor notes:
/// The gyroscope returns the current rate of rotation in radians per second.
/// The rotation is positive in the counter-clockwise direction.
/// That is, an observer looking from a positive location on one of the axes would see positive rotation on that axis when it appeared to be rotating counter-clockwise:
/// * `values[0]`: Angular speed around the x axis (pitch).
/// * `values[1]`: Angular speed around the y axis (yaw).
/// * `values[2]`: Angular speed around the z axis (roll).
///
/// For phones and tablets held in natural orientation and game controllers held in front of you, the axes are defined as follows:
/// * -X ... +X : left ... right
/// * -Y ... +Y : bottom ... top
/// * -Z ... +Z : farther ... closer
///
/// The gyroscope axis data is not changed when the device is rotated.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Type = enum(c.SDL_SensorType) {
    /// Unknown sensor type.
    unknown = c.SDL_SENSOR_UNKNOWN,
    /// Accelerometer.
    accelerometer = c.SDL_SENSOR_ACCEL,
    /// Gyroscope.
    gyroscope = c.SDL_SENSOR_GYRO,
    /// Accelerometer for left Joy-Con controller and Wii nunchuk.
    accelerometer_left = c.SDL_SENSOR_ACCEL_L,
    /// Gyroscope for left Joy-Con controller.
    gyroscope_left = c.SDL_SENSOR_GYRO_L,
    /// Accelerometer for right Joy-Con controller.
    accelerometer_right = c.SDL_SENSOR_ACCEL_R,
    /// Gyroscope for right Joy-Con controller.
    gyroscope_right = c.SDL_SENSOR_GYRO_R,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_SensorType) ?Type {
        if (value == c.SDL_SENSOR_INVALID)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?Type) c.SDL_SensorType {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_SENSOR_INVALID;
    }
};

/// This is a unique ID for a sensor for the time it is connected to the system, and is never reused for the lifetime of the application.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Id = packed struct {
    value: c.SDL_SensorID,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_SensorID) == @sizeOf(Id));
    }

    /// Return the SDL_Sensor associated with an instance ID.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor instance ID.
    ///
    /// ## Return Value
    /// Returns a sensor instance object.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSensor(
        self: Id,
    ) !Sensor {
        const ret = c.SDL_GetSensorFromID(
            self.value,
        );
        return Sensor{ .value = try errors.wrapCallNull(*c.SDL_Sensor, ret) };
    }

    /// Get the implementation dependent name of a sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor instance ID.
    ///
    /// ## Return Value
    /// Returns the sensor name or `null` if this ID is not valid.
    ///
    /// ## Remarks
    /// This can be called before any sensors are opened.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Id,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetSensorNameForID(
            self.value,
        );
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }

    /// Get the platform dependent type of a sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor instance ID.
    ///
    /// ## Return Value
    /// Returns the sensor platform dependent type or `null` if invalid.
    ///
    /// ## Remarks
    /// This can be called before any sensors are opened.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNonPortableType(
        self: Id,
    ) ?c_int {
        const ret = c.SDL_GetSensorNonPortableTypeForID(
            self.value,
        );
        if (ret == -1)
            return null;
        return @intCast(ret);
    }

    /// Get the type of a sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor instance ID.
    ///
    /// ## Return Value
    /// Returns the sensor type or `null` if the sensor is invalid.
    ///
    /// ## Remarks
    /// This can be called before any sensors are opened.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getType(
        self: Id,
    ) ?Type {
        const ret = c.SDL_GetSensorTypeForID(
            self.value,
        );
        return Type.fromSdl(ret);
    }
};

/// The opaque structure used to identify an opened SDL sensor.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Sensor = extern struct {
    value: *c.SDL_Sensor,

    /// Close a sensor previously opened with `Sensor.init()`.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor to close.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Sensor,
    ) void {
        c.SDL_CloseSensor(
            self.value,
        );
    }

    /// Get the current state of an opened sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor object to query.
    /// * `data`: Slice of data to fill with the current sensor state.
    ///
    /// ## Remarks
    /// The number of values and interpretation of the data is sensor dependent.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getData(
        self: Sensor,
        data: []f32,
    ) !void {
        const ret = c.SDL_GetSensorData(
            self.value,
            data.ptr,
            @intCast(data.len),
        );
        return errors.wrapCallBool(ret);
    }

    /// Get the instance ID of a sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The object to inspect.
    ///
    /// ## Return Value
    /// Returns the sensor instance ID.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getId(
        self: Sensor,
    ) !Id {
        const ret = c.SDL_GetSensorID(
            self.value,
        );
        return Id{ .value = try errors.wrapCall(c.SDL_SensorID, ret, 0) };
    }

    /// Get the implementation dependent name of a sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor object.
    ///
    /// ## Return Value
    /// Returns the sensor name.
    pub fn getName(
        self: Sensor,
    ) ![:0]const u8 {
        const ret = c.SDL_GetSensorName(
            self.value,
        );
        return errors.wrapCallCString(ret);
    }

    /// Get the platform dependent type of a sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor to inspect.
    ///
    /// ## Return Value
    /// Returns the sensor platform dependent type.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNonPortableType(
        self: Sensor,
    ) c_int {
        const ret = c.SDL_GetSensorNonPortableType(
            self.value,
        );
        return @intCast(ret);
    }

    /// Get the properties associated with a sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor object.
    ///
    /// ## Return Value
    /// Returns a valid property group.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProperties(
        self: Sensor,
    ) !properties.Group {
        const ret = c.SDL_GetSensorProperties(
            self.value,
        );
        return properties.Group{ .value = try errors.wrapCall(c.SDL_PropertiesID, ret, 0) };
    }

    /// Get the type of a sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The sensor object to inspect.
    ///
    /// ## Return Value
    /// Returns the sensor type.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getType(
        self: Sensor,
    ) ?Type {
        const ret = c.SDL_GetSensorType(
            self.value,
        );
        return Type.fromSdl(ret).?; // Sensor should not be null so this should not happen.
    }

    /// Open a sensor for use.
    ///
    /// ## Function Parameters
    /// * `id`: The sensor instance ID.
    ///
    /// ## Return Value
    /// Returns a sensor instance object.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        id: Id,
    ) !Sensor {
        const ret = c.SDL_OpenSensor(
            id.value,
        );
        return Sensor{ .value = try errors.wrapCallNull(*c.SDL_Sensor, ret) };
    }
};

/// Get a list of currently connected sensors.
///
/// ## Return Value
/// Returns a slice of sensor IDs.
/// This must be freed with `free()`.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getSensors() ![]Id {
    var count: c_int = undefined;
    const ret: [*]Id = @ptrCast(try errors.wrapCallCPtr(c.SDL_SensorID, c.SDL_GetSensors(
        &count,
    )));
    return ret[0..@intCast(count)];
}

/// Update the current state of the open sensors.
///
/// ## Remarks
/// This is called automatically by the event loop if sensor events are enabled.
///
/// This needs to be called from the thread that initialized the sensor subsystem.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn update() void {
    c.SDL_UpdateSensors();
}

// Sensor related tests.
test "Sensor" {
    errors.refAllDeclsRecursive(@This());

    defer sdl3.shutdown();
    try sdl3.init(.{ .sensor = true });
    defer sdl3.quit(.{ .sensor = true });

    update();

    const sensors = try getSensors();
    defer sdl3.free(sensors);
    for (sensors) |id| {
        _ = id.getType();
        _ = id.getNonPortableType();
        _ = id.getName();

        const sensor = try Sensor.init(id);
        defer sensor.deinit();

        var data: [3]f32 = undefined;
        _ = sensor.getData(&data) catch {};
        _ = sensor.getType();
        _ = sensor.getName() catch {};
        _ = sensor.getNonPortableType();
        _ = sensor.getProperties() catch {};
        try std.testing.expectEqual(id, try sensor.getId());
        try std.testing.expectEqual(sensor, try id.getSensor());
    }
}
