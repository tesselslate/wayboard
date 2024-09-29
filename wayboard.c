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

// Used for memfd_create
#define _GNU_SOURCE

#include "xdg-shell.h"
#include <assert.h>
#include <errno.h>
#include <fcft/fcft.h>
#include <fcntl.h>
#include <libconfig.h>
#include <libinput.h>
#include <libudev.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <uchar.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>

#define ARRAY_LEN(x) ((sizeof((x)) / sizeof(*(x))))
#define KEY_DEFINED(wb, code) ((wb)->cfg.keys[(code)].w != 0)
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MAX_KEYS 256

struct cfg {
    // Appearance
    int width, height;
    pixman_color_t background;
    pixman_color_t fg_active, fg_inactive;
    pixman_color_t txt_active, txt_inactive;
    char *font;

    // Function
    int time_threshold; // maximum duration to show keypress length
    int threshold_life; // number of ms to show keypress length for

    // Layout
    struct cfg_key {
        int x, y, w, h;
        char *text_active, *text_inactive;
    } keys[MAX_KEYS];
};

struct wayboard {
    // Configuration
    struct cfg cfg;
    struct fcft_font *font;

    // libinput state
    struct libinput *libinput;
    struct udev *udev;

    // Wayland state
    struct {
        struct wl_display *display;
        struct wl_registry *registry;

        struct wl_compositor *compositor;
        struct wl_shm *shm;
        struct xdg_wm_base *xdg_wm_base;

        struct wl_buffer *buffer;
        struct wl_surface *surface;
        struct xdg_surface *xdg_surface;
        struct xdg_toplevel *xdg_toplevel;

        struct wl_callback *frame_cb;
    } wl;

    // General state
    struct {
        pixman_image_t *pixman_image;
        int shm_fd;
        void *shm_data;
        bool buf_released;

        bool should_close;
        uint32_t last_render;

        struct wb_key_state {
            uint64_t last_press_usec, last_release_usec;
            uint64_t unrender_at_usec;
        } keys[MAX_KEYS];
    } state;
};

static const struct wl_buffer_listener buffer_listener;
static const struct wl_callback_listener callback_frame_listener;
static const struct wl_registry_listener registry_listener;
static const struct xdg_wm_base_listener xdg_wm_base_listener;
static const struct xdg_surface_listener xdg_surface_listener;
static const struct xdg_toplevel_listener xdg_toplevel_listener;

static void cfg_destroy(struct cfg *cfg);
static int cfg_read(struct cfg *cfg, config_t *conf);
static int cfg_read_color(const char *color_str, pixman_color_t *out);
static int cfg_read_colors(struct cfg *cfg, config_t *conf);
static int cfg_read_keys(struct cfg *cfg, config_t *conf);
static int cfg_read_toplevel(struct cfg *cfg, config_t *conf);
static void render_frame(struct wayboard *wb);
static void render_key(struct wayboard *wb, uint32_t keycode);
static void render_key_text(struct wayboard *wb, struct cfg_key *key, const pixman_color_t *text,
                            const char *text_str);
static inline uint64_t usec_now();
static void wayboard_commit_frame(struct wayboard *wb, uint32_t time);
static void wayboard_process_key(struct wayboard *wb, uint32_t keycode,
                                 enum libinput_key_state state, uint64_t usec);
static int wayboard_process_libinput(struct wayboard *wb);
static int wayboard_run(struct wayboard *wb);
static void wayboard_spin_buffer_release(struct wayboard *wb);

static void
on_buffer_release(void *data, struct wl_buffer *buffer) {
    struct wayboard *wb = data;

    wb->state.buf_released = true;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = on_buffer_release,
};

static void
on_callback_frame_done(void *data, struct wl_callback *callback, uint32_t time) {
    struct wayboard *wb = data;

    render_frame(wb);

    wayboard_commit_frame(wb, time);
    wl_callback_destroy(callback);
}

static const struct wl_callback_listener callback_frame_listener = {
    .done = on_callback_frame_done,
};

