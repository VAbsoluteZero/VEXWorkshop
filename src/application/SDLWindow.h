#pragma once

#include <VCore/Utils/VMath.h>

#include <memory>
#include <string>

struct SDL_Window;
struct SDL_Renderer;
namespace vp
{
	enum class EWindowMode : unsigned char
	{
		SDLWindow,
		BorderlessFullscreen,
		Fullscreen
	};

	struct WindowParams
	{
		std::string name = "demo";
		int w = 2840;
		int h = 1620;

		EWindowMode m = EWindowMode::SDLWindow;
	};

	class SDLWindow
	{
	public:
		using tWindowHandle = SDL_Window; 

		int height() { return params.h; }
		int width() { return params.w; }
		EWindowMode mode() { return params.m; }

		static std::unique_ptr<SDLWindow> create(const WindowParams& params);

		tWindowHandle* getRawWindow() { return sdl_window; } 

		v2i32 windowSize() { return {params.w, params.h}; }
		v2i32 display_size{0, 0};

		~SDLWindow();
		 

	private:
		SDLWindow() = default;
		SDLWindow(const SDLWindow&) = default;

		WindowParams params;
		tWindowHandle* sdl_window = nullptr; 
	};
} // namespace vp