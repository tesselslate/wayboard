const std = @import("std");
const display   = @import("display.zig");
const keyboard  = @import("keyboard.zig");
const xcb       = @import("lib/xcb.zig");

pub fn main() !void {
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
        .keyboard = kbd,
        .renderer = null,
        .running = true,
        .window = null
    };

    try input_display.run();
}
