//! BK_EDITOR_AUTO="frame:action,frame:action,..." drives the real editor loop
//! the way the game's own BK_AUTO_UI drives it (Game/GameMain.cpp:1262-1279):
//! a compact, comma-separated schedule of one action per frame, replayed
//! through smoke.zig's `AutoRunner` over the same synthetic-event machinery
//! the fixed `--smoke` table already uses (plan 5's own note: "BK_EDITOR_AUTO
//! generalises the table").
//!
//! std-only: no window toolkit, no engine bridge, no C import of any kind -
//! this file's own tests (`zig build test-map-editor-auto`) run on the host
//! with nothing staged.
//!
//! Action grammar, one per schedule entry (`name` or `name=value`):
//!   key=<name>[+ctrl][+shift][+alt][+cmd]   a key down then up
//!   press=<point>   drag=<point>   release=<point>   click=<point>
//!                                   a point is `<x>x<y>` (absolute, screen
//!                                   pixels) or `c<dx>x<dy>` (from the
//!                                   screen's centre); click presses and
//!                                   releases at the point in one action
//!   rclick=<point>  rpress=<point>  rdrag=<point>  rrelease=<point>
//!                                   the same for the right button (an
//!                                   M2 tool that takes the right button, or a
//!                                   gesture of Ctrl+left where it asks)
//!   dblclick=<point>               a click, then its second click of a double
//!                                   click (SDL clicks 1, then clicks 2) at the
//!                                   point
//!   tool=<label>                   switch to the tool of that registry label
//!                                   (tool_registry.zig: `select`, `brush`,
//!                                   `place`, ...), `[a-z_]{1,32}`
//!   text=<text>                    type the text into the focused ImGui field
//!                                   (printable ASCII, no comma, 1-64 chars)
//!   wheel=<dx>x<dy>[x<count>][+ctrl][+shift][+alt][+cmd]
//!                                   `count` wheel events of dx,dy (default
//!                                   1), at wherever the last press/drag/
//!                                   release/click left the pointer
//!   open=<path>                    File > Open that OS path
//!   save                           Save (redirects to Save As on a shipped
//!                                   or new map, like the menu item)
//!   saveas=<path>                  Save As to that OS path
//!   test                           Test in game
//!   waitgame=<seconds>             wait for the test game to exit; fails
//!                                   unless it exits 0 within the time
//!   shot=<name>                    capture the frame to `<name>.tga`
//!   compare=<name>[@<percent>]     compare `<name>.tga` against
//!                                   `reference/<name>.tga`, seeding the
//!                                   reference when it does not exist yet;
//!                                   `percent` (default 1.0) is the greatest
//!                                   acceptable percentage of differing
//!                                   pixels
//!   differ=<a>/<b>[@<percent>[/<tolerance>]]
//!                                  compare two shots of this run, `<a>.tga` and
//!                                   `<b>.tga`: the run fails unless MORE than
//!                                   `percent` (default 0.1) of the pixels
//!                                   differ - the proof that something drawn
//!                                   moved (a camera jump, a panel that filled);
//!                                   a pixel differs when a channel moves by more
//!                                   than `tolerance` (default 24)
//!   do=<name>[:<arg>]              run a named editor command (commands.zig:
//!                                   the same one a menu item or panel button
//!                                   runs); the run fails when the name is
//!                                   unknown, the argument is bad or the
//!                                   command is refused
//!   expect=<name>[:<arg>]          ask a named predicate (commands.zig); the
//!                                   run fails "expect=<name>:<arg> was false"
//!                                   when it does not hold
//!   exit                           end the run
//!
//! `do=`/`expect=`'s name is `[A-Za-z0-9_]{1,32}`; the argument, when there
//! is one after the name's ':', is printable ASCII without a space or comma
//! (a comma ends the entry) and at most 64 characters (T-04-03-01). The
//! frame's own ':' is the first one in the entry, so `3:do=camera_player:0`
//! is frame 3, action `do=camera_player:0`.
//!
//! `shot=`/`compare=`'s own name (T-03-12-01: names become file paths inside
//! one fixed directory) is restricted to `[A-Za-z0-9_-]{1,64}`; every other
//! malformed token - an unknown action, a non-numeric frame or number, an
//! unknown modifier - is rejected by `parse`, naming the offending entry.
const std = @import("std");

pub const Mods = struct {
    ctrl: bool = false,
    shift: bool = false,
    alt: bool = false,
    cmd: bool = false,
};

/// A screen point: absolute pixels, or `from_centre` offsets from the
/// screen's own centre (smoke.zig's own convention - the same map point on
/// every screen size).
pub const Point = struct {
    x: f32,
    y: f32,
    from_centre: bool = false,
};