static void
on_registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface,
                   uint32_t version) {
    static const int USE_COMPOSITOR_VERSION = WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION;
    static const int USE_SHM_VERSION = 1;
    static const int USE_XDG_WM_BASE_VERSION = XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION;

    struct wayboard *wb = data;

    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        if (version < USE_COMPOSITOR_VERSION) {
            fprintf(stderr, "outdated %s: expected v%d, got v%d\n", wl_compositor_interface.name,
                    USE_COMPOSITOR_VERSION, version);
            return;
        }

        wb->wl.compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, USE_COMPOSITOR_VERSION);
        assert(wb->wl.compositor);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        if (version < USE_SHM_VERSION) {
            fprintf(stderr, "outdated %s: expected v%d, got v%d\n", wl_shm_interface.name,
                    USE_SHM_VERSION, version);
            return;
        }

        wb->wl.shm = wl_registry_bind(registry, name, &wl_shm_interface, USE_SHM_VERSION);
        assert(wb->wl.shm);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        if (version < USE_XDG_WM_BASE_VERSION) {
            fprintf(stderr, "outdated %s: expected v%d, got v%d\n", xdg_wm_base_interface.name,
                    USE_XDG_WM_BASE_VERSION, version);
            return;
        }

        wb->wl.xdg_wm_base =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, USE_XDG_WM_BASE_VERSION);
        assert(wb->wl.xdg_wm_base);

        xdg_wm_base_add_listener(wb->wl.xdg_wm_base, &xdg_wm_base_listener, wb);
    }
}

static void
on_registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    // Unused.
}

static const struct wl_registry_listener registry_listener = {
    .global = on_registry_global,
    .global_remove = on_registry_global_remove,
};

static void
on_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = on_xdg_wm_base_ping,
};

static void
on_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    struct wayboard *wb = data;

    xdg_surface_ack_configure(xdg_surface, serial);
    wl_surface_commit(wb->wl.surface);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = on_xdg_surface_configure,
};

static void
on_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    struct wayboard *wb = data;

    wb->state.should_close = true;
}

static void
on_xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width,
                          int32_t height, struct wl_array *states) {
    // Unused.
}

static void
on_xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width,
                                 int32_t height) {
    // Unused.
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .close = on_xdg_toplevel_close,
    .configure = on_xdg_toplevel_configure,
    .configure_bounds = on_xdg_toplevel_configure_bounds,
};

static void
libinput_iface_close_restricted(int fd, void *data) {
    close(fd);
}

static int
libinput_iface_open_restricted(const char *path, int flags, void *data) {
    int fd = open(path, flags);
    return fd > 0 ? fd : -errno;
}

static const struct libinput_interface libinput_iface = {
    .close_restricted = libinput_iface_close_restricted,
    .open_restricted = libinput_iface_open_restricted,
};

static void
cfg_destroy(struct cfg *cfg) {
    free(cfg->font);

    for (size_t i = 0; i < ARRAY_LEN(cfg->keys); i++) {
        if (cfg->keys[i].text_active) {
            free(cfg->keys[i].text_active);
        }
        if (cfg->keys[i].text_inactive) {
            free(cfg->keys[i].text_inactive);
        }
    }
}

static int
cfg_read(struct cfg *cfg, struct config_t *conf) {
    if (cfg_read_colors(cfg, conf) != 0) {
        return 1;
    }

    if (cfg_read_toplevel(cfg, conf) != 0) {
        return 1;
    }

    if (cfg_read_keys(cfg, conf) != 0) {
        free(cfg->font);
        return 1;
    }

    return 0;
}

static int
cfg_read_color(const char *color_str, pixman_color_t *out) {
    size_t len = strlen(color_str);
    if (color_str[0] == '#') {
        len--;
        color_str++;
    }

    unsigned int r, g, b, a;
    size_t n = 0;
    switch (len) {
    case 6:
        n = sscanf(color_str, "%2x%2x%2x", &r, &g, &b);
        a = 255;
        break;
    case 8:
        n = sscanf(color_str, "%2x%2x%2x%2x", &r, &g, &b, &a);
        break;
    default:
        return 1;
    }
    if (n != len / 2) {
        return 1;
    }

    out->red = r * 256;
    out->green = g * 256;
    out->blue = b * 256;
    out->alpha = a * 256;
    return 0;
}

