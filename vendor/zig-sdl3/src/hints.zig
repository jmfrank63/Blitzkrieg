const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// A callback used to send notifications of hint value changes.
///
/// ## Function Parameters
/// * `user_data`: User-data passed to `hints.addCallback()`.
/// * `name`: Hint name passed to `hints.addCallback()`. The type can be gathered with the `hints.Type.fromSdl()` function.
/// * `old_value`: The previous hint value.
/// * `new_value`: The new value hint is to be set to.
///
/// ## Remarks
/// This is called an initial time during `hints.addCallback()` with the hint's current value, and then again each time the hint's value changes.
///
/// This callback is fired from whatever thread is setting a new hint value.
/// SDL holds a lock on the hint subsystem when calling this callback.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub fn Callback(comptime UserData: type) type {
    return *const fn (
        user_data: ?*UserData,
        name: [:0]const u8,
        old_value: ?[:0]const u8,
        new_value: ?[:0]const u8,
    ) void;
}

/// A callback used to send notifications of hint value changes (C edition).
///
/// ## Function Parameters
/// * `user_data`: User-data passed to `hints.addCallback()`.
/// * `name`: Hint name passed to `hints.addCallback()`. The type can be gathered with the `hints.Type.fromSdl()` function.
/// * `old_value`: The previous hint value.
/// * `new_value`: The new value hint is to be set to.
///
/// ## Remarks
/// This is called an initial time during `hints.addCallback()` with the hint's current value, and then again each time the hint's value changes.
///
/// This callback is fired from whatever thread is setting a new hint value.
/// SDL holds a lock on the hint subsystem when calling this callback.
///
/// ## Version
/// This datatype is available since SDL 3.2.0.
pub const CallbackC = *const fn (
    user_data_c: ?*anyopaque,
    name_c: [*c]const u8,
    old_value_c: [*c]const u8,
    new_value_c: [*c]const u8,
) callconv(.c) void;

/// An enumeration of hint priorities.
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Priority = enum(c.SDL_HintPriority) {
    default = c.SDL_HINT_DEFAULT,
    normal = c.SDL_HINT_NORMAL,
    override = c.SDL_HINT_OVERRIDE,
};

