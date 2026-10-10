//! Audits the installed Windows layouts for modules that still bind the CRT's
//! operator new/delete instead of BkMemory's.
//!
//! Every C++ module carries its own forwarding copy of the operators
//! (Sources/src/BkMemory/new_delete.cpp), so the loader never has a reason to
//! import them from a CRT DLL. A module that does import `??2@`, `??3@`,
//! `??_U@` or `??_V@` from msvcp*/vcruntime*/ucrtbase* still allocates through
//! the CRT heap, and a block it hands to another module is then freed by a
//! different allocator. A module that imports the C++ standard library DLL
//! (msvcp*) must also import BkMemory.dll, or it has no path to the one
//! allocator.
//!
//! usage: bk_memory_import_audit --os <os tag> --dir <layout> [--module <name>]...
//!                               [--optional <name>]... [--file <path>]...
//! or:    zig build bk-memory-import-audit
//!
//! Off Windows the tool prints "not run on <os>" and succeeds: the check is a
//! PE import-table walk, and the other layouts are ELF and Mach-O.
const std = @import("std");

/// Mangled operator new, delete, new[] and delete[] (MSVC x64). The import
/// names are prefixes of `??2@YAPEAX_K@Z` and its relatives.
const operator_prefixes = [_][]const u8{ "??2@", "??3@", "??_U@", "??_V@" };

/// DLLs an operator import from which is a failure.
const crt_dll_prefixes = [_][]const u8{ "msvcp", "vcruntime", "ucrtbase", "api-ms-win-crt" };

/// DLLs whose presence marks a module as C++ and so requires BkMemory.dll. Only
/// the C++ standard library counts: vcruntime is also the C side (memcpy,
/// setjmp), which pure Zig modules such as CloudSync import. A module that
/// binds the operators from vcruntime is caught by the operator check instead.
const cxx_runtime_prefixes = [_][]const u8{"msvcp"};

/// Third-party and non-engine binaries the audit does not hold to the rule:
/// they are not built from engine sources and are not part of the allocator
/// contract. BkMemory.dll is the allocator and imports nothing of it.
const allowlist = [_][]const u8{ "SDL3.dll", "rclone.exe", "BkMemory.dll" };

pub const Report = struct {
    imports_bk_memory: bool = false,
    imports_cxx_runtime: bool = false,
    /// "<dll>!<symbol>" for every CRT operator import found.
    operator_imports: []const []const u8 = &.{},
};

fn startsWithIgnoreCase(haystack: []const u8, prefix: []const u8) bool {
    return haystack.len >= prefix.len and std.ascii.eqlIgnoreCase(haystack[0..prefix.len], prefix);
}

fn matchesAny(name: []const u8, prefixes: []const []const u8) bool {
    for (prefixes) |prefix| if (startsWithIgnoreCase(name, prefix)) return true;
    return false;
}

const AuditError = error{ NotAPortableExecutable, BadImportDirectory, OutOfMemory };

fn rvaToOffset(coff: *const std.coff.Coff, rva: u32) ?usize {
    for (coff.getSectionHeaders()) |section| {
        const size = @max(section.virtual_size, section.size_of_raw_data);
        if (rva >= section.virtual_address and rva < section.virtual_address + size) {
            return section.pointer_to_raw_data + (rva - section.virtual_address);
        }
    }
    return null;
}

fn cstringAt(data: []const u8, offset: usize) ?[]const u8 {
    if (offset >= data.len) return null;
    const end = std.mem.indexOfScalarPos(u8, data, offset, 0) orelse return null;
    return data[offset..end];
}

