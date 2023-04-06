// replace_start
const pi : f32 = 3.14159265359; 
const pi2 : f32 = pi * 2; 
alias v2f = vec2<f32>;
alias v3f = vec4<f32>;
alias v4f = vec4<f32>;
alias v2i32 = vec2<i32>;
alias v2u32 = vec2<u32>;
alias mtx4 =  mat4x4<f32>;
// replace_end

struct Args {
    size: v2u32,
    flags: u32,
    dummy: u32,
}
struct Cells {
    cells: array<u32>,
}; 
struct Vectors {
    cells: array<v2f>,
};  

@group(0) @binding(0) var<uniform> args : Args; 
@group(0) @binding(1) var<storage> distances : Cells;
@group(0) @binding(2) var<storage, read_write> flow_directions : Vectors;
@group(0) @binding(2) var<storage, read> flow_read : Vectors;
@group(0) @binding(3) var<storage, read_write> flow_sub2 : Vectors;
// @group(0) @binding(3) var<storage, write> output_gpu : Vectors;

const dist_mask: u32 = ~(1u<<15u);
const write_x: u32 = 24; //0xff00'0000
const write_y: u32 = 16; //0x00ff'0000

const oob_val: u32 = 0xffffffff; 

const top_l: u32 = 0u;  
const top: u32 = 1u; 
const top_r: u32 = 2u; 
const left: u32 = 3u; 
const rigth: u32 = 4u; 
const bot_l: u32 = 5u; 
const bot: u32 = 6u; 
const bot_r: u32 = 7u; 

// 1.00001 mult is for selection bias to avoid vectors canceling out
const vecs: array<v2f, 8> = array<v2f, 8>( 
    v2f(-1, 1), // top-left
    v2f(0, 1), // top (CW sart)
    v2f(1, 1), // top-right
    v2f(-1, 0), // left
    v2f(1, 0), // right
    v2f(-1, -1), // bot-left
    v2f(0, -1) , // bot
    v2f(1, -1), // bot-right
);

override disabled:bool = false;

@compute @workgroup_size(64)
fn cs_main(@builtin(workgroup_id) gid: vec3u, @builtin(local_invocation_id) lid: vec3u, @builtin(num_workgroups) num_workers: vec3u) {

    if disabled { return;}

    let k_wallbias: bool = (args.flags & 1u) > 0;
    let k_smooth: bool = (args.flags & 2u) > 0;

    let loc_idx: u32 = lid.x;
    let size_1d: u32 = args.size.x * args.size.y;
    let stride: u32 = (size_1d / (num_workers.x * 64));
    let group_offset = gid.x * stride * 64;
    let start_idx: u32 = loc_idx * stride + group_offset;// + stride * 64 * gid.x;
    if start_idx >= size_1d {
        return;
    }
    let len: u32 = stride;
    let offsets: array<i32, 8> = array<i32, 8>(
        -i32(args.size.x) - 1, // top-left
        -i32(args.size.x) + 0, // top (CW sart)
        -i32(args.size.x) + 1, // top-right
        -1, // left
        1, // right
        i32(args.size.x) - 1, // bot-left
        i32(args.size.x) + 0, // bot
        i32(args.size.x) + 1, // bot-right
    );

    var c_grid: array<i32, 8> = array(0, 0, 0, 0, 0, 0, 0, 0);

    for (var i: u32 = 0; i < len; i++) {
        let cur_i: u32 = i + start_idx;
        let row_x: u32 = cur_i % args.size.x;

        let cell_raw: u32 = distances.cells[cur_i];
        let cell_val: i32 = i32(cell_raw & dist_mask);

        flow_directions.cells[cur_i] = v2f(0, 0);

        let skip_blocked = u32(cell_raw & (1u << 15u)) != 0;
        if skip_blocked || (cell_val == 0) { continue;}

        let wall_val: i32 = cell_val + i32(k_wallbias) * 1;

        for (var j: i32 = 0; j < 8; j++) {
            let loc_offset: u32 = u32(offsets[j] + i32(cur_i));
            let loc_row_x: u32 = loc_offset % args.size.x;

            let oob: bool = (loc_offset >= size_1d) || (abs(i32(loc_row_x) - i32(row_x)) > 22);
            // test grid boundry
            c_grid[j] = wall_val;
            if oob {continue;}
            let neighbor_val: u32 = distances.cells[loc_offset];
            let neighbor_wall: bool = (neighbor_val & (1u << 15u)) > 0;
            c_grid[j] = select(i32(neighbor_val & dist_mask), wall_val, neighbor_wall);
        }

        var r: v2f = v2f();
        for (var j: i32 = 0; j < 8; j++) {
            let sign_val = sign(cell_val - c_grid[j]) ;// > 0;
            var v: v2f = vecs[j] * f32(sign_val);
            //v *= f32(c_grid[j] != 0);
            r += v;
        }

        flow_directions.cells[cur_i] = select(v2f(0, 0), normalize(r), r.x != 0 || r.y != 0);
    }
}
const c19 = 1.0 /9.0;

