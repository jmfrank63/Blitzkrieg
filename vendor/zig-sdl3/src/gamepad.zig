const c = @import("c");
const errors = @import("errors.zig");
const io_stream = @import("io_stream.zig");
const joystick = @import("joystick.zig");
const power = @import("power.zig");
const properties = @import("properties.zig");
const sensor = @import("sensor.zig");
const sdl3 = @import("sdl3.zig");
const std = @import("std");

/// The list of axes available on a gamepad.
///
/// ## Remarks
/// Thumbstick axis values range from `joystick.axis_min` to `joystick.axis_max`, and are centered within `~8000` of zero,
/// though advanced UI will allow users to set or autodetect the dead zone, which varies between gamepads.
///
/// Trigger axis values range from `0` (released) to `joystick.axis_max` (fully pressed) when reported by `gamepad.Gamepad.getAxis()`.
/// Note that this is not the same range that will be reported by the lower-level `joystick.Joystick.getAxis()`.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Axis = enum(c.SDL_GamepadAxis) {
    left_x = c.SDL_GAMEPAD_AXIS_LEFTX,
    left_y = c.SDL_GAMEPAD_AXIS_LEFTY,
    right_x = c.SDL_GAMEPAD_AXIS_RIGHTX,
    right_y = c.SDL_GAMEPAD_AXIS_RIGHTY,
    left_trigger = c.SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    right_trigger = c.SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,

    /// Convert from SDL.
    pub fn fromSdl(value: c_int) ?Axis {
        if (value == c.SDL_GAMEPAD_AXIS_INVALID)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to SDL.
    pub fn toSdl(self: ?Axis) c_int {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_GAMEPAD_AXIS_INVALID;
    }

    /// Convert a string into an axis enum.
    ///
    /// ## Function Parameters
    /// * `str`: String representing a gamepad axis.
    ///
    /// ## Return Value
    /// Returns the axis enum corresponding to the input string, or `null` if no match was found.
    ///
    /// ## Remarks
    /// This function is called internally to translate gamepad mapping strings for the underlying joystick device into the consistent gamepad mapping.
    /// You do not normally need to call this function unless you are parsing gamepad mappings in your own code.
    ///
    /// Note specially that "righttrigger" and "lefttrigger" map to `gamepad.Axis.right_trigger` and `gamepad.Axis.left_trigger`, respectively.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fromString(
        str: [:0]const u8,
    ) ?Axis {
        return Axis.fromSdl(c.SDL_GetGamepadAxisFromString(str.ptr));
    }

    /// Convert from an axis enum to a string.
    ///
    /// ## Function Parameters
    /// * `self`: An enum value.
    ///
    /// ## Return Value
    /// Returns a string for the given axis, or `null` if an invalid axis is specified.
    /// The string returned is of the format used by gamepad mapping strings.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getString(
        self: Axis,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetGamepadStringForAxis(Axis.toSdl(self));
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }
};

/// A mapping between one joystick input to a gamepad control.
///
/// ## Remarks
/// A gamepad has a collection of several bindings, to say, for example, when joystick button number `5` is pressed, that should be treated like the gamepad's "start" button.
///
/// SDL has these bindings built-in for many popular controllers, and can add more with a simple text string.
/// Those strings are parsed into a collection of these structs to make it easier to operate on the data.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Binding = struct {
    /// Pointer to the original binding.
    /// Write to it with `gamepad.Binding.set()`.
    original: *c.SDL_GamepadBinding,
    /// Joystick information to bind from.
    input: union(BindingType) {
        /// Joystick button index.
        button: usize,
        /// Joystick axis index and min/max.
        axis: packed struct {
            index: usize,
            min: i16,
            max: i16,
        },
        /// Joystick hat index and type.
        hat: packed struct {
            index: usize,
            type: joystick.Hat,
        },
    },
    /// Output gamepad button or axis to use.
    output: union(BindingType) {
        /// Maps to a gamepad button.
        button: Button,
        /// Maps to a gamepad axis.
        axis: packed struct {
            index: Axis,
            min: i16,
            max: i16,
        },
        /// Do not use this!
        hat: Button,
    },

    /// Convert from SDL in place.
    pub fn fromSdl(ptr: *c.SDL_GamepadBinding) Binding {
        return .{
            .original = ptr,
            .input = switch (ptr.input_type) {
                c.SDL_GAMEPAD_BINDTYPE_AXIS => .{ .axis = .{
                    .index = @intCast(ptr.input.axis.axis),
                    .min = @intCast(ptr.input.axis.axis_min),
                    .max = @intCast(ptr.input.axis.axis_max),
                } },
                c.SDL_GAMEPAD_BINDTYPE_HAT => .{ .hat = .{
                    .index = @intCast(ptr.input.hat.hat),
                    .type = @enumFromInt(ptr.input.hat.hat_mask),
                } },
                else => .{
                    .button = @intCast(ptr.input.button),
                },
            },
            .output = switch (ptr.output_type) {
                c.SDL_GAMEPAD_BINDTYPE_AXIS => .{ .axis = .{
                    .index = @enumFromInt(ptr.output.axis.axis),
                    .min = @intCast(ptr.output.axis.axis_min),
                    .max = @intCast(ptr.output.axis.axis_max),
                } },
                else => .{
                    .button = @enumFromInt(ptr.output.button),
                },
            },
        };
    }

    /// Set the original binding using values here.
    ///
    /// ## Function Parameters
    /// * `self`: The binding to set with.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn set(
        self: Binding,
    ) void {
        var ret = c.SDL_GamepadBinding{
            .input_type = BindingType.toSdl(@as(BindingType, self.input)),
            .output_type = BindingType.toSdl(@as(BindingType, self.output)),
        };
        switch (self.input) {
            .button => |val| ret.input.button = @intCast(val),
            .axis => |val| ret.input.axis = .{ .axis = @intCast(val.index), .axis_min = @intCast(val.min), .axis_max = @intCast(val.max) },
            .hat => |val| ret.input.hat = .{ .hat = @intCast(val.index), .hat_mask = @intFromEnum(val.type) },
        }
        switch (self.output) {
            .button => |val| ret.output.button = Button.toSdl(val),
            .axis => |val| ret.output.axis = .{ .axis = Axis.toSdl(val.index), .axis_min = @intCast(val.min), .axis_max = @intCast(val.max) },
            else => {},
        }
        self.original.* = ret;
    }
};

