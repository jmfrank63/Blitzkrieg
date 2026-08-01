const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");
const surface = @import("surface.zig");

/// A callback that is invoked when a tray entry is selected.
///
/// ## Function Parameters
/// * `user_data`: An optional pointer to pass extra data to the callback when it will be invoked.
/// * `entry`: The tray entry that was selected.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn Callback(comptime UserData: type) type {
    return *const fn (
        user_data: ?*UserData,
        entry: Entry,
    ) void;
}

/// An opaque handle representing an entry on a system tray object.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Entry = struct {
    value: *c.SDL_TrayEntry,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(*c.SDL_TrayEntry) == @sizeOf(Entry));
    }

    /// Simulate a click on a tray entry.
    ///
    /// ## Function Parameters
    /// * `self`: The entry to activate.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn click(
        self: Entry,
    ) void {
        c.SDL_ClickTrayEntry(self.value);
    }

    /// Create a submenu for a system tray entry.
    ///
    /// ## Function Parameters
    /// * `self`: The tray entry to bind the menu to.
    ///
    /// ## Return Value
    /// Returns the newly created menu.
    ///
    /// ## Remarks
    /// This should be called at most once per tray entry.
    ///
    /// This function does the same thing as `tray.Tray.createMenu()`, except that it takes a `tray.Entry` instead of a `tray.Tray`.
    ///
    /// A menu does not need to be destroyed; it will be destroyed with the tray.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn createSubmenu(
        self: Entry,
    ) !Menu {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_TrayMenu, c.SDL_CreateTraySubmenu(self.value)),
        };
    }

    /// Gets whether or not an entry is checked.
    ///
    /// ## Function Parameters
    /// * `self`: The entry to be read.
    ///
    /// ## Return Value
    /// Returns true if the entry is checked; false otherwise.
    ///
    /// ## Remarks
    /// The entry must have been created with the `entry` field being a `checkbox`.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getChecked(
        self: Entry,
    ) bool {
        return c.SDL_GetTrayEntryChecked(self.value);
    }

    /// Gets whether or not an entry is enabled.
    ///
    /// ## Function Parameters
    /// * `self`: The entry to be read.
    ///
    /// ## Return Value
    /// Returns true if the entry is enabled; false otherwise.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getEnabled(
        self: Entry,
    ) bool {
        return c.SDL_GetTrayEntryEnabled(self.value);
    }

    /// Gets the label of an entry.
    ///
    /// ## Function Parameters
    /// * `self`: The entry to be read.
    ///
    /// ## Return Value
    /// Returns the label of the entry in UTF-8 encoding.
    ///
    /// ## Remarks
    /// If the returned value is `null`, the entry is a separator.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getLabel(
        self: Entry,
    ) ?[:0]const u8 {
        const ret = c.SDL_GetTrayEntryLabel(self.value);
        if (ret == null)
            return null;
        return std.mem.span(ret);
    }

    /// Gets the menu containing a certain tray entry.
    ///
    /// ## Function Parameters
    /// * `self`: The entry for which to get the parent menu.
    ///
    /// ## Return Value
    /// Returns the parent menu.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getParent(
        self: Entry,
    ) Menu {
        return .{
            .value = c.SDL_GetTrayEntryParent(self.value).?,
        };
    }

    /// Gets a previously created tray entry submenu.
    ///
    /// ## Function Parameters
    /// * `self`: The tray entry to bind the menu to.
    ///
    /// ## Return Value
    /// Returns the newly created menu.
    ///
    /// ## Remarks
    /// You should have called `tray.Entry.createSubmenu()` on the entry object.
    /// This function allows you to fetch it again later.
    ///
    /// This function does the same thing as `tray.Tray.getMenu()`, except that it takes a `tray.Entry` instead of a `tray.Tray`.
    ///
    /// A menu does not need to be destroyed; it will be destroyed with the tray.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getSubmenu(
        self: Entry,
    ) Menu {
        return .{
            .value = c.SDL_GetTraySubmenu(self.value).?,
        };
    }

    /// Removes a tray entry.
    ///
    /// ## Function Parameters
    /// * `self`: The entry to be deleted.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn remove(
        self: Entry,
    ) void {
        c.SDL_RemoveTrayEntry(self.value);
    }

    /// Sets a callback to be invoked when the entry is selected.
    ///
    /// ## Function Parameters
    /// * `self`: The entry to be updated.
    /// * `UserData`: Type of user data to be passed to the callback.
    /// * `callback`: A callback to be invoked when the entry is selected.
    /// * `user_data`: An optional pointer to pass extra data to the callback when it will be invoked.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setCallback(
        self: Entry,
        comptime UserData: type,
        comptime callback: Callback(UserData),
        user_data: ?*UserData,
    ) void {
        const Cb = struct {
            fn run(
                user_data_c: ?*anyopaque,
                entry_c: ?*c.SDL_TrayEntry,
            ) callconv(.c) void {
                callback(@ptrCast(@alignCast(user_data_c)), .{ .value = entry_c.? });
            }
        };
        c.SDL_SetTrayEntryCallback(self.value, Cb.run, @ptrCast(user_data));
    }

    /// Sets whether or not an entry is checked.
    ///
    /// ## Function Parameters
    /// * `self`: The entry to be updated.
    /// * `checked`: True if the entry should be checked; false otherwise.
    ///
    /// ## Remarks
    /// The entry must have been created with the `EntryFlags.entry.checkbox` value used.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setChecked(
        self: Entry,
        checked: bool,
    ) void {
        c.SDL_SetTrayEntryChecked(self.value, checked);
    }

    /// Sets whether or not an entry is enabled.
    ///
    /// ## Function Parameters
    /// * `self`: The entry to be updated.
    /// * `enabled`: True if the entry should be enabled; false otherwise.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setEnabled(
        self: Entry,
        enabled: bool,
    ) void {
        c.SDL_SetTrayEntryEnabled(self.value, enabled);
    }

    /// Sets the label of an entry.
    ///
    /// ## Function Parameters
    /// * `self`: The entry to be updated.
    /// * `label`: The new label for the entry in UTF-8 encoding.
    ///
    /// ## Remarks
    /// An entry cannot change between a separator and an ordinary entry; that is, it is not possible to set a non-`null` label on an entry that has a `null` label (separators).
    /// The function will silently fail if that happens.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setLabel(
        self: Entry,
        label: [:0]const u8,
    ) void {
        c.SDL_SetTrayEntryLabel(self.value, label.ptr);
    }
};

