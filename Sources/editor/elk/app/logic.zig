//! The ELK app's pure parts, no SDL, ImGui or window: the session model and
//! the settings file. The root of the `test-elk-app` tier.
pub const session = @import("session.zig");
pub const settings = @import("settings.zig");

test {
    @import("std").testing.refAllDecls(@This());
}
