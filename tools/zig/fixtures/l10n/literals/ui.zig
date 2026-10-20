//! Fixture for the literal rule: one English literal in a UI call and one wrapped text.
const c = @import("c.zig");
const l10n = @import("l10n.zig");

pub fn draw() void {
    _ = c.igButton("Save", .{ .x = 0, .y = 0 });
    _ = c.igButton(l10n.t("file.save"), .{ .x = 0, .y = 0 });
    c.igText("%s", "not English");
}
