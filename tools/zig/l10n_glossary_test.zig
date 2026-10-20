//! Structure check of the nine glossaries in `Localization/glossary` (M002 S02 T03): every file
//! has the same headings and table header, the same row counts per section, and CRLF endings,
//! so the S03 checker and the S08 documents can read them alike.
const std = @import("std");
const testing = std.testing;

const codes = [_][]const u8{ "ru", "zh-Hans", "ja", "hi", "es", "fr", "de", "pt-BR", "id" };
const sections = [_][]const u8{ "## Unit types", "## Ranks", "## Interface" };
const table_header = "| English term | Translation | Note |";

test "every glossary has the three sections, the table header and CRLF lines" {
    var reference_rows: [sections.len]usize = undefined;
    for (codes, 0..) |code, ci| {
        const path = try std.fmt.allocPrint(testing.allocator, "Localization/glossary/{s}.md", .{code});
        defer testing.allocator.free(path);
        const bytes = try std.Io.Dir.cwd().readFileAlloc(testing.io, path, testing.allocator, .limited(1 << 20));
        defer testing.allocator.free(bytes);
        try testing.expect(std.mem.startsWith(u8, bytes, "# Glossary: "));
        var bare_lf: usize = 0;
        for (bytes, 0..) |c, i| {
            if (c == '\n' and (i == 0 or bytes[i - 1] != '\r')) bare_lf += 1;
        }
        try testing.expectEqual(@as(usize, 0), bare_lf);
        try testing.expect(std.mem.indexOf(u8, bytes, "Review state:") != null);
        var section: ?usize = null;
        var rows: [sections.len]usize = @splat(0);
        var headers: usize = 0;
        var lines = std.mem.splitSequence(u8, bytes, "\r\n");
        while (lines.next()) |line| {
            for (sections, 0..) |s, si| {
                if (std.mem.eql(u8, line, s)) section = si;
            }
            if (std.mem.eql(u8, line, table_header)) headers += 1;
            if (section) |si| {
                if (std.mem.startsWith(u8, line, "| ") and !std.mem.eql(u8, line, table_header)) rows[si] += 1;
            }
        }
        try testing.expectEqual(sections.len, headers);
        for (rows, 0..) |n, si| {
            try testing.expect(n > 0);
            if (ci == 0) reference_rows[si] = n else try testing.expectEqual(reference_rows[si], n);
        }
    }
}
