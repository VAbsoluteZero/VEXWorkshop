#include "Application.h"
//
#include <SDL.h>
#include <flags.h>
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <misc/cpp/imgui_stdlib.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <Tracy.hpp>
#include <algorithm>
#include <functional>
#include <thread>
//
#ifdef VEX_RENDER_DX11
    #include <render/Dx11/DirectX11App.h>
    #include <render/legacyDx11.h>
#else
    #include <webgpu/render/WgpuApp.h>
#endif
#ifdef VEX_EMSCRIPTEN
    #include <emscripten.h>
    #include <emscripten/html5.h>

static auto g_cached_canvas_sz = v2i32{0, 0};
#else
    #include <ini.h>
#endif

#include <utils/CLI.h>
#include <utils/ImGuiUtils.h>

using namespace std::literals::string_view_literals;

static inline const auto opt_imgui_demo = vex::SettingsContainer::EntryDesc<bool>{
    .key_name = "app.ShowIMGUIDemoWindow",
    .info = "Display demo.",
    .default_val = false,
    .flags = 0,
};

static void setupLoggers();
static void setupSettings(vex::Application& app);

#ifdef VEX_EMSCRIPTEN
extern "C"
{
    void vex_em_update_wnd_size(int x, int y) {}
}
#endif

vex::Application& vex::Application::init(const StartupConfig& in_config, DemoSamples&& samples)
{
    FrameMarkNamed("Initialization 'frame'");
    ZoneScopedN("startup:initialization");
    setupLoggers();

    auto config = in_config;
#ifndef VEX_EMSCRIPTEN
    { // #fixme
        mINI::INIFile file("cfg.ini");
        mINI::INIStructure ini;
        if (file.read(ini))
        {
            std::string& s_pos = ini["window"sv]["pos"sv];
            std::string& s_size = ini["window"sv]["size"sv];

            v2i32 size;
            v2i32 pos;
            std::stringstream stream(s_pos);
            if (!stream.eof())
            {
                stream >> pos.x >> pos.y;
            }
            stream = std::stringstream(s_size);
            if (!stream.eof())
            {
                stream >> size.x >> size.y;
            }
            if (size.x > 0 && size.y > 0 && pos.x > 0 && pos.y > 0)
            {
                config.WindowArgs.h = size.y;
                config.WindowArgs.w = size.x;
                config.WindowArgs.x = pos.x;
                config.WindowArgs.y = pos.y;
            }
        }
    }
#else
    {
        int width = 0;
        int height = 0;
        emscripten_get_canvas_element_size("canvas", &width, &height);
        auto ratio = emscripten_get_device_pixel_ratio();
        config.WindowArgs.h = height;
        config.WindowArgs.w = width;
        config.WindowArgs.x = 0;
        config.WindowArgs.y = 0;

        SPDLOG_INFO("pixel ratio: {}  ", ratio);
        SPDLOG_INFO("canvas area: {} : {}", width, height);
        emscripten_get_screen_size(&width, &height);
        SPDLOG_INFO("scree area: {} : {}", width, height);
        g_cached_canvas_sz = {width, height};
    }
#endif

    vex::console::makeAndRegisterCmd("app.quit", "Exit application.\n", true,
        [](const vex::console::CmdCtx& ctx)
        {
            vex::Application::get().quit();
            return true;
        });
    vex::console::makeAndRegisterCmd("app.set_fps", "Limit fps\n", true,
        [](const vex::console::CmdCtx& ctx)
        {
            auto opt_int = ctx.parsed_args->get<int>(0);
            if (opt_int)
            {
                i32 v = opt_int.value_or(-1);
                vex::Application::get().setMaxFps(v);
                SPDLOG_WARN("# try to limit fps to {}", v);
                return true;
            }
            return false;
        });

    Application& self = staticSelf();

    self.created = true;
    self.main_window = tWindow::create(config.WindowArgs);

    self.framerate.target_framerate = config.target_framerate;
    self.allow_demo_transition = config.allow_demo_changes;

    // using namespace std::literals::chrono_literals;
    // std::this_thread::sleep_for(2ms);

#if ENABLE_IMGUI // Init ImGui related stuff
    [[maybe_unused]] auto c = ImGui::CreateContext();
    g_view_hub.onFirstPaintCall(self.main_window->windowSize(), self.main_window->display_size.y);
    ImGui::GetStyle() = g_view_hub.visuals.styles[EImGuiStyle::DeepDark];
#endif

    if (!self.gfx_backend)
    {
        ZoneScopedN("startup:create graphics backend");
#ifdef VEX_RENDER_DX11
        // self.setGraphicsBackend<SdlDx11Application>(true);
        self.setGraphicsBackend<DirectX11App>(true);
#else
        self.setGraphicsBackend<WebGpuBackend>(true);
        SPDLOG_INFO("created gfx backend instance");
#endif
    }
    setupSettings(self);
    self.all_demos = samples;
    opt_imgui_demo.addTo(self.getSettings());
    opt_time_scale.addTo(self.getSettings());

    SPDLOG_INFO("created application instance");

    return self;
}

