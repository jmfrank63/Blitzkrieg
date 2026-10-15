//! The text tree (`CELKTreeWindow::FillTree`/`FillFolder` in
//! Sources/src/ELK/ELK_TreeWindow.cpp): one root per element of `elk.xml`,
//! its texts' folders below it and the texts as leaves, each node carrying a
//! state. A text's state is its state file's; a folder's or root's is the
//! lowest state below it (Not translated < Outdated < Translated <
//! Approved), which is the colour the MFC tree's icons showed, and it
//! follows a text's change up to the root (`UpdateFolderState`). Folders and
//! texts of one folder are ordered together by name without regard to case,
//! the order the MFC tree took from the directory listing. Browsing walks
//! the texts in the tree's order, through a filter when one is current
//! (`GetNextItem` and its kin).
const std = @import("std");
const database_mod = @import("database.zig");
const state_mod = @import("state.zig");
const filter_mod = @import("filter.zig");

const Database = database_mod.Database;
pub const State = state_mod.State;
pub const Filter = filter_mod.Filter;

/// One text: its element in `elk.xml` and its key below that element's
/// `<element>_data_base/` folder. The key is the database's own (borrowed).
pub const Text = struct {
    element: usize,
    key: []const u8,
};

/// A node of the tree. The nodes are stored depth first, so a node's
/// subtree is the nodes after it up to `end`.
pub const Node = struct {
    /// The folder's or text's own name; a root's is its element's title.
    name: []const u8,
    depth: u16,
    parent: ?u32,
    /// One past the last node of this node's subtree.
    end: u32,
    /// The index into `Tree.texts` for a text, null for a folder or root.
    text: ?u32,
    state: State,
    /// A text's `Changed` flag (the MFC tree kept it beside the state for
    /// the filters); false for a folder.
    changed: bool = false,

    pub fn isText(self: Node) bool {
        return self.text != null;
    }
};

