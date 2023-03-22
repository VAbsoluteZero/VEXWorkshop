#include "PathfinderDemo.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/stopwatch.h>
#include <utils/CLI.h>
#include <utils/ImGuiUtils.h>

#include <glm/gtx/hash.hpp>
#include <glm/gtx/matrix_decompose.hpp>

using namespace vex;
using namespace vex::input;
using namespace vex::pf_demo;
using namespace wgfx;
using namespace std::literals::string_view_literals;
constexpr const char* console_name = "Console##pathfind";
/*
* draw map with pixel grid (point-filter)
 > TEXTURE -> data -> TEXTURE on init
* draw frame
* draw cell text
* draw boid (arrow head or circle)
* A*
* A* heatmap
* flow fields
* split work -> threads mb?
*
*/
static InitData fromImage(const char* img = "content/sprites/pf_demo/grid_map32.png")
{
    spdlog::stopwatch sw;
    defer_ { SPDLOG_WARN("Texture loader: loading PNG, elapsed: {} seconds", sw); };

    auto texture = loadImage(img);
    if (!checkAlwaysRel(texture.data, "invalid source texture"))
        return {};
    const u32 bytes_per_row = texture.width * texture.channel_count;
    check_(texture.channel_count == 4);
    const u32 rows = texture.height;
    const u32 cols = texture.width;
    const u32 byte_len = bytes_per_row * rows;
    u32* data_as_u32 = reinterpret_cast<u32*>(texture.data);

    InitData out = {.grid = {.size = {texture.width, texture.height}}};

    out.grid.array.addUninitialized(texture.width * texture.height);
    u32* out_write_ptr = out.grid.array.data();
    for (u32 i = 0; i < rows * cols; i++)
    {
        u32 c = *(data_as_u32 + i);
        *out_write_ptr = (u8)(c & 0x000000ff);
        out_write_ptr++;
        if ((c & 0x00ff0000) > 16)
            out.spawn_location = {i % texture.width, i / texture.width};
        if ((c & 0x0000ff00) > 16)
            out.initial_goal = {i % texture.width, i / texture.width};
    }
    std::string result = "map:\n";
    result.reserve(byte_len * 4 + rows + 4);
    auto inserter = std::back_inserter(result);
    for (i32 i = 0; i < rows * cols; ++i)
    {
        fmt::format_to(inserter, "{:2x} ", *(out.grid.atOffset(i)));
        if ((i + 1) % texture.width == 0)
            fmt::format_to(inserter, "\n");
    }
    spdlog::info(result);

    return out;
}
inline bool shouldPause(Application& owner)
{
    auto& options = owner.getSettings();
    auto opt_dict = options.settings;
    if (options.changed_this_tick)
    {
        if (auto entry = opt_dict.find("gfx.pause"); entry && entry->getValueOrDefault<bool>(false))
        {
            return true;
        }
    }
    return false;
}

void PathfinderDemo::init(Application& owner, InitArgs args)
{
    auto* backend = owner.getGraphicsBackend();
    checkLethal(backend->id == GfxBackendID::Webgpu, "unsupported gfx backend");
    this->wgpu_backend = static_cast<WebGpuBackend*>(backend);
    auto& globals = wgpu_backend->getGlobalResources();
    RenderContext ctx = {
        .device = globals.device,
        .encoder = wgpuDeviceCreateCommandEncoder(globals.device, nullptr),
        .queue = globals.queue,
    };

    { // add viewport
        ViewportOptions options{.name = ids::main_vp_name, .size = {400, 600}};
        viewports.add(ctx, options);
        options = {.name = ids::debug_vp_name, .size = {400, 600}};
        viewports.add(ctx, options);
        viewports.imgui_views.back().gui_enabled = false;
    }
    {
        init_data = fromImage();
        heatmap.init(ctx, wgpu_backend->text_shad_lib, init_data.grid.array.constSpan());

        ui.should_config_docking = false;
        ui.console_wnd.name = console_name;

        view_grid.init(ctx, wgpu_backend->text_shad_lib);
        temp_geom.init(ctx, wgpu_backend->text_shad_lib);
        background.init(ctx, wgpu_backend->text_shad_lib);
    }

    owner.input.addTrigger("DEBUG"_trig,
        Trigger{
            .fn_logic =
                [](Trigger& self, const InputState& state)
            {
                const auto code = (u8)(SignalId::KeySpace);
                return state.this_frame[code].state == SignalState::Started;
            },
        },
        true);
    frame_alloc_resource.reset();
}

