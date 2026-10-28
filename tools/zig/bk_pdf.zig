//! A small PDF writer for bk-views' review documents: pages of vector paths
//! and text in the standard Helvetica fonts, RGBA pictures as Flate images
//! with a soft mask, and a flat outline (bookmarks). Std-only Zig, PDF 1.4.

const std = @import("std");

pub const Pdf = struct {
    gpa: std.mem.Allocator,
    /// Each object's body, numbered from 1 in order.
    objects: std.ArrayList([]const u8) = .empty,
    pages: std.ArrayList(u32) = .empty,
    marks: std.ArrayList(struct { title: []const u8, page: u32 }) = .empty,

    pub fn init(gpa: std.mem.Allocator) Pdf {
        return .{ .gpa = gpa };
    }

    fn add(pdf: *Pdf, body: []const u8) !u32 {
        try pdf.objects.append(pdf.gpa, body);
        return @intCast(pdf.objects.items.len);
    }

    fn reserve(pdf: *Pdf) !u32 {
        return pdf.add("");
    }

    fn stream(pdf: *Pdf, dict: []const u8, data: []const u8) !u32 {
        const packed_data = try deflate(pdf.gpa, data);
        return pdf.add(try std.fmt.allocPrint(pdf.gpa, "<< {s} /Filter /FlateDecode /Length {d} >>\nstream\n{s}\nendstream", .{ dict, packed_data.len, packed_data }));
    }

    /// A picture (8-bit RGBA, rows top down) as an image XObject; its alpha
    /// is the soft mask. Returns the name's number for `Page.image`.
    pub fn image(pdf: *Pdf, width: u32, height: u32, rgba: []const [4]u8) !u32 {
        const rgb = try pdf.gpa.alloc(u8, rgba.len * 3);
        const alpha = try pdf.gpa.alloc(u8, rgba.len);
        for (rgba, 0..) |p, i| {
            rgb[i * 3 ..][0..3].* = p[0..3].*;
            alpha[i] = p[3];
        }
        const mask = try pdf.stream(try std.fmt.allocPrint(pdf.gpa, "/Type /XObject /Subtype /Image /Width {d} /Height {d} /ColorSpace /DeviceGray /BitsPerComponent 8", .{ width, height }), alpha);
        return pdf.stream(try std.fmt.allocPrint(pdf.gpa, "/Type /XObject /Subtype /Image /Width {d} /Height {d} /ColorSpace /DeviceRGB /BitsPerComponent 8 /SMask {d} 0 R", .{ width, height, mask }), rgb);
    }

    /// Adds a page of `width` x `height` points drawn by `content` (a page's
    /// content stream, Page's text); `images` are the image numbers it uses,
    /// each named /Im<number>. Returns the page's index.
    pub fn page(pdf: *Pdf, width: f64, height: f64, content: []const u8, images: []const u32) !u32 {
        var xobjects: std.Io.Writer.Allocating = .init(pdf.gpa);
        for (images) |i| try xobjects.writer.print("/Im{d} {d} 0 R ", .{ i, i });
        const contents = try pdf.stream("", content);
        // The page tree is object 1, the fonts 2 and 3 (written by `write`).
        const id = try pdf.add(try std.fmt.allocPrint(pdf.gpa, "<< /Type /Page /Parent 1 0 R /MediaBox [0 0 {d:.0} {d:.0}] /Resources << /Font << /F1 2 0 R /F2 3 0 R >> /XObject << {s}>> >> /Contents {d} 0 R >>", .{ width, height, xobjects.written(), contents }));
        try pdf.pages.append(pdf.gpa, id);
        return @intCast(pdf.pages.items.len - 1);
    }

    /// A bookmark to page `index`.
    pub fn bookmark(pdf: *Pdf, title: []const u8, index: u32) !void {
        try pdf.marks.append(pdf.gpa, .{ .title = title, .page = index });
    }

    pub fn write(pdf: *Pdf, w: *std.Io.Writer) !void {
        // Objects 1 to 3 are the page tree and the fonts, filled in here.
        var kids: std.Io.Writer.Allocating = .init(pdf.gpa);
        for (pdf.pages.items) |p| try kids.writer.print("{d} 0 R ", .{p});
        pdf.objects.items[0] = try std.fmt.allocPrint(pdf.gpa, "<< /Type /Pages /Kids [{s}] /Count {d} >>", .{ kids.written(), pdf.pages.items.len });
        pdf.objects.items[1] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>";
        pdf.objects.items[2] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>";
        var outlines: ?u32 = null;
        if (pdf.marks.items.len > 0) {
            const root = try pdf.reserve();
            const first: u32 = @intCast(pdf.objects.items.len + 1);
            const n: u32 = @intCast(pdf.marks.items.len);
            for (pdf.marks.items, 0..) |m, i| {
                const at: u32 = first + @as(u32, @intCast(i));
                var title: std.Io.Writer.Allocating = .init(pdf.gpa);
                try escape(&title.writer, m.title);
                _ = try pdf.add(try std.fmt.allocPrint(pdf.gpa, "<< /Title ({s}) /Parent {d} 0 R {s}{s}/Dest [{d} 0 R /Fit] >>", .{
                    title.written(),
                    root,
                    if (i > 0) try std.fmt.allocPrint(pdf.gpa, "/Prev {d} 0 R ", .{at - 1}) else "",
                    if (i + 1 < n) try std.fmt.allocPrint(pdf.gpa, "/Next {d} 0 R ", .{at + 1}) else "",
                    pdf.pages.items[m.page],
                }));
            }
            pdf.objects.items[root - 1] = try std.fmt.allocPrint(pdf.gpa, "<< /Type /Outlines /First {d} 0 R /Last {d} 0 R /Count {d} >>", .{ first, first + n - 1, n });
            outlines = root;
        }
        const catalog = try pdf.add(if (outlines) |o| try std.fmt.allocPrint(pdf.gpa, "<< /Type /Catalog /Pages 1 0 R /Outlines {d} 0 R /PageMode /UseOutlines >>", .{o}) else "<< /Type /Catalog /Pages 1 0 R >>");
        var offset: usize = 0;
        const header = "%PDF-1.4\n%\xe2\xe3\xcf\xd3\n";
        try w.writeAll(header);
        offset += header.len;
        const offsets = try pdf.gpa.alloc(usize, pdf.objects.items.len);
        for (pdf.objects.items, offsets, 1..) |body, *o, i| {
            o.* = offset;
            const head = try std.fmt.allocPrint(pdf.gpa, "{d} 0 obj\n", .{i});
            try w.writeAll(head);
            try w.writeAll(body);
            try w.writeAll("\nendobj\n");
            offset += head.len + body.len + "\nendobj\n".len;
        }
        try w.print("xref\n0 {d}\n0000000000 65535 f \n", .{pdf.objects.items.len + 1});
        for (offsets) |o| try w.print("{d:0>10} 00000 n \n", .{o});
        try w.print("trailer\n<< /Size {d} /Root {d} 0 R >>\nstartxref\n{d}\n%%EOF\n", .{ pdf.objects.items.len + 1, catalog, offset });
    }
};

