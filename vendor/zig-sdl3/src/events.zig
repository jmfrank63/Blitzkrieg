const audio = @import("audio.zig");
const c = @import("c");
const camera = @import("camera.zig");
const errors = @import("errors.zig");
const gamepad = @import("gamepad.zig");
const joystick = @import("joystick.zig");
const keyboard = @import("keyboard.zig");
const keycode = @import("keycode.zig");
const mouse = @import("mouse.zig");
const pen = @import("pen.zig");
const power = @import("power.zig");
const scancode = @import("scancode.zig");
const sdl3 = @import("sdl3.zig");
const sensor = @import("sensor.zig");
const std = @import("std");
const touch = @import("touch.zig");
const video = @import("video.zig");

/// The type of action to request from `events.peep()`.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Action = enum(c.SDL_EventAction) {
    /// Add events to the back of the queue.
    add,
    /// Check but don't remove events from the queue front.
    peek,
    /// Retrieve/remove events from the front of the queue.
    get,
};

/// A function pointer used for callbacks that watch the event queue.
///
/// ## Function Parameters
/// * `user_data`: What was passed as `user_data` to `events.setFilter()` or `events.addWatch()`.
/// * `event`: The event that triggered the callback.
///
/// ## Return Value
/// Returns true to permit event to be added to the queue, and false to disallow it.
/// When used with `events.addWatch()`, the return value is ignored.
///
/// ## Thread Safety
/// SDL may call this callback at any time from any thread; the application is responsible for locking resources the callback touches that need to be protected.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn Filter(comptime UserData: type) type {
    return *const fn (
        user_data: ?*UserData,
        event: *Event,
    ) bool;
}

/// A function pointer used for callbacks that watch the event queue.
///
/// ## Function Parameters
/// * `user_data`: What was passed as `user_data` to `events.setFilter()` or `events.addWatch()`.
/// * `event`: The event that triggered the callback.
///
/// ## Return Value
/// Returns true to permit event to be added to the queue, and false to disallow it.
/// When used with `events.addWatch()`, the return value is ignored.
///
/// ## Thread Safety
/// SDL may call this callback at any time from any thread; the application is responsible for locking resources the callback touches that need to be protected.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const FilterC = *const fn (user_data: ?*anyopaque, event: [*c]c.SDL_Event) callconv(.c) bool;

/// For clearing out a group of events.
///
/// ## Version
/// This enum is provided by zig-sdl3.
pub const Group = enum {
    /// Clear all events.
    all,
    /// Application based events.
    application,
    /// Display events.
    display,
    /// Window events.
    window,
    /// Keyboard events.
    keyboard,
    /// Mouse events.
    mouse,
    /// Joystick events.
    joystick,
    /// Gamepad events.
    gamepad,
    /// Touch events.
    touch,
    /// Pinch events.
    pinch,
    /// Clipboard events.
    clipboard,
    /// Drag and drop events.
    drag_and_drop,
    /// Audio hotplug events.
    audio,
    /// Sensor events.
    sensor,
    /// Pressure-sensitive pen events.
    pen,
    /// Camera hotplug events.
    camera,
    /// Render events.
    render,
    /// Reserved events for private platforms.
    reserved,
    /// Internal events.
    internal,
    /// User events.
    user,

    /// Iterate over all event types in the group.
    ///
    /// ## Version
    /// Provided by zig-sdl3.
    pub const Iterator = struct {
        curr: c.SDL_EventType,
        max: c.SDL_EventType,

        /// Get the next event type in the iterator.
        ///
        /// ## Function Parameters
        /// * `self`: The group iterator.
        ///
        /// ## Return Value
        /// Returns the next event type in the iterator, or `null` if none left.
        ///
        /// ## Thread Safety
        /// This function is not thread safe.
        ///
        /// ## Version
        /// Provided by zig-sdl3.
        pub fn next(
            self: *Iterator,
        ) ?c.SDL_EventType {
            if (self.curr <= self.max) {
                const ret = self.curr;
                self.curr += 1;
                return ret;
            }
            return null;
        }
    };

    /// Check if an event type is in a group.
    ///
    /// ## Function Parameters
    /// * `self`: Group to check the event is in.
    /// * `event_type`: Type of the event to see if it is in a specified group.
    ///
    /// ## Return Value
    /// Returns if the `event_type` is in the `self` group.
    ///
    /// ## Thread Safety
    /// This function may be called from any thread.
    ///
    /// ## Version
    /// Provided by zig-sdl3.
    pub fn eventIn(
        self: Group,
        event_type: Type,
    ) bool {
        const raw: c.SDL_EventType = @intFromEnum(event_type);
        const min, const max = self.minMax();
        return raw >= min and raw <= max;
    }

    /// Create an iterator for every type in the group.
    ///
    /// ## Function Parameters
    /// * `self`: Group to iterate over.
    ///
    /// ## Return Value
    /// Returns an iterator that can iterate all over SDL event types.
    ///
    /// ## Thread Safety
    /// This function is thread safe.
    ///
    /// ## Version
    /// Provided by zig-sdl3.
    pub fn iterator(
        self: Group,
    ) Iterator {
        const min, const max = self.minMax();
        return Iterator{
            .curr = min,
            .max = max,
        };
    }

    /// Get the minimum and maximum `c.SDL_EventType` for the provided group.
    ///
    /// ## Function Parameters
    /// * `self`: Group to get the min and max types for.
    ///
    /// ## Return Value
    /// Returns the minimum and maximum SDL event types for its raw enum.
    ///
    /// ## Thread Safety
    /// This function may be called from any thread.
    ///
    /// ## Version
    /// Provided by zig-sdl3.
    pub fn minMax(
        self: Group,
    ) struct { c.SDL_EventType, c.SDL_EventType } {
        return switch (self) {
            .all => .{ 0, std.math.maxInt(c.SDL_EventType) },
            .application => .{ c.SDL_EVENT_QUIT, c.SDL_EVENT_SYSTEM_THEME_CHANGED },
            .display => .{ c.SDL_EVENT_DISPLAY_FIRST, c.SDL_EVENT_DISPLAY_LAST },
            .window => .{ c.SDL_EVENT_WINDOW_FIRST, c.SDL_EVENT_WINDOW_LAST },
            .keyboard => .{ c.SDL_EVENT_KEY_DOWN, c.SDL_EVENT_SCREEN_KEYBOARD_HIDDEN },
            .mouse => .{ c.SDL_EVENT_MOUSE_MOTION, c.SDL_EVENT_MOUSE_REMOVED },
            .joystick => .{ c.SDL_EVENT_JOYSTICK_AXIS_MOTION, c.SDL_EVENT_JOYSTICK_UPDATE_COMPLETE },
            .gamepad => .{ c.SDL_EVENT_GAMEPAD_AXIS_MOTION, c.SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED },
            .touch => .{ c.SDL_EVENT_FINGER_DOWN, c.SDL_EVENT_FINGER_CANCELED },
            .pinch => .{ c.SDL_EVENT_PINCH_BEGIN, c.SDL_EVENT_PINCH_END },
            .clipboard => .{ c.SDL_EVENT_CLIPBOARD_UPDATE, c.SDL_EVENT_CLIPBOARD_UPDATE },
            .drag_and_drop => .{ c.SDL_EVENT_DROP_FILE, c.SDL_EVENT_DROP_POSITION },
            .audio => .{ c.SDL_EVENT_AUDIO_DEVICE_ADDED, c.SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED },
            .sensor => .{ c.SDL_EVENT_SENSOR_UPDATE, c.SDL_EVENT_SENSOR_UPDATE },
            .pen => .{ c.SDL_EVENT_PEN_PROXIMITY_IN, c.SDL_EVENT_PEN_AXIS },
            .camera => .{ c.SDL_EVENT_CAMERA_DEVICE_ADDED, c.SDL_EVENT_CAMERA_DEVICE_DENIED },
            .render => .{ c.SDL_EVENT_RENDER_TARGETS_RESET, c.SDL_EVENT_RENDER_DEVICE_LOST },
            .reserved => .{ c.SDL_EVENT_PRIVATE0, c.SDL_EVENT_PRIVATE3 },
            .internal => .{ c.SDL_EVENT_POLL_SENTINEL, c.SDL_EVENT_POLL_SENTINEL },
            .user => .{ c.SDL_EVENT_USER, c.SDL_EVENT_LAST },
        };
    }
};

