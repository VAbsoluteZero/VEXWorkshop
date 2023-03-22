#pragma once

#include <VFramework/VEXBase.h>
#include <utils/CLI.h>
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
        v2i32 last_seen_size{0, 0};
        v2i32 viewport_origin{0, 0};
        v2f mouse_pos_normalized{0, 0}; // in viewport space (-1:+1)
        wgfx::Viewport render_target;

        bool gui_enabled = true;
        bool changed_last_frame = false;
        bool gui_visible = false;
        bool mouse_over = false;

        const bool isEnableAndVisisble() const 
        {
            return gui_enabled && gui_visible;
        }

        inline v2f pixelScaleToNormalizedView(v2i32 delta) const
        {
            const v2f view_size = v2f{last_seen_size};
            auto out_half = v3f{(delta.x / view_size.x), (delta.y / view_size.y), 0.0f};
            return out_half * 2.0f;
        }
        inline v2f viewportToNormalizedView(v2i32 mouse_pos_window) const
        {
            const v2i32 pos = mouse_pos_window;
            const v2i32 origin = viewport_origin;
            const v2i32 pos_wnd = pos - origin;
            const v2f view_size = v2f{last_seen_size};
            auto out_half = v3f{
                (pos_wnd.x / view_size.x) - 0.5f, (pos_wnd.y / view_size.y) - 0.5f, 0.0f};
            return out_half * 2.0f;
        }
    };
    struct ViewportHandler
    {
        std::vector<UiViewport> imgui_views;

        void add(const wgfx::RenderContext& wgpu_ctx, vex::ViewportOptions options);
        void draw();
        void update(v2i32 wnd_mouse_pos);
        void postFrame();

        void release()
        {
            for (auto& it : imgui_views)
            {
                if (it.render_target.isValid())
                    it.render_target.release();
            }
            imgui_views.clear();
        }
    };

    struct BasicDemoUI
    {
        bool docking_not_configured = true;
        bool should_config_docking = true;
        vex::console::ConsoleWindow console_wnd;

        void drawStandardUI(vex::Application& app, ViewportHandler* viewports = nullptr); 
    };
} // namespace wgfx::ui