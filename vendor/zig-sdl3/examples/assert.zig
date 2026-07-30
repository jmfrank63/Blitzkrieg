const sdl3 = @import("sdl3");
const std = @import("std");

fn printReport() void {
    const report = sdl3.assert.getReport();
    var item: ?*const sdl3.assert.AssertData = if (report) |val| &val else null;
    while (item) |val| {
        std.debug.print("'{s}', {s} ({s}:{d}), triggered {d} times, always ignore: {s}\n", .{
            val.condition orelse "[Unknown Condition]",
            val.function orelse "[Unknown Function]",
            val.filename orelse "[Unknown Filename]",
            val.linenum,
            val.trigger_count,
            if (val.always_ignore) "yes" else "no",
        });
        item = val.next;
    }
}

fn customAssertionHandler(
    assert_data: sdl3.assert.AssertData,
    user_data: ?*void,
) sdl3.assert.State {
    _ = user_data;
    std.debug.print("Assertion encountered! {s}\n", .{assert_data.condition orelse "[Unknown]"});
    return .ignore;
}

pub fn main() !void {
    // Do some assertions.
    // See what happens when playing around with optimization flags!
    while (sdl3.assert.assert(5 == 5, "Obviously true", @src())) {} // While loops necessary for any "repeatable" assertions.
    while (sdl3.assert.assert(3 == 5, "Obviously false", @src())) {}
    while (sdl3.assert.assertAlways(7 == 7, "Another obviously true", @src())) {}
    while (sdl3.assert.assertAlways(std.mem.eql(u8, "Hello", "World"), "Another obviously false", @src())) {}

    // Showcase how we can print a report, reset it, and even have our own callback for handling assertions.
    printReport();
    sdl3.assert.resetReport();
    sdl3.assert.setHandler(void, customAssertionHandler, null);

    while (sdl3.assert.assertRelease(true, "Truth", @src())) {}
    while (sdl3.assert.assertRelease(false, "Lies", @src())) {}
    while (sdl3.assert.assertParanoid(-1 < 0, "The rats made me crazy", @src())) {}
    while (sdl3.assert.assertParanoid(-1 >= 0, "The walls are oozing", @src())) {}
    printReport();
}