/// Reads the import table of one PE image. The list is allocated from
/// `allocator`, each label with it.
pub fn auditImage(allocator: std.mem.Allocator, bytes: []const u8) AuditError!Report {
    const coff = std.coff.Coff.init(bytes, false) catch return error.NotAPortableExecutable;
    if (!coff.is_image) return error.NotAPortableExecutable;
    var report: Report = .{};
    const directories = coff.getDataDirectories();
    if (directories.len <= 1 or directories[1].size == 0) return report;
    var is_64 = false;
    switch (@backingInt(coff.getOptionalHeader().magic)) {
        std.coff.IMAGE_NT_OPTIONAL_HDR64_MAGIC => is_64 = true,
        std.coff.IMAGE_NT_OPTIONAL_HDR32_MAGIC => {},
        else => return error.NotAPortableExecutable,
    }
    const thunk_size: usize = if (is_64) 8 else 4;
    var found: std.ArrayList([]const u8) = .empty;
    errdefer found.deinit(allocator);
    var offset = rvaToOffset(&coff, directories[1].virtual_address) orelse return error.BadImportDirectory;
    while (true) : (offset += @sizeOf(std.coff.ImportDirectoryEntry)) {
        if (offset + @sizeOf(std.coff.ImportDirectoryEntry) > bytes.len) return error.BadImportDirectory;
        const entry = std.mem.bytesToValue(std.coff.ImportDirectoryEntry, bytes[offset..][0..@sizeOf(std.coff.ImportDirectoryEntry)]);
        if (entry.name_rva == 0 and entry.import_lookup_table_rva == 0 and entry.import_address_table_rva == 0) break;
        const name_offset = rvaToOffset(&coff, entry.name_rva) orelse return error.BadImportDirectory;
        const dll = cstringAt(bytes, name_offset) orelse return error.BadImportDirectory;
        if (std.ascii.eqlIgnoreCase(dll, "BkMemory.dll")) report.imports_bk_memory = true;
        if (matchesAny(dll, &cxx_runtime_prefixes)) report.imports_cxx_runtime = true;
        if (!matchesAny(dll, &crt_dll_prefixes)) continue;
        const table_rva = if (entry.import_lookup_table_rva != 0) entry.import_lookup_table_rva else entry.import_address_table_rva;
        var thunk = rvaToOffset(&coff, table_rva) orelse return error.BadImportDirectory;
        while (thunk + thunk_size <= bytes.len) : (thunk += thunk_size) {
            const value: u64 = if (is_64) std.mem.readInt(u64, bytes[thunk..][0..8], .little) else std.mem.readInt(u32, bytes[thunk..][0..4], .little);
            if (value == 0) break;
            const ordinal_bit: u64 = if (is_64) 1 << 63 else 1 << 31;
            if (value & ordinal_bit != 0) continue;
            const hint_offset = rvaToOffset(&coff, @intCast(value & 0x7fff_ffff)) orelse return error.BadImportDirectory;
            const symbol = cstringAt(bytes, hint_offset + 2) orelse return error.BadImportDirectory;
            if (matchesAny(symbol, &operator_prefixes)) {
                const label = std.fmt.allocPrint(allocator, "{s}!{s}", .{ dll, symbol }) catch return error.OutOfMemory;
                found.append(allocator, label) catch return error.OutOfMemory;
            }
        }
    }
    report.operator_imports = found.toOwnedSlice(allocator) catch return error.OutOfMemory;
    return report;
}

fn isAllowlisted(name: []const u8) bool {
    for (allowlist) |entry| if (std.ascii.eqlIgnoreCase(name, entry)) return true;
    return false;
}

fn hasPeExtension(name: []const u8) bool {
    return std.ascii.endsWithIgnoreCase(name, ".dll") or std.ascii.endsWithIgnoreCase(name, ".exe");
}

/// One module to audit: the label printed, the file to read, whether the path
/// is relative to the layout, and whether a missing file is a failure (a
/// listed engine module) or just not installed (an editor the caller did not
/// install).
const Target = struct {
    label: []const u8,
    path: []const u8,
    in_layout: bool,
    optional: bool,
};

