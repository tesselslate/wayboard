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

pub fn readFromFile(filename: []const u8, alloc: *std.mem.Allocator) !Config {
    // convert path to absolute path, if necessary
    var filepath: []const u8 = undefined;

    if (std.fs.path.isAbsolute(filename)) {
        filepath = filename;
    } else {
        const cwd = try std.process.getCwdAlloc(alloc);

        filepath = try std.fs.path.join(alloc, &[_][]const u8 {
            cwd, filename
        });
    }

    const file = try std.fs.openFileAbsolute(filepath, .{ .read = true });
    const file_content = try file.reader().readAllAlloc(alloc, 1048576);
    defer alloc.free(file_content);

    var valid = std.json.validate(file_content);
    if (!valid) {
        std.log.crit("Configuration file contains invalid JSON", .{});
        return error.InvalidJson;
    }

    var stream = std.json.TokenStream.init(file_content);
    var result = try std.json.parse(Config, &stream, .{ .allocator = alloc });

    return result;
}