/// Starts a document: the page tree and the two fonts are objects 1 to 3.
pub fn begin(gpa: std.mem.Allocator) !Pdf {
    var pdf: Pdf = .init(gpa);
    for (0..3) |_| _ = try pdf.reserve();
    return pdf;
}

/// A page's content stream: paths in points from the bottom left, text in
/// Helvetica (F1) or Helvetica-Bold (F2).
pub const Page = struct {
    out: std.Io.Writer.Allocating,

    pub fn init(gpa: std.mem.Allocator) Page {
        return .{ .out = .init(gpa) };
    }

    pub fn text(p: *Page, x: f64, y: f64, size: f64, bold: bool, rgb: [3]f64, s: []const u8) !void {
        const w = &p.out.writer;
        try w.print("BT /{s} {d:.1} Tf {d:.3} {d:.3} {d:.3} rg {d:.2} {d:.2} Td (", .{ if (bold) "F2" else "F1", size, rgb[0], rgb[1], rgb[2], x, y });
        try escape(w, s);
        try w.writeAll(") Tj ET\n");
    }

    /// A closed polygon, filled (`fill`), stroked (`stroke`) or both.
    pub fn polygon(p: *Page, points: []const [2]f64, fill: ?[3]f64, stroke: ?[3]f64, width: f64) !void {
        if (points.len < 2) return;
        const w = &p.out.writer;
        if (fill) |c| try w.print("{d:.3} {d:.3} {d:.3} rg ", .{ c[0], c[1], c[2] });
        if (stroke) |c| try w.print("{d:.3} {d:.3} {d:.3} RG {d:.2} w 1 j ", .{ c[0], c[1], c[2], width });
        for (points, 0..) |q, i| try w.print("{d:.2} {d:.2} {s} ", .{ q[0], q[1], if (i == 0) "m" else "l" });
        try w.writeAll(if (fill != null and stroke != null) "b\n" else if (fill != null) "f\n" else "s\n");
    }

    pub fn line(p: *Page, a: [2]f64, b: [2]f64, rgb: [3]f64, width: f64) !void {
        try p.out.writer.print("{d:.3} {d:.3} {d:.3} RG {d:.2} w 1 J {d:.2} {d:.2} m {d:.2} {d:.2} l S\n", .{ rgb[0], rgb[1], rgb[2], width, a[0], a[1], b[0], b[1] });
    }

    /// Image number `id` (`Pdf.image`) at (x, y) bottom left, w x h points.
    pub fn image(p: *Page, id: u32, x: f64, y: f64, w: f64, h: f64) !void {
        try p.out.writer.print("q {d:.3} 0 0 {d:.3} {d:.3} {d:.3} cm /Im{d} Do Q\n", .{ w, h, x, y, id });
    }
};

