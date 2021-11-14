# input-display
`input-display` is a small, simple application for displaying keyboard inputs on Linux. At the moment, it requires that you are using X11.

# Usage
`input-display {PATH_TO_CONFIGURATION_FILE}`
- Start `input-display` with the specified configuration file.

You will also need to make sure that you have all of the necessary [dependencies.](#dependencies)

# Configuration
A sample configuration file can be found [here.](https://github.com/WoofWoofDoggo/input-display/blob/main/sample_keyboard.json)

As manually writing a configuration file is far more tedious this way than
with the previous Rust version, I am planning on creating a simple
configuration UI.

# Dependencies
For running `input-display`, you will need:
- `libxcb`
- `sdl2`

Additionally, for building `input-display`, you will need to make sure that you have:
- The `sdl2` development headers
  - These come with `sdl2` on certain distributions, such as Arch.
  - On others (such as Debian), you will need `libsdl2-dev` (or your distribution's equivalent).
- The [Zig](https://github.com/ziglang/zig) compiler

# Planned Features
- A configuration GUI
- Fix crash on relative path
- Improve error messages and handling
- Add support for text/image rendering
