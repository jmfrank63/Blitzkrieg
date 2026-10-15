//! A text's state file, `<key>.xml` (D-05): `SELKTextProperty` from
//! Sources/src/ELK/ELK_Types.h, written by the engine's data tree as
//! `<base State="n" Changed="0|1"/>` after the XML declaration, CRLF after
//! each line. `Changed` (the MFC `bTranslated`) becomes true the first time a
//! state other than "Not translated" is set and stays true.
const std = @import("std");
const xml = @import("xml");

pub const State = enum(u2) {
    not_translated = 0,
    /// Set only by an update whose original changed, never by hand.
    outdated = 1,
    translated = 2,
    approved = 3,
};

pub const TextProperty = struct {
    state: State = .not_translated,
    changed: bool = false,

    /// `CELK::SetState`'s rule: the new state, and `changed` once any state
    /// but "Not translated" is set.
    pub fn withState(self: TextProperty, state: State) TextProperty {
        return .{ .state = state, .changed = self.changed or state != .not_translated };
    }
};

/// The property a state file holds. A missing attribute, an unreadable file
/// or a file of another shape (four tracked state files hold a whole `CELK`
/// that an old ELK wrote over them) gives the defaults, as the data tree
/// reader did. A state outside 0..3 reads as "Not translated", the clamp the
/// MFC import applied, so no caller indexes past the four states.
pub fn parse(bytes: []const u8) TextProperty {
    // A state file is one element; a file too big for this buffer is not a
    // state file and reads as the defaults like any other foreign shape.
    var memory: [8 * 1024]u8 = undefined;
    var fixed = std.heap.FixedBufferAllocator.init(&memory);
    const document = xml.parse(fixed.allocator(), bytes) catch return .{};
    var property: TextProperty = .{};
    if (xml.attribute(document.root, "State")) |text| {
        const value = std.fmt.parseInt(i32, std.mem.trim(u8, text, " \t\r\n"), 10) catch 0;
        property.state = if (value >= 0 and value <= 3) @enumFromInt(value) else .not_translated;
    }
    if (xml.attribute(document.root, "Changed")) |text| property.changed = parseBool(text);
    return property;
}

pub fn parseBool(text: []const u8) bool {
    const value = std.mem.trim(u8, text, " \t\r\n");
    if (std.ascii.eqlIgnoreCase(value, "true")) return true;
    return (std.fmt.parseInt(i32, value, 10) catch 0) != 0;
}

/// The longest state file `format` writes.
pub const max_len = 64;

/// The state file's bytes, exactly as the engine's data tree writes them.
pub fn format(buffer: *[max_len]u8, property: TextProperty) []const u8 {
    return std.fmt.bufPrint(buffer, "<?xml version=\"1.0\"?>\r\n<base State=\"{d}\" Changed=\"{d}\"/>\r\n", .{
        @intFromEnum(property.state),
        @intFromBool(property.changed),
    }) catch unreachable;
}

const testing = std.testing;

test "the shipped state file forms parse" {
    try testing.expectEqual(TextProperty{ .state = .outdated, .changed = true }, parse("<?xml version=\"1.0\"?>\r\n<base State=\"1\" Changed=\"1\"/>\r\n"));
    try testing.expectEqual(TextProperty{ .state = .approved, .changed = true }, parse("<?xml version=\"1.0\"?>\r\n<base State=\"3\" Changed=\"1\"/>\r\n"));
    try testing.expectEqual(TextProperty{}, parse("<?xml version=\"1.0\"?>\r\n<base State=\"0\" Changed=\"0\"/>\r\n"));
}

test "a broken, foreign or out-of-range state file reads as the defaults" {
    try testing.expectEqual(TextProperty{}, parse(""));
    try testing.expectEqual(TextProperty{}, parse("<base State=\"1\""));
    try testing.expectEqual(TextProperty{}, parse("<?xml version=\"1.0\"?>\r\n<base><Elements/><Path>x.xml</Path></base>\r\n"));
    try testing.expectEqual(TextProperty{ .changed = true }, parse("<base State=\"7\" Changed=\"1\"/>"));
}

test "Changed becomes true on the first state but Not translated and stays true" {
    const fresh: TextProperty = .{};
    try testing.expectEqual(TextProperty{}, fresh.withState(.not_translated));
    const translated = fresh.withState(.translated);
    try testing.expect(translated.changed);
    try testing.expectEqual(TextProperty{ .state = .not_translated, .changed = true }, translated.withState(.not_translated));
}

test "every tracked state file formats back to its own bytes" {
    // The four files an old ELK overwrote with a whole CELK are the only
    // exceptions, and they read as the defaults.
    const io = testing.io;
    const roots = [_][]const u8{ "Data/ELK/game_data_base", "Data/AmericanELK/game_data_base" };
    var checked: usize = 0;
    var foreign: usize = 0;
    for (roots) |root| {
        var dir = try std.Io.Dir.cwd().openDir(io, root, .{ .iterate = true });
        defer dir.close(io);
        var walker = try dir.walk(testing.allocator);
        defer walker.deinit();
        while (try walker.next(io)) |entry| {
            if (entry.kind != .file or !std.mem.endsWith(u8, entry.basename, ".xml")) continue;
            const bytes = try dir.readFileAlloc(io, entry.path, testing.allocator, .limited(1 << 20));
            defer testing.allocator.free(bytes);
            var buffer: [max_len]u8 = undefined;
            const property = parse(bytes);
            checked += 1;
            if (std.mem.eql(u8, bytes, format(&buffer, property))) continue;
            try testing.expect(std.mem.indexOf(u8, bytes, "<Elements") != null);
            try testing.expectEqual(TextProperty{}, property);
            foreign += 1;
        }
    }
    try testing.expect(checked > 6_000);
    try testing.expectEqual(@as(usize, 4), foreign);
}
