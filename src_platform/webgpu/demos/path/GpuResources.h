#pragma once

#include <VCore/Containers/Dict.h>
#include <VFramework/VEXBase.h>
#include <application/Platfrom.h>
#include <gfx/GfxUtils.h>
#include <webgpu/render/LayoutManagement.h>
#include <webgpu/render/WgpuTypes.h>

namespace vex::flow
{
    static inline const auto opt_grid_thickness = SettingsContainer::EntryDesc<i32>{
        .key_name = "pf.GridSize",
        .info = "Thickness of the grid. Value less than 2 disables the grid.",
        .default_val = 2,
        .min = 0,
        .max = 8,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };

    static inline const auto opt_grid_color = SettingsContainer::EntryDesc<v4f>{
        .key_name = "pf.GridColor",
        .info = "Color of the grid.",
        .default_val = v4f(Color::gray()) * 0.7f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_color = SettingsContainer::EntryDesc<v4f>{
        .key_name = "pf.ParticleColor",
        .info = "Color of a particle.",
        .default_val = v4f(Color::green()),
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_auto_color = SettingsContainer::EntryDesc<bool>{
        .key_name = "pf.AutoParticleColor",
        .info = "Color of a particle determined by its generation.",
        .default_val = false,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    /*
        add to particle  : force, reserved
    */
    static inline const auto opt_part_speed = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_BaseSpeed",
        .info = "Base speed of a particle in units per second.",
        .default_val = 2.0f,
        .min = 0.00f,
        .max = 8.00f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_speed_max = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_SpeedClamp",
        .info = "Max normalized speed value.",
        .default_val = 2.0f,
        .min = 0.00f,
        .max = 12.00f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_inertia = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_Inertia",
        .info = "Constant that determins how quickly velocity may change.",
        .default_val = 1.00f,
        .min = 0.00f,
        .max = 2.00f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_radius = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_PhysicalRadius",
        .info = "Radius of a particle relative to cell size.",
        .default_val = 0.25f,
        .min = 0.125f,
        .max = 0.5f,
        .flags = SettingsContainer::Flags::k_visible_in_ui |
                 SettingsContainer::Flags::k_ui_min_as_step,
    };
    static inline const auto opt_part_drag = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_Drag",
        .info = "How fast particle should deccelarate.",
        .default_val = 0.05f,
        .min = 0.00f,
        .max = 0.75f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    static inline const auto opt_part_sep = SettingsContainer::EntryDesc<float>{
        .key_name = "pf.PtSym_Separation",
        .info = "Force that pushes particles away when they get too close.",
        .default_val = 0.05f,
        .min = 0.00f,
        .max = 0.75f,
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };

    struct DrawContext
    {
        SettingsContainer* settings = nullptr;
        mtx4 camera_mvp = mtx4_identity;
        mtx4 camera_model_view = mtx4_identity;
        mtx4 camera_projection = mtx4_identity;
        v2u32 viewport_size;
        v2f pixel_size_nm = {0.002f, 0.002f};
        float delta_time = 0.0f;
        float time = 0.0f;
        float grid_half_size = 16;
        float camera_h = 4.0f;
    };

    struct UBOMvp4Colors4Floats
    {
        mtx4 model_view_proj = mtx4_identity;
        v4f data1 = {};
        v4f data2 = {};
    };
    struct ViewportGrid
    {
        static constexpr const char* grid_shader_file = "content/shaders/wgsl/grid.wgsl";
        wgfx::GpuBuffer vtx_buf;
        wgfx::GpuBuffer idx_buf;
        wgfx::GpuBuffer uniform_buf;
        WGPUBindGroup bind_group;

        wgfx::SimplePipeline<PosNormUv> pipeline_data; // needed for reload
        WGPUBindGroupLayout bgl_layout;
        WGPURenderPipeline pipeline;

        void init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib);

        void draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ctx);

        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context);