static int
cfg_read_colors(struct cfg *cfg, config_t *conf) {
    const char *color_str;

    const struct config_color {
        const char *name;
        pixman_color_t *out;
        bool optional;
    } colors[] = {
        {"background", &cfg->background, false},
        {"foreground_active", &cfg->fg_active, false},
        {"foreground_inactive", &cfg->fg_inactive, false},
        {"text_active", &cfg->txt_active, true},
        {"text_inactive", &cfg->txt_inactive, true},
    };
    for (size_t i = 0; i < ARRAY_LEN(colors); i++) {
        const struct config_color *color = &colors[i];

        if (!config_lookup_string(conf, color->name, &color_str)) {
            if (color->optional) {
                continue;
            }

            fprintf(stderr, "no '%s' property set in config\n", color->name);
            return 1;
        }

        if (cfg_read_color(color_str, color->out) != 0) {
            fprintf(stderr, "invalild color '%s' for property '%s'", color_str, color->name);
            return 1;
        }
    }

    return 0;
}

static int
cfg_read_keys(struct cfg *cfg, config_t *conf) {
    config_setting_t *keys = config_lookup(conf, "keys");
    if (!keys) {
        fprintf(stderr, "no 'keys' table set in config\n");
        return 1;
    }

    size_t num_keys = config_setting_length(keys);
    bool keys_set[MAX_KEYS] = {0};
    size_t i = 0;
    for (i = 0; i < num_keys; i++) {
        config_setting_t *key = config_setting_get_elem(keys, i);
        assert(key);

        int code;
        if (!config_setting_lookup_int(key, "scancode", &code)) {
            fprintf(stderr, "no 'scancode' property set on key %zu in config\n", i);
            goto fail_key;
        }
        if (code < 0 || code >= MAX_KEYS) {
            fprintf(stderr, "invalid 'scancode' property %d set on key %zu in config\n", code, i);
            goto fail_key;
        }

        if (keys_set[code]) {
            fprintf(stderr, "more than one key uses scancode %d\n", code);
            goto fail_key;
        }
        keys_set[code] = true;

        if (!config_setting_lookup_int(key, "x", &cfg->keys[code].x)) {
            fprintf(stderr, "no 'x' property set on key %zu in config\n", i);
            goto fail_key;
        }
        if (!config_setting_lookup_int(key, "y", &cfg->keys[code].y)) {
            fprintf(stderr, "no 'y' property set on key %zu in config\n", i);
            goto fail_key;
        }
        if (!config_setting_lookup_int(key, "w", &cfg->keys[code].w)) {
            fprintf(stderr, "no 'w' property set on key %zu in config\n", i);
            goto fail_key;
        }
        if (!config_setting_lookup_int(key, "h", &cfg->keys[code].h)) {
            fprintf(stderr, "no 'h' property set on key %zu in config\n", i);
            goto fail_key;
        }

        const char *text_str;
        if (config_setting_lookup_string(key, "text_active", &text_str)) {
            cfg->keys[code].text_active = strdup(text_str);
            assert(cfg->keys[code].text_active);
        }
        if (config_setting_lookup_string(key, "text_inactive", &text_str)) {
            cfg->keys[code].text_inactive = strdup(text_str);
            assert(cfg->keys[code].text_inactive);
        }
    }

    return 0;

fail_key:
    for (size_t j = 0; j < i; j++) {
        if (cfg->keys[j].text_active) {
            free(cfg->keys[j].text_active);
        }
        if (cfg->keys[j].text_inactive) {
            free(cfg->keys[j].text_inactive);
        }
    }

    return 1;
}

static int
cfg_read_toplevel(struct cfg *cfg, config_t *conf) {
    const char *font_str;
    if (!config_lookup_string(conf, "font", &font_str)) {
        fprintf(stderr, "no 'font' property set in config\n");
        return 1;
    }
    cfg->font = strdup(font_str);
    assert(cfg->font);

    if (!config_lookup_int(conf, "width", &cfg->width)) {
        fprintf(stderr, "no 'width' property set in config\n");
        goto fail_width;
    }
    if (!config_lookup_int(conf, "height", &cfg->height)) {
        fprintf(stderr, "no 'height' property set in config\n");
        goto fail_height;
    }
    if (cfg->width < 0 || cfg->height < 0 || cfg->width > 4096 || cfg->height > 4096) {
        fprintf(stderr, "invalid window size (%dx%d) set in config\n", cfg->width, cfg->height);
        goto fail_size;
    }

    bool has_time_threshold = config_lookup_int(conf, "time_threshold", &cfg->time_threshold);
    bool has_threshold_life = config_lookup_int(conf, "threshold_life", &cfg->threshold_life);
    if (has_time_threshold != has_threshold_life) {
        fprintf(stderr, "did not set both 'time_threshold' and 'threshold_life' properties\n");
        goto fail_threshold;
    }

    return 0;

fail_threshold:
fail_size:
fail_height:
fail_width:
    free(cfg->font);
    return 1;
}

