const xcb = @import("xcb.zig");
const std = @import("std");

pub const Keyboard = struct {
    keys: [256]bool,

    pub fn captureKeyboard(self: *Keyboard, conn: ?*xcb.Connection) void {
        if (conn) |xcb_conn| {
            var result: ?xcb.QueryKeymapCookie = xcb.QueryKeymap(conn);
            if (result) |res| {
                var err: [*c][*c]xcb.GenericError = null;
                var result2: ?*xcb.QueryKeymapResult = xcb.QueryKeymapReply(conn, res, err);

                if (result2) |res2| {
                    for (res2.keys) |byte, index| {
                        var i: u3 = 0;
                        const one: u8 = 1;

                        while (i <= 7) {
                            self.keys[index * 8 + i] = byte & (one << i) != 0;

                            if (i == 7) break;
                            i += 1;
                        }
                    }
                } else {
                    // error
                }
            } else {
                // error
            }
        } else {
            // error
        }

        return;
    }
};
