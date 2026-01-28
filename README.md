# wayboard

`wayboard` is a libinput-based keyboard input display for Wayland.

> [!TIP]
> This repository previously contained a keyboard display for X11. If you are
> interested, take a look at the `x11` tag.

# Building

The following dependencies are required only at build time:

- `wayland-protocols`

The following dependencies are required for both building and running:

- `fcft`
- `libconfig`
- `libinput`
- `libudev`
- `pixman`
- `wayland-client`

To build and install wayboard, clone the repository and run `make
install`.

> [!IMPORTANT]
> wayboard requires additional privileges to read keyboard input. If your user
> is not a member of the `input` group, you can set the setuid bit on the
> `wayboard` binary.
>
> ```
> # chmod u+s $(which wayboard)
> ```

# License

wayboard is licensed under the GNU General Public License v3 **only**, no later
version. wayboard is partially based off of MIT-licensed code from other
projects; see the source code for more information.

# Configuration

See the [example](https://github.com/tesselslate/wayboard/blob/main/example.cfg)
configuration file.
