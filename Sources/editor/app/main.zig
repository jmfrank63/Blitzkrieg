//! MapEditor. For now it knows one mode, the host check:
//!   MapEditor --check <map> [<out.tga>]
//! It starts the engine hidden with ImGui over it, opens the map, draws frames
//! with a magenta ImGui window at a known place, captures one frame as it was
//! presented and checks that both the panel and the map are in it. Prints
//! "map-editor: host check PASS (<driver>, <w>x<h>)" and exits 0, or a
//! "FAIL:" line naming what was wrong and exits 1.
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const host_mod = @import("host.zig");
const c = host_mod.c;

const default_output = "zig-out/local-test/map-editor-check.tga";

/// The probe window, in screen pixels (a window point is a screen pixel).
const probe = struct {
    const x = 40;
    const y = 40;
    const w = 120;
    const h = 80;
};

const probe_frames = 10;

/// A tile's side in world units: fWorldCellSize (Formats/fmtTerrain.h), 32 * sqrt(2).
const world_cell_size: f32 = 32.0 * std.math.sqrt2;

const windows_crt = if (builtin.os.tag == .windows) struct {
    // Plain externs, not extern "c": the CRT is linked by the build
    // (linkMsvcRuntime), and naming libc here would make Zig link its own.
    extern fn _set_error_mode(mode: c_int) c_int;
    extern fn _set_abort_behavior(flags: c_uint, mask: c_uint) c_uint;
    extern fn _CrtSetReportMode(report_type: c_int, mode: c_int) c_int;
    extern fn _CrtSetReportFile(report_type: c_int, file: ?*anyopaque) ?*anyopaque;
} else struct {};

/// A failed assert in a Windows debug build prints and then calls abort(),
/// which the debug CRT reports as a "Debug Error!" message box that nobody
/// on a CI runner can click. Report both to stderr instead
/// (tools/zig/editor_bridge_test.cpp main does the same).
fn routeCrtReportsToStderr() void {
    if (builtin.os.tag != .windows) return;
    const OUT_TO_STDERR = 1; // stdlib.h _OUT_TO_STDERR
    const WRITE_ABORT_MSG = 0x1; // stdlib.h _WRITE_ABORT_MSG
    const CALL_REPORTFAULT = 0x2; // stdlib.h _CALL_REPORTFAULT
    _ = windows_crt._set_error_mode(OUT_TO_STDERR);
    _ = windows_crt._set_abort_behavior(0, WRITE_ABORT_MSG | CALL_REPORTFAULT);
    // _CrtSetReportMode/_CrtSetReportFile exist only in the debug CRT, which
    // the build links exactly in Debug (linkMsvcRuntime); in a release CRT they
    // are macros that do nothing.
    if (builtin.mode != .Debug) return;
    const CRT_ERROR = 1; // crtdbg.h _CRT_ERROR
    const CRT_ASSERT = 2; // crtdbg.h _CRT_ASSERT
    const CRTDBG_MODE_FILE = 0x1; // crtdbg.h _CRTDBG_MODE_FILE
    const CRTDBG_FILE_STDERR: ?*anyopaque = @ptrFromInt(@as(usize, @bitCast(@as(isize, -5)))); // ((_HFILE)-5)
    _ = windows_crt._CrtSetReportMode(CRT_ASSERT, CRTDBG_MODE_FILE);
    _ = windows_crt._CrtSetReportFile(CRT_ASSERT, CRTDBG_FILE_STDERR);
    _ = windows_crt._CrtSetReportMode(CRT_ERROR, CRTDBG_MODE_FILE);
    _ = windows_crt._CrtSetReportFile(CRT_ERROR, CRTDBG_FILE_STDERR);
}

pub fn main(minimal: std.process.Init.Minimal) !void {
    routeCrtReportsToStderr();
    const gpa = std.heap.smp_allocator;
    const io = std.Io.Threaded.global_single_threaded.io();

    var args = try std.process.Args.Iterator.initAllocator(minimal.args, gpa);
    defer args.deinit();
    _ = args.next();
    const mode = args.next() orelse usage();
    if (!std.mem.eql(u8, mode, "--check")) usage();
    const map = args.next() orelse usage();
    const output = args.next() orelse default_output;
    if (args.next() != null) usage();

    const passed = try check(gpa, io, map, output);
    std.process.exit(if (passed) 0 else 1);
}

