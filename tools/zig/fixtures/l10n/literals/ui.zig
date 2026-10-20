//! Fixture for the literal rule: one English literal in each kind of UI call and the wrapped or format-only texts that stay legal.
const c = @import("c.zig");
const l10n = @import("l10n.zig");

pub fn draw() void {
    _ = c.igButton("Save", .{ .x = 0, .y = 0 });
    _ = c.igButton(l10n.t("file.save"), .{ .x = 0, .y = 0 });
    c.igText("%s", "not English");
    _ = c.igBeginCombo("Mode", "Preview", 0);
    _ = c.igInputText("Name", &buf, buf.len, 0, null, null);
    _ = c.igInputTextWithHint(l10n.id("file.name"), "Type a name", &buf, buf.len, 0, null, null);
    _ = c.igInputFloat("Scale", &value, 0, 0, "%.2f", 0);
    _ = c.igInputFloatEx("Offset", &value, 0, 0, "%.2f", 0);
    _ = c.igInputInt("Count", &number, 1, 10, 0);
    _ = c.igSliderInt("Level", &number, 0, 9, "%d", 0);
    _ = c.igSliderFloat("Zoom", &value, 0, 1, "%.2f", 0);
    _ = c.igSliderFloatEx("Gamma", &value, 0, 1, "%.2f", 0);
    c.igOpenPopup("Confirm", 0);
    _ = c.igBeginPopup("Context", 0);
    _ = c.igBeginTabItem("Terrain", null, 0);
    _ = c.igBeginChild("Objects", .{ .x = 0, .y = 0 }, 0, 0);
    c.igSetItemTooltip("Hint text");
    _ = c.igCollapsingHeader("Advanced", 0);
    _ = c.igTreeNode("Branch");
    text("Hello", .{});
    beginPanel("Tools", &open);
    setStatus(state, "Saved the map", .{});
    setStatusFrom(state, l10n.t("status.ok"), "Failed");
    note(state, "Careful");
    say(&buf, "Saved the map", .{});
    sayImport(state, "Imported", .{});
    say(&buf, "file.saved", .{});
    sayImport(state, "file.imported", .{});
    _ = c.igBeginCombo("##mode", l10n.t("file.mode"), 0);
    setStatus(state, l10n.format("file.saved", .{"x"}), .{});
    _ = c.igInputTextWithHint("##name", l10n.t("file.hint"), &buf, buf.len, 0, null, null);
}
