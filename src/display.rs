use crate::keyboard_capture::Keymap;
use gtk::prelude::*;
use gtk::{Application, ApplicationWindow, Frame};

pub fn start_ui(app: &Application) {
    let window = ApplicationWindow::builder()
        .application(app)
        .default_width(320)
        .default_height(180)
        .title("Input Display")
        .build();

    let fixed = gtk::Fixed::new();

    window.set_child(Some(&fixed));
    window.set_resizable(false);
    window.present();
}

pub struct BoundingBox(i32, i32, i32, i32);
pub struct Color(u8, u8, u8, u8);

pub struct KeyDisplay {
    pub keys: Vec<KeyDisplayElement>,

    pub background: Color,
    pub unpressed: Color,
    pub unpressed_text: Color,
    pub pressed: Color,
    pub pressed_text: Color
}

pub struct KeyDisplayElement {
    pub keys: Vec<u8>,
    pub display: String,
    x: i32,
    y: i32,
    w: i32,
    h: i32,

    pressed: bool,
    frame: Frame
}

// TODO: Serde [de]serialization

impl KeyDisplay {
    pub fn new() -> Self {
        KeyDisplay {
            keys: Vec::new(),
            background: Color(0, 0, 0, 255),
            unpressed: Color(255, 255, 255, 255),
            unpressed_text: Color(0, 0, 0, 255),
            pressed: Color(255, 0, 0, 255),
            pressed_text: Color(0, 0, 0, 255)
        }
    }
}

impl KeyDisplayElement {
    pub fn new(x: i32, y: i32, w: i32, h: i32, keys: Vec<u8>, display: String) -> Self {
        let frame = Frame::new(Some(&display));

        KeyDisplayElement {
            keys: keys,
            display: display,
            x: x,
            y: y,
            w: w,
            h: h,
            pressed: false,
            frame: frame
        }
    }

    pub fn get_position(self: &Self) -> BoundingBox {
        BoundingBox(self.x, self.y, self.w, self.h)
    }

    pub fn get_pressed(self: &Self) -> bool {
        self.pressed
    }

    pub fn update_position(self: &mut Self, rect: &BoundingBox) {
        self.x = rect.0;
        self.y = rect.1;
        self.w = rect.2;
        self.h = rect.3;
    }

    pub fn update_pressed(self: &mut Self, keymap: &Keymap) {
        self.pressed = false;

        for (_, key) in self.keys.iter().enumerate() {
            if keymap.get_key(*key) {
                self.pressed = true;
            }
        }
    }
}