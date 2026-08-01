const errors = @import("../errors.zig");
const std = @import("std");
const timer = @import("../timer.zig");

/// Capper for keeping the framerate within a given range.
///
/// ## Remarks
/// VSync can be a good limiter for framerates, but in case VSync is not desired having unlimited FPS may be a problem.
/// This framerate capper solves this issue.
///
/// ## Version
/// This struct is provided by zig-sdl3.
pub fn FramerateCapper(
    comptime Accuracy: type,
) type {
    return struct {
        /// Mode for the capper to run at.
        mode: Mode,
        /// Current frame number, maxes out and will not go past the size of a `usize`.
        frame_num: usize = 0,
        /// The number of elapsed nanoseconds present since last update.
        elapsed_ns: u64 = 0,
        /// Nanoseconds mark for the previous frame.
        prev_ns: u64 = 0,
        /// Delta time since the last frame.
        dt: u64 = 0,

        // Type check.
        comptime {
            if (@typeInfo(Accuracy) != .float)
                @compileError("Framerate capper accuracy only works on floating-point types.");
        }

        /// Framerate capper mode.
        ///
        /// ## Version
        /// This struct is provided by zig-sdl3.
        pub const Mode = union(enum) {
            /// Run at unlimited FPS.
            /// You may want to use this in case you have VSync in order to get dt.
            unlimited: void,
            /// Run at the given FPS.
            limited: Accuracy,
        };

        /// Delay to achieve the target FPS.
        ///
        /// ## Function Parameters
        /// * `self`: The FPS limiter.
        ///
        /// ## Return Value
        /// Returns the delta time since the last frame in seconds.
        ///
        /// ## Version
        /// This function is provided since zig-sdl3.
        pub fn delay(
            self: *@This(),
        ) Accuracy {

            // Useful for diagnostics.
            self.frame_num +|= 1; // If this duration is exceeded, overflow or panic probably not ideal?

            const curr_ns = timer.getNanosecondsSinceInit();
            self.dt = @max(curr_ns -% self.prev_ns, 1);
            switch (self.mode) {
                .unlimited => {},
                .limited => |fps| {
                    if (fps != 0) {
                        // Nanoseconds per frame. 1 / (Frames / Seconds) = Seconds / Frame -> Nanoseconds / Frame.
                        const expected_ns = @as(u64, @intFromFloat(timer.nanoseconds_per_second / fps));
                        const ns_diff = curr_ns -% self.elapsed_ns;
                        if (ns_diff < expected_ns) {
                            timer.delayNanoseconds(expected_ns -% ns_diff);
                        }
                    }
                },
            }
            self.prev_ns = curr_ns;
            self.elapsed_ns = timer.getNanosecondsSinceInit();
            return @as(Accuracy, @floatFromInt(self.dt)) / @as(Accuracy, @floatFromInt(timer.nanoseconds_per_second));
        }

        /// Get the actual FPS.
        ///
        /// ## Function Parameters
        /// * `self`: The framerate capper.
        ///
        /// ## Return Value
        /// Returns the FPS as observed (in contrast to the target FPS set).
        ///
        /// ## Version
        /// This function is provided by zig-sdl3.
        pub fn getObservedFps(
            self: @This(),
        ) Accuracy {
            // Frames / Second.
            // 1 Frame / (Nanoseconds / 1000).
            return 1 / (@as(Accuracy, @floatFromInt(@max(self.dt, 1))) / timer.nanoseconds_per_second);
        }
    };
}

test "extras framerate capper" {
    errors.refAllDeclsRecursive(@This());
}
