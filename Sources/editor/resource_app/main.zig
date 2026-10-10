//! ResourceEditor:
//!   ResourceEditor [-mod=<Folder>|-mod=None] [--hidden] [<kind>|<project>]  interactive
//!   ResourceEditor [-mod=...] --check [<kind>] [<out.tga>] [<picture.tga>]  headless host check
//!   ResourceEditor [-mod=...] --smoke [<kind>] [<out.<ext>>]          scripted new/save/reopen
//!   ResourceEditor [-mod=...] --batch <kind|all> <src> <dst> [-f] [-os]  batch export (batch_cli.zig)
//!   ResourceEditor [-mod=...] --batch-check <fixtures> <scratch>      the batch tier
//!   ResourceEditor [-mod=...] --smoke-edit <project.unt> <scratch>    open, edit, save, undo, save, compare
//!   ResourceEditor [-mod=...] --auto <fixtures> <scratch>             BK_EDITOR_AUTO's schedule (scenario.zig)
//!
//! <kind> is a project extension the MFC editor registered (wpn, mcp, trc,
//! scp, spt, unt, msh, obt, fnc, bld, bdg, pcp, eff, til, 3rd, 3rv, mip, chc,
//! cgc, mdc, gui); wpn when it is left out of --check and --smoke. The
//! interactive mode starts on <project> when one is named, else on a new
//! project of <kind>, else of the last sub-editor (resourceeditor.cfg; the
//! Infantry Editor at first).
//!
//! -mod=<Folder> or -mod=None, like the game's own -mod= and MapEditor's, is
//! pulled out of the argument list first and applied through BkEditorSetMod
//! right after the engine starts, before anything reads the database. A
//! refusal fails the mode the way its other startup failures do.
//!
//! The interactive mode opens a window, starts the engine on it, opens the
//! first project and shows its tree under the File menu (lifecycle_ui.zig:
//! New, Open, Open Recent, Close, Save, Save As, the unsaved-changes and lock
//! prompts, autosave and recovery) until the window closes or File > Exit,
//! both through the unsaved-changes prompt. --hidden runs the same loop with
//! the window hidden and not focusable for `hidden_frames` frames and then
//! quits, so an automated run never pops a window or waits for a person; it
//! never reads or writes the user's settings, recovery folder or layout. A step that fails before there is a window to show anything in is
//! reported through SDL_ShowSimpleMessageBox and exits non-zero.
//!
//! The host check starts the engine hidden with ImGui over it, makes a new
//! project of <kind>, draws frames with the project tree and an orange probe
//! window at a known place, captures one frame as it was presented and
//! measures it: the capture is the screen's size, the probe's centre is
//! orange and the screen's centre, far from every ImGui window, is still the
//! engine's own clear colour, so ImGui sits over the engine's frame rather
//! than replacing it. Prints "resource-editor: host check PASS (<driver>,
//! <w>x<h>, <kind>, <n> nodes)". A runner with no GPU device prints
//! "resource-editor: host check skipped: no GPU device (<reason>)" and exits
//! 0, the rule map-editor-host-check and the engine tier follow too.
//! Given <picture.tga> (a tracked fixture), a second half (docks_check.zig)
//! imports the shipped Gunner as infantry, begins its preview scene behind
//! the docks, shows a copy of the picture in the thumbnail list and measures
//! a second capture, <out>-docks.tga: the thumbnail has the picture's colour
//! and the screen's middle, the preview's, is still the scene's own frame.
//!
//! The smoke makes a new project of <kind>, saves it to <out> (deleted first,
//! so an old file cannot pass), closes it, opens that file again and checks
//! the kind and the node count survived, with frames of the interactive
//! loop's own drawing in between. Prints "resource-editor: smoke PASS".
const std = @import("std");
const builtin = @import("builtin");
const sdl3 = @import("sdl3");
const imgui = @import("editor_imgui");
const kit = @import("editor_kit");
const resource_core = @import("resource_core");
const host_mod = kit.host;
const crt = kit.crt;
const Kind = resource_core.bridge.Kind;
const lifecycle_ui = @import("lifecycle_ui.zig");
const panels_logic = @import("panels_logic.zig");
const panels_mod = @import("panels.zig");
const tools_ui = @import("tools_ui.zig");
const batch_cli = @import("batch_cli.zig");
const docks_mod = @import("docks.zig");
const settings_mod = @import("settings.zig");
const view_logic = @import("view_logic.zig");
const docks_check = @import("docks_check.zig");
const scenario = @import("scenario.zig");

/// resource_bridge.h, which includes bridge.h: the BkRes* half of the engine's
/// C ABI. A second translation beside kit.host's own bridge.h one, so the
/// session handle crosses between the two by pointer cast (`resSession`).
const c = @import("resource_bridge_c");

const default_kind: Kind = .weapon;
const default_output = "zig-out/local-test/resource_editor/resource-editor-check.tga";
const default_smoke_dir = "zig-out/local-test/resource_editor";

