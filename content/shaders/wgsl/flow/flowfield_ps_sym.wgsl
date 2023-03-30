// replace_start
const pi : f32 = 3.14159265359; 
const pi2 : f32 = pi * 2; 
alias v2f = vec2<f32>;
alias v4f = vec4<f32>;
alias v2u32 = vec2<u32>;
alias mtx4 =  mat4x4<f32>;
// replace_end

struct Args {
    size: v2u32,
    grid_min: v2f,
    grid_size: v2f,
    cell_size: v2f,
    num_particles: u32,
    speed_base: f32,
    radius: f32,
    delta_time: f32,
    flags: u32,
} 
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

@group(0) @binding(0) var<uniform> args : Args;  
@group(0) @binding(1) var<storage, read_write> particles: ParticleData;
@group(0) @binding(2) var<storage, read> flow_directions : Vectors;   
const vecs: array<v2f, 8> = array<v2f, 8>(
    v2f(-1, 1), // top-left
    v2f(0, 1), // top (CW sart)
    v2f(1, 1), // top-right
    v2f(-1, 0), // left
    v2f(1, 0), // right
    v2f(-1, -1), // bot-left
    v2f(0, -1) *1.00001, // bot
    v2f(1, -1), // bot-right
);

fn lerp(s: f32, e: f32, t: f32) -> f32 {
    return s + ((e - s) * t);
}
fn lerpVec(s: v2f, e: v2f, t: f32) -> v2f {
    return s + ((e - s) * t);
} 

override disabled:bool = false;

//const drag = f32(0.05);

@compute @workgroup_size(64) // @builtin(num_workgroups) num_workers: vec3u
fn cs_main(@builtin(global_invocation_id) gid: vec3u, @builtin(local_invocation_id) lid: vec3u) {

    if disabled { return;}
    let idx: u32 = gid.x;
    if idx > args.num_particles { return;}

    let size_1d: u32 = args.size.x * args.size.y;

    let grid_min = args.grid_min;
    let cell_sz = args.cell_size;
    let pos_rel_to_min = particles.data[idx].pos - grid_min;
    //pos_rel_to_min.y = args.grid_size.y - pos_rel_to_min.y; // flip y;

    var cell_x = u32((pos_rel_to_min.x / cell_sz.x));
    var cell_y = args.size.y - u32(((pos_rel_to_min.y / cell_sz.y) + 1.0));
    cell_x = min(cell_x, args.size.x);
    cell_y = min(cell_y, args.size.y);
    let cell_idx = cell_y * args.size.x + cell_x;

    var flow_dir: v2f = flow_directions.cells[cell_idx];

    let cur_vel = particles.data[idx].vel;
    let target_vel = flow_dir * args.speed_base ;

    var vel = cur_vel * (1.0 - 0.17 * args.delta_time);
    vel += target_vel * args.delta_time * 1.3;

    particles.data[idx].vel = clamp(vel, v2f(-1.2, -1.2), v2f(1.2, 1.2));
    particles.data[idx].pos += particles.data[idx].vel * args.delta_time ; //

    // let stride: u32 = (size_1d / (num_workers.x * 64));
    // let group_offset = gid.x * stride * 64;
    // let start_idx: u32 = loc_idx * stride + group_offset;// + stride * 64 * gid.x;
    // if start_idx >= size_1d {
    //     return;
    // }
}
