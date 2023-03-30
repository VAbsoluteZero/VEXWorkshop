#pragma once
#include <VCore/Containers/Dict.h>
#include <VCore/Utils/VMath.h>
#include <VFramework/Misc/Color.h>
#include <VFramework/VEXBase.h>

#include <span>
#ifdef NDEBUG
    #define VEX_SHADER_CONTENT_ROOT "../../../"
#else
    #define VEX_SHADER_CONTENT_ROOT "../../../"
#endif

namespace vex
{
    static constexpr float default_cam_height = 10;
    // #todo move it where it makeDefault sense
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
        void reload(const char* name);
    };

    static constexpr v2i32 vp_default_sz{800, 600};
    struct ViewportOptions
    {
        const char* name = nullptr;
        v2u32 size = vp_default_sz;
        bool depth_enabled = true;
        bool srgb = false;
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

    struct PosNormColor
    {
        v3f pos = v3f_zero;
        v3f norm = v3f_zero;
        v4f color = v4f_zero;

        bool operator==(const PosNormColor&) const = default;
        bool operator!=(const PosNormColor&) const = default;
    }; 

    struct BasicCamera
    {
        enum class Type
        {
            Ortho,
            FOV
        } type;
        v3f pos{};
        // v3f rot{};
        union
        {
            float height = 0;
            float fov_rad;
        };
        float aspect = 1;
        float near_depth = 0;
        float far_depth = 1;
        struct
        {
            mtx4 projection;
            mtx4 view;
        } mtx;
        u8 invert_y : 1 = 1u;

        inline v3f normalizedViewToWorld(v2f view_pos)
        {
            auto i_mvp = glm::inverse(mtx.projection * mtx.view);
            v3f world_loc = i_mvp * v4f{view_pos, 0, 1};
            return world_loc;
        }
        inline v3f viewportToCamera(v2f view_pos)
        {
            auto i_mvp = glm::inverse(mtx.projection);
            v3f world_loc = i_mvp * v4f{view_pos, 0, 1};
            return world_loc;
        }
        inline v2f orthoSize() const { return {height * aspect, height}; }

        void update();
        void setPerspective(float fov_degrees, float aspect, float in_near, float in_far);
        void orthoSetSize(v2f size);
        inline static BasicCamera makeOrtho(v3f in_pos, v2f size, float near = 0, float far = 1)
        {
            BasicCamera out{.pos = in_pos, .near_depth = near, .far_depth = far};
            out.orthoSetSize(size);
            return out;
        }
    };

    struct UBOMvp
    {
        mtx4 model_view_proj = mtx4_identity;
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

        constexpr auto vtxBufSize() -> u32 { return vertices.size() * sizeof(VertType); }
        constexpr auto idxBufSize() -> u32 { return indices.size() * sizeof(IndType); }

        FORCE_INLINE void ensureHasSpaceFor(u32 num_vert, u32 num_ind)
        {
            vertices.reserve(vertices.size() + num_vert);
            indices.reserve(indices.size() + num_ind);
        }

        FORCE_INLINE void addVertex(VertType&& v) { vertices.add(v); }
        FORCE_INLINE void makeTriangle(IndType a, IndType b, IndType c)
        {
            indices.add(a);
            indices.add(b);
            indices.add(c);
        }
        FORCE_INLINE void clear()
        {
            vertices.len = 0;
            indices.len = 0;
        }
    };
    template <typename VertType>
    auto buildPlane(vex::Allocator al, u32 tiles, v2f uv_min, v2f uv_max, float scale = 1.0f)
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
                mesh.addVertex({v3f(x0, y0, 0) * scale, v3f(0, 0, -1), v2f(u0, v0)}); // top left
                mesh.addVertex({v3f(x0, y1, 0) * scale, v3f(0, 0, -1), v2f(u0, v1)});
                mesh.addVertex({v3f(x1, y1, 0) * scale, v3f(0, 0, -1), v2f(u1, v1)});
                mesh.addVertex({v3f(x1, y0, 0) * scale, v3f(0, 0, -1), v2f(u1, v0)});

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

    auto makeQuadUV(vex::Allocator al, float w, float h, v3f origin = {0, 0, 0})
        -> DynMeshBuilder<PosNormUv>;

    struct QuadColors
    {
        union
        {
            Color clr[4];
            v4f tl;
            v4f tr;
            v4f br;
            v4f bl;
        };
        QuadColors(Color one = Color::red()) : clr{one, one, one, one} {}
        QuadColors(const QuadColors&) = default;
        ~QuadColors() = default;
    };

    auto makeQuadColors(vex::Allocator al, float w, float h, QuadColors colors,
        v3f origin = {0, 0, 0}) -> DynMeshBuilder<PosNormColor>;

    struct PolyLine
    {
        enum class Corners : u8
        {
            None = 0,
            CloseTri = 2,
            // Fan
        };
        struct Args
        {
            v3f* points = nullptr;
            i32 len = 0;
            v4f color;
            Corners corner_type = Corners::None;
            float thickness = 1;
            bool closed = true;
        };
        static void buildPolyXY(DynMeshBuilder<PosNormColor>& out_builder, PolyLine::Args args);

        static void buildSegmentXY(DynMeshBuilder<PosNormColor>& out_builder, PolyLine::Args args);
    };

} // namespace vex

namespace std
{
    template <>
    struct hash<vex::PosNormUv>
    {
        size_t operator()(vex::PosNormUv const& v) const { return vex::util::fnv1a_obj(v); }
    };
    template <>
    struct hash<vex::PosNormColor>
    {
        size_t operator()(vex::PosNormColor const& v) const { return vex::util::fnv1a_obj(v); }
    };
} // namespace std