static int
init_fcft(struct wayboard *wb) {
    if (!fcft_init(FCFT_LOG_COLORIZE_AUTO, false, FCFT_LOG_CLASS_WARNING)) {
        fprintf(stderr, "failed to initialize fcft\n");
        return 1;
    }

    // SAFETY: init_read_config ensures that `cfg->font` is non-NULL.
    const char *names[] = {wb->cfg.font};
    wb->font = fcft_from_name(1, names, NULL);
    if (!wb->font) {
        fprintf(stderr, "failed to load font '%s'\n", wb->cfg.font);
        fcft_fini();
        return 1;
    }

    return 0;
}

static int
init_libinput(struct wayboard *wb) {
    wb->udev = udev_new();
    if (!wb->udev) {
        fprintf(stderr, "failed to initialize udev\n");
        return 1;
    }

    wb->libinput = libinput_udev_create_context(&libinput_iface, &wb, wb->udev);
    if (!wb->libinput) {
        fprintf(stderr, "failed to initialize libinput\n");
        goto fail_context;
    }

    const char *seat = getenv("XDG_SEAT");
    if (!seat) {
        seat = "seat0";
    }
    if (libinput_udev_assign_seat(wb->libinput, seat) != 0) {
        fprintf(stderr, "failed to assign seat to libinput context\n");
        goto fail_seat;
    }
    int err = libinput_dispatch(wb->libinput);
    if (err != 0) {
        fprintf(stderr, "failed to dispatch libinput: %s\n", strerror(-err));
        goto fail_dispatch;
    }

    return 0;

fail_dispatch:
fail_seat:
    libinput_unref(wb->libinput);

fail_context:
    udev_unref(wb->udev);
    return 1;
}

static int
init_read_config(struct wayboard *wb, const char *path) {
    config_t conf;
    config_init(&conf);
    if (config_read_file(&conf, path) != CONFIG_TRUE) {
        fprintf(stderr, "failed to read config file\n");
        return 1;
    }

    int ret = cfg_read(&wb->cfg, &conf);

    config_destroy(&conf);
    return ret;
}

static int
init_render(struct wayboard *wb) {
    size_t shm_stride = wb->cfg.width * 4;
    wb->state.pixman_image = pixman_image_create_bits(PIXMAN_a8r8g8b8, wb->cfg.width, wb->cfg.width,
                                                      wb->state.shm_data, shm_stride);
    if (!wb->state.pixman_image) {
        fprintf(stderr, "failed to create pixman image\n");
        return 1;
    }

    pixman_image_fill_rectangles(PIXMAN_OP_SRC, wb->state.pixman_image, &wb->cfg.background, 1,
                                 &(pixman_rectangle16_t){
                                     0,
                                     0,
                                     wb->cfg.width,
                                     wb->cfg.height,
                                 });

    for (size_t i = 0; i < MAX_KEYS; i++) {
        if (!wb->cfg.keys[i].text_inactive) {
            continue;
        }

        render_key_text(wb, &wb->cfg.keys[i], &wb->cfg.txt_inactive, wb->cfg.keys[i].text_inactive);
    }

    wl_surface_damage_buffer(wb->wl.surface, 0, 0, INT32_MAX, INT32_MAX);
    wayboard_commit_frame(wb, 0);

    return 0;
}

