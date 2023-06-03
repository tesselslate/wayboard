#include "xdg-shell.h"
#include <errno.h>
#include <fcft/fcft.h>
#include <fcntl.h>
#include <libconfig.h>
#include <libinput.h>
#include <pixman-1/pixman.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <tllist.h>
#include <uchar.h>
#include <unistd.h>
#include <utf8proc.h>
#include <wayland-client.h>

#undef assert // tllist.h includes assert.h
#define assert(expr) assert_impl(__func__, __LINE__, #expr, expr)
#define panic(...) panic_impl(__func__, __LINE__, __VA_ARGS__)

struct config_key {
    char *text;
    int x, y, w, h;
    uint8_t scancode;
    double time_threshold;
};

struct config {
    char *font;
    int width, height;
    pixman_color_t background;
    pixman_color_t foreground_active, foreground_inactive;
    pixman_color_t text_active, text_inactive;
    struct config_key keys[256];
    uint8_t count;
};

struct key_state {
    uint64_t last_press, last_release;
};

static void assert_impl(const char *func, const int line, const char *expr, bool expr_value);
static void cleanup();
static _Noreturn void panic_impl(const char *func, const int line, const char *fmt, ...);
static void render_clear_buffer();
static void render_frame(uint32_t time);
static void render_key(struct config_key *key, struct key_state *state);
static void create_window();
static void read_hex_value(const char *in, pixman_color_t *out);
static void read_config(const char *path);
static void setup_libinput();

static void libinput_close_restricted(int fd, void *data);
static int libinput_open_restricted(const char *path, int flags, void *data);
static void wl_registry_on_global(void *data, struct wl_registry *registry, uint32_t name,
                                  const char *interface, uint32_t version);
static void wl_registry_on_global_remove(void *data, struct wl_registry *registry, uint32_t name);
static void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time);
static void xdg_surface_on_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial);
static void xdg_toplevel_on_close(void *data, struct xdg_toplevel *xdg_toplevel);
static void xdg_toplevel_on_configure(void *data, struct xdg_toplevel *xdg_toplevel, int32_t width,
                                      int32_t height, struct wl_array *states);
static void xdg_toplevel_on_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel,
                                            struct wl_array *capabilities);
static void xdg_wm_base_on_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial);
