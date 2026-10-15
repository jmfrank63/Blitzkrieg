//! The vendored Hunspell (vendor/hunspell, D-11) as the core's spell
//! `Engine`, through Hunspell's own C API (`hunspell.h`). Only UTF-8
//! dictionaries are taken: ELK's texts are UTF-8, and Hunspell compares
//! bytes in the dictionary's own encoding without converting.
const std = @import("std");
const elk = @import("elk_core");

const Handle = opaque {};
extern fn Hunspell_create(affpath: [*:0]const u8, dpath: [*:0]const u8) ?*Handle;
extern fn Hunspell_destroy(handle: *Handle) void;
extern fn Hunspell_spell(handle: *Handle, word: [*:0]const u8) c_int;
extern fn Hunspell_get_dic_encoding(handle: *Handle) ?[*:0]const u8;
extern fn Hunspell_suggest(handle: *Handle, list: *?[*][*:0]u8, word: [*:0]const u8) c_int;
extern fn Hunspell_free_list(handle: *Handle, list: *?[*][*:0]u8, count: c_int) void;
extern fn Hunspell_add(handle: *Handle, word: [*:0]const u8) c_int;

/// Longer words are reported misspelt without asking: Hunspell itself
/// rejects words this long.
const max_word = 256;

pub const Hunspell = struct {
    handle: *Handle,

    pub const OpenError = error{ OutOfMemory, NotLoaded };

    /// The dictionary pair at these paths; the caller has checked that both
    /// files exist, since Hunspell loads a missing file as an empty one.
    pub fn open(gpa: std.mem.Allocator, aff_path: []const u8, dic_path: []const u8) OpenError!Hunspell {
        const aff = try gpa.dupeSentinel(u8, aff_path, 0);
        defer gpa.free(aff);
        const dic = try gpa.dupeSentinel(u8, dic_path, 0);
        defer gpa.free(dic);
        return .{ .handle = Hunspell_create(aff, dic) orelse return error.NotLoaded };
    }

    pub fn close(self: *Hunspell) void {
        Hunspell_destroy(self.handle);
    }

    /// The `SET` line of the affix file (Hunspell's default is ISO8859-1).
    pub fn encoding(self: *const Hunspell) []const u8 {
        return std.mem.span(Hunspell_get_dic_encoding(self.handle) orelse return "");
    }

    pub fn isUtf8(self: *const Hunspell) bool {
        return std.ascii.eqlIgnoreCase(self.encoding(), "UTF-8");
    }

    pub fn engine(self: *Hunspell) elk.spell.Engine {
        return .{ .context = self, .vtable = &.{ .spell = spell, .suggest = suggest, .add = add } };
    }

    fn spell(context: *anyopaque, word: []const u8) bool {
        const self: *Hunspell = @ptrCast(@alignCast(context));
        var buffer: [max_word + 1]u8 = undefined;
        const terminated = std.mem.printSentinel(&buffer, "{s}", .{word}, 0) catch return false;
        return Hunspell_spell(self.handle, terminated) != 0;
    }

    fn suggest(context: *anyopaque, allocator: std.mem.Allocator, word: []const u8) error{OutOfMemory}![]const []const u8 {
        const self: *Hunspell = @ptrCast(@alignCast(context));
        var buffer: [max_word + 1]u8 = undefined;
        const terminated = std.mem.printSentinel(&buffer, "{s}", .{word}, 0) catch return allocator.alloc([]const u8, 0);
        var list: ?[*][*:0]u8 = null;
        const count = Hunspell_suggest(self.handle, &list, terminated);
        defer Hunspell_free_list(self.handle, &list, count);
        const items = list orelse return allocator.alloc([]const u8, 0);
        const suggestions = try allocator.alloc([]const u8, @intCast(@max(count, 0)));
        var made: usize = 0;
        errdefer {
            for (suggestions[0..made]) |made_one| allocator.free(made_one);
            allocator.free(suggestions);
        }
        for (suggestions, 0..) |*slot, index| {
            slot.* = try allocator.dupe(u8, std.mem.span(items[index]));
            made += 1;
        }
        return suggestions;
    }

    fn add(context: *anyopaque, word: []const u8) void {
        const self: *Hunspell = @ptrCast(@alignCast(context));
        var buffer: [max_word + 1]u8 = undefined;
        const terminated = std.mem.printSentinel(&buffer, "{s}", .{word}, 0) catch return;
        _ = Hunspell_add(self.handle, terminated);
    }
};