// On Windows the engine's C++ statics are linked into this executable, and
// only the CRT's own entry point (mainCRTStartup) initialises the CRT and runs
// their constructors; Zig's entry point runs neither. The build makes
// mainCRTStartup the entry (as it does for editor-bridge-test), and it calls
// a C main, which Zig exports itself only when it links libc - which this
// executable does not, so that the engine's CRT (linkMsvcRuntime) is the only
// one. The arguments are read from the PEB, as Zig's own Windows entry does.
comptime {
    if (builtin.os.tag == .windows and !builtin.link_libc) @export(&crtMain, .{ .name = "main" });
}

fn crtMain(argc: c_int, argv: ?*anyopaque) callconv(.c) c_int {
    _ = argc;
    _ = argv;
    main(.{
        .args = .{ .vector = std.os.windows.peb().ProcessParameters.CommandLine.slice() },
        .environ = .{ .block = .global },
    }) catch |err| {
        std.debug.print("map-editor: {s}\n", .{@errorName(err)});
        return 1;
    };
    return 0;
}

fn usage() noreturn {
    std.debug.print("usage: MapEditor --check <map> [<out.tga>]\n", .{});
    std.process.exit(2);
}

fn fail(comptime format: []const u8, args: anytype) bool {
    std.debug.print("map-editor: host check FAIL: " ++ format ++ "\n", args);
    return false;
}

fn check(gpa: std.mem.Allocator, io: std.Io, map: []const u8, output: []const u8) !bool {
    if (std.fs.path.dirname(output)) |directory| try std.Io.Dir.cwd().createDirPath(io, directory);
    const map_z = try gpa.dupeZ(u8, map);
    defer gpa.free(map_z);
    const output_z = try gpa.dupeZ(u8, output);
    defer gpa.free(output_z);

    var host = host_mod.Host.start(.{ .title = "Map Editor", .hidden = true }) catch |err|
        return fail("the host did not start ({s})", .{@errorName(err)});
    defer host.stop();

    var summary: c.BkEditorMapSummary = std.mem.zeroes(c.BkEditorMapSummary);
    if (c.BkEditorOpenMap(host.session, map_z.ptr, &summary) != c.BK_EDITOR_OK)
        return fail("{s} did not open: {s}", .{ map, std.mem.span(c.BkEditorLastMessage(host.session)) });
    // The camera opens on the map's corner, where half the screen is off the
    // map; the middle of the map has ground under all of it.
    const centre_x = @as(f32, @floatFromInt(summary.width_tiles)) * world_cell_size / 2;
    const centre_y = @as(f32, @floatFromInt(summary.height_tiles)) * world_cell_size / 2;
    if (c.BkEditorSetCamera(host.session, centre_x, centre_y) != c.BK_EDITOR_OK)
        return fail("the camera would not move to the map's middle: {s}", .{std.mem.span(c.BkEditorLastMessage(host.session))});

    var frame: u32 = 0;
    while (frame < probe_frames) : (frame += 1) {
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
        host.beginFrame();
        drawProbe();
        host.endFrame() catch |err| return fail("frame {d}: {s}: {s}", .{ frame, @errorName(err), std.mem.span(c.BkEditorLastMessage(host.session)) });
    }
    // The last frame's draw data stays ImGui's until the next igNewFrame, so
    // the captured frame has the probe over it too.
    if (c.BkEditorCaptureFrame(host.session, output_z.ptr) != c.BK_EDITOR_OK)
        return fail("the frame was not captured: {s}", .{std.mem.span(c.BkEditorLastMessage(host.session))});

    var width: c_int = 0;
    var height: c_int = 0;
    if (c.BkEditorScreenSize(host.session, &width, &height) != c.BK_EDITOR_OK)
        return fail("no screen size: {s}", .{std.mem.span(c.BkEditorLastMessage(host.session))});
    var device: ?*anyopaque = null;
    var format: c_uint = 0;
    _ = c.BkEditorGpuDevice(host.session, &device, &format);
    const driver_name = if (device != null) sdl3.c.SDL_GetGPUDeviceDriver(@ptrCast(device)) else null;
    const driver: []const u8 = if (driver_name != null) std.mem.span(driver_name) else "unknown";

    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, output, gpa, .limited(64 << 20));
    defer gpa.free(bytes);
    const image = Tga.parse(bytes) catch |err| return fail("{s} is not an uncompressed 32-bit TGA ({s})", .{ output, @errorName(err) });
    if (image.width != width or image.height != height)
        return fail("{s} is {d}x{d}, the screen {d}x{d}", .{ output, image.width, image.height, width, height });

    const inside_x = probe.x + probe.w / 2;
    const inside_y = probe.y + probe.h / 2;
    const inside = image.pixel(inside_x, inside_y);
    if (!inside.near(magenta))
        return fail("the probe window's centre ({d},{d}) is ({d},{d},{d}), not magenta", .{ inside_x, inside_y, inside.r, inside.g, inside.b });
    // The screen's centre is far from the probe; the map is drawn there.
    const outside_x: u32 = @intCast(@divTrunc(width, 2));
    const outside_y: u32 = @intCast(@divTrunc(height, 2));
    const outside = image.pixel(outside_x, outside_y);
    if (outside.near(magenta) or outside.near(clear_colour))
        return fail("the screen's centre ({d},{d}) is ({d},{d},{d}), not the map", .{ outside_x, outside_y, outside.r, outside.g, outside.b });

    std.debug.print("map-editor: host check PASS ({s}, {d}x{d})\n", .{ driver, width, height });
    return true;
}

