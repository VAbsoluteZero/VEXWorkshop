#include "Application.h"
//
#include <SDL.h>
#include <flags.h>
#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_internal.h>
#include <ini.h>
#include <misc/cpp/imgui_stdlib.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <algorithm>
#include <functional>
#include <thread>
//
#include <VCore/Containers/Ring.h>
#include <render/Dx11/DirectX11App.h>
#include <utils/CLI.h>
#include <utils/ImGuiUtils.h>


using namespace std::literals::string_view_literals;
// fwd decl
namespace vexgui
{
    static auto setupImGuiForDrawPass(vp::Application& app) -> void;

    static std::unique_ptr<class ConsoleWindow> g_console;

    static void createConsole();
    static void showConsoleWindow(bool* p_open);

    static bool g_console_open = true;

} // namespace vexgui

static void setupLoggers();
static void setupSettings(vp::Application& app);


vp::Application& vp::Application::init(const StartupConfig& in_config)
{
    setupLoggers();
    vexgui::createConsole();

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

    self.created = true;
    self.main_window = tWindow::create(config.WindowArgs);

    self.framerate.target_framerate = config.target_framerate;

    Sleep(10);
    if (!self.app_impl)
    {
        // self.setApplicationType<SdlDx11Application>(true);
        self.setApplicationType<DirectX11App>(true);
    }
    setupSettings(self);

    return self;
}

i32 vp::Application::runLoop()
{
    spdlog::stopwatch g_frame_sw;
    running = true;
    if (pending_stop)
        return 0;

    // Init ImGui related stuff
    {
        g_view_hub.onFirstPaintCall(main_window->windowSize(), main_window->display_size.y);
        ImGui::GetStyle() = g_view_hub.visuals.styles[EImGuiStyle::DeepDark];
    }

    do
    {
        // Handle events on queue
        SDL_Event sdl_event = {};
        while (SDL_PollEvent(&sdl_event) != 0)
        {
            ImGui_ImplSDL2_ProcessEvent(&sdl_event);
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
                        this->app_impl->handleWindowResize(*this, size);
                    }
                }
                break;

                default: break;
            }
        }
        //-----------------------------------------------------------------------------
        // prepare frame
        {
            app_impl->preFrame(*this);
        }
        //-----------------------------------------------------------------------------
        // frame
        {
            app_impl->frame(*this);
        }
        //-----------------------------------------------------------------------------
        // imgui draw callbacks
        {
            settings.changed_this_tick = false;
            vexgui::setupImGuiForDrawPass(*this);

            for (auto& [view_name, view] : g_view_hub.views)
            {
                for (auto& callback : view.delayed_gui_drawcalls)
                {
                    if (callback)
                        callback({ImGui::GetCurrentContext()});
                }
                view.delayed_gui_drawcalls.clear();
            }
        }
        //-----------------------------------------------------------------------------
        // post frame
        {
            app_impl->postFrame(*this);
        }

        // time & fps
        {
            using namespace std::chrono_literals;
            framerate.frame_number++;
            double dt_sec = g_frame_sw.elapsed() / 1.0s;
            ftime.update(dt_sec);

            g_frame_sw.reset();
        }
    } while (running && !pending_stop);

    if (app_impl)
    {
        app_impl->teardown(*this);
        app_impl = {};
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
void vexgui::setupImGuiForDrawPass(vp::Application& app)
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
            if (ImGui::MenuItem("Console", nullptr, &g_console_open))
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
            ImGui::MenuItem("Show ImGui Demo", nullptr, &g_demo_shown);
            ImGui::MenuItem("Show ImGui Metrics", nullptr, &g_metric_shown);
        }
    }

    if (g_demo_shown)
        ImGui::ShowDemoWindow();
    if (g_metric_shown)
        ImGui::ShowMetricsWindow();

    [[maybe_unused]] ImGuiID main_dockspace = ImGui::DockSpaceOverViewport(nullptr);

    static ImGuiDockNodeFlags dockspace_flags =
        ImGuiDockNodeFlags_NoDockingInCentralNode; // ImGuiDockNodeFlags_KeepAliveOnly;
    static auto do_once_guard = true;
    if (std::exchange(do_once_guard, false))
    {
        ImGuiID dockspace_id = main_dockspace;
        auto viewport = ImGui::GetMainViewport();

        ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
        ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

        ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(
            main_dockspace, ImGuiDir_Left, 0.16f, nullptr, &dockspace_id);
        ImGuiID dock_id_down = ImGui::DockBuilderSplitNode(
            dockspace_id, ImGuiDir_Down, 0.25f, nullptr, &dockspace_id);

        ImGui::DockBuilderDockWindow("Console", dock_id_down);
        ImGui::DockBuilderDockWindow("Details", dock_id_left);
        ImGui::DockBuilderFinish(main_dockspace);
    }

    ImGui::Begin("Details");
    ImGui::Text("Inspector/toolbar placeholder");
    ImGui::End();

    // ImGui::Begin("Details");
    // ImGui::Text("Details/Console placeholder");
    // ImGui::End();
    if (g_console_open)
        vexgui::showConsoleWindow(&g_console_open);
}