/// Types of gamepad control bindings.
///
/// ## Remarks
/// A gamepad is a collection of bindings that map arbitrary joystick buttons, axes and hat switches to specific positions on a generic console-style gamepad.
/// This enum is used as part of `gamepad.Binding` to specify those mappings.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const BindingType = enum(c.SDL_GamepadBindingType) {
    button = c.SDL_GAMEPAD_BINDTYPE_BUTTON,
    axis = c.SDL_GAMEPAD_BINDTYPE_AXIS,
    hat = c.SDL_GAMEPAD_BINDTYPE_HAT,

    /// Convert from SDL.
    pub fn fromSdl(value: c.SDL_GamepadBindingType) ?BindingType {
        if (value == c.SDL_GAMEPAD_BINDTYPE_NONE)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to SDL.
    pub fn toSdl(self: ?BindingType) c.SDL_GamepadBindingType {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_GAMEPAD_BINDTYPE_NONE;
    }
};

/// The list of buttons available on a gamepad.
///
/// ## Remarks
/// For controllers that use a diamond pattern for the face buttons, the south/east/west/north buttons below correspond to the locations in the diamond pattern.
/// For Xbox controllers, this would be A/B/X/Y, for Nintendo Switch controllers,
/// this would be B/A/Y/X, for GameCube controllers this would be A/X/B/Y, for PlayStation controllers this would be Cross/Circle/Square/Triangle.
///
/// For controllers that don't use a diamond pattern for the face buttons,
/// the south/east/west/north buttons indicate the buttons labeled A, B, C, D, or 1, 2, 3, 4, or for controllers that aren't labeled, they are the primary, secondary, etc. buttons.
///
/// The activate action is often the south button and the cancel action is often the east button,
/// but in some regions this is reversed, so your game should allow remapping actions based on user preferences.
///
/// You can query the labels for the face buttons using `gamepad.Button.getLabel()`.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Button = enum(c.SDL_GamepadButton) {
    /// Bottom face button (e.g. Xbox A button).
    south = c.SDL_GAMEPAD_BUTTON_SOUTH,
    /// Right face button (e.g. Xbox B button).
    east = c.SDL_GAMEPAD_BUTTON_EAST,
    /// Left face button (e.g. Xbox X button).
    west = c.SDL_GAMEPAD_BUTTON_WEST,
    /// Top face button (e.g. Xbox Y button).
    north = c.SDL_GAMEPAD_BUTTON_NORTH,
    back = c.SDL_GAMEPAD_BUTTON_BACK,
    guide = c.SDL_GAMEPAD_BUTTON_GUIDE,
    start = c.SDL_GAMEPAD_BUTTON_START,
    left_stick = c.SDL_GAMEPAD_BUTTON_LEFT_STICK,
    right_stick = c.SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    left_shoulder = c.SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    right_shoulder = c.SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    dpad_up = c.SDL_GAMEPAD_BUTTON_DPAD_UP,
    dpad_down = c.SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    dpad_left = c.SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    dpad_right = c.SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    /// Additional button (e.g. Xbox Series X share button, PS5 microphone button, Nintendo Switch Pro capture button, Amazon Luna microphone button, Google Stadia capture button).
    misc1 = c.SDL_GAMEPAD_BUTTON_MISC1,
    /// Upper or primary paddle, under your right hand (e.g. Xbox Elite paddle P1)
    right_paddle1 = c.SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    /// Upper or primary paddle, under your left hand (e.g. Xbox Elite paddle P3).
    left_paddle1 = c.SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    /// Lower or secondary paddle, under your right hand (e.g. Xbox Elite paddle P2).
    right_paddle2 = c.SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
    /// Lower or secondary paddle, under your left hand (e.g. Xbox Elite paddle P4).
    left_paddle2 = c.SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
    /// PS4/PS5 touchpad button.
    touchpad = c.SDL_GAMEPAD_BUTTON_TOUCHPAD,
    /// Additional button.
    misc2 = c.SDL_GAMEPAD_BUTTON_MISC2,
    /// Additional button.
    misc3 = c.SDL_GAMEPAD_BUTTON_MISC3,
    /// Additional button.
    misc4 = c.SDL_GAMEPAD_BUTTON_MISC4,
    /// Additional button.
    misc5 = c.SDL_GAMEPAD_BUTTON_MISC5,
    /// Additional button.
    misc6 = c.SDL_GAMEPAD_BUTTON_MISC6,

    /// Convert from SDL.
    pub fn fromSdl(value: c.SDL_GamepadButton) ?Button {
        if (value == c.SDL_GAMEPAD_BUTTON_INVALID)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to SDL.
    pub fn toSdl(self: ?Button) c.SDL_GamepadButton {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_GAMEPAD_BUTTON_INVALID;
    }

    /// Convert a string into a button.
    ///
    /// ## Function Parameters
    /// * `str`: String representing a gamepad button.
    ///
    /// ## Return Value
    /// Returns the button corresponding to the input string, or `null` if no match was found.
    ///
    /// ## Remarks
    /// This function is called internally to translate gamepad mapping strings for the underlying joystick device into the consistent gamepad mapping.
    /// You do not normally need to call this function unless you are parsing gamepad mappings in your own code.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fromString(
        str: [:0]const u8,
    ) ?Button {
        return Button.fromSdl(c.SDL_GetGamepadButtonFromString(str.ptr));
    }

    /// Convert from a button enum to a string.
    ///
    /// ## Function Parameters
    /// * `self`: An enum value.
    ///
    /// ## Return Value
    /// Returns a string for the given button, or `null` if an invalid button is specified.
    /// The string returned is of the format used by gamepad mapping strings.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getString(
        self: Button,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetGamepadStringForButton(Button.toSdl(self));
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }
};