/// Flags that control the creation of system tray entries.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const EntryFlags = struct {
    /// Type of entry.
    entry: union(EntryType) {
        /// Make the entry a simple button.
        button: void,
        /// Make the entry a checkbox, value indicates if checked.
        checkbox: bool,
        /// Prepare the entry to have a submenu.
        submenu: void,
    },
    /// Make the entry disabled.
    disabled: bool = false,

    /// Convert this to an SDL value.
    pub fn toSdl(self: EntryFlags) c.SDL_TrayEntryFlags {
        var ret: c.SDL_TrayEntryFlags = 0;
        switch (self.entry) {
            .button => ret |= c.SDL_TRAYENTRY_BUTTON,
            .checkbox => |val| {
                ret |= c.SDL_TRAYENTRY_CHECKBOX;
                if (val)
                    ret |= c.SDL_TRAYENTRY_CHECKED;
            },
            .submenu => ret |= c.SDL_TRAYENTRY_SUBMENU,
        }
        if (self.disabled)
            ret |= c.SDL_TRAYENTRY_DISABLED;
        return ret;
    }
};

/// An entry type for a system tray.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const EntryType = enum(c.SDL_TrayEntryFlags) {
    /// Make the entry a simple button.
    button = c.SDL_TRAYENTRY_BUTTON,
    /// Make the entry a checkbox.
    checkbox = c.SDL_TRAYENTRY_CHECKBOX,
    /// Prepare the entry to have a submenu.
    submenu = c.SDL_TRAYENTRY_SUBMENU,
};

/// An opaque handle representing a menu/submenu on a system tray object.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Menu = struct {
    value: *c.SDL_TrayMenu,

    /// Returns a list of entries in the menu, in order.
    ///
    /// ## Function Parameters
    /// * `self`: The menu to get entries from.
    ///
    /// ## Return Value
    /// Returns a slice of tray entries within the given menu.
    /// This becomes invalid once any function that creates or destroys entries in the menu is called.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getEntries(
        self: Menu,
    ) []const Entry {
        var count: c_int = undefined;
        const ret: [*]const Entry = @ptrCast(c.SDL_GetTrayEntries(self.value, &count).?);
        return ret[0..@intCast(count)];
    }

    /// Gets the entry for which the menu is a submenu, if the current menu is a submenu.
    ///
    /// ## Function Parameters
    /// * `self`: The menu for which to get the parent entry.
    ///
    /// ## Return Value
    /// Returns the parent entry, or `null` if this menu is not a submenu.
    ///
    /// ## Remarks
    /// Either this function or `tray.Menu.getParentTray()` will return non-`null` for any given menu.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getParentEntry(
        self: Menu,
    ) ?Entry {
        const ret = c.SDL_GetTrayMenuParentEntry(self.value);
        if (ret) |val| {
            return .{
                .value = val,
            };
        }
        return null;
    }

    /// Gets the tray for which this menu is the first-level menu, if the current menu isn't a submenu.
    ///
    /// ## Function Parameters
    /// * `self`: The menu for which to get the parent entry.
    ///
    /// ## Return Value
    /// Returns the parent tray, or `null` if this menu is a submenu.
    ///
    /// ## Remarks
    /// Either this function or `tray.Menu.getParentEntry()` will return non-`null` for any given menu.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getParentTray(
        self: Menu,
    ) ?Tray {
        const ret = c.SDL_GetTrayMenuParentTray(self.value);
        if (ret) |val| {
            return .{
                .value = val,
            };
        }
        return null;
    }

    /// Insert a tray entry at a given position.
    ///
    /// ## Function Parameters
    /// * `self`: The menu to append the entry to.
    /// * `pos`: The desired position for the new entry. Entries at or following this place will be moved. If this is `null`, the entry is appended.
    /// * `label`: The text to be displayed on the entry, in UTF-8 encoding, or `null` for a separator.
    /// * `flags`: How to create the entry.
    ///
    /// ## Return Value
    /// Returns the newly created entry, or `null` if `pos` is out of bounds.
    ///
    /// ## Remarks
    /// If label is `null`, the entry will be a separator.
    /// Many functions won't work for an entry that is a separator.
    ///
    /// An entry does not need to be destroyed; it will be destroyed with the tray.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn insertAt(
        self: Menu,
        pos: ?usize,
        label: ?[:0]const u8,
        flags: EntryFlags,
    ) ?Entry {
        const ret = c.SDL_InsertTrayEntryAt(
            self.value,
            if (pos) |val| @intCast(val) else -1,
            if (label) |val| val.ptr else null,
            flags.toSdl(),
        );
        if (ret) |val| {
            return .{ .value = val };
        }
        return null;
    }
};

