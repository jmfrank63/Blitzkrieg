//! The fastest path (Remastered gameplay, docs/superpowers/specs/2026-10-28-fastest-path-design.md):
//! A* over a snapshot of the AI tile grid that CStandartPathFinder builds for one order.
//!
//! The search is a pure function of the snapshot and the request: integer costs, a fixed
//! neighbour order and a total order on the open list, so every machine of a lockstep game
//! finds the same path. It allocates nothing: the per-cell work arrays are this library's
//! own static buffers, sized for the largest AI map, and the caller passes the output.
//! The AI runs on one thread, so one search at a time is all it needs.
const std = @import("std");

/// The AI's largest map side in tiles (SAIConsts::MAX_MAP_SIZE).
pub const max_side: usize = 1024;
pub const max_cells: usize = max_side * max_side;

/// A step's length: 70 straight, 99 diagonal (99 / 70 = 1.4143, sqrt 2 to 0.01%).
pub const straight: u32 = 70;
pub const diagonal: u32 = 99;

/// A snapshot cell: the weight of entering the tile per unit of length in the low bits (0 is a
/// tile the unit cannot enter), and the "good tile" bit of the order's point checking.
pub const cell_good: u16 = 0x8000;
pub const cell_weight: u16 = 0x1FFF;

pub const Status = enum(i32) {
    /// The path ends on the goal.
    goal = 0,
    /// It ends on the first good tile the search took (an attack order's range).
    good = 1,
    /// The goal cannot be entered; the path ends beside it, as the 2003 walk did.
    beside_goal = 2,
    /// The goal cannot be reached: the path ends on the reachable tile nearest to it.
    closest = 3,
    /// The node budget ran out; the caller falls back to the 2003 finder.
    budget = 4,
    invalid = 5,
};

pub const Grid = extern struct {
    cells: [*]const u16,
    width: i32,
    height: i32,
};

pub const Request = extern struct {
    start_x: i32,
    start_y: i32,
    goal_x: i32,
    goal_y: i32,
    /// No path longer than this many steps (the 2003 upperLimit).
    max_steps: i32,
    /// Expansions before the search gives up (a safety net, not a limit shipped maps reach).
    node_budget: i32,
};

/// A tile of the path and the cost of reaching it from the start.
pub const PathPoint = extern struct {
    x: i32,
    y: i32,
    cost: u32,
};

pub const Result = extern struct {
    status: i32,
    /// Points written to the output, start and end included; 0 for budget and invalid.
    length: i32,
    expanded: i32,
};

const dirs = [8][2]i32{ .{ 1, 0 }, .{ 0, 1 }, .{ -1, 0 }, .{ 0, -1 }, .{ 1, 1 }, .{ -1, 1 }, .{ -1, -1 }, .{ 1, -1 } };
const no_dir: u8 = 0x0F;
const closed_bit: u8 = 0x80;

// The work arrays, valid for a cell only while its stamp is the current search's.
var stamp: [max_cells]u32 = undefined;
var cost: [max_cells]u32 = undefined;
var estimate: [max_cells]u32 = undefined;
var steps: [max_cells]u32 = undefined;
var from: [max_cells]u8 = undefined;
var heap_pos: [max_cells]u32 = undefined;
var heap: [max_cells]u32 = undefined;
var heap_len: usize = 0;
var current_stamp: u32 = 0;

fn octile(dx: i32, dy: i32) u32 {
    const ax: u32 = @abs(dx);
    const ay: u32 = @abs(dy);
    return straight * @max(ax, ay) + (diagonal - straight) * @min(ax, ay);
}

/// Open-list order: lower estimate, then nearer the goal (lower heuristic), then lower index.
fn before(a: u32, b: u32) bool {
    if (estimate[a] != estimate[b]) return estimate[a] < estimate[b];
    const ha = estimate[a] - cost[a];
    const hb = estimate[b] - cost[b];
    if (ha != hb) return ha < hb;
    return a < b;
}

fn place(at: usize, node: u32) void {
    heap[at] = node;
    heap_pos[node] = @intCast(at);
}

fn siftUp(start_at: usize) void {
    var at = start_at;
    const node = heap[at];
    while (at > 0) {
        const parent = (at - 1) / 2;
        if (!before(node, heap[parent])) break;
        place(at, heap[parent]);
        at = parent;
    }
    place(at, node);
}

fn siftDown(start_at: usize) void {
    var at = start_at;
    const node = heap[at];
    while (true) {
        var child = 2 * at + 1;
        if (child >= heap_len) break;
        if (child + 1 < heap_len and before(heap[child + 1], heap[child])) child += 1;
        if (!before(heap[child], node)) break;
        place(at, heap[child]);
        at = child;
    }
    place(at, node);
}

