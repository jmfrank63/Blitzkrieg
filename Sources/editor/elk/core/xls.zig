//! The legacy `.xls` reader (D-10): read only, so the sheets translators
//! made with the MFC ELK's Excel export still import. It reads what those
//! sheets hold and nothing more, as measured on the tracked
//! `Data/ELK/desc.xls` (an Excel 97-2003 BIFF8 workbook, sheet
//! `BlitzkriegELK`, 154 rows, every cell a `LABELSST`, one `SST` continued
//! once):
//!
//! - The OLE compound file: the header, the FAT through the header's 109
//!   DIFAT slots (a FAT of up to 109 sectors, a 6.8 MB file at 512-byte
//!   sectors), the directory, and the `Workbook` stream through the FAT.
//!   A file needing more DIFAT sectors, or whose workbook stream lies in the
//!   mini stream (Excel pads it past 4,096 bytes so it never does), fails
//!   with its own error.
//! - The workbook globals: `BOF` (BIFF8 only; a BIFF5 `Book` stream fails),
//!   `BOUNDSHEET` for the sheet `BlitzkriegELK`, `SST` with its `CONTINUE`
//!   records (8-bit and 16-bit characters, a string split across records,
//!   the rich-text and phonetic data skipped).
//! - The sheet: `LABELSST` cells; `RK` and `NUMBER`, a number typed into a
//!   cell, as its shortest decimal text, which is how the Excel driver
//!   handed a number to a text column; `BLANK` and `MULBLANK` (formatting
//!   only) as empty cells. Any other cell record (`LABEL`, `RSTRING`,
//!   `FORMULA`, `MULRK`, `BOOLERR`) fails with `UnsupportedXlsCell` rather
//!   than being guessed at.
//!
//! The backtick swap the MFC export applied is undone by the caller
//! (sheet.zig), since only this format carries it.
const std = @import("std");

pub const sheet_name = "BlitzkriegELK";

pub const Grid = []const []const []const u8;

pub const Error = error{
    /// Not a compound file, or one whose sectors or records run out.
    InvalidXls,
    /// A FAT beyond the header's 109 DIFAT slots.
    XlsTooLarge,
    /// The workbook stream is in the mini stream.
    XlsWorkbookInMiniStream,
    /// An Excel 5/95 (BIFF5) or older workbook.
    NotBiff8,
    NoBlitzkriegElkSheet,
    /// A cell record the ELK's sheets never held.
    UnsupportedXlsCell,
};

