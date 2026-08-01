const c = @import("c");
const errors = @import("errors.zig");
const joystick = @import("joystick.zig");
const mouse = @import("mouse.zig");
const std = @import("std");

/// Use this value to play an effect on the steering wheel axis.
///
/// ## Remarks
/// This provides better compatibility across platforms and devices as SDL will guess the correct axis.
///
/// ## Version
/// This constant is available since SDL 3.2.0.
pub const steering_axis = c.SDL_HAPTIC_STEERING_AXIS;

/// Structure that represents a haptic direction.
///
/// ## Remarks
/// This is the direction where the force comes from, instead of the direction in which the force is exerted.
///
/// Cardinal directions of the haptic device are relative to the positioning of the device.
/// North is considered to be away from the user.
///
/// The following diagram represents the cardinal directions:
/// ```
///                    .--.
///                    |__| .-------.
///                    |=.| |.-----.|
///                    |--| ||     ||
///                    |  | |'-----'|
///                    |__|~')_____('
///                      [ COMPUTER ]
///
///
///                        North (0,-1)
///                            ^
///                            |
///                            |
///     (-1,0)  West <----[ HAPTIC ]----> East (1,0)
///                            |
///                            |
///                            v
///                        South (0,1)
///
///
///                         [ USER ]
///                           \|||/
///                           (o o)
///                     ---ooO-(_)-Ooo---
/// ```
///
/// If type is `haptic.DirectionType.polar`, direction is encoded by hundredths of a degree starting north and turning clockwise.
/// `haptic.DirectionType.polar` only uses the first dir parameter.
/// The cardinal directions would be:
/// * North: 0 (0 degrees)
/// * East: 9000 (90 degrees)
/// * South: 18000 (180 degrees)
/// * West: 27000 (270 degrees)
///
/// If type is `haptic.DirectionType.cartesian`, direction is encoded by three positions (X axis, Y axis and Z axis (with 3 axes)).
/// `haptic.DirectionType.cartesian` uses the first three dir parameters.
/// The cardinal directions would be:
/// * North: 0,-1, 0
/// * East: 1, 0, 0
/// * South: 0, 1, 0
/// * West: -1, 0, 0
///
/// The Z axis represents the height of the effect if supported, otherwise it's unused.
/// In cartesian encoding (1, 2) would be the same as (2, 4), you can use any multiple you want, only the direction matters.
///
/// If type is `haptic.DirectionType.spherical`, direction is encoded by two rotations.
/// The first two dir parameters are used.
/// The dir parameters are as follows (all values are in hundredths of degrees):
/// * Degrees from (1, 0) rotated towards (0, 1).
/// * Degrees towards (0, 0, 1) (device needs at least 3 axes).
///
/// Example of force coming from the south with all encodings (force coming from the south means the user will have to pull the stick to counteract):
/// ```zig
/// var direction: haptic.Direction = undefined;
///
/// // Cartesian directions.
/// direction.direction_type = .cartesian;
/// direction.dir[0] = 0;
/// direction.dir[1] = 1;
/// // Assuming the device has 2 axes, we don't need to specify third parameter.
///
/// // Polar directions.
/// direction.direction_type = .polar;
/// direction.dir[0] = 18000; // Polar only uses first parameter.
///
/// // Spherical coordinates.
/// direction.direction_type = .spherical;
/// direction.dir[0] = 9000; // Since we only have two axes we don't need more parameters.
/// ```
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Direction = struct {
    /// The type of encoding.
    direction_type: DirectionType,
    /// The encoded direction.
    dir: [3]i32,

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_HapticDirection) Direction {
        return .{
            .direction_type = @enumFromInt(value.type),
            .dir = value.dir,
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: Direction) c.SDL_HapticDirection {
        return .{
            .type = @intFromEnum(self.direction_type),
            .dir = self.dir,
        };
    }
};

/// Type of coordinates used for haptic direction.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const DirectionType = enum(u8) {
    /// Uses polar coordinates for the direction.
    polar = c.SDL_HAPTIC_POLAR,
    /// Uses cartesian coordinates for the direction.
    cartesian = c.SDL_HAPTIC_CARTESIAN,
    /// Uses spherical coordinates for the direction.
    spherical = c.SDL_HAPTIC_SPHERICAL,
};

