
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
  @location(1) @interpolate(flat) vert_idx: u32,
}

struct Vectors {
    cells: array<v2f>,
}; 

@group(0) @binding(0) var<uniform> u: Uniforms;
@group(0) @binding(1) var<storage, read> vecmap: Vectors;
@group(0) @binding(2) var<storage, read> subdiv_map: Vectors;
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

    let use_subdiv =  u.data2.y > 0;

    let point_div: u32 = 6u;
    let num_cell: u32 = i / point_div;

    var cell_xy = v2u32();

    if use_subdiv {
        cell_xy = v2u32(num_cell % (u.bounds.x * 2), num_cell / (u.bounds.x * 2));
    } else {
        cell_xy = v2u32(num_cell % u.bounds.x, num_cell / u.bounds.x);
    }

    var dir = v2f();
    if use_subdiv {
        dir = (subdiv_map.cells[num_cell]);
    } else {
        dir = (vecmap.cells[num_cell]);
    }

    let size: v2f = select(v2f(u.data1.z, u.data1.w), v2f(u.data1.z, u.data1.w) * 0.5, use_subdiv);
    let v_end = dir * u.data1.w * select(0.6, 0.4, use_subdiv);

    let main = (i % point_div) >= 3;

    let dx = dir.x * u.data1.z * select(0.09, 0.05, main);
    let dy = dir.y * u.data1.z * select(0.09, 0.05, main);

    let pos2 = array(
        vec2(dy, -dx),
        v_end,
        vec2(-dy, dx),
    );

    let cell_offset = v2f(size.x * f32(cell_xy.x), -size.y * f32(cell_xy.y));
    let orig: v2f = v2f(-u.quad_size.x, u.quad_size.y) * 0.5 + cell_offset - v2f(-size.x, size.y) * select(0.5, 0.6, use_subdiv);

    var output: VertexOutput;
    if (dir.x == 0 && dir.y == 0) || num_cell > arrayLength(&subdiv_map.cells){
        output.pos = v4f(10, 10, 10, 1);
        output.uv = v2f(-10, -10);
        return output;
    }
    let p: v2f = orig + pos2[i % 3];

    output.pos = u.camera_vp * v4f(p.x, p.y, 1, 1);
    output.uv = uv[i];
    output.vert_idx = i;
    return output;
} 
@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    if in.uv.x < -1 {discard;}
    let main = (in.vert_idx % 6) >= 3;
    return v4f(0.50, 0.99, 0.6, select(0.25, 0.7, main));
} 