fn push(node: u32) void {
    heap_len += 1;
    place(heap_len - 1, node);
    siftUp(heap_len - 1);
}

fn pop() u32 {
    const top = heap[0];
    heap_len -= 1;
    if (heap_len > 0) {
        place(0, heap[heap_len]);
        siftDown(0);
    }
    return top;
}

/// Starts a search: a stamp no cell holds yet, so no array needs clearing (except once, and
/// when the counter wraps).
fn nextStamp() void {
    if (current_stamp == 0 or current_stamp == std.math.maxInt(u32)) {
        @memset(&stamp, 0);
        current_stamp = 0;
    }
    current_stamp += 1;
}

/// Finds the cheapest path from the request's start to its goal over the grid and writes it to
/// out (start first). The start is taken as free even where the grid blocks it (a unit standing
/// on a blocked tile). A diagonal step needs both tiles beside it free, so no unit cuts a corner.
pub fn find(grid: Grid, request: Request, out: []PathPoint) Result {
    const invalid = Result{ .status = @intFromEnum(Status.invalid), .length = 0, .expanded = 0 };
    if (grid.width <= 0 or grid.height <= 0 or out.len == 0) return invalid;
    const width: usize = @intCast(grid.width);
    const height: usize = @intCast(grid.height);
    if (width > max_side or height > max_side) return invalid;
    if (request.start_x < 0 or request.start_y < 0 or request.start_x >= grid.width or request.start_y >= grid.height) return invalid;
    if (request.goal_x < 0 or request.goal_y < 0 or request.goal_x >= grid.width or request.goal_y >= grid.height) return invalid;
    const cells = grid.cells[0 .. width * height];

    // The heuristic is the octile distance at the fastest weight on the grid: admissible and
    // consistent, so the first time a tile is taken its cost is final.
    var min_weight: u32 = cell_weight;
    for (cells) |c| {
        const w = c & cell_weight;
        if (w != 0 and w < min_weight) min_weight = w;
    }
    const max_steps: u32 = @intCast(@max(0, @min(request.max_steps, @as(i32, @intCast(@min(out.len - 1, std.math.maxInt(i32)))))));
    const budget: u32 = @intCast(@max(1, request.node_budget));

    nextStamp();
    heap_len = 0;

    const goal: u32 = @intCast(@as(usize, @intCast(request.goal_y)) * width + @as(usize, @intCast(request.goal_x)));
    const start: u32 = @intCast(@as(usize, @intCast(request.start_y)) * width + @as(usize, @intCast(request.start_x)));
    const goal_open = (cells[goal] & cell_weight) != 0;

    stamp[start] = current_stamp;
    cost[start] = 0;
    steps[start] = 0;
    from[start] = no_dir;
    estimate[start] = octile(request.goal_x - request.start_x, request.goal_y - request.start_y) *| min_weight;
    push(start);

    var best = start;
    var best_distance = octile(request.goal_x - request.start_x, request.goal_y - request.start_y);
    var expanded: u32 = 0;
    var status = Status.closest;
    var end = start;

    while (heap_len > 0) {
        const node = pop();
        from[node] |= closed_bit;
        const x: i32 = @intCast(node % width);
        const y: i32 = @intCast(node / width);
        if (node == goal) {
            status = .goal;
            end = node;
            break;
        }
        if (!goal_open and @abs(x - request.goal_x) <= 1 and @abs(y - request.goal_y) <= 1) {
            status = .beside_goal;
            end = node;
            break;
        }
        if (node != start and (cells[node] & cell_good) != 0) {
            status = .good;
            end = node;
            break;
        }
        const distance = octile(request.goal_x - x, request.goal_y - y);
        if (distance < best_distance or (distance == best_distance and (cost[node] < cost[best] or (cost[node] == cost[best] and node < best)))) {
            best = node;
            best_distance = distance;
        }
        expanded += 1;
        if (expanded > budget) {
            return Result{ .status = @intFromEnum(Status.budget), .length = 0, .expanded = @intCast(expanded) };
        }
        if (steps[node] >= max_steps) continue;

        for (dirs, 0..) |d, k| {
            const nx = x + d[0];
            const ny = y + d[1];
            if (nx < 0 or ny < 0 or nx >= grid.width or ny >= grid.height) continue;
            const next: u32 = @intCast(@as(usize, @intCast(ny)) * width + @as(usize, @intCast(nx)));
            const weight: u32 = cells[next] & cell_weight;
            if (weight == 0) continue;
            const is_diagonal = d[0] != 0 and d[1] != 0;
            if (is_diagonal) {
                const side_a = @as(usize, @intCast(y)) * width + @as(usize, @intCast(nx));
                const side_b = @as(usize, @intCast(ny)) * width + @as(usize, @intCast(x));
                if ((cells[side_a] & cell_weight) == 0 or (cells[side_b] & cell_weight) == 0) continue;
            }
            const seen = stamp[next] == current_stamp;
            if (seen and (from[next] & closed_bit) != 0) continue;
            const step_cost = (if (is_diagonal) diagonal else straight) * weight;
            const next_cost = cost[node] +| step_cost;
            if (seen and next_cost >= cost[next]) continue;
            const h = octile(request.goal_x - nx, request.goal_y - ny) *| min_weight;
            cost[next] = next_cost;
            estimate[next] = next_cost +| h;
            steps[next] = steps[node] + 1;
            from[next] = @intCast(k);
            if (seen) {
                siftUp(heap_pos[next]);
            } else {
                stamp[next] = current_stamp;
                push(next);
            }
        }
    } else {
        end = best;
    }

    // Walk back from the end along the recorded steps.
    const length: usize = steps[end] + 1;
    var at = end;
    var i = length;
    while (i > 0) {
        i -= 1;
        out[i] = .{ .x = @intCast(at % width), .y = @intCast(at / width), .cost = cost[at] };
        const dir = from[at] & ~closed_bit;
        if (dir == no_dir) break;
        const back = dirs[dir];
        const px: i32 = @as(i32, @intCast(at % width)) - back[0];
        const py: i32 = @as(i32, @intCast(at / width)) - back[1];
        at = @intCast(@as(usize, @intCast(py)) * width + @as(usize, @intCast(px)));
    }
    return Result{ .status = @intFromEnum(status), .length = @intCast(length), .expanded = @intCast(expanded) };
}

