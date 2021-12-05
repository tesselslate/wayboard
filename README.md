# input-display
`input-display` is a small, simple application for displaying keyboard inputs 
on Linux. At the moment, it requires that you are using X11.

# Usage
`input-display {COLORS} ...`
- Start `input-display` with the specified configuration.

You will also need to make sure that you have all of the necessary [dependencies.](#dependencies)

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

> You should probably run `input-display` with a shell script of some sort so you don't have to retype your configuration.

# Dependencies
For running `input-display`, you will need:
- `libxcb`
- `sdl2`

Additionally, for building `input-display`, you will need to make sure that 
you have the development headers for both `libxcb` and `sdl2`.
  - These come with the normal packages on certain distributions, such as Arch.
  - On others (such as Debian), you will need to get them manually.

# Planned Features
- A configuration GUI
- Add support for text/image rendering
