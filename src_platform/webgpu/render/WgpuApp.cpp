#include "WgpuApp.h"

// wgpu

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
#include <stb/stb_image_write.h>
#include <utils/ImGuiUtils.h>

#include "WgpuTypes.h"

using namespace std::literals::chrono_literals;

void WgpuViewport::initialize(WgpuGlobals& globals, const ViewportInitArgs& args)
{
    initial_args = args;
    WGPUTextureDescriptor tex_desc{
        .label = "viewport texture",
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment |
                 WGPUTextureUsage_CopySrc,
        .dimension = WGPUTextureDimension_2D,
        .size =
            (WGPUExtent3D){
                .width = args.size.x,
                .height = args.size.y,
                .depthOrArrayLayers = 1,
            },
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };
    texture = wgpuDeviceCreateTexture(globals.device, &tex_desc);

    WGPUTextureViewDescriptor view_desc{.label = "viewport texture view",
        .format = tex_desc.format,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All};
    view = wgpuTextureCreateView(texture, &view_desc);
    color_attachment.view = view;
}

struct WgpuRenderInterface
{
    static constexpr i32 k_max_viewports = 8;
    WgpuGlobals globals;
    // WgpuWindow window;
    WgpuFrameResources frame_data;

    std::vector<WgpuViewport> viewports;

    bool initialized = false;
    WgpuRenderInterface() = default;
    WgpuRenderInterface(const WgpuRenderInterface&) = delete;
    ~WgpuRenderInterface() { release(); }

