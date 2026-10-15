//! A named filter (`SSimpleFilter` in Sources/src/ELK/ELK_Types.h): which
//! texts browsing walks and the filtered export writes. A text passes when
//! its folder path holds every substring of at least one condition (no
//! condition passes every folder), its state is one the filter keeps, and,
//! when the filter asks for it, its `Changed` flag is set
//! (`SSimpleFilter::Check` in ELK_Methods.cpp). With no filter in the
//! settings the MFC ELK made the five below, "All" current.
//!
//! A filter is one line, `<name>|<states>|<changed>|<conditions>`, the way
//! elk.cfg keeps it (D-16): the states are four `0`/`1` in state order (Not
//! translated, Outdated, Translated, Approved), `<changed>` is `0` or `1`,
//! and the conditions are AND-lists of substrings joined by `,`, the lists
//! joined by `;`. A `Filter` borrows its line's bytes and checks the
//! conditions in place, so it needs no allocation.
const std = @import("std");
const state_mod = @import("state.zig");

const State = state_mod.State;
const TextProperty = state_mod.TextProperty;

pub const Filter = struct {
    name: []const u8,
    states: [4]bool = @splat(true),
    /// Only texts whose state was ever set (`bTranslated`, the state file's
    /// `Changed`).
    changed: bool = false,
    /// The OR of AND-lists, `a,b;c`; "" passes every folder.
    conditions: []const u8 = "",

    /// `SSimpleFilter::Check` on a text's key: the folder is the key without
    /// its last name, in the MFC tree's form (`medals\1`, lower case), and
    /// each substring is looked for as given, as the MFC ELK did.
    pub fn matches(self: Filter, key: []const u8, property: TextProperty) bool {
        if (!self.states[@intFromEnum(property.state)]) return false;
        if (self.changed and !property.changed) return false;
        if (self.conditions.len == 0) return true;
        var folder_buffer: [1024]u8 = undefined;
        const folder_part = key[0 .. std.mem.lastIndexOfScalar(u8, key, '/') orelse 0];
        // A folder too long for the buffer is cut; no real key comes near.
        const folder = folder_buffer[0..@min(folder_part.len, folder_buffer.len)];
        for (folder, folder_part[0..folder.len]) |*out, byte| out.* = if (byte == '/') '\\' else std.ascii.toLower(byte);
        var any = std.mem.splitScalar(u8, self.conditions, ';');
        while (any.next()) |condition| {
            var every = std.mem.splitScalar(u8, condition, ',');
            const all = while (every.next()) |part| {
                if (std.mem.indexOf(u8, folder, part) == null) break false;
            } else true;
            if (all) return true;
        }
        return false;
    }

    /// The filter a settings line holds; null when the line is not one.
    pub fn parse(line: []const u8) ?Filter {
        var fields = std.mem.splitScalar(u8, line, '|');
        const name = fields.next() orelse return null;
        const states = fields.next() orelse return null;
        const changed = fields.next() orelse return null;
        const conditions = fields.rest();
        if (name.len == 0 or states.len != 4 or changed.len != 1) return null;
        var filter: Filter = .{ .name = name, .conditions = conditions };
        for (states, &filter.states) |digit, *keep| keep.* = switch (digit) {
            '0' => false,
            '1' => true,
            else => return null,
        };
        filter.changed = switch (changed[0]) {
            '0' => false,
            '1' => true,
            else => return null,
        };
        return filter;
    }

    pub fn format(self: Filter, writer: *std.Io.Writer) std.Io.Writer.Error!void {
        try writer.print("{s}|", .{self.name});
        for (self.states) |keep| try writer.writeByte(if (keep) '1' else '0');
        try writer.print("|{c}|{s}", .{ @as(u8, if (self.changed) '1' else '0'), self.conditions });
    }
};

/// The five filters the MFC ELK made when the settings held none
/// (`SMainFrameParams::LoadFromRegistry`), in the order the combo lists them.
pub const built_in = [_]Filter{
    .{ .name = "All" },
    .{ .name = "Not Translated", .states = .{ true, false, false, false } },
    .{ .name = "Outdated", .states = .{ false, true, false, false } },
    .{ .name = "Translated", .states = .{ false, false, true, false } },
    .{ .name = "Approved", .states = .{ false, false, false, true } },
};

pub const default_name = "All";

const testing = std.testing;

test "Check: the states mask, the changed flag and the OR of AND-lists of folder substrings" {
    const plain: TextProperty = .{};
    const approved: TextProperty = .{ .state = .approved, .changed = true };
    try testing.expect(built_in[0].matches("medals/1/name", plain));
    try testing.expect(built_in[1].matches("medals/1/name", plain));
    try testing.expect(!built_in[4].matches("medals/1/name", plain));
    try testing.expect(built_in[4].matches("medals/1/name", approved));
    // Not translated again after a state was set: Changed stays.
    const reset: TextProperty = .{ .state = .not_translated, .changed = true };
    const only_changed: Filter = .{ .name = "c", .changed = true };
    try testing.expect(!only_changed.matches("a/b", plain));
    try testing.expect(only_changed.matches("a/b", reset));

    const folders: Filter = .{ .name = "f", .conditions = "medals,1;units\\tanks" };
    try testing.expect(folders.matches("Medals/1/name", plain));
    try testing.expect(!folders.matches("medals/2/name", plain));
    try testing.expect(folders.matches("units/tanks/t34/name", plain));
    // The folder only, never the text's own name; the substring as given.
    try testing.expect(!folders.matches("units/tanks", plain));
    const upper: Filter = .{ .name = "u", .conditions = "Medals" };
    try testing.expect(!upper.matches("medals/1/name", plain));
}

test "a filter round-trips through its settings line" {
    const filter: Filter = .{ .name = "Tanks done", .states = .{ false, false, true, true }, .changed = true, .conditions = "units,tanks;medals" };
    var buffer: [256]u8 = undefined;
    var writer: std.Io.Writer = .fixed(&buffer);
    try filter.format(&writer);
    try testing.expectEqualStrings("Tanks done|0011|1|units,tanks;medals", writer.buffered());
    const again = Filter.parse(writer.buffered()).?;
    try testing.expectEqualStrings(filter.name, again.name);
    try testing.expectEqual(filter.states, again.states);
    try testing.expectEqual(filter.changed, again.changed);
    try testing.expectEqualStrings(filter.conditions, again.conditions);
    try testing.expectEqualStrings("", Filter.parse("All|1111|0|").?.conditions);
    for ([_][]const u8{ "", "All", "All|111|0|", "All|1111|2|", "|1111|0|", "All|11x1|0|" }) |bad| {
        try testing.expectEqual(@as(?Filter, null), Filter.parse(bad));
    }
}
