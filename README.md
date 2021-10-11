# input-display
`input-display` is a small, simple application for displaying keyboard inputs on Linux. At the moment, it requires that you are using X11.

# Usage
`input-display [PATH_TO_CONFIGURATION_FILE]`
- Will start the display with the specified configuration file.

`input-display --show-keycodes`
- Will display the keycode of whichever keys you are pressing on screen. These keycodes can be used in the configuration file.

# Configuration
A sample configuration file can be found [here.](https://github.com/WoofWoofDoggo/input-display/blob/main/sample_keyboard)
Your configuration file should follow this format:

```
pressed:r=[0-255],g=[0-255],b=[0-255],a=[0-255]
background:r=[0-255],g=[0-255],b=[0-255],a=[0-255]
key:x=[],y=[],w=[],h=[],keycode=[]
... (more key entries go here)
```

- The `pressed` and `background` entries denote the color of keys when pressed, and the color of the window background, respectively.
- There can be as many `key` entries as you want.
- `key` entries should use the keycodes from `--show-keycodes`.
- The window size is currently fixed at 160x160. Keep this in mind when writing your configuration.

# Disclaimer
This is my first serious attempt at writing an application in Rust. I'm sure the code is not particularly idiomatic or well-written by many's standards. There are some areas where I used likely less-than-optimal methods of solving issues due to my inexperience with the language.