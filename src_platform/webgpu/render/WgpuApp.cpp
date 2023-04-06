#include "WgpuApp.h"

// wgfx

// #include <wgpu/webgpu.hpp>
//  sdl / imgui / third party
#include <SDL_filesystem.h>
#include <SDL_syswm.h>
#include <SDL_video.h>
#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_wgpu.h>
#include <imgui.h>
#include <imgui/gizmo/ImGuizmo.h>
// vex
#include <application/Application.h>
#include <application/Environment.h>
#include <stb/stb_image_write.h>
#include <utils/ImGuiUtils.h>
#include <spdlog/stopwatch.h>

using namespace std::literals::chrono_literals; 


struct WgpuRenderInterface
{
    wgfx::Globals globals;
    wgfx::GpuContext frame_data;
    WGPUSupportedLimits limits;

    bool initialized = false;
    WgpuRenderInterface() = default;
    WgpuRenderInterface(const WgpuRenderInterface&) = delete;
    ~WgpuRenderInterface() { release(); }

    auto init(vex::Application& owner) -> i32
    {
        SDL_Window* sdl_window = owner.getMainWindow()->getRawWindow();
        int wnd_x = 128;
        int wnd_y = 128;
        SDL_GetWindowSize(sdl_window, &wnd_x, &wnd_y);
        // initialize adapte, device, chain & surface
        {
            WGPUInstanceDescriptor desc{.nextInChain = nullptr};
            globals.instance = wgfx::createInstance(desc);

            if (!wgfx::k_using_emscripten) // emscripten REQUIRES instance to be NULL
                checkLethal(globals.instance, "wgpu initialization failed.");

            globals.surface = wgfx::getWGPUSurface(globals.instance, sdl_window);

            WGPURequestAdapterOptions req_opts = {.nextInChain = nullptr,
                .compatibleSurface = globals.surface,
                .powerPreference = WGPUPowerPreference_Undefined,
                .forceFallbackAdapter = false};
            wgfx::requestAdapter(globals, req_opts);

            wgpuAdapterGetLimits(globals.adapter, &limits);

            // WGPURequiredLimits required{.limits = limits.limits};
            WGPUDeviceDescriptor deviceDesc{
                .label = "wgpu device",
                //.requiredLimits = &required,
                .defaultQueue = {.label = "The default queue"},
            };
            wgfx::requestDevice(globals, &deviceDesc);

            auto onDeviceError = [](WGPUErrorType type, char const* message, void*)
            {
                SPDLOG_ERROR("wgpu device encountered error:[c{}]:{}", (u32)type, message);
                
                check_(false);  
            };  
            wgpuDeviceSetUncapturedErrorCallback(globals.device, onDeviceError, nullptr);
            globals.queue = wgpuDeviceGetQueue(globals.device);
              
            globals.main_texture_fmt =
#if defined(VEX_GFX_WEBGPU_DAWN) || defined(__EMSCRIPTEN__)
                WGPUTextureFormat_BGRA8Unorm;
#else
                wgpuSurfaceGetPreferredFormat(globals.surface, globals.adapter);
#endif 
              
            WGPUSwapChainDescriptor desc_swap_chain{
                .nextInChain = nullptr,
                .label = "chain",
                .usage = WGPUTextureUsage_RenderAttachment,
                .format = globals.main_texture_fmt,
                .width = (u32)wnd_x, 
                .height = (u32)wnd_y,
                .presentMode = WGPUPresentMode_Immediate,
            };

            globals.swap_chain = wgpuDeviceCreateSwapChain(
                globals.device, globals.surface, &desc_swap_chain);
        } 

        bool valid = checkRel(globals.isValid(), "wgpu initialization failed");
        return valid ? 0 : 1;
    }

    auto resizeWindow(vex::Application& owner, u32 w, u32 h) -> void
    { //
        WGPUSwapChainDescriptor desc_swap_chain{
            .nextInChain = nullptr,
            .label = "chain",
            .usage = WGPUTextureUsage_RenderAttachment,
            .format = globals.main_texture_fmt,
            .width = (u32)w,
            .height = (u32)h,
            .presentMode = WGPUPresentMode_Immediate,
        };

        auto old_chain = globals.swap_chain;
        globals.swap_chain = wgpuDeviceCreateSwapChain(
            globals.device, globals.surface, &desc_swap_chain);
        wgpuQueueSubmit(globals.queue, 0, nullptr);
        SPDLOG_INFO("resize to : ({:04},{:04}) chain {} -> {}", w, h, (void*)old_chain,
            (void*)globals.swap_chain);
    }

