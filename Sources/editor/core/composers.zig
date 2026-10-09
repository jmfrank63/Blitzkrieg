//! The Containers and Graphs Composers' working state (M3 05-09): the open
//! file of each (an `rmg.Document`: its own undo and dirty flag, never the
//! map's history), the folder scans their Open lists show, the last Check!
//! report, the canvas gesture state and the patch copy-in that waits for its
//! YES/NO. Std-only over the Editor (so the fake bridge tests all of it); the
//! ImGui windows in panels_m3.zig and the named commands in commands.zig are
//! thin layers over these functions - a button and a BK_EDITOR_AUTO `do=` run
//! the same code.
//!
//! Names: a command or a file row speaks in names relative to the kind's own
//! folder ("common\road_cross_asph_we"); `fullName` puts the folder in front
//! unless the name already starts with "scenarios\".

const std = @import("std");
const bridge_mod = @import("bridge.zig");
const rmg = @import("rmg.zig");
const records = @import("records.zig");
const editor_mod = @import("editor.zig");

const Allocator = std.mem.Allocator;
const Editor = editor_mod.Editor;

pub const container_folder = "scenarios\\containers\\";
pub const graph_folder = "scenarios\\graphs\\";
pub const patch_folder = "scenarios\\patches\\";
pub const field_folder = "scenarios\\fieldsets\\";
pub const template_folder = "scenarios\\templates\\";

/// A name as the storages spell it: lower-cased and with backslashes by the
/// bridge, so the comparison here is too. Unchanged when it already starts at
/// "scenarios\"; otherwise `folder` goes in front. Null when it does not fit.
pub fn fullName(buffer: []u8, folder: []const u8, name: []const u8) ?[]const u8 {
    var text = name;
    if (text.len > 4 and std.ascii.eqlIgnoreCase(text[text.len - 4 ..], ".xml")) text = text[0 .. text.len - 4];
    const prefix = if (startsWithName(text, "scenarios\\")) "" else folder;
    if (prefix.len + text.len > buffer.len) return null;
    @memcpy(buffer[0..prefix.len], prefix);
    for (text, 0..) |byte, i| buffer[prefix.len + i] = if (byte == '/') '\\' else byte;
    return buffer[0 .. prefix.len + text.len];
}

fn startsWithName(text: []const u8, prefix: []const u8) bool {
    if (text.len < prefix.len) return false;
    for (prefix, text[0..prefix.len]) |want, got| {
        const g = if (got == '/') '\\' else got;
        if (std.ascii.toLower(g) != want) return false;
    }
    return true;
}

/// The name a list shows: the kind's folder taken off the front.
pub fn relativeName(folder: []const u8, full: []const u8) []const u8 {
    return if (startsWithName(full, folder)) full[folder.len..] else full;
}

/// A patch property edit's cell: leave it, clear it or set it (the MFC's
/// tri-state check box when several patches are selected).
pub const Tri = enum { keep, off, on };

pub const LinkField = enum {
    kind,
    desc,
    /// Cells (the dialog's own unit: world units / 32).
    radius,
    parts,
    /// Cells.
    min_length,
    /// The dialog's "Width", 0..1.
    distance,
    disturbance,

    pub fn fromName(name: []const u8) ?LinkField {
        inline for (comptime std.enums.values(LinkField)) |field| {
            if (std.mem.eql(u8, name, @tagName(field))) return field;
        }
        return null;
    }
};

pub const SaveResult = enum { saved, needs_save_as, failed };

pub const PendingImport = struct {
    path: [1024]u8 = undefined,
    path_len: usize = 0,
    dest: bridge_mod.RmgName = .{},
    active: bool = false,

    pub fn pathSlice(self: *const PendingImport) []const u8 {
        return self.path[0..self.path_len];
    }
};

/// Whether an object name is one of the database's: the app hands the object
/// catalogue it already holds (the bridge's core vtable has no catalogue), tests
/// hand a table. Without one every name counts as known.
pub const ObjectLookup = struct {
    ctx: *anyopaque,
    has_fn: *const fn (ctx: *anyopaque, name: []const u8) bool,
};

