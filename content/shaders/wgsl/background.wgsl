
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
  @builtin(position) pos : vec4<f32>,
  @location(0) uv : vec2<f32>,
}

struct Cells {
    cells: array<u32>,
}; 

@group(0) @binding(0) var<uniform> uniforms: Uniforms; 
@vertex
fn vs_main(@builtin(vertex_index) i: u32 ) -> VertexOutput {
   const pos = array(
    vec2( 1.0,  1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2(-1.0,  1.0),
  );

  const uv = array(
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0),
    vec2(1.0, 0.0),
    vec2(0.0, 1.0),
    vec2(0.0, 0.0),
  );
  
  var tc:f32 = cos(uniforms.color1.x*0.1);
  var ts:f32 = sin(uniforms.color1.x*0.1);
  var m:mtx4 = mtx4( 
    tc, -ts, 0., 0.,//
    ts, tc, 0., 0., //
    0., 0., 1., 0.,//
    0., 0., 0., 1.,//
  ); 

  var output : VertexOutput;
  output.pos = vec4(pos[i], 0.8, 1.0);

  var uv4 = v4f(uv[i], 0.0, 1.0); 
  //var uv4 = v4f(tc, ts, 0.0, 1.0);
  output.uv = uv[i].xy * 0.8 + (m * uv4).xy * 0.2; 
  //output.uv =  uv4.xy; 

  return output;
} 
@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> { 
    return v4f(in.uv.x*0.8, in.uv.y * 0.9, 0.4, 1.0);
} 