/// Configuration hints for the library.
/// May or may not be useful depending on the platform.
///
/// See https://wiki.libsdl.org/SDL3/CategoryHints for usage.
///
/// ## Version
/// This enum is provided by zig-sdl3.
pub const Type = enum {
    allow_alt_tab_while_grabbed,
    android_allow_recreate_activity,
    android_block_on_pause,
    android_low_latency_audio,
    android_trap_back_button,
    app_id,
    app_name,
    apple_tv_controller_ui_events,
    apple_tv_remote_allow_rotation,
    audio_alsa_default_device,
    audio_alsa_default_playback_device,
    audio_alsa_default_recording_device,
    audio_category,
    audio_channels,
    audio_device_app_icon_name,
    audio_device_raw_stream,
    audio_device_sample_frames,
    audio_device_stream_name,
    audio_device_stream_role,
    audio_disk_input_file,
    audio_disk_output_file,
    audio_disk_timescale,
    audio_driver,
    audio_dummy_timescale,
    audio_format,
    audio_frequency,
    audio_include_monitors,
    auto_update_joysticks,
    auto_update_sensors,
    bmp_save_legacy_format,
    camera_driver,
    cpu_feature_mask,
    joystick_directinput,
    file_dialog_driver,
    display_usable_bounds,
    emscripten_asyncify,
    emscripten_canvas_selector,
    emscripten_keyboard_element,
    enable_screen_keyboard,
    evdev_devices,
    event_logging,
    force_raisewindow,
    framebuffer_acceleration,
    gamecontrollerconfig,
    gamecontrollerconfig_file,
    gamecontrollertype,
    gamecontroller_ignore_devices,
    gamecontroller_ignore_devices_except,
    gamecontroller_sensor_fusion,
    gdk_textinput_default_text,
    gdk_textinput_description,
    gdk_textinput_max_length,
    gdk_textinput_scope,
    gdk_textinput_title,
    hidapi_libusb,
    hidapi_libusb_whitelist,
    hidapi_udev,
    gpu_driver,
    hidapi_enumerate_only_controllers,
    hidapi_ignore_devices,
    ime_implemented_ui,
    ios_hide_home_indicator,
    joystick_allow_background_events,
    joystick_arcadestick_devices,
    joystick_arcadestick_devices_excluded,
    joystick_blacklist_devices,
    joystick_blacklist_devices_excluded,
    joystick_device,
    joystick_enhanced_reports,
    joystick_flightstick_devices,
    joystick_flightstick_devices_excluded,
    joystick_gameinput,
    joystick_gamecube_devices,
    joystick_gamecube_devices_excluded,
    joystick_hidapi,
    joystick_hidapi_combine_joy_cons,
    joystick_hidapi_gamecube,
    joystick_hidapi_gamecube_rumble_brake,
    joystick_hidapi_joy_cons,
    joystick_hidapi_joycon_home_led,
    joystick_hidapi_luna,
    joystick_hidapi_nintendo_classic,
    joystick_hidapi_ps3,
    joystick_hidapi_ps3_sixaxis_driver,
    joystick_hidapi_ps4,
    joystick_hidapi_ps4_report_interval,
    joystick_hidapi_ps5,
    joystick_hidapi_ps5_player_led,
    joystick_hidapi_shield,
    joystick_hidapi_stadia,
    joystick_hidapi_steam,
    joystick_hidapi_steam_home_led,
    joystick_hidapi_steamdeck,
    joystick_hidapi_steam_hori,
    joystick_hidapi_lg4ff,
    joystick_hidapi_8bitdo,
    joystick_hidapi_sinput,
    joystick_hidapi_flydigi,
    joystick_hidapi_switch,
    joystick_hidapi_switch_home_led,
    joystick_hidapi_switch_player_led,
    joystick_hidapi_vertical_joy_cons,
    joystick_hidapi_wii,
    joystick_hidapi_wii_player_led,
    joystick_hidapi_xbox,
    joystick_hidapi_xbox_360,
    joystick_hidapi_xbox_360_player_led,
    joystick_hidapi_xbox_360_wireless,
    joystick_hidapi_xbox_one,
    joystick_hidapi_xbox_one_home_led,
    joystick_hidapi_gip,
    joystick_hidapi_gip_reset_for_metadata,
    joystick_iokit,
    joystick_linux_classic,
    joystick_linux_deadzones,
    joystick_linux_digital_hats,
    joystick_linux_hat_deadzones,
    joystick_mfi,
    joystick_rawinput,
    joystick_rawinput_correlate_xinput,
    joystick_rog_chakram,
    joystick_thread,
    joystick_throttle_devices,
    joystick_throttle_devices_excluded,
    joystick_wgi,
    joystick_wheel_devices,
    joystick_wheel_devices_excluded,
    joystick_zero_centered_devices,
    joystick_haptic_axes,
    keycode_options,
    kmsdrm_atomic,
    kmsdrm_device_index,
    kmsdrm_require_drm_master,
    logging,
    mac_background_app,
    mac_ctrl_click_emulate_right_click,
    mac_opengl_async_dispatch,
    mac_option_as_alt,
    mac_press_and_hold,
    mac_scroll_momentum,
    main_callback_rate,
    mouse_auto_capture,
    mouse_double_click_radius,
    mouse_double_click_time,
    mouse_default_system_cursor,
    mouse_dpi_scale_cursors,
    mouse_emulate_warp_with_relative,
    mouse_focus_clickthrough,
    mouse_normal_speed_scale,
    mouse_relative_mode_center,
    mouse_relative_speed_scale,
    mouse_relative_system_scale,
    mouse_relative_warp_motion,
    mouse_relative_cursor_visible,
    mouse_touch_events,
    mute_console_keyboard,
    no_signal_handlers,
    opengl_library,
    egl_library,
    opengl_es_driver,
    openvr_library,
    orientations,
    poll_sentinel,
    preferred_locales,
    ps2_gs_height,
    ps2_gs_mode,
    ps2_gs_progressive,
    ps2_gs_width,
    quit_on_last_window_close,
    render_direct3d_threadsafe,
    render_direct3d11_debug,
    render_direct3d11_warp,
    render_vulkan_debug,
    render_gpu_debug,
    render_gpu_low_power,
    render_driver,
    render_line_method,
    render_metal_prefer_low_power_device,
    render_vsync,
    return_key_hides_ime,
    rog_gamepad_mice,
    rog_gamepad_mice_excluded,
    rpi_video_layer,
    screensaver_inhibit_activity_name,
    shutdown_dbus_on_quit,
    storage_title_driver,
    storage_user_driver,
    thread_force_realtime_time_critical,
    thread_priority_policy,
    timer_resolution,
    touch_mouse_events,
    trackpad_is_touch_only,
    tv_remote_as_joystick,
    video_allow_screensaver,
    video_display_priority,
    video_double_buffer,
    video_driver,
    video_dummy_save_frames,
    video_egl_allow_getdisplay_fallback,
    video_force_egl,
    video_mac_fullscreen_spaces,
    video_mac_fullscreen_menu_visibility,
    video_match_exclusive_mode_on_move,
    video_minimize_on_focus_loss,
    video_offscreen_save_frames,
    video_sync_window_operations,
    video_wayland_allow_libdecor,
    video_wayland_mode_emulation,
    video_wayland_mode_scaling,
    video_wayland_prefer_libdecor,
    video_wayland_scale_to_display,
    video_win_d3dcompiler,
    video_x11_external_window_input,
    video_x11_net_wm_bypass_compositor,
    video_x11_net_wm_ping,
    video_x11_nodirectcolor,
    video_x11_scaling_factor,
    video_x11_visualid,
    video_x11_window_visualid,
    video_x11_xrandr,
    vita_enable_back_touch,
    vita_enable_front_touch,
    vita_module_path,
    vita_pvr_init,
    vita_resolution,
    vita_pvr_opengl,
    vita_touch_mouse_device,
    vulkan_display,
    vulkan_library,
    wave_fact_chunk,
    wave_chunk_limit,
    wave_riff_chunk_size,
    wave_truncation,
    window_activate_when_raised,
    window_activate_when_shown,
    window_allow_topmost,
    window_frame_usable_while_cursor_hidden,
    windows_close_on_alt_f4,
    windows_enable_menu_mnemonics,
    windows_enable_messageloop,
    windows_gameinput,
    windows_raw_keyboard,
    windows_raw_keyboard_exclude_hotkeys,
    windows_force_semaphore_kernel,
    windows_intresource_icon,
    windows_intresource_icon_small,
    windows_use_d3d9ex,
    windows_erase_background_mode,
    x11_force_override_redirect,
    x11_window_type,
    x11_xcb_library,
    xinput_enabled,
    assert,
    pen_mouse_events,
    pen_touch_events,
    // debug_logging,

    /// Convert to an SDL value.
    pub fn toSdl(self: Type) [*c]const u8 {
        return switch (self) {
            .allow_alt_tab_while_grabbed => c.SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED,
            .android_allow_recreate_activity => c.SDL_HINT_ANDROID_ALLOW_RECREATE_ACTIVITY,
            .android_block_on_pause => c.SDL_HINT_ANDROID_BLOCK_ON_PAUSE,
            .android_low_latency_audio => c.SDL_HINT_ANDROID_LOW_LATENCY_AUDIO,
            .android_trap_back_button => c.SDL_HINT_ANDROID_TRAP_BACK_BUTTON,
            .app_id => c.SDL_HINT_APP_ID,
            .app_name => c.SDL_HINT_APP_NAME,
            .apple_tv_controller_ui_events => c.SDL_HINT_APPLE_TV_CONTROLLER_UI_EVENTS,
            .apple_tv_remote_allow_rotation => c.SDL_HINT_APPLE_TV_REMOTE_ALLOW_ROTATION,
            .audio_alsa_default_device => c.SDL_HINT_AUDIO_ALSA_DEFAULT_DEVICE,
            .audio_alsa_default_playback_device => c.SDL_HINT_AUDIO_ALSA_DEFAULT_PLAYBACK_DEVICE,
            .audio_alsa_default_recording_device => c.SDL_HINT_AUDIO_ALSA_DEFAULT_RECORDING_DEVICE,
            .audio_category => c.SDL_HINT_AUDIO_CATEGORY,
            .audio_channels => c.SDL_HINT_AUDIO_CHANNELS,
            .audio_device_app_icon_name => c.SDL_HINT_AUDIO_DEVICE_APP_ICON_NAME,
            .audio_device_raw_stream => c.SDL_HINT_AUDIO_DEVICE_RAW_STREAM,
            .audio_device_sample_frames => c.SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES,
            .audio_device_stream_name => c.SDL_HINT_AUDIO_DEVICE_STREAM_NAME,
            .audio_device_stream_role => c.SDL_HINT_AUDIO_DEVICE_STREAM_ROLE,
            .audio_disk_input_file => c.SDL_HINT_AUDIO_DISK_INPUT_FILE,
            .audio_disk_output_file => c.SDL_HINT_AUDIO_DISK_OUTPUT_FILE,
            .audio_disk_timescale => c.SDL_HINT_AUDIO_DISK_TIMESCALE,
            .audio_driver => c.SDL_HINT_AUDIO_DRIVER,
            .audio_dummy_timescale => c.SDL_HINT_AUDIO_DUMMY_TIMESCALE,
            .audio_format => c.SDL_HINT_AUDIO_FORMAT,
            .audio_frequency => c.SDL_HINT_AUDIO_FREQUENCY,
            .audio_include_monitors => c.SDL_HINT_AUDIO_INCLUDE_MONITORS,
            .auto_update_joysticks => c.SDL_HINT_AUTO_UPDATE_JOYSTICKS,
            .auto_update_sensors => c.SDL_HINT_AUTO_UPDATE_SENSORS,
            .bmp_save_legacy_format => c.SDL_HINT_BMP_SAVE_LEGACY_FORMAT,
            .camera_driver => c.SDL_HINT_CAMERA_DRIVER,
            .cpu_feature_mask => c.SDL_HINT_CPU_FEATURE_MASK,
            .joystick_directinput => c.SDL_HINT_JOYSTICK_DIRECTINPUT,
            .file_dialog_driver => c.SDL_HINT_FILE_DIALOG_DRIVER,
            .display_usable_bounds => c.SDL_HINT_DISPLAY_USABLE_BOUNDS,
            .emscripten_asyncify => c.SDL_HINT_EMSCRIPTEN_ASYNCIFY,
            .emscripten_canvas_selector => c.SDL_HINT_EMSCRIPTEN_CANVAS_SELECTOR,
            .emscripten_keyboard_element => c.SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT,
            .enable_screen_keyboard => c.SDL_HINT_ENABLE_SCREEN_KEYBOARD,
            .evdev_devices => c.SDL_HINT_EVDEV_DEVICES,
            .event_logging => c.SDL_HINT_EVENT_LOGGING,
            .force_raisewindow => c.SDL_HINT_FORCE_RAISEWINDOW,
            .framebuffer_acceleration => c.SDL_HINT_FRAMEBUFFER_ACCELERATION,
            .gamecontrollerconfig => c.SDL_HINT_GAMECONTROLLERCONFIG,
            .gamecontrollerconfig_file => c.SDL_HINT_GAMECONTROLLERCONFIG_FILE,
            .gamecontrollertype => c.SDL_HINT_GAMECONTROLLERTYPE,
            .gamecontroller_ignore_devices => c.SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES,
            .gamecontroller_ignore_devices_except => c.SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,
            .gamecontroller_sensor_fusion => c.SDL_HINT_GAMECONTROLLER_SENSOR_FUSION,
            .gdk_textinput_default_text => c.SDL_HINT_GDK_TEXTINPUT_DEFAULT_TEXT,
            .gdk_textinput_description => c.SDL_HINT_GDK_TEXTINPUT_DESCRIPTION,
            .gdk_textinput_max_length => c.SDL_HINT_GDK_TEXTINPUT_MAX_LENGTH,
            .gdk_textinput_scope => c.SDL_HINT_GDK_TEXTINPUT_SCOPE,
            .gdk_textinput_title => c.SDL_HINT_GDK_TEXTINPUT_TITLE,
            .hidapi_libusb => c.SDL_HINT_HIDAPI_LIBUSB,
            .hidapi_libusb_whitelist => c.SDL_HINT_HIDAPI_LIBUSB_WHITELIST,
            .hidapi_udev => c.SDL_HINT_HIDAPI_UDEV,
            .gpu_driver => c.SDL_HINT_GPU_DRIVER,
            .hidapi_enumerate_only_controllers => c.SDL_HINT_HIDAPI_ENUMERATE_ONLY_CONTROLLERS,
            .hidapi_ignore_devices => c.SDL_HINT_HIDAPI_IGNORE_DEVICES,
            .ime_implemented_ui => c.SDL_HINT_IME_IMPLEMENTED_UI,
            .ios_hide_home_indicator => c.SDL_HINT_IOS_HIDE_HOME_INDICATOR,
            .joystick_allow_background_events => c.SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
            .joystick_arcadestick_devices => c.SDL_HINT_JOYSTICK_ARCADESTICK_DEVICES,
            .joystick_arcadestick_devices_excluded => c.SDL_HINT_JOYSTICK_ARCADESTICK_DEVICES_EXCLUDED,
            .joystick_blacklist_devices => c.SDL_HINT_JOYSTICK_BLACKLIST_DEVICES,
            .joystick_blacklist_devices_excluded => c.SDL_HINT_JOYSTICK_BLACKLIST_DEVICES_EXCLUDED,
            .joystick_device => c.SDL_HINT_JOYSTICK_DEVICE,
            .joystick_enhanced_reports => c.SDL_HINT_JOYSTICK_ENHANCED_REPORTS,
            .joystick_flightstick_devices => c.SDL_HINT_JOYSTICK_FLIGHTSTICK_DEVICES,
            .joystick_flightstick_devices_excluded => c.SDL_HINT_JOYSTICK_FLIGHTSTICK_DEVICES_EXCLUDED,
            .joystick_gameinput => c.SDL_HINT_JOYSTICK_GAMEINPUT,
            .joystick_gamecube_devices => c.SDL_HINT_JOYSTICK_GAMECUBE_DEVICES,
            .joystick_gamecube_devices_excluded => c.SDL_HINT_JOYSTICK_GAMECUBE_DEVICES_EXCLUDED,
            .joystick_hidapi => c.SDL_HINT_JOYSTICK_HIDAPI,
            .joystick_hidapi_combine_joy_cons => c.SDL_HINT_JOYSTICK_HIDAPI_COMBINE_JOY_CONS,
            .joystick_hidapi_gamecube => c.SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE,
            .joystick_hidapi_gamecube_rumble_brake => c.SDL_HINT_JOYSTICK_HIDAPI_GAMECUBE_RUMBLE_BRAKE,
            .joystick_hidapi_joy_cons => c.SDL_HINT_JOYSTICK_HIDAPI_JOY_CONS,
            .joystick_hidapi_joycon_home_led => c.SDL_HINT_JOYSTICK_HIDAPI_JOYCON_HOME_LED,
            .joystick_hidapi_luna => c.SDL_HINT_JOYSTICK_HIDAPI_LUNA,
            .joystick_hidapi_nintendo_classic => c.SDL_HINT_JOYSTICK_HIDAPI_NINTENDO_CLASSIC,
            .joystick_hidapi_ps3 => c.SDL_HINT_JOYSTICK_HIDAPI_PS3,
            .joystick_hidapi_ps3_sixaxis_driver => c.SDL_HINT_JOYSTICK_HIDAPI_PS3_SIXAXIS_DRIVER,
            .joystick_hidapi_ps4 => c.SDL_HINT_JOYSTICK_HIDAPI_PS4,
            .joystick_hidapi_ps4_report_interval => c.SDL_HINT_JOYSTICK_HIDAPI_PS4_REPORT_INTERVAL,
            .joystick_hidapi_ps5 => c.SDL_HINT_JOYSTICK_HIDAPI_PS5,
            .joystick_hidapi_ps5_player_led => c.SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED,
            .joystick_hidapi_shield => c.SDL_HINT_JOYSTICK_HIDAPI_SHIELD,
            .joystick_hidapi_stadia => c.SDL_HINT_JOYSTICK_HIDAPI_STADIA,
            .joystick_hidapi_steam => c.SDL_HINT_JOYSTICK_HIDAPI_STEAM,
            .joystick_hidapi_steam_home_led => c.SDL_HINT_JOYSTICK_HIDAPI_STEAM_HOME_LED,
            .joystick_hidapi_steamdeck => c.SDL_HINT_JOYSTICK_HIDAPI_STEAMDECK,
            .joystick_hidapi_steam_hori => c.SDL_HINT_JOYSTICK_HIDAPI_STEAM_HORI,
            .joystick_hidapi_lg4ff => c.SDL_HINT_JOYSTICK_HIDAPI_LG4FF,
            .joystick_hidapi_8bitdo => c.SDL_HINT_JOYSTICK_HIDAPI_8BITDO,
            .joystick_hidapi_sinput => c.SDL_HINT_JOYSTICK_HIDAPI_SINPUT,
            .joystick_hidapi_flydigi => c.SDL_HINT_JOYSTICK_HIDAPI_FLYDIGI,
            .joystick_hidapi_switch => c.SDL_HINT_JOYSTICK_HIDAPI_SWITCH,
            .joystick_hidapi_switch_home_led => c.SDL_HINT_JOYSTICK_HIDAPI_SWITCH_HOME_LED,
            .joystick_hidapi_switch_player_led => c.SDL_HINT_JOYSTICK_HIDAPI_SWITCH_PLAYER_LED,
            .joystick_hidapi_vertical_joy_cons => c.SDL_HINT_JOYSTICK_HIDAPI_VERTICAL_JOY_CONS,
            .joystick_hidapi_wii => c.SDL_HINT_JOYSTICK_HIDAPI_WII,
            .joystick_hidapi_wii_player_led => c.SDL_HINT_JOYSTICK_HIDAPI_WII_PLAYER_LED,
            .joystick_hidapi_xbox => c.SDL_HINT_JOYSTICK_HIDAPI_XBOX,
            .joystick_hidapi_xbox_360 => c.SDL_HINT_JOYSTICK_HIDAPI_XBOX_360,
            .joystick_hidapi_xbox_360_player_led => c.SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_PLAYER_LED,
            .joystick_hidapi_xbox_360_wireless => c.SDL_HINT_JOYSTICK_HIDAPI_XBOX_360_WIRELESS,
            .joystick_hidapi_xbox_one => c.SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE,
            .joystick_hidapi_xbox_one_home_led => c.SDL_HINT_JOYSTICK_HIDAPI_XBOX_ONE_HOME_LED,
            .joystick_hidapi_gip => c.SDL_HINT_JOYSTICK_HIDAPI_GIP,
            .joystick_hidapi_gip_reset_for_metadata => c.SDL_HINT_JOYSTICK_HIDAPI_GIP_RESET_FOR_METADATA,
            .joystick_iokit => c.SDL_HINT_JOYSTICK_IOKIT,
            .joystick_linux_classic => c.SDL_HINT_JOYSTICK_LINUX_CLASSIC,
            .joystick_linux_deadzones => c.SDL_HINT_JOYSTICK_LINUX_DEADZONES,
            .joystick_linux_digital_hats => c.SDL_HINT_JOYSTICK_LINUX_DIGITAL_HATS,
            .joystick_linux_hat_deadzones => c.SDL_HINT_JOYSTICK_LINUX_HAT_DEADZONES,
            .joystick_mfi => c.SDL_HINT_JOYSTICK_MFI,
            .joystick_rawinput => c.SDL_HINT_JOYSTICK_RAWINPUT,
            .joystick_rawinput_correlate_xinput => c.SDL_HINT_JOYSTICK_RAWINPUT_CORRELATE_XINPUT,
            .joystick_rog_chakram => c.SDL_HINT_JOYSTICK_ROG_CHAKRAM,
            .joystick_thread => c.SDL_HINT_JOYSTICK_THREAD,
            .joystick_throttle_devices => c.SDL_HINT_JOYSTICK_THROTTLE_DEVICES,
            .joystick_throttle_devices_excluded => c.SDL_HINT_JOYSTICK_THROTTLE_DEVICES_EXCLUDED,
            .joystick_wgi => c.SDL_HINT_JOYSTICK_WGI,
            .joystick_wheel_devices => c.SDL_HINT_JOYSTICK_WHEEL_DEVICES,
            .joystick_wheel_devices_excluded => c.SDL_HINT_JOYSTICK_WHEEL_DEVICES_EXCLUDED,
            .joystick_zero_centered_devices => c.SDL_HINT_JOYSTICK_ZERO_CENTERED_DEVICES,
            .joystick_haptic_axes => c.SDL_HINT_JOYSTICK_HAPTIC_AXES,
            .keycode_options => c.SDL_HINT_KEYCODE_OPTIONS,
            .kmsdrm_atomic => c.SDL_HINT_KMSDRM_ATOMIC,
            .kmsdrm_device_index => c.SDL_HINT_KMSDRM_DEVICE_INDEX,
            .kmsdrm_require_drm_master => c.SDL_HINT_KMSDRM_REQUIRE_DRM_MASTER,
            .logging => c.SDL_HINT_LOGGING,
            .mac_background_app => c.SDL_HINT_MAC_BACKGROUND_APP,
            .mac_ctrl_click_emulate_right_click => c.SDL_HINT_MAC_CTRL_CLICK_EMULATE_RIGHT_CLICK,
            .mac_opengl_async_dispatch => c.SDL_HINT_MAC_OPENGL_ASYNC_DISPATCH,
            .mac_option_as_alt => c.SDL_HINT_MAC_OPTION_AS_ALT,
            .mac_press_and_hold => c.SDL_HINT_MAC_PRESS_AND_HOLD,
            .mac_scroll_momentum => c.SDL_HINT_MAC_SCROLL_MOMENTUM,
            .main_callback_rate => c.SDL_HINT_MAIN_CALLBACK_RATE,
            .mouse_auto_capture => c.SDL_HINT_MOUSE_AUTO_CAPTURE,
            .mouse_double_click_radius => c.SDL_HINT_MOUSE_DOUBLE_CLICK_RADIUS,
            .mouse_double_click_time => c.SDL_HINT_MOUSE_DOUBLE_CLICK_TIME,
            .mouse_default_system_cursor => c.SDL_HINT_MOUSE_DEFAULT_SYSTEM_CURSOR,
            .mouse_dpi_scale_cursors => c.SDL_HINT_MOUSE_DPI_SCALE_CURSORS,
            .mouse_emulate_warp_with_relative => c.SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE,
            .mouse_focus_clickthrough => c.SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH,
            .mouse_normal_speed_scale => c.SDL_HINT_MOUSE_NORMAL_SPEED_SCALE,
            .mouse_relative_mode_center => c.SDL_HINT_MOUSE_RELATIVE_MODE_CENTER,
            .mouse_relative_speed_scale => c.SDL_HINT_MOUSE_RELATIVE_SPEED_SCALE,
            .mouse_relative_system_scale => c.SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE,
            .mouse_relative_warp_motion => c.SDL_HINT_MOUSE_RELATIVE_WARP_MOTION,
            .mouse_relative_cursor_visible => c.SDL_HINT_MOUSE_RELATIVE_CURSOR_VISIBLE,
            .mouse_touch_events => c.SDL_HINT_MOUSE_TOUCH_EVENTS,
            .mute_console_keyboard => c.SDL_HINT_MUTE_CONSOLE_KEYBOARD,
            .no_signal_handlers => c.SDL_HINT_NO_SIGNAL_HANDLERS,
            .opengl_library => c.SDL_HINT_OPENGL_LIBRARY,
            .egl_library => c.SDL_HINT_EGL_LIBRARY,
            .opengl_es_driver => c.SDL_HINT_OPENGL_ES_DRIVER,
            .openvr_library => c.SDL_HINT_OPENVR_LIBRARY,
            .orientations => c.SDL_HINT_ORIENTATIONS,
            .poll_sentinel => c.SDL_HINT_POLL_SENTINEL,
            .preferred_locales => c.SDL_HINT_PREFERRED_LOCALES,
            .ps2_gs_height => c.SDL_HINT_PS2_GS_HEIGHT,
            .ps2_gs_mode => c.SDL_HINT_PS2_GS_MODE,
            .ps2_gs_progressive => c.SDL_HINT_PS2_GS_PROGRESSIVE,
            .ps2_gs_width => c.SDL_HINT_PS2_GS_WIDTH,
            .quit_on_last_window_close => c.SDL_HINT_QUIT_ON_LAST_WINDOW_CLOSE,
            .render_direct3d_threadsafe => c.SDL_HINT_RENDER_DIRECT3D_THREADSAFE,
            .render_direct3d11_debug => c.SDL_HINT_RENDER_DIRECT3D11_DEBUG,
            .render_direct3d11_warp => c.SDL_HINT_RENDER_DIRECT3D11_WARP,
            .render_vulkan_debug => c.SDL_HINT_RENDER_VULKAN_DEBUG,
            .render_gpu_debug => c.SDL_HINT_RENDER_GPU_DEBUG,
            .render_gpu_low_power => c.SDL_HINT_RENDER_GPU_LOW_POWER,
            .render_driver => c.SDL_HINT_RENDER_DRIVER,
            .render_line_method => c.SDL_HINT_RENDER_LINE_METHOD,
            .render_metal_prefer_low_power_device => c.SDL_HINT_RENDER_METAL_PREFER_LOW_POWER_DEVICE,
            .render_vsync => c.SDL_HINT_RENDER_VSYNC,
            .return_key_hides_ime => c.SDL_HINT_RETURN_KEY_HIDES_IME,
            .rog_gamepad_mice => c.SDL_HINT_ROG_GAMEPAD_MICE,
            .rog_gamepad_mice_excluded => c.SDL_HINT_ROG_GAMEPAD_MICE_EXCLUDED,
            .rpi_video_layer => c.SDL_HINT_RPI_VIDEO_LAYER,
            .screensaver_inhibit_activity_name => c.SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME,
            .shutdown_dbus_on_quit => c.SDL_HINT_SHUTDOWN_DBUS_ON_QUIT,
            .storage_title_driver => c.SDL_HINT_STORAGE_TITLE_DRIVER,
            .storage_user_driver => c.SDL_HINT_STORAGE_USER_DRIVER,
            .thread_force_realtime_time_critical => c.SDL_HINT_THREAD_FORCE_REALTIME_TIME_CRITICAL,
            .thread_priority_policy => c.SDL_HINT_THREAD_PRIORITY_POLICY,
            .timer_resolution => c.SDL_HINT_TIMER_RESOLUTION,
            .touch_mouse_events => c.SDL_HINT_TOUCH_MOUSE_EVENTS,
            .trackpad_is_touch_only => c.SDL_HINT_TRACKPAD_IS_TOUCH_ONLY,
            .tv_remote_as_joystick => c.SDL_HINT_TV_REMOTE_AS_JOYSTICK,
            .video_allow_screensaver => c.SDL_HINT_VIDEO_ALLOW_SCREENSAVER,
            .video_display_priority => c.SDL_HINT_VIDEO_DISPLAY_PRIORITY,
            .video_double_buffer => c.SDL_HINT_VIDEO_DOUBLE_BUFFER,
            .video_driver => c.SDL_HINT_VIDEO_DRIVER,
            .video_dummy_save_frames => c.SDL_HINT_VIDEO_DUMMY_SAVE_FRAMES,
            .video_egl_allow_getdisplay_fallback => c.SDL_HINT_VIDEO_EGL_ALLOW_GETDISPLAY_FALLBACK,
            .video_force_egl => c.SDL_HINT_VIDEO_FORCE_EGL,
            .video_mac_fullscreen_spaces => c.SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES,
            .video_mac_fullscreen_menu_visibility => c.SDL_HINT_VIDEO_MAC_FULLSCREEN_MENU_VISIBILITY,
            .video_match_exclusive_mode_on_move => c.SDL_HINT_VIDEO_MATCH_EXCLUSIVE_MODE_ON_MOVE,
            .video_minimize_on_focus_loss => c.SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,
            .video_offscreen_save_frames => c.SDL_HINT_VIDEO_OFFSCREEN_SAVE_FRAMES,
            .video_sync_window_operations => c.SDL_HINT_VIDEO_SYNC_WINDOW_OPERATIONS,
            .video_wayland_allow_libdecor => c.SDL_HINT_VIDEO_WAYLAND_ALLOW_LIBDECOR,
            .video_wayland_mode_emulation => c.SDL_HINT_VIDEO_WAYLAND_MODE_EMULATION,
            .video_wayland_mode_scaling => c.SDL_HINT_VIDEO_WAYLAND_MODE_SCALING,
            .video_wayland_prefer_libdecor => c.SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR,
            .video_wayland_scale_to_display => c.SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY,
            .video_win_d3dcompiler => c.SDL_HINT_VIDEO_WIN_D3DCOMPILER,
            .video_x11_external_window_input => c.SDL_HINT_VIDEO_X11_EXTERNAL_WINDOW_INPUT,
            .video_x11_net_wm_bypass_compositor => c.SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR,
            .video_x11_net_wm_ping => c.SDL_HINT_VIDEO_X11_NET_WM_PING,
            .video_x11_nodirectcolor => c.SDL_HINT_VIDEO_X11_NODIRECTCOLOR,
            .video_x11_scaling_factor => c.SDL_HINT_VIDEO_X11_SCALING_FACTOR,
            .video_x11_visualid => c.SDL_HINT_VIDEO_X11_VISUALID,
            .video_x11_window_visualid => c.SDL_HINT_VIDEO_X11_WINDOW_VISUALID,
            .video_x11_xrandr => c.SDL_HINT_VIDEO_X11_XRANDR,
            .vita_enable_back_touch => c.SDL_HINT_VITA_ENABLE_BACK_TOUCH,
            .vita_enable_front_touch => c.SDL_HINT_VITA_ENABLE_FRONT_TOUCH,
            .vita_module_path => c.SDL_HINT_VITA_MODULE_PATH,
            .vita_pvr_init => c.SDL_HINT_VITA_PVR_INIT,
            .vita_resolution => c.SDL_HINT_VITA_RESOLUTION,
            .vita_pvr_opengl => c.SDL_HINT_VITA_PVR_OPENGL,
            .vita_touch_mouse_device => c.SDL_HINT_VITA_TOUCH_MOUSE_DEVICE,
            .vulkan_display => c.SDL_HINT_VULKAN_DISPLAY,
            .vulkan_library => c.SDL_HINT_VULKAN_LIBRARY,
            .wave_fact_chunk => c.SDL_HINT_WAVE_FACT_CHUNK,
            .wave_chunk_limit => c.SDL_HINT_WAVE_CHUNK_LIMIT,
            .wave_riff_chunk_size => c.SDL_HINT_WAVE_RIFF_CHUNK_SIZE,
            .wave_truncation => c.SDL_HINT_WAVE_TRUNCATION,
            .window_activate_when_raised => c.SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED,
            .window_activate_when_shown => c.SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN,
            .window_allow_topmost => c.SDL_HINT_WINDOW_ALLOW_TOPMOST,
            .window_frame_usable_while_cursor_hidden => c.SDL_HINT_WINDOW_FRAME_USABLE_WHILE_CURSOR_HIDDEN,
            .windows_close_on_alt_f4 => c.SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4,
            .windows_enable_menu_mnemonics => c.SDL_HINT_WINDOWS_ENABLE_MENU_MNEMONICS,
            .windows_enable_messageloop => c.SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP,
            .windows_gameinput => c.SDL_HINT_WINDOWS_GAMEINPUT,
            .windows_raw_keyboard => c.SDL_HINT_WINDOWS_RAW_KEYBOARD,
            .windows_raw_keyboard_exclude_hotkeys => c.SDL_HINT_WINDOWS_RAW_KEYBOARD_EXCLUDE_HOTKEYS,
            .windows_force_semaphore_kernel => c.SDL_HINT_WINDOWS_FORCE_SEMAPHORE_KERNEL,
            .windows_intresource_icon => c.SDL_HINT_WINDOWS_INTRESOURCE_ICON,
            .windows_intresource_icon_small => c.SDL_HINT_WINDOWS_INTRESOURCE_ICON_SMALL,
            .windows_use_d3d9ex => c.SDL_HINT_WINDOWS_USE_D3D9EX,
            .windows_erase_background_mode => c.SDL_HINT_WINDOWS_ERASE_BACKGROUND_MODE,
            .x11_force_override_redirect => c.SDL_HINT_X11_FORCE_OVERRIDE_REDIRECT,
            .x11_window_type => c.SDL_HINT_X11_WINDOW_TYPE,
            .x11_xcb_library => c.SDL_HINT_X11_XCB_LIBRARY,
            .xinput_enabled => c.SDL_HINT_XINPUT_ENABLED,
            .assert => c.SDL_HINT_ASSERT,
            .pen_mouse_events => c.SDL_HINT_PEN_MOUSE_EVENTS,
            .pen_touch_events => c.SDL_HINT_PEN_TOUCH_EVENTS,
            // .debug_logging => c.SDL_HINT_DEBUG_LOGGING,
        };
    }
};