pub const Tree = struct {
    /// Owns `nodes`, `texts`, `node_of_text` and the root titles.
    arena: std.heap.ArenaAllocator,
    nodes: []Node,
    /// Every text, element by element and in key order within one, as the
    /// database lists them.
    texts: []Text,
    /// The node of each text.
    node_of_text: []u32,

    /// Builds the tree of `database`, reading every text's state file once,
    /// as FillTree did. The keys stay the database's, so the tree must not
    /// outlive it.
    pub fn build(gpa: std.mem.Allocator, database: *Database) !Tree {
        var arena = std.heap.ArenaAllocator.init(gpa);
        errdefer arena.deinit();
        const allocator = arena.allocator();

        var count: usize = 0;
        for (database.bases) |base| count += base.keys.len;
        const texts = try allocator.alloc(Text, count);
        var builder: Builder = .{ .allocator = allocator };
        var next: usize = 0;
        for (database.bases, database.project.elements, 0..) |base, element, element_index| {
            const title = try std.fmt.allocPrint(allocator, "{s}, VERSION: {s}", .{ element.description.name, element.version });
            const top = try builder.add(null, title, null);
            for (base.keys) |key| {
                texts[next] = .{ .element = element_index, .key = key };
                var parent = top;
                var parts = std.mem.splitScalar(u8, key, '/');
                var name = parts.first();
                while (parts.next()) |rest| {
                    parent = try builder.folder(parent, name);
                    name = rest;
                }
                const leaf = try builder.add(parent, name, @intCast(next));
                const property = try database.readState(element_index, key);
                builder.items.items[leaf].state = property.state;
                builder.items.items[leaf].changed = property.changed;
                next += 1;
            }
        }

        const nodes = try allocator.alloc(Node, builder.items.items.len);
        const node_of_text = try allocator.alloc(u32, count);
        var written: u32 = 0;
        for (builder.roots.items) |top| _ = builder.flatten(top, null, 0, nodes, node_of_text, &written);
        return .{ .arena = arena, .nodes = nodes, .texts = texts, .node_of_text = node_of_text };
    }

    pub fn deinit(self: *Tree) void {
        self.arena.deinit();
    }

    /// The node of `key` in `element`, null when it has no such text.
    pub fn findText(self: *const Tree, element: usize, key: []const u8) ?u32 {
        for (self.texts, 0..) |text, index| {
            if (text.element == element and std.mem.eql(u8, text.key, key)) return self.node_of_text[index];
        }
        return null;
    }

    /// The folder `path` (slash-separated names below the element's root)
    /// in `element`, or the root itself for "". Names compare without
    /// regard to case, as the folders on disk do.
    pub fn findFolder(self: *const Tree, element: usize, path: []const u8) ?u32 {
        var at = self.rootOf(element) orelse return null;
        if (path.len == 0) return at;
        var parts = std.mem.splitScalar(u8, path, '/');
        while (parts.next()) |name| {
            at = self.child(at, name, false) orelse return null;
        }
        return at;
    }

    /// The root of `element`.
    pub fn rootOf(self: *const Tree, element: usize) ?u32 {
        var index: u32 = 0;
        var seen: usize = 0;
        while (index < self.nodes.len) : (index = self.nodes[index].end) {
            if (seen == element) return index;
            seen += 1;
        }
        return null;
    }

    /// The child of `parent` named `name`; a folder unless `text` is set.
    pub fn child(self: *const Tree, parent: u32, name: []const u8, text: bool) ?u32 {
        var index = parent + 1;
        while (index < self.nodes[parent].end) : (index = self.nodes[index].end) {
            const node = self.nodes[index];
            if (node.isText() == text and std.ascii.eqlIgnoreCase(node.name, name)) return index;
        }
        return null;
    }

    /// The selected text's new state, and every folder above it given the
    /// lowest state of its children again (`UpdateSelectedText`). `Changed`
    /// follows `TextProperty.withState`, as the state file does.
    pub fn setTextState(self: *Tree, text: u32, new_state: State) void {
        const node = self.node_of_text[text];
        self.nodes[node].state = new_state;
        self.nodes[node].changed = self.nodes[node].changed or new_state != .not_translated;
        var parent = self.nodes[node].parent;
        while (parent) |folder| : (parent = self.nodes[folder].parent) {
            self.nodes[folder].state = self.lowestChild(folder);
        }
    }

    /// Whether the text passes `filter`; every text passes no filter.
    pub fn textMatches(self: *const Tree, text: u32, filter: ?Filter) bool {
        const chosen = filter orelse return true;
        const node = self.nodes[self.node_of_text[text]];
        return chosen.matches(self.texts[text].key, .{ .state = node.state, .changed = node.changed });
    }

    /// Browse First, Previous, Next and Last (`GetFirstItem` ...): the text
    /// that step reaches through `filter`, in the tree's order across every
    /// element; null when none does. Previous and Next start from `from`
    /// and reach nothing without it, as the MFC commands did with nothing
    /// selected; First and Last ignore it.
    pub fn browse(self: *const Tree, step: Step, from: ?u32, filter: ?Filter) ?u32 {
        const count: u32 = @intCast(self.nodes.len);
        var at: u32 = switch (step) {
            .first => 0,
            .last => count,
            .next => self.node_of_text[from orelse return null] + 1,
            .previous => self.node_of_text[from orelse return null],
        };
        if (step == .first or step == .next) {
            while (at < count) : (at += 1) if (self.passes(at, filter)) return self.nodes[at].text;
        } else {
            while (at > 0) {
                at -= 1;
                if (self.passes(at, filter)) return self.nodes[at].text;
            }
        }
        return null;
    }

    fn passes(self: *const Tree, node: u32, filter: ?Filter) bool {
        const text = self.nodes[node].text orelse return false;
        return self.textMatches(text, filter);
    }

    /// One flag per node: a text passes `filter`, a folder or root holds a
    /// text that does. The view dims the rest.
    pub fn markMatches(self: *const Tree, filter: ?Filter, out: []bool) void {
        @memset(out, false);
        var index = self.nodes.len;
        // A node comes after its parent, so its subtree is marked first.
        while (index > 0) {
            index -= 1;
            const node = self.nodes[index];
            if (node.text) |text| out[index] = self.textMatches(text, filter);
            if (out[index]) if (node.parent) |parent| {
                out[parent] = true;
            };
        }
    }

    fn lowestChild(self: *const Tree, folder: u32) State {
        var lowest: State = .approved;
        var index = folder + 1;
        while (index < self.nodes[folder].end) : (index = self.nodes[index].end) {
            lowest = @enumFromInt(@min(@intFromEnum(lowest), @intFromEnum(self.nodes[index].state)));
        }
        return lowest;
    }
};

pub const Step = enum { first, previous, next, last };

