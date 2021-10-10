mod display;
mod keyboard_capture;

use gtk::prelude::*;
use gtk::{Application};

fn main() {
    let app = Application::builder()
        .application_id("com.github.WoofWoofDoggo.input-display")
        .build();

    app.connect_activate(display::start_ui);
    app.run();
}