namespace vexgui
{
    // NOTE: modified sample from imgui_demo.cpp : ShowExampleAppConsole
    // slightly tweaked to better match needs of the app.

    // #todo thread safe double buffer.
    struct PedningEntries
    {
        void copy_from(const std::vector<std::string>& vec)
        {
            std::lock_guard<std::mutex> lock(mutex);
            buff.insert(buff.end(), vec.begin(), vec.end());
        }
        void push(const char* str)
        {
            std::lock_guard<std::mutex> lock(mutex);
            buff.push_back(str);
        }
        void push(const char* str, size_t size)
        {
            std::lock_guard<std::mutex> lock(mutex);
            buff.push_back(std::string{str, size});
        }

        void move_to(auto& pushable)
        {
            std::lock_guard<std::mutex> lock(mutex);
            for (auto& str : buff)
            {
                pushable.push(std::move(str));
            }
            buff.clear();
        }

    private:
        std::mutex mutex;
        std::vector<std::string> buff;
    };
    static PedningEntries g_pending_console_lines;

    // Portable helpers
    static char* Strdup(const char* s)
    {
        IM_ASSERT(s);
        size_t len = strlen(s) + 1;
        void* buf = malloc(len);
        IM_ASSERT(buf);
        return (char*)memcpy(buf, (const void*)s, len);
    }
    static void Strtrim(char* s)
    {
        char* str_end = s + strlen(s);
        while (str_end > s && str_end[-1] == ' ')
            str_end--;
        *str_end = 0;
    }

    struct ConsoleWindow
    {
        char input_buf[256];
        vex::StaticRing<std::string, 256, true> items_rbuf;
        std::vector<std::string> history_buf;

        int history_pos = -1; // -1: new line, 0..History.Size-1 browsing history.
        ImGuiTextFilter text_filter;
        bool auto_scroll = true;
        bool scroll_to_bottom = false;

        unsigned fetch_cooldown : 3 = 0;

        ConsoleWindow()
        {
            memset(input_buf, 0, sizeof(input_buf));
            history_pos = -1;
            auto_scroll = true;
            scroll_to_bottom = false;

            static bool once = false;
            if (!once)
            {
                once = true;
                vp::console::makeAndRegisterCmd("test",
                    "blah blah\nlong \n msg\n in\n lines\n test.\n", true,
                    [](const vp::console::CmdCtx& ctx)
                    {
                        return true;
                    });

                vp::console::makeAndRegisterCmd("cmd", " I am cmd!\n desc desc.\n", true,
                    [](const vp::console::CmdCtx& ctx)
                    {
                        SPDLOG_INFO("# other stuff: {}\n", ctx.parsed_args->get<int>("id", -1));
                        return true;
                    });
                vp::console::makeAndRegisterCmd("cmd_2", " I am cmd!\n desc desc.\n", true,
                    [](const vp::console::CmdCtx& ctx)
                    {
                        return ctx.parsed_args->get<int>("id", -1) == 42;
                    });
            }
        }
        ~ConsoleWindow() {}

        void ClearLog() { items_rbuf.clear(); }

        void Add(std::string str) { items_rbuf.push(std::move(str)); }

        void Draw(const char* title, bool* p_open)
        {
            fetch_cooldown++;
            if (fetch_cooldown == 0)
            {
                g_pending_console_lines.move_to(items_rbuf);
            }

            ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
            if (!ImGui::Begin(title, p_open))
            {
                ImGui::End();
                return;
            }
            defer_ { ImGui::End(); };


            // As a specific feature guaranteed by the library, after calling Begin() the last Item
            // represent the title bar. So e.g. IsItemHovered() will return true when hovering the
            // title bar. Here we create a context menu only available from the title bar.
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Close Console"))
                    *p_open = false;
                ImGui::EndPopup();
            }
            // ImGui::TextWrapped("Enter 'HELP' for help.");

            // TODO: display items starting from the bottom
            if (ImGui::SmallButton("Clear"))
            {
                ClearLog();
            }
            ImGui::SameLine();

            // Options menu
            if (ImGui::BeginPopup("Options"))
            {
                ImGui::Checkbox("Auto-scroll", &auto_scroll);
                ImGui::EndPopup();
            }

