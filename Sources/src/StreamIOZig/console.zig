const std = @import("std");

const channel_count = 32;
const Line = struct { text: [:0]u16, ascii: [:0]u8, color: u32, backup: bool };
const Channel = struct { lines: std.ArrayListUnmanaged(Line) = .empty, read_index: usize = 0 };

pub const Console = struct {
    allocator: std.mem.Allocator,
    channels: [channel_count]Channel = [_]Channel{.{}} ** channel_count,
    duplicates: [channel_count]u32 = [_]u32{0} ** channel_count,
    log_path: ?[:0]u8 = null,

    pub fn init(allocator: std.mem.Allocator) Console { return .{ .allocator = allocator }; }
    pub fn deinit(self: *Console) void {
        for (&self.channels) |*channel| {
            for (channel.lines.items) |line| {
                self.allocator.free(line.text);
                self.allocator.free(line.ascii);
            }
            channel.lines.deinit(self.allocator);
        }
        if (self.log_path) |path| self.allocator.free(path);
    }

    pub fn configure(self: *Console, config: []const u8) bool {
        var tokens = std.mem.splitScalar(u8, config, ';');
        const command = tokens.next() orelse return false;
        if (std.mem.eql(u8, command, "logfile")) {
            const value = tokens.next() orelse return false;
            const copy = self.allocator.dupeZ(u8, value) catch return false;
            if (self.log_path) |old| self.allocator.free(old);
            self.log_path = copy;
            return true;
        }
        if (std.mem.eql(u8, command, "dublicate")) {
            const source = parseChannel(tokens.next()) orelse return false;
            while (tokens.next()) |token| {
                if (parseChannel(token)) |target| self.duplicates[source] |= @as(u32, 1) << @intCast(target);
            }
            return true;
        }
        return std.mem.eql(u8, command, "name") and tokens.next() != null and tokens.next() != null;
    }

    pub fn writeWide(self: *Console, channel: i32, text: [*:0]const u16, color: u32, backup: bool) void {
        if (channel < 0 or channel >= channel_count) return;
        self.append(@intCast(channel), std.mem.span(text), color, backup);
        const mask = self.duplicates[@intCast(channel)];
        for (0..channel_count) |target| if ((mask & (@as(u32, 1) << @intCast(target))) != 0) self.append(target, std.mem.span(text), color, false);
    }

    pub fn writeAscii(self: *Console, channel: i32, text: [*:0]const u8, color: u32, backup: bool) void {
        const bytes = std.mem.span(text);
        const wide = self.allocator.allocSentinel(u16, bytes.len, 0) catch return;
        defer self.allocator.free(wide);
        for (bytes, 0..) |byte, index| wide[index] = byte;
        self.writeWide(channel, wide.ptr, color, backup);
    }

    fn append(self: *Console, channel: usize, text: []const u16, color: u32, backup: bool) void {
        const copy = self.allocator.dupeZ(u16, text) catch return;
        const ascii = self.allocator.allocSentinel(u8, text.len, 0) catch {
            self.allocator.free(copy);
            return;
        };
        for (text, 0..) |unit, index| ascii[index] = @truncate(unit);
        self.channels[channel].lines.append(self.allocator, .{ .text = copy, .ascii = ascii, .color = color, .backup = backup }) catch {
            self.allocator.free(ascii);
            self.allocator.free(copy);
        };
    }

    pub fn readWide(self: *Console, channel: i32, color: ?*u32) ?[*:0]const u16 {
        if (channel < 0 or channel >= channel_count) return null;
        const queue = &self.channels[@intCast(channel)];
        if (queue.read_index >= queue.lines.items.len) return null;
        const line = &queue.lines.items[queue.read_index];
        queue.read_index += 1;
        if (color) |result| result.* = line.color;
        return line.text.ptr;
    }

    pub fn readAscii(self: *Console, channel: i32, color: ?*u32) ?[*:0]const u8 {
        if (channel < 0 or channel >= channel_count) return null;
        const queue = &self.channels[@intCast(channel)];
        if (queue.read_index >= queue.lines.items.len) return null;
        const line = &queue.lines.items[queue.read_index];
        queue.read_index += 1;
        if (color) |result| result.* = line.color;
        return line.ascii.ptr;
    }
};

fn parseChannel(value: ?[]const u8) ?usize {
    const parsed = std.fmt.parseInt(usize, value orelse return null, 10) catch return null;
    return if (parsed < channel_count) parsed else null;
}

test "console queues ASCII commands and duplicates channels" {
    var console = Console.init(std.testing.allocator);
    defer console.deinit();
    try std.testing.expect(console.configure("dublicate;3;2"));
    console.writeAscii(3, "Exec", 0xff00ff00, true);
    try std.testing.expectEqualStrings("Exec", std.mem.span(console.readAscii(3, null).?));
    try std.testing.expectEqualStrings("Exec", std.mem.span(console.readAscii(2, null).?));
}

test "ASCII read results remain valid across differently sized lines" {
    var console = Console.init(std.testing.allocator);
    defer console.deinit();
    console.writeAscii(1, "A", 0, false);
    console.writeAscii(1, "second command", 0, false);

    const first = console.readAscii(1, null).?;
    const second = console.readAscii(1, null).?;

    try std.testing.expectEqualStrings("A", std.mem.span(first));
    try std.testing.expectEqualStrings("second command", std.mem.span(second));
}
