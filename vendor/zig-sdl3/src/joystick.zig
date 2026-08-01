const c = @import("c");
const errors = @import("errors.zig");
const guid = @import("guid.zig");
const power = @import("power.zig");
const properties = @import("properties.zig");
const sensor = @import("sensor.zig");
const std = @import("std");

/// The largest value a joystick axis can report.
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub const axis_max: i16 = @intCast(c.SDL_JOYSTICK_AXIS_MAX);

/// The smallest value a joystick axis can report.
///
/// ## Remarks
/// This is a negative number!
///
/// ## Version
/// This macro is available since SDL 3.2.0.
pub const axis_min: i16 = @intCast(c.SDL_JOYSTICK_AXIS_MIN);

/// The list of axes available on a gamepad.
///
/// ## Remarks
/// Thumbstick axis values range from `joystick.axis_min` to `joystick.axis_max`, and are centered within ~8000 of zero,
/// though advanced UI will allow users to set or autodetect the dead zone, which varies between gamepads.
///
/// Trigger axis values range from `0` (released) to `joystick.axis_max` (fully pressed) when reported by `gamepad.Gamepad.getAxis()`.
/// Note that this is not the same range that will be reported by the lower-level `joystick.Joystick.getAxis()`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const AxisMask = packed struct(c_uint) {
    left_x: bool = false,
    left_y: bool = false,
    right_x: bool = false,
    right_y: bool = false,
    left_trigger: bool = false,
    right_trigger: bool = false,
    _: u26 = 0,

    // Struct tests.
    comptime {
        std.debug.assert(@as(c_int, @bitCast(AxisMask{ .left_x = true })) == 1 << c.SDL_GAMEPAD_AXIS_LEFTX);
        std.debug.assert(@as(c_int, @bitCast(AxisMask{ .left_y = true })) == 1 << c.SDL_GAMEPAD_AXIS_LEFTY);
        std.debug.assert(@as(c_int, @bitCast(AxisMask{ .right_x = true })) == 1 << c.SDL_GAMEPAD_AXIS_RIGHTX);
        std.debug.assert(@as(c_int, @bitCast(AxisMask{ .right_y = true })) == 1 << c.SDL_GAMEPAD_AXIS_RIGHTY);
        std.debug.assert(@as(c_int, @bitCast(AxisMask{ .left_trigger = true })) == 1 << c.SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
        std.debug.assert(@as(c_int, @bitCast(AxisMask{ .right_trigger = true })) == 1 << c.SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    }
};

/// The list of buttons available on a gamepad.
///
/// ## Remarks
/// For controllers that use a diamond pattern for the face buttons, the south/east/west/north buttons below correspond to the locations in the diamond pattern.
/// For Xbox controllers, this would be A/B/X/Y, for Nintendo Switch controllers, this would be B/A/Y/X, for GameCube controllers this would be A/X/B/Y,
/// for PlayStation controllers this would be Cross/Circle/Square/Triangle.
///
/// For controllers that don't use a diamond pattern for the face buttons, the south/east/west/north buttons indicate the buttons labeled A, B, C, D, or 1, 2, 3, 4,
/// or for controllers that aren't labeled, they are the primary, secondary, etc. buttons.
///
/// The activate action is often the south button and the cancel action is often the east button, but in some regions this is reversed,
/// so your game should allow remapping actions based on user preferences.
///
/// You can query the labels for the face buttons using `gamepad.Gamepad.getButtonLabel()`.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const ButtonMask = packed struct(c.SDL_GamepadButton) {
    /// Bottom face button (e.g. Xbox A button).
    south: bool = false,
    /// Right face button (e.g. Xbox B button).
    east: bool = false,
    /// Left face button (e.g. Xbox X button).
    west: bool = false,
    /// Top face button (e.g. Xbox Y button).
    north: bool = false,
    back: bool = false,
    guide: bool = false,
    start: bool = false,
    left_stick: bool = false,
    right_stick: bool = false,
    left_shoulder: bool = false,
    right_shoulder: bool = false,
    dpad_up: bool = false,
    dpad_down: bool = false,
    dpad_left: bool = false,
    dpad_right: bool = false,
    /// Additional button (e.g. Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro capture button, Amazon Luna microphone button, Google Stadia capture button).
    misc1: bool = false,
    /// Upper or primary paddle, under your right hand (e.g. Xbox Elite paddle P1).
    right_paddle1: bool = false,
    /// Upper or primary paddle, under your left hand (e.g. Xbox Elite paddle P3).
    left_paddle1: bool = false,
    /// Lower or secondary paddle, under your right hand (e.g. Xbox Elite paddle P2).
    right_paddle2: bool = false,
    /// Lower or secondary paddle, under your left hand (e.g. Xbox Elite paddle P4).
    left_paddle2: bool = false,
    /// PS4/PS5 touchpad button.
    touchpad: bool = false,
    /// Additional button.
    misc2: bool = false,
    /// Additional button.
    misc3: bool = false,
    /// Additional button.
    misc4: bool = false,
    /// Additional button.
    misc5: bool = false,
    /// Additional button.
    misc6: bool = false,
    _: u6 = 0,

    // Struct tests.
    comptime {
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .south = true })) == 1 << c.SDL_GAMEPAD_BUTTON_SOUTH);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .east = true })) == 1 << c.SDL_GAMEPAD_BUTTON_EAST);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .west = true })) == 1 << c.SDL_GAMEPAD_BUTTON_WEST);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .north = true })) == 1 << c.SDL_GAMEPAD_BUTTON_NORTH);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .back = true })) == 1 << c.SDL_GAMEPAD_BUTTON_BACK);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .guide = true })) == 1 << c.SDL_GAMEPAD_BUTTON_GUIDE);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .start = true })) == 1 << c.SDL_GAMEPAD_BUTTON_START);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .left_stick = true })) == 1 << c.SDL_GAMEPAD_BUTTON_LEFT_STICK);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .right_stick = true })) == 1 << c.SDL_GAMEPAD_BUTTON_RIGHT_STICK);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .left_shoulder = true })) == 1 << c.SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .right_shoulder = true })) == 1 << c.SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .dpad_up = true })) == 1 << c.SDL_GAMEPAD_BUTTON_DPAD_UP);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .dpad_down = true })) == 1 << c.SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .dpad_left = true })) == 1 << c.SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .dpad_right = true })) == 1 << c.SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .misc1 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_MISC1);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .right_paddle1 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .left_paddle1 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_LEFT_PADDLE1);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .right_paddle2 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .left_paddle2 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_LEFT_PADDLE2);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .touchpad = true })) == 1 << c.SDL_GAMEPAD_BUTTON_TOUCHPAD);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .misc2 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_MISC2);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .misc3 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_MISC3);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .misc4 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_MISC4);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .misc5 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_MISC5);
        std.debug.assert(@as(c_int, @bitCast(ButtonMask{ .misc6 = true })) == 1 << c.SDL_GAMEPAD_BUTTON_MISC6);
    }
};

