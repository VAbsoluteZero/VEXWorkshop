#pragma once
#include <VCore/Containers/Dict.h>
#include <VFramework/VEXBase.h>

namespace vex
{
    // #todo move it where it make sense
    enum class ShaderSourceType : u8
    {
        WGSL,
        HLSL,
        Invalid = 0xff,
    };
    struct ShaderSource
    {
        std::string text;
        ShaderSourceType type;
    };
    struct TextShaderLib
    {
        vex::Dict<std::string, ShaderSource> shad_src;
        std::string path_to_dir;
        void build(const char* rel_path);
        void reload();
    };

    static constexpr v2i32 vp_default_sz{800, 600};
    struct ViewportOptions
    {
        const char* name = nullptr;
        v2u32 size = vp_default_sz;
        bool depth_enabled = true;
        // bool wireframe = false;
        // bool paused = false;
    };

    struct PosNormUv
    {
        v3f pos = v3f_zero;
        v3f norm = v3f_zero;
        v2f tex = v2f_zero;

        bool operator==(const PosNormUv&) const = default;
        bool operator!=(const PosNormUv&) const = default;
    };

    template <typename VertType, typename IndType = u32>
    struct DynMeshBuilder
    {
        DynMeshBuilder(vex::Allocator al, u32 reserve_indices = 128, u32 reserve_vertices = 64)
            : vertices(al, reserve_vertices), indices(al, reserve_indices)
        {
        }
        vex::Buffer<VertType> vertices;
        vex::Buffer<IndType> indices;

        static constexpr u32 size_of_vertex_t = sizeof(VertType); 
        static constexpr u32 size_of_index_t = sizeof(IndType);

        constexpr auto vertBufSize() -> u32 { return vertices.size() * sizeof(VertType); }
        constexpr auto indBufSize() -> u32 { return indices.size() * sizeof(IndType); }

        void addVertex(VertType&& v) { vertices.add(v); }
        void makeTriangle(IndType a, IndType b, IndType c)
        {
            indices.add(a);
            indices.add(b);
            indices.add(c);
        }
    };
    template <typename VertType>
    auto buildPlane(vex::Allocator al, u32 tiles, v2f uv_min, v2f uv_max)
    {
        const u32 i_cnt = 6 * tiles * tiles;
        const u32 v_cnt = 4 * tiles * tiles; // with duplication

        DynMeshBuilder<VertType> mesh{al, i_cnt, v_cnt};
        const float tile_width = 2.0f / tiles;
        // we will create quads from -1 to +1 x & y
        // with avg UV in the middle
        v2f uv_step = (uv_max - uv_min) * (1.0f / tiles);
        v2f uv_cur = uv_min;

        for (i32 cur_y = 0; cur_y < tiles; cur_y += 1)
        {
            const float y0 = cur_y * tile_width - 1.0f;
            const float y1 = y0 + tile_width;

            const float v0 = uv_cur.y;
            const float v1 = uv_cur.y + uv_step.y;

            for (i32 cur_x = 0; cur_x < tiles; cur_x += 1)
            {
                const float x0 = cur_x * tile_width - 1.0f;
                const float x1 = x0 + tile_width;

                const float u0 = uv_cur.x;
                const float u1 = uv_cur.x + uv_step.x;
                //  pos | normals : -z, towards viewport | uv
                mesh.addVertex({v3f(x0, y0, 0), v3f(0, 0, -1), v2f(u0, v0)}); // top left
                mesh.addVertex({v3f(x0, y1, 0), v3f(0, 0, -1), v2f(u0, v1)});
                mesh.addVertex({v3f(x1, y1, 0), v3f(0, 0, -1), v2f(u1, v1)});
                mesh.addVertex({v3f(x1, y0, 0), v3f(0, 0, -1), v2f(u1, v0)});

                int vert_index = 4 * (cur_x + cur_y * tiles); // 4 v per tile
                mesh.makeTriangle(vert_index, vert_index + 1, vert_index + 2);
                mesh.makeTriangle(vert_index, vert_index + 2, vert_index + 3);
                uv_cur.x += uv_step.x;
            }
            uv_cur.y += uv_step.y;
            uv_cur.x = uv_min.x;
        }
        return mesh;
    }
    template <typename VertType>
    auto makeQuad(vex::Allocator al, float w, float h)
    {
        DynMeshBuilder<VertType> mesh{al, 6, 4};
        v3f tl{-w / 2, +h / 2, 0.1f};
        v3f tr{+w / 2, +h / 2, 0.1f};
        v3f br{+w / 2, -h / 2, 0.1f};
        v3f bl{-w / 2, -h / 2, 0.1f};
        mesh.addVertex({bl, v3f(0, 0, -1), v2f(0, 1)});
        mesh.addVertex({br, v3f(0, 0, -1), v2f(1, 1)});
        mesh.addVertex({tr, v3f(0, 0, -1), v2f(1, 0)});
        mesh.addVertex({tl, v3f(0, 0, -1), v2f(0, 0)});
        u32 zero = 0;
        mesh.makeTriangle(zero, zero + 1, zero + 2);
        mesh.makeTriangle(zero, zero + 2, zero + 3);
        return mesh;
    }
} // namespace vex

namespace std
{
    template <>
    struct hash<vex::PosNormUv>
    {
        size_t operator()(vex::PosNormUv const& v) const { return vex::util::fnv1a_obj(v); }
    };
} // namespace std