const signature = [_]u8{ 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
const end_of_chain: u32 = 0xFFFFFFFE;
const free_sector: u32 = 0xFFFFFFFF;

/// The rows of the sheet `BlitzkriegELK`, every slice in `arena`.
pub fn read(arena: std.mem.Allocator, bytes: []const u8) !Grid {
    const stream = try workbookStream(arena, bytes);
    return readWorkbook(arena, stream);
}

/// The `Workbook` stream of the compound file `bytes`.
fn workbookStream(arena: std.mem.Allocator, bytes: []const u8) ![]const u8 {
    if (bytes.len < 512 or !std.mem.eql(u8, bytes[0..8], &signature)) return Error.InvalidXls;
    const shift = std.mem.readInt(u16, bytes[0x1E..0x20], .little);
    if (shift != 9 and shift != 12) return Error.InvalidXls;
    const sector_size = @as(usize, 1) << @intCast(shift);
    const fat_sectors = std.mem.readInt(u32, bytes[0x2C..0x30], .little);
    const first_directory = std.mem.readInt(u32, bytes[0x30..0x34], .little);
    const mini_cutoff = std.mem.readInt(u32, bytes[0x38..0x3C], .little);
    if (fat_sectors > 109) return Error.XlsTooLarge;

    const entries_per_sector = sector_size / 4;
    const fat = try arena.alloc(u32, fat_sectors * entries_per_sector);
    for (0..fat_sectors) |index| {
        const sector = std.mem.readInt(u32, bytes[0x4C + index * 4 ..][0..4], .little);
        const data = try sectorBytes(bytes, sector, sector_size);
        for (0..entries_per_sector) |at| fat[index * entries_per_sector + at] = std.mem.readInt(u32, data[at * 4 ..][0..4], .little);
    }

    const directory = try chain(arena, bytes, fat, first_directory, sector_size, null);
    var at: usize = 0;
    while (at + 128 <= directory.len) : (at += 128) {
        const entry = directory[at..][0..128];
        const name_bytes = std.mem.readInt(u16, entry[0x40..0x42], .little);
        if (entry[0x42] != 2 or name_bytes < 2 or name_bytes > 64) continue;
        var name: [32]u8 = undefined;
        const len = name_bytes / 2 - 1;
        for (0..len) |unit| {
            const value = std.mem.readInt(u16, entry[unit * 2 ..][0..2], .little);
            name[unit] = if (value < 0x80) @intCast(value) else '?';
        }
        const stream_name = name[0..len];
        if (std.ascii.eqlIgnoreCase(stream_name, "Book")) return Error.NotBiff8;
        if (!std.ascii.eqlIgnoreCase(stream_name, "Workbook")) continue;
        const start = std.mem.readInt(u32, entry[0x74..0x78], .little);
        const size = std.mem.readInt(u32, entry[0x78..0x7C], .little);
        if (size < mini_cutoff) return Error.XlsWorkbookInMiniStream;
        return chain(arena, bytes, fat, start, sector_size, size);
    }
    return Error.NotBiff8;
}

fn sectorBytes(bytes: []const u8, sector: u32, sector_size: usize) ![]const u8 {
    const offset = (@as(u64, sector) + 1) * sector_size;
    if (offset + sector_size > bytes.len) return Error.InvalidXls;
    return bytes[@intCast(offset)..][0..sector_size];
}

/// The sectors of the chain from `first`, cut to `size` when given. A chain
/// that loops or leaves the FAT is broken.
fn chain(arena: std.mem.Allocator, bytes: []const u8, fat: []const u32, first: u32, sector_size: usize, size: ?u32) ![]const u8 {
    var out: std.ArrayListUnmanaged(u8) = .empty;
    var sector = first;
    var steps: usize = 0;
    while (sector != end_of_chain) : (steps += 1) {
        if (sector >= fat.len or steps > fat.len) return Error.InvalidXls;
        try out.appendSlice(arena, try sectorBytes(bytes, sector, sector_size));
        if (size) |wanted| if (out.items.len >= wanted) break;
        sector = fat[sector];
        if (sector == free_sector) return Error.InvalidXls;
    }
    if (size) |wanted| {
        if (out.items.len < wanted) return Error.InvalidXls;
        return out.items[0..wanted];
    }
    return out.items;
}

const Record = struct { kind: u16, data: []const u8 };

const Records = struct {
    stream: []const u8,
    at: usize,

    fn next(self: *Records) !?Record {
        if (self.at >= self.stream.len) return null;
        if (self.at + 4 > self.stream.len) return Error.InvalidXls;
        const kind = std.mem.readInt(u16, self.stream[self.at..][0..2], .little);
        const len = std.mem.readInt(u16, self.stream[self.at + 2 ..][0..2], .little);
        const start = self.at + 4;
        if (start + len > self.stream.len) return Error.InvalidXls;
        self.at = start + len;
        return .{ .kind = kind, .data = self.stream[start..self.at] };
    }

    fn peekKind(self: *const Records) ?u16 {
        if (self.at + 4 > self.stream.len) return null;
        return std.mem.readInt(u16, self.stream[self.at..][0..2], .little);
    }
};

const record_bof: u16 = 0x0809;
const record_eof: u16 = 0x000A;
const record_boundsheet: u16 = 0x0085;
const record_sst: u16 = 0x00FC;
const record_continue: u16 = 0x003C;
const record_labelsst: u16 = 0x00FD;
const record_rk: u16 = 0x027E;
const record_number: u16 = 0x0203;
const record_blank: u16 = 0x0201;
const record_mulblank: u16 = 0x00BE;
const unsupported_cells = [_]u16{ 0x0204, 0x00D6, 0x0006, 0x00BD, 0x0205 };

fn readWorkbook(arena: std.mem.Allocator, stream: []const u8) !Grid {
    var records: Records = .{ .stream = stream, .at = 0 };
    const bof = try records.next() orelse return Error.InvalidXls;
    if (bof.kind != record_bof or bof.data.len < 2 or std.mem.readInt(u16, bof.data[0..2], .little) != 0x0600) return Error.NotBiff8;
    var sheet_offset: ?u32 = null;
    var strings: []const []const u8 = &.{};
    while (try records.next()) |record| {
        switch (record.kind) {
            record_eof => break,
            record_boundsheet => {
                if (record.data.len < 8) return Error.InvalidXls;
                const name = try shortString(arena, record.data[6..]);
                if (sheet_offset == null and record.data[5] == 0 and std.ascii.eqlIgnoreCase(name, sheet_name)) sheet_offset = std.mem.readInt(u32, record.data[0..4], .little);
            },
            record_sst => {
                var parts: std.ArrayListUnmanaged([]const u8) = .empty;
                try parts.append(arena, record.data);
                while (records.peekKind() == record_continue) try parts.append(arena, (try records.next()).?.data);
                strings = try sharedStrings(arena, parts.items);
            },
            else => {},
        }
    }
    const offset = sheet_offset orelse return Error.NoBlitzkriegElkSheet;
    return readSheet(arena, stream, offset, strings);
}

fn readSheet(arena: std.mem.Allocator, stream: []const u8, offset: u32, strings: []const []const u8) !Grid {
    var records: Records = .{ .stream = stream, .at = offset };
    const bof = try records.next() orelse return Error.InvalidXls;
    if (bof.kind != record_bof) return Error.InvalidXls;
    var rows: std.ArrayListUnmanaged(std.ArrayListUnmanaged([]const u8)) = .empty;
    while (try records.next()) |record| {
        if (record.kind == record_eof) break;
        if (std.mem.indexOfScalar(u16, &unsupported_cells, record.kind) != null) return Error.UnsupportedXlsCell;
        const value: []const u8 = switch (record.kind) {
            record_labelsst => value: {
                if (record.data.len < 10) return Error.InvalidXls;
                const index = std.mem.readInt(u32, record.data[6..10], .little);
                if (index >= strings.len) return Error.InvalidXls;
                break :value strings[index];
            },
            record_rk => value: {
                if (record.data.len < 10) return Error.InvalidXls;
                break :value try std.fmt.allocPrint(arena, "{d}", .{rkValue(std.mem.readInt(u32, record.data[6..10], .little))});
            },
            record_number => value: {
                if (record.data.len < 14) return Error.InvalidXls;
                break :value try std.fmt.allocPrint(arena, "{d}", .{@as(f64, @bitCast(std.mem.readInt(u64, record.data[6..14], .little)))});
            },
            record_blank, record_mulblank => continue,
            else => continue,
        };
        if (record.data.len < 4) return Error.InvalidXls;
        const row = std.mem.readInt(u16, record.data[0..2], .little);
        const column = std.mem.readInt(u16, record.data[2..4], .little);
        while (rows.items.len <= row) try rows.append(arena, .empty);
        const target = &rows.items[row];
        while (target.items.len <= column) try target.append(arena, "");
        target.items[column] = value;
    }
    const grid = try arena.alloc([]const []const u8, rows.items.len);
    for (rows.items, grid) |cells, *out| out.* = cells.items;
    return grid;
}

/// An `RK` number: a 30-bit integer or the top of a double, maybe /100.
fn rkValue(rk: u32) f64 {
    const base: f64 = if (rk & 2 != 0)
        @floatFromInt(@as(i32, @bitCast(rk)) >> 2)
    else
        @bitCast(@as(u64, rk & 0xFFFFFFFC) << 32);
    return if (rk & 1 != 0) base / 100 else base;
}

/// A `BOUNDSHEET` name: a count byte, a flags byte, the characters.
fn shortString(arena: std.mem.Allocator, data: []const u8) ![]const u8 {
    if (data.len < 2) return Error.InvalidXls;
    const count = data[0];
    const wide = data[1] & 1 != 0;
    const units = try arena.alloc(u16, count);
    const width: usize = if (wide) 2 else 1;
    if (2 + count * width > data.len) return Error.InvalidXls;
    for (units, 0..) |*unit, index| unit.* = if (wide) std.mem.readInt(u16, data[2 + index * 2 ..][0..2], .little) else data[2 + index];
    return std.unicode.wtf16LeToWtf8Alloc(arena, units);
}

/// The `SST` and its `CONTINUE` records read as one run of bytes, except
/// that a string's characters going on in the next record start there with
/// a fresh flags byte saying whether they are 8 or 16 bits wide.
const Parts = struct {
    parts: []const []const u8,
    part: usize = 0,
    at: usize = 0,

    fn settle(self: *Parts) !void {
        while (self.part < self.parts.len and self.at == self.parts[self.part].len) {
            self.part += 1;
            self.at = 0;
        }
        if (self.part >= self.parts.len) return Error.InvalidXls;
    }

    fn byte(self: *Parts) !u8 {
        try self.settle();
        defer self.at += 1;
        return self.parts[self.part][self.at];
    }

    fn int(self: *Parts, comptime T: type) !T {
        var value: T = 0;
        inline for (0..@sizeOf(T)) |index| value |= @as(T, try self.byte()) << (8 * index);
        return value;
    }

    fn skip(self: *Parts, count: usize) !void {
        var left = count;
        while (left > 0) {
            try self.settle();
            const take = @min(left, self.parts[self.part].len - self.at);
            self.at += take;
            left -= take;
        }
    }

    fn characters(self: *Parts, units: []u16, first_wide: bool) !void {
        var wide = first_wide;
        var filled: usize = 0;
        while (filled < units.len) {
            if (self.part >= self.parts.len) return Error.InvalidXls;
            if (self.at == self.parts[self.part].len) {
                self.part += 1;
                self.at = 0;
                if (self.part >= self.parts.len or self.parts[self.part].len == 0) return Error.InvalidXls;
                wide = self.parts[self.part][0] & 1 != 0;
                self.at = 1;
                continue;
            }
            const data = self.parts[self.part];
            if (wide) {
                if (self.at + 2 > data.len) return Error.InvalidXls;
                units[filled] = std.mem.readInt(u16, data[self.at..][0..2], .little);
                self.at += 2;
            } else {
                units[filled] = data[self.at];
                self.at += 1;
            }
            filled += 1;
        }
    }
};

fn sharedStrings(arena: std.mem.Allocator, parts: []const []const u8) ![]const []const u8 {
    var reader: Parts = .{ .parts = parts };
    _ = try reader.int(u32);
    const unique = try reader.int(u32);
    // A bad count cannot ask for more strings than the bytes could hold.
    var total: usize = 0;
    for (parts) |part| total += part.len;
    if (unique > total / 3) return Error.InvalidXls;
    const strings = try arena.alloc([]const u8, unique);
    for (strings) |*string| {
        const count = try reader.int(u16);
        const flags = try reader.byte();
        const runs: usize = if (flags & 0x08 != 0) try reader.int(u16) else 0;
        const extension: usize = if (flags & 0x04 != 0) try reader.int(u32) else 0;
        const units = try arena.alloc(u16, count);
        try reader.characters(units, flags & 0x01 != 0);
        try reader.skip(runs * 4 + extension);
        string.* = try std.unicode.wtf16LeToWtf8Alloc(arena, units);
    }
    return strings;
}

const testing = std.testing;

test "Data/ELK/desc.xls reads as LibreOffice decodes it" {
    // The independent decoding is LibreOffice's (24.2, Ubuntu 24.04):
    //   soffice --headless --convert-to "csv:Text - txt - csv (StarCalc):44,34,76,1,,0,false,true,false,false,false,-1" desc.xls
    // kept as tools/zig/fixtures/elk/desc-libreoffice.csv; sheet.zig's test
    // compares every cell. This pins the shape the reader was measured on.
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const bytes = try std.Io.Dir.cwd().readFileAlloc(testing.io, "Data/ELK/desc.xls", arena, .limited(1 << 20));
    const grid = try read(arena, bytes);
    try testing.expectEqual(@as(usize, 154), grid.len);
    const header = [_][]const u8{ "Path", "Original", "Translation", "State", "Description" };
    for (header, grid[0]) |want, got| try testing.expectEqualStrings(want, got);
    try testing.expectEqualStrings("0\\scenarios\\descriptions\\defend_area00\\0h", grid[1][0]);
    try testing.expectEqualStrings("Secure bridge", grid[1][1]);
}

test "a truncated, foreign or BIFF5 file fails with its reason" {
    var arena_state = std.heap.ArenaAllocator.init(testing.allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const bytes = try std.Io.Dir.cwd().readFileAlloc(testing.io, "Data/ELK/desc.xls", arena, .limited(1 << 20));
    try testing.expectError(Error.InvalidXls, read(arena, "PK\x03\x04 not a compound file"));
    try testing.expectError(Error.InvalidXls, read(arena, bytes[0 .. bytes.len / 2]));
    // The same file with its stream renamed `Book`, as Excel 95 named it.
    const renamed = try arena.dupe(u8, bytes);
    const at = std.mem.indexOf(u8, renamed, &[_]u8{ 'W', 0, 'o', 0, 'r', 0, 'k', 0, 'b', 0, 'o', 0, 'o', 0, 'k', 0, 0, 0 }).?;
    @memcpy(renamed[at..][0..10], &[_]u8{ 'B', 0, 'o', 0, 'o', 0, 'k', 0, 0, 0 });
    std.mem.writeInt(u16, renamed[at + 0x40 ..][0..2], 10, .little);
    try testing.expectError(Error.NotBiff8, read(arena, renamed));
}

test "RK numbers decode as integers, doubles and hundredths" {
    try testing.expectEqual(@as(f64, 1), rkValue((1 << 2) | 2));
    try testing.expectEqual(@as(f64, -3), rkValue(@bitCast(@as(i32, -3 << 2) | 2)));
    try testing.expectEqual(@as(f64, 0.25), rkValue((25 << 2) | 3));
    try testing.expectEqual(@as(f64, 1.5), rkValue(@intCast(@as(u64, @bitCast(@as(f64, 1.5))) >> 32)));
}
