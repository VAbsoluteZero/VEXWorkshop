#include "WgpuApp.h"

// wgpu

// #include <wgpu/webgpu.hpp>
//  sdl / imgui / third party
#include <SDL.h>
#include <SDL_filesystem.h>
#include <SDL_syswm.h>
#include <SDL_video.h>
#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_wgpu.h>
#include <imgui.h>
#include <imgui/gizmo/ImGuizmo.h>
// vex
#include <application/Application.h>
#include <utils/ImGuiUtils.h>

#include "GfxTypes.h"

using namespace std::literals::chrono_literals;

void wgpulWait(std::atomic_bool& flag)
{
    while (!flag)
    {
        std::this_thread::yield();
    }
}
void wgpuPollWait(WGPUQueue queue, std::atomic_bool& flag)
{
    while (!flag)
    {
        // #fixme - in dawn there is Tick to poll events, in wgpu this is workaround
        wgpuQueueSubmit(queue, 0, nullptr);
    }
}
// void wgpuPollWait(WGPUInstance isntance, std::atomic_bool& flag)
//{
//     while (!flag)
//     {
//         checkLethal(false, "");
//         // #fixme - in dawn there is Tick to poll events, in wgpu this is workaround
//         // wgpuInstanceProcessEvents(isntance);
//     }
// }
//  add awaitable, needed proper wgpu api for that - processevents does not work currently
//  version #fixme
template <typename Fptr, typename Callback, typename... TArgs>
auto wgpuRequest(Fptr request, Callback callback, TArgs... args)
{
    static auto lambda_decay_to_ptr = [](auto... params)
    {
        auto last = (vex::traits::identityFunc(params), ...);
        static_assert(std::is_same_v<decltype(last), void*>, "expected payload as last asrg");
        auto callback_ptr = reinterpret_cast<Callback*>(last);
        (*callback_ptr)(params...);
    };

    request(args..., lambda_decay_to_ptr, reinterpret_cast<void*>(&callback));
}

void requestDevice(WgpuGlobals& globals, WGPUDeviceDescriptor const* descriptor)
{
    std::atomic_bool request_done = ATOMIC_VAR_INIT(false);
    auto cb = [&](WGPURequestDeviceStatus status, WGPUDevice in_device, char const* message, void*)
    {
        if (!check(status == 0, "expected success result code"))
            SPDLOG_ERROR(" wgpu error: {} ", message);
        globals.device = in_device;
        request_done = true;
    };
    wgpuRequest(&wgpuAdapterRequestDevice, cb, globals.adapter, descriptor);
    // wgpuPollWait(instance, request_done);
    checkLethal(request_done.load(), "should exec syncrounously");
}
void requestAdapter(WgpuGlobals& globals, WGPURequestAdapterOptions& options)
{
    std::atomic_bool request_done = ATOMIC_VAR_INIT(false);
    auto cb =
        [&](WGPURequestAdapterStatus status, WGPUAdapter in_adapter, char const* message, void*)
    {
        if (!check(status == 0, "expected success result code"))
            SPDLOG_ERROR(" wgpu error: {} ", message);
        globals.adapter = in_adapter;
        request_done = true;
    };
    wgpuRequest(&wgpuInstanceRequestAdapter, cb, globals.instance, &options);
    // wgpuPollWait(instance, request_done);
    checkLethal(request_done.load(), "should exec syncrounously");
}

