use crate::gl_wrapper::*;
use crate::keyboard_capture::Keymap;

pub struct Color(pub f32, pub f32, pub f32, pub f32);

pub struct Keyboard {
    pub keys: Vec<KeyboardElement>,

    pub background: Color,
    pub pressed: Color
}

pub struct KeyboardElement {
    pub pressed: bool,
    pub area: Rectangle,
    pub keys: Vec<u8>,

    pub vertices: Option<glium::vertex::VertexBuffer<Vertex>>
}

impl Keyboard {
    pub fn new(background: Color, pressed: Color) -> Self {
        Keyboard { 
            keys: Vec::new(), 
            background: background, 
            pressed: pressed
        }
    }

    pub fn update(&mut self, keymap: &mut Keymap) {
        for key in self.keys.iter_mut() {
            key.update(keymap);
        }
    }
}

impl KeyboardElement {
    pub fn new(area: Rectangle, keys: Vec<u8>) -> Self {
        KeyboardElement {
            pressed: false,
            area: area, 
            keys: keys,
            vertices: None
        }
    }

    pub fn update(&mut self, keymap: &Keymap) {
        self.pressed = false;

        for key in self.keys.iter() {
            if keymap.get_key(*key) {
                self.pressed = true;
            }
        }
    }
}