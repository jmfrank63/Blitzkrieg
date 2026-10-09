//! The object filters (M3, D-31): the named conditions of folder words the
//! shipped `Data/Editor/filter.xml` carries, the palette's nine quick toggles
//! and combo gate objects with, and the Filters Composer edits.
//!
//! The model mirrors the MFC's `SSimpleFilter` (CreateFilterDialog.h, last in the
//! tree at 045ddda7f; the MFC editor was deleted in 05-11): a filter is a list of word lists - a word list is a
//! condition (every word must appear in the object's folder path), and the
//! filter passes when ANY condition matches. The MFC lowercases the folder
//! name before checking (TabSimpleObjectsDialog.cpp:178) and the shipped words
//! are lowercase, so matching here is case-insensitive on both sides.
//!
//! Std-only: the slices borrow their storage from whoever built them (the
//! bridge's fixed buffers, the fake's fixtures, a test literal); nothing here
//! allocates except `merge`'s result array.
const std = @import("std");

/// One condition: every word must appear in the folder path (an AND).
pub const WordList = []const []const u8;

/// One named filter: ANY condition matching passes the object (an OR over
/// `lists`). A filter with no lists matches nothing - the MFC's own
/// `filter.empty()` answer (CreateFilterDialog.cpp:15).
pub const Filter = struct {
    name: []const u8,
    lists: []const WordList,

    /// The MFC's SSimpleFilter::Check with its caller's ToLower folded in:
    /// true when at least one word list has every word present in the
    /// lowercased folder path. A word list with no words matches everything
    /// (a vacuous AND - the MFC's inner loop leaves bInnerChecked true),
    /// which only arises for a hand-edited file; the composer never makes
    /// one. The folder path is the object database's key (GameDB.cpp:523:
    /// the key IS the folder, e.g. "buildings\Africa\Summer\A_Cisterns").
    pub fn matches(self: Filter, folder_path: []const u8) bool {
        if (self.lists.len == 0) return false;
        for (self.lists) |list| {
            var all = true;
            for (list) |word| {
                if (std.ascii.findIgnoreCase(folder_path, word) == null) {
                    all = false;
                    break;
                }
            }
            if (all) return true;
        }
        return false;
    }
};

/// `merge`'s rule, shared with the bridge's read: the shipped filters keep
/// their order, a user filter with a byte-equal name replaces the shipped one
/// in place, and user-only names are appended after in their own order.
/// The result's filters borrow their slices from the inputs, so the inputs
/// must outlive it; only the outer array is new (free it with `allocator`).
pub fn merge(allocator: std.mem.Allocator, shipped: []const Filter, user: []const Filter) std.mem.Allocator.Error![]Filter {
    var appended: usize = 0;
    for (user) |one| {
        if (findNamedConst(shipped, one.name) == null) appended += 1;
    }
    const out = try allocator.alloc(Filter, shipped.len + appended);
    @memcpy(out[0..shipped.len], shipped);
    var tail = shipped.len;
    for (user) |one| {
        if (findNamed(out[0..shipped.len], one.name)) |slot| {
            slot.* = one;
        } else {
            out[tail] = one;
            tail += 1;
        }
    }
    return out;
}

fn findNamed(filters: []Filter, name: []const u8) ?*Filter {
    for (filters) |*one| {
        if (std.mem.eql(u8, one.name, name)) return one;
    }
    return null;
}

fn findNamedConst(filters: []const Filter, name: []const u8) ?usize {
    for (filters, 0..) |one, index| {
        if (std.mem.eql(u8, one.name, name)) return index;
    }
    return null;
}

/// A filter name the composer may create or rename to: 1..63 bytes, no
/// control characters, and no `|` (the `filter_rename=old|new` command's own
/// separator). Names are compared byte-exactly everywhere (the MFC's
/// unordered_map key did).
pub fn nameValid(name: []const u8) bool {
    if (name.len == 0 or name.len > max_name_len) return false;
    for (name) |char| {
        if (char < 0x20 or char == 0x7f or char == '|') return false;
    }
    return true;
}

/// A name's room: the bridge's fixed char[64] minus its NUL.
pub const max_name_len = 64 - 1;

test "matches mirrors the MFC's Check: OR of ANDs over folder words" {
    const buildings = Filter{ .name = "Buildings", .lists = &.{&.{"buildings"}} };
    try std.testing.expect(buildings.matches("buildings\\Africa\\Summer\\A_Cisterns"));
    try std.testing.expect(buildings.matches("buildings"));
    try std.testing.expect(!buildings.matches("units\\german\\tank"));
    // Substring, not path segment: the MFC's std::string::find.
    try std.testing.expect(buildings.matches("xbuildingsx"));
}