/// The generic template for any haptic effect.
///
/// ## Remarks
/// All values max at `32767 (0x7FFF)`.
/// Signed values also can be negative.
/// Time values unless specified otherwise are in milliseconds.
///
/// Fade will not be used when the effect is infinite since effect never ends.
///
/// Additionally, the `haptic.Effect.Type.ramp` effect does not support a duration of infinity.
///
/// Button triggers may not be supported on all devices, it is advised to not use them if possible.
/// Buttons start at index 1 instead of index 0 like the joystick.
///
/// If both `attack_length` and `fade_level` are 0, the envelope is not used, otherwise both values are used.
///
/// Here we have an example of a constant effect evolution in time:
///
///```
///  Strength
///  ^
///  |
///  |    effect level -->  _________________
///  |                     /                 \
///  |                    /                   \
///  |                   /                     \
///  |                  /                       \
///  | attack_level --> |                        \
///  |                  |                        |  <---  fade_level
///  |
///  +--------------------------------------------------> Time
///                     [--]                 [---]
///                     attack_length        fade_length
///
///  [------------------][-----------------------]
///  delay               length
/// ```
///
/// Note either the `attack_level` or the `fade_level` may be above the actual effect level.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Effect = union(Effect.Type) {
    constant: Constant,
    periodic: Periodic,
    condition: Condition,
    ramp: Ramp,
    left_right: LeftRight,
    custom: Custom,

    /// Common effect data.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub const CommonData = struct {
        /// Duration of the effect.
        length: Length,
        /// Delay before starting the effect.
        delay: u15,
        /// Button that triggers the effect.
        button: u15,
        /// How soon it can be triggered again after button.
        interval: u15,
    };

    /// A structure containing a template for a Condition effect.
    ///
    /// ## Remarks
    /// The struct handles the following effects:
    /// * `spring`: Effect based on axes position.
    /// * `damper`: Effect based on axes velocity.
    /// * `inertia`: Effect based on axes acceleration.
    /// * `friction`: Effect based on axes movement.
    ///
    /// Direction is handled by condition internals instead of a direction member.
    /// The condition effect specific members have three parameters.
    /// The first refers to the X axis, the second refers to the Y axis and the third refers to the Z axis.
    /// The right terms refer to the positive side of the axis and the left terms refer to the negative side of the axis.
    /// Please refer to the `haptic.Direction` diagram for which side is positive and which is negative.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const Condition = struct {
        /// Effect type.
        effect_type: enum(u16) {
            /// Condition haptic effect that simulates a spring.
            /// Effect is based on the axes position.
            spring = c.SDL_HAPTIC_SPRING,
            /// Condition haptic effect that simulates dampening.
            /// Effect is based on the axes velocity.
            damper = c.SDL_HAPTIC_DAMPER,
            /// Condition haptic effect that simulates inertia.
            /// Effect is based on the axes acceleration.
            inertia = c.SDL_HAPTIC_INERTIA,
            /// Condition haptic effect that simulates friction.
            /// Effect is based on the axes movement.
            friction = c.SDL_HAPTIC_FRICTION,
        },
        /// Common effect data.
        common: CommonData,
        /// Direction of the effect.
        direction: Direction,
        /// Level when joystick is to the positive side.
        right_sat: [3]u16,
        /// Level when joystick is to the negative side.
        left_sat: [3]u16,
        /// How fast to increase the force towards the positive side.
        right_coeff: [3]i16,
        /// How fast to increase the force towards the negative side.
        left_coeff: [3]i16,
        /// Size of the dead zone; whole axis-range when 0-centered.
        deadband: [3]u16,
        /// Position of the dead zone.
        center: [3]i16,

        /// Convert from an SDL value.
        pub fn fromSdl(value: c.SDL_HapticCondition) Condition {
            return .{
                .effect_type = @enumFromInt(value.type),
                .common = .{
                    .length = Length.fromSdl(value.length),
                    .delay = @intCast(value.delay),
                    .button = @intCast(value.button),
                    .interval = @intCast(value.interval),
                },
                .direction = Direction.fromSdl(value.direction),
                .right_sat = value.right_sat,
                .left_sat = value.left_sat,
                .right_coeff = value.right_coeff,
                .left_coeff = value.left_coeff,
                .deadband = value.deadband,
                .center = value.center,
            };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: Condition) c.SDL_HapticCondition {
            return .{
                .type = @intFromEnum(self.effect_type),
                .length = self.common.length.toSdl(),
                .delay = @intCast(self.common.delay),
                .button = @intCast(self.common.button),
                .interval = @intCast(self.common.interval),
                .direction = self.direction.toSdl(),
                .right_sat = self.right_sat,
                .left_sat = self.left_sat,
                .right_coeff = self.right_coeff,
                .left_coeff = self.left_coeff,
                .deadband = self.deadband,
                .center = self.center,
            };
        }
    };

    /// A structure containing a template for a constant effect.
    ///
    /// ## Remarks
    /// This struct is exclusively for the `haptic.Effect.Type.constant` effect.
    ///
    /// A constant effect applies a constant force in the specified direction to the joystick.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const Constant = struct {
        /// Common effect data.
        common: CommonData,
        /// Effect envelope.
        envelope: Envelope,
        /// Direction of the effect.
        direction: Direction,
        /// Strength of the constant effect.
        level: i16,

        /// Convert from an SDL value.
        pub fn fromSdl(value: c.SDL_HapticConstant) Constant {
            return .{
                .common = .{
                    .length = Length.fromSdl(value.length),
                    .delay = @intCast(value.delay),
                    .button = @intCast(value.button),
                    .interval = @intCast(value.interval),
                },
                .envelope = .{
                    .attack_length = @intCast(value.attack_length),
                    .attack_level = @intCast(value.attack_level),
                    .fade_length = @intCast(value.fade_length),
                    .fade_level = @intCast(value.fade_level),
                },
                .direction = Direction.fromSdl(value.direction),
                .level = value.level,
            };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: Constant) c.SDL_HapticConstant {
            return .{
                .type = c.SDL_HAPTIC_CONSTANT,
                .length = self.common.length.toSdl(),
                .delay = @intCast(self.common.delay),
                .button = @intCast(self.common.button),
                .interval = @intCast(self.common.interval),
                .attack_length = @intCast(self.envelope.attack_length),
                .attack_level = @intCast(self.envelope.attack_level),
                .fade_length = @intCast(self.envelope.fade_length),
                .fade_level = @intCast(self.envelope.fade_level),
                .direction = self.direction.toSdl(),
                .level = self.level,
            };
        }
    };

    /// A structure containing a template for a custom effect.
    ///
    /// ## Remarks
    /// This struct is exclusively for the custom effect.
    ///
    /// A custom force feedback effect is much like a periodic effect, where the application can define its exact shape.
    /// You will have to allocate the data yourself.
    /// Data should consist of `channels * samples` u16 samples.
    ///
    /// If channels is one, the effect is rotated using the defined direction.
    /// Otherwise it uses the samples in data for the different axes.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const Custom = struct {
        /// Common effect data.
        common: CommonData,
        /// Effect envelope.
        envelope: Envelope,
        /// Direction of the effect.
        direction: Direction,
        /// Axes to use, minimum of one.
        channels: u8,
        /// Sample periods.
        period: u16,
        /// Amount of samples.
        samples: u16,
        /// Should contain `channels * samples` items.
        data: [*]u16,

        /// Convert from an SDL value.
        pub fn fromSdl(value: c.SDL_HapticCustom) Custom {
            return .{
                .common = .{
                    .length = Length.fromSdl(value.length),
                    .delay = @intCast(value.delay),
                    .button = @intCast(value.button),
                    .interval = @intCast(value.interval),
                },
                .envelope = .{
                    .attack_length = @intCast(value.attack_length),
                    .attack_level = @intCast(value.attack_level),
                    .fade_length = @intCast(value.fade_length),
                    .fade_level = @intCast(value.fade_level),
                },
                .direction = Direction.fromSdl(value.direction),
                .channels = value.channels,
                .period = value.period,
                .samples = value.samples,
                .data = value.data,
            };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: Custom) c.SDL_HapticCustom {
            return .{
                .type = c.SDL_HAPTIC_CUSTOM,
                .length = self.common.length.toSdl(),
                .delay = @intCast(self.common.delay),
                .button = @intCast(self.common.button),
                .interval = @intCast(self.common.interval),
                .attack_length = @intCast(self.envelope.attack_length),
                .attack_level = @intCast(self.envelope.attack_level),
                .fade_length = @intCast(self.envelope.fade_length),
                .fade_level = @intCast(self.envelope.fade_level),
                .direction = self.direction.toSdl(),
                .channels = self.channels,
                .period = self.period,
                .samples = self.samples,
                .data = self.data,
            };
        }
    };

    /// An envelope for an effect.
    ///
    /// ## Version
    /// This struct is provided by zig-sdl3.
    pub const Envelope = struct {
        /// Duration of the attack in milliseconds.
        attack_length: u15,
        /// Level at the start of the attack.
        attack_level: u15,
        /// Duration of the fade in milliseconds.
        fade_length: u15,
        /// Level at the end of the fade.
        fade_level: u15,
    };

    /// ID for haptic effects.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const Id = struct {
        value: c_int,
    };

    /// A structure containing a template for a Left/Right effect.
    ///
    /// ## Remarks
    /// This struct is exclusively for the left right effect.
    ///
    /// The Left/Right effect is used to explicitly control the large and small motors, commonly found in modern game controllers.
    /// The small (right) motor is high frequency, and the large (left) motor is low frequency.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const LeftRight = struct {
        /// Duration of the effect.
        length: Length,
        /// Control of the large controller motor.
        large_magnitude: u16,
        /// Control of the small controller motor.
        small_magnitude: u16,

        /// Convert from an SDL value.
        pub fn fromSdl(value: c.SDL_HapticLeftRight) LeftRight {
            return .{
                .length = Length.fromSdl(value.length),
                .large_magnitude = value.large_magnitude,
                .small_magnitude = value.small_magnitude,
            };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: LeftRight) c.SDL_HapticLeftRight {
            return .{
                .type = c.SDL_HAPTIC_LEFTRIGHT,
                .length = self.length.toSdl(),
                .large_magnitude = self.large_magnitude,
                .small_magnitude = self.small_magnitude,
            };
        }
    };

    /// Length of an effect.
    ///
    /// ## Version
    /// This union is provided by zig-sdl3.
    pub const Length = union(enum) {
        // Number of iterations.
        iterations: u15,
        /// Go on for infinity.
        infinite: void,

        /// Convert from an SDL value.
        pub fn fromSdl(value: u32) Length {
            if (value == c.SDL_HAPTIC_INFINITY)
                return .{ .infinite = {} };
            return .{ .iterations = @intCast(value) };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: Length) u32 {
            return switch (self) {
                .iterations => |val| @intCast(val),
                .infinite => c.SDL_HAPTIC_INFINITY,
            };
        }
    };

    /// A structure containing a template for a Periodic effect.
    ///
    /// ## Remarks
    /// The struct handles the following effects:
    /// * Sine.
    /// * Square.
    /// * Triangle.
    /// * Sawtooth Up.
    /// * Sawtooth Down.
    ///
    /// A periodic effect consists in a wave-shaped effect that repeats itself over time.
    /// The type determines the shape of the wave and the parameters determine the dimensions of the wave.
    ///
    /// Phase is given by hundredth of a degree meaning that giving the phase a value of `9000` will displace it `25%` of its period.
    /// Here are sample values:
    /// * `0`: No phase displacement.
    /// * `9000`: Displaced 25% of its period.
    /// * `18000`: Displaced 50% of its period.
    /// * `27000`: Displaced 75% of its period.
    /// * `36000`: Displaced 100% of its period, same as 0, but 0 is preferred.
    ///
    /// Examples:
    ///
    /// ```
    ///   Sine
    ///     __      __      __      __
    ///    /  \    /  \    /  \    /
    ///   /    \__/    \__/    \__/
    ///
    ///   Square
    ///    __    __    __    __    __
    ///   |  |  |  |  |  |  |  |  |  |
    ///   |  |__|  |__|  |__|  |__|  |
    ///
    ///   Triangle
    ///     /\    /\    /\    /\    /\
    ///    /  \  /  \  /  \  /  \  /
    ///   /    \/    \/    \/    \/
    ///
    ///   Sawtooth Up
    ///     /|  /|  /|  /|  /|  /|  /|
    ///    / | / | / | / | / | / | / |
    ///   /  |/  |/  |/  |/  |/  |/  |
    ///
    ///   Sawtooth Down
    ///   \  |\  |\  |\  |\  |\  |\  |
    ///    \ | \ | \ | \ | \ | \ | \ |
    ///     \|  \|  \|  \|  \|  \|  \|
    /// ```
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const Periodic = struct {
        /// Effect type.
        effect_type: enum(u16) {
            /// Periodic haptic effect that simulates sine waves.
            sine = c.SDL_HAPTIC_SINE,
            /// Periodic haptic effect that simulates square waves.
            square = c.SDL_HAPTIC_SQUARE,
            /// Periodic haptic effect that simulates triangular waves.
            triangle = c.SDL_HAPTIC_TRIANGLE,
            /// Periodic haptic effect that simulates saw tooth up waves.
            sawtooth_up = c.SDL_HAPTIC_SAWTOOTHUP,
            /// Periodic haptic effect that simulates saw tooth down waves.
            sawtooth_down = c.SDL_HAPTIC_SAWTOOTHDOWN,
        },
        /// Common effect data.
        common: CommonData,
        /// Effect envelope.
        envelope: Envelope,
        /// Direction of the effect.
        direction: Direction,
        /// Period of the wave.
        period: u16,
        /// Peak value; if negative, equivalent to 180 degrees extra phase shift.
        magnitude: i16,
        /// Mean value of the wave.
        offset: i16,
        /// Positive phase shift given by hundredth of a degree.
        phase: u16,

        /// Convert from an SDL value.
        pub fn fromSdl(value: c.SDL_HapticPeriodic) Periodic {
            return .{
                .effect_type = @enumFromInt(value.type),
                .common = .{
                    .length = Length.fromSdl(value.length),
                    .delay = @intCast(value.delay),
                    .button = @intCast(value.button),
                    .interval = @intCast(value.interval),
                },
                .envelope = .{
                    .attack_length = @intCast(value.attack_length),
                    .attack_level = @intCast(value.attack_level),
                    .fade_length = @intCast(value.fade_length),
                    .fade_level = @intCast(value.fade_level),
                },
                .direction = Direction.fromSdl(value.direction),
                .period = value.period,
                .magnitude = value.magnitude,
                .offset = value.offset,
                .phase = value.phase,
            };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: Periodic) c.SDL_HapticPeriodic {
            return .{
                .type = @intFromEnum(self.effect_type),
                .length = self.common.length.toSdl(),
                .delay = @intCast(self.common.delay),
                .button = @intCast(self.common.button),
                .interval = @intCast(self.common.interval),
                .attack_length = @intCast(self.envelope.attack_length),
                .attack_level = @intCast(self.envelope.attack_level),
                .fade_length = @intCast(self.envelope.fade_length),
                .fade_level = @intCast(self.envelope.fade_level),
                .direction = self.direction.toSdl(),
                .period = self.period,
                .magnitude = self.magnitude,
                .offset = self.offset,
                .phase = self.phase,
            };
        }
    };

    /// A structure containing a template for a constant effect.
    ///
    /// ## Remarks
    /// This struct is exclusively for the ramp effect.
    ///
    /// The ramp effect starts at start strength and ends at end strength.
    /// It augments in linear fashion.
    /// If you use attack and fade with a ramp the effects get added to the ramp effect making the effect become quadratic instead of linear.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const Ramp = struct {
        /// Common effect data.
        common: CommonData,
        /// Effect envelope.
        envelope: Envelope,
        /// Direction of the effect.
        direction: Direction,
        /// Beginning strength level.
        start: i16,
        /// Ending strength level.
        end: i16,

        /// Convert from an SDL value.
        pub fn fromSdl(value: c.SDL_HapticRamp) Ramp {
            return .{
                .common = .{
                    .length = Length.fromSdl(value.length),
                    .delay = @intCast(value.delay),
                    .button = @intCast(value.button),
                    .interval = @intCast(value.interval),
                },
                .envelope = .{
                    .attack_length = @intCast(value.attack_length),
                    .attack_level = @intCast(value.attack_level),
                    .fade_length = @intCast(value.fade_length),
                    .fade_level = @intCast(value.fade_level),
                },
                .direction = Direction.fromSdl(value.direction),
                .start = value.start,
                .end = value.end,
            };
        }

        /// Convert to an SDL value.
        pub fn toSdl(self: Ramp) c.SDL_HapticRamp {
            return .{
                .type = c.SDL_HAPTIC_RAMP,
                .length = self.common.length.toSdl(),
                .delay = @intCast(self.common.delay),
                .button = @intCast(self.common.button),
                .interval = @intCast(self.common.interval),
                .attack_length = @intCast(self.envelope.attack_length),
                .attack_level = @intCast(self.envelope.attack_level),
                .fade_length = @intCast(self.envelope.fade_length),
                .fade_level = @intCast(self.envelope.fade_level),
                .direction = self.direction.toSdl(),
                .start = self.start,
                .end = self.end,
            };
        }
    };

    /// Type of haptic effect.
    ///
    /// ## Version
    /// This struct is available since SDL 3.2.0.
    pub const Type = enum(u32) {
        /// Constant haptic effect.
        constant = c.SDL_HAPTIC_CONSTANT,
        /// Periodic haptic effect.
        periodic,
        /// Condition haptic effect.
        condition,
        /// Ramp haptic effect.
        ramp = c.SDL_HAPTIC_RAMP,
        /// Haptic effect for direct control over high/low frequency motors.
        left_right = c.SDL_HAPTIC_LEFTRIGHT,
        /// User defined custom haptic effect.
        custom = c.SDL_HAPTIC_CUSTOM,
    };

    /// Convert from an SDL value.
    pub fn fromSdl(value: c.SDL_HapticEffect) ?Effect {
        return switch (value.type) {
            c.SDL_HAPTIC_CONSTANT => .{ .constant = Constant.fromSdl(value.constant) },
            c.SDL_HAPTIC_SINE, c.SDL_HAPTIC_SQUARE, c.SDL_HAPTIC_TRIANGLE, c.SDL_HAPTIC_SAWTOOTHUP, c.SDL_HAPTIC_SAWTOOTHDOWN => .{ .periodic = Periodic.fromSdl(value.periodic) },
            c.SDL_HAPTIC_SPRING, c.SDL_HAPTIC_DAMPER, c.SDL_HAPTIC_INERTIA, c.SDL_HAPTIC_FRICTION => .{ .condition = Condition.fromSdl(value.condition) },
            c.SDL_HAPTIC_RAMP => .{ .ramp = Ramp.fromSdl(value.ramp) },
            c.SDL_HAPTIC_LEFTRIGHT => .{ .left_right = LeftRight.fromSdl(value.leftright) },
            c.SDL_HAPTIC_CUSTOM => .{ .custom = Custom.fromSdl(value.custom) },
            else => null,
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: Effect) c.SDL_HapticEffect {
        return switch (self) {
            .constant => |val| .{ .constant = val.toSdl() },
            .periodic => |val| .{ .periodic = val.toSdl() },
            .condition => |val| .{ .condition = val.toSdl() },
            .ramp => |val| .{ .ramp = val.toSdl() },
            .left_right => |val| .{ .leftright = val.toSdl() },
            .custom => |val| .{ .custom = val.toSdl() },
        };
    }
};