/// The C ABI (FastestPath.h).
export fn bk_path_find(grid: *const Grid, request: *const Request, out: [*]PathPoint, out_capacity: i32, result: *Result) void {
    if (out_capacity <= 0) {
        result.* = .{ .status = @intFromEnum(Status.invalid), .length = 0, .expanded = 0 };
        return;
    }
    result.* = find(grid.*, request.*, out[0..@intCast(out_capacity)]);
}

// ---------------------------------------------------------------------------------------------
// Tests on fixture grids. A fixture is a picture of the grid, one character a tile:
//   '.' field (weight 384: speed 2/3), '=' road (weight 256: full speed), '#' blocked,
//   '~' swamp (weight 1024), 'S' start and 'G' goal on field.

const field_weight: u16 = 384;
const road_weight: u16 = 256;
const swamp_weight: u16 = 1024;

const Fixture = struct {
    cells: [64 * 64]u16 = undefined,
    width: i32 = 0,
    height: i32 = 0,
    start: [2]i32 = .{ 0, 0 },
    goal: [2]i32 = .{ 0, 0 },

    fn parse(rows: []const []const u8) Fixture {
        var f = Fixture{};
        f.height = @intCast(rows.len);
        f.width = @intCast(rows[0].len);
        for (rows, 0..) |row, y| {
            for (row, 0..) |c, x| {
                const at = y * rows[0].len + x;
                f.cells[at] = switch (c) {
                    '#' => 0,
                    '=' => road_weight,
                    '~' => swamp_weight,
                    else => field_weight,
                };
                if (c == 'S') f.start = .{ @intCast(x), @intCast(y) };
                if (c == 'G') f.goal = .{ @intCast(x), @intCast(y) };
            }
        }
        return f;
    }

    /// Blocks every tile closer than radius + 1 to a blocked one, as the static map's maxes do
    /// for a unit of that bound radius (CStaticMap::CanUnitGo).
    fn inflate(self: *Fixture, radius: i32) void {
        var blocked: [64 * 64]bool = undefined;
        const w: usize = @intCast(self.width);
        const n = w * @as(usize, @intCast(self.height));
        for (0..n) |i| blocked[i] = self.cells[i] == 0;
        for (0..n) |i| {
            const x: i32 = @intCast(i % w);
            const y: i32 = @intCast(i / w);
            var dy = -radius;
            while (dy <= radius) : (dy += 1) {
                var dx = -radius;
                while (dx <= radius) : (dx += 1) {
                    const sx = x + dx;
                    const sy = y + dy;
                    if (sx < 0 or sy < 0 or sx >= self.width or sy >= self.height) {
                        self.cells[i] = 0;
                        continue;
                    }
                    if (blocked[@as(usize, @intCast(sy)) * w + @as(usize, @intCast(sx))]) self.cells[i] = 0;
                }
            }
        }
    }

    fn run(self: *const Fixture, out: []PathPoint) Result {
        return find(.{ .cells = &self.cells, .width = self.width, .height = self.height }, .{
            .start_x = self.start[0],
            .start_y = self.start[1],
            .goal_x = self.goal[0],
            .goal_y = self.goal[1],
            .max_steps = 5000,
            .node_budget = 1 << 20,
        }, out);
    }

    fn weightAt(self: *const Fixture, x: i32, y: i32) u16 {
        return self.cells[@as(usize, @intCast(y)) * @as(usize, @intCast(self.width)) + @as(usize, @intCast(x))];
    }
};

