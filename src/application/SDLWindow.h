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
		int x = -1;
		int y = -1; 
		EWindowMode m;
		bool resizable = true;
	};

	class SDLWindow
	{
	public:
		using tWindowHandle = SDL_Window;   

		static std::unique_ptr<SDLWindow> create(const WindowParams& params);

		tWindowHandle* getRawWindow() { return sdl_window; } 

		v2u32 windowSize();
		v2i32 display_size{0, 0};

		~SDLWindow();
		 

	private:
		SDLWindow() = default;
		SDLWindow(const SDLWindow&) = default;

		WindowParams params;
		tWindowHandle* sdl_window = nullptr; 
	};
} // namespace vp