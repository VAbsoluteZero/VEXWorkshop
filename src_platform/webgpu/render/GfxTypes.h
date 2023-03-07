#pragma once

#include <VFramework/VEXBase.h>
//
#include <d3d11.h>
#include <d3dcompiler.h> // shader compiler
#include <dxgi.h>        // direct X driver interface


#ifdef VEX_WGPU_DAWN
#define WGPU_REL(x, a) x##Release(a)
#include <dawn/webgpu.h>
//#include <dawn/webgpu_cpp.h>
#else
#include <wgpu/webgpu.h>
#include <wgpu/wgpu.h>
#define WGPU_REL(x, a) x##Drop
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
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;

    WGPUSwapChain swap_chain;
    WGPUTextureFormat main_texture_fmt;

    WGPUPipelineLayout debug_layout;
    WGPURenderPipeline debug_pipeline;

    auto isValid() const -> bool
    {
        return instance && device && instance && queue //
               && surface && swap_chain;
    }
    void release()
    {//
        WGPU_REL(wgpuInstance, instance);
        WGPU_REL(wgpuAdapter, adapter);
        WGPU_REL(wgpuDevice, device);
        //WGPU_REL(wgpuQueue, queue);
        WGPU_REL(wgpuSurface, surface);
        //WGPU_REL(wgpuCommandEncoder, encoder);
        WGPU_REL(wgpuSwapChain, swap_chain);
        wgpuDeviceDestroy(device);
    }
};

struct WgpuFrameResources
{
    WGPUTextureView cur_tex_view;
    WGPUCommandEncoder encoder;
    WGPURenderPassEncoder render_pass_encoder;

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

struct WgpuViewport
{
    static inline u32 g_generation = 0;
    const u32 uid = ++g_generation;

    ViewportInitArgs initial_args;

    auto isValid() const -> bool
    {
        bool v = false;
        return v;
    }

    void initialize(const ViewportInitArgs& args);

    void release() {}
};