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
    spatial_table_size: v2u32,
    size: v2u32,
    grid_min: v2f,
    grid_size: v2f,
    cell_size: v2f,

    num_particles: u32,
    table_depth: u32,

    speed_base: f32,
    radius: f32,

    separation: f32,
    inertia: f32,

    drag: f32,
    speed_max: f32,

    delta_time: f32,
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
struct Cells {
    cells: array<u32>,
}; 

@group(0) @binding(0) var<uniform> args : Args;  
@group(0) @binding(1) var<storage, read_write> particles: ParticleData;
@group(0) @binding(2) var<storage, read> flow_directions : Vectors;   
@group(0) @binding(3) var<storage, read> map_data : Cells; 


struct Table {
    cells: array<i32>,
};   
@group(0) @binding(4) var<storage, read> table : Table;  
@group(0) @binding(5) var<storage, read_write> dbg : Table; 
//@group(0) @binding(4) var<storage, read_write> particles: ParticleData;

fn hash(pos: v2i32) -> u32 {
    return u32(pos.x) | (u32(pos.y) << 12);
}
fn lerp(s: f32, e: f32, t: f32) -> f32 {
    return s + ((e - s) * t);
}
fn lerpVec(s: v2f, e: v2f, t: f32) -> v2f {
    return s + ((e - s) * t);
}  

override disabled:bool = false;
override solver_disabled:bool = false;
override buckets_disabled:bool = false;

 
fn solve_v1(row_offset: u32, row_idx: u32, lines_per_inv: u32) {
    let offsets: array<i32, 9> = array<i32, 9>(
        0, // self
        -i32(args.spatial_table_size.x) - 1, // top-left
        -i32(args.spatial_table_size.x) + 0, // top (CW sart)
        -i32(args.spatial_table_size.x) + 1, // top-right
        -1, // left
        1, // right
        i32(args.spatial_table_size.x) - 1, // bot-left
        i32(args.spatial_table_size.x) + 0, // bot
        i32(args.spatial_table_size.x) + 1, // bot-right
    );

    let max_index = args.spatial_table_size.x * (args.spatial_table_size.y - 1);

    let base_idx = (row_offset + row_idx) * args.spatial_table_size.x;
    let r = args.radius;
    let len = (args.spatial_table_size.x - 1);
 
    // dbg.cells[dbg_c] = i32(row_idx + row_offset);
    // dbg.cells[dbg_c + 1u] = i32(row_offset);

    for (var num_line = 0u; num_line < lines_per_inv; num_line++) {

        //   dbg.cells[(num_line * args.spatial_table_size.x + base_idx) * 2] = i32(row_offset);
        //    dbg.cells[(num_line * args.spatial_table_size.x + base_idx) * 2 + 1] = i32(row_idx + num_line);

        for (var this_line_index: u32 = 1u; this_line_index < len; this_line_index++) {
            let actual_idx: u32 = this_line_index + base_idx + num_line * args.spatial_table_size.x;
            //dbg.cells[actual_idx * 2] = i32(actual_idx);
            //dbg.cells[actual_idx * 2 + 1] = i32(row_idx + row_offset + num_line);
            if actual_idx >= max_index {return;}

            // for every entry in list untill -1 or list len
            for (var cell_depth_pos: u32 = 0u; cell_depth_pos < args.table_depth ; cell_depth_pos++) {

                let main_particle_idx: i32 = table.cells[actual_idx + cell_depth_pos];
                if main_particle_idx < 0 {break;} // use select

                let pos: v2f = particles.data[main_particle_idx].pos;

                // for every neighboring cell AND self
                for (var j: u32 = 0u; j < 9u ; j++) {
                    let neighbor_cell_idx: u32 = u32(offsets[j] + i32(actual_idx));
                
                    // again, iter list of enries
                    for (var inner_depth_pos = 0u; inner_depth_pos < args.table_depth ; inner_depth_pos++) {
                        let other_particle_idx = table.cells[neighbor_cell_idx + inner_depth_pos];
                        if other_particle_idx < 0 {break;} // use select

                        let pos_other = particles.data[other_particle_idx].pos;
                        let diff = pos_other - pos;
                        let separate: f32 = (length(diff) - 2 * r) * 0.15 ;


                        particles.data[main_particle_idx].vel += select(v2f(), normalize(-diff) * abs(separate), diff.x != 0 && separate < 0);
                        particles.data[other_particle_idx].vel += select(v2f(), normalize(diff) * abs(separate), diff.x != 0 && separate < 0);
                    }
                }
            }
        }
    }
}

