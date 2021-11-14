const std = @import("std");

const keyboard = @import("keyboard.zig");
const xcb = @import("lib/xcb.zig");

pub fn main() !void {
    // set up xcb connection
    var display: ?*u8 = null;
    var screen: ?*c_int = null;

    var conn: ?*xcb.Connection = xcb.Connect(display, screen);
    var kbd = keyboard.Keyboard { 
        .keys = [_]bool {false} ** 256
    };

    defer xcb.Disconnect(conn);
}