/// Add a function to watch a particular hint.
///
/// ## Function Parameters
/// * `hint`: Hint to watch.
/// * `UserData`: Type for callback user data.
/// * `callback`: An `hints.Callback` function that will be called when the hint value changes.
/// * `user_data`: A pointer to pass to the callback function.
///
/// ## Return Value
/// Returns the callback to be used later by `hints.removeCallback()` if desired.
///
/// ## Remarks
/// The callback function is called *during* this function, to provide it an initial value, and again each time the hint's value changes.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn addCallback(
    hint: Type,
    comptime UserData: type,
    comptime callback: Callback(UserData),
    user_data: ?*UserData,
) !CallbackC {
    const Cb = struct {
        fn run(
            user_data_c: ?*anyopaque,
            name_c: [*c]const u8,
            old_value_c: [*c]const u8,
            new_value_c: [*c]const u8,
        ) callconv(.c) void {
            callback(@ptrCast(@alignCast(user_data_c)), std.mem.span(name_c), if (old_value_c) |val| std.mem.span(val) else null, if (new_value_c) |val| std.mem.span(val) else null);
        }
    };
    const ret = c.SDL_AddHintCallback(
        hint.toSdl(),
        Cb.run,
        @ptrCast(user_data),
    );
    try errors.wrapCallBool(ret);
    return Cb.run;
}