pub fn main(init: std.process.Init) !void {
    var arena_state = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena_state.deinit();
    const arena = arena_state.allocator();
    const io = init.io;

    var iterator = try std.process.Args.Iterator.initAllocator(init.minimal.args, arena);
    defer iterator.deinit();
    _ = iterator.skip();
    var os_tag: []const u8 = "windows";
    var dir_path: ?[]const u8 = null;
    var targets: std.ArrayList(Target) = .empty;
    var listed = false;
    while (iterator.next()) |arg| {
        if (std.mem.eql(u8, arg, "--os")) {
            os_tag = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--dir")) {
            dir_path = try arena.dupe(u8, iterator.next() orelse usage());
        } else if (std.mem.eql(u8, arg, "--module") or std.mem.eql(u8, arg, "--optional")) {
            const name = try arena.dupe(u8, iterator.next() orelse usage());
            listed = true;
            try targets.append(arena, .{ .label = name, .path = name, .in_layout = true, .optional = std.mem.eql(u8, arg, "--optional") });
        } else if (std.mem.eql(u8, arg, "--file")) {
            const path = try arena.dupe(u8, iterator.next() orelse usage());
            try targets.append(arena, .{ .label = std.fs.path.basename(path), .path = path, .in_layout = false, .optional = false });
        } else {
            usage();
        }
    }
    const layout = dir_path orelse usage();

    var stdout_buffer: [4096]u8 = undefined;
    var stdout = std.Io.File.stdout().writer(io, &stdout_buffer);
    const out = &stdout.interface;

    if (!std.mem.eql(u8, os_tag, "windows")) {
        try out.print("bk-memory-import-audit: not run on {s}\n", .{os_tag});
        try out.flush();
        return;
    }

    var dir = try std.Io.Dir.cwd().openDir(io, layout, .{ .iterate = true });
    defer dir.close(io);
    // Without an explicit module list every PE file in the layout is audited.
    if (!listed) {
        var names: std.ArrayList([]const u8) = .empty;
        var it = dir.iterate();
        while (try it.next(io)) |entry| {
            if (entry.kind != .file or !hasPeExtension(entry.name)) continue;
            try names.append(arena, try arena.dupe(u8, entry.name));
        }
        std.mem.sort([]const u8, names.items, {}, struct {
            fn less(_: void, x: []const u8, y: []const u8) bool {
                return std.mem.lessThan(u8, x, y);
            }
        }.less);
        for (names.items) |name| try targets.append(arena, .{ .label = name, .path = name, .in_layout = true, .optional = false });
    }

    var modules: usize = 0;
    var failures: usize = 0;
    for (targets.items) |target| {
        if (isAllowlisted(target.label)) {
            try out.print("{s}: allowlisted\n", .{target.label});
            continue;
        }
        const read = if (target.in_layout)
            dir.readFileAlloc(io, target.path, arena, .limited(512 << 20))
        else
            std.Io.Dir.cwd().readFileAlloc(io, target.path, arena, .limited(512 << 20));
        const bytes = read catch |err| {
            if (err == error.FileNotFound and target.optional) {
                try out.print("{s}: not installed\n", .{target.label});
                continue;
            }
            try out.print("{s}: FAIL cannot read: {s}\n", .{ target.label, @errorName(err) });
            failures += 1;
            continue;
        };
        const report = auditImage(arena, bytes) catch |err| {
            try out.print("{s}: FAIL cannot read import table: {s}\n", .{ target.label, @errorName(err) });
            failures += 1;
            continue;
        };
        modules += 1;
        const missing_bk = report.imports_cxx_runtime and !report.imports_bk_memory;
        const bad = report.operator_imports.len != 0 or missing_bk;
        try out.print("{s}: BkMemory import {s}, C++ runtime {s}, CRT operator imports {d}{s}\n", .{
            target.label,
            if (report.imports_bk_memory) "yes" else "no",
            if (report.imports_cxx_runtime) "yes" else "no",
            report.operator_imports.len,
            if (bad) " FAIL" else "",
        });
        for (report.operator_imports) |label| try out.print("    binds the CRT operator {s}\n", .{label});
        if (missing_bk) try out.print("    imports the C++ standard library DLL but not BkMemory.dll\n", .{});
        if (bad) failures += 1;
    }
    try out.print("bk-memory-import-audit: {d} module(s) audited, {d} failure(s)\n", .{ modules, failures });
    try out.flush();
    if (failures != 0) std.process.exit(1);
}