/// The types of events that can be delivered.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Type = enum(c.SDL_EventType) {
    /// User-requested quit.
    quit = c.SDL_EVENT_QUIT,
    /// The application is being terminated by the OS.
    /// This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationWillTerminate()`.
    /// Called on Android in `onDestroy()`.
    terminating = c.SDL_EVENT_TERMINATING,
    /// The application is low on memory, free memory if possible.
    /// This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationDidReceiveMemoryWarning()`.
    /// Called on Android in `onTrimMemory().
    low_memory = c.SDL_EVENT_LOW_MEMORY,
    /// The application is about to enter the background.
    /// This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationWillResignActive()`.
    /// Called on Android in `onPause()`.
    will_enter_background = c.SDL_EVENT_WILL_ENTER_BACKGROUND,
    /// The application did enter the background and may not get CPU for some time. This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationDidEnterBackground()`.
    /// Called on Android in `onPause()`.
    did_enter_background = c.SDL_EVENT_DID_ENTER_BACKGROUND,
    /// The application is about to enter the foreground.
    /// This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationWillEnterForeground()`.
    /// Called on Android in `onResume()`.
    will_enter_foreground = c.SDL_EVENT_WILL_ENTER_FOREGROUND,
    /// The application is now interactive. This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationDidBecomeActive()`.
    /// Called on Android in `onResume()`.
    did_enter_foreground = c.SDL_EVENT_DID_ENTER_FOREGROUND,
    /// The user's locale preferences have changed.
    locale_changed = c.SDL_EVENT_LOCALE_CHANGED,
    /// The system theme changed.
    system_theme_changed = c.SDL_EVENT_SYSTEM_THEME_CHANGED,
    /// Display orientation has changed.
    display_orientation = c.SDL_EVENT_DISPLAY_ORIENTATION,
    /// Display has been added to the system.
    display_added = c.SDL_EVENT_DISPLAY_ADDED,
    /// Display has been removed from the system.
    display_removed = c.SDL_EVENT_DISPLAY_REMOVED,
    /// Display has changed position.
    display_moved = c.SDL_EVENT_DISPLAY_MOVED,
    /// Display has changed desktop mode.
    display_desktop_mode_changed = c.SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED,
    /// Display has changed current mode.
    display_current_mode_changed = c.SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED,
    /// Display has changed content scale.
    display_content_scale_changed = c.SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED,
    /// Display has changed usable bounds.
    display_usable_bounds_changed = c.SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED,
    /// Window has been shown.
    window_shown = c.SDL_EVENT_WINDOW_SHOWN,
    /// Window has been hidden.
    window_hidden = c.SDL_EVENT_WINDOW_HIDDEN,
    /// Window has been exposed and should be redrawn, and can be redrawn directly from event watchers for this event.
    window_exposed = c.SDL_EVENT_WINDOW_EXPOSED,
    /// Window has been moved.
    window_moved = c.SDL_EVENT_WINDOW_MOVED,
    /// Window has been resized.
    window_resized = c.SDL_EVENT_WINDOW_RESIZED,
    /// The pixel size of the window has changed.
    window_pixel_size_changed = c.SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED,
    /// The pixel size of a Metal view associated with the window has changed.
    window_metal_view_resized = c.SDL_EVENT_WINDOW_METAL_VIEW_RESIZED,
    /// Window has been minimized.
    window_minimized = c.SDL_EVENT_WINDOW_MINIMIZED,
    /// Window has been maximized.
    window_maximized = c.SDL_EVENT_WINDOW_MAXIMIZED,
    /// Window has been restored to normal size and position.
    window_restored = c.SDL_EVENT_WINDOW_RESTORED,
    /// Window has gained mouse focus.
    window_mouse_enter = c.SDL_EVENT_WINDOW_MOUSE_ENTER,
    /// Window has lost mouse focus.
    window_mouse_leave = c.SDL_EVENT_WINDOW_MOUSE_LEAVE,
    /// Window has gained keyboard focus.
    window_focus_gained = c.SDL_EVENT_WINDOW_FOCUS_GAINED,
    /// Window has lost keyboard focus.
    window_focus_lost = c.SDL_EVENT_WINDOW_FOCUS_LOST,
    /// The window manager requests that the window be closed.
    window_close_requested = c.SDL_EVENT_WINDOW_CLOSE_REQUESTED,
    /// Window had a hit test that wasn't normal.
    window_hit_test = c.SDL_EVENT_WINDOW_HIT_TEST,
    /// The ICC profile of the window's display has changed.
    window_icc_profile_changed = c.SDL_EVENT_WINDOW_ICCPROF_CHANGED,
    /// Window has been moved to a display.
    window_display_changed = c.SDL_EVENT_WINDOW_DISPLAY_CHANGED,
    /// Window display scale has been changed.
    window_display_scale_changed = c.SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED,
    /// The window safe area has been changed.
    window_safe_area_changed = c.SDL_EVENT_WINDOW_SAFE_AREA_CHANGED,
    /// The window has been occluded.
    window_occluded = c.SDL_EVENT_WINDOW_OCCLUDED,
    /// The window has entered fullscreen mode.
    window_enter_fullscreen = c.SDL_EVENT_WINDOW_ENTER_FULLSCREEN,
    /// The window has left fullscreen mode.
    window_leave_fullscreen = c.SDL_EVENT_WINDOW_LEAVE_FULLSCREEN,
    /// The window with the associated ID is being or has been destroyed.
    /// If this message is being handled in an event watcher, the window handle is still valid and can still be used to retrieve any properties associated with the window.
    /// Otherwise, the handle has already been destroyed and all resources associated with it are invalid.
    window_destroyed = c.SDL_EVENT_WINDOW_DESTROYED,
    /// Window HDR properties have changed.
    window_hdr_state_changed = c.SDL_EVENT_WINDOW_HDR_STATE_CHANGED,
    /// Key pressed.
    key_down = c.SDL_EVENT_KEY_DOWN,
    /// Key released.
    key_up = c.SDL_EVENT_KEY_UP,
    /// Keyboard text editing (composition).
    text_editing = c.SDL_EVENT_TEXT_EDITING,
    /// Keyboard text input.
    text_input = c.SDL_EVENT_TEXT_INPUT,
    /// Keymap changed due to a system event such as an input language or keyboard layout change.
    keymap_changed = c.SDL_EVENT_KEYMAP_CHANGED,
    /// A new keyboard has been inserted into the system.
    keyboard_added = c.SDL_EVENT_KEYBOARD_ADDED,
    /// A keyboard has been removed.
    keyboard_removed = c.SDL_EVENT_KEYBOARD_REMOVED,
    /// Keyboard text editing candidates.
    text_editing_candidates = c.SDL_EVENT_TEXT_EDITING_CANDIDATES,
    /// The on-screen keyboard has been shown.
    screen_keyboard_shown = c.SDL_EVENT_SCREEN_KEYBOARD_SHOWN,
    /// The on-screen keyboard has been hidden.
    screen_keyboard_hidden = c.SDL_EVENT_SCREEN_KEYBOARD_HIDDEN,
    /// Mouse moved.
    mouse_motion = c.SDL_EVENT_MOUSE_MOTION,
    /// Mouse button pressed.
    mouse_button_down = c.SDL_EVENT_MOUSE_BUTTON_DOWN,
    /// Mouse button released.
    mouse_button_up = c.SDL_EVENT_MOUSE_BUTTON_UP,
    /// Mouse wheel motion.
    mouse_wheel = c.SDL_EVENT_MOUSE_WHEEL,
    /// A new mouse has been inserted into the system.
    mouse_added = c.SDL_EVENT_MOUSE_ADDED,
    /// A mouse has been removed.
    mouse_removed = c.SDL_EVENT_MOUSE_REMOVED,
    /// Joystick axis motion.
    joystick_axis_motion = c.SDL_EVENT_JOYSTICK_AXIS_MOTION,
    /// Joystick trackball motion.
    joystick_ball_motion = c.SDL_EVENT_JOYSTICK_BALL_MOTION,
    /// Joystick hat position change.
    joystick_hat_motion = c.SDL_EVENT_JOYSTICK_HAT_MOTION,
    /// Joystick button pressed.
    joystick_button_down = c.SDL_EVENT_JOYSTICK_BUTTON_DOWN,
    /// Joystick button released.
    joystick_button_up = c.SDL_EVENT_JOYSTICK_BUTTON_UP,
    /// A new joystick has been inserted into the system.
    joystick_added = c.SDL_EVENT_JOYSTICK_ADDED,
    /// An opened joystick has been removed.
    joystick_removed = c.SDL_EVENT_JOYSTICK_REMOVED,
    /// Joystick battery level change.
    joystick_battery_updated = c.SDL_EVENT_JOYSTICK_BATTERY_UPDATED,
    /// Joystick update is complete.
    joystick_update_complete = c.SDL_EVENT_JOYSTICK_UPDATE_COMPLETE,
    /// Gamepad axis motion.
    gamepad_axis_motion = c.SDL_EVENT_GAMEPAD_AXIS_MOTION,
    /// Gamepad button pressed.
    gamepad_button_down = c.SDL_EVENT_GAMEPAD_BUTTON_DOWN,
    /// Gamepad button released.
    gamepad_button_up = c.SDL_EVENT_GAMEPAD_BUTTON_UP,
    /// A new gamepad has been inserted into the system.
    gamepad_added = c.SDL_EVENT_GAMEPAD_ADDED,
    /// A gamepad has been removed.
    gamepad_removed = c.SDL_EVENT_GAMEPAD_REMOVED,
    /// The gamepad mapping was updated.
    gamepad_remapped = c.SDL_EVENT_GAMEPAD_REMAPPED,
    /// Gamepad touchpad was touched.
    gamepad_touchpad_down = c.SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN,
    /// Gamepad touchpad finger was moved.
    gamepad_touchpad_motion = c.SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION,
    /// Gamepad touchpad finger was lifted.
    gamepad_touchpad_up = c.SDL_EVENT_GAMEPAD_TOUCHPAD_UP,
    /// Gamepad sensor was updated.
    gamepad_sensor_update = c.SDL_EVENT_GAMEPAD_SENSOR_UPDATE,
    /// Gamepad update is complete.
    gamepad_update_complete = c.SDL_EVENT_GAMEPAD_UPDATE_COMPLETE,
    /// Gamepad Steam handle has changed.
    gamepad_steam_handle_updated = c.SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED,
    finger_down = c.SDL_EVENT_FINGER_DOWN,
    finger_up = c.SDL_EVENT_FINGER_UP,
    finger_motion = c.SDL_EVENT_FINGER_MOTION,
    finger_canceled = c.SDL_EVENT_FINGER_CANCELED,
    /// Pinch gesture started.
    pinch_begin = c.SDL_EVENT_PINCH_BEGIN,
    /// Pinch gesture updated.
    pinch_update = c.SDL_EVENT_PINCH_UPDATE,
    /// Pinch gesture ended.
    pinch_end = c.SDL_EVENT_PINCH_END,
    /// The clipboard or primary selection changed.
    clipboard_update = c.SDL_EVENT_CLIPBOARD_UPDATE,
    /// The system requests a file open.
    drop_file = c.SDL_EVENT_DROP_FILE,
    /// Text/plain drag-and-drop event.
    drop_text = c.SDL_EVENT_DROP_TEXT,
    /// A new set of drops is beginning.
    drop_begin = c.SDL_EVENT_DROP_BEGIN,
    /// Current set of drops is now complete.
    drop_complete = c.SDL_EVENT_DROP_COMPLETE,
    /// Position while moving over the window.
    drop_position = c.SDL_EVENT_DROP_POSITION,
    /// A new audio device is available.
    audio_device_added = c.SDL_EVENT_AUDIO_DEVICE_ADDED,
    /// An audio device has been removed.
    audio_device_removed = c.SDL_EVENT_AUDIO_DEVICE_REMOVED,
    /// An audio device's format has been changed by the system.
    audio_device_format_changed = c.SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED,
    /// A sensor was updated.
    sensor_update = c.SDL_EVENT_SENSOR_UPDATE,
    /// Pressure-sensitive pen has become available.
    pen_proximity_in = c.SDL_EVENT_PEN_PROXIMITY_IN,
    /// Pressure-sensitive pen has become unavailable.
    pen_proximity_out = c.SDL_EVENT_PEN_PROXIMITY_OUT,
    /// Pressure-sensitive pen touched drawing surface.
    pen_down = c.SDL_EVENT_PEN_DOWN,
    /// Pressure-sensitive pen stopped touching drawing surface.
    pen_up = c.SDL_EVENT_PEN_UP,
    /// Pressure-sensitive pen button pressed.
    pen_button_down = c.SDL_EVENT_PEN_BUTTON_DOWN,
    /// Pressure-sensitive pen button released.
    pen_button_up = c.SDL_EVENT_PEN_BUTTON_UP,
    /// Pressure-sensitive pen is moving on the tablet.
    pen_motion = c.SDL_EVENT_PEN_MOTION,
    /// Pressure-sensitive pen angle/pressure/etc changed.
    pen_axis = c.SDL_EVENT_PEN_AXIS,
    /// A new camera device is available.
    camera_device_added = c.SDL_EVENT_CAMERA_DEVICE_ADDED,
    /// A camera device has been removed.
    camera_device_removed = c.SDL_EVENT_CAMERA_DEVICE_REMOVED,
    /// A camera device has been approved for use by the user.
    camera_device_approved = c.SDL_EVENT_CAMERA_DEVICE_APPROVED,
    /// A camera device has been denied for use by the user.
    camera_device_denied = c.SDL_EVENT_CAMERA_DEVICE_DENIED,
    /// The render targets have been reset and their contents need to be updated.
    render_targets_reset = c.SDL_EVENT_RENDER_TARGETS_RESET,
    /// The device has been reset and all textures need to be recreated.
    render_device_reset = c.SDL_EVENT_RENDER_DEVICE_RESET,
    /// The device has been lost and can't be recovered.
    render_device_lost = c.SDL_EVENT_RENDER_DEVICE_LOST,
    private0 = c.SDL_EVENT_PRIVATE0,
    private1 = c.SDL_EVENT_PRIVATE1,
    private2 = c.SDL_EVENT_PRIVATE2,
    private3 = c.SDL_EVENT_PRIVATE3,
    /// Signals the end of an event poll cycle.
    poll_sentinal = c.SDL_EVENT_POLL_SENTINEL,
    /// User events, should be allocated with `events.register()`.
    user = c.SDL_EVENT_USER,
    /// For padding out the union.
    padding = c.SDL_EVENT_ENUM_PADDING,
    /// An unknown event.
    unknown = c.SDL_EVENT_ENUM_PADDING - 1,
};

/// Audio device event structure.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const AudioDevice = struct {
    /// Common event information.
    common: Common,
    /// Device being added or removed or changing.
    device: audio.Device,
    /// False if a playback device, true if a recording device.
    recording: bool,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) AudioDevice {
        return .{
            .common = Common.fromSdl(val),
            .device = .{ .value = val.adevice.which },
            .recording = val.adevice.recording,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: AudioDevice, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.adevice.which = self.device.value;
        ret.adevice.recording = self.recording;
        return ret;
    }
};

/// Camera device event structure.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const CameraDevice = struct {
    /// Common event information.
    common: Common,
    /// The device being added or removed or changing.
    device: camera.Id,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) CameraDevice {
        return .{
            .common = Common.fromSdl(val),
            .device = .{ .value = val.cdevice.which },
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: CameraDevice, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.cdevice.which = self.device.value;
        return ret;
    }
};

/// An event triggered when the clipboard contents have changed (event.clipboard.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Clipboard = struct {
    /// Common event information.
    common: Common,
    /// Are we owning the clipboard (internal update)?
    owner: bool,
    /// Mime types.
    mime_types: [][*:0]const u8,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) Clipboard {
        return .{
            .common = Common.fromSdl(val),
            .owner = val.clipboard.owner,
            .mime_types = if (val.clipboard.mime_types) |mimes|
                @as([*][*:0]const u8, @ptrCast(mimes))[0..@intCast(val.clipboard.num_mime_types)]
            else
                &[_][*:0]const u8{},
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Clipboard, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.clipboard.owner = self.owner;
        ret.clipboard.num_mime_types = @intCast(self.mime_types.len);
        ret.clipboard.mime_types = @ptrCast(self.mime_types.ptr);
        return ret;
    }
};

/// Fields shared by every event.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Common = struct {
    /// In nanoseconds, populated using `timer.getNanosecondsSinceInit()`.
    timestamp: u64,

    /// Create a common event from an SDL one.
    pub fn fromSdl(event: c.SDL_Event) Common {
        return .{ .timestamp = event.common.timestamp };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Common, event_type: Type) c.SDL_Event {
        return .{
            .common = .{
                .type = @intFromEnum(event_type),
                .timestamp = self.timestamp,
            },
        };
    }
};

/// Display state change event data (event.display.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Display = struct {
    /// Common event information.
    common: Common,
    /// The associated display.
    display: video.Display,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) Display {
        return .{
            .common = Common.fromSdl(val),
            .display = .{ .value = val.display.displayID },
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Display, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.display.displayID = self.display.value;
        return ret;
    }
};

/// Display orientation state change event data (event.display.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const DisplayOrientation = struct {
    /// Common event information.
    common: Common,
    /// The associated display.
    display: video.Display,
    /// The new display orientation.
    orientation: video.Display.Orientation,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) DisplayOrientation {
        return .{
            .common = Common.fromSdl(val),
            .display = .{ .value = val.display.displayID },
            .orientation = @enumFromInt(val.display.data1),
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: DisplayOrientation, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.display.displayID = self.display.value;
        ret.display.data1 = @intCast(@intFromEnum(self.orientation));
        return ret;
    }
};

/// An event used to drop text or request a file open by the system (event.drop.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Drop = struct {
    /// Common event information.
    common: Common,
    /// The window that was dropped on, if any.
    window: ?video.WindowId,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,
    /// The source app that sent this drop event, or `null` if that isn't available.
    source: ?[:0]const u8,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) Drop {
        return .{
            .common = Common.fromSdl(val),
            .window = if (val.drop.windowID == 0) null else val.drop.windowID,
            .x = val.drop.x,
            .y = val.drop.y,
            .source = if (val.drop.source != null) std.mem.span(val.drop.source) else null,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Drop, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.drop.windowID = if (self.window) |val| val else 0;
        ret.drop.x = self.x;
        ret.drop.y = self.y;
        ret.drop.source = if (self.source) |val| val.ptr else null;
        return ret;
    }
};

/// An event used to begin drop event by the system (event.drop.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const DropBegin = struct {
    /// Common event information.
    common: Common,
    /// The window that was dropped on, if any.
    window: ?video.WindowId,
    /// The source app that sent this drop event, or `null` if that isn't available.
    source: ?[:0]const u8,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) DropBegin {
        return .{
            .common = Common.fromSdl(val),
            .window = if (val.drop.windowID == 0) null else val.drop.windowID,
            .source = if (val.drop.source != null) std.mem.span(val.drop.source) else null,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: DropBegin, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.drop.windowID = if (self.window) |val| val else 0;
        ret.drop.source = if (self.source) |val| val.ptr else null;
        return ret;
    }
};

/// An event used to drop a file open by the system (event.drop.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const DropFile = struct {
    /// Common event information.
    common: Common,
    /// The window that was dropped on, if any.
    window: ?video.WindowId,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,
    /// The source app that sent this drop event, or `null` if that isn't available.
    source: ?[:0]const u8,
    /// The file name dropped.
    file_name: [:0]const u8,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) DropFile {
        return .{
            .common = Common.fromSdl(val),
            .window = if (val.drop.windowID == 0) null else val.drop.windowID,
            .x = val.drop.x,
            .y = val.drop.y,
            .source = if (val.drop.source != null) std.mem.span(val.drop.source) else null,
            .file_name = std.mem.span(val.drop.data),
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: DropFile, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.drop.windowID = if (self.window) |val| val else 0;
        ret.drop.x = self.x;
        ret.drop.y = self.y;
        ret.drop.source = if (self.source) |val| val.ptr else null;
        ret.drop.data = self.file_name.ptr;
        return ret;
    }
};

/// An event used to drop text by the system (event.drop.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const DropText = struct {
    /// Common event information.
    common: Common,
    /// The window that was dropped on, if any.
    window: ?video.WindowId,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,
    /// The source app that sent this drop event, or `null` if that isn't available.
    source: ?[:0]const u8,
    /// The text dropped.
    text: [:0]const u8,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) DropText {
        return .{
            .common = Common.fromSdl(val),
            .window = if (val.drop.windowID == 0) null else val.drop.windowID,
            .x = val.drop.x,
            .y = val.drop.y,
            .source = if (val.drop.source != null) std.mem.span(val.drop.source) else null,
            .text = std.mem.span(val.drop.data),
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: DropText, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.drop.windowID = if (self.window) |val| val else 0;
        ret.drop.x = self.x;
        ret.drop.y = self.y;
        ret.drop.source = if (self.source) |val| val.ptr else null;
        ret.drop.data = self.text.ptr;
        return ret;
    }
};

