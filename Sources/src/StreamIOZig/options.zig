const std = @import("std");
const xml = @import("xml.zig");

pub const Option = struct {
    name: [:0]u8,
    value: [:0]u8,
    default_value: [:0]u8,
    value_type: u16,
    editor_type: i32,
    flags: u32,
    order: i32,
    instant_apply: bool,
    action: [:0]u8,
    action_fill: [:0]u8,
};

pub const System = struct {
    allocator: std.mem.Allocator,
    entries: std.ArrayListUnmanaged(Option) = .empty,
    changed: bool = false,

    pub fn init(allocator: std.mem.Allocator) System {
        return .{ .allocator = allocator };
    }

    pub fn deinit(self: *System) void {
        for (self.entries.items) |entry| self.freeOption(entry);
        self.entries.deinit(self.allocator);
    }

    fn freeOption(self: *System, entry: Option) void {
        self.allocator.free(entry.name);
        self.allocator.free(entry.value);
        self.allocator.free(entry.default_value);
        self.allocator.free(entry.action);
        self.allocator.free(entry.action_fill);
    }

    pub fn findIndex(self: *const System, name: []const u8) ?usize {
        for (self.entries.items, 0..) |entry, index| {
            if (std.ascii.eqlIgnoreCase(entry.name, name)) return index;
        }
        return null;
    }

    pub fn get(self: *const System, name: []const u8) ?*const Option {
        const index = self.findIndex(name) orelse return null;
        return &self.entries.items[index];
    }

    pub fn set(self: *System, name: []const u8, value: []const u8, value_type: u16) !void {
        if (self.findIndex(name)) |index| {
            const replacement = try self.allocator.dupeZ(u8, value);
            self.allocator.free(self.entries.items[index].value);
            self.entries.items[index].value = replacement;
            self.entries.items[index].value_type = value_type;
        } else {
            try self.entries.append(self.allocator, .{
                .name = try self.allocator.dupeZ(u8, name),
                .value = try self.allocator.dupeZ(u8, value),
                .default_value = try self.allocator.dupeZ(u8, value),
                .value_type = value_type,
                .editor_type = 4,
                .flags = 0,
                .order = @intCast(self.entries.items.len),
                .instant_apply = false,
                .action = try self.allocator.dupeZ(u8, ""),
                .action_fill = try self.allocator.dupeZ(u8, ""),
            });
        }
        self.changed = true;
    }

    pub fn remove(self: *System, name: []const u8) bool {
        const index = self.findIndex(name) orelse return false;
        self.freeOption(self.entries.items[index]);
        _ = self.entries.orderedRemove(index);
        self.changed = true;
        return true;
    }

    pub fn removePrefix(self: *System, prefix: []const u8) void {
        var index: usize = self.entries.items.len;
        while (index > 0) {
            index -= 1;
            if (std.ascii.startsWithIgnoreCase(self.entries.items[index].name, prefix)) {
                self.freeOption(self.entries.items[index]);
                _ = self.entries.orderedRemove(index);
                self.changed = true;
            }
        }
    }

    pub fn loadXml(self: *System, root: *xml.Node, only_missing: bool) !usize {
        const options_node = if (std.mem.eql(u8, root.name, "Options")) root else xml.child(root, "Options") orelse return 0;
        const vars = xml.child(options_node, "Vars") orelse return 0;
        var loaded: usize = 0;
        for (vars.children.items) |item| {
            if (!std.mem.eql(u8, item.name, "item")) continue;
            const key_node = xml.child(item, "KeyName") orelse continue;
            const name = key_node.text;
            if (name.len == 0) continue;
            if (only_missing and self.findIndex(name) != null) continue;

            const type_text = xml.attribute(item, "Type") orelse "8";
            const value_type = std.fmt.parseInt(u16, type_text, 10) catch 8;
            const value = xml.attribute(item, "Var") orelse if (xml.child(item, "Var")) |node| node.text else "";
            const default_node = xml.child(item, "Default");
            const default_value = if (default_node) |node| (xml.attribute(node, "Var") orelse if (xml.child(node, "Var")) |value_node| value_node.text else value) else value;
            const replacement = Option{
                .name = try self.allocator.dupeZ(u8, name),
                .value = try self.allocator.dupeZ(u8, value),
                .default_value = try self.allocator.dupeZ(u8, default_value),
                .value_type = value_type,
                .editor_type = parseI32(xml.attribute(item, "EditorType"), 4),
                .flags = parseU32Compat(xml.attribute(item, "Flags"), 0),
                .order = parseI32(xml.attribute(item, "Order"), 0),
                .instant_apply = parseI32(xml.attribute(item, "InstantApply"), 0) != 0,
                .action = try self.allocator.dupeZ(u8, if (xml.child(item, "Action")) |node| node.text else ""),
                .action_fill = try self.allocator.dupeZ(u8, if (xml.child(item, "ActionFill")) |node| node.text else ""),
            };
            if (self.findIndex(name)) |index| {
                self.freeOption(self.entries.items[index]);
                self.entries.items[index] = replacement;
            } else try self.entries.append(self.allocator, replacement);
            loaded += 1;
        }
        self.changed = false;
        return loaded;
    }
};

fn parseI32(value: ?[]const u8, fallback: i32) i32 {
    return std.fmt.parseInt(i32, value orelse return fallback, 10) catch fallback;
}

fn parseU32Compat(value: ?[]const u8, fallback: u32) u32 {
    const text = value orelse return fallback;

    if (std.fmt.parseInt(u32, text, 10)) |parsed| return parsed else |_| {}

    if (std.fmt.parseInt(i32, text, 10)) |signed| {
        // Match legacy C/C++ behavior where negative signed values assigned to
        // unsigned fields wrap using two's-complement representation.
        return @bitCast(signed);
    } else |_| {}

    return fallback;
}

test "loads legacy option records and preserves config precedence" {
    const source = "<base><Options><Vars><item Type=\"8\" Flags=\"49\" Order=\"1\"><Var>High</Var><Default Type=\"8\"><Var>Low</Var></Default><KeyName>GFX.Texture.Quality</KeyName></item></Vars></Options></base>";
    var document = try xml.parse(std.testing.allocator, source);
    defer document.deinit();
    var system = System.init(std.testing.allocator);
    defer system.deinit();
    try std.testing.expectEqual(@as(usize, 1), try system.loadXml(document.root, false));
    try std.testing.expectEqualStrings("High", system.get("gfx.texture.quality").?.value);
    try std.testing.expectEqualStrings("Low", system.get("GFX.Texture.Quality").?.default_value);
}
