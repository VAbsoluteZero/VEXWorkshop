#include "SDLWindow.h"

#include <SDL.h>
#include <spdlog/spdlog.h>
using namespace vp;

std::unique_ptr<SDLWindow> vp::SDLWindow::create(const WindowParams& params)
{
	uint32_t mode = SDL_WindowFlags::SDL_WINDOW_SHOWN;

	if (params.m == EWindowMode::BorderlessFullscreen)
		mode |= SDL_WindowFlags::SDL_WINDOW_BORDERLESS;
	if (params.resizable)
		mode |= SDL_WindowFlags::SDL_WINDOW_RESIZABLE;

	auto x = params.x > 0 ? (u32)params.x : SDL_WINDOWPOS_UNDEFINED;
	auto y = params.y > 0 ? (u32)params.y : SDL_WINDOWPOS_UNDEFINED;

	SDL_Window* window = SDL_CreateWindow(params.name.c_str(), x, y, params.w, params.h, mode);

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

v2u32 vp::SDLWindow::windowSize()
{
	v2i32 size;
	SDL_GetWindowSize(sdl_window, &size.x, &size.y);
	return {(u32)size.x, (u32)size.y};
}

vp::SDLWindow::~SDLWindow() { SDL_DestroyWindow(sdl_window); }
