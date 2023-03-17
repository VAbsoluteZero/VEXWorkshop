#pragma once

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
        struct GpuData
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
        struct SceneData
        {
            wgfx::BasicCamera camera;
        } scene;
         
        WebGpuBackend* wgpu_backend = nullptr;
    };
} // namespace vex