            // Options, Filter
            if (ImGui::Button("Options"))
                ImGui::OpenPopup("Options");
            ImGui::SameLine();
            text_filter.Draw("text_filter (\"incl,-excl\") (\"error\")", 180);
            ImGui::Separator();

            // Reserve enough left-over height for 1 separator + 1 input text
            const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y +
                                                   ImGui::GetFrameHeightWithSpacing();
            ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false,
                ImGuiWindowFlags_HorizontalScrollbar);
            if (ImGui::BeginPopupContextWindow())
            {
                if (ImGui::Selectable("Clear"))
                    ClearLog();
                ImGui::EndPopup();
            }

            // #todo consider some sort of clipping rectangle (read about it in imgui_demo.cpp,
            // where original is located)
            {
                ImGui::PushFont(vp::g_view_hub.visuals.fnt_log);
                defer_ { ImGui::PopFont(); };

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
                for (auto riter = items_rbuf.rbegin(); !riter.isDone(); ++riter)
                {
                    const char* item = (*riter).c_str();
                    if (!text_filter.PassFilter(item))
                        continue;

                    ImVec4 color;
                    bool has_color = false;
                    if (strncmp(item, "E ", 2) == 0)
                    {
                        color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                        has_color = true;
                    }
                    else if (strncmp(item, "W ", 2) == 0)
                    {
                        color = ImVec4(1.0f, 1.0f, 0.8f, 1.0f);
                        has_color = true;
                    }
                    else if (strncmp(item, "# ", 2) == 0)
                    {
                        color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f);
                        has_color = true;
                    }
                    if (has_color)
                        ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(item);
                    if (has_color)
                        ImGui::PopStyleColor();
                }

                if (scroll_to_bottom ||
                    (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
                    ImGui::SetScrollHereY(1.0f);
                scroll_to_bottom = false;

                ImGui::PopStyleVar();
            }
            ImGui::EndChild();
            ImGui::Separator();
            // Command-line
            bool reclaim_focus = false;
            ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                                   ImGuiInputTextFlags_CallbackCompletion |
                                                   ImGuiInputTextFlags_CallbackHistory;
            if (ImGui::InputText("Input", input_buf, IM_ARRAYSIZE(input_buf), input_text_flags,
                    &TextEditCallbackStub, (void*)this))
            {
                char* s = input_buf;
                Strtrim(s);
                if (s[0])
                    ExecCommand(s);
                input_buf[0] = '\0';
                reclaim_focus = true;
            }

            // Auto-focus on window apparition
            ImGui::SetItemDefaultFocus();
            if (reclaim_focus)
                ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
        }

        void ExecCommand(const char* command_line)
        {
            std::string full = command_line;
            const char* cmd_s = full.c_str();
            const char* cmd_e = full.c_str() + full.size();

            while (*cmd_s == ' ' && cmd_s < cmd_e)
                cmd_s++;

            const char* cmd_name_e = cmd_s;
            while (*cmd_name_e != ' ' && cmd_name_e < cmd_e)
                cmd_name_e++;

            if (cmd_name_e <= cmd_s)
                return;

            // print out command, to both console and log (console would receive it via sink)
            SPDLOG_INFO("# {}\n", command_line);

            std::string cmd_part(cmd_s, cmd_name_e);
            std::string arg_part(cmd_name_e, cmd_e);

            // Insert into history. First find match and delete it so it can be pushed to the back.
            {
                history_pos = -1;
                for (auto it = history_buf.begin(); history_buf.end() != it; ++it)
                {
                    if (strcmp(it->c_str(), command_line) == 0)
                    {
                        it = history_buf.erase(it);
                        break;
                    }
                }
                history_buf.push_back(command_line);
            }

            // trigger
            {
                vp::console::ClCommand* cmd = vp::console::findCmd(cmd_part.c_str());

                if (nullptr == cmd)
                {
                    SPDLOG_INFO("{} : no such command found\n", command_line);
                    return;
                }
                bool r = cmd->trigger(arg_part);
                if (!r)
                {
                    SPDLOG_INFO("{} : failed. \n", command_line);
                }
            }
            // On command input, we scroll to bottom even if AutoScroll==false
            scroll_to_bottom = true;
        }

        static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
        {
            ConsoleWindow* console = (ConsoleWindow*)data->UserData;
            return console->TextEditCallback(data);
        }