static int
init_wayland(struct wayboard *wb) {
    wb->wl.display = wl_display_connect(NULL);
    if (!wb->wl.display) {
        perror("failed to connect to a wayland display");
        return 1;
    }

    wb->wl.registry = wl_display_get_registry(wb->wl.display);
    assert(wb->wl.registry);
    wl_registry_add_listener(wb->wl.registry, &registry_listener, wb);
    if (wl_display_roundtrip(wb->wl.display) == -1) {
        perror("failed to roundtrip wayland display during init");
        goto fail_roundtrip_globals;
    }

    if (!wb->wl.compositor || !wb->wl.shm || !wb->wl.xdg_wm_base) {
        fprintf(stderr, "missing wayland globals\n");
        goto fail_globals;
    }

    size_t shm_stride = wb->cfg.width * 4;
    size_t shm_size = wb->cfg.height * shm_stride;
    wb->state.shm_fd = memfd_create("wayboard-shm", MFD_CLOEXEC);
    if (wb->state.shm_fd < 0) {
        perror("failed to create memfd");
        goto fail_memfd;
    }
    if (ftruncate(wb->state.shm_fd, shm_size) != 0) {
        perror("failed to expand memfd");
        goto fail_memfd_truncate;
    }
    wb->state.shm_data =
        mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, wb->state.shm_fd, 0);
    if (wb->state.shm_data == MAP_FAILED) {
        perror("failed to mmap memfd");
        goto fail_memfd_mmap;
    }

    struct wl_shm_pool *shm_pool = wl_shm_create_pool(wb->wl.shm, wb->state.shm_fd, shm_size);
    assert(shm_pool);
    wb->wl.buffer = wl_shm_pool_create_buffer(shm_pool, 0, wb->cfg.width, wb->cfg.height,
                                              shm_stride, WL_SHM_FORMAT_ARGB8888);
    assert(wb->wl.buffer);
    wl_buffer_add_listener(wb->wl.buffer, &buffer_listener, wb);
    wl_shm_pool_destroy(shm_pool);

    wb->wl.surface = wl_compositor_create_surface(wb->wl.compositor);
    assert(wb->wl.surface);
    wb->wl.xdg_surface = xdg_wm_base_get_xdg_surface(wb->wl.xdg_wm_base, wb->wl.surface);
    assert(wb->wl.xdg_surface);
    wb->wl.xdg_toplevel = xdg_surface_get_toplevel(wb->wl.xdg_surface);
    assert(wb->wl.xdg_toplevel);

    xdg_surface_add_listener(wb->wl.xdg_surface, &xdg_surface_listener, wb);
    xdg_toplevel_add_listener(wb->wl.xdg_toplevel, &xdg_toplevel_listener, wb);

    xdg_toplevel_set_app_id(wb->wl.xdg_toplevel, "wayboard");
    xdg_toplevel_set_title(wb->wl.xdg_toplevel, "wayboard");
    xdg_toplevel_set_min_size(wb->wl.xdg_toplevel, wb->cfg.width, wb->cfg.height);
    xdg_toplevel_set_max_size(wb->wl.xdg_toplevel, wb->cfg.width, wb->cfg.height);
    wl_surface_commit(wb->wl.surface);
    if (wl_display_roundtrip(wb->wl.display) == -1) {
        perror("failed to roundtrip wayland display during xdg_toplevel init");
        goto fail_roundtrip_globals;
    }

    return 0;

fail_memfd_mmap:
fail_memfd_truncate:
    close(wb->state.shm_fd);

fail_memfd:
fail_globals:
    if (wb->wl.compositor) {
        wl_compositor_destroy(wb->wl.compositor);
    }
    if (wb->wl.shm) {
        wl_shm_destroy(wb->wl.shm);
    }
    if (wb->wl.xdg_wm_base) {
        xdg_wm_base_destroy(wb->wl.xdg_wm_base);
    }

fail_roundtrip_globals:
    wl_registry_destroy(wb->wl.registry);
    wl_display_disconnect(wb->wl.display);
    return 1;
}

static void
render_frame(struct wayboard *wb) {
    // Wait until the SHM buffer is available for new content.
    wayboard_spin_buffer_release(wb);

    // Unrender any keys which were previously in threshold.
    for (size_t i = 0; i < MAX_KEYS; i++) {
        if (!KEY_DEFINED(wb, i)) {
            continue;
        }

        struct wb_key_state *ks = &wb->state.keys[i];
        struct cfg_key *key = &wb->cfg.keys[i];

        bool pressed = ks->last_press_usec > ks->last_release_usec;
        uint64_t time_active_usec = ks->last_release_usec - ks->last_press_usec;
        bool in_threshold = (wb->cfg.time_threshold > 0) && (ks->last_press_usec != 0) &&
                            (time_active_usec < (uint64_t)wb->cfg.time_threshold * 1000) &&
                            !pressed;

        if (in_threshold && usec_now() > ks->unrender_at_usec) {
            pixman_image_fill_rectangles(PIXMAN_OP_SRC, wb->state.pixman_image, &wb->cfg.background,
                                         1,
                                         &(pixman_rectangle16_t){
                                             key->x,
                                             key->y,
                                             key->w,
                                             key->h,
                                         });
            wl_surface_damage_buffer(wb->wl.surface, key->x, key->y, key->w, key->h);

            ks->unrender_at_usec = UINT64_MAX;
        }
    }
}