/// Haptic features supported.
///
/// ## Version
/// This structure is provided by zig-sdl3.
pub const Features = struct {
    /// Constant effect supported.
    constant: bool = false,
    /// Sine wave effect supported.
    sine: bool = false,
    /// Square wave effect supported.
    square: bool = false,
    /// riangle wave effect supported.
    triangle: bool = false,
    /// Sawtoothup wave effect supported.
    sawtooth_up: bool = false,
    /// Sawtoothdown wave effect supported.
    sawtooth_down: bool = false,
    /// Ramp effect supported.
    ramp: bool = false,
    /// Spring effect supported - uses axes position.
    spring: bool = false,
    /// Damper effect supported - uses axes velocity.
    damper: bool = false,
    /// Inertia effect supported - uses axes acceleration.
    inertia: bool = false,
    /// Friction effect supported - uses axes movement.
    friction: bool = false,
    /// Left/Right effect supported.
    left_right: bool = false,
    /// Reserved for future use.
    reserved1: bool = false,
    /// Reserved for future use.
    reserved2: bool = false,
    /// Reserved for future use.
    reserved3: bool = false,
    /// Custom effect is supported.
    custom: bool = false,
    /// Device supports setting the global gain.
    gain: bool = false,
    /// Device supports setting autocenter.
    autocenter: bool = false,
    /// Device supports querying effect status.
    status: bool = false,
    /// Devices supports being paused.
    pause: bool = false,

    /// Convert from an SDL value.
    pub fn fromSdl(value: u32) Features {
        return .{
            .constant = value & c.SDL_HAPTIC_CONSTANT != 0,
            .sine = value & c.SDL_HAPTIC_SINE != 0,
            .square = value & c.SDL_HAPTIC_SQUARE != 0,
            .triangle = value & c.SDL_HAPTIC_TRIANGLE != 0,
            .sawtooth_up = value & c.SDL_HAPTIC_SAWTOOTHUP != 0,
            .sawtooth_down = value & c.SDL_HAPTIC_SAWTOOTHDOWN != 0,
            .ramp = value & c.SDL_HAPTIC_RAMP != 0,
            .spring = value & c.SDL_HAPTIC_SPRING != 0,
            .damper = value & c.SDL_HAPTIC_DAMPER != 0,
            .inertia = value & c.SDL_HAPTIC_INERTIA != 0,
            .friction = value & c.SDL_HAPTIC_FRICTION != 0,
            .left_right = value & c.SDL_HAPTIC_LEFTRIGHT != 0,
            .reserved1 = value & c.SDL_HAPTIC_RESERVED1 != 0,
            .reserved2 = value & c.SDL_HAPTIC_RESERVED2 != 0,
            .reserved3 = value & c.SDL_HAPTIC_RESERVED3 != 0,
            .custom = value & c.SDL_HAPTIC_CUSTOM != 0,
            .gain = value & c.SDL_HAPTIC_GAIN != 0,
            .autocenter = value & c.SDL_HAPTIC_AUTOCENTER != 0,
            .status = value & c.SDL_HAPTIC_STATUS != 0,
            .pause = value & c.SDL_HAPTIC_PAUSE != 0,
        };
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: Features) u32 {
        var ret: u32 = 0;
        if (self.constant)
            ret |= c.SDL_HAPTIC_CONSTANT;
        if (self.sine)
            ret |= c.SDL_HAPTIC_SINE;
        if (self.square)
            ret |= c.SDL_HAPTIC_SQUARE;
        if (self.triangle)
            ret |= c.SDL_HAPTIC_TRIANGLE;
        if (self.sawtooth_up)
            ret |= c.SDL_HAPTIC_SAWTOOTHUP;
        if (self.sawtooth_down)
            ret |= c.SDL_HAPTIC_SAWTOOTHDOWN;
        if (self.ramp)
            ret |= c.SDL_HAPTIC_RAMP;
        if (self.spring)
            ret |= c.SDL_HAPTIC_SPRING;
        if (self.damper)
            ret |= c.SDL_HAPTIC_DAMPER;
        if (self.inertia)
            ret |= c.SDL_HAPTIC_INERTIA;
        if (self.friction)
            ret |= c.SDL_HAPTIC_FRICTION;
        if (self.left_right)
            ret |= c.SDL_HAPTIC_LEFTRIGHT;
        if (self.reserved1)
            ret |= c.SDL_HAPTIC_RESERVED1;
        if (self.reserved2)
            ret |= c.SDL_HAPTIC_RESERVED2;
        if (self.reserved3)
            ret |= c.SDL_HAPTIC_RESERVED3;
        if (self.custom)
            ret |= c.SDL_HAPTIC_CUSTOM;
        if (self.gain)
            ret |= c.SDL_HAPTIC_GAIN;
        if (self.autocenter)
            ret |= c.SDL_HAPTIC_AUTOCENTER;
        if (self.status)
            ret |= c.SDL_HAPTIC_STATUS;
        if (self.pause)
            ret |= c.SDL_HAPTIC_PAUSE;
        return ret;
    }
};

