#include "xdg-shell.h"
#include <fcntl.h>
#include <libconfig.h>
#include <pixman-1/pixman.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#define assert(expr) assert_impl(__func__, __LINE__, #expr, expr)
#define panic(...) panic_impl(__func__, __LINE__, __VA_ARGS__)

struct config_key {
    char *text;
    int x, y, w, h;
    uint8_t scancode;
    bool show_time;
};

struct config {
    char *font;
    int font_size;
    int width, height;
    pixman_color_t background;
    pixman_color_t foreground_active, foreground_inactive;
    pixman_color_t border_active, border_inactive;
    pixman_color_t text_active, text_inactive;
    struct config_key keys[256];
    uint8_t count;
};

static bool stop = false;

static struct wl_buffer *wl_buffer = NULL;
static struct wl_compositor *wl_compositor = NULL;
static struct wl_display *wl_display = NULL;
static struct wl_registry *wl_registry = NULL;
static struct wl_shm *wl_shm = NULL;
static struct wl_surface *wl_surface = NULL;
static struct xdg_wm_base *xdg_wm_base = NULL;
static struct xdg_surface *xdg_surface = NULL;
static struct xdg_toplevel *xdg_toplevel = NULL;

static struct config config;
static pixman_image_t *pix;
static int shm_fd = -1;
static void *shm_data = NULL;

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
assert_impl(const char *func, const int line, const char *expr,
            bool expr_value) {
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
wl_registry_on_global(void *data, struct wl_registry *registry, uint32_t name,
                      const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        wl_compositor =
            wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        wl_shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base =
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
    }
}

static void
wl_registry_on_global_remove(void *data, struct wl_registry *registry,
                             uint32_t name) {
    // Noop.
}

static void
xdg_surface_on_configure(void *data, struct xdg_surface *xdg_surface,
                         uint32_t serial) {
    xdg_surface_ack_configure(xdg_surface, serial);
    wl_surface_commit(wl_surface);
}

static void
xdg_toplevel_on_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    stop = true;
}

static void
xdg_toplevel_on_configure() {
    // Noop.
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_on_configure,
};

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .close = xdg_toplevel_on_close, .configure = xdg_toplevel_on_configure,
    //.configure_bounds
    //.wm_capabilities
};

static const struct wl_registry_listener wl_registry_listener = {
    .global = wl_registry_on_global,
    .global_remove = wl_registry_on_global_remove,
};

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
    wl_buffer = wl_shm_pool_create_buffer(pool, 0, config.width, config.height,
                                          stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);

    wl_surface = wl_compositor_create_surface(wl_compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, wl_surface);
    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);

    xdg_surface_set_window_geometry(xdg_surface, 0, 0, config.width,
                                    config.height);
    xdg_toplevel_set_app_id(xdg_toplevel, "wayboard");
    xdg_toplevel_set_title(xdg_toplevel, "Wayboard");
    xdg_toplevel_set_min_size(xdg_toplevel, config.width, config.height);
    xdg_toplevel_set_max_size(xdg_toplevel, config.width, config.height);
    wl_surface_commit(wl_surface);
    wl_display_roundtrip(wl_display);

    pix = pixman_image_create_bits(PIXMAN_a8r8g8b8, config.width, config.height,
                                   shm_data, stride);
    render_clear_buffer();
    wl_surface_attach(wl_surface, wl_buffer, 0, 0);
    wl_surface_commit(wl_surface);
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
        if (config.width < 0 || config.height < 0 || config.width > 4096 ||
            config.height > 4096) {
            panic("invalid window size");
        }
        panic("missing size information for window");
    }

    config_setting_t *keys = config_lookup(&raw_config, "keys");
    size_t count = config_setting_length(keys);
    if (count > UINT8_MAX) {
        panic("more than %d keys", UINT8_MAX);
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
            int show_time;
            if (config_setting_lookup_bool(key, "show_time", &show_time)) {
                config.keys[i].show_time = show_time;
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

int
main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "USAGE: %s CONFIG_FILE\n", argv[0]);
    }
    read_config(argv[1]);

    wl_display = wl_display_connect(NULL);
    assert(wl_display != NULL);
    wl_registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(wl_registry, &wl_registry_listener, NULL);
    wl_display_roundtrip(wl_display);
    assert(wl_compositor != NULL && wl_shm != NULL && xdg_wm_base != NULL);

    create_window();
    while (wl_display_dispatch(wl_display) != -1 && !stop) {
    }

    cleanup();
    return 0;
}