/// Gamepad axis motion event structure (event.gaxis.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const GamepadAxis = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,
    /// The gamepad axis.
    axis: gamepad.Axis,
    /// The axis value.
    value: i16,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) GamepadAxis {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.gaxis.which },
            .axis = @enumFromInt(val.gaxis.axis),
            .value = val.gaxis.value,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: GamepadAxis, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.gaxis.which = self.id.value;
        ret.gaxis.axis = @intCast(@intFromEnum(self.axis));
        ret.gaxis.value = self.value;
        return ret;
    }
};

/// Gamepad button event structure (event.gbutton.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const GamepadButton = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,
    /// The gamepad button.
    button: gamepad.Button,
    /// True if the button is pressed.
    down: bool,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) GamepadButton {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.gbutton.which },
            .button = @enumFromInt(val.gbutton.button),
            .down = val.gbutton.down,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: GamepadButton, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.gbutton.which = self.id.value;
        ret.gbutton.button = @intCast(@intFromEnum(self.button));
        ret.gbutton.down = self.down;
        return ret;
    }
};

/// Gamepad device event structure (event.gdevice.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const GamepadDevice = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) GamepadDevice {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.gdevice.which },
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: GamepadDevice, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.gdevice.which = self.id.value;
        return ret;
    }
};

/// Gamepad sensor event structure (event.gsensor.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const GamepadSensor = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,
    /// The type of the sensor.
    sensor_type: sensor.Type,
    /// Up to 3 values from the sensor.
    data: [3]f32,
    /// The timestamp of the sensor reading in nanoseconds, not necessarily synchronized with the system clock.
    sensor_timestamp: u64,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) GamepadSensor {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.gsensor.which },
            .sensor_type = @enumFromInt(val.gsensor.type),
            .data = val.gsensor.data,
            .sensor_timestamp = val.gsensor.sensor_timestamp,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: GamepadSensor, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.gsensor.which = self.id.value;
        ret.gsensor.type = @bitCast(@intFromEnum(self.sensor_type));
        ret.gsensor.data = self.data;
        ret.gsensor.sensor_timestamp = self.sensor_timestamp;
        return ret;
    }
};

/// Gamepad touchpad event structure (event.gtouchpad.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const GamepadTouchpad = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,
    /// The index of the touchpad.
    touchpad: usize,
    /// The index of the finger on the touchpad.
    finger: usize,
    /// Normalized in the range `0` to `1` with `0` being the left.
    x: f32,
    /// Normalized in the range `0` to `1` with `0` being the top.
    y: f32,
    /// Normalized in the range `0` to `1`.
    pressure: f32,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) GamepadTouchpad {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.gtouchpad.which },
            .touchpad = @intCast(val.gtouchpad.touchpad),
            .finger = @intCast(val.gtouchpad.finger),
            .x = val.gtouchpad.x,
            .y = val.gtouchpad.y,
            .pressure = val.gtouchpad.pressure,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: GamepadTouchpad, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.gtouchpad.which = self.id.value;
        ret.gtouchpad.touchpad = @intCast(self.touchpad);
        ret.gtouchpad.finger = @intCast(self.finger);
        ret.gtouchpad.x = self.x;
        ret.gtouchpad.y = self.y;
        ret.gtouchpad.pressure = self.pressure;
        return ret;
    }
};

/// Joystick axis motion event structure (event.jaxis.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const JoystickAxis = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,
    /// The joystick axis index.
    index: usize,
    /// The axis value.
    value: i16,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) JoystickAxis {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.jaxis.which },
            .index = @intCast(val.jaxis.axis),
            .value = val.jaxis.value,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: JoystickAxis, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.jaxis.which = self.id.value;
        ret.jaxis.axis = @intCast(self.index);
        ret.jaxis.value = self.value;
        return ret;
    }
};

/// Joystick trackball motion event structure (event.jball.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const JoystickBall = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,
    /// The joystick axis index.
    index: usize,
    /// The relative motion in the X direction.
    x_rel: i16,
    /// The relative motion in the Y direction.
    y_rel: i16,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) JoystickBall {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.jball.which },
            .index = @intCast(val.jball.ball),
            .x_rel = val.jball.xrel,
            .y_rel = val.jball.yrel,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: JoystickBall, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.jball.which = self.id.value;
        ret.jball.ball = @intCast(self.index);
        ret.jball.xrel = self.x_rel;
        ret.jball.yrel = self.y_rel;
        return ret;
    }
};

/// Joystick battery level change event structure (event.jbattery.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const JoystickBattery = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,
    /// The joystick battery state.
    state: power.PowerState,
    /// The joystick battery percent charge remaining.
    percent: u7,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) JoystickBattery {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.jbattery.which },
            .state = @enumFromInt(val.jbattery.state),
            .percent = @intCast(val.jbattery.percent),
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: JoystickBattery, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.jbattery.which = self.id.value;
        ret.jbattery.state = @intFromEnum(self.state);
        ret.jbattery.percent = @intCast(self.percent);
        return ret;
    }
};

/// Joystick button event structure (event.jbutton.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const JoystickButton = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,
    /// The joystick button index.
    index: usize,
    /// True if the button is pressed.
    down: bool,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) JoystickButton {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.jbutton.which },
            .index = @intCast(val.jbutton.button),
            .down = val.jbutton.down,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: JoystickButton, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.jbutton.which = self.id.value;
        ret.jbutton.button = @intCast(self.index);
        ret.jbutton.down = self.down;
        return ret;
    }
};

/// Joystick device event structure (event.jdevice.*).
///
/// ## Remarks
/// SDL will send `events.Type.joystick_added` events for devices that are already plugged in during `init()`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const JoystickDevice = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) JoystickDevice {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.jdevice.which },
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: JoystickDevice, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.jdevice.which = self.id.value;
        return ret;
    }
};

/// Joystick hat position change event structure (event.jhat.*).
///
/// ## Remarks
/// SDL will send `events.Type.joystick_added` events for devices that are already plugged in during `init()`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const JoystickHat = struct {
    /// Common event information.
    common: Common,
    /// The joystick instance id.
    id: joystick.Id,
    /// The joystick hat index.
    index: usize,
    /// The hat position value.
    value: joystick.Hat,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) JoystickHat {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.jhat.which },
            .index = @intCast(val.jhat.hat),
            .value = @enumFromInt(val.jhat.value),
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: JoystickHat, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.jhat.which = self.id.value;
        ret.jhat.hat = @intCast(self.index);
        ret.jhat.value = @intFromEnum(self.value);
        return ret;
    }
};

/// Keyboard device event structure (event.kdevice.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const KeyboardDevice = struct {
    /// Common event information.
    common: Common,
    /// The keyboard instance id.
    id: keyboard.Id,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) KeyboardDevice {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.kdevice.which },
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: KeyboardDevice, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.kdevice.which = self.id.value;
        return ret;
    }
};

/// Keyboard button event structure (event.key.*).
///
/// ## Remarks
/// The key is the base SDL_Keycode generated by pressing the scancode using the current keyboard layout, applying any options specified in `hints.Type.keycode_options`.
/// You can get the keycode corresponding to the event scancode and modifiers directly from the keyboard layout, bypassing `hints.Type.keycode_options`, by calling `keyboard.getKeyFromScancode()`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Keyboard = struct {
    /// Common event information.
    common: Common,
    /// The window with keyboard focus, if any.
    window_id: ?video.WindowId = null,
    /// The keyboard instance ID, or `null` if unknown or virtual.
    id: ?keyboard.Id,
    /// SDL physical key code.
    scancode: ?scancode.Scancode,
    /// SDL virtual key code.
    key: ?keycode.Keycode,
    /// Current key modifiers.
    mod: keycode.KeyModifier,
    /// The platform dependent scancode for this event.
    raw: u16,
    /// True if the key is pressed.
    down: bool,
    /// True if this is a key repeat.
    repeat: bool,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) Keyboard {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.key.windowID == 0) null else val.key.windowID,
            .id = if (val.key.which == 0) null else .{ .value = val.key.which },
            .scancode = scancode.Scancode.fromSdl(val.key.scancode),
            .key = keycode.Keycode.fromSdl(val.key.key),
            .mod = keycode.KeyModifier.fromSdl(val.key.mod),
            .raw = val.key.raw,
            .down = val.key.down,
            .repeat = val.key.repeat,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Keyboard, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.key.windowID = self.window_id orelse 0;
        ret.key.which = if (self.id) |val| val.value else 0;
        ret.key.scancode = scancode.Scancode.toSdl(self.scancode);
        ret.key.key = keycode.Keycode.toSdl(self.key);
        ret.key.mod = self.mod.toSdl();
        ret.key.raw = self.raw;
        ret.key.down = self.down;
        ret.key.repeat = self.repeat;
        return ret;
    }
};

/// Mouse button event structure (event.button.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const MouseButton = struct {
    /// Common event information.
    common: Common,
    /// The window with mouse focus, if any.
    window_id: ?video.WindowId = null,
    /// The mouse instance ID in relative mode, `mouse.Id.touch` for touch events, or `null`.
    id: ?mouse.Id = null,
    /// The mouse button index.
    button: mouse.Button,
    /// If the button is pressed.
    down: bool,
    /// 1 for single-click, 2 for double-click, etc.
    clicks: u8,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) MouseButton {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.button.windowID == 0) null else val.button.windowID,
            .id = if (val.button.which == 0) null else .{ .value = val.button.which },
            .button = @enumFromInt(val.button.button),
            .down = val.button.down,
            .clicks = val.button.clicks,
            .x = val.button.x,
            .y = val.button.y,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: MouseButton, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.button.windowID = self.window_id orelse 0;
        ret.button.which = if (self.id) |val| val.value else 0;
        ret.button.button = @intFromEnum(self.button);
        ret.button.down = self.down;
        ret.button.clicks = self.clicks;
        ret.button.x = self.x;
        ret.button.y = self.y;
        return ret;
    }
};

/// Mouse device event structure (event.mdevice.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const MouseDevice = struct {
    /// Common event information.
    common: Common,
    /// The mouse instance id.
    id: mouse.Id,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) MouseDevice {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.mdevice.which },
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: MouseDevice, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.mdevice.which = self.id.value;
        return ret;
    }
};

/// Mouse motion event structure (event.motion.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const MouseMotion = struct {
    /// Common event information.
    common: Common,
    /// Associated window if any.
    window_id: ?video.WindowId = null,
    /// The mouse instance ID in relative mode, `mouse.Id.touch` for touch events, or `null`.
    id: ?mouse.Id = null,
    /// The current button state.
    state: mouse.ButtonFlags,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,
    /// The relative motion in the X direction.
    x_rel: f32,
    /// The relative motion in the Y direction.
    y_rel: f32,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) MouseMotion {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.motion.windowID == 0) null else val.motion.windowID,
            .id = if (val.motion.which == 0) null else .{ .value = val.motion.which },
            .state = mouse.ButtonFlags.fromSdl(val.motion.state),
            .x = val.motion.x,
            .y = val.motion.y,
            .x_rel = val.motion.xrel,
            .y_rel = val.motion.yrel,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: MouseMotion, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.motion.windowID = self.window_id orelse 0;
        ret.motion.which = if (self.id) |val| val.value else 0;
        ret.motion.state = self.state.toSdl();
        ret.motion.x = self.x;
        ret.motion.y = self.y;
        ret.motion.xrel = self.x_rel;
        ret.motion.yrel = self.y_rel;
        return ret;
    }
};

/// Mouse wheel event structure (event.wheel.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const MouseWheel = struct {
    /// Common event information.
    common: Common,
    /// Associated window if any.
    window_id: ?video.WindowId = null,
    /// The mouse instance ID in relative mode, `mouse.Id.touch?` for touch events, or `null`.
    id: ?mouse.Id = null,
    /// The amount scrolled horizontally, positive to the right and negative to the left.
    scroll_x: f32,
    /// The amount scrolled vertically, positive away from the user and negative toward the user.
    scroll_y: f32,
    /// When `mouse.WheelDirection.flipped`, the values in `x` and `y` will be opposite.
    /// Multiply by `-1` to change them back.
    direction: mouse.WheelDirection,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,
    // /// The amount scrolled horizontally, accumulated to whole scroll "ticks".
    // x_int: i32,
    // /// The amount scrolled vertically, accumulated to whole scroll "ticks".
    // y_int: i32,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) MouseWheel {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.wheel.windowID == 0) null else val.wheel.windowID,
            .id = if (val.wheel.which == 0) null else .{ .value = val.wheel.which },
            .scroll_x = val.wheel.x,
            .scroll_y = val.wheel.y,
            .direction = @enumFromInt(val.wheel.direction),
            .x = val.wheel.mouse_x,
            .y = val.wheel.mouse_y,
            // .x_int = val.wheel.integer_x,
            // .y_int = val.wheel.integer_y,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: MouseWheel, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.wheel.windowID = self.window_id orelse 0;
        ret.wheel.which = if (self.id) |val| val.value else 0;
        ret.wheel.x = self.scroll_x;
        ret.wheel.y = self.scroll_y;
        ret.wheel.direction = @intFromEnum(self.direction);
        ret.wheel.mouse_x = self.x;
        ret.wheel.mouse_y = self.y;
        return ret;
    }
};

/// Pressure-sensitive pen pressure / angle event structure (event.paxis.*).
///
/// ## Remarks
/// You might get some of these events even if the pen isn't touching the tablet.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const PenAxis = struct {
    /// Common event information.
    common: Common,
    /// The window with pen focus, if any.
    window_id: ?video.WindowId,
    /// The pen instance id.
    id: pen.Id,
    /// Complete pen input state at time of event.
    state: pen.InputFlags,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,
    /// Axis that has changed.
    axis: pen.Axis,
    /// New value of axis.
    value: f32,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) PenAxis {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.paxis.windowID == 0) null else val.paxis.windowID,
            .id = .{ .value = val.paxis.which },
            .state = pen.InputFlags.fromSdl(val.paxis.pen_state),
            .x = val.paxis.x,
            .y = val.paxis.y,
            .axis = @enumFromInt(val.paxis.axis),
            .value = val.paxis.value,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: PenAxis, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.paxis.windowID = self.window_id orelse 0;
        ret.paxis.which = self.id.value;
        ret.paxis.pen_state = self.state.toSdl();
        ret.paxis.x = self.x;
        ret.paxis.y = self.y;
        ret.paxis.axis = @intCast(@intFromEnum(self.axis));
        ret.paxis.value = self.value;
        return ret;
    }
};