/// The tree as linked children while it is filled, before it is laid out
/// depth first.
const Builder = struct {
    allocator: std.mem.Allocator,
    items: std.ArrayListUnmanaged(Item) = .empty,
    roots: std.ArrayListUnmanaged(u32) = .empty,

    const Item = struct {
        name: []const u8,
        text: ?u32,
        state: State = .approved,
        changed: bool = false,
        children: std.ArrayListUnmanaged(u32) = .empty,
    };

    fn add(self: *Builder, parent: ?u32, name: []const u8, text: ?u32) !u32 {
        const index: u32 = @intCast(self.items.items.len);
        try self.items.append(self.allocator, .{ .name = name, .text = text });
        const list = if (parent) |folder_index| &self.items.items[folder_index].children else &self.roots;
        try list.append(self.allocator, index);
        return index;
    }

    /// The folder `name` below `parent`, made when it is not there yet. The
    /// keys are sorted, but a folder's texts need not be adjacent (`a/b`,
    /// `a-c`, `a/d`), so this looks among the children.
    fn folder(self: *Builder, parent: u32, name: []const u8) !u32 {
        for (self.items.items[parent].children.items) |index| {
            const item = self.items.items[index];
            if (item.text == null and std.mem.eql(u8, item.name, name)) return index;
        }
        return self.add(parent, name, null);
    }

    /// Writes `item`'s subtree depth first from `written`; returns its state,
    /// the lowest of its texts' (FillFolder's return value).
    fn flatten(self: *Builder, item_index: u32, parent: ?u32, depth: u16, nodes: []Node, node_of_text: []u32, written: *u32) State {
        const item = &self.items.items[item_index];
        const at = written.*;
        written.* += 1;
        std.mem.sort(u32, item.children.items, self, byName);
        var lowest = item.state;
        for (item.children.items) |child_index| {
            const child_state = self.flatten(child_index, at, depth + 1, nodes, node_of_text, written);
            lowest = @enumFromInt(@min(@intFromEnum(lowest), @intFromEnum(child_state)));
        }
        nodes[at] = .{ .name = item.name, .depth = depth, .parent = parent, .end = written.*, .text = item.text, .state = lowest, .changed = item.changed };
        if (item.text) |text| node_of_text[text] = at;
        return lowest;
    }

    fn byName(self: *Builder, a: u32, b: u32) bool {
        return std.ascii.lessThanIgnoreCase(self.items.items[a].name, self.items.items[b].name);
    }
};

const testing = std.testing;
const utf16 = database_mod.utf16;

fn stateFile(comptime state: State) []const u8 {
    return std.fmt.comptimePrint("<?xml version=\"1.0\"?>\r\n<base State=\"{d}\" Changed=\"1\"/>\r\n", .{@intFromEnum(state)});
}

test "folders take the lowest state below them and follow a text's change up" {
    const io = testing.io;
    const root_folder = try database_mod.freshScratch(io, "tree");
    defer testing.allocator.free(root_folder);
    try database_mod.writeFixture(io, root_folder, "elk.xml", "<?xml version=\"1.0\"?>\r\n<base><Elements><item><Path>game</Path><Version>7</Version></item></Elements></base>\r\n");
    try database_mod.writeFixture(io, root_folder, "game_data_base/B/one.elk", utf16("1"));
    try database_mod.writeFixture(io, root_folder, "game_data_base/B/one.xml", stateFile(.approved));
    try database_mod.writeFixture(io, root_folder, "game_data_base/B/two.elk", utf16("2"));
    try database_mod.writeFixture(io, root_folder, "game_data_base/B/two.xml", stateFile(.translated));
    try database_mod.writeFixture(io, root_folder, "game_data_base/a.elk", utf16("a"));
    try database_mod.writeFixture(io, root_folder, "game_data_base/a.xml", stateFile(.approved));
    try database_mod.writeFixture(io, root_folder, "game_data_base/c/d/e.elk", utf16("e"));
    const elk_path = try std.fs.path.join(testing.allocator, &.{ root_folder, "elk.xml" });
    defer testing.allocator.free(elk_path);
    var database = try Database.open(testing.allocator, io, elk_path);
    defer database.deinit();
    var tree = try Tree.build(testing.allocator, &database);
    defer tree.deinit();

    // a, B (one, two), c (d (e)): case-insensitive order, depth first.
    const names = [_][]const u8{ ", VERSION: 7", "a", "B", "one", "two", "c", "d", "e" };
    try testing.expectEqual(names.len, tree.nodes.len);
    for (names, tree.nodes) |name, node| try testing.expectEqualStrings(name, node.name);
    try testing.expectEqual(@as(u32, 5), tree.nodes[2].end);
    try testing.expectEqual(@as(u16, 3), tree.nodes[7].depth);

    // e has no state file: Not translated, and so are its folders and the root.
    const b = tree.findFolder(0, "b").?;
    try testing.expectEqual(State.translated, tree.nodes[b].state);
    try testing.expectEqual(State.not_translated, tree.nodes[tree.findFolder(0, "c/d").?].state);
    try testing.expectEqual(State.not_translated, tree.nodes[0].state);

    const e = tree.nodes[tree.findText(0, "c/d/e").?].text.?;
    tree.setTextState(e, .approved);
    try testing.expectEqual(State.approved, tree.nodes[tree.findFolder(0, "c").?].state);
    try testing.expectEqual(State.translated, tree.nodes[0].state);
    const two = tree.nodes[tree.findText(0, "B/two").?].text.?;
    tree.setTextState(two, .approved);
    try testing.expectEqual(State.approved, tree.nodes[b].state);
    try testing.expectEqual(State.approved, tree.nodes[0].state);
    tree.setTextState(two, .outdated);
    try testing.expectEqual(State.outdated, tree.nodes[0].state);
}

