
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
    // facing: v4f,   would be needed later when rotation is possible
     size: v4f, 
     num_particles: u32,
     flags: u32,
};  

struct Vectors {
    cells: array<v2f>,
};  
struct Particle {
    pos: v2f,
    vel: v2f,
}
struct ParticleData {
    data: array<Particle>,
};  
@group(0) @binding(0) var<uniform> u: Uniforms;
@group(0) @binding(1) var<storage, read> particles: ParticleData;
@group(0) @binding(2) var texture: texture_2d<f32>;
@group(0) @binding(3) var tex_sampler: sampler;

struct VertexOutput { 
    @builtin(position) pos: vec4<f32>,
    @location(0) uv: v2f,
    @location(1) color: v4f,
    //@location(2) @interpolate(flat) particle_idx: u32,
}
 
const set_of_colors  = array<v4f, 5>(
    v4f(0.94, 0.5, 0.5, 1.0), // coral 
    v4f(1.0, 0.84, 0.71, 1.0), // peach puff
    v4f(1.0, 1.0, 0.88, 1.0), // yellow 
    v4f(0.5, 1.0, 0.83, 1.0), // aquamarine  
    v4f(0.83, 0.81, 0.63, 1.0) // ?
);

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
    //let cell_xy = v2u32(num_cell % u.bounds.x, num_cell / u.bounds.x);
    let particle = particles.data[num_cell];
    let dir = particle.vel;

    let size: v2f = u.size.xy;
    // let v_end = dir * size.y ;
    // let dx = dir.x * size.x ;
    // let dy = dir.y * size.x;

    let v_end = v2f(0, 1) * size.y ;
    let dx = size.x ;
    let dy = size.y;
    let orig: v2f = particle.pos;
    const pos = array(
        vec2(1.0, 1.0),
        vec2(1.0, -1.0),
        vec2(-1.0, -1.0),
        vec2(1.0, 1.0),
        vec2(-1.0, -1.0),
        vec2(-1.0, 1.0),
    );
    let positions = array(
        vec2(dy, -dx),
        v_end + vec2(dy, -dx),
        v_end + vec2(-dy, dx),
        vec2(dy, -dx), // second tri
        v_end + vec2(-dy, dx),
        vec2(-dy, dx),
    );

    let particle_rel_pos = (abs(particle.vel.x) + abs(particle.vel.y)) / 4.0 ;
    let color_idx = (particle_rel_pos) * 5.5f;
    let ca = i32(floor(color_idx));
    let cb = ca + 1;

    var out_c = mix(set_of_colors[ca], set_of_colors[cb], color_idx - f32(ca));
    out_c = out_c * 0.54f;
    out_c.a = u.color1.w * 0.8f;

    let p: v2f = orig + pos[i % 6] * dx;
    var output: VertexOutput;
    output.pos = u.camera_vp * v4f(p.x, p.y, 0, 1);
    output.uv = uv[i % 6];
    output.color = select(v4f(u.color1.x, u.color1.y, u.color1.z, u.color1.w), out_c, u.flags > 0);
    return output;
} 

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    var tex_color: v4f = textureSample(texture, tex_sampler, in.uv) ;
    return v4f(in.color.xyz, in.color.a * tex_color.w);
} 