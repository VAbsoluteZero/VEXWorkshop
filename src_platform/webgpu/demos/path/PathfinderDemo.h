#pragma once

#include <VFramework/VEXBase.h>
#include <application/Application.h>
#include <application/Platfrom.h>
#include <webgpu/render/WgpuApp.h> // #fixme
 
namespace vex
{
    struct PathfinderDemo : public IDemoImpl
    {
        struct InitArgs
        {
        };
        void init(Application& owner, InitArgs args);

        void update(Application& owner) override;
        void drawUI(Application& owner) override;
        void setupImGuiForDrawPass(vex::Application& owner);
        virtual ~PathfinderDemo() {}

    private:
        struct Viewport
        {
            ViewportOptions args;
            bool visible = true;
            v2i32 last_seen_size{0, 0};
            wgfx::Viewport render_target;
        };
        struct SceneData
        {
            wgfx::BasicCamera camera;
        } scene;

        void prepareFrameData(); 

        bool docking_not_configured = true;
        std::vector<Viewport> imgui_views;
        wgfx::Context wgpu_ctx;
        // #fixme - for the sake of iteration speed while experimenting with API
        // absolutely should not be held here
        WebGpuBackend* wgpu_backend = nullptr;
    };
} // namespace vex