/// The haptic structure used to identify an SDL haptic.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Haptic = struct {
    value: *c.SDL_Haptic,

    /// Create a new haptic effect on a specified device.
    ///
    /// ## Function Parameters
    /// * `self`: Device to create the effect on.
    /// * `effect`: Effect structure containing the properties of the effect to create.
    ///
    /// ## Return Value
    /// Returns the ID of the effect.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn createEffect(
        self: Haptic,
        effect: Effect,
    ) !Effect.Id {
        const effect_sdl = effect.toSdl();
        const ret = c.SDL_CreateHapticEffect(self.value, &effect_sdl);
        return .{ .value = try errors.wrapCall(c_int, ret, -1) };
    }

    /// Close a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to close.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Haptic,
    ) void {
        c.SDL_CloseHaptic(self.value);
    }

    /// Destroy a haptic effect on the device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to destroy the effect on.
    /// * `effect`: The ID of the haptic effect to destroy.
    ///
    /// ## Remarks
    /// This will stop the effect if it's running.
    /// Effects are automatically destroyed when the device is closed.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn destroyEffect(
        self: Haptic,
        effect: Effect.Id,
    ) void {
        c.SDL_DestroyHapticEffect(self.value, effect.value);
    }

    /// Check to see if an effect is supported by a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to query.
    /// * `effect`: The desired effect to query.
    ///
    /// ## Return Value
    /// Returns true if the effect is supported or false if it isn't.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn effectSupported(
        self: Haptic,
        effect: Effect,
    ) bool {
        const effect_sdl = effect.toSdl();
        return c.SDL_HapticEffectSupported(self.value, &effect_sdl);
    }

    /// Get the status of the current effect on the specified haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to query for the effect status on.
    /// * `effect`: The ID of the haptic effect to query its status.
    ///
    /// ## Return Value
    /// Returns true if it is playing, false if it isn't playing or haptic status isn't supported.
    ///
    /// ## Remarks
    /// Device must support the `haptic.Features.status` feature.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getEffectStatus(
        self: Haptic,
        effect: Effect.Id,
    ) bool {
        return c.SDL_GetHapticEffectStatus(self.value, effect.value);
    }

    /// Get the haptic device's supported features in bitwise manner.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to query for the effect status on.
    ///
    /// ## Return Value
    /// Returns supported haptic features.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getFeatures(
        self: Haptic,
    ) !Features {
        return Features.fromSdl(try errors.wrapCall(u32, c.SDL_GetHapticFeatures(self.value), 0));
    }

    /// Get the instance ID of an opened haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to query.
    ///
    /// ## Return Value
    /// Returns the instance ID of the specified haptic device.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getId(
        self: Haptic,
    ) !Id {
        return .{ .value = try errors.wrapCall(c.SDL_HapticID, c.SDL_GetHapticID(self.value), 0) };
    }

    /// Get the number of effects a haptic device can store.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to query.
    ///
    /// ## Return Value
    /// Returns the number of effects the haptic device can store.
    ///
    /// ## Remarks
    /// On some platforms this isn't fully supported, and therefore is an approximation.
    /// Always check to see if your created effect was actually created and do not rely solely on this.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMaxEffects(
        self: Haptic,
    ) !usize {
        const ret = c.SDL_GetMaxHapticEffects(self.value);
        if (ret < 0) {
            errors.callErrorCallback();
            return error.SdlError;
        }
        return @intCast(ret);
    }

    /// Get the number of effects a haptic device can play at the same time.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to query maximum playing effects.
    ///
    /// ## Return Value
    /// Returns the number of effects the haptic device can play at the same time.
    ///
    /// ## Remarks
    /// This is not supported on all platforms, but will always return a value.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMaxEffectsPlaying(
        self: Haptic,
    ) !usize {
        const ret = c.SDL_GetMaxHapticEffectsPlaying(self.value);
        return @intCast(try errors.wrapCall(c_int, ret, -1));
    }

    /// Get the implementation dependent name of a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic obtained from `joystick.Joystick.init()`.
    ///
    /// ## Return Value
    /// Returns the name of the selected haptic device.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Haptic,
    ) ![:0]const u8 {
        return errors.wrapCallCString(c.SDL_GetHapticName(self.value));
    }

    /// Get the number of haptic axes the device has.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to query.
    ///
    /// ## Return Value
    /// Returns the number of axes.
    ///
    /// ## Remarks
    /// The number of haptic axes might be useful if working with the `haptic.DirectionEffect` effect.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getNumAxes(
        self: Haptic,
    ) !usize {
        const ret = c.SDL_GetNumHapticAxes(self.value);
        return @intCast(try errors.wrapCall(c_int, ret, -1));
    }

    /// Open a haptic device for use.
    ///
    /// ## Function Parameters
    /// * `instance_id`: The haptic device instance ID.
    ///
    /// ## Return Value
    /// Returns the device identifier.
    ///
    /// ## Remarks
    /// The index passed as an argument refers to the N'th haptic device on this system.
    ///
    /// When opening a haptic device, its gain will be set to maximum and autocenter will be disabled.
    /// To modify these values use `haptic.Haptic.setGain()` and `haptic.Haptic.setAutocenter()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn init(
        instance_id: Id,
    ) !Haptic {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Haptic, c.SDL_OpenHaptic(instance_id.value)) };
    }

    /// Open a haptic device for use from a joystick device.
    ///
    /// ## Function Parameters
    /// * `val`: The joystick to create a haptic device from.
    ///
    /// ## Return Value
    /// Returns a valid haptic device identifier.
    ///
    /// ## Remarks
    /// You must still call `haptic.Haptic.deinit()` on the returned value separately.
    /// It will not be closed with the joystick.
    ///
    /// When opened from a joystick you should first deinit the haptic device before deinit'ing the joystick device.
    /// If not, on some implementations the haptic device will also get unallocated and you'll be unable to use force feedback on that device.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initFromJoystick(
        val: joystick.Joystick,
    ) !Haptic {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Haptic, c.SDL_OpenHapticFromJoystick(val.value)) };
    }

    /// Try to open a haptic device from the current mouse.
    ///
    /// ## Return Value
    /// Returns the valid haptic device identifier.
    /// The return value should not be deinit'd.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initFromMouse() !Haptic {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Haptic, c.SDL_OpenHapticFromMouse()) };
    }

    /// Initialize a haptic device for simple rumble playback.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to initialize for simple rumble playback.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn initRumble(
        self: Haptic,
    ) !void {
        return errors.wrapCallBool(c.SDL_InitHapticRumble(self.value));
    }

    /// Pause a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to pause.
    ///
    /// ## Remarks
    /// Device must support the `haptic.Features.pause` feature.
    /// Call `haptic.Haptic.resumePlayback()` to resume playback.
    ///
    /// Do not modify the effects nor add new ones while the device is paused.
    /// That can cause all sorts of weird errors.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn pause(
        self: Haptic,
    ) !void {
        return errors.wrapCallBool(c.SDL_PauseHaptic(self.value));
    }

    /// Run a simple rumble effect on a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to play the rumble effect on.
    /// * `strength`: Strength of the rumble to play as a `0-1` float value.
    /// * `length_milliseconds`: Length of the rumble to play in milliseconds.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn playRumble(
        self: Haptic,
        strength: f32,
        length_milliseconds: u32,
    ) !void {
        return errors.wrapCallBool(c.SDL_PlayHapticRumble(self.value, strength, length_milliseconds));
    }

    /// Resume a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to unpause.
    ///
    /// ## Remarks
    /// Call to unpause after `haptic.Haptic.pause()`.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn resumePlayback(
        self: Haptic,
    ) !void {
        return errors.wrapCallBool(c.SDL_ResumeHaptic(self.value));
    }

    /// Check whether rumble is supported on a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: Haptic device to check for rumble support.
    ///
    /// ## Return Value
    /// Returns true if the effect is supported or false if it isn't.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn rumbleSupported(
        self: Haptic,
    ) bool {
        return c.SDL_HapticRumbleSupported(self.value);
    }

    /// Run the haptic effect on its associated haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to run the effect on.
    /// * `effect`: The ID of the haptic effect to run.
    /// * `iterations`: The number of iterations to run the effect.
    ///
    /// ## Remarks
    /// To repeat the effect over and over indefinitely, set iterations to `infinity` (Repeats the envelope - attack and fade).
    /// To make one instance of the effect last indefinitely (so the effect does not fade), set the effect's length in its structure/union to `infinity` instead.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn runEffect(
        self: Haptic,
        effect: Effect.Id,
        iterations: Effect.Length,
    ) !void {
        return errors.wrapCallBool(c.SDL_RunHapticEffect(self.value, effect.value, iterations.toSdl()));
    }

    /// Set the global autocenter of the device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to set autocentering on.
    /// * `gain`: Value to set autocenter to (0-100).
    ///
    /// ## Remarks
    /// Autocenter should be between 0 and 100. Setting it to 0 will disable autocentering.
    ///
    /// Device must support the `haptic.Features.autocenter` feature.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setAutocenter(
        self: Haptic,
        gain: u7,
    ) !void {
        return errors.wrapCallBool(c.SDL_SetHapticAutocenter(self.value, @intCast(gain)));
    }

    /// Set the global gain of the specified haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to set the gain on.
    /// * `gain`: Value to set the gain to, should be between `0` and `100` `(0 - 100)`.
    ///
    /// ## Remarks
    /// Device must support the `haptic.Features.gain` feature.
    ///
    /// The user may specify the maximum gain by setting the environment variable `SDL_HAPTIC_GAIN_MAX` which should be between 0 and 100.
    /// All calls to this will scale linearly using `SDL_HAPTIC_GAIN_MAX` as the maximum.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setGame(
        self: Haptic,
        gain: u7,
    ) !void {
        return errors.wrapCallBool(c.SDL_SetHapticGain(self.value, @intCast(gain)));
    }

    /// Stop the haptic effect on its associated haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to stop the effect on.
    /// * `id`: The ID of the haptic effect to stop.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn stopEffect(
        self: Haptic,
        id: Effect.Id,
    ) !void {
        return errors.wrapCallBool(c.SDL_StopHapticEffect(self.value, id.value));
    }

    /// Stop all the currently playing effects on a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to stop.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn stopEffects(
        self: Haptic,
    ) !void {
        return errors.wrapCallBool(c.SDL_StopHapticEffects(self.value));
    }

    /// Stop the simple rumble on a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device to stop the rumble effect on.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn stopRumble(
        self: Haptic,
    ) !void {
        return errors.wrapCallBool(c.SDL_StopHapticRumble(self.value));
    }

    /// Update the properties of an effect.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device that has the effect.
    /// * `effect`: The identifier of the effect to update.
    /// * `data`: The effect data structure containing the new effect properties to use.
    ///
    /// ## Remarks
    /// Can be used dynamically, although behavior when dynamically changing direction may be strange.
    /// Specifically the effect may re-upload itself and start playing from the start.
    /// You also cannot change the type either when running this function.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn update(
        self: Haptic,
        effect: Effect.Id,
        data: Effect,
    ) !void {
        const data_sdl = data.toSdl();
        return errors.wrapCallBool(c.SDL_UpdateHapticEffect(self.value, effect.value, &data_sdl));
    }
};

