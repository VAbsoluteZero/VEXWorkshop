struct Uniforms {
    mvp: mat4x4<f32>,
    data1: vec4<f32>, 
    data2: vec4<f32>, 
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
    out.pos = uniforms.mvp * vec4<f32>(in.pos, 1.0);
    //out.pos = vec4<f32>(in.pos, 1.0); pow(in.uv, vec2<f32>(8,8))
    out.color = vec4<f32>(uniforms.data2.y, uniforms.data2.y, 0.3, 1.0);
    out.uv = in.uv;
    return out;
}
const pi : f32 = 3.14159265359; 
const pi2 : f32 = pi * 2; 
alias v2f = vec2<f32>;


@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var color = in.color;
    var side_len = 16.;// uniforms.data2.y;
    let scale_bias = uniforms.data2.z / 100.0f;
    var subdiv_adjusted = round((4.0 * scale_bias) / 4.0f);
    subdiv_adjusted *= 4.0f; 
    
    let main_freq: v2f = in.uv * pi2 * side_len ;
    let sub_main_freq: v2f = in.uv * pi2 * side_len * subdiv_adjusted;
    var uv_sin: vec2<f32> = cos(main_freq) + // 
        (cos(5 * main_freq) * 0.1) - v2f(1.05, 1.05);
    var uv_sin_adj: vec2<f32> = cos(sub_main_freq) + //
        (cos(5 * sub_main_freq) * 0.5) - v2f(1.05, 1.05);

    var p = uv_sin ;
    var p2 = uv_sin_adj ;

    var val_max = max(p.x, p.y);
    var val_max_subdiv = max(p2.x, p2.y); 

    let pos_v = max(val_max, 0.0);
    color.a =  round(pos_v * 20) * 0.55 +  (pos_v * 4) * 0.15; 
    // if subdiv_adjusted > 1 {
    //     color.a += max(val_max_subdiv, 0) * 0.3;
    // }   
    return color;
} 