const solver_chunk  = v2i32(4,4);
override use_solver_v1 = false;
// quads 8 * 8  

// dbg.cells[va * 6] = i32(va);
// dbg.cells[va * 6 + 1] = i32(a_idx);
// dbg.cells[va * 6 + 2] = i32(b_idx);
// dbg.cells[va * 6 + 3] = i32(separate * 100);
// dbg.cells[va * 6 + 4] = i32(11111111);
// dbg.cells[va * 6 + 5] = i32(invalid_cycle);

fn cell_to_idx(cell: v2i32, offset: i32) -> i32 {
    let v = cell.x + cell.y * i32(args.spatial_table_size.y) + offset;
    return v;
}
 
fn cell_to_cell_collision(a: v2i32, b: v2i32, cap: u32) {
    let dummy_particle: i32 = (i32(args.num_particles) - 1);
    let iter_cnt = i32(args.table_depth);
    for (var sx = 0; sx <= iter_cnt; sx++) {
        var va = cell_to_idx(a, 0); 
        // #todo check validity
        var a_idx: i32 = table.cells[va];

        var invalid_cycle: bool = (a_idx < 0) || (a_idx > dummy_particle);
        a_idx = select(a_idx, dummy_particle, invalid_cycle);
        let pos = particles.data[a_idx].pos;

        for (var sy = 0; sy <= iter_cnt; sy++) {
            let ba = cell_to_idx(b, sy);

            var b_idx: i32 = table.cells[ba];
            invalid_cycle = invalid_cycle || (b_idx < 0) || (b_idx > dummy_particle);
            invalid_cycle = invalid_cycle || (b_idx == a_idx);
            b_idx = select(b_idx, dummy_particle, invalid_cycle);

            let pos_other = particles.data[b_idx].pos;
            let diff = pos_other - pos;
            let separate: f32 = (length(diff) - 3.5 * args.radius);

            invalid_cycle = invalid_cycle || (separate > 0) || (diff.x == 0);


            let diff_norm = normalize(diff);
            let sep_mag = args.radius * 0.15;
            if !invalid_cycle {
                particles.data[a_idx].vel += (v2f(-diff_norm.x, -diff_norm.y)) * sep_mag;
                particles.data[b_idx].vel += diff_norm * sep_mag;
            }
        }
    }
}

fn solve_v2(gid: u32, num_groups: u32, dbg_val: i32) {
    let depth_len = args.table_depth;
    let max_index = (args.spatial_table_size.x) * (args.spatial_table_size.y) * args.table_depth;
    let stride_x = args.spatial_table_size.x;

    let chunks_in_row: u32 = (args.spatial_table_size.y - 2) / u32(solver_chunk.x) + 1u;
    let box_y: i32 = i32(gid / chunks_in_row);
    let box_x: i32 = i32(gid % chunks_in_row);

    let quad_tl: v2i32 = v2i32(i32(box_x * solver_chunk.x) + 1, i32(box_y * solver_chunk.y) + 1);

    let limit_x: i32 = min(i32(quad_tl.x) + solver_chunk.x, i32(args.spatial_table_size.x - 1));
    let limit_y: i32 = min(i32(quad_tl.y) + solver_chunk.y, i32(args.spatial_table_size.y - 1));

    for (var x: i32 = quad_tl.x; x < limit_x; x++) {
        for (var y: i32 = quad_tl.y; y < limit_y; y++) {

            let cur_cell: v2i32 = v2i32(x, y);

            for (var sx = -1; sx <= 1; sx++) {
                for (var sy = -1; sy <= 1; sy++) {
                    let other_cell: v2i32 = cur_cell + v2i32(sx, sy);
                    cell_to_cell_collision(cur_cell, other_cell, max_index);
                }
            }
        }
    }
}


@compute @workgroup_size(64)  
fn cs_solve(@builtin(workgroup_id) groupd_id: vec3u, @builtin(global_invocation_id) gid: vec3u, @builtin(num_workgroups) num_groups: vec3u) {
    solve_v2(gid.x * 2 + 0, num_groups.x, 10);
}
@compute @workgroup_size(64)  
fn cs_solve_second_pass(@builtin(workgroup_id) groupd_id: vec3u, @builtin(global_invocation_id) gid: vec3u, @builtin(num_workgroups) num_groups: vec3u) {
    solve_v2(gid.x * 2 + 1, num_groups.x, 20);
}