/// An opaque handle representing a toplevel system tray object.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Tray = struct {
    value: *c.SDL_Tray,

    /// Create a menu for a system tray.
    ///
    /// ## Function Parameters
    /// * `self`: The tray to bind the menu to.
    ///
    /// ## Return Value
    /// Returns the newly created menu.
    ///
    /// ## Remarks
    /// This should be called at most once per tray icon.
    ///
    /// This function does the same thing as `tray.Tray.createMenu()`, except that it takes a `tray.Tray` instead of a `tray.Entry`.
    ///
    /// A menu does not need to be destroyed; it will be destroyed with the tray.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn createMenu(
        self: Tray,
    ) !Menu {
        return .{
            .value = try errors.wrapCallNull(*c.SDL_TrayMenu, c.SDL_CreateTrayMenu(self.value)),
        };
    }

    /// Destroys a tray object.
    ///
    /// ## Function Parameters
    /// * `self`: The tray icon to be destroyed.
    ///
    /// ## Remarks
    /// This also destroys all associated menus and entries.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn deinit(
        self: Tray,
    ) void {
        c.SDL_DestroyTray(self.value);
    }

    /// Gets a previously created tray menu.
    ///
    /// ## Function Parameters
    /// * `self`: The tray entry to bind the menu to.
    ///
    /// ## Return Value
    /// Returns the newly created menu.
    ///
    /// ## Remarks
    /// You should have called `tray.Tray.createMenu()` on the tray object.
    /// This function allows you to fetch it again later.
    ///
    /// This function does the same thing as `tray.Entry.getSubmenu()` except that it takes a `tray.Tray` instead of a `tray.Entry`.
    ///
    /// A menu does not need to be destroyed; it will be destroyed with the tray.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getMenu(
        self: Tray,
    ) Menu {
        return .{
            .value = c.SDL_GetTrayMenu(self.value).?,
        };
    }

    /// Updates the system tray icon's icon.
    ///
    /// ## Function Parameters
    /// * `tray`: The tray icon to be updated.
    /// * `icon`: The new icon. May be `null`.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setIcon(
        self: Tray,
        icon: ?surface.Surface,
    ) void {
        c.SDL_SetTrayIcon(self.value, if (icon) |val| val.value else null);
    }

    /// Create an icon to be placed in the operating system's tray, or equivalent.
    ///
    /// ## Function Parameters
    /// * `icon`: A surface to be used as icon. May be `null`.
    /// * `tooltip`: A tooltip to be displayed when the mouse hovers the icon in UTF-8 encoding. Not supported on all platforms. May be `null`
    ///
    /// ## Return Value
    /// Returns the newly created system tray icon.
    ///
    /// ## Remarks
    /// Many platforms advise not using a system tray unless persistence is a necessary feature.
    /// Avoid needlessly creating a tray icon, as the user may feel like it clutters their interface.
    ///
    /// Using tray icons require the video subsystem.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    ///
    /// ## Code Examples
    /// TODO!!!
    pub fn init(
        icon: ?surface.Surface,
        tooltip: ?[:0]const u8,
    ) !Tray {
        return .{ .value = try errors.wrapCallNull(*c.SDL_Tray, c.SDL_CreateTray(
            if (icon) |val| val.value else null,
            if (tooltip) |val| val.ptr else null,
        )) };
    }

    /// Updates the system tray icon's tooltip.
    ///
    /// ## Function Parameters
    /// * `self`: The tray icon to be updated.
    /// * `tooltip`: The new tooltip in UTF-8 encoding. May be `null`.
    ///
    /// ## Thread Safety
    /// This function should be called on the thread that created the tray.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn setTooltip(
        self: Tray,
        tooltip: ?[:0]const u8,
    ) void {
        c.SDL_SetTrayTooltip(self.value, if (tooltip) |val| val.ptr else null);
    }
};

/// Update the trays.
///
/// ## Remarks
/// This is called automatically by the event loop and is only needed if you're using trays but aren't handling SDL events.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn update() void {
    c.SDL_UpdateTrays();
}

// Tray tests.
test "Tray" {
    errors.refAllDeclsRecursive(@This());
}