static void
render_key(struct wayboard *wb, uint32_t keycode) {
    assert(keycode < MAX_KEYS);

    if (!KEY_DEFINED(wb, keycode)) {
        return;
    }

    struct wb_key_state *ks = &wb->state.keys[keycode];
    struct cfg_key *key = &wb->cfg.keys[keycode];

    // Determine the current state of the key.
    //
    //   - `time_active_usec` will underflow to a large value if the key is currently held. If the
    //     key is not currently held, it will contain the last press duration.
    //   - `in_threshold` is true if all of the following are true:
    //     - the user has set `time_threshold` in their config
    //     - the last press duration was lower than `time_threshold`
    //     - the key has been pressed at least once
    //     - the key is currently not pressed
    bool pressed = ks->last_press_usec > ks->last_release_usec;

    uint64_t time_active_usec = ks->last_release_usec - ks->last_press_usec;
    bool in_threshold = (wb->cfg.time_threshold > 0) && (ks->last_press_usec != 0) &&
                        (time_active_usec < (uint64_t)wb->cfg.time_threshold * 1000) && !pressed;

    // If this is the first frame where the key is within the threshold, then the `unrender_at_usec`
    // value should be updated.
    uint64_t expected_unrender_at = ks->last_release_usec + (uint64_t)wb->cfg.threshold_life * 1000;
    if (in_threshold && ks->unrender_at_usec != expected_unrender_at) {
        ks->unrender_at_usec = expected_unrender_at;
    }

    bool render_threshold = in_threshold && usec_now() < ks->unrender_at_usec;

    const pixman_color_t *foreground, *text;
    if (pressed || render_threshold) {
        foreground = &wb->cfg.fg_active;
        text = &wb->cfg.txt_active;
    } else {
        foreground = &wb->cfg.fg_inactive;
        text = &wb->cfg.txt_inactive;
    }

    // Wait until the SHM buffer is available for new content.
    wayboard_spin_buffer_release(wb);

    // Fill the key rectangle with the correct foreground color.
    pixman_image_fill_rectangles(PIXMAN_OP_SRC, wb->state.pixman_image, foreground, 1,
                                 &(pixman_rectangle16_t){
                                     key->x,
                                     key->y,
                                     key->w,
                                     key->h,
                                 });

    // Determine which string of text to render, if any.
    char *text_str = NULL;
    if (render_threshold) {
        static char threshold_buf[256] = {0};
        snprintf(threshold_buf, sizeof(threshold_buf), "%" PRIu64 " ms", time_active_usec / 1000);

        text_str = threshold_buf;
    } else {
        text_str = pressed ? key->text_active : key->text_inactive;
    }

    // Render text, if any should be shown.
    if (text_str != NULL) {
        render_key_text(wb, key, text, text_str);
    }

    // Damage the modified area of the buffer.
    wl_surface_damage_buffer(wb->wl.surface, key->x, key->y, key->w, key->h);
}

