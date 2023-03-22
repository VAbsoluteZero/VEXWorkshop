#pragma once

#include <VFramework/VEXBase.h>
#include <application/Platfrom.h>
#include <gfx/GfxUtils.h>
#include <webgpu/render/WgpuTypes.h>

/*===================================
#FIXME - once familiar with WEBGPU api
rewrite to not expose it and work through
abstraction layer
====================================*/

struct WgpuRenderInterface;

namespace vex
{
    class Application;

    struct WebGpuBackend : public AGraphicsBackend
    {
        WebGpuBackend() { id = GfxBackendID::Webgpu; }
        ~WebGpuBackend();

        wgfx::Globals& getGlobalResources();
        void pollEvents();

        i32 init(vex::Application& owner) override;

        void startFrame(vex::Application& owner) override;
        void frame(vex::Application& owner) override;
        void postFrame(vex::Application& owner) override;

        void softReset(vex::Application& owner) override;
        void teardown(vex::Application& owner) override;

        void handleWindowResize(vex::Application& owner, v2u32 size) override;
        ;
        TextShaderLib text_shad_lib;
        // vex::Dict<std::string, WGPUShaderModule> modules;
        //  this exists in order to limit header pollution with Dx/Windows specific stuff
    private:
        WgpuRenderInterface* impl = nullptr;
        bool already_initialized = false;
    };

} // namespace vex