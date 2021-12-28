/* ----------------------------
   input-display
   A small, simple application
   for displaying keyboard
   inputs on Linux.
   ---------------------------- */

#include <libconfig.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_video.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// ----------------------------
// configuration types
// ----------------------------

typedef struct {
    const char *text;
    uint8_t active[3];
    uint8_t inactive[3];

    SDL_Texture *texture;
    SDL_Surface *surface;
} text_element;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;

    uint8_t keycode;
    uint8_t active[3];
    uint8_t inactive[3];

    text_element *text;
} kbd_element;

typedef struct {
    uint8_t background[3];
    uint16_t width;
    uint16_t height;

    uint8_t element_count;
    kbd_element elements[256];
} kbd_config;

// ----------------------------
// global state
// ----------------------------

kbd_config config;
xcb_connection_t *connection;
uint8_t keymap[256];

SDL_Renderer *renderer;
SDL_Window *window;

TTF_Font *font;

// ----------------------------
// x11 keyboard grabber
// ----------------------------

int get_keymap(xcb_connection_t *conn) {
    // query keymap state
    xcb_query_keymap_cookie_t cookie = xcb_query_keymap(conn);
    xcb_generic_error_t *error = NULL;
    xcb_query_keymap_reply_t *reply = xcb_query_keymap_reply(conn, cookie, &error);

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
        fprintf(stderr, "xcb_query_keymap_reply returned a null response.\n");
        return 1;
    }
}

// ----------------------------
// sdl functionality
// ----------------------------

int sdl_init() {
    // initialize SDL
    int result = SDL_Init(
            SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_VIDEO);
    
    if (result != 0) {
        const char *err = SDL_GetError();
        fprintf(stderr, "SDL_Init error: %s\n", err);
        return 1;
    }

    result = TTF_Init();
    if (result != 0) {
        const char *err = TTF_GetError();
        fprintf(stderr, "TTF_Init error: %s\n", err);
        return 1;
    }
    
    return 0;
}

int sdl_setup() {
    // set window hints
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    // create window
    window = SDL_CreateWindow(
            "Input Display", 
            SDL_WINDOWPOS_CENTERED, 
            SDL_WINDOWPOS_CENTERED, 
            config.width, config.height, 0);

    if (window == NULL) {
        const char *err = SDL_GetError();
        fprintf(stderr, "SDL_CreateWindow error: %s\n", err);
        return 1;
    }

    // create renderer
    renderer = SDL_CreateRenderer(
            window, -1, SDL_RENDERER_PRESENTVSYNC);

    if (renderer == NULL) {
        const char *err = SDL_GetError();
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", err);
        return 1;
    }

    return 0;
}

void set_color(uint8_t *color) {
    SDL_SetRenderDrawColor(renderer,
            color[0],
            color[1],
            color[2],
            255);
}

void draw_element(kbd_element element) {
    SDL_Rect rect;
    rect.x = element.x;
    rect.y = element.y;
    rect.w = element.w;
    rect.h = element.h;

    uint8_t *r_color;
    uint8_t *t_color;
    if (keymap[element.keycode] != 0) {
        r_color = element.active;
        if (element.text != NULL) {
            t_color = element.text->active;
        }
    } else {
        r_color = element.inactive;
        if (element.text != NULL) {
            t_color = element.text->inactive;
        }
    }

    set_color(r_color);
    SDL_RenderFillRect(renderer, &rect);
    if (element.text != NULL && element.text->texture != NULL) {
        SDL_SetTextureColorMod(element.text->texture, 
                t_color[0], t_color[1], t_color[2]);
        SDL_RenderCopy(renderer, element.text->texture, NULL, &rect);
    }
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
        set_color(config.background);
        SDL_RenderClear(renderer);

        for (int i = 0; i < config.element_count; i++) {
            draw_element(config.elements[i]);
        }
        
        SDL_RenderPresent(renderer);
    }
}

// ----------------------------
// configuration parser
// ----------------------------