/// Pressure-sensitive pen button event structure (event.pbutton.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const PenButton = struct {
    /// Common event information.
    common: Common,
    /// The window with mouse focus, if any.
    window_id: ?video.WindowId = null,
    /// The pen instance id.
    id: pen.Id,
    /// Complete pen input state at time of event.
    state: pen.InputFlags,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,
    /// The pen button index (first button is 1).
    button: usize,
    /// If the button is pressed.
    down: bool,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) PenButton {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.pbutton.windowID == 0) null else val.pbutton.windowID,
            .id = .{ .value = val.pbutton.which },
            .state = pen.InputFlags.fromSdl(val.pbutton.pen_state),
            .x = val.pbutton.x,
            .y = val.pbutton.y,
            .button = @intCast(val.pbutton.button),
            .down = val.pbutton.down,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: PenButton, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.pbutton.windowID = self.window_id orelse 0;
        ret.pbutton.which = self.id.value;
        ret.pbutton.pen_state = self.state.toSdl();
        ret.pbutton.down = self.down;
        ret.pbutton.x = self.x;
        ret.pbutton.y = self.y;
        ret.pbutton.button = @intCast(self.button);
        return ret;
    }
};

/// Pressure-sensitive pen motion event structure (event.pmotion.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const PenMotion = struct {
    /// Common event information.
    common: Common,
    /// The window with mouse focus, if any.
    window_id: ?video.WindowId = null,
    /// The pen instance id.
    id: pen.Id,
    /// Complete pen input state at time of event.
    state: pen.InputFlags,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) PenMotion {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.pmotion.windowID == 0) null else val.pmotion.windowID,
            .id = .{ .value = val.pmotion.which },
            .state = pen.InputFlags.fromSdl(val.pmotion.pen_state),
            .x = val.pmotion.x,
            .y = val.pmotion.y,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: PenMotion, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.pmotion.windowID = self.window_id orelse 0;
        ret.pmotion.which = self.id.value;
        ret.pmotion.pen_state = self.state.toSdl();
        ret.pmotion.x = self.x;
        ret.pmotion.y = self.y;
        return ret;
    }
};

/// Pressure-sensitive pen proximity event structure (event.pproximity.*).
///
/// ## Remarks
/// When a pen becomes visible to the system (it is close enough to a tablet, etc), SDL will send an `events.Type.proximity_in` event with the new pen's ID.
/// This ID is valid until the pen leaves proximity again (has been removed from the tablet's area, the tablet has been unplugged, etc).
/// If the same pen reenters proximity again, it will be given a new ID.
///
/// Note that "proximity" means "close enough for the tablet to know the tool is there".
/// The pen touching and lifting off from the tablet while not leaving the area are handled by `events.Type.pen_down` and `events.Type.pen_up`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const PenProximity = struct {
    /// Common event information.
    common: Common,
    /// The window with mouse focus, if any.
    window_id: ?video.WindowId = null,
    /// The pen instance id.
    id: pen.Id,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) PenProximity {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.pproximity.windowID == 0) null else val.pproximity.windowID,
            .id = .{ .value = val.pproximity.which },
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: PenProximity, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.pproximity.windowID = self.window_id orelse 0;
        ret.pproximity.which = self.id.value;
        return ret;
    }
};

/// Pressure-sensitive pen touched event structure (event.ptouch.*).
///
/// ## Remarks
/// These events come when a pen touches a surface (a tablet, etc), or lifts off from one.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const PenTouch = struct {
    /// Common event information.
    common: Common,
    /// The window with mouse focus, if any.
    window_id: ?video.WindowId = null,
    /// The pen instance id.
    id: pen.Id,
    /// Complete pen input state at time of event.
    state: pen.InputFlags,
    /// X coordinate, relative to window.
    x: f32,
    /// Y coordinate, relative to window.
    y: f32,
    /// True if eraser end is used (not all pens support this).
    eraser: bool,
    /// True if the pen is touching or false if the pen is lifted off.
    down: bool,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) PenTouch {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.ptouch.windowID == 0) null else val.ptouch.windowID,
            .id = .{ .value = val.ptouch.which },
            .state = pen.InputFlags.fromSdl(val.ptouch.pen_state),
            .x = val.ptouch.x,
            .y = val.ptouch.y,
            .eraser = val.ptouch.eraser,
            .down = val.ptouch.down,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: PenTouch, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.ptouch.windowID = self.window_id orelse 0;
        ret.ptouch.which = self.id.value;
        ret.ptouch.pen_state = self.state.toSdl();
        ret.ptouch.x = self.x;
        ret.ptouch.y = self.y;
        ret.ptouch.eraser = self.eraser;
        ret.ptouch.down = self.down;
        return ret;
    }
};

/// Pinch event structure (event.pinch.*).
///
/// ## Version
/// This struct is available since SDL 3.4.0.
pub const PinchFinger = struct {
    /// Common event information.
    common: Common,
    /// The scale change since the last pinch update event.
    /// Scale < 1 is "zoom out".
    /// Scale > 1 is "zoom in".
    scale: f32,
    /// The window underneath the finger, if any.
    window_id: ?video.WindowId = null,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) PinchFinger {
        return .{
            .common = Common.fromSdl(val),
            .scale = val.pinch.scale,
            .window_id = if (val.pinch.windowID == 0) null else val.pinch.windowID,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: PinchFinger, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.pinch.scale = self.scale;
        ret.pinch.windowID = self.window_id orelse 0;
        return ret;
    }
};

/// The "quit requested" event.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Quit = struct {
    /// Common event information.
    common: Common,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) Quit {
        return .{
            .common = Common.fromSdl(val),
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Quit, event_type: Type) c.SDL_Event {
        return self.common.toSdl(event_type);
    }
};

///Renderer event structure (event.render.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Render = struct {
    /// Common event information.
    common: Common,
    /// The window with mouse focus, if any.
    window_id: video.WindowId,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) Render {
        return .{
            .common = Common.fromSdl(val),
            .window_id = val.render.windowID,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Render, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.render.windowID = self.window_id;
        return ret;
    }
};

/// Sensor event structure (event.sensor.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Sensor = struct {
    /// Common event information.
    common: Common,
    /// The instance ID of the sensor.
    id: sensor.Id,
    /// Up to 6 values from the sensor, additional values can be queried using `sensor.Sensor.getData()`.
    data: [6]f32,
    /// The timestamp of the sensor reading in nanoseconds, not necessarily synchronized with the system clock.
    sensor_timestamp: u64,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) Sensor {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.sensor.which },
            .data = val.sensor.data,
            .sensor_timestamp = val.sensor.sensor_timestamp,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Sensor, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.sensor.which = self.id.value;
        ret.sensor.data = self.data;
        ret.sensor.sensor_timestamp = self.sensor_timestamp;
        return ret;
    }
};

/// Keyboard text editing event structure (event.edit.*).
///
/// ## Remarks
/// The start cursor is the position, in UTF-8 characters, where new typing will be inserted into the editing text.
/// The length is the number of UTF-8 characters that will be replaced by new typing.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const TextEditing = struct {
    /// Common event information.
    common: Common,
    /// The window with keyboard focus, if any.
    window_id: ?video.WindowId = null,
    /// The editing text.
    text: [:0]const u8,
    /// The start cursor of selected editing text.
    start: ?usize = null,
    /// The length of selected editing text.
    length: ?usize = null,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) TextEditing {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.edit.windowID == 0) null else val.edit.windowID,
            .text = std.mem.span(val.edit.text),
            .start = if (val.edit.start == -1) null else @intCast(val.edit.start),
            .length = if (val.edit.length == -1) null else @intCast(val.edit.length),
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: TextEditing, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.edit.windowID = self.window_id orelse 0;
        ret.edit.text = self.text.ptr;
        ret.edit.start = if (self.start) |val| @intCast(val) else -1;
        ret.edit.length = if (self.length) |val| @intCast(val) else -1;
        return ret;
    }
};

/// Keyboard IME candidates event structure (event.edit_candidates.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const TextEditingCandidates = struct {
    /// Common event information.
    common: Common,
    /// The window with keyboard focus, if any.
    window_id: ?video.WindowId = null,
    /// The list of candidates, or `null` if there are no candidates available.
    candidates: ?[]const [*:0]const u8,
    /// The index of the selected candidate.
    selected_candidate: ?usize,
    /// True if the list is horizontal, false if it's vertical.
    horizontal: bool,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) TextEditingCandidates {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.edit_candidates.windowID == 0) null else val.edit_candidates.windowID,
            .candidates = if (val.edit_candidates.candidates != null) @as([*]const [*:0]const u8, @ptrCast(val.edit_candidates.candidates))[0..@intCast(val.edit_candidates.num_candidates)] else null,
            .selected_candidate = if (val.edit_candidates.selected_candidate == -1) null else @intCast(val.edit_candidates.selected_candidate),
            .horizontal = val.edit_candidates.horizontal,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: TextEditingCandidates, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.edit_candidates.windowID = self.window_id orelse 0;
        if (self.candidates) |val| {
            ret.edit_candidates.candidates = val.ptr;
            ret.edit_candidates.num_candidates = @intCast(val.len);
        } else {
            ret.edit_candidates.candidates = null;
            ret.edit_candidates.num_candidates = -1;
        }
        ret.edit_candidates.selected_candidate = if (self.selected_candidate) |val| @intCast(val) else -1;
        return ret;
    }
};

/// Keyboard text input event structure (event.text.*).
///
/// ## Remarks
/// This event will never be delivered unless text input is enabled by calling `keyboard.startTextInput()`.
/// Text input is disabled by default!
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const TextInput = struct {
    /// Common event information.
    common: Common,
    /// The window with keyboard focus, if any.
    window_id: ?video.WindowId = null,
    /// The input text, UTF-8 encoded.
    text: [:0]const u8,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) TextInput {
        return .{
            .common = Common.fromSdl(val),
            .window_id = if (val.text.windowID == 0) null else val.text.windowID,
            .text = std.mem.span(val.text.text),
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: TextInput, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.text.windowID = self.window_id orelse 0;
        ret.text.text = self.text.ptr;
        return ret;
    }
};

/// Touch finger event structure (event.tfinger.*).
///
/// ## Remarks
/// Coordinates in this event are normalized. The `x` and `y` are normalized to a range between `0` and `1`, relative to the window, so `(0, 0)` is the top left and `(1, 1)` is the bottom right.
/// Delta coordinates `dx` and `dy` are normalized in the ranges of `-1` (traversed all the way from the bottom or right to all the way up or left)
/// to `1` (traversed all the way from the top or left to all the way down or right).
///
/// Note that while the coordinates are normalized, they are not clamped, which means in some circumstances you can get a value outside of this range.
/// For example, a renderer using logical presentation might give a negative value when the touch is in the letterboxing.
/// Some platforms might report a touch outside of the window, which will also be outside of the range.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const TouchFinger = struct {
    /// Common event information.
    common: Common,
    /// The touch device id.
    id: touch.Id,
    finger_id: touch.FingerId,
    /// Normalized in the range `0` to `1`.
    x: f32,
    /// Normalized in the range `0` to `1`.
    y: f32,
    /// Normalized in the range `-1` to `1`.
    dx: f32,
    /// Normalized in the range `-1` to `1`.
    dy: f32,
    /// Normalized in the range `0` to `1`.
    pressure: f32,
    /// The window with keyboard focus, if any.
    window_id: ?video.WindowId = null,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) TouchFinger {
        return .{
            .common = Common.fromSdl(val),
            .id = .{ .value = val.tfinger.touchID },
            .finger_id = .{ .value = val.tfinger.fingerID },
            .x = val.tfinger.x,
            .y = val.tfinger.y,
            .dx = val.tfinger.dx,
            .dy = val.tfinger.dy,
            .pressure = val.tfinger.pressure,
            .window_id = if (val.tfinger.windowID == 0) null else val.tfinger.windowID,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: TouchFinger, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.tfinger.touchID = self.id.value;
        ret.tfinger.fingerID = self.finger_id.value;
        ret.tfinger.x = self.x;
        ret.tfinger.y = self.y;
        ret.tfinger.dx = self.dx;
        ret.tfinger.dy = self.dy;
        ret.tfinger.pressure = self.pressure;
        ret.tfinger.windowID = self.window_id orelse 0;
        return ret;
    }
};

/// An unknown event.
pub const Unknown = struct {
    /// Common event information.
    common: Common,
    /// Event type that was not known.
    event_type: c.SDL_EventType,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) Unknown {
        return .{
            .common = Common.fromSdl(val),
            .event_type = val.type,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Unknown) c.SDL_Event {
        return .{
            .common = .{
                .type = self.event_type,
                .timestamp = self.common.timestamp,
            },
        };
    }
};

/// A user-defined event type (event.user.*).
///
/// ## Remarks
/// This event is unique; it is never created by SDL, but only by the application.
/// The event can be pushed onto the event queue using `events.push()`.
/// The contents of the structure members are completely up to the programmer;
/// the only requirement is that its type is a value obtained from `events.register()`.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
///
/// ## Code Examples
/// TODO!!!
/// ```zig
/// const event_type = events.register(1);
/// if (event_type) |val| {
///     try events.push(.{
///         .common = .{ .timestamp = timer.getNanosecondsSinceInit() },
///         .event_type = val,
///         .code = 0,
///     });
/// }
/// ```
pub const User = struct {
    /// Common event information.
    common: Common,
    /// The event type.
    event_type: c.SDL_EventType,
    /// Associated window if any.
    window_id: ?video.WindowId = null,
    /// User defined event code.
    code: i32,
    /// User defined pointer 1.
    data1: ?*anyopaque = null,
    /// User defined pointer 2.
    data2: ?*anyopaque = null,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) User {
        return .{
            .common = Common.fromSdl(val),
            .event_type = val.user.type,
            .window_id = if (val.user.windowID == 0) null else val.user.windowID,
            .code = val.user.code,
            .data1 = val.user.data1,
            .data2 = val.user.data2,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: User, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.user.windowID = self.window_id orelse 0;
        ret.user.code = self.code;
        ret.user.data1 = self.data1;
        ret.user.data2 = self.data2;
        return ret;
    }
};

/// Window state change event data (event.window.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const Window = struct {
    /// Common event information.
    common: Common,
    /// Associated window.
    id: video.WindowId,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) Window {
        return .{
            .common = Common.fromSdl(val),
            .id = val.window.windowID,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: Window, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.window.windowID = self.id;
        return ret;
    }
};

/// Window display changed event data (event.window.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const WindowDisplayChanged = struct {
    /// Common event information.
    common: Common,
    /// Associated window.
    id: video.WindowId,
    /// Display window was moved to.
    display: video.Display,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) WindowDisplayChanged {
        return .{
            .common = Common.fromSdl(val),
            .id = val.window.windowID,
            .display = @bitCast(val.window.data1),
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: WindowDisplayChanged, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.window.windowID = self.id;
        ret.window.data1 = @bitCast(self.display);
        return ret;
    }
};

