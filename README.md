# input-display
`input-display` is a small, simple application for displaying keyboard inputs 
on Linux. At the moment, it requires that you are using X11.

# Installation
`input-display` is not currently available on any package repositories. You will have to build and install it from source. It's not too difficult; make sure you have all of the [dependencies](#dependencies) first.

Then, do the following:
- Clone the repository to a directory of your choosing
- Change any configuration values in the `Makefile` as needed
- Run `make install` as root

# Usage
`input-display {COLORS} ...`
- Start `input-display` with the specified configuration.

# Configuration
`input-display` accepts configuration in the form of command line arguments.
The first argument should follow the format `key=HEX,background=HEX`, where `key` denotes the pressed key color and `background` denotes the background color. For example:

```
key=FFFF00,background=000000
```

All following arguments will denote keys on the display. Each should follow this format: `key=(),x=(),y=(),w=(),h=()`, where:
- `key` denotes the keycode of the key
- `x`, `y`, `w`, and `h` denote the position and size of the key
For example:

```
key=40,x=10,y=25,w=50,h=50
```

The size of the display is currently hardcoded at 160x160. Keep this in mind when writing your configuration.

### Tips
- You should probably run `input-display` with a shell script of some sort so you don't have to retype your configuration each time you launch it.
- You can get keycodes using `xmodmap -pke`. If you want to find a specific letter or number, then try `xmodmap -pke | grep '= <letter/num>'`.

# Dependencies
For running `input-display`, you will need:
- `libx11` and `libxcb`
- `sdl2`

Additionally, for building `input-display`, you will need:
- A C compiler toolchain (currently only tested with `gcc`)
- GNU `make`
- The development headers for all 3 required libraries
  - These come with the normal packages on some distributions (e.g. Arch)
  - On others (such as Debian), you'll have to get them manually

# Planned Features
- A configuration GUI
- Add support for text/image rendering