/// The set of gamepad button labels.
///
/// ## Remarks
/// This isn't a complete set, just the face buttons to make it easy to show button prompts.
///
/// For a complete set, you should look at the button and gamepad type and have a set of symbols that work well with your art style.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const ButtonLabel = enum(c.SDL_GamepadButtonLabel) {
    a = c.SDL_GAMEPAD_BUTTON_LABEL_A,
    b = c.SDL_GAMEPAD_BUTTON_LABEL_B,
    x = c.SDL_GAMEPAD_BUTTON_LABEL_X,
    y = c.SDL_GAMEPAD_BUTTON_LABEL_Y,
    cross = c.SDL_GAMEPAD_BUTTON_LABEL_CROSS,
    circle = c.SDL_GAMEPAD_BUTTON_LABEL_CIRCLE,
    square = c.SDL_GAMEPAD_BUTTON_LABEL_SQUARE,
    triangle = c.SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE,

    /// Convert from SDL.
    pub fn fromSdl(value: c.SDL_GamepadButtonLabel) ?ButtonLabel {
        if (value == c.SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to SDL.
    pub fn toSdl(self: ?ButtonLabel) c.SDL_GamepadButtonLabel {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
    }
};

/// The structure used to identify an SDL gamepad.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Gamepad = struct {
    value: *c.SDL_Gamepad,

    /// Gamepad properties.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const Properties = struct {
        /// True if this gamepad has an LED that has adjustable brightness.
        mono_led: ?bool,
        /// True if this gamepad has an LED that has adjustable color.
        rgb_led: ?bool,
        /// True if this gamepad has a player LED.
        player_led: ?bool,
        /// True if this gamepad has left/right rumble.
        rumble: ?bool,
        /// True if this gamepad has simple trigger rumble.
        trigger_rumble: ?bool,

        /// Convert from SDL properties.
        pub fn fromProperties(value: properties.Group) Properties {
            return .{
                .mono_led = if (value.get(c.SDL_PROP_GAMEPAD_CAP_MONO_LED_BOOLEAN)) |val| val.boolean else null,
                .rgb_led = if (value.get(c.SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN)) |val| val.boolean else null,
                .player_led = if (value.get(c.SDL_PROP_GAMEPAD_CAP_PLAYER_LED_BOOLEAN)) |val| val.boolean else null,
                .rumble = if (value.get(c.SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN)) |val| val.boolean else null,
                .trigger_rumble = if (value.get(c.SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN)) |val| val.boolean else null,
            };
        }
    };

    /// Check if a gamepad has been opened and is currently connected.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to check.
    ///
    /// ## Return Value
    /// Returns true if the gamepad has been opened and is currently connected, or false if not.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0
    pub fn connected(
        self: Gamepad,
    ) bool {
        return c.SDL_GamepadConnected(self.value);
    }

    /// Close a gamepad previously opened with `Gamepad.init()`.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to close.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Gamepad,
    ) void {
        c.SDL_CloseGamepad(self.value);
    }

    /// Return the `sfSymbolsName` for a given axis on a gamepad on Apple platforms.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to query.
    /// * `axis`: An axis on the gamepad.
    ///
    /// ## Return Value
    /// Returns the `sfSymbolsName` or `null` if the name can't be found.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getAppleSfSymbolsNameForAxis(
        self: Gamepad,
        axis: Axis,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetGamepadAppleSFSymbolsNameForAxis(self.value, Axis.toSdl(axis));
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }

    /// Return the `sfSymbolsName` for a given button on a gamepad on Apple platforms.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to query.
    /// * `button`: A button on the gamepad.
    ///
    /// ## Return Value
    /// Returns the `sfSymbolsName` or `null` if the name can't be found.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getAppleSfSymbolsNameForButton(
        self: Gamepad,
        button: Button,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetGamepadAppleSFSymbolsNameForButton(self.value, Button.toSdl(button));
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }

    /// Get the current state of an axis control on a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    /// * `axis`: An axis index.
    ///
    /// ## Return Value
    /// Returns axis state.
    ///
    /// ## Remarks
    /// For thumbsticks, the state is a value ranging from `-32768` (up/left) to `32767` (down/right).
    ///
    /// Triggers range from `0` when released to `32767` when fully pressed, and never return a negative value.
    /// Note that this differs from the value reported by the lower-level `joystick.Joystick.getAxis()`, which normally uses the full range.
    ///
    /// Note that for invalid gamepads or axes, this will return `0`.
    /// Zero is also a valid value in normal operation; usually it means a centered axis.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getAxis(
        self: Gamepad,
        axis: Axis,
    ) i16 {
        return c.SDL_GetGamepadAxis(self.value, Axis.toSdl(axis));
    }

    /// An iterator for gamepad bindings.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const BindingIterator = struct {
        value: [][*c]c.SDL_GamepadBinding,
        pos: usize,

        /// Deinitialize the iterator.
        ///
        /// ## Function Parameters
        /// * `self`: The binding iterator.
        ///
        /// ## Version
        /// This function is provided by zig-sdl3.
        pub fn deinit(
            self: BindingIterator,
        ) void {
            sdl3.free(self.value);
        }

        /// Return the next binding.
        ///
        /// ## Function Parameters
        /// * `self`: THe binding iterator.
        ///
        /// ## Return Value
        /// Returns the next binding, or `null` if at the end.
        ///
        /// ## Version
        /// This function is provided by zig-sdl3.
        pub fn next(
            self: *BindingIterator,
        ) ?Binding {
            if (self.pos >= self.value.len)
                return null;
            const ret = Binding.fromSdl(self.value[self.pos]);
            self.pos += 1;
            return ret;
        }
    };

    /// Get the SDL joystick layer bindings for a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    ///
    /// ## Return Value
    /// Returns a joystick bindings iterator.
    /// You need to deinitialize the returned value!
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getBindings(
        self: Gamepad,
    ) !BindingIterator {
        var count: c_int = undefined;
        const src = try errors.wrapCallNull([*][*c]c.SDL_GamepadBinding, c.SDL_GetGamepadBindings(self.value, &count));
        return .{ .value = src[0..@intCast(count)], .pos = 0 };
    }

    /// Get the current state of a button on a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    /// * `button`: An button index.
    ///
    /// ## Return Value
    /// Returns true if the button is pressed, false otherwise.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getButton(
        self: Gamepad,
        button: Button,
    ) bool {
        return c.SDL_GetGamepadButton(self.value, Button.toSdl(button));
    }

    /// Get the label of a button on a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    /// * `button`: A button index.
    ///
    /// ## Return Value
    /// Returns the button label.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getButtonLabel(
        self: Gamepad,
        button: Button,
    ) ?ButtonLabel {
        return ButtonLabel.fromSdl(c.SDL_GetGamepadButtonLabel(self.value, Button.toSdl(button)));
    }

    /// Get the connection state of a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the connection state.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getConnectionState(
        self: Gamepad,
    ) !?joystick.ConnectionState {
        return try joystick.ConnectionState.fromSdl(c.SDL_GetGamepadConnectionState(self.value));
    }

    /// Get the firmware version of an opened gamepad, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the firmware version, or `null` if unavailable.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFirmwareVersion(
        self: Gamepad,
    ) ?u16 {
        const ret = c.SDL_GetGamepadFirmwareVersion(self.value);
        if (ret == 0)
            return null;
        return ret;
    }

    /// Get the gamepad associated with a joystick instance ID, if it has been opened.
    ///
    /// ## Function Parameters
    /// * `id`: The joystick instance ID of the gamepad.
    ///
    /// ## Return Value
    /// Returns a gamepad.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFromJoystickId(
        id: joystick.Id,
    ) !Gamepad {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Gamepad, c.SDL_GetGamepadFromID(id.value)) };
    }

    /// Get the gamepad associated with a player index.
    ///
    /// ## Function Parameters
    /// * `index`: The player index, which different from the instance ID.
    ///
    /// ## Return Value
    /// Returns the gamepad associated with a player index.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFromPlayerIndex(
        index: usize,
    ) ?Gamepad {
        const ret = c.SDL_GetGamepadFromPlayerIndex(@intCast(index));
        if (ret) |val|
            return .{ .value = val };
        return null;
    }

    /// Get the underlying joystick from a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object that you want to get a joystick from.
    ///
    /// ## Return Value
    /// Returns the joystick object.
    ///
    /// ## Remarks
    /// This function will give you a joystick object, which allows you to use the joystick functions with a gamepad object.
    /// This would be useful for getting a joystick's position at any given time, even if it hasn't moved (moving it would produce an event, which would have the axis' value).
    ///
    /// The joystick returned is owned by the gamepad.
    /// You should not call `joystick.Joystick.deinit()` on it, for example, since doing so will likely cause SDL to crash.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getJoystick(
        self: Gamepad,
    ) !joystick.Joystick {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Joystick, c.SDL_GetGamepadJoystick(self.value)) };
    }

    /// Get the instance ID of an opened gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad identifier.
    ///
    /// ## Return Value
    /// Returns the instance ID of the specified gamepad.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getJoystickId(
        self: Gamepad,
    ) !joystick.Id {
        return .{ .value = try errors.wrapCall(c.SDL_JoystickID, c.SDL_GetGamepadID(self.value), 0) };
    }

    /// Get the current mapping of a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad you want to get the current mapping for.
    ///
    /// ## Return Value
    /// Returns a string that has the gamepad's mapping.
    /// This should be freed with `free()`.
    ///
    /// ## Remarks
    /// Details about mappings are discussed with `gamepad.Gamepad.addMapping()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMapping(
        self: Gamepad,
    ) ![:0]u8 {
        return errors.wrapCallCStringMut(c.SDL_GetGamepadMapping(self.value));
    }

    /// Get the implementation-dependent name for an opened gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad identifier.
    ///
    /// ## Return Value
    /// Returns the implementation dependent name for the gamepad.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Gamepad,
    ) ![:0]const u8 {
        return errors.wrapCallCString(c.SDL_GetGamepadName(self.value));
    }

    /// Get the number of supported simultaneous fingers on a touchpad on a game gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    /// * `touchpad`: A touchpad.
    ///
    /// ## Return Value
    /// Returns number of supported simultaneous fingers.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNumTouchpadFingers(
        self: Gamepad,
        touchpad: usize,
    ) usize {
        return @intCast(c.SDL_GetNumGamepadTouchpadFingers(self.value, @intCast(touchpad)));
    }

    /// Get the number of touchpads on a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    ///
    /// ## Return Value
    /// Returns number of touchpads.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNumTouchpads(
        self: Gamepad,
    ) usize {
        return @intCast(c.SDL_GetNumGamepadTouchpads(self.value));
    }

    /// Get the implementation-dependent path for an opened gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad identifier.
    ///
    /// ## Return Value
    /// Returns the implementation dependent path for the gamepad.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPath(
        self: Gamepad,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetGamepadPath(self.value);
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }

    /// Get the player index of an opened gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the player index for gamepad, or `null` if it's not available.
    ///
    /// ## Remarks
    /// For XInput gamepads this returns the XInput user index.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getPlayerIndex(
        self: Gamepad,
    ) ?usize {
        const ret = c.SDL_GetGamepadPlayerIndex(self.value);
        if (ret == -1)
            return null;
        return @intCast(ret);
    }

    /// Get the battery state of a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
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
        self: Gamepad,
    ) !struct { power.PowerState, ?u7 } {
        var percent: c_int = undefined;
        const ret = c.SDL_GetGamepadPowerInfo(
            self.value,
            &percent,
        );
        return .{ @enumFromInt(try errors.wrapCall(c_int, ret, c.SDL_POWERSTATE_ERROR)), if (percent == -1) null else @intCast(percent) };
    }

    /// Get the USB product ID of an opened gamepad, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the USB product ID, or `null` if unavailable.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProduct(
        self: Gamepad,
    ) ?u16 {
        const ret = c.SDL_GetGamepadProduct(self.value);
        if (ret == 0)
            return null;
        return @intCast(ret);
    }

    /// Get the product version of an opened gamepad, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the USB product version, or `null` if unavailable.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getProductVersion(
        self: Gamepad,
    ) ?u16 {
        const ret = c.SDL_GetGamepadProductVersion(self.value);
        if (ret == 0)
            return null;
        return @intCast(ret);
    }

    /// Get the properties associated with an opened gamepad.
    pub fn getProperties(
        self: Gamepad,
    ) !Properties {
        const props = c.SDL_GetGamepadProperties(self.value);
        return Properties.fromProperties(.{ .value = try errors.wrapCall(c.SDL_PropertiesID, props, 0) });
    }

    /// Get the type of an opened gamepad, ignoring any mapping override.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the gamepad type, or `null` if it's not available.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getRealType(
        self: Gamepad,
    ) ?Type {
        return Type.fromSdl(c.SDL_GetRealGamepadType(self.value));
    }

    /// Get the current state of a gamepad sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to query.
    /// * `sensor_type`: The type of sensor to query.
    /// * `data`: Slice of data to fill with the current sensor state.
    ///
    /// ## Remarks
    /// The number of values and interpretation of the data is sensor dependent.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSensorData(
        self: Gamepad,
        sensor_type: sensor.Type,
        data: []f32,
    ) !void {
        const ret = c.SDL_GetGamepadSensorData(
            self.value,
            sensor.Type.toSdl(sensor_type),
            data.ptr,
            @intCast(data.len),
        );
        return errors.wrapCallBool(ret);
    }

    /// Get the data rate (number of events per second) of a gamepad sensor.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to query.
    /// * `sensor_type`: The type of sensor to query.
    ///
    /// ## Return Value
    /// Returns the data rate, or `null` if the data rate is not available.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSensorDataRate(
        self: Gamepad,
        sensor_type: sensor.Type,
    ) ?f32 {
        const ret = c.SDL_GetGamepadSensorDataRate(self.value, sensor.Type.toSdl(sensor_type));
        if (ret == 0)
            return null;
        return ret;
    }

    /// Get the serial number of an opened gamepad, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the serial number of the gamepad, or `null` if unavailable.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSerial(
        self: Gamepad,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetGamepadSerial(
            self.value,
        );
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }

    /// Get the Steam Input handle of an opened gamepad, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the gamepad handle, or `null` if unavailable.
    ///
    /// ## Remarks
    /// Returns an `InputHandle_t` for the gamepad that can be used with Steam Input API: https://partner.steamgames.com/doc/api/ISteamInput
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSteamHandle(
        self: Gamepad,
    ) ?u64 {
        const ret = c.SDL_GetGamepadSteamHandle(self.value);
        if (ret == 0)
            return 0;
        return @intCast(ret);
    }

    /// Get the current state of a finger on a touchpad on a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    /// * `touchpad`: A touchpad.
    /// * `finger`: A finger.
    ///
    /// ## Return Value
    /// Returns to if the finger is down, the upper-left origin coordinates normalize from `0` to `1`, and the pressure value.
    pub fn getTouchpadFinger(
        self: Gamepad,
        touchpad: usize,
        finger: usize,
    ) !struct { down: bool, x: f32, y: f32, pressure: f32 } {
        var down: bool = undefined;
        var x: f32 = undefined;
        var y: f32 = undefined;
        var pressure: f32 = undefined;
        try errors.wrapCallBool(c.SDL_GetGamepadTouchpadFinger(self.value, @intCast(touchpad), @intCast(finger), &down, &x, &y, &pressure));
        return .{ .down = down, .x = x, .y = y, .pressure = pressure };
    }

    /// Get the type of an opened gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the gamepad type, or `null` if it's not available.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getType(
        self: Gamepad,
    ) ?Type {
        return Type.fromSdl(c.SDL_GetGamepadType(self.value));
    }

    /// Get the USB vendor ID of an opened gamepad, if available.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to query.
    ///
    /// ## Return Value
    /// Returns the USB vendor ID, or `null` if unavailable.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getVendor(
        self: Gamepad,
    ) ?u16 {
        const ret = c.SDL_GetGamepadVendor(
            self.value,
        );
        if (ret == 0)
            return null;
        return ret;
    }

    /// Query whether a gamepad has a given axis.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    /// * `axis`: An axis value.
    ///
    /// ## Return Value
    /// Returns true if the gamepad has this axis, false otherwise.
    ///
    /// ## Remarks
    /// This merely reports whether the gamepad's mapping defined this axis, as that is all the information SDL has about the physical device.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn hasAxis(
        self: Gamepad,
        axis: Axis,
    ) bool {
        return c.SDL_GamepadHasAxis(self.value, Axis.toSdl(axis));
    }

    /// Query whether a gamepad has a given button.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    /// * `button`: A button value.
    ///
    /// ## Return Value
    /// Returns true if the gamepad has this button, false otherwise.
    ///
    /// ## Remarks
    /// This merely reports whether the gamepad's mapping defined this button, as that is all the information SDL has about the physical device.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn hasButton(
        self: Gamepad,
        button: Button,
    ) bool {
        return c.SDL_GamepadHasButton(self.value, Button.toSdl(button));
    }

    /// Return whether a gamepad has a particular sensor.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    /// * `sensor`: The type of sensor to query.
    ///
    /// ## Return Value
    /// Returns true if the sensor exists, false otherwise.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn hasSensor(
        self: Gamepad,
        sensor_type: sensor.Type,
    ) bool {
        return c.SDL_GamepadHasSensor(self.value, sensor.Type.toSdl(sensor_type));
    }

    /// Open a gamepad for use.
    ///
    /// ## Function Parameters
    /// * `id`: The joystick instance ID.
    ///
    /// ## Return Value
    /// Returns a gamepad identifier.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        id: joystick.Id,
    ) !Gamepad {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Gamepad, c.SDL_OpenGamepad(id.value)) };
    }

    /// Start a rumble effect on a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to vibrate.
    /// * `low_frequency_rumble`: The intensity of the low frequency (left) rumble motor.
    /// * `high_frequency_rumble`: The intensity of the high frequency (right) rumble motor.
    /// * `duration_milliseconds`: The duration of the rumble effect, in milliseconds.
    ///
    /// ## Remarks
    /// Each call to this function cancels any previous trigger rumble effect, and calling it with `0` intensity stops any rumbling.
    ///
    /// This function requires you to process SDL events or call `joystick.update()` to update rumble state.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn rumble(
        self: Gamepad,
        low_frequency_rumble: u16,
        high_frequency_rumble: u16,
        duration_milliseconds: u32,
    ) !void {
        return errors.wrapCallBool(c.SDL_RumbleGamepad(self.value, low_frequency_rumble, high_frequency_rumble, duration_milliseconds));
    }

    /// Start a rumble effect in the gamepad's triggers.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to vibrate.
    /// * `low_frequency_rumble`: The intensity of the low frequency (left) rumble motor.
    /// * `high_frequency_rumble`: The intensity of the high frequency (right) rumble motor.
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
        self: Gamepad,
        low_frequency_rumble: u16,
        high_frequency_rumble: u16,
        duration_milliseconds: u32,
    ) !void {
        return errors.wrapCallBool(c.SDL_RumbleGamepadTriggers(self.value, low_frequency_rumble, high_frequency_rumble, duration_milliseconds));
    }

    /// Send a gamepad specific effect packet.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to affect.
    /// * `data`: The data to send to the gamepad.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn sendEffect(
        self: Gamepad,
        data: []const u8,
    ) !void {
        const ret = c.SDL_SendGamepadEffect(
            self.value,
            data.ptr,
            @intCast(data.len),
        );
        return errors.wrapCallBool(ret);
    }

    /// Query whether sensor data reporting is enabled for a gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: A gamepad.
    /// * `sensor`: The type of sensor to query.
    ///
    /// ## Return Value
    /// Returns true if the sensor is enabled, false otherwise.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn sensorEnabled(
        self: Gamepad,
        sensor_type: sensor.Type,
    ) bool {
        return c.SDL_GamepadSensorEnabled(self.value, sensor.Type.toSdl(sensor_type));
    }

    /// Update a gamepad's LED color.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to update.
    /// * `r`: The intensity of the red LED.
    /// * `g`: The intensity of the green LED.
    /// * `b`: The intensity of the blue LED.
    ///
    /// ## Remarks
    /// An example of a joystick LED is the light on the back of a PlayStation 4's DualShock 4 controller.
    ///
    /// For gamepads with a single color LED, the maximum of the RGB values will be used as the LED brightness.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setLed(
        self: Gamepad,
        r: u8,
        g: u8,
        b: u8,
    ) !void {
        const ret = c.SDL_SetGamepadLED(
            self.value,
            @intCast(r),
            @intCast(g),
            @intCast(b),
        );
        return errors.wrapCallBool(ret);
    }

    /// Set the player index of an opened gamepad.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad object to adjust.
    /// * `index`: Player index to assign to this gamepad, or `null` to clear the player index and turn off player LEDs.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setPlayerIndex(
        self: Gamepad,
        index: ?usize,
    ) !void {
        return errors.wrapCallBool(c.SDL_SetGamepadPlayerIndex(self.value, if (index) |val| @intCast(val) else -1));
    }

    /// Set whether data reporting for a gamepad sensor is enabled.
    ///
    /// ## Function Parameters
    /// * `self`: The gamepad to update.
    /// * `sensor_type`: The type of sensor to enable/disable.
    /// * `enabled`: Whether data reporting should be enabled.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setSensorEnabled(
        self: Gamepad,
        sensor_type: sensor.Type,
        enabled: bool,
    ) !void {
        return errors.wrapCallBool(c.SDL_SetGamepadSensorEnabled(self.value, sensor.Type.toSdl(sensor_type), enabled));
    }
};