    auto init(vp::Application& owner) -> i32
    {
        SDL_Window* sdl_window = owner.getMainWindow()->getRawWindow();
        viewports.reserve(k_max_viewports + 1);
        int wnd_x = 128;
        int wnd_y = 128;
        SDL_GetWindowSize(sdl_window, &wnd_x, &wnd_y);

        // initialize globals
        {
            WGPUInstanceDescriptor desc{.nextInChain = nullptr};
            globals.instance = wgpu::createInstance(desc);

            if (!wgpu::k_using_emscripten) // emscripten REQUIRES instance to be NULL
                checkLethal(globals.instance, "wgpu initialization failed.");

            globals.surface = wgpu::getWGPUSurface(globals.instance, sdl_window);

            WGPURequestAdapterOptions req_opts = {.nextInChain = nullptr,
                .compatibleSurface = globals.surface,
                .powerPreference = WGPUPowerPreference_Undefined,
                .forceFallbackAdapter = false};
            wgpu::requestAdapter(globals, req_opts);

            WGPUDeviceDescriptor deviceDesc{
                .label = "wgpu device",
                .defaultQueue = {.label = "The default queue"},
            };
            wgpu::requestDevice(globals, &deviceDesc);

            auto onDeviceError = [](WGPUErrorType type, char const* message, void*)
            {
                if (message)
                    SPDLOG_ERROR(" wgpu error: {} ", message);
                checkAlways_(false);
            };
            wgpuDeviceSetUncapturedErrorCallback(globals.device, onDeviceError, nullptr);
            globals.queue = wgpuDeviceGetQueue(globals.device);
        }
        //
        {
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
        {
            const char* shaderSource = R"(
        @vertex
        fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4<f32> {
	        var p = vec2<f32>(0.0, 0.0);
	        if (in_vertex_index == 0u) {
		        p = vec2<f32>(-0.5, -0.5);
	        } else if (in_vertex_index == 1u) {
		        p = vec2<f32>(0.5, -0.5);
	        } else {
		        p = vec2<f32>(0.0, 0.5);
	        }
	        return vec4<f32>(p, 0.0, 1.0);
        }
        @fragment
        fn fs_main() -> @location(0) vec4<f32> {
            return vec4<f32>(0.5, 0.4, 4.0, 1.0);
        }
)";

            WGPUShaderModuleDescriptor shaderDesc{};
            WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};
            shaderCodeDesc.chain.next = nullptr;
            shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
            shaderDesc.nextInChain = &shaderCodeDesc.chain;

#ifdef VEX_GFX_WEBGPU_DAWN
            shaderCodeDesc.source = shaderSource;
#else
            shaderCodeDesc.code = shaderSource;
#endif

            WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(
                globals.device, &shaderDesc);
            WGPURenderPipelineDescriptor pipelineDesc{};

            // Vertex fetch
            // (We don't use any input buffer so far)
            pipelineDesc.vertex.bufferCount = 0;
            pipelineDesc.vertex.buffers = nullptr;

            // Vertex shader
            pipelineDesc.vertex.module = shaderModule;
            pipelineDesc.vertex.entryPoint = "vs_main";
            pipelineDesc.vertex.constantCount = 0;
            pipelineDesc.vertex.constants = nullptr;
            pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
            pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
            pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
            pipelineDesc.primitive.cullMode = WGPUCullMode_None;

            // Fragment shader
            WGPUFragmentState fragmentState{};
            fragmentState.nextInChain = nullptr;
            fragmentState.module = shaderModule;
            fragmentState.entryPoint = "fs_main";
            fragmentState.constantCount = 0;
            fragmentState.constants = nullptr;

            // Configure blend state
            WGPUBlendState blendState{};
            // Usual alpha blending for the color:
            blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
            blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
            blendState.color.operation = WGPUBlendOperation_Add;
            // We leave the target alpha untouched:
            blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
            blendState.alpha.dstFactor = WGPUBlendFactor_One;
            blendState.alpha.operation = WGPUBlendOperation_Add;

            WGPUColorTargetState colorTarget{};
            colorTarget.nextInChain = nullptr;
            colorTarget.format = globals.main_texture_fmt;
            colorTarget.blend = &blendState;
            colorTarget.writeMask = WGPUColorWriteMask_All;

            // We have only one target because our render pass has only one output color
            // attachment.
            fragmentState.targetCount = 1;
            fragmentState.targets = &colorTarget;

            // Depth and stencil tests are not used here
            pipelineDesc.depthStencil = nullptr;
            pipelineDesc.fragment = &fragmentState;
            pipelineDesc.multisample.count = 1;
            pipelineDesc.multisample.mask = ~0u;
            pipelineDesc.multisample.alphaToCoverageEnabled = false;

            // Pipeline layout
            WGPUPipelineLayoutDescriptor layoutDesc{};
            layoutDesc.nextInChain = nullptr;
            layoutDesc.bindGroupLayoutCount = 0;
            layoutDesc.bindGroupLayouts = nullptr;
            globals.debug_layout = wgpuDeviceCreatePipelineLayout(globals.device, &layoutDesc);
            pipelineDesc.layout = globals.debug_layout;

            globals.debug_pipeline = wgpuDeviceCreateRenderPipeline(globals.device, &pipelineDesc);

            colorTarget.format = WGPUTextureFormat_RGBA8Unorm;
            globals.debug_pipeline2 = wgpuDeviceCreateRenderPipeline(globals.device, &pipelineDesc);
        }

