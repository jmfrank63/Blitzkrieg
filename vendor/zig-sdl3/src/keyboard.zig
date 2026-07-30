const c = @import("c");
const errors = @import("errors.zig");
const keycode = @import("keycode.zig");
const properties = @import("properties.zig");
const rect = @import("rect.zig");
const scancode = @import("scancode.zig");
const std = @import("std");
const video = @import("video.zig");

/// Auto capitalization type.
///
/// These are the valid values for `keyboard.TextInputProperties.capitalization`.
/// Not every value is valid on every platform, but where a value isn't supported, a reasonable fallback will be used.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Capitalization = enum(c.SDL_Capitalization) {
    /// No capitalization will be done.
    none,
    /// The first letter of sentences will be capitalized.
    sentences = c.SDL_CAPITALIZE_SENTENCES,
    /// The first letter of words will be capitalized.
    words = c.SDL_CAPITALIZE_WORDS,
    /// All letters will be capitalized.
    letters = c.SDL_CAPITALIZE_LETTERS,

    /// Get from an SDL value.
    pub fn fromSdl(value: c.SDL_Capitalization) Capitalization {
        return @enumFromInt(value);
    }

    /// Convert to an SDL value.
    pub fn toSdl(self: Capitalization) c.SDL_Capitalization {
        return @intFromEnum(self);
    }
};

/// Properties for text input.
///
/// ## Version
/// This struct is provided by SDL 3.2.0.
pub const TextInputProperties = struct {
    /// Value that describes text being input, defaults to `TextInputType.text`.
    input_type: ?TextInputType = null,
    /// Describes how text should be capitalized.
    /// Defaults to `Capitalization.sentences` for normal text entry, `Capitalization.words` for `TextInputType.text_name`, and `Capitalization.none` for email, username, and passwords.
    capitalization: ?Capitalization = null,
    /// True to enable auto completion and auto correction, defaults to true.
    auto_correct: ?bool = null,
    /// True if multiple lines of text are allowed.
    /// This defaults to true if `hints.Type.return_key_hides_ime` is false or is not set, and defaults to false if `hints.Type.return_key_hides_ime` is true.
    multi_line: ?bool = null,
    /// On Android you can directly specify the input type overriding other properties.
    /// This is documented at https://developer.android.com/reference/android/text/InputType.
    android_input_type: ?i64 = null,

    /// Convert to SDL properties, must be freed.
    pub fn toSdl(self: TextInputProperties) !properties.Group {
        const ret = try properties.Group.init();
        if (self.input_type) |val|
            try ret.set(c.SDL_PROP_TEXTINPUT_TYPE_NUMBER, .{ .number = @intFromEnum(val) });
        if (self.capitalization) |val|
            try ret.set(c.SDL_PROP_TEXTINPUT_CAPITALIZATION_NUMBER, .{ .number = @intFromEnum(val) });
        if (self.auto_correct) |val|
            try ret.set(c.SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, .{ .boolean = val });
        if (self.multi_line) |val|
            try ret.set(c.SDL_PROP_TEXTINPUT_MULTILINE_BOOLEAN, .{ .boolean = val });
        if (self.android_input_type) |val|
            try ret.set(c.SDL_PROP_TEXTINPUT_ANDROID_INPUTTYPE_NUMBER, .{ .number = val });
        return ret;
    }
};

/// Text input type.
///
/// ## Remarks
/// These are the valid values for `keyboard.TextInputProperties.input_type`.
/// Not every value is valid on every platform, but where a value isn't supported, a reasonable fallback will be used.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const TextInputType = enum(c.SDL_TextInputType) {
    /// The input is text.
    text = c.SDL_TEXTINPUT_TYPE_TEXT,
    /// The input is a person's name.
    text_name = c.SDL_TEXTINPUT_TYPE_TEXT_NAME,
    /// The input is an e-mail address.
    text_email = c.SDL_TEXTINPUT_TYPE_TEXT_EMAIL,
    /// The input is a username.
    text_username = c.SDL_TEXTINPUT_TYPE_TEXT_USERNAME,
    /// he input is a secure password that is hidden.
    text_password_hidden = c.SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_HIDDEN,
    /// The input is a secure password that is visible.
    text_password_visible = c.SDL_TEXTINPUT_TYPE_TEXT_PASSWORD_VISIBLE,
    /// The input is a number.
    number = c.SDL_TEXTINPUT_TYPE_NUMBER,
    /// The input is a secure PIN that is hidden.
    number_password_hidden = c.SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_HIDDEN,
    /// The input is a secure PIN that is visible.
    number_password_visible = c.SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_VISIBLE,
};