/// Possible connection states for a joystick device.
///
/// ## Remarks
/// This is used by `joystick.Joystick.getConnectionState()` to report how a device is connected to the system.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const ConnectionState = enum(c.SDL_JoystickConnectionState) {
    wired = c.SDL_JOYSTICK_CONNECTION_WIRED,
    wireless = c.SDL_JOYSTICK_CONNECTION_WIRELESS,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_JoystickConnectionState) !?ConnectionState {
        if (value == c.SDL_JOYSTICK_CONNECTION_INVALID) {
            errors.callErrorCallback();
            return error.SdlError;
        }
        if (value == c.SDL_JOYSTICK_CONNECTION_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: errors.Error!?ConnectionState) c.SDL_JoystickConnectionState {
        if (self) |val| {
            if (val) |num| {
                return @intFromEnum(num);
            }
            return c.SDL_JOYSTICK_CONNECTION_UNKNOWN;
        } else |_| return c.SDL_JOYSTICK_CONNECTION_INVALID;
    }
};

/// This is a unique ID for a joystick for the time it is connected to the system, and is never reused for the lifetime of the application.
///
/// ## Remarks
/// If the joystick is disconnected and reconnected, it will get a new ID.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Id = packed struct {
    value: c.SDL_JoystickID,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_JoystickID) == @sizeOf(Id));
    }

    /// Detach a virtual joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinitVirtual(
        self: Id,
    ) !void {
        return errors.wrapCallBool(c.SDL_DetachVirtualJoystick(self.value));
    }

    /// Attach a new virtual joystick.
    ///
    /// ## Function Parameters
    /// * `virtual`: Joystick description.
    ///
    /// ## Return Value
    /// Returns the joystick instance ID.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initVirtual(
        comptime UserData: type,
        virtual: VirtualJoystickDescription(UserData),
    ) !Id {
        const virtual_sdl = virtual.toSdl();
        return .{ .value = try errors.wrapCall(c.SDL_JoystickID, c.SDL_AttachVirtualJoystick(&virtual_sdl), 0) };
    }

    /// Get the implementation-dependent GUID of a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns the GUID of the selected joystick.
    /// If called with an invalid `instance_id`, this function returns a zero GUID.
    ///
    /// ## Remarks
    /// This can be called before any joysticks are opened.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getGuid(
        self: Id,
    ) guid.Guid {
        const ret = c.SDL_GetJoystickGUIDForID(
            self.value,
        );
        return .{
            .value = ret,
        };
    }

    /// Get the implementation dependent name of a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns the name of the selected joystick.
    ///
    /// ## Remarks
    /// This can be called before any joysticks are opened.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Id,
    ) ![:0]const u8 {
        const ret = c.SDL_GetJoystickNameForID(
            self.value,
        );
        return errors.wrapCallCString(ret);
    }

    /// Get the implementation dependent path of a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns the path of the selected joystick.
    ///
    /// ## Remarks
    /// This can be called before any joysticks are opened.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPath(
        self: Id,
    ) ![:0]const u8 {
        const ret = c.SDL_GetJoystickPathForID(
            self.value,
        );
        return errors.wrapCallCString(ret);
    }

    /// Get the player index of a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns the player index of a joystick, or `null` if it's not available.
    ///
    /// ## Remarks
    /// This can be called before any joysticks are opened.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPlayerIndex(
        self: Id,
    ) ?usize {
        const ret = c.SDL_GetJoystickPlayerIndexForID(
            self.value,
        );
        if (ret == -1)
            return null;
        return @intCast(ret);
    }

    /// Get the USB product ID of a joystick, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns the USB product ID of the selected joystick.
    /// If called with an invalid `instance_id`, this function returns `null`.
    ///
    /// ## Remarks
    /// This can be called before any joysticks are opened.
    /// If the product ID isn't available this function returns `null`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProduct(
        self: Id,
    ) ?u16 {
        const ret = c.SDL_GetJoystickProductForID(
            self.value,
        );
        if (ret == 0)
            return null;
        return ret;
    }

    /// Get the USB product version of a joystick, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns the product version of the selected joystick.
    /// If called with an invalid `instance_id`, this function returns `null`.
    ///
    /// ## Remarks
    /// This can be called before any joysticks are opened.
    /// If the product version isn't available this function returns `null`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProductVersion(
        self: Id,
    ) ?u16 {
        const ret = c.SDL_GetJoystickProductVersionForID(
            self.value,
        );
        if (ret == 0)
            return null;
        return ret;
    }

    /// Get the type of a joystick, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns the USB type of the selected joystick.
    ///
    /// ## Remarks
    /// This can be called before any joysticks are opened.
    /// If called with an invalid `instance_id`, this function returns `null`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getType(
        self: Id,
    ) ?Type {
        const ret = c.SDL_GetJoystickTypeForID(
            self.value,
        );
        return Type.fromSdl(ret);
    }

    /// Get the USB vendor ID of a joystick, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns the USB vendor ID of the selected joystick.
    /// If called with an invalid `instance_id`, this function returns `null`.
    ///
    /// ## Remarks
    /// This can be called before any joysticks are opened.
    /// If the vendor ID isn't available this function returns `null`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getVendor(
        self: Id,
    ) ?u16 {
        const ret = c.SDL_GetJoystickVendorForID(
            self.value,
        );
        if (ret == 0)
            return null;
        return ret;
    }

    /// Query whether or not a joystick is virtual.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns true if the joystick is virtual, false otherwise.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn isVirtual(
        self: Id,
    ) bool {
        const ret = c.SDL_IsJoystickVirtual(
            self.value,
        );
        return ret;
    }
};

