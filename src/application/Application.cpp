#include "Application.h"

#include <algorithm>
#include <functional>
//
#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_internal.h>
#include <utils/ImGuiUtils.h>

// fwd decl
namespace vexgui
{
	auto setupImGuiForDrawPass(vp::Application& app) -> void;
}

vp::Application& vp::Application::init(const StartupConfig& config)
{
	Application& self = staticSelf();

	self.main_window = tWindow::create(config.WindowArgs);
	self.created = true;

	self.framerate.TargetFramerate = config.TargetFramerate;

	Sleep(10);
	if (!self.app_impl)
	{
		self.setApplicationType<SdlDx11Application>(true);
	}

	return self;
}

i32 vp::Application::runLoop()
{
	running = true;
	if (pending_stop)
		return 0;

	// Init ImGui related stuff
	{
		g_view_hub.onFirstPaintCall(main_window->windowSize(), main_window->display_size.y);
		ImGui::GetStyle() = g_view_hub.visuals.styles[EImGuiStyle::DeepDark];
	}

	// Event handler
	SDL_Event sdl_event = {};
	do
	{
		// Handle events on queue
		while (SDL_PollEvent(&sdl_event) != 0)
		{
			ImGui_ImplSDL2_ProcessEvent(&sdl_event);
			if (sdl_event.type == SDL_QUIT)
			{
				spdlog::info("Received SDL_QUIT, now app is pending stop.");
				pending_stop = true;
			}
		}
		// prepare frame
		{
			app_impl->preFrame(*this); 
		} 
		// frame
		{
			app_impl->frame(*this);
		}
		// imgui draw callbacks
		{
			vexgui::setupImGuiForDrawPass(*this);

			for (auto& [view_name, view] : g_view_hub.views)
			{
				for (auto& callback : view.delayed_gui_drawcalls)
				{
					if (callback)
						callback({ImGui::GetCurrentContext()});
				}
				view.delayed_gui_drawcalls.clear();
			}
		}
		// post frame
		{
			app_impl->postFrame(*this);
		}
	} while (running && !pending_stop);

	if (app_impl)
	{
		app_impl->teardown(*this);
		app_impl = {};
	}
	SDL_Quit();

	return 0;
}

// ======================================================================================== 
void vexgui::setupImGuiForDrawPass(vp::Application& app)
{
	// init view
	if (ImGui::BeginMainMenuBar())
	{
		defer_ { ImGui::EndMainMenuBar(); };

		if (ImGui::BeginMenu("File"))
		{
			defer_ { ImGui::EndMenu(); };
			if (ImGui::MenuItem("Quit" /*, "CTRL+Q"*/))
			{
				spdlog::warn(" Shutting down : initiated by user.");
				app.quit();
			}
		}
		if (ImGui::BeginMenu("View"))
		{
			defer_ { ImGui::EndMenu(); };
			// ImGui::Separator();
			if (ImGui::MenuItem("Reset UI Layout"))
			{
				ImGui::ClearIniSettings();
				spdlog::warn("Clearing ImGui ini file data.");
			}
		}
		if (ImGui::BeginMenu("Help"))
		{
			defer_ { ImGui::EndMenu(); };
			if (ImGui::MenuItem("Show ImGui Demo"))
			{
				ImGui::ShowDemoWindow();
			}
			if (ImGui::MenuItem("Show ImGui Metrics"))
			{
				ImGui::ShowMetricsWindow();
			}
		}

		ImGui::Bullet();
		ImGui::Text(" perf: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
			ImGui::GetIO().Framerate);
	}

	[[maybe_unused]] ImGuiID main_dockspace = ImGui::DockSpaceOverViewport(nullptr);

	static ImGuiDockNodeFlags dockspace_flags =
		ImGuiDockNodeFlags_NoDockingInCentralNode; // ImGuiDockNodeFlags_KeepAliveOnly;
	static auto do_once_guard = true;
	if (std::exchange(do_once_guard, false))
	{
		ImGuiID dockspace_id = main_dockspace;
		auto viewport = ImGui::GetMainViewport();

		ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
		ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

		ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(
			main_dockspace, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
		ImGuiID dock_id_down =
			ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.25f, nullptr, &dockspace_id);

		ImGui::DockBuilderDockWindow("Tools", dock_id_down);
		ImGui::DockBuilderDockWindow("Details", dock_id_left);
		ImGui::DockBuilderFinish(main_dockspace);
	}

	ImGui::Begin("Tools");
	ImGui::Text("Inspector/toolbar placeholder");
	ImGui::End();

	ImGui::Begin("Details");
	ImGui::Text("Details/Console placeholder");
	ImGui::End();
}