/// This is a unique ID for a keyboard for the time it is connected to the system, and is never reused for the lifetime of the application.
///
/// ## Remarks
/// If the keyboard is disconnected and reconnected, it will get a new ID.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const Id = packed struct {
    value: c.SDL_KeyboardID,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_KeyboardID) == @sizeOf(Id));
    }

    /// Get the name of a keyboard.
    ///
    /// ## Function Parameters
    /// * `self`: The keyboard instance ID.
    ///
    /// ## Return Value
    /// Returns the name of the selected keyboard.
    ///
    /// ## Remarks
    /// This function returns `null` if the keyboard doesn't have a name.
    ///
    /// ## Thread Safety
    /// This function should only be called on the main thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getName(
        self: Id,
    ) !?[:0]const u8 {
        const ret = try errors.wrapCallCString(c.SDL_GetKeyboardNameForID(
            self.value,
        ));
        if (std.mem.eql(u8, ret, ""))
            return null;
        return ret;
    }
};

/// Dismiss the composition window/IME without disabling the subsystem.
///
/// ## Function Parameters
/// * `window`: The window to affect.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn clearComposition(
    window: video.Window,
) !void {
    const ret = c.SDL_ClearComposition(
        window.value,
    );
    return errors.wrapCallBool(ret);
}

/// Query the window which currently has keyboard focus.
///
/// ## Return Value
/// Returns the window with keyboard focus.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getFocus() ?video.Window {
    const ret = c.SDL_GetKeyboardFocus();
    if (ret) |val|
        return video.Window{ .value = val };
    return null;
}

/// Get a list of currently connected keyboards.
///
/// ## Return Value
/// Returns all the keyboards.
/// This should be freed with `free()`.
///
/// ## Remarks
/// Note that this will include any device or virtual driver that includes keyboard functionality, including some mice, KVM switches, motherboard power buttons, etc.
/// You should wait for input from a device before you consider it actively in use.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getKeyboards() ![]Id {
    var count: c_int = undefined;
    const ret: [*]Id = @ptrCast(try errors.wrapCallCPtr(c.SDL_KeyboardID, c.SDL_GetKeyboards(
        &count,
    )));
    return ret[0..@intCast(count)];
}

/// Get a key code from a human-readable name.
///
/// ## Function Parameters
/// * `name`: The human-readable key name.
///
/// ## Return Value
/// Returns key code.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getKeyFromName(
    name: [:0]const u8,
) !?keycode.Keycode {
    const ret = c.SDL_GetKeyFromName(
        name,
    );
    return keycode.Keycode.fromSdl(try errors.wrapCall(c.SDL_Keycode, ret, c.SDLK_UNKNOWN));
}

/// Get the key code corresponding to the given scancode according to the current keyboard layout.
///
/// ## Function Parameters
/// * `code`: The desired scancode to query.
/// * `modifier`: The modifier state to use when translating the scancode to a keycode.
/// * `used_in_key_events`: True if the keycode will be used in key events.
///
/// ## Return Value
/// Returns the keycode that corresponds to the given scancode.
///
/// ## Remarks
/// If you want to get the keycode as it would be delivered in key events, including options specified in `hints.keycode_options`,
/// then you should pass `used_in_key_events` as true.
/// Otherwise this function simply translates the scancode based on the given modifier state.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getKeyFromScancode(
    code: scancode.Scancode,
    modifier: keycode.KeyModifier,
    used_in_key_events: bool,
) ?keycode.Keycode {
    const ret = c.SDL_GetKeyFromScancode(
        code.toSdl(),
        modifier.toSdl(),
        used_in_key_events,
    );
    if (ret == c.SDLK_UNKNOWN)
        return null;
    return keycode.Keycode.fromSdl(ret);
}

/// Get a human-readable name for a key.
///
/// ## Function Parameters
/// * `key`: The desired keycode to query.
///
/// ## Return Value
/// Returns a UTF-8 encoded string of the key name.
///
/// ## Remarks
/// If the key doesn't have a name, this function returns `null`.
///
/// Letters will be presented in their uppercase form, if applicable.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getKeyName(
    key: keycode.Keycode,
) ?[:0]const u8 {
    const ret = c.SDL_GetKeyName(
        key.toSdl(),
    );
    const converted_ret = std.mem.span(ret);
    if (std.mem.eql(u8, converted_ret, ""))
        return null;
    return converted_ret;
}

/// Get the current key modifier state for the keyboard.
///
/// ## Return Value
/// Returns modifier keys for the keyboard.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getModState() keycode.KeyModifier {
    const ret = c.SDL_GetModState();
    return keycode.KeyModifier.fromSdl(ret);
}

