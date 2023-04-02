// replace_start
const pi : f32 = 3.14159265359; 
const pi2 : f32 = pi * 2; 
alias v2f = vec2<f32>;
alias v4f = vec4<f32>;
alias v2i32 = vec2<i32>;
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
struct Cells {
    cells: array<u32>,
}; 

@group(0) @binding(0) var<uniform> args : Args;  
@group(0) @binding(1) var<storage, read_write> particles: ParticleData;
@group(0) @binding(2) var<storage, read> flow_directions : Vectors;   
@group(0) @binding(3) var<storage, read> map_data : Cells; 
//@group(0) @binding(4) var<storage, read_write> particles: ParticleData;

fn lerp(s: f32, e: f32, t: f32) -> f32 {
    return s + ((e - s) * t);
}
fn lerpVec(s: v2f, e: v2f, t: f32) -> v2f {
    return s + ((e - s) * t);
} 
const drag = f32(0.05);

override disabled:bool = false;
override solver_disabled:bool = false;
override buckets_disabled:bool = false;


fn wall_collide(center: v2f, cell: v2i32, delta_vec: v2f) -> v2f {
    // #fixme - collide with walls
    if cell.x < 0 || cell.y < 0 || cell.x >= i32(args.size.x) || cell.y >= i32(args.size.y) {  return delta_vec;}
    if (map_data.cells[cell.x + cell.y * i32(args.size.x)] & (1u << 15u)) == 0 { return delta_vec;}

    let cell_f = v2f(cell);
    let orig = v2f(args.grid_min.x, args.grid_min.y + args.grid_size.y);

    let box_tl = v2f(orig + v2f(args.cell_size.x * cell_f.x, -args.cell_size.y * cell_f.y));
    let box_br = v2f(orig + v2f(args.cell_size.x * (cell_f.x + 1), -args.cell_size.y * (cell_f.y + 1)));

    let moved_center = center + delta_vec;
    let cx = clamp(moved_center.x, box_tl.x, box_br.x);
    let cy = clamp(moved_center.y, box_br.y, box_tl.y);

    let diff = v2f(cx, cy) - moved_center;
    let diff_len_sq = diff.x * diff.x + diff.y * diff.y;
    let r = args.radius;
    if diff_len_sq > r * r { return delta_vec;}

    if diff_len_sq == 0 {
        return delta_vec - delta_vec * 0.999;
    }

    let out_v = delta_vec - normalize(diff) * (r - sqrt(diff_len_sq));

    return  out_v;
} 

@compute @workgroup_size(64)  
fn cs_bucket_particles(@builtin(global_invocation_id)  gid: vec3u, @builtin(local_invocation_id) lid: vec3u) {
    if buckets_disabled { return;}
    let idx: u32 = gid.x;
}

// #fixme - spatial collisons instead of avoidance
@compute @workgroup_size(64)  
fn cs_solve(@builtin(global_invocation_id)  gid: vec3u, @builtin(local_invocation_id) lid: vec3u) {
   // if solver_disabled { return;}
    let idx: u32 = gid.x;

    let rsq = args.radius * args.radius;
    let mpos = particles.data[idx].pos;
    let mvel = particles.data[idx].vel;
    var separate: v2f = v2f(0, 0);
    let l = arrayLength(&particles.data);
    var ni = 8;
    for (var i = 0u; i < l; i++) {
        let pos = particles.data[i].pos.xy;
        let diff = pos - mpos;
        separate -= f32((diff.x * diff.x + diff.y * diff.y) < rsq * 1.102 ) * f32(i != idx) * diff;
    }
    particles.data[idx].vel += select(v2f(), normalize(separate) * args.radius * 0.5, separate.x != 0);
}

@compute @workgroup_size(64)  
fn cs_main(@builtin(global_invocation_id)  gid: vec3u, @builtin(local_invocation_id) lid: vec3u) {
    if disabled { return;}
    let idx: u32 = gid.x;
    let offsets: array<i32, 4> = array<i32, 4>(
        -i32(args.size.x) + 0, // top (CW sart) 
        -1, // left
        1, // right 
        i32(args.size.x) + 0, // bot 
    );
    if idx > args.num_particles { return;}

    let size_1d: u32 = args.size.x * args.size.y;

    let grid_min = args.grid_min;
    let cell_sz = args.cell_size;
    let pos = particles.data[idx].pos;
    let pos_rel_to_min = pos - grid_min;
    //pos_rel_to_min.y = args.grid_size.y - pos_rel_to_min.y; // flip y;

    // #fixme - replace with spatial lookup & prepass
    var cell_x = u32((pos_rel_to_min.x / cell_sz.x));
    var cell_y = args.size.y - u32(((pos_rel_to_min.y / cell_sz.y) + 1.0));
    cell_x = min(cell_x, args.size.x);
    cell_y = min(cell_y, args.size.y);
    let this_cell: v2i32 = v2i32(i32(cell_x), i32(cell_y));
    let cell_idx = cell_y * args.size.x + cell_x;

    var flow_dir: v2f = flow_directions.cells[cell_idx];

    let cur_vel = particles.data[idx].vel;
    let target_vel = flow_dir * args.speed_base * 3 ;

    var vel = cur_vel * (1.0 - drag * args.delta_time);
    vel = select(vel, target_vel, vel.x == 0 && vel.y == 0);
    vel = lerpVec(vel, target_vel, args.delta_time * 2.8);

    particles.data[idx].vel = clamp(vel, v2f(-3, -3), v2f(3, 3));
    var delta_vec = particles.data[idx].vel * args.delta_time ; 

    // #fixme only check in direction of movement
    delta_vec = wall_collide(pos, this_cell + v2i32(0, 1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(0, -1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(-1, 0), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(1, 0), delta_vec);

    delta_vec = wall_collide(pos, this_cell + v2i32(1, 1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(-1, -1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(-1, 1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(1, -1), delta_vec);

    particles.data[idx].pos += delta_vec; // 
}
