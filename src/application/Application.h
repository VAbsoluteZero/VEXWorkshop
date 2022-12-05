#pragma once
#include <VCore/Containers/Union.h>
#include <VCore/Utils/VMath.h>
#include <VFramework/VEXBase.h>

#include "Platfrom.h"
#include "SDLDx11Application.h"

namespace vp
{
	struct FramerateArgs
	{
		static constexpr i32 k_max_allowed = 4096 * 16;
		i32 target_framerate = -1;

		u32 frame_number = 0;

		inline bool fpsUnconstrained() const { return target_framerate < 0; }
		inline u32 desiredFramerate() const
		{
			if (fpsUnconstrained())
				return (u32)k_max_allowed;
			return (u32)target_framerate;
		}
		inline auto framerateClamped(i32 Min = 8, i32 Max = k_max_allowed)
		{
			return glm::clamp<i32>(desiredFramerate(), Min, Max);
		}
	};

	/*
	 * Scaled time exists for situations when whole simulation runs with time multiplier.
	 * It is not to meant to handle any game logic, only naive cases and debug.
	 */
	struct FrameTime
	{
		inline const float getUnscaled() { return unscalled_dt_f32; }
		inline const double getUnscaledF64() { return unscaled_dt; }

		inline const float get() { return delta_time_f32; }
		inline const double getF64() { return dt; }

		inline const void setTimeMutlipier(double value) { mult = (value < 0 ? 0 : value); }
		inline const double getTimeMutlipier() { return mult; }

		friend class Application;

	private:
		double unscaled_dt = 0;
		double dt = 0;
		double mult = 1.0f;
		// cached f32 version so there is no need to cast.
		float unscalled_dt_f32 = 0;
		float delta_time_f32 = 0;
	};

	class Application
	{
	public:
		static Application& init(const StartupConfig& config);

		static inline Application& get()
		{
			auto& me = staticSelf();
			checkAlwaysRel(me.created, "application was not created");
			return me;
		}

		inline bool isRunning() const { return running; }
		inline void quit() { pending_stop = true; }
		inline void setMaxFps(i32 target) { framerate.target_framerate = target; }
		inline i32 getMaxFps(i32 target) { return framerate.desiredFramerate(); }
		inline tWindow* getMainWindow() const { return main_window.get(); }

		template <typename T>
		void setApplicationType(bool init);

		i32 runLoop();

	private:
		static Application& staticSelf()
		{
			static Application instance;
			return instance;
		}
		Application() = default;

		std::unique_ptr<IGraphicsImpl> app_impl;
		std::unique_ptr<tWindow> main_window;

		FramerateArgs framerate;

		bool running = false;
		bool pending_stop = false;
		bool created = false;
	};

	template <typename T>
	inline void Application::setApplicationType(bool init)
	{
		if (app_impl)
		{
			app_impl->teardown(*this);
		}
		app_impl.reset(new T());
		if (init)
		{
			app_impl->init(*this);
		}
	}

	//=====================================================================================
	// variables
	inline FrameTime gTime = {};

} // namespace vp
