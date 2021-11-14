const std = @import("std");

pub const Color = struct {
    r: u8,
    g: u8,
    b: u8,
    a: u8
};

pub const Config = struct {
    background: Color,
    keys: []Element
};

pub const Element = struct {
    pressed: Color,
    unpressed: Color,
    keycode: u8,
    x: u16,
    y: u16,
    w: u16,
    h: u16
};

pub fn readFromFile(filename: []const u8) !Config {
    var alloc = std.heap.c_allocator;

    var file = try std.fs.openFileAbsolute(filename, .{ .read = true });
    const file_content = try file.reader().readAllAlloc(alloc, 1048576);
    defer alloc.free(file_content);

    var valid = std.json.validate(file_content);
    if (!valid) {
        std.debug.print("Invalid configuration file!", .{});
        return error.InvalidJson;
    }

    var stream = std.json.TokenStream.init(file_content);
    var result = try std.json.parse(Config, &stream, .{ .allocator = alloc });
    std.json.parseFree(Config, result, .{ .allocator = alloc });

    return result;
}