/// Get the current state of a POV hat on a joystick.
///
/// ## Version
/// This enum is provided by zig-sdl3.
pub const Hat = enum(u8) {
    centered = c.SDL_HAT_CENTERED,
    up = c.SDL_HAT_UP,
    right = c.SDL_HAT_RIGHT,
    down = c.SDL_HAT_DOWN,
    left = c.SDL_HAT_LEFT,
    right_up = c.SDL_HAT_RIGHTUP,
    right_down = c.SDL_HAT_RIGHTDOWN,
    left_up = c.SDL_HAT_LEFTUP,
    left_down = c.SDL_HAT_LEFTDOWN,
};

/// The joystick structure used to identify an SDL joystick.
///
/// ## Remarks
/// This is opaque data.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Joystick = struct {
    value: *c.SDL_Joystick,

    /// Read only properties provided by SDL.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const Properties = struct {
        /// True if this joystick has an LED that has adjustable brightness.
        mono_led: ?bool,
        /// True if this joystick has an LED that has adjustable color.
        rgb_led: ?bool,
        /// True if this joystick has a player LED.
        player_led: ?bool,
        /// True if this joystick has left/right rumble.
        rumble: ?bool,
        /// True if this joystick has simple trigger rumble.
        trigger_rumble: ?bool,

        /// Get properties from SDL.
        pub fn fromSdl(value: properties.Group) Properties {
            return .{
                .mono_led = if (value.get(c.SDL_PROP_JOYSTICK_CAP_MONO_LED_BOOLEAN)) |val| val.boolean else null,
                .rgb_led = if (value.get(c.SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN)) |val| val.boolean else null,
                .player_led = if (value.get(c.SDL_PROP_JOYSTICK_CAP_PLAYER_LED_BOOLEAN)) |val| val.boolean else null,
                .rumble = if (value.get(c.SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN)) |val| val.boolean else null,
                .trigger_rumble = if (value.get(c.SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN)) |val| val.boolean else null,
            };
        }
    };

    /// Get the status of a specified joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick to query.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn connected(
        self: Joystick,
    ) !void {
        const ret = c.SDL_JoystickConnected(
            self.value,
        );
        return errors.wrapCallBool(ret);
    }

    /// Close a joystick previously opened.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick device to close.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Joystick,
    ) void {
        c.SDL_CloseJoystick(
            self.value,
        );
    }

    /// Get the current state of an axis control on a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    /// * `index`: The axis to query; the axis indices start at index `0`.
    ///
    /// ## Return Value
    /// Returns a 16-bit signed integer representing the current position of the axis.
    ///
    /// ## Remarks
    /// SDL makes no promises about what part of the joystick any given axis refers to.
    /// Your game should have some sort of configuration UI to let users specify what each axis should be bound to.
    /// Alternately, SDL's higher-level Game Controller API makes a great effort to apply order to this lower-level interface,
    /// so you know that a specific axis is the "left thumb stick," etc.
    ///
    /// The value returned by this is a signed integer (`-32768` to `32767`) representing the current position of the axis.
    /// It may be necessary to impose certain tolerances on these values to account for jitter.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getAxis(
        self: Joystick,
        index: usize,
    ) !i16 {
        return errors.wrapCall(i16, c.SDL_GetJoystickAxis(self.value, @intCast(index)), 0);
    }

    /// Get the initial state of an axis control on a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    /// * `index`: The axis to query; the axis indices start at index `0`.
    ///
    /// ## Return Value
    /// Returns the axis initial value if it exists.
    ///
    /// ## Remarks
    /// The state is a value ranging from `-32768` to `32767`.
    /// The axis indices start at index `0`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getAxisInitialState(
        self: Joystick,
        index: usize,
    ) ?i16 {
        var state: i16 = undefined;
        const ret = c.SDL_GetJoystickAxisInitialState(self.value, @intCast(index), &state);
        if (ret)
            return state;
        return null;
    }

    /// Get the ball axis change since the last poll.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    /// * `index`: The ball index to query; ball indices start at index `0`.
    ///
    /// ## Return Value
    /// Returns the difference in x and y position since the last poll in that order.
    ///
    /// ## Remarks
    /// Trackballs can only return relative motion since the last call to this, these motion deltas are returned.
    ///
    /// Most joysticks do not have trackballs.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getBall(
        self: Joystick,
        index: usize,
    ) !struct { isize, isize } {
        var dx: c_int = undefined;
        var dy: c_int = undefined;
        const ret = c.SDL_GetJoystickBall(self.value, @intCast(index), &dx, &dy);
        try errors.wrapCallBool(ret);
        return .{ @intCast(dx), @intCast(dy) };
    }

    /// Get the current state of a button on a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    /// * `index`: The button index to get the state from; the button indices start at index `0`.
    ///
    /// ## Return Value
    /// Returns true if the button is pressed, false otherwise.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getButton(
        self: Joystick,
        index: usize,
    ) bool {
        return c.SDL_GetJoystickButton(self.value, @intCast(index));
    }

    /// Get the connection state of a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the connection state.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getConnectionState(
        self: Joystick,
    ) !?ConnectionState {
        return ConnectionState.fromSdl(c.SDL_GetJoystickConnectionState(self.value));
    }

    /// Get the firmware version of an opened joystick, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the firmware version of the selected joystick, or `null` if unavailable.
    ///
    /// ## Remarks
    /// If the firmware version isn't available this function returns `null`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFirmwareVersion(
        self: Joystick,
    ) ?u16 {
        const ret = c.SDL_GetJoystickFirmwareVersion(
            self.value,
        );
        if (ret == 0)
            return null;
        return @intCast(ret);
    }

    /// Get the implementation-dependent GUID of a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the GUID of the selected joystick.
    /// If called with an invalid inedx, this function returns a zero GUID.
    ///
    /// ## Remarks
    /// This function requires an open joystick.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getGuid(
        self: Joystick,
    ) guid.Guid {
        const ret = c.SDL_GetJoystickGUID(
            self.value,
        );
        return .{
            .value = ret,
        };
    }

    /// Get the current state of a POV hat on a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    /// * `hat_index`: The hat index to get the state from; indices start at index `0`.
    ///
    /// ## Return Value
    /// Returns the current hat position.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getHat(
        self: Joystick,
        hat_index: usize,
    ) Hat {
        const ret = c.SDL_GetJoystickHat(
            self.value,
            @intCast(hat_index),
        );
        return @enumFromInt(ret);
    }

    /// Get the instance ID of an opened joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the instance ID of the specified joystick.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getId(
        self: Joystick,
    ) !Id {
        const ret = c.SDL_GetJoystickID(
            self.value,
        );
        return Id{ .value = try errors.wrapCall(c.SDL_JoystickID, ret, 0) };
    }

    /// Get the implementation dependent name of a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the name of the selected joystick.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Joystick,
    ) ![:0]const u8 {
        const ret = c.SDL_GetJoystickName(
            self.value,
        );
        return errors.wrapCallCString(ret);
    }

    /// Get the number of general axis controls on a joystick.
    ///
    /// ## Function Paramaters
    /// * `self`: Structure containing joystick information.
    ///
    /// ## Return Value
    /// Returns the number of axis controls/number of axes.
    ///
    /// ## Remarks
    /// Often, the directional pad on a game controller will either look like 4 separate buttons or a POV hat, and not axes, but all of this is up to the device and platform.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNumAxes(
        self: Joystick,
    ) !usize {
        const ret = c.SDL_GetNumJoystickAxes(
            self.value,
        );
        return @intCast(try errors.wrapCall(c_int, ret, -1));
    }

    /// Get the number of trackballs on a joystick.
    ///
    /// ## Function Paramaters
    /// * `self`: Structure containing joystick information.
    ///
    /// ## Return Value
    /// Returns the number of trackballs.
    ///
    /// ## Remarks
    /// Joystick trackballs have only relative motion events associated with them and their state cannot be polled.
    ///
    /// Most joysticks do not have trackballs.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNumBalls(
        self: Joystick,
    ) !usize {
        const ret = c.SDL_GetNumJoystickBalls(
            self.value,
        );
        return @intCast(try errors.wrapCall(c_int, ret, -1));
    }

    /// Get the number of buttons on a joystick.
    ///
    /// ## Function Paramaters
    /// * `self`: Structure containing joystick information.
    ///
    /// ## Return Value
    /// Returns the number of buttons.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNumButtons(
        self: Joystick,
    ) !usize {
        const ret = c.SDL_GetNumJoystickButtons(
            self.value,
        );
        return @intCast(try errors.wrapCall(c_int, ret, -1));
    }

    /// Get the number of POV hats on a joystick.
    ///
    /// ## Function Paramaters
    /// * `self`: Structure containing joystick information.
    ///
    /// ## Return Value
    /// Returns the number of POV hats.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNumHats(
        self: Joystick,
    ) !usize {
        const ret = c.SDL_GetNumJoystickHats(
            self.value,
        );
        return @intCast(try errors.wrapCall(c_int, ret, -1));
    }

    /// Get the implementation dependent path of a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the path of the selected joystick.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPath(
        self: Joystick,
    ) ![:0]const u8 {
        const ret = c.SDL_GetJoystickPath(
            self.value,
        );
        return errors.wrapCallCString(ret);
    }

    /// Get the player index of an opened joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the player index of a joystick, or `null` if it's not available.
    ///
    /// ## Remarks
    /// For XInput controllers this returns the XInput user index.
    /// Many joysticks will not be able to supply this information.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPlayerIndex(
        self: Joystick,
    ) ?usize {
        const ret = c.SDL_GetJoystickPlayerIndex(
            self.value,
        );
        if (ret == -1)
            return null;
        return @intCast(ret);
    }

    /// Get the battery state of a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the power state and the percent 0 to 100 (if possible to get).
    ///
    /// ## Remarks
    /// You should never take a battery status as absolute truth.
    /// Batteries (especially failing batteries) are delicate hardware, and the values reported here are best estimates based on what that hardware reports.
    /// It's not uncommon for older batteries to lose stored power much faster than it reports, or completely drain when reporting it has 20 percent left, etc.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPowerInfo(
        self: Joystick,
    ) !struct { power.PowerState, ?u7 } {
        var percent: c_int = undefined;
        const ret = c.SDL_GetJoystickPowerInfo(
            self.value,
            &percent,
        );
        return .{ @enumFromInt(try errors.wrapCall(c_int, ret, c.SDL_POWERSTATE_ERROR)), if (percent == -1) null else @intCast(percent) };
    }

    /// Get the USB product ID of an opened joystick, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the USB product ID of the selected joystick, or `null` if unavailable.
    ///
    /// ## Remarks
    /// If the product ID isn't available this function returns `null`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProduct(
        self: Joystick,
    ) ?u16 {
        const ret = c.SDL_GetJoystickProduct(
            self.value,
        );
        if (ret == 0)
            return null;
        return ret;
    }

    /// Get the product version of an opened joystick, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the product version of the selected joystick, or `null` if unavailable.
    ///
    /// ## Remarks
    /// If the product version isn't available this function returns `null`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProductVersion(
        self: Joystick,
    ) ?u16 {
        const ret = c.SDL_GetJoystickProductVersion(
            self.value,
        );
        if (ret == 0)
            return null;
        return ret;
    }

    /// Get the properties associated with a joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns properties.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProperties(
        self: Joystick,
    ) !Properties {
        const ret = c.SDL_GetJoystickProperties(self.value);
        return Properties.fromSdl(.{ .value = try errors.wrapCall(c.SDL_PropertiesID, ret, 0) });
    }

    /// Get the serial number of an opened joystick, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the serial number of the selected joystick, or `null` if unavailable.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSerial(
        self: Joystick,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetJoystickSerial(
            self.value,
        );
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }

    /// Get the type of an opened joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns type of the selected joystick.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getType(
        self: Joystick,
    ) ?Type {
        const ret = c.SDL_GetJoystickType(
            self.value,
        );
        return Type.fromSdl(ret);
    }

    /// Get the USB vendor ID of an opened joystick, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick.
    ///
    /// ## Return Value
    /// Returns the USB vendor ID of the selected joystick, or `null` if unavailable.
    ///
    /// ## Remarks
    /// If the vendor ID isn't available this function returns `null`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getVendor(
        self: Joystick,
    ) ?u16 {
        const ret = c.SDL_GetJoystickVendor(
            self.value,
        );
        if (ret == 0)
            return null;
        return ret;
    }

    /// Open a joystick for use.
    ///
    /// ## Function Parameters
    /// * `id`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns an opened joystick.
    ///
    /// ## Remarks
    /// The joystick subsystem must be initialized before a joystick can be opened for use.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        id: Id,
    ) !Joystick {
        const ret = c.SDL_OpenJoystick(
            id.value,
        );
        return Joystick{ .value = try errors.wrapCallNull(*c.SDL_Joystick, ret) };
    }

    /// Start a rumble effect.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick to vibrate.
    /// * `low_frequency_rumble`: The intensity of the low frequency (left) rumble motor.
    /// * `high_frequency_rumble`: The intensity of the high frequency (right) rumble motor.
    /// * `duration_milliseconds`: The duration of the rumble effect, in milliseconds.
    ///
    /// ## Return Value
    /// Returns if rumble is supported.
    ///
    /// ## Remarks
    /// Each call to this function cancels any previous trigger rumble effect, and calling it with `0` intensity stops any rumbling.
    ///
    /// This function requires you to process SDL events or call `joystick.update()` to update rumble state.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn rumble(
        self: Joystick,
        low_frequency_rumble: u16,
        high_frequency_rumble: u16,
        duration_milliseconds: u32,
    ) bool {
        const ret = c.SDL_RumbleJoystick(
            self.value,
            low_frequency_rumble,
            high_frequency_rumble,
            duration_milliseconds,
        );
        return ret;
    }

    /// Start a rumble effect in the joystick's triggers.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick to vibrate.
    /// * `left_rumble`: The intensity of the left trigger rumble motor.
    /// * `right_rumble`: The intensity of the right trigger rumble motor.
    /// * `duration_milliseconds`: The duration of the rumble effect, in milliseconds.
    ///
    /// ## Remarks
    /// Each call to this function cancels any previous trigger rumble effect, and calling it with `0` intensity stops any rumbling.
    ///
    /// Note that this is rumbling of the triggers and not the game controller as a whole.
    /// This is currently only supported on Xbox One controllers.
    /// If you want the (more common) whole-controller rumble, use `joystick.Joystick.rumble()` instead.
    ///
    /// This function requires you to process SDL events or call `joystick.update()` to update rumble state.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn rumbleTriggers(
        self: Joystick,
        left_rumble: u16,
        right_rumble: u16,
        duration_milliseconds: u32,
    ) !void {
        const ret = c.SDL_RumbleJoystickTriggers(
            self.value,
            left_rumble,
            right_rumble,
            duration_milliseconds,
        );
        return errors.wrapCallBool(ret);
    }

    /// Send a joystick specific effect packet.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick to affect.
    /// * `data`: The data to send to the joystick.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn sendEffect(
        self: Joystick,
        data: []const u8,
    ) !void {
        const ret = c.SDL_SendJoystickEffect(
            self.value,
            data.ptr,
            @intCast(data.len),
        );
        return errors.wrapCallBool(ret);
    }

    /// Send a sensor update for an opened virtual joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The virtual joystick on which to set state.
    /// * `sensor_type`: The type of the sensor on the virtual joystick to update.
    /// * `sensor_timestamp_nanoseconds`: A 64-bit timestamp in nanoseconds associated with the sensor reading.
    /// * `data`: The data associated with the sensor reading.
    ///
    /// ## Remarks
    /// Please note that values set here will not be applied until the next call to `joystick.update()`, which can either be called directly,
    /// or can be called indirectly through various other SDL APIs, including, but not limited to the following:
    /// `events.poll()`, `events.pump()`, `events.waitTimeout()`, `events.wait()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn sendVirtualSensorData(
        self: Joystick,
        sensor_type: sensor.Type,
        sensor_timestamp_nanoseconds: u64,
        data: []const f32,
    ) !void {
        const ret = c.SDL_SendJoystickVirtualSensorData(
            self.value,
            @intFromEnum(sensor_type),
            @intCast(sensor_timestamp_nanoseconds),
            data.ptr,
            @intCast(data.len),
        );
        return errors.wrapCallBool(ret);
    }

    /// Update a joystick's LED color.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick to update.
    /// * `r`: The intensity of the red LED.
    /// * `g`: The intensity of the green LED.
    /// * `b`: The intensity of the blue LED.
    ///
    /// ## Remarks
    /// An example of a joystick LED is the light on the back of a PlayStation 4's DualShock 4 controller.
    ///
    /// For joysticks with a single color LED, the maximum of the RGB values will be used as the LED brightness.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setLed(
        self: Joystick,
        r: u8,
        g: u8,
        b: u8,
    ) !void {
        const ret = c.SDL_SetJoystickLED(
            self.value,
            @intCast(r),
            @intCast(g),
            @intCast(b),
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the player index of an opened joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The joystick opened.
    /// * `player_index`: Player index to assign to this joystick, or `null` to clear the player index and turn off player LEDs.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setPlayerIndex(
        self: Joystick,
        player_index: ?usize,
    ) !void {
        const ret = c.SDL_SetJoystickPlayerIndex(
            self.value,
            if (player_index) |val| @intCast(val) else -1,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the state of an axis on an opened virtual joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The virtual joystick on which to set state.
    /// * `axis_index`: The index of the axis on the virtual joystick to update.
    /// * `value`: The new value for the specified axis.
    ///
    /// ## Remarks
    /// Please note that values set here will not be applied until the next call to `joystick.update()`, which can either be called directly,
    /// or can be called indirectly through various other SDL APIs, including, but not limited to the following:
    /// `events.poll()`, `events.pump()`, `events.waitTimeout()`, `events.wait()`.
    ///
    /// Note that when sending trigger axes, you should scale the value to the full range of `i16`.
    /// For example, a trigger at rest would have the value of `joystick.axis_min`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setVirtualAxis(
        self: Joystick,
        axis_index: usize,
        value: i16,
    ) !void {
        const ret = c.SDL_SetJoystickVirtualAxis(
            self.value,
            @intCast(axis_index),
            @intCast(value),
        );
        return errors.wrapCallBool(ret);
    }

    /// Generate ball motion on an opened virtual joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The virtual joystick on which to set state.
    /// * `ball_index`: The index of the ball on the virtual joystick to update.
    /// * `x_rel`: The relative motion on the X axis.
    /// * `y_rel`: The relative motion on the Y axis.
    ///
    /// ## Remarks
    /// Please note that values set here will not be applied until the next call to `joystick.update()`, which can either be called directly,
    /// or can be called indirectly through various other SDL APIs, including, but not limited to the following:
    /// `events.poll()`, `events.pump()`, `events.waitTimeout()`, `events.wait()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setVirtualBall(
        self: Joystick,
        ball_index: u31,
        x_rel: i16,
        y_rel: i16,
    ) !void {
        const ret = c.SDL_SetJoystickVirtualBall(
            self.value,
            @intCast(ball_index),
            @intCast(x_rel),
            @intCast(y_rel),
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the state of a button on an opened virtual joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The virtual joystick on which to set state.
    /// * `button_index`: The index of the button on the virtual joystick to update.
    /// * `down`: True if the button is pressed, false otherwise.
    ///
    /// ## Remarks
    /// Please note that values set here will not be applied until the next call to `joystick.update()`, which can either be called directly,
    /// or can be called indirectly through various other SDL APIs, including, but not limited to the following:
    /// `events.poll()`, `events.pump()`, `events.waitTimeout()`, `events.wait()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setVirtualButton(
        self: Joystick,
        button_index: usize,
        down: bool,
    ) !void {
        const ret = c.SDL_SetJoystickVirtualButton(
            self.value,
            @intCast(button_index),
            down,
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the state of a hat on an opened virtual joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The virtual joystick on which to set state.
    /// * `hat_index`: The index of the hat on the virtual joystick to update.
    /// * `value`: The new value for the specified hat.
    ///
    /// ## Remarks
    /// Please note that values set here will not be applied until the next call to `joystick.update()`, which can either be called directly,
    /// or can be called indirectly through various other SDL APIs, including, but not limited to the following:
    /// `events.poll()`, `events.pump()`, `events.waitTimeout()`, `events.wait()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setVirtualHat(
        self: Joystick,
        hat_index: usize,
        value: Hat,
    ) !void {
        const ret = c.SDL_SetJoystickVirtualHat(
            self.value,
            @intCast(hat_index),
            @intFromEnum(value),
        );
        return errors.wrapCallBool(ret);
    }

    /// Set touchpad finger state on an opened virtual joystick.
    ///
    /// ## Function Parameters
    /// * `self`: The virtual joystick on which to set state.
    /// * `touchpad_index`: The index of the touchpad on the virtual joystick to update.
    /// * `finger_index`: The index of the finger on the touchpad to set.
    /// * `down`: True if the finger is pressed, false if the finger is released.
    /// * `x`: The x coordinate of the finger on the touchpad, normalized `0` to `1`, with the origin in the upper left.
    /// * `y`: The y coordinate of the finger on the touchpad, normalized `0` to `1`, with the origin in the upper left.
    /// * `pressure`: The pressure of the finger.
    ///
    /// ## Remarks
    /// Please note that values set here will not be applied until the next call to `joystick.update()`, which can either be called directly,
    /// or can be called indirectly through various other SDL APIs, including, but not limited to the following:
    /// `events.poll()`, `events.pump()`, `events.waitTimeout()`, `events.wait()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setVirtualTouchpad(
        self: Joystick,
        touchpad_index: usize,
        finger_index: usize,
        down: bool,
        x: f32,
        y: f32,
        pressure: f32,
    ) !void {
        const ret = c.SDL_SetJoystickVirtualTouchpad(
            self.value,
            @intCast(touchpad_index),
            @intCast(finger_index),
            down,
            x,
            y,
            pressure,
        );
        return errors.wrapCallBool(ret);
    }
};

/// An enum of some common joystick types.
///
/// ## Remarks
/// In some cases, SDL can identify a low-level joystick as being a certain type of device,
/// and will report it through `joystick.Joystick.getType()` (or `joystick.ID.getType()`).
///
/// This is by no means a complete list of everything that can be plugged into a computer.
///
/// You may refer to XInput Controller Types table for a general understanding of each joystick type.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Type = enum(c.SDL_JoystickType) {
    gamepad = c.SDL_JOYSTICK_TYPE_GAMEPAD,
    wheel = c.SDL_JOYSTICK_TYPE_WHEEL,
    arcade_stick = c.SDL_JOYSTICK_TYPE_ARCADE_STICK,
    flight_stick = c.SDL_JOYSTICK_TYPE_FLIGHT_STICK,
    dance_pad = c.SDL_JOYSTICK_TYPE_DANCE_PAD,
    guitar = c.SDL_JOYSTICK_TYPE_GUITAR,
    drum_kit = c.SDL_JOYSTICK_TYPE_DRUM_KIT,
    arcade_pad = c.SDL_JOYSTICK_TYPE_ARCADE_PAD,
    throttle = c.SDL_JOYSTICK_TYPE_THROTTLE,

    /// Convert from SDL value.
    pub fn fromSdl(value: c.SDL_JoystickType) ?Type {
        if (value == c.SDL_JOYSTICK_TYPE_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: ?Type) c.SDL_JoystickType {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_JOYSTICK_TYPE_UNKNOWN;
    }
};

/// The structure that describes a virtual joystick.
///
/// ## Remarks
/// All elements of this structure are optional.
///
/// Version
/// This struct is available since SDL 3.2.0.
pub fn VirtualJoystickDescription(comptime UserData: type) type {
    return struct {
        /// Joystick type.
        joystick_type: ?Type = null,
        /// The USB vendor ID of this joystick.
        vendor_id: u16 = 0,
        /// The USB product ID of this joystick.
        product_id: u16 = 0,
        /// The number of axes on this joystick.
        num_axes: u16 = 0,
        /// The number of buttons on this joystick.
        num_buttons: u16 = 0,
        /// The number of balls on this joystick.
        num_balls: u16 = 0,
        /// The number of hats on this joystick.
        num_hats: u16 = 0,
        /// A mask of which buttons are valid for this controller.
        buttons: ButtonMask = .{},
        /// A mask of which axes are valid for this controller.
        axes: AxisMask = .{},
        /// The name of the joystick.
        name: ?[:0]const u8 = null,
        /// Touchpad descriptions.
        touchpads: []const VirtualJoystickTouchpadDescription = &.{},
        /// Sensor descriptions.
        sensors: []const VirtualJoystickSensorDescription = &.{},
        /// User data pointer passed to callbacks.
        user_data: ?*UserData = null,

        /// Called when the joystick state should be updated.
        comptime update: ?*const fn (user_data: ?*UserData) void = null,

        /// Called when the player index is set.
        comptime set_player_index: ?*const fn (user_data: ?*UserData, player_index: usize) void = null,

        /// Implements rumble functionality.
        comptime rumble: ?*const fn (user_data: ?*UserData, low_frequency_rumble: u16, high_frequency_rumble: u16) bool = null,

        /// Implements rumble triggers functionality.
        comptime rumble_triggers: ?*const fn (user_data: ?*UserData, left_rumble: u16, right_rumble: u16) anyerror!void = null,

        /// Set the joystick LED.
        comptime set_led: ?*const fn (user_data: ?*UserData, red: u8, green: u8, blue: u8) anyerror!void = null,

        /// Send a joystick effect.
        comptime send_effect: ?*const fn (user_data: ?*UserData, data: []const u8) anyerror!void = null,

        /// Set sensors enabled.
        comptime set_sensors_enabled: ?*const fn (user_data: ?*UserData, enabled: bool) anyerror!void = null,

        /// Cleans up the userdata when the joystick is detached.
        comptime cleanup: ?*const fn (user_data: ?*UserData) void = null,

        /// Convert to an SDL value.
        pub fn toSdl(self: VirtualJoystickDescription(UserData)) c.SDL_VirtualJoystickDesc {
            const Cb = struct {
                fn update(user_data_c: ?*anyopaque) callconv(.c) void {
                    self.update.?(@ptrCast(@alignCast(user_data_c)));
                }
                fn set_player_index(user_data_c: ?*anyopaque, player_index: c_int) callconv(.c) void {
                    self.set_player_index.?(@ptrCast(@alignCast(user_data_c)), @intCast(player_index));
                }
                fn rumble(user_data_c: ?*anyopaque, low_frequency_rumble: u16, high_frequency_rumble: u16) callconv(.c) bool {
                    return self.rumble.?(@ptrCast(@alignCast(user_data_c)), low_frequency_rumble, high_frequency_rumble);
                }
                fn rumble_triggers(user_data_c: ?*anyopaque, left_rumble: u16, right_rumble: u16) callconv(.c) bool {
                    self.rumble_triggers.?(@ptrCast(@alignCast(user_data_c)), left_rumble, right_rumble) catch return false;
                    return true;
                }
                fn set_led(user_data_c: ?*anyopaque, red: u8, green: u8, blue: u8) callconv(.c) bool {
                    self.set_led.?(@ptrCast(@alignCast(user_data_c)), red, green, blue) catch return false;
                    return true;
                }
                fn send_effect(user_data_c: ?*anyopaque, data: ?*const anyopaque, size: c_int) callconv(.c) bool {
                    self.send_effect.?(@ptrCast(@alignCast(user_data_c)), @as([*]const u8, @ptrCast(@alignCast(data)))[0..@intCast(size)]) catch return false;
                    return true;
                }
                fn set_sensors_enabled(user_data_c: ?*anyopaque, enabled: bool) callconv(.c) bool {
                    self.set_sensors_enabled.?(@ptrCast(@alignCast(user_data_c)), enabled) catch return false;
                    return true;
                }
                fn cleanup(user_data_c: ?*anyopaque) callconv(.c) void {
                    self.cleanup.?(@ptrCast(@alignCast(user_data_c)));
                }
            };
            return .{
                .type = @intCast(Type.toSdl(self.joystick_type)),
                .vendor_id = self.vendor_id,
                .product_id = self.product_id,
                .naxes = self.num_axes,
                .nbuttons = self.num_buttons,
                .nballs = self.num_balls,
                .nhats = self.num_hats,
                .button_mask = @bitCast(self.buttons),
                .axis_mask = @bitCast(self.axes),
                .name = if (self.name) |val| val.ptr else null,
                .touchpads = @as([*c]const c.SDL_VirtualJoystickTouchpadDesc, @ptrCast(self.touchpads.ptr)),
                .ntouchpads = @intCast(self.touchpads.len),
                .sensors = @as([*c]const c.SDL_VirtualJoystickSensorDesc, @ptrCast(self.sensors.ptr)),
                .nsensors = @intCast(self.sensors.len),
                .userdata = self.user_data,
                .Update = if (self.update != null) Cb.update else null,
                .SetPlayerIndex = if (self.set_player_index != null) Cb.set_player_index else null,
                .Rumble = if (self.rumble != null) Cb.rumble else null,
                .RumbleTriggers = if (self.rumble_triggers != null) Cb.rumble_triggers else null,
                .SetLED = if (self.set_led != null) Cb.set_led else null,
                .SendEffect = if (self.send_effect != null) Cb.send_effect else null,
                .SetSensorsEnabled = if (self.set_sensors_enabled != null) Cb.set_sensors_enabled else null,
                .Cleanup = if (self.cleanup != null) Cb.cleanup else null,
                .version = @sizeOf(c.SDL_VirtualJoystickDesc),
            };
        }
    };
}

/// The structure that describes a virtual joystick sensor.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const VirtualJoystickSensorDescription = extern struct {
    /// The type of this sensor.
    sensor_type: sensor.Type,
    /// The update frequency of this sensor, may be `0`s.
    rate: f32,

    // Size tests.
    comptime {
        errors.assertStructsEqual(c.SDL_VirtualJoystickSensorDesc, VirtualJoystickSensorDescription);
    }
};