void parse_hex(const char *in, uint8_t *out) {
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

    // read window width/height
    int width, height;
    if (config_lookup_int(conf, "width", &width) &&
        config_lookup_int(conf, "height", &height)) {
        new_conf.width = (uint16_t) width;
        new_conf.height = (uint16_t) height;
    } else {
        fprintf(stderr, "Could not find a window width/height in configuration.\n");
        config_destroy(conf);
        return 1;
    }

    // read and load font (if necessary)
    int fontsize;
    if (config_lookup_string(conf, "font", &string) &&
        config_lookup_int(conf, "fontsize", &fontsize)) {
        font = TTF_OpenFont(string, fontsize);
        if (font == NULL) {
            const char *err = TTF_GetError();
            fprintf(stderr, "Could not load font: %s\n", err);
            config_destroy(conf);
            return 1;
        }
    }

    // read keys
    config_setting_t *keys = config_lookup(conf, "keys");
    if (keys != NULL) {
        int keycount = config_setting_length(keys);
        if (keycount > 256) {
            fprintf(stderr, "Amount of keys cannot exceed 256.\n");
            config_destroy(conf);
            return 1;
        }
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

                // check if there is text
                const char *text, *t_active, *t_inactive;
                if (config_setting_lookup_string(key, "text", &text) &&
                    config_setting_lookup_string(key, "text_active", &t_active) &&
                    config_setting_lookup_string(key, "text_inactive", &t_inactive)) {
                    // put text values into keyboard element
                    text_element *text_el = malloc(sizeof(text_element));
                    if (text_el == NULL) {
                        fprintf(stderr, "Failed to allocate memory for text element\n");
                        config_destroy(conf);
                        return 1;
                    }

                    char* newtext = strdup(text);
                    if (newtext == NULL) {
                        fprintf(stderr, "Failed to copy text element string\n");
                        config_destroy(conf);
                        return 1;
                    }

                    text_el->text = newtext;
                    parse_hex(t_active, text_el->active);
                    parse_hex(t_inactive, text_el->inactive);

                    el->text = text_el;
                } else {
                    el->text = NULL;
                }
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
    config_destroy(conf);
    return 0;
}

int read_config_stream(FILE *stream) {
    config_t conf;
    config_init(&conf);

    if (!config_read(&conf, stream)) {
        fprintf(stderr, "Failed to read configuration stream: %s\n", 
                config_error_text(&conf));
        config_destroy(&conf);
        return 1;
    }

    return parse_config(&conf);
}

int read_config_file(char *path) {
    config_t conf;
    config_init(&conf);

    if (!config_read_file(&conf, path)) {
        fprintf(stderr, "Failed to read configuration file (line %i): %s\n",
                config_error_line(&conf),
                config_error_text(&conf));
        config_destroy(&conf);
        return 1;
    }

    return parse_config(&conf);
}

int generate_font_textures() {
    for (int i = 0; i < config.element_count; i++) {
        if (config.elements[i].text != NULL) {
            text_element *text_el = config.elements[i].text;
            SDL_Color color = { 255, 255, 255, 255 };
            if (font != NULL) {
                text_el->surface = TTF_RenderUTF8_Blended(font, text_el->text, color);
                if (text_el->surface != NULL) {
                    text_el->texture = SDL_CreateTextureFromSurface(renderer, text_el->surface);
                    if (text_el->texture == NULL) {
                        const char *err = SDL_GetError();
                        fprintf(stderr, "Failed to create text texture: %s\n", err);
                        return 1;
                    }
                } else {
                    const char *err = TTF_GetError();
                    fprintf(stderr, "Failed to create text surface: %s\n", err);
                    return 1;
                }
            } else {
                const char *err = TTF_GetError();
                fprintf(stderr, "Font not loaded: %s\n", err);
                return 1;
            }
        }
    }

    return 0;
}

// ----------------------------
// run
// ----------------------------

void cleanup() {
    // clean up allocations from kbd_config
    for (int i = 0; i < config.element_count; i++) {
        kbd_element el = config.elements[i];
        if (el.text != NULL) {
            if (el.text->text != NULL) {
                free((void*) el.text->text);
            }

            if (el.text->texture != NULL) {
                SDL_DestroyTexture(el.text->texture);
            }

            if (el.text->surface != NULL) {
                SDL_FreeSurface(el.text->surface);
            }

            free((void*) el.text);
        }
    }

    // dispose of SDL and xcb resources
    if (connection != NULL) {
        xcb_disconnect(connection);
    }

    SDL_Quit();
    TTF_Quit();
}

int main(int argc, char* argv[]) {
    // get arguments
    char *filename;

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments.\n");
        return 1;
    }

    filename = argv[1];

    // check if config file exists and is a file
    if (access(filename, F_OK) != 0) {
        fprintf(stderr, "Configuration file does not exist.\n");
        return 1;
    }

    struct stat file_stat;
    stat(filename, &file_stat);
    if (!S_ISREG(file_stat.st_mode)) {
        fprintf(stderr, "Configuration file is a folder.\n");
        return 1;
    }

    // establish xorg connection
    connection = xcb_connect(NULL, NULL);

    // sdl initialization
    int sdl_result = sdl_init();
    if (sdl_result == 1) {
        // if sdl initialization failed, terminate
        cleanup();
        return 1;
    }

    // read configuration file
    if (read_config_file(filename)) {
        return 1;
    }

    // finish sdl setup
    sdl_result = sdl_setup();
    if (sdl_result == 1) {
        cleanup();
        return 1;
    }

    // generate any font textures, if needed
    generate_font_textures();

    // start display loop
    sdl_loop();

    // dispose of resources and terminate
    cleanup();

    return 0;
}