/// How many frames --hidden draws before it quits on its own: enough for
/// ImGui to lay its windows out and the engine to present several frames.
const hidden_frames = 30;

/// The probe window, in screen pixels (a window point is a screen pixel).
pub const probe = struct {
    pub const x = 40;
    pub const y = 40;
    pub const w = 120;
    pub const h = 80;
};

pub const probe_frames = 10;

/// Where the tree window sits: top right, clear of the probe and of the
/// screen's centre, which the host check measures as the engine's frame.
const tree_window = struct {
    const w = 260;
    const h = 220;
    const margin = 20;
};

/// The node list read from the bridge each frame; a project bigger than this
/// shows its first `max_nodes` (BkResNodes refuses a short buffer, so the
/// count is asked first and the read sized to it).
const max_nodes = 512;

/// BK_REQUIRE_ENGINE (set by CI on a runner that has a GPU device, software or not) turns the
/// no-device skip below into a failure, as it does for the C++ tiers: a missing driver must not
/// pass a tier that never ran.
var require_engine = false;

pub fn main(minimal: std.process.Init.Minimal) !void {
    crt.routeCrtReportsToStderr();
    const gpa = std.heap.smp_allocator;
    var threaded: std.Io.Threaded = .init(gpa, .{ .environ = minimal.environ });
    defer threaded.deinit();
    const io = threaded.io();
    if (minimal.environ.getAlloc(gpa, "BK_REQUIRE_ENGINE")) |value| {
        require_engine = value.len != 0 and !std.mem.eql(u8, value, "0");
        gpa.free(value);
    } else |_| {}

    var args = try std.process.Args.Iterator.initAllocator(minimal.args, gpa);
    defer args.deinit();
    _ = args.next();

    var mod_arg: ?[]const u8 = null; // the raw text after "=", "None" included
    var hidden = false;
    var rest: std.ArrayList([]const u8) = .empty;
    defer rest.deinit(gpa);
    while (args.next()) |arg| {
        if (std.mem.startsWith(u8, arg, "-mod=")) {
            mod_arg = arg["-mod=".len..];
        } else if (std.mem.eql(u8, arg, "--hidden")) {
            hidden = true;
        } else {
            try rest.append(gpa, arg);
        }
    }
    const mod: ModRequest = if (mod_arg) |raw| (if (std.mem.eql(u8, raw, "None")) .none else .{ .folder = raw }) else .unchanged;

    const items = rest.items;
    if (items.len > 0 and std.mem.eql(u8, items[0], "--check")) {
        if (items.len > 4) usage();
        const kind = if (items.len > 1) parseKind(items[1]) orelse usage() else default_kind;
        const output = if (items.len > 2) items[2] else default_output;
        const picture = if (items.len > 3) items[3] else null;
        const passed = try check(gpa, io, kind, output, picture, mod);
        std.process.exit(if (passed) 0 else 1);
    }
    if (items.len > 0 and std.mem.eql(u8, items[0], "--smoke")) {
        if (items.len > 3) usage();
        const kind = if (items.len > 1) parseKind(items[1]) orelse usage() else default_kind;
        var output_buffer: [256]u8 = undefined;
        const output = if (items.len > 2) items[2] else std.fmt.bufPrint(&output_buffer, "{s}/smoke.{s}", .{ default_smoke_dir, kind.extension() }) catch unreachable;
        const passed = try smoke(gpa, io, kind, output, mod);
        std.process.exit(if (passed) 0 else 1);
    }
    if (items.len > 0 and (std.mem.eql(u8, items[0], "--batch") or std.mem.eql(u8, items[0], "--batch-check"))) {
        std.process.exit(batchMode(gpa, io, items[1..], mod, std.mem.eql(u8, items[0], "--batch-check")));
    }
    if (items.len > 0 and (std.mem.eql(u8, items[0], "--smoke-edit") or std.mem.eql(u8, items[0], "--auto"))) {
        if (items.len != 3) usage();
        std.process.exit(scenarioMode(gpa, io, minimal.environ, items[0], items[1], items[2], mod));
    }
    if (items.len > 1) usage();
    var first_kind: ?Kind = null;
    var first_path: ?[]const u8 = null;
    if (items.len == 1) {
        if (parseKind(items[0])) |kind| {
            first_kind = kind;
        } else if (panels_logic.kindFromPath(items[0]) != null) {
            first_path = items[0];
        } else usage();
    }
    interactive(gpa, io, minimal.environ, first_kind, first_path, mod, hidden);
}

/// What -mod= asked for: nothing (leave the engine's own choice), no mod, or
/// a mod folder.
const ModRequest = union(enum) {
    unchanged,
    none,
    folder: []const u8,
};

/// A kind from its project extension, case-insensitively.
fn parseKind(text: []const u8) ?Kind {
    for (std.enums.values(Kind)) |kind| {
        if (std.ascii.eqlIgnoreCase(text, kind.extension())) return kind;
    }
    return null;
}