const gaussian_kernel: array<f32, 9> = array<f32, 9>(
    1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0,
    2.0 / 16.0, 4.0 / 16.0, 2.0 / 16.0,
    1.0 / 16.0, 2.0 / 16.0, 1.0 / 16.0
);
const box_kernel: array<f32, 9> =  array<f32, 9>(
    c19, c19, c19,
    c19, c19, c19,
    c19, c19, c19
);


const c14 = 1.0 /4.0; 
const mid = 1.0 /2.0; 
const box_kernel_TL: array<f32, 9> =  array<f32, 9>(
    c14, c14, 0.0,
    c14,  mid, 0.0,
    0.0,  0.0, 0.0
);
const box_kernel_TR: array<f32, 9> =  array<f32, 9>(
    0.0, c14, c14,
    0.0, mid, c14,
    0.0, 0.0, 0.0
);
const box_kernel_BR: array<f32, 9> =  array<f32, 9>(
    0.0, 0.0, 0.0,
    0.0, mid, c14,
    0.0, c14, c14
);
const box_kernel_BL: array<f32, 9> =  array<f32, 9>(
    0.0, 0.0, 0.0,
    c14, mid, 0.0,
    c14, c14, 0.0
);
const box_kernel_TL_fx: array<f32, 9> =  array<f32, 9>(
    c14, c14, c14,
    c14,  c14, 0.0,
    0.0,  0.0, 0.0
);
const box_kernel_TR_fx: array<f32, 9> =  array<f32, 9>(
    c14, c14, c14,
    0.0, c14, c14,
    0.0, 0.0, 0.0
);
const box_kernel_BR_fx: array<f32, 9> =  array<f32, 9>(
    0.0, 0.0, 0.0,
    0.0, c14, c14,
    c14, c14, c14
);
const box_kernel_BL_fx: array<f32, 9> =  array<f32, 9>(
    0.0, 0.0, 0.0,
    c14, c14, 0.0,
    c14, c14, c14
);


const kernles = array(box_kernel_TL, box_kernel_TR, box_kernel_BL, box_kernel_BR);
const kernles_fx = array(box_kernel_TL_fx, box_kernel_TR_fx, box_kernel_BL_fx, box_kernel_BR_fx);

const repell =array(
    normalize(v2f(1.0, -1.0)) * 0.0,
    normalize(v2f(0.0, -1.0)),
    normalize(v2f(-1.0, -1.0))* 0.0,
    normalize(v2f(1.0, 0.0)),
     v2f(0.0, 0.0) ,
    normalize(v2f(-1.0, 0.0)),
    normalize(v2f(1.0, 1.0))* 0.0,
    normalize(v2f(0.0, 1.0)),
    normalize(v2f(-1.0, 1.0))* 0.0
);
const point_to =array(
    normalize(vec2<f32>(-1.0, 1.0)),
    normalize(vec2<f32>(0.0, 1.0)),
    normalize(vec2<f32>(1.0, 1.0)),
    normalize(vec2<f32>(-1.0, 0.0)),
     (vec2<f32>(0.0, 0.0)),
    normalize(vec2<f32>(1.0, 0.0)),
    normalize(vec2<f32>(-1.0, -1.0)),
    normalize(vec2<f32>(0.0, -1.0)),
    normalize(vec2<f32>(1.0, -1.0))
);