        bool valid = checkRel(globals.isValid(), "wgpu initialization failed");
        return valid ? 0 : 1;
    }

    auto resizeWindow(vp::Application& owner, u32 w, u32 h) -> void
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

    auto preFrame(vp::Application& owner) -> void
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
            .clearValue = WGPUColor{0.1, 0.9, 0.2, 1.0},
        };
        WGPURenderPassDescriptor pass{.colorAttachmentCount = 1,
            .colorAttachments = &color_attachment,
            .depthStencilAttachment = nullptr};

        frame_data.render_pass_encoder = wgpuCommandEncoderBeginRenderPass(
            frame_data.encoder, &pass);
        // base
        {
            wgpuRenderPassEncoderSetPipeline(
                frame_data.render_pass_encoder, globals.debug_pipeline);
            wgpuRenderPassEncoderDraw(frame_data.render_pass_encoder, 3, 1, 0, 0);
            wgpuQueueSubmit(globals.queue, 0, nullptr);
        }
        wgpuDeviceTick(globals.device);
    }

    auto frame(vp::Application& owner) -> void
    {
        // viewports
        for (auto& vp : viewports)
        {
            if (!vp.isValid())
                continue;

            vp.frame_data.encoder = wgpuDeviceCreateCommandEncoder(
                globals.device, &vp.encoder_desc);

            auto encoder = vp.frame_data.encoder;
            WGPURenderPassDescriptor pass{
                .colorAttachmentCount = 1,
                .colorAttachments = &vp.color_attachment,
                .depthStencilAttachment = nullptr,
            };
            vp.frame_data.render_pass_encoder = wgpuCommandEncoderBeginRenderPass(
                vp.frame_data.encoder, &pass);

            wgpuRenderPassEncoderSetPipeline(
                vp.frame_data.render_pass_encoder, globals.debug_pipeline2);
            wgpuRenderPassEncoderDraw(vp.frame_data.render_pass_encoder, 3, 1, 0, 0);
            wgpuRenderPassEncoderEnd(vp.frame_data.render_pass_encoder);

            WGPUCommandBufferDescriptor cmd_desc{};
            cmd_desc.label = "Command buffer";
            WGPUCommandBuffer command = wgpuCommandEncoderFinish(vp.frame_data.encoder, &cmd_desc);
            wgpuQueueSubmit(globals.queue, 1, &command);
        }
        wgpuDeviceTick(globals.device);
    }
    auto postFrame(vp::Application& owner) -> void {}
    auto present() -> void
    {
        // window
        {
            wgpuRenderPassEncoderEnd(frame_data.render_pass_encoder);
            // Present
            WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
            cmdBufferDescriptor.nextInChain = nullptr;
            cmdBufferDescriptor.label = "Command buffer";
            WGPUCommandBuffer command = wgpuCommandEncoderFinish(
                frame_data.encoder, &cmdBufferDescriptor);
            wgpuQueueSubmit(globals.queue, 1, &command);

            wgpu::swapchainPresent(globals.swap_chain);

            frame_data.release();
        }
        // viewports
        for (auto& vp : viewports)
        {
            if (!vp.isValid())
                continue;
            frame_data.release();
        }
        wgpuDeviceTick(globals.device);
    }
    auto release() -> void
    {
        initialized = false;
        globals.release();
    }

    auto canAddViewport() const -> bool { return viewports.size() <= k_max_viewports; }
    auto addViewport(const ViewportInitArgs& args) -> u32
    {
        if (viewports.size() >= k_max_viewports)
            return 0;
        WgpuViewport vp;
        vp.initialize(globals, args);
        viewports.emplace_back(std::move(vp));
        return vp.uid;
    }
    auto findViewport(u32 id) -> WgpuViewport*
    {
        for (auto& vp : viewports)
        {
            if (vp.uid == id)
                return &vp;
        }
        return nullptr;
    }
    auto resetViewport(u32 id, const ViewportInitArgs& args) -> bool
    { //
        if (WgpuViewport* vp = findViewport(id); vp && vp->isValid())
        {
            vp->release();
            vp->initialize(globals, args);
            return true;
        }
        return false;
    }
    auto removeViewport(u32 id) -> bool
    {
        if (WgpuViewport* vp = findViewport(id); vp && vp->isValid())
        {
            vp->release();
            return true;
        }
        return false;
    }
};
//----------------------------------------------------------------------
vp::WgpuApp::~WgpuApp()
{
    if (impl)
    {
        impl->release();
        delete impl;
    }
}
i32 vp::WgpuApp::init(vp::Application& owner)
{
    if (!checkAlways(impl == nullptr, "must not call init more than once"))
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

    ViewportInitArgs args;
    u32 id = impl->addViewport(args);
    imgui_views.push_back({true, false, id});

    return err;
}
static bool br_ch = false;
void vp::WgpuApp::preFrame(vp::Application& owner)
{
    checkLethal(impl != nullptr, "unexpected null in vp::WgpuApp::preFrame");
    impl->preFrame(owner);
#if ENABLE_IMGUI
    ImGui_ImplWGPU_NewFrame(); // compiles shaders when called first time 
    ImGui_ImplSDL2_NewFrame(); // window & inputs related frame init specific for SDL
    ImGui::NewFrame(); 
#endif 
}
void vp::WgpuApp::frame(vp::Application& owner)
{
    impl->frame(owner); 
}
void vp::WgpuApp::postFrame(vp::Application& owner)
{
    // clear screen buffer
    ImGuiIO& io2 = ImGui::GetIO(); 
    impl->postFrame(owner);

    // ----------------------------------------------------------------
    // ImGui pass

#if ENABLE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    auto nav_old = io.ConfigWindowsMoveFromTitleBarOnly;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    defer_ { io.ConfigWindowsMoveFromTitleBarOnly = nav_old; };
    // add to menu
    if (ImGui::BeginMainMenuBar())
    {
        defer_ { ImGui::EndMainMenuBar(); };
        // imgui stuff
        if (ImGui::BeginMenu("Viewports"))
        {
            defer_ { ImGui::EndMenu(); };

            if (impl->canAddViewport() && ImGui::Button("Add Viewport"))
            {
                ViewportInitArgs args;
                u32 id = impl->addViewport(args);
                imgui_views.push_back({true, false, id});
            }
            for (ImViewport& vp : imgui_views)
            {
                auto s = fmt::format("Viewport {}", vp.native_vp_id);
                if (ImGui::MenuItem(s.data(), nullptr, vp.visible))
                {
                    vp.visible = !vp.visible;
                }
                if (!vp.visible)
                    continue;
            }
        }

        ImGui::Bullet();
        ImGui::Text(" perf: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
    }

    // draw enabled viewports
    for (ImViewport& vp : imgui_views)
    {
        auto s = fmt::format("Viewport {}", vp.native_vp_id);
        if (!vp.visible)
            continue;

        bool visible = ImGui::Begin(s.data());
        defer_ { ImGui::End(); };

        ImVec2 vmin = ImGui::GetWindowContentRegionMin();
        ImVec2 vmax = ImGui::GetWindowContentRegionMax();
        auto deltas = ImVec2{vmax.x - vmin.x, vmax.y - vmin.y};
        v2i32 cur_size = {deltas.x > 0 ? deltas.x : 32, deltas.y > 0 ? deltas.y : 32};

        WgpuViewport* native_vp = impl->findViewport(vp.native_vp_id);

        if (!visible || native_vp == nullptr || !native_vp->isValid())
            continue;

        if (cur_size != vp.last_seen_size)
        {
            ViewportInitArgs args = native_vp->initial_args;
            args.size = cur_size;
            impl->resetViewport(vp.native_vp_id, args);
        }
        vp.last_seen_size = cur_size;
        ImGui::Image(
            (void*)(uintptr_t)native_vp->view, deltas, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        ImGui::SetCursorPos({20, 40});
        if (ImGui::Checkbox("pause", &vp.paused))
        {
        }
        ImGui::Separator();
    }
     
    // ImGuizmo::BeginFrame();
    ImGui::Render();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
    auto draw_data = ImGui::GetDrawData();  
    ImGui_ImplWGPU_RenderDrawData(draw_data, impl->frame_data.render_pass_encoder);
    wgpuDeviceTick(impl->globals.device);
#endif
    // ----------------------------------------------------------------
    impl->present();
}
void vp::WgpuApp::teardown(vp::Application& owner)
{
    impl->release();
    delete impl;
    impl = nullptr;
}
void vp::WgpuApp::handleWindowResize(vp::Application& owner, v2u32 size)
{ //
    br_ch = true;
    impl->resizeWindow(owner, size.x, size.y);
}
