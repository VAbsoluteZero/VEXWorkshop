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
using namespace vex::flow;
using namespace wgfx;
using namespace std::literals::string_view_literals;
constexpr const char* console_name = "Console##pathfind";

static InitData_v1 fromImage(const char* img = "content/sprites/flow/grid_map32.png")
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

    InitData_v1 out = {.grid = {.size = {texture.width, texture.height}}};

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
    if (auto entry = opt_dict.find("gfx.pause"); entry && entry->value.getValueOr<bool>(false))
    {
        return true;
    }
    return false;
}

vex::flow::FlowfieldPF::~FlowfieldPF()
{
    for (auto& it : defer_till_dtor)
        it();
    viewports.release();
}
void FlowfieldPF::init(Application& owner, InitArgs args)
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
    {
        auto& options = owner.getSettings();
        opt_grid_thickness.addTo(options);
        opt_grid_color.addTo(options);

        defer_till_dtor.emplace_back(
            [&owner]
            {
                auto& options = owner.getSettings();
                opt_grid_thickness.removeFrom(options);
                opt_grid_color.removeFrom(options);
            });
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

static void simpleGridBFS_DictBased(Application& owner, v2u32 start, MapGrid_v1& grid)
{ 
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
    previous[start] = 0;
    while (stack.size() > 0)
    {
        auto current = stack.popUnchecked();
        // bool added_new = false;

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
                // added_new = true;
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
    owner.input.ifTriggered("EscDown"_trig,
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
            return true;
        });
}

void FlowfieldPF::update(Application& owner)
{
    // skip update if paused, invalid or modal window is shown
    if (shouldPause(owner) || viewports.imgui_views.size() < 1)
        return;
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        return;
    Viewport& viewport = viewports.imgui_views[0].render_target;
    if (viewport.options.size.x <= 1 || viewport.options.size.y <= 1)
    {
        return;
    }

    auto& globals = wgpu_backend->getGlobalResources();
    { // update viewports
        const v2i32 pos = owner.input.global.mouse_pos_window;
        viewports.update(pos);

        owner.input.ifTriggered("DEBUG"_trig,
            [&](const input::Trigger& self)
            {
                spdlog::stopwatch sw;
                SPDLOG_INFO(ICON_CI_WARNING "  pppppp pp  ");
                defer_ { SPDLOG_WARN("shaders are realoded, it took: {} seconds", sw); };
                background.reloadShaders(wgpu_backend->text_shad_lib, globals.asContext());
                heatmap.reloadShaders(wgpu_backend->text_shad_lib, globals.asContext());
                view_grid.reloadShaders(wgpu_backend->text_shad_lib, globals.asContext());
                return true;
            });
    }
    {
        auto& wgpu_ctx = viewport.setupForDrawing(globals);

        auto camera = BasicCamera::makeOrtho({0.f, 0.f, -4.0}, {12 * 1.333f, 16}, -10, 10);

        camera.aspect = viewport.options.size.x / (float)viewport.options.size.y;
        camera.update();
        auto model_view_proj = camera.mtx.projection * camera.mtx.view;

        auto& time = owner.getTime();

        DrawContext draw_args{
            .settings = &owner.getSettings(),
            .camera_mvp = model_view_proj,
            .camera_model_view = camera.mtx.view,
            .camera_projection = camera.mtx.projection,
            .viewport_size = viewport.options.size,
            .pixel_size_nm = {0.5f / viewport.options.size.x, 0.5f / viewport.options.size.y},
            .delta_time = time.unscalled_dt_f32,
            .time = (float)time.unscaled_runtime,
            .grid_half_size = init_data.grid.size.x / 2.0f,
            .camera_h = camera.height,
        };
        { // draw HEATMAP & background
            auto mpos = camera.normalizedViewToWorld(viewports.imgui_views[0].mouse_pos_normalized);
            float r = init_data.grid.size.y / camera.height;
            v2u32 cell = {mpos.x * r + 16, -mpos.y * r + 16};
            if (init_data.grid.contains(cell))
                simpleGridBFS_DictBased(owner, cell, init_data.grid);
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
        if (false)
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

void FlowfieldPF::drawUI(Application& owner)
{ //
    ImGui::PushFont(vex::g_view_hub.visuals.fnt_accent);
    defer_ { ImGui::PopFont(); };

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
        ImGui::DockBuilderDockWindow(ui.console_wnd.name, dock_id_down);
        ImGui::DockBuilderDockWindow(ids::main_vp_name, dockspace_id);
        ImGui::DockBuilderDockWindow(ids::debug_vp_name, dockspace_id);
        ImGui::DockBuilderFinish(main_dockspace);
    }
    ui.drawStandardUI(owner, &viewports);
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

    if (ImGui::Begin(ids::main_vp_name))
    {
        ImGui::PushFont(vex::g_view_hub.visuals.fnt_log);
        defer_ { ImGui::PopFont(); };

        auto& options = owner.getSettings();

        if (ImGui::Button(ICON_CI_GRIPPER "##pf_show_options"))
            gui_options.options_shown = !gui_options.options_shown;
        if (gui_options.options_shown)
        {
            bool visible = ImGui::BeginChild("##pf_options", GuiRelVec{0.25, 0}, true);
            defer_ { ImGui::EndChild(); };
            ImGui::Indent(4);
            defer_ { ImGui::Unindent(); };
            for (auto& [k, v] : options.settings)
            {
                if ((SettingsContainer::Flags::k_visible_in_ui | v.flags) == 0)
                    continue;
                if (!k.starts_with("pf.") || k.length() < 4)
                    continue;

                const bool has_min = v.min.hasAnyValue();
                const bool has_max = v.max.hasAnyValue();
                const char* name_adj = k.data() + 3;
                v.value.match(
                    [&, &vv = v](i32& a)
                    {
                        if (has_min && has_max)
                            ImGui::SliderInt(name_adj, &a, vv.min.get<i32>(), vv.max.get<i32>());
                        else
                            ImGui::InputInt(name_adj, &a);
                    },
                    [&, &vv = v](f32& a)
                    {
                        if (has_min && has_max)
                            ImGui::SliderFloat(name_adj, &a, vv.min.get<f32>(), vv.max.get<f32>());
                        else
                            ImGui::InputFloat(name_adj, &a);
                    },
                    [&](bool& a)
                    {
                        ImGui::Checkbox(name_adj, &a);
                    },
                    [&](v4f& a)
                    {
                        ImGui::BeginChild("Color picker");
                        ImGui::TextUnformatted("GridColor");
                        ImGui::SetNextItemWidth(256);
                        ImGui::ColorPicker4("##GridColor", (float*)&a.x);
                        ImGui::EndChild();
                    });
            }
        }
    }
    ImGui::End(); 
}
void FlowfieldPF::postFrame(Application& owner)
{
    viewports.postFrame();
    frame_alloc_resource.reset();
}
