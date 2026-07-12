const std = @import("std");

// Deliberately small, allocation-owned XML reader for the legacy Data XML.
// It accepts declarations, comments, quoted attributes, nested elements, and
// UTF-8 text.  Slices point into the caller-owned source buffer.
pub const Attribute = struct { name: []const u8, value: []const u8 };

pub const Node = struct {
    name: []const u8,
    attributes: std.ArrayListUnmanaged(Attribute) = .empty,
    children: std.ArrayListUnmanaged(*Node) = .empty,
    text: []const u8 = "",
};

pub const Document = struct {
    root: *Node,
    allocator: std.mem.Allocator,

    pub fn deinit(self: *Document) void {
        destroyNode(self.allocator, self.root);
    }
};

const Parser = struct {
    bytes: []const u8,
    pos: usize = 0,
    allocator: std.mem.Allocator,

    fn skipSpace(self: *Parser) void {
        while (self.pos < self.bytes.len and std.ascii.isWhitespace(self.bytes[self.pos])) : (self.pos += 1) {}
    }

    fn starts(self: *const Parser, value: []const u8) bool {
        return self.pos + value.len <= self.bytes.len and std.mem.eql(u8, self.bytes[self.pos .. self.pos + value.len], value);
    }

    fn skipUntil(self: *Parser, marker: []const u8) !void {
        const found = std.mem.indexOfPos(u8, self.bytes, self.pos, marker) orelse return error.MalformedXml;
        self.pos = found + marker.len;
    }

    fn skipPreamble(self: *Parser) !void {
        while (true) {
            self.skipSpace();
            if (self.starts("<?")) {
                self.pos += 2;
                try self.skipUntil("?>");
            } else if (self.starts("<!--")) {
                self.pos += 4;
                try self.skipUntil("-->");
            } else break;
        }
    }

    fn parseName(self: *Parser) ![]const u8 {
        const start = self.pos;
        while (self.pos < self.bytes.len) : (self.pos += 1) {
            const c = self.bytes[self.pos];
            if (!(std.ascii.isAlphanumeric(c) or c == '_' or c == '-' or c == ':' or c == '.')) break;
        }
        if (start == self.pos) return error.MalformedXml;
        return self.bytes[start..self.pos];
    }

    fn parseElement(self: *Parser) !*Node {
        try self.skipPreamble();
        if (self.pos >= self.bytes.len or self.bytes[self.pos] != '<') return error.MalformedXml;
        self.pos += 1;
        const name = try self.parseName();
        const node = try self.allocator.create(Node);
        node.* = .{ .name = name };
        errdefer destroyNode(self.allocator, node);

        while (true) {
            self.skipSpace();
            if (self.starts("/>")) { self.pos += 2; return node; }
            if (self.pos >= self.bytes.len or self.bytes[self.pos] == '>') break;
            const attribute_name = try self.parseName();
            self.skipSpace();
            if (self.pos >= self.bytes.len or self.bytes[self.pos] != '=') return error.MalformedXml;
            self.pos += 1;
            self.skipSpace();
            if (self.pos >= self.bytes.len or (self.bytes[self.pos] != '\'' and self.bytes[self.pos] != '"')) return error.MalformedXml;
            const quote = self.bytes[self.pos];
            self.pos += 1;
            const value_start = self.pos;
            while (self.pos < self.bytes.len and self.bytes[self.pos] != quote) : (self.pos += 1) {}
            if (self.pos == self.bytes.len) return error.MalformedXml;
            try node.attributes.append(self.allocator, .{ .name = attribute_name, .value = self.bytes[value_start..self.pos] });
            self.pos += 1;
        }
        if (self.pos >= self.bytes.len) return error.MalformedXml;
        self.pos += 1;

        while (true) {
            if (self.pos >= self.bytes.len) return error.MalformedXml;
            if (self.starts("</")) {
                self.pos += 2;
                const close_name = try self.parseName();
                if (!std.mem.eql(u8, close_name, name)) return error.MalformedXml;
                self.skipSpace();
                if (self.pos >= self.bytes.len or self.bytes[self.pos] != '>') return error.MalformedXml;
                self.pos += 1;
                return node;
            }
            if (self.starts("<!--")) { self.pos += 4; try self.skipUntil("-->"); continue; }
            if (self.bytes[self.pos] == '<') {
                const child_node = try self.parseElement();
                try node.children.append(self.allocator, child_node);
            } else {
                const start = self.pos;
                while (self.pos < self.bytes.len and self.bytes[self.pos] != '<') : (self.pos += 1) {}
                const value = std.mem.trim(u8, self.bytes[start..self.pos], " \t\r\n");
                if (value.len != 0) node.text = value;
            }
        }
    }
};

pub fn parse(allocator: std.mem.Allocator, bytes: []const u8) !Document {
    var parser = Parser{ .bytes = bytes, .allocator = allocator };
    return .{ .root = try parser.parseElement(), .allocator = allocator };
}

pub fn child(node: *const Node, name: []const u8) ?*Node {
    for (node.children.items) |candidate| if (std.mem.eql(u8, candidate.name, name)) return candidate;
    return null;
}

pub fn attribute(node: *const Node, name: []const u8) ?[]const u8 {
    for (node.attributes.items) |candidate| if (std.mem.eql(u8, candidate.name, name)) return candidate.value;
    return null;
}

fn destroyNode(allocator: std.mem.Allocator, node: *Node) void {
    for (node.children.items) |child_node| destroyNode(allocator, child_node);
    node.children.deinit(allocator);
    node.attributes.deinit(allocator);
    allocator.destroy(node);
}

test "parses nested XML with attributes and comments" {
    var document = try parse(std.testing.allocator, "<?xml version=\"1.0\"?><base><!-- c --><item value=\"42\"> text </item></base>");
    defer document.deinit();
    const item = child(document.root, "item").?;
    try std.testing.expectEqualStrings("42", attribute(item, "value").?);
    try std.testing.expectEqualStrings("text", item.text);
}