static void
render_key_text(struct wayboard *wb, struct cfg_key *key, const pixman_color_t *text,
                const char *text_str) {
    // TODO: Cache rasterized text runs from fcft.

    // Convert the given text to UTF32.
    size_t len = strlen(text_str);

    char32_t *utf32 = calloc(len + 1, sizeof(char32_t));
    assert(utf32);

    mbstate_t mbstate = {0};
    const char *in = text_str;
    const char *const end = text_str + len + 1;
    size_t pos = 0;
    for (;;) {
        size_t n = mbrtoc32(&utf32[pos], in, end - in, &mbstate);

        switch (n) {
        case 0: // null byte
            goto done_utf32;
        case (size_t)(-1): // error cases
        case (size_t)(-2):
        case (size_t)(-3):
            fprintf(stderr, "failed to convert '%s' to UTF-32\n", text_str);
            goto fail;
        default: // normal character
            break;
        }

        pos++;
        in += n;
    }
done_utf32:;

    // Rasterize the text with fcft and then render it into the shared memory buffer.
    // It is assumed that the text run will be able to fit within the key rectangle.
    //
    // TODO: Use subpixel configuration of output
    struct fcft_text_run *run =
        fcft_rasterize_text_run_utf32(wb->font, pos, utf32, FCFT_SUBPIXEL_DEFAULT);
    if (!run) {
        fprintf(stderr, "failed to rasterize text run for '%s'\n", text_str);
        goto fail;
    }

    int text_width = 0;
    int text_height = 0;
    for (size_t i = 0; i < run->count; i++) {
        if (!run->glyphs[i]) {
            continue;
        }

        text_width += run->glyphs[i]->advance.x;
        text_height = MAX(text_height, run->glyphs[i]->height);
    }
    int x = key->x + (key->w - text_width) / 2;
    int y = key->y + (key->h - text_height) / 2;

    for (size_t i = 0; i < run->count; i++) {
        const struct fcft_glyph *glyph = run->glyphs[i];
        if (!glyph) {
            continue;
        }

        if (pixman_image_get_format(glyph->pix) == PIXMAN_a8r8g8b8) {
            pixman_image_composite32(PIXMAN_OP_OVER, glyph->pix, NULL, wb->state.pixman_image, 0, 0,
                                     0, 0, x + glyph->x, y + wb->font->ascent - glyph->y,
                                     glyph->width, glyph->height);
        } else {
            pixman_image_t *color = pixman_image_create_solid_fill(text);
            pixman_image_composite32(PIXMAN_OP_OVER, color, glyph->pix, wb->state.pixman_image, 0,
                                     0, 0, 0, x + glyph->x, y + wb->font->ascent - glyph->y,
                                     glyph->width, glyph->height);
            pixman_image_unref(color);
        }
        x += glyph->advance.x;
    }

    fcft_text_run_destroy(run);
    free(utf32);
    return;

fail:
    free(utf32);
    return;
}

static inline uint64_t
usec_now() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint64_t)ts.tv_sec * 1000000 + (uint64_t)ts.tv_nsec / 1000;
}

static void
wayboard_fini_wl(struct wayboard *wb) {
    if (wb->wl.frame_cb) {
        wl_callback_destroy(wb->wl.frame_cb);
    }

    xdg_toplevel_destroy(wb->wl.xdg_toplevel);
    xdg_surface_destroy(wb->wl.xdg_surface);
    wl_surface_destroy(wb->wl.surface);
    wl_buffer_destroy(wb->wl.buffer);

    munmap(wb->state.shm_data, wb->cfg.width * wb->cfg.height * 4);
    close(wb->state.shm_fd);

    xdg_wm_base_destroy(wb->wl.xdg_wm_base);
    wl_shm_destroy(wb->wl.shm);
    wl_compositor_destroy(wb->wl.compositor);
    wl_registry_destroy(wb->wl.registry);
    wl_display_disconnect(wb->wl.display);
}

static void
wayboard_commit_frame(struct wayboard *wb, uint32_t time) {
    wb->wl.frame_cb = wl_surface_frame(wb->wl.surface);
    wl_callback_add_listener(wb->wl.frame_cb, &callback_frame_listener, wb);

    wl_surface_attach(wb->wl.surface, wb->wl.buffer, 0, 0);
    wl_surface_commit(wb->wl.surface);

    wb->state.last_render = time;
    wb->state.buf_released = false;
}

static void
wayboard_process_key(struct wayboard *wb, uint32_t keycode, enum libinput_key_state state,
                     uint64_t usec) {
    if (keycode >= MAX_KEYS) {
        fprintf(stderr, "warn: keycode %d over max processed (%d)\n", keycode, MAX_KEYS);
        return;
    }

    struct wb_key_state *ks = &wb->state.keys[keycode];
    switch (state) {
    case LIBINPUT_KEY_STATE_PRESSED:
        ks->last_press_usec = usec;
        break;
    case LIBINPUT_KEY_STATE_RELEASED:
        ks->last_release_usec = usec;
        break;
    }

    render_key(wb, keycode);
}

