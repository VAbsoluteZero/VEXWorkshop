
const pi : f32 = 3.14159265359; 
const pi2 : f32 = pi * 2; 
alias v2f = vec2<f32>;
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

struct Cells {
    cells: array<u32>,
}; 

@group(0) @binding(0) var<uniform> u: Uniforms;
@group(0) @binding(1) var<storage, read> heatmap: Cells;
@vertex
fn vs_main(@builtin(vertex_index) i: u32) -> VertexOutput {
    const pos = array(
        vec2(1.0, 1.0),
        vec2(1.0, -1.0),
        vec2(-1.0, -1.0),
        vec2(1.0, 1.0),
        vec2(-1.0, -1.0),
        vec2(-1.0, 1.0),
    );

    const uv = array(
        vec2(1.0, 0.0),
        vec2(1.0, 1.0),
        vec2(0.0, 1.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(0.0, 0.0),
    );

    var output: VertexOutput;
    output.pos = u.camera_vp * //
        vec4(pos[i].x * u.quad_size.x * 0.5, pos[i].y * u.quad_size.y * 0.5, 5.0, 1.0);
    output.uv = uv[i];
    return output;
} 
@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var p: v2u32 = v2u32(u32(in.uv.x * f32(u.bounds.x)), u32(in.uv.y * f32(u.bounds.y)));

    var tile = heatmap.cells[p.y * u.bounds.x + p.x];
    if (tile & 0xf000) > 0 {
        return v4f(0.64342, 0.85543, 0.8349, 1.0);
    }
    var part = tile & 0x7fff;
    if part == 0 {
        return v4f(0.5342, 0.9543, 0.9, 1.0);
    }
    let expected_extreme = f32(u.bounds.x + u.bounds.y);
    var fpart = clamp(0., 1., f32(part) / expected_extreme);
    var r = fpart;// 0.95 + cos(fpart * pi * u.data2.x * 9) * 0.041;
    var b = 0.0532 / (fpart + 0.0021);
    return v4f(r, (1.02 - sin(fpart * pi)) * 1.05 - b * 0.12, b, 1);//mix(0, 0.1, 1 / fpart  )fpart
} 