/// Window exposed event data (event.window.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const WindowExposed = struct {
    /// Common event information.
    common: Common,
    /// Associated window.
    id: video.WindowId,
    /// If event was sent during live resizing.
    live_resize: bool,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) WindowExposed {
        return .{
            .common = Common.fromSdl(val),
            .id = val.window.windowID,
            .live_resize = val.window.data1 == 1,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: WindowExposed, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.window.windowID = self.id;
        ret.window.data1 = if (self.live_resize) 1 else 0;
        return ret;
    }
};

/// Window moved event data (event.window.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const WindowMoved = struct {
    /// Common event information.
    common: Common,
    /// Associated window.
    id: video.WindowId,
    x: i32,
    y: i32,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) WindowMoved {
        return .{
            .common = Common.fromSdl(val),
            .id = val.window.windowID,
            .x = val.window.data1,
            .y = val.window.data2,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: WindowMoved, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.window.windowID = self.id;
        ret.window.data1 = self.x;
        ret.window.data2 = self.y;
        return ret;
    }
};

/// Window pixel size changed event data (event.window.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const WindowPixelSizeChanged = struct {
    /// Common event information.
    common: Common,
    /// Associated window.
    id: video.WindowId,
    width: i32,
    height: i32,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) WindowPixelSizeChanged {
        return .{
            .common = Common.fromSdl(val),
            .id = val.window.windowID,
            .width = val.window.data1,
            .height = val.window.data2,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: WindowPixelSizeChanged, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.window.windowID = self.id;
        ret.window.data1 = self.width;
        ret.window.data2 = self.height;
        return ret;
    }
};

/// Window resized event data (event.window.*).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const WindowResized = struct {
    /// Common event information.
    common: Common,
    /// Associated window.
    id: video.WindowId,
    width: i32,
    height: i32,

    /// Convert from SDL.
    pub fn fromSdl(val: c.SDL_Event) WindowResized {
        return .{
            .common = Common.fromSdl(val),
            .id = val.window.windowID,
            .width = val.window.data1,
            .height = val.window.data2,
        };
    }

    /// Convert to SDL.
    pub fn toSdl(self: WindowResized, event_type: Type) c.SDL_Event {
        var ret = self.common.toSdl(event_type);
        ret.window.windowID = self.id;
        ret.window.data1 = self.width;
        ret.window.data2 = self.height;
        return ret;
    }
};

/// Needed to calculate padding.
const DummyEnum = enum(c.SDL_EventType) {
    empty,
};

/// Needed to calculate padding.
const DummyUnion = union(DummyEnum) {
    empty: void,
};