WGPUSurface getWGPUSurface(WGPUInstance instance, SDL_Window* sdl_window)
{
#ifdef _WIN64
    {
        SDL_SysWMinfo sdl_wm_info;
        SDL_VERSION(&sdl_wm_info.version);
        SDL_GetWindowWMInfo(sdl_window, &sdl_wm_info);
        HWND hwnd = (HWND)sdl_wm_info.info.win.window;
        HINSTANCE hinstance = sdl_wm_info.info.win.hinstance;
        WGPUSurfaceDescriptorFromWindowsHWND hwnd_desc{
            .chain =
                {
                    .next = NULL,
                    .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND,
                },
            .hinstance = hinstance,
            .hwnd = hwnd,
        };
        WGPUSurfaceDescriptor d{
            .nextInChain = (const WGPUChainedStruct*)&hwnd_desc, .label = "surface"};
        return wgpuInstanceCreateSurface(instance, &d);
    }
#else
#error "Unsupported WGPU_TARGET"
#endif
}
void WgpuViewport::initialize(const ViewportInitArgs& args) { initial_args = args; }
#include <dawn/webgpu_cpp.h>
#include <dawn/include/dawn/native/DawnNative.h>
#include <dawn/include/dawn/dawn_proc.h>
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

        auto procs = dawn::native::GetProcs();

        dawnProcSetProcs(&procs);
        // initialize globals Idevice, adapter, etc)
        {
            WGPUInstanceDescriptor desc{.nextInChain = nullptr};
            globals.instance = wgpuCreateInstance(&desc);
            checkLethal(globals.instance, "wgpu initialization failed.");

            globals.surface = getWGPUSurface(globals.instance, sdl_window);

            WGPURequestAdapterOptions req_opts = {.nextInChain = nullptr,
                .compatibleSurface = globals.surface,
                .powerPreference = WGPUPowerPreference_Undefined,
                .forceFallbackAdapter = false};

            requestAdapter(globals, req_opts);

            std::vector<WGPUFeatureName> features;

            // Call the function a first time with a null return address, just to get
            // the entry count.
            size_t featureCount = wgpuAdapterEnumerateFeatures(globals.adapter, nullptr);
            features.resize(featureCount);
            wgpuAdapterEnumerateFeatures(globals.adapter, features.data());

            SPDLOG_INFO("Adapter features:");
            for (auto f : features)
            {
                SPDLOG_INFO(" - {}", f);
            }
            WGPUDeviceDescriptor deviceDesc = {};
            deviceDesc.nextInChain = nullptr;
            deviceDesc.label = "wgpu device";
            deviceDesc.requiredFeaturesCount = 0;
            deviceDesc.requiredLimits = nullptr;
            deviceDesc.defaultQueue.nextInChain = nullptr;
            deviceDesc.defaultQueue.label = "The default queue";

            requestDevice(globals, &deviceDesc);
            auto onDeviceError = [](WGPUErrorType type, char const* message, void*)
            {
                if (message)
                    SPDLOG_ERROR(" wgpu error: {} ", message);
                check_(false);
            };
            wgpuDeviceSetUncapturedErrorCallback(globals.device, onDeviceError, nullptr);
            globals.queue = wgpuDeviceGetQueue(globals.device);
        }
        //
        {
            globals.main_texture_fmt =
#ifdef VEX_WGPU_DAWN
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
                .presentMode = WGPUPresentMode_Fifo,
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


            // Use the extension mechanism to load a WGSL shader source code
            WGPUShaderModuleWGSLDescriptor shaderCodeDesc{};
            // Set the chained struct's header
            shaderCodeDesc.chain.next = nullptr;
            shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
            shaderDesc.nextInChain = &shaderCodeDesc.chain;

#ifdef VEX_WGPU_DAWN
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
            pipelineDesc.fragment = &fragmentState;
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

            // Multi-sampling
            // Samples per pixel
            pipelineDesc.multisample.count = 1;
            // Default value for the mask, meaning "all bits on"
            pipelineDesc.multisample.mask = ~0u;
            // Default value as well (irrelevant for count = 1 anyways)
            pipelineDesc.multisample.alphaToCoverageEnabled = false;

            // Pipeline layout
            // (Our example does not use any resource)
            WGPUPipelineLayoutDescriptor layoutDesc{};
            layoutDesc.nextInChain = nullptr;
            layoutDesc.bindGroupLayoutCount = 0;
            layoutDesc.bindGroupLayouts = nullptr;
            globals.debug_layout = wgpuDeviceCreatePipelineLayout(globals.device, &layoutDesc);
            pipelineDesc.layout = globals.debug_layout;

            globals.debug_pipeline = wgpuDeviceCreateRenderPipeline(globals.device, &pipelineDesc);
        }

        createWindowResources((u32)wnd_x, (u32)wnd_y);

        bool valid = checkRel(globals.isValid(), "wgpu initialization failed");
        return valid ? 0 : 1;
    }

    auto createWindowResources(u32 wnd_x, u32 wnd_y) -> void {}

    auto resizeWindow(vp::Application& owner, u32 w, u32 h) -> void {}

    auto preFrame(vp::Application& owner) -> void
    {
        WGPUCommandEncoderDescriptor encoderDesc = {
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        frame_data.encoder = wgpuDeviceCreateCommandEncoder(globals.device, &encoderDesc);
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
    }
    auto frame(vp::Application& owner) -> void
    {
        wgpuRenderPassEncoderSetPipeline(frame_data.render_pass_encoder, globals.debug_pipeline);

        wgpuRenderPassEncoderDraw(frame_data.render_pass_encoder, 3, 1, 0, 0);

        wgpuQueueSubmit(globals.queue, 0, nullptr);
    }
    auto postFrame(vp::Application& owner) -> void
    {
        // clear frame and buffers, set targets
        wgpuQueueSubmit(globals.queue, 0, nullptr);
    }
    auto endFrame() -> void
    {
        wgpuRenderPassEncoderEnd(frame_data.render_pass_encoder);
        // Present
        WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
        cmdBufferDescriptor.nextInChain = nullptr;
        cmdBufferDescriptor.label = "Command buffer";
        WGPUCommandBuffer command = wgpuCommandEncoderFinish(
            frame_data.encoder, &cmdBufferDescriptor);
        wgpuQueueSubmit(globals.queue, 1, &command);

        // We can tell the swap chain to present the next texture.
        wgpuSwapChainPresent(globals.swap_chain);
        frame_data.release();
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

        return 0;
    }
    auto findViewport(u32 id) -> WgpuViewport* { return nullptr; }
    auto resetViewport(u32 id, const ViewportInitArgs& args) -> bool
    { //
        if (WgpuViewport* vp = findViewport(id); vp && vp->isValid())
        {
            vp->release();
            vp->initialize(args);
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
void vp::WgpuApp::preFrame(vp::Application& owner)
{
    checkLethal(impl != nullptr, "unexpected null in vp::WgpuApp::preFrame");
#if ENABLE_IMGUI
    ImGui_ImplSDL2_NewFrame(); // window & inputs related frame init specific for SDL
    ImGui::NewFrame();
#endif

    impl->preFrame(owner);
}
void vp::WgpuApp::frame(vp::Application& owner) { impl->frame(owner); }

void vp::WgpuApp::postFrame(vp::Application& owner)
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

        checkLethal(false, " ");
        // ImGui::Image(
        //     (void*)(uintptr_t)native_vp->srv, deltas, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        ImGui::SetCursorPos({20, 40});
        if (ImGui::Checkbox("pause", &vp.paused))
        {
        }
        ImGui::Separator();
    }

    ImGui_ImplWGPU_NewFrame(); // compiles shaders when called first time
    // ImGuizmo::BeginFrame();
    ImGui::Render();

    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), impl->frame_data.render_pass_encoder);
#endif
    // ----------------------------------------------------------------
    // present frame
    impl->endFrame();
}
void vp::WgpuApp::teardown(vp::Application& owner)
{
    impl->release();
    delete impl;
    impl = nullptr;
}
void vp::WgpuApp::handleWindowResize(vp::Application& owner, v2u32 size)
{ //
    impl->resizeWindow(owner, size.x, size.y);
}
