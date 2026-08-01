const c = @import("c");
const errors = @import("errors.zig");
const std = @import("std");

pub const FloatingType = f32;
pub const IntegerType = i32;

/// A positional point.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub fn Point(comptime Type: type) type {
    return extern struct {
        const Self = @This();
        x: Type,
        y: Type,

        // Size tests.
        comptime {
            std.debug.assert(@sizeOf(c.SDL_Point) == @sizeOf(IPoint));
            std.debug.assert(@offsetOf(c.SDL_Point, "x") == @offsetOf(IPoint, "x"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_Point, "x")) == @sizeOf(@FieldType(IPoint, "x")));
            std.debug.assert(@offsetOf(c.SDL_Point, "y") == @offsetOf(IPoint, "y"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_Point, "y")) == @sizeOf(@FieldType(IPoint, "y")));

            std.debug.assert(@sizeOf(c.SDL_FPoint) == @sizeOf(FPoint));
            std.debug.assert(@offsetOf(c.SDL_FPoint, "x") == @offsetOf(FPoint, "x"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_FPoint, "x")) == @sizeOf(@FieldType(FPoint, "x")));
            std.debug.assert(@offsetOf(c.SDL_FPoint, "y") == @offsetOf(FPoint, "y"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_FPoint, "y")) == @sizeOf(@FieldType(FPoint, "y")));
        }

        /// Get this as a different type of point.
        ///
        /// ## Function Parameters
        /// * `self`: Point to convert.
        /// * `NewType`: New underlying type to use for values in the point.
        ///
        /// ## Return Value
        /// Returns a new point with each member casted to the new type.
        pub fn asOtherPoint(
            self: Self,
            comptime NewType: type,
        ) Point(NewType) {
            const self_int = switch (@typeInfo(Type)) {
                .int, .comptime_int => true,
                else => false,
            };
            const new_int = switch (@typeInfo(NewType)) {
                .int, .comptime_int => true,
                else => false,
            };
            if (self_int) {
                if (new_int) {
                    return .{
                        .x = @as(NewType, @intCast(self.x)),
                        .y = @as(NewType, @intCast(self.y)),
                    };
                } else {
                    return .{
                        .x = @as(NewType, @floatFromInt(self.x)),
                        .y = @as(NewType, @floatFromInt(self.y)),
                    };
                }
            } else {
                if (new_int) {
                    return .{
                        .x = @as(NewType, @intFromFloat(self.x)),
                        .y = @as(NewType, @intFromFloat(self.y)),
                    };
                } else {
                    return .{
                        .x = @as(NewType, @floatCast(self.x)),
                        .y = @as(NewType, @floatCast(self.y)),
                    };
                }
            }
        }

        /// From an SDL point.
        fn fromSdlFPoint(self: c.SDL_FPoint) Self {
            return .{
                .x = @floatCast(self.x),
                .y = @floatCast(self.y),
            };
        }

        /// From an SDL point.
        fn fromSdlIPoint(self: c.SDL_Point) Self {
            return .{
                .x = @intCast(self.x),
                .y = @intCast(self.y),
            };
        }

        /// Get the SDL point.
        fn toSdlFPoint(self: Self) c.SDL_FPoint {
            return .{
                .x = @floatCast(self.x),
                .y = @floatCast(self.y),
            };
        }

        /// Get the SDL point.
        fn toSdlIPoint(self: Self) c.SDL_Point {
            return .{
                .x = @intCast(self.x),
                .y = @intCast(self.y),
            };
        }

        // Put other SDL declarations here.
        const isFPoint = Type == FloatingType;
        const isIPoint = Type == IntegerType;

        /// Get a point from an SDL point.
        pub const fromSdl = if (isIPoint) fromSdlIPoint else if (isFPoint) fromSdlFPoint else null;
        /// Get the SDL point.
        pub const toSdl = if (isIPoint) toSdlIPoint else if (isFPoint) toSdlFPoint else null;
    };
}

