#include "WgpuTypes.h"

#include <SDL.h>
#include <SDL_filesystem.h>
#include <SDL_syswm.h>
#include <SDL_video.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#else
#endif

void wgpu::wgpuWait(std::atomic_bool& flag)
{
    while (!flag.load())
    {
#ifndef __EMSCRIPTEN__
        std::this_thread::yield();
#else
        SDL_Delay(1);
#endif
    }
}

void wgpu::wgpuPollWait(WGPUQueue queue, std::atomic_bool& flag)
{
    while (!flag)
    { // #fixme - in dawn there is Tick to poll events, in wgpu this is workaround
        wgpuQueueSubmit(queue, 0, nullptr);
    }
}

WGPUInstance wgpu::createInstance(WGPUInstanceDescriptor desc)
{
    return k_using_emscripten ? nullptr : wgpuCreateInstance(&desc);
}

void wgpu::requestDevice(WgpuGlobals& globals, WGPUDeviceDescriptor const* descriptor)
{
#ifndef __EMSCRIPTEN__
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
    checkLethal(request_done.load(), "should exec synchronously");
#else
    globals.device = emscripten_webgpu_get_device();
#endif
}

void wgpu::requestAdapter(WgpuGlobals& globals, WGPURequestAdapterOptions& options)
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

#ifdef __EMSCRIPTEN__
    wgpu::wgpuWait(request_done);
#else
    checkLethal(request_done.load(), "should exec synchronously");
#endif
}

WGPUSurface wgpu::getWGPUSurface(WGPUInstance instance, SDL_Window* sdl_window)
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
#elif __EMSCRIPTEN__
    WGPUSurfaceDescriptorFromCanvasHTMLSelector canvDesc = {};
    canvDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
    canvDesc.selector = "canvas";

    WGPUSurfaceDescriptor surfDesc = {};
    surfDesc.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&canvDesc);
    return wgpuInstanceCreateSurface(instance, &surfDesc);
#endif
}

#ifdef __EMSCRIPTEN__
EM_BOOL v_emscripten_stub(double /*time*/, void* userData)
{
    //  userData;
    return true;
}
void wgpu::swapchainPresent(WGPUSwapChain swap_chain)
{
    emscripten_request_animation_frame(v_emscripten_stub, nullptr);
}
#else
void wgpu::swapchainPresent(WGPUSwapChain swap_chain) { wgpuSwapChainPresent(swap_chain); }
#endif
