#include "CLI.h"

#include <flags.h>
#include <spdlog/spdlog.h>

#include "ImGuiUtils.h"


vex::console::CmdCtx::~CmdCtx() {}

void vex::console::tokenize(
    const std::string& args_as_text, std::vector<std::string_view>& out_args)
{
    auto eptr = args_as_text.c_str() + args_as_text.size();

    const char* wb = args_as_text.c_str();
    while (wb < eptr && *wb == ' ')
    {
        wb++;
    }
    const char* we = wb;
    while (we < eptr)
    {
        if (*we == ' ')
        {
            auto end_of_slice = we;
            while (*we == ' ')
            {
                we++;
            }

            out_args.push_back({wb, end_of_slice});
            wb = we;
            continue;
        }
        we++;
    }
    if (wb != we)
        out_args.push_back({wb, we});
}


vex::console::CmdRunner::CmdRunner()
{
    auto list_all = [&](const CmdCtx& ctx)
    {
        std::optional<std::string_view> opt_filter = ctx.parsed_args->get<std::string_view>(
            "filter");

        std::string_view filter_str = opt_filter.value_or("-");
        bool filter = opt_filter.has_value();
        bool verbose = ctx.parsed_args->get<bool>("verbose", false);

        for (auto& [name, cmd] : commands)
        {
            std::string_view name_v = name;
            bool do_print = !filter || (name_v.starts_with(filter_str));
            if (!do_print)
                continue;

            SPDLOG_INFO("*{}", name);
            if (verbose && (cmd.desc != nullptr))
            {
                SPDLOG_INFO("*  -{}\n", cmd.desc);
            }
        }
        return true;
    };

    ClCommand cmd{.name = "help",
        .desc =
            "List all commands, -verbose=1 to print help, -filter=\"text\" to filter "
            "by substring.",
        .registered = true,
        .command = list_all};

    commands.emplace(cmd.name, std::move(cmd));
}


bool vex::console::ClCommand::trigger(std::string args_as_text)
{
    if (command)
    {
        CmdCtx ctx;
        ctx.full_arg_string = std::move(args_as_text);
        tokenize(ctx.full_arg_string, ctx.tokens);
        ctx.parsed_args.reset(new flags::args(ctx.tokens));
        ctx.name = name;

        if (ctx.parsed_args->get<bool>("help", false) && (nullptr != desc))
        {
            SPDLOG_INFO("Help for [{}] command : \n{}\n", name, desc);
            return true;
        }

        return command(ctx);
    }
    return false;
}

vex::console::ConsoleWindow::ConsoleWindow()
{
    memset(input_buf, 0, sizeof(input_buf));
    history_pos = -1;
    auto_scroll = true;
    scroll_to_bottom = false;
}

inline void Strtrim(char* s)
{
    char* str_end = s + strlen(s);
    while (str_end > s && str_end[-1] == ' ')
        str_end--;
    *str_end = 0;
}

void vex::console::ConsoleWindow::Draw(const char* title)
{
    fetch_cooldown++;
    if (fetch_cooldown == 0)
    {
        g_pending_console_lines.move_to(items_rbuf);
    }

    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title, &is_open))
    {
        ImGui::End();
        return;
    }
    defer_ { ImGui::End(); };

    ImGui::PushFont(vex::g_view_hub.visuals.fnt_log);
    defer_ { ImGui::PopFont(); };

    // As a specific feature guaranteed by the library, after calling Begin() the last Item
    // represent the title bar. So e.g. IsItemHovered() will return true when hovering the
    // title bar. Here we create a context menu only available from the title bar.
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Close Console"))
            is_open = false;
        ImGui::EndPopup();
    }

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

    // #todo consider clipping rectangle (read about it in imgui_demo.cpp, where original
    // is)
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

    if (scroll_to_bottom || (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    scroll_to_bottom = false;

    ImGui::PopStyleVar();
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

void vex::console::ConsoleWindow::ExecCommand(const char* command_line)
{
    // static bool tst = false;
    // if (!tst)
    //{
    //     tst = true;
    //     vex::console::makeAndRegisterCmd("test", "blah blah", true,
    //         [](const vex::console::CmdCtx& ctx)
    //         {
    //             return true;
    //         });

    //    vex::console::makeAndRegisterCmd("cmd", " I am cmd!\n desc desc.\n", true,
    //        [](const vex::console::CmdCtx& ctx)
    //        {
    //            // tst
    //            SPDLOG_INFO("# other stuff: {}\n", ctx.parsed_args->get<int>("id", -1));
    //            return true;
    //        });
    //    vex::console::makeAndRegisterCmd("cmd_2", " I am cmd!\n desc desc.\n", true,
    //        [](const vex::console::CmdCtx& ctx)
    //        {
    //            // tst
    //            return ctx.parsed_args->get<int>("id", -1) == 42;
    //        });
    //}

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
        vex::console::ClCommand* cmd = vex::console::findCmd(cmd_part.c_str());

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

int vex::console::ConsoleWindow::TextEditCallback(ImGuiInputTextCallbackData* data)
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

            using CmdMap = vex::Dict<const char*, vex::console::ClCommand>;
            const CmdMap& cmd_dict = vex::console::CmdRunner::global().commands;

            const bool is_cmd_name = word_start == data->Buf;

            // Build a list of candidates
            // #fixme : frame allocator
            static std::vector<const char*> candidates;
            candidates.clear();
            if (is_cmd_name)
            {
                for ([[maybe_unused]] const auto& [key, cmd] : cmd_dict)
                {
                    // as it is case sensetive impl, we can just compare bytes
                    if (std::memcmp(key, word_start, (int)(word_end - word_start)) == 0)
                    {
                        candidates.push_back(key);
                    }
                }
            }
            //else if (data->Buf) #todo - declare and register args
            //{
            //    const char* wb = data->Buf;
            //    const char* we = wb;
            //    while (we < word_end && *we != ' ')
            //    {
            //        we++;
            //    }
            //    vex::console::ClCommand* cmd = cmd_dict.find(std::string_view(wb, wb - we));  
            //    if (cmd)
            //    {
            //        for ([[maybe_unused]] const auto& [key, cmd] : cmd->registered)
            //        {
            //            // as it is case sensetive impl, we can just compare bytes
            //            if (std::memcmp(key, word_start, (int)(word_end - word_start)) == 0)
            //            {
            //                candidates.push_back(key);
            //            }
            //        }
            //    }
            //}

            const i32 canditate_count = candidates.size();
            if (canditate_count == 0)
            {
                return 0;
            }
            else if (canditate_count == 1)
            {
                // Single match. Delete the beginning of the word and replace it entirely so
                // we've got nice casing.
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
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
                    data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                    data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
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
                    if (++history_pos >= (i32)history_buf.size())
                        history_pos = -1;
            }
            // A better implementation would preserve the data on the current input line
            // along with cursor position.
            if (prev_history_pos != history_pos)
            {
                const char* history_str = (history_pos >= 0) ? history_buf[(u32)history_pos].c_str()
                                                             : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, history_str);
            }
        }
    }
    return 0;
}