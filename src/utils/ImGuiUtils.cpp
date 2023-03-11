#include "ImGuiUtils.h"

#include <SDL.h>
#include <spdlog/spdlog.h>

auto colorFromBytes(uint8_t r, uint8_t g, uint8_t b)
{
	return ImVec4((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, 1.0f);
}; 

void vp::ImViewHub::onFirstPaintCall(v2f window_size, i32 display_h)
{
	check(ImGui::GetCurrentContext(), "ImGui must be initialized at this point");
	// =================================
	{
		float scaling_f = static_cast<float>(display_h) / g_default_cvs_sz.y;
		scaling_f *= 2;
		scaling_f = (i32)std::ceil(scaling_f);
		g_imgui_sz_scale = scaling_f / 2.0f;

		g_cvs_sz = window_size;
	}

	auto scale_font = [sf = g_imgui_sz_scale](int def) { return (int)(def * sf); };

	// =================================
	// fonts
    auto& io = ImGui::GetIO();
#ifndef __EMSCRIPTEN__
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigDockingWithShift = false;
	{
		//ImGuiStyle& style = ImGui::GetStyle();

		auto sz = io.DisplaySize;

		spdlog::info("imgui sees : size : {} , {}", sz.x, sz.y);

		visuals.fnt_normal = io.Fonts->AddFontFromFileTTF("content/fonts/roboto_light.ttf",
			scale_font(12), NULL, io.Fonts->GetGlyphRangesDefault());

		visuals.fnt_header = io.Fonts->AddFontFromFileTTF("content/fonts/fira_medium.ttf",
			scale_font(16), NULL, io.Fonts->GetGlyphRangesDefault());

		visuals.fnt_tiny = io.Fonts->AddFontFromFileTTF("content/fonts/roboto_light.ttf",
			scale_font(9), NULL, io.Fonts->GetGlyphRangesDefault());

		visuals.fnt_log = io.Fonts->AddFontFromFileTTF("content/fonts/ubuntu_mono.ttf",
			scale_font(9), NULL, io.Fonts->GetGlyphRangesDefault());

		io.Fonts->Build();
	}
	// ================================= 
	ImGui::GetStyle().Colors[ImGuiCol_DockingEmptyBg] = colorFromBytes(47, 47, 49);
	// styles
	{ // vs-like
		auto new_style = ImGui::GetStyle();
		ImVec4* colors = new_style.Colors;
		 
		const ImVec4 bg_clr = colorFromBytes(37, 37, 38);
		const ImVec4 light_bg = colorFromBytes(82, 82, 85);
		const ImVec4 very_light = colorFromBytes(90, 90, 95);

		const ImVec4 panel_clr = colorFromBytes(51, 51, 55);
		const ImVec4 panel_hover_clr = colorFromBytes(29, 151, 236);
		const ImVec4 panel_active_color = colorFromBytes(0, 119, 200);

		const ImVec4 text_clr = colorFromBytes(255, 255, 255);
		const ImVec4 text_disabled_clr = colorFromBytes(151, 151, 151);
		const ImVec4 border_clr = colorFromBytes(78, 78, 78);

		colors[ImGuiCol_Text] = text_clr;
		colors[ImGuiCol_TextDisabled] = text_disabled_clr;
		colors[ImGuiCol_TextSelectedBg] = panel_active_color;
		colors[ImGuiCol_WindowBg] = bg_clr;
		colors[ImGuiCol_ChildBg] = bg_clr;
		colors[ImGuiCol_PopupBg] = bg_clr;
		colors[ImGuiCol_Border] = border_clr;
		colors[ImGuiCol_BorderShadow] = border_clr;
		colors[ImGuiCol_FrameBg] = panel_clr;
		colors[ImGuiCol_FrameBgHovered] = panel_hover_clr;
		colors[ImGuiCol_FrameBgActive] = panel_active_color;
		colors[ImGuiCol_TitleBg] = bg_clr;
		colors[ImGuiCol_TitleBgActive] = bg_clr;
		colors[ImGuiCol_TitleBgCollapsed] = bg_clr;
		colors[ImGuiCol_MenuBarBg] = panel_clr;
		colors[ImGuiCol_ScrollbarBg] = panel_clr;
		colors[ImGuiCol_ScrollbarGrab] = light_bg;
		colors[ImGuiCol_ScrollbarGrabHovered] = very_light;
		colors[ImGuiCol_ScrollbarGrabActive] = very_light;
		colors[ImGuiCol_CheckMark] = panel_active_color;
		colors[ImGuiCol_SliderGrab] = panel_hover_clr;
		colors[ImGuiCol_SliderGrabActive] = panel_active_color;
		colors[ImGuiCol_Button] = panel_clr;
		colors[ImGuiCol_ButtonHovered] = panel_hover_clr;
		colors[ImGuiCol_ButtonActive] = panel_hover_clr;
		colors[ImGuiCol_Header] = panel_clr;
		colors[ImGuiCol_HeaderHovered] = panel_hover_clr;
		colors[ImGuiCol_HeaderActive] = panel_active_color;
		colors[ImGuiCol_Separator] = border_clr;
		colors[ImGuiCol_SeparatorHovered] = border_clr;
		colors[ImGuiCol_SeparatorActive] = border_clr;
		colors[ImGuiCol_ResizeGrip] = bg_clr;
		colors[ImGuiCol_ResizeGripHovered] = panel_clr;
		colors[ImGuiCol_ResizeGripActive] = light_bg;
		colors[ImGuiCol_PlotLines] = panel_active_color;
		colors[ImGuiCol_PlotLinesHovered] = panel_hover_clr;
		colors[ImGuiCol_PlotHistogram] = panel_active_color;
		colors[ImGuiCol_PlotHistogramHovered] = panel_hover_clr;
		colors[ImGuiCol_ModalWindowDimBg] = bg_clr;
		colors[ImGuiCol_DragDropTarget] = bg_clr;
		colors[ImGuiCol_NavHighlight] = bg_clr;
		colors[ImGuiCol_DockingPreview] = panel_active_color;
		colors[ImGuiCol_Tab] = bg_clr;
		colors[ImGuiCol_TabActive] = panel_active_color;
		colors[ImGuiCol_TabUnfocused] = bg_clr;
		colors[ImGuiCol_TabUnfocusedActive] = panel_active_color;
		colors[ImGuiCol_TabHovered] = panel_hover_clr;

		new_style.WindowRounding = 0.0f;
		new_style.ChildRounding = 0.0f;
		new_style.FrameRounding = 0.0f;
		new_style.GrabRounding = 0.0f;
		new_style.PopupRounding = 0.0f;
		new_style.ScrollbarRounding = 0.0f;
		new_style.TabRounding = 0.0f;

		visuals.styles.emplace(EImGuiStyle::VS, new_style);
	}
	// deep dark
	{
		auto new_style = ImGui::GetStyle();
		ImVec4* colors = new_style.Colors;
		colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 1.0f);
		colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
		colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
		colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
		colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

		new_style.WindowPadding = ImVec2(8.00f, 8.00f);
		new_style.FramePadding = ImVec2(5.00f, 2.00f);
		new_style.CellPadding = ImVec2(6.00f, 6.00f);
		new_style.ItemSpacing = ImVec2(6.00f, 6.00f);
		new_style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
		new_style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
		new_style.IndentSpacing = 25;
		new_style.ScrollbarSize = 15;
		new_style.GrabMinSize = 10;
		new_style.WindowBorderSize = 1;
		new_style.ChildBorderSize = 1;
		new_style.PopupBorderSize = 1;
		new_style.FrameBorderSize = 1;
		new_style.TabBorderSize = 1;
		new_style.WindowRounding = 7;
		new_style.ChildRounding = 4;
		new_style.FrameRounding = 3;
		new_style.PopupRounding = 4;
		new_style.ScrollbarRounding = 9;
		new_style.GrabRounding = 3;
		new_style.LogSliderDeadzone = 4;
		new_style.TabRounding = 4;

		visuals.styles.emplace(EImGuiStyle::DeepDark, new_style);
	}
	// hackerman
	{
		auto new_style = ImGui::GetStyle();
		ImVec4* colors = new_style.Colors; 

		new_style.WindowPadding = ImVec2(8.00f, 8.00f);
		new_style.FramePadding = ImVec2(5.00f, 2.00f);
		new_style.CellPadding = ImVec2(6.00f, 6.00f);
		new_style.ItemSpacing = ImVec2(6.00f, 6.00f);
		new_style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
		new_style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
		new_style.IndentSpacing = 25;
		new_style.ScrollbarSize = 15;
		new_style.WindowBorderSize = 1;
		new_style.ChildBorderSize = 1;
		new_style.PopupBorderSize = 1;
		new_style.FrameBorderSize = 1;
		new_style.TabBorderSize = 1;
		new_style.WindowRounding = 7;
		new_style.ChildRounding = 4;
		new_style.PopupRounding = 4;
		new_style.ScrollbarRounding = 9;
		new_style.LogSliderDeadzone = 4;
		new_style.TabRounding = 4;

		new_style.Alpha = 1.0;
		new_style.WindowRounding = 3;
		new_style.GrabRounding = 1;
		new_style.GrabMinSize = 20;
		new_style.FrameRounding = 3;

		colors[ImGuiCol_Text] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.00f, 0.40f, 0.41f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);

		colors[ImGuiCol_Border] = ImVec4(0.00f, 1.00f, 1.00f, 0.65f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.44f, 0.80f, 0.80f, 0.18f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.44f, 0.80f, 0.80f, 0.27f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.44f, 0.81f, 0.86f, 0.66f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.18f, 0.21f, 0.73f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.27f);

		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.22f, 0.29f, 0.30f, 0.71f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.44f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 1.00f, 1.00f, 0.68f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 1.00f, 1.00f, 0.36f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.76f);
		colors[ImGuiCol_Button] = ImVec4(0.00f, 0.65f, 0.65f, 0.46f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.01f, 1.00f, 1.00f, 0.43f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.62f);
		colors[ImGuiCol_Header] = ImVec4(0.00f, 1.00f, 1.00f, 0.33f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.42f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);

		colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
		colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f); 

		colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 1.00f, 1.00f, 0.54f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.74f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 1.00f, 1.00f, 0.22f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
		colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

		visuals.styles.emplace(EImGuiStyle::Hackerman, new_style);
	}

	if (g_imgui_sz_scale > 1.25f)
	{
		for (auto& [k, v] : visuals.styles)
		{
			v.ScaleAllSizes(g_imgui_sz_scale * 0.8f);
		}
	}
}