/// kit.host's session as resource_bridge.h's handle: the same object
/// (BkResSession is a typedef of BkEditorSession), translated twice.
fn resSession(host: *const host_mod.Host) *c.BkResSession {
    return @ptrCast(host.session);
}

fn lastMessage(host: *const host_mod.Host) []const u8 {
    return std.mem.span(c.BkEditorLastMessage(resSession(host)));
}

/// Applies -mod=, or returns the bridge's reason it refused.
fn applyMod(host: *const host_mod.Host, mod: ModRequest) ?[]const u8 {
    var buffer: [128]u8 = undefined;
    const folder: ?[*:0]const u8 = switch (mod) {
        .unchanged => return null,
        .none => null,
        .folder => |name| (std.mem.printSentinel(&buffer, "{s}", .{name}, 0) catch return "the mod folder's name is too long").ptr,
    };
    if (c.BkEditorSetMod(resSession(host), folder) != c.BK_EDITOR_OK) return lastMessage(host);
    return null;
}

/// The open project's nodes, root first, as BkResNodes lists them.
const Tree = struct {
    nodes: [max_nodes]c.BkResNodeRecord = undefined,
    count: usize = 0,
    total: usize = 0,

    /// Two-pass read: the total first, then a buffer of that size (capped).
    fn read(self: *Tree, session: *c.BkResSession) bool {
        var total: c_int = 0;
        const counted = c.BkResNodes(session, null, 0, &total);
        if (counted != c.BK_EDITOR_OK and counted != c.BK_EDITOR_REFUSED) return false;
        if (total < 0) return false;
        self.total = @intCast(total);
        if (self.total > max_nodes) {
            // A short buffer is refused outright, so a project this large is
            // shown by its count alone until the tree panel pages it.
            self.count = 0;
            return true;
        }
        var count: c_int = 0;
        if (self.total > 0 and c.BkResNodes(session, &self.nodes, @intCast(self.total), &count) != c.BK_EDITOR_OK) return false;
        self.count = @intCast(@min(count, max_nodes));
        return true;
    }

    fn depthOf(self: *const Tree, index: usize) usize {
        var depth: usize = 0;
        var parent = self.nodes[index].parent;
        var guard: usize = 0;
        while (guard < self.count) : (guard += 1) {
            const at = self.indexOf(parent) orelse break;
            depth += 1;
            parent = self.nodes[at].parent;
        }
        return depth;
    }

    fn indexOf(self: *const Tree, id: c_int) ?usize {
        for (self.nodes[0..self.count], 0..) |node, i| {
            if (node.id == id) return i;
        }
        return null;
    }
};

/// The project tree, indented by depth, in a window at the top right.
fn drawTree(tree: *const Tree, kind: Kind) void {
    const io = imgui.c.igGetIO();
    imgui.c.igSetNextWindowPos(.{ .x = io.*.DisplaySize.x - tree_window.w - tree_window.margin, .y = tree_window.margin + 20 }, imgui.c.ImGuiCond_Always);
    imgui.c.igSetNextWindowSize(.{ .x = tree_window.w, .y = tree_window.h }, imgui.c.ImGuiCond_Always);
    var title_buffer: [64]u8 = undefined;
    const title = std.mem.printSentinel(&title_buffer, "Project ({s})###project", .{kind.extension()}, 0) catch "Project###project";
    if (imgui.c.igBegin(title.ptr, null, imgui.c.ImGuiWindowFlags_NoSavedSettings)) {
        if (tree.count == 0 and tree.total != 0) {
            var line_buffer: [64]u8 = undefined;
            const line = std.mem.printSentinel(&line_buffer, "{d} nodes", .{tree.total}, 0) catch "";
            imgui.c.igTextUnformattedEx(line.ptr, line.ptr + line.len);
        }
        for (tree.nodes[0..tree.count], 0..) |*node, i| {
            // igIndentEx(0) indents by the style's default, so depth 0 skips it.
            const indent = @as(f32, @floatFromInt(tree.depthOf(i))) * 12;
            if (indent > 0) imgui.c.igIndentEx(indent);
            const name = std.mem.sliceTo(&node.display_name, 0);
            imgui.c.igTextUnformattedEx(name.ptr, name.ptr + name.len);
            if (indent > 0) imgui.c.igUnindentEx(indent);
        }
    }
    imgui.c.igEnd();
}

