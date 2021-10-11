use glium::{Display, implement_vertex};

#[derive(Clone, Copy, Debug, Default)]
pub struct Vertex {
    position: [f32; 2]
}

implement_vertex!(Vertex, position);

pub struct Rectangle {
    pub x: f32,
    pub y: f32,
    pub w: f32,
    pub h: f32
}

impl Rectangle {
    pub fn new(x: i32, y: i32, w: i32, h: i32) -> Self {
        Rectangle {
            x: (x as f32 / 80.0) - 1.0,
            y: (y as f32 / 80.0) - 1.0,
            w: w as f32 / 80.0,
            h: h as f32 / 80.0
        }
    }

    pub fn vertices(&self) -> Vec<Vertex> {
        vec![
            Vertex {position:[self.x, self.y]},
            Vertex {position:[self.x + self.w, self.y]},
            Vertex {position:[self.x, self.y + self.h]},

            Vertex {position:[self.x + self.w, self.y]},
            Vertex {position:[self.x + self.w, self.y + self.h]},
            Vertex {position:[self.x, self.y + self.h]}
        ]
    }
}

pub fn get_program(display: &Display) -> glium::Program {
    let vertex_shader = r#"
        #version 140

        in vec2 position;
        uniform mat4 matrix;

        void main() {
            gl_Position = vec4(position, 0.0, 1.0);
        }
    "#;

    let fragment_shader = r#"
        #version 140

        out vec4 color;
        uniform vec4 in_color;

        void main() {
            color = in_color;
        }
    "#;

    glium::Program::from_source(display, vertex_shader, fragment_shader, None).expect("Failed to create shader")
}