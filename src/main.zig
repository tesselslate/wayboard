const std       = @import("std");
const config    = @import("config.zig");
const display   = @import("display.zig");
const keyboard  = @import("keyboard.zig");
const xcb       = @import("lib/xcb.zig");

pub fn main() !void {
    // read cli args
    var alloc = std.heap.ArenaAllocator.init(std.heap.c_allocator);
    var a = &alloc.allocator;
    defer alloc.deinit();

    var args = std.process.args();
    _ = args.skip(); // skip executable name
    const config_path = try args.next(a) orelse {
        std.log.crit("No configuration file specified.", .{});
        return;
    };
    
    const cfg = try config.readFromFile(config_path, a);

    // set up xcb connection
    var x_display: ?*u8 = null;
    var screen: ?*c_int = null;

    var conn: ?*xcb.Connection = xcb.Connect(x_display, screen);
    defer xcb.Disconnect(conn);
    
    var kbd = keyboard.Keyboard { 
        .connection = conn,
        .keys = [_]bool {false} ** 256
    };

    var input_display = display.Display {
        .config = cfg,
        .keyboard = kbd,
        .renderer = null,
        .running = true,
        .window = null
    };

    try input_display.run();
}
