#pragma once
#include <VCore/Containers/Dict.h>
#include <VCore/Utils/CoreTemplates.h>
#include <VCore/Utils/VMath.h>
#include <VCore/Utils/VUtilsBase.h>
#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace vp
{
	static constexpr v2i32 g_default_cvs_sz{1920, 1080};
	static v2f g_cvs_sz{1920, 1080};
	static inline float g_imgui_sz_scale = 1.0f;

	//struct ImguiVec2
	//{
	//	float x = 0;
	//	float y = 0;

	//	operator ImVec2() const { return ImVec2{x * g_imgui_sz_scale, y * g_imgui_sz_scale}; }
	//};
	struct ImguiVec2Rel
	{
		float x = 0;
		float y = 0;

		operator ImVec2() const { return ImVec2{x * g_cvs_sz.x, y * g_cvs_sz.y}; }
	};

	enum class EImGuiStyle : u8
	{
		VS = 0,
		DeepDark,
		Hackerman
	};

	struct ImVisLib
	{
		ImFont* fnt_header = nullptr;
		ImFont* fnt_normal = nullptr;
		ImFont* fnt_tiny = nullptr;
		ImFont* fnt_log = nullptr;

		vex::Dict<EImGuiStyle, ImGuiStyle> styles;
	};

	struct ArgsImViewUpd
	{
		ImGuiContext* ctx = nullptr; 
	};

	using ImGuiDrawFn = std::function<void(ArgsImViewUpd)>;
	 
	struct ImView
	{
		const char* view_name = "none";

		// cleared each frame !
		std::vector<ImGuiDrawFn> delayed_gui_drawcalls;

		// float override_alpha = 1.0f;
		bool enabled = true;
	};

	struct ImViewHub
	{
		void onFirstPaintCall(v2f window_size, i32 display_h);
		ImVisLib visuals;

		vex::Dict<const char*, ImView> views;

		template<typename TCallable>
		inline bool tryPushCall(const char* key, TCallable&& callback)
		{
			auto view = views.tryGet(key);
			if (view)
			{
				view->delayed_gui_drawcalls.push_back(callback);
			}
			return view != nullptr;
		}
	};

	inline ImViewHub g_view_hub {};
} // namespace vp 