/// Get the value of a hint.
///
/// ## Function Parameters
/// * `hint`: The hint to query.
///
/// ## Return Value
/// Returns the string value of a hint or `null` if the hint isn't set.
///
/// ## Thread Safety
/// It is safe to call this function from any thread, however the return value only remains valid until the hint is changed;
/// if another thread might do so, the app should supply locks and/or make a copy of the string.
/// Note that using a hint callback instead is always thread-safe, as SDL holds a lock on the thread subsystem during the callback.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn get(
    hint: Type,
) ?[:0]const u8 {
    const ret = c.SDL_GetHint(
        hint.toSdl(),
    );
    if (ret == null)
        return null;
    return std.mem.span(ret);
}

/// Get the boolean value of a hint variable.
///
/// ## Function Parameters
/// * `hint`: The hint to query.
///
/// ## Return Value
/// Returns the boolean value of a hint or `null` if the hint does not exist.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn getBoolean(
    hint: Type,
) ?bool {
    const ret = c.SDL_GetHintBoolean(
        hint.toSdl(),
        false,
    );
    if (get(hint) == null) return null;
    return ret;
}

/// Remove a function watching a particular hint.
///
/// ## Function Parameters
/// * `hint`: The hint to watch.
/// * `callback`: A `hint.Callback` function that will be called when the hint value changes.
/// * `user_data`: A pointer being passed to the callback function.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn removeCallback(
    hint: Type,
    callback: CallbackC,
    user_data: ?*anyopaque,
) void {
    c.SDL_RemoveHintCallback(
        hint.toSdl(),
        callback,
        user_data,
    );
}

