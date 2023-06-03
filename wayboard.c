/*
 * wayboard: A keyboard input display for Wayland.
 * Licensed under GPL v3.0 only.
 *
 * This file contains code from:
 * - fcft : MIT (https://codeberg.org/dnkl/fcft/src/branch/master/example/main.c)
 *   Copyright (c) 2019 Daniel Eklöf
 *
 * - hello-wayland : MIT (https://github.com/emersion/hello-wayland)
 *   Copyright (c) 2018 emersion
 *
 * - wshowkeys : GPL3 (https://github.com/ammgws/wshowkeys)
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "wayboard.h"

static struct libinput *libinput = NULL;
static struct udev *udev = NULL;

static struct wl_buffer *wl_buffer = NULL;
static struct wl_compositor *wl_compositor = NULL;
static struct wl_display *wl_display = NULL;
static struct wl_registry *wl_registry = NULL;
static struct wl_shm *wl_shm = NULL;
static struct wl_surface *wl_surface = NULL;
static struct xdg_wm_base *xdg_wm_base = NULL;
static struct xdg_surface *xdg_surface = NULL;
static struct xdg_toplevel *xdg_toplevel = NULL;

static struct fcft_font *font = NULL;
static pixman_image_t *pix = NULL;
static int shm_fd = -1;
static void *shm_data = NULL;

static struct config config = {0};
static uint64_t frame_count = 0;
static bool stop = false;
static struct key_state states[256] = {0};
static bool updated[256] = {0};

static const struct libinput_interface libinput_interface = {
    .close_restricted = libinput_close_restricted,
    .open_restricted = libinput_open_restricted,
};

static const struct wl_registry_listener wl_registry_listener = {
    .global = wl_registry_on_global,
    .global_remove = wl_registry_on_global_remove,
};

static const struct wl_callback_listener wl_surface_frame_listener = {
    .done = wl_surface_frame_done,
};

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_on_configure,
};

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .close = xdg_toplevel_on_close,
    .configure = xdg_toplevel_on_configure,
    .wm_capabilities = xdg_toplevel_on_wm_capabilities,
    //.configure_bounds
};

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_on_ping,
};

static void
cleanup() {
    if (xdg_toplevel != NULL)
        xdg_toplevel_destroy(xdg_toplevel);
    if (xdg_surface != NULL)
        xdg_surface_destroy(xdg_surface);
    if (wl_surface != NULL)
        wl_surface_destroy(wl_surface);
    if (wl_buffer != NULL)
        wl_buffer_destroy(wl_buffer);
    if (shm_fd != -1)
        close(shm_fd);
    if (font != NULL)
        fcft_destroy(font);
    fcft_fini();
}

static void
assert_impl(const char *func, const int line, const char *expr, bool expr_value) {
    if (!expr_value) {
        cleanup();
        fprintf(stderr, "[%s:%d] assert failed: '%s'\n", func, line, expr);
        exit(1);
    }
}

static _Noreturn void
panic_impl(const char *func, const int line, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[%s:%d] panic: ", func, line);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);

    cleanup();
    exit(1);
}

static void
libinput_close_restricted(int fd, void *data) {
    close(fd);
}

static int
libinput_open_restricted(const char *path, int flags, void *data) {
    int fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}

static void
wl_registry_on_global(void *data, struct wl_registry *registry, uint32_t name,
                      const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        wl_compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 6);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        wl_shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 5);
    }
}

static void
wl_registry_on_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    // Noop. TODO
}

static void
wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time) {
    wl_callback_destroy(cb);
    cb = wl_surface_frame(wl_surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, NULL);
    render_frame(time);
}

static void
xdg_surface_on_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    wl_surface_commit(wl_surface);
}

static void
xdg_toplevel_on_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    stop = true;
}

static void
xdg_toplevel_on_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width,
                          int32_t height, struct wl_array *states) {
    // Noop
}

static void
xdg_toplevel_on_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel,
                                struct wl_array *capabilities) {
    // Noop
}

static void
xdg_wm_base_on_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static void
render_clear_buffer() {
    pixman_image_fill_rectangles(PIXMAN_OP_SRC, pix, &config.background, 1,
                                 &(pixman_rectangle16_t){
                                     0,
                                     0,
                                     config.width,
                                     config.height,
                                 });
}

static void
render_frame(uint32_t time) {
    wl_surface_attach(wl_surface, wl_buffer, 0, 0);
    for (size_t i = 0; i < config.count; i++) {
        if (updated[i]) {
            render_key(&config.keys[i], &states[i]);
            updated[i] = false;
        } else if (states[i].unrender_at == frame_count) {
            render_key(&config.keys[i], &states[i]);
        }
    }
    wl_surface_commit(wl_surface);
    frame_count++;
}

static void
render_key(struct config_key *key, struct key_state *state) {
    bool active = state->last_release < state->last_press;
    uint64_t time_active = (state->last_release - state->last_press) / 1000000;
    bool in_threshold = config.time_threshold > 0 && state->last_press != 0 &&
                        time_active < (uint64_t)config.time_threshold && !active;

    pixman_color_t foreground, text;
    if (active || (in_threshold && state->unrender_at != frame_count)) {
        foreground = config.foreground_active;
        text = config.text_active;
        state->unrender_at = frame_count + config.threshold_life;
    } else {
        foreground = config.foreground_inactive;
        text = config.text_inactive;
    }
    pixman_image_fill_rectangles(PIXMAN_OP_SRC, pix, &foreground, 1,
                                 &(pixman_rectangle16_t){
                                     key->x,
                                     key->y,
                                     key->w,
                                     key->h,
                                 });
    char *text_str = NULL;
    if (in_threshold && state->unrender_at != frame_count) {
        text_str = malloc(32);
        snprintf(text_str, 32, "%" PRIu64 " ms", time_active);
    } else if (key->text != NULL) {
        text_str = strdup(key->text);
    }
    if (text_str != NULL) {
        // Convert to UTF-32.
        char32_t *unicode = calloc(strlen(text_str) + 1, sizeof(char32_t));
        mbstate_t state = {0};
        const char *in = text_str;
        const char *const end = text_str + strlen(text_str) + 1;
        size_t ret = 0;
        size_t len = 0;

        while ((ret = mbrtoc32(&unicode[len], in, end - in, &state)) != 0) {
            if (ret >= (size_t)-3 && ret <= (size_t)-1) {
                break;
            }
            in += ret;
            len++;
        }

        // Rasterize.
        struct fcft_text_run *run =
            fcft_rasterize_text_run_utf32(font, len, unicode, FCFT_SUBPIXEL_DEFAULT);
        assert(run != NULL);
        int width = 0;
        int height = 0;
        for (size_t i = 0; i < run->count; i++) {
            width += run->glyphs[i]->advance.x;
            if (run->glyphs[i]->height > height) {
                height = run->glyphs[i]->height;
            }
        }
        int x = key->x + (key->w - width) / 2;
        int y = key->y + (key->h - height) / 2;

        for (size_t i = 0; i < run->count; i++) {
            const struct fcft_glyph *g = run->glyphs[i];
            if (g == NULL) {
                continue;
            }
            pixman_image_t *color = pixman_image_create_solid_fill(&text);
            if (pixman_image_get_format(g->pix) == PIXMAN_a8r8g8b8) {
                pixman_image_composite32(PIXMAN_OP_OVER, g->pix, NULL, pix, 0, 0, 0, 0, x + g->x,
                                         y + font->ascent - g->y, g->width, g->height);
            } else {
                pixman_image_composite32(PIXMAN_OP_OVER, color, g->pix, pix, 0, 0, 0, 0, x + g->x,
                                         y + font->ascent - g->y, g->width, g->height);
            }
            pixman_image_unref(color);
            x += g->advance.x;
        }
        free(unicode);
        free(text_str);
        fcft_text_run_destroy(run);
    }
    wl_surface_damage_buffer(wl_surface, key->x, key->y, key->w, key->h);
}

static void
create_window() {
    char name[32];
    snprintf(name, 32, "/wayboard-%d", getpid());
    shm_fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
    assert(shm_fd >= 0);
    shm_unlink(name);
    size_t size = config.width * config.height * 4;
    size_t stride = config.width * 4;
    assert(ftruncate(shm_fd, size) == 0);
    shm_data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    assert(shm_data != MAP_FAILED);

    struct wl_shm_pool *pool = wl_shm_create_pool(wl_shm, shm_fd, size);
    wl_buffer = wl_shm_pool_create_buffer(pool, 0, config.width, config.height, stride,
                                          WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    wl_surface = wl_compositor_create_surface(wl_compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, wl_surface);
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

    xdg_surface_set_window_geometry(xdg_surface, 0, 0, config.width, config.height);
    xdg_toplevel_set_app_id(xdg_toplevel, "wayboard");
    xdg_toplevel_set_title(xdg_toplevel, "Wayboard");
    xdg_toplevel_set_min_size(xdg_toplevel, config.width, config.height);
    xdg_toplevel_set_max_size(xdg_toplevel, config.width, config.height);
    wl_surface_commit(wl_surface);
    wl_display_roundtrip(wl_display);

    pix = pixman_image_create_bits(PIXMAN_a8r8g8b8, config.width, config.height, shm_data, stride);
    pixman_region32_t clip;
    pixman_region32_init_rect(&clip, 0, 0, config.width, config.height);
    pixman_image_set_clip_region32(pix, &clip);
    pixman_region32_fini(&clip);
    render_clear_buffer();
    for (size_t i = 0; i < config.count; i++) {
        render_key(&config.keys[i], &states[i]);
    }
    wl_surface_attach(wl_surface, wl_buffer, 0, 0);
    wl_surface_commit(wl_surface);

    struct wl_callback *cb = wl_surface_frame(wl_surface);
    wl_callback_add_listener(cb, &wl_surface_frame_listener, NULL);
}

static void
read_hex_value(const char *in, pixman_color_t *out) {
    size_t len = strnlen(in, 10);
    if (len < 6) {
        panic("invalid color");
    }
    if (in[0] == '#') {
        in++;
        len--;
    }

    unsigned int r, g, b, a;
    switch (len) {
    case 6:
        sscanf(in, "%2x%2x%2x", &r, &g, &b);
        a = 255;
        break;
    case 8:
        sscanf(in, "%2x%2x%2x%2x", &r, &g, &b, &a);
        break;
    default:
        panic("invalid color");
    }
    out->red = r * 256;
    out->green = g * 256;
    out->blue = b * 256;
    out->alpha = a * 256;
}

static void
read_config(const char *path) {
    const char *str;
    config_t raw_config;
    config_init(&raw_config);
    assert(config_read_file(&raw_config, path) == CONFIG_TRUE);

    struct color {
        const char *name;
        pixman_color_t *data;
        bool optional;
    };
    struct color base_colors[] = {
        {"background", &config.background, false},
        {"foreground_active", &config.foreground_active, false},
        {"foreground_inactive", &config.foreground_inactive, false},
        {"text_active", &config.text_active, true},
        {"text_inactive", &config.text_inactive, true},
    };
    for (size_t i = 0; i < sizeof(base_colors) / sizeof(struct color); i++) {
        struct color color = base_colors[i];
        if (config_lookup_string(&raw_config, color.name, &str)) {
            read_hex_value(str, color.data);
        } else if (!color.optional) {
            panic("missing color: %s", color.name);
        }
    }

    if (config_lookup_string(&raw_config, "font", &str)) {
        config.font = strdup(str);
    }

    if (!config_lookup_int(&raw_config, "width", &config.width) ||
        !config_lookup_int(&raw_config, "height", &config.height)) {
        if (config.width < 0 || config.height < 0 || config.width > 4096 || config.height > 4096) {
            panic("invalid window size");
        }
        panic("missing size information for window");
    }

    config_setting_t *keys = config_lookup(&raw_config, "keys");
    size_t count = config_setting_length(keys);
    if (count > 256) {
        panic("more than 256 keys");
    }

    bool font_warning = false;
    if (config_lookup_int(&raw_config, "time_threshold", &config.time_threshold)) {
        if (config.font == NULL && !font_warning) {
            font_warning = true;
            config.font = "";
            fprintf(stderr, "WARNING: No font specified.\n");
        }
        if (!config_lookup_int(&raw_config, "threshold_life", &config.threshold_life)) {
            panic("no threshold_life value with a set time_threshold");
        }
    }
    config.count = count;
    for (size_t i = 0; i < count; i++) {
        int scancode;
        config_setting_t *key = config_setting_get_elem(keys, i);
        if (config_setting_lookup_int(key, "x", &config.keys[i].x) &&
            config_setting_lookup_int(key, "y", &config.keys[i].y) &&
            config_setting_lookup_int(key, "w", &config.keys[i].w) &&
            config_setting_lookup_int(key, "h", &config.keys[i].h) &&
            config_setting_lookup_int(key, "scancode", &scancode)) {
            config.keys[i].scancode = scancode;
            if (config_setting_lookup_string(key, "text", &str)) {
                if (config.font == NULL && !font_warning) {
                    font_warning = true;
                    config.font = "";
                    fprintf(stderr, "WARNING: No font specified.\n");
                }
                config.keys[i].text = strdup(str);
            }
        } else {
            panic("key %d missing required information", i);
        }
    }
    config_destroy(&raw_config);
}

static void
setup_libinput() {
    assert((udev = udev_new()));
    assert((libinput = libinput_udev_create_context(&libinput_interface, NULL, udev)));
    const char *seat = getenv("XDG_SEAT");
    if (seat == NULL) {
        seat = "seat0";
    }
    libinput_udev_assign_seat(libinput, seat);
    libinput_dispatch(libinput);

    if (geteuid() == 0) {
        printf("INFO: dropping root privileges (geteuid() == 0)\n");
        if (setgid(getgid()) != 0) {
            panic("drop root (setgid): %s", strerror(errno));
        }
        if (setuid(getuid()) != 0) {
            panic("drop root (setuid): %s", strerror(errno));
        }
        if (setuid(0) == 0) {
            panic("root privileges not dropped (use the setuid bit)");
        } else {
            printf("INFO: dropped root privileges\n");
        }
    }
}

int
main(int argc, char **argv) {
    fcft_init(FCFT_LOG_COLORIZE_AUTO, false, FCFT_LOG_CLASS_WARNING);
    setup_libinput();

    if (argc != 2) {
        fprintf(stderr, "USAGE: %s CONFIG_FILE\n", argv[0]);
        return 1;
    }
    read_config(argv[1]);
    if (config.font != NULL) {
        tll(const char *) font_names = tll_init();
        tll_push_back(font_names, config.font);
        size_t i = 0;
        const char *names[tll_length(font_names)];
        tll_foreach(font_names, iter) names[i++] = iter->item;
        font = fcft_from_name(1, names, NULL);
        if (font == NULL) {
            panic("no font loaded");
        }
        tll_free(font_names);
    }

    assert((wl_display = wl_display_connect(NULL)));
    assert((wl_registry = wl_display_get_registry(wl_display)));
    wl_registry_add_listener(wl_registry, &wl_registry_listener, NULL);
    wl_display_roundtrip(wl_display);
    assert(wl_compositor != NULL && wl_shm != NULL && xdg_wm_base != NULL);
    xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
    create_window();

    struct pollfd fds[] = {
        {.fd = libinput_get_fd(libinput), .events = POLLIN},
        {.fd = wl_display_get_fd(wl_display), .events = POLLIN},
    };
    while (!stop) {
        wl_display_flush(wl_display);
        if (poll(fds, sizeof(fds) / sizeof(struct pollfd), -1) < 0) {
            panic("poll: %s\n", strerror(errno));
        }

        // Check libinput.
        if ((fds[0].revents & POLLIN) != 0) {
            libinput_dispatch(libinput);
            struct libinput_event *event;
            while ((event = libinput_get_event(libinput)) != NULL) {
                if (libinput_event_get_type(event) != LIBINPUT_EVENT_KEYBOARD_KEY) {
                    goto next;
                }
                struct libinput_event_keyboard *kevt = libinput_event_get_keyboard_event(event);
                uint32_t key = libinput_event_keyboard_get_key(kevt);
                enum libinput_key_state state = libinput_event_keyboard_get_key_state(kevt);
                struct timespec ts_now;
                clock_gettime(CLOCK_MONOTONIC, &ts_now);
                uint64_t time = ts_now.tv_sec * 1000000000 + ts_now.tv_nsec;

                struct key_state *key_state = NULL;
                for (size_t i = 0; i < config.count; i++) {
                    if (config.keys[i].scancode == key + 8) {
                        key_state = &states[i];
                        updated[i] = true;
                        break;
                    }
                }
                if (key_state == NULL) {
                    goto next;
                }

                switch (state) {
                case LIBINPUT_KEY_STATE_PRESSED:
                    key_state->last_press = time;
                    break;
                case LIBINPUT_KEY_STATE_RELEASED:
                    key_state->last_release = time;
                    break;
                }
            next:
                libinput_event_destroy(event);
            }
        }

        // Check wayland.
        if ((fds[1].revents & POLLIN) != 0) {
            assert(wl_display_dispatch(wl_display) != -1);
        }
    }

    cleanup();
    return 0;
}
