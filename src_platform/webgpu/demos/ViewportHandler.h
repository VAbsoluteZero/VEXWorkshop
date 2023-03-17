#pragma once

#include <VFramework/VEXBase.h>
#include <webgpu/render/WgpuTypes.h>

namespace vex
{
    class Application;
}
namespace wgfx::ui
{
    struct UiViewport
    {
        vex::ViewportOptions args;
        bool visible = true;
        v2i32 last_seen_size{0, 0};
        wgfx::Viewport render_target;
        bool changed_last_frame = false;
    };
    struct ViewportHandler
    {
        std::vector<UiViewport> imgui_views;

        void add(const wgfx::Context& wgpu_ctx, vex::ViewportOptions options);
        void draw();

        // void releaseFrameResources();
    };

    struct BasicDemoUI
    {
        bool docking_not_configured = true;

        void drawStandardUI(vex::Application& app, ViewportHandler* viewports = nullptr);
    };
} // namespace wgfx::ui