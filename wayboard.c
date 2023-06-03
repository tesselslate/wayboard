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

static pixman_image_t *pix = NULL;
static int shm_fd = -1;
static void *shm_data = NULL;

static struct config config = {0};
static uint32_t last_frame = 0;
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
    for (int i = 0; i < 256; i++) {
        if (updated[i]) {
            render_key(&config.keys[i], &states[i]);
            updated[i] = false;
        }
    }
    wl_surface_commit(wl_surface);
    last_frame = time;
}

static void
render_key(struct config_key *key, struct key_state *state) {
    bool active = state->last_release < state->last_press;
    uint64_t time_active = active ? 0 : state->last_release - state->last_press;

    pixman_color_t border, foreground, text;
    if (active) {
        border = config.border_active;
        foreground = config.foreground_active;
        text = config.text_active;
    } else {
        border = config.border_inactive;
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
    if (memcmp(&border, &foreground, sizeof(pixman_color_t)) != 0) {
        // TODO: Draw border
    }
    if (key->time_threshold > 0 && !active) {
        // TODO: Draw text
    } else if (key->text != NULL) {
        // TODO: Draw text
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
    render_clear_buffer();
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
        {"border_active", &config.border_active, true},
        {"border_inactive", &config.border_inactive, true},
        {"text_active", &config.border_active, true},
        {"text_inactive", &config.border_inactive, true},
    };
    for (int i = 0; i < sizeof(base_colors) / sizeof(struct color); i++) {
        struct color color = base_colors[i];
        if (config_lookup_string(&raw_config, color.name, &str)) {
            read_hex_value(str, color.data);
        } else if (!color.optional) {
            panic("missing color: %s", color.name);
        }
    }

    if (config_lookup_string(&raw_config, "font", &str)) {
        config.font = strdup(str);
        if (!config_lookup_int(&raw_config, "font_size", &config.font_size)) {
            panic("missing font size");
        }
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
    config.count = count;
    for (int i = 0; i < count; i++) {
        int scancode;
        config_setting_t *key = config_setting_get_elem(keys, i);
        if (config_setting_lookup_int(key, "x", &config.keys[i].x) &&
            config_setting_lookup_int(key, "y", &config.keys[i].y) &&
            config_setting_lookup_int(key, "w", &config.keys[i].w) &&
            config_setting_lookup_int(key, "h", &config.keys[i].h) &&
            config_setting_lookup_int(key, "scancode", &scancode)) {
            config.keys[i].scancode = scancode;
            double time_threshold;
            if (config_setting_lookup_float(key, "time_threshold", &time_threshold)) {
                config.keys[i].time_threshold = time_threshold;
            }
            if (config_setting_lookup_string(key, "text", &str)) {
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
}

int
main(int argc, char **argv) {
    // TODO: Implement root privilege dropping.
    setup_libinput();

    if (argc != 2) {
        fprintf(stderr, "USAGE: %s CONFIG_FILE\n", argv[0]);
    }
    read_config(argv[1]);

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
                for (int i = 0; i < config.count; i++) {
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
