//! The Game's pixel size per size class (D102, D108), in one place so the Game, l10n-check and
//! language-mods measure at exactly the sizes the Game draws. The values are the shipped bitmap
//! fonts' heights, so layouts built for them keep their proportions. A Shaper's pixel size is an
//! em size (HarfBuzz scale = px * 64, stb_truetype scale_for_mapping_em_to_pixels).
const std = @import("std");

pub const SizeClass = enum(u32) { tiny = 0, small = 1, medium = 2, large = 3 };

/// The em size in pixels of `class` at `ui_scale` (1.0 is the Game's own).
pub fn pixelSize(class: SizeClass, ui_scale: f32) f32 {
    const base: f32 = switch (class) {
        .tiny => 10.0,
        .small => 16.0,
        .medium => 20.0,
        .large => 40.0,
    };
    return base * ui_scale;
}

test "pixelSize is the Game's table" {
    try std.testing.expectEqual(@as(f32, 10), pixelSize(.tiny, 1.0));
    try std.testing.expectEqual(@as(f32, 16), pixelSize(.small, 1.0));
    try std.testing.expectEqual(@as(f32, 20), pixelSize(.medium, 1.0));
    try std.testing.expectEqual(@as(f32, 40), pixelSize(.large, 1.0));
    try std.testing.expectEqual(@as(f32, 30), pixelSize(.medium, 1.5));
}
