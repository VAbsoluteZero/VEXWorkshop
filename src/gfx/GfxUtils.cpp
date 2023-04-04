#include "GfxUtils.h"

#include <SDL.h>
#include <SDL_filesystem.h>
#include <spdlog/stopwatch.h>

#include <filesystem>
#include <fstream>
#include <glm/gtx/norm.hpp>

using namespace vex;

void vex::TextShaderLib::build(const char* rel_path)
{
    spdlog::stopwatch sw;
    defer_ { SPDLOG_WARN("Building TextShaderLib took {} seconds", sw); };

    path_to_dir = std::string(rel_path);
    vex::InlineBufferAllocator<1024 * 20> stackbuf;
    auto al = stackbuf.makeAllocatorHandle();

    for (const auto& entry : std::filesystem::recursive_directory_iterator(path_to_dir))
    {
        const auto& path = entry.path();
        auto file_name = path.generic_string();

        auto adjusted_name = std::string(VEX_SHADER_CONTENT_ROOT) + file_name;
        std::ifstream m_stream(adjusted_name, std::ios::binary | std::ios::ate);
         
        if (m_stream.fail() || m_stream.bad())
            continue;

        auto f_time = std::filesystem::last_write_time(adjusted_name);
        u64 timestamp = f_time.time_since_epoch().count();

        size_t size = std::filesystem::file_size(adjusted_name);

        char* buffer = (char*)al.alloc(size + 1, 1);
        m_stream.seekg(0);
        m_stream.read(buffer, size);
        if (size == 0)
            continue;

        auto ext = path.extension().string();
        auto type = [](std::string& ext) -> ShaderSourceType
        {
            if (ext == ".wgsl")
                return ShaderSourceType::WGSL;
            else if (ext == ".hlsl")
                return ShaderSourceType::HLSL;
            return ShaderSourceType::Invalid;
        }(ext);

        if (type == ShaderSourceType::Invalid)
            continue;

        buffer[size] = '\0';
        this->shad_src.emplace(std::move(file_name), ShaderSource{buffer, type, timestamp});

        stackbuf.reset();
    }

    SPDLOG_INFO("shaders found : {} :", this->shad_src.size());
    for (auto& [k, entry] : this->shad_src)
    {
        SPDLOG_INFO("-- {}", k);
    }
}

void vex::TextShaderLib::reload()
{
    vex::InlineBufferAllocator<1024 * 20> stackbuf;
    auto al = stackbuf.makeAllocatorHandle();
    for (auto& [file_name, entry] : this->shad_src)
    {
        auto adjusted_name = std::string(VEX_SHADER_CONTENT_ROOT) + file_name;
        std::ifstream m_stream(adjusted_name, std::ios::binary | std::ios::ate);
        size_t size = std::filesystem::file_size(adjusted_name);
        if (size == 0)
            continue;

        auto f_time = std::filesystem::last_write_time(adjusted_name);
        u64 timestamp = f_time.time_since_epoch().count();

        if (timestamp == entry.timestamp)
            continue;

        char* buffer = (char*)al.alloc(size + 1, 1);
        m_stream.seekg(0);
        m_stream.read(buffer, size);
        buffer[size] = '\0';

        entry.text = buffer;
        entry.timestamp = timestamp;
        entry.reloaded_dirty_flag = true;

        stackbuf.reset();
    }
}
void vex::TextShaderLib::reloadIfNewer(const char* file_name)
{
    vex::InlineBufferAllocator<1024 * 20> stackbuf;
    auto al = stackbuf.makeAllocatorHandle();
    if (auto* entry = this->shad_src.find(file_name); entry)
    {
        auto adjusted_name = std::string(VEX_SHADER_CONTENT_ROOT) + file_name;
        std::ifstream m_stream(adjusted_name, std::ios::binary | std::ios::ate);
        size_t size = std::filesystem::file_size(adjusted_name);

        auto f_time = std::filesystem::last_write_time(adjusted_name);
        u64 timestamp = f_time.time_since_epoch().count();

        if (timestamp == entry->timestamp || size == 0)
            return;

        char* buffer = (char*)al.alloc(size + 1, 1);
        m_stream.seekg(0);
        m_stream.read(buffer, size); 
        buffer[size] = '\0';

        entry->text = buffer;
        entry->timestamp= timestamp;
        entry->reloaded_dirty_flag = true;

        stackbuf.reset();
    }
}

void vex::BasicCamera::update()
{
    mtx4 rot_mat = mtx4_identity;
    mtx4 trans_mat = mtx4_identity;
    trans_mat = glm::translate(trans_mat, pos);
    this->mtx.view = (trans_mat * rot_mat);
    if (type == Type::FOV)
    {
        mtx.projection = glm::perspective(fov_rad, aspect, near_depth, far_depth);
    }
    else
    {
        auto sz = orthoSize() * 0.5f;
        i32 sign = invert_y ? -1 : 1;
        mtx.projection = glm::orthoLH_ZO(
            -sz.x, sz.x, sign * -sz.y, sign * sz.y, near_depth, far_depth);
    }
}

