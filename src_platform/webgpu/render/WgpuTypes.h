#pragma once

#include <VFramework/VEXBase.h>
#ifdef VEX_GFX_WEBGPU_DAWN
#define WGPU_REL(x, a) x##Release(a)
#include <webgpu.h> 
#else
#include <wgpu/webgpu.h>
#include <wgpu/wgpu.h>
#define WGPU_REL(x, a) x##Drop
#endif
#ifdef __EMSCRIPTEN__
#define EMSCRIPTEN_WGPU = 1
#else
#define EMSCRIPTEN_WGPU = 0
#endif

template <typename ComPtr>
auto releaseAndNullCOMPtr(ComPtr& ptr) -> u32
{
    if (nullptr != ptr)
    {
        auto rv = ptr->Release();
        ptr = nullptr;
        return rv;
    }
    return 0u;
}

struct WgpuGlobals
{
    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    WGPUSurface surface = nullptr;

    WGPUSwapChain swap_chain = nullptr;
    WGPUTextureFormat main_texture_fmt{};

    WGPUPipelineLayout debug_layout = nullptr;
    WGPURenderPipeline debug_pipeline = nullptr; 
    WGPURenderPipeline debug_pipeline2 = nullptr;

    auto isValid() const -> bool
    {
#ifdef __EMSCRIPTEN__
        return device && swap_chain;
#else
        return instance && device && instance && queue //
               && surface && swap_chain;
#endif
    }
    void release()
    { //
        WGPU_REL(wgpuSurface, surface);
        WGPU_REL(wgpuSwapChain, swap_chain);
        // WGPU_REL(wgpuDevice, device);
        wgpuDeviceDestroy(device);
        WGPU_REL(wgpuAdapter, adapter);
        WGPU_REL(wgpuInstance, instance);
    }
};

struct WgpuFrameResources
{
    WGPUTextureView cur_tex_view = nullptr;
    WGPUCommandEncoder encoder = nullptr;
    WGPURenderPassEncoder render_pass_encoder = nullptr;

    void release()
    {
        WGPU_REL(wgpuTextureView, cur_tex_view);
        WGPU_REL(wgpuRenderPassEncoder, render_pass_encoder);
        WGPU_REL(wgpuCommandEncoder, encoder);
    };
};

static constexpr v2i32 vp_default_sz{800, 600};
struct ViewportInitArgs
{
    v2u32 size = vp_default_sz;
    bool wireframe = false;
};

struct ViewportFrameResources
{
    WGPUCommandEncoder encoder = nullptr;
    WGPURenderPassEncoder render_pass_encoder = nullptr;

    void release()
    {
        WGPU_REL(wgpuRenderPassEncoder, render_pass_encoder);
        WGPU_REL(wgpuCommandEncoder, encoder);
    };
};

struct WgpuViewport
{
    static inline u32 g_generation = 0;
    const u32 uid = ++g_generation;

    ViewportInitArgs initial_args;
    WGPUTexture texture = nullptr;
    WGPUTextureView view = nullptr;

    WGPUCommandEncoderDescriptor encoder_desc = {
        .nextInChain = nullptr,
        .label = "viewport cmd encoder",
    };
    WGPURenderPassColorAttachment color_attachment{
        .resolveTarget = nullptr,
        .loadOp = WGPULoadOp_Clear,
        .storeOp = WGPUStoreOp_Store,
        .clearValue = WGPUColor{0.1, 0.3, 0.1, 1.0},
    };

    ViewportFrameResources frame_data;

    auto size() -> v2u32& { return initial_args.size;}

    auto isValid() const -> bool { return texture && view; }

    void initialize(WgpuGlobals& globals, const ViewportInitArgs& args);

    void setupForDrawing();
    void drawAndSubmit();

    void release()
    {
        WGPU_REL(wgpuTexture, texture);
        WGPU_REL(wgpuTextureView, view);
    }
};

struct SDL_Window;
namespace wgpu
{
    static constexpr bool k_using_emscripten =
#ifdef __EMSCRIPTEN__
        true;
#else
        false;
#endif
    void wgpuWait(std::atomic_bool& flag);
    void wgpuPollWait(WGPUQueue queue, std::atomic_bool& flag);

    WGPUInstance createInstance(WGPUInstanceDescriptor desc);

    template <typename Fptr, typename Callback, typename... TArgs>
    auto wgpuRequest(Fptr request, Callback&& callback, TArgs... args)
    {
        static auto lambda_decay_to_ptr = [](auto... params)
        {
            auto last = (vex::traits::identityFunc(params), ...);
            static_assert(std::is_same_v<decltype(last), void*>, "expected payload as last asrg");
            auto callback_ptr = reinterpret_cast<std::decay_t<Callback>*>(last);
            (*callback_ptr)(params...);
        };
        request(args..., lambda_decay_to_ptr, reinterpret_cast<void*>(&callback));
    }

    void requestDevice(WgpuGlobals& globals, WGPUDeviceDescriptor const* descriptor);
    void requestAdapter(WgpuGlobals& globals, WGPURequestAdapterOptions& options);
    WGPUSurface getWGPUSurface(WGPUInstance instance, SDL_Window* sdl_window);

    void swapchainPresent(WGPUSwapChain swap_chain);
} // namespace wgpu