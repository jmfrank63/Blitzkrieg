const std = @import("std");
pub const TargetError = error{ InvalidExtent, MismatchedExtent, UnsupportedAttachments };
pub const Target = struct { width: u32, height: u32, samples: u8, has_depth: bool, active: bool = false };
pub fn create(width: u32, height: u32, samples: u8, has_depth: bool, color_count: u8) TargetError!Target { if (width == 0 or height == 0) return TargetError.InvalidExtent; if (color_count != 1) return TargetError.UnsupportedAttachments; return .{ .width = width, .height = height, .samples = samples, .has_depth = has_depth }; }
pub fn compatible(color: Target, depth: Target) TargetError!void { if (color.width != depth.width or color.height != depth.height or color.samples != depth.samples) return TargetError.MismatchedExtent; }
test "targets require one color attachment and matching depth extent" { const color = try create(320, 200, 1, false, 1); const depth = try create(320, 200, 1, true, 1); try compatible(color, depth); try std.testing.expectError(TargetError.UnsupportedAttachments, create(1, 1, 1, false, 2)); }
