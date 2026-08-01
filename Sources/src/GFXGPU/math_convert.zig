const std = @import("std");

pub const Matrix = [16]f32;
pub const Viewport = struct { x: f32, y: f32, width: f32, height: f32, min_depth: f32, max_depth: f32 };
pub const ClipPolicy = enum { direct3d, vulkan, metal };
pub const MathError = error{ InvalidViewport };

pub fn identity() Matrix { return .{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }; }

pub fn translation(x: f32, y: f32, z: f32) Matrix {
    var result = identity(); result[12] = x; result[13] = y; result[14] = z; return result;
}

pub fn multiply(left: Matrix, right: Matrix) Matrix {
    var result: Matrix = undefined;
    for (0..4) |row| for (0..4) |column| {
        var value: f32 = 0;
        for (0..4) |inner| value += left[row * 4 + inner] * right[inner * 4 + column];
        result[row * 4 + column] = value;
    };
    return result;
}

pub fn perspective(fov_y: f32, aspect: f32, near: f32, far: f32, policy: ClipPolicy) Matrix {
    const f = 1.0 / @tan(fov_y * 0.5);
    var result = [_]f32{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    result[0] = f / aspect; result[5] = f; result[11] = -1;
    if (policy == .direct3d) { result[10] = far / (near - far); result[14] = near * far / (near - far); } else { result[10] = far / (near - far); result[14] = near * far / (near - far); }
    return result;
}

pub fn orthographic(width: f32, height: f32, policy: ClipPolicy) Matrix {
    var result = identity(); result[0] = 2 / width; result[5] = if (policy == .direct3d) -2 / height else 2 / height; result[12] = -1; result[13] = 1; return result;
}

pub fn validateViewport(viewport: Viewport) MathError!Viewport {
    if (!std.math.isFinite(viewport.x) or !std.math.isFinite(viewport.y) or !std.math.isFinite(viewport.width) or !std.math.isFinite(viewport.height) or !std.math.isFinite(viewport.min_depth) or !std.math.isFinite(viewport.max_depth)) return MathError.InvalidViewport;
    if (viewport.width < 0 or viewport.height < 0 or viewport.min_depth > viewport.max_depth) return MathError.InvalidViewport;
    return viewport;
}

pub fn argb8ToRgba(value: u32) [4]f32 {
    return .{ @as(f32, @floatFromInt((value >> 16) & 0xff)) / 255.0, @as(f32, @floatFromInt((value >> 8) & 0xff)) / 255.0, @as(f32, @floatFromInt(value & 0xff)) / 255.0, @as(f32, @floatFromInt((value >> 24) & 0xff)) / 255.0 };
}

test "matrix and color golden conversions" {
    try std.testing.expectEqual(identity(), multiply(identity(), identity()));
    try std.testing.expectEqual(@as(f32, 3), multiply(translation(1, 2, 3), identity())[12]);
    try std.testing.expectEqual(@as(f32, 0.2), argb8ToRgba(0xff336699)[0]);
    try std.testing.expectEqual(@as(f32, 0.4), argb8ToRgba(0xff336699)[1]);
    try std.testing.expectEqual(@as(f32, 0.6), argb8ToRgba(0xff336699)[2]);
}

test "viewport validation rejects invalid ranges and non-finite values" {
    const valid = Viewport{ .x = 0, .y = 0, .width = 320, .height = 200, .min_depth = 0, .max_depth = 1 };
    _ = try validateViewport(valid);
    try std.testing.expectError(MathError.InvalidViewport, validateViewport(.{ .x = 0, .y = 0, .width = -1, .height = 1, .min_depth = 0, .max_depth = 1 }));
    try std.testing.expectError(MathError.InvalidViewport, validateViewport(.{ .x = 0, .y = 0, .width = 1, .height = 1, .min_depth = 1, .max_depth = 0 }));
}
