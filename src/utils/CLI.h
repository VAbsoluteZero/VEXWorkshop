#pragma once
#include <VCore/Containers/Dict.h>
#include <VCore/Containers/Ring.h>
#include <VCore/Utils/CoreTemplates.h>
#include <VCore/Utils/VMath.h>
#include <VCore/Utils/VUtilsBase.h>
#include <imgui.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace flags
{
    struct args;
}

namespace vex::console
{
    // struct with params
    struct CmdCtx
    {
        const char* name = nullptr;
        std::string full_arg_string;
        std::vector<std::string_view> tokens;
        std::unique_ptr<flags::args> parsed_args;
        ~CmdCtx();
    };

    void tokenize(const std::string& args_as_text, std::vector<std::string_view>& out_args);

    struct ClCommand
    {
        const char* name = nullptr;
        const char* desc = nullptr;
        bool registered = false;
        // const char* last_error_msg = nullptr;
        std::function<bool(const CmdCtx&)> command;
        bool trigger(std::string args_as_text);
    };

    struct CmdRunner
    {
        static CmdRunner& global()
        {
            static CmdRunner runner;
            return runner;
        }

        bool registerCmd(ClCommand cmd, bool replace_if_needed = true)
        {
            auto key = cmd.name;
            bool b_does_not_contain = !commands.contains(key);
            if (replace_if_needed || b_does_not_contain)
            {
                cmd.registered = true;
                commands.emplace(cmd.name, std::move(cmd));
                return true;
            }
            return false;
        }
        void remove(const char* key) { commands.remove(key); }

        CmdRunner();
        vex::Dict<const char*, ClCommand> commands;
    };
} // namespace vex::console

namespace vex::console
{
    template <typename Callable>
    inline bool makeAndRegisterCmd(
        const char* name, const char* desc, bool replace_if_needed, Callable&& callable)
    {
        ClCommand cmd{.name = name, .desc = desc, .registered = false, .command = callable};

        return CmdRunner::global().registerCmd(cmd, replace_if_needed);
    }
    inline bool registerCmd(ClCommand cmd, bool replace_if_needed = true)
    {
        return CmdRunner::global().registerCmd(std::move(cmd), replace_if_needed);
    }
    inline void removeCmd(const char* key) { CmdRunner::global().remove(key); }
    inline ClCommand* findCmd(const char* key) { return CmdRunner::global().commands.tryGet(key); }
}; // namespace vex::console


namespace vex::console
{ 
    struct PedningEntries
    {
        // #fixme get rid of statics
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

    // NOTE: modified sample from imgui_demo.cpp : ShowExampleAppConsole
    // slightly tweaked to better match needs of the app.
    struct ConsoleWindow
    {
        // #fixme get rid of statics
        static inline PedningEntries g_pending_console_lines;
        static inline std::unique_ptr<struct ConsoleWindow> g_console;
        bool is_open = true;

        char input_buf[256];
        vex::StaticRing<std::string, 256, true> items_rbuf;
        std::vector<std::string> history_buf;

        int history_pos = -1; // -1: new line, 0..History.Size-1 browsing history.
        ImGuiTextFilter text_filter;
        bool auto_scroll = true;
        bool scroll_to_bottom = false;
        unsigned fetch_cooldown : 3 = 0;

        ConsoleWindow();
        ~ConsoleWindow() {}

        void ClearLog() { items_rbuf.clear(); }

        void Add(std::string str) { items_rbuf.push(std::move(str)); }

        void Draw(const char* title);
        void ExecCommand(const char* command_line);

        static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
        {
            ConsoleWindow* console = (ConsoleWindow*)data->UserData;
            return console->TextEditCallback(data);
        } 
        int TextEditCallback(ImGuiInputTextCallbackData* data);
    };
    static void createConsole() { ConsoleWindow::g_console = std::make_unique<ConsoleWindow>(); }
    static void showConsoleWindow(ConsoleWindow* window)
    {
        window->Draw("Console");
    }
} // namespace vex::console