pub const Key = struct {
    /// A single character ('0'-'9', 'A'-'Z') or a named key (DELETE, HOME,
    /// ESCAPE/ESC, SPACE, ENTER/RETURN, TAB, UP, DOWN, LEFT, RIGHT, HOME,
    /// END, BACKSPACE, INSERT) - matched case-insensitively for the named form.
    /// smoke.zig's `AutoRunner` maps this to an SDL keycode/scancode; auto.zig
    /// itself does not know SDL.
    name: []const u8,
    mods: Mods = .{},
};

pub const Wheel = struct {
    dx: f32,
    dy: f32,
    count: u8 = 1,
    mods: Mods = .{},
};

pub const Compare = struct {
    name: []const u8,
    percent: f32 = default_compare_percent,
};

pub const default_compare_percent: f32 = 1.0;

/// `differ=<a>/<b>[@<percent>[/<tolerance>]]`: two shots of this run that must
/// differ by more than `percent` of their pixels (the opposite of `compare=`'s
/// tolerance). A low-contrast motion, such as a river's water scrolling over
/// its bottom, moves every pixel by a few levels only, below the default
/// channel tolerance; such a check names a smaller one.
pub const Differ = struct {
    a: []const u8,
    b: []const u8,
    percent: f32 = default_differ_percent,
    channel_tolerance: u8 = default_channel_tolerance,
};

pub const default_differ_percent: f32 = 0.1;

/// `shot=`/`compare=`'s own name: at most this many characters, from
/// `[A-Za-z0-9_-]`.
pub const max_name_len = 64;

/// The channel-tolerance `compare=` failure describes: "a pixel differs
/// when a channel differs by more than 24" (03-12-PLAN.md Task 2).
pub const default_channel_tolerance: u8 = 24;

/// A named command or predicate call: `do=<name>[:<arg>]`, `expect=...`.
pub const Named = struct {
    name: []const u8,
    /// Empty when the entry has no ':<arg>'.
    arg: []const u8 = "",
};

/// `do=`/`expect=`'s own limits (T-04-03-01).
pub const max_named_len = 32;
pub const max_arg_len = 64;

pub const Action = union(enum) {
    key: Key,
    press: Point,
    drag: Point,
    release: Point,
    click: Point,
    rpress: Point,
    rdrag: Point,
    rrelease: Point,
    rclick: Point,
    dblclick: Point,
    tool: []const u8,
    text: []const u8,
    wheel: Wheel,
    open: []const u8,
    save,
    saveas: []const u8,
    test_in_game,
    waitgame: u32,
    shot: []const u8,
    compare: Compare,
    differ: Differ,
    do: Named,
    expect: Named,
    exit,
};

pub const Scheduled = struct {
    frame: u32,
    action: Action,
    /// The raw "name=value" text after the frame's ':' (borrowed from the
    /// schedule string `parse` was given) - reprinted verbatim by the
    /// runner's own `BK_EDITOR_AUTO: frame N action ...` log line, the way
    /// the game's own BK_AUTO_UI reprints `szAction.c_str()`.
    text: []const u8,
};

pub const ParseError = error{
    /// The entry has no ':', or the text before it is not a whole number.
    BadFrame,
    /// The action's name is not one of the actions above, or a bare action
    /// (save/test/exit) was given a value, or a valued one was given none.
    BadAction,
    /// A point, wheel delta/count, waitgame's seconds or compare's percent
    /// did not parse as a number.
    BadNumber,
    /// An unknown modifier, an empty key name, or a shot/compare name
    /// outside `[A-Za-z0-9_-]{1,64}`, or a do/expect name outside
    /// `[A-Za-z0-9_]{1,32}`.
    BadName,
    /// A do/expect argument that is too long or holds a character outside
    /// printable ASCII (or a space).
    BadArgument,
} || std.mem.Allocator.Error;

/// What went wrong, and the exact entry ("frame:action") that said so - for
/// a caller to print (main.zig: "a parse error prints and exits 2").
pub const Failure = struct {
    token: []const u8 = "",
    reason: []const u8 = "",
};

/// Parses `text` ("frame:action,frame:action,..."). The returned slice
/// (allocated with `allocator`) and every `Action`'s string field borrow
/// directly from `text` - `text` must outlive the result, exactly as
/// smoke.zig's own `Input.save_as` borrows its caller-owned save path.
/// Blank entries (an empty string between two commas) are skipped, so a
/// trailing comma is harmless.
pub fn parse(allocator: std.mem.Allocator, text: []const u8, failure: *Failure) ParseError![]Scheduled {
    var list: std.ArrayList(Scheduled) = .empty;
    errdefer list.deinit(allocator);
    var it = std.mem.splitScalar(u8, text, ',');
    while (it.next()) |raw_entry| {
        const entry = std.mem.trim(u8, raw_entry, " \t");
        if (entry.len == 0) continue;
        const colon = std.mem.indexOfScalar(u8, entry, ':') orelse
            return fail(failure, entry, "missing ':' between the frame and the action", error.BadFrame);
        const frame = std.fmt.parseInt(u32, entry[0..colon], 10) catch
            return fail(failure, entry, "the frame is not a whole number", error.BadFrame);
        const action_text = entry[colon + 1 ..];
        const action = try parseAction(action_text, entry, failure);
        try list.append(allocator, .{ .frame = frame, .action = action, .text = action_text });
    }
    return list.toOwnedSlice(allocator);
}