bool vex::Application::activateDemo(const char* id)
{
    ZoneScopedN("create demo instance");

    if (auto* entry = all_demos.demos.find(id); entry)
    {
        SPDLOG_INFO(" activate demo : {}", id);
        active_demo.reset(entry->callback(*this));
        return true;
    }
    SPDLOG_INFO(" failed to activated ");
    return false;
}

i32 vex::Application::runLoop()
{
    if (nullptr == gfx_backend)
        return 1;
    spdlog::stopwatch g_frame_sw;
    running = true;
    if (pending_stop)
        return 0;

    input.global.quit_delegate = [&]()
    {
        SPDLOG_INFO("* quit pressed");
        pending_stop = true;
    };
    input.global.resize_delegate = [&]()
    {
        v2u32 size = main_window->windowSize();
        this->gfx_backend->handleWindowResize(*this, size);
        ImGuiSizes::g_cvs_sz = size;
        SPDLOG_INFO("window resized, {}x : {}y", size.x, size.y);
    };

    do
    {
        FrameMarkNamed("Main thread loop");
        input.poll(ftime.unscalled_dt_f32);
        input.updateTriggers();

        //-----------------------------------------------------------------------------
        // prepare frame
        {
            ZoneScopedN("Gfx::StartFrame");
            if (pending_demo.hasAnyValue())
            {
                DemoSamples::Entry& ent = pending_demo.get<DemoSamples::Entry>();
                if (active_demo)
                {
                    SPDLOG_WARN(" [Experimental] Demo reset began.");
                    active_demo.reset(nullptr);
                    ImGui::DestroyContext(ImGui::GetCurrentContext());
                    active_demo.reset(ent.callback(*this));
                    [[maybe_unused]] auto c = ImGui::CreateContext();
                    g_view_hub.onFirstPaintCall(
                        this->main_window->windowSize(), this->main_window->display_size.y);
                    ImGui::GetStyle() = g_view_hub.visuals.styles[EImGuiStyle::DeepDark];
                    gfx_backend->softReset(*this);
                    SPDLOG_WARN(" [Experimental] Demo reset completed.");
                }
                else
                {
                    active_demo.reset(ent.callback(*this));
                }
                SPDLOG_WARN(" Active demo : {}", ent.readable_name);
                pending_demo = {};
            }
            gfx_backend->startFrame(*this);
        }
        //-----------------------------------------------------------------------------
        // frame
        {
            if (active_demo)
            {
                ZoneScopedN("Demo Update");
                active_demo->update(*this);
            }
            ZoneScopedN("Gfx::Frame");
            gfx_backend->frame(*this);
        }
        //-----------------------------------------------------------------------------
        // imgui draw callbacks
        {
            ZoneScopedN("App::UI");
            showAppLevelUI();
            settings.ifHasValue<float>(opt_time_scale.key_name,
                [&](float scale)
                {
                    ftime.setTimeMutlipier(scale);
                });
        }
        //-----------------------------------------------------------------------------
        // post frame
        {
            {
                ZoneScopedN("Gfx::PostFrame");
                gfx_backend->postFrame(*this);
            }
            ZoneScopedN("Demo::PostFrame");
            if (active_demo)
                active_demo->postFrame(*this);
            input.onFrameEnd();
        }
        // time & fps
        {
            ZoneScopedN("App::Waiting(FPS limit)");
            using namespace std::chrono_literals;
            framerate.frame_number++;
            auto ft_elapsed = g_frame_sw.elapsed();

            // dumb implementation #fixme - rewrite
            if (!framerate.fpsUnconstrained())
            {
                double dt_ms = ft_elapsed / 1.0ms;
                auto desired_frame_dur = framerate.desiredFrameDurationMs();
                auto left_ms = desired_frame_dur - dt_ms;
                if (left_ms > 0.1f)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds((i64)(left_ms - 0.05f)));
                }
            }
            ft_elapsed = g_frame_sw.elapsed();
            double dt_sec = ft_elapsed / 1.0s;
            ftime.update(dt_sec);
            g_frame_sw.reset();
        }
