#include "Application.h"
//
#include <SDL.h>
#include <flags.h>
#include <imgui.h>
#include <imgui_impl_sdl.h> 
#include <ini.h>
#include <misc/cpp/imgui_stdlib.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

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
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#include <utils/CLI.h>
#include <utils/ImGuiUtils.h>

using namespace std::literals::string_view_literals;

static void setupLoggers();
static void setupSettings(vp::Application& app);

vp::Application& vp::Application::init(const StartupConfig& in_config, DemoSamples&& samples)
{
    setupLoggers();
    vp::console::createConsole();

    auto config = in_config;
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

    vp::console::makeAndRegisterCmd("app.quit", "Exit application.\n", true,
        [](const vp::console::CmdCtx& ctx)
        {
            vp::Application::get().quit();
            return true;
        });
    vp::console::makeAndRegisterCmd("app.set_fps", "Limit fps\n", true,
        [](const vp::console::CmdCtx& ctx)
        {
            auto opt_int = ctx.parsed_args->get<int>(0);
            if (opt_int)
            {
                i32 v = opt_int.value_or(-1);
                vp::Application::get().setMaxFps(v);
                SPDLOG_WARN("# try to limit fps to {}", v);
                return true;
            }
            return false;
        });

    Application& self = staticSelf();

    self.all_demos = samples;
    self.created = true;
    self.main_window = tWindow::create(config.WindowArgs);

    self.framerate.target_framerate = config.target_framerate;

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);

#if ENABLE_IMGUI // Init ImGui related stuff
    [[maybe_unused]] auto c = ImGui::CreateContext();
    g_view_hub.onFirstPaintCall(self.main_window->windowSize(), self.main_window->display_size.y);
    ImGui::GetStyle() = g_view_hub.visuals.styles[EImGuiStyle::DeepDark];
#endif
    if (!self.gfx_backend)
    {
#ifdef VEX_RENDER_DX11
        // self.setGraphicsBackend<SdlDx11Application>(true);
        self.setGraphicsBackend<DirectX11App>(true);
#else
        self.setGraphicsBackend<WgpuApp>(true);
#endif
    }
    setupSettings(self);

    return self;
}

bool vp::Application::activateDemo(const char* id)
{
    if (auto* entry = all_demos.demos.find(id); entry)
    {
        active_demo.reset(entry->callback(*this));
    } 
    return false;
}

i32 vp::Application::runLoop()
{
    if (nullptr == gfx_backend)
        return 1;
    spdlog::stopwatch g_frame_sw;
    running = true;
    if (pending_stop)
        return 0;

    do
    {
        // Handle events on queue
        SDL_Event sdl_event = {};
        while (SDL_PollEvent(&sdl_event) != 0)
        {
#if ENABLE_IMGUI
            ImGui_ImplSDL2_ProcessEvent(&sdl_event);
#endif
            switch (sdl_event.type)
            {
                case SDL_QUIT:
                {
                    spdlog::info("Received SDL_QUIT, now app is pending stop.");
                    pending_stop = true;
                    break;
                }
                case SDL_WINDOWEVENT:
                {
                    if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED)
                    {
                        v2u32 size = main_window->windowSize();
                        this->gfx_backend->handleWindowResize(*this, size);
                    }
                    break;
                }

                default: break;
            }
        }
        //-----------------------------------------------------------------------------
        // prepare frame
        {
            gfx_backend->preFrame(*this);
        }
        //-----------------------------------------------------------------------------
        // frame
        {
            if (active_demo)
            {
                active_demo->runloop(*this);
            }
            gfx_backend->frame(*this);
        }
        //-----------------------------------------------------------------------------
        // imgui draw callbacks
        {
            settings.changed_this_tick = false;

#if ENABLE_IMGUI
            // setupImGuiForDrawPass(*this);
            if (active_demo) 
                active_demo->drawUI(*this); 
            else 
                showDemoSelection(); 

            for (auto& [view_name, view] : g_view_hub.views)
            {
                for (auto& callback : view.delayed_gui_drawcalls)
                {
                    if (callback)
                        callback({ImGui::GetCurrentContext()});
                }
                view.delayed_gui_drawcalls.clear();
            }
#endif
        }
        //-----------------------------------------------------------------------------
        // post frame
        {
            gfx_backend->postFrame(*this);
        }

        // time & fps
        {
            using namespace std::chrono_literals;
            framerate.frame_number++;
            double dt_sec = g_frame_sw.elapsed() / 1.0s;
            ftime.update(dt_sec);

            g_frame_sw.reset();
        }
#if __EMSCRIPTEN__
        SDL_Delay(1);
#endif

    } while (running && !pending_stop);

    if (gfx_backend)
    {
        gfx_backend->teardown(*this);
        gfx_backend = {};
    }

    // save #fixme
    {
        { // set to dict
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
        }
    }

    SDL_Quit();

    return 0;
}

