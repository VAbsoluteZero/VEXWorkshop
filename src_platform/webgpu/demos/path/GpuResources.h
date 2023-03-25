#pragma once

#include <VCore/Containers/Dict.h>
#include <VFramework/VEXBase.h>
#include <gfx/GfxUtils.h>

#include <webgpu/render/LayoutManagement.h>
#include <webgpu/render/WgpuTypes.h>
#include <application/Platfrom.h>

namespace vex::flow
{ 
    //struct SettingsFlags // predefined flags
    //{
    //    static constexpr u32 k_pf_demo_setting = 0x0001'0000u;
    //};
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
        .default_val = Color::gray(), 
        .flags = SettingsContainer::Flags::k_visible_in_ui,
    };
    struct DrawContext
    {
        SettingsContainer* settings = nullptr;
        mtx4 camera_mvp = mtx4_identity;
        mtx4 camera_model_view = mtx4_identity;
        mtx4 camera_projection = mtx4_identity;
        v2u32 viewport_size;
        v2f pixel_size_nm = {0.002f,0.002f};
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

        void init(const wgfx::RenderContext& ctx, const TextShaderLib& text_shad_lib);

        void draw(const wgfx::RenderContext& ctx, const DrawContext& draw_ctx);

        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::RenderContext& context);

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

        void init(const wgfx::RenderContext& ctx, const TextShaderLib& text_shad_lib);

        void draw(const wgfx::RenderContext& ctx, const DrawContext& draw_ctx, ROSpan<u32> indices,
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

        void init(const wgfx::RenderContext& ctx, const TextShaderLib& text_shad_lib);
        void draw(const wgfx::RenderContext& ctx, const DrawContext& draw_ct);
        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::RenderContext& context);

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
    struct CellHeatmapV1
    {
        static constexpr const char* hm_shader_file = "content/shaders/wgsl/cell_heatmap.wgsl";

        wgfx::GpuBuffer uniform_buf;
        wgfx::GpuBuffer storage_buf;
        WGPUBindGroup bind_group;

        wgfx::SimplePipeline<wgfx::EmptyVertex> pipeline_data; // needed for reload
        WGPUBindGroupLayout bgl_layout;
        WGPURenderPipeline pipeline;

        void init(
            const wgfx::RenderContext& ctx, const TextShaderLib& text_shad_lib, ROSpan<u32> heatmap);
        void draw(const wgfx::RenderContext& ctx, const DrawContext& draw_ctx,
            const HeatmapDynamicData& args);
        bool reloadShaders(vex::TextShaderLib& shader_lib, const wgfx::RenderContext& context);

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
} // namespace vex::flow