/// The structure that defines a point (using floating point values).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const FPoint = Point(FloatingType);

/// The structure that defines a point (using integers).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const IPoint = Point(IntegerType);

/// Rectangle with position and size.
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub fn Rect(comptime Type: type) type {
    return extern struct {
        const Self = @This();
        x: Type,
        y: Type,
        w: Type,
        h: Type,

        // Size tests.
        comptime {
            std.debug.assert(@sizeOf(c.SDL_Rect) == @sizeOf(IRect));
            std.debug.assert(@offsetOf(c.SDL_Rect, "x") == @offsetOf(IRect, "x"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_Rect, "x")) == @sizeOf(@FieldType(IRect, "x")));
            std.debug.assert(@offsetOf(c.SDL_Rect, "y") == @offsetOf(IRect, "y"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_Rect, "y")) == @sizeOf(@FieldType(IRect, "y")));
            std.debug.assert(@offsetOf(c.SDL_Rect, "w") == @offsetOf(IRect, "w"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_Rect, "w")) == @sizeOf(@FieldType(IRect, "w")));
            std.debug.assert(@offsetOf(c.SDL_Rect, "h") == @offsetOf(IRect, "h"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_Rect, "h")) == @sizeOf(@FieldType(IRect, "h")));

            std.debug.assert(@sizeOf(c.SDL_FRect) == @sizeOf(FRect));
            std.debug.assert(@offsetOf(c.SDL_FRect, "x") == @offsetOf(FRect, "x"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_FRect, "x")) == @sizeOf(@FieldType(FRect, "x")));
            std.debug.assert(@offsetOf(c.SDL_FRect, "y") == @offsetOf(FRect, "y"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_FRect, "y")) == @sizeOf(@FieldType(FRect, "y")));
            std.debug.assert(@offsetOf(c.SDL_FRect, "w") == @offsetOf(FRect, "w"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_FRect, "w")) == @sizeOf(@FieldType(FRect, "w")));
            std.debug.assert(@offsetOf(c.SDL_FRect, "h") == @offsetOf(FRect, "h"));
            std.debug.assert(@sizeOf(@FieldType(c.SDL_FRect, "h")) == @sizeOf(@FieldType(FRect, "h")));
        }

        /// Get this as a different type of rectangle.
        ///
        /// ## Function Parameters
        /// * `self`: Rectangle to convert.
        /// * `NewType`: New underlying type to use for values in the rectangle.
        ///
        /// ## Return Value
        /// Returns a new rectangle with each member casted to the new type.
        pub fn asOtherRect(
            self: Self,
            comptime NewType: type,
        ) Rect(NewType) {
            const self_int = switch (@typeInfo(Type)) {
                .int, .comptime_int => true,
                else => false,
            };
            const new_int = switch (@typeInfo(NewType)) {
                .int, .comptime_int => true,
                else => false,
            };
            if (self_int) {
                if (new_int) {
                    return .{
                        .x = @as(NewType, @intCast(self.x)),
                        .y = @as(NewType, @intCast(self.y)),
                        .w = @as(NewType, @intCast(self.w)),
                        .h = @as(NewType, @intCast(self.h)),
                    };
                } else {
                    return .{
                        .x = @as(NewType, @floatFromInt(self.x)),
                        .y = @as(NewType, @floatFromInt(self.y)),
                        .w = @as(NewType, @floatFromInt(self.w)),
                        .h = @as(NewType, @floatFromInt(self.h)),
                    };
                }
            } else {
                if (new_int) {
                    return .{
                        .x = @as(NewType, @intFromFloat(self.x)),
                        .y = @as(NewType, @intFromFloat(self.y)),
                        .w = @as(NewType, @intFromFloat(self.w)),
                        .h = @as(NewType, @intFromFloat(self.h)),
                    };
                } else {
                    return .{
                        .x = @as(NewType, @floatCast(self.x)),
                        .y = @as(NewType, @floatCast(self.y)),
                        .w = @as(NewType, @floatCast(self.w)),
                        .h = @as(NewType, @floatCast(self.h)),
                    };
                }
            }
        }

        /// Determine whether a rectangle has no area.
        ///
        /// ## Function Parameters
        /// * `self`: The rectangle to test.
        ///
        /// ## Return Value
        /// Returns true if the rectangle is "empty", false otherwise.
        ///
        /// ## Remarks
        /// A rectangle is considered "empty" for this function if the rectangle's width and/or height are <= 0.
        ///
        /// ## Thread Safety
        /// It is safe to call this function from any thread.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub fn empty(
            self: Self,
        ) bool {
            return self.w <= 0 or self.h <= 0;
        }

        /// Determine whether two rectangles are equal.
        ///
        /// ## Function Parameters
        /// * `self`: The first rectangle to test.
        /// * `other`: The second rectangle to test.
        ///
        /// ## Return Value
        /// Returns true if the rectangles are equal, false otherwise.
        ///
        /// Rectangles are considered equal if each of their x, y, width and height match.
        ///
        /// ## Thread Safety
        /// It is safe to call this function from any thread.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub fn equal(
            self: Self,
            other: Self,
        ) bool {
            return std.meta.eql(self, other);
        }

        /// Test for equality with an epsilon value.
        fn equalEpsilonFRect(
            self: FRect,
            other: FRect,
            epsilon: Type,
        ) bool {
            const a = self.toSdl();
            const b = other.toSdl();
            return c.SDL_RectsEqualEpsilon(&a, &b, epsilon);
        }

        /// Create from an SDL rect.c
        fn fromSdlFRect(rect: c.SDL_FRect) FRect {
            return .{
                .x = @floatCast(rect.x),
                .y = @floatCast(rect.y),
                .w = @floatCast(rect.w),
                .h = @floatCast(rect.h),
            };
        }

        /// Create from an SDL rect.
        fn fromSdlIRect(rect: c.SDL_Rect) IRect {
            return .{
                .x = @intCast(rect.x),
                .y = @intCast(rect.y),
                .w = @intCast(rect.w),
                .h = @intCast(rect.h),
            };
        }

        /// Get intersection with another rect.
        fn getIntersectionFRect(
            self: FRect,
            other: FRect,
        ) ?FRect {
            const a = self.toSdl();
            const b = other.toSdl();
            var ret: c.SDL_FRect = undefined;
            if (!c.SDL_GetRectIntersectionFloat(&a, &b, &ret))
                return null;
            return fromSdl(ret);
        }

        /// Get intersection with another rect.
        fn getIntersectionIRect(
            self: IRect,
            other: IRect,
        ) ?IRect {
            const a = self.toSdl();
            const b = other.toSdl();
            var ret: c.SDL_Rect = undefined;
            if (!c.SDL_GetRectIntersection(&a, &b, &ret))
                return null;
            return fromSdl(ret);
        }

        /// Calculate the intersection between a rect and lines. Returns null if there is no intersection.
        fn getLineIntersectionFRect(
            self: FRect,
            line: [2]FPoint,
        ) ?[2]FPoint {
            const rect = self.toSdl();
            var p1 = line[0].toSdl();
            var p2 = line[1].toSdl();
            if (!c.SDL_GetRectAndLineIntersectionFloat(&rect, &p1.x, &p1.y, &p2.x, &p2.y))
                return null;
            return [_]FPoint{ FPoint.fromSdl(p1), FPoint.fromSdl(p2) };
        }

        /// Calculate the intersection between a rect and lines. Returns null if there is no intersection.
        fn getLineIntersectionIRect(
            self: IRect,
            line: [2]IPoint,
        ) ?[2]IPoint {
            const rect = self.toSdl();
            var p1 = line[0].toSdl();
            var p2 = line[1].toSdl();
            if (!c.SDL_GetRectAndLineIntersection(&rect, &p1.x, &p1.y, &p2.x, &p2.y))
                return null;
            return [_]IPoint{ IPoint.fromSdl(p1), IPoint.fromSdl(p2) };
        }

        /// Calculate a minimal rectangle enclosing a set of points.
        ///
        /// ## Function Parameters
        /// * `point`: Points to be enclosed, must be more than 1 or else `null` will be returned.
        /// * `clip`: Used for clipping, but may be `null` to enclose all points.
        ///
        /// ## Return Value
        /// Returns a rectangle enclosing all the points, or `null` if less than 2 points are provided or if all the points are outside the clipping rectangle.
        ///
        /// ## Remarks
        /// If clip is not `null` then only points inside of the clipping rectangle are considered.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub fn getRectEnclosingPoints(
            points: []const Point(Type),
            clip: ?Self,
        ) ?Self {
            if (points.len < 1)
                return null;

            // If clipping rectangle exists.
            var min_x: Type = undefined;
            var min_y: Type = undefined;
            var max_x: Type = undefined;
            var max_y: Type = undefined;
            if (clip) |rect| {
                if (rect.empty())
                    return null;
                var added = false;
                const clip_min_x = rect.x;
                const clip_min_y = rect.y;
                const clip_max_x = rect.x + rect.w;
                const clip_max_y = rect.y + rect.h;
                for (points) |point| {
                    const x = point.x;
                    const y = point.y;
                    if (x < clip_min_x or x > clip_max_x or y < clip_min_y or y > clip_max_y)
                        continue;
                    if (!added) {
                        min_x = x;
                        max_x = x;
                        min_y = y;
                        max_y = y;
                        added = true;
                        continue;
                    }
                    min_x = @min(x, min_x);
                    min_y = @min(y, min_y);
                    max_x = @max(x, max_x);
                    max_y = @max(y, max_y);
                }
                if (!added)
                    return null;
            } else {
                min_x = points[0].x;
                max_x = points[0].x;
                min_y = points[0].y;
                max_y = points[0].y;
                for (points[1..]) |point| {
                    min_x = @min(min_x, point.x);
                    min_y = @min(min_y, point.y);
                    max_x = @max(max_x, point.x);
                    max_y = @max(max_y, point.y);
                }
            }
            return .{
                .x = min_x,
                .y = min_y,
                .w = (max_x - min_x),
                .h = (max_y - min_y),
            };
        }

        /// Get the union between two rectangles.
        fn getUnionFRect(
            self: FRect,
            other: FRect,
        ) !FRect {
            const a = self.toSdl();
            const b = other.toSdl();
            var ret: c.SDL_FRect = undefined;
            try errors.wrapCallBool(c.SDL_GetRectUnionFloat(&a, &b, &ret));
            return fromSdl(ret);
        }

        /// Get the union between two rectangles.
        fn getUnionIRect(
            self: IRect,
            other: IRect,
        ) !IRect {
            const a = self.toSdl();
            const b = other.toSdl();
            var ret: c.SDL_Rect = undefined;
            try errors.wrapCallBool(c.SDL_GetRectUnion(&a, &b, &ret));
            return fromSdl(ret);
        }

        /// If two rectangles are intersecting.
        fn hasIntersectionFRect(
            self: FRect,
            other: FRect,
        ) bool {
            const a = self.toSdl();
            const b = other.toSdl();
            return c.SDL_HasRectIntersectionFloat(&a, &b);
        }

        /// If two rectangles are intersecting.
        fn hasIntersectionIRect(
            self: IRect,
            other: IRect,
        ) bool {
            const a = self.toSdl();
            const b = other.toSdl();
            return c.SDL_HasRectIntersection(&a, &b);
        }

        /// Determine whether a point resides inside a rectangle.
        ///
        /// ## Function Parameters
        /// * `self`: The rectangle to check for if a point is inside.
        /// * `point`: The point to see if it is in the rectangle.
        ///
        /// ## Return Value
        /// Returns if the point is inside the rectangle or not.
        ///
        /// ## Remarks
        /// A point is considered part of a rectangle if the point's x and y coordinates are >= to the rectangle's top left corner,
        /// and < the rectangle's x+w and y+h.
        /// So a 1x1 rectangle considers point (0,0) as "inside" and (0,1) as not.
        ///
        /// ## Thread Safety
        /// It is safe to call this function from any thread.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub fn pointIn(
            self: Self,
            point: Point(Type),
        ) bool {
            return point.x >= self.x and (point.x < (self.x + self.w)) and
                (point.y >= self.y) and (point.y < (self.y + self.h));
        }

        /// Get the SDL rect.
        fn toSdlFRect(self: FRect) c.SDL_FRect {
            return c.SDL_FRect{
                .x = @floatCast(self.x),
                .y = @floatCast(self.y),
                .w = @floatCast(self.w),
                .h = @floatCast(self.h),
            };
        }

        /// Get the SDL rect.
        fn toSdlIRect(self: IRect) c.SDL_Rect {
            return c.SDL_Rect{
                .x = @intCast(self.x),
                .y = @intCast(self.y),
                .w = @intCast(self.w),
                .h = @intCast(self.h),
            };
        }

        // Put other SDL declarations here.
        const isFRect = Type == FloatingType;
        const isIRect = Type == IntegerType;

        /// Determine whether two floating point rectangles are equal, within some given epsilon.
        ///
        /// ## Function Parameters
        /// * `self`: First rectangle to test.
        /// * `other`: Second rectangle to test.
        /// * `epsilon`: The epsilon value for comparison.
        ///
        /// ## Return Value
        /// Returns true if the rectangles are equal, false otherwise.
        ///
        /// ## Remarks
        /// Rectangles are considered equal if each of their x, y, width and height are within epsilon of each other.
        /// If you don't know what value to use for epsilon, you should call the `rects.equal()` function instead.
        ///
        /// ## Thread Safety
        /// It is safe to call this function from any thread.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub const equalEpsilon = if (isFRect) equalEpsilonFRect else {};

        /// Create a rectangle from an SDL rectangle.
        pub const fromSdl = if (isIRect) fromSdlIRect else if (isFRect) fromSdlFRect else {};

        /// Calculate the intersection of two rectangles.
        ///
        /// ## Function Parameters
        /// * `self`: The first rectangle.
        /// * `other`: The other rectangle.
        ///
        /// ## Return Value
        /// Returns the rectangle intersection, or `null` otherwise.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub const getIntersection = if (isIRect) getIntersectionIRect else if (isFRect) getIntersectionFRect else {};

        /// Calculate the intersection of a rectangle and line segment.
        ///
        /// ## Function Parameters
        /// * `self`: Rectangle to intersect with.
        /// * `line`: Two points representing the starting and ending coordinates of the line.
        ///
        /// ## Return Value
        /// Returns if there is an intersection.
        ///
        /// ## Remarks
        /// This function is used to clip a line segment to a rectangle.
        /// A line segment contained entirely within the rectangle or that does not intersect will remain unchanged.
        /// A line segment that crosses the rectangle at either or both ends will be clipped to the boundary of the rectangle
        /// and the new coordinates saved in X1, Y1, X2, and/or Y2 as necessary.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub const getLineIntersection = if (isIRect) getLineIntersectionIRect else if (isFRect) getLineIntersectionFRect else {};

        /// Calculate the union of two rectangles.
        ///
        /// ## Function Parameters
        /// * `self`: First rectangle.
        /// * `other`: Second rectangle.
        ///
        /// ## Return Value
        /// Returns the union region between the two rectangles.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub const getUnion = if (isIRect) getUnionIRect else if (isFRect) getUnionFRect else {};

        /// Determine whether two rectangles intersect.
        ///
        /// ## Function Parameters
        /// * `self`: First rectangle.
        /// * `other`: Second rectangle.
        ///
        /// ## Return Value
        /// Returns true if there is an intersection, false otherwise.
        ///
        /// ## Thread Safety
        /// It is safe to call this function from any thread.
        ///
        /// ## Version
        /// This function is available since SDL 3.2.0.
        pub const hasIntersection = if (isIRect) hasIntersectionIRect else if (isFRect) hasIntersectionFRect else {};

        /// Get the SDL rectangle.
        pub const toSdl = if (isIRect) toSdlIRect else if (isFRect) toSdlFRect else {};
    };
}