@compute @workgroup_size(8, 8)
fn cs_postprocess_main(@builtin(global_invocation_id) gid: vec3u) {
    if disabled { return;}

    let k_wallbias: bool = (args.flags & 1u) > 0;
    let k_smooth: bool = (args.flags & 2u) > 0;

    let tmp = gid.xy  ;
    let read_cell = v2i32(i32(tmp.x), i32(tmp.y));
    let size_1d: u32 = args.size.x * args.size.y * 4;
    let read_size_1d: u32 = args.size.x * args.size.y * 4;
    let subdiv_stride_x = i32(args.size.y * 2);

    let write_cell = read_cell * 2;
    if read_cell.y >= i32(args.size.y) {return;}
    if read_cell.x >= i32(args.size.x) {return;}

    let self_idx = read_cell.x + read_cell.y * i32(args.size.x);
    let self_val: u32 = distances.cells[self_idx];
    let self_wall: bool = (self_val & (1u << 15u)) > 0;
    if self_wall { return;}

    let neigbors = array(
        read_cell + vec2<i32>(-1, -1),
        read_cell + vec2<i32>(0, -1),
        read_cell + vec2<i32>(1, -1),
        read_cell + vec2<i32>(-1, 0),
        read_cell + vec2<i32>(0, 0),
        read_cell + vec2<i32>(1, 0),
        read_cell + vec2<i32>(-1, 1),
        read_cell + vec2<i32>(0, 1),
        read_cell + vec2<i32>(1, 1),
    );

    let self_vec = flow_read.cells[self_idx];
    let pointing_to = read_cell + v2i32(i32(ceil(self_vec.x)), i32(ceil(-self_vec.y)));
    let pointing_oob = pointing_to.y >= i32(args.size.y) || pointing_to.x >= i32(args.size.x) || pointing_to.x < 0 || pointing_to.y < 0;

    var point_to_wall = false;
    if !pointing_oob {
        let neighbor_val: u32 = distances.cells[ pointing_to.x + pointing_to.y * i32(args.size.x)];
        point_to_wall = (neighbor_val & (1u << 15u)) > 0;
    }

    let self_dist: u32 = distances.cells[self_idx];
    // messed up order
    for (var i = 0; i < 4; i++) {
        let off_part = select(i, subdiv_stride_x + i % 2, i >= 2);
        let idx_to_write = write_cell.x + write_cell.y * subdiv_stride_x + off_part;

        var sum = v2f();
        for (var j = 0; j < 9; j++) {
            let cell = neigbors[j];
            let oob = cell.y >= i32(args.size.y) || cell.x >= i32(args.size.x) || cell.x < 0 || cell.y < 0;

            if oob {continue;}

            let read_idx = cell.x + cell.y * i32(args.size.x);
            let cell_val = flow_read.cells[read_idx];
            let neighbor_val: u32 = distances.cells[read_idx];
            let neighbor_wall: bool = (neighbor_val & (1u << 15u)) > 0;

            let fixup = select(v2f(0.0, 0.0), kernles[i][j] * point_to[j] * 23, point_to_wall && neighbor_val < self_dist);
            sum += fixup;
            //if kernles[i][j] > 0 && neighbor_wall {sum += kernles[i][j] * select(cell_val, repell[j] * 14, neighbor_wall); }// repell[j]

            sum += kernles[i][j] * select(cell_val, repell[j] * 0.72 * f32(1 + i32(point_to_wall) * 1), neighbor_wall);
        }
        flow_sub2.cells[idx_to_write] = select(normalize(sum), v2f(), sum.x == 0 && sum.y == 0);
    }
}