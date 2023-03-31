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
using namespace std::literals::chrono_literals;
constexpr const char* console_name = "Console##pathfind";

void Flow::Map1b::fromImage(Flow::Map1b& out, const char* img)
{
    spdlog::stopwatch sw;
    defer_ { SPDLOG_WARN("Texture loader: loading PNG, elapsed: {} seconds", sw); };

    auto texture = loadImage(img);
    if (!checkAlwaysRel(texture.data, "invalid source texture"))
        return;
    const u32 bytes_per_row = texture.width * texture.channel_count;
    check_(texture.channel_count == 4);
    const u32 rows = texture.height;
    const u32 cols = texture.width;
    u32* data_as_u32 = reinterpret_cast<u32*>(texture.data);

    vex::Buffer<u8>& source = out.source;
    source.reserve(texture.width * texture.height);
    vex::Buffer<u8>& matrix = out.matrix;
    matrix.addZeroed(texture.width * texture.height);
    out.size = {cols, rows};

    out.debug_layer.addZeroed(texture.width * texture.height);

    for (u32 i = 0; i < rows * cols; i++)
    {
        u32 c = *(data_as_u32 + i);
        bool blocked = (c & 0x000000ff) > 200;
        source.add(blocked ? 0 : 1);
    }

    constexpr v2i32 neighbors[8] = {
        {0, -1},  // top (CW sart)
        {1, -1},  // top-right
        {1, 0},   // right
        {1, 1},   // bot-right
        {0, 1},   // bot
        {-1, 1},  // bot-left
        {-1, 0},  // left
        {-1, -1}, // top-left
    };

    for (u32 y = 0; y < rows; y++)
    {
        for (u32 x = 0; x < cols; x++)
        {
            v2i32 cur_xy = {x, y};
            u8* cur_out = matrix.data() + rows * y + x;
            u8 self_mask = *(source.data() + rows * cur_xy.y + cur_xy.x);
            if (self_mask)
            {
                for (u8 i = 0; i < 8; ++i)
                {
                    v2i32 xy = cur_xy + neighbors[i];
                    u8 mask = 1 << i;
                    mask &= [&]()
                    {
                        if (xy.x < 0 || xy.x >= cols || xy.y < 0 || xy.y >= rows)
                            return 0;
                        return *(source.data() + rows * xy.y + xy.x) ? 0xff : 0;
                    }();
                    *cur_out |= mask;
                }
            }
            else
            {
                *cur_out = 0;
            }
            out.debug_layer.at(rows * y + x) = *cur_out;
        }
    }
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
    defer_ { frame_alloc_resource.reset(); };

    auto* backend = owner.getGraphicsBackend();
    checkLethal(backend->id == GfxBackendID::Webgpu, "unsupported gfx backend");
    this->wgpu_backend = static_cast<WebGpuBackend*>(backend);
    auto& globals = wgpu_backend->getGlobalResources();
    GpuContext ctx = {
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
    // init webgpu stuff
    {
        Flow::Map1b::fromImage(init_data, "content/sprites/flow/grid_map32.png");
        processed_map.data.reserve(init_data.size.x * init_data.size.y);
        for (u8 c : init_data.source)
            processed_map.data.add(c ? 0 : ~ProcessedData::dist_mask);

        heatmap.init(ctx, wgpu_backend->text_shad_lib, processed_map.data.constSpan(),
            "content/shaders/wgsl/cell_heatmap.wgsl");
        debug_overlay.init(ctx, wgpu_backend->text_shad_lib, processed_map.data.constSpan(),
            "content/shaders/wgsl/cell_debugmap.wgsl");

        ui.should_config_docking = false;
        ui.console_wnd.name = console_name;

        view_grid.init(ctx, wgpu_backend->text_shad_lib);
        temp_geom.init(ctx, wgpu_backend->text_shad_lib);
        background.init(ctx, wgpu_backend->text_shad_lib);

        compute_pass.init(ctx, wgpu_backend->text_shad_lib,
            "content/shaders/wgsl/flow/flowfield_conv.wgsl", heatmap.storage_buf, init_data.size);
        flow_overlay.init(ctx, wgpu_backend->text_shad_lib, compute_pass.output_buf,
            "content/shaders/wgsl/flow/flowfield_overlay.wgsl");

        part_sys.init(ctx, wgpu_backend->text_shad_lib,
            ParticleSym::InitArgs{
                .flow_v2f_buf = &compute_pass.output_buf,
                .cells_buf = &heatmap.storage_buf
            });
    }
    // init console variables
    {
        auto& options = owner.getSettings();
        opt_grid_thickness.addTo(options);
        opt_grid_color.addTo(options);
        opt_show_dbg_overlay.addTo(options);
        opt_allow_diagonal.addTo(options);
        opt_show_numbers.addTo(options);
        opt_smooth_flow.addTo(options);
        opt_wallbias_numbers.addTo(options);
        opt_show_ff_overlay.addTo(options);

        defer_till_dtor.emplace_back(
            [&owner]
            {
                auto& options = owner.getSettings();
                opt_grid_thickness.removeFrom(options);
                opt_grid_color.removeFrom(options);
                opt_show_dbg_overlay.removeFrom(options);
                opt_allow_diagonal.removeFrom(options);
                opt_show_numbers.removeFrom(options);

                opt_smooth_flow.removeFrom(options);
                opt_wallbias_numbers.removeFrom(options);
                opt_show_ff_overlay.removeFrom(options);
            });
    }
    // add input hooks
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
}


void FlowfieldPF::trySpawningParticlesAtLocation(const wgfx::GpuContext& ctx, SpawnArgs args)
{
    struct
    {
        vex::Dict<u32, u32> near;
        i32 max_len = -1;
        u32 max_dist = 0;
        u32 cur_max_dist = 0;
        void init(v2u32 size, ROSpan<u8> map) {}
        FORCE_INLINE void setCellDist(u32 cell, u32 dist)
        {
            cur_max_dist = cur_max_dist < dist ? dist : cur_max_dist;
            near.emplace(cell, dist);
        }
        FORCE_INLINE auto getCellDist(u32 cell) { return near[cell]; }
        FORCE_INLINE bool shouldTerminate() const
        {
            return (max_len > 0 && near.size() >= max_len) || //
                   (max_dist >= cur_max_dist);
        }
    } client{
        .near = {32, frame_alloc},
        .max_len = 37,
    };

    using Part = ParticleSym::Particle;
    constexpr i32 num_particles = 16000;

    Flow::gridSyncBFSWithClient<decltype(client), false>({args.cell}, init_data, client);

    const u32 client_len = (u32)client.near.size();
    vex::Buffer<v2f> cells = {frame_alloc, (i32)client_len};
    const float sz = map_area.cell_size.x;
    const v2f orig = map_area.top_left;

    auto cell_cnt_xy = init_data.size;
    for (auto [k, v] : client.near)
    {
        v2i32 cell_xy{k % cell_cnt_xy.x, k / cell_cnt_xy.x};
        cells.add(orig + v2f(cell_xy.x * sz, -cell_xy.y * sz - sz));
    }

    vex::Buffer<Part> particles = {frame_alloc, num_particles};
    particles.addUninitialized(num_particles);

    for (auto i = 0; i < num_particles; ++i)
    {
        v2f rand_cell = cells[rng::Rand::randMod(client_len)];
        volatile float rnd_x = rng::Rand::randFloat01() * sz;
        volatile float rnd_y = rng::Rand::randFloat01() * sz;
        particles[i].pos = rand_cell + v2f{rnd_x, rnd_y};
        particles[i].vel = v2f_zero;
    }
    part_sys.spawnForSymulation(ctx, particles.constSpan());
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

    if (init_data.contains(goal_cell) && !init_data.isBlocked(goal_cell))
    {
        spdlog::stopwatch sw;
        defer_ { bfs_search_dur_ms = sw.elapsed() / 1ms; };
        if (owner.getSettings().valueOr(opt_allow_diagonal.key_name, true))
            Flow::gridSyncBFS<true>({goal_cell}, init_data, processed_map);
        else
            Flow::gridSyncBFS<false>({goal_cell}, init_data, processed_map);
    }

    // #fixme - movable camera
    auto camera = BasicCamera::makeOrtho({0.f, 0.f, -4.0}, {12 * 1.333f, 16}, -10, 10);
    camera.aspect = viewport.options.size.x / (float)viewport.options.size.y;
    camera.update();

    const v2i32 pos = owner.input.global.mouse_pos_window;
    viewports.updateMouseLoc(pos);

    auto& globals = wgpu_backend->getGlobalResources();
    { // process input
        owner.input.ifTriggered("DEBUG"_trig,
            [&](const input::Trigger& self)
            {
                spdlog::stopwatch sw;
                defer_
                {
                    SPDLOG_WARN(ICON_CI_WARNING "shaders are reloaded, it took: {} seconds", sw);
                };
                auto gctx = globals.asContext();
                background.reloadShaders(wgpu_backend->text_shad_lib, gctx);
                heatmap.reloadShaders(wgpu_backend->text_shad_lib, gctx);
                debug_overlay.reloadShaders(wgpu_backend->text_shad_lib, gctx);
                view_grid.reloadShaders(wgpu_backend->text_shad_lib, gctx);
                flow_overlay.reloadShaders(wgpu_backend->text_shad_lib, gctx);
                compute_pass.reloadShaders(wgpu_backend->text_shad_lib, gctx);
                part_sys.reloadShaders(wgpu_backend->text_shad_lib, gctx);
                return true;
            });

        auto mpos = camera.normalizedViewToWorld(viewports.imgui_views[0].mouse_pos_normalized);

        float r = init_data.size.y / camera.height;
        float cell_w = camera.height / init_data.size.y;
        // #fixme rewrite this mess to properly set up size indep. from camera
        map_area.top_left = v2f{-camera.height, camera.height} * 0.5f;
        map_area.bot_left = v2f{-camera.height, -camera.height} * 0.5f;
        map_area.cell_size = {cell_w, cell_w};

        v2u32 m_cell = {mpos.x * r + init_data.size.x / 2, -mpos.y * r + init_data.size.y / 2};
        owner.input.ifTriggered("MouseRightHeld"_trig,
            [&](const input::Trigger& self)
            {
                if (init_data.contains(m_cell) && !init_data.isBlocked(m_cell))
                    goal_cell = m_cell;
                return true;
            });
        owner.input.ifTriggered("MouseLeftDown"_trig,
            [&](const input::Trigger& self)
            {
                if (init_data.contains(m_cell) && !init_data.isBlocked(m_cell))
                {
                    trySpawningParticlesAtLocation(
                        globals.asContext(), {.cell = m_cell, .world_pos = mpos});
                }
                return true;
            });
    }
    {
        auto& wgpu_ctx = viewport.setupForDrawing(globals);
        auto model_view_proj = camera.mtx.projection * camera.mtx.view;
        auto& time = owner.getTime();
        auto int_sz = init_data.size;

        DrawContext draw_args{
            .settings = &owner.getSettings(),
            .camera_mvp = model_view_proj,
            .camera_model_view = camera.mtx.view,
            .camera_projection = camera.mtx.projection,
            .viewport_size = viewport.options.size,
            .pixel_size_nm = {0.5f / viewport.options.size.x, 0.5f / viewport.options.size.y},
            .delta_time = time.unscalled_dt_f32,
            .time = (float)time.unscaled_runtime,
            .grid_half_size = init_data.size.x / 2.0f,
            .camera_h = camera.height,
        };
        { // draw HEATMAP & background

            // heatmap WRITES to storage buffer that contains search result
            // #fixme - restructure whole thing so buffers and layers are separated
            heatmap.draw(wgpu_ctx, draw_args,
                HeatmapDynamicData{
                    .buffer = processed_map.data.constSpan(),
                    .bounds = {(u32)int_sz.x, (u32)int_sz.y},
                    .color1 = {0.340f, 0.740f, 0.707f, 1.f},
                    .color2 = {0.930f, 0.400f, 0.223f, 1.f},
                });
        }
        { // compute pass
            auto encoder = wgpuDeviceCreateCommandEncoder(wgpu_ctx.device, nullptr);

            CompContext compute_ctx{
                .device = wgpu_ctx.device,
                .encoder = encoder,
                .queue = wgpu_ctx.queue,
                .comp_pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr),
            };
            auto submit_cmp = [](CompContext& ctx) -> void
            {
                WGPUCommandBufferDescriptor ctx_desc_fin{nullptr, "compute submit"};
                auto cmd_buf = wgpuCommandEncoderFinish(ctx.encoder, &ctx_desc_fin);
                wgpuQueueSubmit(ctx.queue, 1, &cmd_buf);
                wgpuDeviceTick(ctx.device);
            };

            u32 flags_comp = draw_args.settings->valueOr(opt_wallbias_numbers.key_name, false) //
                             | (2 * draw_args.settings->valueOr(opt_smooth_flow.key_name, false));
            compute_pass.compute(compute_ctx, ComputeArgs{
                                                  .map_size = int_sz,
                                                  .flags = flags_comp,
                                              });

            submit_cmp(compute_ctx);

            compute_ctx.encoder = wgpuDeviceCreateCommandEncoder(wgpu_ctx.device, nullptr);
            compute_ctx.comp_pass = wgpuCommandEncoderBeginComputePass(
                compute_ctx.encoder, nullptr);
            part_sys.compute(compute_ctx, ParticleSym::CompArgs{
                                              .bounds = int_sz,
                                              .grid_min = map_area.bot_left,
                                              .grid_size = {camera.height, camera.height},
                                              .cell_size = map_area.cell_size,
                                              .time = (float)time.unscaled_runtime,
                                              .dt = time.delta_time_f32,
                                          });
            submit_cmp(compute_ctx);
        }
        { // overlays
            if (draw_args.settings->valueOr(opt_show_dbg_overlay.key_name, false))
            {
                debug_overlay.draw(wgpu_ctx, draw_args,
                    HeatmapDynamicData{
                        .buffer = init_data.debug_layer.constSpan(),
                        .bounds = {(u32)int_sz.x, (u32)int_sz.y},
                        .color1 = {0.340f, 0.740f, 0.707f, 1.f},
                        .color2 = {0.930f, 0.400f, 0.223f, 1.f},
                    });
            }
            if (draw_args.settings->valueOr(opt_show_ff_overlay.key_name, false))
            {
                flow_overlay.draw(wgpu_ctx, draw_args, {.bounds = int_sz});
            }

            background.draw(wgpu_ctx, draw_args);
            view_grid.draw(wgpu_ctx, draw_args);
        }
        // particle system - draw
        part_sys.draw(wgpu_ctx, draw_args, {.bounds = init_data.size});

        // SUBMIT
        viewport.finishAndSubmit();
        wgpuDeviceTick(wgpu_ctx.device);
    }
}

