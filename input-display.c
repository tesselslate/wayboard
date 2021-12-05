// input-display is a small application
// to display your keyboard inputs on X11.

#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <stdint.h>
#include <stdio.h>

// config types
typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned int w;
    unsigned int h;
    unsigned int keycode;
} kbd_element;

typedef struct {
    uint8_t ar;
    uint8_t ag;
    uint8_t ab;
    uint8_t ir;
    uint8_t ig;
    uint8_t ib;

    uint8_t element_count;
    kbd_element elements[255];
} kbd_config;

// global state
kbd_config config;
xcb_connection_t* connection;
uint8_t keymap[255];

SDL_Renderer* renderer;
SDL_Window* window;

// functions
int get_keymap(xcb_connection_t* conn) {
    // query keymap state
    xcb_query_keymap_cookie_t cookie = xcb_query_keymap(conn);
    xcb_generic_error_t** error = NULL;
    xcb_query_keymap_reply_t* reply = xcb_query_keymap_reply(conn, cookie, error);

    // update keymap array
    if (reply != NULL) {
        for (int i = 0; i < 32; i++) {
            uint8_t keybyte = reply->keys[i];
            for (int b = 0; b < 8; b++) {
                keymap[i * 8 + b] = keybyte & (1 << b);
            }
        }

        return 0;
    } else {
        fprintf(stderr, "xcb_query_keymap_reply returned a null response.");
        return 1;
    }
}

int sdl_setup() {
    // initialize SDL
    int result = SDL_Init(
            SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO);
    
    if (result != 0) {
        const char* err = SDL_GetError();
        fprintf(stderr, "SDL_Init error: %s", err);
        return 1;
    }

    // set window hints
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    // create window
    window = SDL_CreateWindow(
            "Input Display", 
            SDL_WINDOWPOS_CENTERED, 
            SDL_WINDOWPOS_CENTERED, 
            160, 160, 0);

    if (window == NULL) {
        const char* err = SDL_GetError();
        fprintf(stderr, "SDL_CreateWindow error: %s", err);
        return 1;
    }

    // create renderer
    renderer = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_PRESENTVSYNC);

    if (renderer == NULL) {
        const char* err = SDL_GetError();
        fprintf(stderr, "SDL_CreateRenderer error: %s", err);
        return 1;
    }

    return 0;
}

void sdl_loop() {
    SDL_Event evt;

    while (1) {
        while (SDL_PollEvent(&evt) != 0) {
            if (evt.type == SDL_QUIT) {
                return;
            }
        }

        // update keymap
        get_keymap(connection);

        // render display
        SDL_SetRenderDrawColor(renderer, config.ir, config.ig, config.ib, 255);
        SDL_RenderClear(renderer);

        for (int i = 0; i < config.element_count; i++) {
            kbd_element element = config.elements[i];

            if (keymap[element.keycode] != 0) {
                SDL_Rect rect;
                rect.x = element.x;
                rect.y = element.y;
                rect.w = element.w;
                rect.h = element.h;

                SDL_SetRenderDrawColor(renderer, config.ar, config.ag, config.ab, 255);
                SDL_RenderFillRect(renderer, &rect);
            }
        }
        
        SDL_RenderPresent(renderer);
    }
}

void parse_hex(unsigned int color, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = ((color >> 16) & 0xFF);
    *g = ((color >> 8)  & 0xFF);
    *b = ((color)       & 0xFF);
}

// run
int main(int argc, char* argv[]) {
    // parse config args
    for (int i = 1; i < argc; i++) {
        if (i == 1) {
            // top-level configuration
            config.element_count = argc - 2;
            
            unsigned int keycolor, bgcolor;
            int scanned = sscanf(argv[1], "key=%x,background=%x", &keycolor, &bgcolor);

            if (scanned != 2) {
                fprintf(
                        stderr, 
                        "Failed to read initial configuration element. Scanned %u of 2 values\n", 
                        scanned);

                return 1;
            }

            parse_hex(keycolor, &config.ar, &config.ag, &config.ab);
            parse_hex(bgcolor, &config.ir, &config.ig, &config.ib);
        } else {
            // keyboard element
            kbd_element element;
            int scanned = sscanf(
                    argv[i], "key=%u,x=%u,y=%u,w=%u,h=%u",
                    &element.keycode,
                    &element.x,
                    &element.y,
                    &element.w,
                    &element.h);

            if (scanned != 5) {
                fprintf(
                        stderr, 
                        "Failed to read element %u: scanned %u of 5 variables\n", 
                        i, scanned);

                fprintf(stderr, "%s", argv[i]);
                return 1;
            }

            config.elements[i - 2] = element;
        }
    }

    if (argc < 3) {
        fprintf(stderr, "Configuration not specified.\n");
        return 1;
    }

    // establish xorg connection
    connection = xcb_connect(NULL, NULL);

    // sdl initialization
    int sdl_result = sdl_setup();
    if (sdl_result == 1) {
        // if sdl initialization failed, terminate
        xcb_disconnect(connection);
        SDL_Quit();
        return 1;
    }

    // start display loop
    sdl_loop();

    // dispose of resources and terminate
    xcb_disconnect(connection);
    SDL_Quit();

    return 0;
}
