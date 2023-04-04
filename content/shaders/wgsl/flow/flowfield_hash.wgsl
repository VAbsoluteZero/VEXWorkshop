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
struct IndexEntry { 
    index: atomic<u32>, 
}  
struct HelperAtomicTable {
    cells: array<IndexEntry>,
}; 
struct Table {
    cells: array<i32>,
};  
struct Dbg {
    cells: array<u32>,
};  

@group(0) @binding(0) var<uniform> u : Args;  
@group(0) @binding(1) var<storage, read> particles: ParticleData; 
@group(0) @binding(2) var<storage, read_write> help_table : HelperAtomicTable;  
@group(0) @binding(3) var<storage, read_write> table : Table; 
@group(0) @binding(4) var<storage, read_write> dbg : Dbg; 

fn hash(pos: v2i32) -> u32 {
    return u32(pos.x) | (u32(pos.y) << 12);
}
 
@compute @workgroup_size(64)  
fn cs_zero(@builtin(workgroup_id) wgid: vec3u, @builtin(global_invocation_id) gid: vec3u, @builtin(num_workgroups) num_groups: vec3u) {

    let total_size_1d = u.spatial_table_size.x * u.spatial_table_size.y;
    let per_invo = total_size_1d / (num_groups.x * 64) + 1;
    let start = gid.x * per_invo;
    for (var line_idx = u32(start); line_idx < start + per_invo; line_idx++) {
        if line_idx >= total_size_1d {return;}
        atomicStore(&help_table.cells[line_idx].index, 0u);
        let loc_idx = line_idx * u.table_depth;
        for (var i = 0u; i < u.table_depth; i++) {
            table.cells[loc_idx + i] = -1;
        }
    }
}

override disabled = true;

@compute @workgroup_size(64)  
fn cs_count(@builtin(global_invocation_id)  gid: vec3u, @builtin(local_invocation_id) lid: vec3u) {
    let particle_idx: u32 = gid.x;

    if particle_idx >= u.num_particles {return;}
    let pos = particles.data[particle_idx].pos;

    let orig = u.grid_min + v2f(0, u.grid_size.y); // -8,8
    let cell_sz = u.grid_size.y / f32(u.spatial_table_size.y);
    let pos_rel = pos - orig;
    var cell_x = u32(pos_rel.x / cell_sz);
    var cell_y = u32(abs(pos_rel.y) / cell_sz);
    cell_x = min(cell_x, u.spatial_table_size.x);
    cell_y = min(cell_y, u.spatial_table_size.y);
    let this_cell: v2i32 = v2i32(i32(cell_x), i32(cell_y));
    let table_idx = cell_y * u.spatial_table_size.x + cell_x;

   // dbg.cells[particle_idx * 4] = u32(cell_x);
    let write_pos: u32 = atomicAdd(&help_table.cells[table_idx].index, 1u);
    //workgroupBarrier();
    // dbg.cells[particle_idx * 4 + 1] = u32(cell_y);
    // dbg.cells[particle_idx * 4 + 2] = u32(table_idx);
    // dbg.cells[particle_idx * 4 + 3] = u32(write_pos);
    let do_not_modify: bool = write_pos >= u.table_depth;
    if do_not_modify == false {
        table.cells[table_idx + write_pos] = i32(particle_idx);
    }
}
