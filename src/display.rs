use crate::gl_wrapper::*;
use crate::keyboard_capture::Keymap;
use crate::keyboard::*;
use glium::{Surface, glutin, uniform, vertex::VertexBuffer};

pub fn start_display(mut keyboard: Keyboard) {
    // Set up X server connection and keymap
    let (conn, _) = x11rb::connect(None).expect("Failed to connect to X server");
    let mut km = Keymap::new(conn);

    // Glium setup
    let event_loop = glutin::event_loop::EventLoop::new();
    let wb = glutin::window::WindowBuilder::new()
        .with_title("Input Display")
        .with_inner_size(glium::glutin::dpi::PhysicalSize::new(160, 160))
        .with_resizable(false);

    let cb = glutin::ContextBuilder::new()
        .with_vsync(true);

    let display = glium::Display::new(wb, cb, &event_loop).expect("Failed to create window");
    let index_buffer = glium::index::NoIndices(glium::index::PrimitiveType::TrianglesList);
    let program = get_program(&display);
    let pressed_color: [f32; 4] = [keyboard.pressed.0, keyboard.pressed.1, keyboard.pressed.2, keyboard.pressed.3];

    // Set up vertex buffers for each key
    for key in keyboard.keys.iter_mut() {
        let vtx = VertexBuffer::new(&display, &key.area.vertices()).expect("Failed to create vertex buffer");
        key.vertices = Some(vtx);
    }

    event_loop.run(move |ev, _, control_flow| {
        let next_frame_time = std::time::Instant::now() + std::time::Duration::from_nanos(16_666_667);

        match ev {
            glutin::event::Event::WindowEvent { event, .. } => match event {
                glutin::event::WindowEvent::CloseRequested => {
                    *control_flow = glutin::event_loop::ControlFlow::Exit;
                    return;
                },
                _ => return,
            },
            glutin::event::Event::MainEventsCleared => {
                // Update keyboard state
                km.update_keymap().expect("Failed to update keymap");
                keyboard.update(&mut km);

                let mut target = display.draw();
                let bg = &keyboard.background;
                target.clear_color(bg.0, bg.1, bg.2, bg.3);

                // Update key display
                for key in keyboard.keys.iter() {
                    if key.pressed {
                        target.draw(
                            key.vertices.as_ref().expect("Failed to draw vertices"), 
                            &index_buffer, 
                            &program, 
                            &uniform! { 
                                in_color: pressed_color
                            }, 
                            &Default::default()
                        ).expect("Failed to draw key display");
                    }
                }

                target.finish().expect("Failed to finalize frame");
            },
            _ => (),
        }

        *control_flow = glutin::event_loop::ControlFlow::WaitUntil(next_frame_time);
    });
}