void vex::BasicCamera::setPerspective(float fov_degrees, float aspect, float in_near, float in_far)
{
    type = Type::FOV;
    this->fov_rad = glm::radians(fov_degrees);
    this->near_depth = in_near;
    this->far_depth = in_far;
    this->mtx.projection = glm::perspective(fov_rad, aspect, near_depth, far_depth);
    update();
}

void vex::BasicCamera::orthoSetSize(v2f size)
{
    type = Type::Ortho;
    this->aspect = size.x / size.y;
    height = size.y;
    update();
}

DynMeshBuilder<PosNormUv> vex::makeQuadUV(vex::Allocator al, float w, float h, v3f origin)
{
    DynMeshBuilder<PosNormUv> mesh{al, 6, 4};
    v3f tl{-w / 2, +h / 2, 0.1f};
    v3f tr{+w / 2, +h / 2, 0.1f};
    v3f br{+w / 2, -h / 2, 0.1f};
    v3f bl{-w / 2, -h / 2, 0.1f};
    mesh.addVertex({origin + bl, v3f(0, 0, -1), v2f(0, 1)});
    mesh.addVertex({origin + br, v3f(0, 0, -1), v2f(1, 1)});
    mesh.addVertex({origin + tr, v3f(0, 0, -1), v2f(1, 0)});
    mesh.addVertex({origin + tl, v3f(0, 0, -1), v2f(0, 0)});
    u32 zero = 0;
    mesh.makeTriangle(zero, zero + 1, zero + 2);
    mesh.makeTriangle(zero, zero + 2, zero + 3);
    return mesh;
}
DynMeshBuilder<PosNormColor> vex::makeQuadColors(
    vex::Allocator al, float w, float h, QuadColors colors, v3f origin)
{
    DynMeshBuilder<PosNormColor> mesh{al, 6, 4};
    v3f tl{-w / 2, +h / 2, 0.1f};
    v3f tr{+w / 2, +h / 2, 0.1f};
    v3f br{+w / 2, -h / 2, 0.1f};
    v3f bl{-w / 2, -h / 2, 0.1f};
    mesh.addVertex({origin + bl, v3f(0, 0, -1), colors.bl});
    mesh.addVertex({origin + br, v3f(0, 0, -1), colors.br});
    mesh.addVertex({origin + tr, v3f(0, 0, -1), colors.tr});
    mesh.addVertex({origin + tl, v3f(0, 0, -1), colors.tl});
    u32 zero = 0;
    mesh.makeTriangle(zero, zero + 1, zero + 2);
    mesh.makeTriangle(zero, zero + 2, zero + 3);
    return mesh;
}

#define vex_safe_norm_2d(vec)                                                            \
    (((vec.x * vec.x + vec.y * vec.y) > vex::epsilon) ? v3f{glm::normalize(v2f{vec}), 0} \
                                                      : v3f{0, 0, 0})

