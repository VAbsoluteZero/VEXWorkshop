#pragma once
#include <VCore/Containers/Ring.h>
#include <VFramework/VEXBase.h>
#include <application/Application.h>
#include <application/Platfrom.h>
#include <webgpu/demos/ViewportHandler.h>
#include <webgpu/render/WgpuApp.h> // #fixme

namespace vex
{
    struct TexturedQuadDemo : public IDemoImpl
    {
        struct InitArgs
        {
        };
        void init(Application& owner, InitArgs args);
        void update(Application& owner) override;
        void postFrame(Application& owner) override;
        void drawUI(Application& owner) override;
        virtual ~TexturedQuadDemo() {}

    private:
        wgfx::ui::ViewportHandler viewports;
        wgfx::ui::BasicDemoUI ui;
        vex::InlineBufferAllocator<2 * 1024 * 1024> frame_alloc;
        vex::Allocator alloc = frame_alloc.makeAllocatorHandle();

        struct QuadGpuData
        {
            wgfx::GpuBuffer idx_buf;
            wgfx::GpuBuffer vtx_buf;
            wgfx::TextureView tex_view;

            bool isValid() const
            {
                return idx_buf.isValid() && vtx_buf.isValid() && tex_view.isValid();
            }
        } bg_quad;
        struct QuadPipelineResources
        {
            wgfx::GpuBuffer uniform_buf;
            WGPUBindGroupLayout bind_group_layout;
            WGPUPipelineLayout pipe_layout;
            WGPUBindGroup bind_group;
            WGPURenderPipeline pipeline;

            bool isValid() const
            {
                return uniform_buf.isValid() && bind_group_layout && pipe_layout && //
                       bind_group && pipeline;
            }
        } gpu_data;

        struct TempGeometry
        {
            static constexpr u32 max_vtx_buff_size = 512 * 4 * sizeof(PosNormColor);
            static constexpr u32 max_idx_buff_size = 512 * 6 * sizeof(u32);

            wgfx::GpuBuffer idx_buf;
            wgfx::GpuBuffer vtx_buf;
            wgfx::GpuBuffer uniform_buf;
            WGPUBindGroup bind_group;
            WGPURenderPipeline pipeline;

            bool isValid() const
            {
                return uniform_buf.isValid() && //
                       bind_group && pipeline;
            }
        } temp_geom;

        struct Point
        {
            v3f pos = {};
            v4f color = Color::yellow();
        };
        struct MouseDraw
        {
            static constexpr i32 max_points = 128;
            StaticRing<Point, max_points> points{};
            v4f color = Color::yellow();
            bool loop = false;
            float width = 0.5f;
        } mouse_points;

        struct SceneData
        {
            wgfx::BasicCamera camera;
        } scene;

        WebGpuBackend* wgpu_backend = nullptr;
    };
} // namespace vex