#pragma once

#include <VCore/Containers/Dict.h>
#include <VFramework/VEXBase.h>
#include <application/Platfrom.h>
#include <gfx/GfxUtils.h>
#include <webgpu/demos/ConsoleVars.h>
#include <webgpu/render/LayoutManagement.h>
#include <webgpu/render/WgpuTypes.h>

namespace vex::flow
{
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
        float cell_sz = 4.0f;
    };

    struct UBOGrid
    {
        mtx4 model_view_proj = mtx4_identity;
        v4f data1 = {};
        v4f data2 = {};
        v2f grid_sz{1.0f, 1.0f};
        v2f unused_padding{ 1.0f, 1.0f };
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
        v4f shrink_x_to_y;
        float opacity = 1.0f;
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
        wgfx::GpuBuffer subdiv_buf;

        struct
        {
            WGPUBindGroup bind_group;
            wgfx::ComputePipeline pipeline_data;
            WGPUBindGroupLayout bgl_layout;
            WGPUComputePipeline pipeline;
        } main_pass;

        struct
        {
            WGPUBindGroup bind_group;
            wgfx::ComputePipeline pipeline_data{
                .descriptor =
                    {
                        .entryPoint = "cs_postprocess_main",
                    },
            };
            WGPUBindGroupLayout bgl_layout;
            WGPUComputePipeline pipeline;
        } subdiv_pass;

        void init(const wgfx::GpuContext& ctx, const TextShaderLib& text_shad_lib,
            const char* in_shader_file, wgfx::GpuBuffer& map_data_buf, v2u32 size);

        void compute(wgfx::CompContext& ctx, const ComputeArgs& args);

        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context);

        void release()
        {
            WGPU_REL(BindGroup, main_pass.bind_group);
            WGPU_REL(BindGroupLayout, main_pass.bgl_layout);
            WGPU_REL(ComputePipeline, main_pass.pipeline);
            WGPU_REL(BindGroup, subdiv_pass.bind_group);
            WGPU_REL(BindGroupLayout, subdiv_pass.bgl_layout);
            WGPU_REL(ComputePipeline, subdiv_pass.pipeline);
            // vtx_buf.release();
            uniform_buf.release();
            output_buf.release();
            subdiv_buf.release();
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
            wgfx::GpuBuffer& flow_v2f_buf, wgfx::GpuBuffer& subdiv_buf, const char* in_shader_file);
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
            u32 num_particles = 1000;
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
            wgfx::GpuBuffer* flow_sub_v2f_buf = nullptr;
            wgfx::GpuBuffer* cells_buf = nullptr;
            u32 max_particles = 200'000;
            v2u32 bounds{};
        };

        void resetParticleBuffer(const wgfx::GpuContext& ctx, ROSpan<Particle> parts);
        void spawnForSimulation(const wgfx::GpuContext& ctx, ROSpan<Particle> parts);

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
            v4f color2 = Color::red();
            v2f cell_size{};
            u32 num_particles = 1000;
        };
        void draw(const wgfx::GpuContext& ctx, const DrawContext& draw_ctx, DrawArgs args);

        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::GpuContext& context);

        void release();
    };
} // namespace vex::flow