        int TextEditCallback(ImGuiInputTextCallbackData* data)
        {
            switch (data->EventFlag)
            {
                case ImGuiInputTextFlags_CallbackCompletion:
                {
                    // Example of TEXT COMPLETION
                    // Locate beginning of current word
                    const char* word_end = data->Buf + data->CursorPos;
                    const char* word_start = word_end;
                    while (word_start > data->Buf)
                    {
                        const char c = word_start[-1];
                        if (c == ' ' || c == '\t' || c == ',' || c == ';')
                            break;
                        word_start--;
                    }

                    using CmdMap = vex::Dict<const char*, vp::console::ClCommand>;
                    const CmdMap& cmd_dict = vp::console::CmdRunner::global().commands;

                    // Build a list of candidates
                    // #fixme : frame allocator
                    static std::vector<const char*> candidates;
                    candidates.clear();
                    for ([[maybe_unused]] const auto& [key, cmd] : cmd_dict)
                    {
                        // as it is case sensetive impl, we can just compare bytes
                        if (std::memcmp(key, word_start, (int)(word_end - word_start)) == 0)
                        {
                            candidates.push_back(key);
                        }
                    }

                    const i32 canditate_count = candidates.size();
                    if (canditate_count == 0)
                    {
                        return 0;
                    }
                    else if (canditate_count == 1)
                    {
                        // Single match. Delete the beginning of the word and replace it entirely so
                        // we've got nice casing.
                        data->DeleteChars(
                            (int)(word_start - data->Buf), (int)(word_end - word_start));
                        data->InsertChars(data->CursorPos, candidates[0]);
                        data->InsertChars(data->CursorPos, " ");
                    }
                    else
                    {
                        // Multiple matches. Complete as much as we can..
                        // So inputing "C"+Tab will complete to "CL" then display "CLEAR" and
                        // "CLASSIFY" as matches.
                        int match_len = (int)(word_end - word_start);

                        int dbg_guard = 1000;
                        for (; dbg_guard > 0; dbg_guard--) // meant to be infinite
                        {
                            int c = 0;
                            bool all_candidates_matches = true;
                            for (int i = 0; i < canditate_count && all_candidates_matches; i++)
                            {
                                if (i == 0)
                                {
                                    c = candidates[i][match_len];
                                }
                                else if (c == 0 || (c != candidates[i][match_len]))
                                {
                                    all_candidates_matches = false;
                                    break;
                                }
                            }
                            if (!all_candidates_matches)
                                break;
                            match_len++;
                        }
                        check(dbg_guard > 0, "infinite loop prevented");

                        if (match_len > 0)
                        {
                            data->DeleteChars(
                                (int)(word_start - data->Buf), (int)(word_end - word_start));
                            data->InsertChars(
                                data->CursorPos, candidates[0], candidates[0] + match_len);
                        }

                        // List matches
                        Add("Possible matches:\n");
                        for (int i = 0; i < canditate_count; i++)
                            Add(fmt::format("- {}\n", candidates[i]));
                    }

                    break;
                }
                case ImGuiInputTextFlags_CallbackHistory:
                {
                    const int prev_history_pos = history_pos;
                    if (data->EventKey == ImGuiKey_UpArrow)
                    {
                        if (history_pos == -1)
                            history_pos = ((i32)history_buf.size()) - 1;
                        else if (history_pos > 0)
                            history_pos--;
                    }
                    else if (data->EventKey == ImGuiKey_DownArrow)
                    {
                        if (history_pos != -1)
                            if (++history_pos >= history_buf.size())
                                history_pos = -1;
                    }
                    // A better implementation would preserve the data on the current input line
                    // along with cursor position.
                    if (prev_history_pos != history_pos)
                    {
                        const char* history_str = (history_pos >= 0)
                                                      ? history_buf[history_pos].c_str()
                                                      : "";
                        data->DeleteChars(0, data->BufTextLen);
                        data->InsertChars(0, history_str);
                    }
                }
            }
            return 0;
        }
    };

    static void createConsole() { g_console = std::make_unique<ConsoleWindow>(); }

    static void showConsoleWindow(bool* p_open)
    {
        if (g_console)
            g_console->Draw("Console", p_open);
    }
} // namespace vexgui

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
                vexgui::g_pending_console_lines.push(msg.payload.data(), msg.payload.size());
                return;
            }

            spdlog::sinks::base_sink<spdlog::details::null_mutex>::formatter_->format(
                msg, formatted);

            vexgui::g_pending_console_lines.push(formatted.data(), formatted.size());

            formatted.clear();
        }

        void flush_() override {}

        // contested resources
    };
} // namespace spdlog::sinks

static void setupLoggers()
{
    auto vs_output = std::make_shared<spdlog::sinks::msvc_sink_st>();
    auto cls_output = std::make_shared<spdlog::sinks::console_buf_sink>();

    spdlog::sinks_init_list sink_list = {vs_output, cls_output};

    spdlog::set_default_logger(std::make_shared<spdlog::logger>("vex", sink_list));
}

void setupSettings(vp::Application& app)
{
    auto& settings_container = app.getSettings();

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