/// Standard gamepad types.
///
/// ## Remarks
/// This type does not necessarily map to first-party controllers from Microsoft/Sony/Nintendo; in many cases, third-party controllers can report as these,
/// either because they were designed for a specific console, or they simply most closely match that console's controllers (does it have A/B/X/Y buttons or X/O/Square/Triangle?
/// Does it have a touchpad? etc).
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Type = enum(c.SDL_GamepadType) {
    standard = c.SDL_GAMEPAD_TYPE_STANDARD,
    xbox360 = c.SDL_GAMEPAD_TYPE_XBOX360,
    xbox_one = c.SDL_GAMEPAD_TYPE_XBOXONE,
    ps3 = c.SDL_GAMEPAD_TYPE_PS3,
    ps4 = c.SDL_GAMEPAD_TYPE_PS4,
    ps5 = c.SDL_GAMEPAD_TYPE_PS5,
    switch_pro = c.SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO,
    switch_joycon_left = c.SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT,
    switch_joycon_right = c.SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT,
    switch_joycon_pair = c.SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR,
    // gamecube = c.SDL_GAMEPAD_TYPE_GAMECUBE,

    /// Convert from SDL.
    pub fn fromSdl(value: c.SDL_GamepadType) ?Type {
        if (value == c.SDL_GAMEPAD_TYPE_UNKNOWN)
            return null;
        return @enumFromInt(value);
    }

    /// Convert to SDL.
    pub fn toSdl(self: ?Type) c.SDL_GamepadType {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_GAMEPAD_TYPE_UNKNOWN;
    }

    /// Convert a string into a type enum.
    ///
    /// ## Function Parameters
    /// * `str`: String representing a gamepad type.
    ///
    /// ## Return Value
    /// Returns the type enum corresponding to the input string, or `null` if no match was found.
    ///
    /// ## Remarks
    /// This function is called internally to translate gamepad mapping strings for the underlying joystick device into the consistent gamepad mapping.
    /// You do not normally need to call this function unless you are parsing gamepad mappings in your own code.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn fromString(
        str: [:0]const u8,
    ) ?Type {
        return Type.fromSdl(c.SDL_GetGamepadTypeFromString(str.ptr));
    }

    /// Get the label of a button on a gamepad.
    ///
    /// ## Function Parameters
    /// * `button_type`: The type of gamepad to check.
    /// * `button`: A button index.
    ///
    /// ## Return Value
    /// Returns a button label for the button.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getButtonLabelForType(
        button_type: Type,
        button: Button,
    ) ButtonLabel {
        return @enumFromInt(c.SDL_GetGamepadButtonLabelForType(Type.toSdl(button_type), Button.toSdl(button)));
    }

    /// Convert from a type enum to a string.
    ///
    /// ## Function Parameters
    /// * `self`: An enum value.
    ///
    /// ## Return Value
    /// Returns a string for the given type, or `null` if an invalid button is specified.
    /// The string returned is of the format used by gamepad mapping strings.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getString(
        self: Type,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetGamepadStringForType(Type.toSdl(self));
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }
};

