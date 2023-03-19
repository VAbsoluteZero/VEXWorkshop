#include "InputSystem.h"

#include <SDL_events.h>
#include <VCore/Containers/Ring.h>
#include <imgui_impl_sdl.h>
#include <utils/ImGuiUtils.h>

#define sid(v) ((u32)SignalId::v)

using namespace vex;
using namespace vex::input;


inline SignalState processSignal(SignalData old, SignalData fresh)
{ // should map onto enum. higher bit - cur state, lower - old state
    u8 v = ((old.isActive() << 1) | (u8)fresh.isActive());
    static_assert(0x0 == (u8)SignalState::None);
    static_assert(0x1 == (u8)SignalState::Started);
    static_assert(0x2 == (u8)SignalState::Finished);
    static_assert(0x3 == (u8)SignalState::Going);
    return (SignalState)v;
}
vex::input::InputSystem::InputSystem()
{
    const auto cnt = (u32)SignalId::sMax;
    state.prev_frame.addZeroed(cnt);
    for (u32 i = 0; i < cnt; ++i)
    {
        state.prev_frame[i].id = (SignalId)i;
    }
    state.this_frame.addRange(state.prev_frame.data(), cnt);

    // adding default triggers
    addTrigger("MouseMoved"_trig,
        Trigger{
            .fn_logic =
                [](Trigger& self, const InputState& state)
            { // should be active if mouse move any distance. (maybe limit to WITHIN a window)?
                if (state.this_frame[sid(MouseMove)].state > SignalState::None &&
                    state.this_frame[sid(MouseMove)].state != SignalState::Canceled)
                {
                    return true;
                }

                return false;
            },
        });
    addTrigger("MouseMidMove"_trig,
        Trigger{
            .fn_logic =
                [](Trigger& self, const InputState& state)
            {
                const auto moves = state.this_frame[sid(MouseMove)].state > SignalState::None;
                if ((state.this_frame[sid(MouseMID)].state == SignalState::Going) && moves)
                {
                    return true;
                }

                return false;
            },
        });
    addTrigger("MouseLeftDown"_trig,
        Trigger{
            .fn_logic =
                [](Trigger& self, const InputState& state)
            {
                if (state.this_frame[sid(MouseLBK)].state == SignalState::Started)
                {
                    return true;
                }
                return false;
            },
        });
    addTrigger("MouseRightDown"_trig,
        Trigger{
            .fn_logic =
                [](Trigger& self, const InputState& state)
            {
                if (state.this_frame[sid(MouseRBK)].state == SignalState::Started)
                {
                    return true;
                }
                return false;
            },
        });
    addTrigger("EscDown"_trig,
        Trigger{
            .fn_logic =
                [](Trigger& self, const InputState& state)
            {
                if (state.this_frame[sid(KeyEscape)].state == SignalState::Started)
                {
                    return true;
                }

                return false;
            },
        });
}

void vex::input::InputSystem::poll(float dt)
{
    SDL_Event sdl_event = {};
    auto& st = state.this_frame;
    while (SDL_PollEvent(&sdl_event) != 0)
    {
        switch (sdl_event.type)
        {
#if ENABLE_IMGUI
            ImGui_ImplSDL2_ProcessEvent(&sdl_event);
#endif
            case SDL_QUIT:
            {
                if (global.quit_delegate)
                    global.quit_delegate();
                break;
            }
            case SDL_WINDOWEVENT:
            {
                if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    if (global.resize_delegate)
                        global.resize_delegate();
                }
                break;
            }
            case SDL_MOUSEMOTION:
            {
                global.mouse_delta.x = sdl_event.motion.x;
                global.mouse_delta.y = sdl_event.motion.y;
                SDL_GetMouseState(&global.mouse_pos_window.x, &global.mouse_pos_window.y);
                SDL_GetGlobalMouseState(&global.mouse_pos_global.x, &global.mouse_pos_global.y);
                st[sid(MouseMove)].value_raw = global.mouse_delta;
            }
            break;
            case SDL_KEYDOWN:
                switch (sdl_event.key.keysym.sym)
                {
                    case SDLK_w: st[sid(KeyW)].value_raw.x = 1; break;
                    case SDLK_a: st[sid(KeyA)].value_raw.x = 1; break;
                    case SDLK_s: st[sid(KeyS)].value_raw.x = 1; break;
                    case SDLK_d: st[sid(KeyD)].value_raw.x = 1; break;
                    case SDLK_SPACE: st[sid(KeySpace)].value_raw.x = 1; break;
                    case SDLK_ESCAPE: st[sid(KeyEscape)].value_raw.x = 1; break;
                }
                break;

            default: break;
        }
    }
    // Uint8* keystate = SDL_GetKeyboardState(NULL);
    {
        int dx, dy;
        auto mask = SDL_GetMouseState(&dx, &dy);
        st[sid(MouseLBK)].value_raw.x = (mask & SDL_BUTTON_LMASK) != 0;
        st[sid(MouseRBK)].value_raw.x = (mask & SDL_BUTTON_RMASK) != 0;
        st[sid(MouseMID)].value_raw.x = (mask & SDL_BUTTON_MMASK) != 0;
    } 

    static StaticRing<SignalState, 128> debug_history[sid(sMax)]{};

    const auto cnt = (u32)SignalId::sMax;
    for (u32 i = 0; i < cnt; ++i)
    {
        auto& prev = state.prev_frame[i];
        auto& cur = state.this_frame[i];
        cur.flag = default_prep.prepareValue(cur.value_raw);
        cur.time_active = cur.isActive() * (prev.time_active + dt);
        cur.state = processSignal(prev, cur);
        debug_history[i].push(cur.state);
    }
    //SPDLOG_INFO("{}  = {} | {} ", st[sid(MouseRBK)].value_raw, (u32)st[sid(MouseRBK)].flag,
    //    (u32)st[sid(MouseRBK)].state);
}

void vex::input::InputSystem::updateTriggers()
{
    for (auto& [name, trigger] : triggers)
    {
        bool triggered = trigger.callProcessDelegate(trigger, state);
        trigger.triggered_this_frame = triggered;
    }
}

void vex::input::InputSystem::onFrameEnd()
{
    const auto cnt = (u32)SignalId::sMax;
    for (u32 i = 0; i < cnt; ++i)
    {
        state.prev_frame[i] = state.this_frame[i];
        state.this_frame[i] = {.id = (SignalId)i};
    }
    for (auto& [name, trigger] : triggers)
    {
        trigger.triggered_this_frame = false;
    }
}

bool vex::input::InputSystem::addTrigger(u64 hash, const Trigger& trigger)
{
    if (triggers.contains(hash))
    {
        std::string m = fmt::format("hash collision, rename trigger to unique id, hash:{}", hash);
        check(false, (m.c_str()));
        return false;
    }

    triggers.emplace(hash, trigger);

    return true;
}
