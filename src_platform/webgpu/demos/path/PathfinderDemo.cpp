#include "PathfinderDemo.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <utils/CLI.h>
#include <utils/ImGuiUtils.h>

#include <glm/gtx/hash.hpp>
#include <glm/gtx/matrix_decompose.hpp>

using namespace vex;
// #fixme : this whole file is WIP & hardcoded

/*
* draw map with pixel grid (point-filter)
 > make a quad
 > get / create RenderTarget
* >
* ? allow drawing on the map
* ? draw cursor
* draw line/arrow
* draw cell text
* draw boid (arrow head or circle)
* A*
* A* heatmap
* flow fields
* split work -> threads mb?
*
*
* NEEDED KNOWLEDGE:
* blending
* textures
* instancing
* compute
*/
// !!!!!!!!!!!!!!!!!!!!!!!!!! #fixme !!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// for now using hard coded WEBGPU stuff with concrete backend to experiment
// goal is to create abstraction later

struct GraphicsState
{
};
struct PerFrameData
{
    vex::ExpandableBufferAllocator frame_alloc{1024 * 16, 2};
};
struct FrameGfxContext
{
};
struct UBOCamera
{
    mtx4 projection;
    mtx4 model_view;
   // mtx4 view_pos;
};

static inline WGPUCommandEncoderDescriptor ctx_desc{nullptr, "PathfinderDemo cmd descriptor"};
void vex::PathfinderDemo::init(Application& owner, InitArgs args)
{
    AGraphicsBackend* backend = owner.getGraphicsBackend();
    checkLethal(backend->id == GfxBackendID::Webgpu, "unsupported gfx backend");
    this->wgpu_backend = static_cast<WebGpuBackend*>(backend);
    auto& globals = wgpu_backend->getGlobalResources();

    ViewportOptions options;
    wgfx::Viewport vex;
    // wgfx::Context ctx = {
    //     .device = globals.device,
    //     .encoder = wgpuDeviceCreateCommandEncoder(globals.device, &ctx_desc),
    //     .queue = globals.queue,
    // };
    vex.initialize(globals.device, options);
    imgui_views.push_back({
        .args = {.name = "Demo"},
        .visible = true,
        .render_target = vex,
    });
}

void vex::PathfinderDemo::prepareFrameData() {}

void vex::PathfinderDemo::update(Application& owner)
{
    auto& globals = wgpu_backend->getGlobalResources();

    wgpu_ctx = {
        .device = globals.device,
        .encoder = wgpuDeviceCreateCommandEncoder(globals.device, &ctx_desc),
        .queue = globals.queue,
    };
    vex::InlineBufferAllocator<32 * 1024> temp_alloc_resource;
    auto tmp_alloc = temp_alloc_resource.makeAllocatorHandle();

    return; 
}

void vex::PathfinderDemo::drawUI(Application& owner)
{ //
    setupImGuiForDrawPass(owner);
}

void vex::PathfinderDemo::setupImGuiForDrawPass(vex::Application& app)
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

    // whindows
    if (g_metric_shown)
        ImGui::ShowMetricsWindow();
    if (vex::console::ConsoleWindow::g_console->is_open)
        vex::console::showConsoleWindow(vex::console::ConsoleWindow::g_console.get());
}
