const std = @import("std");
const xcb = @import("lib/xcb.zig");

pub const Keyboard = struct {
    connection: ?*xcb.Connection,
    keys: [256]bool,

    pub fn captureKeyboard(self: *Keyboard) !void {
        if (self.connection) |xcb_conn| {
            var result: ?xcb.QueryKeymapCookie = xcb.QueryKeymap(xcb_conn);
            if (result) |res| {
                // this error should probably be handled but i don't know how to
                // the documentation for xcb is Not Good
                //
                // the manpage for xcb_query_keymap_reply says it shouldn't error
                // (hopefully)
                var err: [*c][*c]xcb.GenericError = null;

                var result2: ?*xcb.QueryKeymapResult = xcb.QueryKeymapReply(xcb_conn, res, err);
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
                    std.log.crit("xcb_query_keymap_reply returned a null response", .{});
                    return error.XCB_ResponseNull;
                }
            } else {
                std.log.crit("xcb_query_keymap returned a null response", .{});
                return error.XCB_CookieNull;
            }
        } else {
            std.log.crit("xcb_connection was null", .{});
            return error.XCB_ConnectionNull;
        }

        return;
    }
};