fn fail(failure: *Failure, entry: []const u8, reason: []const u8, err: ParseError) ParseError {
    failure.* = .{ .token = entry, .reason = reason };
    return err;
}

fn parseAction(text: []const u8, entry: []const u8, failure: *Failure) ParseError!Action {
    const eq = std.mem.indexOfScalar(u8, text, '=');
    const name = if (eq) |i| text[0..i] else text;
    const value: ?[]const u8 = if (eq) |i| text[i + 1 ..] else null;

    if (std.mem.eql(u8, name, "key"))
        return .{ .key = try parseKey(value orelse return fail(failure, entry, "key needs a name", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "press"))
        return .{ .press = try parsePoint(value orelse return fail(failure, entry, "press needs a point", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "drag"))
        return .{ .drag = try parsePoint(value orelse return fail(failure, entry, "drag needs a point", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "release"))
        return .{ .release = try parsePoint(value orelse return fail(failure, entry, "release needs a point", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "click"))
        return .{ .click = try parsePoint(value orelse return fail(failure, entry, "click needs a point", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "rpress"))
        return .{ .rpress = try parsePoint(value orelse return fail(failure, entry, "rpress needs a point", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "rdrag"))
        return .{ .rdrag = try parsePoint(value orelse return fail(failure, entry, "rdrag needs a point", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "rrelease"))
        return .{ .rrelease = try parsePoint(value orelse return fail(failure, entry, "rrelease needs a point", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "rclick"))
        return .{ .rclick = try parsePoint(value orelse return fail(failure, entry, "rclick needs a point", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "dblclick"))
        return .{ .dblclick = try parsePoint(value orelse return fail(failure, entry, "dblclick needs a point", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "tool")) {
        const label = value orelse return fail(failure, entry, "tool needs a label", error.BadAction);
        try validateToolLabel(label, entry, failure);
        return .{ .tool = label };
    }
    if (std.mem.eql(u8, name, "text")) {
        const typed = value orelse return fail(failure, entry, "text needs the text to type", error.BadAction);
        try validateText(typed, entry, failure);
        return .{ .text = typed };
    }
    if (std.mem.eql(u8, name, "wheel"))
        return .{ .wheel = try parseWheel(value orelse return fail(failure, entry, "wheel needs a value", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "open")) {
        const path = value orelse return fail(failure, entry, "open needs a path", error.BadAction);
        if (path.len == 0) return fail(failure, entry, "open's path is empty", error.BadAction);
        return .{ .open = path };
    }
    if (std.mem.eql(u8, name, "save")) {
        if (value != null) return fail(failure, entry, "save takes no value", error.BadAction);
        return .save;
    }
    if (std.mem.eql(u8, name, "saveas")) {
        const path = value orelse return fail(failure, entry, "saveas needs a path", error.BadAction);
        if (path.len == 0) return fail(failure, entry, "saveas's path is empty", error.BadAction);
        return .{ .saveas = path };
    }
    if (std.mem.eql(u8, name, "test")) {
        if (value != null) return fail(failure, entry, "test takes no value", error.BadAction);
        return .test_in_game;
    }
    if (std.mem.eql(u8, name, "waitgame")) {
        const v = value orelse return fail(failure, entry, "waitgame needs a number of seconds", error.BadAction);
        const seconds = std.fmt.parseInt(u32, v, 10) catch return fail(failure, entry, "waitgame's seconds is not a whole number", error.BadNumber);
        return .{ .waitgame = seconds };
    }
    if (std.mem.eql(u8, name, "shot")) {
        const v = value orelse return fail(failure, entry, "shot needs a name", error.BadAction);
        try validateName(v, entry, failure);
        return .{ .shot = v };
    }
    if (std.mem.eql(u8, name, "compare"))
        return .{ .compare = try parseCompare(value orelse return fail(failure, entry, "compare needs a name", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "differ"))
        return .{ .differ = try parseDiffer(value orelse return fail(failure, entry, "differ needs two shot names", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "do"))
        return .{ .do = try parseNamed(value orelse return fail(failure, entry, "do needs a name", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "expect"))
        return .{ .expect = try parseNamed(value orelse return fail(failure, entry, "expect needs a name", error.BadAction), entry, failure) };
    if (std.mem.eql(u8, name, "exit")) {
        if (value != null) return fail(failure, entry, "exit takes no value", error.BadAction);
        return .exit;
    }

    return fail(failure, entry, "unknown action", error.BadAction);
}

/// `<name>[+mod]*`.
fn parseKey(text: []const u8, entry: []const u8, failure: *Failure) ParseError!Key {
    var it = std.mem.splitScalar(u8, text, '+');
    const name = it.next() orelse return fail(failure, entry, "key needs a name", error.BadName);
    if (name.len == 0) return fail(failure, entry, "key's name is empty", error.BadName);
    var mods: Mods = .{};
    while (it.next()) |mod_name| try applyMod(&mods, mod_name, entry, failure);
    return .{ .name = name, .mods = mods };
}

fn applyMod(mods: *Mods, name: []const u8, entry: []const u8, failure: *Failure) ParseError!void {
    if (std.mem.eql(u8, name, "ctrl")) {
        mods.ctrl = true;
        return;
    }
    if (std.mem.eql(u8, name, "shift")) {
        mods.shift = true;
        return;
    }
    if (std.mem.eql(u8, name, "alt")) {
        mods.alt = true;
        return;
    }
    if (std.mem.eql(u8, name, "cmd")) {
        mods.cmd = true;
        return;
    }
    return fail(failure, entry, "unknown modifier", error.BadName);
}

/// `<x>x<y>` or `c<dx>x<dy>`.
fn parsePoint(text: []const u8, entry: []const u8, failure: *Failure) ParseError!Point {
    var rest = text;
    var from_centre = false;
    if (rest.len != 0 and rest[0] == 'c') {
        from_centre = true;
        rest = rest[1..];
    }
    const sep = std.mem.indexOfScalar(u8, rest, 'x') orelse return fail(failure, entry, "a point needs '<x>x<y>'", error.BadNumber);
    const x = std.fmt.parseFloat(f32, rest[0..sep]) catch return fail(failure, entry, "the point's x is not a number", error.BadNumber);
    const y = std.fmt.parseFloat(f32, rest[sep + 1 ..]) catch return fail(failure, entry, "the point's y is not a number", error.BadNumber);
    return .{ .x = x, .y = y, .from_centre = from_centre };
}

/// `<dx>x<dy>[x<count>][+mod]*`.
fn parseWheel(text: []const u8, entry: []const u8, failure: *Failure) ParseError!Wheel {
    var plus_it = std.mem.splitScalar(u8, text, '+');
    const core_text = plus_it.next() orelse return fail(failure, entry, "wheel needs a value", error.BadNumber);
    var mods: Mods = .{};
    while (plus_it.next()) |mod_name| try applyMod(&mods, mod_name, entry, failure);

    var parts = std.mem.splitScalar(u8, core_text, 'x');
    const dx_text = parts.next() orelse return fail(failure, entry, "wheel needs '<dx>x<dy>'", error.BadNumber);
    const dy_text = parts.next() orelse return fail(failure, entry, "wheel needs '<dx>x<dy>'", error.BadNumber);
    const dx = std.fmt.parseFloat(f32, dx_text) catch return fail(failure, entry, "wheel's dx is not a number", error.BadNumber);
    const dy = std.fmt.parseFloat(f32, dy_text) catch return fail(failure, entry, "wheel's dy is not a number", error.BadNumber);
    var count: u8 = 1;
    if (parts.next()) |count_text|
        count = std.fmt.parseInt(u8, count_text, 10) catch return fail(failure, entry, "wheel's count is not a whole number", error.BadNumber);
    if (parts.next() != null) return fail(failure, entry, "wheel takes at most dx, dy and a count", error.BadNumber);
    return .{ .dx = dx, .dy = dy, .count = count, .mods = mods };
}

/// `<name>[@<percent>]`.
fn parseCompare(text: []const u8, entry: []const u8, failure: *Failure) ParseError!Compare {
    const at = std.mem.indexOfScalar(u8, text, '@');
    const name = if (at) |i| text[0..i] else text;
    try validateName(name, entry, failure);
    var percent: f32 = default_compare_percent;
    if (at) |i|
        percent = std.fmt.parseFloat(f32, text[i + 1 ..]) catch return fail(failure, entry, "compare's percent is not a number", error.BadNumber);
    return .{ .name = name, .percent = percent };
}

/// `<a>/<b>[@<percent>[/<tolerance>]]`.
fn parseDiffer(text: []const u8, entry: []const u8, failure: *Failure) ParseError!Differ {
    const at = std.mem.indexOfScalar(u8, text, '@');
    const names = if (at) |i| text[0..i] else text;
    const slash = std.mem.indexOfScalar(u8, names, '/') orelse return fail(failure, entry, "differ needs '<a>/<b>'", error.BadName);
    try validateName(names[0..slash], entry, failure);
    try validateName(names[slash + 1 ..], entry, failure);
    var percent: f32 = default_differ_percent;
    var channel_tolerance: u8 = default_channel_tolerance;
    if (at) |i| {
        const limits = text[i + 1 ..];
        const tolerance_slash = std.mem.indexOfScalar(u8, limits, '/');
        const percent_text = if (tolerance_slash) |j| limits[0..j] else limits;
        percent = std.fmt.parseFloat(f32, percent_text) catch return fail(failure, entry, "differ's percent is not a number", error.BadNumber);
        if (tolerance_slash) |j|
            channel_tolerance = std.fmt.parseInt(u8, limits[j + 1 ..], 10) catch return fail(failure, entry, "differ's channel tolerance is not a number from 0 to 255", error.BadNumber);
    }
    return .{ .a = names[0..slash], .b = names[slash + 1 ..], .percent = percent, .channel_tolerance = channel_tolerance };
}

/// A tool label: `[a-z_]{1,32}` (the registry's ToolId names).
fn validateToolLabel(label: []const u8, entry: []const u8, failure: *Failure) ParseError!void {
    if (label.len == 0 or label.len > max_named_len) return fail(failure, entry, "the tool label must be 1-32 characters", error.BadName);
    for (label) |ch| {
        if (!((ch >= 'a' and ch <= 'z') or ch == '_')) return fail(failure, entry, "the tool label may only use lower-case letters and '_'", error.BadName);
    }
}

/// `text=`: printable ASCII (a space is fine), 1..64 characters. A comma
/// cannot occur: it ends the schedule entry before this is read.
fn validateText(typed: []const u8, entry: []const u8, failure: *Failure) ParseError!void {
    if (typed.len == 0 or typed.len > max_arg_len) return fail(failure, entry, "the text must be 1-64 characters", error.BadArgument);
    for (typed) |ch| {
        if (ch < ' ' or ch > '~') return fail(failure, entry, "the text may only use printable ASCII", error.BadArgument);
    }
}

/// `<name>[:<arg>]`.
fn parseNamed(text: []const u8, entry: []const u8, failure: *Failure) ParseError!Named {
    const colon = std.mem.indexOfScalar(u8, text, ':');
    const name = if (colon) |i| text[0..i] else text;
    const arg = if (colon) |i| text[i + 1 ..] else "";
    if (name.len == 0 or name.len > max_named_len) return fail(failure, entry, "the name must be 1-32 characters", error.BadName);
    for (name) |ch| {
        const ok = (ch >= 'A' and ch <= 'Z') or (ch >= 'a' and ch <= 'z') or (ch >= '0' and ch <= '9') or ch == '_';
        if (!ok) return fail(failure, entry, "the name may only use letters, digits and '_'", error.BadName);
    }
    try validateArgument(arg, entry, failure);
    return .{ .name = name, .arg = arg };
}

/// Printable ASCII without a space, at most `max_arg_len` characters; empty
/// is fine (a command with no argument).
fn validateArgument(arg: []const u8, entry: []const u8, failure: *Failure) ParseError!void {
    if (arg.len > max_arg_len) return fail(failure, entry, "the argument must be at most 64 characters", error.BadArgument);
    for (arg) |ch| {
        if (ch <= ' ' or ch > '~') return fail(failure, entry, "the argument may only use printable ASCII without spaces", error.BadArgument);
    }
}

fn validateName(name: []const u8, entry: []const u8, failure: *Failure) ParseError!void {
    if (name.len == 0 or name.len > max_name_len) return fail(failure, entry, "the name must be 1-64 characters", error.BadName);
    for (name) |ch| {
        const ok = (ch >= 'A' and ch <= 'Z') or (ch >= 'a' and ch <= 'z') or (ch >= '0' and ch <= '9') or ch == '_' or ch == '-';
        if (!ok) return fail(failure, entry, "the name may only use letters, digits, '_' and '-'", error.BadName);
    }
}

/// An uncompressed 32-bit TGA: an 18-byte header, an optional ID, then BGRA
/// rows, bottom row first unless bit 5 of the descriptor (byte 17) is set -
/// the same layout main.zig's own `--check` probe already reads by hand.
pub const Tga = struct {
    width: u32,
    height: u32,
    top_first: bool,
    pixels: []const u8,

    pub const Error = error{ Truncated, NotUncompressedTrueColour, Not32Bit };

    pub fn parse(bytes: []const u8) Error!Tga {
        if (bytes.len < 18) return error.Truncated;
        if (bytes[1] != 0 or bytes[2] != 2) return error.NotUncompressedTrueColour;
        if (bytes[16] != 32) return error.Not32Bit;
        const width = std.mem.readInt(u16, bytes[12..14], .little);
        const height = std.mem.readInt(u16, bytes[14..16], .little);
        const start = 18 + @as(usize, bytes[0]);
        const length = @as(usize, width) * @as(usize, height) * 4;
        if (bytes.len < start + length) return error.Truncated;
        return .{ .width = width, .height = height, .top_first = bytes[17] & 0x20 != 0, .pixels = bytes[start .. start + length] };
    }

    /// BGRA at (x, y), y measured from the top regardless of this file's own
    /// row order - so comparing two descriptors of the same image (one
    /// top-down, one bottom-up) reads the same logical pixel from both.
    pub fn pixel(self: Tga, x: u32, y: u32) [4]u8 {
        const row = if (self.top_first) y else self.height - 1 - y;
        const i = (@as(usize, row) * self.width + x) * 4;
        return self.pixels[i..][0..4].*;
    }
};

pub const Diff = struct {
    same_size: bool,
    differing: usize,
    total: usize,

    pub fn fraction(self: Diff) f32 {
        if (self.total == 0) return 0;
        return @as(f32, @floatFromInt(self.differing)) / @as(f32, @floatFromInt(self.total));
    }
};

/// `.same_size = false` (and `differing`/`total` both 0) when the images are
/// different sizes - the caller decides what that means (`compare=`: an
/// unconditional failure, sizes are never "close enough"). Otherwise a pixel
/// differs when any of its first three channels (B, G, R - alpha is not
/// compared, matching main.zig's own probe-pixel comparison) is more than
/// `channel_tolerance` apart between the two images.
pub fn compareTga(a: Tga, b: Tga, channel_tolerance: u8) Diff {
    if (a.width != b.width or a.height != b.height) return .{ .same_size = false, .differing = 0, .total = 0 };
    var differing: usize = 0;
    var y: u32 = 0;
    while (y < a.height) : (y += 1) {
        var x: u32 = 0;
        while (x < a.width) : (x += 1) {
            const pa = a.pixel(x, y);
            const pb = b.pixel(x, y);
            var differs = false;
            for (0..3) |i| {
                if (@abs(@as(i16, pa[i]) - @as(i16, pb[i])) > channel_tolerance) {
                    differs = true;
                    break;
                }
            }
            if (differs) differing += 1;
        }
    }
    return .{ .same_size = true, .differing = differing, .total = @as(usize, a.width) * a.height };
}

fn expectAction(text: []const u8, want: Action) !void {
    var failure: Failure = .{};
    const schedule = try parse(std.testing.allocator, text, &failure);
    defer std.testing.allocator.free(schedule);
    try std.testing.expectEqual(@as(usize, 1), schedule.len);
    try std.testing.expectEqual(@as(u32, 3), schedule[0].frame);
    try std.testing.expectEqualDeep(want, schedule[0].action);
}

test "parse: every action" {
    try expectAction("3:key=E", .{ .key = .{ .name = "E" } });
    try expectAction("3:key=Z+ctrl+shift", .{ .key = .{ .name = "Z", .mods = .{ .ctrl = true, .shift = true } } });
    try expectAction("3:press=c-120x-160", .{ .press = .{ .x = -120, .y = -160, .from_centre = true } });
    try expectAction("3:drag=15x25", .{ .drag = .{ .x = 15, .y = 25, .from_centre = false } });
    try expectAction("3:release=c0x0", .{ .release = .{ .x = 0, .y = 0, .from_centre = true } });
    try expectAction("3:click=5x5", .{ .click = .{ .x = 5, .y = 5, .from_centre = false } });
    try expectAction("3:wheel=0x1x3+shift", .{ .wheel = .{ .dx = 0, .dy = 1, .count = 3, .mods = .{ .shift = true } } });
    try expectAction("3:wheel=0.15x0.35", .{ .wheel = .{ .dx = 0.15, .dy = 0.35, .count = 1 } });
    try expectAction("3:open=maps/foo.bzm", .{ .open = "maps/foo.bzm" });
    try expectAction("3:save", .save);
    try expectAction("3:saveas=maps/bar.bzm", .{ .saveas = "maps/bar.bzm" });
    try expectAction("3:test", .test_in_game);
    try expectAction("3:waitgame=240", .{ .waitgame = 240 });
    try expectAction("3:shot=painted", .{ .shot = "painted" });
    try expectAction("3:compare=painted", .{ .compare = .{ .name = "painted", .percent = default_compare_percent } });
    try expectAction("3:compare=painted@2.5", .{ .compare = .{ .name = "painted", .percent = 2.5 } });
    try expectAction("3:differ=before/after", .{ .differ = .{ .a = "before", .b = "after", .percent = default_differ_percent } });
    try expectAction("3:differ=a-1/b_2@0.5", .{ .differ = .{ .a = "a-1", .b = "b_2", .percent = 0.5 } });
    try expectAction("3:differ=a/b@1/4", .{ .differ = .{ .a = "a", .b = "b", .percent = 1, .channel_tolerance = 4 } });
    try expectAction("3:rpress=c1x2", .{ .rpress = .{ .x = 1, .y = 2, .from_centre = true } });
    try expectAction("3:rdrag=10x20", .{ .rdrag = .{ .x = 10, .y = 20 } });
    try expectAction("3:rrelease=c0x0", .{ .rrelease = .{ .x = 0, .y = 0, .from_centre = true } });
    try expectAction("3:rclick=c0x0", .{ .rclick = .{ .x = 0, .y = 0, .from_centre = true } });
    try expectAction("3:dblclick=c10x10", .{ .dblclick = .{ .x = 10, .y = 10, .from_centre = true } });
    try expectAction("3:tool=select", .{ .tool = "select" });
    try expectAction("3:tool=roads_rivers", .{ .tool = "roads_rivers" });
    try expectAction("3:text=Area 1", .{ .text = "Area 1" });
    try expectAction("3:key=INSERT", .{ .key = .{ .name = "INSERT" } });
    try expectAction("3:do=camera_player:0", .{ .do = .{ .name = "camera_player", .arg = "0" } });
    try expectAction("3:do=camera_neutral", .{ .do = .{ .name = "camera_neutral", .arg = "" } });
    try expectAction("3:expect=anchor_set:neutral", .{ .expect = .{ .name = "anchor_set", .arg = "neutral" } });
    try expectAction("3:expect=undo_depth:12", .{ .expect = .{ .name = "undo_depth", .arg = "12" } });
    try expectAction("3:exit", .exit);
}

test "parse: a whole schedule, in order, frames included" {
    var failure: Failure = .{};
    const text = "3:key=2,4:press=c-120x-160,5:drag=c-95x-160,6:release=c-70x-160,8:shot=painted,9:exit";
    const schedule = try parse(std.testing.allocator, text, &failure);
    defer std.testing.allocator.free(schedule);
    try std.testing.expectEqual(@as(usize, 6), schedule.len);
    const frames = [_]u32{ 3, 4, 5, 6, 8, 9 };
    for (schedule, frames) |item, frame| try std.testing.expectEqual(frame, item.frame);
    try std.testing.expectEqualStrings("key=2", schedule[0].text);
    try std.testing.expect(schedule[5].action == .exit);
}

test "parse: a trailing comma is harmless" {
    var failure: Failure = .{};
    const schedule = try parse(std.testing.allocator, "3:exit,", &failure);
    defer std.testing.allocator.free(schedule);
    try std.testing.expectEqual(@as(usize, 1), schedule.len);
}

fn expectBad(text: []const u8, err: ParseError) !void {
    var failure: Failure = .{};
    try std.testing.expectError(err, parse(std.testing.allocator, text, &failure));
    try std.testing.expect(failure.token.len != 0);
    try std.testing.expect(failure.reason.len != 0);
}

test "parse: bad tokens are rejected, naming the entry" {
    try expectBad("nope:exit", error.BadFrame); // not a whole number
    try expectBad("3exit", error.BadFrame); // no ':'
    try expectBad("3:frobnicate", error.BadAction); // unknown action
    try expectBad("3:save=x", error.BadAction); // a bare action given a value
    try expectBad("3:key=", error.BadName); // no name
    try expectBad("3:key=E+sideways", error.BadName); // unknown modifier
    try expectBad("3:press=cabcxdef", error.BadNumber); // malformed numbers
    try expectBad("3:press=5", error.BadNumber); // no 'x' separator
    try expectBad("3:wheel=1x2x3x4", error.BadNumber); // too many parts
    try expectBad("3:waitgame=soon", error.BadNumber); // not a whole number
    try expectBad("3:shot=bad name", error.BadName); // space is not allowed
    try expectBad("3:shot=bad$name", error.BadName); // '$' is not allowed
    try expectBad("3:shot=", error.BadName); // empty name
    try expectBad("3:compare=painted@soon", error.BadNumber); // percent not a number
    try expectBad("3:shot=" ++ ("a" ** 65), error.BadName); // over max_name_len
    try expectBad("3:differ", error.BadAction); // differ needs two names
    try expectBad("3:differ=one", error.BadName); // no '/'
    try expectBad("3:differ=a/", error.BadName); // an empty second name
    try expectBad("3:differ=a b/c", error.BadName); // a space
    try expectBad("3:differ=a/b@lots", error.BadNumber); // percent not a number
    try expectBad("3:differ=a/b@1/300", error.BadNumber); // a channel holds 0..255
    try expectBad("3:rclick", error.BadAction); // rclick needs a point
    try expectBad("3:rpress=5", error.BadNumber); // no 'x' separator
    try expectBad("3:rdrag=axb", error.BadNumber);
    try expectBad("3:rrelease", error.BadAction);
    try expectBad("3:dblclick", error.BadAction);
    try expectBad("3:dblclick=cxx", error.BadNumber);
    try expectBad("3:tool", error.BadAction); // tool needs a label
    try expectBad("3:tool=", error.BadName); // empty label
    try expectBad("3:tool=Select", error.BadName); // upper case
    try expectBad("3:tool=roads-rivers", error.BadName); // '-'
    try expectBad("3:tool=" ++ ("a" ** 33), error.BadName); // over 32
    try expectBad("3:text", error.BadAction); // text needs a value
    try expectBad("3:text=", error.BadArgument); // empty
    try expectBad("3:text=" ++ ("a" ** 65), error.BadArgument); // over 64
    try expectBad("3:text=tab\there", error.BadArgument); // control character
    try expectBad("3:text=caf\xc3\xa9", error.BadArgument); // non-ASCII
    try expectBad("3:do", error.BadAction); // do needs a name
    try expectBad("3:expect", error.BadAction); // expect needs a name
    try expectBad("3:do=", error.BadName); // empty name
    try expectBad("3:do=:5", error.BadName); // empty name before the argument
    try expectBad("3:do=bad-name:1", error.BadName); // '-' is not allowed in a command name
    try expectBad("3:expect=bad$name", error.BadName);
    try expectBad("3:do=" ++ ("a" ** 33), error.BadName); // over max_named_len
    try expectBad("3:do=camera_player:" ++ ("9" ** 65), error.BadArgument); // argument over 64
    try expectBad("3:do=camera_player:a b", error.BadArgument); // a space in the argument
    try expectBad("3:expect=anchor_set:caf\xc3\xa9", error.BadArgument); // non-ASCII
    try expectBad("3:do=camera_player:tab\there", error.BadArgument); // a control character
}

test "parse: a do argument may hold its own ':' and '='" {
    try expectAction("3:do=name:a:b=c", .{ .do = .{ .name = "name", .arg = "a:b=c" } });
}

fn writeTga(buffer: []u8, width: u16, height: u16, top_first: bool, pixels_bgra: []const u8) []const u8 {
    std.debug.assert(buffer.len >= 18 + pixels_bgra.len);
    @memset(buffer[0..18], 0);
    buffer[2] = 2;
    std.mem.writeInt(u16, buffer[12..14], width, .little);
    std.mem.writeInt(u16, buffer[14..16], height, .little);
    buffer[16] = 32;
    buffer[17] = if (top_first) 0x28 else 0x08;
    @memcpy(buffer[18..][0..pixels_bgra.len], pixels_bgra);
    return buffer[0 .. 18 + pixels_bgra.len];
}

// 2x2, top row (y=0) red, bottom row (y=1) blue, in BGRA.
const red = [4]u8{ 0, 0, 255, 255 };
const blue = [4]u8{ 255, 0, 0, 255 };

test "compareTga: identical images differ nowhere" {
    var buffer_a: [18 + 16]u8 = undefined;
    var buffer_b: [18 + 16]u8 = undefined;
    const pixels = red ++ red ++ blue ++ blue;
    const a = try Tga.parse(writeTga(&buffer_a, 2, 2, true, &pixels));
    const b = try Tga.parse(writeTga(&buffer_b, 2, 2, true, &pixels));
    const diff = compareTga(a, b, default_channel_tolerance);
    try std.testing.expect(diff.same_size);
    try std.testing.expectEqual(@as(usize, 0), diff.differing);
    try std.testing.expectEqual(@as(usize, 4), diff.total);
}

test "compareTga: one pixel off is counted" {
    var buffer_a: [18 + 16]u8 = undefined;
    var buffer_b: [18 + 16]u8 = undefined;
    const pixels_a = red ++ red ++ blue ++ blue;
    const pixels_b = red ++ blue ++ blue ++ blue;
    const a = try Tga.parse(writeTga(&buffer_a, 2, 2, true, &pixels_a));
    const b = try Tga.parse(writeTga(&buffer_b, 2, 2, true, &pixels_b));
    const diff = compareTga(a, b, default_channel_tolerance);
    try std.testing.expect(diff.same_size);
    try std.testing.expectEqual(@as(usize, 1), diff.differing);
    try std.testing.expect(diff.fraction() > 0);
}

test "compareTga: a size mismatch never compares equal" {
    var buffer_a: [18 + 16]u8 = undefined;
    var buffer_b: [18 + 4]u8 = undefined;
    const pixels_a = red ++ red ++ blue ++ blue;
    const pixels_b = red;
    const a = try Tga.parse(writeTga(&buffer_a, 2, 2, true, &pixels_a));
    const b = try Tga.parse(writeTga(&buffer_b, 1, 1, true, &pixels_b));
    const diff = compareTga(a, b, default_channel_tolerance);
    try std.testing.expect(!diff.same_size);
}

test "compareTga: a bottom-up descriptor of the same image as a top-down one compares equal" {
    var buffer_top: [18 + 16]u8 = undefined;
    var buffer_bottom: [18 + 16]u8 = undefined;
    const top_down = red ++ red ++ blue ++ blue; // row 0 (top) first
    const bottom_up = blue ++ blue ++ red ++ red; // row 1 (bottom) first
    const a = try Tga.parse(writeTga(&buffer_top, 2, 2, true, &top_down));
    const b = try Tga.parse(writeTga(&buffer_bottom, 2, 2, false, &bottom_up));
    const diff = compareTga(a, b, default_channel_tolerance);
    try std.testing.expect(diff.same_size);
    try std.testing.expectEqual(@as(usize, 0), diff.differing);
}