fn expectConnected(path: []const PathPoint) !void {
    for (path[1..], 0..) |p, i| {
        try std.testing.expect(@abs(p.x - path[i].x) <= 1 and @abs(p.y - path[i].y) <= 1);
        try std.testing.expect(p.x != path[i].x or p.y != path[i].y);
    }
}

test "road beside a field: the longer way on the road is cheaper" {
    // Straight across the field is 16 tiles at 384; down to the road, along it and back up
    // is 20 tiles, 16 of them at 256.
    const f = Fixture.parse(&.{
        "S................G",
        "..................",
        "==================",
    });
    var out: [256]PathPoint = undefined;
    const r = f.run(&out);
    try std.testing.expectEqual(@intFromEnum(Status.goal), r.status);
    const path = out[0..@intCast(r.length)];
    try expectConnected(path);
    var on_road: usize = 0;
    for (path) |p| {
        if (f.weightAt(p.x, p.y) == road_weight) on_road += 1;
    }
    try std.testing.expect(on_road >= 12);
    // The straight line across the field would cost 17 * 70 * 384.
    try std.testing.expect(path[path.len - 1].cost < 17 * straight * field_weight);
}

test "swamp: a detour round it beats the straight line" {
    const f = Fixture.parse(&.{
        "..........",
        "S~~~~~~~~G",
        "..........",
    });
    var out: [64]PathPoint = undefined;
    const r = f.run(&out);
    try std.testing.expectEqual(@intFromEnum(Status.goal), r.status);
    for (out[1 .. @as(usize, @intCast(r.length)) - 1]) |p| try std.testing.expect(p.y != 1);
}

test "wall: the path goes round through the gap" {
    const f = Fixture.parse(&.{
        "..........",
        "....#.....",
        "S...#....G",
        "....#.....",
        "....#.....",
        "..........",
    });
    var out: [64]PathPoint = undefined;
    const r = f.run(&out);
    try std.testing.expectEqual(@intFromEnum(Status.goal), r.status);
    const path = out[0..@intCast(r.length)];
    try expectConnected(path);
    for (path) |p| try std.testing.expect(f.weightAt(p.x, p.y) != 0);
    // Over the top (row 0) is shorter than under the bottom (row 5).
    var crossed_top = false;
    for (path) |p| {
        if (p.x == 4) crossed_top = p.y == 0;
    }
    try std.testing.expect(crossed_top);
}

test "no corner cutting between two blocked tiles" {
    const f = Fixture.parse(&.{
        "S#.",
        "#..",
        "..G",
    });
    var out: [64]PathPoint = undefined;
    const r = f.run(&out);
    // The only way out of the corner is the diagonal between the two walls.
    try std.testing.expectEqual(@intFromEnum(Status.closest), r.status);
    try std.testing.expectEqual(@as(i32, 1), r.length);
}

test "unreachable goal: the path ends on the nearest reachable tile" {
    const f = Fixture.parse(&.{
        "S.....#...",
        "......#.G.",
        "......#...",
    });
    var out: [64]PathPoint = undefined;
    const r = f.run(&out);
    try std.testing.expectEqual(@intFromEnum(Status.closest), r.status);
    const end = out[@as(usize, @intCast(r.length)) - 1];
    try std.testing.expectEqual(@as(i32, 5), end.x);
    try std.testing.expectEqual(@as(i32, 1), end.y);
}

test "blocked goal: the path stops beside it" {
    var f = Fixture.parse(&.{
        "S.......",
        "........",
        "......G.",
    });
    f.cells[2 * 8 + 6] = 0;
    var out: [64]PathPoint = undefined;
    const r = f.run(&out);
    try std.testing.expectEqual(@intFromEnum(Status.beside_goal), r.status);
    const end = out[@as(usize, @intCast(r.length)) - 1];
    try std.testing.expect(@abs(end.x - 6) <= 1 and @abs(end.y - 2) <= 1);
}

