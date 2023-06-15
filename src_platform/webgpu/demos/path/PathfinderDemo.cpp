#include "PathfinderDemo.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/stopwatch.h>
#include <utils/CLI.h>
#include <utils/ImGuiUtils.h>

#include <Tracy.hpp>
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
    ZoneScopedN("create map from file");
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
    ZoneScopedN("FlowfieldPF::Init");
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
#if VEX_PF_StressPreset
        Flow::Map1b::fromImage(init_data, "content/sprites/flow/grid_map128.png");
#elif VEX_PF_WebDemo
        Flow::Map1b::fromImage(init_data, "content/sprites/flow/grid_map64.png");
#else
        Flow::Map1b::fromImage(init_data, "content/sprites/flow/grid_map64.png");
#endif

        ZoneScopedN("create wgpu pipelines");
        processed_map.size = {init_data.size.x, init_data.size.y};
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
            compute_pass.subdiv_buf, "content/shaders/wgsl/flow/flowfield_overlay.wgsl");

        part_sys.init(ctx, wgpu_backend->text_shad_lib,
            ParticleSym::InitArgs{
                .flow_v2f_buf = &compute_pass.output_buf,
                .flow_sub_v2f_buf = &compute_pass.subdiv_buf,
                .cells_buf = &heatmap.storage_buf,
                .max_particles = max_particles,
                .bounds = init_data.size,
            });
    }

    // init console variables
    {
        ZoneScopedN("Init options");
        auto& options = owner.getSettings();
        opt_grid_thickness.addTo(options);
        opt_grid_color.addTo(options);
        opt_heatmap_opacity.addTo(options);

        opt_show_dbg_overlay.addTo(options);
        opt_allow_diagonal.addTo(options);
        opt_show_numbers.addTo(options);
        // opt_smooth_flow.addTo(options);
        opt_wallbias_numbers.addTo(options);
        opt_show_ff_overlay.addTo(options);

        opt_part_auto_color.addTo(options);
        opt_part_color.addTo(options);

        opt_part_speed.addTo(options);
        opt_part_speed_max.addTo(options);
        opt_part_inertia.addTo(options);
        opt_part_radius.addTo(options);
        opt_part_drag.addTo(options);
        opt_part_sep.addTo(options);

        defer_till_dtor.emplace_back(
            [&owner]
            {
                auto& options = owner.getSettings();
                opt_grid_thickness.removeFrom(options);
                opt_grid_color.removeFrom(options);
                opt_heatmap_opacity.removeFrom(options);

                opt_show_dbg_overlay.removeFrom(options);
                opt_allow_diagonal.removeFrom(options);
                opt_show_numbers.removeFrom(options);

                // opt_smooth_flow.removeFrom(options);
                opt_wallbias_numbers.removeFrom(options);
                opt_show_ff_overlay.removeFrom(options);

                opt_part_auto_color.removeFrom(options);
                opt_part_color.removeFrom(options);

                opt_part_speed.removeFrom(options);
                opt_part_speed_max.removeFrom(options);
                opt_part_inertia.removeFrom(options);
                opt_part_radius.removeFrom(options);
                opt_part_drag.removeFrom(options);
                opt_part_sep.removeFrom(options);
            });
    }
    // init ccamera
    const auto cam_h = default_cell_w * init_data.size.y;
    map_area.unscalled_camera_h = cam_h;
    map_area.camera = BasicCamera::makeOrtho({0.f, 0.f, -4.0}, // pos
        {cam_h * 1.333f, cam_h},                               // size
        -10, 10);                                              // near/far

    // add input hooks
#if VEX_PF_WebDemo == 0
    owner.input.addTrigger("ShaderReload"_trig,
        Trigger{
            .fn_logic =
                [](Trigger& self, const InputState& state)
            {
                const auto code = (u8)(SignalId::KeySpace);
                return state.this_frame[code].state == SignalState::Started;
            },
        },
        true);
#endif
}

