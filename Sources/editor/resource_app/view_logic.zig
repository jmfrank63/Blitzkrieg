//! The View menu's state that needs no window (A-13, A-16, A-17): which of
//! the toolbar, status line, project tree and object inspector are shown,
//! the preview background colour, and MFC's Expand/Collapse flag. Kept apart
//! so `test-resource-app-logic` covers it and the auto tier drives the same
//! functions the menu calls. The toggles and the colour persist in
//! resourceeditor.cfg (settings.zig); the flag does not, as in MFC.
const std = @import("std");
const settings_mod = @import("settings.zig");

/// The four toggles, named as the auto tier's `do=view:<name>:<on|off>` says them.
pub const Part = enum {
    toolbar,
    status_bar,
    tree,
    inspector,

    pub fn parse(name: []const u8) ?Part {
        inline for (std.enums.values(Part)) |tag| {
            if (std.mem.eql(u8, name, @tagName(tag))) return @field(Part, @tagName(tag));
        }
        return null;
    }

    /// The menu's label, MFC's View menu wording.
    pub fn label(self: Part) [:0]const u8 {
        return switch (self) {
            .toolbar => "Toolbar",
            .status_bar => "Status Bar",
            .tree => "Project Tree",
            .inspector => "Object Inspector",
        };
    }
};

pub const View = struct {
    toolbar: bool = true,
    status_bar: bool = true,
    tree: bool = true,
    inspector: bool = true,
    /// 0xRRGGBB, or null for the engine's own clear colour.
    background: ?u32 = null,
    /// CParentFrame::bTreeExpand: true at start, flipped by every Expand/
    /// Collapse all, so the first use collapses.
    tree_expand: bool = true,

    pub fn fromSettings(settings: *const settings_mod.Settings) View {
        return .{
            .toolbar = settings.view_toolbar,
            .status_bar = settings.view_status_bar,
            .tree = settings.view_tree,
            .inspector = settings.view_inspector,
            .background = settings.background_colour,
        };
    }

    pub fn storeInto(self: View, settings: *settings_mod.Settings) void {
        settings.view_toolbar = self.toolbar;
        settings.view_status_bar = self.status_bar;
        settings.view_tree = self.tree;
        settings.view_inspector = self.inspector;
        settings.background_colour = self.background;
    }

    pub fn shown(self: View, part: Part) bool {
        return switch (part) {
            .toolbar => self.toolbar,
            .status_bar => self.status_bar,
            .tree => self.tree,
            .inspector => self.inspector,
        };
    }

    /// Returns whether the state changed.
    pub fn set(self: *View, part: Part, on: bool) bool {
        if (self.shown(part) == on) return false;
        switch (part) {
            .toolbar => self.toolbar = on,
            .status_bar => self.status_bar = on,
            .tree => self.tree = on,
            .inspector => self.inspector = on,
        }
        return true;
    }

    /// Flips the Expand/Collapse flag and returns the state to apply.
    pub fn flipExpand(self: *View) bool {
        self.tree_expand = !self.tree_expand;
        return self.tree_expand;
    }
};

/// "RRGGBB" (no '#') for the auto tier's `do=background:<rrggbb>`.
pub fn parseColour(text: []const u8) ?u32 {
    if (text.len != 6) return null;
    return std.fmt.parseInt(u32, text, 16) catch null;
}

/// The colour picker's three floats (0..1) as 0xRRGGBB.
pub fn packColour(r: f32, g: f32, b: f32) u32 {
    return (channel(r) << 16) | (channel(g) << 8) | channel(b);
}

fn channel(value: f32) u32 {
    return @intFromFloat(std.math.clamp(value, 0, 1) * 255 + 0.5);
}

pub fn unpackColour(rgb: u32) [3]f32 {
    return .{
        @as(f32, @floatFromInt((rgb >> 16) & 0xFF)) / 255,
        @as(f32, @floatFromInt((rgb >> 8) & 0xFF)) / 255,
        @as(f32, @floatFromInt(rgb & 0xFF)) / 255,
    };
}

const testing = std.testing;

test "the four toggles start shown, report a change once and round-trip through the settings" {
    var view: View = .{};
    for ([_]Part{ .toolbar, .status_bar, .tree, .inspector }) |part| try testing.expect(view.shown(part));
    try testing.expect(view.set(.tree, false));
    try testing.expect(!view.set(.tree, false));
    try testing.expect(view.set(.status_bar, false));
    try testing.expect(!view.shown(.tree) and view.shown(.inspector) and view.shown(.toolbar));
    view.background = 0x336699;

    var settings: settings_mod.Settings = .{};
    view.storeInto(&settings);
    var buffer: [512]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try settings_mod.format(&settings, &writer);
    const parsed = settings_mod.parse(writer.buffered());
    const back = View.fromSettings(&parsed);
    try testing.expect(!back.tree and !back.status_bar and back.toolbar and back.inspector);
    try testing.expectEqual(@as(u32, 0x336699), back.background.?);
}

test "the Expand/Collapse flag starts true, so the first use collapses, as MFC's" {
    var view: View = .{};
    try testing.expect(!view.flipExpand());
    try testing.expect(view.flipExpand());
}

test "part names, colour text and the picker's floats" {
    try testing.expect(Part.parse("status_bar").? == .status_bar);
    try testing.expect(Part.parse("nothing") == null);
    try testing.expectEqual(@as(u32, 0xff8000), parseColour("ff8000").?);
    try testing.expect(parseColour("ff80") == null and parseColour("gg0000") == null);
    try testing.expectEqual(@as(u32, 0xff8000), packColour(1, 128.0 / 255.0, 0));
    const back = unpackColour(0x0a1b2c);
    try testing.expectEqual(@as(u32, 0x0a1b2c), packColour(back[0], back[1], back[2]));
}