test "bound radius: a big unit goes round the gap a small one takes" {
    // A wall with a gap one tile wide under the start and one three tiles wide further on.
    const rows = [_][]const u8{
        "................",
        ".......S........",
        "................",
        "#######.###...##",
        "................",
        "................",
        ".......G........",
        "................",
    };
    const small = Fixture.parse(&rows);
    var big = Fixture.parse(&rows);
    big.inflate(1);
    var out_small: [256]PathPoint = undefined;
    var out_big: [256]PathPoint = undefined;
    const rs = small.run(&out_small);
    const rb = big.run(&out_big);
    try std.testing.expectEqual(@intFromEnum(Status.goal), rs.status);
    try std.testing.expectEqual(@intFromEnum(Status.goal), rb.status);
    for (out_small[0..@intCast(rs.length)]) |p| {
        if (p.y == 3) try std.testing.expectEqual(@as(i32, 7), p.x);
    }
    for (out_big[0..@intCast(rb.length)]) |p| {
        try std.testing.expect(big.weightAt(p.x, p.y) != 0);
        if (p.y == 3) try std.testing.expectEqual(@as(i32, 12), p.x);
    }
}

test "max steps: no path longer than the limit" {
    const f = Fixture.parse(&.{"S.................G"});
    var out: [64]PathPoint = undefined;
    const r = find(.{ .cells = &f.cells, .width = f.width, .height = f.height }, .{
        .start_x = f.start[0],
        .start_y = f.start[1],
        .goal_x = f.goal[0],
        .goal_y = f.goal[1],
        .max_steps = 5,
        .node_budget = 1 << 20,
    }, &out);
    try std.testing.expectEqual(@intFromEnum(Status.closest), r.status);
    try std.testing.expectEqual(@as(i32, 6), r.length);
}

test "good tile ends the search" {
    var f = Fixture.parse(&.{
        "S.........",
        "..........",
        ".........G",
    });
    // One good tile on the way, one off it: the search takes the one it meets first.
    f.cells[2 * 10 + 5] |= cell_good;
    f.cells[0 * 10 + 8] |= cell_good;
    var out: [64]PathPoint = undefined;
    const r = f.run(&out);
    try std.testing.expectEqual(@intFromEnum(Status.good), r.status);
    const end = out[@as(usize, @intCast(r.length)) - 1];
    try std.testing.expectEqual(@as(i32, 5), end.x);
    try std.testing.expectEqual(@as(i32, 2), end.y);
}

test "budget: a search that runs out says so" {
    const f = Fixture.parse(&.{
        "S.........",
        "..........",
        ".........G",
    });
    var out: [64]PathPoint = undefined;
    const r = find(.{ .cells = &f.cells, .width = f.width, .height = f.height }, .{
        .start_x = f.start[0],
        .start_y = f.start[1],
        .goal_x = f.goal[0],
        .goal_y = f.goal[1],
        .max_steps = 5000,
        .node_budget = 3,
    }, &out);
    try std.testing.expectEqual(@intFromEnum(Status.budget), r.status);
}

test "determinism: the same request gives the same path, also after other searches" {
    // An open field: many paths cost the same, so the tie-break decides.
    const f = Fixture.parse(&.{
        "S...........",
        "............",
        "............",
        "............",
        "...........G",
    });
    var first: [64]PathPoint = undefined;
    var again: [64]PathPoint = undefined;
    var other: [64]PathPoint = undefined;
    const r1 = f.run(&first);
    var g = f;
    g.goal = .{ 3, 4 };
    _ = g.run(&other);
    const r2 = f.run(&again);
    try std.testing.expectEqual(r1.length, r2.length);
    try std.testing.expectEqual(r1.expanded, r2.expanded);
    for (first[0..@intCast(r1.length)], again[0..@intCast(r2.length)]) |a, b| {
        try std.testing.expectEqual(a.x, b.x);
        try std.testing.expectEqual(a.y, b.y);
        try std.testing.expectEqual(a.cost, b.cost);
    }
    // Optimal: 4 diagonal and 7 straight steps on the field.
    try std.testing.expectEqual((4 * diagonal + 7 * straight) * field_weight, first[@as(usize, @intCast(r1.length)) - 1].cost);
}

test "a start on a blocked tile still leaves it" {
    var f = Fixture.parse(&.{
        "S....G",
    });
    f.cells[0] = 0;
    var out: [64]PathPoint = undefined;
    const r = f.run(&out);
    try std.testing.expectEqual(@intFromEnum(Status.goal), r.status);
    try std.testing.expectEqual(@as(i32, 6), r.length);
}
