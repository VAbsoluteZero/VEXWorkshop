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

		virtual void handleWindowResize(class Application& owner, v2u32 size){};

		virtual ~IGraphicsImpl(){}
	};

	// for config parsing/serialization
	using DynVal = vex::Union<i32, f32, bool, v3f, v2i32, const char*, std::string>;

	using OptNumValue = vex::Union<i32, f32, bool>;

	struct SettingsContainer
	{
		vex::Dict<std::string, OptNumValue> settings;
		vex::Dict<std::string, std::function<void(OptNumValue)>> on_changed;
		bool changed_this_tick = true;

		template <typename T>
		bool setValue(std::string key, T val)
		{
			if (checkAlways(on_changed.contains(key), "Entry does not exist in app settings"))
			{
				return false;
			}
			OptNumValue& opt_val = settings[key];
			if (T* stored_val = opt_val.find<std::decay_t<T>>(); nullptr != stored_val)
			{
				*stored_val = val;
				if (std::function<void(OptNumValue)>* cb = on_changed.tryGet(key); nullptr != cb)
				{
					(*cb)(val);
					return true;
				}
			}
			return false;
		}

		template <typename T>
		void addSetting(std::string key, T default_val)
		{
			constexpr bool v_is_supported = std::is_same_v<T, i32> || std::is_same_v<T, f32> ||
											std::is_same_v<T, bool>;
			static_assert(v_is_supported, "type is not supported");

			OptNumValue v = default_val;
			settings.emplace(std::move(key), std::move(v));
		}
		template <typename T>
		void addSetting(std::string key, T default_val, std::function<void(OptNumValue)> cb_changed)
		{
			addSetting(key, default_val);

			checkAlways(!on_changed.contains(key),
				"Warning: overriding 'on_changed' callback in app settings");

			on_changed.emplace(std::move(key), cb_changed);
		}

		void removeSetting(std::string key)
		{
			settings.remove(key);
			on_changed.remove(key);
		}
	};
} // namespace vp