/// The structure that describes a virtual joystick touchpad.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const VirtualJoystickTouchpadDescription = extern struct {
    /// The number of simultaneous fingers on this touchpad.
    num_fingers: u16,
    _: [3]u16 = .{ 0, 0, 0 },

    // Size tests.
    comptime {
        errors.assertStructsEqual(c.SDL_VirtualJoystickTouchpadDesc, VirtualJoystickTouchpadDescription);
    }
};

/// Query the state of joystick event processing.
///
/// ## Return Value
/// Returns true if joystick events are being processed, false otherwise.
///
/// ## Remarks
/// If joystick events are disabled, you must call `joystick.update()` yourself and check the state of the joystick when you want joystick information.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn eventsEnabled() bool {
    return c.SDL_JoystickEventsEnabled();
}

/// Get a list of currently connected joysticks.
///
/// ## Return Value
/// Returns an array of joystick instance IDs.
/// This should be freed with `free()` when it is no longer needed.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn get() ![]Id {
    var count: c_int = undefined;
    const ret = c.SDL_GetJoysticks(&count);
    return @as([*]Id, @ptrCast(try errors.wrapCallNull([*]c.SDL_JoystickID, ret)))[0..@intCast(count)];
}

/// Get the joystick associated with an instance ID, if it has been opened.
///
/// ## Function Parameters
/// * `id`: The instance ID.
///
/// ## Return Value
/// Returns a joystick.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getFromId(
    id: Id,
) !Joystick {
    const ret = c.SDL_GetJoystickFromID(
        id.value,
    );
    return Joystick{ .value = try errors.wrapCallNull(*c.SDL_Joystick, ret) };
}