/// Reset a hint to the default value.
///
/// ## Function Parameters
/// * `hint`: The hint to reset.
///
/// ## Remarks
/// This will reset a hint to the value of the environment variable, or `null` if the environment isn't set.
/// Callbacks will be called normally with this change.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn reset(
    hint: Type,
) !void {
    const ret = c.SDL_ResetHint(
        hint.toSdl(),
    );
    return errors.wrapCallBool(ret);
}

/// Reset all hints to the default values.
///
/// ## Remarks
/// This will reset all hints to the value of the associated environment variable, or `null` if the environment isn't set.
/// Callbacks will be called normally with this change.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn resetAll() void {
    c.SDL_ResetHints();
}

/// Set a hint with normal priority.
///
/// ## Function Parameters
/// * `hint`: The hint to set.
/// * `value:` The value of the hint variable.
///
/// ## Remarks
/// Hints will not be set if there is an existing override hint or environment variable that takes precedence.
/// You can use `hints.setWithPriority()` to set the hint with override priority instead.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
///
/// ## Code Examples
/// TODO!!!
pub fn set(
    hint: Type,
    value: [:0]const u8,
) !void {
    const ret = c.SDL_SetHint(
        hint.toSdl(),
        value.ptr,
    );
    return errors.wrapCallBool(ret);
}