fn escape(w: *std.Io.Writer, s: []const u8) !void {
    for (s) |c| {
        if (c == '(' or c == ')' or c == '\\') try w.writeByte('\\');
        try w.writeByte(if (c < 32 or c > 126) '?' else c);
    }
}

fn deflate(gpa: std.mem.Allocator, data: []const u8) ![]u8 {
    var out: std.Io.Writer.Allocating = try .initCapacity(gpa, 64 * 1024);
    const window = try gpa.alloc(u8, std.compress.flate.max_window_len);
    defer gpa.free(window);
    var c = try std.compress.flate.Compress.init(&out.writer, window, .zlib, .default);
    try c.writer.writeAll(data);
    try c.finish();
    return out.toOwnedSlice();
}

test "a page with a path, text and a picture is a PDF a reader can find its parts in" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    const gpa = arena.allocator();
    var pdf = try begin(gpa);
    var pixels = [_][4]u8{ .{ 255, 0, 0, 255 }, .{ 0, 255, 0, 0 } };
    const im = try pdf.image(2, 1, &pixels);
    var p: Page = .init(gpa);
    try p.text(10, 10, 12, true, .{ 0, 0, 0 }, "33.N (a)");
    try p.polygon(&.{ .{ 0, 0 }, .{ 10, 0 }, .{ 0, 10 } }, .{ 1, 0, 0 }, .{ 0, 0, 0 }, 1);
    try p.image(im, 0, 0, 20, 10);
    _ = try pdf.page(100, 100, p.out.written(), &.{im});
    try pdf.bookmark("33 test", 0);
    var out: std.Io.Writer.Allocating = .init(gpa);
    try pdf.write(&out.writer);
    const s = out.written();
    try std.testing.expect(std.mem.startsWith(u8, s, "%PDF-1.4"));
    try std.testing.expect(std.mem.endsWith(u8, s, "%%EOF\n"));
    try std.testing.expect(std.mem.indexOf(u8, s, "/Count 1") != null);
    try std.testing.expect(std.mem.indexOf(u8, s, "/Outlines") != null);
    // Every xref offset points at its object.
    const xref = std.mem.lastIndexOf(u8, s, "xref\n").?;
    var lines = std.mem.splitScalar(u8, s[xref..], '\n');
    _ = lines.next();
    _ = lines.next();
    _ = lines.next();
    var i: usize = 1;
    while (lines.next()) |l| : (i += 1) {
        if (l.len < 18 or l[17] != 'n') break;
        const at = try std.fmt.parseInt(usize, l[0..10], 10);
        try std.testing.expect(std.mem.startsWith(u8, s[at..], try std.fmt.allocPrint(gpa, "{d} 0 obj", .{i})));
    }
}
