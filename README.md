# wayboard

`wayboard` is a libinput-based keyboard input display for Wayland. This
repository was previously home to an X11-based keyboard display. See the
`x11` tag if you are interested in that.

> **Disclaimer:** This is my first ever attempt at writing a program for
> Wayland. I make no guarantees that it is functional, stable, or secure.
> It may not work on more elaborate setups.

# Installation

First, install the necessary [dependencies](#dependencies).

```
$ git clone https://github.com/woofdoggo/wayboard
$ cd wayboard
$ meson setup build
$ ninja -C build install
```

If your user is not a member of the `input` group, set the setuid bit on the
`wayboard` binary.

```
# chmod u+s $(which wayboard)
```

## Dependencies

- `fcft`
- `libconfig`
- `libinput`
- `libudev`
- `pixman`
- `tllist`
- `wayland-client` (`libwayland`)

Build-only:
- C compiler
- `meson`
- `wayland-protocols`

# Configuration

See the [example](https://github.com/woofdoggo/input-display/blob/main/example.cfg)
configuration file.
