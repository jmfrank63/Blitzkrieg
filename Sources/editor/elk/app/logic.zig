//! The ELK app's pure parts, no SDL, ImGui or window: the session model,
//! the settings file, the spell check over the vendored Hunspell and the
//! long operations' worker. The root of the `test-elk-app` tier.
pub const session = @import("session.zig");
pub const jobs = @import("jobs.zig");
pub const settings = @import("settings.zig");
pub const spelling = @import("spelling.zig");

test {
    @import("std").testing.refAllDecls(@This());
}