fn usage() noreturn {
    std.debug.print(
        \\usage: bk_memory_import_audit --os <os tag> --dir <layout> [--module <name>]...
        \\                              [--optional <name>]... [--file <path>]...
        \\Parses the import table of each named module in <layout> (every .dll and .exe
        \\when none is named) and of each --file, and fails when one binds a CRT
        \\operator new/delete, or imports the C++ standard library DLL without
        \\importing BkMemory.dll. --optional skips a module that is not installed.
        \\Off Windows it prints "not run" and succeeds.
        \\
    , .{});
    std.process.exit(2);
}

// ---------------------------------------------------------------------------
// tests: a minimal PE32+ image built in memory

const testing = std.testing;

const TestImport = struct { dll: []const u8, symbols: []const []const u8 };

/// One section (".idata") at RVA 0x1000, file offset 0x200, holding the import
/// directory, the lookup tables and the strings. Nothing else a loader needs.
fn buildTestImage(allocator: std.mem.Allocator, imports: []const TestImport) ![]u8 {
    const section_rva: u32 = 0x1000;
    const section_offset: u32 = 0x200;
    var body: std.ArrayList(u8) = .empty;
    defer body.deinit(allocator);
    const directory_size = (imports.len + 1) * @sizeOf(std.coff.ImportDirectoryEntry);
    try body.appendNTimes(allocator, 0, directory_size);
    for (imports, 0..) |import, index| {
        // Hint/name entries first so the table can point at them.
        var name_rvas: std.ArrayList(u32) = .empty;
        defer name_rvas.deinit(allocator);
        for (import.symbols) |symbol| {
            if (body.items.len % 2 != 0) try body.append(allocator, 0);
            try name_rvas.append(allocator, section_rva + @as(u32, @intCast(body.items.len)));
            try body.appendNTimes(allocator, 0, 2);
            try body.appendSlice(allocator, symbol);
            try body.append(allocator, 0);
        }
        while (body.items.len % 8 != 0) try body.append(allocator, 0);
        const table_rva = section_rva + @as(u32, @intCast(body.items.len));
        for (name_rvas.items) |rva| {
            var thunk: [8]u8 = undefined;
            std.mem.writeInt(u64, &thunk, rva, .little);
            try body.appendSlice(allocator, &thunk);
        }
        try body.appendNTimes(allocator, 0, 8);
        const dll_rva = section_rva + @as(u32, @intCast(body.items.len));
        try body.appendSlice(allocator, import.dll);
        try body.append(allocator, 0);
        const entry: std.coff.ImportDirectoryEntry = .{
            .import_lookup_table_rva = table_rva,
            .time_date_stamp = 0,
            .forwarder_chain = 0,
            .name_rva = dll_rva,
            .import_address_table_rva = table_rva,
        };
        @memcpy(body.items[index * @sizeOf(std.coff.ImportDirectoryEntry) ..][0..@sizeOf(std.coff.ImportDirectoryEntry)], std.mem.asBytes(&entry));
    }

    const pe_offset: u32 = 0x40;
    const optional_size: u16 = @sizeOf(std.coff.OptionalHeader.@"PE32+") + std.coff.IMAGE_NUMBEROF_DIRECTORY_ENTRIES * @sizeOf(std.coff.ImageDataDirectory);
    var image: std.ArrayList(u8) = .empty;
    errdefer image.deinit(allocator);
    try image.appendNTimes(allocator, 0, section_offset);
    image.items[0] = 'M';
    image.items[1] = 'Z';
    std.mem.writeInt(u32, image.items[std.coff.pe_pointer_offset..][0..4], pe_offset, .little);
    @memcpy(image.items[pe_offset..][0..4], std.coff.pe_signature);
    // COFF header: machine, number_of_sections, ..., size_of_optional_header at +16.
    const header = image.items[pe_offset + 4 ..];
    std.mem.writeInt(u16, header[0..2], 0x8664, .little);
    std.mem.writeInt(u16, header[2..4], 1, .little);
    std.mem.writeInt(u16, header[16..18], optional_size, .little);
    const optional = header[@sizeOf(std.coff.Header)..];
    std.mem.writeInt(u16, optional[0..2], std.coff.IMAGE_NT_OPTIONAL_HDR64_MAGIC, .little);
    // number_of_rva_and_sizes is the last field before the directories.
    const directories_offset = @sizeOf(std.coff.OptionalHeader.@"PE32+");
    std.mem.writeInt(u32, optional[directories_offset - 4 ..][0..4], std.coff.IMAGE_NUMBEROF_DIRECTORY_ENTRIES, .little);
    const import_directory = optional[directories_offset + @sizeOf(std.coff.ImageDataDirectory) ..];
    std.mem.writeInt(u32, import_directory[0..4], section_rva, .little);
    std.mem.writeInt(u32, import_directory[4..8], @intCast(directory_size), .little);
    // One section header after the optional header.
    const section_header = optional[optional_size..];
    @memcpy(section_header[0..6], ".idata");
    std.mem.writeInt(u32, section_header[8..12], @intCast(body.items.len), .little);
    std.mem.writeInt(u32, section_header[12..16], section_rva, .little);
    std.mem.writeInt(u32, section_header[16..20], @intCast(body.items.len), .little);
    std.mem.writeInt(u32, section_header[20..24], section_offset, .little);
    try image.appendSlice(allocator, body.items);
    return image.toOwnedSlice(allocator);
}