void FlowfieldPF::drawUI(Application& owner)
{ //
    ImGui::PushFont(vex::g_view_hub.visuals.fnt_accent);
    defer_ { ImGui::PopFont(); };

    // setup dockspaces
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

    if (ImGui::BeginMainMenuBar())
    {
        defer_ { ImGui::EndMainMenuBar(); };
        ImGui::Bullet();
        float bfs_time = (float)bfs_search_dur_ms;
        ImGui::Text(" bfs: %.3f ms", bfs_search_dur_ms);
    }

    auto& options = owner.getSettings();
    if (ImGui::Begin(ids::main_vp_name))
    {
        if (options.valueOr(opt_show_numbers.key_name, false) && processed_map.size.x > 0 &&
            processed_map.size.x < 65) // messy on large map. #fixme test for visible size instead
        {
            const ImColor color = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
            ImDrawList* dlist = ImGui::GetWindowDrawList();
            i32 pos = 0;
            ImVec2 min_xy = ImGui::GetWindowContentRegionMin();
            auto vp_size = viewports.imgui_views[0].last_seen_size;
            v2f center = vp_size / 2;
            center.x += min_xy.x + 2;
            center.y += min_xy.y + 12;

            f32 cell_w = (vp_size.y / 2.0f) / (processed_map.size.y / 2.0f);
            v2f orig = center - v2f{vp_size.y / 2 - cell_w / 2, vp_size.y / 2 - cell_w / 2};
            for (auto it : processed_map.data)
            {
                auto dist = it & processed_map.dist_mask;
                auto cell = v2i32{pos % processed_map.size.x, pos / processed_map.size.x};
                ImVec2 spos = {(orig.x + cell.x * cell_w), (orig.y + cell.y * cell_w)};
                std::string buff = fmt::format("{}", dist);
                dlist->AddText(vex::g_view_hub.visuals.fnt_tiny, ImGui::GetFontSize() * 0.8, spos,
                    color, buff.data());
                pos++;
            }
        }

        // move diagonally toggle
        auto is_diag_move_allowed = options.settings.find(opt_allow_diagonal.key_name);
        if (is_diag_move_allowed && is_diag_move_allowed->value.getValueOr<bool>(true))
        {
            if (ImGui::Button(ICON_CI_DIFF_ADDED " disable diagonal movement##pf_hide_overlay"))
                is_diag_move_allowed->value.set<bool>(false);
        }
        else if (is_diag_move_allowed)
        {
            if (ImGui::Button(ICON_CI_DIFF_IGNORED " enable diagonal movement##pf_show_overlay"))
                is_diag_move_allowed->value.set<bool>(true);
        }
        // flow toggle
        auto is_ff_overlay_active = options.settings.find(opt_show_ff_overlay.key_name);
        if (is_ff_overlay_active && is_ff_overlay_active->value.getValueOr<bool>(false))
        {
            if (ImGui::Button(ICON_CI_DIFF_MODIFIED " disable fields overlay##pf_hide_overlay"))
                is_ff_overlay_active->value.set<bool>(false);
        }
        else if (is_ff_overlay_active)
        {
            if (ImGui::Button(ICON_CI_DIFF_RENAMED " enable fields overlay##pf_show_overlay"))
                is_ff_overlay_active->value.set<bool>(true);
        }
        // neighbors toggle
        auto should_show_overlay_opt = options.settings.find(opt_show_dbg_overlay.key_name);
        if (should_show_overlay_opt && should_show_overlay_opt->value.getValueOr<bool>(false))
        {
            if (ImGui::Button(ICON_CI_DEBUG " debug overlay##pf_hide_overlay"))
                should_show_overlay_opt->value.set<bool>(false);
        }
        else if (should_show_overlay_opt)
        {
            if (ImGui::Button(ICON_CI_DEBUG " debug overlay##pf_show_overlay"))
                should_show_overlay_opt->value.set<bool>(true);
        }

        if (ImGui::Button(ICON_CI_SETTINGS " settings##pf_show_options"))
            gui_options.options_shown = !gui_options.options_shown;
        if (gui_options.options_shown)
        {
            ImGui::PushFont(vex::g_view_hub.visuals.fnt_log);
            defer_ { ImGui::PopFont(); };
            [[maybe_unused]] bool visible = ImGui::BeginChild(
                "##pf_options", GuiRelVec{0.25, 0}, false);
            defer_ { ImGui::EndChild(); };
            ImGui::Indent(4);
            defer_ { ImGui::Unindent(); };
            for (auto& [k, v] : options.settings)
            {
                if ((SettingsContainer::Flags::k_visible_in_ui & v.flags) == 0)
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
                        // ImGui::BeginChild("Color picker");
                        ImGui::TextUnformatted("GridColor");
                        ImGui::SetNextItemWidth(256);
                        ImGui::ColorPicker4("##GridColor", (float*)&a.x);
                        // ImGui::EndChild();
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