/// Add support for gamepads that SDL is unaware of or change the binding of an existing gamepad.
///
/// ## Function Parameters
/// * `mapping`: The mapping string.
///
/// ## Return Value
/// Returns `true` if a new mapping is added, or `false` if an existing mapping is updated.
///
/// ## Remarks
/// The mapping string has the format "GUID,name,mapping", where GUID is the string value from `GUID.toString()`,
/// name is the human readable string for the device and mappings are gamepad mappings to joystick ones.
/// Under Windows there is a reserved GUID of "xinput" that covers all XInput devices.
///
/// The mapping format for joystick is:
/// bX: A joystick button, index X.
/// hX.Y: Hat X with value Y.
/// aX: Axis X of the joystick.
///
/// Buttons can be used as a gamepad axes and vice versa.
///
/// If a device with this GUID is already plugged in, SDL will generate an `events.Type.gamepad_added` event.
///
/// This string shows an example of a valid mapping for a gamepad:
/// "341a3608000000000000504944564944,Afterglow PS3 Controller,a:b1,b:b2,y:b3,x:b0,start:b9,guide:b12,back:b8,dpup:h0.1,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,leftshoulder:b4,rightshoulder:b5,leftstick:b10,rightstick:b11,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:b6,righttrigger:b7"
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn addMapping(
    mapping: [:0]const u8,
) !bool {
    const ret = try errors.wrapCall(c_int, c.SDL_AddGamepadMapping(mapping.ptr), -1);
    if (ret == 0)
        return false;
    return true;
}

