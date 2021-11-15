const std = @import("std");
const config = @import("config.zig");
const keyboard = @import("keyboard.zig");
const sdl = @import("lib/sdl.zig");

pub const Display = struct {
    config: config.Config,
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
            self.keyboard.captureKeyboard() catch |err| {
                std.log.warn("Keyboard capture was not successful: {any}", err);
            };

            // render input display
            _ = sdl.SetRenderDrawColor(
                self.renderer, 
                self.config.background.r, 
                self.config.background.g, 
                self.config.background.b, 
                self.config.background.a
            );
            _ = sdl.RenderClear(self.renderer);

            for (self.config.keys) |key, _| {
                var color: config.Color = undefined;
                if (self.keyboard.keys[key.keycode]) {
                    color = key.pressed;
                } else {
                    color = key.unpressed;
                }
                
                _ = sdl.SetRenderDrawColor(
                    self.renderer,
                    color.r, color.g, color.b, color.a
                );

                var rect = sdl.Rect {
                    .x = key.x,
                    .y = key.y,
                    .w = key.w,
                    .h = key.h
                };

                _ = sdl.RenderFillRect(self.renderer, &rect);
            }

            sdl.RenderPresent(self.renderer);
        }
    }

    pub fn run(self: *Display) !void {
        // sdl initialization
        var init_result = sdl.Init(sdl.INIT_EVENTS | sdl.INIT_TIMER | sdl.INIT_VIDEO);
        defer sdl.Quit();

        if (init_result != 0) {
            std.log.crit("SDL_Init error: {s}", .{ sdl.GetError() });
            return error.SDL_Init;
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
            std.log.crit("SDL_CreateWindow error: {s}", .{ sdl.GetError() });
            return error.SDL_CreateWindow;
        };
        defer sdl.DestroyWindow(self.window);

        // renderer initialization
        self.renderer = sdl.CreateRenderer(self.window, -1, 0) orelse {
            std.log.crit("SDL_CreateRenderer error: {s}", .{ sdl.GetError() });
            return error.SDL_CreateRenderer;
        };
        defer sdl.DestroyRenderer(self.renderer);

        // main loop
        try self.loop();
    }
};