/// The main menu: File > New <kind>, File > Quit. Returns the kind asked
/// for, if any, and sets `quit` on Quit.
fn drawMenu(quit: *bool) ?Kind {
    var chosen: ?Kind = null;
    if (!imgui.c.igBeginMainMenuBar()) return null;
    if (imgui.c.igBeginMenuEx("File", true)) {
        if (imgui.c.igBeginMenuEx("New", true)) {
            for (std.enums.values(Kind)) |kind| {
                var label_buffer: [16]u8 = undefined;
                const label = std.mem.printSentinel(&label_buffer, "{s}", .{kind.extension()}, 0) catch unreachable;
                if (imgui.c.igMenuItemEx(label.ptr, null, false, true)) chosen = kind;
            }
            imgui.c.igEndMenu();
        }
        imgui.c.igSeparator();
        if (imgui.c.igMenuItemEx("Quit", null, false, true)) quit.* = true;
        imgui.c.igEndMenu();
    }
    imgui.c.igEndMainMenuBar();
    return chosen;
}

fn interactive(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, first_kind: ?Kind, first_path: ?[]const u8, mod: ModRequest, hidden: bool) void {
    if (hidden) crt.attachParentConsole();
    var host = host_mod.Host.start(.{ .title = "Resource Editor", .hidden = hidden }) catch |err| {
        const reason = host_mod.failureReason();
        fatal(startupStepName(err), if (reason.len != 0) reason else @errorName(err));
    };
    defer host.stop();
    if (applyMod(&host, mod)) |reason| fatal("the mod", reason);

    const session = resSession(&host);
    const ui = lifecycle_ui.Ui.create(gpa, io, environ, host.session, host.window, !hidden) catch |err| fatal("the editor state", @errorName(err));
    defer ui.destroy();
    defer _ = c.BkResClose(session);
    ui.start(first_path, first_kind);
    var tree: Tree = .{};
    var panels: panels_mod.Panels = .{ .view = view_logic.View.fromSettings(&ui.settings) };
    defer panels.deinit(gpa);
    // Deinit runs before BkResClose and host.stop: the preview scene and the
    // thumbnails' textures belong to the engine.
    var docks = docks_mod.Docks.init(gpa, io, &ui.real);
    defer docks.deinit();
    // The GUI editor's own templates sit beside the settings file; an automated run has none.
    var templates_buffer: [std.Io.Dir.max_path_bytes]u8 = undefined;
    if (ui.settings_path) |path| if (settings_mod.templatesPath(&templates_buffer, path)) |folder| docks.gui.setUserFolder(folder);
    const tools = tools_ui.Tools.create(gpa, io, environ, ui, host.window) catch |err| fatal("the editor state", @errorName(err));
    defer tools.destroy();

    var frame: u32 = 0;
    while (!ui.wantsQuit()) : (frame += 1) {
        if (hidden and frame >= hidden_frames) break;
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) {
            _ = host.handleEvent(&event);
            switch (event.type) {
                sdl3.c.SDL_EVENT_QUIT, sdl3.c.SDL_EVENT_WINDOW_CLOSE_REQUESTED => ui.requestQuit(),
                else => {},
            }
        }
        // The tree is read each frame: any File action may have replaced the
        // project, and a resource tree is small.
        if (ui.session.life.is_open) {
            if (!tree.read(session)) tree = .{};
        } else tree = .{};
        // The preview scene behind the windows follows the open project.
        docks.syncPreview(&ui.session.life);
        host.beginFrame();
        if (imgui.c.igBeginMainMenuBar()) {
            if (imgui.c.igBeginMenuEx("File", true)) {
                docks.drawFileMenuItems();
                ui.drawFileMenuItems();
                imgui.c.igEndMenu();
            }
            if (imgui.c.igBeginMenuEx("Edit", true)) {
                panels.drawEditMenuItems(gpa, ui.real.bridge(), &ui.session.life);
                tools.drawEditMenuItems();
                docks.gui.drawEditMenuItems(ui.real.bridge(), &ui.session.life);
                imgui.c.igEndMenu();
            }
            tools.drawToolsMenu();
            tools.drawEditorsMenu();
            if (imgui.c.igBeginMenuEx("View", true)) {
                docks.drawViewMenuItems();
                panels.drawViewMenuItems(gpa, ui.real.bridge(), &ui.session.life);
                imgui.c.igEndMenu();
            }
            if (imgui.c.igBeginMenuEx("Preview", true)) {
                docks.drawPreviewMenuItems(&ui.session.life);
                imgui.c.igEndMenu();
            }
            if (imgui.c.igBeginMenuEx("Help", true)) {
                docks.drawHelpMenuItems();
                imgui.c.igEndMenu();
            }
            if (panels.view.toolbar) tools.drawEditorCombo();
            imgui.c.igEndMainMenuBar();
        }
        ui.handleShortcuts();
        panels.handleShortcuts(gpa, ui.real.bridge(), &ui.session.life);
        docks.handleShortcuts();
        tools.handleShortcuts();
        // The project tree and the inspector (panels.zig) replace the
        // skeleton's read-only tree window in the interactive mode.
        panels.preview_showing = docks.preview.running;
        docks.status_bar = panels.view.status_bar;
        panels.draw(gpa, ui.real.bridge(), &ui.session.life, host.window);
        if (panels.view_changed) {
            panels.view_changed = false;
            panels.view.storeInto(&ui.settings);
            ui.session.settings_changed = true;
        }
        const project_path = if (ui.session.life.is_open) ui.session.life.doc.pathSlice() else null;
        docks.drawDocks(if (project_path) |p| std.fs.path.dirname(p) else null, &ui.session.life, panels.selection.primary);
        docks.drawDialogs(ui, host.window);
        tools.drawModals();
        ui.drawModals();
        host.endFrame() catch |err| {
            std.debug.print("resource-editor: frame {d}: {s}: {s}\n", .{ frame, @errorName(err), lastMessage(&host) });
        };
        ui.afterFrame(sdl3.c.SDL_GetTicks());
        tools.afterFrame();
    }
    if (hidden) std.debug.print("resource-editor: hidden run PASS ({d} frames, {s})\n", .{ frame, if (ui.session.life.is_open) ui.session.life.doc.kind.extension() else "no project" });
}