/// Load a set of gamepad mappings from a file.
///
/// ## Function Parameters
/// * `file`: The mappings file to load.
///
/// ## Return Value
/// Returns the number of mappings added.
///
/// ## Remarks
/// You can call this function several times, if needed, to load different database files.
///
/// If a new mapping is loaded for an already known gamepad GUID, the later version will overwrite the one currently loaded.
///
/// Any new mappings for already plugged in controllers will generate an `events.Type.gamepad_added` event.
///
/// Mappings not belonging to the current platform or with no platform field specified will be ignored (i.e. mappings for Linux will be ignored in Windows, etc).
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn addMappingFromFile(
    mapping: [:0]const u8,
) !usize {
    const ret = try errors.wrapCall(c_int, c.SDL_AddGamepadMapping(mapping.ptr), -1);
    return @intCast(ret);
}

/// Load a set of gamepad mappings from an IO stream.
///
/// ## Function Parameters
/// * `src`: The data stream for the mappings to be added.
/// * `close_io`: If true, closes the stream before returning even in case of error.
///
/// ## Return Value
/// Returns the number of mappings added.
///
/// ## Remarks
/// You can call this function several times, if needed, to load different database files.
///
/// If a new mapping is loaded for an already known gamepad GUID, the later version will overwrite the one currently loaded.
///
/// Any new mappings for already plugged in controllers will generate `events.Type.gamepad_added` events.
///
/// Mappings not belonging to the current platform or with no platform field specified will be ignored (i.e. mappings for Linux will be ignored in Windows, etc).
///
/// This function will load the text database entirely in memory before processing it, so take this into consideration if you are in a memory constrained environment.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn addMappingFromIo(
    src: io_stream.Stream,
    close_io: bool,
) !usize {
    const ret = try errors.wrapCall(c_int, c.SDL_AddGamepadMappingsFromIO(src.value, close_io), -1);
    return @intCast(ret);
}

