const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// Pen axis indices.
///
/// ## Remarks
/// These are the valid values for the axis field in SDL_PenAxisEvent.
/// All axes are either normalized to 0..1 or report a (positive or negative) angle in degrees, with 0.0 representing the centre.
/// Not all pens/backends support all axes: unsupported axes are always zero.
///
/// To convert angles for tilt and rotation into vector representation, use `@sin()` on the XTILT, YTILT, or ROTATION component, for example:
///
/// `@sin(x_tilt * std.math.pi / 180)`.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Axis = enum(c.SDL_PenAxis) {
    /// Pen pressure. Unidirectional 0 to 1.0
    pressure = c.SDL_PEN_AXIS_PRESSURE,
    /// Pen horizontal tilt angle. Bidirectional -90.0 to 90.0 (left-to-right).
    x_tilt = c.SDL_PEN_AXIS_XTILT,
    /// Pen vertical tilt angle. Bidirectional -90.0 to 90.0 (top-to-down).
    y_tilt = c.SDL_PEN_AXIS_YTILT,
    /// Pen distance to drawing surface. Unidirectional 0.0 to 1.0.
    distance = c.SDL_PEN_AXIS_DISTANCE,
    /// Pen barrel rotation. Bidirectional -180 to 179.9 (clockwise, 0 is facing up, -180.0 is facing down).
    rotation = c.SDL_PEN_AXIS_ROTATION,
    /// Pen finger wheel or slider (e.g., Airbrush Pen). Unidirectional 0 to 1.0.
    slider = c.SDL_PEN_AXIS_SLIDER,
    /// Pressure from squeezing the pen (barrel pressure).
    tangential_pressure = c.SDL_PEN_AXIS_TANGENTIAL_PRESSURE,
};

/// An enum that describes the type of a pen device.
///
/// ## Remarks
/// A "direct" device is a pen that touches a graphic display (like an Apple Pencil on an iPad's screen).
/// "Indirect" devices touch an external tablet surface that is connected to the machine but is not a display (like a lower-end Wacom tablet connected over USB).
///
/// Apps may use this information to decide if they should draw a cursor; if the pen is touching the screen directly,
/// a cursor doesn't make sense and can be in the way, but becomes necessary for indirect devices to know where on the display they are interacting.
///
/// ## Version
/// This enum is available since SDL 3.4.0.
pub const DeviceType = enum(c.SDL_PenDeviceType) {
    /// Pen touches display.
    direct = c.SDL_PEN_DEVICE_TYPE_DIRECT,
    /// Pen touches something that isn't the display.
    indirect = c.SDL_PEN_DEVICE_TYPE_INDIRECT,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_PenDeviceType) !?DeviceType {
        if (value == c.SDL_PEN_DEVICE_TYPE_INVALID) {
            errors.callErrorCallback();
            return error.SdlError;
        }
        if (value == c.SDL_PEN_DEVICE_TYPE_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }
};

/// SDL pen instance IDs.
///
/// ## Remarks
/// These show up in pen events when SDL sees input from them.
/// They remain consistent as long as SDL can recognize a tool to be the same pen; but if a pen physically leaves the area and returns, it might get a new ID.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Id = packed struct {
    value: c.SDL_PenID,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_PenID) == @sizeOf(Id));
    }

    /// Get the device type of the given pen.
    ///
    /// ## Function Parameters
    /// * `self`: The pen instance ID.
    ///
    /// ## Return Value
    /// Returns the pen device type, or `null` if unknown.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn getDeviceType(
        self: Id,
    ) !?DeviceType {
        return try DeviceType.fromSdl(c.SDL_GetPenDeviceType(self.value));
    }
};

/// Pen input flags, as reported by various pen events.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const InputFlags = struct {
    /// Pen is pressed down.
    down: bool = false,
    /// Pen button 1 is pressed down.
    button1: bool = false,
    /// Pen button 2 is pressed down.
    button2: bool = false,
    /// Pen button 3 is pressed down.
    button3: bool = false,
    /// Pen button 4 is pressed down.
    button4: bool = false,
    /// Pen button 5 is pressed down.
    button5: bool = false,
    /// Eraser tip is used.
    eraser_tip: bool = false,

    /// Convert from an SDL value.
    pub fn fromSdl(flags: c.SDL_PenInputFlags) InputFlags {
        return .{
            .down = (flags & c.SDL_PEN_INPUT_DOWN) != 0,
            .button1 = (flags & c.SDL_PEN_INPUT_BUTTON_1) != 0,
            .button2 = (flags & c.SDL_PEN_INPUT_BUTTON_2) != 0,
            .button3 = (flags & c.SDL_PEN_INPUT_BUTTON_3) != 0,
            .button4 = (flags & c.SDL_PEN_INPUT_BUTTON_4) != 0,
            .button5 = (flags & c.SDL_PEN_INPUT_BUTTON_5) != 0,
            .eraser_tip = (flags & c.SDL_PEN_INPUT_ERASER_TIP) != 0,
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: InputFlags) c.SDL_PenInputFlags {
        return (if (self.down) @as(c.SDL_PenInputFlags, c.SDL_PEN_INPUT_DOWN) else 0) |
            (if (self.button1) @as(c.SDL_PenInputFlags, c.SDL_PEN_INPUT_BUTTON_1) else 0) |
            (if (self.button2) @as(c.SDL_PenInputFlags, c.SDL_PEN_INPUT_BUTTON_2) else 0) |
            (if (self.button3) @as(c.SDL_PenInputFlags, c.SDL_PEN_INPUT_BUTTON_3) else 0) |
            (if (self.button4) @as(c.SDL_PenInputFlags, c.SDL_PEN_INPUT_BUTTON_4) else 0) |
            (if (self.button5) @as(c.SDL_PenInputFlags, c.SDL_PEN_INPUT_BUTTON_5) else 0) |
            (if (self.eraser_tip) @as(c.SDL_PenInputFlags, c.SDL_PEN_INPUT_ERASER_TIP) else 0) |
            0;
    }
};

// Pen tests.
test "Pen" {
    errors.refAllDeclsRecursive(@This());
}
