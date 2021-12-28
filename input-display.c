/* ----------------------------
   input-display
   A small, simple application
   for displaying keyboard
   inputs on Linux.
   ----------------------------
 */

#include <libconfig.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <stdlib.h>
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

void parse_hex(const char* in, uint8_t* out) {
    unsigned int r, g, b;
    sscanf(in, "%2x%2x%2x", &r, &g, &b);

    out[0] = (uint8_t) r;
    out[1] = (uint8_t) g;
    out[2] = (uint8_t) b;
}

int parse_config(config_t *conf) {
    const char *string;
    kbd_config new_conf;

    // read background color
    if (config_lookup_string(conf, "background", &string)) {
        parse_hex(string, new_conf.background);
    } else {
        fprintf(stderr, "Could not find a background color in configuration.\n");
        config_destroy(conf);
        return 1;
    }

    // read keys
    config_setting_t *keys = config_lookup(conf, "keys");
    if (keys != NULL) {
        int keycount = config_setting_length(keys);
        new_conf.element_count = keycount;

        for (int i = 0; i < keycount; i++) {
            config_setting_t *key = config_setting_get_elem(keys, i);
            const char *active, *inactive;
            int x, y, w, h, keycode;

            if (config_setting_lookup_string(key, "inactive", &inactive) &&
                config_setting_lookup_string(key, "active", &active) &&
                config_setting_lookup_int(key, "x", &x) &&
                config_setting_lookup_int(key, "y", &y) &&
                config_setting_lookup_int(key, "w", &w) &&
                config_setting_lookup_int(key, "h", &h) &&
                config_setting_lookup_int(key, "keycode", &keycode)) {
                // put values into keyboard element
                kbd_element *el = &new_conf.elements[i];

                parse_hex(active, el->active);
                parse_hex(inactive, el->inactive);
                el->x = (uint8_t) x;
                el->y = (uint8_t) y;
                el->w = (uint8_t) w;
                el->h = (uint8_t) h;
                el->keycode = (uint8_t) keycode;
            } else {
                fprintf(stderr, "Failed to read key %i from configuration.\n", i);
                config_destroy(conf);
                return 1;
            }
        }
    } else {
        fprintf(stderr, "Could not find a 'keys' element in configuration.\n");
        config_destroy(conf);
        return 1;
    }

    config = new_conf;
    return 0;
}

int read_confing_stream(FILE* stream) {
    config_t conf;
    config_init(&conf);

    if (!config_read(&conf, stream)) {
        fprintf(stderr, "Failed to read configuration stream: %s", 
                config_error_text(&conf));
        config_destroy(&conf);
        return 1;
    }

    return parse_config(&conf);
}

int read_config_file(char* path) {
    config_t conf;
    config_init(&conf);

    if (!config_read_file(&conf, path)) {
        fprintf(stderr, "Failed to read configuration file: %s",
                config_error_text(&conf));
        config_destroy(&conf);
        return 1;
    }

    return parse_config(&conf);
}

// ----------------------------
// run
// ----------------------------

int main() {
    read_config_file("./example.cfg");

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
