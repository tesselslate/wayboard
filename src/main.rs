use crate::gl_wrapper::Rectangle;
use crate::keyboard::*;
use crate::keyboard_capture::Keymap;
use scan_fmt::scan_fmt;
use std::env;
use std::fs;

mod display;
mod gl_wrapper;
mod keyboard_capture;
mod keyboard;

#[cfg(target_os="linux")]
fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() > 1 {
        if args.contains(&String::from("--show-keycodes")) {
            show_keycodes();
        }

        // Read from specified file
        let filename = args.first().unwrap();
        let file_contents = fs::read_to_string(&filename).expect("Failed to read configuration file");
        let file_contents = file_contents.lines();

        let mut pressed: Option<Color> = None;
        let mut background: Option<Color> = None;
        let mut keys: Vec<KeyboardElement> = Vec::new();

        for line in file_contents {
            if line.starts_with("pressed:") {
                let (r, g, b, a) = scan_fmt!(&line, "pressed:r={},g={},b={},a={}", i32, i32, i32, i32).expect("Failed to read pressed key color");
                
                if background.is_none() {
                    background = Some(Color(r as f32 / 255.0, g as f32 / 255.0, b as f32 / 255.0, a as f32 / 255.0));
                } else {
                    println!("Warning: Your configuration file contains multiple pressed key colors");
                }
            } else if line.starts_with("background:") {
                let (r, g, b, a) = scan_fmt!(&line, "background:r={},g={},b={},a={}", i32, i32, i32, i32).expect("Failed to read background key color");
            
                if pressed.is_none() {
                    pressed = Some(Color(r as f32 / 255.0, g as f32 / 255.0, b as f32 / 255.0, a as f32 / 255.0));
                } else {
                    println!("Warning: Your configuration file contains multiple background colors");
                }
            } else if line.starts_with("key:") {
                let (x, y, w, h, keycode) = scan_fmt!(&line, "key:x={},y={},w={},h={},keycode={}", i32, i32, i32, i32, u8).expect("Failed to read key");
            
                let key = KeyboardElement::new(Rectangle::new(x, y, w, h), keycode);
                keys.push(key);
            } else {
                println!("Invalid configuration key, refer to --help");
                std::process::exit(0);
            }
        }

        if pressed.is_none() {
            println!("Your configuration file does not contain a pressed key color.");
            std::process::exit(0);
        }

        if background.is_none() {
            println!("Your configuration file does not contain a background color.");
            std::process::exit(0);
        }

        if keys.len() == 0 {
            println!("Your configuration file does not contain any keys.");
            std::process::exit(0);
        }

        let mut keyboard = Keyboard::new(background.unwrap(), pressed.unwrap());
        for key in keys {
            keyboard.keys.push(key);
        }

        display::start_display(keyboard);
    } else {
        print_help();
    }
}

#[cfg(not(target_os="linux"))]
fn main() {
    println!("input-display currently only works on Linux.");
}

fn print_help() {
    println!(
r#"Usage: input-display [CONFIGURATION_FILE]
    CONFIGURATION_FILE should be a path to a valid configuration file.

Alternatively: input-display --show-keycodes
    This will print the keycodes of any keys you press.
    You can leave this mode with the usual SIGINT (Ctrl+C)

Your configuration file should follow the following format:
    pressed:r=[0-255],g=[0-255],b=[0-255],a=[0-255]
    background:r=[0-255],g=[0-255],b=[0-255],a=[0-255]
    key:x=[],y=[],w=[],h=[],keycode=[]

Considerations:
    There can be any number of key entries.
    The window size is 160x160. Take this into account when determining key positions."#);
}

fn show_keycodes() -> ! {
    let (conn, _) = x11rb::connect(None).expect("Failed to connect to X server");
    let mut keymap = Keymap::new(conn);

    loop {
        keymap.update_keymap().unwrap();

        for n in 0..247_u8 {
            if keymap.get_key(n) {
                println!("Key pressed: {}", n);
            }
        }

        std::thread::sleep(std::time::Duration::from_millis(25));
    }
}