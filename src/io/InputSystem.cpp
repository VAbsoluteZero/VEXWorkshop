#include "InputSystem.h"

#include <SDL_events.h>
#include <VCore/Containers/Ring.h>
#include <imgui_impl_sdl.h>
#include <utils/ImGuiUtils.h>

#include <Tracy.hpp>

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

// inline void cancelActive(InputState& state, SignalState signal = SignalState::None)
//{
//     const auto cnt = (u32)SignalId::sMax;
//     for (u32 i = 0; i < cnt; ++i)
//     {
//         state.this_frame[i] = {.id = (SignalId)i, };
//     }
// }

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
    addTrigger("MouseMidScroll"_trig,
        Trigger{
            .fn_logic =
                [](Trigger& self, const InputState& state)
            {
                // const auto moves = state.this_frame[sid(MouseScroll)].state > SignalState::None;
                if ((state.this_frame[sid(MouseScroll)].state > SignalState::None))
                {
                    self.input_data[SignalId::MouseScroll] = state.this_frame[sid(MouseScroll)];
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
                    self.input_data[SignalId::KeyModCtrl] = state.this_frame[sid(KeyModCtrl)];
                    return true;
                }
                return false;
            },
        });
    addTrigger("MouseRightHeld"_trig,
        Trigger{
            .fn_logic =
                [](Trigger& self, const InputState& state)
            {
                if (state.this_frame[sid(MouseRBK)].state == SignalState::Started ||
                    state.this_frame[sid(MouseRBK)].state == SignalState::Going)
                {
                    self.input_data[SignalId::KeyModCtrl] = state.this_frame[sid(KeyModCtrl)];
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
                // self.input_data[sid(KeyEscape)]
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
    ZoneScopedN("Input:Poll");
    SDL_Event sdl_event = {};
    auto& st = state.this_frame;
    while (SDL_PollEvent(&sdl_event) != 0)
    {
#if ENABLE_IMGUI
        ImGui_ImplSDL2_ProcessEvent(&sdl_event);
#endif
        switch (sdl_event.type)
        {
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
                global.mouse_delta.x = sdl_event.motion.xrel;
                global.mouse_delta.y = sdl_event.motion.yrel;
                SDL_GetMouseState(&global.mouse_pos_window.x, &global.mouse_pos_window.y);
                SDL_GetGlobalMouseState(&global.mouse_pos_global.x, &global.mouse_pos_global.y);
                st[sid(MouseMove)].value_raw = global.mouse_delta;
                break;
            }
            case SDL_MOUSEWHEEL:
            {
                st[sid(MouseScroll)].value_raw.y = {sdl_event.wheel.preciseY};
                st[sid(MouseScroll)].value_raw.x = {sdl_event.wheel.preciseX};

                break;
            }

            default: break;
        }
    }
    // read keystates
    {
        i32 num = 0;
        auto* keystate = SDL_GetKeyboardState(&num);
        auto update_key = [&](u32 key, decltype(SDLK_w) sdlkey)
        {
            auto code = SDL_GetScancodeFromKey(sdlkey);
            check_(code <= num);
            auto state = keystate[code];
            st[key].value_raw.x = (state == SDL_PRESSED);
        };
        auto update_key_any_of = [&](u32 key, auto... keys)
        {
            check_((... && (SDL_GetScancodeFromKey(keys) <= num)));
            st[key].value_raw.x = (... || (keystate[SDL_GetScancodeFromKey(keys)] == SDL_PRESSED));
        };

        update_key(sid(KeyW), SDLK_w);
        update_key(sid(KeyA), SDLK_a);
        update_key(sid(KeyS), SDLK_s);
        update_key(sid(KeyD), SDLK_d);
        update_key(sid(KeySpace), SDLK_SPACE);
        update_key(sid(KeyEscape), SDLK_ESCAPE);
        // intended to read left/right into one value
        update_key_any_of(sid(KeyModAlt), SDLK_LALT, SDLK_RALT);
        update_key_any_of(sid(KeyModShift), SDLK_LSHIFT, SDLK_RSHIFT);
        update_key_any_of(sid(KeyModCtrl), SDLK_LCTRL, SDLK_RCTRL);
        // update_key(sid(KeyModAlt), SDLK_RALT);
        // update_key(sid(KeyModShift), SDLK_RSHIFT);
        // update_key(sid(KeyModCtrl), SDLK_RCTRL);
    }
    // read mouse states
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
    // SPDLOG_INFO("{}  = {} | {} ", st[sid(MouseRBK)].value_raw, (u32)st[sid(MouseRBK)].flag,
    //     (u32)st[sid(MouseRBK)].state);
}

void vex::input::InputSystem::updateTriggers()
{
    ZoneScopedN("Input:UpdateTriggers");
    for (auto& [name, trigger] : triggers)
    {
        bool triggered = trigger.callProcessDelegate(trigger, state);
        trigger.triggered_this_frame = triggered;
    }
}

void vex::input::InputSystem::onFrameEnd()
{
    ZoneScopedN("Input:FrameEnd");
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

bool vex::input::InputSystem::addTrigger(u64 hash, const Trigger& trigger, bool replace)
{
    if (triggers.contains(hash) && !replace)
    {
        std::string m = fmt::format("hash collision, rename trigger to unique id, hash:{}", hash);
        check(false, (m.c_str()));
        return false;
    }

    triggers.emplace(hash, trigger);

    return true;
}