test "matches: a multi-word condition needs every word" {
    const filter = Filter{ .name = "Obj T Russian", .lists = &.{&.{ "objects", "ussr" }} };
    try std.testing.expect(filter.matches("objects\\terraobjects\\ussr\\barrel"));
    try std.testing.expect(!filter.matches("objects\\europe\\barrel"));
    try std.testing.expect(!filter.matches("ussr"));
}

test "matches: any matching condition passes, an empty filter matches nothing" {
    const or_lists = Filter{ .name = "two", .lists = &.{ &.{"buildings"}, &.{ "objects", "ussr" } } };
    try std.testing.expect(or_lists.matches("buildings\\house"));
    try std.testing.expect(or_lists.matches("objects\\terraobjects\\ussr\\barrel"));
    try std.testing.expect(!or_lists.matches("objects\\europe\\barrel"));
    const empty = Filter{ .name = "empty", .lists = &.{} };
    try std.testing.expect(!empty.matches("buildings\\house"));
}

test "matches: case-insensitive on both sides, like the MFC's ToLower" {
    const filter = Filter{ .name = "Buildings", .lists = &.{&.{"buildings"}} };
    try std.testing.expect(filter.matches("BUILDINGS\\House"));
    const upper_words = Filter{ .name = "Upper", .lists = &.{&.{"BUILDINGS"}} };
    try std.testing.expect(upper_words.matches("buildings\\house"));
}

test "matches: a condition with no words matches everything (vacuous AND)" {
    const vacuous = Filter{ .name = "vacuous", .lists = &.{&.{}} };
    try std.testing.expect(vacuous.matches("anything\\at\\all"));
    try std.testing.expect(vacuous.matches(""));
}

test "merge: a user filter overrides the shipped one in place, others append in order" {
    const shipped_a = Filter{ .name = "A", .lists = &.{&.{"a"}} };
    const shipped_b = Filter{ .name = "B", .lists = &.{&.{"b"}} };
    const user_b = Filter{ .name = "B", .lists = &.{&.{"b2"}} };
    const user_c = Filter{ .name = "C", .lists = &.{&.{"c"}} };

    const merged = try merge(std.testing.allocator, &.{ shipped_a, shipped_b }, &.{ user_b, user_c });
    defer std.testing.allocator.free(merged);

    try std.testing.expectEqual(@as(usize, 3), merged.len);
    try std.testing.expectEqualStrings("A", merged[0].name);
    try std.testing.expectEqualStrings("b2", merged[1].lists[0][0]);
    try std.testing.expectEqualStrings("C", merged[2].name);
    // The override keeps the shipped position and borrows the user's lists.
    try std.testing.expect(merged[1].lists.ptr == user_b.lists.ptr);
}

test "merge: an empty user list leaves the shipped ones, empty shipped takes the user's" {
    const shipped = Filter{ .name = "A", .lists = &.{&.{"a"}} };
    const user = Filter{ .name = "U", .lists = &.{&.{"u"}} };

    const only_shipped = try merge(std.testing.allocator, &.{shipped}, &.{});
    defer std.testing.allocator.free(only_shipped);
    try std.testing.expectEqual(@as(usize, 1), only_shipped.len);
    try std.testing.expectEqualStrings("A", only_shipped[0].name);

    const only_user = try merge(std.testing.allocator, &.{}, &.{user});
    defer std.testing.allocator.free(only_user);
    try std.testing.expectEqual(@as(usize, 1), only_user.len);
    try std.testing.expectEqualStrings("U", only_user[0].name);

    const both_empty = try merge(std.testing.allocator, &.{}, &.{});
    defer std.testing.allocator.free(both_empty);
    try std.testing.expectEqual(@as(usize, 0), both_empty.len);
}

test "merge: names compare byte-exactly, so case differences append" {
    const shipped = Filter{ .name = "Buildings", .lists = &.{&.{"buildings"}} };
    const user = Filter{ .name = "buildings", .lists = &.{&.{"x"}} };
    const merged = try merge(std.testing.allocator, &.{shipped}, &.{user});
    defer std.testing.allocator.free(merged);
    try std.testing.expectEqual(@as(usize, 2), merged.len);
}

test "nameValid: 1..63 bytes, no control characters, no pipe" {
    try std.testing.expect(nameValid("Buildings"));
    try std.testing.expect(nameValid("Obj T Russian"));
    try std.testing.expect(!nameValid(""));
    try std.testing.expect(!nameValid("a|b"));
    try std.testing.expect(!nameValid("a\nb"));
    try std.testing.expect(nameValid(&@as([63:0]u8, @splat('a'))));
    try std.testing.expect(!nameValid(&@as([64:0]u8, @splat('a'))));
}