/// Get the scancode corresponding to the given key code according to the current keyboard layout.
///
/// ## Function Parameters
/// * `key`: The desired keycode to query.
///
/// ## Return Value
/// Get the scancode corresponding to the given key code according to the current keyboard layout.
/// The key modifier is the first one that matches.
///
/// ## Remarks
/// Note that there may be multiple scancode+modifier states that can generate this keycode, this will just return the first one found.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getScancodeFromKey(
    key: keycode.Keycode,
) ?struct { ?scancode.Scancode, keycode.KeyModifier } {
    var key_mod: c.SDL_Keymod = undefined;
    const ret = c.SDL_GetScancodeFromKey(
        key.toSdl(),
        &key_mod,
    );
    if (ret == c.SDL_SCANCODE_UNKNOWN)
        return null;
    return .{ scancode.Scancode.fromSdl(@intCast(ret)), keycode.KeyModifier.fromSdl(key_mod) };
}

/// Get a scancode from a human-readable name.
///
/// ## Function Parameters
/// * `name`: The human-readable scancode name.
///
/// ## Return Value
/// Returns the scancode.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getScancodeFromName(
    name: [:0]const u8,
) !scancode.Scancode {
    const ret = c.SDL_GetScancodeFromName(
        name,
    );
    return scancode.Scancode.fromSdl(@intCast(try errors.wrapCall(c.SDL_Scancode, ret, c.SDL_SCANCODE_UNKNOWN))).?;
}

/// Get a human-readable name for a scancode.
///
/// ## Function Parameters
/// * `code`: The scancode to query.
///
/// ## Return Value
/// The name of the scancode.
///
/// ## Remarks
/// Warning: The returned name is by design not stable across platforms,
/// e.g. the name for `Scancode.left_gui` is "Left GUI" under Linux but "Left Windows" under Microsoft Windows,
/// and some scancodes like `Scancode.non_us_blackslash` don't have any name at all.
/// There are even scancodes that share names, e.g. `Scancode.return_key` and `Scancode.return2` (both called "Return").
/// This function is therefore unsuitable for creating a stable cross-platform two-way mapping between strings and scancodes.
///
/// ## Thread Safety
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getScancodeName(
    code: scancode.Scancode,
) ?[:0]const u8 {
    const ret = c.SDL_GetScancodeName(
        code.toSdl(),
    );
    const converted_ret = std.mem.span(ret);
    if (std.mem.eql(u8, converted_ret, ""))
        return null;
    return converted_ret;
}

/// Get a snapshot of the current state of the keyboard.
///
/// ## Return Value
/// Returns a slice of key states.
///
/// ## Remarks
/// The pointer returned is a pointer to an internal SDL array.
/// It will be valid for the whole lifetime of the application and should not be freed by the caller.
///
/// A array element with a value of true means that the key is pressed and a value of false means that it is not.
/// Indexes into this array are obtained by using `Scancode` values.
///
/// Use `events.pump()` to update the state array.
///
/// This function gives you the current state after all events have been processed, so if a key or button has been pressed and released before you process events,
/// then the pressed state will never show up in the `keyboard.getState()` calls.
///
/// Note: This function doesn't take into account whether shift has been pressed or not.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getState() []const bool {
    var num_keys: c_int = undefined;
    const ret = c.SDL_GetKeyboardState(
        &num_keys,
    );
    return ret[0..@intCast(num_keys)];
}

/// Get the area used to type Unicode text input.
///
/// ## Function Parameters
/// * `window`: The window for which to query the text input area.
///
/// ## Return Value
/// Returns the text input area rectangle along with the cursor offset relative to `input_area.x`.
///
/// ## Remarks
/// This returns the values previously set by `keyboard.setTextInputArea()`.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getTextInputArea(
    window: video.Window,
) !struct { rect.IRect, i32 } {
    var input_area: c.SDL_Rect = undefined;
    var cursor_offset: c_int = undefined;
    const ret = c.SDL_GetTextInputArea(
        window.value,
        &input_area,
        &cursor_offset,
    );
    try errors.wrapCallBool(ret);
    return .{ rect.IRect.fromSdl(input_area), @intCast(cursor_offset) };
}

/// Return whether a keyboard is currently connected.
///
/// ## Return Value
/// Returns true if a keyboard is connected, false otherwise.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasKeyboard() bool {
    return c.SDL_HasKeyboard();
}

/// Check whether the platform has screen keyboard support.
///
/// ## Return Value
/// Returns true if the platform has some screen keyboard support or false if not.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasScreenSupport() bool {
    return c.SDL_HasScreenKeyboardSupport();
}

/// Clear the state of the keyboard.
///
/// ## Remarks
/// This function will generate key up events for all pressed keys.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn reset() void {
    c.SDL_ResetKeyboard();
}