/// Get the joystick with a player index.
///
/// ## Function Parameters
/// * `index`: The player index.
///
/// ## Return Value
/// Returns a joystick.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getFromIndex(
    index: usize,
) !Joystick {
    const ret = c.SDL_GetJoystickFromPlayerIndex(
        @intCast(index),
    );
    return Joystick{ .value = try errors.wrapCallNull(*c.SDL_Joystick, ret) };
}

/// Get the device information encoded in a GUID structure.
///
/// ## Function Parameters
/// * `guid_val`: The GUID to get information about.
///
/// ## Return Value
/// Returns the device VID, PID, version, and CRC to distinguish between devices with the same VID/PID.
/// Any of these fields may not be available.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getGuidInfo(
    guid_val: guid.Guid,
) struct { vendor: ?u16, product: ?u16, version: ?u16, crc16: ?u16 } {
    var vendor: u16 = undefined;
    var product: u16 = undefined;
    var version: u16 = undefined;
    var crc16: u16 = undefined;
    c.SDL_GetJoystickGUIDInfo(
        guid_val.value,
        &vendor,
        &product,
        &version,
        &crc16,
    );
    return .{
        .vendor = if (vendor == 0) null else vendor,
        .product = if (product == 0) null else product,
        .version = if (version == 0) null else version,
        .crc16 = if (crc16 == 0) null else crc16,
    };
}