static void simple_bfs(Application& owner, v2u32 start, MapGrid& grid)
{
    //spdlog::stopwatch sw;
   // defer_{ SPDLOG_WARN("shaders are realoded, it took: {} seconds", sw); };
    vex::InlineBufferAllocator<2096> temp_alloc_resource;
    auto al = temp_alloc_resource.makeAllocatorHandle();

    vex::StaticRing<v2i32, 2024, true> stack;
    vex::Buffer<v2u32> neighbors{al, 64};

    struct HS
    {
        inline static i32 hash(v2u32 key) { return (int)(key.x << 16 | key.y); }
        inline static bool is_equal(v2u32 a, v2u32 b) { return a.x == b.x && a.y == b.y; }
    };
    Dict<v2u32, float, HS> previous{1024};

    stack.push(start);
    u32 iter = 0;
    while (stack.size() > 0)
    {
        auto current = stack.popUnchecked();
        bool added_new = false;

        neighbors.len = 0;
        neighbors.add(current - v2i32{-1, 0});
        neighbors.add(current - v2i32{1, 0});
        neighbors.add(current - v2i32{0, 1});
        neighbors.add(current - v2i32{0, -1});

        for (int i = 0; i < neighbors.len; ++i)
        {
            iter++;
            auto next = neighbors[i];
            u32 tile = *(grid.at(next)) & 0x000000ff;
            bool can_move = tile != 255;

            if (!can_move)
                continue;
            if (next.x >= grid.size.x || next.y >= grid.size.y)
                continue;

            i32 dist_so_far = 1;
            if (previous.contains(current))
            {
                dist_so_far += previous[current];
            }
            if (!previous.contains(next) || previous[next] > dist_so_far)
            {
                added_new = true;
                stack.push(next);
                previous[next] = dist_so_far;
            }
        }
    }

    for (auto& [k, d] : previous)
    {
        grid[k] &= 0x000000ff;
        grid[k] |= (~0x000000ffu) & (((u32)d) << 8);
    }
    owner.input.if_triggered("EscDown"_trig,
        [&](const input::Trigger& self)
        {
            std::string result = "map:\n";
            result.reserve(4096 * 4);
            auto inserter = std::back_inserter(result);
            for (i32 i = 0; i < grid.array.len; ++i)
            {
                fmt::format_to(inserter, "{:2x} ", (((u8)0xffffff00u | *(grid.atOffset(i))) >> 8));
                if ((i + 1) % grid.size.x == 0)
                    fmt::format_to(inserter, "\n");
            }
            spdlog::info(result);
            int guard = 2048;
            return true;
        });
}

void PathfinderDemo::update(Application& owner)
{
    auto& globals = wgpu_backend->getGlobalResources();
    { // skip update if paused, invalid or modal window is shown
        if (viewports.imgui_views.size() < 1)
            return;
        if (shouldPause(owner) || ImGui::IsPopupOpen("Select demo"))
            return;
    }
    { // update viewports
        const v2i32 pos = owner.input.global.mouse_pos_window;
        viewports.update(pos);

        owner.input.if_triggered("DEBUG"_trig,
            [&](const input::Trigger& self)
            {
                spdlog::stopwatch sw;
                defer_ { SPDLOG_WARN("shaders are realoded, it took: {} seconds", sw); };
                background.reloadShaders(wgpu_backend->text_shad_lib, globals.asContext());
                heatmap.reloadShaders(wgpu_backend->text_shad_lib, globals.asContext());
                view_grid.reloadShaders(wgpu_backend->text_shad_lib, globals.asContext());
                // temp_geom.reloadShaders(wgpu_backend->text_shad_lib, globals.asContext());
                return true;
            });
    }
    {
        Viewport& viewport = viewports.imgui_views[0].render_target;
        auto& wgpu_ctx = viewport.setupForDrawing(globals);

        auto camera = BasicCamera::makeOrtho({0.f, 0.f, -4.0}, {12 * 1.333f, 16}, -10, 10);

        camera.aspect = viewport.options.size.x / (float)viewport.options.size.y;
        camera.update();
        auto model_view_proj = camera.mtx.projection * camera.mtx.view;

        const float px_size_x = viewport.options.size.x / camera.orthoSize().x;
        const float px_size_y = viewport.options.size.y / camera.orthoSize().y;
        const float px_in_unit = vex::max(px_size_x, px_size_y);
        auto& time = owner.getTime();

        DrawContext draw_args{
            .camera_mvp = model_view_proj,
            .camera_model_view = camera.mtx.view,
            .camera_projection = camera.mtx.projection,
            .pixel_scale = px_in_unit,
            .delta_time = time.unscalled_dt_f32,
            .time = (float)time.unscaled_runtime,
            .grid_half_size = init_data.grid.size.x / 2.0f,
            .grid_w = 4.0f,
            .camera_h = camera.height,
        };
        { // draw HEATMAP
            auto mpos = camera.normalizedViewToWorld(viewports.imgui_views[0].mouse_pos_normalized);
            float r = init_data.grid.size.y / camera.height;
            v2u32 cell = {mpos.x * r + 16, -mpos.y * r + 16};
            if (init_data.grid.contains(cell))
                simple_bfs(owner, cell, init_data.grid);

            // SPDLOG_WARN(" {} : {}  ", cell.x, cell.y);

            auto int_sz = init_data.grid.size;
            check_(int_sz.x > 0 && int_sz.y > 0);
            heatmap.draw(wgpu_ctx, draw_args,
                HeatmapDynamicData{
                    .buffer = init_data.grid.array.constSpan(),
                    .bounds = {(u32)int_sz.x, (u32)int_sz.y},
                    .color1 = {0.340f, 0.740f, 0.707f, 1.f},
                    .color2 = {0.930f, 0.400f, 0.223f, 1.f},
                });

            background.draw(wgpu_ctx, draw_args);
        }
        { // draw GRID
            view_grid.draw(wgpu_ctx, draw_args);
        }
        {
            DynMeshBuilder<PosNormColor> mesh_data = {frame_alloc, 512, 256};

            vex::Buffer<v3f> lines{frame_alloc, 10};
            lines.addList({
                {0.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f},
                {1.0f, 1.0f, 0.0f},
                {1.0f, 2.0f, 0.0f},
                {2.0f, 2.0f, 0.0f},
                {2.0f, 3.0f, 0.0f},
                {3.0f, 3.0f, 0.0f},
                {3.0f, 4.0f, 0.0f},
                {4.0f, 4.0f, 0.0f},
                {4.0f, 5.0f, 0.0f},
            });

            vex::PolyLine::buildPolyXY(mesh_data, //
                PolyLine::Args{
                    .points = lines.data(),
                    .len = lines.size(),
                    .color = Color::cyan(),
                    .corner_type = PolyLine::Corners::CloseTri,
                    .thickness = 0.1f,
                    .closed = false,
                });
            auto& verts = mesh_data.vertices;
            auto& indices = mesh_data.indices;
            temp_geom.draw(wgpu_ctx, draw_args, indices.constSpan(), verts.constSpan());
        }
        viewport.finishAndSubmit();
        wgpuDeviceTick(wgpu_ctx.device);
    }
}

