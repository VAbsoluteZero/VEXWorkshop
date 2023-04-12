struct Uniforms {
    mvp: mat4x4<f32>,
    data1: vec4<f32>, 
    data2: vec4<f32>, 
    grid_sz: vec2<f32>, 
}; 

struct VertexInput {
    @location(0) pos: vec3<f32>,
    @location(1) norm: vec3<f32>,
    @location(2) uv: vec2<f32>,
};
struct VertexOutput {
    @builtin(position) pos: vec4<f32>, 
    @location(0) color: vec4<f32>, 
    @location(1) uv: vec2<f32>, 
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@vertex
fn vs_main(@builtin(vertex_index) i: u32, in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.pos = vec4<f32>(in.pos, 1.0);

    var v = (v2f(out.pos.x, out.pos.y) * uniforms.grid_sz.x * 0.5f);

    out.pos.x = floor(v.x / uniforms.data2.z) * uniforms.data2.z;
    out.pos.y = floor(v.y / uniforms.data2.w) * uniforms.data2.w;

    out.pos = uniforms.mvp * out.pos;
    out.color = uniforms.data1;
    out.uv = in.uv;
    return out;
}

const pi : f32 = 3.14159265359; 
const pi2 : f32 = pi * 2; 
alias v2f = vec2<f32>;
alias v4f = vec4<f32>;
alias v2u32 = vec2<u32>;
alias mtx4 =  mat4x4<f32>;


@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var color = in.color;
    var side_len = uniforms.data2.x;
    var thickness = uniforms.data2.y;

    let main_freq: v2f = in.uv * pi2 * side_len;
    //let sub_main_freq: v2f = in.uv * pi2 * side_len * subdiv_adjusted;
    let mult: f32 = 1;
    var uv_cos: vec2<f32> = cos(main_freq) * (mult);
    let iter_cnt = pi2 * side_len  ;
    var discard_y = cos(iter_cnt * thickness * 0.5) * mult;

    var max_uv = max(uv_cos.x, uv_cos.y);
    if (max_uv) < (discard_y) { 
        discard;
    }

    return color;
} 