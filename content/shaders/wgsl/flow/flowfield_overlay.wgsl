
const pi : f32 = 3.14159265359; 
const pi2 : f32 = pi * 2; 
alias v2f = vec2<f32>;
alias v3f = vec3<f32>;
alias v4f = vec4<f32>;
alias v2u32 = vec2<u32>;
alias mtx4 =  mat4x4<f32>;

struct Uniforms {
     camera_vp: mtx4,
     color1: v4f,
     color2: v4f,
     data1: v4f,
     data2: v4f,
     quad_size: v2f,
     bounds: v2u32,
}; 

struct VertexOutput {
  @builtin(position) pos: vec4<f32>,
  @location(0) uv: vec2<f32>,
}

struct Vectors {
    cells: array<v2f>,
}; 

@group(0) @binding(0) var<uniform> u: Uniforms;
@group(0) @binding(1) var<storage, read> vecmap: Vectors;
@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOutput {
    const uv = array(
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.0, 0.0),
    );

    let num_cell: u32 = i / 6;
    let cell_xy = v2u32(num_cell % u.bounds.x, num_cell / u.bounds.x);
    let dir = vecmap.cells[num_cell];
    let size: v2f = v2f(u.data1.z, u.data1.w);

    let v_end = dir * size.y * 0.6;
    let dx = dir.x * size.x * 0.05;
    let dy = dir.y * size.x * 0.05;

    let pos2 = array(
        vec2(dy, -dx),
        v_end + vec2(dy, -dx),
        v_end + vec2(-dy, dx),
        vec2(dy, -dx), // second tri
        v_end + vec2(-dy, dx),
        vec2(-dy, dx),
    );
    let cell_offset = v2f(size.x * f32(cell_xy.x), -size.y * f32(cell_xy.y));
    let orig: v2f = v2f(-u.quad_size.x, u.quad_size.y) * 0.5 + cell_offset - v2f(-size.x, size.y) * 0.5;

    var output: VertexOutput;
    output.pos = v4f(10, 10, 10, 1);
    output.uv = v2f(-10, -10);
    if dir.x == 0 && dir.y == 0 {
        return output;
    } 
    let p: v2f = orig + pos2[i % 6];

    output.pos = u.camera_vp * v4f(p.x, p.y, 1, 1);
    output.uv = uv[i];
    return output;
} 
@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    if in.uv.x < -1 {discard;}
    return v4f(0.50, 0.99, 0.6, 0.99);
    // var p: v2u32 = v2u32(u32(in.uv.x * f32(u.bounds.x)), u32(in.uv.y * f32(u.bounds.y)));
    // var tile: v2f = vecmap.cells[p.y * u.bounds.x + p.x];
    // if tile.x == 0 && tile.y == 0 {
    //     discard;
    // }
    // if (tile.x * 0) < 200000 {
    //     discard;
    // }
    // return v4f((tile.xy + v2f(1, 1)) * 0.5, 0, 0.4);
} 