/// --smoke-edit and --auto (scenario.zig): the engine on a hidden window, the
/// scripted tier on it. A host with no GPU device prints the skip and passes,
/// the rule the other tiers follow. --auto reads BK_EDITOR_AUTO's schedule
/// and refuses to run without one.
fn scenarioMode(gpa: std.mem.Allocator, io: std.Io, environ: std.process.Environ, mode: []const u8, first: []const u8, scratch: []const u8, mod: ModRequest) u8 {
    crt.attachParentConsole();
    const is_auto = std.mem.eql(u8, mode, "--auto");
    const tier = if (is_auto) "auto" else "smoke";
    const schedule_text: ?[]const u8 = if (is_auto) (environ.getAlloc(gpa, "BK_EDITOR_AUTO") catch null) else null;
    defer if (schedule_text) |text| gpa.free(text);
    if (is_auto and schedule_text == null) {
        std.debug.print("resource-editor: auto FAIL: BK_EDITOR_AUTO is not set\n", .{});
        return 2;
    }
    var host = host_mod.Host.start(.{ .title = "Resource Editor", .hidden = true }) catch |err| {
        if (err == error.NoDevice and !require_engine) {
            std.debug.print("resource-editor: {s} skipped: no GPU device ({s})\n", .{ tier, host_mod.failureReason() });
            return 0;
        }
        std.debug.print("resource-editor: {s} FAIL: the host did not start ({s}: {s})\n", .{ tier, @errorName(err), host_mod.failureReason() });
        return 1;
    };
    defer host.stop();
    if (applyMod(&host, mod)) |reason| {
        std.debug.print("resource-editor: {s} FAIL: the mod would not load: {s}\n", .{ tier, reason });
        return 1;
    }
    const passed = if (is_auto) scenario.auto(gpa, io, environ, &host, schedule_text.?, first, scratch) else scenario.smokeEdit(gpa, io, &host, first, scratch);
    return if (passed) 0 else 1;
}

/// --batch and --batch-check (batch_cli.zig): the engine on a hidden window,
/// -mod= applied, nothing drawn and none of the user's settings touched.
/// Returns the exit code. A host with no GPU device cannot start the engine
/// session: --batch-check reports a skip (0), as the other tiers do, and
/// --batch fails (3), since its work was not done.
fn batchMode(gpa: std.mem.Allocator, io: std.Io, args: []const []const u8, mod: ModRequest, self_check: bool) u8 {
    crt.attachParentConsole();
    if (!self_check) if (batch_cli.refuseArgs(args)) |code| return code;
    var host = host_mod.Host.start(.{ .title = "Resource Editor", .hidden = true }) catch |err| {
        if (err == error.NoDevice and !require_engine) {
            std.debug.print("resource-editor: batch skipped: no GPU device ({s})\n", .{host_mod.failureReason()});
            return if (self_check) 0 else 3;
        }
        std.debug.print("resource-editor: batch: the host did not start ({s}: {s})\n", .{ @errorName(err), host_mod.failureReason() });
        return 1;
    };
    defer host.stop();
    if (applyMod(&host, mod)) |reason| {
        std.debug.print("resource-editor: batch: the mod would not load: {s}\n", .{reason});
        return 1;
    }
    if (!self_check) return batch_cli.run(gpa, io, host.session, args);
    var paths = std.mem.zeroes(c.BkEditorPathSet);
    _ = c.BkEditorPaths(resSession(&host), &paths);
    return batch_cli.check(gpa, io, host.session, std.mem.sliceTo(&paths.base_root, 0), args);
}