struct NearSearchClient
{
    vex::Dict<u32, u32> near;
    i32 max_len = -1;
    u32 max_dist = 0x0fff;
    u32 start_dist = 1;
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
               (max_dist <= cur_max_dist);
    }
    FORCE_INLINE u32 startDist() const { return start_dist; }
};

void FlowfieldPF::trySpawningParticlesAtLocation(const wgfx::GpuContext& ctx, SpawnLocation args)
{
    ZoneScopedN("Spawning particles");
    if ((num_particles == max_particles) && !spawn_args.reset)
        return;
    if (init_data.isBlocked(goal_cell))
        return;

    NearSearchClient client{
        .near = {(u32)(init_data.size.y * init_data.size.x / 2), frame_alloc},
        .max_dist = (u32)spawn_args.radius,
    };

    const float sz = map_area.cell_size.x;
    const v2f orig = map_area.top_left;
    using Part = ParticleSym::Particle;
    //    const i32 max_per_cell = 1.5f / map_area.part_radius;

    const i32 num_to_spawn = glm::clamp(spawn_args.count, 0, max_particles);

    SPDLOG_INFO("spawning {} particles", num_to_spawn);
    {
        ZoneScopedN("BFS with client");
        Flow::gridSyncBFSWithClient<decltype(client), false>({args.cell}, init_data, client);
    }
    const u32 client_len = (u32)client.near.size();
    vex::Buffer<v2f> cells = {frame_alloc, (i32)client_len};

    auto cell_cnt_xy = init_data.size;
    for (auto [k, v] : client.near)
    {
        v2i32 cell_xy{k % cell_cnt_xy.x, k / cell_cnt_xy.x};
        cells.add(orig + v2f(cell_xy.x * sz, -cell_xy.y * sz - sz));
    }

    vex::Buffer<Part> particles = {frame_alloc, num_to_spawn};
    particles.reserve(num_to_spawn);

    if (spawn_args.reset)
    {
        num_particles = 0;
    }
    // place particles
    {
        ZoneScopedN("Place particles randomly");
        const float padding = map_area.part_radius * sz * 1.01f;
        for (auto i = 0; i < num_to_spawn; ++i)
        {
            if (num_particles + i >= max_particles)
                break;

            float f_idx = i / ((float)num_to_spawn);
            auto entry_idx = (i32)(glm::floor(f_idx * client_len));
            v2f cell_idx = cells[entry_idx];
            volatile float rnd_x = padding + rng::Rand::randFloat01() * (sz - padding * 2);
            volatile float rnd_y = padding + rng::Rand::randFloat01() * (sz - padding * 2);
            Part particle = {
                .pos = cell_idx + v2f{rnd_x, rnd_y},
                .vel = v2f_zero,
            };
            particles.add(particle);
        }
    }

    if (spawn_args.reset)
    {
        ZoneScopedN("copy particles to gpu");
        num_particles = num_to_spawn;
        part_sys.resetParticleBuffer(ctx, particles.constSpan());
    }
    else
    {
        ZoneScopedN("copy particles to gpu");
        part_sys.spawnForSimulation(ctx, particles.constSpan());
        num_particles += particles.len;
    }
}