fn freeReport(allocator: std.mem.Allocator, report: Report) void {
    for (report.operator_imports) |label| allocator.free(label);
    allocator.free(report.operator_imports);
}

test "a module importing BkMemory and plain CRT functions passes" {
    const image = try buildTestImage(testing.allocator, &.{
        .{ .dll = "MSVCP140D.dll", .symbols = &.{"?_Xbad_alloc@std@@YAXXZ"} },
        .{ .dll = "BkMemory.dll", .symbols = &.{"bk_mem_alloc"} },
        .{ .dll = "KERNEL32.dll", .symbols = &.{"ExitProcess"} },
    });
    defer testing.allocator.free(image);
    const report = try auditImage(testing.allocator, image);
    defer freeReport(testing.allocator, report);
    try testing.expect(report.imports_bk_memory);
    try testing.expect(report.imports_cxx_runtime);
    try testing.expectEqual(@as(usize, 0), report.operator_imports.len);
}

test "operator new and delete imported from a CRT DLL are reported by name" {
    const image = try buildTestImage(testing.allocator, &.{
        .{ .dll = "VCRUNTIME140D.dll", .symbols = &.{ "??2@YAPEAX_K@Z", "memcpy", "??_V@YAXPEAX@Z" } },
        .{ .dll = "BkMemory.dll", .symbols = &.{"bk_mem_alloc"} },
    });
    defer testing.allocator.free(image);
    const report = try auditImage(testing.allocator, image);
    defer freeReport(testing.allocator, report);
    try testing.expectEqual(@as(usize, 2), report.operator_imports.len);
    try testing.expectEqualStrings("VCRUNTIME140D.dll!??2@YAPEAX_K@Z", report.operator_imports[0]);
    try testing.expectEqualStrings("VCRUNTIME140D.dll!??_V@YAXPEAX@Z", report.operator_imports[1]);
}

test "a C++ standard library import without BkMemory.dll is visible to the caller" {
    const image = try buildTestImage(testing.allocator, &.{
        .{ .dll = "MSVCP140.dll", .symbols = &.{"?_Xlength_error@std@@YAXPEBD@Z"} },
    });
    defer testing.allocator.free(image);
    const report = try auditImage(testing.allocator, image);
    defer freeReport(testing.allocator, report);
    try testing.expect(report.imports_cxx_runtime);
    try testing.expect(!report.imports_bk_memory);
}

test "a vcruntime-only module is not a C++ module" {
    const image = try buildTestImage(testing.allocator, &.{
        .{ .dll = "VCRUNTIME140.dll", .symbols = &.{"memcpy"} },
    });
    defer testing.allocator.free(image);
    const report = try auditImage(testing.allocator, image);
    defer freeReport(testing.allocator, report);
    try testing.expect(!report.imports_cxx_runtime);
    try testing.expectEqual(@as(usize, 0), report.operator_imports.len);
}

test "a file that is not a PE image is an error" {
    try testing.expectError(error.NotAPortableExecutable, auditImage(testing.allocator, "not a portable executable at all, just text padding the length out"));
}