/// Return whether a joystick is currently connected.
///
/// ## Return Value
/// Returns true if a joystick is connected, false otherwise.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn has() bool {
    return c.SDL_HasJoystick();
}

/// Locking for atomic access to the joystick API.
///
/// ## Remarks
/// The SDL joystick functions are thread-safe, however you can lock the joysticks while processing to guarantee that the joystick list won't change and joystick
/// and gamepad events will not be delivered.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn lock() void {
    c.SDL_LockJoysticks();
}

/// Set the state of joystick event processing.
///
/// ## Function Parameters
/// * `events_enabled`: Whether to process joystick events or not.
///
/// ## Remarks
/// If joystick events are disabled, you must call `joystick.update()` yourself and check the state of the joystick when you want joystick information.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setEventsEnabled(
    events_enabled: bool,
) void {
    c.SDL_SetJoystickEventsEnabled(
        events_enabled,
    );
}

/// Unlocking for atomic access to the joystick API.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn unlock() void {
    c.SDL_UnlockJoysticks();
}

/// Update the current state of the open joysticks.
///
/// ## Remarks
/// This is called automatically by the event loop if any joystick events are enabled.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn update() void {
    c.SDL_UpdateJoysticks();
}

// Joystick tests.
test "Joystick" {
    errors.refAllDeclsRecursive(@This());
}