/// The structure for all events in SDL.
///
/// TODO!!!
pub const Event = union(Type) {
    /// User-requested quit.
    quit: Quit,
    /// The application is being terminated by the OS.
    /// This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationWillTerminate()`.
    /// Called on Android in `onDestroy()`.
    terminating: Common,
    /// The application is low on memory, free memory if possible.
    /// This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationDidReceiveMemoryWarning()`.
    /// Called on Android in `onTrimMemory().
    low_memory: Common,
    /// The application is about to enter the background.
    /// This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationWillResignActive()`.
    /// Called on Android in `onPause()`.
    will_enter_background: Common,
    /// The application did enter the background and may not get CPU for some time. This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationDidEnterBackground()`.
    /// Called on Android in `onPause()`.
    did_enter_background: Common,
    /// The application is about to enter the foreground.
    /// This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationWillEnterForeground()`.
    /// Called on Android in `onResume()`.
    will_enter_foreground: Common,
    /// The application is now interactive. This event must be handled in a callback set with `events.addWatch()`.
    /// Called on iOS in `applicationDidBecomeActive()`.
    /// Called on Android in `onResume()`.
    did_enter_foreground: Common,
    /// The user's locale preferences have changed.
    locale_changed: Common,
    /// The system theme changed.
    system_theme_changed: Common,
    /// Display orientation has changed.
    display_orientation: DisplayOrientation,
    /// Display has been added to the system.
    display_added: Display,
    /// Display has been removed from the system.
    display_removed: Display,
    /// Display has changed position.
    display_moved: Display,
    /// Display has changed desktop mode.
    display_desktop_mode_changed: Display,
    /// Display has changed current mode.
    display_current_mode_changed: Display,
    /// Display has changed content scale.
    display_content_scale_changed: Display,
    /// Display has changed usable bounds
    display_usable_bounds_changed: Display,
    /// Window has been shown.
    window_shown: Window,
    /// Window has been hidden.
    window_hidden: Window,
    /// Window has been exposed and should be redrawn, and can be redrawn directly from event watchers for this event.
    window_exposed: WindowExposed,
    /// Window has been moved.
    window_moved: WindowMoved,
    /// Window has been resized.
    window_resized: WindowResized,
    /// The pixel size of the window has changed.
    window_pixel_size_changed: WindowPixelSizeChanged,
    /// The pixel size of a Metal view associated with the window has changed.
    window_metal_view_resized: Window,
    /// Window has been minimized.
    window_minimized: Window,
    /// Window has been maximized.
    window_maximized: Window,
    /// Window has been restored to normal size and position.
    window_restored: Window,
    /// Window has gained mouse focus.
    window_mouse_enter: Window,
    /// Window has lost mouse focus.
    window_mouse_leave: Window,
    /// Window has gained keyboard focus.
    window_focus_gained: Window,
    /// Window has lost keyboard focus.
    window_focus_lost: Window,
    /// The window manager requests that the window be closed.
    window_close_requested: Window,
    /// Window had a hit test that wasn't normal.
    window_hit_test: Window,
    /// The ICC profile of the window's display has changed.
    window_icc_profile_changed: Window,
    /// Window has been moved to a display.
    window_display_changed: WindowDisplayChanged,
    /// Window display scale has been changed.
    window_display_scale_changed: Window,
    /// The window safe area has been changed.
    window_safe_area_changed: Window,
    /// The window has been occluded.
    window_occluded: Window,
    /// The window has entered fullscreen mode.
    window_enter_fullscreen: Window,
    /// The window has left fullscreen mode.
    window_leave_fullscreen: Window,
    /// The window with the associated ID is being or has been destroyed.
    /// If this message is being handled in an event watcher, the window handle is still valid and can still be used to retrieve any properties associated with the window.
    /// Otherwise, the handle has already been destroyed and all resources associated with it are invalid.
    window_destroyed: Window,
    /// Window HDR properties have changed.
    window_hdr_state_changed: Window,
    /// Key pressed.
    key_down: Keyboard,
    /// Key released.
    key_up: Keyboard,
    /// Keyboard text editing (composition).
    text_editing: TextEditing,
    /// Keyboard text input.
    text_input: TextInput,
    /// Keymap changed due to a system event such as an input language or keyboard layout change.
    keymap_changed: Common,
    /// A new keyboard has been inserted into the system.
    keyboard_added: KeyboardDevice,
    /// A keyboard has been removed.
    keyboard_removed: KeyboardDevice,
    /// Keyboard text editing candidates.
    text_editing_candidates: TextEditingCandidates,
    /// The on-screen keyboard has been shown.
    screen_keyboard_shown: Common,
    /// The on-screen keyboard has been hidden.
    screen_keyboard_hidden: Common,
    /// Mouse moved.
    mouse_motion: MouseMotion,
    /// Mouse button pressed.
    mouse_button_down: MouseButton,
    /// Mouse button released.
    mouse_button_up: MouseButton,
    /// Mouse wheel motion.
    mouse_wheel: MouseWheel,
    /// A new mouse has been inserted into the system.
    mouse_added: MouseDevice,
    /// A mouse has been removed.
    mouse_removed: MouseDevice,
    /// Joystick axis motion.
    joystick_axis_motion: JoystickAxis,
    /// Joystick trackball motion.
    joystick_ball_motion: JoystickBall,
    /// Joystick hat position change.
    joystick_hat_motion: JoystickHat,
    /// Joystick button pressed.
    joystick_button_down: JoystickButton,
    /// Joystick button released.
    joystick_button_up: JoystickButton,
    /// A new joystick has been inserted into the system.
    joystick_added: JoystickDevice,
    /// An opened joystick has been removed.
    joystick_removed: JoystickDevice,
    /// Joystick battery level change.
    joystick_battery_updated: JoystickBattery,
    /// Joystick update is complete.
    joystick_update_complete: JoystickDevice,
    /// Gamepad axis motion.
    gamepad_axis_motion: GamepadAxis,
    /// Gamepad button pressed.
    gamepad_button_down: GamepadButton,
    /// Gamepad button released.
    gamepad_button_up: GamepadButton,
    /// A new gamepad has been inserted into the system.
    gamepad_added: GamepadDevice,
    /// A gamepad has been removed.
    gamepad_removed: GamepadDevice,
    /// The gamepad mapping was updated.
    gamepad_remapped: GamepadDevice,
    /// Gamepad touchpad was touched.
    gamepad_touchpad_down: GamepadTouchpad,
    /// Gamepad touchpad finger was moved.
    gamepad_touchpad_motion: GamepadTouchpad,
    /// Gamepad touchpad finger was lifted.
    gamepad_touchpad_up: GamepadTouchpad,
    /// Gamepad sensor was updated.
    gamepad_sensor_update: GamepadSensor,
    /// Gamepad update is complete.
    gamepad_update_complete: GamepadDevice,
    /// Gamepad Steam handle has changed.
    gamepad_steam_handle_updated: GamepadDevice,
    finger_down: TouchFinger,
    finger_up: TouchFinger,
    finger_motion: TouchFinger,
    finger_canceled: TouchFinger,
    /// Pinch gesture started.
    pinch_begin: PinchFinger,
    /// Pinch gesture updated.
    pinch_update: PinchFinger,
    /// Pinch gesture ended.
    pinch_end: PinchFinger,
    /// The clipboard or primary selection changed.
    clipboard_update: Clipboard,
    /// The system requests a file open.
    drop_file: DropFile,
    /// Text/plain drag-and-drop event.
    drop_text: DropText,
    /// A new set of drops is beginning.
    drop_begin: DropBegin,
    /// Current set of drops is now complete.
    drop_complete: Drop,
    /// Position while moving over the window.
    drop_position: Drop,
    /// A new audio device is available.
    audio_device_added: AudioDevice,
    /// An audio device has been removed.
    audio_device_removed: AudioDevice,
    /// An audio device's format has been changed by the system.
    audio_device_format_changed: AudioDevice,
    /// A sensor was updated.
    sensor_update: Sensor,
    /// Pressure-sensitive pen has become available.
    pen_proximity_in: PenProximity,
    /// Pressure-sensitive pen has become unavailable.
    pen_proximity_out: PenProximity,
    /// Pressure-sensitive pen touched drawing surface.
    pen_down: PenTouch,
    /// Pressure-sensitive pen stopped touching drawing surface.
    pen_up: PenTouch,
    /// Pressure-sensitive pen button pressed.
    pen_button_down: PenButton,
    /// Pressure-sensitive pen button released.
    pen_button_up: PenButton,
    /// Pressure-sensitive pen is moving on the tablet.
    pen_motion: PenMotion,
    /// Pressure-sensitive pen angle/pressure/etc changed.
    pen_axis: PenAxis,
    /// A new camera device is available.
    camera_device_added: CameraDevice,
    /// A camera device has been removed.
    camera_device_removed: CameraDevice,
    /// A camera device has been approved for use by the user.
    camera_device_approved: CameraDevice,
    /// A camera device has been denied for use by the user.
    camera_device_denied: CameraDevice,
    /// The render targets have been reset and their contents need to be updated.
    render_targets_reset: Render,
    /// The device has been reset and all textures need to be recreated.
    render_device_reset: Render,
    /// The device has been lost and can't be recovered.
    render_device_lost: Render,
    private0: Common,
    private1: Common,
    private2: Common,
    private3: Common,
    /// Signals the end of an event poll cycle.
    poll_sentinal: Common,
    /// User events, should be allocated with `events.register()`.
    user: User,
    /// For padding out the union.
    padding: [@sizeOf(c.SDL_Event) - @sizeOf(DummyUnion)]u8,
    /// An unknown event.
    unknown: Common,

    // Size tests.
    comptime {
        std.debug.assert(@sizeOf(c.SDL_Event) <= @sizeOf(Event));
    }

    /// Create a managed event from an SDL event.
    ///
    /// ## Function Parameters
    /// * `event`: SDL event to manage.
    ///
    /// ## Return Value
    /// A managed event union.
    ///
    /// ## Remarks
    /// This makes a copy of the event provided.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn fromSdl(event: c.SDL_Event) Event {
        return switch (event.type) {
            c.SDL_EVENT_QUIT => .{ .quit = @FieldType(Event, "quit").fromSdl(event) },
            c.SDL_EVENT_TERMINATING => .{ .terminating = @FieldType(Event, "terminating").fromSdl(event) },
            c.SDL_EVENT_LOW_MEMORY => .{ .low_memory = @FieldType(Event, "low_memory").fromSdl(event) },
            c.SDL_EVENT_WILL_ENTER_BACKGROUND => .{ .will_enter_background = @FieldType(Event, "will_enter_background").fromSdl(event) },
            c.SDL_EVENT_DID_ENTER_BACKGROUND => .{ .did_enter_background = @FieldType(Event, "did_enter_background").fromSdl(event) },
            c.SDL_EVENT_WILL_ENTER_FOREGROUND => .{ .will_enter_foreground = @FieldType(Event, "will_enter_foreground").fromSdl(event) },
            c.SDL_EVENT_DID_ENTER_FOREGROUND => .{ .did_enter_foreground = @FieldType(Event, "did_enter_foreground").fromSdl(event) },
            c.SDL_EVENT_LOCALE_CHANGED => .{ .locale_changed = @FieldType(Event, "locale_changed").fromSdl(event) },
            c.SDL_EVENT_SYSTEM_THEME_CHANGED => .{ .system_theme_changed = @FieldType(Event, "system_theme_changed").fromSdl(event) },
            c.SDL_EVENT_DISPLAY_ORIENTATION => .{ .display_orientation = @FieldType(Event, "display_orientation").fromSdl(event) },
            c.SDL_EVENT_DISPLAY_ADDED => .{ .display_added = @FieldType(Event, "display_added").fromSdl(event) },
            c.SDL_EVENT_DISPLAY_REMOVED => .{ .display_removed = @FieldType(Event, "display_removed").fromSdl(event) },
            c.SDL_EVENT_DISPLAY_MOVED => .{ .display_moved = @FieldType(Event, "display_moved").fromSdl(event) },
            c.SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED => .{ .display_desktop_mode_changed = @FieldType(Event, "display_desktop_mode_changed").fromSdl(event) },
            c.SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED => .{ .display_current_mode_changed = @FieldType(Event, "display_current_mode_changed").fromSdl(event) },
            c.SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED => .{ .display_content_scale_changed = @FieldType(Event, "display_content_scale_changed").fromSdl(event) },
            c.SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED => .{ .display_usable_bounds_changed = @FieldType(Event, "display_usable_bounds_changed").fromSdl(event) },
            c.SDL_EVENT_WINDOW_SHOWN => .{ .window_shown = @FieldType(Event, "window_shown").fromSdl(event) },
            c.SDL_EVENT_WINDOW_HIDDEN => .{ .window_hidden = @FieldType(Event, "window_hidden").fromSdl(event) },
            c.SDL_EVENT_WINDOW_EXPOSED => .{ .window_exposed = @FieldType(Event, "window_exposed").fromSdl(event) },
            c.SDL_EVENT_WINDOW_MOVED => .{ .window_moved = @FieldType(Event, "window_moved").fromSdl(event) },
            c.SDL_EVENT_WINDOW_RESIZED => .{ .window_resized = @FieldType(Event, "window_resized").fromSdl(event) },
            c.SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED => .{ .window_pixel_size_changed = @FieldType(Event, "window_pixel_size_changed").fromSdl(event) },
            c.SDL_EVENT_WINDOW_METAL_VIEW_RESIZED => .{ .window_metal_view_resized = @FieldType(Event, "window_metal_view_resized").fromSdl(event) },
            c.SDL_EVENT_WINDOW_MINIMIZED => .{ .window_minimized = @FieldType(Event, "window_minimized").fromSdl(event) },
            c.SDL_EVENT_WINDOW_MAXIMIZED => .{ .window_maximized = @FieldType(Event, "window_maximized").fromSdl(event) },
            c.SDL_EVENT_WINDOW_RESTORED => .{ .window_restored = @FieldType(Event, "window_restored").fromSdl(event) },
            c.SDL_EVENT_WINDOW_MOUSE_ENTER => .{ .window_mouse_enter = @FieldType(Event, "window_mouse_enter").fromSdl(event) },
            c.SDL_EVENT_WINDOW_MOUSE_LEAVE => .{ .window_mouse_leave = @FieldType(Event, "window_mouse_leave").fromSdl(event) },
            c.SDL_EVENT_WINDOW_FOCUS_GAINED => .{ .window_focus_gained = @FieldType(Event, "window_focus_gained").fromSdl(event) },
            c.SDL_EVENT_WINDOW_FOCUS_LOST => .{ .window_focus_lost = @FieldType(Event, "window_focus_lost").fromSdl(event) },
            c.SDL_EVENT_WINDOW_CLOSE_REQUESTED => .{ .window_close_requested = @FieldType(Event, "window_close_requested").fromSdl(event) },
            c.SDL_EVENT_WINDOW_HIT_TEST => .{ .window_hit_test = @FieldType(Event, "window_hit_test").fromSdl(event) },
            c.SDL_EVENT_WINDOW_ICCPROF_CHANGED => .{ .window_icc_profile_changed = @FieldType(Event, "window_icc_profile_changed").fromSdl(event) },
            c.SDL_EVENT_WINDOW_DISPLAY_CHANGED => .{ .window_display_changed = @FieldType(Event, "window_display_changed").fromSdl(event) },
            c.SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED => .{ .window_display_scale_changed = @FieldType(Event, "window_display_scale_changed").fromSdl(event) },
            c.SDL_EVENT_WINDOW_SAFE_AREA_CHANGED => .{ .window_safe_area_changed = @FieldType(Event, "window_safe_area_changed").fromSdl(event) },
            c.SDL_EVENT_WINDOW_OCCLUDED => .{ .window_occluded = @FieldType(Event, "window_occluded").fromSdl(event) },
            c.SDL_EVENT_WINDOW_ENTER_FULLSCREEN => .{ .window_enter_fullscreen = @FieldType(Event, "window_enter_fullscreen").fromSdl(event) },
            c.SDL_EVENT_WINDOW_LEAVE_FULLSCREEN => .{ .window_leave_fullscreen = @FieldType(Event, "window_leave_fullscreen").fromSdl(event) },
            c.SDL_EVENT_WINDOW_DESTROYED => .{ .window_destroyed = @FieldType(Event, "window_destroyed").fromSdl(event) },
            c.SDL_EVENT_WINDOW_HDR_STATE_CHANGED => .{ .window_hdr_state_changed = @FieldType(Event, "window_hdr_state_changed").fromSdl(event) },
            c.SDL_EVENT_KEY_DOWN => .{ .key_down = @FieldType(Event, "key_down").fromSdl(event) },
            c.SDL_EVENT_KEY_UP => .{ .key_up = @FieldType(Event, "key_up").fromSdl(event) },
            c.SDL_EVENT_TEXT_EDITING => .{ .text_editing = @FieldType(Event, "text_editing").fromSdl(event) },
            c.SDL_EVENT_TEXT_INPUT => .{ .text_input = @FieldType(Event, "text_input").fromSdl(event) },
            c.SDL_EVENT_KEYMAP_CHANGED => .{ .keymap_changed = @FieldType(Event, "keymap_changed").fromSdl(event) },
            c.SDL_EVENT_KEYBOARD_ADDED => .{ .keyboard_added = @FieldType(Event, "keyboard_added").fromSdl(event) },
            c.SDL_EVENT_KEYBOARD_REMOVED => .{ .keyboard_removed = @FieldType(Event, "keyboard_removed").fromSdl(event) },
            c.SDL_EVENT_TEXT_EDITING_CANDIDATES => .{ .text_editing_candidates = @FieldType(Event, "text_editing_candidates").fromSdl(event) },
            c.SDL_EVENT_SCREEN_KEYBOARD_SHOWN => .{ .screen_keyboard_shown = @FieldType(Event, "screen_keyboard_shown").fromSdl(event) },
            c.SDL_EVENT_SCREEN_KEYBOARD_HIDDEN => .{ .screen_keyboard_hidden = @FieldType(Event, "screen_keyboard_hidden").fromSdl(event) },
            c.SDL_EVENT_MOUSE_MOTION => .{ .mouse_motion = @FieldType(Event, "mouse_motion").fromSdl(event) },
            c.SDL_EVENT_MOUSE_BUTTON_DOWN => .{ .mouse_button_down = @FieldType(Event, "mouse_button_down").fromSdl(event) },
            c.SDL_EVENT_MOUSE_BUTTON_UP => .{ .mouse_button_up = @FieldType(Event, "mouse_button_up").fromSdl(event) },
            c.SDL_EVENT_MOUSE_WHEEL => .{ .mouse_wheel = @FieldType(Event, "mouse_wheel").fromSdl(event) },
            c.SDL_EVENT_MOUSE_ADDED => .{ .mouse_added = @FieldType(Event, "mouse_added").fromSdl(event) },
            c.SDL_EVENT_MOUSE_REMOVED => .{ .mouse_removed = @FieldType(Event, "mouse_removed").fromSdl(event) },
            c.SDL_EVENT_JOYSTICK_AXIS_MOTION => .{ .joystick_axis_motion = @FieldType(Event, "joystick_axis_motion").fromSdl(event) },
            c.SDL_EVENT_JOYSTICK_BALL_MOTION => .{ .joystick_ball_motion = @FieldType(Event, "joystick_ball_motion").fromSdl(event) },
            c.SDL_EVENT_JOYSTICK_HAT_MOTION => .{ .joystick_hat_motion = @FieldType(Event, "joystick_hat_motion").fromSdl(event) },
            c.SDL_EVENT_JOYSTICK_BUTTON_DOWN => .{ .joystick_button_down = @FieldType(Event, "joystick_button_down").fromSdl(event) },
            c.SDL_EVENT_JOYSTICK_BUTTON_UP => .{ .joystick_button_up = @FieldType(Event, "joystick_button_up").fromSdl(event) },
            c.SDL_EVENT_JOYSTICK_ADDED => .{ .joystick_added = @FieldType(Event, "joystick_added").fromSdl(event) },
            c.SDL_EVENT_JOYSTICK_REMOVED => .{ .joystick_removed = @FieldType(Event, "joystick_removed").fromSdl(event) },
            c.SDL_EVENT_JOYSTICK_BATTERY_UPDATED => .{ .joystick_battery_updated = @FieldType(Event, "joystick_battery_updated").fromSdl(event) },
            c.SDL_EVENT_JOYSTICK_UPDATE_COMPLETE => .{ .joystick_update_complete = @FieldType(Event, "joystick_update_complete").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_AXIS_MOTION => .{ .gamepad_axis_motion = @FieldType(Event, "gamepad_axis_motion").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_BUTTON_DOWN => .{ .gamepad_button_down = @FieldType(Event, "gamepad_button_down").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_BUTTON_UP => .{ .gamepad_button_up = @FieldType(Event, "gamepad_button_up").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_ADDED => .{ .gamepad_added = @FieldType(Event, "gamepad_added").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_REMOVED => .{ .gamepad_removed = @FieldType(Event, "gamepad_removed").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_REMAPPED => .{ .gamepad_remapped = @FieldType(Event, "gamepad_remapped").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN => .{ .gamepad_touchpad_down = @FieldType(Event, "gamepad_touchpad_down").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION => .{ .gamepad_touchpad_motion = @FieldType(Event, "gamepad_touchpad_motion").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_TOUCHPAD_UP => .{ .gamepad_touchpad_up = @FieldType(Event, "gamepad_touchpad_up").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_SENSOR_UPDATE => .{ .gamepad_sensor_update = @FieldType(Event, "gamepad_sensor_update").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_UPDATE_COMPLETE => .{ .gamepad_update_complete = @FieldType(Event, "gamepad_update_complete").fromSdl(event) },
            c.SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED => .{ .gamepad_steam_handle_updated = @FieldType(Event, "gamepad_steam_handle_updated").fromSdl(event) },
            c.SDL_EVENT_FINGER_DOWN => .{ .finger_down = @FieldType(Event, "finger_down").fromSdl(event) },
            c.SDL_EVENT_FINGER_UP => .{ .finger_up = @FieldType(Event, "finger_up").fromSdl(event) },
            c.SDL_EVENT_FINGER_MOTION => .{ .finger_motion = @FieldType(Event, "finger_motion").fromSdl(event) },
            c.SDL_EVENT_FINGER_CANCELED => .{ .finger_canceled = @FieldType(Event, "finger_canceled").fromSdl(event) },
            c.SDL_EVENT_PINCH_BEGIN => .{ .pinch_begin = @FieldType(Event, "pinch_begin").fromSdl(event) },
            c.SDL_EVENT_PINCH_UPDATE => .{ .pinch_update = @FieldType(Event, "pinch_update").fromSdl(event) },
            c.SDL_EVENT_PINCH_END => .{ .pinch_end = @FieldType(Event, "pinch_end").fromSdl(event) },
            c.SDL_EVENT_CLIPBOARD_UPDATE => .{ .clipboard_update = @FieldType(Event, "clipboard_update").fromSdl(event) },
            c.SDL_EVENT_DROP_FILE => .{ .drop_file = @FieldType(Event, "drop_file").fromSdl(event) },
            c.SDL_EVENT_DROP_TEXT => .{ .drop_text = @FieldType(Event, "drop_text").fromSdl(event) },
            c.SDL_EVENT_DROP_BEGIN => .{ .drop_begin = @FieldType(Event, "drop_begin").fromSdl(event) },
            c.SDL_EVENT_DROP_COMPLETE => .{ .drop_complete = @FieldType(Event, "drop_complete").fromSdl(event) },
            c.SDL_EVENT_DROP_POSITION => .{ .drop_position = @FieldType(Event, "drop_position").fromSdl(event) },
            c.SDL_EVENT_AUDIO_DEVICE_ADDED => .{ .audio_device_added = @FieldType(Event, "audio_device_added").fromSdl(event) },
            c.SDL_EVENT_AUDIO_DEVICE_REMOVED => .{ .audio_device_removed = @FieldType(Event, "audio_device_removed").fromSdl(event) },
            c.SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED => .{ .audio_device_format_changed = @FieldType(Event, "audio_device_format_changed").fromSdl(event) },
            c.SDL_EVENT_SENSOR_UPDATE => .{ .sensor_update = @FieldType(Event, "sensor_update").fromSdl(event) },
            c.SDL_EVENT_PEN_PROXIMITY_IN => .{ .pen_proximity_in = @FieldType(Event, "pen_proximity_in").fromSdl(event) },
            c.SDL_EVENT_PEN_PROXIMITY_OUT => .{ .pen_proximity_out = @FieldType(Event, "pen_proximity_out").fromSdl(event) },
            c.SDL_EVENT_PEN_DOWN => .{ .pen_down = @FieldType(Event, "pen_down").fromSdl(event) },
            c.SDL_EVENT_PEN_UP => .{ .pen_up = @FieldType(Event, "pen_up").fromSdl(event) },
            c.SDL_EVENT_PEN_BUTTON_DOWN => .{ .pen_button_down = @FieldType(Event, "pen_button_down").fromSdl(event) },
            c.SDL_EVENT_PEN_BUTTON_UP => .{ .pen_button_up = @FieldType(Event, "pen_button_up").fromSdl(event) },
            c.SDL_EVENT_PEN_MOTION => .{ .pen_motion = @FieldType(Event, "pen_motion").fromSdl(event) },
            c.SDL_EVENT_PEN_AXIS => .{ .pen_axis = @FieldType(Event, "pen_axis").fromSdl(event) },
            c.SDL_EVENT_CAMERA_DEVICE_ADDED => .{ .camera_device_added = @FieldType(Event, "camera_device_added").fromSdl(event) },
            c.SDL_EVENT_CAMERA_DEVICE_REMOVED => .{ .camera_device_removed = @FieldType(Event, "camera_device_removed").fromSdl(event) },
            c.SDL_EVENT_CAMERA_DEVICE_APPROVED => .{ .camera_device_approved = @FieldType(Event, "camera_device_approved").fromSdl(event) },
            c.SDL_EVENT_CAMERA_DEVICE_DENIED => .{ .camera_device_denied = @FieldType(Event, "camera_device_denied").fromSdl(event) },
            c.SDL_EVENT_RENDER_TARGETS_RESET => .{ .render_targets_reset = @FieldType(Event, "render_targets_reset").fromSdl(event) },
            c.SDL_EVENT_RENDER_DEVICE_RESET => .{ .render_device_reset = @FieldType(Event, "render_device_reset").fromSdl(event) },
            c.SDL_EVENT_RENDER_DEVICE_LOST => .{ .render_device_lost = @FieldType(Event, "render_device_lost").fromSdl(event) },
            c.SDL_EVENT_PRIVATE0 => .{ .private0 = @FieldType(Event, "private0").fromSdl(event) },
            c.SDL_EVENT_PRIVATE1 => .{ .private1 = @FieldType(Event, "private1").fromSdl(event) },
            c.SDL_EVENT_PRIVATE2 => .{ .private2 = @FieldType(Event, "private2").fromSdl(event) },
            c.SDL_EVENT_PRIVATE3 => .{ .private3 = @FieldType(Event, "private3").fromSdl(event) },
            c.SDL_EVENT_POLL_SENTINEL => .{ .poll_sentinal = @FieldType(Event, "poll_sentinal").fromSdl(event) },
            c.SDL_EVENT_USER => .{ .user = @FieldType(Event, "user").fromSdl(event) },
            c.SDL_EVENT_ENUM_PADDING => .{ .padding = @splat(0) },
            else => .{ .unknown = @FieldType(Event, "unknown").fromSdl(event) },
        };
    }

    /// Generate an English description of an event.
    ///
    /// ## Function Parameters
    /// * `self`: An event to describe.
    /// * `allocator`: Allocator used to allocate memory for the returned event description.
    ///
    /// ## Return Value
    /// Returns an owned buffer containing the English event description.
    /// This must be freed.
    ///
    /// ## Remarks
    /// The returned event description might look something like this:
    /// `SDL_EVENT_MOUSE_MOTION (timestamp=1140256324 windowid=2 which=0 state=0 x=492.99 y=139.09 xrel=52 yrel=6)`
    ///
    /// The exact format of the string is not guaranteed; it is intended for logging purposes, to be read by a human, and not parsed by a computer.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.4.0.
    pub fn getDescription(
        self: Event,
        allocator: std.mem.Allocator,
    ) ![:0]u8 {
        const c_event = self.toSdl();
        const len = c.SDL_GetEventDescription(&c_event, null, 0);
        const ret: [:0]u8 = try allocator.allocSentinel(u8, @intCast(len), 0);
        _ = c.SDL_GetEventDescription(&c_event, ret.ptr, len);
        return ret;
    }

    /// Get window associated with an event.
    ///
    /// ## Function Parameters
    /// * `self`: An event containing a window.
    ///
    /// ## Return Value
    /// Returns the associated window on success or `null` if there is none.
    ///
    /// ## Thread Safety
    /// It is safe to call this function from any thread.
    ///
    /// ## Version
    /// This function is available since SDL 3.2.0.
    pub fn getWindow(
        self: Event,
    ) ?video.Window {
        const event = toSdl(self);
        const ret = c.SDL_GetWindowFromEvent(&event);
        if (ret) |val| {
            return .{ .value = val };
        }
        return null;
    }

    /// Create an unmanaged event from an SDL event.
    ///
    /// ## Function Parameters
    /// * `event`: Managed event to unmanage.
    ///
    /// ## Return Value
    /// Returns an unmanaged SDL event.
    ///
    /// ## Remarks
    /// This makes a copy of the event provided.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn toSdl(event: Event) c.SDL_Event {
        return switch (event) {
            .quit => |val| val.toSdl(.quit),
            .terminating => |val| val.toSdl(.terminating),
            .low_memory => |val| val.toSdl(.low_memory),
            .will_enter_background => |val| val.toSdl(.will_enter_background),
            .did_enter_background => |val| val.toSdl(.did_enter_background),
            .will_enter_foreground => |val| val.toSdl(.will_enter_foreground),
            .did_enter_foreground => |val| val.toSdl(.did_enter_foreground),
            .locale_changed => |val| val.toSdl(.locale_changed),
            .system_theme_changed => |val| val.toSdl(.system_theme_changed),
            .display_orientation => |val| val.toSdl(.display_orientation),
            .display_added => |val| val.toSdl(.display_added),
            .display_removed => |val| val.toSdl(.display_removed),
            .display_moved => |val| val.toSdl(.display_moved),
            .display_desktop_mode_changed => |val| val.toSdl(.display_desktop_mode_changed),
            .display_current_mode_changed => |val| val.toSdl(.display_current_mode_changed),
            .display_content_scale_changed => |val| val.toSdl(.display_content_scale_changed),
            .display_usable_bounds_changed => |val| val.toSdl(.display_usable_bounds_changed),
            .window_shown => |val| val.toSdl(.window_shown),
            .window_hidden => |val| val.toSdl(.window_hidden),
            .window_exposed => |val| val.toSdl(.window_exposed),
            .window_moved => |val| val.toSdl(.window_moved),
            .window_resized => |val| val.toSdl(.window_resized),
            .window_pixel_size_changed => |val| val.toSdl(.window_pixel_size_changed),
            .window_metal_view_resized => |val| val.toSdl(.window_metal_view_resized),
            .window_minimized => |val| val.toSdl(.window_minimized),
            .window_maximized => |val| val.toSdl(.window_maximized),
            .window_restored => |val| val.toSdl(.window_restored),
            .window_mouse_enter => |val| val.toSdl(.window_mouse_enter),
            .window_mouse_leave => |val| val.toSdl(.window_mouse_leave),
            .window_focus_gained => |val| val.toSdl(.window_focus_gained),
            .window_focus_lost => |val| val.toSdl(.window_focus_lost),
            .window_close_requested => |val| val.toSdl(.window_close_requested),
            .window_hit_test => |val| val.toSdl(.window_hit_test),
            .window_icc_profile_changed => |val| val.toSdl(.window_icc_profile_changed),
            .window_display_changed => |val| val.toSdl(.window_display_changed),
            .window_display_scale_changed => |val| val.toSdl(.window_display_scale_changed),
            .window_safe_area_changed => |val| val.toSdl(.window_safe_area_changed),
            .window_occluded => |val| val.toSdl(.window_occluded),
            .window_enter_fullscreen => |val| val.toSdl(.window_enter_fullscreen),
            .window_leave_fullscreen => |val| val.toSdl(.window_leave_fullscreen),
            .window_destroyed => |val| val.toSdl(.window_destroyed),
            .window_hdr_state_changed => |val| val.toSdl(.window_hdr_state_changed),
            .key_down => |val| val.toSdl(.key_down),
            .key_up => |val| val.toSdl(.key_up),
            .text_editing => |val| val.toSdl(.text_editing),
            .text_input => |val| val.toSdl(.text_input),
            .keymap_changed => |val| val.toSdl(.keymap_changed),
            .keyboard_added => |val| val.toSdl(.keyboard_added),
            .keyboard_removed => |val| val.toSdl(.keyboard_removed),
            .text_editing_candidates => |val| val.toSdl(.text_editing_candidates),
            .screen_keyboard_shown => |val| val.toSdl(.screen_keyboard_shown),
            .screen_keyboard_hidden => |val| val.toSdl(.screen_keyboard_hidden),
            .mouse_motion => |val| val.toSdl(.mouse_motion),
            .mouse_button_down => |val| val.toSdl(.mouse_button_down),
            .mouse_button_up => |val| val.toSdl(.mouse_button_up),
            .mouse_wheel => |val| val.toSdl(.mouse_wheel),
            .mouse_added => |val| val.toSdl(.mouse_added),
            .mouse_removed => |val| val.toSdl(.mouse_removed),
            .joystick_axis_motion => |val| val.toSdl(.joystick_axis_motion),
            .joystick_ball_motion => |val| val.toSdl(.joystick_ball_motion),
            .joystick_hat_motion => |val| val.toSdl(.joystick_hat_motion),
            .joystick_button_down => |val| val.toSdl(.joystick_button_down),
            .joystick_button_up => |val| val.toSdl(.joystick_button_up),
            .joystick_added => |val| val.toSdl(.joystick_added),
            .joystick_removed => |val| val.toSdl(.joystick_removed),
            .joystick_battery_updated => |val| val.toSdl(.joystick_battery_updated),
            .joystick_update_complete => |val| val.toSdl(.joystick_update_complete),
            .gamepad_axis_motion => |val| val.toSdl(.gamepad_axis_motion),
            .gamepad_button_down => |val| val.toSdl(.gamepad_button_down),
            .gamepad_button_up => |val| val.toSdl(.gamepad_button_up),
            .gamepad_added => |val| val.toSdl(.gamepad_added),
            .gamepad_removed => |val| val.toSdl(.gamepad_removed),
            .gamepad_remapped => |val| val.toSdl(.gamepad_remapped),
            .gamepad_touchpad_down => |val| val.toSdl(.gamepad_touchpad_down),
            .gamepad_touchpad_motion => |val| val.toSdl(.gamepad_touchpad_motion),
            .gamepad_touchpad_up => |val| val.toSdl(.gamepad_touchpad_up),
            .gamepad_sensor_update => |val| val.toSdl(.gamepad_sensor_update),
            .gamepad_update_complete => |val| val.toSdl(.gamepad_update_complete),
            .gamepad_steam_handle_updated => |val| val.toSdl(.gamepad_steam_handle_updated),
            .finger_down => |val| val.toSdl(.finger_down),
            .finger_up => |val| val.toSdl(.finger_up),
            .finger_motion => |val| val.toSdl(.finger_motion),
            .finger_canceled => |val| val.toSdl(.finger_canceled),
            .pinch_begin => |val| val.toSdl(.pinch_begin),
            .pinch_update => |val| val.toSdl(.pinch_update),
            .pinch_end => |val| val.toSdl(.pinch_end),
            .clipboard_update => |val| val.toSdl(.clipboard_update),
            .drop_file => |val| val.toSdl(.drop_file),
            .drop_text => |val| val.toSdl(.drop_text),
            .drop_begin => |val| val.toSdl(.drop_begin),
            .drop_complete => |val| val.toSdl(.drop_complete),
            .drop_position => |val| val.toSdl(.drop_position),
            .audio_device_added => |val| val.toSdl(.audio_device_added),
            .audio_device_removed => |val| val.toSdl(.audio_device_removed),
            .audio_device_format_changed => |val| val.toSdl(.audio_device_format_changed),
            .sensor_update => |val| val.toSdl(.sensor_update),
            .pen_proximity_in => |val| val.toSdl(.pen_proximity_in),
            .pen_proximity_out => |val| val.toSdl(.pen_proximity_out),
            .pen_down => |val| val.toSdl(.pen_down),
            .pen_up => |val| val.toSdl(.pen_up),
            .pen_button_down => |val| val.toSdl(.pen_button_down),
            .pen_button_up => |val| val.toSdl(.pen_button_up),
            .pen_motion => |val| val.toSdl(.pen_motion),
            .pen_axis => |val| val.toSdl(.pen_axis),
            .camera_device_added => |val| val.toSdl(.camera_device_added),
            .camera_device_removed => |val| val.toSdl(.camera_device_removed),
            .camera_device_approved => |val| val.toSdl(.camera_device_approved),
            .camera_device_denied => |val| val.toSdl(.camera_device_denied),
            .render_targets_reset => |val| val.toSdl(.render_targets_reset),
            .render_device_reset => |val| val.toSdl(.render_device_reset),
            .render_device_lost => |val| val.toSdl(.render_device_lost),
            .private0 => |val| val.toSdl(.private0),
            .private1 => |val| val.toSdl(.private1),
            .private2 => |val| val.toSdl(.private2),
            .private3 => |val| val.toSdl(.private3),
            .poll_sentinal => |val| val.toSdl(.poll_sentinal),
            .user => |val| val.toSdl(.user),
            .padding => .{
                .type = c.SDL_EVENT_ENUM_PADDING,
            },
            .unknown => |val| val.toSdl(.unknown),
        };
    }

    /// Create an unmanaged event from an SDL event in place.
    ///
    /// ## Function Parameters
    /// * `event`: Managed event to unmanage. The `event` passed in will be unusable after.
    ///
    /// ## Return Value
    /// Returns an unmanaged SDL event.
    ///
    /// ## Remarks
    /// This will modify memory in-place.
    /// This means that using the `event` passed into this afterwards will result in undefined behavior.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn toSdlInPlace(event: *Event) *c.SDL_Event {
        const unmanaged: *c.SDL_Event = @ptrCast(event);
        unmanaged.* = toSdl(event.*);
        return unmanaged;
    }
};

/// Add a callback to be triggered when an event is added to the event queue.
///
/// ## Function Parameters
/// * `UserData`: Type of user data.
/// * `event_filter`: An `events.Filter` function to call when an event happens.
/// * `user_data`: A pointer that is passed to `event_filter`.
///
/// ## Return Value
/// A C callback filter to use with `events.removeWatch()`.
///
/// ## Remarks
/// The `event_filter` will be called when an event happens, and its return value is ignored.
///
/// WARNING: Be very careful of what you do in the event filter function, as it may run in a different thread!
///
/// If the quit event is generated by a signal (e.g. SIGINT), it will bypass the internal queue and be delivered to the watch callback immediately,
/// and arrive at the next event poll.
///
/// Note: the callback is called for events posted by the user through `events.push()`, but not for disabled events,
/// nor for events by a filter callback set with `events.setFilter()`, nor for events posted by the user through `events.peep()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn addWatch(
    comptime UserData: type,
    comptime event_filter: Filter(UserData),
    user_data: ?*anyopaque,
) !FilterC {
    const Cb = struct {
        pub fn run(user_data_c: ?*anyopaque, event_c: [*c]c.SDL_Event) callconv(.c) bool {
            var event = Event.fromSdl(event_c.*);
            const ret = event_filter(@ptrCast(@alignCast(user_data_c)), &event);
            event_c.* = event.toSdl();
            return ret;
        }
    };
    try errors.wrapCallBool(c.SDL_AddEventWatch(Cb.run, @ptrCast(user_data)));
    return Cb.run;
}

/// If an event is available in the queue.
///
/// ## Return Value
/// Returns true if there is at least one event in the queue, false otherwise.
///
/// ## Remarks
/// This will not effect the events in the queue.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This is provided by zig-sdl3.
pub fn available() bool {
    return c.SDL_PollEvent(null);
}

/// Query the state of processing events by type.
///
/// ## Function Parameters
/// * `event_type`: The type of event.
///
/// ## Return Value
/// Returns true if the event is being processed, false otherwise.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn enabled(
    event_type: Type,
) bool {
    return c.SDL_EventEnabled(@intFromEnum(event_type));
}

/// Run a specific filter function on the current event queue, removing any events for which the filter returns false.
///
/// ## Function Parameters
/// * `UserData`: Type of user data.
/// * `event_filter`: An `events.Filter` function to call when an event happens.
/// * `user_data`: A pointer that is passed to `event_filter`.
///
/// ## Remarks
/// See `events.setFilter()` for more information.
/// Unlike `events.setFilter()`, this function does not change the filter permanently, it only uses the supplied filter until this function returns.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn filter(
    comptime UserData: type,
    comptime event_filter: Filter(UserData),
    user_data: ?*UserData,
) void {
    const Cb = struct {
        pub fn run(user_data_c: ?*anyopaque, event_c: [*c]c.SDL_Event) callconv(.c) bool {
            var event = Event.fromSdl(event_c.*);
            const ret = event_filter(@ptrCast(@alignCast(user_data_c)), &event);
            event_c.* = event.toSdl();
            return ret;
        }
    };
    c.SDL_FilterEvents(Cb.run, @ptrCast(user_data));
}

/// Clear events of a specific type from the event queue.
///
/// ## Function Parameters
/// * `event_type`: The type of event to be cleared.
///
/// ## Remarks
/// This will unconditionally remove any events from the queue that match type.
/// If you need to remove a range of event types, use `events.flushGroup()` instead.
///
/// It's also normal to just ignore events you don't care about in your event loop without calling this function.
///
/// This function only affects currently queued events.
/// If you want to make sure that all pending OS events are flushed, you can call `events.pump()` on the main thread immediately before the flush call.
///
/// If you have user events with custom data that needs to be freed,
/// you should use `events.peep()` to remove and clean up those events before calling this function.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn flush(
    event_type: Type,
) void {
    c.SDL_FlushEvent(@intFromEnum(event_type));
}

/// Clear events of a range of types from the event queue.
///
/// ## Function Parameters
/// * `group`: The group of event types to flush from the event queue.
///
/// ## Remarks
/// This will unconditionally remove any events from the queue that are in the range of the category.
/// If you need to remove a single event type, use `events.flush()` instead.
///
/// It's also normal to just ignore events you don't care about in your event loop without calling this function.
///
/// This function only affects currently queued events.
/// If you want to make sure that all pending OS events are flushed, you can call `events.pump()` on the main thread immediately before the flush call.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn flushGroup(
    group: Group,
) void {
    const min, const max = group.minMax();
    c.SDL_FlushEvents(min, max);
}

/// Query the current event filter.
///
/// ## Return Value
/// Returns the current event filter and user data passed to it.
/// This will return `null` if no event filter has been set.
///
/// ## Remarks
/// This function can be used to "chain" filters, by saving the existing filter before replacing it with a function that will call that saved filter.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// This function is available since SDL 3.2.0.
pub fn getFilter() ?struct { FilterC, ?*anyopaque } {
    var event_filter: c.SDL_EventFilter = undefined;
    var user_data: ?*anyopaque = undefined;
    const ret = c.SDL_GetEventFilter(&event_filter, &user_data);
    if (!ret)
        return null;
    return .{ @ptrCast(event_filter), user_data };
}

/// Check for the existence of a certain event type in the event queue.
///
/// ## Function Parameters
/// * `event_type`: The type of event to be queried.
///
/// ## Return Value
/// Returns true if events matching type are present, or false if events matching type are not present.
///
/// ## Remarks
/// If you need to check for a range of event types, use `events.hasGroup()` instead.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
pub fn has(
    event_type: Type,
) bool {
    return c.SDL_HasEvent(@intFromEnum(event_type));
}

/// Check for the existence of certain event types in the event queue.
///
/// ## Function Parameters
/// * `group`: The group to check for if present in the event queue.
///
/// ## Return Value
/// Returns true if events matching the group are present, or false if events matching the group are not present.
///
/// ## Remarks
/// If you need to check for a single event type, use `events.has()` instead.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn hasGroup(
    group: Group,
) bool {
    const min, const max = group.minMax();
    return c.SDL_HasEvents(min, max);
}

/// Check the event queue for messages and optionally return them.
///
/// ## Function Parameters
/// * `events`: Destination slice to store events to.
/// * `action`: Action to take. Note that the `group` option does not apply to `events.Action.add`.
/// * `group`: When peeking or getting events, only consider events in the particular group.
///
/// ## Return Value
/// Returns the number of events actually stored.
///
/// ## Remarks
/// You may have to call `events.pump()` before calling this function.
/// Otherwise, the events may not be ready to be filtered when you call `events.peep()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn peep(
    events: []Event,
    action: Action,
    group: Group,
) !usize {
    const min, const max = group.minMax();

    // Hacky! We ensure in unit tests our enum is the same size or larger so we can do this.
    // If they are of the same size its just in-place conversion if zig events are larger provided buffer will be longer so coping in reverse still works.
    // /-----------------\
    // |  a  |  b  |  c  |  - [*]c.SDL_Events
    // |---------------------------
    // |   a    |   b    |   c    | - []Event
    // \--------------------------/
    // Coping shorter item to longer item can only destroy source item but not any item before.
    const raw: [*]c.SDL_Event = @ptrCast(events.ptr);
    const ret = c.SDL_PeepEvents(raw, @intCast(events.len), @intFromEnum(action), min, max);
    for (0..@intCast(ret)) |ind| {
        const rev_ind = events.len - ind - 1;
        events[rev_ind] = Event.fromSdl(raw[rev_ind]);
    }
    return @intCast(try errors.wrapCall(c_int, ret, -1));
}

/// Check the event queue for messages to see how many there are.
///
/// ## Function Parameters
/// * `num_events`: Max number of events to check for.
/// * `action`: Action to take. Note that the `group` option does not apply to `events.Action.add`.
/// * `group`: When peeking or getting events, only consider events in the particular group.
///
/// ## Return Value
/// Returns the number of events that would be peeked.
///
/// ## Remarks
/// You may have to call `events.pump()` before calling this function.
/// Otherwise, the events may not be ready to be filtered when you call `events.peep()`.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn peepSize(
    num_events: usize,
    action: Action,
    group: Group,
) !usize {
    const min, const max = group.minMax();
    const ret = c.SDL_PeepEvents(null, @intCast(num_events), @intFromEnum(action), min, max);
    return @intCast(try errors.wrapCall(c_int, ret, -1));
}