test "the tracked database's tree holds every text under its folders" {
    const io = testing.io;
    var database = try Database.open(testing.allocator, io, "Data/ELK/elk.xml");
    defer database.deinit();
    var tree = try Tree.build(testing.allocator, &database);
    defer tree.deinit();
    try testing.expectEqual(@as(usize, 2852), tree.texts.len);
    try testing.expectEqualStrings("Game Resources, VERSION: ", tree.nodes[0].name[0.."Game Resources, VERSION: ".len]);
    const medals = tree.findFolder(0, "medals/1").?;
    const name = tree.child(medals, "name", true).?;
    try testing.expectEqual(name, tree.findText(0, "medals/1/name").?);
    try testing.expectEqualStrings("medals/1/name", tree.texts[tree.nodes[name].text.?].key);
    // Every node is inside its parent's subtree, and a folder's state is
    // the lowest of its children's.
    for (tree.nodes, 0..) |node, index| {
        if (node.parent) |parent| {
            try testing.expect(parent < index and index < tree.nodes[parent].end);
            try testing.expect(@intFromEnum(tree.nodes[parent].state) <= @intFromEnum(node.state));
        }
        if (!node.isText()) try testing.expectEqual(tree.lowestChild(@intCast(index)), node.state);
    }
}

test "browsing walks the texts in the tree's order through a filter" {
    const io = testing.io;
    var database = try Database.open(testing.allocator, io, "Data/ELK/elk.xml");
    defer database.deinit();
    var tree = try Tree.build(testing.allocator, &database);
    defer tree.deinit();

    // No filter: First and Last are the tree's first and last texts, and
    // Next and Previous step over folders to the neighbouring text.
    const first = tree.browse(.first, null, null).?;
    var first_text: u32 = 0;
    while (!tree.nodes[first_text].isText()) first_text += 1;
    try testing.expectEqual(tree.nodes[first_text].text.?, first);
    try testing.expectEqual(@as(?u32, null), tree.browse(.previous, first, null));
    try testing.expectEqual(@as(?u32, null), tree.browse(.next, null, null));
    const second = tree.browse(.next, first, null).?;
    try testing.expectEqual(first, tree.browse(.previous, second, null).?);
    const last = tree.browse(.last, null, null).?;
    try testing.expectEqual(@as(?u32, null), tree.browse(.next, last, null));

    // The four Translated texts of Data/ELK, in the tree's order.
    const translated = filter_mod.built_in[3];
    const walk = [_][]const u8{
        "scenarios/descriptions/defend_area00/1h",
        "scenarios/descriptions/defend_area00/header",
        "scenarios/descriptions/escort00/0h",
        "scenarios/descriptions/sa_10_12_13_15_16/0",
    };
    var at = tree.browse(.first, first, translated).?;
    for (walk, 0..) |key, step| {
        try testing.expectEqualStrings(key, tree.texts[at].key);
        if (step + 1 < walk.len) at = tree.browse(.next, at, translated).?;
    }
    try testing.expectEqual(@as(?u32, null), tree.browse(.next, at, translated));
    try testing.expectEqual(at, tree.browse(.last, null, translated).?);
    try testing.expectEqualStrings(walk[2], tree.texts[tree.browse(.previous, at, translated).?].key);
    // From a text the filter rejects, Next finds the next one that passes.
    const medal = tree.nodes[tree.findText(0, "medals/1/name").?].text.?;
    try testing.expect(!tree.textMatches(medal, translated));
    try testing.expectEqualStrings(walk[0], tree.texts[tree.browse(.next, medal, translated).?].key);

    // The marks: the texts that pass and every folder above them.
    const marks = try testing.allocator.alloc(bool, tree.nodes.len);
    defer testing.allocator.free(marks);
    tree.markMatches(translated, marks);
    var passing: usize = 0;
    for (tree.nodes, marks) |node, mark| {
        if (node.isText() and mark) passing += 1;
    }
    try testing.expectEqual(walk.len, passing);
    try testing.expect(marks[0]);
    try testing.expect(marks[tree.findFolder(0, "scenarios/descriptions/escort00").?]);
    try testing.expect(!marks[tree.findFolder(0, "medals").?]);
    tree.markMatches(null, marks);
    for (marks) |mark| try testing.expect(mark);

    // A state set by hand moves the text into the filter's walk.
    tree.setTextState(medal, .translated);
    try testing.expect(tree.nodes[tree.node_of_text[medal]].changed);
    try testing.expectEqual(medal, tree.browse(.first, null, translated).?);
}
