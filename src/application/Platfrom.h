#pragma once
#include <VFramework/VEXBase.h>
#include "SDLWindow.h"  

namespace vp
{
	using tWindow = SDLWindow;
	struct StartupConfig
	{
		WindowParams WindowArgs;
		i32 target_framerate = -1;
	}; 

	struct DrawCtx
	{
		tWindow* window = nullptr;
	};

	enum class LayersBaseOrder : u64
	{
		Earliest = 1000,
		Ealry = 2000,
		PreDefault = 3000,
		Default = 6000,
		PostDefault = 9000,
		Late = 12000,
		Latest = 14000
	}; 
	  
	struct IGraphicsImpl
	{
		virtual i32 init(class Application& owner) = 0;

		virtual void preFrame(class Application& owner) {}
		virtual void frame(class Application& owner) {}
		virtual void postFrame(class Application& owner) {}

		virtual void teardown(class Application& owner) {} 

		virtual ~IGraphicsImpl(){}
	};
} // namespace vp
