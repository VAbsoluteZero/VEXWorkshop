#include "FlowFieldsDemo.h"

#include <SDL.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <utils/ImGuiUtils.h>

#include <algorithm>
#include <functional>

constexpr auto DemoView = "TestDrawPanel";

void vp::FlowFieldsDemo::init()
{  
	initialized = true;

	vp::g_view_hub.views.emplace(DemoView, vp::ImView{DemoView});
}

// make docking area and separate viewport surface. 
void vp::FlowFieldsDemo::onPaint(DrawCtx& ctx)
{
	if (!initialized)
		init();

	vp::ImView* view = vp::g_view_hub.views.tryGet(DemoView);
	auto v2 = view;
	bool as_bool = static_cast<bool>(view);
	if (!checkRel(view, "Could not find a view with given name"))
	{
		return;
	}

	view->delayed_gui_drawcalls.push_back([&](ArgsImViewUpd args) { //
		if (ImGui::BeginMainMenuBar())
		{
			defer_ { ImGui::EndMainMenuBar(); };

			if (ImGui::BeginMenu("File"))
			{
				ImGui::EndMenu();
			}
		}

		bool visible = ImGui::Begin("Samples");
		defer_ { ImGui::End(); };

		if (!visible)
			return;

		if (ImGui::Button("test", ImVec2{100, 10}))
		{
			VEX_DBGBREAK();
		}
	}); 
}