/// Query the state of gamepad event processing.
///
/// ## Return Value
/// Returns Returns true if gamepad events are being processed, false otherwise.
///
/// ## Remarks
/// If gamepad events are disabled, you must call `gamepad.update()` yourself and check the state of the gamepad when you want gamepad information.
///
/// ## Version
/// This function is available since SDL 3.2.0
pub fn eventsEnabled() bool {
    return c.SDL_GamepadEventsEnabled();
}

/// Get a list of currently connected gamepads.
///
/// ## Return Value
/// Returns a slice of joystick instance IDs.
/// This should be freed with `free()` when no longer needed.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getGamepads() ![]joystick.Id {
    var count: c_int = undefined;
    const ret = try errors.wrapCallNull(*c.SDL_JoystickID, c.SDL_GetGamepads(&count));
    return @as([*]joystick.Id, @ptrCast(ret))[0..@intCast(count)];
}

/// Get the implementation-dependent GUID of a gamepad.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns the GUID of the selected gamepad.
/// If called on an invalid index, this function returns `null`.
///
/// ## Remarks
/// This can be called before any gamepads are opened.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getGuidForJoystickId(
    id: joystick.Id,
) ?sdl3.Guid {
    const ret = sdl3.Guid{ .value = c.SDL_GetGamepadGUIDForID(id.value) };
    if (std.mem.allEqual(u8, &ret.value.data, 0))
        return null;
    return ret;
}