/// Poll for currently pending events.
///
/// ## Return Value
/// Returns the next event in the queue or `null` if there is none available.
///
/// ## Remarks
/// The next event is removed from the queue and returned.
///
/// As this function may implicitly call `events.pump()`, you can only call this function in the thread that set the video mode.
///
/// `events.poll()` is the favored way of receiving system events since it can be done from the main loop
/// and does not suspend the main loop while waiting on an event to be posted.
///
/// The common practice is to fully process the event queue once every frame, usually as a first step before updating the game's state:
/// ```zig
/// while (game_is_still_running) {
///     while (events.poll()) |event| {  // Poll until all events are handled!
///         // Decide what to do with this event.
///     }
///
///     // Update game state, draw the current frame.
/// }
/// ```
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn poll() ?Event {
    var event: c.SDL_Event = undefined;
    const ret = c.SDL_PollEvent(&event);
    if (!ret)
        return null;
    return Event.fromSdl(event);
}

/// Pump the event loop, gathering events from the input devices.
///
/// ## Remarks
/// This function updates the event queue and internal input device state.
///
/// `events.pump()` gathers all the pending input information from devices and places it in the event queue.
/// Without calls to `events.pump()` no events would ever be placed on the queue.
/// Often the need for calls to `events.pump()` is hidden from the user since `events.poll()` and `events.wait()` implicitly call `events.pump()`.
/// However, if you are not polling or waiting for events (e.g. you are filtering them), then you must call `events.pump()` to force an event queue update.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn pump() void {
    c.SDL_PumpEvents();
}