/// Set the current key modifier state for the keyboard.
///
/// ## Function Parameters
/// * `modifiers`: The desired modifiers for the keyboard.
///
/// ## Remarks
/// The inverse of `keyboard.getModState()`, `keyboard.setModState()` allows you to impose modifier key states on your application.
/// Simply pass your desired modifier states into modstate.
///
/// This does not change the keyboard state, only the key modifier flags that SDL reports.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setModState(
    modifiers: keycode.KeyModifier,
) void {
    c.SDL_SetModState(
        modifiers.toSdl(),
    );
}

/// Set a human-readable name for a scancode.
///
/// ## Function Parameters
/// * `code`: The desired scancode.
/// * `name`: The name to use for the scancode, encoded as UTF-8. The string is not copied, so the pointer given to this function must stay valid while SDL is being used.
///
/// ## Return Value
/// This function is not thread safe.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setScancodeName(
    code: scancode.Scancode,
    name: [:0]const u8,
) !void {
    const ret = c.SDL_SetScancodeName(
        code.toSdl(),
        name,
    );
    return errors.wrapCallBool(ret);
}

/// Set the area used to type Unicode text input.
///
/// ## Function Parameters
/// * `window`: The window for which to set the text input area.
/// * `input_area`: The rect representing the text input area in window coordinates, or `null` to clear it.
/// * `cursor`: The offset of the current cursor location relative to `input_area.x`, in window coordinates.
///
/// ## Remarks
/// Native input methods may place a window with word suggestions near the cursor, without covering the text being entered.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setTextInputArea(
    window: video.Window,
    input_area: ?rect.IRect,
    cursor: i32,
) !void {
    const input_area_sdl: ?c.SDL_Rect = if (input_area == null) null else input_area.?.toSdl();
    const ret = c.SDL_SetTextInputArea(
        window.value,
        if (input_area_sdl == null) null else &(input_area_sdl.?),
        @intCast(cursor),
    );
    return errors.wrapCallBool(ret);
}

/// Check whether the screen keyboard is shown for given window.
///
/// ## Function Parameters
/// * `window`: The window for which screen keyboard should be queried.
///
/// ## Return Value
/// Returns true if screen keyboard is shown or false if not.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn shownOnScreen(
    window: video.Window,
) bool {
    const ret = c.SDL_ScreenKeyboardShown(
        window.value,
    );
    return ret;
}

/// Start accepting Unicode text input events in a window.
///
/// ## Function Parameters
/// * `window`: The window to enable text input.
///
/// ## Remarks
/// This function will enable text input (`events.Type.text_input` and `events.Type.text_input` events) in the specified window.
/// Please use this function paired with `keyboard.stopTextInput()`.
///
/// Text input events are not received by default.
///
/// On some platforms using this function shows the screen keyboard and/or activates an IME, which can prevent some key press events from being passed through.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn startTextInput(
    window: video.Window,
) !void {
    const ret = c.SDL_StartTextInput(
        window.value,
    );
    return errors.wrapCallBool(ret);
}

/// Start accepting Unicode text input events in a window, with properties describing the input.
///
/// ## Function Parameters
/// * `window`: The window to enable text input.
/// * `props`: The properties to use.
///
/// ## Remarks
/// This function will enable text input (`events.Type.text_input` and `events.Type.text_editing` events) in the specified window.
/// Please use this function paired with `keyboard.stopTextInput()`.
///
/// Text input events are not received by default.
///
/// On some platforms using this function shows the screen keyboard and/or activates an IME, which can prevent some key press events from being passed through.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn startTextInputWithProperties(
    window: video.Window,
    props: TextInputProperties,
) !void {
    const input_properties = try props.toSdl();
    defer input_properties.deinit();
    const ret = c.SDL_StartTextInputWithProperties(
        window.value,
        input_properties.value,
    );
    return errors.wrapCallBool(ret);
}

/// Stop receiving any text input events in a window.
///
/// ## Function Parameters
/// * `window`: The window to disable text input.
///
/// ## Remarks
/// If `keyboard.startTextInput()` showed the screen keyboard, this function will hide it.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn stopTextInput(
    window: video.Window,
) !void {
    const ret = c.SDL_StopTextInput(
        window.value,
    );
    return errors.wrapCallBool(ret);
}

/// Check whether or not Unicode text input events are enabled for a window.
///
/// ## Function Parameters
/// * `window`: The window to check.
///
/// ## Return Value
/// Returns true if text input events are enabled else false.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn textInputActive(
    window: video.Window,
) bool {
    const ret = c.SDL_TextInputActive(
        window.value,
    );
    return ret;
}

// Test keyboard functions.
test "Keyboard" {
    errors.refAllDeclsRecursive(@This());
}
