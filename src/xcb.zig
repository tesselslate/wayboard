const c = @cImport({
    @cInclude("xcb/xproto.h");
});

pub const Connection = c.xcb_connection_t;
pub const GenericError = c.xcb_generic_error_t;
pub const QueryKeymapCookie = c.xcb_query_keymap_cookie_t;
pub const QueryKeymapResult = c.xcb_query_keymap_reply_t;

pub const Connect = c.xcb_connect;
pub const Disconnect = c.xcb_disconnect;
pub const QueryKeymap = c.xcb_query_keymap;
pub const QueryKeymapReply = c.xcb_query_keymap_reply;
