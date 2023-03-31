#include "ViewportHandler.h"

#include <application/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <utils/ImGuiUtils.h>

void wgfx::ui::ViewportHandler::add(const wgfx::GpuContext& wgpu_ctx, vex::ViewportOptions options)
{
    wgfx::Viewport vex;
    vex.initialize(wgpu_ctx.device, options);
    options.name = options.name ? options.name : "debug";
    imgui_views.push_back({
        .args = options,
        .render_target = vex,
        .gui_enabled = true,
    });
}

void wgfx::ui::ViewportHandler::draw()
{
    for (auto& view : imgui_views)
    {
        auto s = fmt::format("{}", view.args.name);
        if (!view.gui_enabled)
            continue;

        ImGui::SetNextWindowSize(
            ImVec2{(float)view.args.size.x, (float)view.args.size.y}, ImGuiCond_FirstUseEver);
        view.gui_visible = ImGui::Begin(s.data());
        defer_ { ImGui::End(); };

        ImVec2 vmin = ImGui::GetWindowContentRegionMin();
        ImVec2 vmax = ImGui::GetWindowContentRegionMax();
        auto deltas = ImVec2{vmax.x - vmin.x, vmax.y - vmin.y};
        v2i32 cur_size = {deltas.x > 0 ? deltas.x : 32, deltas.y > 0 ? deltas.y : 32};

        auto wp = ImGui::GetWindowPos();
        view.viewport_origin = v2i32{wp.x, wp.y} + v2i32{vmin.x, vmin.y};

        wgfx::Viewport& render_target = view.render_target;
        if (!view.gui_visible || !render_target.isValid())
            continue;

        ImGui::Image(
            (void*)(uintptr_t)render_target.view, deltas, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

        if (cur_size != view.last_seen_size)
        {
            view.args.size = cur_size;
            view.changed_last_frame = true;
        }
        view.last_seen_size = cur_size;

        ImGui::SetCursorPos({20, 40});
        ImGui::Separator();
    }
}

void wgfx::ui::ViewportHandler::updateMouseLoc(v2i32 wnd_mouse_pos)
{
    for (auto& vp : imgui_views)
    {
        vp.mouse_pos_normalized = vp.viewportToNormalizedView(wnd_mouse_pos);
        vp.mouse_over = glm::abs(vp.mouse_pos_normalized).x < 1 &&
                        glm::abs(vp.mouse_pos_normalized).y < 1;
    }
}

void wgfx::ui::ViewportHandler::postFrame()
{
    for (auto& vp : imgui_views)
    {
        vp.render_target.context.release();
        if (vp.changed_last_frame && vp.render_target.context.device)
        {
            vp.changed_last_frame = false;
            vp.render_target.release();
            vp.render_target.initialize(vp.render_target.context.device, vp.args);
        }
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
            if (ImGui::MenuItem("Console", nullptr, &console_wnd.is_open))
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
        const i32 target_fps = app.getMaxFps();
        ImGui::Text(" perf: %.3f ms/frame (%.1f FPS) / limit: %d", 1000.0f / ImGui::GetIO().Framerate,
            ImGui::GetIO().Framerate, target_fps);
    }

    // dockspaces
    if (should_config_docking)
    {
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
            // ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(
            //     dockspace_id, ImGuiDir_Left, 0.16f, nullptr, &dockspace_id);
            ImGui::DockBuilderDockWindow("Console", dock_id_down);
            ImGui::DockBuilderDockWindow("MainViewport", dockspace_id);
            ImGui::DockBuilderFinish(main_dockspace);
        }
    }
    if (g_metric_shown)
        ImGui::ShowMetricsWindow();
    if (console_wnd.is_open)
        vex::console::showConsoleWindow(&console_wnd);
    if (viewports)
        viewports->draw();
}
