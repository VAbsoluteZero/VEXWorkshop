struct Uniforms {
    projection: mat4x4<f32>,
    model_view: mat4x4<f32>, 
}; 

struct VertexInput {
    @location(0) pos: vec3<f32>,
    @location(1) norm: vec3<f32>,
    @location(2) uv: vec2<f32>,
};
struct VertexOutput {
    @builtin(position) pos: vec4<f32>, 
    @location(0) uv : vec2<f32>, 
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;
@vertex
fn vs_main(@builtin(vertex_index) i: u32, in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.pos = uniforms.projection 
        * uniforms.model_view 
        * vec4<f32>(in.pos, 1.0); 
    out.uv = in.uv;
    return out;
}

@group(0) @binding(2) var tex_sampler: sampler;
@group(0) @binding(1) var texture: texture_2d<f32>;
@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var tex_color = textureSample(texture, tex_sampler, in.uv) ;
    return tex_color;
} 