        void release()
        {
            WGPU_REL(BindGroup, bind_group);
            WGPU_REL(BindGroupLayout, bgl_layout);
            WGPU_REL(RenderPipeline, pipeline);
            vtx_buf.release();
            idx_buf.release();
            uniform_buf.release();
        }
        bool isValid() const
        {
            return uniform_buf.isValid() && vtx_buf.isValid() && //
                   idx_buf.isValid() && bind_group && pipeline;
        }
    };

    struct TempGeometry
    {
        static constexpr u32 max_vtx_buff_size = 4096 * 4 * sizeof(PosNormColor);
        static constexpr u32 max_idx_buff_size = 4096 * 6 * sizeof(u32);

        wgfx::GpuBuffer idx_buf;
        wgfx::GpuBuffer vtx_buf;
        wgfx::GpuBuffer uniform_buf;
        WGPUBindGroup bind_group;
        WGPURenderPipeline pipeline;

        void init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib);

        void draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ctx, ROSpan<u32> indices,
            ROSpan<PosNormColor> vertices);

        void release()
        {
            WGPU_REL(BindGroup, bind_group);
            WGPU_REL(RenderPipeline, pipeline);
            idx_buf.release();
            vtx_buf.release();
            uniform_buf.release();
        }
        bool isValid() const
        {
            return uniform_buf.isValid() && //
                   bind_group && pipeline;
        }
    };

    struct UBOHeatMap
    {
        mtx4 camera_vp = mtx4_identity;
        v4f data1 = {};
        v4f data2 = {};
        v4f data3 = {};
        v4f data4 = {};
        v2f quad_size = {1, 1};
        v2u32 buffer_dimensions;
    };
    struct HeatmapDynamicData
    {
        ROSpan<u32> buffer;
        v2u32 bounds;
        v4f color1;
        v4f color2;
    };
    struct ColorQuad
    {
        static constexpr const char* bg_shader_file = "content/shaders/wgsl/background.wgsl";

        wgfx::GpuBuffer uniform_buf;
        WGPUBindGroup bind_group;

        wgfx::SimplePipeline<wgfx::EmptyVertex> pipeline_data; // needed for reload
        WGPUBindGroupLayout bgl_layout;
        WGPURenderPipeline pipeline;

        void init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib);
        void draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ct);
        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context);

        void release()
        {
            WGPU_REL(BindGroup, bind_group);
            WGPU_REL(BindGroupLayout, bgl_layout);
            WGPU_REL(RenderPipeline, pipeline);
        }
        bool isValid() const
        {
            return uniform_buf.isValid() && bind_group && pipeline;
            // && vtx_buf.isValid() && idx_buf.isValid() ;
        }
    };

    struct SharedData
    {
    };

    struct CellHeatmapV1
    {
        const char* hm_shader_file = "content/shaders/wgsl/cell_heatmap.wgsl";

        wgfx::GpuBuffer uniform_buf;
        wgfx::GpuBuffer storage_buf;
        WGPUBindGroup bind_group;

        wgfx::SimplePipeline<wgfx::EmptyVertex> pipeline_data; // needed for reload
        WGPUBindGroupLayout bgl_layout;
        WGPURenderPipeline pipeline;

        void init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib,
            ROSpan<u32> heatmap, const char* in_shader_file);

        // void copyBufferToGPU();

        void draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ctx,
            const HeatmapDynamicData& args);
        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context);

        void release()
        {
            WGPU_REL(BindGroup, bind_group);
            WGPU_REL(BindGroupLayout, bgl_layout);
            WGPU_REL(RenderPipeline, pipeline);
            // vtx_buf.release();
            storage_buf.release();
            uniform_buf.release();
        }
        bool isValid() const
        {
            return uniform_buf.isValid() && storage_buf.isValid() && bind_group && pipeline;
            // && vtx_buf.isValid() && idx_buf.isValid() ;
        }
    };
    struct ComputeArgs
    {
        v2u32 map_size{0, 0};
        u32 flags = 0;
        u32 flags2 = 0;
    };
    struct ComputeFields
    {
        struct UBO
        {
            v2u32 size;
            u32 flags = 0;
            u32 dummy = 0;
        };
        const char* cf_shader_file = "content/shaders/wgsl/flow/flowfield_conv.wgsl";

        wgfx::GpuBuffer uniform_buf;
        wgfx::GpuBuffer output_buf;
        wgfx::GpuBuffer staging_buf;
        WGPUBindGroup bind_group;

        wgfx::ComputePipeline pipeline_data;
        WGPUBindGroupLayout bgl_layout;
        WGPUComputePipeline pipeline;

        void init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib,
            const char* in_shader_file, wgfx::GpuBuffer& map_data_buf, v2u32 size);

        void compute(wgfx::CompContext& ctx, const ComputeArgs& args);

        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context);

        void release()
        {
            WGPU_REL(BindGroup, bind_group);
            WGPU_REL(BindGroupLayout, bgl_layout);
            WGPU_REL(ComputePipeline, pipeline);
            // vtx_buf.release();
            uniform_buf.release();
            output_buf.release();
            staging_buf.release();
        }
        bool isValid() const
        {
            return uniform_buf.isValid() && output_buf.isValid() && bind_group && pipeline;
        }
    };
    struct OverlayData
    {
        v2u32 bounds;
        v4f color1;
        v4f color2;
    };
    struct FlowFieldsOverlay
    {
        const char* shader_file = "content/shaders/wgsl/flow/flowfield_overlay.wgsl";
        WGPUBindGroup bind_group;
        wgfx::GpuBuffer uniform_buf;
        wgfx::SimplePipeline<wgfx::EmptyVertex> pipeline_data;
        WGPUBindGroupLayout bgl_layout;
        WGPURenderPipeline pipeline;

        void init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib,
            wgfx::GpuBuffer& flow_v2f_buf, const char* in_shader_file);
        void draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ctx, OverlayData data);
        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context);

        void release()
        {
            WGPU_REL(BindGroup, bind_group);
            WGPU_REL(BindGroupLayout, bgl_layout);
            WGPU_REL(RenderPipeline, pipeline);
            uniform_buf.release();
        }
        bool isValid() const { return uniform_buf.isValid() && bind_group && pipeline; }
    };

    // #todo : select solve pipeline based on part cnt
    struct ParticleSym
    {
        static constexpr u32 spatial_table_depth = 4;
        static constexpr bool experimental = true;         // enable spatial experimental stuff
        static constexpr float default_rel_radius = 0.25f; // relative to cell size
                                                           // static constexpr
        struct Particle
        {
            v2f pos;
            v2f vel;
        };
        struct VisualUBO
        {
            mtx4 camera_vp;
            v4f color1;
            v4f color2;
            v4f size;
            u32 flags;
            u32 padding[4];
        };
        struct SimulateUBO
        {
            v2u32 spatial_table_size;
            v2u32 bounds{};
            v2f grid_min{};
            v2f grid_size{};
            v2f cell_size{};
            u32 num_particles = 0;
            u32 table_depth = spatial_table_depth;

            f32 speed_base = 1.25f;
            f32 radius = 0.25;

            f32 separation = 0.2f;
            f32 inertia = 1.2f;

            f32 drag = 0.05f;
            f32 speed_max = 8.0f;

            f32 delta_time = 0.01f;
            u32 flags = 0;
            u32 padding[4];
        };
        struct
        {
            const char* shader = nullptr;
            wgfx::GpuBuffer uniform_buf;
            wgfx::GpuBuffer particle_data_buf;
            wgfx::GpuBuffer spatial_table;
            wgfx::GpuBuffer counters;
            WGPUBindGroup bind_group;
            WGPUBindGroupLayout bgl_layout;

            wgfx::ComputePipeline zero_pipeline_data{
                .descriptor =
                    {
                        .entryPoint = "cs_zero",
                    },
            };
            WGPUComputePipeline zero_pipeline;
            wgfx::ComputePipeline count_pipeline_data{
                .descriptor =
                    {
                        .entryPoint = "cs_count",
                    },
            };
            WGPUComputePipeline count_pipeline;

            u32 num_particles = 0;
        } hash_data;
        struct
        {
            const char* shader = nullptr;
            WGPUBindGroup bind_group;

            wgfx::ComputePipeline move_pipeline_data;
            wgfx::ComputePipeline solve_pipeline_data{
                .descriptor =
                    {
                        .entryPoint = experimental ? "cs_solve" : "cs_solve_few",
                    },
            };
            wgfx::ComputePipeline solve_pass2_data{
                .descriptor =
                    {
                        .entryPoint = "cs_solve_second_pass",
                    },
            };
            WGPUBindGroupLayout bgl_layout;
            WGPUComputePipeline solve_pipeline;
            WGPUComputePipeline solve_pass2_pipeline;
            WGPUComputePipeline move_pipeline;

            u32 num_particles = 0;
        } sym_data;

        struct
        {
            const char* shader = nullptr;
            wgfx::TextureView tex_view;

            WGPUBindGroup bind_group;
            wgfx::GpuBuffer uniform_buf;
            wgfx::SimplePipeline<wgfx::EmptyVertex> pipeline_data;
            WGPUBindGroupLayout bgl_layout;
            WGPURenderPipeline pipeline;
        } vis_data;

        struct InitArgs
        {
            const char* shader_hash = "content/shaders/wgsl/flow/flowfield_hash.wgsl";
            const char* shader_compute = "content/shaders/wgsl/flow/flowfield_ps_sym.wgsl";
            const char* shader_visual = "content/shaders/wgsl/flow/flowfield_ps_quad_vf.wgsl";
            const char* particle_texture = "content/sprites/flow/particle.png";
            wgfx::GpuBuffer* flow_v2f_buf = nullptr;
            wgfx::GpuBuffer* cells_buf = nullptr;
            u32 max_particles = 200'000;
            v2u32 bounds{};
        };

        void spawnForSymulation(const wgfx::GpuContext& ctx, ROSpan<Particle> parts);

        void init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib, InitArgs args);

        struct CompArgs
        {
            SettingsContainer* settings = nullptr;
            v2u32 bounds{};
            v2f grid_min{};
            v2f grid_size{};
            v2f cell_size{};
            float time = 0;
            float dt = 0;
        };
        void compute(wgfx::CompContext& ctx, CompArgs args);
        struct DrawArgs
        {
            SettingsContainer* settings = nullptr;
            v2u32 bounds;
        };
        void draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ctx, DrawArgs args);

        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context);

        void release();
        bool isValid() const;
    };
} // namespace vex::flow