static int
wayboard_process_libinput(struct wayboard *wb) {
    int err = libinput_dispatch(wb->libinput);
    if (err != 0) {
        fprintf(stderr, "failed to dispatch libinput: %s\n", strerror(-err));
        return 1;
    }

    for (;;) {
        struct libinput_event *event = libinput_get_event(wb->libinput);
        if (!event) {
            return 0;
        }

        // Only process keyboard key events.
        if (libinput_event_get_type(event) != LIBINPUT_EVENT_KEYBOARD_KEY) {
            libinput_event_destroy(event);
            continue;
        }

        struct libinput_event_keyboard *kbd_event = libinput_event_get_keyboard_event(event);
        uint32_t keycode = libinput_event_keyboard_get_key(kbd_event);
        enum libinput_key_state state = libinput_event_keyboard_get_key_state(kbd_event);
        uint64_t usec = libinput_event_keyboard_get_time_usec(kbd_event);

        // Utilities such as `wev` show XKB keycodes, which are 8 greater than libinput keycodes.
        wayboard_process_key(wb, keycode + 8, state, usec);
        libinput_event_destroy(event);
    }
}

static int
wayboard_run(struct wayboard *wb) {
    struct pollfd pollfds[] = {
        {.fd = libinput_get_fd(wb->libinput), .events = POLLIN},
        {.fd = wl_display_get_fd(wb->wl.display), .events = POLLIN},
    };

    while (!wb->state.should_close) {
        if (wl_display_flush(wb->wl.display) == -1) {
            perror("failed to flush wayland display");
            return 1;
        }
        if (poll(pollfds, ARRAY_LEN(pollfds), -1) < 0) {
            perror("failed to poll fds");
            return 1;
        }

        if (pollfds[0].revents & POLLIN) {
            if (wayboard_process_libinput(wb) != 0) {
                return 1;
            }
        }
        if (pollfds[1].revents & POLLIN) {
            if (wl_display_dispatch(wb->wl.display) == -1) {
                perror("failed to dispatch wayland display");
                return 1;
            }
        }
    }

    return 0;
}

static void
wayboard_spin_buffer_release(struct wayboard *wb) {
    if (wb->state.buf_released) {
        return;
    }

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    uint64_t start_usec = (uint64_t)start.tv_sec * 1000000 + (uint64_t)start.tv_nsec / 1000;

    while (!wb->state.buf_released) {
        uint64_t now_usec = usec_now();
        if (now_usec - start_usec > 10 * 1000000) {
            fprintf(stderr, "no wl_buffer.release sent after 10 seconds\n");
            goto fail;
        }

        if (wl_display_flush(wb->wl.display) == -1) {
            fprintf(stderr, "failed to flush wayland display while awaiting buffer release\n");
            goto fail;
        }
        if (wl_display_dispatch(wb->wl.display) == -1) {
            fprintf(stderr, "failed to dispatch wayland display while awaiting buffer release\n");
            goto fail;
        }
    }

    return;

fail:
    wb->state.should_close = true;
    return;
}

int
main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s CONFIG_FILE\n", argv[0] ? argv[0] : "wayboard");
        return 1;
    }

    struct wayboard wb = {0};

    if (init_libinput(&wb) != 0) {
        return 1;
    }
    if (init_read_config(&wb, argv[1]) != 0) {
        goto fail_config;
    }
    if (init_wayland(&wb) != 0) {
        goto fail_wayland;
    }
    if (init_fcft(&wb) != 0) {
        goto fail_fcft;
    }
    if (init_render(&wb) != 0) {
        goto fail_render;
    }

    int ret = wayboard_run(&wb);

    pixman_image_unref(wb.state.pixman_image);
    fcft_fini();
    wayboard_fini_wl(&wb);
    cfg_destroy(&wb.cfg);
    libinput_unref(wb.libinput);
    udev_unref(wb.udev);
    return ret;

fail_render:
    fcft_fini();

fail_fcft:
    wayboard_fini_wl(&wb);

fail_wayland:
    cfg_destroy(&wb.cfg);

fail_config:
    libinput_unref(wb.libinput);
    udev_unref(wb.udev);
    return 1;
}