/// Get the gamepad mapping string for a given GUID.
///
/// ## Function Parameters
/// * `guid`: A structure containing the GUID for which a mapping is desired.
///
/// ## Return Value
/// Returns a mapping string.
/// This should be freed with `free()`.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getMappingForGuid(
    guid: sdl3.Guid,
) ![:0]u8 {
    return errors.wrapCallCStringMut(c.SDL_GetGamepadMappingForGUID(guid.value));
}

/// Get the mapping of a gamepad.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns a mapping string.
/// This should be freed with `free()`.
///
/// ## Remarks
/// This can be called before any gamepads are opened.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getMappingForJoystickId(
    id: joystick.Id,
) ![:0]u8 {
    return errors.wrapCallCStringMut(c.SDL_GetGamepadMappingForID(id.value));
}

/// Get the current gamepad mappings.
///
/// ## Return Value
/// Returns a slice of the mapping strings.
/// This should be freed with `free()`.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getMappings() ![][*:0]u8 {
    var count: c_int = undefined;
    const ret = try errors.wrapCallNull([*][*c]u8, c.SDL_GetGamepadMappings(&count));
    return @as([*][*:0]u8, @ptrCast(ret))[0..@intCast(count)];
}

/// Get the implementation dependent name of a gamepad.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns the name of the selected gamepad.
///
/// ## Remarks
/// This can be called before any gamepads are opened.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getNameForJoystickId(
    id: joystick.Id,
) ![:0]const u8 {
    return errors.wrapCallCString(c.SDL_GetGamepadNameForID(id.value));
}

/// Get the implementation dependent path of a gamepad.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns the path of the selected gamepad.
///
/// ## Remarks
/// This can be called before any gamepads are opened.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPathForJoystickId(
    id: joystick.Id,
) ![:0]const u8 {
    return errors.wrapCallCString(c.SDL_GetGamepadPathForID(id.value));
}

/// Get the player index of a gamepad.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns the player index for gamepad, or `null` if it's not available.
///
/// ## Remarks
/// This can be called before any gamepads are opened.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getPlayerIndexForJoystickId(
    id: joystick.Id,
) ?usize {
    const ret = c.SDL_GetGamepadPlayerIndexForID(id.value);
    if (ret == -1)
        return null;
    return @intCast(ret);
}

/// Get the USB product ID of a gamepad, if available.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns the USB product ID, or `null` if unavailable or invalid index.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getProductForJoystickId(
    id: joystick.Id,
) ?u16 {
    const ret = c.SDL_GetGamepadProductForID(id.value);
    if (ret == 0)
        return null;
    return @intCast(ret);
}

/// Get the product version of a gamepad, if available.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns the USB version ID, or `null` if unavailable or invalid index.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getProductVersionForJoystickId(
    id: joystick.Id,
) ?u16 {
    const ret = c.SDL_GetGamepadProductVersionForID(id.value);
    if (ret == 0)
        return null;
    return @intCast(ret);
}

///Get the type of a gamepad, ignoring any mapping override.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns the gamepad type.
///
/// ## Remarks
/// This can be called before any gamepads are opened.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getRealTypeForJoystickId(
    id: joystick.Id,
) ?Type {
    return Type.fromSdl(c.SDL_GetGamepadTypeForID(id.value));
}

/// Get the type of a gamepad.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns the gamepad type.
///
/// ## Remarks
/// This can be called before any gamepads are opened.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getTypeForJoystickId(
    id: joystick.Id,
) ?Type {
    return Type.fromSdl(c.SDL_GetGamepadTypeForID(id.value));
}

/// Get the USB vendor ID of an opened gamepad, if available.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns the USB vendor ID, or `null` if unavailable.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getVendorForJoystickId(
    id: joystick.Id,
) ?u16 {
    const ret = c.SDL_GetGamepadVendorForID(id.value);
    if (ret == 0)
        return null;
    return ret;
}

/// Return whether a gamepad is currently connected.
///
/// ## Return Value
/// Returns true if a gamepad is connected, false otherwise.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasGamepad() bool {
    return c.SDL_HasGamepad();
}

/// Check if the given joystick is supported by the gamepad interface.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
///
/// ## Return Value
/// Returns true if the given joystick is supported by the gamepad interface, false if it isn't or it's an invalid index.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn isJoystickGamepad(
    id: joystick.Id,
) bool {
    return c.SDL_IsGamepad(id.value);
}

/// Reinitialize the SDL mapping database to its initial state.
///
/// Remarks
/// This will generate gamepad events as needed if device mappings change.
///
/// Version
/// This function is available since SDL 3.2.0.
pub fn reloadMappings() !void {
    return errors.wrapCallBool(c.SDL_ReloadGamepadMappings());
}

/// Set the state of gamepad event processing.
///
/// ## Function Parameters
/// * `events_enabled`: Whether to process gamepad events or not.
///
/// ## Remarks
/// If gamepad events are disabled, you must call `gamepad.update()` yourself and check the state of the gamepad when you want gamepad information.
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

/// Set the current mapping of a joystick or gamepad.
///
/// ## Function Parameters
/// * `id`: The joystick instance ID.
/// * `mapping`: The mapping to use for this device, or `null` to clear the mapping.
///
/// ## Remarks
/// Details about mappings are discussed with `gamepad.addMapping()`.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setMapping(
    id: joystick.Id,
    mapping: ?[:0]const u8,
) !void {
    return errors.wrapCallBool(c.SDL_SetGamepadMapping(id.value, if (mapping) |val| val.ptr else null));
}

/// Manually pump gamepad updates if not using the loop.
///
/// ## Remarks
/// This function is called automatically by the event loop if events are enabled. Under such circumstances, it will not be necessary to call this function.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn update() void {
    c.SDL_UpdateGamepads();
}

// Gamepad tests.
test "Gamepad" {
    errors.refAllDeclsRecursive(@This());
}
