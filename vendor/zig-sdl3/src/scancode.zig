const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

/// The SDL keyboard scancode representation.
///
/// ## Remarks
/// An SDL scancode is the physical representation of a key on the keyboard, independent of language and keyboard mapping.
///
/// Values of this type are used to represent keyboard keys, among other places in the scancode field of the `events.Keyboard` structure.
///
/// The values in this enumeration are based on the USB usage page standard: https://usb.org/sites/default/files/hut1_5.pdf
///
/// ## Version
/// This enum is available since SDL 3.2.0.
pub const Scancode = enum(c.SDL_Scancode) {
    a = c.SDL_SCANCODE_A,
    b = c.SDL_SCANCODE_B,
    c = c.SDL_SCANCODE_C,
    d = c.SDL_SCANCODE_D,
    e = c.SDL_SCANCODE_E,
    f = c.SDL_SCANCODE_F,
    g = c.SDL_SCANCODE_G,
    h = c.SDL_SCANCODE_H,
    i = c.SDL_SCANCODE_I,
    j = c.SDL_SCANCODE_J,
    k = c.SDL_SCANCODE_K,
    l = c.SDL_SCANCODE_L,
    m = c.SDL_SCANCODE_M,
    n = c.SDL_SCANCODE_N,
    o = c.SDL_SCANCODE_O,
    p = c.SDL_SCANCODE_P,
    q = c.SDL_SCANCODE_Q,
    r = c.SDL_SCANCODE_R,
    s = c.SDL_SCANCODE_S,
    t = c.SDL_SCANCODE_T,
    u = c.SDL_SCANCODE_U,
    v = c.SDL_SCANCODE_V,
    w = c.SDL_SCANCODE_W,
    x = c.SDL_SCANCODE_X,
    y = c.SDL_SCANCODE_Y,
    z = c.SDL_SCANCODE_Z,
    one = c.SDL_SCANCODE_1,
    two = c.SDL_SCANCODE_2,
    three = c.SDL_SCANCODE_3,
    four = c.SDL_SCANCODE_4,
    five = c.SDL_SCANCODE_5,
    six = c.SDL_SCANCODE_6,
    seven = c.SDL_SCANCODE_7,
    eight = c.SDL_SCANCODE_8,
    nine = c.SDL_SCANCODE_9,
    zero = c.SDL_SCANCODE_0,
    return_key = c.SDL_SCANCODE_RETURN,
    escape = c.SDL_SCANCODE_ESCAPE,
    backspace = c.SDL_SCANCODE_BACKSPACE,
    tab = c.SDL_SCANCODE_TAB,
    space = c.SDL_SCANCODE_SPACE,
    minus = c.SDL_SCANCODE_MINUS,
    equals = c.SDL_SCANCODE_EQUALS,
    left_bracket = c.SDL_SCANCODE_LEFTBRACKET,
    right_bracket = c.SDL_SCANCODE_RIGHTBRACKET,

    /// Located at the lower left of the return key on ISO keyboards and at the right end of the QWERTY row on ANSI keyboards.
    /// Produces REVERSE SOLIDUS (backslash) and VERTICAL LINE in a US layout, REVERSE SOLIDUS and VERTICAL LINE in a UK Mac layout,
    /// NUMBER SIGN and TILDE in a UK Windows layout, DOLLAR SIGN and POUND SIGN in a Swiss German layout, NUMBER SIGN and APOSTROPHE in a German layout,
    /// GRAVE ACCENT and POUND SIGN in a French Mac layout, and ASTERISK and MICRO SIGN in a French Windows layout.
    backslash = c.SDL_SCANCODE_BACKSLASH,

    /// ISO USB keyboards actually use this code instead of 49 for the same key, but all OSes I've seen treat the two codes identically.
    /// So, as an implementor, unless your keyboard generates both of those codes and your OS treats them differently,
    /// you should generate SDL_SCANCODE_BACKSLASH instead of this code.
    /// As a user, you should not rely on this code because SDL will never generate it with most (all?) keyboards.
    non_us_hash = c.SDL_SCANCODE_NONUSHASH,
    semicolon = c.SDL_SCANCODE_SEMICOLON,
    apostrophe = c.SDL_SCANCODE_APOSTROPHE,

    /// Located in the top left corner (on both ANSI and ISO keyboards).
    /// Produces GRAVE ACCENT and TILDE in a US Windows layout and in US and UK Mac layouts on ANSI keyboards, GRAVE ACCENT and NOT SIGN in a UK Windows layout,
    /// SECTION SIGN and PLUS-MINUS SIGN in US and UK Mac layouts on ISO keyboards, SECTION SIGN and DEGREE SIGN in a Swiss German layout (Mac: only on ISO keyboards),
    /// CIRCUMFLEX ACCENT and DEGREE SIGN in a German layout (Mac: only on ISO keyboards), SUPERSCRIPT TWO and TILDE in a French Windows layout,
    /// COMMERCIAL AT and NUMBER SIGN in a French Mac layout on ISO keyboards, and LESS-THAN SIGN and GREATER-THAN SIGN in a Swiss German, German,
    /// or French Mac layout on ANSI keyboards.
    grave = c.SDL_SCANCODE_GRAVE,
    comma = c.SDL_SCANCODE_COMMA,
    period = c.SDL_SCANCODE_PERIOD,
    slash = c.SDL_SCANCODE_SLASH,
    caps_lock = c.SDL_SCANCODE_CAPSLOCK,
    func1 = c.SDL_SCANCODE_F1,
    func2 = c.SDL_SCANCODE_F2,
    func3 = c.SDL_SCANCODE_F3,
    func4 = c.SDL_SCANCODE_F4,
    func5 = c.SDL_SCANCODE_F5,
    func6 = c.SDL_SCANCODE_F6,
    func7 = c.SDL_SCANCODE_F7,
    func8 = c.SDL_SCANCODE_F8,
    func9 = c.SDL_SCANCODE_F9,
    func10 = c.SDL_SCANCODE_F10,
    func11 = c.SDL_SCANCODE_F11,
    func12 = c.SDL_SCANCODE_F12,
    print_screen = c.SDL_SCANCODE_PRINTSCREEN,
    scroll_lock = c.SDL_SCANCODE_SCROLLLOCK,
    pause = c.SDL_SCANCODE_PAUSE,

    /// Insert on PC, help on some Mac keyboards (but does send code 73, not 117).
    insert = c.SDL_SCANCODE_INSERT,
    home = c.SDL_SCANCODE_HOME,
    pageup = c.SDL_SCANCODE_PAGEUP,
    delete = c.SDL_SCANCODE_DELETE,
    end = c.SDL_SCANCODE_END,
    pagedown = c.SDL_SCANCODE_PAGEDOWN,
    right = c.SDL_SCANCODE_RIGHT,
    left = c.SDL_SCANCODE_LEFT,
    down = c.SDL_SCANCODE_DOWN,
    up = c.SDL_SCANCODE_UP,

    /// Num lock on PC, clear on Mac keyboards.
    num_lock_clear = c.SDL_SCANCODE_NUMLOCKCLEAR,
    kp_divide = c.SDL_SCANCODE_KP_DIVIDE,
    kp_multiply = c.SDL_SCANCODE_KP_MULTIPLY,
    kp_minus = c.SDL_SCANCODE_KP_MINUS,
    kp_plus = c.SDL_SCANCODE_KP_PLUS,
    kp_enter = c.SDL_SCANCODE_KP_ENTER,
    kp_1 = c.SDL_SCANCODE_KP_1,
    kp_2 = c.SDL_SCANCODE_KP_2,
    kp_3 = c.SDL_SCANCODE_KP_3,
    kp_4 = c.SDL_SCANCODE_KP_4,
    kp_5 = c.SDL_SCANCODE_KP_5,
    kp_6 = c.SDL_SCANCODE_KP_6,
    kp_7 = c.SDL_SCANCODE_KP_7,
    kp_8 = c.SDL_SCANCODE_KP_8,
    kp_9 = c.SDL_SCANCODE_KP_9,
    kp_0 = c.SDL_SCANCODE_KP_0,
    kp_period = c.SDL_SCANCODE_KP_PERIOD,

    /// This is the additional key that ISO keyboards have over ANSI ones, located between left shift and Y.
    /// Produces GRAVE ACCENT and TILDE in a US or UK Mac layout, REVERSE SOLIDUS (backslash) and VERTICAL LINE in a US or UK Windows layout,
    /// and LESS-THAN SIGN and GREATER-THAN SIGN in a Swiss German, German, or French layout.
    non_us_backslash = c.SDL_SCANCODE_NONUSBACKSLASH,

    /// Windows contextual menu, compose.
    application = c.SDL_SCANCODE_APPLICATION,

    /// The USB document says this is a status flag, not a physical key, but some Mac keyboards do have a power key.
    power = c.SDL_SCANCODE_POWER,
    kp_equals = c.SDL_SCANCODE_KP_EQUALS,
    func13 = c.SDL_SCANCODE_F13,
    func14 = c.SDL_SCANCODE_F14,
    func15 = c.SDL_SCANCODE_F15,
    func16 = c.SDL_SCANCODE_F16,
    func17 = c.SDL_SCANCODE_F17,
    func18 = c.SDL_SCANCODE_F18,
    func19 = c.SDL_SCANCODE_F19,
    func20 = c.SDL_SCANCODE_F20,
    func21 = c.SDL_SCANCODE_F21,
    func22 = c.SDL_SCANCODE_F22,
    func23 = c.SDL_SCANCODE_F23,
    func24 = c.SDL_SCANCODE_F24,
    execute = c.SDL_SCANCODE_EXECUTE,

    /// AL Integrated Help Center.
    help = c.SDL_SCANCODE_HELP,

    /// Menu (show menu).
    menu = c.SDL_SCANCODE_MENU,
    select = c.SDL_SCANCODE_SELECT,

    /// AC Stop.
    stop = c.SDL_SCANCODE_STOP,

    /// AC Redo/Repeat.
    again = c.SDL_SCANCODE_AGAIN,

    /// AC Undo.
    undo = c.SDL_SCANCODE_UNDO,

    /// AC Cut.
    cut = c.SDL_SCANCODE_CUT,

    /// AC Copy.
    copy = c.SDL_SCANCODE_COPY,

    /// AC Paste.
    paste = c.SDL_SCANCODE_PASTE,

    /// AC Find.
    find = c.SDL_SCANCODE_FIND,
    mute = c.SDL_SCANCODE_MUTE,
    volume_up = c.SDL_SCANCODE_VOLUMEUP,
    volume_down = c.SDL_SCANCODE_VOLUMEDOWN,
    kp_comma = c.SDL_SCANCODE_KP_COMMA,
    kp_equals_as_400 = c.SDL_SCANCODE_KP_EQUALSAS400,

    /// Used on Asian keyboards, see footnotes in USB doc.
    international1 = c.SDL_SCANCODE_INTERNATIONAL1,
    international2 = c.SDL_SCANCODE_INTERNATIONAL2,

    /// Yen.
    international3 = c.SDL_SCANCODE_INTERNATIONAL3,
    international4 = c.SDL_SCANCODE_INTERNATIONAL4,
    international5 = c.SDL_SCANCODE_INTERNATIONAL5,
    international6 = c.SDL_SCANCODE_INTERNATIONAL6,
    international7 = c.SDL_SCANCODE_INTERNATIONAL7,
    international8 = c.SDL_SCANCODE_INTERNATIONAL8,
    international9 = c.SDL_SCANCODE_INTERNATIONAL9,

    /// Hangul/English toggle.
    lang1 = c.SDL_SCANCODE_LANG1,

    /// Hanja conversion.
    lang2 = c.SDL_SCANCODE_LANG2,

    /// Katakana.
    lang3 = c.SDL_SCANCODE_LANG3,

    /// Hiragana.
    lang4 = c.SDL_SCANCODE_LANG4,

    /// Zenkaku/Hankaku.
    lang5 = c.SDL_SCANCODE_LANG5,

    /// Reserved.
    lang6 = c.SDL_SCANCODE_LANG6,

    /// Reserved.
    lang7 = c.SDL_SCANCODE_LANG7,

    /// Reserved.
    lang8 = c.SDL_SCANCODE_LANG8,

    /// Reserved.
    lang9 = c.SDL_SCANCODE_LANG9,

    /// Erase-Eaze.
    alt_erase = c.SDL_SCANCODE_ALTERASE,
    sysreq = c.SDL_SCANCODE_SYSREQ,

    /// AC Cancel.
    cancel = c.SDL_SCANCODE_CANCEL,
    clear = c.SDL_SCANCODE_CLEAR,
    prior = c.SDL_SCANCODE_PRIOR,
    return2 = c.SDL_SCANCODE_RETURN2,
    separator = c.SDL_SCANCODE_SEPARATOR,
    out = c.SDL_SCANCODE_OUT,
    oper = c.SDL_SCANCODE_OPER,
    clear_again = c.SDL_SCANCODE_CLEARAGAIN,
    cr_sel = c.SDL_SCANCODE_CRSEL,
    ex_sel = c.SDL_SCANCODE_EXSEL,
    kp_00 = c.SDL_SCANCODE_KP_00,
    kp_000 = c.SDL_SCANCODE_KP_000,
    thousands_separator = c.SDL_SCANCODE_THOUSANDSSEPARATOR,
    decimals_eparator = c.SDL_SCANCODE_DECIMALSEPARATOR,
    currency_unit = c.SDL_SCANCODE_CURRENCYUNIT,
    currency_subunit = c.SDL_SCANCODE_CURRENCYSUBUNIT,
    kp_left_paren = c.SDL_SCANCODE_KP_LEFTPAREN,
    kp_right_paren = c.SDL_SCANCODE_KP_RIGHTPAREN,
    kp_left_brace = c.SDL_SCANCODE_KP_LEFTBRACE,
    kp_right_brace = c.SDL_SCANCODE_KP_RIGHTBRACE,
    kp_tab = c.SDL_SCANCODE_KP_TAB,
    kp_backspace = c.SDL_SCANCODE_KP_BACKSPACE,
    kp_a = c.SDL_SCANCODE_KP_A,
    kp_b = c.SDL_SCANCODE_KP_B,
    kp_c = c.SDL_SCANCODE_KP_C,
    kp_d = c.SDL_SCANCODE_KP_D,
    kp_e = c.SDL_SCANCODE_KP_E,
    kp_f = c.SDL_SCANCODE_KP_F,
    kp_xor = c.SDL_SCANCODE_KP_XOR,
    kp_power = c.SDL_SCANCODE_KP_POWER,
    kp_percent = c.SDL_SCANCODE_KP_PERCENT,
    kp_less = c.SDL_SCANCODE_KP_LESS,
    kp_greater = c.SDL_SCANCODE_KP_GREATER,
    kp_ampersand = c.SDL_SCANCODE_KP_AMPERSAND,
    kp_dbl_ampersand = c.SDL_SCANCODE_KP_DBLAMPERSAND,
    kp_vertical_bar = c.SDL_SCANCODE_KP_VERTICALBAR,
    kp_dbl_vertical_bar = c.SDL_SCANCODE_KP_DBLVERTICALBAR,
    kp_colon = c.SDL_SCANCODE_KP_COLON,
    kp_hash = c.SDL_SCANCODE_KP_HASH,
    kp_space = c.SDL_SCANCODE_KP_SPACE,
    kp_at = c.SDL_SCANCODE_KP_AT,
    kp_exclam = c.SDL_SCANCODE_KP_EXCLAM,
    kp_mem_store = c.SDL_SCANCODE_KP_MEMSTORE,
    kp_mem_recall = c.SDL_SCANCODE_KP_MEMRECALL,
    kp_mem_clear = c.SDL_SCANCODE_KP_MEMCLEAR,
    kp_mem_add = c.SDL_SCANCODE_KP_MEMADD,
    kp_mem_subtract = c.SDL_SCANCODE_KP_MEMSUBTRACT,
    kp_mem_multiply = c.SDL_SCANCODE_KP_MEMMULTIPLY,
    kp_mem_divide = c.SDL_SCANCODE_KP_MEMDIVIDE,
    kp_plus_minus = c.SDL_SCANCODE_KP_PLUSMINUS,
    kp_clear = c.SDL_SCANCODE_KP_CLEAR,
    kp_clear_entry = c.SDL_SCANCODE_KP_CLEARENTRY,
    kp_binary = c.SDL_SCANCODE_KP_BINARY,
    kp_octal = c.SDL_SCANCODE_KP_OCTAL,
    kp_decimal = c.SDL_SCANCODE_KP_DECIMAL,
    kp_hexadecimal = c.SDL_SCANCODE_KP_HEXADECIMAL,
    left_ctrl = c.SDL_SCANCODE_LCTRL,
    left_shift = c.SDL_SCANCODE_LSHIFT,

    /// Alt, option.
    left_alt = c.SDL_SCANCODE_LALT,

    /// Windows, command (apple), meta.
    left_gui = c.SDL_SCANCODE_LGUI,
    right_ctrl = c.SDL_SCANCODE_RCTRL,
    right_shift = c.SDL_SCANCODE_RSHIFT,

    /// Alt gr, option.
    right_alt = c.SDL_SCANCODE_RALT,

    /// Windows, command (apple), meta.
    right_gui = c.SDL_SCANCODE_RGUI,

    /// I'm not sure if this is really not covered by any of the above, but since there's a special `keycode.KeyModifier.mode` for it I'm adding it here.
    mode = c.SDL_SCANCODE_MODE,

    /// Sleep.
    sleep = c.SDL_SCANCODE_SLEEP,

    /// Wake.
    wake = c.SDL_SCANCODE_WAKE,

    /// Channel increment.
    channel_increment = c.SDL_SCANCODE_CHANNEL_INCREMENT,

    /// Channel decrement.
    channel_decrement = c.SDL_SCANCODE_CHANNEL_DECREMENT,

    /// Play.
    media_play = c.SDL_SCANCODE_MEDIA_PLAY,

    /// Pause.
    media_pause = c.SDL_SCANCODE_MEDIA_PAUSE,

    /// Record.
    media_record = c.SDL_SCANCODE_MEDIA_RECORD,

    /// Fast forward.
    media_fast_forward = c.SDL_SCANCODE_MEDIA_FAST_FORWARD,

    /// Rewind.
    media_rewind = c.SDL_SCANCODE_MEDIA_REWIND,

    /// Next track.
    media_next_track = c.SDL_SCANCODE_MEDIA_NEXT_TRACK,

    /// Previous track.
    media_previous_track = c.SDL_SCANCODE_MEDIA_PREVIOUS_TRACK,

    /// Stop.
    media_stop = c.SDL_SCANCODE_MEDIA_STOP,

    /// Eject.
    media_eject = c.SDL_SCANCODE_MEDIA_EJECT,

    /// Play/pause.
    media_play_pause = c.SDL_SCANCODE_MEDIA_PLAY_PAUSE,

    /// Media select.
    media_select = c.SDL_SCANCODE_MEDIA_SELECT,

    /// AC New.
    ac_new = c.SDL_SCANCODE_AC_NEW,

    /// AC Open.
    ac_open = c.SDL_SCANCODE_AC_OPEN,

    /// AC Close.
    ac_close = c.SDL_SCANCODE_AC_CLOSE,

    /// AC Exist.
    ac_exit = c.SDL_SCANCODE_AC_EXIT,

    /// AC Save.
    ac_save = c.SDL_SCANCODE_AC_SAVE,

    /// AC Print.
    ac_print = c.SDL_SCANCODE_AC_PRINT,

    /// AC Properties.
    ac_properties = c.SDL_SCANCODE_AC_PROPERTIES,

    /// AC Search.
    ac_search = c.SDL_SCANCODE_AC_SEARCH,

    /// AC Home.
    ac_home = c.SDL_SCANCODE_AC_HOME,

    /// AC Back.
    ac_back = c.SDL_SCANCODE_AC_BACK,

    /// AC Forward.
    ac_forward = c.SDL_SCANCODE_AC_FORWARD,

    /// AC Stop.
    ac_stop = c.SDL_SCANCODE_AC_STOP,

    /// AC Refresh.
    ac_refresh = c.SDL_SCANCODE_AC_REFRESH,

    /// AC Bookmarks.
    ac_bookmarks = c.SDL_SCANCODE_AC_BOOKMARKS,

    /// Usually situated below the display on phones and used as a multi-function feature key for selecting a software defined function shown on the bottom left of the display.
    soft_left = c.SDL_SCANCODE_SOFTLEFT,

    /// Usually situated below the display on phones and used as a multi-function feature key for selecting a software defined function shown on the bottom right of the display.
    soft_right = c.SDL_SCANCODE_SOFTRIGHT,

    /// Used for accepting phone calls.
    call = c.SDL_SCANCODE_CALL,

    /// Used for rejecting phone calls.
    end_call = c.SDL_SCANCODE_ENDCALL,

    /// Create an unmanaged scancode from a scancode enum.
    ///
    /// ## Function Parameters
    /// * `self`: scancode enum to make unmanaged.
    ///
    /// ## Return Value
    /// Returns an unmanaged SDL scancode.
    ///
    /// ## Remarks
    /// This makes a copy of the scancode provided.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn toSdl(self: ?Scancode) c.SDL_Scancode {
        if (self) |val|
            return @intFromEnum(val);
        return c.SDL_SCANCODE_UNKNOWN;
    }

    /// Create a scancode enum from an SDL scancode.
    ///
    /// ## Function Parameters
    /// * `event`: SDL scancode to manage.
    ///
    /// ## Return Value
    /// A managed scancode enum.
    ///
    /// ## Remarks
    /// This makes a copy of the scancode provided.
    ///
    /// ## Version
    /// This function is provided by zig-sdl3.
    pub fn fromSdl(key_code: c.SDL_Scancode) ?Scancode {
        if (key_code == c.SDL_SCANCODE_UNKNOWN)
            return null;
        return @enumFromInt(key_code);
    }
};

// Scancode tests.
test "Scancode" {
    errors.refAllDeclsRecursive(@This());
}