// #fixme it is very dumb algo for doing this, but I'am doing it at 2AM
// => rewrite based on some existing solution or maybe do it on GPU instead
void vex::PolyLine ::buildPolyXY(DynMeshBuilder<PosNormColor>& out_builder, PolyLine::Args args)
{
    const i32 fulllen = args.len;
    const i32 len = args.len - (args.closed ? 0 : 1);
    if (fulllen <= 2)
    {
        buildSegmentXY(out_builder, args);
        return;
    }
    u32 si = out_builder.vertices.size();
    const v3f* points = args.points;

    const u32 corner_tris = args.corner_type == Corners::CloseTri ? 2 : 0;
    const auto [idx_len, vtx_len] = [&]()
    {
        if (args.corner_type == Corners::CloseTri)
        {
            return vex::Tuple<u32, u32>{
                len * 6 + (len - (args.closed ? 0 : 1)) * corner_tris * 3,
                len * 4 + (len - (args.closed ? 0 : 1)) * corner_tris * 2,
            };
        }
        else
            return vex::Tuple<u32, u32>{len * 6, len * 4};
    }();

    out_builder.vertices.addUninitialized(vtx_len);
    out_builder.indices.addUninitialized(idx_len);
    PosNormColor* vtx_back = out_builder.vertices.end() - vtx_len;
    u32* idx_back = out_builder.indices.end() - idx_len;
    PosNormColor* vtx_back_orig = vtx_back;

    // only merging X & Y for now.
    const float half_w = args.thickness * 0.5f;

    /* naming legend:
     * [v3]--p2--[v2]
     *  |   \     |
     *  | t2 \ t1 |   l1 : p1 to p2
     *  |     \   |
     * [v4]--p1--[v1]
     */

    for (i32 i1 = 0; i1 < len; i1++)
    {
        const i32 i2 = ((i1 + 1) % fulllen);
        const i32 i3 = ((i1 + 2) % fulllen);
        const v3f& p1 = points[i1];
        const v3f& p2 = points[i2];
        const v3f& p3 = points[i3];

        v2f p1p2 = p2 - p1;
        v2f p1p2_norm = vex_safe_norm_2d(p1p2);
        p1p2_norm *= (half_w);
        auto dx = p1p2_norm.x;
        auto dy = p1p2_norm.y;
        auto v1 = p1 + v3f{+dy, -dx, 0.f};
        auto v2 = p2 + v3f{+dy, -dx, 0.f};
        auto v3 = p2 + v3f{-dy, +dx, 0.f};
        auto v4 = p1 + v3f{-dy, +dx, 0.f};

        auto line_norm = v3f(0, 0, -1); // #fixme
        // fill data
        {
            vtx_back[0] = {v1, line_norm, args.color};
            vtx_back[1] = {v2, line_norm, args.color};
            vtx_back[2] = {v3, line_norm, args.color};
            vtx_back[3] = {v4, line_norm, args.color};
            idx_back[0] = si + 0;
            idx_back[1] = si + 1;
            idx_back[2] = si + 2;
            idx_back[3] = si + 0;
            idx_back[4] = si + 2;
            idx_back[5] = si + 3;
        }

        bool cannot_close = !args.closed && ((i1 + 1) == len);
        if (Corners::CloseTri == args.corner_type && !cannot_close)
        { // #fixme redundant work - will be calculated next iter again
            auto p2p3 = p3 - p2;
            auto p2p1 = p1 - p2;
            v3f p2p3_norm = vex_safe_norm_2d(p2p3);
            v3f sum_p1p3 = p2p3_norm + vex_safe_norm_2d(p2p1); // p1 + p3 with p2 as origin
            v3f p1p3_norm = vex_safe_norm_2d(sum_p1p3);
            p1p3_norm = p1p3_norm * -1.0f;


            float cross_p1 = cross2d(p1p3_norm, p1 - p2);
            // float cross_p3 = cross2d(p3, p1p3_norm);
            p1p3_norm *= (half_w);
            v3f mid_point = p1p3_norm + p2;

            p2p3_norm *= (half_w);
            auto p2v1 = p2 + v3f{+p2p3_norm.y, -p2p3_norm.x, 0.f};
            auto p2v4 = p2 + v3f{-p2p3_norm.y, +p2p3_norm.x, 0.f};
            // select aligned points (in direction of norm)

            auto left = cross_p1 <= 0 ? p2v1 : p2v4;
            auto right = cross_p1 <= 0 ? v2 : v3;

            vtx_back[4] = {p2, p1p3_norm, args.color};
            vtx_back[5] = {mid_point, v3f{cross_p1}, args.color};
            vtx_back[6] = {left, sum_p1p3, args.color};
            vtx_back[7] = {right, p2p3_norm, args.color};
            idx_back[6] = si + 4; // p2
            idx_back[7] = si + 5; // m
            idx_back[8] = si + 6;
            idx_back[9] = si + 4; // p2
            idx_back[10] = si + 7;
            idx_back[11] = si + 5; // m

            vtx_back += 8;
            idx_back += 12;
            si += 8;
        }
        else
        {
            vtx_back += 4;
            idx_back += 6;
            si += 4;
        }
    }

    // debug
    check_(vtx_back <= out_builder.vertices.end());
}

void vex::PolyLine::buildSegmentXY(DynMeshBuilder<PosNormColor>& out_builder, PolyLine::Args args)
{
    const i32 len = (i32)args.len;
    if (len < 2)
    {
        return;
    }
    out_builder.ensureHasSpaceFor(4, 6);
    const v3f* points = args.points;
    const v3f& p1 = points[0];
    const v3f& p2 = points[1];

    v2f delta = p2 - p1;
    const auto magn = glm::length2(delta);
    v2f delta_norm = (magn > vex::epsilon) ? v3f{glm::normalize(v2f{p2 - p1}), 0} : v3f{0, 0, 0};
    delta_norm *= (args.thickness * 0.5f);

    auto dx = delta_norm.x;
    auto dy = delta_norm.y;

    auto d1 = v3f{+dy, -dx, 0.f};
    auto d2 = v3f{+dy, -dx, 0.f};
    auto d3 = v3f{-dy, +dx, 0.f};
    auto d4 = v3f{-dy, +dx, 0.f};
    auto norm = v3f(0, 0, -1);
    out_builder.addVertex({p1 + d1, norm, args.color});
    out_builder.addVertex({p2 + d2, norm, args.color});
    out_builder.addVertex({p2 + d3, norm, args.color});
    out_builder.addVertex({p1 + d4, norm, args.color});
    out_builder.makeTriangle(0, 1, 2);
    out_builder.makeTriangle(0, 2, 3);
}