/// Add an event to the event queue.
///
/// ## Function Parameters
/// * `event`: The event to be added to the queue.
///
/// ## Remarks
/// The event queue can actually be used as a two way communication channel.
/// Not only can events be read from the queue, but the user can also push their own events onto it.
/// The event is copied into the queue.
///
/// Note: Pushing device input events onto the queue doesn't modify the state of the device within SDL.
///
/// Note: Events pushed onto the queue with `events.push()` get passed through the event filter but events added with `events.peep()` do not.
///
/// For pushing application-specific events, please use `events.register()` to get an event type that does not conflict with other code
/// that also wants its own custom event types.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn push(
    event: Event,
) !void {
    var event_umanaged = event.toSdl();
    const ret = c.SDL_PushEvent(&event_umanaged);
    return errors.wrapCallBool(ret);
}

/// Allocate a set of user-defined events, and return the beginning event number for that set of events.
///
/// ## Function Parameters
/// * `num_events`: The number of events to be allocated.
///
/// ## Return Value
/// Returns the beginning event number, or `null` if `num_events` is invalid or if there are not enough user-defined events left.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn register(
    num_events: usize,
) ?c.SDL_EventType {
    const ret = c.SDL_RegisterEvents(@intCast(num_events));
    if (ret == 0)
        return null;
    return ret;
}

/// Remove an event watch callback added with `events.addWatch()`.
///
/// ## Function Parameters
/// * `event_filter`: Function originally passed to `events.addWatch()`.
/// * `user_data`: The user data originally passed to `events.addWatch()`.
///
/// ## Remarks
/// This function takes the same input as `events.addWatch()` to identify and delete the corresponding callback.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn removeWatch(
    event_filter: FilterC,
    user_data: ?*anyopaque,
) void {
    c.SDL_RemoveEventWatch(event_filter, @ptrCast(user_data));
}

/// Set the state of processing events by type.
///
/// ## Function Parameters
/// * `event_type`: The type of event.
/// * `enabled`: Whether to process the event or not.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setEnabled(
    event_type: Type,
    enable: bool,
) void {
    c.SDL_SetEventEnabled(@intFromEnum(event_type), enable);
}

/// Set up a filter to process all events before they are added to the internal event queue.
///
/// ## Function Parameters
/// * `UserData`: User data type.
/// * `event_filter`: A function to call when an event happens.
/// * `user_data`: User data passed to `event_filter`.
///
/// ## Return Value
/// Returns the C callback used.
///
/// If you just want to see events without modifying them or preventing them from being queued, you should use `events.addWatch()` instead.
///
/// If the filter function returns true when called, then the event will be added to the internal queue.
/// If it returns false, then the event will be dropped from the queue, but the internal state will still be updated.
/// This allows selective filtering of dynamically arriving events.
///
/// WARNING: Be very careful of what you do in the event filter function, as it may run in a different thread!
///
/// On platforms that support it, if the quit event is generated by an interrupt signal (e.g. pressing Ctrl-C),
/// it will be delivered to the application at the next event poll.
///
/// Note: Disabled events never make it to the event filter function; see `events.enabled()`.
///
/// Note: Events pushed onto the queue with `events.push()` get passed through the event filter, but events pushed onto the queue with `events.peep()` do not.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
///
/// ## Code Examples
/// TODO!!!
pub fn setFilter(
    comptime UserData: type,
    comptime event_filter: Filter(UserData),
    user_data: ?*UserData,
) FilterC {
    const Cb = struct {
        pub fn run(user_data_c: ?*anyopaque, event_c: [*c]c.SDL_Event) callconv(.c) bool {
            var event = Event.fromSdl(event_c.*);
            const ret = event_filter(@ptrCast(@alignCast(user_data_c)), &event);
            event_c.* = event.toSdl();
            return ret;
        }
    };
    c.SDL_SetEventFilter(Cb.run, @ptrCast(user_data));
    return Cb.run;
}

/// Wait indefinitely for the next available event.
///
/// ## Remarks
/// As this function may implicitly call `events.pump()`, you can only call this function in the thread that initialized the video subsystem.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn wait() !void {
    return errors.wrapCallBool(c.SDL_WaitEvent(null));
}

/// Wait indefinitely for the next available event and pop it from the event queue.
///
/// ## Return Value
/// Returns the event popped.
///
/// ## Remarks
/// The next event is removed from the queue and returned.
///
/// As this function may implicitly call `events.pump()`, you can only call this function in the thread that initialized the video subsystem.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn waitAndPop() !Event {
    var event: c.SDL_Event = undefined;
    const ret = c.SDL_WaitEvent(&event);
    try errors.wrapCallBool(ret);
    return Event.fromSdl(event);
}

/// Wait until the specified timeout (in milliseconds) for the next available event.
///
/// ## Function Parameters
/// * `timeout_millseconds`: The maximum number of milliseconds to wait for the next available event.
///
/// ## Return Value
/// Returns true if an event appeared before the timeout.
///
/// ## Remarks
/// As this function may implicitly call `events.pump()`, you can only call this function in the thread that initialized the video subsystem.
///
/// The timeout is not guaranteed, the actual wait time could be longer due to system scheduling.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn waitTimeout(
    timeout_milliseconds: u31,
) bool {
    return c.SDL_WaitEventTimeout(null, @intCast(timeout_milliseconds));
}

/// Wait until the specified timeout (in milliseconds) for the next available event and pop it.
///
/// ## Function Parameters
/// * `timeout_millseconds`: The maximum number of milliseconds to wait for the next available event.
///
/// ## Return Value
/// If the call times out, then `null` is returned.
/// Returns the event popped from the event queue if event has appeared before the timeout.
///
/// ## Remarks
/// The next event is removed from the queue and returned.
///
/// As this function may implicitly call `events.pump()`, you can only call this function in the thread that initialized the video subsystem.
///
/// The timeout is not guaranteed, the actual wait time could be longer due to system scheduling.
///
/// ## Thread Safety
/// This function should only be called on the main thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn waitAndPopTimeout(
    timeout_milliseconds: u31,
) ?Event {
    var event: c.SDL_Event = undefined;
    const ret = c.SDL_WaitEventTimeout(&event, @intCast(timeout_milliseconds));
    if (!ret)
        return null;
    return Event.fromSdl(event);
}

fn dummyFilter(
    user_data: ?*void,
    event: *Event,
) bool {
    _ = user_data;
    _ = event;
    return true;
}

// Test SDL events.
test "Events" {
    errors.refAllDeclsRecursive(@This());

    defer sdl3.shutdown();
    try sdl3.init(.{ .events = true });
    defer sdl3.quit(.{ .events = true });

    setEnabled(.quit, true);
    try push(.{ .quit = .{ .common = .{ .timestamp = 27 } } });
    pump();
    try std.testing.expect(has(.quit));
    try std.testing.expect(hasGroup(.application));
    try std.testing.expect(available());
    try std.testing.expect(enabled(.quit));

    try std.testing.expect(try peepSize(1, .peek, .application) > 0);
    var buf = [_]Event{undefined};
    _ = try peep(&buf, .peek, .application);

    try std.testing.expect(poll() != null);
    // _ = try wait(false); // This is not deterministic and may hang so don't.
    _ = waitTimeout(1);

    flush(.quit);
    flushGroup(.all);

    const group = Group.application;
    try std.testing.expect(group.eventIn(.quit));
    _ = group.minMax();
    var group_iter = group.iterator();
    while (group_iter.next()) |val| {
        _ = val;
    }

    filter(void, dummyFilter, null);
    const curr_filter = setFilter(void, dummyFilter, null);
    try std.testing.expectEqual(@intFromPtr(curr_filter), @intFromPtr(getFilter().?[0]));

    const watch = try addWatch(void, dummyFilter, null);
    removeWatch(watch, null);

    try std.testing.expect(register(1) != null);
    _ = buf[0].getWindow();
}
