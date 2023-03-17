#include "ViewportHandler.h"

#include <application/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <utils/CLI.h>
#include <utils/ImGuiUtils.h>

void wgfx::ui::ViewportHandler::add(const wgfx::Context& wgpu_ctx, vex::ViewportOptions options)
{
    wgfx::Viewport vex;
    vex.initialize(wgpu_ctx.device, options);
    imgui_views.push_back({
        .args = {.name = options.name ? options.name : "Demo"},
        .visible = true,
        .render_target = vex,
    });
}

void wgfx::ui::ViewportHandler::draw()
{
    for (auto& vex : imgui_views)
    {
        auto s = fmt::format("{}", vex.args.name);
        if (!vex.visible)
            continue;

        bool imgui_visible = ImGui::Begin(s.data());
        defer_ { ImGui::End(); };

        ImVec2 vmin = ImGui::GetWindowContentRegionMin();
        ImVec2 vmax = ImGui::GetWindowContentRegionMax();
        auto deltas = ImVec2{vmax.x - vmin.x, vmax.y - vmin.y};
        v2i32 cur_size = {deltas.x > 0 ? deltas.x : 32, deltas.y > 0 ? deltas.y : 32};

        wgfx::Viewport& render_target = vex.render_target;
        if (!imgui_visible || !render_target.isValid())
            continue;

        ImGui::Image(
            (void*)(uintptr_t)render_target.view, deltas, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        if (cur_size != vex.last_seen_size)
        {
            vex.args.size = cur_size;
            vex.changed_last_frame = true;
        }
        vex.last_seen_size = cur_size;

        ImGui::SetCursorPos({20, 40});
        ImGui::Separator();
    }
}

void wgfx::ui::BasicDemoUI::drawStandardUI(vex::Application& app, ViewportHandler* viewports)
{
    // init view
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
            if (ImGui::MenuItem(
                    "Console", nullptr, &vex::console::ConsoleWindow::g_console->is_open))
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
            ImGui::MenuItem("Show ImGui Metrics", nullptr, &g_metric_shown);
        }
        ImGui::Bullet();
        ImGui::Text(" perf: %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate);
    }

    // dockspaces
    [[maybe_unused]] ImGuiID main_dockspace = ImGui::DockSpaceOverViewport(nullptr);
    {
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
            // ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(
            //     dockspace_id, ImGuiDir_Left, 0.16f, nullptr, &dockspace_id);
            ImGui::DockBuilderDockWindow("Console", dock_id_down);
            // ImGui::DockBuilderDockWindow("Details", dock_id_left);
            ImGui::DockBuilderDockWindow("Viewport 1", dockspace_id);
            ImGui::DockBuilderFinish(main_dockspace);
        }
    }
    if (g_metric_shown)
        ImGui::ShowMetricsWindow();
    if (vex::console::ConsoleWindow::g_console->is_open)
        vex::console::showConsoleWindow(vex::console::ConsoleWindow::g_console.get());

    // viewports
    if (viewports)
    {
        viewports->draw();
    }
}