/// Set a hint with a specific priority.
///
/// ## Function Parameters
/// * `hint`: The hint to set.
/// * `value:` The value of the hint variable.
/// * `priority`: The `hint.Priority` level for the hint.
///
/// ## Remarks
/// The priority controls the behavior when setting a hint that already has a value.
/// Hints will replace existing hints of their priority and lower.
/// Environment variables are considered to have override priority.
///
/// ## Thread Safety
/// It is safe to call this function from any thread.
///
/// ## Version
/// This function is available since SDL 3.2.0.
pub fn setWithPriority(
    hint: Type,
    value: [:0]const u8,
    priority: Priority,
) !void {
    const ret = c.SDL_SetHintWithPriority(
        hint.toSdl(),
        value.ptr,
        @intFromEnum(priority),
    );
    return errors.wrapCallBool(ret);
}

fn testHintCb(user_data: ?*i32, name: [:0]const u8, old_value: ?[:0]const u8, new_value: ?[:0]const u8) void {
    const ctr_ptr = user_data.?;
    _ = name;
    _ = old_value;
    _ = new_value;
    ctr_ptr.* = ctr_ptr.* + 1;
}

// Test hint functions.
test "Hints" {
    errors.refAllDeclsRecursive(@This());

    var ctr: i32 = 0;
    const cb = try addCallback(.app_name, i32, testHintCb, &ctr);
    try std.testing.expectEqual(1, ctr);
    try std.testing.expectEqual(null, get(.app_name));
    try std.testing.expectEqual(null, getBoolean(.app_name));
    try set(.app_name, "True");
    try std.testing.expectEqual(2, ctr);
    try std.testing.expectEqualStrings("True", get(.app_name).?);
    try std.testing.expectEqual(true, getBoolean(.app_name));
    try setWithPriority(.app_name, "False", .override);
    try std.testing.expectEqual(3, ctr);
    try std.testing.expectEqualStrings("False", get(.app_name).?);
    try std.testing.expectEqual(false, getBoolean(.app_name));
    try reset(.app_name);
    try std.testing.expectEqual(4, ctr);
    try std.testing.expectEqual(null, get(.app_name));
    try std.testing.expectEqual(null, getBoolean(.app_name));
    try set(.app_name, "Reset Again");
    removeCallback(.app_name, cb, &ctr);
    resetAll();
    try std.testing.expectEqual(5, ctr);
    try std.testing.expectEqual(null, get(.app_name));
    try std.testing.expectEqual(null, getBoolean(.app_name));
}
