const c = @cImport({
    @cInclude("SDL2/SDL.h");
});

// SDL.h
pub const INIT_EVENTS   = c.SDL_INIT_EVENTS;
pub const INIT_TIMER    = c.SDL_INIT_TIMER;
pub const INIT_VIDEO    = c.SDL_INIT_VIDEO;

pub const Init          = c.SDL_Init;
pub const Quit          = c.SDL_Quit;

// SDL_error.h
pub const GetError      = c.SDL_GetError;

// SDL_events.h
pub const Event         = c.SDL_Event;
pub const PollEvent     = c.SDL_PollEvent;
pub const EventType = enum(u32) {
    Quit = c.SDL_QUIT,
    AppTerminating = c.SDL_APP_TERMINATING,
    AppLowMemory = c.SDL_APP_LOWMEMORY,
    
    WillEnterBackground = c.SDL_APP_WILLENTERBACKGROUND,
    DidEnterBackground = c.SDL_APP_DIDENTERBACKGROUND,
    WillEnterForeground = c.SDL_APP_WILLENTERFOREGROUND,
    DidEnterForeground = c.SDL_APP_DIDENTERFOREGROUND,
    
    WindowEvent = c.SDL_WINDOWEVENT,
    SysWmEvent = c.SDL_SYSWMEVENT,

    KeyDown = c.SDL_KEYDOWN,
    KeyUp = c.SDL_KEYUP,
    TextEditing = c.SDL_TEXTEDITING,
    TextInput = c.SDL_TEXTINPUT,
    KeymapChanged = c.SDL_KEYMAPCHANGED,

    MouseMotion = c.SDL_MOUSEMOTION,
    MouseButtonDown = c.SDL_MOUSEBUTTONDOWN,
    MouseButtonUp = c.SDL_MOUSEBUTTONUP,
    MouseWheel = c.SDL_MOUSEWHEEL,

    DropFile = c.SDL_DROPFILE,
    DropText = c.SDL_DROPTEXT,
    DropBegin = c.SDL_DROPBEGIN,
    DropComplete = c.SDL_DROPCOMPLETE,

    RenderTargetsReset = c.SDL_RENDER_TARGETS_RESET,
    RenderDeviceReset = c.SDL_RENDER_DEVICE_RESET
};

// SDL_hints.h
pub const HintBypassCompositor  = c.SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR;
pub const HintRenderVsync       = c.SDL_HINT_RENDER_VSYNC;
pub const SetHint               = c.SDL_SetHint;

// SDL_rect.h
pub const Rect                  = c.SDL_Rect;

// SDL_render.h
pub const CreateRenderer        = c.SDL_CreateRenderer;
pub const DestroyRenderer       = c.SDL_DestroyRenderer;
pub const Renderer              = c.SDL_Renderer;
pub const RenderClear           = c.SDL_RenderClear;
pub const RenderPresent         = c.SDL_RenderPresent;

pub const RenderFillRects       = c.SDL_RenderFillRects;
pub const SetRenderDrawColor    = c.SDL_SetRenderDrawColor;

// SDL_video.h
pub const CreateWindow          = c.SDL_CreateWindow;
pub const DestroyWindow         = c.SDL_DestroyWindow;
pub const Window                = c.SDL_Window;
pub const WindowResizable       = c.SDL_WINDOW_RESIZABLE;
pub const WindowPosCentered     = c.SDL_WINDOWPOS_CENTERED;