/// A rectangle, with the origin at the upper left (using floating point values).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const FRect = Rect(FloatingType);

/// A rectangle, with the origin at the upper left (using integers).
///
/// ## Version
/// This struct is available since SDL 3.2.0.
pub const IRect = Rect(IntegerType);

test "Rect" {
    errors.refAllDeclsRecursive(@This());

    const a = IRect{ .x = 10, .y = 20, .w = 50, .h = 50 }; // X: 10, 60; Y: 20, 70.
    const b = IRect{ .x = 30, .y = 30, .w = 10, .h = 10 }; // X: 30, 40; Y: 30, 40.

    const line_intersections = [_]IPoint{ .{ .x = 10, .y = 20 }, .{ .x = 59, .y = 69 } };
    try std.testing.expectEqual(
        line_intersections,
        a.getLineIntersection(.{ .{ .x = 0, .y = 10 }, .{ .x = 70, .y = 80 } }),
    );

    try std.testing.expectEqual(
        a,
        IRect.getRectEnclosingPoints(&.{ .{ .x = 10, .y = 20 }, .{ .x = 60, .y = 70 } }, a),
    );

    try std.testing.expectEqual(b, a.getIntersection(b));
    try std.testing.expectEqual(a, a.getUnion(b));
    try std.testing.expect(a.hasIntersection(b));
    try std.testing.expect(a.pointIn(.{ .x = 21, .y = 25 }));
    try std.testing.expect(!b.pointIn(.{ .x = 21, .y = 25 }));
    try std.testing.expect(!a.empty());
    try std.testing.expect(!a.equal(b));

    const af = FRect{ .x = 10, .y = 20, .w = 50, .h = 50 }; // X: 10, 60; Y: 20, 70.
    const bf = FRect{ .x = 30, .y = 30, .w = 10, .h = 10 }; // X: 30, 40; Y: 30, 40.

    const line_intersections_f = [_]FPoint{ .{ .x = 10, .y = 20 }, .{ .x = 60, .y = 70 } };
    try std.testing.expectEqual(
        line_intersections_f,
        af.getLineIntersection(.{ .{ .x = 0, .y = 10 }, .{ .x = 70, .y = 80 } }),
    );

    try std.testing.expectEqual(
        af,
        FRect.getRectEnclosingPoints(&.{ .{ .x = 10, .y = 20 }, .{ .x = 60, .y = 70 } }, af),
    );

    try std.testing.expectEqual(bf, af.getIntersection(bf));
    try std.testing.expectEqual(af, af.getUnion(bf));
    try std.testing.expect(af.hasIntersection(bf));
    try std.testing.expect(af.pointIn(.{ .x = 21, .y = 25 }));
    try std.testing.expect(!bf.pointIn(.{ .x = 21, .y = 25 }));
    try std.testing.expect(!af.empty());
    try std.testing.expect(!af.equal(bf));

    try std.testing.expect(af.equalEpsilon(.{ .x = 10.1, .y = 19.7, .w = 49.77, .h = 50.25 }, 0.5));

    try std.testing.expect(a.asOtherRect(FloatingType).equal(af));
    try std.testing.expect(af.asOtherRect(IntegerType).equal(a));

    const p: IPoint = .{ .x = 45, .y = 67 };
    const fp: FPoint = .{ .x = 45.0, .y = 67.0 };

    try std.testing.expectEqual(p.asOtherPoint(FloatingType), fp);
    try std.testing.expectEqual(fp.asOtherPoint(IntegerType), p);
}
