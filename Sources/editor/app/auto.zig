//! BK_EDITOR_AUTO's MapEditor-side adapter: a thin re-export shim over
//! `editor_kit.auto_schedule`, so smoke.zig's `AutoRunner` and main.zig's
//! schedule-driven run keep their `auto_mod.<Name>` surface after S02/T04
//! moved the parser + TGA compare into the kit. The `tool=<label>` /
//! `do=<named_cmd>` dispatch (plan: "keep MapEditor's tool/do handlers in
//! app/auto.zig") already lives in `smoke.zig`'s AutoRunner and
//! `commands.zig`'s registry - no action table here.
const schedule = @import("editor_kit").auto_schedule;

pub const Mods = schedule.Mods;
pub const Point = schedule.Point;
pub const Key = schedule.Key;
pub const Wheel = schedule.Wheel;
pub const Compare = schedule.Compare;
pub const Differ = schedule.Differ;
pub const Named = schedule.Named;
pub const Action = schedule.Action;
pub const Scheduled = schedule.Scheduled;
pub const ParseError = schedule.ParseError;
pub const Failure = schedule.Failure;
pub const Tga = schedule.Tga;
pub const Diff = schedule.Diff;

pub const default_compare_percent = schedule.default_compare_percent;
pub const default_differ_percent = schedule.default_differ_percent;
pub const max_name_len = schedule.max_name_len;
pub const default_channel_tolerance = schedule.default_channel_tolerance;
pub const max_named_len = schedule.max_named_len;
pub const max_arg_len = schedule.max_arg_len;

pub const parse = schedule.parse;
pub const compareTga = schedule.compareTga;
