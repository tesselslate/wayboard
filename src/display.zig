const std = @import("std");
const keyboard = @import("keyboard.zig");
const sdl = @import("lib/sdl.zig");

pub const DisplayError = error {
    SDL_Init,
    SDL_Renderer,
    SDL_Window
};

pub const Display = struct {
    keyboard: keyboard.Keyboard,
    renderer: ?*sdl.Renderer,
    running: bool,
    window: ?*sdl.Window,

    pub fn loop(self: *Display) !void {
        self.running = true;
        var evt: sdl.Event = undefined;

        while (self.running) {
            while (sdl.PollEvent(&evt) == 1) {
                var evt_type = @intToEnum(sdl.EventType, evt.@"type");

                switch (evt_type) {
                    sdl.EventType.Quit => self.running = false,
                    else => {}
                }
            }

            // update pressed keys
            try self.keyboard.captureKeyboard();

            // render input display
            _ = sdl.RenderClear(self.renderer);
            sdl.RenderPresent(self.renderer);
        }
    }

    pub fn run(self: *Display) !void {
        // sdl initialization
        var init_result = sdl.Init(sdl.INIT_EVENTS | sdl.INIT_TIMER | sdl.INIT_VIDEO);
        defer sdl.Quit();

        if (init_result != 0) {
            std.debug.print("SDL_Init error: {s}", .{sdl.GetError()});
            return DisplayError.SDL_Init;
        }

        // set hints
        _ = sdl.SetHint(sdl.HintBypassCompositor, "0");
        _ = sdl.SetHint(sdl.HintRenderVsync, "1");

        // window initialization
        self.window = sdl.CreateWindow(
            "Input Display",
            sdl.WindowPosCentered, 
            sdl.WindowPosCentered, 
            160, 160, 
            sdl.WindowResizable
        ) orelse {
            std.debug.print("SDL_CreateWindow error: {s}", .{sdl.GetError()});
            return DisplayError.SDL_Window;
        };
        defer sdl.DestroyWindow(self.window);

        // renderer initialization
        self.renderer = sdl.CreateRenderer(self.window, -1, 0) orelse {
            std.debug.print("SDL_CreateRenderer error: {s}", .{sdl.GetError()});
            return DisplayError.SDL_Renderer;
        };
        defer sdl.DestroyRenderer(self.renderer);

        // main loop
        try loop(self);
    }
};