//-----------------------------------------------------------------------------
// [SECTION] ImGUI
//-----------------------------------------------------------------------------
void vp::Application::showDemoSelection()
{
    if (!ImGui::IsPopupOpen("Select demo"))
        ImGui::OpenPopup("Select demo"); 
    if (ImGui::BeginPopupModal("Select demo", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Available code samples:\n");
        ImGui::Separator();

        for (auto& [k, entry] : all_demos.demos)
        {
            if (ImGui::Button(entry.readable_name))
            {
                ImGui::CloseCurrentPopup();
                active_demo.reset(entry.callback(*this));
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50.0f);
                ImGui::Text(entry.description);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
        ImGui::EndPopup();
    }
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
                vp::console::ConsoleWindow::g_pending_console_lines.push(
                    msg.payload.data(), msg.payload.size());
                return;
            }

            spdlog::sinks::base_sink<spdlog::details::null_mutex>::formatter_->format(
                msg, formatted);

            vp::console::ConsoleWindow::g_pending_console_lines.push(
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
#ifndef __EMSCRIPTEN__
    auto vs_output = std::make_shared<spdlog::sinks::msvc_sink_st>();
    spdlog::sinks_init_list sink_list = {vs_output, cls_output};
#else
    spdlog::sinks_init_list sink_list = {cls_output};
#endif // ! __EMSCRIPTEN__


    spdlog::set_default_logger(std::make_shared<spdlog::logger>("vex", sink_list));
}

void setupSettings(vp::Application& app)
{
    // auto& settings_container = app.getSettings();

    // settings_container.addSetting("gfx.wireframe", false);
    // settings_container.addSetting("gfx.cullmode", 1);

    vp::console::makeAndRegisterCmd("app.set",
        "Change application settings. Type 'list' to list all. \n", true,
        [](const vp::console::CmdCtx& ctx)
        {
            std::optional<std::string_view> cmd_key = ctx.parsed_args->get<std::string_view>(0);
            if (!cmd_key)
                return false;

            std::string option{cmd_key.value()};

            auto& settings_dict = vp::Application::get().getSettings().settings;

            if (option == "list")
            {
                SPDLOG_INFO(
                    "* available options & expected type of arg (additional constraints may "
                    "apply):");
                for (auto& [k, v] : settings_dict)
                {
                    const char* type_name = nullptr;
                    v.match(
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
            vp::OptNumValue* entry = settings_dict.tryGet(option);

            if (nullptr == entry)
                return false;

            bool matched = false;
            entry->match(
                [&](i32& v)
                {
                    matched = true;
                    auto cmd_value = ctx.parsed_args->get<i32>(1);
                    if (cmd_value)
                        v = cmd_value.value();
                },
                [&](f32& v)
                {
                    matched = true;
                    auto cmd_value = ctx.parsed_args->get<f32>(1);
                    if (cmd_value)
                        v = cmd_value.value();
                },
                [&](bool& v)
                {
                    matched = true;
                    auto cmd_value = ctx.parsed_args->get<bool>(1);
                    if (cmd_value)
                        v = cmd_value.value();
                });

            vp::Application::get().getSettings().changed_this_tick = true;
            // SPDLOG_WARN("# could not set value to setting");
            return matched;
        });
}
