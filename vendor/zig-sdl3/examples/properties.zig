const sdl3 = @import("sdl3");
const std = @import("std");

/// Simple property to string example function.
fn printPropertyType(
    typ: ?sdl3.properties.Type,
) []const u8 {
    if (typ) |val|
        return switch (val) {
            .pointer => "pointer",
            .string => "string",
            .number => "number",
            .float => "float",
            .boolean => "boolean",
        };
    return "[does not exist]";
}

/// Callback for cleaning up an array property.
fn arrayCleanupCallback(
    user_data: ?*void,
    val: *std.array_list.Managed(u32),
) void {
    _ = user_data;
    val.deinit();
}

const PrintItemsContext = struct { index: usize, io: std.Io };

/// Callback to print items in a properties group.
fn printItems(
    user_data: ?*PrintItemsContext,
    props: sdl3.properties.Group,
    name: [:0]const u8,
) void {
    const index = &user_data.?.index;
    const io = user_data.?.io;

    var stdout_buffer: [256]u8 = undefined;
    var stdout_writer = std.Io.File.stdout().writer(io, &stdout_buffer);
    var stderr_buffer: [256]u8 = undefined;
    var stderr_writer = std.Io.File.stderr().writer(io, &stderr_buffer);

    stdout_writer.interface.print("Index: {d}, Name: \"{s}\", Type: {s}\n", .{
        index.*,
        name,
        printPropertyType(props.getType(name)),
    }) catch stderr_writer.interface.print("Standard writer error\n", .{}) catch {};

    stdout_writer.interface.flush() catch {};
    stderr_writer.interface.flush() catch {};
    index.* += 1;
}

pub fn main(init: std.process.Init) !void {
    const allocator = init.gpa;
    const io = init.io;

    // Example setting some properties.
    const properties = try sdl3.properties.Group.init();
    defer properties.deinit();
    var num: u32 = 3;
    try properties.set("myBool", .{ .boolean = true });
    try properties.set("myNum", .{ .number = 7 });
    try properties.set("myNumPtr", .{ .pointer = &num });
    try properties.set("myStr", .{ .string = "Hello World!" });

    // Set an array list property with automatic cleanup.
    var arr = std.array_list.Managed(u32).init(allocator);
    try properties.setPointerPropertyWithCleanup("myArr", std.array_list.Managed(u32), &arr, void, arrayCleanupCallback, null);

    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer = std.Io.File.stdout().writer(io, &stdout_buffer);
    var writer = stdout_file_writer.interface;
    try writer.print("Type of \"myStr\" is {s}\n", .{printPropertyType(properties.getType("myStr"))});
    try writer.print("Type of \"isNotThere\" is {s}\n\n", .{printPropertyType(properties.getType("isNotThere"))});

    // Show enumerating properties with a callback.
    var context: PrintItemsContext = .{ .index = 0, .io = io };
    try properties.enumerateProperties(
        PrintItemsContext,
        printItems,
        &context,
    );

    // Show results after copying properties to the global ones.
    try writer.print("\nNotice that since \"myArr\" has a custom deleter that it is not present!\n", .{});
    const global_properties = try sdl3.properties.getGlobal();
    try properties.copyTo(global_properties);
    var set = try global_properties.getAll(allocator);
    defer set.deinit();
    var iterator = set.iterator();
    context.index = 0;
    while (iterator.next()) |item| {
        try writer.print("Index: {d}, Name: \"{s}\", Type: {s}\n", .{ context.index, item.key_ptr.*, printPropertyType(global_properties.getType(@ptrCast(item.key_ptr.*))) });
        context.index += 1;
    }

    // Clearing properties example.
    try writer.print("\nYou can clear items\n", .{});
    context.index = 0;
    try properties.clear("myNumPtr");
    try properties.clear("myStr");
    try properties.enumerateProperties(PrintItemsContext, printItems, &context);

    // Fetching properties example.
    if (properties.get("myBool")) |val| {
        try writer.print("\nValue of \"myBool\" is {s}\n", .{if (val.boolean) "true" else "false"});
    }
    if (properties.get("myStr")) |val| {
        try writer.print("\nValue of \"myStr\" is {s}\n", .{val.string}); // Will not print.
    }

    try stdout_file_writer.interface.flush();
}