fn check(gpa: std.mem.Allocator, io: std.Io, kind: Kind, output: []const u8, picture: ?[]const u8, mod: ModRequest) !bool {
    crt.attachParentConsole();
    if (std.fs.path.dirname(output)) |directory| try std.Io.Dir.cwd().createDirPath(io, directory);
    const output_z = try gpa.dupeSentinel(u8, output, 0);
    defer gpa.free(output_z);

    var host = host_mod.Host.start(.{ .title = "Resource Editor", .hidden = true }) catch |err| {
        if (err == error.NoDevice and !require_engine) {
            std.debug.print("resource-editor: host check skipped: no GPU device ({s})\n", .{host_mod.failureReason()});
            return true;
        }
        return fail("the host did not start ({s}: {s})", .{ @errorName(err), host_mod.failureReason() });
    };
    defer host.stop();
    if (applyMod(&host, mod)) |reason| return fail("the mod would not load: {s}", .{reason});

    const session = resSession(&host);
    if (c.BkResNew(session, @intFromEnum(kind)) != c.BK_EDITOR_OK)
        return fail("BkResNew({s}) refused: {s}", .{ kind.extension(), lastMessage(&host) });
    defer _ = c.BkResClose(session);
    var kind_back: c.BkResKind = -1;
    if (c.BkResKindOf(session, &kind_back) != c.BK_EDITOR_OK or kind_back != @intFromEnum(kind))
        return fail("BkResKindOf after BkResNew({s}) is {d}", .{ kind.extension(), kind_back });
    var tree: Tree = .{};
    if (!tree.read(session)) return fail("BkResNodes failed: {s}", .{lastMessage(&host)});
    if (tree.count == 0) return fail("the new {s} project has no root node", .{kind.extension()});
    if (tree.nodes[0].parent == tree.nodes[0].id) return fail("the root node is its own parent", .{});

    var frame: u32 = 0;
    while (frame < probe_frames) : (frame += 1) {
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
        host.beginFrame();
        drawTree(&tree, kind);
        drawProbe();
        host.endFrame() catch |err| return fail("frame {d}: {s}: {s}", .{ frame, @errorName(err), lastMessage(&host) });
    }
    // The last frame's draw data stays ImGui's until the next igNewFrame, so
    // the captured frame has the probe over it too.
    if (c.BkEditorCaptureFrame(session, output_z.ptr) != c.BK_EDITOR_OK)
        return fail("the frame was not captured: {s}", .{lastMessage(&host)});

    var width: c_int = 0;
    var height: c_int = 0;
    if (c.BkEditorScreenSize(session, &width, &height) != c.BK_EDITOR_OK)
        return fail("no screen size: {s}", .{lastMessage(&host)});
    var device: ?*anyopaque = null;
    var format: c_uint = 0;
    _ = c.BkEditorGpuDevice(session, &device, &format);
    const driver_name = if (device != null) sdl3.c.SDL_GetGPUDeviceDriver(@ptrCast(device)) else null;
    const driver: []const u8 = if (driver_name != null) std.mem.span(driver_name) else "unknown";

    const bytes = try std.Io.Dir.cwd().readFileAlloc(io, output, gpa, .limited(64 << 20));
    defer gpa.free(bytes);
    const image = Tga.parse(bytes) catch |err| return fail("{s} is not an uncompressed 32-bit TGA ({s})", .{ output, @errorName(err) });
    if (image.width != width or image.height != height)
        return fail("{s} is {d}x{d}, the screen {d}x{d}", .{ output, image.width, image.height, width, height });

    const inside_x = probe.x + probe.w / 2;
    const inside_y = probe.y + probe.h / 2;
    const inside = image.pixel(inside_x, inside_y) orelse
        return fail("the probe window's centre ({d},{d}) is outside the {d}x{d} capture", .{ inside_x, inside_y, image.width, image.height });
    if (!inside.isProbeColour())
        return fail("the probe window's centre ({d},{d}) is ({d},{d},{d}), not orange", .{ inside_x, inside_y, inside.r, inside.g, inside.b });
    // The preview scene is not begun yet (the docks half begins it), so the
    // engine's frame at the screen's centre is its clear colour; ImGui drew
    // nothing there.
    const outside_x: u32 = @intCast(@divTrunc(width, 2));
    const outside_y: u32 = @intCast(@divTrunc(height, 2));
    const outside = image.pixel(outside_x, outside_y) orelse
        return fail("the screen's centre ({d},{d}) is outside the {d}x{d} capture", .{ outside_x, outside_y, image.width, image.height });
    if (!outside.near(clear_colour))
        return fail("the screen's centre ({d},{d}) is ({d},{d},{d}), not the engine's clear colour", .{ outside_x, outside_y, outside.r, outside.g, outside.b });
    // The tree window's middle is ImGui's, not the clear colour, so a frame
    // that dropped every window but the probe fails too.
    const tree_x: u32 = @intCast(width - tree_window.w / 2 - tree_window.margin);
    const tree_y: u32 = tree_window.margin + 20 + tree_window.h / 2;
    const tree_pixel = image.pixel(tree_x, tree_y) orelse
        return fail("the tree window's middle ({d},{d}) is outside the {d}x{d} capture", .{ tree_x, tree_y, image.width, image.height });
    if (tree_pixel.near(clear_colour) or tree_pixel.isProbeColour())
        return fail("the tree window's middle ({d},{d}) is ({d},{d},{d}), not the tree window", .{ tree_x, tree_y, tree_pixel.r, tree_pixel.g, tree_pixel.b });

    if (builtin.os.tag == .macos and !host_mod.command_w_freed)
        return fail("Cmd+W still belongs to the Window menu's Close, which would quit the editor", .{});

    // The docks over the preview scene, Import and the preview's refusals
    // (docks_check.zig), in a second capture beside the first.
    const docks = if (picture) |fixture| (try docks_check.run(gpa, io, &host, kind, output, fixture)) orelse return false else null;

    std.debug.print("resource-editor: host check PASS ({s}, {d}x{d}, {s}, {d} nodes)\n", .{ driver, width, height, kind.extension(), tree.total });
    if (docks) |measured| {
        std.debug.print("resource-editor: docks PASS (import .unt from Gunner, .pcp import refused, .unt preview begun, Run refused with the reason, thumbnail ({d},{d},{d}) decoded by the engine, {d} preview samples clear)\n", .{ measured.thumbnail.r, measured.thumbnail.g, measured.thumbnail.b, measured.preview_samples });
    } else std.debug.print("resource-editor: docks half skipped: no fixture picture given\n", .{});
    return true;
}