void PathfinderDemo::drawUI(Application& owner)
{ //
    [[maybe_unused]] ImGuiID main_dockspace = ImGui::DockSpaceOverViewport(nullptr);
    static ImGuiDockNodeFlags dockspace_flags =
        ImGuiDockNodeFlags_NoDockingInCentralNode |
        ImGuiDockNodeFlags_AutoHideTabBar; // ImGuiDockNodeFlags_KeepAliveOnly;

    if (std::exchange(docking_not_configured, false))
    {
        ImGuiID dockspace_id = main_dockspace;
        auto viewport = ImGui::GetMainViewport();
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(
            main_dockspace, ImGuiDir_Down, 0.21f, nullptr, &dockspace_id);
        // ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(
        //     dockspace_id, ImGuiDir_Left, 0.16f, nullptr, &dockspace_id);
        ImGui::DockBuilderDockWindow(ui.console_wnd.name, dock_id_down);
        ImGui::DockBuilderDockWindow(ids::main_vp_name, dockspace_id);
        ImGui::DockBuilderDockWindow(ids::debug_vp_name, dockspace_id);
        ImGui::DockBuilderFinish(main_dockspace);
    }
    ui.drawStandardUI(owner, &viewports);
    // draw base UI
    {
        // ImGui::Begin(ids::main_vp_name);
        // const v2i32 pos = viewports.imgui_views[0].mouse_pos_normalized;
        //
        // auto old = ImGui::GetCursorScreenPos();
        // ImGui::SetCursorScreenPos({(float)pos.x, (float)pos.y});
        // ImGui::Text("   {%f, %f}",pos.x, pos.y);
        // ImGui::SetCursorScreenPos(old);
        // ImGui::End();
    }
    // add items to top bar
    if ((viewports.imgui_views.size() > 1) && ImGui::BeginMainMenuBar())
    {
        defer_ { ImGui::EndMainMenuBar(); };
        if (ImGui::BeginMenu("View"))
        {
            defer_ { ImGui::EndMenu(); };
            ImGui::MenuItem("Debug Viewport", nullptr, &viewports.imgui_views[1].gui_enabled);
            ImGui::Separator();
        }
    }
}
void PathfinderDemo::postFrame(Application& owner)
{
    viewports.postFrame();
    frame_alloc_resource.reset();
}

vex::pf_demo::PathfinderDemo::~PathfinderDemo() { viewports.release(); }
