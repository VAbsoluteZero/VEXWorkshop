#include "PathfinderDemo.h"

#include <imgui.h>
#include <utils/CLI.h>
#include <utils/ImGuiUtils.h>
#include <imgui_internal.h>
// #fixme : this whole file

// split work -> threads mb?

/*
 */
void vp::PathfinderDemo::runloop(Application& owner)
{ //
}

void vp::PathfinderDemo::drawUI(Application& owner)
{ //
    setupImGuiForDrawPass(owner);
}

void vp::PathfinderDemo::setupImGuiForDrawPass(vp::Application& app)
{
    // init view
    static bool g_demo_shown = false;
    static bool g_metric_shown = false;
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
            //
            if (ImGui::MenuItem(
                    "Console", nullptr, &vp::console::ConsoleWindow::g_console->is_open))
            {
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Reset UI Layout"))
            {
                ImGui::ClearIniSettings();
                spdlog::warn("Clearing ImGui ini file data.");
            }
        }
        if (ImGui::BeginMenu("Help"))
        {
            defer_ { ImGui::EndMenu(); };
            //ImGui::MenuItem("Show ImGui Demo", nullptr, &g_demo_shown);
            ImGui::MenuItem("Show ImGui Metrics", nullptr, &g_metric_shown);
        }
    }

//    if (g_demo_shown)  ImGui::ShowDemoWindow();
    if (g_metric_shown)
        ImGui::ShowMetricsWindow();

    [[maybe_unused]] ImGuiID main_dockspace = ImGui::DockSpaceOverViewport(nullptr); 
    static ImGuiDockNodeFlags dockspace_flags =
        ImGuiDockNodeFlags_NoDockingInCentralNode; // ImGuiDockNodeFlags_KeepAliveOnly;
     
    if (std::exchange(docking_not_configured, false))
    {
        ImGuiID dockspace_id = main_dockspace;
        auto viewport = ImGui::GetMainViewport();

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(
            main_dockspace, ImGuiDir_Down, 0.25f, nullptr, &dockspace_id);
        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(
            dockspace_id, ImGuiDir_Left, 0.16f, nullptr, &dockspace_id);

        ImGui::DockBuilderDockWindow("Console", dock_id_down);
        ImGui::DockBuilderDockWindow("Details", dock_id_left);
        ImGui::DockBuilderDockWindow("Viewport 1", dockspace_id);
        ImGui::DockBuilderFinish(main_dockspace);
    }

    ImGui::Begin("Details");
    ImGui::Text("Inspector/toolbar placeholder");
    ImGui::End();

    if ( vp::console::ConsoleWindow::g_console->is_open)
        vp::console::showConsoleWindow(vp::console::ConsoleWindow::g_console.get());
}