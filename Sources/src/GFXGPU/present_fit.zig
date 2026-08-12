const std = @import("std");

pub const Rect = struct { x: u32, y: u32, w: u32, h: u32 };

/// Letterbox rect for presenting a scene into a drawable: aspect-fit,
/// but the scale is capped at 1.0 - a scene smaller than the drawable is
/// shown 1:1 in a black frame, never upscaled (the spec's shrink-only rule).
pub fn fitRect(source_w: u32, source_h: u32, dest_w: u32, dest_h: u32) Rect {
    if (source_w == 0 or source_h == 0 or dest_w == 0 or dest_h == 0)
        return .{ .x = 0, .y = 0, .w = dest_w, .h = dest_h };
    const scale_w = @as(f64, @floatFromInt(dest_w)) / @as(f64, @floatFromInt(source_w));
    const scale_h = @as(f64, @floatFromInt(dest_h)) / @as(f64, @floatFromInt(source_h));
    const scale = @min(1.0, @min(scale_w, scale_h));
    const fit_w: u32 = @max(1, @min(dest_w, @as(u32, @intFromFloat(@round(@as(f64, @floatFromInt(source_w)) * scale)))));
    const fit_h: u32 = @max(1, @min(dest_h, @as(u32, @intFromFloat(@round(@as(f64, @floatFromInt(source_h)) * scale)))));
    return .{ .x = (dest_w - fit_w) / 2, .y = (dest_h - fit_h) / 2, .w = fit_w, .h = fit_h };
}

test "small scene is shown 1:1 in a black frame, not upscaled" {
    const r = fitRect(800, 600, 1440, 900);
    try std.testing.expectEqual(@as(u32, 800), r.w);
    try std.testing.expectEqual(@as(u32, 600), r.h);
    try std.testing.expectEqual(@as(u32, 320), r.x);
    try std.testing.expectEqual(@as(u32, 150), r.y);
}

test "large scene shrinks to fit with aspect kept" {
    const r = fitRect(1920, 1080, 1440, 900);
    try std.testing.expectEqual(@as(u32, 1440), r.w);
    try std.testing.expectEqual(@as(u32, 810), r.h);
    try std.testing.expectEqual(@as(u32, 0), r.x);
    try std.testing.expectEqual(@as(u32, 45), r.y);
}

test "exact fit is identity" {
    const r = fitRect(1440, 900, 1440, 900);
    try std.testing.expectEqual(Rect{ .x = 0, .y = 0, .w = 1440, .h = 900 }, r);
}
