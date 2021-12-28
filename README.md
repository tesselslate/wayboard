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
`input-display {CONFIG_FILE} ...`
- Start `input-display` with the specified configuration file.

# Configuration
`input-display` accepts configuration files in a human-readable format. You can see a sample with some additional documentation comments [here.](https://github.com/WoofWoofDoggo/input-display/blob/main/example.cfg)

**Tip:** You can get keycodes using `xmodmap -pke`. If you want to find a specific letter or number, then try `xmodmap -pke | grep '= <letter/num>'`.

# Dependencies
For running `input-display`, you will need:
- `libconfig`
- `libx11` and `libxcb`
- `sdl2`

Additionally, for building `input-display`, you will need:
- A C compiler toolchain (currently only tested with `gcc`)
- GNU `make`
- The development headers for all required libraries
  - These come with the normal packages on some distributions (e.g. Arch)
  - On others (such as Debian), you'll have to get them manually

# Planned Features
- A configuration GUI
- Add support for text/image rendering