pub const Composers = struct {
    allocator: Allocator,
    cdoc: rmg.ContainerDoc,
    gdoc: rmg.GraphDoc,
    /// The Fields Composer (05-10): its open file, the scan its Open list shows,
    /// the last Check! report.
    fdoc: rmg.FieldSetDoc,
    field_names: std.ArrayListUnmanaged([]u8) = .empty,
    field_report: ?rmg.Report = null,
    /// The Templates Composer (05-10): the same three.
    tdoc: rmg.TemplateDoc,
    template_names: std.ArrayListUnmanaged([]u8) = .empty,
    template_report: ?rmg.Report = null,
    object_lookup: ?ObjectLookup = null,
    /// The scans the Open lists and the patch picker show (full storage names).
    container_names: std.ArrayListUnmanaged([]u8) = .empty,
    graph_names: std.ArrayListUnmanaged([]u8) = .empty,
    patch_names: std.ArrayListUnmanaged([]u8) = .empty,
    container_report: ?rmg.Report = null,
    graph_report: ?rmg.Report = null,
    canvas: rmg.Canvas = .{},
    pending_import: PendingImport = .{},
    /// What the last add, check or gesture said, for the window's status line.
    message_buffer: [768]u8 = undefined,
    message_len: usize = 0,
    /// Bumped by every change of either file or report: windows that cache
    /// rows compare it.
    generation: u32 = 1,
    /// Whether the Open lists were read this session (the windows read them
    /// when first opened, and after every save).
    scanned: bool = false,

    pub fn init(allocator: Allocator) Composers {
        return .{ .allocator = allocator, .cdoc = rmg.ContainerDoc.init(allocator), .gdoc = rmg.GraphDoc.init(allocator), .fdoc = rmg.FieldSetDoc.init(allocator), .tdoc = rmg.TemplateDoc.init(allocator) };
    }

    pub fn deinit(self: *Composers) void {
        self.cdoc.deinit();
        self.gdoc.deinit();
        self.fdoc.deinit();
        self.tdoc.deinit();
        freeList(self.allocator, &self.template_names);
        if (self.template_report) |*report| report.deinit(self.allocator);
        freeList(self.allocator, &self.field_names);
        if (self.field_report) |*report| report.deinit(self.allocator);
        freeList(self.allocator, &self.container_names);
        freeList(self.allocator, &self.graph_names);
        freeList(self.allocator, &self.patch_names);
        if (self.container_report) |*report| report.deinit(self.allocator);
        if (self.graph_report) |*report| report.deinit(self.allocator);
        self.canvas.deinit(self.allocator);
    }

    fn freeList(allocator: Allocator, list: *std.ArrayListUnmanaged([]u8)) void {
        for (list.items) |name| allocator.free(name);
        list.deinit(allocator);
        list.* = .empty;
    }

    pub fn message(self: *const Composers) []const u8 {
        return self.message_buffer[0..self.message_len];
    }

    fn say(self: *Composers, comptime format: []const u8, args: anytype) void {
        const text = std.fmt.bufPrint(&self.message_buffer, format, args) catch self.message_buffer[0..];
        self.message_len = text.len;
    }

    fn sayText(self: *Composers, text: []const u8) void {
        const len = @min(text.len, self.message_buffer.len);
        @memcpy(self.message_buffer[0..len], text[0..len]);
        self.message_len = len;
    }

    fn clearContainerReport(self: *Composers) void {
        if (self.container_report) |*report| report.deinit(self.allocator);
        self.container_report = null;
        self.generation +%= 1;
    }

    fn clearGraphReport(self: *Composers) void {
        if (self.graph_report) |*report| report.deinit(self.allocator);
        self.graph_report = null;
        self.generation +%= 1;
    }

    // --- Scans ---------------------------------------------------------

    fn scanKind(self: *Composers, editor: *Editor, kind: bridge_mod.RmgKind, list: *std.ArrayListUnmanaged([]u8)) Allocator.Error!void {
        freeList(self.allocator, list);
        var total: usize = 0;
        _ = editor.bridge.listRmg(kind, &.{}, &total);
        if (total == 0) return;
        const entries = try self.allocator.alloc(bridge_mod.RmgName, total);
        defer self.allocator.free(entries);
        var got: usize = 0;
        if (editor.bridge.listRmg(kind, entries, &got) != .ok) return;
        try list.ensureTotalCapacity(self.allocator, got);
        for (entries[0..@min(got, entries.len)]) |entry| list.appendAssumeCapacity(try self.allocator.dupe(u8, entry.nameSlice()));
        self.generation +%= 1;
    }

    /// The Open lists: the containers and graphs the storages hold, shipped and
    /// the user's alike (D-08: a scan, never Default*.xml).
    pub fn refreshNames(self: *Composers, editor: *Editor) Allocator.Error!void {
        try self.scanKind(editor, .containers, &self.container_names);
        try self.scanKind(editor, .graphs, &self.graph_names);
        try self.scanKind(editor, .field_sets, &self.field_names);
        try self.scanKind(editor, .templates, &self.template_names);
        self.scanned = true;
    }

    pub fn ensureScanned(self: *Composers, editor: *Editor) void {
        if (!self.scanned) self.refreshNames(editor) catch {};
    }

    /// The patch maps the storages hold under Scenarios\Patches, .bzm and .xml
    /// together, extension off (the picker's list).
    pub fn refreshPatches(self: *Composers, editor: *Editor) Allocator.Error!void {
        freeList(self.allocator, &self.patch_names);
        var seen = std.StringHashMapUnmanaged(void).empty;
        defer seen.deinit(self.allocator);
        for ([_][:0]const u8{ ".bzm", ".xml" }) |extension| {
            var total: usize = 0;
            _ = editor.bridge.listStorageFiles(patch_folder, extension.ptr, &.{}, &total);
            if (total == 0) continue;
            const entries = try self.allocator.alloc(bridge_mod.RmgName, total);
            defer self.allocator.free(entries);
            var got: usize = 0;
            if (editor.bridge.listStorageFiles(patch_folder, extension.ptr, entries, &got) != .ok) continue;
            for (entries[0..@min(got, entries.len)]) |entry| {
                const full = entry.nameSlice();
                const bare = full[0 .. full.len - extension.len];
                if (seen.contains(bare)) continue;
                const copy = try self.allocator.dupe(u8, bare);
                errdefer self.allocator.free(copy);
                try seen.put(self.allocator, copy, {});
                try self.patch_names.append(self.allocator, copy);
            }
        }
        std.mem.sort([]u8, self.patch_names.items, {}, struct {
            fn less(_: void, a: []u8, b: []u8) bool {
                return std.mem.lessThan(u8, a, b);
            }
        }.less);
        self.generation +%= 1;
    }

    // --- Containers ----------------------------------------------------

    pub fn newContainer(self: *Composers) Allocator.Error!void {
        try self.cdoc.load("", .{}, false);
        self.clearContainerReport();
        self.message_len = 0;
    }

    /// Open: the named container through the storages (the user's root first).
    pub fn openContainer(self: *Composers, editor: *Editor, name: []const u8) !void {
        var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
        const full = fullName(&buffer, container_folder, name) orelse return error.Refused;
        var container = editor.readContainer(full) catch |err| {
            self.sayText(editor.status());
            return err;
        };
        errdefer container.deinit(self.allocator);
        try self.cdoc.load(full, container, false);
        self.clearContainerReport();
        self.message_len = 0;
    }

    /// Save over the file's own name. A shipped name is read-only: the answer
    /// is `needs_save_as` (the window then asks for a name), nothing written.
    pub fn saveContainer(self: *Composers, editor: *Editor) Allocator.Error!SaveResult {
        if (self.cdoc.name.len == 0) {
            self.say("a new container has no file yet: Save As", .{});
            return .needs_save_as;
        }
        return self.writeContainerTo(editor, try self.allocator.dupe(u8, self.cdoc.name));
    }

    pub fn saveContainerAs(self: *Composers, editor: *Editor, name: []const u8) Allocator.Error!SaveResult {
        var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
        const full = fullName(&buffer, container_folder, name) orelse {
            self.say("that name is too long", .{});
            return .failed;
        };
        return self.writeContainerTo(editor, try self.allocator.dupe(u8, full));
    }

    fn writeContainerTo(self: *Composers, editor: *Editor, owned_name: []u8) Allocator.Error!SaveResult {
        defer self.allocator.free(owned_name);
        editor.writeContainer(owned_name, &self.cdoc.current) catch |err| {
            self.sayText(editor.status());
            if (err == error.OutOfMemory) return error.OutOfMemory;
            if (std.mem.indexOf(u8, editor.status(), "Save As") != null) {
                self.cdoc.shipped = true;
                return .needs_save_as;
            }
            return .failed;
        };
        try self.cdoc.markSaved(owned_name);
        try self.refreshNames(editor);
        self.say("saved {s}", .{owned_name});
        return .saved;
    }

    /// Add patches from the storages (D-10's first path). Each is checked the
    /// MFC's way; the first that does not belong stops the batch and says why
    /// (earlier ones stay, as one undo step). Returns how many were added.
    pub fn addPatches(self: *Composers, editor: *Editor, names: []const []const u8) !usize {
        if (names.len == 0) return 0;
        const container = try self.cdoc.begin();
        var added: usize = 0;
        for (names) |name| {
            var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
            const full = fullName(&buffer, patch_folder, name) orelse {
                self.say("the patch name is too long", .{});
                break;
            };
            var outcome = try rmg.addPatchChecked(self.allocator, container, editor.rmgSource(), full);
            defer outcome.deinit(self.allocator);
            switch (outcome) {
                .added => added += 1,
                .unreadable => {
                    self.say("Can't Add Patch to Container! Patch <{s}> does not load as a map.", .{full});
                    break;
                },
                .mismatch => |text| {
                    self.say("Can't Add Patch to Container! Patch <{s}>: {s}", .{ full, text });
                    break;
                },
            }
        }
        if (added == 0) self.cdoc.cancel() else self.clearContainerReport();
        // A batch that stopped keeps its refusal text; a clean one says nothing.
        if (added == names.len) self.message_len = 0;
        return added;
    }

    /// D-10's second path: a map outside the storages. Validates it and names
    /// the destination (nothing is copied yet); the window then asks YES/NO.
    pub fn beginImport(self: *Composers, editor: *Editor, host_path: []const u8) !void {
        var dest: bridge_mod.RmgName = .{};
        editor.importPatch(host_path, false, &dest) catch |err| {
            self.sayText(editor.status());
            return err;
        };
        if (host_path.len > self.pending_import.path.len) return error.Refused;
        @memcpy(self.pending_import.path[0..host_path.len], host_path);
        self.pending_import.path_len = host_path.len;
        self.pending_import.dest = dest;
        self.pending_import.active = true;
        self.generation +%= 1;
    }

    /// YES: the copy goes in and the container lists it.
    pub fn confirmImport(self: *Composers, editor: *Editor) !usize {
        if (!self.pending_import.active) return error.Refused;
        defer self.pending_import.active = false;
        var done: bridge_mod.RmgName = .{};
        editor.importPatch(self.pending_import.pathSlice(), true, &done) catch |err| {
            self.sayText(editor.status());
            return err;
        };
        var names = [_][]const u8{done.nameSlice()};
        const added = try self.addPatches(editor, &names);
        try self.refreshPatches(editor);
        return added;
    }

    pub fn cancelImport(self: *Composers) void {
        self.pending_import.active = false;
        self.generation +%= 1;
    }

    pub fn deletePatches(self: *Composers, indices: []const usize) Allocator.Error!void {
        if (indices.len == 0) return;
        const container = try self.cdoc.begin();
        container.removePatches(self.allocator, indices);
        self.clearContainerReport();
    }

    /// Patch properties: the setting and the four direction cells for every
    /// index given (multi-edit: a `keep` cell is left as each patch has it).
    /// `place` null leaves the setting; "<any setting>" or "" clears it.
    pub fn setPatchProperties(self: *Composers, indices: []const usize, place: ?[]const u8, flags: [4]Tri) Allocator.Error!void {
        if (indices.len == 0) return;
        const container = try self.cdoc.begin();
        for (indices) |index| {
            if (index >= container.patches.items.len) continue;
            if (place) |text| {
                const any = std.mem.eql(u8, text, "<any setting>") or text.len == 0;
                try container.setPlace(self.allocator, index, if (any) "" else text);
            }
            for (flags, 0..) |flag, d| switch (flag) {
                .keep => {},
                .off => try container.setDirection(self.allocator, index, @enumFromInt(d), false),
                .on => try container.setDirection(self.allocator, index, @enumFromInt(d), true),
            };
        }
        self.clearContainerReport();
    }

    pub fn checkContainer(self: *Composers, editor: *Editor) Allocator.Error!usize {
        self.clearContainerReport();
        self.container_report = try rmg.checkContainer(self.allocator, &self.cdoc.current, editor.rmgSource());
        self.generation +%= 1;
        const report = &self.container_report.?;
        self.say("Check!: {d} errors, {d} findings", .{ report.errorCount(), report.findings.items.len });
        return report.findings.items.len;
    }

    /// One finding's fix, one undo step, then the check runs again.
    pub fn fixContainerFinding(self: *Composers, editor: *Editor, index: usize) !void {
        const report = &(self.container_report orelse return error.Refused);
        if (index >= report.findings.items.len or report.findings.items[index].fix == .none) return error.Refused;
        const fix = report.findings.items[index].fix;
        const container = try self.cdoc.begin();
        try rmg.applyContainerFix(self.allocator, container, editor.rmgSource(), fix);
        _ = try self.checkContainer(editor);
    }

    pub fn fixContainerAll(self: *Composers, editor: *Editor) !usize {
        const report = &(self.container_report orelse return error.Refused);
        const container = try self.cdoc.begin();
        const fixed = try rmg.fixAllContainer(self.allocator, container, editor.rmgSource(), report);
        if (fixed == 0) self.cdoc.cancel();
        _ = try self.checkContainer(editor);
        return fixed;
    }

    pub fn undoContainer(self: *Composers) Allocator.Error!bool {
        const done = try self.cdoc.undo();
        if (done) self.clearContainerReport();
        return done;
    }

    pub fn redoContainer(self: *Composers) Allocator.Error!bool {
        const done = try self.cdoc.redo();
        if (done) self.clearContainerReport();
        return done;
    }

    // --- Graphs --------------------------------------------------------

    pub fn newGraph(self: *Composers) Allocator.Error!void {
        try self.gdoc.load("", .{}, false);
        self.clearGraphReport();
        self.canvas.cancel(self.allocator, &self.gdoc);
        self.message_len = 0;
    }

    pub fn openGraph(self: *Composers, editor: *Editor, name: []const u8) !void {
        var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
        const full = fullName(&buffer, graph_folder, name) orelse return error.Refused;
        var graph = editor.readGraph(full) catch |err| {
            self.sayText(editor.status());
            return err;
        };
        errdefer graph.deinit(self.allocator);
        graph.refreshSize();
        try self.gdoc.load(full, graph, false);
        self.canvas.cancel(self.allocator, &self.gdoc);
        self.canvas.fitTo(&self.gdoc.current);
        self.clearGraphReport();
        self.message_len = 0;
    }

    pub fn saveGraph(self: *Composers, editor: *Editor) Allocator.Error!SaveResult {
        if (self.gdoc.name.len == 0) {
            self.say("a new graph has no file yet: Save As", .{});
            return .needs_save_as;
        }
        return self.writeGraphTo(editor, try self.allocator.dupe(u8, self.gdoc.name));
    }

    pub fn saveGraphAs(self: *Composers, editor: *Editor, name: []const u8) Allocator.Error!SaveResult {
        var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
        const full = fullName(&buffer, graph_folder, name) orelse {
            self.say("that name is too long", .{});
            return .failed;
        };
        return self.writeGraphTo(editor, try self.allocator.dupe(u8, full));
    }

    fn writeGraphTo(self: *Composers, editor: *Editor, owned_name: []u8) Allocator.Error!SaveResult {
        defer self.allocator.free(owned_name);
        self.gdoc.current.refreshSize();
        editor.writeGraph(owned_name, &self.gdoc.current) catch |err| {
            self.sayText(editor.status());
            if (err == error.OutOfMemory) return error.OutOfMemory;
            if (std.mem.indexOf(u8, editor.status(), "Save As") != null) {
                self.gdoc.shipped = true;
                return .needs_save_as;
            }
            return .failed;
        };
        try self.gdoc.markSaved(owned_name);
        try self.refreshNames(editor);
        self.say("saved {s}", .{owned_name});
        return .saved;
    }

    /// One canvas drag, press to release, in tile coordinates (y up) - the
    /// mouse path the window drives event by event, here as one call so a
    /// script can run it. `half_width` is the link hit tolerance in tiles.
    pub fn gesture(self: *Composers, from: rmg.Tile, to: rmg.Tile, ctrl: bool, half_width: f32) Allocator.Error!rmg.CanvasOutcome {
        try self.canvas.press(self.allocator, &self.gdoc, from, ctrl, half_width);
        self.canvas.drag(&self.gdoc, to);
        return self.finishGesture(to);
    }

    pub fn finishGesture(self: *Composers, to: rmg.Tile) Allocator.Error!rmg.CanvasOutcome {
        const outcome = try self.canvas.release(self.allocator, &self.gdoc, to);
        switch (outcome) {
            .node_added, .node_moved, .node_resized, .link_added => self.clearGraphReport(),
            .reverted => self.say("the node would overlap another: put back", .{}),
            .rejected => self.say("nothing added: a node needs a patch each way and no overlap; a link two different nodes", .{}),
            .none => {},
        }
        self.canvas.fitTo(&self.gdoc.current);
        return outcome;
    }

    /// Node properties' OK: the container (path from Browse or typed). The
    /// MFC's refusals are the message; the size warning is said and the
    /// change kept.
    pub fn setNodeContainer(self: *Composers, editor: *Editor, index: usize, name: []const u8) !void {
        if (index >= self.gdoc.current.nodes.items.len) return error.Refused;
        var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
        const full = if (name.len == 0) name else (fullName(&buffer, container_folder, name) orelse return error.Refused);
        const graph = try self.gdoc.begin();
        var outcome = try rmg.setNodeContainer(self.allocator, graph, index, full, editor.rmgSource());
        defer outcome.deinit(self.allocator);
        switch (outcome) {
            .set => {
                self.clearGraphReport();
                if (full.len != 0) {
                    if (editor.rmgSource().container(self.allocator, full)) |summary| {
                        var info = summary;
                        defer info.deinit(self.allocator);
                        if (!rmg.containerFitsNode(graph.nodes.items[index].rect, info.size_x, info.size_y)) {
                            self.say("Warning! Possibly invalid size in some directions: node {d} is {d}x{d} tiles, container <{s}> is {d}x{d} patches", .{ index, graph.nodes.items[index].rect.width(), graph.nodes.items[index].rect.height(), full, info.size_x, info.size_y });
                            return;
                        }
                    }
                }
                self.message_len = 0;
            },
            .unreadable => {
                self.gdoc.cancel();
                self.say("container <{s}> does not load as a container", .{full});
                return error.Refused;
            },
            .mismatch => |text| {
                self.gdoc.cancel();
                self.say("Can't Add Container to Graph! Container <{s}>: {s}", .{ full, text });
                return error.Refused;
            },
        }
    }

    pub fn deleteNode(self: *Composers, index: usize) Allocator.Error!void {
        if (index >= self.gdoc.current.nodes.items.len) return;
        const graph = try self.gdoc.begin();
        graph.removeNode(self.allocator, index);
        self.clearGraphReport();
    }

    pub fn deleteLink(self: *Composers, index: usize) Allocator.Error!void {
        if (index >= self.gdoc.current.links.items.len) return;
        const graph = try self.gdoc.begin();
        graph.removeLink(self.allocator, index);
        self.clearGraphReport();
    }

    /// One link field, as the properties dialog's edit commits it: the radius
    /// and min length in cells, the others as they are. A value that does not
    /// parse changes nothing.
    pub fn setLinkField(self: *Composers, index: usize, field: LinkField, text: []const u8) !void {
        if (index >= self.gdoc.current.links.items.len) return error.Refused;
        var parsed_float: f32 = 0;
        var parsed_int: i32 = 0;
        switch (field) {
            .kind, .parts => {
                parsed_int = std.fmt.parseInt(i32, text, 10) catch return error.Refused;
                if (field == .kind and (parsed_int < rmg.link_road or parsed_int > rmg.link_river)) return error.Refused;
                if (field == .parts and (parsed_int < 0 or parsed_int > 100000)) return error.Refused;
            },
            .radius, .min_length, .distance, .disturbance => {
                parsed_float = std.fmt.parseFloat(f32, text) catch return error.Refused;
                if (!std.math.isFinite(parsed_float)) return error.Refused;
            },
            .desc => {},
        }
        const graph = try self.gdoc.begin();
        const link = &graph.links.items[index];
        switch (field) {
            .kind => link.kind = parsed_int,
            .parts => link.parts = parsed_int,
            .radius => link.radius = parsed_float * rmg.world_cell,
            .min_length => link.min_length = parsed_float * rmg.world_cell,
            .distance => link.distance = parsed_float,
            .disturbance => link.disturbance = parsed_float,
            .desc => {
                const copy = try self.allocator.dupe(u8, text);
                self.allocator.free(link.desc);
                link.desc = copy;
            },
        }
        self.clearGraphReport();
    }

    pub fn checkGraph(self: *Composers, editor: *Editor) Allocator.Error!usize {
        self.clearGraphReport();
        self.graph_report = try rmg.checkGraph(self.allocator, &self.gdoc.current, editor.rmgSource());
        self.generation +%= 1;
        const report = &self.graph_report.?;
        self.say("Check!: {d} errors, {d} findings", .{ report.errorCount(), report.findings.items.len });
        return report.findings.items.len;
    }

    pub fn fixGraphFinding(self: *Composers, editor: *Editor, index: usize) !void {
        const report = &(self.graph_report orelse return error.Refused);
        if (index >= report.findings.items.len or report.findings.items[index].fix == .none) return error.Refused;
        const fix = report.findings.items[index].fix;
        const graph = try self.gdoc.begin();
        try rmg.applyGraphFix(self.allocator, graph, editor.rmgSource(), fix);
        _ = try self.checkGraph(editor);
    }

    pub fn fixGraphAll(self: *Composers, editor: *Editor) !usize {
        const report = &(self.graph_report orelse return error.Refused);
        const graph = try self.gdoc.begin();
        const fixed = try rmg.fixAllGraph(self.allocator, graph, editor.rmgSource(), report);
        if (fixed == 0) self.gdoc.cancel();
        _ = try self.checkGraph(editor);
        return fixed;
    }

    /// The supported settings of one graph (SRMGraph::GetSupportedSettings): every
    /// node's container is read; "<any setting>" alone when all of them take any,
    /// else the settings every node that is not "any" supports. Empty when a node
    /// is empty or its container does not load, or there are no nodes (the C++
    /// answers 0). Sorted; the caller frees with `rmg.freeNames`.
    pub fn graphSettingList(self: *Composers, editor: *Editor, graph: *const rmg.Graph) Allocator.Error![][]u8 {
        var out = std.ArrayListUnmanaged([]u8).empty;
        errdefer {
            for (out.items) |item| self.allocator.free(item);
            out.deinit(self.allocator);
        }
        const nodes = graph.nodes.items;
        if (nodes.len == 0) return try out.toOwnedSlice(self.allocator);
        var counts = std.StringArrayHashMapUnmanaged(usize).empty;
        defer {
            for (counts.keys()) |key| self.allocator.free(key);
            counts.deinit(self.allocator);
        }
        var any_nodes: usize = 0;
        for (nodes) |node| {
            if (node.container.len == 0) return try out.toOwnedSlice(self.allocator);
            var container = editor.readContainer(node.container) catch return try out.toOwnedSlice(self.allocator);
            defer container.deinit(self.allocator);
            const settings = try container.supportedSettings(self.allocator);
            defer rmg.freeNames(self.allocator, settings);
            if (settings.len == 0) continue;
            var takes_any = false;
            for (settings) |name| {
                if (std.mem.eql(u8, name, "<any setting>")) takes_any = true;
            }
            if (takes_any) {
                any_nodes += 1;
                continue;
            }
            for (settings) |name| {
                if (counts.getPtr(name)) |slot| {
                    slot.* += 1;
                } else {
                    try counts.put(self.allocator, try self.allocator.dupe(u8, name), 1);
                }
            }
        }
        if (any_nodes == nodes.len) {
            try out.append(self.allocator, try self.allocator.dupe(u8, "<any setting>"));
            return try out.toOwnedSlice(self.allocator);
        }
        for (counts.keys(), counts.values()) |name, count| {
            if (count >= nodes.len - any_nodes) try out.append(self.allocator, try self.allocator.dupe(u8, name));
        }
        std.mem.sort([]u8, out.items, {}, struct {
            fn less(_: void, a: []u8, b: []u8) bool {
                return std.mem.lessThan(u8, a, b);
            }
        }.less);
        return try out.toOwnedSlice(self.allocator);
    }

    /// The open graph's supported settings joined with "; " (the list's column).
    pub fn graphSettingsText(self: *Composers, editor: *Editor, out: *std.ArrayListUnmanaged(u8)) Allocator.Error!void {
        out.clearRetainingCapacity();
        const names = try self.graphSettingList(editor, &self.gdoc.current);
        defer rmg.freeNames(self.allocator, names);
        for (names, 0..) |name, i| {
            if (i != 0) try out.appendSlice(self.allocator, "; ");
            try out.appendSlice(self.allocator, name);
        }
    }

    /// The open template's supported settings (SRMTemplate::GetSupportedSettings):
    /// every graph of weight above 0 is read (one that does not load makes it empty);
    /// "<any setting>" alone when every one takes any, else the settings every one
    /// that is not "any" supports. Joined with "; ", sorted.
    pub fn templateSettingsText(self: *Composers, editor: *Editor, out: *std.ArrayListUnmanaged(u8)) Allocator.Error!void {
        out.clearRetainingCapacity();
        var counts = std.StringArrayHashMapUnmanaged(usize).empty;
        defer {
            for (counts.keys()) |key| self.allocator.free(key);
            counts.deinit(self.allocator);
        }
        var graphs: usize = 0;
        var any_graphs: usize = 0;
        for (self.tdoc.current.graphs.items) |entry| {
            if (entry.weight <= 0) continue;
            var graph = editor.readGraphQuiet(entry.name) orelse return;
            defer graph.deinit(self.allocator);
            const names = try self.graphSettingList(editor, &graph);
            defer rmg.freeNames(self.allocator, names);
            var takes_any = false;
            for (names) |name| {
                if (std.mem.eql(u8, name, "<any setting>")) takes_any = true;
            }
            if (names.len != 0) {
                if (takes_any) {
                    any_graphs += 1;
                } else for (names) |name| {
                    if (counts.getPtr(name)) |slot| {
                        slot.* += 1;
                    } else {
                        try counts.put(self.allocator, try self.allocator.dupe(u8, name), 1);
                    }
                }
            }
            graphs += 1;
        }
        if (graphs == 0) return;
        if (any_graphs == graphs) {
            try out.appendSlice(self.allocator, "<any setting>");
            return;
        }
        var names = std.ArrayListUnmanaged([]const u8).empty;
        defer names.deinit(self.allocator);
        for (counts.keys(), counts.values()) |name, count| {
            if (count >= graphs - any_graphs) try names.append(self.allocator, name);
        }
        std.mem.sort([]const u8, names.items, {}, struct {
            fn less(_: void, a: []const u8, b: []const u8) bool {
                return std.mem.lessThan(u8, a, b);
            }
        }.less);
        for (names.items, 0..) |name, i| {
            if (i != 0) try out.appendSlice(self.allocator, "; ");
            try out.appendSlice(self.allocator, name);
        }
    }

    pub fn undoGraph(self: *Composers) Allocator.Error!bool {
        const done = try self.gdoc.undo();
        if (done) self.clearGraphReport();
        return done;
    }

    pub fn redoGraph(self: *Composers) Allocator.Error!bool {
        const done = try self.gdoc.redo();
        if (done) self.clearGraphReport();
        return done;
    }

    // --- Field sets (05-10) ---------------------------------------------

    fn clearFieldReport(self: *Composers) void {
        if (self.field_report) |*report| report.deinit(self.allocator);
        self.field_report = null;
        self.generation +%= 1;
    }

    pub fn newField(self: *Composers) Allocator.Error!void {
        try self.fdoc.load("", try rmg.FieldSet.initNew(self.allocator), false);
        self.clearFieldReport();
        self.message_len = 0;
    }

    pub fn openField(self: *Composers, editor: *Editor, name: []const u8) !void {
        var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
        const full = fullName(&buffer, field_folder, name) orelse return error.Refused;
        var field = editor.readFieldSet(full) catch |err| {
            self.sayText(editor.status());
            return err;
        };
        errdefer field.deinit(self.allocator);
        try self.fdoc.load(full, field, false);
        self.clearFieldReport();
        self.message_len = 0;
    }

    pub fn saveField(self: *Composers, editor: *Editor) Allocator.Error!SaveResult {
        if (self.fdoc.name.len == 0) {
            self.say("a new field set has no file yet: Save As", .{});
            return .needs_save_as;
        }
        return self.writeFieldTo(editor, try self.allocator.dupe(u8, self.fdoc.name));
    }

    pub fn saveFieldAs(self: *Composers, editor: *Editor, name: []const u8) Allocator.Error!SaveResult {
        var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
        const full = fullName(&buffer, field_folder, name) orelse {
            self.say("that name is too long", .{});
            return .failed;
        };
        return self.writeFieldTo(editor, try self.allocator.dupe(u8, full));
    }

    fn writeFieldTo(self: *Composers, editor: *Editor, owned_name: []u8) Allocator.Error!SaveResult {
        defer self.allocator.free(owned_name);
        editor.writeFieldSet(owned_name, &self.fdoc.current) catch |err| {
            self.sayText(editor.status());
            if (err == error.OutOfMemory) return error.OutOfMemory;
            if (std.mem.indexOf(u8, editor.status(), "Save As") != null) {
                self.fdoc.shipped = true;
                return .needs_save_as;
            }
            return .failed;
        };
        try self.fdoc.markSaved(owned_name);
        try self.refreshNames(editor);
        self.say("saved {s}", .{owned_name});
        return .saved;
    }

    /// An edit of the open field set: the document keeps its state before, the
    /// edit runs, and a refused or no-op edit takes the snapshot back. `edit`
    /// returns true when it changed something.
    fn editField(self: *Composers, comptime Context: type, context: Context, comptime edit: fn (Context, Allocator, *rmg.FieldSet) Allocator.Error!bool) Allocator.Error!bool {
        const field = try self.fdoc.begin();
        const changed = edit(context, self.allocator, field) catch |err| {
            self.fdoc.cancel();
            return err;
        };
        if (!changed) {
            self.fdoc.cancel();
            return false;
        }
        self.clearFieldReport();
        return true;
    }

    pub fn setFieldSeason(self: *Composers, index: usize) Allocator.Error!bool {
        if (index >= rmg.season_folders.len) return false;
        return self.editField(usize, index, struct {
            fn run(i: usize, a: Allocator, f: *rmg.FieldSet) Allocator.Error!bool {
                if (f.seasonSlot() == i and f.season == rmg.real_seasons[i] and std.ascii.eqlIgnoreCase(f.season_folder, rmg.season_folders[i])) return false;
                try f.setSeasonIndex(a, i);
                return true;
            }
        }.run);
    }

    pub fn addFieldShell(self: *Composers, objects: bool) Allocator.Error!usize {
        const field = try self.fdoc.begin();
        const index = (if (objects) field.addObjectShell(self.allocator) else field.addTileShell(self.allocator)) catch |err| {
            self.fdoc.cancel();
            return err;
        };
        self.clearFieldReport();
        return index;
    }

    /// Takes the shells at `doomed` out (the MFC asked first; the window does).
    pub fn removeFieldShells(self: *Composers, objects: bool, doomed: []const usize) Allocator.Error!bool {
        const field = try self.fdoc.begin();
        const before = if (objects) field.object_shells.items.len else field.tile_shells.items.len;
        if (objects) field.removeObjectShells(self.allocator, doomed) else field.removeTileShells(self.allocator, doomed);
        const after = if (objects) field.object_shells.items.len else field.tile_shells.items.len;
        if (after == before) {
            self.fdoc.cancel();
            return false;
        }
        self.clearFieldReport();
        return true;
    }

    pub const ShellField = enum { width, step, ratio };

    /// One shell property (the shell properties dialogs' edits): the width
    /// 0 or more (tile and object shells), the step above 0 and the ratio in
    /// PERCENT 0..100 (object shells). A value the dialog would not take is
    /// refused and changes nothing.
    pub fn setFieldShell(self: *Composers, objects: bool, index: usize, field_kind: ShellField, value: f32) Allocator.Error!bool {
        if (!std.math.isFinite(value)) return false;
        const Context = struct { objects: bool, index: usize, kind: ShellField, value: f32 };
        return self.editField(Context, .{ .objects = objects, .index = index, .kind = field_kind, .value = value }, struct {
            fn run(c: Context, _: Allocator, f: *rmg.FieldSet) Allocator.Error!bool {
                switch (c.kind) {
                    .width => {
                        if (c.value < 0) return false;
                        const slot: *f32 = if (c.objects) (if (c.index < f.object_shells.items.len) &f.object_shells.items[c.index].width else return false) else (if (c.index < f.tile_shells.items.len) &f.tile_shells.items[c.index].width else return false);
                        if (slot.* == c.value) return false;
                        slot.* = c.value;
                        return true;
                    },
                    .step => {
                        if (!c.objects or c.index >= f.object_shells.items.len or c.value < 1) return false;
                        const step: i32 = @intFromFloat(@min(c.value, 1.0e6));
                        if (f.object_shells.items[c.index].step == step) return false;
                        f.object_shells.items[c.index].step = step;
                        return true;
                    },
                    .ratio => {
                        if (!c.objects or c.index >= f.object_shells.items.len or c.value < 0 or c.value > 100) return false;
                        const ratio = c.value / 100.0;
                        if (f.object_shells.items[c.index].ratio == ratio) return false;
                        f.object_shells.items[c.index].ratio = ratio;
                        return true;
                    },
                }
            }
        }.run);
    }

    /// Terrain types added to a tile shell (the MFC's OnAddTile skipped a
    /// repeat); returns how many were new.
    pub fn addShellTiles(self: *Composers, shell: usize, tiles: []const i32) Allocator.Error!usize {
        const field = try self.fdoc.begin();
        var added: usize = 0;
        for (tiles) |tile| {
            if (try field.addTile(self.allocator, shell, tile)) added += 1;
        }
        if (added == 0) {
            self.fdoc.cancel();
            return 0;
        }
        self.clearFieldReport();
        return added;
    }

    pub fn removeShellTiles(self: *Composers, shell: usize, indices: []const usize) Allocator.Error!bool {
        const field = try self.fdoc.begin();
        const before = field.tileEntryCount();
        field.removeTiles(shell, indices);
        if (field.tileEntryCount() == before) {
            self.fdoc.cancel();
            return false;
        }
        self.clearFieldReport();
        return true;
    }

    /// The tile properties dialog's weight (0 or more) for every index given.
    pub fn setShellTileWeights(self: *Composers, shell: usize, indices: []const usize, weight: i32) Allocator.Error!bool {
        if (weight < 0) return false;
        const field = try self.fdoc.begin();
        var changed = false;
        if (shell < field.tile_shells.items.len) {
            const list = field.tile_shells.items[shell].tiles.items;
            for (indices) |i| {
                if (i < list.len and list[i].weight != weight) {
                    list[i].weight = weight;
                    changed = true;
                }
            }
        }
        if (!changed) {
            self.fdoc.cancel();
            return false;
        }
        self.clearFieldReport();
        return true;
    }

    pub fn addShellObjects(self: *Composers, shell: usize, names: []const []const u8) Allocator.Error!usize {
        const field = try self.fdoc.begin();
        var added: usize = 0;
        for (names) |name| {
            if (try field.addObject(self.allocator, shell, name)) added += 1;
        }
        if (added == 0) {
            self.fdoc.cancel();
            return 0;
        }
        self.clearFieldReport();
        return added;
    }

    pub fn removeShellObjects(self: *Composers, shell: usize, indices: []const usize) Allocator.Error!bool {
        const field = try self.fdoc.begin();
        const before = field.objectEntryCount();
        field.removeObjects(self.allocator, shell, indices);
        if (field.objectEntryCount() == before) {
            self.fdoc.cancel();
            return false;
        }
        self.clearFieldReport();
        return true;
    }

    pub fn setShellObjectWeights(self: *Composers, shell: usize, indices: []const usize, weight: i32) Allocator.Error!bool {
        if (weight < 0) return false;
        const field = try self.fdoc.begin();
        var changed = false;
        if (shell < field.object_shells.items.len) {
            const list = field.object_shells.items[shell].objects.items;
            for (indices) |i| {
                if (i < list.len and list[i].weight != weight) {
                    list[i].weight = weight;
                    changed = true;
                }
            }
        }
        if (!changed) {
            self.fdoc.cancel();
            return false;
        }
        self.clearFieldReport();
        return true;
    }

    pub const HeightField = enum { height, pattern_min, pattern_max, positive, profile };

    /// The Heights tab's fields: height 0..5, the pattern sizes 1..16 (the other
    /// end follows), the positive ratio in percent 0..100, and the profile (a
    /// name the storages hold as a .tga - the MFC ignored one they did not).
    pub fn setFieldHeights(self: *Composers, editor: *Editor, kind: HeightField, text: []const u8) Allocator.Error!bool {
        const Context = struct { kind: HeightField, text: []const u8, known_profile: bool };
        const known = kind != .profile or editor.rmgFileExists(text, ".tga");
        const changed = try self.editField(Context, .{ .kind = kind, .text = text, .known_profile = known }, struct {
            fn run(c: Context, a: Allocator, f: *rmg.FieldSet) Allocator.Error!bool {
                switch (c.kind) {
                    .height => {
                        const value = std.fmt.parseFloat(f32, c.text) catch return false;
                        if (f.height == value) return false;
                        return f.setHeight(value);
                    },
                    .pattern_min, .pattern_max => {
                        const value = std.fmt.parseInt(i32, c.text, 10) catch return false;
                        const before = [2]i32{ f.pattern_min, f.pattern_max };
                        const ok = if (c.kind == .pattern_min) f.setPatternMin(value) else f.setPatternMax(value);
                        return ok and (before[0] != f.pattern_min or before[1] != f.pattern_max);
                    },
                    .positive => {
                        const value = std.fmt.parseFloat(f32, c.text) catch return false;
                        const before = f.positive_ratio;
                        return f.setPositivePercent(value) and f.positive_ratio != before;
                    },
                    .profile => {
                        if (!c.known_profile or std.mem.eql(u8, f.profile, c.text)) return false;
                        try f.setProfile(a, c.text);
                        return true;
                    },
                }
            }
        }.run);
        if (!changed and kind == .profile and !known) self.say("\"{s}\" is not a .tga of the storages: the profile stays", .{text});
        return changed;
    }

    /// What the field set's Check! reads that the value does not hold: the
    /// tileset's terrain types per season (asked once each), the profile in the
    /// storages, the object in the catalogue.
    const FieldFacts = struct {
        editor: *Editor,
        composers: *Composers,
        counts: [4]?usize = .{ null, null, null, null },
        asked: [4]bool = .{ false, false, false, false },

        fn tileCount(ctx: *anyopaque, slot: usize) ?usize {
            const self: *FieldFacts = @ptrCast(@alignCast(ctx));
            if (slot >= 4) return null;
            if (!self.asked[slot]) {
                self.asked[slot] = true;
                const types = self.editor.tilesetTypes(self.composers.allocator, slot) catch return null;
                defer self.composers.allocator.free(types);
                self.counts[slot] = if (types.len == 0) null else types.len;
            }
            return self.counts[slot];
        }
        fn hasObject(ctx: *anyopaque, name: []const u8) bool {
            const self: *FieldFacts = @ptrCast(@alignCast(ctx));
            const lookup = self.composers.object_lookup orelse return true;
            return lookup.has_fn(lookup.ctx, name);
        }
        fn hasProfile(ctx: *anyopaque, name: []const u8) bool {
            const self: *FieldFacts = @ptrCast(@alignCast(ctx));
            return self.editor.rmgFileExists(name, ".tga");
        }
        fn source(self: *FieldFacts) rmg.FieldSource {
            return .{ .ctx = self, .tile_count_fn = tileCount, .object_fn = hasObject, .profile_fn = hasProfile };
        }
    };

    pub fn checkField(self: *Composers, editor: *Editor) Allocator.Error!usize {
        self.clearFieldReport();
        var facts: FieldFacts = .{ .editor = editor, .composers = self };
        self.field_report = try rmg.checkFieldSet(self.allocator, &self.fdoc.current, facts.source());
        self.generation +%= 1;
        const report = &self.field_report.?;
        self.say("Check!: {d} errors, {d} findings", .{ report.errorCount(), report.findings.items.len });
        return report.findings.items.len;
    }

    pub fn fixFieldFinding(self: *Composers, editor: *Editor, index: usize) !void {
        const report = &(self.field_report orelse return error.Refused);
        if (index >= report.findings.items.len or report.findings.items[index].fix == .none) return error.Refused;
        const fix = report.findings.items[index].fix;
        const field = try self.fdoc.begin();
        try rmg.applyFieldFix(self.allocator, field, fix);
        _ = try self.checkField(editor);
    }

    pub fn fixFieldAll(self: *Composers, editor: *Editor) !usize {
        const report = &(self.field_report orelse return error.Refused);
        const field = try self.fdoc.begin();
        const fixed = try rmg.fixAllField(self.allocator, field, report);
        if (fixed == 0) self.fdoc.cancel();
        _ = try self.checkField(editor);
        return fixed;
    }

    pub fn undoField(self: *Composers) Allocator.Error!bool {
        const done = try self.fdoc.undo();
        if (done) self.clearFieldReport();
        return done;
    }

    pub fn redoField(self: *Composers) Allocator.Error!bool {
        const done = try self.fdoc.redo();
        if (done) self.clearFieldReport();
        return done;
    }

    // --- Templates (05-10) ----------------------------------------------

    fn clearTemplateReport(self: *Composers) void {
        if (self.template_report) |*report| report.deinit(self.allocator);
        self.template_report = null;
        self.generation +%= 1;
    }

    pub fn newTemplate(self: *Composers) Allocator.Error!void {
        try self.tdoc.load("", try rmg.Template.initNew(self.allocator), false);
        self.clearTemplateReport();
        self.message_len = 0;
    }

    pub fn openTemplate(self: *Composers, editor: *Editor, name: []const u8) !void {
        var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
        const full = fullName(&buffer, template_folder, name) orelse return error.Refused;
        var template = editor.readTemplate(full) catch |err| {
            self.sayText(editor.status());
            return err;
        };
        errdefer template.deinit(self.allocator);
        try self.tdoc.load(full, template, false);
        self.clearTemplateReport();
        self.message_len = 0;
    }

    pub fn saveTemplate(self: *Composers, editor: *Editor) Allocator.Error!SaveResult {
        if (self.tdoc.name.len == 0) {
            self.say("a new template has no file yet: Save As", .{});
            return .needs_save_as;
        }
        return self.writeTemplateTo(editor, try self.allocator.dupe(u8, self.tdoc.name));
    }

    pub fn saveTemplateAs(self: *Composers, editor: *Editor, name: []const u8) Allocator.Error!SaveResult {
        var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
        const full = fullName(&buffer, template_folder, name) orelse {
            self.say("that name is too long", .{});
            return .failed;
        };
        return self.writeTemplateTo(editor, try self.allocator.dupe(u8, full));
    }

    fn writeTemplateTo(self: *Composers, editor: *Editor, owned_name: []u8) Allocator.Error!SaveResult {
        defer self.allocator.free(owned_name);
        editor.writeTemplate(owned_name, &self.tdoc.current) catch |err| {
            self.sayText(editor.status());
            if (err == error.OutOfMemory) return error.OutOfMemory;
            if (std.mem.indexOf(u8, editor.status(), "Save As") != null) {
                self.tdoc.shipped = true;
                return .needs_save_as;
            }
            return .failed;
        };
        try self.tdoc.markSaved(owned_name);
        try self.refreshNames(editor);
        self.say("saved {s} (the Template entry and its QuickLoadMapInfo)", .{owned_name});
        return .saved;
    }

    fn editTemplate(self: *Composers, comptime Context: type, context: Context, comptime edit: fn (Context, Allocator, *rmg.Template) Allocator.Error!bool) Allocator.Error!bool {
        const template = try self.tdoc.begin();
        const changed = edit(context, self.allocator, template) catch |err| {
            self.tdoc.cancel();
            return err;
        };
        if (!changed) {
            self.tdoc.cancel();
            return false;
        }
        self.clearTemplateReport();
        return true;
    }

    /// What the template rules read: graphs and field sets as the storages hold
    /// them (the editor's status line left alone), a file's presence, the
    /// container and tileset facts the graph and field set rules ask.
    const TemplateFacts = struct {
        editor: *Editor,
        fields: FieldFacts,

        fn graph(ctx: *anyopaque, a: Allocator, name: []const u8) ?rmg.Graph {
            _ = a;
            const self: *TemplateFacts = @ptrCast(@alignCast(ctx));
            return self.editor.readGraphQuiet(name);
        }
        fn field(ctx: *anyopaque, a: Allocator, name: []const u8) ?rmg.FieldSet {
            _ = a;
            const self: *TemplateFacts = @ptrCast(@alignCast(ctx));
            return self.editor.readFieldSetQuiet(name);
        }
        fn exists(ctx: *anyopaque, name: []const u8, extension: []const u8) bool {
            const self: *TemplateFacts = @ptrCast(@alignCast(ctx));
            var buffer: [16:0]u8 = undefined;
            const extension_z = std.mem.printSentinel(&buffer, "{s}", .{extension}, 0) catch return false;
            return self.editor.rmgFileExists(name, extension_z);
        }
        fn source(self: *TemplateFacts) rmg.TemplateSource {
            return .{ .ctx = self, .graph_fn = graph, .field_fn = field, .exists_fn = exists, .containers = self.editor.rmgSource(), .facts = self.fields.source() };
        }
    };

    /// Adds graphs from the storages the MFC's way (OnAddGraphButton): the first
    /// that does not belong stops the batch and says why; earlier ones stay as one
    /// undo step. A script-list difference is reported and the graph kept.
    pub fn addTemplateGraphs(self: *Composers, editor: *Editor, names: []const []const u8) !usize {
        if (names.len == 0) return 0;
        var facts: TemplateFacts = .{ .editor = editor, .fields = .{ .editor = editor, .composers = self } };
        const template = try self.tdoc.begin();
        var added: usize = 0;
        var warned = false;
        for (names) |name| {
            var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
            const full = fullName(&buffer, graph_folder, name) orelse {
                self.say("the graph name is too long", .{});
                break;
            };
            var outcome = try rmg.addTemplateGraph(self.allocator, template, facts.source(), full);
            defer outcome.deinit(self.allocator);
            switch (outcome) {
                .added => added += 1,
                .added_warning => |text| {
                    added += 1;
                    warned = true;
                    self.say("Warning! Graph <{s}>: {s}", .{ full, text });
                },
                .unreadable => {
                    self.say("Can't Add Graph to Template! Graph <{s}> does not load as a graph.", .{full});
                    break;
                },
                .mismatch => |text| {
                    self.say("Can't Add Graph to Template! Graph <{s}>: {s}", .{ full, text });
                    break;
                },
            }
        }
        if (added == 0) self.tdoc.cancel() else self.clearTemplateReport();
        if (added == names.len and !warned) self.message_len = 0;
        return added;
    }

    /// Field sets (OnAddFieldButton): one that does not load is refused; the
    /// batch stops there. Returns how many were added.
    pub fn addTemplateFields(self: *Composers, editor: *Editor, names: []const []const u8) !usize {
        if (names.len == 0) return 0;
        const template = try self.tdoc.begin();
        var added: usize = 0;
        for (names) |name| {
            var buffer: [bridge_mod.field_set_name_capacity]u8 = undefined;
            const full = fullName(&buffer, field_folder, name) orelse {
                self.say("the field set name is too long", .{});
                break;
            };
            var field = editor.readFieldSetQuiet(full) orelse {
                self.say("Can't Add Field to Template! Field set <{s}> does not load as a field set.", .{full});
                break;
            };
            field.deinit(self.allocator);
            try template.addField(self.allocator, full);
            added += 1;
        }
        if (added == 0) self.tdoc.cancel() else self.clearTemplateReport();
        if (added == names.len) self.message_len = 0;
        return added;
    }

    /// Road and river descriptors (OnAddVsoButton): any descriptor of the storages.
    pub fn addTemplateVsos(self: *Composers, editor: *Editor, names: []const []const u8) !usize {
        if (names.len == 0) return 0;
        const template = try self.tdoc.begin();
        var added: usize = 0;
        for (names) |name| {
            if (!editor.rmgFileExists(name, ".xml")) {
                self.say("Can't Add VSO to Template! <{s}> is not a descriptor of the storages.", .{name});
                break;
            }
            try template.addVso(self.allocator, name);
            added += 1;
        }
        if (added == 0) self.tdoc.cancel() else self.clearTemplateReport();
        if (added == names.len) self.message_len = 0;
        return added;
    }

    pub const TemplateList = enum { fields, graphs, vso };

    pub fn removeTemplateEntries(self: *Composers, list: TemplateList, doomed: []const usize) Allocator.Error!bool {
        const template = try self.tdoc.begin();
        const before = switch (list) {
            .fields => template.fields.items.len,
            .graphs => template.graphs.items.len,
            .vso => template.vso.items.len,
        };
        switch (list) {
            .fields => template.removeFields(self.allocator, doomed),
            .graphs => template.removeGraphs(self.allocator, doomed),
            .vso => template.removeVso(self.allocator, doomed),
        }
        const after = switch (list) {
            .fields => template.fields.items.len,
            .graphs => template.graphs.items.len,
            .vso => template.vso.items.len,
        };
        if (after == before) {
            self.tdoc.cancel();
            return false;
        }
        self.clearTemplateReport();
        return true;
    }

    /// One entry's weight (the properties dialogs' own edit, 0 or more), and for the
    /// field list whether it is the default.
    pub fn setTemplateWeight(self: *Composers, list: TemplateList, index: usize, weight: i32) Allocator.Error!bool {
        if (weight < 0) return false;
        const Context = struct { list: TemplateList, index: usize, weight: i32 };
        return self.editTemplate(Context, .{ .list = list, .index = index, .weight = weight }, struct {
            fn run(c: Context, _: Allocator, t: *rmg.Template) Allocator.Error!bool {
                const slot: *i32 = switch (c.list) {
                    .fields => if (c.index < t.fields.items.len) &t.fields.items[c.index].weight else return false,
                    .graphs => if (c.index < t.graphs.items.len) &t.graphs.items[c.index].weight else return false,
                    .vso => if (c.index < t.vso.items.len) &t.vso.items[c.index].weight else return false,
                };
                if (slot.* == c.weight) return false;
                slot.* = c.weight;
                return true;
            }
        }.run);
    }

    /// The VSO properties dialog: the width in cells (above 0) and the opacity in
    /// percent (0..100) - the file holds world units and 0..1.
    pub fn setTemplateVso(self: *Composers, index: usize, width_cells: f32, opacity_percent: f32) Allocator.Error!bool {
        if (!std.math.isFinite(width_cells) or width_cells <= 0 or !std.math.isFinite(opacity_percent) or opacity_percent < 0 or opacity_percent > 100) return false;
        const Context = struct { index: usize, width: f32, opacity: f32 };
        return self.editTemplate(Context, .{ .index = index, .width = width_cells * rmg.world_cell, .opacity = opacity_percent / 100.0 }, struct {
            fn run(c: Context, _: Allocator, t: *rmg.Template) Allocator.Error!bool {
                if (c.index >= t.vso.items.len) return false;
                const entry = &t.vso.items[c.index];
                if (entry.width == c.width and entry.opacity == c.opacity) return false;
                entry.width = c.width;
                entry.opacity = c.opacity;
                return true;
            }
        }.run);
    }

    /// The field set the generator falls back to: an index into the listed ones, or
    /// -1 for none.
    pub fn setTemplateDefaultField(self: *Composers, index: i32) Allocator.Error!bool {
        return self.editTemplate(i32, index, struct {
            fn run(i: i32, _: Allocator, t: *rmg.Template) Allocator.Error!bool {
                if (t.default_field == i) return false;
                return t.setDefaultField(i);
            }
        }.run);
    }

    pub fn setTemplateText(self: *Composers, which: rmg.Template.TextField, text: []const u8) Allocator.Error!bool {
        const Context = struct { which: rmg.Template.TextField, text: []const u8 };
        return self.editTemplate(Context, .{ .which = which, .text = text }, struct {
            fn run(c: Context, a: Allocator, t: *rmg.Template) Allocator.Error!bool {
                if (std.mem.indexOfAny(u8, c.text, "\x00\r\n") != null or c.text.len >= bridge_mod.field_set_name_capacity) return false;
                const slot = t.texts()[@intFromEnum(c.which)];
                if (std.mem.eql(u8, slot.*, c.text)) return false;
                try t.setText(a, c.which, c.text);
                return true;
            }
        }.run);
    }

    /// The MOD combo: a mod's name and version, or both empty for none
    /// (OnSelchangeModComboBox).
    pub fn setTemplateMod(self: *Composers, name: []const u8, version: []const u8) Allocator.Error!bool {
        const Context = struct { name: []const u8, version: []const u8 };
        return self.editTemplate(Context, .{ .name = name, .version = version }, struct {
            fn run(c: Context, a: Allocator, t: *rmg.Template) Allocator.Error!bool {
                if (std.mem.eql(u8, t.mod_name, c.name) and std.mem.eql(u8, t.mod_version, c.version)) return false;
                try t.setText(a, .mod_name, c.name);
                try t.setText(a, .mod_version, c.version);
                return true;
            }
        }.run);
    }

    /// The Diplomacy dialog's result: the table of sides (players, then the
    /// neutral 2), the game type and the attacking side. The unit creation follows
    /// the player count. One undo step; a table the dialog would not allow is refused.
    pub fn setTemplateDiplomacy(self: *Composers, sides: []const u8, game_type: i32, attacking_side: i32) Allocator.Error!bool {
        const Context = struct { sides: []const u8, game_type: i32, attacking: i32 };
        return self.editTemplate(Context, .{ .sides = sides, .game_type = game_type, .attacking = attacking_side }, struct {
            fn run(c: Context, a: Allocator, t: *rmg.Template) Allocator.Error!bool {
                const before = try t.clone(a);
                var kept = before;
                defer kept.deinit(a);
                if (!try t.setDiplomacy(a, c.sides, c.game_type, c.attacking)) return false;
                return !t.eql(&kept);
            }
        }.run);
    }

    /// A player added on `side` before the neutral (the Diplomacy dialog's Insert).
    pub fn addTemplatePlayer(self: *Composers, side: u8) Allocator.Error!bool {
        const t = &self.tdoc.current;
        if (side > 1 or t.diplomacies.items.len >= rmg.max_diplomacies) return false;
        var sides = std.ArrayListUnmanaged(u8).empty;
        defer sides.deinit(self.allocator);
        try sides.appendSlice(self.allocator, t.diplomacies.items);
        try sides.insert(self.allocator, sides.items.len - 1, side);
        return self.setTemplateDiplomacy(sides.items, t.game_type, t.attacking_side);
    }

    /// A player deleted (never the neutral, and the table keeps two players).
    pub fn deleteTemplatePlayer(self: *Composers, player: usize) Allocator.Error!bool {
        const t = &self.tdoc.current;
        if (player >= t.playerCount() or t.playerCount() <= 2) return false;
        var sides = std.ArrayListUnmanaged(u8).empty;
        defer sides.deinit(self.allocator);
        try sides.appendSlice(self.allocator, t.diplomacies.items);
        _ = sides.orderedRemove(player);
        return self.setTemplateDiplomacy(sides.items, t.game_type, t.attacking_side);
    }

    pub fn setTemplatePlayerSide(self: *Composers, player: usize, side: u8) Allocator.Error!bool {
        const t = &self.tdoc.current;
        if (player >= t.playerCount() or side > 1) return false;
        var sides = std.ArrayListUnmanaged(u8).empty;
        defer sides.deinit(self.allocator);
        try sides.appendSlice(self.allocator, t.diplomacies.items);
        sides.items[player] = side;
        return self.setTemplateDiplomacy(sides.items, t.game_type, t.attacking_side);
    }

    pub fn setTemplateGameType(self: *Composers, game_type: i32, attacking_side: i32) Allocator.Error!bool {
        const t = &self.tdoc.current;
        return self.setTemplateDiplomacy(t.diplomacies.items, game_type, attacking_side);
    }

    /// The names a unit creation combo offers (parties, aircraft, squads), asked
    /// of the bridge; the caller frees. Empty when it will not say.
    pub fn unitChoices(self: *Composers, editor: *Editor, kind: bridge_mod.UcChoice) Allocator.Error![]bridge_mod.UcName {
        var total: usize = 0;
        _ = editor.bridge.unitCreationChoices(kind, &.{}, &total);
        if (total == 0) return try self.allocator.alloc(bridge_mod.UcName, 0);
        const names = try self.allocator.alloc(bridge_mod.UcName, total);
        errdefer self.allocator.free(names);
        var got: usize = 0;
        if (editor.bridge.unitCreationChoices(kind, names, &got) != .ok or got != total) {
            self.allocator.free(names);
            return try self.allocator.alloc(bridge_mod.UcName, 0);
        }
        return names;
    }

    pub const max_unit_value: i32 = 255;
    pub const max_formation_size: i32 = 32;

    fn nameAllowed(name: []const u8, current: []const u8, list: []const bridge_mod.UcName) bool {
        if (name.len == 0 or name.len >= records.uc_name_capacity) return false;
        if (std.mem.eql(u8, name, current)) return true;
        for (list) |*item| if (std.mem.eql(u8, item.nameSlice(), name)) return true;
        return false;
    }

    /// One field of one player's unit creation in the template's table (the
    /// Units... grid; MutableValidate's rules and the combos' lists): party, one of
    /// partys.xml; `aircraftN_name` an aviation unit, `aircraftN_formation` 1..32,
    /// `aircraftN_count` 0..255; `paratroop_name` a squad, `paratroop_count`
    /// 0..255; `relax` seconds, 1 or more. A value the entry already holds is
    /// always accepted. A refusal changes nothing and says which rule.
    pub fn setTemplateUnit(self: *Composers, editor: *Editor, player: usize, field_name: []const u8, value: []const u8) !bool {
        if (player >= self.tdoc.current.units.items.len) return false;
        const Kind = enum { party, aircraft_name, aircraft_formation, aircraft_count, paratroop_name, paratroop_count, relax };
        var kind: Kind = undefined;
        var slot: usize = 0;
        if (std.mem.eql(u8, field_name, "party")) {
            kind = .party;
        } else if (std.mem.eql(u8, field_name, "paratroop_name")) {
            kind = .paratroop_name;
        } else if (std.mem.eql(u8, field_name, "paratroop_count")) {
            kind = .paratroop_count;
        } else if (std.mem.eql(u8, field_name, "relax")) {
            kind = .relax;
        } else if (std.mem.startsWith(u8, field_name, "aircraft") and field_name.len > "aircraft".len + 2) {
            slot = std.fmt.parseInt(usize, field_name["aircraft".len .. "aircraft".len + 1], 10) catch return false;
            if (slot >= records.uc_aircraft_slots or field_name["aircraft".len + 1] != '_') return false;
            const rest = field_name["aircraft".len + 2 ..];
            kind = if (std.mem.eql(u8, rest, "name")) .aircraft_name else if (std.mem.eql(u8, rest, "formation")) .aircraft_formation else if (std.mem.eql(u8, rest, "count")) .aircraft_count else return false;
        } else return false;
        const current = self.tdoc.current.units.items[player];
        var number: i32 = 0;
        switch (kind) {
            .aircraft_formation, .aircraft_count, .paratroop_count, .relax => {
                number = std.fmt.parseInt(i32, value, 10) catch {
                    self.say("{s}: \"{s}\" is not a whole number", .{ field_name, value });
                    return false;
                };
            },
            else => {},
        }
        switch (kind) {
            .party => {
                const list = try self.unitChoices(editor, .parties);
                defer self.allocator.free(list);
                if (!nameAllowed(value, current.partySlice(), list)) {
                    self.say("the party \"{s}\" is not in partys.xml", .{value});
                    return false;
                }
            },
            .aircraft_name => {
                const list = try self.unitChoices(editor, .aircraft);
                defer self.allocator.free(list);
                if (!nameAllowed(value, current.aircraft[slot].nameSlice(), list)) {
                    self.say("{s}: \"{s}\" is no aircraft of the object database", .{ records.uc_aircraft_labels[slot], value });
                    return false;
                }
            },
            .paratroop_name => {
                const list = try self.unitChoices(editor, .squads);
                defer self.allocator.free(list);
                if (!nameAllowed(value, current.paratroopSlice(), list)) {
                    self.say("the paratroop squad \"{s}\" is no squad of the object database", .{value});
                    return false;
                }
            },
            .aircraft_formation => if (number < 1 or number > max_formation_size) {
                self.say("{s}: formation size {d} is outside 1..{d}", .{ records.uc_aircraft_labels[slot], number, max_formation_size });
                return false;
            },
            .aircraft_count => if (number < 0 or number > max_unit_value) {
                self.say("{s}: count {d} is outside 0..{d}", .{ records.uc_aircraft_labels[slot], number, max_unit_value });
                return false;
            },
            .paratroop_count => if (number < 0 or number > max_unit_value) {
                self.say("the paratroop squads count {d} is outside 0..{d}", .{ number, max_unit_value });
                return false;
            },
            .relax => if (number < 1) {
                self.say("the relax time {d} is below 1 second", .{number});
                return false;
            },
        }
        const Context = struct { player: usize, kind: Kind, slot: usize, text: []const u8, number: i32 };
        return self.editTemplate(Context, .{ .player = player, .kind = kind, .slot = slot, .text = value, .number = number }, struct {
            fn run(c: Context, _: Allocator, t: *rmg.Template) Allocator.Error!bool {
                const unit = &t.units.items[c.player];
                const before = unit.*;
                switch (c.kind) {
                    .party => unit.setParty(c.text),
                    .aircraft_name => unit.aircraft[c.slot].setName(c.text),
                    .aircraft_formation => unit.aircraft[c.slot].formation_size = c.number,
                    .aircraft_count => unit.aircraft[c.slot].count = c.number,
                    .paratroop_name => unit.setParatroop(c.text),
                    .paratroop_count => unit.paratroop_count = c.number,
                    .relax => unit.relax_time = c.number,
                }
                return !before.eql(unit.*);
            }
        }.run);
    }

    /// An appear point of a player's template unit creation, in MAP units (the
    /// grid shows tiles, 64 to a tile): added, moved or removed. A point must be
    /// finite and not negative; at most 32 per player.
    pub fn addTemplateAppear(self: *Composers, player: usize, x: f32, y: f32) Allocator.Error!bool {
        if (!std.math.isFinite(x) or !std.math.isFinite(y) or x < 0 or y < 0) return false;
        const Context = struct { player: usize, x: f32, y: f32 };
        return self.editTemplate(Context, .{ .player = player, .x = x, .y = y }, struct {
            fn run(c: Context, _: Allocator, t: *rmg.Template) Allocator.Error!bool {
                if (c.player >= t.units.items.len) return false;
                return t.units.items[c.player].addAppear(.{ .x = c.x, .y = c.y, .z = 0 });
            }
        }.run);
    }

    pub fn setTemplateAppear(self: *Composers, player: usize, index: usize, x: f32, y: f32) Allocator.Error!bool {
        if (!std.math.isFinite(x) or !std.math.isFinite(y) or x < 0 or y < 0) return false;
        const Context = struct { player: usize, index: usize, x: f32, y: f32 };
        return self.editTemplate(Context, .{ .player = player, .index = index, .x = x, .y = y }, struct {
            fn run(c: Context, _: Allocator, t: *rmg.Template) Allocator.Error!bool {
                if (c.player >= t.units.items.len or c.index >= t.units.items[c.player].appear_count) return false;
                const point = &t.units.items[c.player].appear[c.index];
                if (point.x == c.x and point.y == c.y) return false;
                point.x = c.x;
                point.y = c.y;
                return true;
            }
        }.run);
    }

    pub fn removeTemplateAppear(self: *Composers, player: usize, index: usize) Allocator.Error!bool {
        const Context = struct { player: usize, index: usize };
        return self.editTemplate(Context, .{ .player = player, .index = index }, struct {
            fn run(c: Context, _: Allocator, t: *rmg.Template) Allocator.Error!bool {
                if (c.player >= t.units.items.len) return false;
                return t.units.items[c.player].removeAppear(c.index);
            }
        }.run);
    }

    pub fn checkTemplate(self: *Composers, editor: *Editor) Allocator.Error!usize {
        self.clearTemplateReport();
        var facts: TemplateFacts = .{ .editor = editor, .fields = .{ .editor = editor, .composers = self } };
        self.template_report = try rmg.checkTemplate(self.allocator, &self.tdoc.current, facts.source());
        self.generation +%= 1;
        const report = &self.template_report.?;
        self.say("Check!: {d} errors, {d} findings", .{ report.errorCount(), report.findings.items.len });
        return report.findings.items.len;
    }

    pub fn fixTemplateFinding(self: *Composers, editor: *Editor, index: usize) !void {
        const report = &(self.template_report orelse return error.Refused);
        if (index >= report.findings.items.len or report.findings.items[index].fix == .none) return error.Refused;
        const fix = report.findings.items[index].fix;
        var facts: TemplateFacts = .{ .editor = editor, .fields = .{ .editor = editor, .composers = self } };
        const template = try self.tdoc.begin();
        try rmg.applyTemplateFix(self.allocator, template, facts.source(), fix);
        _ = try self.checkTemplate(editor);
    }

    pub fn fixTemplateAll(self: *Composers, editor: *Editor) !usize {
        const report = &(self.template_report orelse return error.Refused);
        var facts: TemplateFacts = .{ .editor = editor, .fields = .{ .editor = editor, .composers = self } };
        const template = try self.tdoc.begin();
        const fixed = try rmg.fixAllTemplate(self.allocator, template, facts.source(), report);
        if (fixed == 0) self.tdoc.cancel();
        _ = try self.checkTemplate(editor);
        return fixed;
    }

    pub fn undoTemplate(self: *Composers) Allocator.Error!bool {
        const done = try self.tdoc.undo();
        if (done) self.clearTemplateReport();
        return done;
    }

    pub fn redoTemplate(self: *Composers) Allocator.Error!bool {
        const done = try self.tdoc.redo();
        if (done) self.clearTemplateReport();
        return done;
    }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

const testing = std.testing;
const fake_mod = @import("fake_bridge.zig");

test "names: the kind's folder goes in front unless the name already starts at scenarios" {
    var buffer: [192]u8 = undefined;
    try testing.expectEqualStrings("scenarios\\containers\\common\\x", fullName(&buffer, container_folder, "common\\x").?);
    try testing.expectEqualStrings("scenarios\\containers\\common\\x", fullName(&buffer, container_folder, "common/x.xml").?);
    try testing.expectEqualStrings("scenarios\\patches\\summer\\p", fullName(&buffer, container_folder, "scenarios\\patches\\summer\\p").?);
    try testing.expectEqualStrings("scenarios\\patches\\summer\\p", fullName(&buffer, patch_folder, "summer\\p").?);
    try testing.expectEqualStrings("common\\x", relativeName(container_folder, "scenarios\\containers\\common\\x"));
    try testing.expectEqualStrings("other\\x", relativeName(container_folder, "other\\x"));
    var tiny: [6]u8 = undefined;
    try testing.expect(fullName(&tiny, container_folder, "x") == null);
}

test "the containers composer opens a shipped container, adds a patch from the storages, edits and saves as a user file" {
    var fake = try fake_mod.fixture(testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(testing.allocator, fake.bridge());
    defer editor.deinit();
    var composers = Composers.init(testing.allocator);
    defer composers.deinit();
    try composers.refreshNames(&editor);
    try testing.expectEqual(@as(usize, 1), composers.container_names.items.len);
    try testing.expectEqualStrings("summer\\road_a", relativeName(container_folder, composers.container_names.items[0]));
    try composers.openContainer(&editor, "summer\\road_a");
    try testing.expectEqual(@as(usize, 2), composers.cdoc.current.patchCount());
    try testing.expect(!composers.cdoc.dirty);
    // A patch of the same season and script lists joins, all four cells Yes.
    // (road_b1 uses another script ID: refused, naming it; the batch stops.)
    var names = [_][]const u8{ "scenarios\\patches\\summer\\road_a1", "summer\\road_b1" };
    try testing.expectEqual(@as(usize, 1), try composers.addPatches(&editor, &names));
    try testing.expect(std.mem.indexOf(u8, composers.message(), "ScriptIDs") != null);
    try testing.expectEqual(@as(usize, 2), composers.cdoc.current.patchCount());
    // A repeated name replaced the earlier entry: still two patches.
    try testing.expect(composers.cdoc.dirty);
    // Edits: setting and direction cells over a selection with a tri-state.
    try composers.setPatchProperties(&.{ 0, 1 }, "summer_france", .{ .keep, .off, .keep, .on });
    try testing.expectEqualStrings("summer_france", composers.cdoc.current.patches.items[0].place);
    try testing.expect(!composers.cdoc.current.hasDirection(0, .east));
    try testing.expect(composers.cdoc.current.hasDirection(1, .west));
    try testing.expect(composers.cdoc.current.hasDirection(1, .north));
    try composers.setPatchProperties(&.{0}, "<any setting>", .{ .keep, .keep, .keep, .keep });
    try testing.expectEqualStrings("", composers.cdoc.current.patches.items[0].place);
    // Delete one, then undo walks back.
    try composers.deletePatches(&.{1});
    try testing.expectEqual(@as(usize, 1), composers.cdoc.current.patchCount());
    try testing.expect(try composers.undoContainer());
    try testing.expectEqual(@as(usize, 2), composers.cdoc.current.patchCount());
    // Save over the shipped name: read-only, Save As.
    try testing.expectEqual(SaveResult.needs_save_as, try composers.saveContainer(&editor));
    try testing.expect(composers.cdoc.shipped);
    try testing.expect(composers.cdoc.dirty);
    // Save As a user name: written, clean, listed.
    try testing.expectEqual(SaveResult.saved, try composers.saveContainerAs(&editor, "user\\mine"));
    try testing.expect(!composers.cdoc.dirty and !composers.cdoc.shipped);
    try testing.expectEqualStrings("scenarios\\containers\\user\\mine", composers.cdoc.name);
    try testing.expectEqual(@as(usize, 2), composers.container_names.items.len);
    try testing.expectEqual(SaveResult.saved, try composers.saveContainer(&editor));
    // Not a plain name: refused by the bridge, the file stays as it was.
    try testing.expectEqual(SaveResult.failed, try composers.saveContainerAs(&editor, "..\\..\\x"));
    // A new one has no file: Save asks for a name.
    try composers.newContainer();
    try testing.expectEqual(SaveResult.needs_save_as, try composers.saveContainer(&editor));
}

test "Check! on a container is a list of findings with explicit fixes, never a silent rewrite" {
    var fake = try fake_mod.fixture(testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(testing.allocator, fake.bridge());
    defer editor.deinit();
    var composers = Composers.init(testing.allocator);
    defer composers.deinit();
    try composers.openContainer(&editor, "summer\\road_a");
    try testing.expectEqual(@as(usize, 0), try composers.checkContainer(&editor));
    // Break it by hand: a patch the storages do not hold and a stale size.
    const container = try composers.cdoc.begin();
    try container.addPatch(testing.allocator, "scenarios\\patches\\summer\\gone", 1, 1);
    container.size_x = 9;
    composers.clearContainerReport();
    const found = try composers.checkContainer(&editor);
    try testing.expect(found >= 2);
    try testing.expectEqual(@as(usize, 3), composers.cdoc.current.patchCount());
    var removable: ?usize = null;
    for (composers.container_report.?.findings.items, 0..) |finding, i| {
        if (finding.fix == .remove_patch) removable = i;
    }
    try testing.expect(removable != null);
    const depth = composers.cdoc.undo_stack.items.len;
    try composers.fixContainerFinding(&editor, removable.?);
    try testing.expectEqual(depth + 1, composers.cdoc.undo_stack.items.len);
    try testing.expectEqual(@as(usize, 2), composers.cdoc.current.patchCount());
    // Taking the patch out re-derived the size too: the check is clean, and
    // Fix all with nothing left to fix changes nothing (no undo step).
    try testing.expectEqual(@as(usize, 0), composers.container_report.?.errorCount());
    try testing.expectEqual(@as(i32, 2), composers.cdoc.current.size_x);
    const steps = composers.cdoc.undo_stack.items.len;
    try testing.expectEqual(@as(usize, 0), try composers.fixContainerAll(&editor));
    try testing.expectEqual(steps, composers.cdoc.undo_stack.items.len);
    // A stale size alone is a finding of its own, and Fix all repairs it.
    composers.cdoc.current.size_x = 5;
    _ = try composers.checkContainer(&editor);
    try testing.expectEqual(@as(usize, 1), composers.container_report.?.errorCount());
    try testing.expectEqual(@as(usize, 1), try composers.fixContainerAll(&editor));
    try testing.expectEqual(@as(i32, 2), composers.cdoc.current.size_x);
    // A fix with nothing to fix is refused.
    try testing.expectError(error.Refused, composers.fixContainerFinding(&editor, 99));
}

test "a patch outside the storages waits for its YES, then it is copied in and listed" {
    var fake = try fake_mod.fixture(testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(testing.allocator, fake.bridge());
    defer editor.deinit();
    var composers = Composers.init(testing.allocator);
    defer composers.deinit();
    try composers.newContainer();
    try testing.expectError(error.Refused, composers.beginImport(&editor, "relative.bzm"));
    try testing.expect(std.mem.indexOf(u8, composers.message(), "full path") != null);
    try testing.expect(!composers.pending_import.active);
    try composers.beginImport(&editor, "/outside/Mine.bzm");
    try testing.expect(composers.pending_import.active);
    try testing.expectEqualStrings("scenarios\\patches\\summer\\mine", composers.pending_import.dest.nameSlice());
    try testing.expectEqual(@as(usize, 0), fake.rmg_imports);
    // NO leaves everything as it was.
    composers.cancelImport();
    try testing.expect(!composers.pending_import.active);
    try testing.expectEqual(@as(usize, 0), composers.cdoc.current.patchCount());
    try composers.beginImport(&editor, "/outside/Mine.bzm");
    try testing.expectEqual(@as(usize, 1), try composers.confirmImport(&editor));
    try testing.expectEqual(@as(usize, 1), fake.rmg_imports);
    try testing.expectEqualStrings("scenarios\\patches\\summer\\mine", composers.cdoc.current.patches.items[0].name);
    // With no pending import there is nothing to confirm.
    try testing.expectError(error.Refused, composers.confirmImport(&editor));
}

test "the graphs composer: gestures, node and link properties, Check!, Save As" {
    var fake = try fake_mod.fixture(testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(testing.allocator, fake.bridge());
    defer editor.deinit();
    var composers = Composers.init(testing.allocator);
    defer composers.deinit();
    try composers.refreshNames(&editor);
    try testing.expectEqual(@as(usize, 1), composers.graph_names.items.len);
    try composers.openGraph(&editor, "summer\\graph_a");
    try testing.expectEqual(@as(usize, 2), composers.gdoc.current.nodes.items.len);
    // Drag on empty ground adds a node; its container is set through the dialog.
    try testing.expectEqual(rmg.CanvasOutcome.node_added, try composers.gesture(.{ .x = 96, .y = 0 }, .{ .x = 127, .y = 31 }, false, 1));
    try testing.expectEqual(@as(usize, 3), composers.gdoc.current.nodes.items.len);
    try composers.setNodeContainer(&editor, 2, "summer\\road_a");
    try testing.expectEqualStrings("scenarios\\containers\\summer\\road_a", composers.gdoc.current.nodes.items[2].container);
    // Ctrl+drag from node 1 to node 2 links them; the link's fields edit in cells.
    try testing.expectEqual(rmg.CanvasOutcome.link_added, try composers.gesture(.{ .x = 60, .y = 10 }, .{ .x = 100, .y = 10 }, true, 1));
    try testing.expectEqual(@as(usize, 2), composers.gdoc.current.links.items.len);
    try composers.setLinkField(1, .radius, "5");
    try testing.expectEqual(@as(f32, 160.0), composers.gdoc.current.links.items[1].radius);
    try composers.setLinkField(1, .parts, "4");
    try composers.setLinkField(1, .kind, "1");
    try composers.setLinkField(1, .desc, "terrain\\sets\\1\\rivers3d\\river_small");
    try testing.expectError(error.Refused, composers.setLinkField(1, .kind, "7"));
    try testing.expectError(error.Refused, composers.setLinkField(1, .distance, "abc"));
    try testing.expectError(error.Refused, composers.setLinkField(9, .parts, "9"));
    // Check! finds the 4-part link, Fix all raises it to 8.
    _ = try composers.checkGraph(&editor);
    var parts_finding = false;
    for (composers.graph_report.?.findings.items) |finding| {
        if (finding.fix == .set_parts) parts_finding = true;
    }
    try testing.expect(parts_finding);
    try testing.expect(try composers.fixGraphAll(&editor) >= 1);
    try testing.expectEqual(rmg.min_parts, composers.gdoc.current.links.items[1].parts);
    // A container of another season is refused by name.
    try composers.setNodeContainer(&editor, 0, "");
    try testing.expectEqual(@as(usize, 0), composers.gdoc.current.nodes.items[0].container.len);
    // Delete a node: its links go with it.
    try composers.deleteNode(2);
    try testing.expectEqual(@as(usize, 2), composers.gdoc.current.nodes.items.len);
    try testing.expectEqual(@as(usize, 1), composers.gdoc.current.links.items.len);
    // Save is Save As for the shipped graph; Save As a user name writes it.
    try testing.expectEqual(SaveResult.needs_save_as, try composers.saveGraph(&editor));
    try testing.expectEqual(SaveResult.saved, try composers.saveGraphAs(&editor, "user\\mine"));
    try testing.expect(!composers.gdoc.dirty);
    var back = try editor.readGraph("scenarios\\graphs\\user\\mine");
    defer back.deinit(testing.allocator);
    try testing.expect(back.eql(&composers.gdoc.current));
    // Undo walks every step back, redo forward.
    var steps: usize = 0;
    while (try composers.undoGraph()) steps += 1;
    try testing.expect(steps >= 5);
    try testing.expectEqual(@as(usize, 2), composers.gdoc.current.nodes.items.len);
    try testing.expectEqual(@as(usize, 1), composers.gdoc.current.links.items.len);
    try testing.expect(try composers.redoGraph());
}

test "a graph's supported settings follow the MFC: any alone when every container takes any, nothing for an empty node" {
    var fake = try fake_mod.fixture(testing.allocator);
    defer fake.deinit();
    var editor = Editor.init(testing.allocator, fake.bridge());
    defer editor.deinit();
    var composers = Composers.init(testing.allocator);
    defer composers.deinit();
    var text = std.ArrayListUnmanaged(u8).empty;
    defer text.deinit(testing.allocator);
    try composers.openGraph(&editor, "summer\\graph_a");
    // Node 1 of the fixture is empty: the graph answers nothing.
    try composers.graphSettingsText(&editor, &text);
    try testing.expectEqualStrings("", text.items);
    // Fill it with the same container, whose patches take any setting.
    try composers.setNodeContainer(&editor, 1, "summer\\road_a");
    try composers.graphSettingsText(&editor, &text);
    try testing.expectEqualStrings("<any setting>", text.items);
}

fn testObjectKnown(_: *anyopaque, name: []const u8) bool {
    return std.mem.eql(u8, name, "_Birch") or std.mem.eql(u8, name, "_Lime");
}

test "the fields composer opens a shipped field set, edits it by the tabs' rules, checks it and saves it as a user file" {
    const a = testing.allocator;
    var fake = try fake_mod.fixture(a);
    defer fake.deinit();
    var editor = Editor.init(a, fake.bridge());
    defer editor.deinit();
    var shipped = try rmg.FieldSet.initNew(a);
    {
        const shell = try shipped.addTileShell(a);
        _ = try shipped.addTile(a, shell, 3);
        _ = try shipped.addTile(a, shell, 5);
        const oshell = try shipped.addObjectShell(a);
        _ = try shipped.addObject(a, oshell, "_Birch");
    }
    try fake.addFieldSetFixture("scenarios\\fieldsets\\summer\\field00", true, shipped);
    var dummy: u8 = 0;
    var composers = Composers.init(a);
    defer composers.deinit();
    composers.object_lookup = .{ .ctx = &dummy, .has_fn = testObjectKnown };
    try composers.refreshNames(&editor);
    try testing.expectEqual(@as(usize, 1), composers.field_names.items.len);
    try testing.expectEqualStrings("summer\\field00", relativeName(field_folder, composers.field_names.items[0]));
    try composers.openField(&editor, "summer\\field00");
    try testing.expectEqual(@as(usize, 2), composers.fdoc.current.tileEntryCount());
    try testing.expect(!composers.fdoc.dirty);

    // Terrain tab: a shell, two tiles (one repeat), a weight, the season.
    const shell = try composers.addFieldShell(false);
    try testing.expectEqual(@as(usize, 1), shell);
    try testing.expectEqual(@as(usize, 1), try composers.addShellTiles(shell, &.{ 2, 2 }));
    try testing.expectEqual(@as(usize, 0), try composers.addShellTiles(shell, &.{2}));
    try testing.expect(try composers.setShellTileWeights(shell, &.{0}, 6));
    try testing.expect(!(try composers.setShellTileWeights(shell, &.{0}, 6)));
    try testing.expect(!(try composers.setShellTileWeights(shell, &.{0}, -1)));
    try testing.expect(try composers.setFieldShell(false, shell, .width, 3.5));
    try testing.expect(!(try composers.setFieldShell(false, shell, .width, -1)));
    try testing.expect(try composers.setFieldSeason(1));
    try testing.expect(!(try composers.setFieldSeason(1)));
    try testing.expectEqual(@as(i32, 1), composers.fdoc.current.season);
    // Objects tab: a shell with the MFC's defaults, an object, step and percent.
    const oshell = try composers.addFieldShell(true);
    try testing.expectEqual(@as(usize, 1), oshell);
    try testing.expectEqual(@as(usize, 1), try composers.addShellObjects(oshell, &.{"_Lime"}));
    try testing.expect(try composers.setFieldShell(true, oshell, .step, 6));
    try testing.expect(try composers.setFieldShell(true, oshell, .ratio, 40));
    try testing.expect(!(try composers.setFieldShell(true, oshell, .ratio, 140)));
    try testing.expect(!(try composers.setFieldShell(false, shell, .step, 6)));
    try testing.expect(composers.fdoc.current.object_shells.items[oshell].step == 6 and composers.fdoc.current.object_shells.items[oshell].ratio == 0.4);
    // Heights tab: each field by its text; a profile the storages lack stays out.
    try testing.expect(try composers.setFieldHeights(&editor, .height, "3.5"));
    try testing.expect(!(try composers.setFieldHeights(&editor, .height, "9")));
    try testing.expect(try composers.setFieldHeights(&editor, .pattern_min, "6"));
    try testing.expect(composers.fdoc.current.pattern_max == 6);
    try testing.expect(!(try composers.setFieldHeights(&editor, .profile, "scenarios\\profiles\\nope")));
    try testing.expect(std.mem.indexOf(u8, composers.message(), "not a .tga") != null);
    try testing.expect(try composers.setFieldHeights(&editor, .positive, "10"));
    try testing.expect(composers.fdoc.current.positive_ratio == 0.1);
    // Every edit is its own undo step: walk them all back and forward.
    try testing.expect(composers.fdoc.dirty and composers.fdoc.canUndo());
    var steps: usize = 0;
    while (try composers.undoField()) steps += 1;
    try testing.expect(steps >= 12);
    try testing.expectEqual(@as(usize, 2), composers.fdoc.current.tileEntryCount());
    try testing.expect(composers.fdoc.current.eql(&shipped));
    while (try composers.redoField()) {}
    try testing.expect(composers.fdoc.current.pattern_max == 6);

    // Check! lists, never rewrites: the new tile shell's tile 2 is fine (12 types);
    // put a tile past the tileset and an unknown object in, then find them.
    _ = try composers.addShellTiles(shell, &.{99});
    _ = try composers.addShellObjects(oshell, &.{"Ghost"});
    try testing.expect(try composers.checkField(&editor) >= 2);
    const before_tiles = composers.fdoc.current.tileEntryCount();
    try testing.expectEqual(before_tiles, composers.fdoc.current.tileEntryCount());
    const fixed = try composers.fixFieldAll(&editor);
    try testing.expect(fixed >= 2);
    try testing.expectEqual(before_tiles - 1, composers.fdoc.current.tileEntryCount());
    try testing.expectEqual(@as(usize, 0), composers.field_report.?.findings.items.len);
    // The shipped one is read-only: Save asks for a name, nothing is written.
    try testing.expectEqual(SaveResult.needs_save_as, try composers.saveField(&editor));
    try testing.expect(composers.fdoc.shipped);
    try testing.expectEqual(SaveResult.saved, try composers.saveFieldAs(&editor, "user\\mine"));
    try testing.expectEqualStrings("scenarios\\fieldsets\\user\\mine", composers.fdoc.name);
    try testing.expect(!composers.fdoc.shipped and !composers.fdoc.dirty);
    try testing.expectEqual(@as(usize, 2), composers.field_names.items.len);
    // The saved file reads back equal.
    var back = try editor.readFieldSet("scenarios\\fieldsets\\user\\mine");
    defer back.deinit(a);
    try testing.expect(back.eql(&composers.fdoc.current));
    // A new field set starts from the MFC's defaults and has no file yet.
    try composers.newField();
    try testing.expectEqual(SaveResult.needs_save_as, try composers.saveField(&editor));
    try testing.expect(composers.fdoc.current.height == 2.0);
    // An unknown name does not open and leaves the document alone.
    try testing.expectError(error.Refused, composers.openField(&editor, "summer\\nope"));
    try testing.expectEqual(@as(usize, 0), composers.fdoc.name.len);
}

test "the fields composer's tileset and profile facts come from the bridge" {
    const a = testing.allocator;
    var fake = try fake_mod.fixture(a);
    defer fake.deinit();
    fake.tileset_counts = .{ 12, 0, 12, 12 };
    var editor = Editor.init(a, fake.bridge());
    defer editor.deinit();
    const types = try editor.tilesetTypes(a, 0);
    defer a.free(types);
    try testing.expectEqual(@as(usize, 12), types.len);
    try testing.expectEqualStrings("terrain0", types[0].nameSlice());
    const none = try editor.tilesetTypes(a, 1);
    defer a.free(none);
    try testing.expectEqual(@as(usize, 0), none.len);
    try testing.expect(editor.rmgFileExists("scenarios\\profiles\\profile", ".tga"));
    try testing.expect(editor.rmgFileExists("\\Scenarios\\Profiles\\Profile", ".tga"));
    try testing.expect(!editor.rmgFileExists("scenarios\\profiles\\other", ".tga"));
    try testing.expect(!editor.rmgFileExists("..\\x", ".tga"));
}

test "the templates composer opens a shipped template, edits its lists, players and units, checks it and saves it with its QuickLoadMapInfo as a user file" {
    const a = testing.allocator;
    var fake = try fake_mod.fixture(a);
    defer fake.deinit();
    fake.known_files = &.{ "scenarios\\profiles\\profile.tga", "terrain\\sets\\1\\roads3d\\road_grunt.xml", "scenarios\\scripts\\sa\\secure_area.lua" };
    // Two field sets that load, and a shipped template of the fixture graph.
    var field = try rmg.FieldSet.initNew(a);
    {
        const shell = try field.addTileShell(a);
        _ = try field.addTile(a, shell, 3);
    }
    try fake.addFieldSetFixture("scenarios\\fieldsets\\summer\\field00", true, field);
    var field_two = try rmg.FieldSet.initNew(a);
    {
        const shell = try field_two.addTileShell(a);
        _ = try field_two.addTile(a, shell, 4);
    }
    try fake.addFieldSetFixture("scenarios\\fieldsets\\summer\\field01", true, field_two);
    var shipped = try rmg.Template.initNew(a);
    try shipped.addField(a, "scenarios\\fieldsets\\summer\\field00");
    try shipped.graphs.append(a, .{ .name = try a.dupe(u8, "scenarios\\graphs\\summer\\graph_a"), .weight = 2 });
    try shipped.setText(a, .season_folder, "terrain\\sets\\1\\");
    try shipped.script_ids.append(a, 3);
    try shipped.script_areas.append(a, try a.dupe(u8, "Ambush"));
    try shipped.setText(a, .script_file, "scenarios\\scripts\\sa\\secure_area");
    _ = shipped.setDefaultField(0);
    try fake.addTemplateFixture("scenarios\\templates\\summer\\template00", true, shipped);
    var editor = Editor.init(a, fake.bridge());
    defer editor.deinit();
    var dummy: u8 = 0;
    var composers = Composers.init(a);
    defer composers.deinit();
    composers.object_lookup = .{ .ctx = &dummy, .has_fn = testObjectKnown };
    try composers.refreshNames(&editor);
    try testing.expectEqual(@as(usize, 1), composers.template_names.items.len);
    try composers.openTemplate(&editor, "summer\\template00");
    try testing.expect(!composers.tdoc.dirty and composers.tdoc.current.graphs.items.len == 1 and composers.tdoc.current.default_field == 0);
    {
        var names = std.ArrayListUnmanaged(u8).empty;
        defer names.deinit(a);
        try composers.templateSettingsText(&editor, &names);
        // The fixture graph has an empty second node, so nothing is supported (the C++ answers 0).
        try testing.expectEqualStrings("", names.items);
    }

    // The lists: a graph that does not load stops the batch naming it; a field set
    // repeated goes to the end; a vso of the storages joins, one nobody has does not.
    var graph_names = [_][]const u8{ "summer\\graph_a", "summer\\nope" };
    try testing.expectEqual(@as(usize, 1), try composers.addTemplateGraphs(&editor, &graph_names));
    try testing.expect(std.mem.indexOf(u8, composers.message(), "Can't Add Graph to Template!") != null);
    try testing.expectEqual(@as(usize, 1), composers.tdoc.current.graphs.items.len);
    var field_names = [_][]const u8{ "summer\\field01", "summer\\field00", "summer\\missing" };
    try testing.expectEqual(@as(usize, 2), try composers.addTemplateFields(&editor, &field_names));
    try testing.expectEqual(@as(usize, 2), composers.tdoc.current.fields.items.len);
    try testing.expectEqualStrings("scenarios\\fieldsets\\summer\\field00", composers.tdoc.current.fields.items[1].name);
    try testing.expectEqual(@as(i32, 1), composers.tdoc.current.default_field);
    var vso_names = [_][]const u8{ "terrain\\sets\\1\\roads3d\\road_grunt", "terrain\\sets\\1\\roads3d\\nobody" };
    try testing.expectEqual(@as(usize, 1), try composers.addTemplateVsos(&editor, &vso_names));
    try testing.expect(try composers.setTemplateWeight(.graphs, 0, 5));
    try testing.expect(!(try composers.setTemplateWeight(.graphs, 0, 5)));
    try testing.expect(!(try composers.setTemplateWeight(.graphs, 0, -1)));
    try testing.expect(!(try composers.setTemplateWeight(.fields, 9, 1)));
    try testing.expect(try composers.setTemplateVso(0, 3.5, 40));
    try testing.expect(!(try composers.setTemplateVso(0, 0, 40)) and !(try composers.setTemplateVso(0, 3, 140)));
    try testing.expect(composers.tdoc.current.vso.items[0].width == 3.5 * 32 and composers.tdoc.current.vso.items[0].opacity == 0.4);
    try testing.expect(try composers.setTemplateDefaultField(0));
    try testing.expect(!(try composers.setTemplateDefaultField(0)) and !(try composers.setTemplateDefaultField(5)));
    try testing.expect(try composers.setTemplateText(.script_file, "scenarios\\scripts\\sa\\other"));
    try testing.expect(try composers.setTemplateMod("Mod One", "1.0"));
    try testing.expectEqualStrings("Mod One", composers.tdoc.current.mod_name);
    try testing.expect(try composers.setTemplateMod("", ""));

    // Diplomacy: a player on side 1 gets the defaults; the unit creation follows;
    // the table keeps two players; the game type and the attacking side go with it.
    try testing.expect(try composers.addTemplatePlayer(1));
    try testing.expectEqual(@as(usize, 3), composers.tdoc.current.units.items.len);
    try testing.expectEqualSlices(u8, &.{ 0, 1, 1, 2 }, composers.tdoc.current.diplomacies.items);
    try testing.expect(try composers.setTemplatePlayerSide(2, 0));
    try testing.expect(!(try composers.setTemplatePlayerSide(3, 0)));
    try testing.expect(try composers.setTemplateGameType(2, 1));
    try testing.expect(try composers.deleteTemplatePlayer(2));
    try testing.expect(!(try composers.deleteTemplatePlayer(1)));
    try testing.expectEqual(@as(usize, 2), composers.tdoc.current.units.items.len);

    // Units...: the rules the map's own unit creation holds.
    try testing.expect(try composers.setTemplateUnit(&editor, 0, "party", "Germany"));
    try testing.expect(!(try composers.setTemplateUnit(&editor, 0, "party", "Narnia")));
    try testing.expect(std.mem.indexOf(u8, composers.message(), "partys.xml") != null);
    try testing.expect(try composers.setTemplateUnit(&editor, 1, "aircraft0_name", "Yak-7"));
    try testing.expect(!(try composers.setTemplateUnit(&editor, 1, "aircraft0_name", "Zeppelin")));
    try testing.expect(try composers.setTemplateUnit(&editor, 1, "aircraft2_formation", "4"));
    try testing.expect(!(try composers.setTemplateUnit(&editor, 1, "aircraft2_formation", "33")));
    try testing.expect(try composers.setTemplateUnit(&editor, 1, "aircraft4_count", "0"));
    try testing.expect(!(try composers.setTemplateUnit(&editor, 1, "aircraft4_count", "256")));
    try testing.expect(try composers.setTemplateUnit(&editor, 0, "paratroop_name", "German_rpd_43"));
    try testing.expect(try composers.setTemplateUnit(&editor, 0, "paratroop_count", "12"));
    try testing.expect(try composers.setTemplateUnit(&editor, 0, "relax", "240"));
    try testing.expect(!(try composers.setTemplateUnit(&editor, 0, "relax", "0")));
    try testing.expect(!(try composers.setTemplateUnit(&editor, 0, "relax", "abc")));
    try testing.expect(!(try composers.setTemplateUnit(&editor, 0, "nonsense", "1")));
    try testing.expect(!(try composers.setTemplateUnit(&editor, 7, "relax", "9")));
    try testing.expect(try composers.addTemplateAppear(0, 1024, 0));
    try testing.expect(try composers.addTemplateAppear(0, 3072, 0));
    try testing.expect(try composers.setTemplateAppear(0, 1, 2048, 64));
    try testing.expect(!(try composers.addTemplateAppear(0, -1, 0)) and !(try composers.setTemplateAppear(0, 5, 1, 1)));
    try testing.expectEqual(@as(u32, 2), composers.tdoc.current.units.items[0].appear_count);
    try testing.expect(try composers.removeTemplateAppear(0, 0));
    try testing.expectEqual(@as(u32, 1), composers.tdoc.current.units.items[0].appear_count);
    try testing.expectEqual(@as(f32, 2048), composers.tdoc.current.units.items[0].appear[0].x);
    try testing.expectEqual(@as(i32, 240), composers.tdoc.current.units.items[0].relax_time);

    // Every edit was one step of the file's own undo.
    try testing.expect(composers.tdoc.dirty);
    var steps: usize = 0;
    while (try composers.undoTemplate()) steps += 1;
    try testing.expect(steps >= 20);
    try testing.expect(composers.tdoc.current.eql(&shipped));
    while (try composers.redoTemplate()) {}
    try testing.expectEqual(@as(usize, 1), composers.tdoc.current.units.items[0].appear_count);

    // Check! (the addition): the saved-in-the-fixture facts are clean but the vso with its
    // 3.5-cell width is fine, the script is not there (a warning), a field set the storages do not
    // hold is an error with its removal as the explicit fix.
    try testing.expect(try composers.checkTemplate(&editor) >= 1);
    try testing.expectEqual(@as(usize, 0), composers.template_report.?.errorCount());
    try composers.tdoc.current.fields.append(a, .{ .name = try a.dupe(u8, "scenarios\\fieldsets\\summer\\ghost"), .weight = 1 });
    _ = try composers.tdoc.begin();
    try testing.expect(try composers.checkTemplate(&editor) >= 2);
    try testing.expectEqual(@as(usize, 1), composers.template_report.?.errorCount());
    try testing.expectEqual(@as(usize, 1), try composers.fixTemplateAll(&editor));
    try testing.expectEqual(@as(usize, 0), composers.template_report.?.errorCount());
    try testing.expectEqual(@as(usize, 2), composers.tdoc.current.fields.items.len);

    // The shipped one is read-only: Save asks for a name, nothing is written; Save As writes.
    try testing.expectEqual(SaveResult.needs_save_as, try composers.saveTemplate(&editor));
    try testing.expect(composers.tdoc.shipped);
    try testing.expectEqual(SaveResult.saved, try composers.saveTemplateAs(&editor, "user\\mine"));
    try testing.expectEqualStrings("scenarios\\templates\\user\\mine", composers.tdoc.name);
    try testing.expect(!composers.tdoc.shipped and !composers.tdoc.dirty);
    try testing.expectEqual(@as(usize, 2), composers.template_names.items.len);
    var back = try editor.readTemplate("scenarios\\templates\\user\\mine");
    defer back.deinit(a);
    try testing.expect(back.eql(&composers.tdoc.current));
    try composers.newTemplate();
    try testing.expectEqual(SaveResult.needs_save_as, try composers.saveTemplate(&editor));
    try testing.expectEqual(@as(usize, 2), composers.tdoc.current.units.items.len);
    try testing.expectError(error.Refused, composers.openTemplate(&editor, "summer\\nope"));
}