void FlowfieldPF::update(Application& owner)
{
    ZoneScopedN("FlowfieldPF::Update");
    // skip update if paused, invalid or modal window is shown
    if (shouldPause(owner) || viewports.imgui_views.size() < 1)
        return;
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
        return;

    auto& vp_wrapper = viewports.imgui_views[0];
    Viewport& viewport = vp_wrapper.render_target;
    if (viewport.options.size.x <= 1 || viewport.options.size.y <= 1)
    {
        return;
    }

    const auto& time = owner.getTime();
    auto& options = owner.getSettings();
    if (init_data.contains(goal_cell) && !init_data.isBlocked(goal_cell))
    {
        spdlog::stopwatch sw;
        defer_ { bfs_search_dur_ms = sw.elapsed() / 1ms; };
        ZoneNamedN(main_bfs, "BFS", true);
        if (options.valueOr(opt_allow_diagonal.key_name, true))
            Flow::gridSyncBFS<true>({goal_cell}, init_data, processed_map);
        else
            Flow::gridSyncBFS<false>({goal_cell}, init_data, processed_map);

        ZoneNamedN(sub_bfs, "process sub goals", true);
        NearSearchClient client{
            .near = {32, frame_alloc},
            .max_len = (i32)(init_data.size.y * 3),
            .max_dist = 6,
        };
        for (auto it : sub_goals)
        {
            auto s_cell = it;
            auto d_old = processed_map[s_cell];
            if (d_old < 3)
                continue;
            client.start_dist = 2;
            client.max_dist = client.start_dist + 6;
            Flow::gridSyncBFSWithClient<decltype(client), false>({it}, init_data, client);
            for (auto [n_cell, n_dist] : client.near)
            {
                if (processed_map[n_cell] > n_dist)
                {
                    auto v = processed_map[n_cell] + (((i32)client.max_dist + 1) - ((i32)n_dist));
                    processed_map[n_cell] = (v > 1) ? v : 1;
                }
            }

            client.near.clear();
            client.cur_max_dist = 0;
        }
    }
    // #fixme - movable camera
    auto& camera = map_area.camera;
    camera.aspect = viewport.options.size.x / (float)viewport.options.size.y;
    // map_area.camera_scale = glm::mix(
    //     map_area.camera_scale, map_area.camera_scale_target, time.unscalled_dt_f32 * 20.0f);
    // camera.height = map_area.unscalled_camera_h * map_area.camera_scale;
    camera.update();

    map_area.part_radius = options.valueOr(
        opt_part_radius.key_name, ParticleSym::default_rel_radius);

    auto& globals = wgpu_backend->getGlobalResources();

    const v2i32 pos = owner.input.global.mouse_pos_window;
    viewports.updateMouseLoc(pos);
    bool has_focus = vp_wrapper.mouse_over && vp_wrapper.gui_visible;
    if (has_focus)
    { // process input
#if VEX_PF_WebDemo == 0
        owner.input.ifTriggered("ShaderReload"_trig,
            [&](const input::Trigger& self)
            {
                ZoneScopedN("Shader Reload");
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
#endif
        auto mpos = camera.normalizedViewToWorld(viewports.imgui_views[0].mouse_pos_normalized);

        float r = init_data.size.y / camera.height;
        float cell_w = camera.height / init_data.size.y;
        // #fixme rewrite this mess to properly set up size indep. from camera
        map_area.top_left = v2f{-camera.height, camera.height} * 0.5f;
        map_area.bot_left = v2f{-camera.height, -camera.height} * 0.5f;
        map_area.cell_size = {default_cell_w, default_cell_w};

        v2i32 m_cell = {mpos.x * r + init_data.size.x / 2, -mpos.y * r + init_data.size.y / 2};

        owner.input.ifTriggered("MouseRightDown"_trig,
            [&](const input::Trigger& self)
            {
                auto trig = self.input_data.find(vex::input::SignalId::KeyModCtrl);
                auto ctrl_down = trig && trig->isActive();
                bool valid = init_data.contains(m_cell) && !init_data.isBlocked(m_cell);
                if (ctrl_down && valid)
                    sub_goals.push(m_cell);
                else if (ctrl_down)
                    sub_goals.popDiscard();

                return true;
            });
        owner.input.ifTriggered("MouseRightHeld"_trig,
            [&](const input::Trigger& self)
            {
                auto trig = self.input_data.find(vex::input::SignalId::KeyModCtrl);
                auto ctrl_down = trig && trig->isActive();
                bool valid = !init_data.isBlocked(m_cell);
                if (!ctrl_down && valid)
                    goal_cell = m_cell;

                return true;
            });
        owner.input.ifTriggered("MouseLeftDown"_trig,
            [&](const input::Trigger& self)
            {
                if (!init_data.isBlocked(m_cell))
                {
                    trySpawningParticlesAtLocation(
                        globals.asContext(), {.cell = m_cell, .world_pos = mpos});
                }
                return true;
            });
        owner.input.ifTriggered("MouseMidMove"_trig,
            [&](const input::Trigger& self)
            {
                v3f world_delta = camera.viewportToCamera(
                    vp_wrapper.pixelScaleToNormalizedView(owner.input.global.mouse_delta));
                camera.pos = camera.pos + v3f(world_delta.x, world_delta.y, 0.0f);
                const float clamp_v = map_area.cell_size.x * processed_map.size.x * 0.5f;
                camera.pos.x = glm::clamp(camera.pos.x, -clamp_v, clamp_v);
                camera.pos.y = glm::clamp(camera.pos.y, -clamp_v, clamp_v);
                return true;
            });
        // owner.input.ifTriggered("MouseMidScroll"_trig,
        //     [&](const input::Trigger& self)
        //     {
        //         auto input_data = self.input_data.find(vex::input::SignalId::MouseScroll);
        //         if (!input_data)
        //             return false;
        //         float delta = -input_data->value_raw.y * 0.03f;
        //         map_area.camera_scale_target = glm::clamp(map_area.camera_scale_target + delta,
        //             camera_scale_clamp_min, camera_scale_clamp_max);
        //         return true;
        //     });
    }
    { 
        ZoneScopedN("Wgpu draw and compute");
        auto& wgpu_ctx = viewport.setupForDrawing(globals);

        auto model_view_proj = camera.mtx.projection * camera.mtx.view;
        auto int_sz = init_data.size;

        DrawContext draw_args{.settings = &options,
            .camera_mvp = model_view_proj,
            .camera_model_view = camera.mtx.view,
            .camera_projection = camera.mtx.projection,
            .viewport_size = viewport.options.size,
            .pixel_size_nm = {0.5f / viewport.options.size.x, 0.5f / viewport.options.size.y},
            .delta_time = time.unscalled_dt_f32,
            .time = (float)time.unscaled_runtime,
            .grid_half_size = init_data.size.x / 2.0f,
            .camera_h = camera.height,
            .cell_sz = map_area.cell_size.x};
        { // draw HEATMAP & background 
            ZoneScopedN("Draw heatmap");
            // heatmap WRITES to storage buffer that contains search result
            // #fixme - restructure whole thing so buffers and layers are separated
            heatmap.draw(wgpu_ctx, draw_args,
                HeatmapDynamicData{
                    .buffer = processed_map.data.constSpan(),
                    .bounds = {(u32)int_sz.x, (u32)int_sz.y},
                    .shrink_x_to_y = {0.930f, 0.400f, 0.0f, 1.0f}, // shrink x_to_y
                    .opacity = options.valueOr(opt_heatmap_opacity.key_name, 1.0f),
                });
        }
        { // compute pass
            ZoneScopedN("Compute fields");
            wgpuDeviceTick(wgpu_ctx.device);
            auto encoder = wgpuDeviceCreateCommandEncoder(wgpu_ctx.device, nullptr);

            CompContext compute_ctx{
                .device = wgpu_ctx.device,
                .encoder = encoder,
                .queue = wgpu_ctx.queue,
                .comp_pass = wgpuCommandEncoderBeginComputePass(encoder, nullptr),
            };
            auto submit_cmds_fn = [](CompContext& ctx) -> void
            {
                WGPUCommandBufferDescriptor ctx_desc_fin{nullptr, "compute submit"};
                auto cmd_buf = wgpuCommandEncoderFinish(ctx.encoder, &ctx_desc_fin);
                wgpuQueueSubmit(ctx.queue, 1, &cmd_buf);
                wgpuDeviceTick(ctx.device);
            };

            u32 flags_comp = draw_args.settings->valueOr(opt_wallbias_numbers.key_name, false); //
            /* | (2 * draw_args.settings->valueOr(opt_smooth_flow.key_name, false));*/
            compute_pass.compute(compute_ctx, ComputeArgs{
                                                  .map_size = int_sz,
                                                  .flags = flags_comp,
                                              });

            submit_cmds_fn(compute_ctx);

            compute_ctx.encoder = wgpuDeviceCreateCommandEncoder(wgpu_ctx.device, nullptr);
            compute_ctx.comp_pass = wgpuCommandEncoderBeginComputePass(
                compute_ctx.encoder, nullptr);
            part_sys.compute(compute_ctx, ParticleSym::CompArgs{
                                              .settings = &options,
                                              .bounds = int_sz,
                                              .grid_min = map_area.bot_left,
                                              .grid_size = {camera.height, camera.height},
                                              .cell_size = map_area.cell_size,
                                              .time = (float)time.unscaled_runtime,
                                              .dt = time.delta_time_f32,
                                          });
            submit_cmds_fn(compute_ctx);
        }

        { // overlays 
            ZoneScopedN("Draw overlays");
            if (draw_args.settings->valueOr(opt_show_dbg_overlay.key_name, false))
            {
                debug_overlay.draw(wgpu_ctx, draw_args,
                    HeatmapDynamicData{
                        .buffer = init_data.debug_layer.constSpan(),
                        .bounds = {(u32)int_sz.x, (u32)int_sz.y},
                        .shrink_x_to_y = {1.00f, 0.30f, 0.223f, 1.f},
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
        part_sys.draw(wgpu_ctx, draw_args,
            {
                .settings = &options,
                .bounds = init_data.size,
                .cell_size = map_area.cell_size,
                .num_particles = (u32)num_particles,
            });

        // SUBMIT 
        { 
            ZoneScopedN("Submit commands and Tick");
            viewport.finishAndSubmit();
            wgpuDeviceTick(wgpu_ctx.device);
        }
    }
}

void FlowfieldPF::drawUI(Application& owner)
{ //
    ZoneScopedN("Draw FlowfieldPF UI");
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
#if VEX_PF_WebDemo == 1
        ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(
            main_dockspace, ImGuiDir_Right, 0.31f, nullptr, &dockspace_id);
        ImGui::DockBuilderDockWindow(ui.console_wnd.name, dock_id_down);
#endif
        ImGui::DockBuilderDockWindow(ids::main_vp_name, dockspace_id);
        ImGui::DockBuilderDockWindow(ids::debug_vp_name, dockspace_id);
        ImGui::DockBuilderFinish(main_dockspace);
    }
#if VEX_PF_WebDemo == 1
    ui.console_wnd.is_open = false;
#endif
    {
        ZoneScopedN("Draw Standard UI");
        ui.drawStandardUI(owner, &viewports);
    }

    if (ImGui::BeginMainMenuBar())
    {
        defer_ { ImGui::EndMainMenuBar(); };
        ImGui::Bullet();
        float bfs_time = (float)bfs_search_dur_ms;
        ImGui::Text(" bfs: %.3f ms", bfs_search_dur_ms);
    }

    const auto vp_size = viewports.imgui_views[0].last_seen_size;
    auto& options = owner.getSettings();

    defer_ { ImGui::End(); }; // close window region
    if (ImGui::Begin(ids::main_vp_name))
    { 
        ZoneScopedN("Draw MainViewport UI");
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
            ImGui::PushFont(vex::g_view_hub.visuals.fnt_tiny);
            ImGui::Indent(4);
            defer_
            {
                ImGui::PopFont();
                ImGui::PopStyleColor();
                ImGui::Unindent(4);
            };
            ImVec2 side_panel_size = {250, 200};
            [[maybe_unused]] bool visible = ImGui::BeginChild(
                "##pf_spawn_options", side_panel_size, false);
            defer_ { ImGui::EndChild(); };
            ImGui::Text("Particle spawn settings:");
            ImGui::Text("count per click:        ");
            ImGui::DragInt("##num_ps_spawn-num", &spawn_args.count, 10, 1, max_particles / 2);

            ImGui::Text("radius (in valid cells):");
            ImGui::SliderInt("##num_ps_spawn-area", &spawn_args.radius, 2, init_data.size.x / 2);

            ImGui::Checkbox("clear others on spawn", &spawn_args.reset);
        }
        ImGui::NewLine();
        if (options.valueOr(opt_show_numbers.key_name, false) && processed_map.size.x > 0 &&
            processed_map.size.x < 65) // messy on large map. #fixme test for visible size instead
        {
            const ImColor color = ImColor(1.0f, 1.0f, 1.0f, 1.0f);
            ImDrawList* dlist = ImGui::GetWindowDrawList();
            i32 pos = 0;
            ImVec2 min_xy = ImGui::GetWindowContentRegionMin();
            v2f vp_center = vp_size / 2;
            vp_center.x += min_xy.x + 2;
            vp_center.y += min_xy.y + 12;

            f32 cell_w = (vp_size.y / 2.0f) / (processed_map.size.y / 2.0f);
            v2f orig = vp_center - v2f{vp_size.y / 2 - cell_w / 2, vp_size.y / 2 - cell_w / 2};
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

        if (ImGui::Button(ICON_CI_SETTINGS " settings##pf_show_options"))
            gui_options.options_shown = !gui_options.options_shown;

        ImGui::SameLine();
        auto is_pause_enabled_cv = options.settings.find("gfx.pause");
        if (is_pause_enabled_cv && !is_pause_enabled_cv->value.getValueOr<bool>(false))
        {
            if (ImGui::Button(ICON_CI_DEBUG_PAUSE "##pf_pause"))
                is_pause_enabled_cv->value.set<bool>(true);
        }
        else if (is_pause_enabled_cv)
        {
            if (ImGui::Button(ICON_CI_DEBUG_START "##pf_pause"))
                is_pause_enabled_cv->value.set<bool>(false);
        }

        std::string item_id;
        item_id.reserve(80);

        if (gui_options.options_shown)
        {
            // move diagonally toggle
            auto is_diag_move_allowed = options.settings.find(opt_allow_diagonal.key_name);
            if (is_diag_move_allowed && is_diag_move_allowed->value.getValueOr<bool>(true))
            {
                if (ImGui::Button(ICON_CI_DIFF_ADDED " disable diagonal movement##pf_hide_overlay"))
                    is_diag_move_allowed->value.set<bool>(false);
            }
            else if (is_diag_move_allowed)
            {
                if (ImGui::Button(
                        ICON_CI_DIFF_IGNORED " enable diagonal movement##pf_show_overlay"))
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
                if (ImGui::Button(ICON_CI_DEBUG " debug: walls nearby##pf_hide_overlay"))
                    should_show_overlay_opt->value.set<bool>(false);
            }
            else if (should_show_overlay_opt)
            {
                if (ImGui::Button(ICON_CI_DEBUG " debug: walls nearby##pf_show_overlay"))
                    should_show_overlay_opt->value.set<bool>(true);
            }

            ImGui::PushFont(vex::g_view_hub.visuals.fnt_tiny);
            defer_ { ImGui::PopFont(); };
            ImVec2 side_panel_size = {350, 0};
            // side_panel_size.x = max(side_panel_size.x, 200.0f);GuiRelVec{ 0.25, 0 };
            [[maybe_unused]] bool visible = ImGui::BeginChild(
                "##pf_options", side_panel_size, false);
            defer_ { ImGui::EndChild(); };
            ImGui::Indent(4);
            defer_ { ImGui::Unindent(); };

            for (auto& [k, v] : options.settings)
            {
                if ((SettingsContainer::Flags::k_visible_in_ui & v.flags) == 0)
                    continue;
                if (!k.starts_with("pf.") || k.length() < 4)
                    continue;

                item_id.clear();

                const bool has_min = v.min.hasAnyValue();
                const bool has_max = v.max.hasAnyValue();
                const char* name_adj = k.data() + 3;
                fmt::format_to(std::back_inserter(item_id), "##a{}", name_adj);

                v.value.match(
                    [&, &vv = v](i32& a)
                    {
                        ImGui::TextUnformatted(name_adj);
                        // ImGui::SameLine();
                        // ImGui::PushItemWidth(-40);
                        if (has_min && has_max)
                            ImGui::SliderInt(
                                item_id.c_str(), &a, vv.min.get<i32>(), vv.max.get<i32>());
                        else
                            ImGui::InputInt(item_id.c_str(), &a);
                    },
                    [&, &vv = v](f32& a)
                    {
                        ImGui::TextUnformatted(name_adj);
                        // ImGui::SameLine();
                        // ImGui::PushItemWidth(-40);
                        if (has_min && has_max)
                        {
                            ImGui::SliderFloat(item_id.c_str(), &a, vv.min.get<f32>(),
                                vv.max.get<f32>(), "%.3f", ImGuiSliderFlags_AlwaysClamp);
                            if ((SettingsContainer::Flags::k_ui_min_as_step & vv.flags) != 0)
                                a = round(a / vv.min.get<f32>()) * vv.min.get<f32>();
                        }
                        else
                            ImGui::InputFloat(item_id.c_str(), &a);
                    },
                    [&](bool& a)
                    {
                        ImGui::Checkbox(name_adj, &a);
                    },
                    [&](v4f& a)
                    {
                        // ImGui::BeginChild("Color picker");
                        ImGui::TextUnformatted(name_adj);
                        ImGui::SetNextItemWidth(128);
                        ImGui::ColorPicker4(item_id.c_str(), (float*)&a.x, //
                            ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoLabel |
                                ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_AlphaBar |
                                ImGuiColorEditFlags_NoInputs);
                        // ImGui::EndChild();
                    });
            }
        }
    }
    if (ImGui::BeginMainMenuBar())
    {
        defer_ { ImGui::EndMainMenuBar(); };
        ImGui::Bullet();
        ImGui::Text("[particles:%d/%d] ", num_particles, max_particles);
        ImGui::Bullet();
        {
            ImGui::SetNextItemWidth(105);
            auto time_scale = options.settings.find(opt_time_scale.key_name);
            auto& ts_value = time_scale->value.get<float>();
            ImGui::SliderFloat("Time scale##pf_time_scale", &ts_value, 0.25f, 2.0f, "%.3f",
                ImGuiSliderFlags_AlwaysClamp);
        }
    }
    const bool has_no_goal = init_data.isBlocked(goal_cell);
    const bool has_no_particles = (num_particles == 0);
    if (has_no_goal || has_no_particles)
    {
        const auto offset1 = ImGui::CalcTextSize(" Left click on any empty cell to set goal").x / 2;
        const auto offset2 =
            ImGui::CalcTextSize(" Right click on any empty cell to spawn particles").x / 2;
        ImGui::PushFont(vex::g_view_hub.visuals.fnt_header);
        defer_ { ImGui::PopFont(); };
        if (has_no_goal)
        {
            ImGui::SetCursorPos({vp_size.x / 2.0f - offset1, vp_size.y / 2.0f});
            ImGui::Text(" Right click on any empty cell to set goal ");
        }
        else if (has_no_particles)
        {
            ImGui::SetCursorPos({vp_size.x / 2.0f - offset2, vp_size.y / 2.0f});
            ImGui::Text(" Left click on any empty cell to spawn particles ");
        }
    }
}
void FlowfieldPF::postFrame(Application& owner)
{
    ZoneScopedN("FlowfieldPF::PostFrame");
    frame_alloc_resource.reset();
    viewports.postFrame();
}