@compute @workgroup_size(64)
fn cs_solve_few(@builtin(global_invocation_id) gid: vec3u, @builtin(local_invocation_id) lid: vec3u) {
   // if solver_disabled { return;}
    let idx: u32 = gid.x;
    if idx > args.num_particles { return;}

    let rsq = args.radius * args.radius;
    let mpos = particles.data[idx].pos;
    let mvel = particles.data[idx].vel;
    var separate: v2f = v2f(0, 0);
    let l = args.num_particles;
    var ni = 8;
    for (var i = 0u; i < l; i++) {
        let pos = particles.data[i].pos.xy;
        let diff = pos - mpos;
        separate -= f32((diff.x * diff.x + diff.y * diff.y) < rsq * 1.502) * f32(i != idx) * diff;
    }
    particles.data[idx].vel += select(v2f(), normalize(separate) * args.radius * 12.5, separate.x != 0);
}

fn wall_collide(center: v2f, cell: v2i32, delta_vec: v2f) -> v2f {
    // #fixme - collide with walls
    let oob = cell.x < 0 || cell.y < 0 || cell.x >= i32(args.size.x) || cell.y >= i32(args.size.y) ;
    if !oob {
        let blocked = (map_data.cells[cell.x + cell.y * i32(args.size.x)] & (1u << 15u)) == 0;
        if blocked {
            return delta_vec;
        }
    }

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
fn cs_main(@builtin(global_invocation_id)gid: vec3u, @builtin(local_invocation_id)lid: vec3u) {
    if disabled { return;}
    let idx: u32 = gid.x;
    let offsets: array<i32, 4> = array<i32, 4>(
        -i32(args.size.x) + 0, // top (CW sart) 
        -1, // left
        1, // right 
        i32(args.size.x) + 0, // bot 
    );
    if idx > args.num_particles { return;}

    let dt = select(args.delta_time, 0.032, args.delta_time > 0.032);

    let grid_min = args.grid_min;
    let cell_sz = args.cell_size;
    let pos = particles.data[idx].pos;
    let pos_rel_to_min = pos - grid_min;
    //pos_rel_to_min.y = args.grid_size.y - pos_rel_to_min.y; // flip y;

    var cell_x = u32((pos_rel_to_min.x / cell_sz.x));
    var cell_y = args.size.y - u32(((pos_rel_to_min.y / cell_sz.y) + 1.0));
    cell_x = min(cell_x, args.size.x);
    cell_y = min(cell_y, args.size.y);
    let this_cell: v2i32 = v2i32(i32(cell_x), i32(cell_y));
    let cell_idx = cell_y * args.size.x + cell_x;

    var flow_dir: v2f = flow_directions.cells[cell_idx];

    let cur_vel = particles.data[idx].vel;
    let target_vel = flow_dir * args.speed_base * 2 ;
 
    // for (let i = 0; i < 4;)      
 
    var vel = cur_vel * (1.0 - args.drag * dt);
    vel = select(vel, target_vel, vel.x == 0 && vel.y == 0);
    vel = lerpVec(vel, target_vel, dt * ((2 - args.inertia) * 2 + 0.5));

    particles.data[idx].vel = clamp(vel, v2f(-4, -4), v2f(4, 4));
    var delta_vec = particles.data[idx].vel * dt * 0.5 ; 

    // #fixme only check in direction of movement
    delta_vec = wall_collide(pos, this_cell + v2i32(0, 1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(0, -1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(-1, 0), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(1, 0), delta_vec);
 
    delta_vec = wall_collide(pos, this_cell + v2i32(1, 1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(-1, -1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(-1, 1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(1, -1), delta_vec);
    // split into two to reduce chance of glitching out
    delta_vec = particles.data[idx].vel * dt * 0.5 ;
    delta_vec = wall_collide(pos, this_cell + v2i32(0, 1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(0, -1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(-1, 0), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(1, 0), delta_vec);

    delta_vec = wall_collide(pos, this_cell + v2i32(1, 1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(-1, -1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(-1, 1), delta_vec);
    delta_vec = wall_collide(pos, this_cell + v2i32(1, -1), delta_vec);
  
    particles.data[idx].pos += delta_vec;
}