/// This is a unique ID for a haptic device for the time it is connected to the system, and is never reused for the lifetime of the application.
///
/// ## Remarks
/// If the haptic device is disconnected and reconnected, it will get a new ID.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Id = packed struct {
    value: c.SDL_HapticID,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_HapticID) == @sizeOf(Id));
    }

    /// Get the haptic associated with an instance ID, if it has been opened.
    ///
    /// ## Function Parameters
    /// * `self`: The instance ID to get the haptic for.
    ///
    /// ## Return Value
    /// Returns the haptic.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getHaptic(
        self: Id,
    ) !Haptic {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Haptic, c.SDL_GetHapticFromID(self.value)) };
    }

    /// Get the implementation dependent name of a haptic device.
    ///
    /// ## Function Parameters
    /// * `self`: The haptic device instance ID.
    ///
    /// ## Return Value
    /// Returns the name of the selected haptic device.
    ///
    /// ## Remarks
    /// This can be called before any haptic devices are opened.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Id,
    ) ![:0]const u8 {
        return errors.wrapCallCString(c.SDL_GetHapticNameForID(self.value));
    }
};

/// Get a list of currently connected haptic devices.
///
/// ## Return Value
/// Returns a slice of haptic device instance IDs.
/// This should be freed with `free()` when it is no longer needed.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getHaptics() ![]Id {
    var count: c_int = undefined;
    const ret = @as([*]Id, @ptrCast(try errors.wrapCallNull(*c.SDL_HapticID, c.SDL_GetHaptics(&count))));
    return ret[0..@intCast(count)];
}

/// Query if a joystick has haptic features.
///
/// ## Function Parameters
/// * `value`: The joystick to test for haptic capabilities.
///
/// ## Return Value
/// Returns true if the joystick is haptic or false if it isn't.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn isJoystickHaptic(
    value: joystick.Joystick,
) bool {
    return c.SDL_IsJoystickHaptic(value.value);
}

/// Query whether or not the current mouse has haptic capabilities.
///
/// ## Return Value
/// Returns true if the mouse is haptic or false if it isn't.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn isMouseHaptic() bool {
    return c.SDL_IsMouseHaptic();
}

// Haptic tests.
test "Haptic" {
    errors.refAllDeclsRecursive(@This());
}