fn drawProbe() void {
    imgui.c.igSetNextWindowPos(.{ .x = probe.x, .y = probe.y }, imgui.c.ImGuiCond_Always);
    imgui.c.igSetNextWindowSize(.{ .x = probe.w, .y = probe.h }, imgui.c.ImGuiCond_Always);
    imgui.c.igPushStyleColorImVec4(imgui.c.ImGuiCol_WindowBg, .{ .x = 1, .y = 0, .z = 1, .w = 1 });
    _ = imgui.c.igBegin("probe", null, imgui.c.ImGuiWindowFlags_NoDecoration | imgui.c.ImGuiWindowFlags_NoMove | imgui.c.ImGuiWindowFlags_NoSavedSettings);
    imgui.c.igEnd();
    imgui.c.igPopStyleColor();
}

const Rgb = struct {
    r: u8,
    g: u8,
    b: u8,

    fn near(self: Rgb, other: Rgb) bool {
        return close(self.r, other.r) and close(self.g, other.g) and close(self.b, other.b);
    }

    fn close(a: u8, b: u8) bool {
        return @abs(@as(i16, a) - @as(i16, b)) <= 2;
    }
};

const magenta = Rgb{ .r = 255, .g = 0, .b = 255 };
/// What DrawSessionFrame clears to before the scene is drawn.
const clear_colour = Rgb{ .r = 0, .g = 0, .b = 0 };

/// An uncompressed 32-bit TGA: an 18-byte header, an optional ID, then BGRA
/// rows, bottom row first unless bit 5 of the descriptor (byte 17) is set.
const Tga = struct {
    width: u32,
    height: u32,
    top_first: bool,
    pixels: []const u8,

    fn parse(bytes: []const u8) !Tga {
        if (bytes.len < 18) return error.Truncated;
        if (bytes[1] != 0 or bytes[2] != 2) return error.NotUncompressedTrueColour;
        if (bytes[16] != 32) return error.Not32Bit;
        const width = std.mem.readInt(u16, bytes[12..14], .little);
        const height = std.mem.readInt(u16, bytes[14..16], .little);
        const start = 18 + @as(usize, bytes[0]);
        const length = @as(usize, width) * height * 4;
        if (bytes.len < start + length) return error.Truncated;
        return .{ .width = width, .height = height, .top_first = bytes[17] & 0x20 != 0, .pixels = bytes[start .. start + length] };
    }

    fn pixel(self: Tga, x: u32, y: u32) Rgb {
        const row = if (self.top_first) y else self.height - 1 - y;
        const i = (@as(usize, row) * self.width + x) * 4;
        return .{ .r = self.pixels[i + 2], .g = self.pixels[i + 1], .b = self.pixels[i] };
    }
};