fn smoke(gpa: std.mem.Allocator, io: std.Io, kind: Kind, output: []const u8, mod: ModRequest) !bool {
    crt.attachParentConsole();
    if (std.fs.path.dirname(output)) |directory| try std.Io.Dir.cwd().createDirPath(io, directory);
    std.Io.Dir.cwd().deleteFile(io, output) catch |err| switch (err) {
        error.FileNotFound => {},
        else => return smokeFail("{s} could not be deleted first: {s}", .{ output, @errorName(err) }),
    };
    const output_z = try gpa.dupeSentinel(u8, output, 0);
    defer gpa.free(output_z);

    var host = host_mod.Host.start(.{ .title = "Resource Editor", .hidden = true }) catch |err| {
        if (err == error.NoDevice and !require_engine) {
            std.debug.print("resource-editor: smoke skipped: no GPU device ({s})\n", .{host_mod.failureReason()});
            return true;
        }
        return smokeFail("the host did not start ({s}: {s})", .{ @errorName(err), host_mod.failureReason() });
    };
    defer host.stop();
    if (applyMod(&host, mod)) |reason| return smokeFail("the mod would not load: {s}", .{reason});

    const session = resSession(&host);
    if (c.BkResNew(session, @intFromEnum(kind)) != c.BK_EDITOR_OK)
        return smokeFail("BkResNew({s}): {s}", .{ kind.extension(), lastMessage(&host) });
    var made: Tree = .{};
    if (!made.read(session)) return smokeFail("BkResNodes on the new project: {s}", .{lastMessage(&host)});
    if (!smokeFrames(&host, &made, kind)) return false;
    if (c.BkResSave(session, output_z.ptr) != c.BK_EDITOR_OK)
        return smokeFail("BkResSave({s}): {s}", .{ output, lastMessage(&host) });
    _ = c.BkResClose(session);
    if (c.BkResOpen(session, output_z.ptr) != c.BK_EDITOR_OK)
        return smokeFail("BkResOpen({s}): {s}", .{ output, lastMessage(&host) });
    defer _ = c.BkResClose(session);
    var kind_back: c.BkResKind = -1;
    if (c.BkResKindOf(session, &kind_back) != c.BK_EDITOR_OK or kind_back != @intFromEnum(kind))
        return smokeFail("the reopened project's kind is {d}, not {s}", .{ kind_back, kind.extension() });
    var reopened: Tree = .{};
    if (!reopened.read(session)) return smokeFail("BkResNodes on the reopened project: {s}", .{lastMessage(&host)});
    if (reopened.total != made.total)
        return smokeFail("the reopened project has {d} nodes, the new one {d}", .{ reopened.total, made.total });
    if (!smokeFrames(&host, &reopened, kind)) return false;
    std.debug.print("resource-editor: smoke PASS ({s}, {d} nodes, {s})\n", .{ kind.extension(), made.total, output });
    return true;
}

fn smokeFrames(host: *host_mod.Host, tree: *const Tree, kind: Kind) bool {
    var frame: u32 = 0;
    while (frame < probe_frames) : (frame += 1) {
        var event: sdl3.c.SDL_Event = undefined;
        while (sdl3.c.SDL_PollEvent(&event)) _ = host.handleEvent(&event);
        host.beginFrame();
        var quit = false;
        _ = drawMenu(&quit);
        drawTree(tree, kind);
        host.endFrame() catch |err| return smokeFail("frame {d}: {s}: {s}", .{ frame, @errorName(err), lastMessage(host) });
    }
    return true;
}