#if __EMSCRIPTEN__
        SDL_Delay(1);
#endif

    } while (running && !pending_stop);

    if (active_demo)
    {
        active_demo = {};
    }
    if (gfx_backend)
    {
        gfx_backend->teardown(*this);
        gfx_backend = {};
    }

    // save #fixme
    {
#ifndef __EMSCRIPTEN__
        v2i32 size;
        v2i32 pos;
        auto wnd = main_window->getRawWindow();
        SDL_GetWindowSize(wnd, &size.x, &size.y);
        SDL_GetWindowPosition(wnd, &pos.x, &pos.y);

        mINI::INIFile file("cfg.ini");
        mINI::INIStructure ini;

        ini["window"sv]["pos"sv] = fmt::format("{} {}", pos.x, pos.y);
        ini["window"sv]["size"sv] = fmt::format("{} {}", size.x, size.y);
        file.generate(ini);
#endif
    }

    SDL_Quit();

    return 0;
}

//-----------------------------------------------------------------------------
// [SECTION] ImGUI
//-----------------------------------------------------------------------------
static vex::SettingsContainer::VersionFilter imgui_demo{opt_imgui_demo.key_name};
void vex::Application::showAppLevelUI()
{
#if ENABLE_IMGUI
    static bool gui_demo_selection_flag = false;
    if (allow_demo_transition)
    {
        ImGui::PushFont(vex::g_view_hub.visuals.fnt_accent);
        defer_ { ImGui::PopFont(); };
        if (ImGui::BeginMainMenuBar())
        {
            defer_ { ImGui::EndMainMenuBar(); };
            if (ImGui::BeginMenu("File"))
            {
                defer_ { ImGui::EndMenu(); };
                ImGui::MenuItem("Show demo selection", nullptr, &gui_demo_selection_flag);
            }
        }
    }
    if ((gui_demo_selection_flag && !pending_demo.hasAnyValue()) || !active_demo)
    {
        if (showDemoSelection())
            gui_demo_selection_flag = false;
    }
    if (active_demo)
    {
        ZoneScopedN("Demo::DrawUI");
        active_demo->drawUI(*this);
    }

    for (auto& [view_name, view] : g_view_hub.views)
    {
        for (auto& callback : view.delayed_gui_drawcalls)
        {
            if (callback)
                callback({ImGui::GetCurrentContext()});
        }
        view.delayed_gui_drawcalls.clear();
    }
    imgui_demo.ifHasValue<bool>(settings,
        [](bool& v)
        {
            if (v)
                ImGui::ShowDemoWindow();
        });
#endif
}
bool vex::Application::showDemoSelection()
{
#if ENABLE_IMGUI
    if (allow_demo_transition == false && active_demo)
        return false;

    bool selected = false;
    if (!ImGui::IsPopupOpen("Select demo"))
        ImGui::OpenPopup("Select demo");
    if (ImGui::BeginPopupModal(
            "Select demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Available code samples:\n");
        ImGui::Separator();

        for (auto& [k, entry] : all_demos.demos)
        {
            if (ImGui::Button(entry.readable_name))
            {
                ImGui::CloseCurrentPopup();
                pending_demo = entry;
                selected = true;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
                ImGui::Text("%s", entry.description);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
        ImGui::EndPopup();
    }
    if (selected && pending_demo.hasAnyValue())
    {
        SPDLOG_WARN(" Demo selected. Loading. ");
    }
    return selected;
#endif
}
//-----------------------------------------------------------------------------
// [SECTION] LOGGERS
//-----------------------------------------------------------------------------
namespace spdlog::sinks
{
    class console_buf_sink : public spdlog::sinks::base_sink<spdlog::details::null_mutex>
    {
    public:
        console_buf_sink() { set_pattern_("%L [%T.%e]: %v"); }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            thread_local spdlog::memory_buf_t formatted; // thread_local implies 'static'

            // pass unformatted, considered to be 'raw message'
            if ((msg.payload.size() > 2) && (msg.payload[0] == '#' || msg.payload[0] == '*'))
            {
                vex::console::ConsoleWindow::g_pending_console_lines.push(
                    msg.payload.data(), msg.payload.size());
                return;
            }

            spdlog::sinks::base_sink<spdlog::details::null_mutex>::formatter_->format(
                msg, formatted);

            vex::console::ConsoleWindow::g_pending_console_lines.push(
                formatted.data(), formatted.size());

            formatted.clear();
        }

        void flush_() override {}

        // contested resources
    };
} // namespace spdlog::sinks

static void setupLoggers()
{
    auto cls_output = std::make_shared<spdlog::sinks::console_buf_sink>();
    auto std_output = std::make_shared<spdlog::sinks::stdout_color_sink_st>();
#ifndef __EMSCRIPTEN__
    auto vs_output = std::make_shared<spdlog::sinks::msvc_sink_st>();
    spdlog::sinks_init_list sink_list = {vs_output, cls_output};
#else
    spdlog::sinks_init_list sink_list = {cls_output, std_output};
#endif // ! __EMSCRIPTEN__


    spdlog::set_default_logger(std::make_shared<spdlog::logger>("vex", sink_list));
}

void setupSettings(vex::Application& app)
{
    auto& settings_container = app.getSettings();
    settings_container.addSetting("gfx.pause", false);

    vex::console::makeAndRegisterCmd("set",
        "Change application settings. Type 'list' to list all. \n", true,
        [](const vex::console::CmdCtx& ctx)
        {
            auto& settings_dict = vex::Application::get().getSettings().settings;
            std::optional<std::string_view> cmd_key = ctx.parsed_args->get<std::string_view>(0);
            bool list = ctx.parsed_args->get<bool>("list", false);
            if (list)
            {
                SPDLOG_INFO(
                    "* available options & expected type of arg (additional constraints may "
                    "apply):");
                for (auto& [k, v] : settings_dict)
                {
                    const char* type_name = nullptr;
                    v.value.match(
                        [&, &kk = k](i32& a)
                        {
                            type_name = "int32";
                            SPDLOG_INFO("* {} : {} | expects {}", kk, a, type_name);
                        },
                        [&, &kk = k](f32& a)
                        {
                            type_name = "f32";
                            SPDLOG_INFO("* {} : {} | expects {}", kk, a, type_name);
                        },
                        [&, &kk = k](bool& a)
                        {
                            type_name = "bool";
                            SPDLOG_INFO("* {} : {} | expects {}", kk, a, type_name);
                        });
                }
                return true;
            }

            if (!cmd_key)
                return false;

            std::string option{cmd_key.value()};

            auto* entry = settings_dict.find(option);

            if (nullptr == entry)
                return false;

            bool matched = false;
            entry->value.match(
                [&](i32& v)
                {
                    matched = true;
                    auto cmd_value = ctx.parsed_args->get<i32>(1);
                    if (cmd_value)
                        entry->setValue<i32>(*cmd_value);
                },
                [&](f32& v)
                {
                    matched = true;
                    auto cmd_value = ctx.parsed_args->get<f32>(1);
                    if (cmd_value)
                        entry->setValue<f32>(*cmd_value);
                },
                [&](bool& v)
                {
                    matched = true;
                    auto cmd_value = ctx.parsed_args->get<bool>(1);
                    if (cmd_value)
                        entry->setValue<bool>(*cmd_value);
                });

            entry->current_version++;
            // SPDLOG_WARN("# could not set value to setting");
            return matched;
        });
}
