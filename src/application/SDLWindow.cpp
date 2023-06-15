#include "SDLWindow.h"

#include <SDL.h>
#include <spdlog/spdlog.h>
#include <Tracy.hpp>


#ifdef VEX_EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5.h> 
#endif

using namespace vex;

std::unique_ptr<SDLWindow> vex::SDLWindow::create(const WindowParams& params)
{
    ZoneScopedN("startup:create window");
    uint32_t mode = SDL_WindowFlags::SDL_WINDOW_SHOWN | SDL_WindowFlags::SDL_WINDOW_ALLOW_HIGHDPI;

    if (params.m == EWindowMode::BorderlessFullscreen)
        mode |= SDL_WindowFlags::SDL_WINDOW_BORDERLESS;
    if (params.resizable)
        mode |= SDL_WindowFlags::SDL_WINDOW_RESIZABLE;

    auto x = params.x > 0 ? (u32)params.x : SDL_WINDOWPOS_UNDEFINED;
    auto y = params.y > 0 ? (u32)params.y : SDL_WINDOWPOS_UNDEFINED;
    auto w = params.w;
    auto h = params.h;

    auto init_result = SDL_Init(SDL_INIT_VIDEO);
    checkLethal(init_result >= 0, "sdl init failed");
    if (w <= 0 || h <= 0)
    {
        SDL_Rect r;
        if (SDL_GetDisplayBounds(0, &r) >= 0)
        {
            w = (int)(r.w * 0.7f);
            h = (int)(r.h * 0.75f);
        }
    }

    SPDLOG_INFO("SDL::CreateWindow: {}, {}", w, h);
    SDL_Window* window = SDL_CreateWindow(params.name.c_str(), x, y, w, h, mode);

    SDL_GetWindowSize(window, &w, &h);
    SPDLOG_INFO("SDL::SDL_GetWindowSize: {}, {}", w, h);

    if (nullptr == window)
    {
        spdlog::error("failed to create window, Error(SDL):{0}", SDL_GetError());
        return nullptr;
    }
    else
    {
        auto wrapper = std::unique_ptr<SDLWindow>(new SDLWindow());
        wrapper->params = params;
        wrapper->sdl_window = window;

        auto display_index = SDL_GetWindowDisplayIndex(window);
        float ddpi = 0, hdpi = 0, vdpi = 0;
        SDL_Rect r;
        if (SDL_GetDisplayDPI(display_index, &ddpi, &hdpi, &vdpi) >= 0)
        {
            spdlog::info(" ddpi: {} hdpi: {} vdpi: {}", ddpi, hdpi, vdpi);
        }
        if (SDL_GetDisplayBounds(display_index, &r) >= 0)
        {
            spdlog::info(" display rect x: {} y: {} w: {} h: {} ", r.x, r.y, r.w, r.h);
        }
        wrapper->display_size = {r.w, r.h};

        return wrapper;
    }

    return nullptr;
}

v2u32 vex::SDLWindow::windowSize()
{
    v2i32 size;
#ifdef VEX_EMSCRIPTEN
    emscripten_get_canvas_element_size("canvas", &size.x, &size.y);
#else
    SDL_GetWindowSize(sdl_window, &size.x, &size.y);
#endif
    return {(u32)size.x, (u32)size.y};
}

vex::SDLWindow::~SDLWindow() { SDL_DestroyWindow(sdl_window); }
