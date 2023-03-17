struct VertexInput {
    @location(0) pos: vec3<f32>,
    @location(1) color: vec3<f32>, 
};
struct VertexOutput {
    @builtin(position) pos: vec4<f32>, 
    @location(0) color: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    out.pos = vec4<f32>(in.pos, 1.0); 
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return vec4<f32>(in.color);
}