    auto startFrame(vex::Application& owner) -> void
    {
        WGPUCommandEncoderDescriptor encoder_desc = {
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        frame_data.encoder = wgpuDeviceCreateCommandEncoder(globals.device, &encoder_desc);
        frame_data.cur_tex_view = wgpuSwapChainGetCurrentTextureView(globals.swap_chain);
        WGPURenderPassColorAttachment color_attachment{
            .view = frame_data.cur_tex_view,
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = WGPUColor{0.21, 0.35, 0.25, 0.0},
        };
        WGPURenderPassDescriptor pass{.colorAttachmentCount = 1,
            .colorAttachments = &color_attachment,
            .depthStencilAttachment = nullptr};

        frame_data.render_pass = wgpuCommandEncoderBeginRenderPass(frame_data.encoder, &pass);
        wgpuDeviceTick(globals.device);
    }

    auto frame(vex::Application& owner) -> void { wgpuDeviceTick(globals.device); }
    auto postFrame(vex::Application& owner) -> void {}
    auto present() -> void
    {
        // window
        {
            wgpuRenderPassEncoderEnd(frame_data.render_pass);
            // Present
            WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
            cmdBufferDescriptor.nextInChain = nullptr;
            cmdBufferDescriptor.label = "Command buffer";
            WGPUCommandBuffer command = wgpuCommandEncoderFinish(
                frame_data.encoder, &cmdBufferDescriptor);
            wgpuQueueSubmit(globals.queue, 1, &command);

            wgfx::swapchainPresent(globals.swap_chain);

            frame_data.release(); 
            WGPU_REL(RenderPassEncoder, frame_data.render_pass);
            WGPU_REL(CommandBuffer, command);
            WGPU_REL(TextureView, frame_data.cur_tex_view);
        }
        wgpuDeviceTick(globals.device);
    }
    auto release() -> void
    {
        initialized = false;
        globals.release();
    }
};
//----------------------------------------------------------------------
vex::WebGpuBackend::~WebGpuBackend()
{
    if (impl)
    {
        impl->release();
        delete impl;
    }
}
wgfx::Globals& vex::WebGpuBackend::getGlobalResources()
{
    checkLethal(impl != nullptr, "unexpected null in vex::WebGpuBackend::startFrame");
    return (impl->globals);
}
void vex::WebGpuBackend::pollEvents()
{
    if (impl)
        wgpuDeviceTick(impl->globals.device);
}
i32 vex::WebGpuBackend::init(vex::Application& owner)
{
    spdlog::stopwatch sw;
    defer_{ SPDLOG_WARN("Initializing GPU Backend took {} seconds", sw); };

    already_initialized = true;
    if (!checkAlways(already_initialized, "must not call init more than once"))
        return 1;
    auto tst = SDL_GetBasePath();
    spdlog::info("path base : {}", tst);

    impl = new WgpuRenderInterface();
    auto err = impl->init(owner);
    if (err == 0)
    {
        SDL_Window* sdl_window = owner.getMainWindow()->getRawWindow();
        [[maybe_unused]] auto c = ImGui::CreateContext();
#if ENABLE_IMGUI
        ImGui_ImplWGPU_Init(impl->globals.device, 1, impl->globals.main_texture_fmt);
        ImGui_ImplSDL2_InitForSDLRenderer(sdl_window);
        ImGui_ImplWGPU_CreateDeviceObjects();
#endif
    }
    else
    {
        checkLethal(false, "wgpu demo - initialization failed");
    }

    text_shad_lib.build("content/shaders/wgsl/");

    return err;
}

void vex::WebGpuBackend::startFrame(vex::Application& owner)
{
    checkLethal(impl != nullptr, "unexpected null in vex::WebGpuBackend::startFrame");
    impl->startFrame(owner);
#if ENABLE_IMGUI
    ImGui_ImplWGPU_NewFrame(); // compiles shaders when called first time
    ImGui_ImplSDL2_NewFrame(); // window & inputs related frame init specific for SDL
    ImGui::NewFrame();
#endif
}
void vex::WebGpuBackend::frame(vex::Application& owner) { impl->frame(owner); }
void vex::WebGpuBackend::postFrame(vex::Application& owner)
{
    // clear screen buffer
    impl->postFrame(owner);

    // ----------------------------------------------------------------
    // ImGui pass
#if ENABLE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    auto nav_old = io.ConfigWindowsMoveFromTitleBarOnly;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    defer_ { io.ConfigWindowsMoveFromTitleBarOnly = nav_old; };

    // ImGuizmo::BeginFrame();
    ImGui::Render();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
    auto draw_data = ImGui::GetDrawData();
    ImGui_ImplWGPU_RenderDrawData(draw_data, impl->frame_data.render_pass);
    wgpuDeviceTick(impl->globals.device);
#endif
    // ----------------------------------------------------------------
    impl->present();
}
void vex::WebGpuBackend::teardown(vex::Application& owner)
{
    impl->release();
    delete impl;
    impl = nullptr;
}
void vex::WebGpuBackend::softReset(vex::Application& owner)
{
    ImGui_ImplSDL2_InitForSDLRenderer(owner.getMainWindow()->getRawWindow());
    ImGui_ImplWGPU_Init(impl->globals.device, 1, impl->globals.main_texture_fmt);
    ImGui_ImplWGPU_CreateDeviceObjects();
}
void vex::WebGpuBackend::handleWindowResize(vex::Application& owner, v2u32 size)
{ //
    impl->resizeWindow(owner, size.x, size.y);
}
