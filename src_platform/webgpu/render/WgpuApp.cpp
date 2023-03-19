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

using namespace std::literals::chrono_literals;

void wgfx::Viewport::initialize(WGPUDevice device, const vex::ViewportOptions& args)
{
    options = args;
    {
        WGPUTextureDescriptor tex_desc{
            .label = "viewport texture",
            .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment |
                     WGPUTextureUsage_CopySrc,
            .dimension = WGPUTextureDimension_2D,
            .size =
                {
                    .width = args.size.x,
                    .height = args.size.y,
                    .depthOrArrayLayers = 1,
                },
            .format = WGPUTextureFormat_RGBA8Unorm,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };
        texture = wgpuDeviceCreateTexture(device, &tex_desc);

        WGPUTextureViewDescriptor view_desc{
            .label = "viewport texture view",
            .format = tex_desc.format,
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        };
        view = wgpuTextureCreateView(texture, &view_desc);
        color_attachment.view = view;
    }
    // depth
    if (options.depth_enabled)
    {
        WGPUTextureFormat format = WGPUTextureFormat_Depth24PlusStencil8;
        u32 sample_count = 1;

        WGPUTextureDescriptor depth_texture_desc = {
            .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc,
            .dimension = WGPUTextureDimension_2D,
            .size =
                {
                    .width = args.size.x,
                    .height = args.size.y,
                    .depthOrArrayLayers = 1,
                },
            .format = format,
            .mipLevelCount = 1,
            .sampleCount = sample_count,
        };
        depth_texture = wgpuDeviceCreateTexture(device, &depth_texture_desc);

        WGPUTextureViewDescriptor view_desc = {
            .format = depth_texture_desc.format,
            .dimension = WGPUTextureViewDimension_2D,
            .mipLevelCount = 1,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
        };
        depth_view = wgpuTextureCreateView(depth_texture, &view_desc);
        depth_attachment.view = depth_view;
    }
}

wgfx::Context& wgfx::Viewport::setupForDrawing(const wgfx::Globals& globals)
{
    auto encoder = wgpuDeviceCreateCommandEncoder(globals.device, &this->encoder_desc);

    this->color_attachment.view = view;
    this->depth_attachment.view = depth_view;
    WGPURenderPassDescriptor pass{
        .label = "viewport render pass",
        .colorAttachmentCount = 1,
        .colorAttachments = &this->color_attachment,
        .depthStencilAttachment = depth_view ? &this->depth_attachment : nullptr,
    };
    auto render_pass_encoder = wgpuCommandEncoderBeginRenderPass(encoder, &pass);
    // #fixme REVERSE DEPTH ORDER
    wgpuRenderPassEncoderSetViewport(
        render_pass_encoder, 0.0f, 0.0f, options.size.x, options.size.y, 0.0f, 1.0f);
    wgpuRenderPassEncoderSetScissorRect(
        render_pass_encoder, 0u, 0u, options.size.x, options.size.y);

    context = wgfx::Context{
        .device = globals.device,
        .encoder = encoder,
        .queue = globals.queue,
        .render_pass = render_pass_encoder,
        .cur_tex_view = view,
    };
    return context;
}

// void wgfx::Viewport::submit(const wgfx::Context& globals)
//{
//     WGPUCommandBufferDescriptor cmd_desc{};
//     cmd_desc.label = "Command buffer";
//     WGPUCommandBuffer command = wgpuCommandEncoderFinish(frame_data.encoder, &cmd_desc);
//     wgpuQueueSubmit(globals.queue, 1, &command);
// }

struct WgpuRenderInterface
{
    // static constexpr i32 k_max_viewports = 8;
    wgfx::Globals globals;
    wgfx::Context frame_data;
    WGPUSupportedLimits limits;

    // std::vector<wgfx::Viewport> viewports;

    bool initialized = false;
    WgpuRenderInterface() = default;
    WgpuRenderInterface(const WgpuRenderInterface&) = delete;
    ~WgpuRenderInterface() { release(); }

    auto init(vex::Application& owner) -> i32
    {
        SDL_Window* sdl_window = owner.getMainWindow()->getRawWindow();
        // viewports.reserve(k_max_viewports + 1);
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

                // checkAlways_(false);
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
        // default pipelines
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
            .clearValue = WGPUColor{0.1, 0.9, 0.2, 1.0},
        };
        WGPURenderPassDescriptor pass{.colorAttachmentCount = 1,
            .colorAttachments = &color_attachment,
            .depthStencilAttachment = nullptr};

        frame_data.render_pass = wgpuCommandEncoderBeginRenderPass(frame_data.encoder, &pass);
        // base
        {
            wgpuRenderPassEncoderSetPipeline(frame_data.render_pass, globals.debug_pipeline);
            wgpuRenderPassEncoderDraw(frame_data.render_pass, 3, 1, 0, 0);
            wgpuQueueSubmit(globals.queue, 0, nullptr);
        }
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
void vex::WebGpuBackend::handleWindowResize(vex::Application& owner, v2u32 size)
{ //
    impl->resizeWindow(owner, size.x, size.y);
}
