use gtk::prelude::*;
use gtk::{Application, ApplicationWindow};

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