fn startupStepName(err: host_mod.HostError) []const u8 {
    return switch (err) {
        error.SdlInitFailed => "SDL init",
        error.WindowFailed => "the window",
        error.EngineFailed => "the engine start",
        error.NoDevice => "no GPU device",
        error.ImguiFailed => "ImGui",
        error.FrameFailed => "the frame",
    };
}

/// Names the step that failed through the platform's own message box and
/// exits; the automated modes print to stderr instead (`fail`).
fn fatal(step: []const u8, reason: []const u8) noreturn {
    var buffer: [768]u8 = undefined;
    const message = std.mem.printSentinel(&buffer, "{s} failed: {s}", .{ step, reason }, 0) catch "Resource Editor failed to start";
    std.debug.print("resource-editor: {s}\n", .{message});
    _ = sdl3.c.SDL_ShowSimpleMessageBox(sdl3.c.SDL_MESSAGEBOX_ERROR, "Resource Editor", message, null);
    std.process.exit(1);
}

// The C main mainCRTStartup calls on Windows (crt.zig minimalFromPeb says why).
comptime {
    if (crt.exports_c_main) @export(&crtMain, .{ .name = "main" });
}

fn crtMain(argc: c_int, argv: ?*anyopaque) callconv(.c) c_int {
    _ = argc;
    _ = argv;
    main(crt.minimalFromPeb()) catch |err| {
        std.debug.print("resource-editor: {s}\n", .{@errorName(err)});
        return 1;
    };
    return 0;
}

fn usage() noreturn {
    crt.attachParentConsole();
    std.debug.print("usage: ResourceEditor [-mod=<Folder>|-mod=None] [--hidden] [<kind>|<project>]\n       ResourceEditor [-mod=...] --check [<kind>] [<out.tga>] [<picture.tga>]\n       ResourceEditor [-mod=...] --smoke [<kind>] [<out.<ext>>]\n       ResourceEditor [-mod=...] --smoke-edit <project.unt> <scratch>\n       ResourceEditor [-mod=...] --auto <fixtures> <scratch>  (BK_EDITOR_AUTO)\n       " ++ batch_cli.usage_line ++ "\n", .{});
    std.process.exit(2);
}

fn fail(comptime format: []const u8, args: anytype) bool {
    std.debug.print("resource-editor: host check FAIL: " ++ format ++ "\n", args);
    return false;
}

fn smokeFail(comptime format: []const u8, args: anytype) bool {
    std.debug.print("resource-editor: smoke FAIL: " ++ format ++ "\n", args);
    return false;
}

pub fn drawProbe() void {
    imgui.c.igSetNextWindowPos(.{ .x = probe.x, .y = probe.y }, imgui.c.ImGuiCond_Always);
    imgui.c.igSetNextWindowSize(.{ .x = probe.w, .y = probe.h }, imgui.c.ImGuiCond_Always);
    // Orange, as in MapEditor's check: its channels all differ, so a
    // red/blue swap in the readback fails isProbeColour.
    imgui.c.igPushStyleColorImVec4(imgui.c.ImGuiCol_WindowBg, .{ .x = 1, .y = 128.0 / 255.0, .z = 0, .w = 1 });
    _ = imgui.c.igBegin("probe", null, imgui.c.ImGuiWindowFlags_NoDecoration | imgui.c.ImGuiWindowFlags_NoMove | imgui.c.ImGuiWindowFlags_NoSavedSettings);
    imgui.c.igEnd();
    imgui.c.igPopStyleColor();
}

pub const Rgb = struct {
    r: u8,
    g: u8,
    b: u8,

    pub fn near(self: Rgb, other: Rgb) bool {
        return close(self.r, other.r) and close(self.g, other.g) and close(self.b, other.b);
    }

    fn close(a: u8, b: u8) bool {
        return @abs(@as(i16, a) - @as(i16, b)) <= 2;
    }

    pub fn isProbeColour(self: Rgb) bool {
        return self.r > 200 and self.g > 100 and self.g < 160 and self.b < 40;
    }
};

/// What DrawSessionFrame clears to before the scene is drawn.
pub const clear_colour = Rgb{ .r = 0, .g = 0, .b = 0 };

/// An uncompressed 32-bit TGA: an 18-byte header, an optional ID, then BGRA
/// rows, bottom row first unless bit 5 of the descriptor (byte 17) is set.
pub const Tga = struct {
    width: u32,
    height: u32,
    top_first: bool,
    pixels: []const u8,

    pub fn parse(bytes: []const u8) !Tga {
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

    pub fn pixel(self: Tga, x: u32, y: u32) ?Rgb {
        if (x >= self.width or y >= self.height) return null;
        const row = if (self.top_first) y else self.height - 1 - y;
        const i = (@as(usize, row) * self.width + x) * 4;
        return .{ .r = self.pixels[i + 2], .g = self.pixels[i + 1], .b = self.pixels[i] };
    }
};
