# input-display
`input-display` is a small, simple application for displaying keyboard inputs 
on Linux. At the moment, it requires that you are using X11.

# Usage
`input-display {PATH_TO_CONFIGURATION_FILE}`
- Start `input-display` with the specified configuration file.

You will also need to make sure that you have all of the necessary [dependencies.](#dependencies)

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
- Fix crash on relative path
- Improve error messages and handling
- Add support for text/image rendering
