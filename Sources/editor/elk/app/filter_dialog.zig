//! Browse > Create Filter... (`CCreateFilterDialog` with its name box,
//! `CCreateFilterNameDialog`): the named filters' list, Add, Rename and
//! Delete, and the selected filter's criteria the core checks, the four
//! states, "changed only" and the folder conditions. The dialog edits a copy
//! of the settings; OK keeps it, Cancel drops it, as the MFC dialog edited a
//! copy of `params.filters`. The buttons and the scripted runs call the same
//! methods.
//!
//! Adaptations: the name is typed in the dialog's own Name box instead of a
//! second dialog, and a condition is typed as folder substrings joined by
//! `&` instead of ticked in a list of the database's folders.
const std = @import("std");
const imgui = @import("editor_imgui");
const elk = @import("elk_core");
const settings_mod = @import("settings.zig");

const ig = imgui.c;
const Settings = settings_mod.Settings;
const filter_mod = elk.filter;

pub const popup_id = "Create Filter";

pub const FilterDialog = struct {
    draft: Settings,
    /// The filter the controls show; the current one when the dialog opens.
    selected: ?usize = null,
    condition_selected: ?usize = null,
    name: [settings_mod.max_filter_name:0]u8 = @splat(0),
    condition: [128:0]u8 = @splat(0),
    /// The popup is opened in the next frame (ImGui opens popups inside one).
    opening: bool = true,
    /// The last frame drew the dialog (a scripted press needs it).
    drawn: bool = false,
    /// A scripted OK or Cancel, taken inside the popup at the next frame
    /// as the buttons are.
    pending: ?Outcome = null,

    pub fn start(settings: *const Settings) FilterDialog {
        var dialog: FilterDialog = .{ .draft = settings.* };
        dialog.pick(dialog.find(settings.currentFilter()));
        return dialog;
    }

    pub fn find(self: *const FilterDialog, name: []const u8) ?usize {
        for (0..self.draft.filterCount()) |index| {
            if (std.mem.eql(u8, self.draft.filterAt(index).name, name)) return index;
        }
        return null;
    }

    /// A click in the list: the filter's name goes into the Name box.
    pub fn pick(self: *FilterDialog, index: ?usize) void {
        self.selected = index;
        self.condition_selected = null;
        const name = if (index) |at| self.draft.filterAt(at).name else "";
        _ = std.mem.printSentinel(&self.name, "{s}", .{name[0..@min(name.len, self.name.len - 1)]}, 0) catch {};
    }

    pub fn typedName(self: *const FilterDialog) []const u8 {
        return std.mem.sliceTo(&self.name, 0);
    }

    /// Add: a new filter keeping every text, named from the Name box.
    pub fn add(self: *FilterDialog) bool {
        const index = self.draft.addFilter(self.typedName()) orelse return false;
        self.pick(index);
        return true;
    }

    pub fn rename(self: *FilterDialog) bool {
        const index = self.selected orelse return false;
        if (!self.draft.renameFilter(index, self.typedName())) return false;
        self.pick(index);
        return true;
    }

    pub fn delete(self: *FilterDialog) bool {
        const index = self.selected orelse return false;
        if (!self.draft.deleteFilter(index)) return false;
        self.pick(if (self.draft.filterCount() == 0) null else @min(index, self.draft.filterCount() - 1));
        return true;
    }

    pub fn selectedFilter(self: *const FilterDialog) ?filter_mod.Filter {
        return self.draft.filterAt(self.selected orelse return null);
    }

    /// The state boxes and "changed only" of the selected filter.
    pub fn setCriteria(self: *FilterDialog, states: [4]bool, changed: bool) bool {
        var filter = self.selectedFilter() orelse return false;
        filter.states = states;
        filter.changed = changed;
        return self.draft.setFilter(self.selected.?, filter);
    }

    /// Add Condition: the Condition box's substrings as one more AND-list.
    pub fn addCondition(self: *FilterDialog, typed: []const u8) bool {
        var filter = self.selectedFilter() orelse return false;
        var buffer: [settings_mod.max_path]u8 = undefined;
        filter.conditions = filter_mod.withCondition(filter.conditions, typed, &buffer) orelse return false;
        if (!self.draft.setFilter(self.selected.?, filter)) return false;
        self.condition = @splat(0);
        return true;
    }

    pub fn removeCondition(self: *FilterDialog) bool {
        var filter = self.selectedFilter() orelse return false;
        const index = self.condition_selected orelse return false;
        var buffer: [settings_mod.max_path]u8 = undefined;
        filter.conditions = filter_mod.withoutCondition(filter.conditions, index, &buffer);
        if (!self.draft.setFilter(self.selected.?, filter)) return false;
        self.condition_selected = null;
        return true;
    }

    /// OK: the edited filters and the current one (which a rename or a
    /// delete may have moved) go back into `settings`.
    pub fn keep(self: *const FilterDialog, settings: *Settings) void {
        settings.filter_storage = self.draft.filter_storage;
        settings.filter_count = self.draft.filter_count;
        settings.current_filter_storage = self.draft.current_filter_storage;
    }

    pub const Outcome = enum { open, ok, cancel };

    pub fn draw(self: *FilterDialog) Outcome {
        self.drawn = false;
        if (self.opening) {
            _ = ig.igOpenPopup(popup_id, 0);
            self.opening = false;
        }
        ig.igSetNextWindowSize(.{ .x = ig.igGetFontSize() * 34, .y = 0 }, ig.ImGuiCond_Appearing);
        if (!ig.igBeginPopupModal(popup_id, null, 0)) return .cancel;
        defer ig.igEndPopup();
        self.drawn = true;
        if (self.pending) |outcome| {
            ig.igCloseCurrentPopup();
            return outcome;
        }
        var label: [settings_mod.max_path:0]u8 = undefined;

        ig.igTextUnformatted("Filters");
        if (ig.igBeginListBox("##filters", .{ .x = -1, .y = ig.igGetTextLineHeightWithSpacing() * 6 })) {
            for (0..self.draft.filterCount()) |index| {
                const filter = self.draft.filterAt(index);
                const text = std.mem.printSentinel(&label, "{s} ({d})", .{ filter.name, filter_mod.conditionCount(filter.conditions) }, 0) catch continue;
                ig.igPushIDInt(@intCast(index));
                if (ig.igSelectableEx(text.ptr, self.selected == index, 0, .{ .x = 0, .y = 0 })) self.pick(index);
                ig.igPopID();
            }
            ig.igEndListBox();
        }
        _ = ig.igInputText("Name", &self.name, self.name.len, 0);
        const has_name = self.typedName().len != 0;
        ig.igBeginDisabled(!has_name or self.draft.filterCount() >= settings_mod.filter_capacity);
        if (ig.igButton("Add")) _ = self.add();
        ig.igEndDisabled();
        ig.igSameLine();
        ig.igBeginDisabled(!has_name or self.selected == null);
        if (ig.igButton("Rename")) _ = self.rename();
        ig.igEndDisabled();
        ig.igSameLine();
        ig.igBeginDisabled(self.selected == null or self.draft.filterCount() <= 1);
        if (ig.igButton("Delete")) _ = self.delete();
        ig.igEndDisabled();

        ig.igSeparator();
        if (self.selectedFilter()) |filter| {
            var states = filter.states;
            var changed = filter.changed;
            var edited = false;
            const names = [_][*:0]const u8{ "Not translated", "Outdated", "Translated", "Approved" };
            for (names, &states, 0..) |name, *keep_state, index| {
                if (index != 0) ig.igSameLine();
                if (ig.igCheckbox(name, keep_state)) edited = true;
            }
            if (ig.igCheckbox("Changed only", &changed)) edited = true;
            if (edited) _ = self.setCriteria(states, changed);

            // The edit rewrote the line `filter` borrowed.
            const conditions = self.selectedFilter().?.conditions;
            ig.igTextUnformatted("Folder conditions (any one; each holds every part)");
            if (ig.igBeginListBox("##conditions", .{ .x = -1, .y = ig.igGetTextLineHeightWithSpacing() * 4 })) {
                for (0..filter_mod.conditionCount(conditions)) |index| {
                    const text = conditionLabel(&label, filter_mod.conditionAt(conditions, index)) catch continue;
                    ig.igPushIDInt(@intCast(index));
                    if (ig.igSelectableEx(text.ptr, self.condition_selected == index, 0, .{ .x = 0, .y = 0 })) self.condition_selected = index;
                    ig.igPopID();
                }
                ig.igEndListBox();
            }
            _ = ig.igInputText("Condition", &self.condition, self.condition.len, 0);
            ig.igSameLine();
            if (ig.igButton("Add Condition")) _ = self.addCondition(std.mem.sliceTo(&self.condition, 0));
            ig.igSameLine();
            ig.igBeginDisabled(self.condition_selected == null);
            if (ig.igButton("Remove Condition")) _ = self.removeCondition();
            ig.igEndDisabled();
        } else ig.igTextUnformatted("Select a filter.");

        ig.igSeparator();
        if (ig.igButton("OK")) {
            ig.igCloseCurrentPopup();
            return .ok;
        }
        ig.igSameLine();
        if (ig.igButton("Cancel") or ig.igIsKeyPressedEx(ig.ImGuiKey_Escape, false)) {
            ig.igCloseCurrentPopup();
            return .cancel;
        }
        return .open;
    }
};

/// A condition as the MFC dialog listed it: `"a" & "b"`.
fn conditionLabel(buffer: []u8, condition: []const u8) ![:0]const u8 {
    var writer: std.Io.Writer = .fixed(buffer[0 .. buffer.len - 1]);
    var parts = std.mem.splitScalar(u8, condition, ',');
    var first = true;
    while (parts.next()) |part| : (first = false) {
        try writer.print("{s}\"{s}\"", .{ if (first) "" else " & ", part });
    }
    const len = writer.buffered().len;
    buffer[len] = 0;
    return buffer[0..len :0];
}
