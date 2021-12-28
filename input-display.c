/* ----------------------------
   input-display
   A small, simple application
   for displaying keyboard
   inputs on Linux.
   ----------------------------
 */

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

// ----------------------------
// configuration types
// ----------------------------

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;

    uint8_t keycode;
    uint8_t active[3];
    uint8_t inactive[3];
} kbd_element;

typedef struct {
    uint8_t background[3];

    uint8_t element_count;
    kbd_element elements[256];
} kbd_config;

// ----------------------------
// global state
// ----------------------------

kbd_config config;
xcb_connection_t* connection;
uint8_t keymap[256];

SDL_Renderer* renderer;
SDL_Window* window;

// ----------------------------
// x11 keyboard grabber
// ----------------------------

int get_keymap(xcb_connection_t* conn) {
    // query keymap state
    xcb_query_keymap_cookie_t cookie = xcb_query_keymap(conn);
    xcb_generic_error_t* error = NULL;
    xcb_query_keymap_reply_t* reply = xcb_query_keymap_reply(conn, cookie, &error);

    // update keymap array
    if (reply != NULL) {
        for (int i = 0; i < 32; i++) {
            uint8_t keybyte = reply->keys[i];
            for (int b = 0; b < 8; b++) {
                keymap[i * 8 + b] = keybyte & (1 << b);
            }
        }

        free(reply);
        return 0;
    } else {
        fprintf(stderr, "xcb_query_keymap_reply returned a null response.");
        return 1;
    }
}

// ----------------------------
// sdl functionality
// ----------------------------

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
        SDL_SetRenderDrawColor(renderer,
                config.background[0],
                config.background[1],
                config.background[2],
                255);

        SDL_RenderClear(renderer);

        for (int i = 0; i < config.element_count; i++) {
            kbd_element element = config.elements[i];

            SDL_Rect rect;
            rect.x = element.x;
            rect.y = element.y;
            rect.w = element.w;
            rect.h = element.h;

            if (keymap[element.keycode] != 0) {
                SDL_SetRenderDrawColor(renderer,
                        element.active[0],
                        element.active[1],
                        element.active[2],
                        255);
            } else {
                SDL_SetRenderDrawColor(renderer,
                        element.inactive[0],
                        element.inactive[1],
                        element.inactive[2],
                        255);
            }

            SDL_RenderFillRect(renderer, &rect);
        }
        
        SDL_RenderPresent(renderer);
    }
}

// ----------------------------
// configuration parser
// ----------------------------

void parse_hex(unsigned int color, uint8_t* out) {
    out[0] = ((color >> 16) & 0xFF);
    out[1] = ((color >> 8)  & 0xFF);
    out[2] = ((color)       & 0xFF);
}

// run
int main() {
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
