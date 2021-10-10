pub struct Keymap {
    connection: x11rb::rust_connection::RustConnection,
    keypresses: [bool; 256],
}

impl Keymap {
    pub fn new(connection: x11rb::rust_connection::RustConnection) -> Self {
        Keymap { connection: connection, keypresses: [false; 256] }
    }
    
    pub fn get_key(self: &Self, keycode: u8) -> bool {
        self.keypresses[keycode as usize + 8]
    }

    pub fn update_keymap(self: &mut Self) -> Result<(), Box<dyn std::error::Error>> {
        let keymap = x11rb::protocol::xproto::query_keymap(&self.connection);
        let keymap = match keymap {
            Ok(kbdmap) => kbdmap.reply(),
            Err(err) => return Result::Err(Box::new(err))
        };

        let keymap = match keymap {
            Ok(kbdmap) => kbdmap,
            Err(err) => return Result::Err(Box::new(err))
        };

        for (index, val) in keymap.keys.iter().enumerate() {
            for bit in 0_u8..8_u8 {
                self.keypresses[index * 8 + bit as usize] = val & (1 << bit) != 0;
            }
        }

        Ok(())
    }
}